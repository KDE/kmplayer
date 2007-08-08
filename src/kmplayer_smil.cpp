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
static const unsigned int event_activated = (unsigned int) Runtime::dur_activated;
const unsigned int event_inbounds = (unsigned int) Runtime::dur_inbounds;
const unsigned int event_outbounds = (unsigned int) Runtime::dur_outbounds;
static const unsigned int event_stopped = (unsigned int) Runtime::dur_end;
static const unsigned int event_started = (unsigned int)Runtime::dur_start;
static const unsigned int event_to_be_started = 1 + (unsigned int) Runtime::dur_last_dur;
const unsigned int event_pointer_clicked = (unsigned int) event_activated;
const unsigned int event_pointer_moved = (unsigned int) -11;
const unsigned int event_timer = (unsigned int) -12;
const unsigned int event_postponed = (unsigned int) -13;
const unsigned int mediatype_attached = (unsigned int) -14;

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
 *    dur_media     |   dur_media    | wait for event
 *        0         |   dur_media    | only wait for child elements
 *    dur_media     |       0        | intrinsic duration finished
 */
//-----------------------------------------------------------------------------

KDE_NO_EXPORT bool KMPlayer::parseTime (const QString & vl, int & dur) {
    const char * cval = vl.ascii ();
    if (!cval) {
        dur = 0;
        return false;
    }
    int sign = 1;
    bool fp_seen = false;
    QString num;
    const char * p = cval;
    for ( ; *p; p++ ) {
        if (*p == '+') {
            if (!num.isEmpty ())
                break;
            else
                sign = 1;
        } else if (*p == '-') {
            if (!num.isEmpty ())
                break;
            else
                sign = -1;
        } else if (*p >= '0' && *p <= '9') {
            num += QChar (*p);
        } else if (*p == '.') {
            if (fp_seen)
                break;
            else
                num += QChar (*p);
            fp_seen = true;
        } else if (*p == ' ') {
            if (!num.isEmpty ())
                break;
        } else
            break;
    }
    bool ok = false;
    double t;
    if (!num.isEmpty ())
        t = sign * num.toDouble (&ok);
    if (ok) {
        dur = (unsigned int) (10 * t);
        for ( ; *p; p++ ) {
            if (*p == 'm') {
                dur = (unsigned int) (t * 60);
                break;
            } else if (*p == 'h') {
                dur = (unsigned int) (t * 60 * 60);
                break;
            } else if (*p != ' ')
                break;
        }
    } else {
        dur = 0;
        return false;
    }
    return true;
}

static SMIL::Region * findRegion (NodePtr p, const QString & id) {
    TrieString regionname_attr ("regionName");
    for (NodePtr c = p->firstChild (); c; c = c->nextSibling ()) {
        if (c->id == SMIL::id_node_region) {
            SMIL::Region * r = convertNode <SMIL::Region> (c);
            QString a = r->getAttribute (regionname_attr);
            if (a.isEmpty ())
                a = r->getAttribute (StringPool::attr_id);
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
                        id == static_cast <Element *> (c)->
                            getAttribute (StringPool::attr_id))
                    return static_cast <SMIL::Transition *> (c);
    }
    return 0L;
}

