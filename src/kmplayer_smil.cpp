/**
 * Copyright (C) 2005 by Koos Vriezen <koos ! vriezen ? xs4all ! nl>
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
 *  the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 *  Boston, MA 02111-1307, USA.
 **/

#include <qtextstream.h>
#include <qcolor.h>
#include <qpainter.h>
#include <qpixmap.h>
#include <qtextcodec.h>
#include <qfont.h>
#include <qfontmetrics.h>
#include <qfile.h>
#include <qapplication.h>
#include <qregexp.h>
#include <qtimer.h>

#include <kdebug.h>
#include <kurl.h>
#include <kio/job.h>
#include <kio/jobclasses.h>

#include "kmplayer_smil.h"

using namespace KMPlayer;

static const unsigned int duration_infinite = (unsigned int) -1;
static const unsigned int duration_media = (unsigned int) -2;
static const unsigned int duration_element_activated = (unsigned int) -3;
static const unsigned int duration_element_inbounds = (unsigned int) -4;
static const unsigned int duration_element_outbounds = (unsigned int) -5;
static const unsigned int duration_element_stopped = (unsigned int) -6;
static const unsigned int duration_last_option = (unsigned int) -7;

//-----------------------------------------------------------------------------

static RegionNodePtr findRegion (RegionNodePtr p, const QString & id) {
    for (RegionNodePtr r = p->firstChild; r; r = r->nextSibling) {
        if (r->regionElement->getAttribute ("id") == id) {
            kdDebug () << "MediaType region found " << id << endl;
            return r;
        }
        RegionNodePtr r1 = findRegion (r, id);
        if (r1)
            return r1;
    }
    return RegionNodePtr ();
}

//-----------------------------------------------------------------------------

