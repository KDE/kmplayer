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

#include <config.h>
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
#include <qmap.h>

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
static const unsigned int duration_data_download = (unsigned int) -7;
static const unsigned int duration_last_option = (unsigned int) -8;

/* Intrinsic duration 
 *  duration_time   |    end_time    |
 *  =======================================================================
 *  duration_media  | duration_media | wait for external stop (audio/video)
 *       0          | duration_media | only wait for child elements
 */
//-----------------------------------------------------------------------------

static RegionNodePtr findRegion (RegionNodePtr p, const QString & id) {
    for (RegionNodePtr r = p->firstChild; r; r = r->nextSibling) {
        QString val = r->regionElement->getAttribute ("id");
        if ((val.isEmpty () && id.isEmpty ()) ||
                r->regionElement->getAttribute ("id") == id) {
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
 : has_mouse (false), x (0), y (0), w (0), h (0),
   xscale (0.0), yscale (0.0),
   z_order (1), regionElement (e) {
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
    repaint ();
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

KDE_NO_EXPORT void RegionNode::calculateChildBounds () {
    SMIL::Region * r = convertNode <SMIL::Region> (regionElement);
    for (RegionNodePtr rn = firstChild; rn; rn = rn->nextSibling) {
        SMIL::Region * cr = convertNode <SMIL::Region> (rn->regionElement);
        cr->calculateBounds (r->w, r->h);
        rn->calculateChildBounds ();
        if (xscale > 0.001)
            scaleRegion (xscale, yscale, x, y);
    }
}

KDE_NO_EXPORT bool RegionNode::pointerClicked (int _x, int _y) {
    bool inside = _x > x && _x < x + w && _y > y && _y < y + h;
    if (!inside)
        return false;
    bool handled = false;
    for (RegionNodePtr r = firstChild; r; r = r->nextSibling)
        handled |= r->pointerClicked (_x, _y);
    if (!handled) { // handle it ..
        if (attached_element) {
            ElementRuntimePtr rt = attached_element->getRuntime ();
            if (rt)
                static_cast <TimedRuntime *> (rt.ptr ())->emitActivateEvent ();
        }
        if (regionElement) {
            ElementRuntimePtr rt = regionElement->getRuntime ();
            if (rt)
                static_cast <RegionRuntime *> (rt.ptr ())->emitActivateEvent ();
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
            ElementRuntimePtr rt = attached_element->getRuntime ();
            if (rt)
                static_cast <TimedRuntime *> (rt.ptr())->emitOutOfBoundsEvent();
        }
        if (regionElement) {
            ElementRuntimePtr rt = regionElement->getRuntime ();
            if (rt)
                static_cast <RegionRuntime *>(rt.ptr())->emitOutOfBoundsEvent();
        }
    } else if (inside && !handled && !has_mouse) { // InBoundsEvent
        has_mouse = true;
        if (attached_element) {
            ElementRuntimePtr rt = attached_element->getRuntime ();
            if (rt)
                static_cast <TimedRuntime *> (rt.ptr ())->emitInBoundsEvent ();
        }
        if (regionElement) {
            ElementRuntimePtr rt = regionElement->getRuntime ();
            if (rt)
                static_cast <RegionRuntime *> (rt.ptr ())->emitInBoundsEvent ();
        }
    }
    return inside;
}

KDE_NO_EXPORT void RegionNode::setSize (int _x, int _y, int _w, int _h, bool keepaspect) {
    RegionBase * region = convertNode <RegionBase> (regionElement);
    if (region && region->w > 0 && region->h > 0) {
        xscale = 1.0 + 1.0 * (_w - region->w) / region->w;
        yscale = 1.0 + 1.0 * (_h - region->h) / region->h;
        if (keepaspect)
            if (xscale > yscale) {
                xscale = yscale;
                _x = (_w - int ((xscale - 1.0) * region->w + region->w)) / 2;
            } else {
                yscale = xscale;
                _y = (_h - int ((yscale - 1.0) * region->h + region->h)) / 2;
            }
        scaleRegion (xscale, yscale, _x, _y);
    }
}

KDE_NO_EXPORT
void RegionNode::scaleRegion (float sx, float sy, int xoff, int yoff) {
    RegionBase * smilregion = convertNode <RegionBase> (regionElement);
    if (smilregion) {  // note WeakPtr can be null
        x = xoff + int (sx * smilregion->x);
        y = yoff + int (sy * smilregion->y);
        w = int (sx * smilregion->w);
        h = int (sy * smilregion->h);
        if (attached_element) {
            // hack to get the one and only audio/video widget sizes
            const char * nn = attached_element->nodeName ();
            PlayListNotify * n = attached_element->document ()->notify_listener;
            if (n && !strcmp (nn, "video") || !strcmp (nn, "audio")) {
                ElementRuntimePtr rt = smilregion->getRuntime ();
                RegionRuntime *rr = static_cast <RegionRuntime*> (rt.ptr());
                if (rr)
                    n->avWidgetSizes (this, rr->have_bg_color ? &rr->background_color : 0L);
            }
        }
        //kdDebug () << "Region size " << x << "," << y << " " << w << "x" << h << endl;
    }
    for (RegionNodePtr r = firstChild; r; r =r->nextSibling)
        r->scaleRegion (sx, sy, x, y);
}

//-----------------------------------------------------------------------------
namespace KMPlayer {
    class ElementRuntimePrivate {
    public:
        QMap <QString, QString> params;
    };
}

ElementRuntime::ElementRuntime (ElementPtr e)
  : element (e), d (new ElementRuntimePrivate) {}

ElementRuntime::~ElementRuntime () {
    delete d;
}

QString ElementRuntime::setParam (const QString & name, const QString & value) {
    QString old_val = d->params [name];
    d->params.insert (name, value);
    return old_val;
}

QString ElementRuntime::param (const QString & name) {
    return d->params [name];
}

KDE_NO_EXPORT void ElementRuntime::init () {
    reset ();
    if (element) {
        for (ElementPtr a= element->attributes().item(0); a; a=a->nextSibling())
            setParam (QString (a->nodeName ()), a->nodeValue ());
    }
}

KDE_NO_EXPORT void ElementRuntime::reset () {
    region_node = RegionNodePtr ();
    d->params.clear ();
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT RegionSignalerRuntime::RegionSignalerRuntime (ElementPtr e)
    : QObject (0L), ElementRuntime (e) {}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT
TimedRuntime::TimedRuntime (ElementPtr e) : RegionSignalerRuntime (e) {
    reset ();
}

KDE_NO_CDTOR_EXPORT TimedRuntime::~TimedRuntime () {}

KDE_NO_EXPORT void TimedRuntime::reset () {
    start_timer = 0;
    dur_timer = 0;
    repeat_count = 0;
    timingstate = timings_reset;
    fill = fill_unknown;
    durations [begin_time].durval = 0;
    durations [duration_time].durval = durations [end_time].durval = duration_media; //intrinsic time duration
    ElementRuntime::reset ();
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
                RegionSignalerRuntime * rs = dynamic_cast <RegionSignalerRuntime*> (rt.ptr ());
                if (rs) {
                    if (vl.find ("activateevent") > -1) {
                        dur = duration_element_activated;
                        connect (rs, SIGNAL (activateEvent ()),
                                 this, SLOT (elementActivateEvent ()));
                    } else if (vl.find ("inboundsevent") > -1) {
                        dur = duration_element_inbounds;
                        connect (rs, SIGNAL (inBoundsEvent ()),
                                 this, SLOT (elementInBoundsEvent ()));
                    } else if (vl.find ("outofboundsevent") > -1) {
                        dur = duration_element_outbounds;
                        connect (rs, SIGNAL (outOfBoundsEvent ()),
                                 this, SLOT (elementOutOfBoundsEvent ()));
                    }
                    breakConnection (item);
                    durations [(int) item].connection = rt;
                } else
                    kdWarning () << "not a RegionSignaler" << endl;
            }
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
    kdDebug () << "TimedRuntime::begin " << element->nodeName() << endl; 
    if (start_timer || dur_timer) {
        end ();
        init ();
    }
    timingstate = timings_began;

    if (durations [begin_time].durval > 0) {
        if (durations [begin_time].durval < duration_last_option) {
            start_timer = startTimer (1000 * durations [begin_time].durval);
        }
    } else {
        timingstate = timings_started;
        QTimer::singleShot (0, this, SLOT (started ()));
    }
}

KDE_NO_EXPORT void TimedRuntime::breakConnection (DurationTime item) {
    TimedRuntime * tr = dynamic_cast <TimedRuntime *> (durations [(int) item].connection.ptr ());
    if (tr && durations [(int) item].connection) {
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

/**
 * forced killing of timers
 */
KDE_NO_EXPORT void TimedRuntime::end () {
    kdDebug () << "TimedRuntime::end " << (element ? element->nodeName() : "-") << endl; 
    if (region_node) {
        region_node->clearAll ();
        region_node = RegionNodePtr ();
    }
    for (int i = 0; i < (int) durtime_last; i++)
        breakConnection ((DurationTime) i);
    killTimers ();
    reset ();
}

/**
 * change behaviour of this runtime, returns old value
 */
KDE_NO_EXPORT
QString TimedRuntime::setParam (const QString & name, const QString & val) {
    kdDebug () << "TimedRuntime::setParam " << name << "=" << val << endl;
    if (name == QString::fromLatin1 ("begin")) {
        setDurationItem (begin_time, val);
        if ((timingstate == timings_began && !start_timer) ||
                timingstate == timings_stopped) {
            if (durations [begin_time].durval > 0) { // create a timer for start
                if (durations [begin_time].durval < duration_last_option)
                    start_timer = startTimer(1000*durations[begin_time].durval);
            } else {                                // start now
                timingstate = timings_started;
                QTimer::singleShot (0, this, SLOT (started ()));
            }
        }
    } else if (name == QString::fromLatin1 ("dur")) {
        setDurationItem (duration_time, val);
    } else if (name == QString::fromLatin1 ("end")) {
        setDurationItem (end_time, val);
        if (durations [end_time].durval < duration_last_option &&
            durations [end_time].durval > durations [begin_time].durval)
            durations [duration_time].durval =
                durations [end_time].durval - durations [begin_time].durval;
    } else if (name == QString::fromLatin1 ("endsync")) {
        if (durations [duration_time].durval == duration_media &&
                durations [end_time].durval == duration_media) {
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
    } else if (name == QString::fromLatin1 ("fill")) {
        if (val == QString::fromLatin1 ("freeze"))
            fill = fill_freeze;
        else
            fill = fill_unknown;
        // else all other fill options ..
    } else if (name == QString::fromLatin1 ("repeatCount")) {
        repeat_count = val.toInt ();
    }
    return ElementRuntime::setParam (name, val);
}

KDE_NO_EXPORT void TimedRuntime::timerEvent (QTimerEvent * e) {
    kdDebug () << "TimedRuntime::timerEvent " << (element ? element->nodeName() : "-") << endl; 
    if (e->timerId () == start_timer) {
        killTimer (start_timer);
        start_timer = 0;
        timingstate = timings_started;
        QTimer::singleShot (0, this, SLOT (started ()));
    } else if (e->timerId () == dur_timer)
        propagateStop (true);
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

/**
 * slot for elementStopped() signal
 */
KDE_NO_EXPORT void TimedRuntime::elementHasStopped () {
    processEvent (duration_element_stopped);
}

KDE_NO_EXPORT void TimedRuntime::processEvent (unsigned int event) {
    kdDebug () << "TimedRuntime::processEvent " << event << " " << (element ? element->nodeName() : "-") << endl; 
    if (timingstate != timings_started && durations [begin_time].durval == event) {
        if (timingstate != timings_started) {
            timingstate = timings_started;
            QTimer::singleShot (0, this, SLOT (started ()));
        }
    } else if (timingstate == timings_started && durations [end_time].durval == event)
        propagateStop (true);
}

KDE_NO_EXPORT void TimedRuntime::propagateStop (bool forced) {
    if (!forced && element) {
        if (durations [end_time].durval > duration_last_option &&
                durations [end_time].durval != duration_media)
            return; // wait for event
        // bail out if a child still running
        for (ElementPtr c = element->firstChild (); c; c = c->nextSibling ())
            if (c->state == Element::state_started)
                return; // a child still running
    }
    if (dur_timer) {
        killTimer (dur_timer);
        dur_timer = 0;
    }
    if (timingstate == timings_started)
        QTimer::singleShot (0, this, SLOT (stopped ()));
    timingstate = timings_stopped;
}

/**
 * start_timer timer expired
 */
KDE_NO_EXPORT void TimedRuntime::started () {
    kdDebug () << "TimedRuntime::started " << (element ? element->nodeName() : "-") << endl; 
    if (durations [duration_time].durval > 0) {
        if (durations [duration_time].durval < duration_last_option) {
            dur_timer = startTimer (1000 * durations [duration_time].durval);
            kdDebug () << "TimedRuntime::started set dur timer " << durations [duration_time].durval << endl;
        }
    } else if (!element ||
            (durations [end_time].durval == duration_media ||
             durations [end_time].durval < duration_last_option))
        // no duration set and no special end, so mark us finished
        propagateStop (false);
}

/**
 * duration_timer timer expired or no duration set after started
 */
KDE_NO_EXPORT void TimedRuntime::stopped () {
    if (!element)
        end ();
    else if (0 < repeat_count--) {
        if (durations [begin_time].durval > 0 &&
                durations [begin_time].durval < duration_last_option) {
            start_timer = startTimer (1000 * durations [begin_time].durval);
        } else {
            timingstate = timings_started;
            QTimer::singleShot (0, this, SLOT (started ()));
        }
    } else if (element->state == Element::state_started) {
        element->stop ();
        emit elementStopped ();
    }
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT RegionRuntime::RegionRuntime (ElementPtr e)
 : RegionSignalerRuntime (e) {
    init ();
}

KDE_NO_EXPORT void RegionRuntime::reset () {
    // Keep region_node, so no ElementRuntime::reset (); or set it back again
    have_bg_color = false;
    active = false;
    left.truncate (0);
    top.truncate (0);
    width.truncate (0);
    height.truncate (0);
    right.truncate (0);
    bottom.truncate (0);
}

KDE_NO_EXPORT void RegionRuntime::paint (QPainter & p) {
    if (have_bg_color && region_node) {
        RegionNode * rn = region_node.ptr ();
        p.fillRect (rn->x, rn->y, rn->w, rn->h, QColor(QRgb(background_color)));
    }
}

KDE_NO_EXPORT
QString RegionRuntime::setParam (const QString & name, const QString & val) {
    kdDebug () << "RegionRuntime::setParam " << name << "=" << val << endl;
    bool needs_bounds_calc = false;
    if (name == QString::fromLatin1 ("background-color") ||
            name == QString::fromLatin1 ("background-color")) {
        background_color = QColor (val).rgb ();
        have_bg_color = true;
    } else if (name == QString::fromLatin1 ("z-index")) {
        if (region_node)
            region_node->z_order = val.toInt ();
    } else if (name == QString::fromLatin1 ("left")) {
        left = val;
        needs_bounds_calc = true;
    } else if (name == QString::fromLatin1 ("top")) {
        top = val;
        needs_bounds_calc = true;
    } else if (name == QString::fromLatin1 ("width")) {
        width = val;
        needs_bounds_calc = true;
    } else if (name == QString::fromLatin1 ("height")) {
        height = val;
        needs_bounds_calc = true;
    } else if (name == QString::fromLatin1 ("right")) {
        right = val;
        needs_bounds_calc = true;
    } else if (name == QString::fromLatin1 ("bottom")) {
        bottom = val;
        needs_bounds_calc = true;
    }
    if (active && needs_bounds_calc && element) {
        RegionNodePtr rn = element->document ()->rootLayout;
        if (rn && rn->regionElement) {
            convertNode <RegionBase> (rn->regionElement)->updateLayout ();
            rn->repaint ();
        }
    }
    return ElementRuntime::setParam (name, val);
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

QString AnimateGroupData::setParam (const QString & name, const QString & val) {
    kdDebug () << "AnimateGroupData::setParam " << name << "=" << val << endl;
    if (name == QString::fromLatin1 ("targetElement")) {
        if (element)
            target_element = element->document ()->getElementById (val);
    } else if (name == QString::fromLatin1 ("attributeName")) {
        changed_attribute = val;
    } else if (name == QString::fromLatin1 ("to")) {
        change_to = val;
    } else
        return TimedRuntime::setParam (name, val);
    return ElementRuntime::setParam (name, val);
}

//-----------------------------------------------------------------------------

/**
 * start_timer timer expired, execute it
 */
KDE_NO_EXPORT void SetData::started () {
    kdDebug () << "SetData::started " << durations [duration_time].durval << endl;
    if (element) {
        if (target_element) {
            ElementRuntimePtr rt = target_element->getRuntime ();
            if (rt) {
                target_region = rt->region_node;
                old_value = rt->setParam (changed_attribute, change_to);
                kdDebug () << "SetData::started " << target_element->nodeName () << "." << changed_attribute << " " << old_value << "->" << change_to << endl;
                if (target_region)
                    target_region->repaint ();
            }
        } else
            kdWarning () << "target element not found" << endl;
    } else
        kdWarning () << "set element disappeared" << endl;
    AnimateGroupData::started ();
}

/**
 * animation finished
 */
KDE_NO_EXPORT void SetData::stopped () {
    kdDebug () << "SetData::stopped " << durations [duration_time].durval << endl;
    if (target_element) {
        ElementRuntimePtr rt = target_element->getRuntime ();
        if (rt) {
            QString ov = rt->setParam (changed_attribute, old_value);
            kdDebug () << "SetData::stopped " << target_element->nodeName () << "." << changed_attribute << " " << ov << "->" << change_to << endl;
            if (target_region)
                target_region->repaint ();
        }
    } else
        kdWarning () << "target element not found" << endl;
    AnimateGroupData::stopped ();
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT AnimateData::AnimateData (ElementPtr e)
 : AnimateGroupData (e), anim_timer (0) {
    reset ();
}

KDE_NO_EXPORT void AnimateData::reset () {
    AnimateGroupData::reset ();
    if (anim_timer) {
        killTimer (anim_timer);
        anim_timer = 0;
    }
    accumulate = acc_none;
    additive = add_replace;
    change_by = 0;
    calcMode = calc_discrete;
    change_from.truncate (0);
    change_values.truncate (0);
    steps = 0;
    change_delta = change_to_val = change_from_val = 0.0;
    change_from_unit.truncate (0);
}

QString AnimateData::setParam (const QString & name, const QString & val) {
    kdDebug () << "AnimateData::setParam " << name << "=" << val << endl;
    if (name == QString::fromLatin1 ("change_by")) {
        change_by = val.toInt ();
    } else if (name == QString::fromLatin1 ("from")) {
        change_from = val;
    } else
        return AnimateGroupData::setParam (name, val);
    return ElementRuntime::setParam (name, val);
}

/**
 * start_timer timer expired, execute it
 */
KDE_NO_EXPORT void AnimateData::started () {
    kdDebug () << "AnimateData::started " << durations [duration_time].durval << endl;
    if (element) {
        if (target_element) {
            ElementRuntimePtr rt = target_element->getRuntime ();
            if (rt) {
                target_region = rt->region_node;
                QRegExp reg ("^\\s*([0-9\\.]+)(\\s*[%a-z]*)?");
                if (change_from.isEmpty ())
                    change_from = rt->param (changed_attribute);
                if (!change_from.isEmpty ()) {
                    old_value = rt->setParam (changed_attribute, change_from);
                    if (reg.search (change_from) > -1) {
                        change_from_val = reg.cap (1).toDouble ();
                        change_from_unit = reg.cap (2);
                    }
                } else {
                    kdDebug () << "AnimateData::started no from found" << endl;
                    return;
                }
                if (!change_to.isEmpty () && reg.search (change_to) > -1) {
                    change_to_val = reg.cap (1).toDouble ();
                } else {
                    kdDebug () << "AnimateData::started no to found" << endl;
                    return;
                }
                steps = durations [duration_time].durval * 1000 / 100;
                if (steps > 0) {
                    anim_timer = startTimer (100); // 100 ms for now FIXME
                    change_delta = (change_to_val - change_from_val) / steps;
                    kdDebug () << "AnimateData::started " << target_element->nodeName () << "." << changed_attribute << " " << change_from_val << "->" << change_to_val << " in " << steps << " using:" << change_delta << " inc" << endl;
                    if (!change_from.isEmpty () && target_region)
                        target_region->repaint ();
                }
            }
        } else
            kdWarning () << "target element not found" << endl;
    } else
        kdWarning () << "set element disappeared" << endl;
    AnimateGroupData::started ();
}

/**
 * undo if necessary
 */
KDE_NO_EXPORT void AnimateData::stopped () {
    kdDebug () << "AnimateData::stopped " << element->state << endl;
    AnimateGroupData::stopped ();
}

/**
 * for animations
 */
KDE_NO_EXPORT void AnimateData::timerEvent (QTimerEvent * e) {
    if (e->timerId () == anim_timer) {
        if (steps-- > 0 && target_element && target_element->getRuntime ()) {
            ElementRuntimePtr rt = target_element->getRuntime ();
            change_from_val += change_delta;
            rt->setParam (changed_attribute, QString ("%1%2").arg (change_from_val).arg(change_from_unit));
        } else {
            killTimer (anim_timer);
            anim_timer = 0;
            propagateStop (true); // not sure, actually
        }
    } else
        AnimateGroupData::timerEvent (e);
}

//-----------------------------------------------------------------------------

namespace KMPlayer {
    class MediaTypeRuntimePrivate {
    public:
        KDE_NO_CDTOR_EXPORT MediaTypeRuntimePrivate ()
         : job (0L) {
            reset ();
        }
        KDE_NO_CDTOR_EXPORT ~MediaTypeRuntimePrivate () {
            delete job;
        }
        void reset () {
            if (job) {
                job->kill (); // quiet, no result signal
                job = 0L; // KIO::Job::kill deletes itself
            }
        }
        KIO::Job * job;
        QByteArray data;
    };
}

KDE_NO_CDTOR_EXPORT MediaTypeRuntime::MediaTypeRuntime (ElementPtr e)
 : TimedRuntime (e), mt_d (new MediaTypeRuntimePrivate) {}

KDE_NO_CDTOR_EXPORT MediaTypeRuntime::~MediaTypeRuntime () {
    killWGet ();
    delete mt_d;
}

/**
 * abort previous wget job
 */
KDE_NO_EXPORT void MediaTypeRuntime::killWGet () {
    if (mt_d->job) {
        mt_d->job->kill (); // quiet, no result signal
        mt_d->job = 0L;
    }
}

/**
 * Gets contents from url and puts it in mt_d->data
 */
KDE_NO_EXPORT bool MediaTypeRuntime::wget (const KURL & url) {
    killWGet ();
    kdDebug () << "MediaTypeRuntime::wget " << url.url () << endl;
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

/**
 * re-implement for pending KIO::Job operations
 */
KDE_NO_EXPORT void KMPlayer::MediaTypeRuntime::end () {
    mt_d->reset ();
    TimedRuntime::end ();
}

/**
 * re-implement for regions and src attributes
 */
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
    } else if (name == QString::fromLatin1 ("src")) {
        source_url = val;
        if (element) {
            QString url = convertNode <SMIL::MediaType> (element)->src;
            if (!url.isEmpty ())
                source_url = url;
        }
    } else
        return TimedRuntime::setParam (name, val);
    return ElementRuntime::setParam (name, val);
}

/**
 * find region node and request a repaint of attached region then
 */
KDE_NO_EXPORT void MediaTypeRuntime::started () {
    if (!region_node && element && element->document ()->rootLayout) {
        region_node = findRegion(element->document()->rootLayout,QString::null);
        if (region_node)
            region_node->attached_element = element;
    }
    if (region_node)
        region_node->repaint ();
    TimedRuntime::started ();
}

/**
 * will request a repaint of attached region
 */
KDE_NO_EXPORT void MediaTypeRuntime::stopped () {
    if (region_node)
        region_node->repaint ();
    TimedRuntime::stopped ();
}

KDE_NO_CDTOR_EXPORT AudioVideoData::AudioVideoData (ElementPtr e)
    : MediaTypeRuntime (e) {}

KDE_NO_EXPORT bool AudioVideoData::isAudioVideo () {
    return timingstate == timings_started;
}

KDE_NO_EXPORT
QString AudioVideoData::setParam (const QString & name, const QString & val) {
    kdDebug () << "AudioVideoData::setParam " << name << "=" << val << endl;
    if (name == QString::fromLatin1 ("src")) {
        QString old_val = MediaTypeRuntime::setParam (name, val);
        if (element) {
            PlayListNotify * n = element->document ()->notify_listener;
            if (n && !source_url.isEmpty ())
                n->requestPlayURL (element, region_node);
        }
        return old_val;
    }
    return MediaTypeRuntime::setParam (name, val);
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
    else if (!strcmp (tag.latin1 (), "animate"))
        return new SMIL::Animate (d);
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
            if (b) {
                static_cast <RegionRuntime *> (rt.ptr ())->init ();
                rt->begin ();
            } else
                rt->end ();
        }
    }
    for (RegionNodePtr c = rn->firstChild; c; c = c->nextSibling)
        beginOrEndRegions (c, b);
}

KDE_NO_EXPORT void Smil::start () {
    kdDebug () << "Smil::start" << endl;
    current_av_media_type = ElementPtr ();
    Element::start ();
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

//-----------------------------------------------------------------------------

KDE_NO_EXPORT ElementPtr SMIL::Head::childFromTag (const QString & tag) {
    if (!strcmp (tag.latin1 (), "layout"))
        return (new SMIL::Layout (m_doc))->self ();
    return ElementPtr ();
}

KDE_NO_EXPORT bool SMIL::Head::expose () {
    return false;
}

KDE_NO_EXPORT void SMIL::Head::closed () {
    for (ElementPtr e = firstChild (); e; e = e->nextSibling ())
        if (!strcmp (e->nodeName (), "layout"))
            return;
    SMIL::Layout * layout = new SMIL::Layout (m_doc);
    appendChild (layout->self ());
    layout->closed (); // add root-layout and a region
}

//-----------------------------------------------------------------------------

KDE_NO_EXPORT ElementPtr SMIL::Layout::childFromTag (const QString & tag) {
    if (!strcmp (tag.latin1 (), "root-layout"))
        return (new SMIL::RootLayout (m_doc))->self ();
    else if (!strcmp (tag.latin1 (), "region"))
        return (new SMIL::Region (m_doc))->self ();
    return ElementPtr ();
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

KDE_NO_EXPORT void SMIL::Layout::closed () {
    RegionNodePtr root;
    RegionBase * smilroot = 0L;
    RegionNodePtr region;
    RegionNodePtr last_region;
    for (ElementPtr e = firstChild (); e; e = e->nextSibling ()) {
        const char * name = e->nodeName ();
        if (!strcmp (name, "root-layout")) {
            root = (new RegionNode (e))->self;
            if (region)
                root->firstChild = region;
            smilroot = convertNode <RegionBase> (e);
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
    if (!root) {
        int w =0, h = 0;
        smilroot = new SMIL::RootLayout (m_doc);
        appendChild (smilroot->self ());
        if (!region) {
            w = 100; h = 100; // have something to start with
            SMIL::Region * r = new SMIL::Region (m_doc);
            appendChild (r->self ());
            region = (new RegionNode (r->self ()))->self;
        } else {
            for (RegionNodePtr rn = region; rn; rn = rn->nextSibling) {
                SMIL::Region *rb = convertNode<SMIL::Region>(rn->regionElement);
                if (rb) {
                    ElementRuntimePtr rt = rb->getRuntime ();
                    static_cast <RegionRuntime *> (rt.ptr ())->init ();
                    rb->calculateBounds (0, 0);
                    if (rb->x + rb->w > w)
                        w = rb->x + rb->w;
                    if (rb->y + rb->h > h)
                        h = rb->y + rb->h;
                }
            }
        }
        smilroot->setAttribute ("width", QString::number (w));
        smilroot->setAttribute ("height", QString::number (h));
        root = (new RegionNode (smilroot->self ()))->self;
        root->firstChild = region;
    } else if (!region) {
        SMIL::Region * r = new SMIL::Region (m_doc);
        appendChild (r->self ());
        region = (new RegionNode (r->self ()))->self;
        root->firstChild = region;
    }
    if (!root || !region) {
        kdError () << "Layout w/o a root-layout w/ regions" << endl;
        return;
    }
    smilroot->updateLayout ();
    if (smilroot->w <= 0 || smilroot->h <= 0) {
        kdError () << "Root layout not having valid dimensions" << endl;
        return;
    }
    rootLayout = root;
    document ()->rootLayout = root;
}

KDE_NO_EXPORT void SMIL::Layout::start () {
    kdDebug () << "SMIL::Layout::start" << endl;
    setState (state_started);
    RegionNodePtr rn = document ()->rootLayout;
    if (rn && rn->regionElement) {
        beginOrEndRegions (rn, true);
        convertNode <RegionBase> (rn->regionElement)->updateLayout ();
        document ()->rootLayout->repaint ();
    }
    stop (); // that's fast :-)
}

//-----------------------------------------------------------------------------

KDE_NO_EXPORT ElementRuntimePtr RegionBase::getRuntime () {
    if (!runtime)
        runtime = ElementRuntimePtr (new RegionRuntime (m_self));
    return runtime;
}

KDE_NO_EXPORT void RegionBase::updateLayout () {
    x = y = 0;
    ElementRuntimePtr rt = getRuntime ();
    if (rt) {
        RegionRuntime * rr = static_cast <RegionRuntime *> (rt.ptr ());
        w = rr->width.toInt ();
        h = rr->height.toInt ();
        kdDebug () << "RegionBase::updateLayout " << w << "," << h << endl;
        if (rr->region_node)
            rr->region_node->calculateChildBounds ();
    }
}

//-----------------------------------------------------------------------------

KDE_NO_EXPORT ElementPtr SMIL::Region::childFromTag (const QString & tag) {
    if (!strcmp (tag.latin1 (), "region"))
        return (new SMIL::Region (m_doc))->self ();
    return ElementPtr ();
}

static int calcLength (const QString & strval, int full) {
    if (strval.isEmpty ())
        return 0;
    int p = strval.find (QChar ('%'));
    if (p > -1)
        return int (strval.left (p).toDouble () * full / 100);
    return int (strval.toDouble ());
}

/**
 * calculates dimensions of this regions with w and h as width and height
 */
KDE_NO_EXPORT void SMIL::Region::calculateBounds (int _w, int _h) {
    ElementRuntimePtr rt = getRuntime ();
    if (rt) {
        RegionRuntime * rr = static_cast <RegionRuntime *> (rt.ptr ());
        x = calcLength (rr->left, _w);
        y = calcLength (rr->top, _h);
        int w1 = calcLength (rr->width, _w);
        int h1 = calcLength (rr->height, _h);
        int r = calcLength (rr->right, _w);
        int b = calcLength (rr->bottom, _h);
        w = w1 > 0 ? w1 : _w - x - (r > 0 ? r : 0);
        h = h1 > 0 ? h1 : _h - y - (b > 0 ? b : 0);
        kdDebug () << "Region::calculateBounds " << x << "," << y << " " << w << "x" << h << endl;
    }
}

//-----------------------------------------------------------------------------

KDE_NO_EXPORT void SMIL::TimedMrl::start () {
    kdDebug () << "SMIL::TimedMrl(" << nodeName() << ")::start" << endl;
    setState (state_started);
    ElementRuntimePtr rt = getRuntime ();
    if (rt) {
        rt->init ();
        rt->begin ();
    }
}

KDE_NO_EXPORT void SMIL::TimedMrl::stop () {
    Mrl::stop ();
}

KDE_NO_EXPORT void SMIL::TimedMrl::reset () {
    kdDebug () << "SMIL::TimedMrl::reset " << endl;
    Mrl::reset ();
    if (runtime)
        runtime->end ();
}

/*
 * Re-implement, but keeping sequential behaviour.
 * Bail out if Runtime is running. In case of duration_media, give Runtime
 * a hand with calling propagateStop(true)
 */
KDE_NO_EXPORT void SMIL::TimedMrl::childDone (ElementPtr c) {
    if (c->nextSibling ())
        c->nextSibling ()->start ();
    else { // check if Runtime still running
        TimedRuntime * tr = static_cast <TimedRuntime *> (getRuntime ().ptr ());
        if (tr && tr->state () < TimedRuntime::timings_stopped) {
            if (tr->state () == TimedRuntime::timings_started)
                tr->propagateStop (false);
            return; // still running, wait for runtime to finish
        }
        stop ();
    }
}

KDE_NO_EXPORT ElementRuntimePtr SMIL::TimedMrl::getRuntime () {
    if (!runtime)
        runtime = getNewRuntime ();
    return runtime;
}

//-----------------------------------------------------------------------------

KDE_NO_EXPORT ElementRuntimePtr SMIL::TimedElement::getRuntime () {
    if (!runtime)
        runtime = getNewRuntime ();
    return runtime;
}

KDE_NO_EXPORT void SMIL::TimedElement::start () {
    setState (state_started);
    ElementRuntimePtr rt = getRuntime ();
    rt->init ();
    rt->begin ();
}

KDE_NO_EXPORT void SMIL::TimedElement::stop () {
    Element::stop ();
}

KDE_NO_EXPORT void SMIL::TimedElement::reset () {
    getRuntime ()->end ();
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
    GroupBase::start (); // calls init() and begin() on runtime
    if (firstChild ()) { // wait for childDone() notifications
        for (ElementPtr e = firstChild (); e; e = e->nextSibling ())
            e->start ();
    } else { // no children, stop if runtime started and no duration set
        TimedRuntime * tr = static_cast <TimedRuntime *> (getRuntime ().ptr ());
        if (tr && tr->state () == TimedRuntime::timings_started) {
            if (tr->durations[(int)TimedRuntime::duration_time].durval == duration_media)
                tr->propagateStop (false);
            return; // still running, wait for runtime to finish
        }
        stop (); // no children to run in parallel
    }
}

KDE_NO_EXPORT void SMIL::Par::stop () {
    kdDebug () << "SMIL::Par::stop" << endl;
    for (ElementPtr e = firstChild (); e; e = e->nextSibling ())
        // children are out of scope now, reset their ElementRuntime
        e->reset (); // will call stop() if necessary
    GroupBase::stop ();
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
        TimedRuntime * tr = static_cast <TimedRuntime *> (getRuntime ().ptr ());
        if (tr && tr->state () == TimedRuntime::timings_started) {
            if (tr->durations[(int)TimedRuntime::duration_time].durval == duration_media)
                tr->propagateStop (false);
            return; // still running, wait for runtime to finish
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
    : TimedMrl (d), m_type (t), bitrate (0) {}

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
        if (!strcmp (cname, "system-bitrate"))
            bitrate = a->nodeValue ().toInt ();
        else if (!strcmp (cname, "src"))
            src = a->nodeValue ();
        else if (!strcmp (cname, "type"))
            mimetype = a->nodeValue ();
    }
}

KDE_NO_EXPORT void SMIL::MediaType::start () {
    setState (state_started);
    ElementRuntimePtr rt = getRuntime ();
    if (rt) {
        rt->init (); // sets all attributes
        if (firstChild ())
            firstChild ()->start ();
        rt->begin ();
    }
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
    TimedMrl::stop ();
    TimedRuntime * tr = static_cast <TimedRuntime *> (getRuntime ().ptr ());
    if (tr && tr->state () == TimedRuntime::timings_started)
        tr->emitElementStopped (); // called from backends
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

KDE_NO_EXPORT ElementRuntimePtr SMIL::Set::getNewRuntime () {
    return ElementRuntimePtr (new SetData (m_self));
}

KDE_NO_EXPORT void SMIL::Set::start () {
    TimedElement::start ();
    stop (); // no livetime of itself
}

//-----------------------------------------------------------------------------

KDE_NO_EXPORT ElementRuntimePtr SMIL::Animate::getNewRuntime () {
    return ElementRuntimePtr (new AnimateData (m_self));
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
            int olddur;
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
        QString old_val = MediaTypeRuntime::setParam (name, val);
        if (!val.isEmpty ()) {
            KURL url (source_url);
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
        return old_val;
    }
    return MediaTypeRuntime::setParam (name, val);
}

KDE_NO_EXPORT void ImageData::paint (QPainter & p) {
    if (d->image && region_node && (timingstate == timings_started ||
                (timingstate == timings_stopped && fill == fill_freeze))) {
        RegionNode * r = region_node.ptr ();
        p.drawPixmap (QRect (r->x, r->y, r->w, r->h), *d->image);
    }
}

/**
 * start_timer timer expired, repaint if we have an image
 */
KDE_NO_EXPORT void ImageData::started () {
    if (mt_d->job) {
        d->olddur = durations [duration_time].durval;
        durations [duration_time].durval = duration_data_download;
        return;
    }
    if (durations [duration_time].durval == duration_media)
        durations [duration_time].durval = 0; // intrinsic duration of 0
    MediaTypeRuntime::started ();
}

KDE_NO_EXPORT void ImageData::slotResult (KIO::Job * job) {
    kdDebug () << "ImageData::slotResult" << endl;
    MediaTypeRuntime::slotResult (job);
    if (mt_d->data.size () && element) {
        QPixmap *pix = new QPixmap (mt_d->data);
        if (!pix->isNull ()) {
            d->image = pix;
            if (region_node && (timingstate == timings_started ||
                 (timingstate == timings_stopped && fill == fill_freeze)))
                region_node->repaint ();
        } else
            delete pix;
    }
    if (timingstate == timings_started &&
            durations [duration_time].durval == duration_data_download) {
        durations [duration_time].durval = d->olddur;
        QTimer::singleShot (0, this, SLOT (started ()));
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
        int olddur;
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
    if (name == QString::fromLatin1 ("src")) {
        d->data.resize (0);
        killWGet ();
        QString old_val = MediaTypeRuntime::setParam (name, val);
        if (!val.isEmpty ()) {
            KURL url (source_url);
            if (url.isLocalFile ()) {
                QFile file (url.path ());
                file.open (IO_ReadOnly);
                d->data = file.readAll ();
            } else
                wget (url);
        }
        return old_val;
    } else if (name == QString::fromLatin1 ("backgroundColor")) {
        d->background_color = QColor (val).rgb ();
    } else if (name == QString ("fontColor")) {
        d->foreground_color = QColor (val).rgb ();
    } else if (name == QString ("charset")) {
        d->codec = QTextCodec::codecForName (val.ascii ());
    } else if (name == QString ("fontFace")) {
        ; //FIXME
    } else if (name == QString ("fontPtSize")) {
        d->font.setPointSize (val.toInt ());
    } else if (name == QString ("fontSize")) {
        d->font.setPointSize (d->font.pointSize () + val.toInt ());
    // TODO: expandTabs fontBackgroundColor fontSize fontStyle fontWeight hAlig vAlign wordWrap
    }
    return MediaTypeRuntime::setParam (name, val);
}

KDE_NO_EXPORT void TextData::paint (QPainter & p) {
    if (region_node && (timingstate == timings_started ||
                (timingstate == timings_stopped && fill == fill_freeze))) {
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

/**
 * start_timer timer expired, repaint if we have text
 */
KDE_NO_EXPORT void TextData::started () {
    if (mt_d->job) {
        d->olddur = durations [duration_time].durval;
        durations [duration_time].durval = duration_data_download;
        return;
    }
    if (durations [duration_time].durval == duration_media)
        durations [duration_time].durval = 0; // intrinsic duration of 0
    MediaTypeRuntime::started ();
}

KDE_NO_EXPORT void TextData::slotResult (KIO::Job * job) {
    MediaTypeRuntime::slotResult (job);
    if (mt_d->data.size () && element) {
        d->data = mt_d->data;
        if (d->data.size () > 0 && !d->data [d->data.size () - 1])
            d->data.resize (d->data.size () - 1); // strip zero terminate char
        if (region_node && (timingstate == timings_started ||
                    (timingstate == timings_stopped && fill == fill_freeze)))
            region_node->repaint ();
    }
    if (timingstate == timings_started &&
            durations [duration_time].durval == duration_data_download) {
        durations [duration_time].durval = d->olddur;
        QTimer::singleShot (0, this, SLOT (started ()));
    }
}

#include "kmplayer_smil.moc"