static NodePtr findLocalNodeById (NodePtr n, const QString & id) {
    //kdDebug() << "findLocalNodeById " << id << endl;
    SMIL::Smil * s = SMIL::Smil::findSmilNode (n);
    if (s)
        return s->document ()->getElementById (s, id, false);
    return 0L;
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT ToBeStartedEvent::ToBeStartedEvent (NodePtr n)
 : Event (event_to_be_started), node (n) {}

TimerEvent::TimerEvent (TimerInfoPtr tinfo)
 : Event (event_timer), timer_info (tinfo), interval (false) {}

PostponedEvent::PostponedEvent (bool postponed)
 : Event (event_postponed), is_postponed (postponed) {}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT Runtime::Runtime (NodePtr e)
 : timingstate (timings_reset),
   element (e), repeat_count (0) {}

KDE_NO_CDTOR_EXPORT Runtime::~Runtime () {
    if (start_timer || duration_timer) // ugh
        reset ();
}

KDE_NO_EXPORT void Runtime::reset () {
    if (element) {
        if (start_timer) {
            element->document ()->cancelTimer (start_timer);
            ASSERT (!start_timer);
        }
        if (duration_timer) {
            element->document ()->cancelTimer (duration_timer);
            ASSERT (!duration_timer);
        }
    } else {
        start_timer = 0L;
        duration_timer = 0L;
    }
    repeat_count = 0;
    timingstate = timings_reset;
    for (int i = 0; i < (int) durtime_last; i++) {
        if (durations [i].connection)
            durations [i].connection->disconnect ();
        durations [i].durval = dur_timer;
        durations [i].offset = 0;
    }
    endTime ().durval = dur_media;
}

KDE_NO_EXPORT
void Runtime::setDurationItem (DurationTime item, const QString & val) {
    int dur = -2; // also 0 for 'media' duration, so it will not update then
    QString vs = val.stripWhiteSpace ();
    QString vl = vs.lower ();
    const char * cval = vl.ascii ();
    int offset = 0;
    //kdDebug () << "setDuration1 " << vl << endl;
    if (cval && cval[0]) {
        QString idref;
        const char * p = cval;
        if (parseTime (vl, offset)) {
            dur = dur_timer;
        } else if (!strncmp (cval, "id(", 3)) {
            p = strchr (cval + 3, ')');
            if (p) {
                idref = vs.mid (3, p - cval - 3);
                p++;
            }
            if (*p) {
                const char *q = strchr (p, '(');
                if (q)
                    p = q;
            }
        } else if (!strncmp (cval, "indefinite", 10)) {
            dur = dur_infinite;
        } else if (!strncmp (cval, "media", 5)) {
            dur = dur_media;
        }
        if (dur == -2) {
            NodePtr target;
            const char * q = p;
            if (idref.isEmpty ()) {
                bool last_esc = false;
                for ( ; *q; q++) {
                    if (*q == '\\') {
                        if (last_esc) {
                            idref += QChar ('\\');
                            last_esc = false;
                        } else
                            last_esc = true;
                    } else if (*q == '.' && !last_esc) {
                        break;
                    } else
                        idref += QChar (*q);
                }
                if (!*q)
                    idref = vs.mid (p - cval);
                else
                    idref = vs.mid (p - cval, q - p);
            }
            ++q;
            if (!idref.isEmpty ()) {
                target = findLocalNodeById (element, idref);
                if (!target)
                    kdWarning () << "Element not found " << idref << endl;
            }
            //kdDebug () << "setDuration q:" << q << endl;
            if (parseTime (vl.mid (q-cval), offset)) {
                dur = dur_start;
            } else if (*q && !strncmp (q, "end", 3)) {
                dur = dur_end;
                parseTime (vl.mid (q + 3 - cval), offset);
            } else if (*q && !strncmp (q, "begin", 5)) {
                dur = dur_start;
                parseTime (vl.mid (q + 5 - cval), offset);
            } else if (*q && !strncmp (q, "activateevent", 13)) {
                dur = dur_activated;
                parseTime (vl.mid (q + 13 - cval), offset);
            } else if (*q && !strncmp (q, "inboundsevent", 13)) {
                dur = dur_inbounds;
                parseTime (vl.mid (q + 13 - cval), offset);
            } else if (*q && !strncmp (q, "outofboundsevent", 16)) {
                dur = dur_outbounds;
                parseTime (vl.mid (q + 16 - cval), offset);
            } else
                kdWarning () << "setDuration no match " << cval << endl;
            if (target && dur != dur_timer) {
                durations [(int) item].connection =
                    target->connectTo (element, dur);
            }
        }
        //kdDebug () << "setDuration " << dur << " id:'" << idref << "' off:" << offset << endl;
    }
    durations [(int) item].durval = (Duration) dur;
    durations [(int) item].offset = offset;
}

/**
 * start, or restart in case of re-use, the durations
 */
KDE_NO_EXPORT void Runtime::begin () {
    if (!element) {
        reset ();
        return;
    }
    //kdDebug () << "Runtime::begin " << element->nodeName() << endl; 
    if (start_timer || duration_timer)
        convertNode <SMIL::TimedMrl> (element)->init ();
    timingstate = timings_began;

    int offset = 0;
    bool stop = true;
    if (beginTime ().durval == dur_start) { // check started/finished
        Connection * con = beginTime ().connection.ptr ();
        if (con && con->connectee &&
                con->connectee->state >= Node::state_began) {
            offset = beginTime ().offset;
            if (SMIL::TimedMrl::isTimedMrl (con->connectee))
                offset -= element->document ()->last_event_time -
                    convertNode <SMIL::TimedMrl>(con->connectee)->begin_time;
            stop = false;
            kdWarning() << "start trigger on started element" << endl;
        } // else wait for start event
    } else if (beginTime ().durval == dur_end) { // check finished
        Connection * con = beginTime ().connection.ptr ();
        if (con && con->connectee &&
                con->connectee->state >= Node::state_finished) {
            int offset = beginTime ().offset;
            if (SMIL::TimedMrl::isTimedMrl (con->connectee))
                offset -= element->document ()->last_event_time -
                    convertNode<SMIL::TimedMrl>(con->connectee)->finish_time;
            stop = false;
            kdWarning() << "start trigger on finished element" << endl;
        } // else wait for end event
    } else if (beginTime ().durval == dur_timer) {
        offset = beginTime ().offset;
        stop = false;
    }
    if (stop)                          // wait for event
        propagateStop (false);
    else if (offset > 0)               // start timer
        start_timer = element->document ()->setTimeout (
                element, 100 * offset, start_timer_id);
    else                               // start now
        propagateStart ();
}

KDE_NO_EXPORT void Runtime::beginAndStart () {
    if (element) {
        if (start_timer || duration_timer)
            convertNode <SMIL::TimedMrl> (element)->init ();
        timingstate = timings_began;
        propagateStart ();
    }
}

KDE_NO_EXPORT
bool Runtime::parseParam (const TrieString & name, const QString & val) {
    //kdDebug () << "Runtime::parseParam " << name << "=" << val << endl;
    if (name == StringPool::attr_begin) {
        setDurationItem (begin_time, val);
        if ((timingstate == timings_began && !start_timer) ||
                timingstate == timings_stopped) {
            if (beginTime ().offset > 0) { // create a timer for start
                if (beginTime ().durval == dur_timer)
                    start_timer = element->document ()->setTimeout
                        (element, 100 * beginTime ().offset, start_timer_id);
            } else                                // start now
                propagateStart ();
        }
    } else if (name == StringPool::attr_dur) {
        setDurationItem (duration_time, val);
    } else if (name == StringPool::attr_end) {
        setDurationItem (end_time, val);
        if (endTime ().durval == dur_timer &&
            endTime ().offset > beginTime ().offset)
            durTime ().offset = endTime ().offset - beginTime ().offset;
        else if (endTime ().durval != dur_timer)
            durTime ().durval = dur_media; // event
    } else if (name == StringPool::attr_title) {
        Mrl * mrl = static_cast <Mrl *> (element.ptr ());
        if (mrl)
            mrl->pretty_name = val;
    } else if (name == "endsync") {
        if ((durTime ().durval == dur_media || durTime ().durval == 0) &&
                endTime ().durval == dur_media) {
            NodePtr e = findLocalNodeById (element, val);
            if (SMIL::TimedMrl::isTimedMrl (e)) {
                SMIL::TimedMrl * tm = static_cast <SMIL::TimedMrl *> (e.ptr ());
                durations [(int) end_time].connection =
                    tm->connectTo (element, event_stopped);
                durations [(int) end_time].durval = (Duration) event_stopped;
            }
        }
    } else if (name.startsWith ("repeat")) {
        if (val.find ("indefinite") > -1)
            repeat_count = dur_infinite;
        else
            repeat_count = val.toInt ();
    } else
        return false;
    return true;
}

KDE_NO_EXPORT void Runtime::processEvent (unsigned int event) {
    SMIL::TimedMrl * tm = convertNode <SMIL::TimedMrl> (element);
    if (tm) {
        if (timingstate != timings_started && beginTime ().durval == event) {
            if (element && beginTime ().offset > 0)
                start_timer = element->document ()->setTimeout (element,
                        100 * beginTime ().offset, start_timer_id);
            else //FIXME neg. offsets
                propagateStart ();
            if (tm->state == Node::state_finished)
                tm->state = Node::state_activated; // rewind to activated
        } else if (timingstate == timings_started &&
                (unsigned int) endTime ().durval == event)
            propagateStop (true);
    } else
        reset ();
}

KDE_NO_EXPORT void Runtime::propagateStop (bool forced) {
    if (state() == timings_reset || state() == timings_stopped)
        return; // nothing to stop
    if (!forced && element) {
        if (durTime ().durval == dur_media && endTime ().durval == dur_media)
            return; // wait for external eof
        if (endTime ().durval != dur_timer && endTime ().durval != dur_media &&
                (state() == timings_started || beginTime().durval == dur_timer))
            return; // wait for event
        if (durTime ().durval == dur_infinite)
            return; // this may take a while :-)
        if (duration_timer)
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
        if (duration_timer) {
            element->document ()->cancelTimer (duration_timer);
            ASSERT (!duration_timer);
        }
        if (was_started)
            element->document ()->setTimeout (element, 0, stopped_timer_id);
        else if (element->unfinished ())
            element->finish ();
    } else {
        start_timer = 0L;
        duration_timer = 0L;
    }
}

KDE_NO_EXPORT void Runtime::propagateStart () {
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
KDE_NO_EXPORT void Runtime::started () {
    //kdDebug () << "Runtime::started " << (element ? element->nodeName() : "-") << endl; 
    NodePtr e = element; // element is weak
    SMIL::TimedMrl * tm = convertNode <SMIL::TimedMrl> (e);
    if (tm) {
        if (durTime ().offset > 0 && durTime ().durval == dur_timer)
            duration_timer = element->document ()->setTimeout
                (element, 100 * durTime ().offset, dur_timer_id);
        // kdDebug () << "Runtime::started set dur timer " << durTime ().offset << endl;
        tm->propagateEvent (new Event (event_started));
        tm->begin ();
    } else
        reset ();
}

/**
 * duration_timer timer expired or no duration set after started
 */
KDE_NO_EXPORT void Runtime::stopped () {
    if (!element) {
        reset ();
    } else if (element->active ()) {
        if (repeat_count == dur_infinite || 0 < repeat_count--) {
            if (beginTime ().offset > 0 &&
                    beginTime ().durval == dur_timer)
                start_timer = element->document ()->setTimeout
                    (element, 100 * beginTime ().offset, start_timer_id);
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

KDE_NO_CDTOR_EXPORT SizeType::SizeType (const QString & s) {
    *this = s;
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
    if (!(_w > 0 && _h > 0))
        return r;
    if (!(r._w > 0 && r._h > 0))
        return *this;
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
                    convertNode<Element>(c)->getAttribute (StringPool::attr_id)
                        == reg_point) {
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
    else if (width.isSet ()) {
        if (right.isSet ())
            xoff = w - width.size (w) - right.size (w);
        else
            xoff = (w - width.size (w)) / 2;
    } else
        xoff = 0;
    if (top.isSet ())
        yoff = top.size (h);
    else if (height.isSet ()) {
        if (bottom.isSet ())
            yoff = h - height.size (h) - bottom.size (h);
        else
            yoff = (h - height.size (h)) / 2;
    } else
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
bool CalculatedSizer::setSizeParam(const TrieString &name, const QString &val, bool &dim_changed) {
    dim_changed = true;
    if (name == StringPool::attr_left) {
        left = val;
        dim_changed = right.isSet ();
    } else if (name == StringPool::attr_top) {
        top = val;
        dim_changed = bottom.isSet ();
    } else if (name == StringPool::attr_width) {
        width = val;
    } else if (name == StringPool::attr_height) {
        height = val;
    } else if (name == StringPool::attr_right) {
        right = val;
        dim_changed = left.isSet ();
    } else if (name == StringPool::attr_bottom) {
        bottom = val;
        dim_changed = top.isSet ();
    } else if (name == "regPoint") {
        reg_point = val;
        dim_changed = false;
    } else if (name == "regAlign") {
        reg_align = val;
        dim_changed = false;
    } else
        return false;
    return true;
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT AnimateGroupData::AnimateGroupData (NodePtr e)
 : Runtime (e), modification_id (-1) {}

bool AnimateGroupData::parseParam (const TrieString &name, const QString &val) {
  //kdDebug () << "AnimateGroupData::parseParam " << name << "=" << val << endl;
    if (name == StringPool::attr_target || name == "targetElement") {
        if (element)
            target_element = findLocalNodeById (element, val);
    } else if (name == "attribute" || name == "attributeName") {
        changed_attribute = TrieString (val);
    } else if (name == "to") {
        change_to = val;
    } else
        return Runtime::parseParam (name, val);
    return true;
}

/**
 * animation finished
 */
KDE_NO_EXPORT void AnimateGroupData::stopped () {
    //kdDebug () << "AnimateGroupData::stopped " << durTime ().durval << endl;
    if (!SMIL::TimedMrl::keepContent (element))
        restoreModification ();
    Runtime::stopped ();
}

KDE_NO_EXPORT void AnimateGroupData::reset () {
    restoreModification ();
    Runtime::reset ();
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

bool AnimateData::parseParam (const TrieString & name, const QString & val) {
    //kdDebug () << "AnimateData::parseParam " << name << "=" << val << endl;
    if (name == "change_by") {
        change_by = val.toInt ();
    } else if (name == "from") {
        change_from = val;
    } else if (name == "values") {
        change_values = QStringList::split (QString (";"), val);
    } else if (name == "calcMode") {
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
    //kdDebug () << "AnimateData::started " << durTime ().durval << endl;
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
            QRegExp reg ("^\\s*(-?[0-9\\.]+)(\\s*[%a-z]*)?");
            if (change_from.isEmpty ()) {
                if (change_values.size () > 0) // check 'values' attribute
                     change_from = change_values.first ();
                else // take current
                    change_from = target->param (changed_attribute);
            }
            if (!change_from.isEmpty ()) {
                target->setParam (changed_attribute, change_from,
                        &modification_id);
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
            steps = 20 * durTime ().offset / 5; // 40 per sec
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
            int interval = 100 * durTime ().offset / (1 + steps);
            if (interval <= 0 || durTime ().durval != dur_timer) {
                 kdWarning () << "animate needs a duration time" << endl;
                 break;
            }
            //kdDebug () << "AnimateData::started " << target_element->nodeName () << "." << changed_attribute << " " << change_values.first () << "->" << change_values.last () << " in " << steps << " interval:" << interval << endl;
            anim_timer = element->document ()->setTimeout (element, interval, anim_timer_id); // 50 /s for now FIXME
            target->setParam (changed_attribute, change_values.first (),
                    &modification_id);
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
    if (element) {
        if (anim_timer) // make sure timers are stopped
            element->document ()->cancelTimer (anim_timer);
        ASSERT (!anim_timer);
        if (steps > 0 && element->active ()) {
            steps = 0;
            if (calcMode == calc_linear)
                change_from_val = change_to_val;
            applyStep (); // we lost some steps ..
        }
    } else
        anim_timer = 0;
    AnimateGroupData::stopped ();
}

KDE_NO_EXPORT void AnimateData::applyStep () {
    Element * target = convertNode <Element> (target_element);
    if (target && calcMode == calc_linear)
        target->setParam (changed_attribute, QString ("%1%2").arg (
                    change_from_val).arg(change_from_unit),
                &modification_id);
    else if (target && calcMode == calc_discrete)
        target->setParam (changed_attribute,
                change_values[change_values.size () - steps -1],
                &modification_id);
}

/**
 * for animations
 */
KDE_NO_EXPORT bool AnimateData::timerTick () {
    if (!anim_timer) {
        kdError () << "spurious anim timer tick" << endl;
    } else if (steps-- > 0) {
        if (calcMode == calc_linear)
            change_from_val += change_delta;
        applyStep ();
        return true;
    } else {
        if (element)
            element->document ()->cancelTimer (anim_timer);
        ASSERT (!anim_timer);
        propagateStop (true); // not sure, actually
    }
    return false;
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
 : Runtime (e), fit (fit_hidden) {}

KDE_NO_CDTOR_EXPORT MediaTypeRuntime::~MediaTypeRuntime () {
    killWGet ();
}

/**
 * re-implement for pending KIO::Job operations
 */
KDE_NO_EXPORT void KMPlayer::MediaTypeRuntime::reset () {
    clear ();
    postpone_lock = 0L;
    Runtime::reset ();
}

/**
 * re-implement for regions and src attributes
 */
KDE_NO_EXPORT
bool MediaTypeRuntime::parseParam (const TrieString &name, const QString &val) {
    SMIL::MediaType * mt = convertNode <SMIL::MediaType> (element);
    if (!mt)
        return false;
    bool update_surface = true;
    if (name == "fit") {
        const char * cval = val.ascii ();
        if (!cval)
            fit = fit_hidden;
        else if (!strcmp (cval, "fill"))
            fit = fit_fill;
        else if (!strcmp (cval, "hidden"))
            fit = fit_hidden;
        else if (!strcmp (cval, "meet"))
            fit = fit_meet;
        else if (!strcmp (cval, "scroll"))
            fit = fit_scroll;
        else if (!strcmp (cval, "slice"))
            fit = fit_slice;
        else
            fit = fit_hidden;
    } else if (fit_hidden == fit &&
            sub_surface && sub_surface->surface &&
            mt->region_node) {
        SMIL::RegionBase *rb = convertNode <SMIL::RegionBase>(mt->region_node);
        SRect rr = rb->region_surface->bounds;
        SRect sr = sub_surface->bounds;
        Single x, y, w = sr.width(), h = sr.height();
        sizes.calcSizes (element, rr.width(), rr.height(), x, y, w, h);
        if (sizes.setSizeParam (name, val, update_surface)) {
            if (!update_surface) {
                // preserve surface by recalculationg sub_surface top-left
                Single x1, y1;
                w = sr.width();
                h = sr.height();
                sizes.calcSizes(element, rr.width(), rr.height(), x1, y1, w, h);
                SRect new_bounds = SRect (sr.x() + x1-x, sr.y() + y1-y,
                        sr.width(), sr.height());
                SRect ur = sub_surface->bounds.unite (new_bounds);
                sub_surface->bounds = new_bounds;
                rb->repaint (ur);
                return true;
            }
        } else {
            return Runtime::parseParam (name, val);
        }
    } else if (!sizes.setSizeParam (name, val, update_surface)) {
        return Runtime::parseParam (name, val);
    }
    if (sub_surface)
        sub_surface->repaint ();
    resetSurface ();
    if (surface ())
        sub_surface->repaint ();
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
    Runtime::stopped ();
}

KDE_NO_EXPORT void MediaTypeRuntime::clipStart () {
    SMIL::MediaType * mt = convertNode <SMIL::MediaType> (element);
    SMIL::RegionBase *r =mt ? convertNode<SMIL::RegionBase>(mt->region_node):0L;
    if (r && r->surface ())
        for (NodePtr n = mt->firstChild (); n; n = n->nextSibling ())
            if ((n->mrl () && n->mrl ()->opener.ptr () == mt) ||
                    n->id == SMIL::id_node_smil ||
                    n->id == RP::id_node_imfl) {
                n->activate ();
                break;
            }
}

KDE_NO_EXPORT void MediaTypeRuntime::clipStop () {
    SMIL::MediaType * mt = convertNode <SMIL::MediaType> (element);
    if (mt && mt->external_tree && mt->external_tree->active ())
        mt->external_tree->deactivate ();
}

KDE_NO_EXPORT SRect MediaTypeRuntime::intrinsicBounds () {
    SMIL::MediaType *mt = convertNode <SMIL::MediaType> (element);
    SMIL::RegionBase *rb = mt
        ? convertNode <SMIL::RegionBase> (mt->region_node)
        : 0L;
    if (rb && rb->surface ())
        return rb->region_surface->bounds;
    return SRect ();
}

KDE_NO_EXPORT SurfacePtr MediaTypeRuntime::surface () {
    SMIL::MediaType * mt = convertNode <SMIL::MediaType> (element);
    if (!mt || !mt->keepContent (element)) {
        resetSurface ();
    } else if (!sub_surface) {
        SMIL::RegionBase * rb = mt
            ? convertNode <SMIL::RegionBase> (mt->region_node)
            : 0L;
        if (rb && rb->surface ()) {
            SRect ir = intrinsicBounds ();
            SRect rr = rb->region_surface->bounds;
            Single x, y, w = ir.width(), h = ir.height();
            sizes.calcSizes (element, rr.width(), rr.height(), x, y, w, h);
            if (ir.width() > 0 && ir.height() > 0 && w > 0 && h > 0)
                switch (fit) {
                    case fit_meet: {
                        float iasp = 1.0 * ir.width() / ir.height();
                        float rasp = 1.0 * w / h;
                        if (iasp > rasp)
                            h = ir.height() * w / ir.width();
                        else
                            w = ir.width() * h / ir.height();
                        break;
                    }
                    case fit_hidden:
                        w = ir.width();
                        h = ir.height();
                        break;
                    case fit_slice: {
                        float iasp = 1.0 * ir.width() / ir.height();
                        float rasp = 1.0 * w / h;
                        if (iasp > rasp)
                            w = ir.width() * h / ir.height();
                        else
                            h = ir.height() * w / ir.width();
                        break;
                    }
                    default: {} // fit_fill
                }
            sub_surface = rb->region_surface->createSurface (element, SRect (x,y,w,h));
            //kdDebug() << sub_surface.ptr() << " " << mt->nodeName() << " " << mt->src << " " << rr.width() << "," << rr.height()  << " => " << x << "," << y << w << "," << h << endl;
        }
    }
    return sub_surface;
}

KDE_NO_EXPORT void MediaTypeRuntime::resetSurface () {
    if (sub_surface && sub_surface->parentNode ())
        sub_surface->parentNode ()->removeChild (sub_surface);
    sub_surface = NULL;
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
    if (durTime ().durval == 0 && endTime ().durval == dur_media)
        durTime ().durval = dur_media; // duration of clip
    MediaTypeRuntime::started ();
}

static void setSmilLinkNode (NodePtr n, NodePtr link) {
    // this works only because we can only play one at a time FIXME
    SMIL::Smil * s = SMIL::Smil::findSmilNode (n.ptr ());
    if (s && (link || s->current_av_media_type == n)) // only reset ones own
        s->current_av_media_type = link;
}

KDE_NO_EXPORT void AudioVideoData::clipStart () {
    NodePtr element_protect = element; // note element is weak
    SMIL::MediaType * mt = convertNode <SMIL::MediaType> (element);
    PlayListNotify * n = mt ? mt->document ()->notify_listener : 0L;
    //kdDebug() << "AudioVideoData::clipStart " << mt->resolved << endl;
    if (n && mt->region_node && !mt->external_tree && !mt->src.isEmpty()) {
        setSmilLinkNode (element, element);
        mt->repeat = repeat_count == dur_infinite ? 9998 : repeat_count;
        repeat_count = 0;
        n->requestPlayURL (mt);
        document_postponed = mt->document()->connectTo(mt, event_postponed);
    }
    MediaTypeRuntime::clipStart ();
}

KDE_NO_EXPORT void AudioVideoData::clipStop () {
    if (durTime ().durval == dur_media)
        durTime ().durval = dur_timer;//reset to make this finish
    MediaTypeRuntime::clipStop ();
    resetSurface ();
    setSmilLinkNode (element, 0L);
}

KDE_NO_EXPORT
bool AudioVideoData::parseParam(const TrieString &name, const QString &val) {
    //kdDebug () << "AudioVideoData::parseParam " << name << "=" << val << endl;
    if (name == StringPool::attr_src) {
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

KDE_NO_CDTOR_EXPORT MouseListeners::MouseListeners () :
   m_ActionListeners (new NodeRefList),
   m_OutOfBoundsListeners (new NodeRefList),
   m_InBoundsListeners (new NodeRefList) {}

NodeRefListPtr MouseListeners::listeners (unsigned int eid) {
    switch (eid) {
        case event_activated:
            return m_ActionListeners;
        case event_inbounds:
            return m_InBoundsListeners;
        case event_outbounds:
            return m_OutOfBoundsListeners;
    }
    return 0L;
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
        return new SMIL::Area (d, tag);
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
    SMIL::Layout * layout = convertNode <SMIL::Layout> (layout_node);
    if (layout &&layout->region_surface) {
        kdError() << "Layout already has a surface" << endl;
    }
    if (layout)
        Element::activate ();
    else
        Element::deactivate(); // some unfortunate reset in parent doc
}

KDE_NO_EXPORT void SMIL::Smil::deactivate () {
    if (layout_node)
        convertNode <SMIL::Layout> (layout_node)->repaint ();
    Mrl::deactivate ();
    if (layout_node)
        convertNode <SMIL::Layout> (layout_node)->region_surface = NULL;
    Mrl::getSurface(0L);
}

KDE_NO_EXPORT bool SMIL::Smil::handleEvent (EventPtr event) {
    return layout_node ? layout_node->handleEvent (event) : false;
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
            const QString name = elm->getAttribute (StringPool::attr_name);
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
        smilroot->setAttribute(StringPool::attr_width, QString::number(w_root));
        smilroot->setAttribute(StringPool::attr_height,QString::number(h_root));
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
    if (surface ()) {
        updateDimensions ();
        repaint ();
    }
    finish (); // proceed and allow 'head' to finish
}

KDE_NO_EXPORT void SMIL::Layout::updateDimensions () {
    RegionBase * rb = static_cast <RegionBase *> (rootLayout.ptr ());
    x = y = 0;
    w = rb->sizes.width.size ();
    h = rb->sizes.height.size ();
    //kdDebug () << "Layout::updateDimensions " << w << "," << h <<endl;
    SMIL::RegionBase::updateDimensions ();
}

KDE_NO_EXPORT SurfacePtr SMIL::Layout::surface () {
    if (!region_surface) {
        SMIL::Smil * s = Smil::findSmilNode (this);
        if (s) {
            SMIL::RegionBase *rl = convertNode <SMIL::RootLayout> (rootLayout);
            if (rl && auxiliaryNode ()) {
                s->width = 0;
                s->height = 0;
            } else if (rl) {
                w = s->width = rl->getAttribute(StringPool::attr_width).toDouble ();
                h = s->height =rl->getAttribute(StringPool::attr_height).toDouble();
            }
            region_surface = s->getSurface (s);
            if (region_surface) {
                SRect rect = region_surface->bounds;
                if (rl && auxiliaryNode ()) {
                    w = rect.width ();
                    h = rect.height ();
                    rl->setAttribute (StringPool::attr_width, QString::number ((int)w));
                    rl->setAttribute (StringPool::attr_height, QString::number ((int)h));
                    rl->setParam (StringPool::attr_width, QString::number((int)w));
                    rl->setParam (StringPool::attr_height,QString::number((int)h));
                } else if (region_surface && w > 0 && h > 0) {
                    region_surface->xscale = 1.0 * rect.width () / w;
                    region_surface->yscale = 1.0 * rect.height () / h;
                    updateDimensions ();
                }
                kdDebug() << "Layout::surface bounds " << rect.width () << "x" << rect.height () << " w:" << w << " h:" << h << " xs:" << region_surface->xscale << " ys:" << region_surface->yscale << endl;
            }
        }
    }
    return region_surface;
}

KDE_NO_EXPORT void SMIL::Layout::accept (Visitor * v) {
    v->visit (this);
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT SMIL::RegionBase::RegionBase (NodePtr & d, short id)
 : Element (d, id), x (0), y (0), w (0), h (0),
   z_order (1), background_color (0)
   {}

KDE_NO_CDTOR_EXPORT SMIL::RegionBase::~RegionBase () {
    if (region_surface && region_surface->parentNode ())
        region_surface->parentNode ()->removeChild (region_surface);
}

KDE_NO_EXPORT void SMIL::RegionBase::activate () {
    init ();
    setState (state_activated);
    for (NodePtr r = firstChild (); r; r = r->nextSibling ())
        if (r->id == id_node_region || r->id == id_node_root_layout)
            r->activate ();
}

KDE_NO_EXPORT void SMIL::RegionBase::childDone (NodePtr child) {
    headChildDone (this, child);
}

KDE_NO_EXPORT void SMIL::RegionBase::deactivate () {
    background_color = 0;
    if (region_surface)
        region_surface->background_color = 0;
    sizes.resetSizes ();
    Element::deactivate ();
}

KDE_NO_EXPORT void SMIL::RegionBase::repaint () {
    if (surface ())
        region_surface->repaint (0, 0, w, h);
}

KDE_NO_EXPORT void SMIL::RegionBase::repaint (const SRect & rect) {
    if (surface ()) {
        SRect r = SRect (0, 0, w, h).intersect (rect);
        region_surface->repaint (r.x (), r.y (), r.width (), r.height ());
    }
}

KDE_NO_EXPORT void SMIL::RegionBase::updateDimensions () {
    if (surface () && active ())
        for (NodePtr r = firstChild (); r; r = r->nextSibling ())
            if (r->id == id_node_region) {
                SMIL::Region * cr = static_cast <SMIL::Region *> (r.ptr ());
                cr->calculateBounds (w, h);
                cr->updateDimensions ();
            }
}

KDE_NO_EXPORT SurfacePtr SMIL::RegionBase::surface () {
    if (!region_surface) {
        Node *n = parentNode ().ptr ();
        if (n &&
                (SMIL::id_node_region == n->id ||
                 SMIL::id_node_layout == n->id)) {
            SurfacePtr ps = static_cast <SMIL::Region *> (n)->surface ();
            if (ps) {
                region_surface = ps->createSurface (this, SRect (x, y, w, h));
                region_surface->background_color = background_color;
            }
        }
    }
    return region_surface;
}

KDE_NO_EXPORT
void SMIL::RegionBase::parseParam (const TrieString & name, const QString & val) {
    //kdDebug () << "RegionBase::parseParam " << getAttribute ("id") << " " << name << "=" << val << " active:" << active() << endl;
    bool need_repaint = false;
    SRect rect = SRect (x, y, w, h);
    bool dim_changed;
    if (name == "background-color" || name == "backgroundColor") {
        if (val.isEmpty ())
            background_color = 0;
        else
            background_color = 0xff000000 | QColor (val).rgb ();
        if (region_surface || (active () && surface ()))
            region_surface->background_color = background_color;
        need_repaint = true;
    } else if (name == "z-index") {
        z_order = val.toInt ();
        need_repaint = true;
    } else if (sizes.setSizeParam (name, val, dim_changed)) {
        if (active ()) {
            if (dim_changed && region_surface && region_surface->parentNode ())
                region_surface->parentNode ()->removeChild (region_surface);
            NodePtr p = parentNode ();
            if (p &&(p->id==SMIL::id_node_region ||p->id==SMIL::id_node_layout))
                convertNode <SMIL::RegionBase> (p)->updateDimensions ();
            rect = rect.unite (SRect (x, y, w, h));
            need_repaint = true;
        }
    }
    if (need_repaint && active () && surface() && region_surface->parentNode ())
        region_surface->parentNode ()->repaint (rect.x(), rect.y(),
                rect.width(), rect.height());
    Element::parseParam (name, val);
}

KDE_NO_CDTOR_EXPORT SMIL::Region::Region (NodePtr & d)
 : RegionBase (d, id_node_region),
   has_mouse (false),
   m_AttachedMediaTypes (new NodeRefList) {}

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
    if (surface ())
        region_surface->bounds = SRect (x, y, w, h);
    //kdDebug () << "Region::calculateBounds parent:" << pw << "x" << ph << " this:" << x << "," << y << " " << w << "x" << h << endl;
}

NodeRefListPtr SMIL::Region::listeners (unsigned int eid) {
    NodeRefListPtr l = mouse_listeners.listeners (eid);
    if (l)
        return l;
    switch (eid) {
        case mediatype_attached:
            return m_AttachedMediaTypes;
    }
    return RegionBase::listeners (eid);
}

KDE_NO_EXPORT void SMIL::Region::accept (Visitor * v) {
    v->visit (this);
}

//-----------------------------------------------------------------------------

KDE_NO_EXPORT
void SMIL::RegPoint::parseParam (const TrieString & p, const QString & v) {
    bool b;
    sizes.setSizeParam (p, v, b); // TODO: if dynamic, make sure to repaint
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
void SMIL::Transition::parseParam (const TrieString & para, const QString & val) {
    if (para == StringPool::attr_type)
        type = val;
    else if (para == StringPool::attr_dur)
        dur = int (10 * val.toDouble ());
    else if (para == "subtype")
        subtype = val;
    else if (para == "fadeColor")
        fade_color = QColor (getAttribute (val)).rgb ();
    else if (para == "direction")
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
   fill_active (fill_auto),
   m_StartListeners (new NodeRefList),
   m_StartedListeners (new NodeRefList),
   m_StoppedListeners (new NodeRefList),
   m_runtime (0L) {}

KDE_NO_CDTOR_EXPORT SMIL::TimedMrl::~TimedMrl () {
    delete m_runtime;
}

KDE_NO_EXPORT void SMIL::TimedMrl::closed () {
    pretty_name = getAttribute (StringPool::attr_title);
    Mrl::closed ();
}

KDE_NO_EXPORT void SMIL::TimedMrl::init () {
    runtime ()->reset ();
    begin_time = finish_time = 0;
    fill = fill_default;
    fill_def = fill_inherit;
    fill_active = getDefaultFill (this);
    Mrl::init ();
}

KDE_NO_EXPORT void SMIL::TimedMrl::activate () {
    //kdDebug () << "SMIL::TimedMrl(" << nodeName() << ")::activate" << endl;
    setState (state_activated);
    Runtime * rt = runtime ();
    init ();
    if (rt == m_runtime) // Runtime might already be dead
        rt->begin ();
    else
        deactivate ();
}

KDE_NO_EXPORT void SMIL::TimedMrl::begin () {
    begin_time = document ()->last_event_time;
    Mrl::begin ();
    runtime ()->propagateStop (false); //see whether this node has a livetime or not
}

KDE_NO_EXPORT void SMIL::TimedMrl::deactivate () {
    //kdDebug () << "SMIL::TimedMrl(" << nodeName() << ")::deactivate" << endl;
    if (unfinished ())
        finish ();
    if (m_runtime) {
        m_runtime->reset ();
        delete m_runtime;
        m_runtime = 0L;
    }
    Mrl::deactivate ();
}

KDE_NO_EXPORT void SMIL::TimedMrl::finish () {
    if (m_runtime &&
            (m_runtime->state () == Runtime::timings_started ||
             m_runtime->state () == Runtime::timings_began)) {
        runtime ()->propagateStop (true); // reschedule through Runtime::stopped
    } else {
        finish_time = document ()->last_event_time;
        Mrl::finish ();
        propagateEvent (new Event (event_stopped));
    }
}

KDE_NO_EXPORT void SMIL::TimedMrl::reset () {
    //kdDebug () << "SMIL::TimedMrl::reset " << endl;
    Mrl::reset ();
    delete m_runtime;
    m_runtime = 0L;
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
 * Bail out if Runtime is running. In case of dur_media, give Runtime
 * a hand with calling propagateStop(true)
 */
KDE_NO_EXPORT void SMIL::TimedMrl::childDone (NodePtr c) {
    if (!active ())
        return; // forced reset
    if (c->nextSibling ())
        c->nextSibling ()->activate ();
    else { // check if Runtime still running
        Runtime * tr = runtime ();
        if (tr->state () < Runtime::timings_stopped) {
            if (tr->state () == Runtime::timings_started)
                tr->propagateStop (false);
            return; // still running, wait for runtime to finish
        }
        finish ();
    }
}

KDE_NO_EXPORT NodeRefListPtr SMIL::TimedMrl::listeners (unsigned int id) {
    if (id == event_stopped)
        return m_StoppedListeners;
    else if (id == event_started)
        return m_StartedListeners;
    else if (id == event_to_be_started)
        return m_StartListeners;
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
                    runtime ()->started ();
                else if (te->timer_info->event_id == stopped_timer_id)
                    runtime ()->stopped ();
                else if (te->timer_info->event_id == start_timer_id)
                    runtime ()->propagateStart ();
                else if (te->timer_info->event_id == dur_timer_id)
                    runtime ()->propagateStop (true);
                else
                    kdWarning () << "unhandled timer event" << endl;
            }
            break;
        }
        default:
            if (m_runtime)
                m_runtime->processEvent (id);
    }
    return true;
}

KDE_NO_EXPORT Runtime * SMIL::TimedMrl::getNewRuntime () {
    return new Runtime (this);
}

KDE_NO_EXPORT
void SMIL::TimedMrl::parseParam (const TrieString &para, const QString &value) {
    if (para.startsWith (StringPool::attr_fill)) {
        Fill * f = &fill;
        if (para != StringPool::attr_fill) {
            f = &fill_def;
            *f = fill_inherit;
        } else
            *f = fill_default;
        fill_active = fill_auto;
        if (value == "freeze")
            *f = fill_freeze;
        else if (value == "hold")
            *f = fill_hold;
        else if (value == "auto")
            *f = fill_auto;
        else if (value == "remove")
            *f = fill_remove;
        else if (value == "transition")
            *f = fill_transition;
        if (fill == fill_default) {
            if (fill_def == fill_inherit)
                fill_active = getDefaultFill (this);
            else
                fill_active = fill_def;
        } else
            fill_active = fill;
    } else if (!runtime ()->parseParam (para, value)) {
        if (para == StringPool::attr_src) //block Mrl src parsing for now
            kdDebug() << "parseParam src on " << nodeName() << endl;
        else
            Mrl::parseParam (para, value);
    }
}

KDE_NO_EXPORT
Runtime::DurationItem * SMIL::TimedMrl::getDuration (NodePtr n) {
    if (!isTimedMrl (n) || !n->active ())
        return 0L;
    TimedMrl * tm = convertNode <SMIL::TimedMrl> (n);
    return &tm->runtime ()->durations [Runtime::duration_time];
}

KDE_NO_EXPORT bool SMIL::TimedMrl::keepContent (NodePtr n) {
    if (isTimedMrl (n)) {
        TimedMrl * tm = convertNode <SMIL::TimedMrl> (n);
        NodePtr p = n->parentNode ();
        if (tm->m_runtime && p && p->active ()) {
            if (tm->runtime ()->timingstate == Runtime::timings_started)
                return true;
            if (tm->runtime ()->timingstate == Runtime::timings_stopped)
                switch (tm->fill_active) {
                    case fill_hold: // keep while parent active
                        return true;
                    case fill_freeze: // keep in parent duration
                        return p->unfinished() &&
                            (p->id == SMIL::id_node_par || p->lastChild() == n);
                    case fill_default:  // as freeze when no duration is set
                    case fill_auto:     // or when parent finished w/o duration
                        return keepContent (p) &&
                            (p->id == SMIL::id_node_par ||
                             p->lastChild() == n) &&
                            tm->runtime ()->durTime ().durval == Runtime::dur_timer &&
                            !tm->runtime ()->durTime ().offset;
                    default:
                        break;
                }
        }
        return false;
    }
    return true; // FIXME check for anchor ..
}

KDE_NO_EXPORT SMIL::TimedMrl::Fill SMIL::TimedMrl::getDefaultFill (NodePtr n) {
    for (NodePtr p = n->parentNode (); p; p = p->parentNode ())
        if (isTimedMrl (p)) {
            SMIL::TimedMrl * tm = convertNode<SMIL::TimedMrl>(p);
            if (tm->fill_def != fill_inherit)
                return tm->fill_def;
            else if (tm->fill == fill_default)
                return tm->fill_active; // assume parent figured out this
        } else if (p->id == SMIL::id_node_smil)
            break;
    return fill_auto;
}

//-----------------------------------------------------------------------------

KDE_NO_EXPORT void SMIL::GroupBase::finish () {
    setState (state_finished); // avoid recurstion through childDone
    for (NodePtr e = firstChild (); e; e = e->nextSibling ())
        if (keepContent (e)) {
            if (e->unfinished ())
                e->finish ();
        } else if (e->active ())
            e->deactivate ();
    TimedMrl::finish ();
}

KDE_NO_EXPORT void SMIL::GroupBase::deactivate () {
    setState (state_deactivated); // avoid recurstion through childDone
    for (NodePtr e = firstChild (); e; e = e->nextSibling ())
        if (e->active ())
            e->deactivate ();
    TimedMrl::deactivate ();
}

KDE_NO_EXPORT void SMIL::GroupBase::setJumpNode (NodePtr n) {
    NodePtr child = n;
    if (state > state_init) {
        for (NodePtr c = firstChild (); c; c = c->nextSibling ())
            if (c->active ())
                c->reset ();
        for (NodePtr c = n->parentNode (); c; c = c->parentNode ()) {
            if (c.ptr () == this || c->id == id_node_body)
                break;
            if (c->id >= id_node_first_group && c->id <= id_node_last_group)
                convertNode <GroupBase> (c)->jump_node = child;
            child = c;
        }
    }
    jump_node = child;
    state = state_activated;
    init ();
    runtime()->beginAndStart (); // undefer through begin()
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
    jump_node = 0L; // TODO: adjust timings
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
        Runtime * tr = runtime ();
        if (tr->state () == Runtime::timings_started) {
            Runtime::Duration dv = tr->durTime ().durval;
            if ((dv == Runtime::dur_timer && !tr->durTime ().offset)
                    || dv == Runtime::dur_media)
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
    if (jump_node) {
        for (NodePtr c = firstChild (); c; c = c->nextSibling ())
            if (c == jump_node) {
                jump_node = 0L;
                c->activate ();
                break;
            } else {
                c->state = state_activated; // TODO: ..
                if (c->isElementNode ())
                    convertNode <Element> (c)->init ();
                c->state = state_finished; // TODO: ..
            }
    } else if (firstChild ())
        firstChild ()->activate ();
    GroupBase::begin ();
}

KDE_NO_EXPORT void SMIL::Seq::childDone (NodePtr child) {
    if (unfinished ()) {
        if (state != state_deferred) {
            if (!keepContent (child) && child->active ())
                child->deactivate ();
            if (child->nextSibling ())
                child->nextSibling ()->activate ();
            else
                finish ();
        } else if (jump_node)
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
        if (isTimedMrl (e)) {
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
        if (isTimedMrl (e)) {
            Runtime *tr = convertNode <SMIL::TimedMrl> (e)->runtime();
            if (tr->state () == Runtime::timings_started)
                return;
        }
    // now finish unless 'dur="indefinite/some event/.."'
    Runtime * tr = runtime ();
    if (tr->state () == Runtime::timings_started)
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
            if (!isTimedMrl (e))
                continue; // definitely a stowaway
            convertNode<SMIL::TimedMrl>(e)->runtime()->propagateStop(true);
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
    init ();
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

KDE_NO_EXPORT Node::PlayType SMIL::Switch::playType () {
    if (cached_ismrl_version != document()->m_tree_version) {
        cached_play_type = play_type_none;
        for (NodePtr e = firstChild (); e; e = e->nextSibling ())
            if (e->playType () != play_type_none) {
                cached_play_type = e->playType ();
                break;
            }
    }
    return cached_play_type;
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT SMIL::LinkingBase::LinkingBase (NodePtr & d, short id)
 : Element(d, id), show (show_replace) {}

KDE_NO_EXPORT void SMIL::LinkingBase::deactivate () {
    mediatype_activated = 0L;
    mediatype_attach = 0L;
    Element::deactivate ();
}

KDE_NO_EXPORT
void SMIL::LinkingBase::parseParam(const TrieString &para, const QString &val) {
    if (para == StringPool::attr_href) {
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
            mediatype_attach = c->connectTo (this, mediatype_attached);
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

KDE_NO_CDTOR_EXPORT SMIL::Area::Area (NodePtr & d, const QString & t)
 : LinkingBase (d, id_node_area), coords (0L), nr_coords (0), tag (t) {}

KDE_NO_CDTOR_EXPORT SMIL::Area::~Area () {
    delete [] coords;
}

KDE_NO_EXPORT void SMIL::Area::activate () {
    init ();
    if (parentNode () &&
            parentNode ()->id >= id_node_first_mediatype &&
            parentNode ()->id <= id_node_last_mediatype) {
        mediatype_activated = parentNode ()->connectTo (this, event_activated);
        mediatype_attach = parentNode ()->connectTo (this, mediatype_attached);
    }
    Element::activate ();
}

KDE_NO_EXPORT
void SMIL::Area::parseParam (const TrieString & para, const QString & val) {
    if (para == "coords") {
        delete [] coords;
        QStringList clist = QStringList::split (QString (","), val);
        nr_coords = clist.count ();
        coords = new SizeType [nr_coords];
        for (int i = 0; i < nr_coords; ++i)
            coords[i] = clist[i];
    } else
        LinkingBase::parseParam (para, val);
}

KDE_NO_EXPORT NodeRefListPtr SMIL::Area::listeners (unsigned int id) {
    NodeRefListPtr l = mouse_listeners.listeners (id);
    if (l)
        return l;
    return Element::listeners (id);
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT SMIL::MediaType::MediaType (NodePtr &d, const QString &t, short id)
 : TimedMrl (d, id), m_type (t), bitrate (0), trans_step (0), trans_steps (0),
   sensitivity (sens_opaque),
   m_MediaAttached (new NodeRefList) {
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
void SMIL::MediaType::parseParam (const TrieString &para, const QString & val) {
    if (para == "system-bitrate")
        bitrate = val.toInt ();
    else if (para == StringPool::attr_type)
        mimetype = val;
    else if (para == "transIn") {
        trans_in = findTransition (this, val);
        if (!trans_in)
            kdWarning() << "Transition " << val << " not found in head" << endl;
    } else if (para == "transOut") {
        trans_out = findTransition (this, val);
        if (!trans_out)
            kdWarning() << "Transition " << val << " not found in head" << endl;
    } else if (para == "sensitivity") {
        if (val == "transparent")
            sensitivity = sens_transparent;
        //else if (val == "percentage") // TODO
        //    sensitivity = sens_percentage;
        else
            sensitivity = sens_opaque;
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
    runtime ()->begin ();
}

KDE_NO_EXPORT void SMIL::MediaType::deactivate () {
    region_paint = 0L;
    region_mouse_enter = 0L;
    region_mouse_leave = 0L;
    region_mouse_click = 0L;
    region_attach = 0L;
    region_attach_external = 0L;
    trans_step = trans_steps = 0;
    if (region_node)
        convertNode <SMIL::RegionBase> (region_node)->repaint ();
    if (trans_timer)
        document ()->cancelTimer (trans_timer);
    TimedMrl::deactivate (); // keep region for runtime rest
    region_node = 0L;
}

KDE_NO_EXPORT void SMIL::MediaType::begin () {
    SMIL::Smil * s = Smil::findSmilNode (parentNode ().ptr ());
    SMIL::Region * r = s ?
        findRegion (s->layout_node, param (StringPool::attr_region)) : 0L;
    MediaTypeRuntime *tr = static_cast<MediaTypeRuntime*>(runtime ());
    if (r) {
        region_node = r;
        region_mouse_enter = r->connectTo (this, event_inbounds);
        region_mouse_leave = r->connectTo (this, event_outbounds);
        region_mouse_click = r->connectTo (this, event_activated);
        region_attach = r->connectTo (this, mediatype_attached);
        r->repaint ();
        tr->clipStart ();
        Transition * trans = convertNode <Transition> (trans_in);
        if (trans && trans->supported ()) {
            trans_step = 1;
            trans_steps = trans->dur; // 10/s FIXME
            trans_timer = document()->setTimeout(this, 100, trans_timer_id);
        }
    } else
        kdWarning () << nodeName() << "::begin " << src << " region '" <<
            param (StringPool::attr_region) << "' not found" << endl;
    TimedMrl::begin ();
}

KDE_NO_EXPORT void SMIL::MediaType::finish () {
    if (trans_timer && !keepContent (this)) {
        document ()->cancelTimer (trans_timer);
        ASSERT(!trans_timer);
    }
    if (region_node)
        convertNode <SMIL::RegionBase> (region_node)->repaint ();
    TimedMrl::finish ();
    static_cast <MediaTypeRuntime *> (runtime ())->clipStop ();
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
        Runtime * tr = runtime ();
        if (tr->state () < Runtime::timings_stopped) {
            if (tr->state () == Runtime::timings_started)
                tr->propagateStop (child_doc); // what about repeat_count ..
            return; // still running, wait for runtime to finish
        }
    }
    if (active ())
        finish ();
}

KDE_NO_EXPORT bool SMIL::MediaType::needsVideoWidget () {
    MediaTypeRuntime * mtr = static_cast <MediaTypeRuntime *> (runtime ());
    SMIL::Smil * s = SMIL::Smil::findSmilNode (this);
    Node * link = s ? s->current_av_media_type.ptr () : 0L;
    return ((link == this || !link) &&
            (state == state_deferred || unfinished ()) && link &&
            mtr->state () != Runtime::timings_stopped &&
            (!strcmp (nodeName (), "video") || !strcmp (nodeName (), "ref")) &&
            mtr->surface ());
}

SurfacePtr SMIL::MediaType::getSurface (NodePtr node) {
    MediaTypeRuntime * mtr = static_cast <MediaTypeRuntime *> (runtime ());
    SurfacePtr s = mtr->surface ();
    if (s) {
        if (node) {
            s->node = node;
            region_attach_external = region_node->connectTo (node, mediatype_attached);
        } else {
            mtr->resetSurface ();
            region_attach_external = NULL;
        }
    }
    return s;
}

bool SMIL::MediaType::handleEvent (EventPtr event) {
    bool ret = false;
    RegionBase * r = convertNode <RegionBase> (region_node);
    SurfacePtr s = static_cast<MediaTypeRuntime*>(runtime())->surface();
    switch (event->id ()) {
        case event_postponed: {
            PostponedEvent * pe = static_cast <PostponedEvent *> (event.ptr ());
            static_cast<MediaTypeRuntime*>(runtime())->postpone (pe->is_postponed);
            ret = true;
            break;
        }
        case event_timer: {
            TimerEvent * te = static_cast <TimerEvent *> (event.ptr ());
            if (r && te && te->timer_info &&
                    te->timer_info->event_id == trans_timer_id) {
                te->interval = ++trans_step < trans_steps;
                if (s)
                    s->repaint ();
                ret = true;
                break;
            }
        } // fall through
        default:
            ret = TimedMrl::handleEvent (event);
    }
    if (s && s->node && s->node.ptr () != this)
        return s->node->handleEvent (event);
    return ret;
}

KDE_NO_EXPORT NodeRefListPtr SMIL::MediaType::listeners (unsigned int id) {
    NodeRefListPtr l = mouse_listeners.listeners (id);
    if (l)
        return l;
    switch (id) {
        case mediatype_attached:
            return m_MediaAttached;
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
    MediaTypeRuntime * mr = static_cast <MediaTypeRuntime *> (runtime ());
    if (mr->state () == Runtime::timings_started)
        mr->postpone_lock = document ()->postpone ();
}

KDE_NO_EXPORT void SMIL::AVMediaType::undefer () {
    setState (state_activated);
    external_tree = findExternalTree (this);
    MediaTypeRuntime * mr = static_cast <MediaTypeRuntime *> (runtime ());
    if (mr->state () == Runtime::timings_started) {
        mr->postpone_lock = 0L;
        mr->started ();
    }
}

KDE_NO_EXPORT void SMIL::AVMediaType::endOfFile () {
    if (!active())
        return; // backend eof after a reset
    MediaTypeRuntime * mr = static_cast <MediaTypeRuntime *> (runtime ());
    mr->postpone_lock = 0L;
    mr->propagateStop (true);
}

KDE_NO_EXPORT Runtime * SMIL::AVMediaType::getNewRuntime () {
    return new AudioVideoData (this);
}

KDE_NO_EXPORT void SMIL::AVMediaType::accept (Visitor * v) {
    v->visit (this);
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT
SMIL::ImageMediaType::ImageMediaType (NodePtr & d)
    : SMIL::MediaType (d, "img", id_node_img) {}

KDE_NO_EXPORT Runtime * SMIL::ImageMediaType::getNewRuntime () {
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

KDE_NO_EXPORT Runtime * SMIL::TextMediaType::getNewRuntime () {
    return new TextRuntime (this);
}

KDE_NO_EXPORT void SMIL::TextMediaType::accept (Visitor * v) {
    v->visit (this);
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT
SMIL::RefMediaType::RefMediaType (NodePtr & d)
    : SMIL::MediaType (d, "ref", id_node_ref) {}

KDE_NO_EXPORT Runtime * SMIL::RefMediaType::getNewRuntime () {
    return new AudioVideoData (this); // FIXME check mimetype first
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

KDE_NO_EXPORT Runtime * SMIL::Brush::getNewRuntime () {
    return new MediaTypeRuntime (this);
}

//-----------------------------------------------------------------------------

KDE_NO_EXPORT Runtime * SMIL::Set::getNewRuntime () {
    return new SetData (this);
}

//-----------------------------------------------------------------------------

KDE_NO_EXPORT Runtime * SMIL::Animate::getNewRuntime () {
    return new AnimateData (this);
}

KDE_NO_EXPORT bool SMIL::Animate::handleEvent (EventPtr event) {
    if (event->id () == event_timer) {
        TimerEvent * te = static_cast <TimerEvent *> (event.ptr ());
        if (te && te->timer_info && te->timer_info->event_id == anim_timer_id) {
            if (static_cast <AnimateData *> (runtime ())->timerTick () &&
                    te->timer_info)
                te->interval = true;
            return true;
        }
    }
    return TimedMrl::handleEvent (event);
}

//-----------------------------------------------------------------------------

KDE_NO_EXPORT void SMIL::Param::activate () {
    setState (state_activated);
    QString name = getAttribute (StringPool::attr_name);
    Node * parent = parentNode ().ptr ();
    if (!name.isEmpty () && parent && parent->isElementNode ())
        static_cast<Element*>(parent)->setParam (name,
                getAttribute (StringPool::attr_value));
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
bool ImageRuntime::parseParam (const TrieString & name, const QString & val) {
    //kdDebug () << "ImageRuntime::param " << name << "=" << val << endl;
    if (name == StringPool::attr_src) {
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

KDE_NO_EXPORT SRect ImageRuntime::intrinsicBounds () {
    ImageData * id = cached_img.data.ptr ();
    if (id && !id->isEmpty () &&
            id->width() > 0 && id->height() > 0)
        return SRect (0, 0, id->width(), id->height());
    return SRect();
}

KDE_NO_EXPORT void ImageRuntime::remoteReady (QByteArray & data) {
    NodePtr element_protect = element; // note element is weak
    SMIL::MediaType * mt = convertNode <SMIL::MediaType> (element);
    if (data.size () && mt) {
        resetSurface ();
        QString mime = mimetype ();
        kdDebug () << "ImageRuntime::remoteReady " << mime << " empty:" << cached_img.data->isEmpty () << " " << mt->src << endl;
        if (mime.startsWith (QString::fromLatin1 ("text/"))) {
            QTextStream ts (data, IO_ReadOnly);
            readXML (element, ts, QString ());
            mt->external_tree = findExternalTree (element);
        }
        if (!mt->external_tree && cached_img.data->isEmpty ()) {
            delete img_movie;
            img_movie = 0L;
            QImage *pix = new QImage (data);
            if (!pix->isNull ()) {
                cached_img.data->image = pix;
                img_movie = new QMovie (data, data.size ());
                img_movie->connectUpdate(this,SLOT(movieUpdated(const QRect&)));
                img_movie->connectStatus (this, SLOT (movieStatus (int)));
                img_movie->connectResize(this,SLOT (movieResize(const QSize&)));
                frame_nr = 0;
                if (surface ())
                    sub_surface->repaint ();
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
        resetSurface ();
        cached_img.setUrl (QString ());
        ASSERT (cached_img.data && cached_img.data->isEmpty ());
        cached_img.data->image = new QImage;
        *cached_img.data->image = (img_movie->framePixmap ());
        if (surface())
            sub_surface->repaint ();
    }
    if (timingstate != timings_started && img_movie)
        img_movie->pause ();
}

KDE_NO_EXPORT void ImageRuntime::movieStatus (int s) {
    if (element && element->state >= Node::state_began &&
            SMIL::TimedMrl::keepContent (element)) {
        if (s == QMovie::EndOfMovie) {
            propagateStop (false);
        }
    }
}

KDE_NO_EXPORT void ImageRuntime::movieResize (const QSize &) {
    if (surface ())
        sub_surface->repaint ();
        //kdDebug () << "movieResize" << endl;
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
    bg_opacity = 100;
    halign = align_left;
    MediaTypeRuntime::reset ();
}

KDE_NO_EXPORT
bool TextRuntime::parseParam (const TrieString & name, const QString & val) {
    //kdDebug () << "TextRuntime::parseParam " << name << "=" << val << endl;
    SMIL::MediaType * mt = convertNode <SMIL::MediaType> (element);
    if (!mt)
        return false; // cannot happen
    if (name == StringPool::attr_src) {
        killWGet ();
        mt->src = val;
        d->data.resize (0);
        if (!val.isEmpty ())
            wget (mt->absolutePath ());
        return true;
    }
    if (name == "backgroundColor" || name == "background-color") {
        background_color = val.isEmpty () ? 0xffffff : QColor (val).rgb ();
    } else if (name == "fontColor") {
        font_color = val.isEmpty () ? 0 : QColor (val).rgb ();
    } else if (name == "charset") {
        d->codec = QTextCodec::codecForName (val.ascii ());
    } else if (name == "fontFace") {
        ; //FIXME
    } else if (name == "fontPtSize") {
        font_size = val.isEmpty () ? d->font.pointSize () : val.toInt ();
    } else if (name == "fontSize") {
        font_size += val.isEmpty () ? d->font.pointSize () : val.toInt ();
    } else if (name == "backgroundOpacity") {
        bg_opacity = (int) SizeType (val).size (100);
    } else if (name == "hAlign") {
        const char * cval = val.ascii ();
        if (!cval)
            halign = align_left;
        else if (!strcmp (cval, "center"))
            halign = align_center;
        else if (!strcmp (cval, "right"))
            halign = align_right;
        else
            halign = align_left;
    // TODO: expandTabs fontBackgroundColor fontSize fontStyle fontWeight hAlig vAlign wordWrap
    } else
        return MediaTypeRuntime::parseParam (name, val);
    resetSurface ();
    if (surface ())
        sub_surface->repaint ();
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
        resetSurface ();
        if (d->data.size () > 0 && !d->data [d->data.size () - 1])
            d->data.resize (d->data.size () - 1); // strip zero terminate char
        QTextStream ts (d->data, IO_ReadOnly);
        if (d->codec)
            ts.setCodec (d->codec);
        text  = ts.read ();
        if (surface ())
            sub_surface->repaint ();
    }
    postpone_lock = 0L;
    if (timingstate == timings_started)
        started ();
}

//-----------------------------------------------------------------------------

#include "kmplayer_smil.moc"
