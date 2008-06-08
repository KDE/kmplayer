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

#include "config-kmplayer.h"

#include <stdlib.h>

#include <qtextstream.h>
#include <qcolor.h>
#include <qfont.h>
#include <qapplication.h>
#include <qregexp.h>
#include <qtimer.h>
#include <QBuffer>

#include <kdebug.h>
#include <kurl.h>
#include <kmimetype.h>
#include <kio/job.h>
#include <kio/jobclasses.h>

#include "kmplayer_smil.h"
#include "kmplayer_rp.h"
#include "mediaobject.h"

using namespace KMPlayer;

namespace KMPlayer {
static const unsigned int event_activated = (unsigned int) Runtime::dur_activated;
const unsigned int event_inbounds = (unsigned int) Runtime::dur_inbounds;
const unsigned int event_outbounds = (unsigned int) Runtime::dur_outbounds;
const unsigned int event_stopped = (unsigned int) Runtime::dur_end;
const unsigned int event_started = (unsigned int)Runtime::dur_start;
static const unsigned int event_to_be_started = 1 + (unsigned int) Runtime::dur_last_dur;
const unsigned int event_pointer_clicked = (unsigned int) event_activated;
const unsigned int event_pointer_moved = (unsigned int) -11;
const unsigned int event_timer = (unsigned int) -12;
const unsigned int event_postponed = (unsigned int) -13;
const unsigned int mediatype_attached = (unsigned int) -14;
const unsigned int event_update = (unsigned int) -15;

static const unsigned int start_timer_id = (unsigned int) 3;
static const unsigned int dur_timer_id = (unsigned int) 4;
static const unsigned int anim_timer_id = (unsigned int) 5;
static const unsigned int trans_timer_id = (unsigned int) 6;
static const unsigned int trans_out_timer_id = (unsigned int) 7;
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
                //kDebug () << "MediaType region found " << id;
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

static Node *findLocalNodeById (Node *n, const QString & id) {
    SMIL::Smil *s = SMIL::Smil::findSmilNode (n);
    if (s)
        return s->document ()->getElementById (s, id, false);
    return 0L;
}

static Fit parseFit (const char *cval) {
    Fit fit;
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
    return fit;
}

//-----------------------------------------------------------------------------

PostponedEvent::PostponedEvent (bool postponed)
 : Event (NULL, event_postponed), is_postponed (postponed) {}

//-----------------------------------------------------------------------------

static Runtime::Fill getDefaultFill (NodePtr n) {
    for (NodePtr p = n->parentNode (); p; p = p->parentNode ()) {
        Runtime *rt = static_cast <Runtime *> (p->role (RoleTypeTiming));
        if (rt) {
            if (rt->fill_def != Runtime::fill_inherit)
                return rt->fill_def;
            else if (rt->fill == Runtime::fill_default)
                return rt->fill_active;//assume parent figured out this
        } else if (p->id == SMIL::id_node_smil)
            break;
    }
    return Runtime::fill_auto;
}

static bool keepContent (Node *n) {
    Runtime *rt = static_cast <Runtime *> (n->role (RoleTypeTiming));
    if (rt) {
        if (rt->started ())
            return true;
        Node *p = n->parentNode ();
        Node *np = rt->element;
        while (p && SMIL::id_node_body != p->id && !p->role (RoleTypeTiming)) {
            np = p;
            p = p->parentNode ().ptr (); // skip anchors
        }
        if (p && p->active ()) {
            if (rt->timingstate == Runtime::timings_stopped)
                switch (rt->fill_active) {
                    case Runtime::fill_hold: // keep while parent active
                        return true;
                    case Runtime::fill_freeze: // keep in parent duration
                        if (p->unfinished() &&
                                (p->id == SMIL::id_node_par ||
                                 p->id == SMIL::id_node_excl ||
                                 p->id == SMIL::id_node_switch ||
                                 p->lastChild().ptr () == np))
                            return true;
                        // else fall through
                    case Runtime::fill_default:  // as freeze when no duration is set
                    case Runtime::fill_auto:     // or when parent finished w/o duration
                        return keepContent (p) &&
                            (p->id == SMIL::id_node_par ||
                             p->id == SMIL::id_node_excl ||
                             p->id == SMIL::id_node_switch ||
                             p->lastChild().ptr () == np) &&
                            rt->durTime ().durval == Runtime::dur_timer &&
                            !rt->durTime ().offset;
                    default:
                        break;
                }
        }
        return false;
    }
    return true;
}

KDE_NO_CDTOR_EXPORT Runtime::Runtime (Node *e)
 : Role (RoleTypeTiming),
   timingstate (timings_reset),
   repeat_count (0),
   m_StartListeners (new NodeRefList),
   m_StartedListeners (new NodeRefList),
   m_StoppedListeners (new NodeRefList),
   fill_active (fill_auto),
   element (e) {}


KDE_NO_CDTOR_EXPORT Runtime::~Runtime () {
    if (start_timer || duration_timer) // ugh
        reset ();
}

KDE_NO_EXPORT void Runtime::reset () {
    if (start_timer) {
        element->document ()->cancelEvent (start_timer);
        ASSERT (!start_timer);
    }
    if (duration_timer) {
        element->document ()->cancelEvent (duration_timer);
        ASSERT (!duration_timer);
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
    start_time = finish_time = 0;
    fill = fill_default;
    fill_def = fill_inherit;
    fill_active = getDefaultFill (element);
}

KDE_NO_EXPORT
void Runtime::setDurationItem (DurationTime item, const QString & val) {
    int dur = -2; // also 0 for 'media' duration, so it will not update then
    QString vs = val.stripWhiteSpace ();
    QString vl = vs.lower ();
    const char * cval = vl.ascii ();
    int offset = 0;
    //kDebug () << "setDuration1 " << vl;
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
                    kWarning () << "Element not found " << idref;
            }
            //kDebug () << "setDuration q:" << q;
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
                kWarning () << "setDuration no match " << cval;
            if (target && dur != dur_timer) {
                durations [(int) item].connection =
                    target->connectTo (element, dur);
            }
        }
        //kDebug () << "setDuration " << dur << " id:'" << idref << "' off:" << offset;
    }
    durations [(int) item].durval = (Duration) dur;
    durations [(int) item].offset = offset;
}

/**
 * start, or restart in case of re-use, the durations
 */
KDE_NO_EXPORT void Runtime::begin () {
    //kDebug () << "Runtime::begin " << element->nodeName();
    if (start_timer || duration_timer)
        convertNode <Element> (element)->init ();
    timingstate = timings_began;

    int offset = 0;
    bool stop = true;
    if (beginTime ().durval == dur_start) { // check started/finished
        Connection * con = beginTime ().connection.ptr ();
        if (con && con->connectee &&
                con->connectee->state >= Node::state_began) {
            offset = beginTime ().offset;
            Runtime *rt = static_cast <Runtime *> (con->connectee->role (RoleTypeTiming));
            if (rt)
                offset -= element->document()->last_event_time/100 - rt->start_time;
            stop = false;
            kWarning() << "start trigger on started element";
        } // else wait for start event
    } else if (beginTime ().durval == dur_end) { // check finished
        Connection * con = beginTime ().connection.ptr ();
        if (con && con->connectee &&
                con->connectee->state >= Node::state_finished) {
            int offset = beginTime ().offset;
            Runtime *rt = static_cast <Runtime *> (con->connectee->role (RoleTypeTiming));
            if (rt)
                offset -= element->document()->last_event_time/100 - rt->finish_time;
            stop = false;
            kWarning() << "start trigger on finished element";
        } // else wait for end event
    } else if (beginTime ().durval == dur_timer) {
        offset = beginTime ().offset;
        stop = false;
    }
    if (stop)                          // wait for event
        propagateStop (false);
    else if (offset > 0)               // start timer
        start_timer = element->document ()->postEvent (
                element, new TimerEvent (100 * offset, start_timer_id));
    else                               // start now
        propagateStart ();
}

KDE_NO_EXPORT void Runtime::finish () {
    if (started () || timingstate == timings_began) {
        propagateStop (true); // reschedule through Runtime::stopped
    } else {
        finish_time = element->document ()->last_event_time/100;
        NodePtrW guard = element;
        element->Node::finish ();
        if (guard && element->document ()->active ()) // check for reset
            element->propagateEvent (new Event (element, event_stopped));
    }
}

KDE_NO_EXPORT void Runtime::beginAndStart () {
    if (start_timer || duration_timer)
        convertNode <Element> (element)->init ();
    timingstate = timings_began;
    propagateStart ();
}

