/**
 * Copyright (C) 2005-2006 by Koos Vriezen <koos.vriezen@gmail.com>
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
#include <qmap.h>

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
static const unsigned int event_inbounds = duration_element_inbounds;
static const unsigned int event_outbounds = duration_element_outbounds;
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

//-----------------------------------------------------------------------------

ElementRuntime * Node::getRuntime () {
    static ElementRuntime runtime (0L);
    kdWarning () << nodeName () << " no runtime available" << endl;
    return &runtime;
}

//-----------------------------------------------------------------------------

namespace KMPlayer {
    struct ParamValue {
        QString val;
        QStringList  * modifications;
        ParamValue (const QString & v) : val (v), modifications (0L) {}
        ~ParamValue () { delete modifications; }
        QString value () { return modifications && modifications->size () ? modifications->back () : val; }
        void setValue (const QString & v) { val = v; }
    };
    class ElementRuntimePrivate {
    public:
        ~ElementRuntimePrivate ();
        QMap <QString, ParamValue *> params;
        void clear ();
    };
}

KDE_NO_CDTOR_EXPORT ElementRuntimePrivate::~ElementRuntimePrivate () {
    clear ();
}

KDE_NO_EXPORT void ElementRuntimePrivate::clear () {
    const QMap <QString, ParamValue *>::iterator e = params.end ();
    for (QMap <QString, ParamValue*>::iterator i = params.begin (); i != e; ++i)
        delete i.data ();
    params.clear ();
}

ElementRuntime::ElementRuntime (NodePtr e)
  : element (e), d (new ElementRuntimePrivate) {}

ElementRuntime::~ElementRuntime () {
    delete d;
}

QString ElementRuntime::setParam (const QString & name, const QString & value, int * id) {
    ParamValue * pv = d->params [name];
    QString old_val;
    if (pv)
        old_val = pv->value ();
    else {
        pv = new ParamValue (id ? QString::null : value);
        d->params.insert (name, pv);
    }
    if (id) {
        if (!pv->modifications)
            pv->modifications = new QStringList;
        if (*id >= 0 && *id < int (pv->modifications->size ())) {
            (*pv->modifications) [*id] = value;
        } else {
            *id = pv->modifications->size ();
            pv->modifications->push_back (value);
        }
    } else
        pv->setValue (value);
    parseParam (name, value);
    return old_val;
}

QString ElementRuntime::param (const QString & name) {
    ParamValue * pv = d->params [name];
    if (pv)
        return pv->value ();
    return QString::null;
}

void ElementRuntime::resetParam (const QString & name, int id) {
    ParamValue * pv = d->params [name];
    if (pv && pv->modifications) {
        if (int (pv->modifications->size ()) > id && id > -1) {
            (*pv->modifications) [id] = QString::null;
            while (pv->modifications->size () > 0 &&
                    pv->modifications->back ().isNull ())
                pv->modifications->pop_back ();
        }
        QString val = pv->value ();
        if (pv->modifications->size () == 0) {
            delete pv->modifications;
            pv->modifications = 0L;
            val = pv->value ();
            if (val.isNull ()) {
                delete pv;
                d->params.remove (name);
            }
        }
        parseParam (name, val);
    } else
        kdError () << "resetting " << name << " that doesn't exists" << endl;
}

KDE_NO_EXPORT void ElementRuntime::init () {
    reset ();
    if (element && element->isElementNode ()) {
        for (AttributePtr a= convertNode <Element> (element)->attributes ()->first (); a; a = a->nextSibling ())
            setParam (QString (a->nodeName ()), a->nodeValue ());
    }
}

KDE_NO_EXPORT void ElementRuntime::reset () {
    d->clear ();
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT ToBeStartedEvent::ToBeStartedEvent (NodePtr n)
 : Event (event_to_be_started), node (n) {}

KDE_NO_CDTOR_EXPORT SizeEvent::SizeEvent (Single a, Single b, Single c, Single d
        , Fit f)
 : Event (event_sized), x (a), y (b), w (c), h (d), fit (f) {}

PointerEvent::PointerEvent (unsigned int event_id, int _x, int _y)
 : Event (event_id), x (_x), y (_y) {}

TimerEvent::TimerEvent (TimerInfoPtr tinfo)
 : Event (event_timer), timer_info (tinfo), interval (false) {}

PostponedEvent::PostponedEvent (bool postponed)
 : Event (event_postponed), is_postponed (postponed) {}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT TimedRuntime::TimedRuntime (NodePtr e)
 : ElementRuntime (e) {
    reset ();
}

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
    ElementRuntime::reset ();
}

KDE_NO_EXPORT
void TimedRuntime::setDurationItem (DurationTime item, const QString & val) {
    unsigned int dur = 0; // also 0 for 'media' duration, so it will not update then
    QString vl = val.lower ();
    parseTime (vl, dur);
    if (!dur && element) {
        int pos = vl.find (QChar ('.'));
        if (pos > 0) {
            NodePtr e = element->document()->getElementById (vl.left(pos));
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
        end ();
        return;
    }
    //kdDebug () << "TimedRuntime::begin " << element->nodeName() << endl; 
    if (start_timer || dur_timer) {
        reset ();
        init ();
    }
    timingstate = timings_began;

    if (durations [begin_time].durval > 0) {
        if (durations [begin_time].durval < duration_last_option)
            start_timer = element->document ()->setTimeout (element, 100 * durations [begin_time].durval, start_timer_id);
        else
            propagateStop (false);
    } else
        propagateStart ();
}

/**
 * forced killing of timers
 */
