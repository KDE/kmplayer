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

static const unsigned int begin_timer_id = (unsigned int) 3;
static const unsigned int dur_timer_id = (unsigned int) 4;
static const unsigned int anim_timer_id = (unsigned int) 5;
static const unsigned int trans_timer_id = (unsigned int) 6;
static const unsigned int trans_out_timer_id = (unsigned int) 7;

}

/* Intrinsic duration
 *  DurTime         |    EndTime     |
 *  =======================================================================
 *    DurMedia      |   DurMedia     | wait for event
 *        0         |   DurMedia     | only wait for child elements
 *    DurMedia      |       0        | intrinsic duration finished
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
        dur = (int) (100 * t);
        for ( ; *p; p++ ) {
            if (*p == 'm') {
                dur = (int) (t * 60);
                break;
            } else if (*p == 'h') {
                dur = (int) (t * 60 * 60);
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

static SMIL::Region *findRegion2 (Node *p, const QString &id) {
    TrieString regionname_attr ("regionName");
    for (Node *c = p->firstChild (); c; c = c->nextSibling ()) {
        if (c->id == SMIL::id_node_region) {
            SMIL::Region *r = static_cast <SMIL::Region *> (c);
            QString a = r->getAttribute (regionname_attr);
            if (a.isEmpty ())
                a = r->getAttribute (StringPool::attr_id);
            if ((a.isEmpty () && id.isEmpty ()) || a == id) {
                //kDebug () << "MediaType region found " << id;
                return r;
            }
        }
        SMIL::Region * r = findRegion2 (c, id);
        if (r)
            return r;
    }
    return 0L;
}

static SMIL::RegionBase *findRegion (Node *n, const QString &id) {
    SMIL::RegionBase *region = NULL;
    SMIL::Smil *smil = SMIL::Smil::findSmilNode (n);
    if (smil) {
        SMIL::Layout *layout = convertNode <SMIL::Layout> (smil->layout_node);
        region = findRegion2 (layout, id);
        if (!region)
            region = convertNode <SMIL::RegionBase> (layout->root_layout);
    }
    return region;
}

static SMIL::Transition *findTransition (Node *n, const QString & id) {
    SMIL::Smil *s = SMIL::Smil::findSmilNode (n);
    if (s) {
        Node *head = s->firstChild ();
        while (head && head->id != SMIL::id_node_head)
            head = head->nextSibling ();
        if (head)
            for (Node *c = head->firstChild (); c; c = c->nextSibling())
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
 : is_postponed (postponed) {}

//-----------------------------------------------------------------------------

Runtime::DurationItem::DurationItem ()
    : durval (DurTimer), offset (0), next (NULL) {}

Runtime::DurationItem &
Runtime::DurationItem::operator = (const Runtime::DurationItem &d) {
    durval = d.durval;
    offset = d.offset;
    connection.assign (&d.connection);
    return *this;
}

void Runtime::DurationItem::clear() {
    durval = DurTimer;
    offset = 0;
    connection.disconnect ();
    if (next) {
        next->clear ();
        delete next;
        next = NULL;
    }
}

static Runtime::Fill getDefaultFill (NodePtr n) {
    for (NodePtr p = n->parentNode (); p; p = p->parentNode ()) {
        Runtime *rt = static_cast <Runtime *> (p->role (RoleTiming));
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

KDE_NO_CDTOR_EXPORT Runtime::Runtime (Element *e)
 : begin_timer (NULL),
   duration_timer (NULL),
   started_timer (NULL),
   stopped_timer (NULL),
   fill_active (fill_auto),
   element (NULL) {
    initialize();
    element = e;
}


KDE_NO_CDTOR_EXPORT Runtime::~Runtime () {
    if (begin_timer)
        element->document ()->cancelPosting (begin_timer);
    if (duration_timer)
        element->document ()->cancelPosting (duration_timer);
    element = NULL;
    initialize ();
}

KDE_NO_EXPORT void Runtime::initialize () {
    if (element && begin_timer) {
        element->document ()->cancelPosting (begin_timer);
        begin_timer = NULL;
    }
    if (element && duration_timer) {
        element->document ()->cancelPosting (duration_timer);
        duration_timer = NULL;
    }
    repeat = repeat_count = 1;
    trans_in_dur = 0;
    timingstate = TimingsInit;
    for (int i = 0; i < (int) DurTimeLast; i++)
        durations [i].clear ();
    endTime ().durval = DurMedia;
    start_time = finish_time = 0;
    fill = fill_default;
    fill_def = fill_inherit;
    if (element)
        fill_active = getDefaultFill (element);
}

static
void setDurationItem (Node *n, const QString &val, Runtime::DurationItem *itm) {
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
            dur = Runtime::DurTimer;
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
            offset = -1;
            dur = Runtime::DurIndefinite;
        } else if (!strncmp (cval, "media", 5)) {
            dur = Runtime::DurMedia;
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
            if (*q) {
                ++q;
                if (!idref.isEmpty ()) {
                    target = findLocalNodeById (n, idref);
                    if (!target)
                        q = p;
                }
            } else {
                q = p;
            }
            //kDebug () << "setDuration q:" << q;
            if (parseTime (vl.mid (q-cval), offset)) {
                dur = Runtime::DurStart;
            } else if (*q && !strncmp (q, "end", 3)) {
                dur = Runtime::DurEnd;
                parseTime (vl.mid (q + 3 - cval), offset);
            } else if (*q && !strncmp (q, "begin", 5)) {
                dur = Runtime::DurStart;
                parseTime (vl.mid (q + 5 - cval), offset);
            } else if (*q && !strncmp (q, "activateevent", 13)) {
                dur = Runtime::DurActivated;
                parseTime (vl.mid (q + 13 - cval), offset);
            } else if (*q && !strncmp (q, "inboundsevent", 13)) {
                dur = Runtime::DurInBounds;
                parseTime (vl.mid (q + 13 - cval), offset);
            } else if (*q && !strncmp (q, "outofboundsevent", 16)) {
                dur = Runtime::DurOutBounds;
                parseTime (vl.mid (q + 16 - cval), offset);
            } else
                kWarning () << "setDuration no match " << cval;
            if (!target &&
                   dur >= Runtime::DurActivated && dur <= Runtime::DurOutBounds)
                target = n;
            if (target && dur != Runtime::DurTimer)
                itm->connection.connect (target, (MessageType)dur, n);
        }
        //kDebug () << "setDuration " << dur << " id:'" << idref << "' off:" << offset;
    }
    itm->durval = (Runtime::Duration) dur;
    itm->offset = offset;
}

static
void setDurationItems (Node *n, const QString &s, Runtime::DurationItem *item) {
    item->clear ();
    Runtime::DurationItem *last = item;
    QStringList list = s.split (QChar (';'));
    bool timer_set = false;
    for (int i = 0; i < list.count(); ++i) {
        QString val = list[i].trimmed();
        if (!val.isEmpty ()) {
            Runtime::DurationItem di;
            setDurationItem (n, val, &di);
            switch (di.durval) {
            case Runtime::DurTimer:
            case Runtime::DurIndefinite:
            case Runtime::DurMedia:
                *item = di;
                timer_set = true;
                break;
            default:
                last = last->next = new Runtime::DurationItem;
                *last = di;
            }
        }
    }
    if (item->next && !timer_set)
        item->durval = Runtime::DurIndefinite;
}

/**
 * start, or restart in case of re-use, the durations
 */
KDE_NO_EXPORT void Runtime::start () {
    //kDebug () << "Runtime::begin " << element->nodeName();
    if (begin_timer || duration_timer)
        element->init ();
    timingstate = timings_began;

    int offset = 0;
    bool stop = true;
    for (DurationItem *dur = durations + (int)BeginTime; dur; dur = dur->next)
        switch (dur->durval) {
        case DurStart: { // check started/finished
            Node *sender = dur->connection.signaler ();
            if (sender && sender->state >= Node::state_began) {
                offset = dur->offset;
                Runtime *rt = (Runtime*)sender->role (RoleTiming);
                if (rt)
                    offset -= element->document()->last_event_time/10 - rt->start_time;
                stop = false;
                kWarning() << "start trigger on started element";
            } // else wait for start event
            break;
        }
        case DurEnd: { // check finished
            Node *sender = dur->connection.signaler ();
            if (sender && sender->state >= Node::state_finished) {
                int offset = dur->offset;
                Runtime *rt = (Runtime*)sender->role (RoleTiming);
                if (rt)
                    offset -= element->document()->last_event_time/10 - rt->finish_time;
                stop = false;
                kWarning() << "start trigger on finished element";
            } // else wait for end event
            break;
        }
        case DurTimer:
            offset = dur->offset;
            stop = false;
            break;
        default:
            break;
    }
    if (stop)   // wait for event
        tryFinish ();
    else if (offset > 0)               // start timer
        begin_timer = element->document ()->post (element,
                new TimerPosting (10 * offset, begin_timer_id));
    else                               // start now
        propagateStart ();
}

KDE_NO_EXPORT void Runtime::finish () {
    if (started () || timingstate == timings_began) {
        doFinish (); // reschedule through Runtime::stopped
    } else {
        finish_time = element->document ()->last_event_time/10;
        repeat_count = repeat;
        NodePtrW guard = element;
        element->Node::finish ();
        if (guard && element->document ()->active ()) { // check for reset
            Posting event (element, MsgEventStopped);
            element->deliver (MsgEventStopped, &event);
        }
    }
}

KDE_NO_EXPORT void Runtime::startAndBeginNode () {
    if (begin_timer || duration_timer)
        element->init ();
    timingstate = timings_began;
    propagateStart ();
}

KDE_NO_EXPORT
bool Runtime::parseParam (const TrieString & name, const QString & val) {
    //kDebug () << "Runtime::parseParam " << name << "=" << val;
    if (name == StringPool::attr_begin) {
        setDurationItems (element, val, durations + (int) BeginTime);
        if ((timingstate == timings_began && !begin_timer) ||
                timingstate >= timings_stopped) {
            if (beginTime ().offset > 0) { // create a timer for start
                if (begin_timer) {
                    element->document ()->cancelPosting (begin_timer);
                    begin_timer = NULL;
                }
                if (beginTime ().durval == DurTimer)
                    begin_timer = element->document ()->post (element,
                            new TimerPosting (10 * beginTime ().offset, begin_timer_id));
            } else {                              // start now
                propagateStart ();
            }
        }
    } else if (name == StringPool::attr_dur) {
        setDurationItems (element, val, durations + (int) DurTime);
    } else if (name == StringPool::attr_end) {
        setDurationItems (element, val, durations + (int) EndTime);
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
        Mrl *mrl = element->mrl ();
        if (mrl)
            mrl->title = val;
    } else if (name == "endsync") {
        if ((durTime ().durval == DurMedia || durTime ().durval == 0) &&
                endTime ().durval == DurMedia) {
            Node *e = findLocalNodeById (element, val);
            if (e) {
                durations [(int) EndTime].connection.connect (
                    e, MsgEventStopped, element);
                durations [(int) EndTime].durval = (Duration) MsgEventStopped;
            }
        }
    } else if (name.startsWith ("repeat")) {
        if (val.indexOf ("indefinite") > -1)
            repeat = repeat_count = DurIndefinite;
        else
            repeat = repeat_count = val.toInt ();
    } else
        return false;
    return true;
}

KDE_NO_EXPORT void Runtime::message (MessageType msg, void *content) {
    switch (msg) {
        case MsgEventTimer: {
            TimerPosting *te = static_cast <TimerPosting *> (content);
            if (te->event_id == begin_timer_id) {
                begin_timer = NULL;
                propagateStart ();
            } else if (te->event_id == dur_timer_id) {
                duration_timer = NULL;
                doFinish ();
            } else {
                kWarning () << "unhandled timer event";
            }
            return;
        }
        case MsgEventStarted: {
            Posting *event = static_cast <Posting *> (content);
            if (event->source.ptr () == element) {
                started_timer = NULL;
                start_time = element->document ()->last_event_time/10;
                setDuration ();
                NodePtrW guard = element;
                element->deliver (MsgEventStarted, event);
                if (guard) {
                    element->begin ();
                    if (!element->document ()->postponed ())
                        tryFinish ();
                }
                return;
            }
            break;
        }
        case MsgEventStopped: {
            Posting *event = static_cast <Posting *> (content);
            if (event->source.ptr () == element) {
                stopped_timer = NULL;
                stopped ();
                return;
            }
            break;
        }
        default:
            break;
    }
    if ((int) msg >= (int) DurLastDuration)
        return;

    if (!started ()) {
        Posting *event = static_cast <Posting *> (content);
        for (DurationItem *dur = beginTime ().next; dur; dur = dur->next)
            if (dur->durval == (Duration) msg &&
                    dur->connection.signaler () == event->source.ptr ()) {
                if (element && dur->offset > 0) {
                    if (begin_timer)
                        element->document ()->cancelPosting (begin_timer);
                    begin_timer = element->document ()->post (element,
                            new TimerPosting(10 * dur->offset, begin_timer_id));
                } else { //FIXME neg. offsets
                    propagateStart ();
                }
                if (element->state == Node::state_finished)
                    element->state = Node::state_activated;//rewind to activated
                break;
            }
    } else if (started ()) {
        Posting *event = static_cast <Posting *> (content);
        for (DurationItem *dur = endTime ().next; dur; dur = dur->next)
            if (dur->durval == (Duration) msg &&
                    dur->connection.signaler () == event->source.ptr ()) {
                if (element && dur->offset > 0) {
                    if (duration_timer)
                        element->document ()->cancelPosting (duration_timer);
                    duration_timer = element->document ()->post (element,
                            new TimerPosting (10 * dur->offset, dur_timer_id));
                } else {
                    doFinish ();
                }
                break;
            }
    }
}

KDE_NO_EXPORT void *Runtime::role (RoleType msg, void *content) {
    switch (msg) {
    case RoleReceivers: {
        switch ((MessageType) (long) content) {
        case MsgEventStopped:
            return &m_StoppedListeners;
        case MsgEventStarted:
            return &m_StartedListeners;
        case MsgEventStarting:
            return &m_StartListeners;
        case MsgChildTransformedIn:
            break;
        default:
            kWarning () << "unknown event requested " << (int)msg;
        }
        return NULL;
    }
    default:
        break;
    }
    return MsgUnhandled;
}

KDE_NO_EXPORT void Runtime::propagateStop (bool forced) {
    if (state() == TimingsInit || state() >= timings_stopped)
        return; // nothing to stop
    if (!forced) {
        if ((durTime ().durval == DurMedia ||
                    durTime ().durval == DurTransition ) &&
                endTime ().durval == DurMedia)
            return; // wait for external eof
        if (endTime ().durval != DurTimer && endTime ().durval != DurMedia &&
                (started () || beginTime().durval == DurTimer))
            return; // wait for event
        if (durTime ().durval == DurIndefinite)
            return; // this may take a while :-)
        if (duration_timer)
            return; // timerEvent will call us with forced=true
        // bail out if a child still running
        for (Node *c = element->firstChild (); c; c = c->nextSibling ())
            if (c->unfinished () || Node::state_deferred == c->state)
                return; // a child still running
    }
    bool was_started (started ());
    timingstate = timings_freezed;
    if (begin_timer) {
        element->document ()->cancelPosting (begin_timer);
        begin_timer = NULL;
    }
    if (duration_timer) {
        element->document ()->cancelPosting (duration_timer);
        duration_timer = NULL;
    }
    if (was_started && element->document ()->active ())
        stopped_timer = element->document()->post (
                element, new Posting (element, MsgEventStopped));
    else if (element->unfinished ())
        element->finish ();
}