ElementRuntimePtr Element::getRuntime () {
    return ElementRuntimePtr ();
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT RegionNode::RegionNode (ElementPtr e)
 : has_mouse (false), x (0), y (0), w (0), h (0), z_order (1), regionElement (e) {
    self = RegionNodePtrW (this);
    ElementRuntimePtr rt = e->getRuntime ();
    if (rt)
        rt->region_node = self;
}

KDE_NO_CDTOR_EXPORT void RegionNode::clearAll () {
    kdDebug () << "RegionNode::clearAll " << endl;
    attached_element = ElementPtr ();
    for (RegionNodePtr r = firstChild; r; r = r->nextSibling)
        r->clearAll ();
}

KDE_NO_EXPORT void RegionNode::paint (QPainter & p) {
    if (regionElement) {
        ElementRuntimePtr rt = regionElement->getRuntime ();
        if (rt)
            rt->paint (p);
    }
    if (attached_element) {
        ElementRuntimePtr rt = attached_element->getRuntime ();
        if (rt)
            rt->paint (p);
    }
    // now paint children, accounting for z-order
    int done_index = -1;
    do {
        int cur_index = 1 << 8 * sizeof (int) - 2;  // should be enough :-)
        int check_index = cur_index;
        for (RegionNodePtr r = firstChild; r; r = r->nextSibling) {
            if (r->z_order > done_index && r->z_order < cur_index)
                cur_index = r->z_order;
        }
        if (check_index == cur_index)
            break;
        for (RegionNodePtr r = firstChild; r; r = r->nextSibling)
            if (r->z_order == cur_index) {
                kdDebug () << "Painting " << cur_index << endl;
                r->paint (p);
            }
        done_index = cur_index;
    } while (true);
}

KDE_NO_EXPORT void RegionNode::repaint () {
    if (regionElement) {
        PlayListNotify * n = regionElement->document()->notify_listener;
        if (n)
            n->repaintRegion (this);
    }
};

KDE_NO_EXPORT bool RegionNode::pointerClicked (int _x, int _y) {
    bool inside = _x > x && _x < x + w && _y > y && _y < y + h;
    if (!inside)
        return false;
    bool handled = false;
    for (RegionNodePtr r = firstChild; r; r = r->nextSibling)
        handled |= r->pointerClicked (_x, _y);
    if (!handled) { // handle it ..
        if (attached_element) {
            kdDebug () << "activateEvent " << attached_element->nodeName () << endl;
            ElementRuntimePtr rt = attached_element->getRuntime ();
            if (rt)
                static_cast <TimedRuntime *> (rt.ptr ())->emitActivateEvent ();
        }
    }
    return inside;
}

KDE_NO_EXPORT bool RegionNode::pointerMoved (int _x, int _y) {
    bool inside = _x > x && _x < x + w && _y > y && _y < y + h;
    bool handled = false;
    if (inside)
        for (RegionNodePtr r = firstChild; r; r = r->nextSibling)
            handled |= r->pointerMoved (_x, _y);
    if (has_mouse && (!inside || handled)) { // OutOfBoundsEvent
        has_mouse = false;
        if (attached_element) {
            kdDebug () << "pointerLeft " << attached_element->nodeName () << endl;
            ElementRuntimePtr rt = attached_element->getRuntime ();
            if (rt)
                static_cast <TimedRuntime *> (rt.ptr ())->emitOutOfBoundsEvent ();
        }
    } else if (inside && !handled && !has_mouse) { // InBoundsEvent
        has_mouse = true;
        if (attached_element) {
            kdDebug () << "pointerEntered " << attached_element->nodeName () << endl;
            ElementRuntimePtr rt = attached_element->getRuntime ();
            if (rt)
                static_cast <TimedRuntime *> (rt.ptr ())->emitInBoundsEvent ();
        }
    }
    return inside;
}

//-----------------------------------------------------------------------------

ElementRuntime::ElementRuntime (ElementPtr e)
  : element (e) {}

ElementRuntime::~ElementRuntime () {}

QString ElementRuntime::setParam (const QString & name, const QString & value) {
    return QString::null;
}
//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT
TimedRuntime::TimedRuntime (ElementPtr e)
 : QObject (0L), ElementRuntime (e) {
    init ();
}

KDE_NO_CDTOR_EXPORT TimedRuntime::~TimedRuntime () {}

KDE_NO_EXPORT void TimedRuntime::init () {
    start_timer = 0;
    dur_timer = 0;
    repeat_count = 0;
    state = state_reset;
    durations [begin_time].durval = durations [end_time].durval = 0;
    durations [duration_time].durval = duration_media; //intrinsic time duration
}

KDE_NO_EXPORT
void TimedRuntime::setDurationItem (DurationTime item, const QString & val) {
    unsigned int dur = 0; // also 0 for 'media' duration, so it will not update then
    QRegExp reg ("^\\s*([0-9\\.]+)\\s*([a-z]*)");
    QString vl = val.lower ();
    kdDebug () << "getSeconds " << val << endl;
    if (reg.search (vl) > -1) {
        bool ok;
        double t = reg.cap (1).toDouble (&ok);
        if (ok && t > 0.000) {
            kdDebug() << "reg.cap (1) " << t << (ok && t > 0.000) << endl;
            QString u = reg.cap (2);
            if (u.startsWith ("m"))
                dur = (unsigned int) (t * 60);
            else if (u.startsWith ("h"))
                dur = (unsigned int) (t * 60 * 60);
            dur = (unsigned int) t;
        }
    } else if (vl.find ("indefinite") > -1)
        dur = duration_infinite;
    else if (vl.find ("media") > -1)
        dur = duration_media;
    if (!dur && element) {
        int pos = vl.find (QChar ('.'));
        if (pos > 0) {
            ElementPtr e = element->document()->getElementById (vl.left(pos));
            if (e) {
                kdDebug () << "getElementById " << vl.left (pos) << " " << e->nodeName () << endl;
                ElementRuntimePtr rt = e->getRuntime ();
                if (rt) {
                    TimedRuntime * tr = static_cast <TimedRuntime*> (rt.ptr ());
                    if (vl.find ("activateevent") > -1) {
                        dur = duration_element_activated;
                        connect (tr, SIGNAL (activateEvent ()),
                                 this, SLOT (elementActivateEvent ()));
                    } else if (vl.find ("inboundsevent") > -1) {
                        dur = duration_element_inbounds;
                        connect (tr, SIGNAL (inBoundsEvent ()),
                                 this, SLOT (elementInBoundsEvent ()));
                    } else if (vl.find ("outofboundsevent") > -1) {
                        dur = duration_element_outbounds;
                        connect (tr, SIGNAL (outOfBoundsEvent ()),
                                 this, SLOT (elementOutOfBoundsEvent ()));
                    }
                    breakConnection (item);
                    durations [(int) item].connection = rt;
                }
            }
        }
    }
    durations [(int) item].durval = dur;
}

KDE_NO_EXPORT void TimedRuntime::begin () {
    if (!element) {
        end ();
        return;
    }
    kdDebug () << "TimedRuntime::begin " << element->nodeName() << endl; 
    if (start_timer || dur_timer)
        end ();
    state = state_began;

    if (durations [begin_time].durval > 0) {
        if (durations [begin_time].durval < duration_last_option)
            start_timer = startTimer (1000 * durations [begin_time].durval);
    } else {
        state = state_started;
        QTimer::singleShot (0, this, SLOT (started ()));
    }
}

KDE_NO_EXPORT void TimedRuntime::breakConnection (DurationTime item) {
    TimedRuntime * tr = dynamic_cast <TimedRuntime *> (durations [(int) item].connection.ptr ());
    if (tr) {
        switch (durations [(int) item].durval) {
            case duration_element_stopped:
                disconnect (tr, SIGNAL (elementStopped ()),
                            this, SLOT (elementHasStopped ()));
                break;
            case duration_element_activated:
                disconnect (tr, SIGNAL (activateEvent ()),
                            this, SLOT (elementActivateEvent ()));
                break;
            case duration_element_inbounds:
                disconnect (tr, SIGNAL (inBoundsEvent ()),
                            this, SLOT (elementInBoundsEvent ()));
                break;
            case duration_element_outbounds:
                disconnect (tr, SIGNAL (outOfBoundsEvent ()),
                            this, SLOT (elementOutOfBoundsEvent ()));
                break;
            default:
                kdWarning () << "Unknown connection, disconnecting all" << endl;
                disconnect (tr, 0, this, 0);
        }
        durations [(int) item].connection = ElementRuntimePtr ();
        durations [(int) item].durval = 0;
    }
}

KDE_NO_EXPORT void TimedRuntime::end () {
    kdDebug () << "TimedRuntime::end " << (element ? element->nodeName() : "-") << endl; 
    if (region_node) {
        region_node->clearAll ();
        region_node = RegionNodePtr ();
    }
    for (int i = 0; i < (int) durtime_last; i++)
        breakConnection ((DurationTime) i);
    killTimers ();
    init ();
}

KDE_NO_EXPORT
QString TimedRuntime::setParam (const QString & name, const QString & val) {
    kdDebug () << "TimedRuntime::setParam " << name << "=" << val << endl;
    QString old_val;
    if (name == QString::fromLatin1 ("begin")) {
        old_val = QString::number (durations [begin_time].durval);
        setDurationItem (begin_time, val);
        if ((state == state_began && !start_timer) || state == state_stopped) {
            if (durations [begin_time].durval > 0) { // create a timer for start
                if (durations [begin_time].durval < duration_last_option)
                    start_timer = startTimer(1000*durations[begin_time].durval);
            } else {                                // start now
                state = state_started;
                QTimer::singleShot (0, this, SLOT (started ()));
            }
        }
    } else if (name == QString::fromLatin1 ("dur")) {
        old_val = QString::number (durations [duration_time].durval);
        setDurationItem (duration_time, val);
    } else if (name == QString::fromLatin1 ("end")) {
        old_val = QString::number (durations [end_time].durval);
        setDurationItem (end_time, val);
        if (durations [end_time].durval < duration_last_option &&
            durations [end_time].durval > durations [begin_time].durval)
            durations [duration_time].durval =
                durations [end_time].durval - durations [begin_time].durval;
    } else if (name == QString::fromLatin1 ("endsync")) {
        // TODO: old_val
        if (durations [duration_time].durval == duration_media &&
                !durations [end_time].durval) {
            ElementPtr e = element->document ()->getElementById (val);
            if (e) {
                ElementRuntimePtr rt = e->getRuntime ();
                TimedRuntime * tr = dynamic_cast <TimedRuntime *> (rt.ptr ());
                if (tr) {
                    breakConnection (end_time);
                    connect (tr, SIGNAL (elementStopped ()),
                             this, SLOT (elementHasStopped ()));
                    durations [end_time].durval = duration_element_stopped;
                    durations [end_time].connection = rt;
                }
            }
        }
    } else if (name == QString::fromLatin1 ("repeatCount")) {
        old_val = QString::number (repeat_count);
        repeat_count = val.toInt ();
    } else
        return ElementRuntime::setParam (name, val);
    return old_val;
}

KDE_NO_EXPORT void TimedRuntime::timerEvent (QTimerEvent * e) {
    kdDebug () << "TimedRuntime::timerEvent " << (element ? element->nodeName() : "-") << endl; 
    if (e->timerId () == start_timer) {
        killTimer (start_timer);
        start_timer = 0;
        state = state_started;
        QTimer::singleShot (0, this, SLOT (started ()));
    } else if (e->timerId () == dur_timer)
        propagateStop ();
}

KDE_NO_EXPORT void TimedRuntime::elementActivateEvent () {
    processEvent (duration_element_activated);
}

KDE_NO_EXPORT void TimedRuntime::elementInBoundsEvent () {
    processEvent (duration_element_inbounds);
}

KDE_NO_EXPORT void TimedRuntime::elementOutOfBoundsEvent () {
    processEvent (duration_element_outbounds);
}

KDE_NO_EXPORT void TimedRuntime::elementHasStopped () {
    processEvent (duration_element_stopped);
}

KDE_NO_EXPORT void TimedRuntime::processEvent (unsigned int event) {
    kdDebug () << "TimedRuntime::processEvent " << event << " " << (element ? element->nodeName() : "-") << endl; 
    if (state != state_started && durations [begin_time].durval == event) {
        if (state != state_started) {
            state = state_started;
            QTimer::singleShot (0, this, SLOT (started ()));
        }
    } else if (state == state_started && durations [end_time].durval == event)
        propagateStop ();
}

KDE_NO_EXPORT void TimedRuntime::propagateStop () {
    if (dur_timer) {
        killTimer (dur_timer);
        dur_timer = 0;
    }
    if (state == state_started)
        QTimer::singleShot (0, this, SLOT (stopped ()));
    state = state_stopped;
}

KDE_NO_EXPORT void TimedRuntime::started () {
    kdDebug () << "TimedRuntime::started " << (element ? element->nodeName() : "-") << " dur:" << durations [duration_time].durval << endl; 
    if (durations [duration_time].durval > 0) {
        if (durations [duration_time].durval < duration_last_option)
            dur_timer = startTimer (1000 * durations [duration_time].durval);
    } else if (!element || durations [end_time].durval < duration_last_option)
        // no duration set and no special end, so mark us finished
        propagateStop ();
}

KDE_NO_EXPORT void TimedRuntime::stopped () {
    if (!element)
        end ();
    else if (0 < repeat_count--) {
        if (durations [begin_time].durval > 0 &&
                durations [begin_time].durval < duration_last_option) {
            start_timer = startTimer (1000 * durations [begin_time].durval);
        } else {
            state = state_started;
            QTimer::singleShot (0, this, SLOT (started ()));
        }
    } else if (element->state == Element::state_started) {
        element->stop ();
        emit elementStopped ();
    }
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT RegionRuntime::RegionRuntime (ElementPtr e)
 : ElementRuntime (e), background_color (0), have_bg_color (false) {}

KDE_NO_EXPORT void RegionRuntime::paint (QPainter & p) {
    if (have_bg_color && region_node) {
        RegionNode * rn = region_node.ptr ();
        p.fillRect (rn->x, rn->y, rn->w, rn->h, QColor(QRgb(background_color)));
    }
}

KDE_NO_EXPORT
QString RegionRuntime::setParam (const QString & name, const QString & val) {
    kdDebug () << "RegionRuntime::setParam " << name << "=" << val << endl;
    QString old_val;
    if (name == QString::fromLatin1 ("background-color") ||
            name == QString::fromLatin1 ("background-color")) {
        if (have_bg_color)
            old_val = QColor(QRgb(background_color)).name ();
        background_color = QColor (val).rgb ();
        have_bg_color = true;
    } else if (name == QString::fromLatin1 ("z-index")) {
        if (region_node) {
            old_val = QString::number (region_node->z_order);
            region_node->z_order = val.toInt ();
        }
    } else
        return ElementRuntime::setParam (name, val);
    return old_val;
}

KDE_NO_EXPORT void RegionRuntime::begin () {
    if (element)
        for (ElementPtr a=element->attributes().item(0); a;a=a->nextSibling()) {
            Attribute * att = convertNode <Attribute> (a);
            setParam (QString (att->nodeName ()), att->nodeValue ());
        }
    ElementRuntime::begin ();
}

KDE_NO_EXPORT void RegionRuntime::end () {
    have_bg_color = false;
    ElementRuntime::end ();
}

//-----------------------------------------------------------------------------

KDE_NO_EXPORT void SetData::started () {
    kdDebug () << "SetData::started " << durations [duration_time].durval << endl;
    if (durations [duration_time].durval == duration_media)
        durations [duration_time].durval = 0; // intrinsic duration of 0
    if (element) {
        QString elm_id = element->getAttribute ("targetElement");
        target_element = element->document ()->getElementById (elm_id);
        changed_attribute = element->getAttribute ("attributeName");
        QString to = element->getAttribute ("to");
        if (target_element) {
            ElementRuntimePtr rt = target_element->getRuntime ();
            if (rt) {
                target_region = rt->region_node;
                old_value = rt->setParam (changed_attribute, to);
                kdDebug () << "SetData::started " << target_element->nodeName () << "." << changed_attribute << " " << old_value << "->" << to << endl;
                if (target_region)
                    target_region->repaint ();
            }
        } else
            kdWarning () << "target element not found" << endl;
    } else
        kdWarning () << "set element disappeared" << endl;
    TimedRuntime::started ();
}

KDE_NO_EXPORT void SetData::stopped () {
    kdDebug () << "SetData::stopped " << durations [duration_time].durval << endl;
    if (target_element) {
        ElementRuntimePtr rt = target_element->getRuntime ();
        if (rt) {
            old_value = rt->setParam (changed_attribute, old_value);
            if (target_region)
                target_region->repaint ();
        }
    } else
        kdWarning () << "target element not found" << endl;
    TimedRuntime::stopped ();
}

//-----------------------------------------------------------------------------

namespace KMPlayer {
    class MediaTypeRuntimePrivate {
    public:
        KDE_NO_CDTOR_EXPORT MediaTypeRuntimePrivate ()
            : job (0L), fill (fill_unknown) {}
        KDE_NO_CDTOR_EXPORT ~MediaTypeRuntimePrivate () {
            delete job;
        }
        KIO::Job * job;
        QByteArray data;
        enum { fill_unknown, fill_freeze } fill;
    };
}

KDE_NO_CDTOR_EXPORT MediaTypeRuntime::MediaTypeRuntime (ElementPtr e)
 : TimedRuntime (e), mt_d (new MediaTypeRuntimePrivate) {}

KDE_NO_CDTOR_EXPORT MediaTypeRuntime::~MediaTypeRuntime () {
    killWGet ();
    delete mt_d;
}

KDE_NO_EXPORT void MediaTypeRuntime::killWGet () {
    if (mt_d->job) {
        mt_d->job->kill (); // quiet, no result signal
        mt_d->job = 0L;
    }
}

KDE_NO_EXPORT bool MediaTypeRuntime::wget (const KURL & url) {
    killWGet ();
    mt_d->job = KIO::get (url, false, false);
    connect (mt_d->job, SIGNAL (data (KIO::Job *, const QByteArray &)),
             this, SLOT (slotData (KIO::Job *, const QByteArray &)));
    connect (mt_d->job, SIGNAL (result (KIO::Job *)),
             this, SLOT (slotResult (KIO::Job *)));
    return true;
}

KDE_NO_EXPORT void MediaTypeRuntime::slotResult (KIO::Job * job) {
    if (job->error ())
        mt_d->data.resize (0);
    mt_d->job = 0L; // signal KIO::Job::result deletes itself
}

KDE_NO_EXPORT void MediaTypeRuntime::slotData (KIO::Job*, const QByteArray& qb) {
    if (qb.size ()) {
        int old_size = mt_d->data.size ();
        mt_d->data.resize (old_size + qb.size ());
        memcpy (mt_d->data.data () + old_size, qb.data (), qb.size ());
    }
}

KDE_NO_EXPORT void KMPlayer::MediaTypeRuntime::end () {
    if (mt_d->job) {
        mt_d->job->kill (); // quiet, no result signal
        mt_d->job = 0L; // KIO::Job::kill deletes itself
    }
    TimedRuntime::end ();
}

KDE_NO_EXPORT
QString MediaTypeRuntime::setParam (const QString & name, const QString & val) {
    if (name == QString::fromLatin1 ("region")) {
        // setup region ..
        if (element && !val.isEmpty ()) {
            RegionNodePtr root = element->document ()->rootLayout;
            if (root) {
                region_node = findRegion (root, val);
                if (region_node) {
                    region_node->clearAll ();
                    region_node->attached_element = element;
                } else
                    kdWarning() << "region " << val << " not found" << endl;
            }
        }
    } else if (name == QString::fromLatin1 ("fill")) {
        if (val == QString::fromLatin1 ("freeze"))
            mt_d->fill = MediaTypeRuntimePrivate::fill_freeze;
            // else all other fill options ..
    } else
        return TimedRuntime::setParam (name, val);
    return QString::null;
}

KDE_NO_EXPORT void MediaTypeRuntime::started () {
    if (region_node)
        region_node->repaint ();
    TimedRuntime::started ();
}

KDE_NO_EXPORT void MediaTypeRuntime::stopped () {
    if (region_node)
        region_node->repaint ();
    TimedRuntime::stopped ();
}

KDE_NO_CDTOR_EXPORT AudioVideoData::AudioVideoData (ElementPtr e)
    : MediaTypeRuntime (e) {}

KDE_NO_EXPORT bool AudioVideoData::isAudioVideo () {
    return state == state_started;
}

KDE_NO_EXPORT
QString AudioVideoData::setParam (const QString & name, const QString & val) {
    kdDebug () << "AudioVideoData::setParam " << name << "=" << val << endl;
    if (name == QString::fromLatin1 ("src")) {
        if (element) {
            PlayListNotify * n = element->document ()->notify_listener;
            if (n && !val.isEmpty ())
                n->requestPlayURL (element, region_node);
        }
    } else
        return MediaTypeRuntime::setParam (name, val);
    return QString::null;
}
//-----------------------------------------------------------------------------

static Element * fromScheduleGroup (ElementPtr & d, const QString & tag) {
    if (!strcmp (tag.latin1 (), "par"))
        return new SMIL::Par (d);
    else if (!strcmp (tag.latin1 (), "seq"))
        return new SMIL::Seq (d);
    // else if (!strcmp (tag.latin1 (), "excl"))
    //    return new Seq (d, p);
    return 0L;
}

static Element * fromParamGroup (ElementPtr & d, const QString & tag) {
    if (!strcmp (tag.latin1 (), "param"))
        return new SMIL::Param (d);
    return 0L;
}

static Element * fromAnimateGroup (ElementPtr & d, const QString & tag) {
    if (!strcmp (tag.latin1 (), "set"))
        return new SMIL::Set (d);
    return 0L;
}

static Element * fromMediaContentGroup (ElementPtr & d, const QString & tag) {
    const char * taglatin = tag.latin1 ();
    if (!strcmp (taglatin, "video") || !strcmp (taglatin, "audio"))
        return new SMIL::AVMediaType (d, tag);
    else if (!strcmp (taglatin, "img"))
        return new SMIL::ImageMediaType (d);
    else if (!strcmp (taglatin, "text"))
        return new SMIL::TextMediaType (d);
    // animation, textstream, ref, brush
    return 0L;
}

static Element * fromContentControlGroup (ElementPtr & d, const QString & tag) {
    if (!strcmp (tag.latin1 (), "switch"))
        return new SMIL::Switch (d);
    return 0L;
}

//-----------------------------------------------------------------------------

KDE_NO_EXPORT ElementPtr Smil::childFromTag (const QString & tag) {
    if (!strcmp (tag.latin1 (), "body"))
        return (new SMIL::Body (m_doc))->self ();
    else if (!strcmp (tag.latin1 (), "head"))
        return (new SMIL::Head (m_doc))->self ();
    return ElementPtr ();
}

static void beginOrEndRegions (RegionNodePtr rn, bool b) {
    if (rn->regionElement) {
        ElementRuntimePtr rt = rn->regionElement->getRuntime ();
        if (rt) {
            if (b)
                rt->begin ();
            else
                rt->end ();
        }
    }
    for (RegionNodePtr c = rn->firstChild; c; c = c->nextSibling)
        beginOrEndRegions (c, b);
}

KDE_NO_EXPORT void Smil::start () {
    kdDebug () << "Smil::start" << endl;
    current_av_media_type = ElementPtr ();
    setState (state_started);
    if (document ()->rootLayout) {
        beginOrEndRegions (document ()->rootLayout, true);
        document ()->rootLayout->repaint ();
    }
    for (ElementPtr e = firstChild (); e; e = e->nextSibling ())
        if (!strcmp (e->nodeName (), "body")) {
            e->start ();
            return;
        }
    stop (); //source->emitEndOfPlayItems ();
}

KDE_NO_EXPORT void Smil::stop () {
    if (document ()->rootLayout) {
        beginOrEndRegions (document ()->rootLayout, false);
        document ()->rootLayout->repaint ();
    }
    Mrl::stop ();
}

KDE_NO_EXPORT ElementPtr Smil::realMrl () {
    return current_av_media_type;
}

KDE_NO_EXPORT bool Smil::isMrl () {
    return true;
}

KDE_NO_EXPORT void Smil::childDone (ElementPtr /*child*/) {
    kdDebug () << "SMIL::Smil::childDone" << endl;
    stop ();
}
//-----------------------------------------------------------------------------

KDE_NO_EXPORT ElementPtr SMIL::Head::childFromTag (const QString & tag) {
    if (!strcmp (tag.latin1 (), "layout"))
        return (new SMIL::Layout (m_doc))->self ();
    return ElementPtr ();
}

KDE_NO_EXPORT bool SMIL::Head::expose () {
    return false;
}

//-----------------------------------------------------------------------------

KDE_NO_EXPORT ElementPtr SMIL::Layout::childFromTag (const QString & tag) {
    if (!strcmp (tag.latin1 (), "root-layout"))
        return (new SMIL::RootLayout (m_doc))->self ();
    else if (!strcmp (tag.latin1 (), "region"))
        return (new SMIL::Region (m_doc))->self ();
    return ElementPtr ();
}

static int calcLength (const QString & strval, int full) {
    if (strval.isEmpty ())
        return 0;
    int p = strval.find (QChar ('%'));
    if (p > -1)
        return strval.left (p).toInt () * full / 100;
    return strval.toInt ();
}

static void buildRegionNodes (ElementPtr p, RegionNodePtr r) {
    RegionNodePtr region;
    RegionNodePtr last_region;
    for (ElementPtr e = p->firstChild (); e; e = e->nextSibling ())
        if (!strcmp (e->nodeName (), "region")) {
            if (region) {
                last_region->nextSibling = (new RegionNode (e))->self;
                last_region = last_region->nextSibling;
            } else {
                region = last_region = (new RegionNode (e))->self;
                r->firstChild = region;
            }
            buildRegionNodes (e, last_region);
        }
}

static void sizeRegionNodes (RegionNodePtr p) {
    RegionBase * rb = convertNode <RegionBase> (p->regionElement);
    for (RegionNodePtr rg = p->firstChild; rg; rg = rg->nextSibling) {
        SMIL::Region *smilregion = convertNode<SMIL::Region>(rg->regionElement);
        int l = calcLength (smilregion->getAttribute("left"), rb->w);
        int t = calcLength (smilregion->getAttribute ("top"), rb->h);
        int w = calcLength (smilregion->getAttribute ("width"), rb->w);
        int h = calcLength (smilregion->getAttribute ("height"), rb->h);
        int r = calcLength (smilregion->getAttribute ("right"), rb->w);
        int b = calcLength (smilregion->getAttribute ("bottom"), rb->h);
        smilregion->x = l;
        smilregion->y = t;
        smilregion->w = w > 0 ? w : rb->w - l - (r > 0 ? r : 0);
        smilregion->h = h > 0 ? h : rb->h - t - (b > 0 ? b : 0);
        sizeRegionNodes (rg);
    }
}

KDE_NO_EXPORT void SMIL::Layout::closed () {
    RegionNodePtr root;
    SMIL::RootLayout * smilroot = 0L;
    RegionNodePtr region;
    RegionNodePtr last_region;
    for (ElementPtr e = firstChild (); e; e = e->nextSibling ()) {
        const char * name = e->nodeName ();
        if (!strcmp (name, "root-layout")) {
            root = (new RegionNode (e))->self;
            if (region)
                root->firstChild = region;
            smilroot = convertNode <SMIL::RootLayout> (e);
        } else if (!strcmp (name, "region")) {
            if (region) {
                last_region->nextSibling = (new RegionNode (e))->self;
                last_region = last_region->nextSibling;
            } else {
                region = last_region = (new RegionNode (e))->self;
                if (root)
                    root->firstChild = region;
            }
            buildRegionNodes (e, last_region);
        }
    }
    if (!root || !region) {
        kdError () << "Layout w/o a root-layout w/ regions" << endl;
        return;
    }
    smilroot->x = smilroot->y = 0;
    smilroot->w = smilroot->getAttribute ("width").toInt ();
    smilroot->h = smilroot->getAttribute ("height").toInt ();
    if (smilroot->w <= 0 || smilroot->h <= 0) {
        kdError () << "Root layout not having valid dimensions" << endl;
        return;
    }
    sizeRegionNodes (root);
    rootLayout = root;
    document ()->rootLayout = root;
}

//-----------------------------------------------------------------------------

KDE_NO_EXPORT ElementRuntimePtr RegionBase::getRuntime () {
    if (!runtime)
        runtime = ElementRuntimePtr (new RegionRuntime (m_self));
    return runtime;
}

//-----------------------------------------------------------------------------

KDE_NO_EXPORT ElementPtr SMIL::Region::childFromTag (const QString & tag) {
    if (!strcmp (tag.latin1 (), "region"))
        return (new SMIL::Region (m_doc))->self ();
    return ElementPtr ();
}

//-----------------------------------------------------------------------------

KDE_NO_EXPORT void SMIL::TimedElement::start () {
    kdDebug () << "SMIL::TimedElement(" << nodeName() << ")::start" << endl;
    setState (state_started);
    ElementRuntimePtr rt = getRuntime ();
    if (rt) {
        for (ElementPtr a = attributes().item (0); a; a= a->nextSibling()) {
            Attribute * att = convertNode <Attribute> (a);
            rt->setParam (QString (att->nodeName ()), att->nodeValue ());
        }
        rt->begin ();
    }
}

KDE_NO_EXPORT void SMIL::TimedElement::stop () {
    Mrl::stop ();
}

KDE_NO_EXPORT void SMIL::TimedElement::reset () {
    kdDebug () << "SMIL::TimedElement::reset " << endl;
    Mrl::reset ();
    if (runtime)
        runtime->end ();
}

KDE_NO_EXPORT ElementRuntimePtr SMIL::TimedElement::getRuntime () {
    if (!runtime)
        runtime = getNewRuntime ();
    return runtime;
}

//-----------------------------------------------------------------------------

KDE_NO_EXPORT bool SMIL::GroupBase::isMrl () {
    return false;
}

KDE_NO_EXPORT ElementRuntimePtr SMIL::GroupBase::getNewRuntime () {
    return ElementRuntimePtr (new TimedRuntime (m_self));
}

//-----------------------------------------------------------------------------

// SMIL::Body was here

//-----------------------------------------------------------------------------

KDE_NO_EXPORT ElementPtr SMIL::Par::childFromTag (const QString & tag) {
    Element * elm = fromScheduleGroup (m_doc, tag);
    if (!elm) elm = fromMediaContentGroup (m_doc, tag);
    if (!elm) elm = fromContentControlGroup (m_doc, tag);
    if (!elm) elm = fromAnimateGroup (m_doc, tag);
    if (elm)
        return elm->self ();
    return ElementPtr ();
}

KDE_NO_EXPORT void SMIL::Par::start () {
    kdDebug () << "SMIL::Par::start" << endl;
    GroupBase::start ();
    if (firstChild ()) {
        for (ElementPtr e = firstChild (); e; e = e->nextSibling ())
            e->start ();
    } else
        stop (); // no children to run in parallel
}

KDE_NO_EXPORT void SMIL::Par::stop () {
    TimedElement::stop ();
    for (ElementPtr e = firstChild (); e; e = e->nextSibling ())
        // children are out of scope now, reset their ElementRuntime
        e->reset (); // will call stop() if necessary
}

KDE_NO_EXPORT void SMIL::Par::reset () {
    GroupBase::reset ();
    for (ElementPtr e = firstChild (); e; e = e->nextSibling ())
        e->reset ();
}

KDE_NO_EXPORT void SMIL::Par::childDone (ElementPtr) {
    if (state != state_finished) {
        kdDebug () << "SMIL::Par::childDone" << endl;
        for (ElementPtr e = firstChild (); e; e = e->nextSibling ()) {
            if (e->state != state_finished)
                return; // not all done
        }
        stop (); // we're done
    }
}

//-----------------------------------------------------------------------------

KDE_NO_EXPORT ElementPtr SMIL::Seq::childFromTag (const QString & tag) {
    Element * elm = fromScheduleGroup (m_doc, tag);
    if (!elm) elm = fromMediaContentGroup (m_doc, tag);
    if (!elm) elm = fromContentControlGroup (m_doc, tag);
    if (!elm) elm = fromAnimateGroup (m_doc, tag);
    if (elm)
        return elm->self ();
    return ElementPtr ();
}

KDE_NO_EXPORT void SMIL::Seq::start () {
    GroupBase::start ();
    Element::start ();
}

//-----------------------------------------------------------------------------

KDE_NO_EXPORT ElementPtr SMIL::Switch::childFromTag (const QString & tag) {
    Element * elm = fromContentControlGroup (m_doc, tag);
    if (!elm) elm = fromMediaContentGroup (m_doc, tag);
    if (elm)
        return elm->self ();
    return ElementPtr ();
}

KDE_NO_EXPORT void SMIL::Switch::start () {
    kdDebug () << "SMIL::Switch::start" << endl;
    setState (state_started);
    if (firstChild ())
        firstChild ()->start (); // start only the first for now FIXME: condition
    else
        stop ();
}

KDE_NO_EXPORT void SMIL::Switch::stop () {
    Element::stop ();
    for (ElementPtr e = firstChild (); e; e = e->nextSibling ())
        if (e->state == state_started) {
            e->stop ();
            break; // stop only the one running
        }
}

KDE_NO_EXPORT void SMIL::Switch::reset () {
    Element::reset ();
    for (ElementPtr e = firstChild (); e; e = e->nextSibling ()) {
        if (e->state != state_init)
            e->reset ();
    }
}

KDE_NO_EXPORT void SMIL::Switch::childDone (ElementPtr) {
    kdDebug () << "SMIL::Switch::childDone" << endl;
    stop (); // only one child can run
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT SMIL::MediaType::MediaType (ElementPtr &d, const QString &t)
    : TimedElement (d), m_type (t), bitrate (0) {}

KDE_NO_EXPORT ElementPtr SMIL::MediaType::childFromTag (const QString & tag) {
    Element * elm = fromContentControlGroup (m_doc, tag);
    if (!elm) elm = fromParamGroup (m_doc, tag);
    if (!elm) elm = fromAnimateGroup (m_doc, tag);
    if (elm)
        return elm->self ();
    return ElementPtr ();
}

KDE_NO_EXPORT void SMIL::MediaType::opened () {
    for (ElementPtr a = m_first_attribute; a; a = a->nextSibling ()) {
        const char * cname = a->nodeName ();
        kdDebug () << "MediaType attr:" << cname <<"="<< a->nodeValue() << endl;
        if (!strcmp (cname, "system-bitrate"))
            bitrate = a->nodeValue ().toInt ();
        else if (!strcmp (cname, "src"))
            src = a->nodeValue ();
        else if (!strcmp (cname, "type"))
            mimetype = a->nodeValue ();
    }
}

KDE_NO_EXPORT void SMIL::MediaType::start () {
    ElementRuntimePtr rt = getRuntime ();
    if (rt) {
        setState (state_started);
        in_start = false;
        if (firstChild ()) {
            in_start = true;
            firstChild ()->start ();
        }
        if (!in_start) { // all children finished
            rt->setParam (QString ("src"), src);
            TimedElement::start (); // sets all attributes and calls rt->begin()
        } // else this points to a playlist
    } else // should not happen
        Element::start ();
}

KDE_NO_EXPORT void SMIL::MediaType::stop () {
    if (in_start) // all children stopped while still in the start() proc
        in_start = false;
    else
        SMIL::TimedElement::stop ();
}
//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT
SMIL::AVMediaType::AVMediaType (ElementPtr & d, const QString & t)
 : SMIL::MediaType (d, t) {}

KDE_NO_EXPORT void SMIL::AVMediaType::start () {
    if (!isMrl ()) { // turned out this URL points to a playlist file
        Element::start ();
        return;
    }
    kdDebug () << "SMIL::AVMediaType::start" << endl;
    ElementPtr p = parentNode ();
    while (p && strcmp (p->nodeName (), "smil"))
        p = p->parentNode ();
    if (p) { // this works only because we can only play one at a time FIXME
        convertNode <Smil> (p)->current_av_media_type = m_self;
        MediaType::start ();
    } else {
        kdError () << nodeName () << " playing and current is not Smil" << endl;
        stop ();
    }
}

KDE_NO_EXPORT void SMIL::AVMediaType::stop () {
    TimedElement::stop ();
    TimedRuntime * tr = static_cast <TimedRuntime *> (getRuntime ().ptr ());
    if (tr && tr->isStarted ()) // are we called from backends
        tr->emitElementStopped ();
    // TODO stop backend player
}

KDE_NO_EXPORT ElementRuntimePtr SMIL::AVMediaType::getNewRuntime () {
    return ElementRuntimePtr (new AudioVideoData (m_self));
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT
SMIL::ImageMediaType::ImageMediaType (ElementPtr & d)
    : SMIL::MediaType (d, "img") {}

KDE_NO_EXPORT ElementRuntimePtr SMIL::ImageMediaType::getNewRuntime () {
    isMrl (); // hack to get relative paths right
    return ElementRuntimePtr (new ImageData (m_self));
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT
SMIL::TextMediaType::TextMediaType (ElementPtr & d)
    : SMIL::MediaType (d, "text") {}

KDE_NO_EXPORT ElementRuntimePtr SMIL::TextMediaType::getNewRuntime () {
    isMrl (); // hack to get relative paths right
    return ElementRuntimePtr (new TextData (m_self));
}

//-----------------------------------------------------------------------------

KDE_NO_EXPORT ElementRuntimePtr SMIL::Set::getRuntime () {
    if (!runtime) // bah code duplicate
        runtime = ElementRuntimePtr (new SetData (m_self));
    return runtime;
}

KDE_NO_EXPORT void SMIL::Set::start () {
    ElementRuntimePtr rt = getRuntime ();
    for (ElementPtr a = attributes ().item (0); a; a = a->nextSibling ()) {
        Attribute * att = convertNode <Attribute> (a);
        rt->setParam (QString (att->nodeName ()), att->nodeValue ());
    }
    rt->begin ();
    stop (); // no livetime of itself
}

KDE_NO_EXPORT void SMIL::Set::reset () {
    getRuntime ()->end ();
}

//-----------------------------------------------------------------------------

KDE_NO_EXPORT void SMIL::Param::start () {
    QString name = getAttribute ("name");
    if (!name.isEmpty () && parentNode ()) {
        ElementRuntimePtr rt = parentNode ()->getRuntime ();
        if (rt)
            rt->setParam (name, getAttribute ("value"));
    }
    stop (); // no livetime of itself
}

//-----------------------------------------------------------------------------

namespace KMPlayer {
    class ImageDataPrivate {
        public:
            ImageDataPrivate () : image (0L) {}
            ~ImageDataPrivate () {
                delete image;
            }
            QPixmap * image;
    };
}

KDE_NO_CDTOR_EXPORT ImageData::ImageData (ElementPtr e)
 : MediaTypeRuntime (e), d (new ImageDataPrivate) {
}

KDE_NO_CDTOR_EXPORT ImageData::~ImageData () {
    delete d;
}

KDE_NO_EXPORT
QString ImageData::setParam (const QString & name, const QString & val) {
    kdDebug () << "ImageData::param " << name << "=" << val << endl;
    if (name == QString::fromLatin1 ("src")) {
        killWGet ();
        if (!val.isEmpty ()) {
            KURL url (val);
            if (url.isLocalFile ()) {
                QPixmap *pix = new QPixmap (url.path ());
                if (pix->isNull ())
                    delete pix;
                else {
                    delete d->image;
                    d->image = pix;
                }
            } else
                wget (url);
        }
    } else
        return MediaTypeRuntime::setParam (name, val);
    return QString::null;
}

KDE_NO_EXPORT void ImageData::paint (QPainter & p) {
    if ((state == state_started || (state == state_stopped &&
                      mt_d->fill == MediaTypeRuntimePrivate::fill_freeze)) &&
            d->image && region_node) {
        RegionNode * r = region_node.ptr ();
        p.drawPixmap (QRect (r->x, r->y, r->w, r->h), *d->image);
    }
}

KDE_NO_EXPORT void ImageData::started () {
    if (durations [duration_time].durval == duration_media)
        durations [duration_time].durval = 0; // intrinsic duration of 0
    MediaTypeRuntime::started ();
}

KDE_NO_EXPORT void ImageData::slotResult (KIO::Job * job) {
    MediaTypeRuntime::slotResult (job);
    if (mt_d->data.size () && element) {
        QPixmap *pix = new QPixmap (mt_d->data);
        if (!pix->isNull ()) {
            d->image = pix;
            if (region_node &&
                (state == state_started || (state == state_stopped &&
                      mt_d->fill == MediaTypeRuntimePrivate::fill_freeze)))
                region_node->repaint ();
        } else
            delete pix;
    }
}

//-----------------------------------------------------------------------------

namespace KMPlayer {
    class TextDataPrivate {
    public:
        TextDataPrivate () {
            reset ();
        }
        void reset () {
            background_color  = 0xFFFFFF;
            foreground_color = 0;
            codec = 0L;
            font = QApplication::font ();
            transparent = false;
        }
        QByteArray data;
        unsigned int background_color;
        unsigned int foreground_color;
        QTextCodec * codec;
        QFont font;
        bool transparent;
    };
}

KDE_NO_CDTOR_EXPORT TextData::TextData (ElementPtr e)
 : MediaTypeRuntime (e), d (new TextDataPrivate) {
}

KDE_NO_CDTOR_EXPORT TextData::~TextData () {
    delete d;
}

KDE_NO_EXPORT void TextData::end () {
    d->reset ();
    MediaTypeRuntime::end ();
}

KDE_NO_EXPORT
QString TextData::setParam (const QString & name, const QString & val) {
    kdDebug () << "TextData::setParam " << name << "=" << val << endl;
    QString old_val;
    if (name == QString::fromLatin1 ("src")) {
        d->data.resize (0);
        killWGet ();
        if (!val.isEmpty ()) {
            KURL url (val);
            if (url.isLocalFile ()) {
                QFile file (url.path ());
                file.open (IO_ReadOnly);
                d->data = file.readAll ();
            } else
                wget (url);
        }
    } else if (name == QString::fromLatin1 ("backgroundColor")) {
        old_val = QColor (QRgb (d->background_color)).name ();
        d->background_color = QColor (val).rgb ();
    } else if (name == QString ("fontColor")) {
        old_val = QColor (QRgb (d->foreground_color)).name ();
        d->foreground_color = QColor (val).rgb ();
    } else if (name == QString ("charset")) {
        d->codec = QTextCodec::codecForName (val.ascii ());
    } else if (name == QString ("fontFace")) {
        ; //FIXME
    } else if (name == QString ("fontPtSize")) {
        old_val = QString::number (d->font.pointSize ());
        d->font.setPointSize (val.toInt ());
    } else if (name == QString ("fontSize")) {
        old_val = QString::number (0); // -1 * val.toInt () ??
        d->font.setPointSize (d->font.pointSize () + val.toInt ());
    // TODO: expandTabs fontBackgroundColor fontSize fontStyle fontWeight hAlig vAlign wordWrap
    } else
        return MediaTypeRuntime::setParam (name, val);
    return old_val;
}

KDE_NO_EXPORT void TextData::paint (QPainter & p) {
    if ((state == state_started || (state == state_stopped &&
                       mt_d->fill == MediaTypeRuntimePrivate::fill_freeze)) &&
            region_node) {
        int x = region_node->x;
        int y = region_node->y;
        if (!d->transparent)
            p.fillRect (x, y, region_node->w, region_node->h,
                    QColor (QRgb (d->background_color)));
        QFontMetrics metrics (d->font);
        QPainter::TextDirection direction = QApplication::reverseLayout () ?
            QPainter::RTL : QPainter::LTR;
        if (direction == QPainter::RTL)
            x += region_node->w;
        int yoff = metrics.lineSpacing ();
        p.setFont (d->font);
        p.setPen (QRgb (d->foreground_color));
        QTextStream text (d->data, IO_ReadOnly);
        if (d->codec)
            text.setCodec (d->codec);
        QString line = text.readLine (); // FIXME word wrap
        while (!line.isNull () && yoff < region_node->h) {
            p.drawText (x, y+yoff, line, region_node->w, direction);
            line = text.readLine ();
            yoff += metrics.lineSpacing ();
        }
    }
}

KDE_NO_EXPORT void TextData::started () {
    if (durations [duration_time].durval == duration_media)
        durations [duration_time].durval = 0; // intrinsic duration of 0
    MediaTypeRuntime::started ();
}

KDE_NO_EXPORT void TextData::slotResult (KIO::Job * job) {
    MediaTypeRuntime::slotResult (job);
    if (mt_d->data.size () && element) {
        d->data = mt_d->data;
        if (region_node && (state == state_started || (state == state_stopped &&
                         mt_d->fill == MediaTypeRuntimePrivate::fill_freeze)))
            region_node->repaint ();
    }
}

#include "kmplayer_smil.moc"