KDE_NO_EXPORT
bool Runtime::parseParam (const TrieString & name, const QString & val) {
    //kDebug () << "Runtime::parseParam " << name << "=" << val;
    if (name == StringPool::attr_begin) {
        setDurationItem (begin_time, val);
        if ((timingstate == timings_began && !start_timer) ||
                timingstate == timings_stopped) {
            if (beginTime ().offset > 0) { // create a timer for start
                if (start_timer)
                    element->document ()->cancelEvent (start_timer);
                if (beginTime ().durval == dur_timer)
                    start_timer = element->document ()->postEvent (element,
                            new TimerEvent (100 * beginTime ().offset, start_timer_id));
            } else {                              // start now
                propagateStart ();
            }
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
    } else if (name.startsWith (StringPool::attr_fill)) {
        Fill * f = &fill;
        if (name != StringPool::attr_fill) {
            f = &fill_def;
            *f = fill_inherit;
        } else
            *f = fill_default;
        fill_active = fill_auto;
        if (val == "freeze")
            *f = fill_freeze;
        else if (val == "hold")
            *f = fill_hold;
        else if (val == "auto")
            *f = fill_auto;
        else if (val == "remove")
            *f = fill_remove;
        else if (val == "transition")
            *f = fill_transition;
        if (fill == fill_default) {
            if (fill_def == fill_inherit)
                fill_active = getDefaultFill (element);
            else
                fill_active = fill_def;
        } else
            fill_active = fill;
    } else if (name == StringPool::attr_title) {
        Mrl * mrl = static_cast <Mrl *> (element);
        if (mrl)
            mrl->pretty_name = val;
    } else if (name == "endsync") {
        if ((durTime ().durval == dur_media || durTime ().durval == 0) &&
                endTime ().durval == dur_media) {
            Node *e = findLocalNodeById (element, val);
            if (e) {
                durations [(int) end_time].connection =
                    e->connectTo (element, event_stopped);
                durations [(int) end_time].durval = (Duration) event_stopped;
            }
        }
    } else if (name.startsWith ("repeat")) {
        if (val.indexOf ("indefinite") > -1)
            repeat_count = dur_infinite;
        else
            repeat_count = val.toInt ();
    } else
        return false;
    return true;
}

KDE_NO_EXPORT bool Runtime::handleEvent (Event *event) {
    int event_id = event->id ();
    switch (event_id) {
        case event_timer: {
            TimerEvent *te = static_cast <TimerEvent *> (event);
            if (te->event_id == start_timer_id)
                propagateStart ();
            else if (te->event_id == dur_timer_id)
                propagateStop (true);
            else
                kWarning () << "unhandled timer event";
            return true;
        }
        case event_started:
            if (event->source.ptr () == element) {
                setDuration ();
                NodePtrW guard = element;
                element->propagateEvent (event);
                if (guard)
                    element->begin ();
                return true;
            }
            break;
        case event_stopped:
            if (event->source.ptr () == element) {
                stopped ();
                return true;
            }
            break;
    }
    processEvent (event_id);
    return true;
}

KDE_NO_EXPORT void Runtime::processEvent (int event_id) {
    if (!started () && beginTime ().durval == event_id) {
        if (start_timer)
            element->document ()->cancelEvent (start_timer);
        if (element && beginTime ().offset > 0)
            start_timer = element->document ()->postEvent (element,
                    new TimerEvent (100 * beginTime ().offset, start_timer_id));
        else //FIXME neg. offsets
            propagateStart ();
        if (element->state == Node::state_finished)
            element->state = Node::state_activated; // rewind to activated
    } else if (started () && (unsigned int) endTime ().durval == event_id) {
        propagateStop (true);
    }
}

KDE_NO_EXPORT NodeRefListPtr Runtime::listeners (unsigned int id) {
    if (id == event_stopped)
        return m_StoppedListeners;
    else if (id == event_started)
        return m_StartedListeners;
    else if (id == event_to_be_started)
        return m_StartListeners;
    kWarning () << "unknown event requested";
    return NodeRefListPtr ();
}

KDE_NO_EXPORT void Runtime::propagateStop (bool forced) {
    if (state() == timings_reset || state() == timings_stopped)
        return; // nothing to stop
    if (!forced) {
        if (durTime ().durval == dur_media && endTime ().durval == dur_media)
            return; // wait for external eof
        if (endTime ().durval != dur_timer && endTime ().durval != dur_media &&
                (started () || beginTime().durval == dur_timer))
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
    bool was_started (started ());
    timingstate = timings_stopped;
    if (start_timer) {
        element->document ()->cancelEvent (start_timer);
        ASSERT (!start_timer);
    }
    if (duration_timer) {
        element->document ()->cancelEvent (duration_timer);
        ASSERT (!duration_timer);
    }
    if (was_started && element->document ()->active ())
        stopped_timer = element->document()->postEvent (
                element, new Event (element, event_stopped));
    else if (element->unfinished ())
        element->finish ();
}

KDE_NO_EXPORT void Runtime::propagateStart () {
    element->propagateEvent (new Event (element, event_to_be_started));
    if (start_timer)
        element->document ()->cancelEvent (start_timer);
    ASSERT (!start_timer);
    timingstate = timings_started;
    started_timer = element->document()->postEvent(
            element, new Event(element,event_started));
}

/**
 * start_timer timer expired
 */
KDE_NO_EXPORT void Runtime::setDuration () {
    //kDebug () << (element ? element->nodeName() : "-");
    if (start_timer)
        element->document ()->cancelEvent (start_timer);
    if (durTime ().offset > 0 && durTime ().durval == dur_timer) {
        if (duration_timer)
            element->document ()->cancelEvent (duration_timer);
        duration_timer = element->document ()->postEvent
            (element, new TimerEvent (100 * durTime ().offset, dur_timer_id));
    }
    // kDebug () << "Runtime::started set dur timer " << durTime ().offset;
}

/**
 * duration_timer timer expired or no duration set after started
 */
KDE_NO_EXPORT void Runtime::stopped () {
    if (element->active ()) {
        if (repeat_count == dur_infinite || 0 < repeat_count--) {
            if (beginTime ().offset > 0 &&
                    beginTime ().durval == dur_timer) {
                if (start_timer)
                    element->document ()->cancelEvent (start_timer);
                start_timer = element->document ()->postEvent
                    (element, new TimerEvent (100 * beginTime ().offset, start_timer_id));
            } else {
                propagateStart ();
            }
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
    perc_size = 0;
    abs_size = 0;
    isset = false;
}

SizeType & SizeType::operator = (const QString & s) {
    QString strval (s);
    int p = strval.indexOf (QChar ('%'));
    if (p > -1) {
        strval.truncate (p);
        perc_size = strval.toDouble (&isset);
    } else
        abs_size = strval.toDouble (&isset);
    return *this;
}

SizeType & SizeType::operator += (const SizeType & s) {
    perc_size += s.perc_size;
    abs_size += s.abs_size;
    return *this;
}

SizeType & SizeType::operator -= (const SizeType & s) {
    perc_size -= s.perc_size;
    abs_size -= s.abs_size;
    return *this;
}

Single SizeType::size (Single relative_to) const {
    Single s = abs_size;
    s += perc_size * relative_to / 100;
    return s;
}

//-----------------%<----------------------------------------------------------

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

IRect IRect::unite (const IRect & r) const {
    if (isEmpty ())
        return r;
    if (r.isEmpty ())
        return *this;
    int a (x < r.x ? x : r.x);
    int b (y < r.y ? y : r.y);
    return IRect (a, b, 
            ((x + w < r.x + r.w) ? r.x + r.w : x + w) - a,
            ((y + h < r.y + r.h) ? r.y + r.h : y + h) - b);
}

IRect IRect::intersect (const IRect & r) const {
    int a (x < r.x ? r.x : x);
    int b (y < r.y ? r.y : y);
    return IRect (a, b,
            ((x + w < r.x + r.w) ? x + w : r.x + r.w) - a,
            ((y + h < r.y + r.h) ? y + h : r.y + r.h) - b);
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
    // kDebug () << "calc rp:" << reg_point << " ra:" << reg_align <<  " w:" << (int)w << " h:" << (int)h << " xoff:" << (int)xoff << " yoff:" << (int)yoff << " w1:" << (int)w1 << " h1:" << (int)h1;
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

KDE_NO_EXPORT void
CalculatedSizer::move (const SizeType &x, const SizeType &y) {
    if (left.isSet ()) {
        if (right.isSet ()) {
            right += x;
            right -= left;
        }
        left = x;
    } else if (right.isSet ()) {
        right = x;
    } else {
        left = x;
    }
    if (top.isSet ()) {
        if (bottom.isSet ()) {
            bottom += y;
            bottom -= top;
        }
        top = y;
    } else if (bottom.isSet ()) {
            bottom = y;
    } else {
        top = y;
    }
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
    else if (!strcmp (ctag, "animateMotion"))
        return new SMIL::AnimateMotion (d);
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
    //kDebug () << "Smil::activate";
    resolved = true;
    SMIL::Layout * layout = convertNode <SMIL::Layout> (layout_node);
    if (layout && layout->region_surface) {
        kError() << "Layout already has a surface" << endl;
    }
    if (layout)
        Element::activate ();
    else
        Element::deactivate(); // some unfortunate reset in parent doc
}

KDE_NO_EXPORT void SMIL::Smil::deactivate () {
    if (layout_node)
        convertNode <SMIL::Layout> (layout_node)->repaint ();
    state = state_deactivated;
    if (layout_node)
        convertNode <SMIL::Layout> (layout_node)->region_surface = NULL;
    Mrl::getSurface (NULL);
    Mrl::deactivate ();
}

KDE_NO_EXPORT bool SMIL::Smil::handleEvent (Event *event) {
    return layout_node ? layout_node->handleEvent (event) : false;
}

KDE_NO_EXPORT void SMIL::Smil::closed () {
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
            pretty_name = str.left (str.indexOf (QChar ('\n')));
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
        kError () << "no <root-layout>" << endl;
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

KDE_NO_EXPORT bool SMIL::Smil::expose () const {
    return !pretty_name.isEmpty () || //return false if no title and only one
        previousSibling () || nextSibling ();
}

void SMIL::Smil::jump (const QString & id) {
    NodePtr n = document ()->getElementById (this, id, false);
    if (n) {
        if (n->unfinished ())
            kDebug() << "Smil::jump node is unfinished " << id;
        else {
            for (NodePtr p = n; p; p = p->parentNode ()) {
                if (p->unfinished () &&
                        p->id >= id_node_first_group &&
                        p->id <= id_node_last_group) {
                    convertNode <GroupBase> (p)->setJumpNode (n);
                    break;
                }
                if (n->id == id_node_body || n->id == id_node_smil) {
                    kError() << "Smil::jump node passed body for " <<id<< endl;
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
        return new DarkNode (m_doc, ctag, id_node_title);
    else if (!strcmp (ctag, "meta"))
        return new DarkNode (m_doc, ctag, id_node_meta);
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
        root_layout = e;
        return e;
    } else if (!strcmp (ctag, "region"))
        return new SMIL::Region (m_doc);
    else if (!strcmp (ctag, "regPoint"))
        return new SMIL::RegPoint (m_doc);
    return NodePtr ();
}

KDE_NO_EXPORT void SMIL::Layout::closed () {
    SMIL::RegionBase * rl = convertNode <SMIL::RootLayout> (root_layout);
    bool has_root (rl);
    if (!has_root) { // just add one if none there
        rl = new SMIL::RootLayout (m_doc);
        NodePtr sr = rl; // protect against destruction
        rl->setAuxiliaryNode (true);
        root_layout = rl;
        int w_root =0, h_root = 0;
        for (NodePtr n = firstChild (); n; n = n->nextSibling ()) {
            if (n->id == id_node_region) {
                SMIL::Region * rb =convertNode <SMIL::Region> (n);
                rb->init ();
                rb->calculateBounds (0, 0);
                if (int (rb->x + rb->w) > w_root)
                    w_root = rb->x + rb->w;
                if (int (rb->y + rb->h) > h_root)
                    h_root = rb->y + rb->h;
            }
        }
        if (!w_root || !h_root)
            w_root = 320; h_root = 240; // have something to start with
        rl->setAttribute(StringPool::attr_width, QString::number(w_root));
        rl->setAttribute(StringPool::attr_height,QString::number(h_root));
        insertBefore (sr, firstChild ());
    } else {
        Smil *s = Smil::findSmilNode (this);
        if (s) {
            s->width = rl->getAttribute(StringPool::attr_width).toDouble ();
            s->height = rl->getAttribute(StringPool::attr_height).toDouble();
        }
    }
    if (!default_region) {
        SMIL::Region *r = new SMIL::Region (m_doc);
        insertBefore (r, rl->nextSibling ());
        r->setAuxiliaryNode (true);
        default_region = r;
    }
}

KDE_NO_EXPORT void SMIL::Layout::activate () {
    //kDebug () << "SMIL::Layout::activate";
    RegionBase::activate ();
    if (surface ()) {
        updateDimensions ();
        repaint ();
    }
    finish (); // proceed and allow 'head' to finish
}

KDE_NO_EXPORT void SMIL::Layout::updateDimensions () {
    RegionBase * rb = static_cast <RegionBase *> (root_layout.ptr ());
    x = y = 0;
    w = rb->sizes.width.size ();
    h = rb->sizes.height.size ();
    //kDebug () << (int)w << "," << (int)h;
    SMIL::RegionBase::updateDimensions ();
}

KDE_NO_EXPORT Surface *SMIL::Layout::surface () {
    if (!region_surface) {
        SMIL::Smil *s = Smil::findSmilNode (this);
        if (s && s->active ()) {
            SMIL::RegionBase *rl = convertNode <SMIL::RootLayout> (root_layout);
            region_surface = s->getSurface (s);
            w = s->width;
            h = s->height;
            if (region_surface) {
                SRect rect = region_surface->bounds;
                if (rl && rl->auxiliaryNode ()) {
                    w = rect.width ();
                    h = rect.height ();
                    rl->setAttribute (StringPool::attr_width, QString::number ((int)w));
                    rl->setAttribute (StringPool::attr_height, QString::number ((int)h));
                    rl->sizes.width = QString::number ((int) w);
                    rl->setParam (StringPool::attr_height, QString::number((int)h));
                } else if (region_surface && w > 0 && h > 0) {
                    updateDimensions ();
                }
                //kDebug() << "Layout::surface bounds " << rect.width () << "x" << rect.height () << " w:" << w << " h:" << h << " xs:" << region_surface->xscale << " ys:" << region_surface->yscale;
            }
        }
    }
    return region_surface.ptr ();
}

KDE_NO_EXPORT void SMIL::Layout::accept (Visitor * v) {
    v->visit (this);
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT SMIL::RegionBase::RegionBase (NodePtr & d, short id)
 : Element (d, id),
   bg_image (NULL),
   x (0), y (0), w (0), h (0),
   z_order (1), background_color (0)
   {}

KDE_NO_CDTOR_EXPORT SMIL::RegionBase::~RegionBase () {
    if (region_surface)
        region_surface->remove ();
}

KDE_NO_EXPORT void SMIL::RegionBase::activate () {
    show_background = ShowAlways;
    fit = fit_default;
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
    background_image.truncate (0);
    if (region_surface)
        region_surface->background_color = 0;
    if (bg_image) {
        bg_image->destroy ();
        bg_image = NULL;
    }
    postpone_lock = NULL;
    sizes.resetSizes ();
    Element::deactivate ();
}

KDE_NO_EXPORT void SMIL::RegionBase::dataArrived () {
    if (!bg_image->isEmpty () && region_surface)
        region_surface->remove (); // FIXME: only surface
    postpone_lock = 0L;
}

KDE_NO_EXPORT void SMIL::RegionBase::repaint () {
    if (surface ())
        region_surface->repaint (SRect (0, 0, w, h));
}

KDE_NO_EXPORT void SMIL::RegionBase::repaint (const SRect & rect) {
    if (surface ())
        region_surface->repaint (SRect (0, 0, w, h).intersect (rect));
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

KDE_NO_EXPORT void SMIL::RegionBase::boundsUpdate () {
    // if there is a region_surface and it's moved, do a limit repaint
    NodePtr p = parentNode ();
    if (p && (p->id==SMIL::id_node_region || p->id==SMIL::id_node_layout) &&
            region_surface) {
        RegionBase *pr = convertNode <SMIL::RegionBase> (p);
        SRect old_bounds = region_surface->bounds;
        w = 0; h = 0;
        sizes.calcSizes (this, pr->w, pr->h, x, y, w, h);
        region_surface->bounds = SRect (x, y, w, h);
        pr->repaint (region_surface->bounds.unite (old_bounds));
    }
}

KDE_NO_EXPORT Surface *SMIL::RegionBase::surface () {
    if (!region_surface) {
        Node *n = parentNode ().ptr ();
        if (n &&
                (SMIL::id_node_region == n->id ||
                 SMIL::id_node_layout == n->id)) {
            Surface *ps = static_cast <SMIL::Region *> (n)->surface ();
            if (ps) {
                region_surface = ps->createSurface (this, SRect (x, y, w, h));
                region_surface->background_color = background_color;
            }
        }
    }
    return region_surface.ptr ();
}

KDE_NO_EXPORT
void SMIL::RegionBase::parseParam (const TrieString & name, const QString & val) {
    //kDebug () << "RegionBase::parseParam " << getAttribute ("id") << " " << name << "=" << val << " active:" << active();
    bool need_repaint = false;
    SRect rect = SRect (x, y, w, h);
    bool dim_changed;
    if (name == StringPool::attr_fit) {
        fit = parseFit (val.ascii ());
        need_repaint = true;
    } else if (name == "background-color" || name == "backgroundColor") {
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
            if (region_surface) {
                if (dim_changed) {
                    region_surface->remove ();
                } else {
                    boundsUpdate ();
                    return; // smart update of old bounds to new moved one
                }
            }
            NodePtr p = parentNode ();
            if (p &&(p->id==SMIL::id_node_region ||p->id==SMIL::id_node_layout))
                convertNode <SMIL::RegionBase> (p)->updateDimensions ();
            rect = rect.unite (SRect (x, y, w, h));
            need_repaint = true;
        }
    } else if (name == "showBackground") {
        if (val == "whenActive")
            show_background = ShowWhenActive;
        else
            show_background = ShowAlways;
        need_repaint = true;
    } else if (name == "backgroundImage") {
        background_image = val;
        Smil * s = SMIL::Smil::findSmilNode (this);
        if (s) {
            if (!bg_image)
                bg_image = static_cast <ImageMedia *> (
                        document()->notify_listener->mediaManager ()->
                        createMedia (MediaManager::Image, this));
            need_repaint = !bg_image->isEmpty ();
            Mrl *mrl = s->parentNode () ? s->parentNode ()->mrl () : NULL;
            QString url = mrl ? KURL (mrl->absolutePath (), val).url () : val;
            if (!bg_image->wget (url))
                postpone_lock = document ()->postpone ();
            else
                need_repaint = true;
        }
    }
    if (need_repaint && active () && surface() && region_surface->parentNode ())
        region_surface->parentNode ()->repaint (rect);
    Element::parseParam (name, val);
}

bool SMIL::RegionBase::handleEvent (Event *event) {
    if (event->id () == event_media_ready)
        dataArrived ();
    else
        return Element::handleEvent (event);
    return true;
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
    //kDebug () << "Region::calculateBounds parent:" << pw << "x" << ph << " this:" << x << "," << y << " " << w << "x" << h;
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

static struct TransTypeInfo {
    const char *name;
    SMIL::Transition::TransType type;
    short sub_types;
    SMIL::Transition::TransSubType sub_type[8];
} transition_type_info[] = {
#include "transitions.txt"
};

static struct SubTransTypeInfo {
    const char *name;
    SMIL::Transition::TransSubType sub_type;
} sub_transition_type_info[] = {
#include "subtrans.txt"
};

static TransTypeInfo *transInfoFromString (const char *t) {
    // TODO binary search
    for (int i = 0; transition_type_info[i].name; ++i)
        if (!strcmp (t, transition_type_info[i].name))
            return transition_type_info + i;
    return NULL;
}

static
SMIL::Transition::TransSubType subTransInfoFromString (const char *s) {
    for (int i = 0; sub_transition_type_info[i].name; ++i)
        if (!strcmp (s, sub_transition_type_info[i].name))
            return sub_transition_type_info[i].sub_type;
    return SMIL::Transition::SubTransTypeNone;
}

KDE_NO_CDTOR_EXPORT SMIL::Transition::Transition (NodePtr & d)
 : Element (d, id_node_transition),
   type_info (NULL), direction (dir_forward), dur (10), fade_color (0) {}

KDE_NO_EXPORT void SMIL::Transition::activate () {
    type = TransTypeNone;
    sub_type = SubTransTypeNone;
    start_progress = 0.0;
    end_progress = 1.0;
    type_info = NULL;
    init ();
    Element::activate ();
}

KDE_NO_EXPORT
void SMIL::Transition::parseParam (const TrieString & para, const QString & val) {
    if (para == StringPool::attr_type) {
        type_info = transInfoFromString (val.ascii ());
        if (type_info) {
            type = type_info->type;
            if (SubTransTypeNone != sub_type) {
                for (int i = 0; i < type_info->sub_types; ++i)
                    if (type_info->sub_type[i] == sub_type)
                        return;
            }
            if (type_info->sub_types > 0)
                sub_type = type_info->sub_type[0];
        }
    } else if (para == StringPool::attr_dur) {
        parseTime (val, dur);
    } else if (para == "subtype") {
        sub_type = subTransInfoFromString (val.ascii ());
        if (type_info) {
            if (SubTransTypeNone != sub_type) {
                for (int i = 0; i < type_info->sub_types; ++i)
                    if (type_info->sub_type[i] == sub_type)
                        return;
            }
            if (type_info->sub_types > 0)
                sub_type = type_info->sub_type[0];
        }
    } else if (para == "fadeColor") {
        fade_color = QColor (getAttribute (val)).rgb ();
    } else if (para == "direction") {
        direction = val == "reverse" ? dir_reverse : dir_forward;
    } else if (para == "startProgress") {
        start_progress = val.toDouble();
        if (start_progress < 0.0)
            start_progress = 0.0;
        else if (start_progress > 1.0)
            start_progress = 1.0;
    } else if (para == "endProgress") {
        end_progress = val.toDouble();
        if (end_progress < start_progress)
            end_progress = start_progress;
        else if (end_progress > 1.0)
            end_progress = 1.0;
    } else {
        Element::parseParam (para, val);
    }
}

KDE_NO_EXPORT bool SMIL::Transition::supported () {
    switch (type) {
        case Fade:
        case BarWipe:
        case BowTieWipe:
        case PushWipe:
        case IrisWipe:
        case ClockWipe:
        case EllipseWipe:
            return true;
        default:
            return false;
    }
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT SMIL::GroupBase::GroupBase (NodePtr & d, short id)
 : Element (d, id),
   runtime (new Runtime (this)),
   inited (false) {}

KDE_NO_CDTOR_EXPORT SMIL::GroupBase::~GroupBase () {
    delete runtime;
}

KDE_NO_EXPORT NodePtr SMIL::GroupBase::childFromTag (const QString & tag) {
    Element * elm = fromScheduleGroup (m_doc, tag);
    if (!elm) elm = fromMediaContentGroup (m_doc, tag);
    if (!elm) elm = fromContentControlGroup (m_doc, tag);
    if (!elm) elm = fromAnimateGroup (m_doc, tag);
    if (elm)
        return elm;
    return NULL;
}

KDE_NO_EXPORT void SMIL::GroupBase::init () {
    if (!inited) {
        runtime->reset ();
        Element::init ();
        inited = true;
    }
}

KDE_NO_EXPORT void SMIL::GroupBase::finish () {
    setState (state_finished); // avoid recurstion through childDone
    for (NodePtr e = firstChild (); e; e = e->nextSibling ())
        if (keepContent (e)) {
            if (e->unfinished ())
                e->finish ();
        } else if (e->active ())
            e->deactivate ();
    runtime->finish ();
}

static bool childMediaTypeReady (Node *p) {
    for (Node *c = p->firstChild ().ptr (); c; c = c->nextSibling ().ptr ())
        if (c->id >= SMIL::id_node_first_mediatype &&
                c->id <= SMIL::id_node_last_mediatype) {
            SMIL::MediaType *mt = static_cast <SMIL::MediaType *> (c);
            if (mt->media_object && mt->media_object->downloading ())
                return false;
        }
    return true;
}

Role *SMIL::GroupBase::role (RoleType rt) {
    switch (rt) {
        case RoleTypeTiming:
            return runtime;
        default:
            break;
    }
    return Element::role (rt);
}

namespace {

class KMPLAYER_NO_EXPORT GroupBaseInitVisitor : public Visitor {
public:
    using Visitor::visit;

    void visit (Node *n) {
        Node *s = n->nextSibling ().ptr ();
        if (s)
            s->accept (this);
    }
    void visit (Element *elm) {
        if (elm->role (RoleTypeTiming))
            elm->init ();
        visit (static_cast <Node *> (elm));
    }
    void visit (SMIL::PriorityClass *pc) {
        pc->init ();
        Node *n = pc->firstChild ().ptr ();
        if (n)
            n->accept (this);
        visit (static_cast <Node *> (pc));
    }
};

}

KDE_NO_EXPORT void SMIL::GroupBase::activate () {
    init ();
    setState (state_activated);
    runtime->begin ();
    Node *n = firstChild ().ptr ();
    if (n) {
        GroupBaseInitVisitor visitor;
        n->accept (&visitor);
    }
}

KDE_NO_EXPORT
void SMIL::GroupBase::parseParam (const TrieString &para, const QString &val) {
    if (!runtime->parseParam (para, val))
        Element::parseParam (para, val);
}

KDE_NO_EXPORT void SMIL::GroupBase::begin () {
    if (!childMediaTypeReady (this))
        postpone_lock = document ()->postpone ();
}

KDE_NO_EXPORT bool SMIL::GroupBase::handleEvent (Event *event) {
    if (event->id () == event_media_ready) {
        if (postpone_lock && childMediaTypeReady (this))
            postpone_lock = NULL;
    } else {
        return runtime->handleEvent (event);
    }
    return true;
}

KDE_NO_EXPORT NodeRefListPtr SMIL::GroupBase::listeners (unsigned int id) {
    return runtime->listeners (id);
}

KDE_NO_EXPORT void SMIL::GroupBase::deactivate () {
    setState (state_deactivated); // avoid recurstion through childDone
    for (NodePtr e = firstChild (); e; e = e->nextSibling ())
        if (e->active ())
            e->deactivate ();
    postpone_lock = NULL;
    if (unfinished ())
        finish ();
    runtime->reset ();
    Element::deactivate ();
}

KDE_NO_EXPORT void SMIL::GroupBase::reset () {
    Element::reset ();
    inited = false;
    runtime->reset ();
}

KDE_NO_EXPORT void SMIL::GroupBase::setJumpNode (NodePtr n) {
    NodePtr child = n;
    if (state > state_init) {
        state = state_deferred;
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
    for (NodePtr n = firstChild (); n; n = n->nextSibling ())
        if (n->role (RoleTypeTiming))
            convertNode <Element> (n)->init ();
    runtime->beginAndStart (); // undefer through begin()
}

//-----------------------------------------------------------------------------

// SMIL::Body was here

//-----------------------------------------------------------------------------

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
        if (runtime->started ()) {
            Runtime::Duration dv = runtime->durTime ().durval;
            if ((dv == Runtime::dur_timer && !runtime->durTime ().offset)
                    || dv == Runtime::dur_media)
                runtime->propagateStop (false);
            return; // still running, wait for runtime to finish
        }
        finish (); // we're done
    }
}

//-----------------------------------------------------------------------------

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

KDE_NO_EXPORT NodePtr SMIL::Excl::childFromTag (const QString &tag) {
    if (tag == "priorityClass")
        return new PriorityClass (m_doc);
    return GroupBase::childFromTag (tag);
}

namespace {

class KMPLAYER_NO_EXPORT ExclActivateVisitor : public Visitor {
    SMIL::Excl *excl;
public:
    ExclActivateVisitor (SMIL::Excl *ex) : excl (ex) {}

    using Visitor::visit;

    void visit (Node *n) {
        Node *s = n->nextSibling ().ptr ();
        if (s)
            s->accept (this);
    }
    void visit (Element *elm) {
        if (elm->role (RoleTypeTiming)) {
            // make aboutToStart connection with Timing
            ConnectionPtr c = elm->connectTo (excl, event_to_be_started);
            excl->started_event_list.append (
                    new SMIL::Excl::ConnectionStoreItem (c));
            elm->activate ();
        }
        visit (static_cast <Node *> (elm));
    }
    void visit (SMIL::PriorityClass *pc) {
        pc->state = Node::state_activated;
        Node *n = pc->firstChild ().ptr ();
        if (n)
            n->accept (this);
        visit (static_cast <Node *> (pc));
    }
};

class KMPLAYER_NO_EXPORT ExclPauseVisitor : public Visitor {
    bool pause;
    Node *paused_by;
    unsigned int cur_time;

    void updatePauseStateEvent (Event *event, int pause_time) {
        if (event) {
            if (pause)
                paused_by->document ()->pauseEvent (event);
            else
                paused_by->document ()->unpauseEvent (event, (cur_time-pause_time)*100);
        }
    }
    static Event *activeEvent (Runtime *r) {
        Event *event = NULL;
        if (r->start_timer)
            event = r->start_timer.ptr ();
        else if (r->started_timer)
            event = r->started_timer.ptr ();
        else if (r->duration_timer)
            event = r->duration_timer.ptr ();
        else if (r->stopped_timer)
            event = r->stopped_timer.ptr ();
        return event;
    }

public:
    ExclPauseVisitor (bool p, Node *pb, unsigned int pt)
        : pause(p), paused_by (pb), cur_time (pt) {}
    ~ExclPauseVisitor () {
        paused_by->document ()->updateTimeout ();
    }

    using Visitor::visit;

    void visit (Node *node) {
        for (Node *c = node->firstChild (); c; c = c->nextSibling ())
            c->accept (this);
    }
    void visit (Element *elm) {
        if (!elm->active ())
            return; // nothing to do
        Runtime *rt = static_cast <Runtime *> (elm->role (RoleTypeTiming));
        if (rt) {
            if (pause) {
                rt->paused_time = cur_time;
                rt->paused_by = paused_by;
                rt->unpaused_state = rt->timingstate;
                rt->timingstate = Runtime::timings_paused;
            } else {
                rt->paused_by = NULL;
                rt->timingstate = rt->unpaused_state;
                rt->start_time += cur_time;
            }
            updatePauseStateEvent (activeEvent (rt), rt->paused_time);
        }
        visit (static_cast <Node *> (elm));
    }
    void visit (SMIL::MediaType *mt) {
        if (mt->media_object) {
            if (pause)
                mt->media_object->pause ();
            else
                mt->media_object->unpause ();
            if (mt->surface ())
                mt->sub_surface->repaint ();
        }

        Event *event = NULL;
        if (mt->trans_timer)
            event = mt->trans_timer.ptr ();
        else if (mt->trans_out_timer)
            event = mt->trans_out_timer.ptr ();
        updatePauseStateEvent (event, mt->runtime->paused_time);

        visit (static_cast <Element *> (mt));
    }
    void visit (SMIL::Animate *an) {
        updatePauseStateEvent(an->anim_timer.ptr(), an->runtime->paused_time);
        visit (static_cast <Element *> (an));
    }
    void visit (SMIL::AnimateMotion *an) {
        updatePauseStateEvent(an->anim_timer.ptr(), an->runtime->paused_time);
        visit (static_cast <Element *> (an));
    }
    void visit (SMIL::Smil *s) {
        for (Node *c = s->firstChild (); c; c = c->nextSibling ())
            if (SMIL::id_node_body == c->id)
                c->accept (this);
    }
};

}

KDE_NO_EXPORT void SMIL::Excl::begin () {
    //kDebug () << "SMIL::Excl::begin";
    Node *n = firstChild ().ptr ();
    if (n) {
        ExclActivateVisitor visitor (this);
        n->accept (&visitor);
    }
    GroupBase::begin ();
}

KDE_NO_EXPORT void SMIL::Excl::deactivate () {
    started_event_list.clear (); // auto disconnect on destruction of data items
    priority_queue.clear ();
    stopped_connection = NULL;
    GroupBase::deactivate ();
}

KDE_NO_EXPORT void SMIL::Excl::childDone (NodePtr /*child*/) {
    // do nothing
}

KDE_NO_EXPORT bool SMIL::Excl::handleEvent (Event *event) {
    if (event->id () == event_to_be_started) {
        if (event->source == cur_node)
            return true; // eg. repeating
        Node *n = cur_node.ptr ();
        cur_node = event->source;
        stopped_connection = cur_node->connectTo (this, event_stopped);
        if (n) {
            if (SMIL::id_node_priorityclass == cur_node->parentNode ()->id) {
                switch (static_cast <SMIL::PriorityClass *>
                        (cur_node->parentNode ().ptr ())->peers) {
                    case PriorityClass::PeersPause: {
                        ExclPauseVisitor visitor (
                                true, this, document ()->last_event_time/100);
                        n->accept (&visitor);
                        priority_queue.insertBefore (
                                new NodeRefItem (n), priority_queue.first ());
                        return true;
                    }
                    default:
                        break; //TODO
                }
            }
            static_cast<Runtime*>(n->role(RoleTypeTiming))->propagateStop(true);
        }
        return true;
    } else if (event->id () == event_stopped && event->source.ptr () != this) {
        ASSERT (event->source == cur_node);

        NodeRefItemPtr ref = priority_queue.first ();
        while (ref && (!ref->data || !ref->data->active ())) {
            // should not happen, but consider a backend crash or so
            priority_queue.remove (ref);
            ref = priority_queue.first ();
        }
        if (ref) {
            cur_node = ref->data;
            priority_queue.remove (ref);
            stopped_connection = cur_node->connectTo (this, event_stopped);
            ExclPauseVisitor visitor (false, this, document()->last_event_time/100);
            cur_node->accept (&visitor);
            // else TODO
        } else {
            cur_node = NULL;
            stopped_connection = NULL;
            // now finish unless 'dur="indefinite/some event/.."'
            if (runtime->started ())
                runtime->propagateStop (false); // wait for runtime to finish
            else
                finish (); // we're done
        }
    } else
        return GroupBase::handleEvent (event);
    return true;
}

//-----------------------------------------------------------------------------

KDE_NO_EXPORT NodePtr SMIL::PriorityClass::childFromTag (const QString &tag) {
    Element * elm = fromScheduleGroup (m_doc, tag);
    if (!elm) elm = fromMediaContentGroup (m_doc, tag);
    if (!elm) elm = fromContentControlGroup (m_doc, tag);
    if (!elm) elm = fromAnimateGroup (m_doc, tag);
    if (elm)
        return elm;
    return NULL;
}

KDE_NO_EXPORT void
SMIL::PriorityClass::parseParam (const TrieString &name, const QString &val) {
    if (name == "peers") {
        if (val == "pause")
            peers = PeersPause;
        else if (val == "defer")
            peers = PeersDefer;
        else if (val == "never")
            peers = PeersNever;
        else
            peers = PeersStop;
    } else if (name == "higher") {
        if (val == "stop")
            higher = HigherStop;
        else
            higher = HigherPause;
    } else if (name == "lower") {
        if (val == "never")
            lower = LowerNever;
        else
            lower = LowerDefer;
    } else if (name == "pauseDisplay") {
        if (val == "disable")
            pause_display = PauseDisplayDisable;
        else if (val == "hide")
            pause_display = PauseDisplayHide;
        else
            pause_display = PauseDisplayShow;
    }
}

KDE_NO_EXPORT void SMIL::PriorityClass::init () {
    //if (!inited) {
    peers = PeersStop;
    higher = HigherPause;
    lower = LowerDefer;
    pause_display = PauseDisplayShow;
    Element::init ();
}

KDE_NO_EXPORT void SMIL::PriorityClass::childDone (NodePtr /*child*/) {
    // do nothing
}

//-----------------------------------------------------------------------------

KDE_NO_EXPORT void SMIL::Switch::begin () {
    //kDebug () << "SMIL::Switch::activate";
    PlayListNotify * n = document()->notify_listener;
    int pref = 0, max = 0x7fffffff, currate = 0;
    if (n)
        n->bitRates (pref, max);
    if (firstChild ()) {
        NodePtr fallback;
        for (NodePtr e = firstChild (); e; e = e->nextSibling ())
            if (e->isElementNode ()) {
                Element *elm = convertNode <Element> (e);
                QString lang = elm->getAttribute ("systemLanguage");
                if (!lang.isEmpty ()) {
                    lang = lang.replace (QChar ('-'), QChar ('_'));
                    char *clang = getenv ("LANG");
                    if (!clang) {
                        if (!fallback)
                            fallback = e;
                    } else if (QString (clang).lower ().startsWith (lang)) {
                        chosenOne = e;
                    } else if (!fallback) {
                        fallback = e->nextSibling ();
                    }
                }
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
        chosenOne->activate ();
    }
    GroupBase::begin ();
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
    //kDebug () << "SMIL::Switch::childDone";
    finish (); // only one child can run
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
    } else if (para == StringPool::attr_target) {
        target = val;
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
 : Mrl (d, id),
   runtime (new Runtime (this)),
   m_type (t),
   bitrate (0),
   trans_start_time (0),
   sensitivity (sens_opaque),
   trans_out_active (false),
   has_mouse (false),
   m_MediaAttached (new NodeRefList),
   inited (false) {
    view_mode = Mrl::WindowMode;
}

KDE_NO_CDTOR_EXPORT SMIL::MediaType::~MediaType () {
    delete runtime;
}

KDE_NO_EXPORT NodePtr SMIL::MediaType::childFromTag (const QString & tag) {
    Element * elm = fromContentControlGroup (m_doc, tag);
    if (!elm) elm = fromParamGroup (m_doc, tag);
    if (!elm) elm = fromAnimateGroup (m_doc, tag);
    if (elm)
        return elm;
    return 0L;
}

static NodePtr findExternalTree (Mrl *mrl) {
    for (NodePtr c = mrl->firstChild (); c; c = c->nextSibling ()) {
        Mrl * m = c->mrl ();
        if (m && (m->opener.ptr () == mrl ||
                    m->id == SMIL::id_node_smil ||
                    m->id == RP::id_node_imfl))
            return c;
    }
    return 0L;
}

KDE_NO_EXPORT void SMIL::MediaType::closed () {
    external_tree = findExternalTree (this);
    Mrl *mrl = external_tree ? external_tree->mrl () : NULL;
    if (mrl) {
        width = mrl->width;
        height = mrl->height;
    }
    pretty_name = getAttribute (StringPool::attr_title);
    Mrl::closed ();
}

KDE_NO_EXPORT
void SMIL::MediaType::parseParam (const TrieString &para, const QString & val) {
    bool update_surface = true;
    if (para == StringPool::attr_fit) {
        fit = parseFit (val.ascii ());
    } else if (para == StringPool::attr_type) {
        mimetype = val;
    } else if (para == "rn:mediaOpacity") {
        opacity = (int) SizeType (val).size (100);
    } else if (para == "system-bitrate") {
        bitrate = val.toInt ();
    } else if (para == "transIn") {
        trans_in = findTransition (this, val);
        if (!trans_in)
            kWarning() << "Transition " << val << " not found in head";
    } else if (para == "transOut") {
        trans_out = findTransition (this, val);
        if (!trans_out)
            kWarning() << "Transition " << val << " not found in head";
    } else if (para == "sensitivity") {
        if (val == "transparent")
            sensitivity = sens_transparent;
        //else if (val == "percentage") // TODO
        //    sensitivity = sens_percentage;
        else
            sensitivity = sens_opaque;
    } else if (sizes.setSizeParam (para, val, update_surface)) {
        SMIL::RegionBase *rb = convertNode <SMIL::RegionBase> (region_node);
        Fit ft = rb && fit_default == fit ? rb->fit : fit;
        if ((!update_surface || fit_default == ft || fit_hidden == ft) &&
                sub_surface
#ifdef KMPLAYER_WITH_CAIRO
                && sub_surface->surface
#endif
                ) {
            boundsUpdate ();
            return; // preserved surface by recalculationg sub_surface top-left
        }
    } else if (!runtime->parseParam (para, val)) {
        if (para != StringPool::attr_src)
            Mrl::parseParam (para, val);
    }
    if (sub_surface)
        sub_surface->repaint ();
    resetSurface ();
    if (surface ())
        sub_surface->repaint ();
}

Role *SMIL::MediaType::role (RoleType rt) {
    switch (rt) {
        case RoleTypeTiming:
            return runtime;
        default:
            break;
    }
    return Mrl::role (rt);
}

KDE_NO_EXPORT void SMIL::MediaType::boundsUpdate () {
    SMIL::RegionBase *rb = convertNode <SMIL::RegionBase> (region_node);
    if (rb && sub_surface) {
        SRect new_bounds = calculateBounds ();
        SRect repaint_rect = sub_surface->bounds.unite (new_bounds);
        sub_surface->bounds = new_bounds;
        rb->repaint (repaint_rect);
    }
}

KDE_NO_EXPORT void SMIL::MediaType::init () {
    if (!inited) {
        trans_out_active = false;
        trans_start_time = 0;
        fit = fit_default;
        opacity = 100;
        runtime->reset ();
        Mrl::init (); // sets all attributes
        inited = true;
    }
}

KDE_NO_EXPORT void SMIL::MediaType::activate () {
    init (); // sets all attributes
    setState (state_activated);
    runtime->begin ();
}

KDE_NO_EXPORT void SMIL::MediaType::deactivate () {
    region_paint = 0L;
    region_mouse_enter = 0L;
    region_mouse_leave = 0L;
    region_mouse_click = 0L;
    region_attach = 0L;
    if (region_node)
        convertNode <SMIL::RegionBase> (region_node)->repaint ();
    if (trans_timer)
        document ()->cancelEvent (trans_timer);
    document ()->notify_listener->removeRepaintUpdater (this);
    if (trans_out_timer)
        document ()->cancelEvent (trans_out_timer);
    if (unfinished ())
        finish ();
    runtime->reset ();
    Mrl::deactivate ();
    region_node = 0L;
    if (media_object) {
        media_object->destroy ();
        media_object = NULL;
    }
    postpone_lock = 0L;
}

KDE_NO_EXPORT void SMIL::MediaType::defer () {
    if (media_object) {
        //media_object->pause ();
        if (unfinished ())
            postpone_lock = document ()->postpone ();
        setState (state_deferred);
    }
}

KDE_NO_EXPORT void SMIL::MediaType::undefer () {
    if (runtime->started ()) {
        setState (state_began);
        if (media_object)
            media_object->unpause ();
        if (surface ())
            sub_surface->repaint ();
    } else {
        setState (state_activated);
    }
    postpone_lock = 0L;
}

KDE_NO_EXPORT void SMIL::MediaType::begin () {
    SMIL::Smil * s = Smil::findSmilNode (parentNode ().ptr ());
    SMIL::Region * r = s ?
        findRegion (s->layout_node, param (StringPool::attr_region)) : 0L;
    if (!r)
        r = convertNode <SMIL::Region> (convertNode <SMIL::Layout> (s->layout_node)->default_region);
    if (trans_timer) // eg transOut and we're repeating
        document ()->cancelEvent (trans_timer);
    for (NodePtr c = firstChild (); c; c = c->nextSibling ())
        if (c != external_tree)
            c->activate (); // activate param/set/animate.. children
    if (r) {
        region_node = r;
        region_mouse_enter = r->connectTo (this, event_inbounds);
        region_mouse_leave = r->connectTo (this, event_outbounds);
        region_mouse_click = r->connectTo (this, event_activated);
        region_attach = r->connectTo (this, mediatype_attached);
        r->repaint ();
        clipStart ();
        Transition * trans = convertNode <Transition> (trans_in);
        if (trans && trans->supported ()) {
            active_trans = trans_in;
            trans_gain = 0.0;
            document ()->notify_listener->addRepaintUpdater (this);
            trans_start_time = document ()->last_event_time;
            trans_end_time = trans_start_time + 100 * trans->dur;
        }
        if (Runtime::dur_timer == runtime->durTime().durval &&
                runtime->durTime().offset > 0) {
            // FIXME: also account for fill duration
            trans = convertNode <Transition> (trans_out);
            if (trans && trans->supported () &&
                    (int) trans->dur < runtime->durTime().offset)
                trans_out_timer = document()->postEvent (
                        this,
                        new TimerEvent ((runtime->durTime().offset - trans->dur) * 100,
                        trans_out_timer_id));
        }
    } else {
        kWarning () << nodeName() << "::begin " << src << " region '" <<
            param (StringPool::attr_region) << "' not found" << endl;
    }
    runtime->start_time = document ()->last_event_time/100;
    Element::begin ();
    runtime->propagateStop (false); //see whether this node has a livetime or not
}

KDE_NO_EXPORT void SMIL::MediaType::clipStart () {
    SMIL::RegionBase *r = convertNode<SMIL::RegionBase> (region_node);
    if (r && r->surface ()) {
        if (external_tree)
            external_tree->activate ();
        else if (media_object)
            media_object->play ();
    }
}

KDE_NO_EXPORT void SMIL::MediaType::clipStop () {
    if (media_object)
        media_object->stop ();
    resetSurface ();
    if (external_tree && external_tree->active ())
        external_tree->deactivate ();
    document_postponed = 0L;
}

KDE_NO_EXPORT void SMIL::MediaType::finish () {
    document ()->notify_listener->removeRepaintUpdater (this);
    if (region_node)
        convertNode <SMIL::RegionBase> (region_node)->repaint ();
    runtime->finish ();
    clipStop ();
}

KDE_NO_EXPORT void SMIL::MediaType::reset () {
    Mrl::reset ();
    inited = false;
    runtime->reset ();
}

/**
 * Re-implement from Mrl, because we may have children like
 * param/set/animatie that should all be activate, but also other smil or imfl
 * documents, that should only be activated if the runtime has started
 */
KDE_NO_EXPORT void SMIL::MediaType::childDone (NodePtr child) {
    bool child_doc = child->mrl () && child->mrl ()->opener.ptr () == this;
    if (child_doc) {
        child->deactivate (); // should only if fill not is freeze or hold
    } else if (active ()) {
        if (runtime->state () < Runtime::timings_stopped) {
            if (runtime->started ())
                runtime->propagateStop (false); // what about repeat_count ..
            return; // still running, wait for runtime to finish
        }
    }
    if (active ())
        finish ();
}

Surface *SMIL::MediaType::getSurface (Mrl *mrl) {
    resetSurface ();
    Surface *s = NULL;
    if (mrl) {
        width = mrl->width;
        height = mrl->height;
        s = surface ();
        if (s)
            s->node = mrl;
    }
    return s;
}

KDE_NO_EXPORT Surface *SMIL::MediaType::surface () {
    if (!keepContent (this)) {
        resetSurface ();
        return NULL;
    }
    if (!sub_surface) {
        SMIL::RegionBase *rb = convertNode <SMIL::RegionBase> (region_node);
        if (rb && rb->surface ()) {
            SRect rect = calculateBounds ();
            sub_surface =rb->region_surface->createSurface (this, rect);
            if (width > 0 && height > 0) {
                sub_surface->xscale = 1.0 * rect.width () / width;
                sub_surface->yscale = 1.0 * rect.height () / height;
            }
            //kDebug() << sub_surface.ptr() << " " << nodeName() << " " << src << " " << rr.width() << "," << rr.height()  << " => " << x << "," << y << w << "," << h;
        }
    }
    return sub_surface.ptr ();
}

KDE_NO_EXPORT void SMIL::MediaType::resetSurface () {
    if (sub_surface)
        sub_surface->remove ();
    sub_surface = NULL;
}

KDE_NO_EXPORT SRect SMIL::MediaType::calculateBounds () {
    SMIL::RegionBase *rb = convertNode <SMIL::RegionBase> (region_node);
    if (rb && rb->surface ()) {
        SRect rr = rb->region_surface->bounds;
        Single x, y, w = width, h = height;
        sizes.calcSizes (this, rr.width(), rr.height(), x, y, w, h);
        Fit ft = fit_default == fit ? rb->fit : fit;
        if (width > 0 && height > 0 && w > 0 && h > 0)
            switch (ft) {
                case fit_meet: {
                    float iasp = 1.0 * width / height;
                    float rasp = 1.0 * w / h;
                    if (iasp > rasp)
                        h = height * w / width;
                    else
                        w = width * h / height;
                    break;
                }
                case fit_scroll:
                case fit_default:
                case fit_hidden:
                     w = width;
                     h = height;
                     break;
                case fit_slice: {
                    float iasp = 1.0 * width / height;
                    float rasp = 1.0 * w / h;
                    if (iasp > rasp)
                        w = width * h / height;
                    else
                        h = height * w / width;
                    break;
                }
                default: {} // fit_fill
            }
        return SRect (x, y, w, h);
    }
    return SRect ();
}

bool SMIL::MediaType::handleEvent (Event *event) {
    Surface *s = surface();
    switch (event->id ()) {
        case event_postponed: {
            PostponedEvent * pe = static_cast <PostponedEvent *> (event);
            if (media_object) {
                if (pe->is_postponed) {
                    if (unfinished ()) {
                        setState (state_deferred);
                        media_object->pause ();
                    }
                } else if (state == Node::state_deferred) {
                    setState (state_began);
                    media_object->unpause ();
                }
            }
            return true;
        }
        case event_update: {
            UpdateEvent *ue = static_cast <UpdateEvent *> (event);

            trans_start_time += ue->skipped_time;
            trans_end_time += ue->skipped_time;

            trans_gain = 1.0 * (ue->cur_event_time - trans_start_time) /
                               (trans_end_time - trans_start_time);
            if (trans_gain > 0.9999) {
                document ()->notify_listener->removeRepaintUpdater (this);
                if (!trans_out_active)
                    active_trans = NULL;
                trans_gain = 1.0;
            }
            if (s && s->parentNode())
                s->parentNode()->repaint (s->bounds);
            return true;
        }
        case event_timer: {
            TimerEvent *te = static_cast <TimerEvent *> (event);
            if (te->event_id == trans_out_timer_id) {
                if (active_trans)
                    document ()->notify_listener->removeRepaintUpdater (this);
                trans_out_timer = NULL;
                active_trans = trans_out;
                Transition * trans = convertNode <Transition> (trans_out);
                if (trans) {
                    trans_gain = 0.0;
                    document ()->notify_listener->addRepaintUpdater (this);
                    trans_start_time = document ()->last_event_time;
                    trans_end_time = trans_start_time + 100 * trans->dur;
                    trans_out_active = true;
                    if (s)
                        s->repaint ();
                }
                return true;
            }
        } // fall through
        default:
            return runtime->handleEvent (event);
    }
}

KDE_NO_EXPORT NodeRefListPtr SMIL::MediaType::listeners (unsigned int id) {
    NodeRefListPtr l = mouse_listeners.listeners (id);
    if (l)
        return l;
    switch (id) {
        case mediatype_attached:
            return m_MediaAttached;
    }
    return runtime->listeners (id);
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT
SMIL::AVMediaType::AVMediaType (NodePtr & d, const QString & t)
 : SMIL::MediaType (d, t, id_node_audio_video) {}

KDE_NO_EXPORT NodePtr SMIL::AVMediaType::childFromTag (const QString & tag) {
    return fromXMLDocumentTag (m_doc, tag);
}

KDE_NO_EXPORT void SMIL::AVMediaType::clipStart () {
    PlayListNotify *n = document ()->notify_listener;
    //kDebug() << resolved;
    if (n && region_node && !external_tree && !src.isEmpty()) {
        repeat = runtime->repeat_count == Runtime::dur_infinite
            ? 9998 : runtime->repeat_count;
        runtime->repeat_count = 0;
        document_postponed = document()->connectTo (this, event_postponed);
    }
    MediaType::clipStart ();
}

KDE_NO_EXPORT void SMIL::AVMediaType::clipStop () {
    if (runtime->durTime ().durval == Runtime::dur_media)
        runtime->durTime ().durval = Runtime::dur_timer;//reset to make this finish
    MediaType::clipStop ();
}

KDE_NO_EXPORT void SMIL::AVMediaType::begin () {
    if (!external_tree && !resolved) {
        defer ();
    } else {
        if (0 == runtime->durTime ().offset &&
            Runtime::dur_media == runtime->endTime ().durval)
            runtime->durTime ().durval = Runtime::dur_media; // duration of clip
        if (!external_tree && !media_object)
            media_object = document ()->notify_listener->mediaManager()->
                createMedia (MediaManager::AudioVideo, this);
        MediaType::begin ();
    }
}

KDE_NO_EXPORT void SMIL::AVMediaType::undefer () {
    if (runtime->started () && resolved && !media_object)
        begin ();
    else
        MediaType::undefer ();
}

KDE_NO_EXPORT void SMIL::AVMediaType::endOfFile () {
    if (!active())
        return; // backend eof after a reset
    if (media_object) {
        media_object->destroy ();
        media_object = NULL;
    }
    postpone_lock = 0L;
    runtime->propagateStop (true);
}

void
SMIL::AVMediaType::parseParam (const TrieString &name, const QString &val) {
    //kDebug () << "AudioVideoData::parseParam " << name << "=" << val << endl;
    if (name == StringPool::attr_src) {
        if (!resolved || src != val) {
            if (external_tree)
                removeChild (external_tree);
            src = val;
            resolved = document ()->notify_listener->resolveURL (this);
        }
        if (state == state_began && resolved)
            clipStart ();
    } else {
        MediaType::parseParam (name, val);
    }
}

KDE_NO_EXPORT void SMIL::AVMediaType::accept (Visitor * v) {
    v->visit (this);
}

KDE_NO_EXPORT bool SMIL::AVMediaType::expose () const {
    return !src.isEmpty () && !external_tree;
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT
SMIL::ImageMediaType::ImageMediaType (NodePtr & d)
    : SMIL::MediaType (d, "img", id_node_img) {}

KDE_NO_EXPORT NodePtr SMIL::ImageMediaType::childFromTag (const QString & tag) {
    if (!strcmp (tag.latin1 (), "imfl"))
        return new RP::Imfl (m_doc);
    return SMIL::MediaType::childFromTag (tag);
}

KDE_NO_EXPORT void SMIL::ImageMediaType::activate () {
    PlayListNotify *n = document ()->notify_listener;
    if (n && !media_object)
        media_object = n->mediaManager()->createMedia(MediaManager::Image,this);
    MediaType::activate ();
}

KDE_NO_EXPORT void SMIL::ImageMediaType::begin () {
    ImageMedia *im = static_cast <ImageMedia *> (media_object);
    if (im && im->downloading ()) { // FIXME we shouldn't loose media_object after a pause
        postpone_lock = document ()->postpone ();
        state = state_began;
        return;
    }
    MediaType::begin ();
}

KDE_NO_EXPORT void SMIL::ImageMediaType::accept (Visitor * v) {
    v->visit (this);
}

void
SMIL::ImageMediaType::parseParam (const TrieString &name, const QString &val) {
    //kDebug () << "ImageRuntime::param " << name << "=" << val << endl;
    if (name == StringPool::attr_src) {
        ImageMedia *im = static_cast <ImageMedia *> (media_object);
        if (im)
            im->killWGet ();
        if (external_tree)
            removeChild (external_tree);
        src = val;
        if (!src.isEmpty ()) {
            if (!media_object) {
                media_object = document ()->notify_listener->
                    mediaManager()->createMedia(MediaManager::Image,this);
                im = static_cast <ImageMedia *> (media_object);
            }
            im->wget (absolutePath ());
        } else if (im) {
            im->clearData ();
        }
    } else {
        MediaType::parseParam (name, val);
    }
}

void SMIL::ImageMediaType::dataArrived () {
    ImageMedia *im = static_cast <ImageMedia *> (media_object);
    resetSurface ();
    QString mime = im->mimetype ();
    kDebug () << "ImageMediaType::dataArrived " << mime << " empty:" << im->isEmpty () << " " << src << " " << state << endl;
    if (mime.startsWith (QString::fromLatin1 ("text/"))) {
        QTextStream ts (im->rawData (), IO_ReadOnly);
        readXML (this, ts, QString ());
        Mrl *mrl = external_tree ? external_tree->mrl () : NULL;
        if (mrl) {
            width = mrl->width;
            height = mrl->height;
        }
    } else if (!im->isEmpty ()) {
        width = im->cached_img->width;
        height = im->cached_img->height;
        if (surface ())
            sub_surface->repaint ();
    }
    postpone_lock = 0L;
    if (state == state_began)
        MediaType::begin ();
}

bool SMIL::ImageMediaType::handleEvent (Event *event) {
    ImageMedia *im = static_cast <ImageMedia *> (media_object);
    if (im && event->id () == event_img_updated) {
        resetSurface ();
        if (surface ())
            sub_surface->repaint ();
        if (state >= state_finished)
            clipStop ();
    } else if (im && event->id () == event_img_anim_finished) {
        if (state >= Node::state_began)
            runtime->propagateStop (false);
    } else if (im && event->id () == event_media_ready) {
        dataArrived ();
        if (parentNode ())
            parentNode ()->handleEvent (event);
    } else {
        return MediaType::handleEvent (event);
    }
    return true;
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT SMIL::TextMediaType::TextMediaType (NodePtr & d)
    : SMIL::MediaType (d, "text", id_node_text) {}

KDE_NO_EXPORT void SMIL::TextMediaType::init () {
    if (!inited) {
        PlayListNotify *n = document ()->notify_listener;
        if (n && !media_object)
            media_object = n->mediaManager()->createMedia(MediaManager::Text, this);

        font_size = static_cast <TextMedia *> (media_object)->default_font_size;
        font_color = 0;
        background_color = 0xffffff;
        bg_opacity = 100;
        halign = align_left;

        MediaType::init ();
    }
}

void
SMIL::TextMediaType::parseParam (const TrieString &name, const QString &val) {
    if (!media_object)
        media_object = document ()->notify_listener->
            mediaManager()->createMedia(MediaManager::Text, this);
    TextMedia *ti = static_cast <TextMedia *> (media_object);
    //kDebug () << "TextRuntime::parseParam " << name << "=" << val << endl;
    if (name == StringPool::attr_src) {
        src = val;
        if (!val.isEmpty ())
            ti->wget (absolutePath ());
        else
            ti->clearData ();
        return;
    }
    if (name == "backgroundColor" || name == "background-color") {
        background_color = val.isEmpty () ? 0xffffff : QColor (val).rgb ();
    } else if (name == "fontColor") {
        font_color = val.isEmpty () ? 0 : QColor (val).rgb ();
    } else if (name == "charset") {
        ti->codec = QTextCodec::codecForName (val.ascii ());
    } else if (name == "fontFace") {
        ; //FIXME
    } else if (name == "fontPtSize") {
        font_size = val.isEmpty () ? ti->default_font_size : val.toInt ();
    } else if (name == "fontSize") {
        font_size += val.isEmpty () ? ti->default_font_size : val.toInt ();
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
    } else {
        MediaType::parseParam (name, val);
        return;
    }
    resetSurface ();
    if (surface ())
        sub_surface->repaint ();
}

KDE_NO_EXPORT void SMIL::TextMediaType::begin () {
    TextMedia *tm = static_cast <TextMedia *> (media_object);
    if (tm && tm->downloading ()) { // FIXME we shouldn't loose media_object after a pause
        postpone_lock = document ()->postpone ();
        state = state_began;
        return;
    }
    MediaType::begin ();
}

void SMIL::TextMediaType::dataArrived () {
    TextMedia *tm = static_cast <TextMedia *> (media_object);
    if (tm->text.length ()) {
        resetSurface ();
        if (surface ())
            sub_surface->repaint ();
    }
    postpone_lock = 0L;
    if (state == state_began)
        MediaType::begin ();
}

bool SMIL::TextMediaType::handleEvent (Event *event) {
    if (event->id () == event_media_ready) {
        dataArrived ();
        if (parentNode ())
            parentNode ()->handleEvent (event);
    } else {
        return MediaType::handleEvent (event);
    }
    return true;
}

KDE_NO_EXPORT void SMIL::TextMediaType::accept (Visitor * v) {
    v->visit (this);
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT
SMIL::RefMediaType::RefMediaType (NodePtr & d)
    : SMIL::MediaType (d, "ref", id_node_ref) {}

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

KDE_NO_CDTOR_EXPORT SMIL::AnimateGroup::AnimateGroup (NodePtr &d, short _id)
 : Element (d, _id),
   runtime (new Runtime (this)),
   modification_id (-1),
   inited (false) {}

KDE_NO_CDTOR_EXPORT SMIL::AnimateGroup::~AnimateGroup () {
    delete runtime;
}

void SMIL::AnimateGroup::parseParam (const TrieString &name, const QString &val) {
    //kDebug () << "AnimateGroup::parseParam " << name << "=" << val << endl;
    if (name == StringPool::attr_target || name == "targetElement") {
        target_element = findLocalNodeById (this, val);
    } else if (name == "attribute" || name == "attributeName") {
        changed_attribute = TrieString (val);
    } else if (name == "to") {
        change_to = val;
    } else if (!runtime->parseParam (name, val)) {
        Element::parseParam (name, val);
    }
}

KDE_NO_EXPORT void SMIL::AnimateGroup::init () {
    if (!inited) {
        runtime->reset ();
        Element::init ();
        inited = true;
    }
}

KDE_NO_EXPORT void SMIL::AnimateGroup::activate () {
    init ();
    setState (state_activated);
    runtime->begin ();
}

KDE_NO_EXPORT void SMIL::AnimateGroup::begin () {
    runtime->start_time = document ()->last_event_time/100;
    Element::begin ();
    runtime->propagateStop (false); //see whether this node has a livetime or not
}

/**
 * animation finished
 */
KDE_NO_EXPORT void SMIL::AnimateGroup::finish () {
    //kDebug () << "AnimateGroup::stopped " << durTime ().durval << endl;
    if (!keepContent (this))
        restoreModification ();
    runtime->finish ();
}

KDE_NO_EXPORT void SMIL::AnimateGroup::reset () {
    Element::reset ();
    inited = false;
    runtime->reset ();
}

KDE_NO_EXPORT void SMIL::AnimateGroup::deactivate () {
    restoreModification ();
    if (unfinished ())
        finish ();
    runtime->reset ();
    Element::deactivate ();
}

KDE_NO_EXPORT bool SMIL::AnimateGroup::handleEvent (Event *event) {
    return runtime->handleEvent (event);
}

KDE_NO_EXPORT NodeRefListPtr SMIL::AnimateGroup::listeners (unsigned int id) {
    return runtime->listeners (id);
}

Role *SMIL::AnimateGroup::role (RoleType rt) {
    switch (rt) {
        case RoleTypeTiming:
            return runtime;
        default:
            break;
    }
    return Element::role (rt);
}

KDE_NO_EXPORT void SMIL::AnimateGroup::restoreModification () {
    if (modification_id > -1 && target_element &&
            target_element->state > Node::state_init) {
        //kDebug () << "AnimateGroup(" << this << ")::restoreModificatio " <<modification_id << endl;
        convertNode <Element> (target_element)->resetParam (
                changed_attribute, modification_id);
    }
    modification_id = -1;
}

KDE_NO_EXPORT NodePtr SMIL::AnimateGroup::targetElement () {
    if (!target_element) {
        for (Node *p = parentNode().ptr(); p; p =p->parentNode().ptr())
            if (SMIL::id_node_first_mediatype <= p->id &&
                    SMIL::id_node_last_mediatype >= p->id) {
                target_element = p;
                break;
            }
    }
    return target_element;
}

//-----------------------------------------------------------------------------

KDE_NO_EXPORT void SMIL::Set::begin () {
    restoreModification ();
    if (targetElement ()) {
        convertNode <Element> (target_element)->setParam (
                changed_attribute, change_to, &modification_id);
        //kDebug () << "Set(" << this << ")::started " << target_element->nodeName () << "." << changed_attribute << " ->" << change_to << " modid:" << modification_id << endl;
    } else {
        kWarning () << "target element not found" << endl;
    }
    AnimateGroup::begin ();
}

//-----------------------------------------------------------------------------
/*
//http://en.wikipedia.org/wiki/B%C3%A9zier_curve
typedef struct {
    float x;
    float y;
} Point2D;

static Point2D PointOnCubicBezier (Point2D *cp, float t) {
    float   ax, bx, cx;
    float   ay, by, cy;
    float   tSquared, tCubed;
    Point2D result;

    // calculate the polynomial coefficients

    cx = 3.0 * (cp[1].x - cp[0].x);
    bx = 3.0 * (cp[2].x - cp[1].x) - cx;
    ax = cp[3].x - cp[0].x - cx - bx;

    cy = 3.0 * (cp[1].y - cp[0].y);
    by = 3.0 * (cp[2].y - cp[1].y) - cy;
    ay = cp[3].y - cp[0].y - cy - by;

    // calculate the curve point at parameter value t

    tSquared = t * t;
    tCubed = tSquared * t;

    result.x = (ax * tCubed) + (bx * tSquared) + (cx * t) + cp[0].x;
    result.y = (ay * tCubed) + (by * tSquared) + (cy * t) + cp[0].y;

    return result;
}
*/

KDE_NO_CDTOR_EXPORT SMIL::Animate::Animate (NodePtr &d)
 : AnimateGroup (d, id_node_animate), change_by (0), steps (0) {}

KDE_NO_EXPORT void SMIL::Animate::init () {
    if (!inited) {
        if (anim_timer)
            document ()->cancelEvent (anim_timer);
        ASSERT (!anim_timer);
        accumulate = acc_none;
        additive = add_replace;
        change_by = 0;
        calcMode = calc_linear;
        change_from.truncate (0);
        change_values.clear ();
        steps = 0;
        change_delta = change_to_val = change_from_val = 0.0;
        change_from_unit.truncate (0);
        AnimateGroup::init ();
    }
}

KDE_NO_EXPORT void SMIL::Animate::deactivate () {
    if (anim_timer)
        document ()->cancelEvent (anim_timer);
    AnimateGroup::deactivate ();
}

KDE_NO_EXPORT void SMIL::Animate::begin () {
    bool success = false;
    //kDebug () << "Animate::started " << rt->durTime ().durval << endl;
    restoreModification ();
    if (anim_timer) // FIXME: repeating doesn't reinit
        document ()->cancelEvent (anim_timer);
    do {
        NodePtr protect = target_element;
        Element *target = convertNode <Element> (targetElement ());
        if (!target) {
            kWarning () << "target element not found" << endl;
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
                kWarning() << "animate couldn't determine start value" << endl;
                break;
            }
            if (change_to.isEmpty () && change_values.size () > 1)
                change_to = change_values.last (); // check 'values' attribute
            if (!change_to.isEmpty () && reg.search (change_to) > -1) {
                change_to_val = reg.cap (1).toDouble ();
            } else {
                kWarning () << "animate couldn't determine end value" << endl;
                break;
            }
            steps = 20 * runtime->durTime ().offset / 5; // 40 per sec
            if (steps > 0) {
                anim_timer = document ()->postEvent (
                        this, new TimerEvent (25, anim_timer_id)); // 25 ms for now FIXME
                change_delta = (change_to_val - change_from_val) / steps;
                //kDebug () << "Animate::started " << target_element->nodeName () << "." << changed_attribute << " " << change_from_val << "->" << change_to_val << " in " << steps << " using:" << change_delta << " inc" << endl;
                success = true;
            }
        } else if (calcMode == calc_discrete) {
            steps = change_values.size () - 1; // we do already the first step
            if (steps < 1) {
                 kWarning () << "animate needs at least two values" << endl;
                 break;
            }
            int interval = 100 * runtime->durTime ().offset / (1 + steps);
            if (interval <= 0 || runtime->durTime ().durval != Runtime::dur_timer) {
                 kWarning () << "animate needs a duration time" << endl;
                 break;
            }
            //kDebug () << "Animate::started " << target_element->nodeName () << "." << changed_attribute << " " << change_values.first () << "->" << change_values.last () << " in " << steps << " interval:" << interval << endl;
            anim_timer = document ()->postEvent (
                    this, new TimerEvent (interval, anim_timer_id));// 50 /s for now FIXME
            target->setParam (changed_attribute, change_values.first (),
                    &modification_id);
            success = true;
        }
    } while (false);
    if (success)
        AnimateGroup::begin ();
    else
        runtime->propagateStop (true);
}

KDE_NO_EXPORT void SMIL::Animate::finish () {
    if (anim_timer) // make sure timers are stopped
        document ()->cancelEvent (anim_timer);
    ASSERT (!anim_timer);
    if (steps > 0 && active ()) {
        steps = 0;
        if (calcMode == calc_linear)
            change_from_val = change_to_val;
        applyStep (); // we lost some steps ..
    }
    AnimateGroup::finish ();
}

void SMIL::Animate::parseParam (const TrieString &name, const QString &val) {
    //kDebug () << "Animate::parseParam " << name << "=" << val << endl;
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
    } else {
        AnimateGroup::parseParam (name, val);
    }
}

KDE_NO_EXPORT bool SMIL::Animate::handleEvent (Event *event) {
    if (event->id () == event_timer) {
        TimerEvent * te = static_cast <TimerEvent *> (event);
        if (te->event_id == anim_timer_id) {
            if (timerTick ())
                te->interval = true;
            return true;
        }
    }
    return AnimateGroup::handleEvent (event);
}

KDE_NO_EXPORT void SMIL::Animate::applyStep () {
    Element *target = convertNode <Element> (target_element);
    if (target && calcMode == calc_linear)
        target->setParam (changed_attribute, QString ("%1%2").arg (
                    change_from_val).arg(change_from_unit),
                &modification_id);
    else if (target && calcMode == calc_discrete)
        target->setParam (changed_attribute,
                change_values[change_values.size () - steps -1],
                &modification_id);
}

KDE_NO_EXPORT bool SMIL::Animate::timerTick () {
    if (!anim_timer) {
        kError () << "spurious anim timer tick" << endl;
    } else if (steps-- > 0) {
        if (calcMode == calc_linear)
            change_from_val += change_delta;
        applyStep ();
        return true;
    } else {
        document ()->cancelEvent (anim_timer);
        ASSERT (!anim_timer);
        runtime->propagateStop (true); // not sure, actually
    }
    return false;
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT SMIL::AnimateMotion::AnimateMotion (NodePtr &d)
 : AnimateGroup (d, id_node_animate),
   keytimes (NULL),
   spline_table (NULL),
   keytime_count (0) {}

KDE_NO_CDTOR_EXPORT SMIL::AnimateMotion::~AnimateMotion () {
    if (keytimes)
        free (keytimes);
    if (spline_table)
        free (spline_table);
}

KDE_NO_EXPORT void SMIL::AnimateMotion::init () {
    if (!inited) {
        if (anim_timer)
            document ()->cancelEvent (anim_timer);
        ASSERT (!anim_timer);
        accumulate = acc_none;
        additive = add_replace;
        calcMode = calc_linear;
        change_from.truncate (0);
        change_by.truncate (0);
        values.clear ();
        if (keytimes)
            free (keytimes);
        keytimes = NULL;
        keytime_count = 0;
        if (spline_table)
            free (spline_table);
        spline_table = NULL;
        splines.clear ();
        cur_x = cur_y = delta_x = delta_y = SizeType();
        AnimateGroup::init ();
    }
}

KDE_NO_EXPORT void SMIL::AnimateMotion::begin () {
    //kDebug () << "AnimateMotion::started " << durTime ().durval << endl;
    Element *target = convertNode <Element> (targetElement ());
    if (!checkTarget (target))
        return;
    if (anim_timer)
        document ()->cancelEvent (anim_timer);
    interval = 0;
    if (change_from.isEmpty ()) {
        if (values.size () > 1) {
            getCoordinates (values[0], begin_x, begin_y);
            getCoordinates (values[1], end_x, end_y);
        } else {
            CalculatedSizer sizes;
            if (SMIL::id_node_region == target->id)
                sizes = static_cast<SMIL::Region*>(target)->sizes;
            else if (SMIL::id_node_first_mediatype <= target->id &&
                    SMIL::id_node_last_mediatype >= target->id)
                sizes = static_cast<SMIL::MediaType*>(target)->sizes;
            if (sizes.left.isSet ()) {
                begin_x = sizes.left;
            } else if (sizes.right.isSet() && sizes.width.isSet ()) {
                begin_x = sizes.right;
                begin_x -= sizes.width;
            } else {
                begin_x = "0";
            }
            if (sizes.top.isSet ()) {
                begin_y = sizes.top;
            } else if (sizes.bottom.isSet() && sizes.height.isSet ()) {
                begin_y = sizes.bottom;
                begin_y -= sizes.height;
            } else {
                begin_y = "0";
            }
        }
    } else {
        getCoordinates (change_from, begin_x, begin_y);
    }
    if (!change_by.isEmpty ()) {
        getCoordinates (change_by, delta_x, delta_y);
        end_x = begin_x;
        end_y = begin_y;
        end_x += delta_x;
        end_y += delta_y;
    } else if (!change_to.isEmpty ()) {
        getCoordinates (change_to, end_x, end_y);
    }
    if (!setInterval ())
        return;
    applyStep ();
    if (calc_discrete != calcMode)
        document ()->notify_listener->addRepaintUpdater (this);
    AnimateGroup::begin ();
}

KDE_NO_EXPORT void SMIL::AnimateMotion::finish () {
    if (anim_timer) // make sure timers are stopped
        document ()->cancelEvent (anim_timer);
    document ()->notify_listener->removeRepaintUpdater (this);
    ASSERT (!anim_timer);
    if (active ()) {
        if (cur_x.size () != end_x.size () || cur_y.size () != end_y.size ()) {
            cur_x = end_x;
            cur_y = end_y;
            applyStep (); // we lost some steps ..
        }
    }
    AnimateGroup::finish ();
}

KDE_NO_EXPORT void SMIL::AnimateMotion::deactivate () {
    if (anim_timer)
        document ()->cancelEvent (anim_timer);
    else
        document ()->notify_listener->removeRepaintUpdater (this);
    if (spline_table)
        free (spline_table);
    spline_table = NULL;
    AnimateGroup::deactivate ();
}

KDE_NO_EXPORT bool SMIL::AnimateMotion::handleEvent (Event *event) {
    if (event->id () == event_timer) {
        TimerEvent * te = static_cast <TimerEvent *> (event);
        if (te->event_id == anim_timer_id) {
            anim_timer = NULL;
            timerTick (0);
            return true;
        }
    } else if (event->id () == event_update) {
        UpdateEvent *ue = static_cast <UpdateEvent *> (event);
        interval_start_time += ue->skipped_time;
        interval_end_time += ue->skipped_time;
        timerTick (ue->cur_event_time);
        return true;
    }
    return AnimateGroup::handleEvent (event);
}

void SMIL::AnimateMotion::parseParam (const TrieString &name, const QString &val) {
    //kDebug () << "AnimateMotion::parseParam " << name << "=" << val << endl;
    if (name == "from") {
        change_from = val;
    } else if (name == "by") {
        change_by = val;
    } else if (name == "values") {
        values = QStringList::split (QString (";"), val);
    } else if (name == "keyTimes") {
        QStringList kts = QStringList::split (QString (";"), val);
        if (keytimes)
            free (keytimes);
        keytime_count = kts.size ();
        if (0 == keytime_count) {
            keytimes = NULL;
            return;
        }
        keytimes = (float *) malloc (sizeof (float) * keytime_count);
        for (int i = 0; i < keytime_count; i++) {
            keytimes[i] = kts[i].stripWhiteSpace().toDouble();
            if (keytimes[i] < 0.0 || keytimes[i] > 1.0)
                kWarning() << "animateMotion wrong keyTimes values" << endl;
            else if (i == 0 && keytimes[i] > 0.01)
                kWarning() << "animateMotion first keyTimes value not 0" << endl;
            else
                continue;
            free (keytimes);
            keytimes = NULL;
            keytime_count = 0;
            return;
        }
    } else if (name == "keySplines") {
        splines = QStringList::split (QString (";"), val);
    } else if (name == "calcMode") {
        if (val == QString::fromLatin1 ("discrete"))
            calcMode = calc_discrete;
        else if (val == QString::fromLatin1 ("linear"))
            calcMode = calc_linear;
        else if (val == QString::fromLatin1 ("paced"))
            calcMode = calc_paced;
        else if (val == QString::fromLatin1 ("spline"))
            calcMode = calc_spline;
    } else
        AnimateGroup::parseParam (name, val);
}

bool SMIL::AnimateMotion::checkTarget (Node *n) {
    if (!n ||
            (SMIL::id_node_region != n->id &&
                !(SMIL::id_node_first_mediatype <= n->id &&
                    SMIL::id_node_last_mediatype >= n->id))) {
        kWarning () << "animateMotion target element not " <<
            (n ? "supported" : "found") << endl;
        if (anim_timer)
            document ()->cancelEvent (anim_timer);
        runtime->propagateStop (true);
        return false;
    }
    return true;
}

bool SMIL::AnimateMotion::getCoordinates (const QString &coord, SizeType &x, SizeType &y) {
    int p = coord.find (QChar (','));
    if (p < 0)
        p = coord.find (QChar (' '));
    if (p > 0) {
        x = coord.left (p).stripWhiteSpace ();
        y = coord.mid (p + 1).stripWhiteSpace ();
        return true;
    }
    return false;
}

static SMIL::AnimateMotion::Point2D cubicBezier (float ax, float bx, float cx,
        float ay, float by, float cy, float t) {
    float   tSquared, tCubed;
    SMIL::AnimateMotion::Point2D result;

    /* calculate the curve point at parameter value t */

    tSquared = t * t;
    tCubed = tSquared * t;

    result.x = (ax * tCubed) + (bx * tSquared) + (cx * t);
    result.y = (ay * tCubed) + (by * tSquared) + (cy * t);

    return result;
}

static
float cubicBezier (SMIL::AnimateMotion::Point2D *table, int a, int b, float x) {
    if (b > a + 1) {
        int mid = (a + b) / 2;
        if (table[mid].x > x)
            return cubicBezier (table, a, mid, x);
        else
            return cubicBezier (table, mid, b, x);
    }
    return table[a].y + (x - table[a].x) / (table[b].x - table[a].x) * (table[b].y - table[a].y);
}

bool SMIL::AnimateMotion::setInterval () {
    int cs = 10 * runtime->durTime ().offset;
    if (keytime_count > interval + 1)
        cs = (int) (cs * (keytimes[interval+1] - keytimes[interval]));
    else if (values.size () > 1)
        cs /= values.size () - 1;
    if (cs < 0) {
        kWarning () << "animateMotion has no valid duration interval " <<
            interval << endl;
        runtime->propagateStop (true);
        return false;
    }
    cur_x = begin_x;
    cur_y = begin_y;
    delta_x = end_x;
    delta_x -= begin_x;
    delta_y = end_y;
    delta_y -= begin_y;
    interval_start_time = document ()->last_event_time;
    interval_end_time = interval_start_time + 10 * cs;
    switch (calcMode) {
        case calc_paced: // FIXME
        case calc_linear:
            break;
        case calc_spline:
            if (splines.size () > interval) {
                QStringList kss = QStringList::split (
                        QString (" "), splines[interval]);
                control_point[0] = control_point[1] = 0;
                control_point[2] = control_point[3] = 1;
                if (kss.size () == 4) {
                    for (int i = 0; i < 4; ++i) {
                        control_point[i] = kss[i].toDouble();
                        if (control_point[i] < 0 || control_point[i] > 1) {
                            kWarning () << "keySplines values not between 0-1"
                                << endl;
                            control_point[i] = i > 1 ? 1 : 0;
                            break;
                        }
                    }
                    if (spline_table)
                        free (spline_table);
                    spline_table = (Point2D *) malloc (100 * sizeof (Point2D));

                    /* calculate the polynomial coefficients */
                    float ax, bx, cx;
                    float ay, by, cy;
                    cx = 3.0 * control_point[0];
                    bx = 3.0 * (control_point[2] - control_point[0]) - cx;
                    ax = 1.0 - cx - bx;

                    cy = 3.0 * control_point[1];
                    by = 3.0 * (control_point[3] - control_point[1]) - cy;
                    ay = 1.0 - cy - by;

                    for (int i = 0; i < 100; ++i)
                        spline_table[i] = cubicBezier (ax, bx, cx, ay, by, cy, 1.0*i/100);
                } else {
                    kWarning () << "keySplines " << interval <<
                        " has not 4 values" << endl;
                }
            }
            break;
        case calc_discrete:
            anim_timer = document ()->postEvent (
                    this, new TimerEvent (10 * cs, anim_timer_id));
            break;
        default:
            break;
    }
    //kDebug() << "setInterval " << steps << " " <<
    //    cur_x.size() << "," << cur_y.size() << "=>"
    //    << end_x.size() << "," << end_y.size() << " d:" << 
    //    delta_x.size() << "," << delta_y.size() << endl;
    return true;
}


KDE_NO_EXPORT void SMIL::AnimateMotion::applyStep () {
    Node *target = target_element.ptr ();
    if (!checkTarget (target))
        return;
    if (SMIL::id_node_region == target->id) {
        SMIL::Region* r = static_cast <SMIL::Region*> (target);
        if (r->surface ()) {
            r->sizes.move (cur_x, cur_y);
            r->boundsUpdate ();
        }
    } else {
        SMIL::MediaType *mt = static_cast <SMIL::MediaType *> (target);
        if (mt->surface ()) {
            mt->sizes.move (cur_x, cur_y);
            mt->boundsUpdate ();
        }
    }
}

KDE_NO_EXPORT bool SMIL::AnimateMotion::timerTick (unsigned int cur_time) {
    if (cur_time && cur_time <= interval_end_time) {
        float gain = 1.0 * (cur_time - interval_start_time) /
                           (interval_end_time - interval_start_time);
        if (gain > 1.0) {
            document ()->notify_listener->removeRepaintUpdater (this);
            gain = 1.0;
        }
        switch (calcMode) {
            case calc_paced: // FIXME
            case calc_linear:
                cur_x = delta_x;
                cur_y = delta_y;
                cur_x *= gain;
                cur_y *= gain;
                cur_x += begin_x;
                cur_y += begin_y;
                break;
            case calc_spline:
                if (spline_table) {
                    float y = cubicBezier (spline_table, 0, 99, gain);
                    cur_x = delta_x;
                    cur_y = delta_y;
                    cur_x *= y;
                    cur_y *= y;
                    cur_x += begin_x;
                    cur_y += begin_y;
                }
                break;
            case calc_discrete:
                return true; // very sub-optimal timer
        }
        applyStep ();
        return true;
    } else if (values.size () > ++interval + 1) {
        getCoordinates (values[interval], begin_x, begin_y);
        getCoordinates (values[interval+1], end_x, end_y);
        if (setInterval ()) {
            applyStep ();
            return true;
        }
    }
    return false;
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

KDE_NO_EXPORT void Visitor::visit (SMIL::Transition * n) {
    visit (static_cast <Element *> (n));
}

KDE_NO_EXPORT void Visitor::visit (SMIL::Animate * n) {
    visit (static_cast <SMIL::AnimateGroup *> (n));
}

KDE_NO_EXPORT void Visitor::visit (SMIL::AnimateMotion * n) {
    visit (static_cast <SMIL::AnimateGroup *> (n));
}

KDE_NO_EXPORT void Visitor::visit (SMIL::PriorityClass * n) {
    visit (static_cast <Element *> (n));
}

KDE_NO_EXPORT void Visitor::visit (SMIL::MediaType * n) {
    visit (static_cast <Mrl *> (n));
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