KDE_NO_EXPORT void Runtime::propagateStart () {
    timingstate = trans_in_dur ? TimingsTransIn : timings_started;
    element->deliver (MsgEventStarting, element);
    if (begin_timer) {
        element->document ()->cancelPosting (begin_timer);
        begin_timer = NULL;
    }
    started_timer = element->document()->post (
            element, new Posting (element, MsgEventStarted));
}

/**
 * begin_timer timer expired
 */
KDE_NO_EXPORT void Runtime::setDuration () {
    //kDebug () << (element ? element->nodeName() : "-");
    if (begin_timer) {
        element->document ()->cancelPosting (begin_timer);
        begin_timer = NULL;
    }
    if (duration_timer) {
        element->document ()->cancelPosting (duration_timer);
        duration_timer = NULL;
    }
    int duration = 0;
    if (durTime ().durval == DurTimer) {
        duration = durTime ().offset;
        if (endTime ().durval == DurTimer &&
                (!duration || endTime().offset - beginTime().offset < duration))
            duration = endTime ().offset - beginTime ().offset;
    } else if (endTime ().durval == DurTimer) {
        duration = endTime ().offset;
    }
    if (duration > 0)
        duration_timer = element->document ()->post (element,
                new TimerPosting (10 * duration, dur_timer_id));
    // kDebug () << "Runtime::started set dur timer " << durTime ().offset;
}

bool Runtime::started () const {
    return timingstate >= timings_started && timingstate < timings_stopped;
}

/**
 * duration_timer timer expired or no duration set after started
 */