KDE_NO_EXPORT void TimedRuntime::end () {
    //kdDebug () << "TimedRuntime::end " << (element ? element->nodeName() : "-") << endl; 
    reset ();
}

/**
 * change behaviour of this runtime, returns old value
 */
KDE_NO_EXPORT
void TimedRuntime::parseParam (const QString & name, const QString & val) {
    //kdDebug () << "TimedRuntime::parseParam " << name << "=" << val << endl;
    if (name == QString::fromLatin1 ("begin")) {
        setDurationItem (begin_time, val);
        if ((timingstate == timings_began && !start_timer) ||
                timingstate == timings_stopped) {
            if (durations [begin_time].durval > 0) { // create a timer for start
                if (durations [begin_time].durval < duration_last_option)
                    start_timer = element->document ()->setTimeout (element, 100 * durations [begin_time].durval, start_timer_id);
            } else                                // start now
                propagateStart ();
        }
    } else if (name == QString::fromLatin1 ("dur")) {
        setDurationItem (duration_time, val);
    } else if (name == QString::fromLatin1 ("end")) {
        setDurationItem (end_time, val);
        if (durations [end_time].durval < duration_last_option &&
            durations [end_time].durval > durations [begin_time].durval)
            durations [duration_time].durval =
                durations [end_time].durval - durations [begin_time].durval;
        else if (durations [end_time].durval > duration_last_option)
            durations [duration_time].durval = duration_media; // event
    } else if (name == QString::fromLatin1 ("endsync")) {
        if ((durations [duration_time].durval == duration_media ||
                    durations [duration_time].durval == 0) &&
                durations [end_time].durval == duration_media) {
            NodePtr e = element->document ()->getElementById (val);
            if (e && SMIL::isTimedMrl (e)) {
                SMIL::TimedMrl * tm = static_cast <SMIL::TimedMrl *> (e.ptr ());
                durations [(int) end_time].connection = tm->connectTo (element, event_stopped);
                durations [(int) end_time].durval = event_stopped;
            }
        }
    } else if (name == QString::fromLatin1 ("fill")) {
        if (val == QString::fromLatin1 ("freeze"))
            fill = fill_freeze;
        else
            fill = fill_unknown;
        // else all other fill options ..
    } else if (name == QString::fromLatin1 ("repeatCount")) {
        if (val.find ("indefinite") > -1)
            repeat_count = duration_infinite;
        else
            repeat_count = val.toInt ();
    } else if (name == QString::fromLatin1 ("title")) {
        Mrl * mrl = static_cast <Mrl *> (element.ptr ());
        if (mrl)
            mrl->pretty_name = val;
    }
    ElementRuntime::parseParam (name, val);
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
        end ();
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
        while (node && node->id != SMIL::id_node_smil)
            node = node->parentNode ().ptr ();
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
                static_cast <RegPointRuntime*> (c->getRuntime ())->sizes.calcSizes (0L, 100, 100, rpx, rpy, i1, i2);
                QString ra = convertNode <Element> (c)->getAttribute ("regAlign");
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
    if (name == QString::fromLatin1 ("left")) {
        left = val;
    } else if (name == QString::fromLatin1 ("top")) {
        top = val;
    } else if (name == QString::fromLatin1 ("width")) {
        width = val;
    } else if (name == QString::fromLatin1 ("height")) {
        height = val;
    } else if (name == QString::fromLatin1 ("right")) {
        right = val;
    } else if (name == QString::fromLatin1 ("bottom")) {
        bottom = val;
    } else if (name == QString::fromLatin1 ("regPoint")) {
        reg_point = val;
    } else if (name == QString::fromLatin1 ("regAlign")) {
        reg_align = val;
    } else
        return false;
    return true;
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT RegionRuntime::RegionRuntime (NodePtr e)
 : ElementRuntime (e) {
    init ();
}

KDE_NO_EXPORT void RegionRuntime::reset () {
    // Keep region_node, so no ElementRuntime::reset (); or set it back again
    active = false;
    sizes.resetSizes ();
}

KDE_NO_EXPORT
void RegionRuntime::parseParam (const QString & name, const QString & val) {
    //kdDebug () << "RegionRuntime::parseParam " << convertNode <Element> (element)->getAttribute ("id") << " " << name << "=" << val << endl;
    SMIL::RegionBase * rb = convertNode <SMIL::RegionBase> (element);
    SRect rect;
    bool need_repaint = false;
    if (rb)
        rect = SRect (rb->x, rb->y, rb->w, rb->h);
    if (name == QString::fromLatin1 ("background-color") ||
            name == QString::fromLatin1 ("backgroundColor")) {
        rb->background_color = 0xff000000 | QColor (val).rgb ();
        if (rb->surface)
            rb->surface->background_color = rb->background_color;
        need_repaint = true;
    } else if (name == QString::fromLatin1 ("z-index")) {
        if (rb)
            rb->z_order = val.toInt ();
        need_repaint = true;
    } else if (sizes.setSizeParam (name, val)) {
        if (active && rb && rb->surface && element) {
            NodePtr p = rb->parentNode ();
            if (p &&(p->id==SMIL::id_node_region ||p->id==SMIL::id_node_layout))
                convertNode <SMIL::RegionBase> (p)->updateDimensions (0L);
            //if (rect.width () == rw && rect.height () == rh) {
            //    PlayListNotify * n = element->document()->notify_listener;
            //    if (n && (rect.x () != rx || rect.y () != ry))
            //        n->moveRect (rect.x(), rect.y(), rect.width (), rect.height (), rx, ry);
            //} else {
                rect = rect.unite (SRect (rb->x, rb->y, rb->w, rb->h));
                need_repaint = true;
            //}
        }
    }
    if (need_repaint && active && rb && element && rb->surface->parentNode ())
        rb->surface->parentNode ()->repaint (rect.x(), rect.y(), rect.width(), rect.height());
    ElementRuntime::parseParam (name, val);
}

KDE_NO_EXPORT void RegionRuntime::begin () {
    active = true;
    ElementRuntime::begin ();
}

KDE_NO_EXPORT void RegionRuntime::end () {
    reset ();
    ElementRuntime::end ();
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT RegPointRuntime::RegPointRuntime (NodePtr e)
    : ElementRuntime (e) {}

KDE_NO_EXPORT
void RegPointRuntime::parseParam (const QString & name, const QString & val) {
    sizes.setSizeParam (name, val); // TODO: if dynamic, make sure to repaint
    ElementRuntime::parseParam (name, val);
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT AnimateGroupData::AnimateGroupData (NodePtr e)
 : TimedRuntime (e), modification_id (-1) {}

void AnimateGroupData::parseParam (const QString & name, const QString & val) {
    //kdDebug () << "AnimateGroupData::parseParam " << name << "=" << val << endl;
    if (name == QString::fromLatin1 ("target") ||
            name == QString::fromLatin1 ("targetElement")) {
        if (element)
            target_element = element->document ()->getElementById (val);
    } else if (name == QString::fromLatin1 ("attribute") ||
            name == QString::fromLatin1 ("attributeName")) {
        changed_attribute = val;
    } else if (name == QString::fromLatin1 ("to")) {
        change_to = val;
    }
    TimedRuntime::parseParam (name, val);
}

/**
 * animation finished
 */
KDE_NO_EXPORT void AnimateGroupData::stopped () {
    //kdDebug () << "AnimateGroupData::stopped " << durations [duration_time].durval << endl;
    if (fill != fill_freeze)
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
        //kdDebug () << "AnimateGroupData::restoreModificatio " <<modification_id << endl;
        target_element->getRuntime ()->resetParam (changed_attribute, modification_id);
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
            ElementRuntime * rt = target_element->getRuntime ();
            rt->setParam (changed_attribute, change_to, &modification_id);
            //kdDebug () << "SetData::started " << target_element->nodeName () << "." << changed_attribute << " " << old_value << "->" << change_to << endl;
        } else
            kdWarning () << "target element not found" << endl;
    } else
        kdWarning () << "set element disappeared" << endl;
    AnimateGroupData::started ();
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT AnimateData::AnimateData (NodePtr e)
 : AnimateGroupData (e) {
    reset ();
}

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

void AnimateData::parseParam (const QString & name, const QString & val) {
    //kdDebug () << "AnimateData::parseParam " << name << "=" << val << endl;
    if (name == QString::fromLatin1 ("change_by")) {
        change_by = val.toInt ();
    } else if (name == QString::fromLatin1 ("from")) {
        change_from = val;
    } else if (name == QString::fromLatin1 ("values")) {
        change_values = QStringList::split (QString (";"), val);
    } else if (name.lower () == QString::fromLatin1 ("calcmode")) {
        if (val == QString::fromLatin1 ("discrete"))
            calcMode = calc_discrete;
        else if (val == QString::fromLatin1 ("linear"))
            calcMode = calc_linear;
        else if (val == QString::fromLatin1 ("paced"))
            calcMode = calc_paced;
    } else {
        AnimateGroupData::parseParam (name, val);
        return;
    }
    ElementRuntime::parseParam (name, val);
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
        if (!target_element) {
            kdWarning () << "target element not found" << endl;
            break;
        }
        ElementRuntime * rt = target_element->getRuntime ();
        if (calcMode == calc_linear) {
            QRegExp reg ("^\\s*([0-9\\.]+)(\\s*[%a-z]*)?");
            if (change_from.isEmpty ()) {
                if (change_values.size () > 0) // check 'values' attribute
                     change_from = change_values.first ();
                else
                    change_from = rt->param (changed_attribute); // take current
            }
            if (!change_from.isEmpty ()) {
                rt->setParam (changed_attribute, change_from, &modification_id);
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
            steps = 10 * durations [duration_time].durval / 4; // 25 per sec
            if (steps > 0) {
                anim_timer = element->document ()->setTimeout (element, 40, anim_timer_id); // 25 /s for now FIXME
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
            rt->setParam (changed_attribute, change_values.first (), &modification_id);
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
    if (steps-- > 0 && target_element) {
        if (calcMode == calc_linear) {
            change_from_val += change_delta;
            target_element->getRuntime ()->setParam (changed_attribute, QString ("%1%2").arg (change_from_val).arg(change_from_unit), &modification_id);
        } else if (calcMode == calc_discrete) {
            target_element->getRuntime ()->setParam (changed_attribute, change_values[change_values.size () - steps -1], &modification_id);
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
    /* If this mediatype pointed to a playlist, activate it */
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
KDE_NO_EXPORT void KMPlayer::MediaTypeRuntime::end () {
    clear ();
    postpone_lock = 0L;
    TimedRuntime::end ();
}

/**
 * re-implement for regions and src attributes
 */
KDE_NO_EXPORT
void MediaTypeRuntime::parseParam (const QString & name, const QString & val) {
    SMIL::MediaType * mt = convertNode <SMIL::MediaType> (element);
    if (!mt)
        return;
    if (name == QString::fromLatin1 ("src")) {
        mt->src = val;
    } else if (name == QString::fromLatin1 ("fit")) {
        if (val == QString::fromLatin1 ("fill"))
            fit = fit_fill;
        else if (val == QString::fromLatin1 ("hidden"))
            fit = fit_hidden;
        else if (val == QString::fromLatin1 ("meet"))
            fit = fit_meet;
        else if (val == QString::fromLatin1 ("scroll"))
            fit = fit_scroll;
        else if (val == QString::fromLatin1 ("slice"))
            fit = fit_slice;
    } else if (!sizes.setSizeParam (name, val)) {
        TimedRuntime::parseParam (name, val);
        return;
    }
    SMIL::RegionBase * rb = convertNode <SMIL::RegionBase> (mt->region_node);
    if ((state () == timings_began ||
                (state () == TimedRuntime::timings_stopped &&
                 fill == TimedRuntime::fill_freeze)) &&
            rb && element)
        rb->repaint ();
    ElementRuntime::parseParam (name, val);
}

/**
 * will request a repaint of attached region
 */
KDE_NO_EXPORT void MediaTypeRuntime::stopped () {
    Node * e = element.ptr ();
    if (e) {
        for (NodePtr n = e->firstChild (); n; n = n->nextSibling ())
            if (n->unfinished ())   // finish child documents
                n->finish ();
    }
    TimedRuntime::stopped ();
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
    if (durations [duration_time].durval == 0 &&
            durations [end_time].durval == duration_media)
        durations [duration_time].durval = duration_media; // duration of clip
    MediaTypeRuntime::started ();
    NodePtr element_protect = element; // note element is weak
    SMIL::MediaType * mt = convertNode <SMIL::MediaType> (element);
    if (mt && mt->region_node && !mt->external_tree) {
        //kdDebug () << "AudioVideoData::started " << mt->absolutePath()<< endl;
        for (NodePtr p = mt->parentNode (); p; p = p->parentNode ())
            if (p->id == SMIL::id_node_smil) {
                // this works only because we can only play one at a time FIXME
                convertNode <SMIL::Smil> (p)->current_av_media_type = element;
                break;
            }
        if (!mt->src.isEmpty ()) {
            mt->positionVideoWidget ();
            if (mt->state != Element::state_deferred) {
                PlayListNotify * n = mt->document ()->notify_listener;
                if (n)
                    n->requestPlayURL (element);
                document_postponed = element->document()->connectTo (element, event_postponed);
                mt->setState (Element::state_began);
            }
        }
    }
}

KDE_NO_EXPORT void AudioVideoData::stopped () {
    //kdDebug () << "AudioVideoData::stopped " << endl;
    avStopped ();
    MediaTypeRuntime::stopped ();
    document_postponed = 0L;
}

KDE_NO_EXPORT void AudioVideoData::avStopped () {
    if (durations [duration_time].durval == duration_media)
        durations [duration_time].durval = 0; // reset to make this finish
}

KDE_NO_EXPORT
void AudioVideoData::parseParam (const QString & name, const QString & val) {
    //kdDebug () << "AudioVideoData::parseParam " << name << "=" << val << endl;
    if (name == QString::fromLatin1 ("src")) {
        NodePtr element_protect = element; // note element is weak
        SMIL::MediaType * mt = convertNode <SMIL::MediaType> (element);
        if (mt) {
            if (!mt->resolved || mt->src != val) {
                //kdDebug () << "AudioVideoData::parseParam remove " << (mt->external_tree ? mt->external_tree->nodeName() : "null") << endl;
                if (mt->external_tree)
                    mt->removeChild (mt->external_tree);
                mt->src = val;
                mt->resolved = mt->document ()->notify_listener->resolveURL (element);
                if (mt->resolved) // update external_tree here 
                    mt->external_tree = findExternalTree (element);
            }
            if (timingstate == timings_started) {
                if (!mt->resolved) {
                    mt->defer ();
                } else if (!mt->external_tree) {
                    PlayListNotify * n = mt->document ()->notify_listener;
                    if (n && !val.isEmpty ()) {
                        n->requestPlayURL (element);
                        mt->setState (Node::state_began);
                        document_postponed = element->document()->connectTo (element, event_postponed);
                    }
                } else
                    mt->external_tree->activate ();
            }
        }
    } else
        MediaTypeRuntime::parseParam (name, val);
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
    if (!strcmp (tag.latin1 (), "par"))
        return new SMIL::Par (d);
    else if (!strcmp (tag.latin1 (), "seq"))
        return new SMIL::Seq (d);
    else if (!strcmp (tag.latin1 (), "excl"))
        return new SMIL::Excl (d);
    return 0L;
}

static Element * fromParamGroup (NodePtr & d, const QString & tag) {
    if (!strcmp (tag.latin1 (), "param"))
        return new SMIL::Param (d);
    return 0L;
}

static Element * fromAnimateGroup (NodePtr & d, const QString & tag) {
    if (!strcmp (tag.latin1 (), "set"))
        return new SMIL::Set (d);
    else if (!strcmp (tag.latin1 (), "animate"))
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
    // animation, textstream, ref, brush
    return 0L;
}

static Element * fromContentControlGroup (NodePtr & d, const QString & tag) {
    if (!strcmp (tag.latin1 (), "switch"))
        return new SMIL::Switch (d);
    return 0L;
}

//-----------------------------------------------------------------------------

KDE_NO_EXPORT NodePtr SMIL::Smil::childFromTag (const QString & tag) {
    if (!strcmp (tag.latin1 (), "body"))
        return new SMIL::Body (m_doc);
    else if (!strcmp (tag.latin1 (), "head"))
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

// FIXME: this should be through the deactivate() calls
static void endLayout (Node * node) {
    if (node) { // note rb can be a Region/Layout/RootLayout object
        node->getRuntime ()->end ();
        for (NodePtr c = node->firstChild (); c; c = c->nextSibling ())
            endLayout (c.ptr ());
    }
}

KDE_NO_EXPORT void SMIL::Smil::deactivate () {
    endLayout (layout_node.ptr ());
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

//-----------------------------------------------------------------------------

KDE_NO_EXPORT NodePtr SMIL::Head::childFromTag (const QString & tag) {
    if (!strcmp (tag.latin1 (), "layout"))
        return new SMIL::Layout (m_doc);
    else if (!strcmp (tag.latin1 (), "title"))
        return new DarkNode (m_doc, tag, id_node_title);
    else if (!strcmp (tag.latin1 (), "meta"))
        return new DarkNode (m_doc, tag, id_node_meta);
    else if (!strcmp (tag.latin1 (), "transition"))
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

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT SMIL::Layout::Layout (NodePtr & d)
 : RegionBase (d, id_node_layout) {}

KDE_NO_EXPORT NodePtr SMIL::Layout::childFromTag (const QString & tag) {
    if (!strcmp (tag.latin1 (), "root-layout")) {
        NodePtr e = new SMIL::RootLayout (m_doc);
        rootLayout = e;
        return e;
    } else if (!strcmp (tag.latin1 (), "region"))
        return new SMIL::Region (m_doc);
    else if (!strcmp (tag.latin1 (), "regPoint"))
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
                static_cast <RegionRuntime *> (rb->getRuntime ())->init ();
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
    x = y = 0;
    RegionRuntime *rr = static_cast<RegionRuntime*> (rootLayout->getRuntime ());
    w = rr->sizes.width.size ();
    h = rr->sizes.height.size ();
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
                rl->setAttribute (QString::fromLatin1 ("width"), QString::number ((int)ew));
                rl->setAttribute (QString::fromLatin1 ("height"), QString::number ((int)eh));
                if (runtime) {
                    rl->getRuntime ()->setParam (QString::fromLatin1 ("width"), QString::number ((int)ew));
                    rl->getRuntime ()->setParam (QString::fromLatin1 ("height"), QString::number ((int)eh));
                    updateDimensions (surface);
                }
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
        case event_pointer_clicked:
        case event_pointer_moved:
            for (NodePtr r = firstChild (); r; r = r->nextSibling ())
                handled |= r->handleEvent (event);
            break;
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
   z_order (1), background_color (0), runtime (0L),
   m_SizeListeners (new NodeRefList),
   m_PaintListeners (new NodeRefList) {}

KDE_NO_CDTOR_EXPORT SMIL::RegionBase::~RegionBase () {
    delete runtime;
}

KDE_NO_EXPORT ElementRuntime * SMIL::RegionBase::getRuntime () {
    if (!runtime)
        runtime = new RegionRuntime (this);
    return runtime;
}

KDE_NO_EXPORT void SMIL::RegionBase::activate () {
    setState (state_activated);
    ElementRuntime * rt = getRuntime ();
    rt->init ();
    rt->begin ();
    for (NodePtr r = firstChild (); r; r = r->nextSibling ())
        if (r->id == id_node_region || r->id == id_node_root_layout)
            r->activate ();
}

KDE_NO_EXPORT void SMIL::RegionBase::reset () {
    Element::reset ();
    background_color = 0;
    if (surface)
        surface->background_color = 0;
    delete runtime;
    runtime = 0L;
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

NodeRefListPtr SMIL::RegionBase::listeners (unsigned int eid) {
    if (eid == event_paint)
        return m_PaintListeners;
    else if (eid == event_sized)
        return m_SizeListeners;
    return Element::listeners (eid);
}

KDE_NO_CDTOR_EXPORT SMIL::Region::Region (NodePtr & d)
 : RegionBase (d, id_node_region),
   m_ActionListeners (new NodeRefList),
   m_OutOfBoundsListeners (new NodeRefList),
   m_InBoundsListeners (new NodeRefList),
   has_mouse (false) {}

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
    static_cast <RegionRuntime *> (getRuntime ())->sizes.calcSizes (this, pw, ph, x, y, w, h);
    if (x1 != x || y1 != y || w1 != w || h1 != h) {
        propagateEvent (new SizeEvent (0, 0, w, h, fit_meet));
    }
    if (surface)
        surface->bounds = SRect (x, y, w, h);
    //kdDebug () << "Region::calculateBounds parent:" << pw << "x" << ph << " this:" << x << "," << y << " " << w << "x" << h << endl;
}

bool SMIL::Region::handleEvent (EventPtr event) {
    Single rx = 0, ry = 0, rw = w, rh = h;
    if (surface)
        surface->toScreen (rx, ry, rw, rh);
    switch (event->id ()) {
        case event_pointer_clicked: {
            PointerEvent * e = static_cast <PointerEvent *> (event.ptr ());
            bool inside = e->x > rx && e->x < rx+rw && e->y > ry && e->y< ry+rh;
            if (!inside)
                return false;
            bool handled = false;
            for (NodePtr r = firstChild (); r; r = r->nextSibling ())
                handled |= r->handleEvent (event);
            if (!handled) // handle it ..
                propagateEvent (event);
            return inside;
        }
        case event_pointer_moved: {
            PointerEvent * e = static_cast <PointerEvent *> (event.ptr ());
            bool inside = e->x > rx && e->x < rx+rw && e->y > ry && e->y< ry+rh;
            bool handled = false;
            if (inside)
                for (NodePtr r = firstChild (); r; r = r->nextSibling ())
                    handled |= r->handleEvent (event);
            EventPtr evt;
            if (has_mouse && (!inside || handled)) { // OutOfBoundsEvent
                has_mouse = false;
                evt = new Event (event_outbounds);
            } else if (inside && !handled && !has_mouse) { // InBoundsEvent
                has_mouse = true;
                evt = new Event (event_inbounds);
            }
            if (evt)
                propagateEvent (evt);
            return inside;
        }
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

KDE_NO_CDTOR_EXPORT SMIL::RegPoint::~RegPoint () {
    delete runtime;
}

KDE_NO_EXPORT ElementRuntime * SMIL::RegPoint::getRuntime () {
    if (!runtime)
        runtime = new RegPointRuntime (this);
    return runtime;
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT SMIL::Transition::~Transition () {
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

KDE_NO_EXPORT void SMIL::TimedMrl::activate () {
    //kdDebug () << "SMIL::TimedMrl(" << nodeName() << ")::activate" << endl;
    setState (state_activated);
    TimedRuntime * rt = timedRuntime ();
    rt->init ();
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
        runtime->end ();
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

KDE_NO_EXPORT ElementRuntime * SMIL::TimedMrl::getRuntime () {
    if (!runtime)
        runtime = getNewRuntime ();
    return runtime;
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
    if (firstChild ())
        firstChild ()->activate ();
    GroupBase::begin ();
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

KDE_NO_CDTOR_EXPORT SMIL::MediaType::MediaType (NodePtr &d, const QString &t, short id)
 : TimedMrl (d, id), m_type (t), bitrate (0),
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

KDE_NO_EXPORT void SMIL::MediaType::opened () {
    for (AttributePtr a = m_attributes->first (); a; a = a->nextSibling ()) {
        const char * cname = a->nodeName ();
        if (!strcmp (cname, "system-bitrate"))
            bitrate = a->nodeValue ().toInt ();
        else if (!strcmp (cname, "src"))
            src = a->nodeValue ();
        else if (!strcmp (cname, "type"))
            mimetype = a->nodeValue ();
    }
}

KDE_NO_EXPORT void SMIL::MediaType::activate () {
    setState (state_activated);
    timedRuntime ()->init (); // sets all attributes
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
    TimedMrl::deactivate ();
}

KDE_NO_EXPORT void SMIL::MediaType::begin () {
    NodePtr e = parentNode ();
    for (; e; e = e->parentNode ())
        if (e->id == SMIL::id_node_smil) {
            e = convertNode <SMIL::Smil> (e)->layout_node;
            break;
        }
    if (e) {
        MediaTypeRuntime *tr = static_cast<MediaTypeRuntime*>(timedRuntime ());
        SMIL::Region * r = findRegion (e, tr->param(QString::fromLatin1("region")));
        if (r) {
            region_node = r;
            region_sized = r->connectTo (this, event_sized);
            region_paint = r->connectTo (this, event_paint);
            region_mouse_enter = r->connectTo (this, event_inbounds);
            region_mouse_leave = r->connectTo (this, event_outbounds);
            region_mouse_click = r->connectTo (this, event_activated);
            for (NodePtr n = firstChild (); n; n = n->nextSibling ())
                switch (n->id) {
                    case SMIL::id_node_smil:   // support nested documents
                    case RP::id_node_imfl:     // by giving a dimension
                        n->handleEvent(new SizeEvent(0,0, r->w, r->h, tr->fit));
                        n->activate ();
                }
            r->repaint ();
        } else
            kdWarning () << "MediaType::begin no region found" << endl;

        if (external_tree)
            external_tree->activate ();
    }
    TimedMrl::begin ();
}

KDE_NO_EXPORT void SMIL::MediaType::finish () {
    if (region_node)
        convertNode <SMIL::RegionBase> (region_node)->repaint ();
    TimedMrl::finish ();
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
    if (child->state == state_finished)
        child->deactivate ();
    if (active ()) {
        for (NodePtr c = child->nextSibling(); c; c = c->nextSibling ())
            if (c != external_tree) {
                c->activate ();
                return;
            }
        TimedRuntime * tr = timedRuntime ();
        if (tr->state () < TimedRuntime::timings_stopped) {
            if (tr->state () == TimedRuntime::timings_started)
                tr->propagateStop (child == external_tree);
            return; // still running, wait for runtime to finish
        }
        finish ();
    }
}

KDE_NO_EXPORT void SMIL::MediaType::positionVideoWidget () {
    //kdDebug () << "AVMediaType::sized " << endl;
    MediaTypeRuntime * mtr = static_cast <MediaTypeRuntime *> (timedRuntime ());
    if (region_node) {
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
    switch (event->id ()) {
        case event_sized:
            break; // make it pass to all listeners
        case event_postponed: {
            PostponedEvent * pe = static_cast <PostponedEvent *> (event.ptr ());
            static_cast<MediaTypeRuntime*>(timedRuntime())->postpone (pe->is_postponed);
            ret = true;
            break;
        }
        case event_activated:
        case event_outbounds:
        case event_inbounds:
            propagateEvent (event);
            // fall through // return true;
        default:
            ret = TimedMrl::handleEvent (event);
    }
    RegionBase * r = convertNode <RegionBase> (region_node);
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

KDE_NO_EXPORT void SMIL::AVMediaType::finish () {
    static_cast <AudioVideoData *> (timedRuntime ())->avStopped ();
    region_sized = 0L;
    MediaType::finish ();
}

KDE_NO_EXPORT TimedRuntime * SMIL::AVMediaType::getNewRuntime () {
    return new AudioVideoData (this);
}

KDE_NO_EXPORT bool SMIL::AVMediaType::handleEvent (EventPtr event) {
    if (event->id ()== event_sized && !external_tree)
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
    if (!name.isEmpty () && parentNode ()) {
        parentNode ()->getRuntime ()->setParam (name, getAttribute ("value"));
    }
    Element::activate (); //finish (); // no livetime of itself, will deactivate
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT ImageRuntime::ImageRuntime (NodePtr e)
 : MediaTypeRuntime (e), image (0L), img_movie (0L) {
}

KDE_NO_CDTOR_EXPORT ImageRuntime::~ImageRuntime () {
    delete image;
}

KDE_NO_EXPORT
void ImageRuntime::parseParam (const QString & name, const QString & val) {
    //kdDebug () << "ImageRuntime::param " << name << "=" << val << endl;
    if (name == QString::fromLatin1 ("src")) {
        killWGet ();
        NodePtr element_protect = element;
        SMIL::MediaType * mt = convertNode <SMIL::MediaType> (element);
        if (!mt)
            return; // can not happen
        if (mt->external_tree)
            mt->removeChild (mt->external_tree);
        mt->src = val;
        if (!val.isEmpty ())
            wget (mt->absolutePath ());
    } else
        MediaTypeRuntime::parseParam (name, val);
}

/**
 * start_timer timer expired, repaint if we have an image
 */
KDE_NO_EXPORT void ImageRuntime::started () {
    if (element && downloading ()) {
        postpone_lock = element->document ()->postpone ();
        return;
    }
    //if (durations [duration_time].durval == 0)
    //    durations [duration_time].durval = duration_media; // intrinsic duration of 0 FIXME gif movies
    if (durations [duration_time].durval == 0 &&
            durations [end_time].durval == duration_media) //no duration/end set
        fill = fill_freeze;
    if (img_movie) {
        img_movie->restart ();
        if (img_movie->paused ())
            img_movie->unpause ();
    }
    MediaTypeRuntime::started ();
}

KDE_NO_EXPORT void ImageRuntime::stopped () {
    if (img_movie && have_frame)
        img_movie->pause ();
    MediaTypeRuntime::stopped ();
}

KDE_NO_EXPORT void ImageRuntime::remoteReady (QByteArray & data) {
    NodePtr element_protect = element; // note element is weak
    SMIL::MediaType * mt = convertNode <SMIL::MediaType> (element);
    if (data.size () && mt) {
        QString mime = mimetype ();
        kdDebug () << "ImageRuntime::remoteReady " << mime << " " << data.size () << endl;
        if (mime.startsWith (QString::fromLatin1 ("text/"))) {
            QTextStream ts (data, IO_ReadOnly);
            readXML (element, ts, QString::null);
            mt->external_tree = findExternalTree (element);
        }
        if (!mt->external_tree) {
            QPixmap *pix = new QPixmap (data);
            if (!pix->isNull ()) {
                image = pix;
                delete img_movie;
                img_movie = new QMovie (data);
                have_frame = false;
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
    have_frame = true;
    SMIL::MediaType * mt = convertNode <SMIL::MediaType> (element);
    if (mt && mt->region_node && (timingstate == timings_started ||
                (timingstate == timings_stopped && fill == fill_freeze))) {
        delete image;
        image = 0;
        convertNode <SMIL::RegionBase> (mt->region_node)->repaint ();
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
void TextRuntime::parseParam (const QString & name, const QString & val) {
    //kdDebug () << "TextRuntime::parseParam " << name << "=" << val << endl;
    SMIL::MediaType * mt = convertNode <SMIL::MediaType> (element);
    if (!mt)
        return; // cannot happen
    if (name == QString::fromLatin1 ("src")) {
        killWGet ();
        mt->src = val;
        d->data.resize (0);
        if (!val.isEmpty ())
            wget (mt->absolutePath ());
        return;
    }
    MediaTypeRuntime::parseParam (name, val);
    if (name == QString::fromLatin1 ("backgroundColor")) {
        background_color = QColor (val).rgb ();
    } else if (name == QString ("fontColor")) {
        font_color = QColor (val).rgb ();
    } else if (name == QString ("charset")) {
        d->codec = QTextCodec::codecForName (val.ascii ());
    } else if (name == QString ("fontFace")) {
        ; //FIXME
    } else if (name == QString ("fontPtSize")) {
        font_size = val.toInt ();
    } else if (name == QString ("fontSize")) {
        font_size += val.toInt ();
    // TODO: expandTabs fontBackgroundColor fontSize fontStyle fontWeight hAlig vAlign wordWrap
    } else
        return;
    if (mt->region_node && (timingstate == timings_started ||
                (timingstate == timings_stopped && fill == fill_freeze)))
        convertNode <SMIL::RegionBase> (mt->region_node)->repaint ();
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
