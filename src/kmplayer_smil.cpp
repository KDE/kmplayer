/**
 * Copyright (C) 2005-2007 by Koos Vriezen <koos.vriezen@gmail.com>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License version 2 as published by the Free Software Foundation.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Steet, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 **/

#include <config.h>
#include <qtextstream.h>
#include <qcolor.h>
#include <qpixmap.h>
#include <qmovie.h>
#include <qimage.h>
#include <qtextcodec.h>
#include <qfont.h>
#include <qapplication.h>
#include <qregexp.h>
#include <qtimer.h>

#include <kdebug.h>
#include <kurl.h>
#include <kmimetype.h>
#include <kio/job.h>
#include <kio/jobclasses.h>

#include "kmplayer_smil.h"
#include "kmplayer_rp.h"

using namespace KMPlayer;

namespace KMPlayer {
static const unsigned int duration_infinite = (unsigned int) -1;
static const unsigned int duration_media = (unsigned int) -2;
static const unsigned int duration_element_activated = (unsigned int) -3;
static const unsigned int duration_element_inbounds = (unsigned int) -4;
static const unsigned int duration_element_outbounds = (unsigned int) -5;
static const unsigned int duration_element_stopped = (unsigned int) -6;
static const unsigned int duration_data_download = (unsigned int) -7;
static const unsigned int duration_last_option = (unsigned int) -8;
static const unsigned int event_activated = duration_element_activated;
const unsigned int event_inbounds = duration_element_inbounds;
const unsigned int event_outbounds = duration_element_outbounds;
static const unsigned int event_stopped = duration_element_stopped;
static const unsigned int event_to_be_started = (unsigned int) -8;
const unsigned int event_paint = (unsigned int) -9;
const unsigned int event_sized = (unsigned int) -10;
const unsigned int event_pointer_clicked = event_activated;
const unsigned int event_pointer_moved = (unsigned int) -11;
const unsigned int event_timer = (unsigned int) -12;
const unsigned int event_postponed = (unsigned int) -13;

static const unsigned int started_timer_id = (unsigned int) 1;
static const unsigned int stopped_timer_id = (unsigned int) 2;
static const unsigned int start_timer_id = (unsigned int) 3;
static const unsigned int dur_timer_id = (unsigned int) 4;
static const unsigned int anim_timer_id = (unsigned int) 5;
static const unsigned int trans_timer_id = (unsigned int) 6;
}

/* Intrinsic duration 
 *  duration_time   |    end_time    |
 *  =======================================================================
 *  duration_media  | duration_media | wait for event
 *       0          | duration_media | only wait for child elements
 */
//-----------------------------------------------------------------------------

KDE_NO_EXPORT
bool KMPlayer::parseTime (const QString & vl, unsigned int & dur) {
    static QRegExp reg ("^\\s*([0-9\\.]+)\\s*([a-z]*)");
    //kdDebug () << "getSeconds " << val << (element ? element->nodeName() : "-") << endl;
    if (reg.search (vl) > -1) {
        bool ok;
        double t = reg.cap (1).toDouble (&ok);
        if (ok && t > 0.000) {
            //kdDebug() << "reg.cap (1) " << t << (ok && t > 0.000) << endl;
            QString u = reg.cap (2);
            if (u.startsWith ("m"))
                dur = (unsigned int) (10 * t * 60);
            else if (u.startsWith ("h"))
                dur = (unsigned int) (10 * t * 60 * 60);
            else
                dur = (unsigned int) (10 * t);
        } else
            dur = 0;
    } else if (vl.find ("indefinite") > -1)
        dur = duration_infinite;
    else if (vl.find ("media") > -1)
        dur = duration_media;
    else
        return false;
    return true;
}

static SMIL::Region * findRegion (NodePtr p, const QString & id) {
    for (NodePtr c = p->firstChild (); c; c = c->nextSibling ()) {
        if (c->id == SMIL::id_node_region) {
            SMIL::Region * r = convertNode <SMIL::Region> (c);
            QString a = r->getAttribute ("regionname");
            if (a.isEmpty ())
                a = r->getAttribute ("id");
            if ((a.isEmpty () && id.isEmpty ()) || a == id) {
                //kdDebug () << "MediaType region found " << id << endl;
                return r;
            }
        }
        SMIL::Region * r = findRegion (c, id);
        if (r)
            return r;
    }
    return 0L;
}

static SMIL::Transition * findTransition (NodePtr n, const QString & id) {
    SMIL::Smil * s = SMIL::Smil::findSmilNode (n);
    if (s) {
        Node * head = s->firstChild ().ptr ();
        while (head && head->id != SMIL::id_node_head)
            head = head->nextSibling ().ptr ();
        if (head)
            for (Node * c = head->firstChild (); c; c = c->nextSibling().ptr ())
                if (c->id == SMIL::id_node_transition &&
                        id == static_cast <Element *> (c)->getAttribute ("id"))
                    return static_cast <SMIL::Transition *> (c);
    }
    return 0L;
}