KDE_NO_EXPORT void Runtime::stopped () {
    if (element->active ()) {
        if (repeat_count == DurIndefinite || 0 < --repeat_count) {
            element->message (MsgStateRewind);
            beginTime ().offset  = 0;
            beginTime ().durval = DurTimer;
            if (begin_timer)
                element->document ()->cancelPosting (begin_timer);
            propagateStart ();
        } else {
            repeat_count = repeat;
            element->finish ();
        }
    }
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT SizeType::SizeType () {
    reset ();
}

KDE_NO_CDTOR_EXPORT SizeType::SizeType (const QString & s, bool perc)
 : perc_size (perc ? -100 : 0) {
    *this = s;
}

void SizeType::reset () {
    perc_size = 0;
    abs_size = 0;
    isset = false;
    has_percentage = false;
}

SizeType & SizeType::operator = (const QString & s) {
    QString strval (s);
    int p = strval.indexOf (QChar ('%'));
    if (p > -1) {
        strval.truncate (p);
        has_percentage = true;
    }
    int px = strval.indexOf (QChar ('p')); // strip px
    if (px > -1)
        strval.truncate (px);
    double d = strval.toDouble (&isset);
    if (isset) {
        if (p > -1)
            perc_size = d;
        else if (perc_size < 0)
            perc_size = 100 * d;
        else
            abs_size = d;
    }
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

QString SizeType::toString () const {
    if (isset) {
        if (has_percentage)
            return QString ("%1%").arg ((int) size (100));
        return QString::number ((double) size (100));
    }
    return QString ();
}

//-----------------%<----------------------------------------------------------

template<>
IRect IRect::intersect (const IRect & r) const {
    int a (point.x < r.point.x ? r.point.x : point.x);
    int b (point.y < r.point.y ? r.point.y : point.y);
    return IRect (a, b,
            ((point.x + size.width < r.point.x + r.size.width)
             ? point.x + size.width : r.point.x + r.size.width) - a,
            ((point.y + size.height < r.point.y + r.size.height)
             ? point.y + size.height : r.point.y + r.size.height) - b);
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
        Node *c = node->firstChild ();
        for (; c; c = c->nextSibling ())
            if (c->id == SMIL::id_node_regpoint &&
                    static_cast<Element*>(c)->getAttribute (StringPool::attr_id)
                        == reg_point) {
                Single i1, i2; // dummies
                SMIL::RegPoint *rp_elm = static_cast <SMIL::RegPoint *> (c);
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
            xoff = 0;
    } else
        xoff = 0;
    if (top.isSet ())
        yoff = top.size (h);
    else if (height.isSet ()) {
        if (bottom.isSet ())
            yoff = h - height.size (h) - bottom.size (h);
        else
            yoff = 0;
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
bool CalculatedSizer::setSizeParam(const TrieString &name, const QString &val) {
    if (name == StringPool::attr_left) {
        left = val;
    } else if (name == StringPool::attr_top) {
        top = val;
    } else if (name == StringPool::attr_width) {
        width = val;
    } else if (name == StringPool::attr_height) {
        height = val;
    } else if (name == StringPool::attr_right) {
        right = val;
    } else if (name == StringPool::attr_bottom) {
        bottom = val;
    } else if (name == "regPoint") {
        reg_point = val;
    } else if (name == "regAlign") {
        reg_align = val;
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

KDE_NO_CDTOR_EXPORT MouseListeners::MouseListeners () {}

ConnectionList *MouseListeners::receivers (MessageType eid) {
    switch (eid) {
        case MsgEventClicked:
            return &m_ActionListeners;
        case MsgEventPointerInBounds:
            return &m_InBoundsListeners;
        case MsgEventPointerOutBounds:
            return &m_OutOfBoundsListeners;
        default:
            break;
    }
    return NULL;
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
    return NULL;
}

static Element * fromParamGroup (NodePtr & d, const QString & tag) {
    const char * ctag = tag.ascii ();
    if (!strcmp (ctag, "param"))
        return new SMIL::Param (d);
    else if (!strcmp (ctag, "area") || !strcmp (ctag, "anchor"))
        return new SMIL::Area (d, tag);
    return NULL;
}

static Element * fromAnimateGroup (NodePtr & d, const QString & tag) {
    const char * ctag = tag.ascii ();
    if (!strcmp (ctag, "set"))
        return new SMIL::Set (d);
    else if (!strcmp (ctag, "animate"))
        return new SMIL::Animate (d);
    else if (!strcmp (ctag, "animateColor"))
        return new SMIL::AnimateColor (d);
    else if (!strcmp (ctag, "animateMotion"))
        return new SMIL::AnimateMotion (d);
    return NULL;
}

static Element * fromMediaContentGroup (NodePtr & d, const QString & tag) {
    const char * taglatin = tag.latin1 ();
    if (!strcmp (taglatin, "video") ||
            !strcmp (taglatin, "audio") ||
            !strcmp (taglatin, "ref"))
        return new SMIL::RefMediaType (d, tag);
    else if (!strcmp (taglatin, "img"))
        return new SMIL::ImageMediaType (d);
    else if (!strcmp (taglatin, "text"))
        return new SMIL::TextMediaType (d);
    else if (!strcmp (taglatin, "brush"))
        return new SMIL::Brush (d);
    else if (!strcmp (taglatin, "a"))
        return new SMIL::Anchor (d);
    else if (!strcmp (taglatin, "smilText"))
        return new SMIL::SmilText (d);
    // animation, textstream
    return NULL;
}

static Element * fromContentControlGroup (NodePtr & d, const QString & tag) {
    if (!strcmp (tag.latin1 (), "switch"))
        return new SMIL::Switch (d);
    return NULL;
}

static Element *fromTextFlowGroup (NodePtr &d, const QString &tag) {
    const char *taglatin = tag.latin1 ();
    if (!strcmp (taglatin, "div"))
        return new SMIL::TextFlow (d, SMIL::id_node_div, tag.toUtf8 ());
    if (!strcmp (taglatin, "span"))
        return new SMIL::TextFlow (d, SMIL::id_node_span, tag.toUtf8 ());
    if (!strcmp (taglatin, "p"))
        return new SMIL::TextFlow (d, SMIL::id_node_p, tag.toUtf8 ());
    return NULL;
}

//-----------------------------------------------------------------------------

KDE_NO_EXPORT Node *SMIL::Smil::childFromTag (const QString & tag) {
    const char * ctag = tag.ascii ();
    if (!strcmp (ctag, "body"))
        return new SMIL::Body (m_doc);
    else if (!strcmp (ctag, "head"))
        return new SMIL::Head (m_doc);
    return NULL;
}

KDE_NO_EXPORT void SMIL::Smil::activate () {
    //kDebug () << "Smil::activate";
    resolved = true;
    if (layout_node)
        Element::activate ();
    else
        Element::deactivate(); // some unfortunate reset in parent doc
}

KDE_NO_EXPORT void SMIL::Smil::deactivate () {
    Mrl::deactivate ();
}

KDE_NO_EXPORT void SMIL::Smil::message (MessageType msg, void *content) {
    switch (msg) {

    case MsgChildFinished: {
        Posting *post = (Posting *) content;
        if (unfinished ()) {
            if (post->source->nextSibling ())
                post->source->nextSibling ()->activate ();
            else {
                for (NodePtr e = firstChild (); e; e = e->nextSibling ())
                    if (e->active ())
                        e->deactivate ();
                finish ();
            }
        }
        break;
    }

    case MsgSurfaceBoundsUpdate: {
        Layout *layout = convertNode <SMIL::Layout> (layout_node);
        if (layout && layout->root_layout)
            layout->root_layout->message (msg, content);
        return;
    }

    default:
        Mrl::message (msg, content);
    }
}

KDE_NO_EXPORT void SMIL::Smil::closed () {
    Node *head = NULL;
    for (Node *e = firstChild (); e; e = e->nextSibling ())
        if (e->id == id_node_head) {
            head = e;
            break;
        }
    if (!head) {
        head = new SMIL::Head (m_doc);
        insertBefore (head, firstChild ());
        head->setAuxiliaryNode (true);
        head->closed ();
    }
    for (Node *e = head->firstChild (); e; e = e->nextSibling ()) {
        if (e->id == id_node_layout) {
            layout_node = e;
        } else if (e->id == id_node_title) {
            QString str = e->innerText ();
            title = str.left (str.indexOf (QChar ('\n')));
        } else if (e->id == id_node_meta) {
            Element *elm = static_cast <Element *> (e);
            const QString name = elm->getAttribute (StringPool::attr_name);
            if (name == QString::fromLatin1 ("title"))
                title = elm->getAttribute ("content");
            else if (name == QString::fromLatin1 ("base"))
                src = elm->getAttribute ("content");
        }
    }
    Mrl::closed ();
}

KDE_NO_EXPORT bool SMIL::Smil::expose () const {
    return !title.isEmpty () || //return false if no title and only one
        previousSibling () || nextSibling ();
}

void SMIL::Smil::jump (const QString & id) {
    Node *n = document ()->getElementById (this, id, false);
    if (n) {
        if (n->unfinished ())
            kDebug() << "Smil::jump node is unfinished " << id;
        else {
            for (Node *p = n; p; p = p->parentNode ()) {
                if (p->unfinished () &&
                        p->id >= id_node_first_group &&
                        p->id <= id_node_last_group) {
                    static_cast <GroupBase *> (p)->setJumpNode (n);
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
    for (Node * e = node; e; e = e->parentNode ())
        if (e->id == SMIL::id_node_smil)
            return static_cast <SMIL::Smil *> (e);
    return 0L;
}

//-----------------------------------------------------------------------------

static void headChildDone (Node *node, Node *child) {
    if (node->unfinished ()) {
        if (child && child->nextSibling ())
            child->nextSibling ()->activate ();
        else
            node->finish (); // we're done
    }
}

KDE_NO_EXPORT Node *SMIL::Head::childFromTag (const QString & tag) {
    const char * ctag = tag.ascii ();
    if (!strcmp (ctag, "layout"))
        return new SMIL::Layout (m_doc);
    else if (!strcmp (ctag, "title"))
        return new DarkNode (m_doc, ctag, id_node_title);
    else if (!strcmp (ctag, "meta"))
        return new DarkNode (m_doc, ctag, id_node_meta);
    else if (!strcmp (ctag, "transition"))
        return new SMIL::Transition (m_doc);
    return NULL;
}

KDE_NO_EXPORT bool SMIL::Head::expose () const {
    return false;
}

KDE_NO_EXPORT void SMIL::Head::closed () {
    for (Node *e = firstChild (); e; e = e->nextSibling ())
        if (e->id == id_node_layout)
            return;
    SMIL::Layout * layout = new SMIL::Layout (m_doc);
    appendChild (layout);
    layout->setAuxiliaryNode (true);
    layout->closed (); // add root-layout and a region
    Element::closed ();
}

KDE_NO_EXPORT void SMIL::Head::message (MessageType msg, void *content) {
    if (MsgChildFinished == msg) {
        headChildDone (this, ((Posting *) content)->source.ptr ());
        return;
    }
    Element::message (msg, content);
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT SMIL::Layout::Layout (NodePtr & d)
 : Element (d, id_node_layout) {}

KDE_NO_EXPORT Node *SMIL::Layout::childFromTag (const QString & tag) {
    const char * ctag = tag.ascii ();
    if (!strcmp (ctag, "root-layout")) {
        Node *e = new SMIL::RootLayout (m_doc);
        root_layout = e;
        return e;
    } else if (!strcmp (ctag, "region"))
        return new SMIL::Region (m_doc);
    else if (!strcmp (ctag, "regPoint"))
        return new SMIL::RegPoint (m_doc);
    return NULL;
}

KDE_NO_EXPORT void SMIL::Layout::closed () {
    if (!root_layout) { // just add one if none there
        root_layout = new SMIL::RootLayout (m_doc);
        root_layout->setAuxiliaryNode (true);
        insertBefore (root_layout, firstChild ());
        root_layout->closed ();
    } else if (root_layout.ptr () != firstChild ()) {
        NodePtr rl = root_layout;
        removeChild (root_layout);
        insertBefore (root_layout, firstChild ());
    }
    Element::closed ();
}

KDE_NO_EXPORT void SMIL::Layout::message (MessageType msg, void *content) {
    if (MsgChildFinished == msg) {
        headChildDone (this, ((Posting *) content)->source.ptr ());
        if (state_finished == state && root_layout)
            root_layout->message (MsgSurfaceBoundsUpdate, (void *) true);
        return;
    }
    Element::message (msg, content);
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT SMIL::RegionBase::RegionBase (NodePtr & d, short id)
 : Element (d, id),
   media_info (NULL),
   z_order (0), background_color (0), bg_opacity (100)
{}

KDE_NO_CDTOR_EXPORT SMIL::RegionBase::~RegionBase () {
    if (region_surface) {
        region_surface->remove ();
        region_surface = NULL;
    }
}

KDE_NO_EXPORT void SMIL::RegionBase::activate () {
    show_background = ShowAlways;
    bg_opacity = 100;
    bg_repeat = BgRepeat;
    fit = fit_default;
    Node *n = parentNode ();
    if (n && SMIL::id_node_layout == n->id)
        n = n->firstChild ();
    state = state_deferred;
    role (RoleDisplay);
    init ();
    Element::activate ();
}

KDE_NO_EXPORT void SMIL::RegionBase::deactivate () {
    show_background = ShowAlways;
    background_color = 0;
    background_image.truncate (0);
    if (media_info) {
        delete media_info;
        media_info = NULL;
    }
    postpone_lock = NULL;
    sizes.resetSizes ();
    Element::deactivate ();
}

KDE_NO_EXPORT void SMIL::RegionBase::dataArrived () {
    ImageMedia *im = media_info ? (ImageMedia *)media_info->media : NULL;
    if (im && !im->isEmpty () && region_surface) {
        region_surface->markDirty ();
        region_surface->repaint ();
    }
    postpone_lock = 0L;
}

KDE_NO_EXPORT void SMIL::RegionBase::repaint () {
    Surface *s = (Surface *) role (RoleDisplay);
    if (s)
        s->repaint ();
}

KDE_NO_EXPORT void SMIL::RegionBase::repaint (const SRect & rect) {
    Surface *s = (Surface *) role (RoleDisplay);
    if (s)
        s->repaint (SRect (0, 0, s->bounds.size).intersect (rect));
}

static void updateSurfaceSort (SMIL::RegionBase *rb) {
    Surface *rs = rb->region_surface.ptr ();
    Surface *prs = rs->parentNode ();
    Surface *next = NULL;
    if (!prs)
        return;
    for (Surface *s = prs->firstChild (); s; s = s->nextSibling ())
        if (s != rs && s->node) {
            if (SMIL::id_node_region == s->node->id) {
                SMIL::Region *r = static_cast <SMIL::Region *> (s->node.ptr ());
                if (r->z_order > rb->z_order) {
                    next = s;
                    break;
                } else if (r->z_order == rb->z_order) {
                    next = s;
                    // now take region order into account
                    Node *n = rb->previousSibling();
                    for (; n; n = n->previousSibling())
                        if (n->id == SMIL::id_node_region) {
                            r = static_cast <SMIL::Region *> (n);
                            if (r->z_order == rb->z_order) {
                                next = r->region_surface->nextSibling ();
                                if (rs == next)
                                    next = next->nextSibling ();
                                break;
                            }
                        }
                    break;
                }
            } else if (SMIL::id_node_root_layout != s->node->id) {
                // break at attached media types
                Surface *m = (Surface *) s->node->role (RoleDisplay);
                if (m) {
                    next = m;
                    break;
                }
            }
        }
    if (rs->nextSibling () == next) {
        return;
    }
    SurfacePtr protect (rs);
    prs->removeChild (rs);
    prs->insertBefore (rs, next);
}

static unsigned int setRGBA (unsigned int color, int opacity) {
    int a = ((color >> 24) & 0xff) * opacity / 100;
    return (a << 24) | (color & 0xffffff);
}

KDE_NO_EXPORT
void SMIL::RegionBase::parseParam (const TrieString & name, const QString & val) {
    //kDebug () << "RegionBase::parseParam " << getAttribute ("id") << " " << name << "=" << val << " active:" << active();
    bool need_repaint = false;
    if (name == StringPool::attr_fit) {
        fit = parseFit (val.ascii ());
        if (region_surface)
            region_surface->scroll = fit_scroll == fit;
        need_repaint = true;
    } else if (name == "background-color" || name == "backgroundColor") {
        if (val.isEmpty ())
            background_color = 0;
        else
            background_color = setRGBA (QColor (val).rgba (), bg_opacity);
    } else if (name == "backgroundOpacity") {
        bg_opacity = SizeType (val, true).size (100);
        background_color = setRGBA (background_color, bg_opacity);
    } else if (name == "z-index") {
        z_order = val.toInt ();
        if (region_surface)
            updateSurfaceSort (this);
        need_repaint = true;
    } else if (sizes.setSizeParam (name, val)) {
        if (state_finished == state && region_surface)
            message (MsgSurfaceBoundsUpdate);
    } else if (name == "showBackground") {
        if (val == "whenActive")
            show_background = ShowWhenActive;
        else
            show_background = ShowAlways;
        need_repaint = true;
    } else if (name == "backgroundRepeat") {
        if (val == "norepeat")
            bg_repeat = BgNoRepeat;
        else if (val == "repeatX")
            bg_repeat = BgRepeatX;
        else if (val == "repeatY")
            bg_repeat = BgRepeatY;
        else if (val == "inherit")
            bg_repeat = BgInherit;
        else
            bg_repeat = BgRepeat;
    } else if (name == "backgroundImage") {
        if (val.isEmpty () || val == "none" || val == "inherit") {
            need_repaint = !background_image.isEmpty () &&
                background_image != val;
            background_image = val;
            if (media_info) {
                delete media_info;
                media_info = NULL;
                postpone_lock = NULL;
            }
        } else if (background_image != val) {
            background_image = val;
            Smil *s = val.isEmpty () ? NULL : SMIL::Smil::findSmilNode (this);
            if (s) {
                if (!media_info)
                    media_info = new MediaInfo (this, MediaManager::Image);
                Mrl *mrl = s->parentNode () ? s->parentNode ()->mrl () : NULL;
                QString url = mrl ? KURL (mrl->absolutePath(), val).url() : val;
                postpone_lock = document ()->postpone ();
                media_info->wget (url);
            }
        }
    }
    if (active ()) {
        Surface *s = (Surface *) role (RoleDisplay);
        if (s && s->background_color != background_color){
            s->setBackgroundColor (background_color);
            need_repaint = true;
        }
        if (need_repaint && s)
            s->repaint ();
    }
    Element::parseParam (name, val);
}

void SMIL::RegionBase::message (MessageType msg, void *content) {
    switch (msg) {

    case MsgMediaReady:
        if (media_info)
            dataArrived ();
        return;

    case MsgChildFinished:
        headChildDone (this, ((Posting *) content)->source.ptr ());
        return;

    default:
        break;
    }
    Element::message (msg, content);
}

void *SMIL::RegionBase::role (RoleType msg, void *content) {
    switch (msg) {

    case RoleSizer:
        return &sizes;

    case RoleReceivers:
        if (MsgSurfaceAttach == (MessageType) (long) content)
            return &m_AttachedMediaTypes;
        // fall through

    default:
        break;
    }
    return Element::role (msg, content);
}


//--------------------------%<-------------------------------------------------

KDE_NO_EXPORT void SMIL::RootLayout::closed () {
    QString width = getAttribute (StringPool::attr_width);
    QString height = getAttribute (StringPool::attr_height);
    if (!width.isEmpty () && !height.isEmpty ()) {
        Smil *s = Smil::findSmilNode (this);
        if (s) {
            s->size.width = width.toDouble ();
            s->size.height = height.toDouble();
        }
    }
    Element::closed ();
}

KDE_NO_CDTOR_EXPORT SMIL::RootLayout::~RootLayout() {
    region_surface = NULL;
}

KDE_NO_EXPORT void SMIL::RootLayout::deactivate () {
    SMIL::Smil *s = Smil::findSmilNode (this);
    if (s)
        s->role (RoleChildDisplay, NULL);
    region_surface = NULL;
    RegionBase::deactivate ();
}

void SMIL::RootLayout::message (MessageType msg, void *content) {
    switch (msg) {

    case MsgSurfaceBoundsUpdate:
        if (region_surface) {
            Surface *surface = region_surface.ptr ();
            Surface *ps = surface->parentNode ();
            Single x, y, w, h, pw, ph;
            if (ps && auxiliaryNode ()) {
                w = ps->bounds.width ();
                h = ps->bounds.height ();
                sizes.width = QString::number ((int) w);
                sizes.height = QString::number ((int) h);
            } else {
                w = sizes.width.size ();
                h = sizes.height.size ();
                if (ps) {
                    pw = ps->bounds.width ();
                    ph = ps->bounds.height ();
                    double pasp = (double) pw / ph;
                    double asp = (double) w / h;
                    if (pasp > asp) {
                        ps->xscale = ps->yscale = 1.0 * ph / h;
                        x += (Single (pw/ps->yscale) -  w) / 2;
                    } else {
                        ps->xscale = ps->yscale = 1.0 * pw / w;
                        y += (Single (ph/ps->xscale) - h) / 2;
                    }
                }
            }
            if (content || surface->bounds.size != SSize (w, h)) {
                surface->bounds = SRect (x, y, w, h);
                if (!auxiliaryNode ()) {
                    SMIL::Smil *s = Smil::findSmilNode (this);
                    s->size = surface->bounds.size;
                }
                if (content)
                    surface->resize (surface->bounds, true);
                else
                    surface->updateChildren (!!content);
            }
        }
        return;

    default:
        break;
    }
    RegionBase::message (msg, content);
}

void *SMIL::RootLayout::role (RoleType msg, void *content) {
    switch (msg) {

    case RoleDisplay:
        if (!region_surface && active ()) {
            SMIL::Smil *s = Smil::findSmilNode (this);
            if (s && s->active ()) {
                Surface *surface = (Surface *)s->role (RoleChildDisplay, s);
                if (surface)
                    region_surface = surface->createSurface (this, SRect ());
            }
        }
        return region_surface.ptr ();

    default:
        break;
    }
    return RegionBase::role (msg, content);
}

//--------------------------%<-------------------------------------------------

KDE_NO_CDTOR_EXPORT SMIL::Region::Region (NodePtr & d)
 : RegionBase (d, id_node_region) {}

KDE_NO_CDTOR_EXPORT SMIL::Region::~Region () {
    if (region_surface)
        region_surface->remove ();
}

KDE_NO_EXPORT void SMIL::Region::deactivate () {
    if (region_surface)
        region_surface->remove ();
    RegionBase::deactivate ();
}

KDE_NO_EXPORT Node *SMIL::Region::childFromTag (const QString & tag) {
    if (!strcmp (tag.latin1 (), "region"))
        return new SMIL::Region (m_doc);
    return NULL;
}

void SMIL::Region::message (MessageType msg, void *content) {
    switch (msg) {

    case MsgSurfaceBoundsUpdate:
        if (region_surface && state == state_finished) {
            Surface *ps = region_surface->parentNode ();
            if (ps) {
                SSize dim = ps->bounds.size;
                Single x, y, w, h;
                sizes.calcSizes (this, dim.width, dim.height, x, y, w, h);
                region_surface->resize (SRect (x, y, w, h), !!content);
            }
        }
        return;

    default:
        break;
    }
    RegionBase::message (msg, content);
}

void *SMIL::Region::role (RoleType msg, void *content) {
    switch (msg) {

    case RoleDisplay:
        if (!region_surface && active ()) {
            Node *n = parentNode ();
            if (n && SMIL::id_node_layout == n->id)
                n = n->firstChild ();
            Surface *s = (Surface *) n->role (RoleDisplay);
            if (s) {
                region_surface = s->createSurface (this, SRect ());
                region_surface->background_color = background_color;
                updateSurfaceSort (this);
            }
        }
        return region_surface.ptr ();

    default: {
        ConnectionList *l = mouse_listeners.receivers ((MessageType)(long)content);
        if (l)
            return l;
    }
    }
    return RegionBase::role (msg, content);
}


//-----------------------------------------------------------------------------

KDE_NO_EXPORT
void SMIL::RegPoint::parseParam (const TrieString & p, const QString & v) {
    sizes.setSizeParam (p, v); // TODO: if dynamic, make sure to repaint
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
   type_info (NULL), direction (dir_forward), dur (100), fade_color (0) {}

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
   runtime (new Runtime (this)) {}

KDE_NO_CDTOR_EXPORT SMIL::GroupBase::~GroupBase () {
    delete runtime;
}

KDE_NO_EXPORT Node *SMIL::GroupBase::childFromTag (const QString & tag) {
    Element * elm = fromScheduleGroup (m_doc, tag);
    if (!elm) elm = fromMediaContentGroup (m_doc, tag);
    if (!elm) elm = fromContentControlGroup (m_doc, tag);
    if (!elm) elm = fromAnimateGroup (m_doc, tag);
    if (elm)
        return elm;
    return NULL;
}

KDE_NO_EXPORT void SMIL::GroupBase::init () {
    if (Runtime::TimingsInitialized > runtime->timingstate) {
        Element::init ();
        runtime->timingstate = Runtime::TimingsInitialized;
    }
}

KDE_NO_EXPORT void SMIL::GroupBase::finish () {
    setState (state_finished); // avoid recurstion through childDone
    for (NodePtr e = firstChild (); e; e = e->nextSibling ())
        if (e->unfinished ())
            e->finish ();
    runtime->finish ();
}

namespace {

class KMPLAYER_NO_EXPORT GroupBaseInitVisitor : public Visitor {
public:
    using Visitor::visit;

    bool ready;

    GroupBaseInitVisitor () : ready (true) {
    }

    void visit (Node *node) {
        if (node->nextSibling ())
            node->nextSibling ()->accept (this);
    }
    void visit (Element *elm) {
        if (elm->role (RoleTiming))
            elm->init ();
        else
            visit (static_cast <Node *> (elm));
    }
    void visit (SMIL::PriorityClass *pc) {
        pc->init ();
        for (NodePtr n = pc->firstChild (); n; n = n->nextSibling ())
            n->accept (this);
    }
    void visit (SMIL::Seq *seq) {
        seq->init ();
        if (seq->firstChild ()) {
             seq->firstChild ()->accept (this);
             ready = !!seq->firstChild ()->role (RoleReady);
        }
    }
    void visit (SMIL::Switch *s) {
        s->init ();
        Node *n = s->chosenOne ();
        if (n)
             n->accept (this);
    }
    void visit (SMIL::Anchor *a) {
        a->init ();
        if (a->firstChild ())
             a->firstChild ()->accept (this);
    }
    void visit (SMIL::Par *par) {
        par->init ();
        for (NodePtr n = par->firstChild (); n; n = n->nextSibling ()) {
            n->accept (this);
            if (ready)
                ready = !!n->role (RoleReady);
        }
    }
};

class KMPLAYER_NO_EXPORT FreezeStateUpdater : public Visitor {

    bool initial_node;
    bool freeze;

    void setFreezeState (Runtime *rt) {
        bool auto_freeze = (Runtime::DurTimer == rt->durTime ().durval &&
                    0 == rt->durTime ().offset &&
                    Runtime::DurMedia == rt->endTime ().durval) &&
            rt->fill_active != Runtime::fill_remove;
        bool cfg_freeze = rt->fill_active == Runtime::fill_freeze ||
            rt->fill_active == Runtime::fill_hold ||
            rt->fill_active == Runtime::fill_transition;

        bool do_freeze = freeze && (auto_freeze || cfg_freeze);
        if (do_freeze && rt->timingstate == Runtime::timings_stopped) {
            rt->timingstate = Runtime::timings_freezed;
            rt->element->message (MsgStateFreeze);
        } else if (!do_freeze && rt->timingstate == Runtime::timings_freezed) {
            rt->timingstate = Runtime::timings_stopped;
            rt->element->message (MsgStateFreeze);
        }
    }
    void updateNode (Node *n) {
        if (initial_node) {
            initial_node = false;
        } else {
            Runtime *rt = (Runtime *) n->role (RoleTiming);
            if (rt && rt->timingstate >= Runtime::timings_stopped)
                setFreezeState (rt);
        }
    }
public:
    using Visitor::visit;

    FreezeStateUpdater () : initial_node (true), freeze (true) {}

    void visit (Element *elm) {
        updateNode (elm);
    }
    void visit (SMIL::PriorityClass *pc) {
        for (NodePtr n = pc->firstChild (); n; n = n->nextSibling ())
            n->accept (this);
    }
    void visit (SMIL::Seq *seq) {
        bool old_freeze = freeze;

        updateNode (seq);
        freeze = freeze && seq->runtime->active ();

        Runtime *prev = NULL;
        for (NodePtr n = seq->firstChild (); n; n = n->nextSibling ()) {
            if (n->active ()) {
                Runtime *rt = (Runtime *) n->role (RoleTiming);
                if (rt) {
                    bool prev_freeze = prev && freeze &&
                        (prev->fill_active == Runtime::fill_hold ||
                         (prev->fill_active == Runtime::fill_transition &&
                          Runtime::TimingsTransIn == rt->state ()));
                    if (rt->timingstate < Runtime::timings_started) {
                        break;
                    } else if (rt->timingstate < Runtime::timings_stopped) {
                        freeze = prev_freeze;
                        break;
                    }
                    if (prev_freeze)
                        prev->element->accept (this);
                    if (rt->timingstate == Runtime::timings_stopped)
                        rt->element->deactivate();
                    else
                        prev = rt;
                }
            }
        }
        if (prev)
            prev->element->accept (this);

        freeze = old_freeze;
    }
    void visit (SMIL::Anchor *a) {
        if (a->firstChild ())
            a->firstChild ()->accept (this);
    }
    void visit (SMIL::Par *par) {
        bool old_freeze = freeze;

        updateNode (par);
        freeze = freeze && par->runtime->active ();

        for (NodePtr n = par->firstChild (); n; n = n->nextSibling ())
            n->accept (this);

        freeze = old_freeze;
    }
    void visit (SMIL::Excl *excl) {
        bool old_freeze = freeze;

        updateNode (excl);
        bool new_freeze = freeze && excl->runtime->active ();

        Node *cur = excl->cur_node.ptr ();
        for (NodePtr n = excl->firstChild (); n; n = n->nextSibling ()) {
            freeze = new_freeze && n.ptr () == cur;
            n->accept (this);
        }

        freeze = old_freeze;
    }
    void visit (SMIL::Switch *s) {
        bool old_freeze = freeze;

        updateNode (s);
        freeze &= s->runtime->active ();

        Node *cur = s->chosenOne ();
        if (cur)
            cur->accept (this);

        freeze = old_freeze;
    }
};

}

KDE_NO_EXPORT void SMIL::GroupBase::activate () {
    GroupBaseInitVisitor visitor;
    accept (&visitor);
    setState (state_activated);
    if (visitor.ready)
        runtime->start ();
    else
        state = state_deferred;
}

KDE_NO_EXPORT
void SMIL::GroupBase::parseParam (const TrieString &para, const QString &val) {
    if (!runtime->parseParam (para, val))
        Element::parseParam (para, val);
}

KDE_NO_EXPORT void SMIL::GroupBase::message (MessageType msg, void *content) {
    switch (msg) {

    case MsgStateRewind:
        if (active ()) {
            State old = state;
            state = state_deactivated;
            for (NodePtr e = firstChild (); e; e = e->nextSibling ())
                e->reset ();
            state = old;
            GroupBaseInitVisitor visitor;
            accept (&visitor);
        }
        return;

    default:
        break;
    }
    if ((int) msg >= (int) Runtime::DurLastDuration)
        Element::message (msg, content);
    else
        runtime->message (msg, content);
}

KDE_NO_EXPORT void *SMIL::GroupBase::role (RoleType msg, void *content) {
    switch (msg) {

    case RoleTiming:
        return runtime;

    default:
        break;
    }
    void *response = runtime->role (msg, content);
    if (response == MsgUnhandled)
        return Element::role (msg, content);
    return response;
}


KDE_NO_EXPORT void SMIL::GroupBase::deactivate () {
    setState (state_deactivated); // avoid recurstion through childDone
    for (NodePtr e = firstChild (); e; e = e->nextSibling ())
        if (e->active ())
            e->deactivate ();
    if (unfinished ())
        finish ();
    runtime->initialize ();
    Element::deactivate ();
}

KDE_NO_EXPORT void SMIL::GroupBase::reset () {
    Element::reset ();
    runtime->initialize ();
}

KDE_NO_EXPORT void SMIL::GroupBase::setJumpNode (NodePtr n) {
    NodePtr child = n;
    if (state > state_init) {
        state = state_deferred;
        for (NodePtr c = firstChild (); c; c = c->nextSibling ())
            if (c->active ())
                c->reset ();
        for (Node *c = n->parentNode (); c; c = c->parentNode ()) {
            if (c == this || c->id == id_node_body)
                break;
            if (c->id >= id_node_first_group && c->id <= id_node_last_group)
                static_cast <SMIL::GroupBase *> (c)->jump_node = child;
            child = c;
        }
    }
    jump_node = child;
    state = state_activated;
    init ();
    for (NodePtr n = firstChild (); n; n = n->nextSibling ())
        if (n->role (RoleTiming))
            convertNode <Element> (n)->init ();
    runtime->startAndBeginNode (); // undefer through begin()
}

//-----------------------------------------------------------------------------

// SMIL::Body was here

//-----------------------------------------------------------------------------

KDE_NO_EXPORT void SMIL::Par::begin () {
    jump_node = 0L; // TODO: adjust timings
    setState (state_began);
    for (NodePtr e = firstChild (); e; e = e->nextSibling ())
        e->activate ();
}

KDE_NO_EXPORT void SMIL::Par::reset () {
    GroupBase::reset ();
    for (NodePtr e = firstChild (); e; e = e->nextSibling ())
        e->reset ();
}

static bool childrenReady (Node *node) {
    for (NodePtr n = node->firstChild (); n; n = n->nextSibling ())
        if (!n->role (RoleReady))
            return false;
    return true;
}

KDE_NO_EXPORT void SMIL::Par::message (MessageType msg, void *content) {
    switch (msg) {

    case MsgChildReady:
        if (childrenReady (this)) {
            const int cur_state = state;
            if (state == state_deferred) {
                state = state_activated;
                runtime->start ();
            }
            if (cur_state == state_init && parentNode ())
                parentNode ()->message (MsgChildReady, this);
        }
        return;

    case MsgChildFinished: {
        if (unfinished ()) {
            FreezeStateUpdater visitor;
            accept (&visitor);
            runtime->tryFinish ();
        }
        return;
    }
    default:
        break;
    }
    GroupBase::message (msg, content);
}

KDE_NO_EXPORT void *SMIL::Par::role (RoleType msg, void *content) {
    switch (msg) {
    case RoleReady:
        return MsgBool (childrenReady (this));
    default:
        break;
    }
    return GroupBase::role (msg, content);
}


//-----------------------------------------------------------------------------

KDE_NO_EXPORT void SMIL::Seq::begin () {
    setState (state_began);
    if (jump_node) {
        starting_connection.disconnect ();
        trans_connection.disconnect ();
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
                Runtime *rt = (Runtime *) c->role (RoleTiming);
                if (rt)
                    rt->timingstate = Runtime::timings_stopped; //TODO fill_hold
            }
    } else if (firstChild ()) {
        if (firstChild ()->nextSibling()) {
            GroupBaseInitVisitor visitor;
            firstChild ()->nextSibling ()->accept (&visitor);
        }
        starting_connection.connect (firstChild (), MsgEventStarted, this);
        firstChild ()->activate ();
    }
}

KDE_NO_EXPORT void SMIL::Seq::message (MessageType msg, void *content) {
    switch (msg) {

        case MsgChildReady:
            if (firstChild () == (Node *) content) {
                if (state == state_deferred) {
                    state = state_activated;
                    runtime->start ();
                }
                if (state == state_init && parentNode ())
                    parentNode ()->message (MsgChildReady, this);
            } else if (unfinished ()) {
                FreezeStateUpdater visitor;
                accept (&visitor);
            }
            return;

        case MsgChildFinished: {
            if (unfinished ()) {
                Posting *post = (Posting *) content;
                if (state != state_deferred) {
                    Node *next = post->source
                        ? post->source->nextSibling ()
                        : NULL;
                    if (next) {
                        if (next->nextSibling()) {
                            GroupBaseInitVisitor visitor;
                            next->nextSibling ()->accept (&visitor);
                        }
                        starting_connection.connect(next, MsgEventStarted,this);
                        trans_connection.connect (
                                next, MsgChildTransformedIn, this);
                        next->activate ();
                    } else {
                        starting_connection.disconnect ();
                        trans_connection.disconnect ();
                        runtime->tryFinish ();
                    }
                    FreezeStateUpdater visitor;
                    accept (&visitor);
                } else if (jump_node) {
                    finish ();
                }
            }
            return;
        }

        case MsgEventStarted: {
            Posting *event = static_cast <Posting *> (content);
            Node *source = event->source;
            if (source != this && source->previousSibling ()) {
                FreezeStateUpdater visitor;
                starting_connection.disconnect ();
                accept (&visitor);
            }
            break;
        }

        case MsgChildTransformedIn: {
            Node *source = (Node *) content;
            if (source != this && source->previousSibling ()) {
                FreezeStateUpdater visitor;
                starting_connection.disconnect ();
                accept (&visitor);
            }
            break;
        }

        default:
            break;
    }
    GroupBase::message (msg, content);
}

KDE_NO_EXPORT void *SMIL::Seq::role (RoleType msg, void *content) {
    switch (msg) {
    case RoleReady:
        return MsgBool (!firstChild () || firstChild ()->role (RoleReady));
    default:
        break;
    }
    return GroupBase::role (msg, content);
}


//-----------------------------------------------------------------------------

KDE_NO_EXPORT Node *SMIL::Excl::childFromTag (const QString &tag) {
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
        Node *s = n->nextSibling ();
        if (s)
            s->accept (this);
    }
    void visit (Element *elm) {
        if (elm->role (RoleTiming)) {
            // make aboutToStart connection with Timing
            excl->started_event_list =
                new SMIL::Excl::ConnectionItem (excl->started_event_list);
            excl->started_event_list->link.connect (elm, MsgEventStarting, excl);
            elm->activate ();
        }
        visit (static_cast <Node *> (elm));
    }
    void visit (SMIL::PriorityClass *pc) {
        pc->init ();
        pc->state = Node::state_activated;
        Node *n = pc->firstChild ();
        if (n)
            n->accept (this);
        visit (static_cast <Node *> (pc));
    }
};

class KMPLAYER_NO_EXPORT ExclPauseVisitor : public Visitor {
    bool pause;
    Node *paused_by;
    unsigned int cur_time;

    void updatePauseStateEvent (Posting *event, int pause_time) {
        if (event) {
            if (pause)
                paused_by->document ()->pausePosting (event);
            else
                paused_by->document ()->unpausePosting (event, (cur_time-pause_time)*10);
        }
    }
    static Posting *activeEvent (Runtime *r) {
        Posting *event = NULL;
        if (r->begin_timer)
            event = r->begin_timer;
        else if (r->started_timer)
            event = r->started_timer;
        else if (r->duration_timer)
            event = r->duration_timer;
        else if (r->stopped_timer)
            event = r->stopped_timer;
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
        Runtime *rt = (Runtime *) elm->role (RoleTiming);
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
        if (mt->media_info && mt->media_info->media) {
            if (pause)
                mt->media_info->media->pause ();
            else
                mt->media_info->media->unpause ();
            Surface *s = mt->surface ();
            if (s)
                s->repaint ();
        }

        Posting *event = NULL;
        if (mt->trans_out_timer)
            event = mt->trans_out_timer;
        updatePauseStateEvent (event, mt->runtime->paused_time);

        visit (static_cast <Element *> (mt));
    }
    void visit (SMIL::AnimateBase *an) {
        updatePauseStateEvent(an->anim_timer, an->runtime->paused_time);
        visit (static_cast <Element *> (an));
    }
    void visit (SMIL::Smil *s) {
        for (Node *c = s->firstChild (); c; c = c->nextSibling ())
            if (SMIL::id_node_body == c->id)
                c->accept (this);
    }
};

}

static void clearList (SMIL::Excl::ConnectionItem **pitem) {
    SMIL::Excl::ConnectionItem *item = *pitem;
    while (item) {
        SMIL::Excl::ConnectionItem *tmp = item;
        item = item->next;
        delete tmp;
    }
    *pitem = NULL;
}

KDE_NO_CDTOR_EXPORT SMIL::Excl::Excl (NodePtr & d)
    : GroupBase (d, id_node_excl), started_event_list (NULL) {}

KDE_NO_CDTOR_EXPORT SMIL::Excl::~Excl () {
    clearList (&started_event_list);
}

KDE_NO_EXPORT void SMIL::Excl::begin () {
    //kDebug () << "SMIL::Excl::begin";
    Node *n = firstChild ();
    if (n) {
        ExclActivateVisitor visitor (this);
        n->accept (&visitor);
    }
}

KDE_NO_EXPORT void SMIL::Excl::deactivate () {
    clearList (&started_event_list);
    priority_queue.clear ();
    stopped_connection.disconnect ();
    GroupBase::deactivate ();
}

KDE_NO_EXPORT void SMIL::Excl::message (MessageType msg, void *content) {
    switch (msg) {
        case MsgEventStarting: {
            Node *source = (Node *) content;
            NodePtr n = cur_node;
            if (source == n.ptr ())
                return; // eg. repeating
            cur_node = source;
            stopped_connection.connect (cur_node, MsgEventStopped, this);
            if (n) {
                if (SMIL::id_node_priorityclass == cur_node->parentNode ()->id) {
                    switch (static_cast <SMIL::PriorityClass *>
                            (cur_node->parentNode ())->peers) {
                        case PriorityClass::PeersPause: {
                            ExclPauseVisitor visitor (
                                  true, this, document ()->last_event_time/10);
                            n->accept (&visitor);
                            priority_queue.insertBefore (
                                  new NodeRefItem (n), priority_queue.first ());
                            return;
                        }
                        default:
                            break; //TODO
                    }
                }
                ((Runtime*)n->role (RoleTiming))->doFinish ();
            }
            return;
        }
        case MsgChildFinished: {
            Posting *event = static_cast <Posting *> (content);
            FreezeStateUpdater visitor;
            accept (&visitor);
            if (event->source == cur_node) {
                Runtime* rt = (Runtime*)cur_node->role (RoleTiming);
                if (rt && rt->timingstate == Runtime::timings_stopped) {
                    cur_node = NULL;
                    stopped_connection.disconnect ();
                }
                runtime->tryFinish ();
            }
            return;
        }
        case MsgEventStopped: {
            Posting *event = static_cast <Posting *> (content);
            if (event->source == cur_node) {

                NodeRefItemPtr ref = priority_queue.first ();
                while (ref && (!ref->data || !ref->data->active ())) {
                    // should not happen, but consider a backend crash or so
                    priority_queue.remove (ref);
                    ref = priority_queue.first ();
                }
                if (ref) {
                    cur_node = ref->data;
                    priority_queue.remove (ref);
                    stopped_connection.connect (cur_node, MsgEventStopped, this);
                    ExclPauseVisitor visitor (false, this, document()->last_event_time/10);
                    cur_node->accept (&visitor);
                    // else TODO
                }
            }
            break;
        }
        default:
            break;
    }
    GroupBase::message (msg, content);
}

//-----------------------------------------------------------------------------

KDE_NO_EXPORT Node *SMIL::PriorityClass::childFromTag (const QString &tag) {
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
    peers = PeersStop;
    higher = HigherPause;
    lower = LowerDefer;
    pause_display = PauseDisplayShow;
    Element::init ();
}

KDE_NO_EXPORT void SMIL::PriorityClass::message (MessageType msg, void *data) {
    if (MsgChildFinished == msg)
        // do nothing
        return;
    Element::message (msg, data);
}

//-----------------------------------------------------------------------------

KDE_NO_EXPORT Node *SMIL::Switch::chosenOne () {
    if (!chosen_one && firstChild ()) {
        PlayListNotify * n = document()->notify_listener;
        int pref = 0, max = 0x7fffffff, currate = 0;
        if (n)
            n->bitRates (pref, max);
        if (firstChild ()) {
            Node *fallback = NULL;
            for (Node *e = firstChild (); e; e = e->nextSibling ())
                if (e->isElementNode ()) {
                    Element *elm = static_cast <Element *> (e);
                    QString lang = elm->getAttribute ("systemLanguage");
                    if (!lang.isEmpty ()) {
                        lang = lang.replace (QChar ('-'), QChar ('_'));
                        char *clang = getenv ("LANG");
                        if (!clang) {
                            if (!fallback)
                                fallback = e;
                        } else if (QString (clang).lower ().startsWith (lang)) {
                            chosen_one = e;
                        } else if (!fallback) {
                            fallback = e->nextSibling ();
                        }
                    }
                    if (e->id == id_node_ref) {
                        SMIL::MediaType * mt = static_cast<SMIL::MediaType*>(e);
                        if (!chosen_one) {
                            chosen_one = e;
                            currate = mt->bitrate;
                        } else if (int (mt->bitrate) <= max) {
                            int delta1 = pref > currate ? pref-currate : currate-pref;
                            int delta2 = pref > int (mt->bitrate) ? pref-mt->bitrate : mt->bitrate-pref;
                            if (delta2 < delta1) {
                                chosen_one = e;
                                currate = mt->bitrate;
                            }
                        }
                    } else if (!fallback && e->isPlayable ())
                        fallback = e;
                }
            if (!chosen_one)
                chosen_one = (fallback ? fallback : firstChild ());
        }
    }
    return chosen_one.ptr ();
}

KDE_NO_EXPORT void SMIL::Switch::begin () {
    Node *n = chosenOne ();
    if (n)
        n->activate ();
    else
        finish ();
}

KDE_NO_EXPORT void SMIL::Switch::deactivate () {
    Element::deactivate ();
    for (Node *e = firstChild (); e; e = e->nextSibling ())
        if (e->active ()) {
            e->deactivate ();
            break; // deactivate only the one running
        }
}

KDE_NO_EXPORT void SMIL::Switch::reset () {
    GroupBase::reset ();
    for (NodePtr e = firstChild (); e; e = e->nextSibling ()) {
        if (e->state != state_init)
            e->reset ();
    }
}

KDE_NO_EXPORT void SMIL::Switch::message (MessageType msg, void *content) {
    if (MsgChildFinished == msg) {
        Posting *post = (Posting *) content;
        if (post->source->state == state_finished)
            post->source->deactivate ();
        finish (); // only one child can run
        return;
    }
    GroupBase::message (msg, content);
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT SMIL::LinkingBase::LinkingBase (NodePtr & d, short id)
 : Element(d, id), show (show_replace) {}

KDE_NO_EXPORT void SMIL::LinkingBase::deactivate () {
    mediatype_attach.disconnect ();
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
    for (Node *c = firstChild(); c; c = c->nextSibling ())
        if (nodeMessageReceivers (c, MsgEventClicked)) {
            mediatype_attach.connect (c, MsgSurfaceAttach, this);
            break;
        }
    Element::activate ();
}

KDE_NO_EXPORT void SMIL::Anchor::message (MessageType msg, void *content) {
    switch (msg) {

        case MsgChildReady:
            if (parentNode ())
                parentNode ()->message (MsgChildReady, this);
            return;

        case MsgChildFinished: {
            Posting *post = (Posting *) content;
            if (unfinished ()) {
                if (post->source->nextSibling ())
                    post->source->nextSibling ()->activate ();
                else
                    finish ();
            }
            return;
        }

        default:
            break;
    }
    LinkingBase::message (msg, content);
}

Node *SMIL::Anchor::childFromTag (const QString & tag) {
    return fromMediaContentGroup (m_doc, tag);
}

KDE_NO_EXPORT void *SMIL::Anchor::role (RoleType msg, void *content) {
    switch (msg) {
    case RoleReady:
        return MsgBool (childrenReady (this));
    default:
        break;
    }
    return LinkingBase::role (msg, content);
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
        mediatype_attach.connect (parentNode (), MsgSurfaceAttach, this);
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

KDE_NO_EXPORT void *SMIL::Area::role (RoleType msg, void *content) {
    ConnectionList *l = mouse_listeners.receivers ((MessageType) (long) content);
    if (l)
        return l;
    return Element::role (msg, content);
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT SMIL::MediaType::MediaType (NodePtr &d, const QString &t, short id)
 : Mrl (d, id),
   runtime (new Runtime (this)),
   m_type (t),
   pan_zoom (NULL),
   bitrate (0),
   trans_start_time (0),
   trans_out_timer (NULL),
   sensitivity (sens_opaque),
   trans_out_active (false) {
    view_mode = Mrl::WindowMode;
}

KDE_NO_CDTOR_EXPORT SMIL::MediaType::~MediaType () {
    delete runtime;
    delete pan_zoom;
}

KDE_NO_EXPORT Node *SMIL::MediaType::childFromTag (const QString & tag) {
    Element * elm = fromContentControlGroup (m_doc, tag);
    if (!elm) elm = fromParamGroup (m_doc, tag);
    if (!elm) elm = fromAnimateGroup (m_doc, tag);
    if (elm)
        return elm;
    return NULL;
}

static NodePtr findExternalTree (Mrl *mrl) {
    for (Node *c = mrl->firstChild (); c; c = c->nextSibling ()) {
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
    if (mrl)
        size = mrl->size;
    title = getAttribute (StringPool::attr_title);
    Mrl::closed ();
}

KDE_NO_EXPORT
void SMIL::MediaType::parseParam (const TrieString &para, const QString & val) {
    if (para == StringPool::attr_fit) {
        fit = parseFit (val.ascii ());
    } else if (para == StringPool::attr_type) {
        mimetype = val;
    } else if (para == "panZoom") {
        QStringList coords = QStringList::split (QString (","), val);
        if (coords.size () < 4) {
            kWarning () << "panZoom less then four nubmers";
            return;
        }
        if (!pan_zoom)
            pan_zoom = new CalculatedSizer;
        pan_zoom->left = coords[0];
        pan_zoom->top = coords[1];
        pan_zoom->width = coords[2];
        pan_zoom->height = coords[3];
    } else if (para == "backgroundColor" || para == "background-color") {
        background_color = val.isEmpty () ? 0 : QColor (val).rgba ();
        background_color = setRGBA (background_color, bg_opacity);
    } else if (para == "mediaOpacity") {
        opacity = (int) SizeType (val, true).size (100);
    } else if (para == "mediaBackgroundOpacity" || para == "backgroundOpacity"){
        bg_opacity = (int) SizeType (val, true).size (100);
        background_color = setRGBA (background_color, bg_opacity);
    } else if (para == "system-bitrate") {
        bitrate = val.toInt ();
    } else if (para == "transIn") {
        Transition *t = findTransition (this, val);
        if (t) {
            trans_in = t;
            runtime->trans_in_dur = t->dur;
        } else {
            kWarning() << "Transition " << val << " not found in head";
        }
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
    } else if (sizes.setSizeParam (para, val)) {
        message (MsgSurfaceBoundsUpdate);
    } else if (!runtime->parseParam (para, val)) {
        if (para != StringPool::attr_src)
            Mrl::parseParam (para, val);
    }
    if (sub_surface) {
        sub_surface->markDirty ();
        sub_surface->setBackgroundColor (background_color);
        sub_surface->repaint ();
    }
}

KDE_NO_EXPORT void SMIL::MediaType::init () {
    if (Runtime::TimingsInitialized > runtime->timingstate) {
        trans_out_active = false;
        trans_start_time = 0;
        fit = fit_default;
        background_color = 0;
        opacity = 100;
        bg_opacity = 100;
        Mrl::init (); // sets all attributes
        for (NodePtr c = firstChild (); c; c = c->nextSibling ())
            if (SMIL::id_node_param == c->id)
                c->activate (); // activate param children
        runtime->timingstate = Runtime::TimingsInitialized;
    }
}

KDE_NO_EXPORT void SMIL::MediaType::activate () {
    init (); // sets all attributes
    if (!media_info) {
        kDebug() << "fallback";
        media_info = new MediaInfo (this, MediaManager::Any);
        if (!resolved && !src.isEmpty () && isPlayable ())
            media_info->wget (linkNode ()->absolutePath ());
    }
    setState (state_activated);
    runtime->start ();
}

KDE_NO_EXPORT void SMIL::MediaType::deactivate () {
    region_paint.disconnect ();
    region_attach.disconnect ();
    if (region_node)
        convertNode <SMIL::RegionBase> (region_node)->repaint ();
    transition_updater.disconnect ();
    if (trans_out_timer) {
        document ()->cancelPosting (trans_out_timer);
        trans_out_timer = NULL;
    }
    if (unfinished ())
        finish ();
    runtime->initialize ();
    Mrl::deactivate ();
    (void) surface ();
    region_node = 0L;
    if (media_info) {
        delete media_info;
        media_info = NULL;
    }
    postpone_lock = 0L;
}

KDE_NO_EXPORT void SMIL::MediaType::defer () {
    if (media_info) {
        //media_info->pause ();
        if (unfinished ())
            postpone_lock = document ()->postpone ();
        setState (state_deferred);
    }
}

KDE_NO_EXPORT void SMIL::MediaType::undefer () {
    if (runtime->started ()) {
        setState (state_began);
        if (media_info && media_info->media)
            media_info->media->unpause ();
        Surface *s = surface ();
        if (s)
            s->repaint ();
    } else {
        setState (state_activated);
    }
    postpone_lock = 0L;
}

KDE_NO_EXPORT void SMIL::MediaType::begin () {
    if (media_info && media_info->downloading ()) {
        postpone_lock = document ()->postpone ();
        state = state_began;
        return; // wait for MsgMediaReady
    }

    SMIL::RegionBase *r = findRegion (this, param (StringPool::attr_region));
    if (trans_out_timer) { // eg transOut and we're repeating
        document ()->cancelPosting (trans_out_timer);
        trans_out_timer = NULL;
    }
    for (NodePtr c = firstChild (); c; c = c->nextSibling ())
        if (SMIL::id_node_param != c->id && c != external_tree)
            c->activate (); // activate set/animate.. children
    if (r) {
        region_node = r;
        region_attach.connect (r, MsgSurfaceAttach, this);
        r->repaint ();
        clipStart ();
        Transition * trans = convertNode <Transition> (trans_in);
        if (trans && trans->supported ()) {
            active_trans = trans_in;
            runtime->timingstate = Runtime::TimingsTransIn;
            trans_gain = 0.0;
            transition_updater.connect (m_doc, MsgSurfaceUpdate, this);
            trans_start_time = document ()->last_event_time;
            trans_end_time = trans_start_time + 10 * trans->dur;
            if (Runtime::DurTimer == runtime->durTime ().durval &&
                    0 == runtime->durTime ().offset &&
                    Runtime::DurMedia == runtime->endTime ().durval)
                runtime->durTime ().durval = Runtime::DurTransition;
        }
        if (Runtime::DurTimer == runtime->durTime().durval &&
                runtime->durTime().offset > 0) {
            // FIXME: also account for fill duration
            trans = convertNode <Transition> (trans_out);
            if (trans && trans->supported () &&
                    (int) trans->dur < runtime->durTime().offset)
                trans_out_timer = document()->post (this,
                        new TimerPosting ((runtime->durTime().offset - trans->dur) * 10,
                        trans_out_timer_id));
        }
    } else {
        kWarning () << nodeName() << "::begin " << src << " region '" <<
            param (StringPool::attr_region) << "' not found" << endl;
    }
    Element::begin ();
}

KDE_NO_EXPORT void SMIL::MediaType::clipStart () {
    if (region_node && region_node->role (RoleDisplay)) {
        if (external_tree)
            external_tree->activate ();
        else if (media_info && media_info->media)
            media_info->media->play ();
    }
}

KDE_NO_EXPORT void SMIL::MediaType::clipStop () {
    if (runtime->timingstate == Runtime::timings_stopped) {
        region_attach.disconnect ();
        if (media_info && media_info->media)
            media_info->media->stop ();
        if (external_tree && external_tree->active ())
            external_tree->deactivate ();
    }
    if (sub_surface)
        sub_surface->repaint ();
    document_postponed.disconnect ();
}

KDE_NO_EXPORT void SMIL::MediaType::finish () {
    transition_updater.disconnect ();
    if (media_info && media_info->media)
        media_info->media->pause ();

    Surface *s = surface ();
    if (s)
        s->repaint ();
    runtime->finish ();
}

KDE_NO_EXPORT void SMIL::MediaType::reset () {
    Mrl::reset ();
    runtime->initialize ();
}

KDE_NO_EXPORT SRect SMIL::MediaType::calculateBounds () {
    SMIL::RegionBase *rb = convertNode <SMIL::RegionBase> (region_node);
    if (rb && rb->role (RoleDisplay)) {
        SRect rr = rb->region_surface->bounds;
        Single x, y, w = size.width, h = size.height;
        sizes.calcSizes (this, rr.width(), rr.height(), x, y, w, h);
        Fit ft = fit_default == fit ? rb->fit : fit;
        ImageMedia *im;
        switch (ft) {
            case fit_scroll:
            case fit_default:
            case fit_hidden:
                if (media_info &&
                        (MediaManager::AudioVideo == media_info->type ||
                        (MediaManager::Image == media_info->type &&
                         (im = static_cast <ImageMedia *>(media_info->media)) &&
                         !im->isEmpty () &&
                         im->cached_img->flags & ImageData::ImageScalable)))
                    ft = fit_meet;
                break;
            default:
                break;
        }

        if (!size.isEmpty () && w > 0 && h > 0)
            switch (ft) {
                case fit_meet: {
                    float iasp = 1.0 * size.width / size.height;
                    float rasp = 1.0 * w / h;
                    if (iasp > rasp)
                        h = size.height * w / size.width;
                    else
                        w = size.width * h / size.height;
                    break;
                }
                case fit_scroll:
                case fit_default:
                case fit_hidden:
                     w = size.width;
                     h = size.height;
                     break;
                case fit_slice: {
                    float iasp = 1.0 * size.width / size.height;
                    float rasp = 1.0 * w / h;
                    if (iasp > rasp)
                        w = size.width * h / size.height;
                    else
                        h = size.height * w / size.width;
                    break;
                }
                default: {} // fit_fill
            }
        return SRect (x, y, w, h);
    }
    return SRect ();
}

void SMIL::MediaType::message (MessageType msg, void *content) {
    switch (msg) {

        case MsgEventPostponed: {
            PostponedEvent *pe = static_cast <PostponedEvent *> (content);
            if (media_info) {
                if (pe->is_postponed) {
                    if (unfinished ()) {
                        setState (state_deferred);
                        if (media_info->media)
                            media_info->media->pause ();
                    }
                } else if (state == Node::state_deferred) {
                    setState (state_began);
                    if (media_info->media)
                        media_info->media->unpause ();
                }
            }
            return;
        }

        case MsgSurfaceUpdate: {
            UpdateEvent *ue = static_cast <UpdateEvent *> (content);

            trans_start_time += ue->skipped_time;
            trans_end_time += ue->skipped_time;

            trans_gain = 1.0 * (ue->cur_event_time - trans_start_time) /
                               (trans_end_time - trans_start_time);
            if (trans_gain > 0.9999) {
                transition_updater.disconnect ();
                if (active_trans == trans_in) {
                    runtime->timingstate = Runtime::timings_started;
                    deliver (MsgChildTransformedIn, this);
                }
                if (!trans_out_active)
                    active_trans = NULL;
                trans_gain = 1.0;
                if (Runtime::DurTransition == runtime->durTime ().durval) {
                    runtime->durTime ().durval = Runtime::DurTimer;
                    runtime->tryFinish ();
                }
            }
            Surface *s = surface ();
            if (s && s->parentNode())
                s->parentNode()->repaint (s->bounds);
            return;
        }

        case MsgSurfaceBoundsUpdate:
            if (sub_surface)
                sub_surface->resize (calculateBounds (), !!content);
            return;

        case MsgEventTimer: {
            TimerPosting *te = static_cast <TimerPosting *> (content);
            if (te->event_id == trans_out_timer_id) {
                if (active_trans)
                    transition_updater.disconnect ();
                trans_out_timer = NULL;
                active_trans = trans_out;
                Transition * trans = convertNode <Transition> (trans_out);
                if (trans) {
                    trans_gain = 0.0;
                    transition_updater.connect (m_doc, MsgSurfaceUpdate, this);
                    trans_start_time = document ()->last_event_time;
                    trans_end_time = trans_start_time + 10 * trans->dur;
                    trans_out_active = true;
                    Surface *s = surface ();
                    if (s)
                        s->repaint ();
                }
                return;
            }
            break;
        }

        case MsgStateFreeze:
            clipStop ();
            return;

        case MsgChildFinished: {
            Posting *post = (Posting *) content;
            if (post->source->mrl () &&
                    post->source->mrl ()->opener.ptr () == this) {
                post->source->deactivate (); // should only if fill not is freeze or hold
            } else if (active ()) {
                if (runtime->state () < Runtime::timings_stopped) {
                    if (runtime->started ())
                        runtime->tryFinish (); // what about repeat_count ..
                    return; // still running, wait for runtime to finish
                }
            }
            if (active ())
                finish ();
            return;
        }

        case MsgStateRewind:
            if (external_tree) {
                State old = state;
                state = state_deactivated;
                external_tree->reset ();
                state = old;
            }
            return;

        case MsgMediaReady: {
            resolved = true;
            Mrl *mrl = external_tree ? external_tree->mrl () : NULL;
            if (mrl) {
                size = mrl->size;
                message (MsgSurfaceBoundsUpdate);
            }
            postpone_lock = 0L;
            if (state == state_began) {
                begin ();
                runtime->tryFinish ();
            } else if (state < state_began && parentNode ()) {
                parentNode ()->message (MsgChildReady, this);
            }
            return;
        }

        case MsgMediaFinished:
            if (unfinished ()) {
                if (runtime->durTime ().durval == Runtime::DurMedia)
                    runtime->durTime ().durval = Runtime::DurTimer;
                if (media_info) {
                    delete media_info;
                    media_info = NULL;
                }
                postpone_lock = 0L;
                runtime->tryFinish ();
            }
            return;

        default:
            break;
    }
    if ((int) msg >= (int) Runtime::DurLastDuration)
        Mrl::message (msg, content);
    else
        runtime->message (msg, content);
}

void *SMIL::MediaType::role (RoleType msg, void *content) {
    switch (msg) {

    case RoleReady:
        return MsgBool (!media_info || !media_info->downloading ());

    case RoleTiming:
        return runtime;

    case RoleDisplay:
        return surface ();

    case RoleSizer:
        return &sizes;

    case RoleChildDisplay: {
        Surface *s = NULL;
        Mrl *mrl = (Mrl *) content;
        if (mrl) {
            size = mrl->size;
            message (MsgSurfaceBoundsUpdate);
            s = surface ();
        }
        return s;
    }

    case RoleReceivers: {
        MessageType m = (MessageType) (long) content;
        ConnectionList *l = mouse_listeners.receivers (m);
        if (l)
            return l;
        if (MsgSurfaceAttach == m)
            return &m_MediaAttached;
        if (MsgChildTransformedIn == m)
            return &m_TransformedIn;
    } // fall through

    default:
        break;
    }
    void *response = runtime->role (msg, content);
    if (response == MsgUnhandled)
        return Mrl::role (msg, content);
    return response;
}


Surface *SMIL::MediaType::surface () {
    if (!runtime->active ()) {
        if (sub_surface)
            sub_surface->remove ();
        sub_surface = NULL;
    } else if (!sub_surface && region_node) {
        Surface *rs = (Surface *) region_node->role (RoleDisplay);
        if (rs) {
            sub_surface = rs->createSurface (this, SRect ());
            sub_surface->setBackgroundColor (background_color);
            message (MsgSurfaceBoundsUpdate);
        }
    }
    return sub_surface.ptr ();
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT
SMIL::RefMediaType::RefMediaType (NodePtr &d, const QString &t)
 : SMIL::MediaType (d, t, id_node_ref) {}

KDE_NO_EXPORT Node *SMIL::RefMediaType::childFromTag (const QString & tag) {
    return fromXMLDocumentTag (m_doc, tag);
}

KDE_NO_EXPORT void SMIL::RefMediaType::clipStart () {
    PlayListNotify *n = document ()->notify_listener;
    if (n && region_node && !external_tree && !src.isEmpty()) {
        repeat = runtime->repeat_count == Runtime::DurIndefinite
            ? 9998 : runtime->repeat_count;
        runtime->repeat_count = 1;
        document_postponed.connect (document(), MsgEventPostponed, this);
    }
    MediaType::clipStart ();
}

KDE_NO_EXPORT void SMIL::RefMediaType::finish () {
    if (runtime->durTime ().durval == Runtime::DurMedia)
        runtime->durTime ().durval = Runtime::DurTimer;//reset to make this finish
    MediaType::finish ();
}

KDE_NO_EXPORT void SMIL::RefMediaType::begin () {
    if (0 == runtime->durTime ().offset &&
            Runtime::DurMedia == runtime->endTime ().durval)
        runtime->durTime ().durval = Runtime::DurMedia; // duration of clip
    MediaType::begin ();
}

void
SMIL::RefMediaType::parseParam (const TrieString &name, const QString &val) {
    //kDebug () << name.toString() << "=" << val;
    if (name == StringPool::attr_src) {
        if (!media_info)
            media_info = new MediaInfo (this, MediaManager::AudioVideo);
        if (!resolved || src != val) {
            if (external_tree)
                removeChild (external_tree);
            src = val;
            resolved = media_info->wget (absolutePath ());
        }
        if (state == state_began && resolved)
            clipStart ();
    } else {
        MediaType::parseParam (name, val);
    }
}

KDE_NO_EXPORT void SMIL::RefMediaType::accept (Visitor * v) {
    v->visit (this);
}

KDE_NO_EXPORT bool SMIL::RefMediaType::expose () const {
    return !src.isEmpty () && !external_tree;
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT
SMIL::ImageMediaType::ImageMediaType (NodePtr & d)
    : SMIL::MediaType (d, "img", id_node_img) {}

namespace {
    class SvgElement : public Element {
        QString tag;
        NodePtrW image;

    public:
        SvgElement (NodePtr &doc, Node *img, const QString &t, short id=0)
            : Element (doc, id), tag (t), image (img) {}

        void parseParam (const TrieString &name, const QString &val) {
            setAttribute (name, val);
            Mrl *mrl = image ? image->mrl () : NULL;
            if (mrl && mrl->media_info &&
                    MediaManager::Image == mrl->media_info->type) {
                ImageMedia *im=static_cast<ImageMedia*>(mrl->media_info->media);
                if (im)
                    im->updateRender ();
            }
        }

        Node *childFromTag (const QString & tag) {
            return new SvgElement (m_doc, image.ptr (), tag);
        }

        const char *nodeName () const {
            return tag.ascii ();
        }

        bool expose () const {
            return false;
        }
    };
}

KDE_NO_EXPORT Node *SMIL::ImageMediaType::childFromTag (const QString & tag) {
    if (!strcmp (tag.latin1 (), "imfl"))
        return new RP::Imfl (m_doc);
    else if (!strcmp (tag.latin1 (), "svg"))
        return new SvgElement (m_doc, this, tag, id_node_svg);
    return SMIL::MediaType::childFromTag (tag);
}

KDE_NO_EXPORT void SMIL::ImageMediaType::activate () {
    if (!media_info)
        media_info = new MediaInfo (this, MediaManager::Image);

    MediaType::activate ();

    if (src.isEmpty () && !media_info->media) {
        Node *n = findChildWithId (this, id_node_svg);
        if (n) {
            media_info->media = new ImageMedia (this);
            message (MsgMediaReady);
        }
    }
}

KDE_NO_EXPORT void SMIL::ImageMediaType::accept (Visitor * v) {
    v->visit (this);
}

void
SMIL::ImageMediaType::parseParam (const TrieString &name, const QString &val) {
    //kDebug () << "ImageRuntime::param " << name.toString() << "=" << val;
    if (name == StringPool::attr_src) {
        if (media_info)
            media_info->killWGet ();
        if (external_tree) {
            removeChild (external_tree);
        } else {
            Node *n = findChildWithId (this, id_node_svg);
            if (n)
                removeChild (n);
        }
        src = val;
        if (!src.isEmpty ()) {
            if (!media_info) {
                media_info = new MediaInfo (this, MediaManager::Image);
            }
            media_info->wget (absolutePath ());
        } else if (media_info) {
            media_info->clearData ();
        }
    } else {
        MediaType::parseParam (name, val);
    }
}

void SMIL::ImageMediaType::message (MessageType msg, void *content) {
    if (media_info) {
        switch (msg) {

            case MsgMediaUpdated: {
                Surface *s = surface ();
                if (s) {
                    s->markDirty ();
                    s->repaint ();
                }
                if (state >= state_finished)
                    clipStop ();
                return;
            }

            case MsgMediaFinished:
                if (state >= Node::state_began)
                    runtime->tryFinish ();
                return;

            case MsgChildFinished:
                if (id_node_svg == ((Posting *) content)->source->id)
                    return;

            case MsgMediaReady:
                if (media_info) {
                    ImageMedia *im = static_cast <ImageMedia *> (media_info->media);
                    if (im && !im->isEmpty ())
                        im->sizes (size);
                }
                break;

            default:
                break;
        }
    }
    MediaType::message (msg, content);
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT SMIL::TextMediaType::TextMediaType (NodePtr & d)
    : SMIL::MediaType (d, "text", id_node_text) {}

KDE_NO_EXPORT void SMIL::TextMediaType::init () {
    if (Runtime::TimingsInitialized > runtime->timingstate) {
        if (!media_info)
            media_info = new MediaInfo (this, MediaManager::Text);
        font_size = TextMedia::defaultFontSize ();
        font_color = 0;
        font_name = "sans";
        halign = align_left;

        MediaType::init ();
    }
}

void
SMIL::TextMediaType::parseParam (const TrieString &name, const QString &val) {
    if (!media_info)
        return;
    //kDebug () << "TextRuntime::parseParam " << name.toString() << "=" << val;
    if (name == StringPool::attr_src) {
        src = val;
        if (!val.isEmpty ())
            media_info->wget (absolutePath ());
        else
            media_info->clearData ();
        return;
    }
    if (name == "fontColor") {
        font_color = val.isEmpty () ? 0 : QColor (val).rgb ();
    } else if (name == "fontFace") {
        if (val.lower ().indexOf ("sans" ) < 0)
            font_name = "serif";
    } else if (name == "fontPtSize") {
        font_size = val.isEmpty() ? TextMedia::defaultFontSize() : val.toInt();
    } else if (name == "fontSize") {
        font_size += val.isEmpty() ? TextMedia::defaultFontSize() : val.toInt();
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
    if (sub_surface) {
        size = SSize ();
        sub_surface->resize (calculateBounds (), true);
    }
}

KDE_NO_EXPORT void SMIL::TextMediaType::accept (Visitor * v) {
    v->visit (this);
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT SMIL::Brush::Brush (NodePtr & d)
    : SMIL::MediaType (d, "brush", id_node_brush) {}

KDE_NO_EXPORT void SMIL::Brush::accept (Visitor * v) {
    v->visit (this);
}

KDE_NO_EXPORT void SMIL::Brush::parseParam (const TrieString &param, const QString &val) {
    if (param == "color") {
        color = val.isEmpty () ? 0 : QColor (val).rgb ();
        Surface *s = surface ();
        if (s)
            s->repaint ();
    } else {
        MediaType::parseParam (param, val);
    }
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT SMIL::SmilText::SmilText (NodePtr &d)
 : Element (d, id_node_smil_text),
   runtime (new Runtime (this)) {}

KDE_NO_CDTOR_EXPORT SMIL::SmilText::~SmilText () {
    delete runtime;
}

void SMIL::SmilText::init () {
    if (Runtime::TimingsInitialized > runtime->timingstate) {
        props.init ();
        Element::init ();
        runtime->timingstate = Runtime::TimingsInitialized;
    }
}

void SMIL::SmilText::activate () {
    init (); // sets all attributes
    setState (state_activated);
    for (NodePtr c = firstChild (); c; c = c->nextSibling ())
        c->activate ();
    runtime->start ();
}

void SMIL::SmilText::begin () {
    SMIL::RegionBase *r = findRegion (this, param (StringPool::attr_region));
    if (r) {
        region_node = r;
        region_attach.connect (r, MsgSurfaceAttach, this);
        r->repaint ();
    }
    Element::begin ();

}

void SMIL::SmilText::finish () {
    runtime->finish ();
}

void SMIL::SmilText::deactivate () {
    region_attach.disconnect ();
    if (text_surface) {
        text_surface->repaint ();
        text_surface->remove ();
        text_surface = NULL;
    }
    runtime->initialize ();
    Element::deactivate ();
}

void SMIL::SmilText::reset () {
    runtime->initialize ();
    Element::reset ();
}

Node *SMIL::SmilText::childFromTag (const QString &tag) {
    const char *ctag = tag.ascii ();
    if (!strcmp (ctag, "tev"))
    {}//return new SMIL::TextTev (m_doc);
    return fromTextFlowGroup (m_doc, tag);
}

void SMIL::SmilText::parseParam (const TrieString &name, const QString &value) {
    if (!props.parseParam (name, value) &&
            !runtime->parseParam (name, value)) {
        Element::parseParam (name, value);
    }
}

void SMIL::SmilText::message (MessageType msg, void *content) {
    switch (msg) {

        case MsgSurfaceBoundsUpdate:
            if (content && text_surface)
                text_surface->resize (text_surface->bounds, true);
            return;

        case MsgStateFreeze:
            if (!runtime->active () && text_surface) {
                text_surface->repaint ();
                text_surface->remove ();
                text_surface = NULL;
            }
            return;

        case MsgChildFinished:
            return;

        default:
            break;
    }
    if ((int) msg >= (int) Runtime::DurLastDuration)
        Element::message (msg, content);
    else
        runtime->message (msg, content);
}

void *SMIL::SmilText::role (RoleType msg, void *content) {
    switch (msg) {

    case RoleTiming:
        return runtime;

    case RoleDisplay:
        return surface ();

    case RoleReceivers: {
        MessageType msgt = (MessageType) (long) content;
        ConnectionList *l = mouse_listeners.receivers (msgt);
        if (l)
            return l;
        if (MsgSurfaceAttach == msgt)
            return &media_attached;
    } // fall through

    default:
        break;
    }
    void *response = runtime->role (msg, content);
    if (response == MsgUnhandled)
        return Element::role (msg, content);
    return response;
}

Surface *SMIL::SmilText::surface () {
    if (!runtime->active ()) {
        if (text_surface) {
            text_surface->remove ();
            text_surface = NULL;
        }
    } else if (region_node) {
        Surface *rs = (Surface *) region_node->role (RoleDisplay);
        if (rs) {
            SRect b = rs->bounds;
            if (!text_surface)
                text_surface = rs->createSurface (this,
                        SRect (0, 0, b.width (), b.height ()));
#ifdef KMPLAYER_WITH_CAIRO
            else if (!text_surface->surface)
                text_surface->bounds = SRect (0, 0, b.width (), b.height ());
#endif
        }
    }
    return text_surface.ptr ();
}

//-----------------------------------------------------------------------------

void SmilTextProperties::init () {
    font_color = -1;
    background_color = -1;
    text_direction = DirInherit;
    font_family = "sans";
    font_size = -1;
    font_style = StyleInherit;
    font_weight = WeightInherit;
    text_mode = ModeInherit;
    text_place = PlaceInherit;
    text_style = "";
    text_wrap = WrapInherit;
    space = SpaceDefault;
    text_writing = WritingLrTb;
    text_align = AlignInherit;
}

bool SmilTextProperties::parseParam(const TrieString &name, const QString &val) {
    if (name == "textWrap") {
        // { Wrap, NoWrap, WrapInherit } text_wrap;
    } else if (name == "space" /*xml:space*/) {
        // { SpaceDefault, SpacePreserve } space;
    } else if (name == "textAlign") {
        if (val == "left")
            text_align = AlignLeft;
        else if (val == "center")
            text_align = AlignCenter;
        else if (val == "right")
            text_align = AlignRight;
        // start, end
        else
            text_align = AlignInherit;
    } else if (name == "textBackgroundColor") {
        background_color = 0xffffff & QColor (val).rgb ();
    } else if (name == "textColor") {
        font_color = 0xffffff & QColor (val).rgb ();
    } else if (name == "textDirection") {
        if (val == "ltr")
            text_direction = DirLtr;
        else if (val == "rtl")
            text_direction = DirRtl;
        else
            text_direction = DirInherit;
        //  DirLtro, DirRtlo
    } else if (name == "textFontFamily") {
        font_family = val;
    } else if (name == "textFontSize") {
        font_size = val.toInt ();
    } else if (name == "textFontStyle") {
        if (val == "normal")
            font_style = StyleNormal;
        else if (val == "italic")
            font_style = StyleItalic;
        else if (val == "oblique")
            font_style = StyleOblique;
        else if (val == "reverseOblique")
            font_style = StyleRevOblique;
        else
            font_style = StyleInherit;
    } else if (name == "textFontWeight") {
        if (val == "normal")
            font_weight = WeightNormal;
        else if (val == "bold")
            font_weight = WeightBold;
        else
            font_weight = WeightInherit;
    } else if (name == "textMode") {
        // { ModeAppend, ModeReplace, ModeInherit } text_mode;
    } else if (name == "textPlace") {
        //enum { PlaceStart, PlaceCenter, PlaceEnd, PlaceInherit } text_place;
    } else if (name == "textStyle") {
        text_style = val;
    } else if (name == "textWritingMode") {
        // { WritingLrTb, WritingRlTb, WritingTbLr, WritingTbRl } text_writing;
    } else {
        return false;
    }
    return true;
}

void SmilTextProperties::mask (const SmilTextProperties &props) {
    if (props.font_size > 0)
        font_size = props.font_size;
    if (props.font_color > -1)
        font_color = props.font_color;
    if (props.background_color > -1)
        background_color = props.background_color;
    if (StyleInherit != props.font_style)
        font_style = props.font_style;
    if (WeightInherit != props.font_weight)
        font_weight = props.font_weight;
    if (AlignInherit != props.text_align)
        text_align = props.text_align;
    font_family = props.font_family;
}

KDE_NO_CDTOR_EXPORT
SMIL::TextFlow::TextFlow (NodePtr &doc, short id, const QByteArray &t)
 : Element (doc, id), tag (t) {}

KDE_NO_CDTOR_EXPORT SMIL::TextFlow::~TextFlow () {}

void SMIL::TextFlow::init () {
    props.init ();
    Element::init ();
}

void SMIL::TextFlow::activate () {
    init ();
    Element::activate ();
}

Node *SMIL::TextFlow::childFromTag (const QString &tag) {
    return fromTextFlowGroup (m_doc, tag);
}

void SMIL::TextFlow::parseParam(const TrieString &name, const QString &val) {
    if (!props.parseParam (name, val))
        Element::parseParam (name, val);
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT SMIL::AnimateGroup::AnimateGroup (NodePtr &d, short _id)
 : Element (d, _id),
   runtime (new Runtime (this)),
   modification_id (-1) {}

KDE_NO_CDTOR_EXPORT SMIL::AnimateGroup::~AnimateGroup () {
    delete runtime;
}

void SMIL::AnimateGroup::parseParam (const TrieString &name, const QString &val) {
    if (name == StringPool::attr_target || name == "targetElement") {
        target_id = val;
    } else if (name == "attribute" || name == "attributeName") {
        changed_attribute = TrieString (val);
    } else if (name == "to") {
        change_to = val;
    } else if (!runtime->parseParam (name, val)) {
        Element::parseParam (name, val);
    }
}

KDE_NO_EXPORT void SMIL::AnimateGroup::init () {
    if (Runtime::TimingsInitialized > runtime->timingstate) {
        Element::init ();
        runtime->timingstate = Runtime::TimingsInitialized;
    }
}

KDE_NO_EXPORT void SMIL::AnimateGroup::activate () {
    init ();
    setState (state_activated);
    runtime->start ();
}

/**
 * animation finished
 */
KDE_NO_EXPORT void SMIL::AnimateGroup::finish () {
    //kDebug () << "AnimateGroup::stopped " << durTime ().durval << endl;
    runtime->finish ();
}

KDE_NO_EXPORT void SMIL::AnimateGroup::reset () {
    Element::reset ();
    target_id.truncate (0);
    runtime->initialize ();
}

KDE_NO_EXPORT void SMIL::AnimateGroup::deactivate () {
    restoreModification ();
    if (unfinished ())
        finish ();
    runtime->initialize ();
    Element::deactivate ();
}

KDE_NO_EXPORT void SMIL::AnimateGroup::message (MessageType msg, void *data) {
    switch (msg) {

        case MsgStateFreeze:
            if (!runtime->active ())
                restoreModification ();
            return;

        case MsgStateRewind:
            restoreModification ();
            return;

        default:
            break;
    }
    if ((int) msg >= (int) Runtime::DurLastDuration)
        Element::message (msg, data);
    else
        runtime->message (msg, data);
}

KDE_NO_EXPORT void *SMIL::AnimateGroup::role (RoleType msg, void *data) {
    switch (msg) {

    case RoleTiming:
        return runtime;

    default:
        break;
    }
    void *response = runtime->role (msg, data);
    if (response == MsgUnhandled)
        return Element::role (msg, data);
    return response;
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

KDE_NO_EXPORT Node *SMIL::AnimateGroup::targetElement () {
    if (target_id.isEmpty ()) {
        for (Node *p = parentNode(); p; p =p->parentNode())
            if (SMIL::id_node_first_mediatype <= p->id &&
                    SMIL::id_node_last_mediatype >= p->id) {
                target_element = p;
                break;
            }
    } else {
        target_element = findLocalNodeById (this, target_id);
    }
    return target_element.ptr ();
}

//-----------------------------------------------------------------------------

KDE_NO_EXPORT void SMIL::Set::begin () {
    restoreModification ();
    Element *target = static_cast <Element *> (targetElement ());
    if (target)
        target->setParam (changed_attribute, change_to, &modification_id);
    else
        kWarning () << "target element not found" << endl;
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

KDE_NO_CDTOR_EXPORT SMIL::AnimateBase::AnimateBase (NodePtr &d, short id)
 : AnimateGroup (d, id),
   anim_timer (NULL),
   keytimes (NULL),
   spline_table (NULL),
   keytime_count (0) {}

KDE_NO_CDTOR_EXPORT SMIL::AnimateBase::~AnimateBase () {
    if (keytimes)
        free (keytimes);
    if (spline_table)
        free (spline_table);
}

KDE_NO_EXPORT void SMIL::AnimateBase::init () {
    if (Runtime::TimingsInitialized > runtime->timingstate) {
        if (anim_timer) {
            document ()->cancelPosting (anim_timer);
            anim_timer = NULL;
        }
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
        AnimateGroup::init ();
    }
}

KDE_NO_EXPORT void SMIL::AnimateBase::begin () {
    interval = 0;
    if (!setInterval ())
        return;
    applyStep ();
    if (calc_discrete != calcMode)
        change_updater.connect (m_doc, MsgSurfaceUpdate, this);
    AnimateGroup::begin ();
}

KDE_NO_EXPORT void SMIL::AnimateBase::finish () {
    if (anim_timer) { // make sure timers are stopped
        document ()->cancelPosting (anim_timer);
        anim_timer = NULL;
    }
    change_updater.disconnect ();
    AnimateGroup::finish ();
}

KDE_NO_EXPORT void SMIL::AnimateBase::deactivate () {
    if (anim_timer) {
        document ()->cancelPosting (anim_timer);
        anim_timer = NULL;
    } else {
        change_updater.disconnect ();
    }
    if (spline_table)
        free (spline_table);
    spline_table = NULL;
    AnimateGroup::deactivate ();
}

KDE_NO_EXPORT void SMIL::AnimateBase::message (MessageType msg, void *data) {
    switch (msg) {
        case MsgEventTimer: {
            TimerPosting *te = static_cast <TimerPosting *> (data);
            if (te->event_id == anim_timer_id) {
                anim_timer = NULL;
                timerTick (0);
                return;
            }
            break;
        }
        case MsgSurfaceUpdate: {
            UpdateEvent *ue = static_cast <UpdateEvent *> (data);
            interval_start_time += ue->skipped_time;
            interval_end_time += ue->skipped_time;
            timerTick (ue->cur_event_time);
            return;
        }
        case MsgStateRewind:
            restoreModification ();
            if (anim_timer) {
                document ()->cancelPosting (anim_timer);
                anim_timer = NULL;
            } else {
                change_updater.disconnect ();
            }
            break;
        default:
            break;
    }
    AnimateGroup::message (msg, data);
}

void SMIL::AnimateBase::parseParam (const TrieString &name, const QString &val) {
    if (name == "from") {
        change_from = val;
    } else if (name == "by" || name == "change_by") {
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
        for (unsigned int i = 0; i < keytime_count; i++) {
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

static SMIL::AnimateBase::Point2D cubicBezier (float ax, float bx, float cx,
        float ay, float by, float cy, float t) {
    float   tSquared, tCubed;
    SMIL::AnimateBase::Point2D result;

    /* calculate the curve point at parameter value t */

    tSquared = t * t;
    tCubed = tSquared * t;

    result.x = (ax * tCubed) + (bx * tSquared) + (cx * t);
    result.y = (ay * tCubed) + (by * tSquared) + (cy * t);

    return result;
}

static
float cubicBezier (SMIL::AnimateBase::Point2D *table, int a, int b, float x) {
    if (b > a + 1) {
        int mid = (a + b) / 2;
        if (table[mid].x > x)
            return cubicBezier (table, a, mid, x);
        else
            return cubicBezier (table, mid, b, x);
    }
    return table[a].y + (x - table[a].x) / (table[b].x - table[a].x) * (table[b].y - table[a].y);
}


bool SMIL::AnimateBase::setInterval () {
    int cs = runtime->durTime ().offset;
    if (keytime_count > interval + 1)
        cs = (int) (cs * (keytimes[interval+1] - keytimes[interval]));
    else if (keytime_count > interval && calc_discrete == calcMode)
        cs = (int) (cs * (1.0 - keytimes[interval]));
    else if (values.size () > 0 && calc_discrete == calcMode)
        cs /= values.size ();
    else if (values.size () > 1)
        cs /= values.size () - 1;
    if (cs < 0) {
        kWarning () << "animateMotion has no valid duration interval " <<
            interval << endl;
        runtime->doFinish ();
        return false;
    }
    interval_start_time = document ()->last_event_time;
    interval_end_time = interval_start_time + 10 * cs;
    switch (calcMode) {
        case calc_paced: // FIXME
        case calc_linear:
            break;
        case calc_spline:
            if (splines.size () > (int)interval) {
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
            anim_timer = document ()->post (this,
                    new TimerPosting (10 * cs, anim_timer_id));
            break;
        default:
            break;
    }
    //kDebug() << "setInterval " << steps << " " <<
    //    cur_x.size () << "," << cur_y.size () << "=>"
    //    << end_x.size () << "," << end_y.size () << " d:" << 
    //    delta_x.size () << "," << delta_y.size () << endl;
    return true;
}

//-----------------------------------------------------------------------------

SMIL::Animate::Animate (NodePtr &doc)
 : AnimateBase (doc, id_node_animate),
   num_count (0), begin_(NULL), cur (NULL), delta (NULL), end (NULL) {
}

KDE_NO_EXPORT void SMIL::Animate::init () {
    if (Runtime::TimingsInitialized > runtime->timingstate) {
        cleanUp ();
        AnimateBase::init ();
    }
}

KDE_NO_EXPORT void SMIL::Animate::cleanUp () {
    if (anim_timer) {
        document ()->cancelPosting (anim_timer);
        anim_timer = NULL;
    }
    delete [] begin_;
    delete [] cur;
    delete [] delta;
    delete [] end;
    begin_ = cur = delta = end = NULL;
    num_count = 0;
}

KDE_NO_EXPORT void SMIL::Animate::deactivate () {
    cleanUp ();
    AnimateBase::deactivate ();
}

KDE_NO_EXPORT void SMIL::Animate::begin () {
    restoreModification ();
    cleanUp (); // FIXME: repeating doesn't reinit

    NodePtr protect = target_element;
    Element *target = static_cast <Element *> (targetElement ());
    if (!target) {
        kWarning () << "target element not found";
        runtime->doFinish ();
        return;
    }
    if (values.size () < 2) {
        values.push_front (change_from.isEmpty ()
                ? target->param (changed_attribute)
                : change_from);
        if (!change_to.isEmpty ()) {
            values.push_back (change_to);
        } else if (!change_by.isEmpty ()) {
            SizeType b (values[0]);
            b += SizeType (change_by);
            values.push_back (b.toString ());
        }
    }
    if (values.size () < 2) {
        kWarning () << "could not determine change values";
        runtime->doFinish ();
        return;
    }
    if (calcMode != calc_discrete) {
        QStringList bnums = QStringList::split (QString (","), values[0]);
        QStringList enums = QStringList::split (QString (","), values[1]);
        num_count = bnums.size ();
        if (num_count) {
            begin_ = new SizeType [num_count];
            end = new SizeType [num_count];
            cur = new SizeType [num_count];
            delta = new SizeType [num_count];
            for (int i = 0; i < num_count; ++i) {
                begin_[i] = bnums[i];
                end[i] = i < enums.size () ? enums[i] : bnums[i];
                cur[i] = begin_[i];
                delta[i] = end[i];
                delta[i] -= begin_[i];
            }
        }
    }
    AnimateBase::begin ();
}

KDE_NO_EXPORT void SMIL::Animate::finish () {
    if (active () && calc_discrete != calcMode)
        for (int i = 0; i < num_count; ++i)
            if (cur[i].size () != end[i].size ()) {
                for (int j = 0; j < num_count; ++j)
                    cur[j] = end[j];
                applyStep (); // we lost some steps ..
                break;
            }
    AnimateBase::finish ();
}

KDE_NO_EXPORT void SMIL::Animate::applyStep () {
    Element *target = convertNode <Element> (target_element);
    if (target) {
        if (calcMode != calc_discrete) {
            if (num_count) {
                QString val (cur[0].toString ());
                for (int i = 1; i < num_count; ++i)
                    val += QChar (',') + cur[i].toString ();
                target->setParam (changed_attribute, val, &modification_id);
            }
        } else if ((int)interval < values.size ()) {
            target->setParam (changed_attribute,
                    values[interval], &modification_id);
        }
    }
}

KDE_NO_EXPORT bool SMIL::Animate::timerTick (unsigned int cur_time) {
    if (cur_time && cur_time <= interval_end_time) {
        float gain = 1.0 * (cur_time - interval_start_time) /
                           (interval_end_time - interval_start_time);
        if (gain > 1.0) {
            change_updater.disconnect ();
            gain = 1.0;
        }
        switch (calcMode) {
            case calc_paced: // FIXME
            case calc_linear:
                break;
            case calc_spline:
                if (spline_table)
                    gain = cubicBezier (spline_table, 0, 99, gain);
                break;
            case calc_discrete:
                return false; // shouldn't come here
        }
        for (int i = 0; i < num_count; ++i) {
            cur[i] = delta[i];
            cur[i] *= gain;
            cur[i] += begin_[i];
        }
        applyStep ();
        return true;
    } else if (values.size () > (int) ++interval) {
        if (calc_discrete != calcMode) {
            if (values.size () <= (int) interval + 1)
                return false;
            QStringList enums = QStringList::split (QString (","), values[interval+1]);
            for (int i = 0; i < num_count; ++i) {
                begin_[i] = end[i];
                if (i < enums.size ())
                    end[i] = enums[i];
                cur[i] = begin_[i];
                delta[i] = end[i];
                delta[i] -= begin_[i];
            }
        }
        if (setInterval ()) {
            applyStep ();
            return true;
        }
    }
    return false;
}

//-----------------------------------------------------------------------------

static
bool getMotionCoordinates (const QString &coord, SizeType &x, SizeType &y) {
    int p = coord.indexOf (QChar (','));
    if (p < 0)
        p = coord.indexOf (QChar (' '));
    if (p > 0) {
        x = coord.left (p).stripWhiteSpace ();
        y = coord.mid (p + 1).stripWhiteSpace ();
        return true;
    }
    return false;
}

KDE_NO_EXPORT void SMIL::AnimateMotion::init () {
    cur_x = cur_y = delta_x = delta_y = SizeType();
    AnimateBase::init ();
}

KDE_NO_EXPORT void SMIL::AnimateMotion::begin () {
    //kDebug () << "AnimateMotion::started " << durTime ().durval << endl;
    Node *t = targetElement ();
    CalculatedSizer *sizes = t ? (CalculatedSizer *) t->role (RoleSizer) : NULL;
    if (!sizes)
        return;
    old_sizes = *sizes;
    if (anim_timer) {
        document ()->cancelPosting (anim_timer);
        anim_timer = NULL;
    }
    if (change_from.isEmpty ()) {
        if (values.size () > 1) {
            getMotionCoordinates (values[0], begin_x, begin_y);
            getMotionCoordinates (values[1], end_x, end_y);
        } else {
            if (sizes->left.isSet ()) {
                begin_x = sizes->left;
            } else if (sizes->right.isSet() && sizes->width.isSet ()) {
                begin_x = sizes->right;
                begin_x -= sizes->width;
            } else {
                begin_x = "0";
            }
            if (sizes->top.isSet ()) {
                begin_y = sizes->top;
            } else if (sizes->bottom.isSet() && sizes->height.isSet ()) {
                begin_y = sizes->bottom;
                begin_y -= sizes->height;
            } else {
                begin_y = "0";
            }
        }
    } else {
        getMotionCoordinates (change_from, begin_x, begin_y);
    }
    if (!change_by.isEmpty ()) {
        getMotionCoordinates (change_by, delta_x, delta_y);
        end_x = begin_x;
        end_y = begin_y;
        end_x += delta_x;
        end_y += delta_y;
    } else if (!change_to.isEmpty ()) {
        getMotionCoordinates (change_to, end_x, end_y);
    }
    cur_x = begin_x;
    cur_y = begin_y;
    delta_x = end_x;
    delta_x -= begin_x;
    delta_y = end_y;
    delta_y -= begin_y;
    AnimateBase::begin ();
}

KDE_NO_EXPORT void SMIL::AnimateMotion::finish () {
    if (active ()) {
        if (calcMode != calc_discrete &&
                (cur_x.size () != end_x.size () ||
                 cur_y.size () != end_y.size ())) {
            cur_x = end_x;
            cur_y = end_y;
            applyStep (); // we lost some steps ..
        }
    }
    AnimateBase::finish ();
}

KDE_NO_EXPORT void SMIL::AnimateMotion::restoreModification () {
    Node *n = target_element.ptr ();
    CalculatedSizer *sizes = n ? (CalculatedSizer *) n->role (RoleSizer) : NULL;
    if (sizes) {
        *sizes = old_sizes;
        n->message (MsgSurfaceBoundsUpdate);
    }
}

KDE_NO_EXPORT void SMIL::AnimateMotion::applyStep () {
    Node *n = target_element.ptr ();
    CalculatedSizer *sizes = n ? (CalculatedSizer *) n->role (RoleSizer) : NULL;
    if (n->role (RoleDisplay)) {
        sizes->move (cur_x, cur_y);
        n->message (MsgSurfaceBoundsUpdate);
    }
}

KDE_NO_EXPORT bool SMIL::AnimateMotion::timerTick (unsigned int cur_time) {
    if (cur_time && cur_time <= interval_end_time) {
        float gain = 1.0 * (cur_time - interval_start_time) /
                           (interval_end_time - interval_start_time);
        if (gain > 1.0) {
            change_updater.disconnect ();
            gain = 1.0;
        }
        switch (calcMode) {
            case calc_paced: // FIXME
            case calc_linear:
                break;
            case calc_spline:
                if (spline_table)
                    gain = cubicBezier (spline_table, 0, 99, gain);
                break;
            case calc_discrete:
                return false; // shouldn't come here
        }
        cur_x = delta_x;
        cur_y = delta_y;
        cur_x *= gain;
        cur_y *= gain;
        cur_x += begin_x;
        cur_y += begin_y;
        applyStep ();
        return true;
    } else if (values.size () > (int) ++interval) {
        getMotionCoordinates (values[interval], begin_x, begin_y);
        cur_x = begin_x;
        cur_y = begin_y;
        if (calcMode != calc_discrete && values.size () > (int) interval + 1) {
            getMotionCoordinates (values[interval+1], end_x, end_y);
            delta_x = end_x;
            delta_x -= begin_x;
            delta_y = end_y;
            delta_y -= begin_y;
        }
        if (setInterval ()) {
            applyStep ();
            return true;
        }
    }
    return false;
}

//-----------------------------------------------------------------------------

static bool getAnimateColor (unsigned int val, SMIL::AnimateColor::Channels &c) {
    c.alpha = val >> 24;
    c.red = (val >> 16) & 0xff;
    c.green = (val >> 8) & 0xff;
    c.blue = val & 0xff;
    return true;
}

static bool getAnimateColor (const QString &val, SMIL::AnimateColor::Channels &c) {
    QColor color (val);
    return getAnimateColor (color.rgba (), c);
}

static short colorNormalise (int c) {
    if (c > 255)
        return 255;
    if (c < -255)
        return -255;
    return c;
}

SMIL::AnimateColor::Channels &SMIL::AnimateColor::Channels::operator *= (const float f) {
    alpha = colorNormalise ((int) (f * alpha));
    red = colorNormalise ((int) (f * red));
    green = colorNormalise ((int) (f * green));
    blue = colorNormalise ((int) (f * blue));
    return *this;
}

SMIL::AnimateColor::Channels &SMIL::AnimateColor::Channels::operator += (const Channels &c) {
    alpha = colorNormalise ((int) alpha + c.alpha);
    red = colorNormalise ((int) red + c.red);
    green = colorNormalise ((int) green + c.green);
    blue = colorNormalise ((int) blue + c.blue);
    return *this;
}

SMIL::AnimateColor::Channels &SMIL::AnimateColor::Channels::operator -= (const Channels &c) {
    alpha = colorNormalise ((int) alpha - c.alpha);
    red = colorNormalise ((int) red - c.red);
    green = colorNormalise ((int) green - c.green);
    blue = colorNormalise ((int) blue - c.blue);
    return *this;
}

unsigned int SMIL::AnimateColor::Channels::argb () {
    unsigned int v =
        (0xff000000 & ((unsigned)(alpha < 0 ? 0 : alpha) << 24)) |
        (0x00ff0000 & ((unsigned)(red < 0 ? 0 : red) << 16)) |
        (0x0000ff00 & ((unsigned)(green < 0 ? 0 : green) << 8)) |
        (0x000000ff & (blue < 0 ? 0 : blue));
    return v;
}

void SMIL::AnimateColor::Channels::clear () {
    alpha = red = blue = green = 0;
}

KDE_NO_EXPORT void SMIL::AnimateColor::init () {
    cur_c.clear ();
    delta_c.clear ();
    changed_attribute = "background-color";
    AnimateBase::init ();
}

KDE_NO_EXPORT void SMIL::AnimateColor::begin () {
    Element *target = static_cast <Element *> (targetElement ());
    if (!target)
        return;
    if (anim_timer) {
        document ()->cancelPosting (anim_timer);
        anim_timer = NULL;
    }
    if (change_from.isEmpty ()) {
        if (values.size () > 1) {
            getAnimateColor (values[0], begin_c);
            getAnimateColor (values[1], end_c);
        } else {
            getAnimateColor (target->param (changed_attribute), begin_c);
        }
    } else {
        getAnimateColor (change_from, begin_c);
    }
    if (!change_by.isEmpty ()) {
        getAnimateColor (change_by, delta_c);
        end_c = begin_c;
        end_c += delta_c;
    } else if (!change_to.isEmpty ()) {
        getAnimateColor (change_to, end_c);
    }
    cur_c = begin_c;
    delta_c = end_c;
    delta_c -= begin_c;
    AnimateBase::begin ();
}

KDE_NO_EXPORT void SMIL::AnimateColor::finish () {
    if (active ()) {
        if (calcMode != calc_discrete && cur_c.argb () != end_c.argb ()) {
            cur_c = end_c;
            applyStep (); // we lost some steps ..
        }
    }
    AnimateBase::finish ();
}

KDE_NO_EXPORT void SMIL::AnimateColor::applyStep () {
    Node *target = target_element.ptr ();
    if (target) {
        QString val; // TODO make more efficient
        val.sprintf ("#%06x", 0xffffff & cur_c.argb ());
        static_cast <Element *> (target)->setParam (changed_attribute, val);
    }
}

KDE_NO_EXPORT bool SMIL::AnimateColor::timerTick (unsigned int cur_time) {
    if (cur_time && cur_time <= interval_end_time) {
        float gain = 1.0 * (cur_time - interval_start_time) /
                           (interval_end_time - interval_start_time);
        if (gain > 1.0) {
            change_updater.disconnect ();
            gain = 1.0;
        }
        switch (calcMode) {
            case calc_paced: // FIXME
            case calc_linear:
                break;
            case calc_spline:
                if (spline_table)
                    gain = cubicBezier (spline_table, 0, 99, gain);
                break;
            case calc_discrete:
                return true; // shouldn't come here
        }
        cur_c = delta_c;
        cur_c *= gain;
        cur_c += begin_c;
        applyStep ();
        return true;
    } else if (values.size () > (int) ++interval) {
        getAnimateColor (values[interval], begin_c);
        cur_c = begin_c;
        if (calcMode != calc_discrete && values.size () > (int) interval + 1) {
            getAnimateColor (values[interval+1], end_c);
            delta_c = end_c;
            delta_c -= begin_c;
        }
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
    Node * parent = parentNode ();
    if (!name.isEmpty () && parent && parent->isElementNode ())
        static_cast<Element*>(parent)->setParam (name,
                getAttribute (StringPool::attr_value));
    Element::activate (); //finish (); // no livetime of itself, will deactivate
}

//-----------------------------------------------------------------------------

KDE_NO_EXPORT void Visitor::visit (SMIL::Layout *n) {
    visit (static_cast <Element *> (n));
}

KDE_NO_EXPORT void Visitor::visit (SMIL::RegionBase *n) {
    visit (static_cast <Element *> (n));
}

KDE_NO_EXPORT void Visitor::visit (SMIL::Seq *n) {
    visit (static_cast <SMIL::GroupBase *> (n));
}

KDE_NO_EXPORT void Visitor::visit (SMIL::Switch *n) {
    visit (static_cast <SMIL::GroupBase *> (n));
}

KDE_NO_EXPORT void Visitor::visit (SMIL::Par *n) {
    visit (static_cast <SMIL::GroupBase *> (n));
}

KDE_NO_EXPORT void Visitor::visit (SMIL::Excl *n) {
    visit (static_cast <SMIL::GroupBase *> (n));
}

KDE_NO_EXPORT void Visitor::visit (SMIL::Transition * n) {
    visit (static_cast <Element *> (n));
}

KDE_NO_EXPORT void Visitor::visit (SMIL::AnimateBase * n) {
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

KDE_NO_EXPORT void Visitor::visit (SMIL::Brush * n) {
    visit (static_cast <SMIL::MediaType *> (n));
}

KDE_NO_EXPORT void Visitor::visit (SMIL::SmilText *n) {
    visit (static_cast <Element *> (n));
}

KDE_NO_EXPORT void Visitor::visit (SMIL::TextFlow *n) {
    visit (static_cast <Element *> (n));
}

KDE_NO_EXPORT void Visitor::visit (SMIL::Anchor * n) {
    visit (static_cast <SMIL::LinkingBase *> (n));
}

KDE_NO_EXPORT void Visitor::visit (SMIL::Area * n) {
    visit (static_cast <SMIL::LinkingBase *> (n));
}