static NodePtr findLocalNodeById (NodePtr n, const QString & id) {
    SMIL::Smil * s = SMIL::Smil::findSmilNode (n);
    if (s)
        return s->document ()->getElementById (s, id, false);
    return 0L;
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT ToBeStartedEvent::ToBeStartedEvent (NodePtr n)
 : Event (event_to_be_started), node (n) {}

KDE_NO_CDTOR_EXPORT SizeEvent::SizeEvent (Single a, Single b, Single c, Single d
        , Fit f)
 : Event (event_sized), x (a), y (b), w (c), h (d), fit (f) {}

TimerEvent::TimerEvent (TimerInfoPtr tinfo)
 : Event (event_timer), timer_info (tinfo), interval (false) {}

PostponedEvent::PostponedEvent (bool postponed)
 : Event (event_postponed), is_postponed (postponed) {}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT TimedRuntime::TimedRuntime (NodePtr e)
 : timingstate (timings_reset), fill (fill_unknown),
   element (e), repeat_count (0) {}

KDE_NO_CDTOR_EXPORT TimedRuntime::~TimedRuntime () {}

KDE_NO_EXPORT void TimedRuntime::reset () {
    if (element) {
        if (start_timer) {
            element->document ()->cancelTimer (start_timer);
            ASSERT (!start_timer);
        }
        if (dur_timer) {
            element->document ()->cancelTimer (dur_timer);
            ASSERT (!dur_timer);
        }
    } else {
        start_timer = 0L;
        dur_timer = 0L;
    }
    repeat_count = 0;
    timingstate = timings_reset;
    fill = fill_unknown;
    for (int i = 0; i < (int) durtime_last; i++) {
        if (durations [i].connection)
            durations [i].connection->disconnect ();
        durations [i].durval = 0;
    }
    durations [end_time].durval = duration_media;
}

KDE_NO_EXPORT
void TimedRuntime::setDurationItem (DurationTime item, const QString & val) {
    unsigned int dur = 0; // also 0 for 'media' duration, so it will not update then
    QString vl = val.lower ();
    parseTime (vl, dur);
    if (!dur && element) {
        int pos = vl.find (QChar ('.'));
        if (pos > 0) {
            NodePtr e = findLocalNodeById (element, vl.left(pos));
            //kdDebug () << "getElementById " << vl.left (pos) << " " << (e ? e->nodeName () : "-") << endl;
            if (e) {
                if (vl.find ("activateevent") > -1) {
                    dur = duration_element_activated;
                } else if (vl.find ("inboundsevent") > -1) {
                    dur = duration_element_inbounds;
                } else if (vl.find ("outofboundsevent") > -1) {
                    dur = duration_element_outbounds;
                }
                durations [(int) item].connection = e->connectTo (element,dur);
            } else
                kdWarning () << "Element not found " << vl.left(pos) << endl;
        }
    }
    durations [(int) item].durval = dur;
}

/**
 * start, or restart in case of re-use, the durations
 */
KDE_NO_EXPORT void TimedRuntime::begin () {
    if (!element) {
        reset ();
        return;
    }
    //kdDebug () << "TimedRuntime::begin " << element->nodeName() << endl; 
    if (start_timer || dur_timer)
        convertNode <SMIL::TimedMrl> (element)->init ();
    timingstate = timings_began;

    if (durations [begin_time].durval > 0) {
        if (durations [begin_time].durval < duration_last_option)
            start_timer = element->document ()->setTimeout (element, 100 * durations [begin_time].durval, start_timer_id);
        else
            propagateStop (false);
    } else
        propagateStart ();
}

KDE_NO_EXPORT
bool TimedRuntime::parseParam (const QString & name, const QString & val) {
    //kdDebug () << "TimedRuntime::parseParam " << name << "=" << val << endl;
    const char * cname = name.ascii ();
    if (!strcmp (cname, "begin")) {
        setDurationItem (begin_time, val);
        if ((timingstate == timings_began && !start_timer) ||
                timingstate == timings_stopped) {
            if (durations [begin_time].durval > 0) { // create a timer for start
                if (durations [begin_time].durval < duration_last_option)
                    start_timer = element->document ()->setTimeout (element, 100 * durations [begin_time].durval, start_timer_id);
            } else                                // start now
                propagateStart ();
        }
    } else if (!strcmp (cname, "dur")) {
        setDurationItem (duration_time, val);
    } else if (!strcmp (cname, "end")) {
        setDurationItem (end_time, val);
        if (durations [end_time].durval < duration_last_option &&
            durations [end_time].durval > durations [begin_time].durval)
            durations [duration_time].durval =
                durations [end_time].durval - durations [begin_time].durval;
        else if (durations [end_time].durval > duration_last_option)
            durations [duration_time].durval = duration_media; // event
    } else if (!strcmp (cname, "endsync")) {
        if ((durations [duration_time].durval == duration_media ||
                    durations [duration_time].durval == 0) &&
                durations [end_time].durval == duration_media) {
            NodePtr e = findLocalNodeById (element, val);
            if (e && SMIL::isTimedMrl (e)) {
                SMIL::TimedMrl * tm = static_cast <SMIL::TimedMrl *> (e.ptr ());
                durations [(int) end_time].connection = tm->connectTo (element, event_stopped);
                durations [(int) end_time].durval = event_stopped;
            }
        }
    } else if (!strcmp (cname, "fill")) {
        if (val == QString::fromLatin1 ("freeze"))
            fill = fill_freeze;
        else
            fill = fill_unknown;
        // else all other fill options ..
    } else if (!strcmp (cname, "repeatCount")) {
        if (val.find ("indefinite") > -1)
            repeat_count = duration_infinite;
        else
            repeat_count = val.toInt ();
    } else if (!strcmp (cname, "title")) {
        Mrl * mrl = static_cast <Mrl *> (element.ptr ());
        if (mrl)
            mrl->pretty_name = val;
    } else
        return false;
    return true;
}

KDE_NO_EXPORT void TimedRuntime::processEvent (unsigned int event) {
    //kdDebug () << "TimedRuntime::processEvent " << (((unsigned int)-1) - event) << " " << (element ? element->nodeName() : "-") << endl; 
    if (timingstate != timings_started && durations [begin_time].durval == event) {
        propagateStart ();
    } else if (timingstate == timings_started && durations [end_time].durval == event)
        propagateStop (true);
}

KDE_NO_EXPORT void TimedRuntime::propagateStop (bool forced) {
    if (state() == timings_reset || state() == timings_stopped)
        return; // nothing to stop
    if (!forced && element) {
        if (durations [duration_time].durval == duration_media &&
                durations [end_time].durval == duration_media)
            return; // wait for external eof
        if (durations [end_time].durval > duration_last_option &&
                durations [end_time].durval != duration_media)
            return; // wait for event
        if (durations [duration_time].durval == duration_infinite)
            return; // this may take a while :-)
        if (dur_timer)
            return; // timerEvent will call us with forced=true
        // bail out if a child still running
        for (NodePtr c = element->firstChild (); c; c = c->nextSibling ())
            if (c->unfinished ())
                return; // a child still running
    }
    bool was_started (timingstate == timings_started);
    timingstate = timings_stopped;
    if (element) {
        if (start_timer) {
            element->document ()->cancelTimer (start_timer);
            ASSERT (!start_timer);
        }
        if (dur_timer) {
            element->document ()->cancelTimer (dur_timer);
            ASSERT (!dur_timer);
        }
        if (was_started)
            element->document ()->setTimeout (element, 0, stopped_timer_id);
        else if (element->unfinished ())
            element->finish ();
    } else {
        start_timer = 0L;
        dur_timer = 0L;
    }
}

KDE_NO_EXPORT void TimedRuntime::propagateStart () {
    SMIL::TimedMrl * tm = convertNode <SMIL::TimedMrl> (element);
    if (tm) {
        tm->propagateEvent (new ToBeStartedEvent (element));
        if (start_timer)
            tm->document ()->cancelTimer (start_timer);
        ASSERT (!start_timer);
    } else
        start_timer = 0L;
    timingstate = timings_started;
    element->document ()->setTimeout (element, 0, started_timer_id);
}

/**
 * start_timer timer expired
 */
KDE_NO_EXPORT void TimedRuntime::started () {
    //kdDebug () << "TimedRuntime::started " << (element ? element->nodeName() : "-") << endl; 
    if (durations [duration_time].durval > 0 &&
            durations [duration_time].durval < duration_last_option)
        dur_timer = element->document ()->setTimeout (element, 100 * durations [duration_time].durval, dur_timer_id);
     // kdDebug () << "TimedRuntime::started set dur timer " << durations [duration_time].durval << endl;
    element->begin ();
}

/**
 * duration_timer timer expired or no duration set after started
 */
KDE_NO_EXPORT void TimedRuntime::stopped () {
    if (!element) {
        reset ();
    } else if (element->active ()) {
        if (repeat_count == duration_infinite || 0 < repeat_count--) {
            if (durations [begin_time].durval > 0 &&
                    durations [begin_time].durval < duration_last_option)
                start_timer = element->document ()->setTimeout (element, 100 * durations [begin_time].durval, start_timer_id);
            else
                propagateStart ();
        } else {
            repeat_count = 0;
            element->finish ();
        }
    }
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT SizeType::SizeType () {
    reset ();
}

void SizeType::reset () {
    m_size = 0;
    percentage = 0;
    isset = false;
}

SizeType & SizeType::operator = (const QString & s) {
    QString strval (s);
    int p = strval.find (QChar ('%'));
    if (p > -1) {
        percentage = true;
        strval.truncate (p);
    } else
        percentage = false;
    m_size = strval.toDouble (&isset);
    return *this;
}

Single SizeType::size (Single relative_to) {
    if (percentage)
        return m_size * relative_to / 100;
    return m_size;
}

SRect SRect::unite (const SRect & r) const {
    Single a (_x < r._x ? _x : r._x);
    Single b (_y < r._y ? _y : r._y);
    return SRect (a, b, 
            ((_x + _w < r._x + r._w) ? r._x + r._w : _x + _w) - a,
            ((_y + _h < r._y + r._h) ? r._y + r._h : _y + _h) - b);
}

SRect SRect::intersect (const SRect & r) const {
    Single a (_x < r._x ? r._x : _x);
    Single b (_y < r._y ? r._y : _y);
    return SRect (a, b,
            ((_x + _w < r._x + r._w) ? _x + _w : r._x + r._w) - a,
            ((_y + _h < r._y + r._h) ? _y + _h : r._y + r._h) - b);
}

//-----------------------------------------------------------------------------

KDE_NO_EXPORT void CalculatedSizer::resetSizes () {
    left.reset ();
    top.reset ();
    width.reset ();
    height.reset ();
    right.reset ();
    bottom.reset ();
    reg_point.truncate (0);
    reg_align = QString::fromLatin1 ("topLeft");
}

static bool regPoints (const QString & str, Single & x, Single & y) {
    QString lower = str.lower ();
    const char * rp = lower.ascii ();
    if (!rp)
        return false;
    if (!strcmp (rp, "center")) {
        x = 50;
        y = 50;
    } else {
        if (!strncmp (rp, "top", 3)) {
            y = 0;
            rp += 3;
        } else if (!strncmp (rp, "mid", 3)) {
            y = 50;
            rp += 3;
        } else if (!strncmp (rp, "bottom", 6)) {
            y = 100;
            rp += 6;
        } else
            return false;
        if (!strcmp (rp, "left")) {
            x = 0;
        } else if (!strcmp (rp, "mid")) {
            x = 50;
        } else if (!strcmp (rp, "right")) {
            x = 100;
        } else
            return false;
    }
    return true;
}

KDE_NO_EXPORT
bool CalculatedSizer::applyRegPoints (Node * node, Single w, Single h,
        Single & xoff, Single & yoff, Single & w1, Single & h1) {
    if (reg_point.isEmpty ())
        return false;
    Single rpx, rpy, rax, ray;
    if (!regPoints (reg_point, rpx, rpy)) {
        node = SMIL::Smil::findSmilNode (node);
        if (!node)
            return false;
        node = static_cast <SMIL::Smil *> (node)->layout_node.ptr ();
        if (!node)
            return false;
        NodePtr c = node->firstChild ();
        for (; c; c = c->nextSibling ())
            if (c->id == SMIL::id_node_regpoint &&
                    convertNode<Element>(c)->getAttribute ("id") == reg_point) {
                Single i1, i2; // dummies
                SMIL::RegPoint *rp_elm = static_cast<SMIL::RegPoint*>(c.ptr());
                rp_elm->sizes.calcSizes (0L, 100, 100, rpx, rpy, i1, i2);
                QString ra = rp_elm->getAttribute ("regAlign");
                if (!ra.isEmpty () && reg_align.isEmpty ())
                    reg_align = ra;
                break;
            }
        if (!c)
            return false; // not found
    }
    if (!regPoints (reg_align, rax, ray))
        rax = ray = 0; // default back to topLeft
    if (!(int)w1 || !(int)h1) {
        xoff = w * (rpx - rax) / 100;
        yoff = h * (rpy - ray) / 100;
        w1 = w - w * (rpx > rax ? (rpx - rax) : (rax - rpx)) / 100;
        h1 = h - h * (rpy > ray ? (rpy - ray) : (ray - rpy)) / 100;
    } else {
        xoff = (w * rpx - w1 * rax) / 100;
        yoff = (h * rpy - h1 * ray) / 100;
    }
    // kdDebug () << "calc rp:" << reg_point << " ra:" << reg_align <<  " w:" << (int)w << " h:" << (int)h << " xoff:" << (int)xoff << " yoff:" << (int)yoff << " w1:" << (int)w1 << " h1:" << (int)h1 << endl;
    return true; // success getting sizes based on regPoint
}

KDE_NO_EXPORT void CalculatedSizer::calcSizes (Node * node, Single w, Single h,
        Single & xoff, Single & yoff, Single & w1, Single & h1) {
    if (applyRegPoints (node, w, h, xoff, yoff, w1, h1))
        return;
    if (left.isSet ())
        xoff = left.size (w);
    else if (width.isSet ())
        xoff = (w - width.size (w)) / 2;
    else
        xoff = 0;
    if (top.isSet ())
        yoff = top.size (h);
    else if (height.isSet ())
        yoff = (h - height.size (h)) / 2;
    else
        yoff = 0;
    if (width.isSet ())
        w1 = width.size (w);
    else if (right.isSet ())
        w1 = w - xoff - right.size (w);
    else
        w1 = w - xoff;
    if (w1 < 0)
        w1 = 0;
    if (height.isSet ())
        h1 = height.size (h);
    else if (bottom.isSet ())
        h1 = h - yoff - bottom.size (h);
    else
        h1 = h - yoff;
    if (h1 < 0)
        h1 = 0;
}

KDE_NO_EXPORT
bool CalculatedSizer::setSizeParam (const QString & name, const QString & val) {
    const char * cname = name.ascii ();
    if (!strcmp (cname, "left")) {
        left = val;
    } else if (!strcmp (cname, "top")) {
        top = val;
    } else if (!strcmp (cname, "width")) {
        width = val;
    } else if (!strcmp (cname, "height")) {
        height = val;
    } else if (!strcmp (cname, "right")) {
        right = val;
    } else if (!strcmp (cname, "bottom")) {
        bottom = val;
    } else if (!strcmp (cname, "regPoint")) {
        reg_point = val;
    } else if (!strcmp (cname, "regAlign")) {
        reg_align = val;
    } else
        return false;
    return true;
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT AnimateGroupData::AnimateGroupData (NodePtr e)
 : TimedRuntime (e), modification_id (-1) {}

bool AnimateGroupData::parseParam (const QString & name, const QString & val) {
    //kdDebug () << "AnimateGroupData::parseParam " << name << "=" << val << endl;
    const char * cname = name.ascii ();
    if (!strcmp (cname, "target") || !strcmp (cname, "targetElement")) {
        if (element)
            target_element = findLocalNodeById (element, val);
    } else if (!strcmp(cname, "attribute") || !strcmp(cname, "attributeName")) {
        changed_attribute = val;
    } else if (!strcmp (cname, "to")) {
        change_to = val;
    } else
        return TimedRuntime::parseParam (name, val);
    return true;
}

/**
 * animation finished
 */
KDE_NO_EXPORT void AnimateGroupData::stopped () {
    //kdDebug () << "AnimateGroupData::stopped " << durations [duration_time].durval << endl;
    if (fill != fill_freeze || durations [end_time].durval != duration_media)
        restoreModification ();
    TimedRuntime::stopped ();
}

KDE_NO_EXPORT void AnimateGroupData::reset () {
    restoreModification ();
    TimedRuntime::reset ();
}

KDE_NO_EXPORT void AnimateGroupData::restoreModification () {
    if (modification_id > -1 && target_element &&
            target_element->state > Node::state_init) {
        //kdDebug () << "AnimateGroupData(" << this << ")::restoreModificatio " <<modification_id << endl;
        convertNode <Element> (target_element)->resetParam (
                changed_attribute, modification_id);
    }
    modification_id = -1;
}

//-----------------------------------------------------------------------------

/**
 * start_timer timer expired, execute it
 */
KDE_NO_EXPORT void SetData::started () {
    //kdDebug () << "SetData::started " << durations [duration_time].durval << endl;
    restoreModification ();
    if (element) {
        if (target_element) {
            convertNode <Element> (target_element)->setParam (
                    changed_attribute, change_to, &modification_id);
            //kdDebug () << "SetData(" << this << ")::started " << target_element->nodeName () << "." << changed_attribute << " ->" << change_to << " modid:" << modification_id << endl;
        } else
            kdWarning () << "target element not found" << endl;
    } else
        kdWarning () << "set element disappeared" << endl;
    AnimateGroupData::started ();
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT AnimateData::AnimateData (NodePtr e)
 : AnimateGroupData (e), change_by (0), steps (0) {}

KDE_NO_EXPORT void AnimateData::reset () {
    AnimateGroupData::reset ();
    if (element) {
        if (anim_timer)
            element->document ()->cancelTimer (anim_timer);
        ASSERT (!anim_timer);
    } else
        anim_timer = 0;
    accumulate = acc_none;
    additive = add_replace;
    change_by = 0;
    calcMode = calc_linear;
    change_from.truncate (0);
    change_values.clear ();
    steps = 0;
    change_delta = change_to_val = change_from_val = 0.0;
    change_from_unit.truncate (0);
}

bool AnimateData::parseParam (const QString & name, const QString & val) {
    //kdDebug () << "AnimateData::parseParam " << name << "=" << val << endl;
    const char * cname = name.ascii ();
    if (!strcmp (cname, "change_by")) {
        change_by = val.toInt ();
    } else if (!strcmp (cname, "from")) {
        change_from = val;
    } else if (!strcmp (cname, "values")) {
        change_values = QStringList::split (QString (";"), val);
    } else if (name.lower () == QString::fromLatin1 ("calcmode")) {
        if (val == QString::fromLatin1 ("discrete"))
            calcMode = calc_discrete;
        else if (val == QString::fromLatin1 ("linear"))
            calcMode = calc_linear;
        else if (val == QString::fromLatin1 ("paced"))
            calcMode = calc_paced;
    } else
        return AnimateGroupData::parseParam (name, val);
    return true;
}

/**
 * start_timer timer expired, execute it
 */
KDE_NO_EXPORT void AnimateData::started () {
    //kdDebug () << "AnimateData::started " << durations [duration_time].durval << endl;
    restoreModification ();
    if (anim_timer) {
        kdWarning () << "AnimateData::started " << anim_timer.ptr() << endl;
        element->document ()->cancelTimer (anim_timer);
    }
    bool success = false;
    do {
        if (!element) {
            kdWarning () << "set element disappeared" << endl;
            break;
        }
        NodePtr protect = target_element;
        Element * target = convertNode <Element> (target_element);
        if (!target) {
            kdWarning () << "target element not found" << endl;
            break;
        }
        if (calcMode == calc_linear) {
            QRegExp reg ("^\\s*([0-9\\.]+)(\\s*[%a-z]*)?");
            if (change_from.isEmpty ()) {
                if (change_values.size () > 0) // check 'values' attribute
                     change_from = change_values.first ();
                else // take current
                    change_from = target->param (changed_attribute);
            }
            if (!change_from.isEmpty ()) {
                target->setParam (changed_attribute, change_from, &modification_id);
                if (reg.search (change_from) > -1) {
                    change_from_val = reg.cap (1).toDouble ();
                    change_from_unit = reg.cap (2);
                }
            } else {
                kdWarning() << "animate couldn't determine start value" << endl;
                break;
            }
            if (change_to.isEmpty () && change_values.size () > 1)
                change_to = change_values.last (); // check 'values' attribute
            if (!change_to.isEmpty () && reg.search (change_to) > -1) {
                change_to_val = reg.cap (1).toDouble ();
            } else {
                kdWarning () << "animate couldn't determine end value" << endl;
                break;
            }
            steps = 20 * durations [duration_time].durval / 5; // 40 per sec
            if (steps > 0) {
                anim_timer = element->document ()->setTimeout (element, 25, anim_timer_id); // 25 ms for now FIXME
                change_delta = (change_to_val - change_from_val) / steps;
                //kdDebug () << "AnimateData::started " << target_element->nodeName () << "." << changed_attribute << " " << change_from_val << "->" << change_to_val << " in " << steps << " using:" << change_delta << " inc" << endl;
                success = true;
            }
        } else if (calcMode == calc_discrete) {
            steps = change_values.size () - 1; // we do already the first step
            if (steps < 1) {
                 kdWarning () << "animate needs at least two values" << endl;
                 break;
            }
            int interval = 100 * durations [duration_time].durval / (1 + steps);
            if (interval <= 0 ||
                    durations [duration_time].durval > duration_last_option) {
                 kdWarning () << "animate needs a duration time" << endl;
                 break;
            }
            //kdDebug () << "AnimateData::started " << target_element->nodeName () << "." << changed_attribute << " " << change_values.first () << "->" << change_values.last () << " in " << steps << " interval:" << interval << endl;
            anim_timer = element->document ()->setTimeout (element, interval, anim_timer_id); // 50 /s for now FIXME
            target->setParam (changed_attribute, change_values.first (), &modification_id);
            success = true;
        }
    } while (false);
    if (success)
        AnimateGroupData::started ();
    else
        propagateStop (true);
}

/**
 * undo if necessary
 */
KDE_NO_EXPORT void AnimateData::stopped () {
    // kdDebug () << "AnimateData::stopped " << element->state << endl;
    if (element) {
        if (anim_timer) // make sure timers are stopped
            element->document ()->cancelTimer (anim_timer);
        ASSERT (!anim_timer);
    } else
        anim_timer = 0;
    AnimateGroupData::stopped ();
}

/**
 * for animations
 */
KDE_NO_EXPORT void AnimateData::timerTick () {
    if (!anim_timer) {
        kdError () << "spurious anim timer tick" << endl;
        return;
    }
    Element * target = convertNode <Element> (target_element);
    if (steps-- > 0 && target) {
        if (calcMode == calc_linear) {
            change_from_val += change_delta;
            target->setParam (changed_attribute, QString ("%1%2").arg (change_from_val).arg(change_from_unit), &modification_id);
        } else if (calcMode == calc_discrete) {
            target->setParam (changed_attribute, change_values[change_values.size () - steps -1], &modification_id);
        }
    } else {
        if (element)
            element->document ()->cancelTimer (anim_timer);
        ASSERT (!anim_timer);
        propagateStop (true); // not sure, actually
    }
}

//-----------------------------------------------------------------------------

static NodePtr findExternalTree (NodePtr mrl) {
    for (NodePtr c = mrl->firstChild (); c; c = c->nextSibling ()) {
        Mrl * m = c->mrl ();
        if (m && m->opener == mrl)
            return c;
    }
    return 0L;
}

KDE_NO_CDTOR_EXPORT MediaTypeRuntime::MediaTypeRuntime (NodePtr e)
 : TimedRuntime (e), fit (fit_hidden) {}

KDE_NO_CDTOR_EXPORT MediaTypeRuntime::~MediaTypeRuntime () {
    killWGet ();
}

/**
 * re-implement for pending KIO::Job operations
 */
KDE_NO_EXPORT void KMPlayer::MediaTypeRuntime::reset () {
    clear ();
    postpone_lock = 0L;
    TimedRuntime::reset ();
}

/**
 * re-implement for regions and src attributes
 */
KDE_NO_EXPORT
bool MediaTypeRuntime::parseParam (const QString & name, const QString & val) {
    SMIL::MediaType * mt = convertNode <SMIL::MediaType> (element);
    if (!mt)
        return false;
    if (name == QString::fromLatin1 ("fit")) {
        const char * cval = val.ascii ();
        if (!strcmp (cval, "fill"))
            fit = fit_fill;
        else if (!strcmp (cval, "hidden"))
            fit = fit_hidden;
        else if (!strcmp (cval, "meet"))
            fit = fit_meet;
        else if (!strcmp (cval, "scroll"))
            fit = fit_scroll;
        else if (!strcmp (cval, "slice"))
            fit = fit_slice;
    } else if (!sizes.setSizeParam (name, val)) {
        return TimedRuntime::parseParam (name, val);
    }
    SMIL::RegionBase * rb = convertNode <SMIL::RegionBase> (mt->region_node);
    if ((state () == timings_began ||
                (state () == TimedRuntime::timings_stopped &&
                 fill == TimedRuntime::fill_freeze)) &&
            rb && element)
        rb->repaint ();
    return true;
}

/**
 * will request a repaint of attached region
 */
KDE_NO_EXPORT void MediaTypeRuntime::stopped () {
    clipStop ();
    document_postponed = 0L;
    Node * e = element.ptr ();
    if (e) {
        for (NodePtr n = e->firstChild (); n; n = n->nextSibling ())
            if (n->unfinished ())   // finish child documents
                n->finish ();
    }
    TimedRuntime::stopped ();
}

KDE_NO_EXPORT void MediaTypeRuntime::clipStart () {
    SMIL::MediaType * mt = convertNode <SMIL::MediaType> (element);
    SMIL::RegionBase *r =mt ? convertNode<SMIL::RegionBase>(mt->region_node):0L;
    if (r && r->surface)
        for (NodePtr n = mt->firstChild (); n; n = n->nextSibling ())
            if ((n->mrl () && n->mrl ()->opener.ptr () == mt) ||
                    n->id == SMIL::id_node_smil ||
                    n->id == RP::id_node_imfl) {
                n->activate ();
                if (r->surface->node)
                    r->surface->node->handleEvent (
                            new SizeEvent (0,0, r->w, r->h, fit));
                break;
            }
}

KDE_NO_EXPORT void MediaTypeRuntime::clipStop () {
    SMIL::MediaType * mt = convertNode <SMIL::MediaType> (element);
    if (mt && mt->external_tree && mt->external_tree->active ())
        mt->external_tree->deactivate ();
}

KDE_NO_EXPORT void MediaTypeRuntime::postpone (bool) {
}

KDE_NO_CDTOR_EXPORT AudioVideoData::AudioVideoData (NodePtr e)
    : MediaTypeRuntime (e) {}

KDE_NO_EXPORT bool AudioVideoData::isAudioVideo () {
    return timingstate == timings_started;
}

/**
 * reimplement for request backend to play audio/video
 */
KDE_NO_EXPORT void AudioVideoData::started () {
    if (element && !element->mrl ()->resolved) {
        element->defer ();
        return;
    }
    if (durations [duration_time].durval == 0 &&
            durations [end_time].durval == duration_media)
        durations [duration_time].durval = duration_media; // duration of clip
    MediaTypeRuntime::started ();
}

static void setSmilLinkNode (NodePtr n, NodePtr link) {
    // this works only because we can only play one at a time FIXME
    SMIL::Smil * s = SMIL::Smil::findSmilNode (n.ptr ());
    if (s)
        s->current_av_media_type = link;
}

KDE_NO_EXPORT void AudioVideoData::clipStart () {
    NodePtr element_protect = element; // note element is weak
    SMIL::MediaType * mt = convertNode <SMIL::MediaType> (element);
    PlayListNotify * n = mt ? mt->document ()->notify_listener : 0L;
    //kdDebug() << "AudioVideoData::clipStart " << mt->resolved << endl;
    if (n && mt->region_node && !mt->external_tree && !mt->src.isEmpty()) {
        setSmilLinkNode (element, element);
        mt->positionVideoWidget ();
        n->requestPlayURL (mt);
        document_postponed = mt->document()->connectTo(mt, event_postponed);
    }
    MediaTypeRuntime::clipStart ();
}

KDE_NO_EXPORT void AudioVideoData::clipStop () {
    if (durations [duration_time].durval == duration_media)
        durations [duration_time].durval = 0; // reset to make this finish
    MediaTypeRuntime::clipStop ();
    setSmilLinkNode (element, 0L);
}

KDE_NO_EXPORT
bool AudioVideoData::parseParam (const QString & name, const QString & val) {
    //kdDebug () << "AudioVideoData::parseParam " << name << "=" << val << endl;
    if (name == QString::fromLatin1 ("src")) {
        NodePtr element_protect = element; // note element is weak
        SMIL::MediaType * mt = convertNode <SMIL::MediaType> (element);
        if (mt) {
            if (!mt->resolved || mt->src != val) {
                if (mt->external_tree)
                    mt->removeChild (mt->external_tree);
                mt->src = val;
                mt->resolved = mt->document ()->notify_listener->resolveURL (element);
                if (mt->resolved) // update external_tree here 
                    mt->external_tree = findExternalTree (element);
            }
            if (timingstate == timings_started && mt->resolved)
                clipStart ();
        }
    } else
        return MediaTypeRuntime::parseParam (name, val);
    return true;
}

KDE_NO_EXPORT void AudioVideoData::postpone (bool b) {
    kdDebug () << "AudioVideoData::postpone " << b << endl;
    if (element->unfinished () && b)
        element->setState (Node::state_deferred);
    else if (element->state == Node::state_deferred && !b)
        element->setState (Node::state_began);
}

//-----------------------------------------------------------------------------

static Element * fromScheduleGroup (NodePtr & d, const QString & tag) {
    const char * ctag = tag.ascii ();
    if (!strcmp (ctag, "par"))
        return new SMIL::Par (d);
    else if (!strcmp (ctag, "seq"))
        return new SMIL::Seq (d);
    else if (!strcmp (ctag, "excl"))
        return new SMIL::Excl (d);
    return 0L;
}

static Element * fromParamGroup (NodePtr & d, const QString & tag) {
    const char * ctag = tag.ascii ();
    if (!strcmp (ctag, "param"))
        return new SMIL::Param (d);
    else if (!strcmp (ctag, "area") || !strcmp (ctag, "anchor"))
        return new SMIL::Area (d);
    return 0L;
}

static Element * fromAnimateGroup (NodePtr & d, const QString & tag) {
    const char * ctag = tag.ascii ();
    if (!strcmp (ctag, "set"))
        return new SMIL::Set (d);
    else if (!strcmp (ctag, "animate"))
        return new SMIL::Animate (d);
    return 0L;
}

static Element * fromMediaContentGroup (NodePtr & d, const QString & tag) {
    const char * taglatin = tag.latin1 ();
    if (!strcmp (taglatin, "video") || !strcmp (taglatin, "audio"))
        return new SMIL::AVMediaType (d, tag);
    else if (!strcmp (taglatin, "img"))
        return new SMIL::ImageMediaType (d);
    else if (!strcmp (taglatin, "text"))
        return new SMIL::TextMediaType (d);
    else if (!strcmp (taglatin, "ref"))
        return new SMIL::RefMediaType (d);
    else if (!strcmp (taglatin, "brush"))
        return new SMIL::Brush (d);
    else if (!strcmp (taglatin, "a"))
        return new SMIL::Anchor (d);
    // animation, textstream
    return 0L;
}

static Element * fromContentControlGroup (NodePtr & d, const QString & tag) {
    if (!strcmp (tag.latin1 (), "switch"))
        return new SMIL::Switch (d);
    return 0L;
}

//-----------------------------------------------------------------------------

KDE_NO_EXPORT NodePtr SMIL::Smil::childFromTag (const QString & tag) {
    const char * ctag = tag.ascii ();
    if (!strcmp (ctag, "body"))
        return new SMIL::Body (m_doc);
    else if (!strcmp (ctag, "head"))
        return new SMIL::Head (m_doc);
    return NodePtr ();
}

KDE_NO_EXPORT void SMIL::Smil::activate () {
    //kdDebug () << "Smil::activate" << endl;
    current_av_media_type = NodePtr ();
    resolved = true;
    PlayListNotify * n = document()->notify_listener;
    if (n)
        n->setCurrent (this);
    SMIL::Layout * layout = convertNode <SMIL::Layout> (layout_node);
    if (layout)
        layout->surface = Mrl::getSurface (layout_node);
    if (layout && layout->surface)
        Element::activate ();
    else
        Element::deactivate(); // some unfortunate reset in parent doc
}

KDE_NO_EXPORT void SMIL::Smil::deactivate () {
    if (layout_node)
        convertNode <SMIL::Layout> (layout_node)->repaint ();    
    Mrl::deactivate ();
    if (layout_node)
        convertNode <SMIL::Layout> (layout_node)->surface = Mrl::getSurface(0L);
}

KDE_NO_EXPORT void SMIL::Smil::closed () {
    width = 0;
    height = 0;
    NodePtr head;
    for (NodePtr e = firstChild (); e; e = e->nextSibling ())
        if (e->id == id_node_head) {
            head = e;
            break;
        }
    if (!head) {
        SMIL::Head * h = new SMIL::Head (m_doc);
        insertBefore (h, firstChild ());
        h->setAuxiliaryNode (true);
        h->closed ();
        head = h;
    }
    for (NodePtr e = head->firstChild (); e; e = e->nextSibling ()) {
        if (e->id == id_node_layout) {
            layout_node = e;
        } else if (e->id == id_node_title) {
            QString str = e->innerText ();
            pretty_name = str.left (str.find (QChar ('\n')));
        } else if (e->id == id_node_meta) {
            Element * elm = convertNode <Element> (e);
            const QString name = elm->getAttribute ("name");
            if (name == QString::fromLatin1 ("title"))
                pretty_name = elm->getAttribute ("content");
            else if (name == QString::fromLatin1 ("base"))
                src = elm->getAttribute ("content");
        }
    }
    if (!layout_node) {
        kdError () << "no <root-layout>" << endl;
        return;
    }
    SMIL::RegionBase * rb = convertNode <SMIL::RegionBase> (layout_node);
    if (rb && !rb->auxiliaryNode ()) {
        width = rb->w;
        height = rb->h;
    }
}

KDE_NO_EXPORT void SMIL::Smil::childDone (NodePtr child) {
    if (unfinished ()) {
        if (child->nextSibling ())
            child->nextSibling ()->activate ();
        else {
            for (NodePtr e = firstChild (); e; e = e->nextSibling ())
                if (e->active ())
                    e->deactivate ();
            finish ();
        }
    }
}

KDE_NO_EXPORT Mrl * SMIL::Smil::linkNode () {
    return current_av_media_type ? current_av_media_type->mrl () : this;
}

KDE_NO_EXPORT bool SMIL::Smil::isPlayable () {
    return true;
}

KDE_NO_EXPORT bool SMIL::Smil::expose () const {
    return !pretty_name.isEmpty () || //return false if no title and only one
        previousSibling () || nextSibling ();
}

KDE_NO_EXPORT void SMIL::Smil::accept (Visitor * v) {
    if (active () && layout_node)
        layout_node->accept( v );
}

void SMIL::Smil::jump (const QString & id) {
    NodePtr n = document ()->getElementById (this, id, false);
    if (n) {
        if (n->unfinished ())
            kdDebug() << "Smil::jump node is unfinished " << id << endl;
        else {
            for (NodePtr p = n; p; p = p->parentNode ()) {
                if (p->unfinished () &&
                        p->id >= id_node_first_group &&
                        p->id <= id_node_last_group) {
                    convertNode <GroupBase> (p)->setJumpNode (n);
                    break;
                }
                if (n->id == id_node_body || n->id == id_node_smil) {
                    kdError() << "Smil::jump node passed body for " <<id<< endl;
                    break;
                }
            }
        }
    }
}

SMIL::Smil * SMIL::Smil::findSmilNode (Node * node) {
    for (Node * e = node; e; e = e->parentNode ().ptr ())
        if (e->id == SMIL::id_node_smil)
            return static_cast <SMIL::Smil *> (e);
    return 0L;
}

//-----------------------------------------------------------------------------

static void headChildDone (NodePtr node, NodePtr child) {
    if (node->unfinished ()) {
        if (child->nextSibling ())
            child->nextSibling ()->activate ();
        else
            node->finish (); // we're done
    }
}

KDE_NO_EXPORT NodePtr SMIL::Head::childFromTag (const QString & tag) {
    const char * ctag = tag.ascii ();
    if (!strcmp (ctag, "layout"))
        return new SMIL::Layout (m_doc);
    else if (!strcmp (ctag, "title"))
        return new DarkNode (m_doc, tag, id_node_title);
    else if (!strcmp (ctag, "meta"))
        return new DarkNode (m_doc, tag, id_node_meta);
    else if (!strcmp (ctag, "transition"))
        return new SMIL::Transition (m_doc);
    return NodePtr ();
}

KDE_NO_EXPORT bool SMIL::Head::expose () const {
    return false;
}

KDE_NO_EXPORT void SMIL::Head::closed () {
    for (NodePtr e = firstChild (); e; e = e->nextSibling ())
        if (e->id == id_node_layout)
            return;
    SMIL::Layout * layout = new SMIL::Layout (m_doc);
    appendChild (layout);
    layout->setAuxiliaryNode (true);
    layout->closed (); // add root-layout and a region
}

KDE_NO_EXPORT void SMIL::Head::childDone (NodePtr child) {
    headChildDone (this, child);
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT SMIL::Layout::Layout (NodePtr & d)
 : RegionBase (d, id_node_layout) {}

KDE_NO_EXPORT NodePtr SMIL::Layout::childFromTag (const QString & tag) {
    const char * ctag = tag.ascii ();
    if (!strcmp (ctag, "root-layout")) {
        NodePtr e = new SMIL::RootLayout (m_doc);
        rootLayout = e;
        return e;
    } else if (!strcmp (ctag, "region"))
        return new SMIL::Region (m_doc);
    else if (!strcmp (ctag, "regPoint"))
        return new SMIL::RegPoint (m_doc);
    return NodePtr ();
}

KDE_NO_EXPORT void SMIL::Layout::closed () {
    SMIL::RegionBase * smilroot = convertNode <SMIL::RootLayout> (rootLayout);
    bool has_root (smilroot);
    if (!has_root) { // just add one if none there
        smilroot = new SMIL::RootLayout (m_doc);
        NodePtr sr = smilroot; // protect against destruction
        smilroot->setAuxiliaryNode (true);
        rootLayout = smilroot;
        int w_root =0, h_root = 0, reg_count = 0;
        for (NodePtr n = firstChild (); n; n = n->nextSibling ()) {
            if (n->id == id_node_region) {
                SMIL::Region * rb =convertNode <SMIL::Region> (n);
                rb->init ();
                rb->calculateBounds (0, 0);
                if (int (rb->x + rb->w) > w_root)
                    w_root = rb->x + rb->w;
                if (int (rb->y + rb->h) > h_root)
                    h_root = rb->y + rb->h;
                reg_count++;
            }
        }
        if (!reg_count) {
            w_root = 320; h_root = 240; // have something to start with
            SMIL::Region * r = new SMIL::Region (m_doc);
            appendChild (r);
            r->setAuxiliaryNode (true);
        }
        smilroot->setAttribute ("width", QString::number (w_root));
        smilroot->setAttribute ("height", QString::number (h_root));
        insertBefore (sr, firstChild ());
    } else if (childNodes ()->length () < 2) { // only a root-layout
        SMIL::Region * r = new SMIL::Region (m_doc);
        appendChild (r);
        r->setAuxiliaryNode (true);
    }
}

KDE_NO_EXPORT void SMIL::Layout::activate () {
    //kdDebug () << "SMIL::Layout::activate" << endl;
    RegionBase::activate ();
    updateDimensions (surface);
    repaint ();
    finish (); // proceed and allow 'head' to finish
}

KDE_NO_EXPORT void SMIL::Layout::updateDimensions (SurfacePtr) {
    RegionBase * rb = static_cast <RegionBase *> (rootLayout.ptr ());
    x = y = 0;
    w = rb->sizes.width.size ();
    h = rb->sizes.height.size ();
    //kdDebug () << "Layout::updateDimensions " << w << "," << h <<endl;
    SMIL::RegionBase::updateDimensions (surface);
}

KDE_NO_EXPORT bool SMIL::Layout::handleEvent (EventPtr event) {
    bool handled = false;
    switch (event->id ()) {
        case event_sized: {
            SizeEvent * e = static_cast <SizeEvent *> (event.ptr ());
            Single ex = e->x, ey = e->y, ew = e->w, eh = e->h;
            Single xoff, yoff;
            float xscale = 1.0, yscale = 1.0;
            float pxscale = 1.0, pyscale = 1.0;
            if (auxiliaryNode () && rootLayout) {
                w = ew;
                h = eh;
                Element * rl = convertNode <Element> (rootLayout);
                rl->setAttribute ("width", QString::number ((int)ew));
                rl->setAttribute ("height", QString::number ((int)eh));
                rl->setParam ("width", QString::number ((int)ew));
                rl->setParam ("height", QString::number ((int)eh));
                updateDimensions (surface);
            } else if (w > 0 && h > 0) {
                xscale += 1.0 * (ew - w) / w;
                yscale += 1.0 * (eh - h) / h;
                if (surface) {
                    pxscale = 1.0 * surface->bounds.width () / w;
                    pyscale = 1.0 * surface->bounds.height () / h;
                }
                if (e->fit == fit_meet)
                    if (xscale > yscale) {
                        xscale = yscale;
                        pxscale = pyscale;
                        xoff = ew - (Single (w * (xscale - 1.0)) + w);
                        ew -= xoff;
                        xoff /= 2;
                        ex += xoff;
                    } else {
                        yscale = xscale;
                        pyscale = pxscale;
                        yoff = eh - (Single (h * (yscale - 1.0)) + h);
                        eh -= yoff;
                        yoff /= 2;
                        ey += yoff;
                    }
            } else
                break;
            if (surface) {
                surface->xoffset = xoff;
                surface->yoffset = yoff;
                surface->xscale = pxscale;
                surface->yscale = pyscale;
            }
            RegionBase::handleEvent (new SizeEvent (0, 0, ew, eh, e->fit));
            handled = true;
            break;
        }
        default:
            return RegionBase::handleEvent (event);
    }
    return handled;
}

KDE_NO_EXPORT void SMIL::Layout::accept (Visitor * v) {
    v->visit (this);
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT SMIL::RegionBase::RegionBase (NodePtr & d, short id)
 : Element (d, id), x (0), y (0), w (0), h (0),
   z_order (1), background_color (0),
   m_SizeListeners (new NodeRefList),
   m_PaintListeners (new NodeRefList) {}

KDE_NO_CDTOR_EXPORT SMIL::RegionBase::~RegionBase () {
}

KDE_NO_EXPORT void SMIL::RegionBase::activate () {
    setState (state_activated);
    init ();
    for (NodePtr r = firstChild (); r; r = r->nextSibling ())
        if (r->id == id_node_region || r->id == id_node_root_layout)
            r->activate ();
}

KDE_NO_EXPORT void SMIL::RegionBase::childDone (NodePtr child) {
    headChildDone (this, child);
}

KDE_NO_EXPORT void SMIL::RegionBase::deactivate () {
    background_color = 0;
    if (surface)
        surface->background_color = 0;
    sizes.resetSizes ();
    Element::deactivate ();
}

KDE_NO_EXPORT void SMIL::RegionBase::repaint () {
    if (surface)
        surface->repaint (0, 0, w, h);
}

KDE_NO_EXPORT void SMIL::RegionBase::updateDimensions (SurfacePtr psurface) {
    if (!surface) {
        surface = psurface->createSurface (this, SRect (x, y, w, h));
        surface->background_color = background_color;
    }
    for (NodePtr r = firstChild (); r; r = r->nextSibling ())
        if (r->id == id_node_region) {
            SMIL::Region * cr = static_cast <SMIL::Region *> (r.ptr ());
            cr->calculateBounds (w, h);
            cr->updateDimensions (surface);
        }
}

bool SMIL::RegionBase::handleEvent (EventPtr event) {
    switch (event->id ()) {
        case event_sized: {
            propagateEvent (event);
            for (NodePtr n = firstChild (); n; n = n->nextSibling ())
                if (n->id == id_node_region)
                    n->handleEvent (event);
            break;
        }
        default:
            return Element::handleEvent (event);
    }
    return true;
}

KDE_NO_EXPORT
void SMIL::RegionBase::parseParam (const QString & name, const QString & val) {
    //kdDebug () << "RegionBase::parseParam " << getAttribute ("id") << " " << name << "=" << val << " active:" << active() << endl;
    bool need_repaint = false;
    SRect rect = SRect (x, y, w, h);
    const char * cn = name.ascii ();
    if (!strcmp (cn, "background-color") || !strcmp (cn, "backgroundColor")) {
        background_color = 0xff000000 | QColor (val).rgb ();
        if (surface)
            surface->background_color = background_color;
        need_repaint = true;
    } else if (!strcmp (cn, "z-index")) {
        z_order = val.toInt ();
        need_repaint = true;
    } else if (sizes.setSizeParam (name, val)) {
        if (active () && surface) {
            NodePtr p = parentNode ();
            if (p &&(p->id==SMIL::id_node_region ||p->id==SMIL::id_node_layout))
                convertNode <SMIL::RegionBase> (p)->updateDimensions (0L);
            rect = rect.unite (SRect (x, y, w, h));
            need_repaint = true;
        }
    }
    if (need_repaint && active () && surface && surface->parentNode ())
        surface->parentNode ()->repaint (rect.x(), rect.y(),
                rect.width(), rect.height());
    Element::parseParam (name, val);
}

NodeRefListPtr SMIL::RegionBase::listeners (unsigned int eid) {
    if (eid == event_paint)
        return m_PaintListeners;
    else if (eid == event_sized)
        return m_SizeListeners;
    return Element::listeners (eid);
}

KDE_NO_CDTOR_EXPORT SMIL::Region::Region (NodePtr & d)
 : RegionBase (d, id_node_region),
   has_mouse (false),
   m_ActionListeners (new NodeRefList),
   m_OutOfBoundsListeners (new NodeRefList),
   m_InBoundsListeners (new NodeRefList) {}

KDE_NO_EXPORT NodePtr SMIL::Region::childFromTag (const QString & tag) {
    if (!strcmp (tag.latin1 (), "region"))
        return new SMIL::Region (m_doc);
    return NodePtr ();
}

/**
 * calculates dimensions of this regions with _w and _h as width and height
 * of parent Region (representing 100%)
 */
KDE_NO_EXPORT
void SMIL::Region::calculateBounds (Single pw, Single ph) {
    Single x1 (x), y1 (y), w1 (w), h1 (h);
    sizes.calcSizes (this, pw, ph, x, y, w, h);
    if (x1 != x || y1 != y || w1 != w || h1 != h) {
        propagateEvent (new SizeEvent (0, 0, w, h, fit_meet));
    }
    if (surface)
        surface->bounds = SRect (x, y, w, h);
    //kdDebug () << "Region::calculateBounds parent:" << pw << "x" << ph << " this:" << x << "," << y << " " << w << "x" << h << endl;
}

bool SMIL::Region::handleEvent (EventPtr event) {
    switch (event->id ()) {
        case event_sized: {
            SizeEvent * e = static_cast <SizeEvent *> (event.ptr ());
            return RegionBase::handleEvent (new SizeEvent (0, 0, w, h, e->fit));
        }
        default:
            return RegionBase::handleEvent (event);
    }
}

NodeRefListPtr SMIL::Region::listeners (unsigned int eid) {
    switch (eid) {
        case event_activated:
            return m_ActionListeners;
        case event_inbounds:
            return m_InBoundsListeners;
        case event_outbounds:
            return m_OutOfBoundsListeners;
    }
    return RegionBase::listeners (eid);
}

KDE_NO_EXPORT void SMIL::Region::accept (Visitor * v) {
    v->visit (this);
}

//-----------------------------------------------------------------------------

KDE_NO_EXPORT
void SMIL::RegPoint::parseParam (const QString & p, const QString & v) {
    sizes.setSizeParam (p, v); // TODO: if dynamic, make sure to repaint
    Element::parseParam (p, v);
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT SMIL::Transition::Transition (NodePtr & d)
 : Element (d, id_node_transition),
   direction (dir_forward), dur (10), fade_color (0) {}

KDE_NO_EXPORT void SMIL::Transition::activate () {
    init ();
    Element::activate ();
}

KDE_NO_EXPORT
void SMIL::Transition::parseParam (const QString & para, const QString & val) {
    const char * cpara = para.ascii ();
    if (!strcmp (cpara, "type"))
        type = val;
    else if (!strcmp (cpara, "subtype"))
        subtype = val;
    else if (!strcmp (cpara, "dur"))
        dur = int (10 * val.toDouble ());
    else if (!strcmp (cpara, "fadeColor"))
        fade_color = QColor (getAttribute (val)).rgb ();
    else if (!strcmp (cpara, "direction"))
        direction = val == "reverse" ? dir_reverse : dir_forward;
    else
        Element::parseParam (para, val);
}

KDE_NO_EXPORT bool SMIL::Transition::supported () {
    return type == "fade";
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT SMIL::TimedMrl::TimedMrl (NodePtr & d, short id)
 : Mrl (d, id),
   m_StartedListeners (new NodeRefList),
   m_StoppedListeners (new NodeRefList),
   runtime (0L) {}

KDE_NO_CDTOR_EXPORT SMIL::TimedMrl::~TimedMrl () {
    delete runtime;
}

KDE_NO_EXPORT void SMIL::TimedMrl::closed () {
    pretty_name = getAttribute ("title");
    Mrl::closed ();
}

KDE_NO_EXPORT void SMIL::TimedMrl::init () {
    timedRuntime ()->reset ();
    Mrl::init ();
}

KDE_NO_EXPORT void SMIL::TimedMrl::activate () {
    //kdDebug () << "SMIL::TimedMrl(" << nodeName() << ")::activate" << endl;
    setState (state_activated);
    TimedRuntime * rt = timedRuntime ();
    init ();
    if (rt == runtime) // Runtime might already be dead
        rt->begin ();
    else
        deactivate ();
}

KDE_NO_EXPORT void SMIL::TimedMrl::begin () {
    Mrl::begin ();
    timedRuntime ()->propagateStop (false); //see whether this node has a livetime or not
}

KDE_NO_EXPORT void SMIL::TimedMrl::deactivate () {
    //kdDebug () << "SMIL::TimedMrl(" << nodeName() << ")::deactivate" << endl;
    if (unfinished ())
        finish ();
    if (runtime)
        runtime->reset ();
    Mrl::deactivate ();
}

KDE_NO_EXPORT void SMIL::TimedMrl::finish () {
    Mrl::finish ();
    timedRuntime ()->propagateStop (true); // in case not yet stopped
    propagateEvent (new Event (event_stopped));
}

KDE_NO_EXPORT void SMIL::TimedMrl::reset () {
    //kdDebug () << "SMIL::TimedMrl::reset " << endl;
    Mrl::reset ();
    delete runtime;
    runtime = 0L;
}

KDE_NO_EXPORT bool SMIL::TimedMrl::expose () const {
    return !pretty_name.isEmpty ();
}

KDE_NO_EXPORT void SMIL::TimedMrl::childBegan (NodePtr) {
    if (state != state_began)
        begin ();
}

/*
 * Re-implement, but keeping sequential behaviour.
 * Bail out if Runtime is running. In case of duration_media, give Runtime
 * a hand with calling propagateStop(true)
 */
KDE_NO_EXPORT void SMIL::TimedMrl::childDone (NodePtr c) {
    if (!active ())
        return; // forced reset
    if (c->nextSibling ())
        c->nextSibling ()->activate ();
    else { // check if Runtime still running
        TimedRuntime * tr = timedRuntime ();
        if (tr->state () < TimedRuntime::timings_stopped) {
            if (tr->state () == TimedRuntime::timings_started)
                tr->propagateStop (false);
            return; // still running, wait for runtime to finish
        }
        finish ();
    }
}

KDE_NO_EXPORT NodeRefListPtr SMIL::TimedMrl::listeners (unsigned int id) {
    if (id == event_stopped)
        return m_StoppedListeners;
    else if (id == event_to_be_started)
        return m_StartedListeners;
    kdWarning () << "unknown event requested" << endl;
    return NodeRefListPtr ();
}

KDE_NO_EXPORT bool SMIL::TimedMrl::handleEvent (EventPtr event) {
    int id = event->id ();
    switch (id) {
        case event_timer: {
            TimerEvent * te = static_cast <TimerEvent *> (event.ptr ());
            if (te && te->timer_info) {
                if (te->timer_info->event_id == started_timer_id)
                    timedRuntime ()->started ();
                else if (te->timer_info->event_id == stopped_timer_id)
                    timedRuntime ()->stopped ();
                else if (te->timer_info->event_id == start_timer_id)
                    timedRuntime ()->propagateStart ();
                else if (te->timer_info->event_id == dur_timer_id)
                    timedRuntime ()->propagateStop (true);
                else
                    kdWarning () << "unhandled timer event" << endl;
            }
            break;
        }
        default:
            timedRuntime ()->processEvent (id);
    }
    return true;
}

KDE_NO_EXPORT TimedRuntime * SMIL::TimedMrl::getNewRuntime () {
    return new TimedRuntime (this);
}

KDE_NO_EXPORT
void SMIL::TimedMrl::parseParam (const QString & para, const QString & value) {
    if (!timedRuntime ()->parseParam (para, value)) {
        if (para != QString::fromLatin1 ("src")) //block Mrl src parsing for now
            Mrl::parseParam (para, value);
        else
            kdDebug() << "parseParam src on " << nodeName() << endl;
    }
}

//-----------------------------------------------------------------------------

KDE_NO_EXPORT bool SMIL::GroupBase::isPlayable () {
    return false;
}

KDE_NO_EXPORT void SMIL::GroupBase::finish () {
    setState (state_finished); // avoid recurstion through childDone
    bool deactivate_children = timedRuntime()->fill!=TimedRuntime::fill_freeze;
    for (NodePtr e = firstChild (); e; e = e->nextSibling ())
        if (deactivate_children) {
            if (e->active ())
                e->deactivate ();
        } else if (e->unfinished ())
            e->finish ();
    TimedMrl::finish ();
}

KDE_NO_EXPORT void SMIL::GroupBase::deactivate () {
    setState (state_deactivated); // avoid recurstion through childDone
    for (NodePtr e = firstChild (); e; e = e->nextSibling ())
        if (e->active ())
            e->deactivate ();
    TimedMrl::deactivate ();
}

KDE_NO_EXPORT void SMIL::GroupBase::defer () {
    if (state == state_init)
        init ();
    state = state_deferred;
}

KDE_NO_EXPORT void SMIL::GroupBase::begin () {
    if (state == state_deferred && jump_node)
        undefer ();
    TimedMrl::begin ();
}

KDE_NO_EXPORT void SMIL::GroupBase::undefer () {
    NodePtr jump = jump_node;
    jump_node = 0L;
    if (jump) { // find next level group to pass jump_node
        NodePtr group;
        NodePtr last_checked;
        for (NodePtr n = jump; n; n = n->parentNode ()) {
            if (n.ptr () == this || n->id == id_node_body)
                break;
            if (n->id >= id_node_first_group && n->id <= id_node_last_group)
                group = n;
            last_checked = n;
        }
        if (jump.ptr () == this || !last_checked) {
            activate ();
        } else {
            if (group) {
                group->defer ();
                convertNode <GroupBase> (group)->jump_node = jump;
                group->begin ();
            } else
                for (NodePtr c = firstChild (); c; c = c->nextSibling ()) {
                    if (c == last_checked) {
                        c->activate ();
                        if (id != id_node_par)
                            break;        // seq behaviour
                    }
                    if (id == id_node_par)
                        c->activate ();   //initialize it
                }
        }
    }
    state = state_activated;
}

KDE_NO_EXPORT void SMIL::GroupBase::setJumpNode (NodePtr n) {
    jump_node = n;
    setState (state_deferred);
    for (NodePtr c = firstChild (); c; c = c->nextSibling ())
        if (c->active ())
            c->reset ();
    undefer ();
}

//-----------------------------------------------------------------------------

// SMIL::Body was here

//-----------------------------------------------------------------------------

KDE_NO_EXPORT NodePtr SMIL::Par::childFromTag (const QString & tag) {
    Element * elm = fromScheduleGroup (m_doc, tag);
    if (!elm) elm = fromMediaContentGroup (m_doc, tag);
    if (!elm) elm = fromContentControlGroup (m_doc, tag);
    if (!elm) elm = fromAnimateGroup (m_doc, tag);
    if (elm)
        return elm;
    return NodePtr ();
}

KDE_NO_EXPORT void SMIL::Par::begin () {
    //kdDebug () << "SMIL::Par::begin" << endl;
    for (NodePtr e = firstChild (); e; e = e->nextSibling ())
        e->activate ();
    GroupBase::begin ();
}

KDE_NO_EXPORT void SMIL::Par::reset () {
    GroupBase::reset ();
    for (NodePtr e = firstChild (); e; e = e->nextSibling ())
        e->reset ();
}

KDE_NO_EXPORT void SMIL::Par::childDone (NodePtr) {
    if (unfinished ()) {
        for (NodePtr e = firstChild (); e; e = e->nextSibling ()) {
            if (e->unfinished ())
                return; // not all finished
        }
        TimedRuntime * tr = timedRuntime ();
        if (tr->state () == TimedRuntime::timings_started) {
            unsigned dv =tr->durations[(int)TimedRuntime::duration_time].durval;
            if (dv == 0 || dv == duration_media)
                tr->propagateStop (false);
            return; // still running, wait for runtime to finish
        }
        finish (); // we're done
    }
}

//-----------------------------------------------------------------------------

KDE_NO_EXPORT NodePtr SMIL::Seq::childFromTag (const QString & tag) {
    Element * elm = fromScheduleGroup (m_doc, tag);
    if (!elm) elm = fromMediaContentGroup (m_doc, tag);
    if (!elm) elm = fromContentControlGroup (m_doc, tag);
    if (!elm) elm = fromAnimateGroup (m_doc, tag);
    if (elm)
        return elm;
    return 0L;
}

KDE_NO_EXPORT void SMIL::Seq::begin () {
    //kdDebug () << "SMIL::Seq::begin" << endl;
    if (state != state_deferred && firstChild ())
        firstChild ()->activate ();
    GroupBase::begin ();
}

KDE_NO_EXPORT void SMIL::Seq::childDone (NodePtr child) {
    if (unfinished ()) {
        if (state != state_deferred)
            GroupBase::childDone (child);
        else if (jump_node)
            finish ();
    }
}

//-----------------------------------------------------------------------------

KDE_NO_EXPORT NodePtr SMIL::Excl::childFromTag (const QString & tag) {
    Element * elm = fromScheduleGroup (m_doc, tag);
    if (!elm) elm = fromMediaContentGroup (m_doc, tag);
    if (!elm) elm = fromContentControlGroup (m_doc, tag);
    if (!elm) elm = fromAnimateGroup (m_doc, tag);
    if (elm)
        return elm;
    return 0L;
}

KDE_NO_EXPORT void SMIL::Excl::begin () {
    //kdDebug () << "SMIL::Excl::begin" << endl;
    for (NodePtr e = firstChild (); e; e = e->nextSibling ())
        e->activate ();
    for (NodePtr e = firstChild (); e; e = e->nextSibling ())
        if (SMIL::isTimedMrl (e)) {
            SMIL::TimedMrl * tm = static_cast <SMIL::TimedMrl *> (e.ptr ());
            if (tm) { // make aboutToStart connection with TimedMrl children
                ConnectionPtr c = tm->connectTo (this, event_to_be_started);
                started_event_list.append (new ConnectionStoreItem (c));
            }
        }
    GroupBase::begin ();
}

KDE_NO_EXPORT void SMIL::Excl::deactivate () {
    started_event_list.clear (); // auto disconnect on destruction of data items
    GroupBase::deactivate ();
}

KDE_NO_EXPORT void SMIL::Excl::childDone (NodePtr /*child*/) {
    // first check if somenode has taken over
    for (NodePtr e = firstChild (); e; e = e->nextSibling ())
        if (SMIL::isTimedMrl (e)) {
            TimedRuntime *tr = convertNode <SMIL::TimedMrl> (e)->timedRuntime();
            if (tr->state () == TimedRuntime::timings_started)
                return;
        }
    // now finish unless 'dur="indefinite/some event/.."'
    TimedRuntime * tr = timedRuntime ();
    if (tr->state () == TimedRuntime::timings_started)
        tr->propagateStop (false); // still running, wait for runtime to finish
    else
        finish (); // we're done
}

KDE_NO_EXPORT bool SMIL::Excl::handleEvent (EventPtr event) {
    if (event->id () == event_to_be_started) {
        ToBeStartedEvent * se = static_cast <ToBeStartedEvent *> (event.ptr ());
        //kdDebug () << "Excl::handleEvent " << se->node->nodeName()<<endl;
        for (NodePtr e = firstChild (); e; e = e->nextSibling ()) {
            if (e == se->node) // stop all _other_ child elements
                continue;
            if (!SMIL::isTimedMrl (e))
                continue; // definitely a stowaway
            convertNode<SMIL::TimedMrl>(e)->timedRuntime()->propagateStop(true);
        }
        return true;
    } else
        return TimedMrl::handleEvent (event);
}

//-----------------------------------------------------------------------------

KDE_NO_EXPORT NodePtr SMIL::Switch::childFromTag (const QString & tag) {
    Element * elm = fromContentControlGroup (m_doc, tag);
    if (!elm) elm = fromMediaContentGroup (m_doc, tag);
    if (elm)
        return elm;
    return 0L;
}

KDE_NO_EXPORT void SMIL::Switch::activate () {
    //kdDebug () << "SMIL::Switch::activate" << endl;
    setState (state_activated);
    PlayListNotify * n = document()->notify_listener;
    int pref = 0, max = 0x7fffffff, currate = 0;
    if (n)
        n->bitRates (pref, max);
    if (firstChild ()) {
        NodePtr fallback;
        for (NodePtr e = firstChild (); e; e = e->nextSibling ()) {
            if (e->id == id_node_audio_video) {
                SMIL::MediaType * mt = convertNode <SMIL::MediaType> (e);
                if (!chosenOne) {
                    chosenOne = e;
                    currate = mt->bitrate;
                } else if (int (mt->bitrate) <= max) {
                    int delta1 = pref > currate ? pref-currate : currate-pref;
                    int delta2 = pref > int (mt->bitrate) ? pref-mt->bitrate : mt->bitrate-pref;
                    if (delta2 < delta1) {
                        chosenOne = e;
                        currate = mt->bitrate;
                    }
                }
            } else if (!fallback && e->isPlayable ())
                fallback = e;
        }
        if (!chosenOne)
            chosenOne = (fallback ? fallback : firstChild ());
        Mrl * mrl = chosenOne->mrl ();
        if (mrl) {
            src = mrl->src;
            if (pretty_name.isEmpty ())
                pretty_name = mrl->pretty_name;
        }
        // we must active chosenOne, it must set video position by itself
        setState (state_activated);
        chosenOne->activate ();
    }
}

KDE_NO_EXPORT Mrl * SMIL::Switch::linkNode () {
    return chosenOne ? chosenOne->mrl () : this;
}

KDE_NO_EXPORT void SMIL::Switch::deactivate () {
    Element::deactivate ();
    for (NodePtr e = firstChild (); e; e = e->nextSibling ())
        if (e->active ()) {
            e->deactivate ();
            break; // deactivate only the one running
        }
}

KDE_NO_EXPORT void SMIL::Switch::reset () {
    Element::reset ();
    for (NodePtr e = firstChild (); e; e = e->nextSibling ()) {
        if (e->state != state_init)
            e->reset ();
    }
}

KDE_NO_EXPORT void SMIL::Switch::childDone (NodePtr child) {
    if (child->state == state_finished)
        child->deactivate ();
    //kdDebug () << "SMIL::Switch::childDone" << endl;
    finish (); // only one child can run
}

KDE_NO_EXPORT bool SMIL::Switch::isPlayable () {
    if (cached_ismrl_version != document()->m_tree_version) {
        cached_ismrl = false;
        for (NodePtr e = firstChild (); e; e = e->nextSibling ())
            if (e->isPlayable ()) {
                cached_ismrl = true;
                break;
            }
    }
    return cached_ismrl;
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT SMIL::LinkingBase::LinkingBase (NodePtr & d, short id)
 : Element(d, id), show (show_replace) {}

KDE_NO_EXPORT void SMIL::LinkingBase::deactivate () {
    mediatype_activated = 0L;
    Element::deactivate ();
}

KDE_NO_EXPORT
void SMIL::LinkingBase::parseParam (const QString & para, const QString & val) {
    if (para == QString ("href")) {
        href = val;
    }
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT SMIL::Anchor::Anchor (NodePtr & d)
 : LinkingBase (d, id_node_anchor) {}

KDE_NO_EXPORT void SMIL::Anchor::activate () {
    init ();
    for (NodePtr c = firstChild(); c; c = c->nextSibling ())
        if (c->id >=id_node_first_mediatype && c->id <=id_node_last_mediatype) {
            mediatype_activated = c->connectTo (this, event_activated);
            break;
        }
    Element::activate ();
}

KDE_NO_EXPORT void SMIL::Anchor::childDone (NodePtr child) {
    if (unfinished ()) {
        if (child->nextSibling ())
            child->nextSibling ()->activate ();
        else
            finish ();
    }
}

NodePtr SMIL::Anchor::childFromTag (const QString & tag) {
    return fromMediaContentGroup (m_doc, tag);
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT SMIL::Area::Area (NodePtr & d)
 : LinkingBase (d, id_node_area) {}

KDE_NO_EXPORT void SMIL::Area::activate () {
    init ();
    if (parentNode () &&
            parentNode ()->id >= id_node_first_mediatype &&
            parentNode ()->id <= id_node_last_mediatype)
        mediatype_activated = parentNode ()->connectTo (this, event_activated);
    Element::activate ();
}

KDE_NO_EXPORT
void SMIL::Area::parseParam (const QString & para, const QString & val) {
    if (para == QString ("coords")) {
        ; // TODO
    } else
        LinkingBase::parseParam (para, val);
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT SMIL::MediaType::MediaType (NodePtr &d, const QString &t, short id)
 : TimedMrl (d, id), m_type (t), bitrate (0), trans_step (0), trans_steps (0),
   m_ActionListeners (new NodeRefList),
   m_OutOfBoundsListeners (new NodeRefList),
   m_InBoundsListeners (new NodeRefList) {
    view_mode = Mrl::WindowMode;
}

KDE_NO_EXPORT NodePtr SMIL::MediaType::childFromTag (const QString & tag) {
    Element * elm = fromContentControlGroup (m_doc, tag);
    if (!elm) elm = fromParamGroup (m_doc, tag);
    if (!elm) elm = fromAnimateGroup (m_doc, tag);
    if (elm)
        return elm;
    return 0L;
}

KDE_NO_EXPORT
void SMIL::MediaType::parseParam (const QString & para, const QString & val) {
    const char * cname = para.ascii ();
    if (!strcmp (cname, "system-bitrate"))
        bitrate = val.toInt ();
    else if (!strcmp (cname, "type"))
        mimetype = val;
    else if (!strcmp (cname, "transIn")) {
        trans_in = findTransition (this, val);
        if (!trans_in)
            kdWarning() << "Transition " << val << " not found in head" << endl;
    } else if (!strcmp (cname, "transOut")) {
        trans_out = findTransition (this, val);
        if (!trans_out)
            kdWarning() << "Transition " << val << " not found in head" << endl;
    } else
        TimedMrl::parseParam (para, val);
}

KDE_NO_EXPORT void SMIL::MediaType::activate () {
    setState (state_activated);
    init (); // sets all attributes
    for (NodePtr c = firstChild (); c; c = c->nextSibling ())
        if (c != external_tree) {
            // activate param/set/animate.. children
            c->activate ();
            break; // childDone will handle next siblings
        }
    timedRuntime ()->begin ();
}

KDE_NO_EXPORT void SMIL::MediaType::deactivate () {
    region_node = 0L;
    region_sized = 0L;
    region_paint = 0L;
    region_mouse_enter = 0L;
    region_mouse_leave = 0L;
    region_mouse_click = 0L;
    trans_step = trans_steps = 0;
    if (trans_timer)
        document ()->cancelTimer (trans_timer);
    TimedMrl::deactivate ();
}

KDE_NO_EXPORT void SMIL::MediaType::begin () {
    SMIL::Smil * s = Smil::findSmilNode (parentNode ().ptr ());
    SMIL::Region * r = s ? findRegion (s->layout_node, param ("region")) : 0L;
    MediaTypeRuntime *tr = static_cast<MediaTypeRuntime*>(timedRuntime ());
    if (r) {
        region_node = r;
        region_sized = r->connectTo (this, event_sized);
        region_paint = r->connectTo (this, event_paint);
        region_mouse_enter = r->connectTo (this, event_inbounds);
        region_mouse_leave = r->connectTo (this, event_outbounds);
        region_mouse_click = r->connectTo (this, event_activated);
        r->repaint ();
        tr->clipStart ();
        Transition * trans = convertNode <Transition> (trans_in);
        if (trans && trans->supported ()) {
            trans_step = 1;
            trans_steps = trans->dur; // 10/s FIXME
            trans_timer = document()->setTimeout(this, 100, trans_timer_id);
        }
    } else
        kdWarning () << "MediaType::begin no region found" << endl;
    TimedMrl::begin ();
}

KDE_NO_EXPORT void SMIL::MediaType::finish () {
    region_sized = 0L;
    if (trans_timer && timedRuntime()->fill != TimedRuntime::fill_freeze) {
        document ()->cancelTimer (trans_timer);
        ASSERT(!trans_timer);
    }
    if (region_node)
        convertNode <SMIL::RegionBase> (region_node)->repaint ();
    TimedMrl::finish ();
    static_cast <MediaTypeRuntime *> (timedRuntime ())->clipStop ();
}

KDE_NO_EXPORT bool SMIL::MediaType::expose () const {
    return TimedMrl::expose () ||               // if title attribute
        (!src.isEmpty () && !external_tree);    // or we're a playable leaf
}

/**
 * Re-implement from TimedMrl, because we may have children like
 * param/set/animatie that should all be activate, but also other smil or imfl
 * documents, that should only be activated if the runtime has started
 */
KDE_NO_EXPORT void SMIL::MediaType::childDone (NodePtr child) {
    bool child_doc = child->mrl () && child->mrl ()->opener.ptr () == this;
    if (child_doc) {
        child->deactivate (); // should only if fill not is freeze or hold
    } else if (active ()) { // traverse param or area children
        for (NodePtr c = child->nextSibling(); c; c = c->nextSibling ())
            if (!c->mrl () || c->mrl ()->opener.ptr () != this ) {
                c->activate ();
                return;
            }
        TimedRuntime * tr = timedRuntime ();
        if (tr->state () < TimedRuntime::timings_stopped) {
            if (tr->state () == TimedRuntime::timings_started)
                tr->propagateStop (child_doc); // what about repeat_count ..
            return; // still running, wait for runtime to finish
        }
    }
    if (active ())
        finish ();
}

KDE_NO_EXPORT void SMIL::MediaType::positionVideoWidget () {
    MediaTypeRuntime * mtr = static_cast <MediaTypeRuntime *> (timedRuntime ());
    if (unfinished () && region_node) {
        RegionBase * rb = convertNode <RegionBase> (region_node);
        Single x = 0, y = 0, w = 0, h = 0;
        if (!strcmp (nodeName (), "video") || !strcmp (nodeName (), "ref"))
            mtr->sizes.calcSizes (this, rb->w, rb->h, x, y, w, h);
        if (rb->surface)
            rb->surface->video (x, y, w, h);
    }
}

SurfacePtr SMIL::MediaType::getSurface (NodePtr node) {
    RegionBase * r = convertNode <RegionBase> (region_node);
    if (r && r->surface) {
        if (node) {
            r->surface->node = node;
            node->handleEvent (new SizeEvent (0, 0, r->w, r->h, fit_meet));
            return r->surface; // FIXME add surface to this one here
        }
        r->surface->node = r;
    }
    return 0L;
}

bool SMIL::MediaType::handleEvent (EventPtr event) {
    bool ret = false;
    RegionBase * r = convertNode <RegionBase> (region_node);
    switch (event->id ()) {
        case event_sized:
            break; // make it pass to all listeners
        case event_postponed: {
            PostponedEvent * pe = static_cast <PostponedEvent *> (event.ptr ());
            static_cast<MediaTypeRuntime*>(timedRuntime())->postpone (pe->is_postponed);
            ret = true;
            break;
        }
        case event_timer: {
            TimerEvent * te = static_cast <TimerEvent *> (event.ptr ());
            if (r && te && te->timer_info &&
                    te->timer_info->event_id == trans_timer_id) {
                te->interval = ++trans_step < trans_steps;
                r->repaint ();
                ret = true;
                break;
            }
        } // fall through
        default:
            ret = TimedMrl::handleEvent (event);
    }
    if (r && r->surface && r->surface->node && r != r->surface->node)
        return r->surface->node->handleEvent (event);
    return ret;
}

KDE_NO_EXPORT NodeRefListPtr SMIL::MediaType::listeners (unsigned int id) {
    switch (id) {
        case event_activated:
            return m_ActionListeners;
        case event_inbounds:
            return m_InBoundsListeners;
        case event_outbounds:
            return m_OutOfBoundsListeners;
    }
    return TimedMrl::listeners (id);
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT
SMIL::AVMediaType::AVMediaType (NodePtr & d, const QString & t)
 : SMIL::MediaType (d, t, id_node_audio_video) {}

KDE_NO_EXPORT NodePtr SMIL::AVMediaType::childFromTag (const QString & tag) {
    return fromXMLDocumentTag (m_doc, tag);
}

KDE_NO_EXPORT void SMIL::AVMediaType::defer () {
    setState (state_deferred);
    MediaTypeRuntime * mr = static_cast <MediaTypeRuntime *> (timedRuntime ());
    if (mr->state () == TimedRuntime::timings_started)
        mr->postpone_lock = document ()->postpone ();
}

KDE_NO_EXPORT void SMIL::AVMediaType::undefer () {
    setState (state_activated);
    external_tree = findExternalTree (this);
    MediaTypeRuntime * mr = static_cast <MediaTypeRuntime *> (timedRuntime ());
    if (mr->state () == TimedRuntime::timings_started) {
        mr->postpone_lock = 0L;
        mr->started ();
    }
}

KDE_NO_EXPORT TimedRuntime * SMIL::AVMediaType::getNewRuntime () {
    return new AudioVideoData (this);
}

KDE_NO_EXPORT bool SMIL::AVMediaType::handleEvent (EventPtr event) {
    if (event->id () == event_sized && !external_tree)
        positionVideoWidget ();
    return MediaType::handleEvent (event);
}

KDE_NO_EXPORT void SMIL::AVMediaType::accept (Visitor * v) {
    v->visit (this);
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT
SMIL::ImageMediaType::ImageMediaType (NodePtr & d)
    : SMIL::MediaType (d, "img", id_node_img) {}

KDE_NO_EXPORT TimedRuntime * SMIL::ImageMediaType::getNewRuntime () {
    return new ImageRuntime (this);
}

KDE_NO_EXPORT NodePtr SMIL::ImageMediaType::childFromTag (const QString & tag) {
    if (!strcmp (tag.latin1 (), "imfl"))
        return new RP::Imfl (m_doc);
    return SMIL::MediaType::childFromTag (tag);
}

KDE_NO_EXPORT void SMIL::ImageMediaType::accept (Visitor * v) {
    v->visit (this);
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT
SMIL::TextMediaType::TextMediaType (NodePtr & d)
    : SMIL::MediaType (d, "text", id_node_text) {}

KDE_NO_EXPORT TimedRuntime * SMIL::TextMediaType::getNewRuntime () {
    return new TextRuntime (this);
}

KDE_NO_EXPORT void SMIL::TextMediaType::accept (Visitor * v) {
    v->visit (this);
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT
SMIL::RefMediaType::RefMediaType (NodePtr & d)
    : SMIL::MediaType (d, "ref", id_node_ref) {}

KDE_NO_EXPORT TimedRuntime * SMIL::RefMediaType::getNewRuntime () {
    return new AudioVideoData (this); // FIXME check mimetype first
}

KDE_NO_EXPORT bool SMIL::RefMediaType::handleEvent (EventPtr event) {
    if (event->id () == event_sized && !external_tree)
        positionVideoWidget ();
    return MediaType::handleEvent (event);
}

KDE_NO_EXPORT void SMIL::RefMediaType::accept (Visitor * v) {
    v->visit (this);
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT SMIL::Brush::Brush (NodePtr & d)
    : SMIL::MediaType (d, "brush", id_node_brush) {}

KDE_NO_EXPORT void SMIL::Brush::accept (Visitor * v) {
    v->visit (this);
}

KDE_NO_EXPORT TimedRuntime * SMIL::Brush::getNewRuntime () {
    return new MediaTypeRuntime (this);
}

//-----------------------------------------------------------------------------

KDE_NO_EXPORT TimedRuntime * SMIL::Set::getNewRuntime () {
    return new SetData (this);
}

//-----------------------------------------------------------------------------

KDE_NO_EXPORT TimedRuntime * SMIL::Animate::getNewRuntime () {
    return new AnimateData (this);
}

KDE_NO_EXPORT bool SMIL::Animate::handleEvent (EventPtr event) {
    if (event->id () == event_timer) {
        TimerEvent * te = static_cast <TimerEvent *> (event.ptr ());
        if (te && te->timer_info && te->timer_info->event_id == anim_timer_id) {
            static_cast <AnimateData *> (timedRuntime ())->timerTick ();
            if (te->timer_info)
                te->interval = true;
            return true;
        }
    }
    return TimedMrl::handleEvent (event);
}

//-----------------------------------------------------------------------------

KDE_NO_EXPORT void SMIL::Param::activate () {
    setState (state_activated);
    QString name = getAttribute ("name");
    Node * parent = parentNode ().ptr ();
    if (!name.isEmpty () && parent && parent->isElementNode ())
        static_cast<Element*>(parent)->setParam (name, getAttribute ("value"));
    Element::activate (); //finish (); // no livetime of itself, will deactivate
}

//-----------------------------------------------------------------------------

KDE_NO_EXPORT void Visitor::visit (SMIL::Region * n) {
    visit (static_cast <SMIL::RegionBase *> (n));
}

KDE_NO_EXPORT void Visitor::visit (SMIL::Layout * n) {
    visit (static_cast <SMIL::RegionBase *> (n));
}

KDE_NO_EXPORT void Visitor::visit (SMIL::TimedMrl * n) {
    visit (static_cast <Element *> (n));
}

KDE_NO_EXPORT void Visitor::visit (SMIL::MediaType * n) {
    visit (static_cast <SMIL::TimedMrl *> (n));
}

KDE_NO_EXPORT void Visitor::visit (SMIL::ImageMediaType * n) {
    visit (static_cast <SMIL::MediaType *> (n));
}

KDE_NO_EXPORT void Visitor::visit (SMIL::TextMediaType * n) {
    visit (static_cast <SMIL::MediaType *> (n));
}

KDE_NO_EXPORT void Visitor::visit (SMIL::RefMediaType * n) {
    visit (static_cast <SMIL::MediaType *> (n));
}

KDE_NO_EXPORT void Visitor::visit (SMIL::AVMediaType * n) {
    visit (static_cast <SMIL::MediaType *> (n));
}

KDE_NO_EXPORT void Visitor::visit (SMIL::Brush * n) {
    visit (static_cast <SMIL::MediaType *> (n));
}

KDE_NO_EXPORT void Visitor::visit (SMIL::Anchor * n) {
    visit (static_cast <SMIL::LinkingBase *> (n));
}

KDE_NO_EXPORT void Visitor::visit (SMIL::Area * n) {
    visit (static_cast <SMIL::LinkingBase *> (n));
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT ImageRuntime::ImageRuntime (NodePtr e)
 : MediaTypeRuntime (e), img_movie (0L)
{}

KDE_NO_CDTOR_EXPORT ImageRuntime::~ImageRuntime () {
}

KDE_NO_EXPORT
bool ImageRuntime::parseParam (const QString & name, const QString & val) {
    //kdDebug () << "ImageRuntime::param " << name << "=" << val << endl;
    if (name == QString::fromLatin1 ("src")) {
        killWGet ();
        NodePtr element_protect = element;
        SMIL::MediaType * mt = convertNode <SMIL::MediaType> (element);
        if (!mt)
            return false; // can not happen
        if (mt->external_tree)
            mt->removeChild (mt->external_tree);
        mt->src = val;
        if (!val.isEmpty ()) {
            QString abs = mt->absolutePath ();
            cached_img.setUrl (abs);
            if (cached_img.data->isEmpty ())
                wget (abs);
        }
    } else
        return MediaTypeRuntime::parseParam (name, val);
    return true;
}

/**
 * start_timer timer expired, repaint if we have an image
 */
KDE_NO_EXPORT void ImageRuntime::started () {
    if (element && downloading ()) {
        postpone_lock = element->document ()->postpone ();
        return;
    }
    if (durations [duration_time].durval == 0 &&
            durations [end_time].durval == duration_media) //no duration/end set
        fill = fill_freeze;
    MediaTypeRuntime::started ();
}

KDE_NO_EXPORT void ImageRuntime::clipStart () {
    if (img_movie) {
        img_movie->restart ();
        if (img_movie->paused ())
            img_movie->unpause ();
    }
    MediaTypeRuntime::clipStart ();
}

KDE_NO_EXPORT void ImageRuntime::clipStop () {
    if (img_movie && frame_nr)
        img_movie->pause ();
    MediaTypeRuntime::clipStop ();
}

KDE_NO_EXPORT void ImageRuntime::remoteReady (QByteArray & data) {
    NodePtr element_protect = element; // note element is weak
    SMIL::MediaType * mt = convertNode <SMIL::MediaType> (element);
    if (data.size () && mt) {
        QString mime = mimetype ();
        kdDebug () << "ImageRuntime::remoteReady " << mime << " " << data.size () << " empty:" << cached_img.data->isEmpty () << endl;
        if (mime.startsWith (QString::fromLatin1 ("text/"))) {
            QTextStream ts (data, IO_ReadOnly);
            readXML (element, ts, QString::null);
            mt->external_tree = findExternalTree (element);
        }
        if (!mt->external_tree && cached_img.data->isEmpty ()) {
            QImage *pix = new QImage (data);
            if (!pix->isNull ()) {
                cached_img.data->image = pix;
                delete img_movie;
                img_movie = new QMovie (data);
                frame_nr = 0;
                img_movie->connectUpdate(this, SLOT(movieUpdated(const QRect&)));
                img_movie->connectStatus (this, SLOT (movieStatus (int)));
                img_movie->connectResize(this, SLOT (movieResize(const QSize&)));
                if (mt->region_node && (timingstate == timings_started ||
                            (timingstate == timings_stopped && fill == fill_freeze)))
                    convertNode <SMIL::RegionBase> (mt->region_node)->repaint();
            } else
                delete pix;
        }
    }
    postpone_lock = 0L;
    if (timingstate == timings_started)
        started ();
}

KDE_NO_EXPORT void ImageRuntime::movieUpdated (const QRect &) {
    if (frame_nr++) {
        SMIL::MediaType * mt = convertNode <SMIL::MediaType> (element);
        if (mt && mt->region_node && (timingstate == timings_started ||
                    (timingstate == timings_stopped && fill == fill_freeze))) {
            cached_img.setUrl (QString ());
            ASSERT (cached_img.data && cached_img.data->isEmpty ());
            cached_img.data->image = new QImage;
            *cached_img.data->image = (img_movie->framePixmap ());
            convertNode <SMIL::RegionBase> (mt->region_node)->repaint ();
        }
    }
    if (timingstate != timings_started && img_movie)
        img_movie->pause ();
}

KDE_NO_EXPORT void ImageRuntime::movieStatus (int s) {
    if (timingstate == timings_started ||
                (timingstate == timings_stopped && fill == fill_freeze)) {
        if (s == QMovie::EndOfMovie)
            propagateStop (false);
    }
}

KDE_NO_EXPORT void ImageRuntime::movieResize (const QSize &) {
    SMIL::MediaType * mt = convertNode <SMIL::MediaType> (element);
    if (mt && mt->region_node && (timingstate == timings_started ||
                (timingstate == timings_stopped && fill == fill_freeze))) {
        convertNode <SMIL::RegionBase> (mt->region_node)->repaint ();
        //kdDebug () << "movieResize" << endl;
    }
}

KDE_NO_EXPORT void ImageRuntime::postpone (bool b) {
    kdDebug () << "ImageRuntime::postpone " << b << endl;
    if (img_movie) {
        if (!img_movie->paused () && b)
            img_movie->pause ();
        else if (img_movie->paused () && !b)
            img_movie->unpause ();
    }
}

//-----------------------------------------------------------------------------

namespace KMPlayer {
    class TextRuntimePrivate {
    public:
        TextRuntimePrivate () {
            reset ();
        }
        void reset () {
            codec = 0L;
            font = QApplication::font ();
            data.truncate (0);
        }
        QByteArray data;
        QTextCodec * codec;
        QFont font;
    };
}

KDE_NO_CDTOR_EXPORT TextRuntime::TextRuntime (NodePtr e)
 : MediaTypeRuntime (e), d (new TextRuntimePrivate) {
    reset ();
}

KDE_NO_CDTOR_EXPORT TextRuntime::~TextRuntime () {
    delete d;
}

KDE_NO_EXPORT void TextRuntime::reset () {
    d->reset ();
    font_size = d->font.pointSize ();
    font_color = 0;
    background_color = 0xffffff;
    transparent = false;
    MediaTypeRuntime::reset ();
}

KDE_NO_EXPORT
bool TextRuntime::parseParam (const QString & name, const QString & val) {
    //kdDebug () << "TextRuntime::parseParam " << name << "=" << val << endl;
    SMIL::MediaType * mt = convertNode <SMIL::MediaType> (element);
    if (!mt)
        return false; // cannot happen
    const char * cname = name.ascii ();
    if (!strcmp (cname, "src")) {
        killWGet ();
        mt->src = val;
        d->data.resize (0);
        if (!val.isEmpty ())
            wget (mt->absolutePath ());
        return true;
    }
    if (!strcmp (cname, "backgroundColor")) {
        background_color = QColor (val).rgb ();
    } else if (!strcmp (cname, "fontColor")) {
        font_color = QColor (val).rgb ();
    } else if (!strcmp (cname, "charset")) {
        d->codec = QTextCodec::codecForName (val.ascii ());
    } else if (!strcmp (cname, "fontFace")) {
        ; //FIXME
    } else if (!strcmp (cname, "fontPtSize")) {
        font_size = val.toInt ();
    } else if (!strcmp (cname, "fontSize")) {
        font_size += val.toInt ();
    // TODO: expandTabs fontBackgroundColor fontSize fontStyle fontWeight hAlig vAlign wordWrap
    } else
        return MediaTypeRuntime::parseParam (name, val);
    if (mt->region_node && (timingstate == timings_started ||
                (timingstate == timings_stopped && fill == fill_freeze)))
        convertNode <SMIL::RegionBase> (mt->region_node)->repaint ();
    return true;
}

/**
 * start_timer timer expired, repaint if we have text
 */
KDE_NO_EXPORT void TextRuntime::started () {
    if (element && downloading ()) {
        postpone_lock = element->document ()->postpone ();
        return;
    }
    MediaTypeRuntime::started ();
}

KDE_NO_EXPORT void TextRuntime::remoteReady (QByteArray & data) {
    QString str (data);
    if (data.size () && element) {
        d->data = data;
        if (d->data.size () > 0 && !d->data [d->data.size () - 1])
            d->data.resize (d->data.size () - 1); // strip zero terminate char
        QTextStream ts (d->data, IO_ReadOnly);
        if (d->codec)
            ts.setCodec (d->codec);
        text  = ts.read ();
        SMIL::MediaType * mt = convertNode <SMIL::MediaType> (element);
        if (mt && mt->region_node && (timingstate == timings_started ||
                    (timingstate == timings_stopped && fill == fill_freeze)))
            convertNode <SMIL::RegionBase> (mt->region_node)->repaint ();
    }
    postpone_lock = 0L;
    if (timingstate == timings_started)
        started ();
}

//-----------------------------------------------------------------------------

#include "kmplayer_smil.moc"
