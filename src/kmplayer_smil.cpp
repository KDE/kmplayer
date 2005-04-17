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
#include <qimage.h>
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
static const unsigned int event_activated = duration_element_activated;
static const unsigned int event_inbounds = duration_element_inbounds;
static const unsigned int event_outbounds = duration_element_outbounds;
static const unsigned int event_stopped = duration_element_stopped;
static const unsigned int event_to_be_started = (unsigned int) -8;
static const unsigned int duration_last_option = (unsigned int) -8;

/* Intrinsic duration 
 *  duration_time   |    end_time    |
 *  =======================================================================
 *  duration_media  | duration_media | wait for external stop (audio/video)
 *       0          | duration_media | only wait for child elements
 */
//-----------------------------------------------------------------------------

static RegionNodePtr findRegion (RegionNodePtr p, const QString & id) {
    for (RegionNodePtr r = p->firstChild (); r; r = r->nextSibling ()) {
        if (r->region_element->isElementNode ()) {
            QString a = convertNode <Element> (r->region_element)->getAttribute ("id");
            if ((a.isEmpty () && id.isEmpty ()) || a == id) {
                //kdDebug () << "MediaType region found " << id << endl;
                return r;
            }
        }
        RegionNodePtr r1 = findRegion (r, id);
        if (r1)
            return r1;
    }
    return RegionNodePtr ();
}

//-----------------------------------------------------------------------------

ElementRuntimePtr Node::getRuntime () {
    return ElementRuntimePtr ();
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT RegionNode::RegionNode (NodePtr e)
 : has_mouse (false), x (0), y (0), w (0), h (0),
   xscale (0.0), yscale (0.0),
   z_order (1), region_element (e),
   element_users ((new NodeRefList)->self ()) {
    ElementRuntimePtr rt = e->getRuntime ();
    if (rt)
        rt->region_node = m_self;
}

KDE_NO_CDTOR_EXPORT void RegionNode::clearAll () {
    kdDebug () << "RegionNode::clearAll " << endl;
    element_users = (new NodeRefList)->self ();
    for (RegionNodePtr r = firstChild (); r; r = r->nextSibling ())
        r->clearAll ();
    repaint ();
}

KDE_NO_EXPORT
void RegionNode::paint (QPainter & p, int _x, int _y, int _w, int _h) {
    if (x + w < _x || _x + _w < x || y + h < _y || _y + _h < y)
        return;
    p.setClipRect (x, y, w, h, QPainter::CoordPainter);

    if (region_element) {
        ElementRuntimePtr rt = region_element->getRuntime ();
        if (rt)
            rt->paint (p);
    }

    for (NodeRefItemPtr c = element_users->first(); c; c =c->nextSibling())
        if (c->data) {
            ElementRuntimePtr rt = c->data->getRuntime ();
            if (rt)
                rt->paint (p);
        }
    // now paint children, accounting for z-order
    int done_index = -1;
    do {
        int cur_index = 1 << 8 * sizeof (int) - 2;  // should be enough :-)
        int check_index = cur_index;
        for (RegionNodePtr r = firstChild (); r; r = r->nextSibling ()) {
            if (r->z_order > done_index && r->z_order < cur_index)
                cur_index = r->z_order;
        }
        if (check_index == cur_index)
            break;
        for (RegionNodePtr r = firstChild (); r; r = r->nextSibling ())
            if (r->z_order == cur_index) {
                //kdDebug () << "Painting " << cur_index << endl;
                r->paint (p, _x, _y, _w, _h);
            }
        done_index = cur_index;
    } while (true);
    p.setClipping (false);
}

KDE_NO_EXPORT void RegionNode::repaint () {
    if (region_element) {
        PlayListNotify * n = region_element->document()->notify_listener;
        if (n)
            n->repaintRect (x, y, w, h);
    }
};

KDE_NO_EXPORT void RegionNode::calculateChildBounds () {
    SMIL::Region * r = convertNode <SMIL::Region> (region_element);
    for (RegionNodePtr rn = firstChild (); rn; rn = rn->nextSibling ()) {
        SMIL::Region * cr = convertNode <SMIL::Region> (rn->region_element);
        cr->calculateBounds (r->w, r->h);
        rn->calculateChildBounds ();
        if (xscale > 0.001)
            scaleRegion (xscale, yscale, x, y);
    }
}

KDE_NO_EXPORT void RegionNode::dispatchMouseEvent (unsigned int event_id) {
    for (NodeRefItemPtr c = element_users->first (); c; c = c->nextSibling ())
        if (c->data) { // FIXME: check if really on Element?
            MouseSignaler * rt = dynamic_cast <MouseSignaler *> (c->data->getRuntime ().ptr ());
            if (rt)
                rt->propagateEvent ((new Event (event_id))->self ());
        }
    if (region_element) {
        MouseSignaler * rt = dynamic_cast <MouseSignaler *> (region_element->getRuntime ().ptr ());
        if (rt)
            rt->propagateEvent ((new Event (event_id))->self ());
    }
}

KDE_NO_EXPORT bool RegionNode::pointerClicked (int _x, int _y) {
    bool inside = _x > x && _x < x + w && _y > y && _y < y + h;
    if (!inside)
        return false;
    bool handled = false;
    for (RegionNodePtr r = firstChild (); r; r = r->nextSibling ())
        handled |= r->pointerClicked (_x, _y);
    if (!handled) // handle it ..
        dispatchMouseEvent (event_activated);
    return inside;
}

KDE_NO_EXPORT bool RegionNode::pointerMoved (int _x, int _y) {
    bool inside = _x > x && _x < x + w && _y > y && _y < y + h;
    bool handled = false;
    if (inside)
        for (RegionNodePtr r = firstChild (); r; r = r->nextSibling ())
            handled |= r->pointerMoved (_x, _y);
    if (has_mouse && (!inside || handled)) { // OutOfBoundsEvent
        has_mouse = false;
        dispatchMouseEvent (event_outbounds);
    } else if (inside && !handled && !has_mouse) { // InBoundsEvent
        has_mouse = true;
        dispatchMouseEvent (event_inbounds);
    }
    return inside;
}

KDE_NO_EXPORT void RegionNode::setSize (int _x, int _y, int _w, int _h, bool keepaspect) {
    RegionBase * region = convertNode <RegionBase> (region_element);
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
    RegionBase * smilregion = convertNode <RegionBase> (region_element);
    if (smilregion) {  // note WeakPtr can be null
        x = xoff + int (sx * smilregion->x);
        y = yoff + int (sy * smilregion->y);
        w = int (sx * smilregion->w);
        h = int (sy * smilregion->h);
        xscale = sx;
        yscale = sy;
        for (NodeRefItemPtr c =element_users->first(); c; c=c->nextSibling()) {
            if (!c->data)
                continue;
            // hack to get the one and only audio/video widget sizes
            const char * nn = c->data->nodeName ();
            PlayListNotify * n = c->data->document ()->notify_listener;
            if (n && !strcmp (nn, "video") || !strcmp (nn, "audio")) {
                ElementRuntimePtr rt = smilregion->getRuntime ();
                RegionRuntime *rr = static_cast <RegionRuntime*> (rt.ptr());
                MediaTypeRuntime * mtr = static_cast <MediaTypeRuntime *> (c->data->getRuntime ().ptr ());
                if (rr && mtr) {
                    int w1, h1;
                    mtr->calcSizes (w, h, xoff, yoff, w1, h1);
                    xoff = int (xoff * xscale);
                    yoff = int (yoff * yscale);
                    n->avWidgetSizes (x+xoff, y+yoff, w1, h1, rr->have_bg_color ? &rr->background_color : 0L);
                }
            }
        }
        //kdDebug () << "Region size " << x << "," << y << " " << w << "x" << h << endl;
    }
    for (RegionNodePtr r = firstChild (); r; r =r->nextSibling ())
        r->scaleRegion (sx, sy, x, y);
}

//-----------------------------------------------------------------------------
namespace KMPlayer {
    class ElementRuntimePrivate {
    public:
        QMap <QString, QString> params;
    };
}

ElementRuntime::ElementRuntime (NodePtr e)
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
    if (element && element->isElementNode ()) {
        for (AttributePtr a= convertNode <Element> (element)->attributes ()->first (); a; a = a->nextSibling ())
            setParam (QString (a->nodeName ()), a->nodeValue ());
    }
}

KDE_NO_EXPORT void ElementRuntime::reset () {
    region_node = RegionNodePtr ();
    d->params.clear ();
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT Connection::Connection (RuntimeRefListPtr ls, ElementRuntimePtr rt) : listeners (ls) {
    if (listeners) {
        RuntimeRefItemPtr rci = (new RuntimeRefItem (rt))->self ();
        listeners->append (rci);
        listen_item = rci;
    }
}

KDE_NO_CDTOR_EXPORT void Connection::disconnect () {
    if (listen_item && listeners)
        listeners->remove (listen_item);
    listen_item = 0L;
    listeners = 0L;
}

KDE_NO_CDTOR_EXPORT ToBeStartedEvent::ToBeStartedEvent (NodePtr n)
 : Event (event_to_be_started), node (n) {}

KDE_NO_EXPORT void Signaler::propagateEvent (EventPtr event) {
    RuntimeRefListPtr nl = listeners (event->id ());
    if (nl)
        for (RuntimeRefItemPtr c = nl->first(); c; c = c->nextSibling ())
            if (c->data) {
                Listener * l = dynamic_cast <Listener *> (c->data.ptr ());
                if (l)
                    l->handleEvent (event);
                else
                    kdWarning () << "Non listener in listeners list" << endl;
            }
}

KDE_NO_EXPORT
ConnectionPtr Signaler::connectTo (ElementRuntimePtr rt, unsigned int evt_id) {
    return ConnectionPtr (new Connection (listeners (evt_id), rt));
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT MouseSignaler::MouseSignaler ()
 : m_ActionListeners ((new RuntimeRefList)->self ()),
   m_OutOfBoundsListeners ((new RuntimeRefList)->self ()),
   m_InBoundsListeners ((new RuntimeRefList)->self ()) {}

RuntimeRefListPtr MouseSignaler::listeners(unsigned int eid) {
    switch (eid) {
        case event_activated:
            return m_ActionListeners;
        case event_inbounds:
            return m_InBoundsListeners;
        case event_outbounds:
            return m_OutOfBoundsListeners;
    }
    return RuntimeRefListPtr ();
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT
TimedRuntime::TimedRuntime (NodePtr e)
 : ElementRuntime (e),
   m_StartedListeners ((new RuntimeRefList)->self ()),
   m_StoppedListeners ((new RuntimeRefList)->self ()) {
    reset ();
}

KDE_NO_CDTOR_EXPORT TimedRuntime::~TimedRuntime () {}

KDE_NO_EXPORT void TimedRuntime::reset () {
    start_timer = 0;
    dur_timer = 0;
    repeat_count = 0;
    timingstate = timings_reset;
    fill = fill_unknown;
    for (int i = 0; i < (int) durtime_last; i++) {
        if (durations [i].connection)
            durations [i].connection->disconnect ();
        durations [i].durval = duration_media;  //intrinsic time duration
    }
    durations [begin_time].durval = 0;
    ElementRuntime::reset ();
}

KDE_NO_EXPORT
void TimedRuntime::setDurationItem (DurationTime item, const QString & val) {
    unsigned int dur = 0; // also 0 for 'media' duration, so it will not update then
    QRegExp reg ("^\\s*([0-9\\.]+)\\s*([a-z]*)");
    QString vl = val.lower ();
    kdDebug () << "getSeconds " << val << (element ? element->nodeName() : "-") << endl;
    if (reg.search (vl) > -1) {
        bool ok;
        double t = reg.cap (1).toDouble (&ok);
        if (ok && t > 0.000) {
            kdDebug() << "reg.cap (1) " << t << (ok && t > 0.000) << endl;
            QString u = reg.cap (2);
            if (u.startsWith ("m"))
                dur = (unsigned int) (10 * t * 60);
            else if (u.startsWith ("h"))
                dur = (unsigned int) (10 * t * 60 * 60);
            dur = (unsigned int) (10 * t);
        }
    } else if (vl.find ("indefinite") > -1)
        dur = duration_infinite;
    else if (vl.find ("media") > -1)
        dur = duration_media;
    if (!dur && element) {
        int pos = vl.find (QChar ('.'));
        if (pos > 0) {
            NodePtr e = element->document()->getElementById (vl.left(pos));
            if (e) {
                kdDebug () << "getElementById " << vl.left (pos) << " " << e->nodeName () << endl;
                ElementRuntimePtr rt = e->getRuntime ();
                MouseSignaler * rs = dynamic_cast<MouseSignaler*>(rt.ptr());
                if (rs) {
                    if (vl.find ("activateevent") > -1) {
                        dur = duration_element_activated;
                    } else if (vl.find ("inboundsevent") > -1) {
                        dur = duration_element_inbounds;
                    } else if (vl.find ("outofboundsevent") > -1) {
                        dur = duration_element_outbounds;
                    }
                    durations [(int) item].connection=rs->connectTo(m_self,dur);
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
        if (durations [begin_time].durval < duration_last_option)
            start_timer = startTimer (100 * durations [begin_time].durval);
    } else
        propagateStart ();
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
    killTimers ();
    reset ();
}

/**
 * change behaviour of this runtime, returns old value
 */
KDE_NO_EXPORT
QString TimedRuntime::setParam (const QString & name, const QString & val) {
    //kdDebug () << "TimedRuntime::setParam " << name << "=" << val << endl;
    if (name == QString::fromLatin1 ("begin")) {
        setDurationItem (begin_time, val);
        if ((timingstate == timings_began && !start_timer) ||
                timingstate == timings_stopped) {
            if (durations [begin_time].durval > 0) { // create a timer for start
                if (durations [begin_time].durval < duration_last_option)
                    start_timer = startTimer(100*durations[begin_time].durval);
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
    } else if (name == QString::fromLatin1 ("endsync")) {
        if (durations [duration_time].durval == duration_media &&
                durations [end_time].durval == duration_media) {
            NodePtr e = element->document ()->getElementById (val);
            if (e) {
                ElementRuntimePtr rt = e->getRuntime ();
                TimedRuntime * tr = dynamic_cast <TimedRuntime *> (rt.ptr ());
                if (tr) {
                    durations [(int) end_time].connection = tr->connectTo (m_self, event_stopped);
                    durations [(int) end_time].durval = event_stopped;
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
        propagateStart ();
    } else if (e->timerId () == dur_timer)
        propagateStop (true);
}

KDE_NO_EXPORT void TimedRuntime::processEvent (unsigned int event) {
    kdDebug () << "TimedRuntime::processEvent " << event << " " << (element ? element->nodeName() : "-") << endl; 
    if (timingstate != timings_started && durations [begin_time].durval == event) {
        if (timingstate != timings_started)
            propagateStart ();
    } else if (timingstate == timings_started && durations [end_time].durval == event)
        propagateStop (true);
}

KDE_NO_EXPORT bool TimedRuntime::handleEvent (EventPtr event) {
    processEvent (event->id ());
    return true;
}

KDE_NO_EXPORT RuntimeRefListPtr TimedRuntime::listeners (unsigned int id) {
    if (id == event_stopped)
        return m_StoppedListeners;
    else if (id == event_to_be_started)
        return m_StartedListeners;
    kdWarning () << "unknown event requested" << endl;
    return RuntimeRefListPtr ();
}

KDE_NO_EXPORT void TimedRuntime::propagateStop (bool forced) {
    if (!forced && element) {
        if (durations [end_time].durval > duration_last_option &&
                durations [end_time].durval != duration_media)
            return; // wait for event
        // bail out if a child still running
        for (NodePtr c = element->firstChild (); c; c = c->nextSibling ())
            if (c->state == Element::state_activated)
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

KDE_NO_EXPORT void TimedRuntime::propagateStart () {
    propagateEvent ((new ToBeStartedEvent (element))->self ());
    timingstate = timings_started;
    QTimer::singleShot (0, this, SLOT (started ()));
}

/**
 * start_timer timer expired
 */
KDE_NO_EXPORT void TimedRuntime::started () {
    kdDebug () << "TimedRuntime::started " << (element ? element->nodeName() : "-") << endl; 
    if (durations [duration_time].durval > 0) {
        if (durations [duration_time].durval < duration_last_option) {
            dur_timer = startTimer (100 * durations [duration_time].durval);
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
            start_timer = startTimer (100 * durations [begin_time].durval);
        } else
            propagateStart ();
    } else if (element->state == Element::state_activated) {
        element->deactivate ();
        propagateEvent ((new Event (event_stopped))->self ());
    }
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT SizeType::SizeType () {
    reset ();
}

void SizeType::reset () {
    m_size = 0;
    percentage = 0;
}

SizeType & SizeType::operator = (const QString & s) {
    percentage = false;
    QString strval (s);
    int p = strval.find (QChar ('%'));
    if (p > -1) {
        percentage = true;
        strval.truncate (p);
    }
    bool ok;
    m_size = int (strval.toDouble (&ok));
    if (!ok)
        m_size = 0;
    return *this;
}

int SizeType::size (int relative_to) {
    if (percentage)
        return m_size * relative_to / 100;
    return m_size;
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT SizedRuntime::SizedRuntime() {}

KDE_NO_EXPORT void SizedRuntime::resetSizes () {
    left.reset ();
    top.reset ();
    width.reset ();
    height.reset ();
    right.reset ();
    bottom.reset ();
}

KDE_NO_EXPORT void SizedRuntime::calcSizes (int w, int h, int & xoff, int & yoff, int & w1, int & h1) {
    xoff = left.size (w);
    yoff = top.size (h);
    w1 = width.size (w);
    h1 = height.size (h);
    int roff = right.size (w);
    int boff = bottom.size (h);
    w1 = w1 > 0 ? w1 : w - xoff - roff;
    h1 = h1 > 0 ? h1 : h - yoff - boff;
}

KDE_NO_EXPORT bool SizedRuntime::setSizeParam (const QString & name, const QString & val) {
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
    have_bg_color = false;
    active = false;
    resetSizes ();
}

KDE_NO_EXPORT void RegionRuntime::paint (QPainter & p) {
    if (have_bg_color && region_node) {
        RegionNode * rn = region_node.ptr ();
        p.fillRect (rn->x, rn->y, rn->w, rn->h, QColor(QRgb(background_color)));
    }
}

KDE_NO_EXPORT
QString RegionRuntime::setParam (const QString & name, const QString & val) {
    //kdDebug () << "RegionRuntime::setParam " << name << "=" << val << endl;
    RegionNode * rn = region_node.ptr ();
    QRect rect;
    bool need_repaint = false;
    if (rn)
        rect = QRect (rn->x, rn->y, rn->w, rn->h);
    if (name == QString::fromLatin1 ("background-color") ||
            name == QString::fromLatin1 ("background-color")) {
        background_color = QColor (val).rgb ();
        have_bg_color = true;
        need_repaint = true;
    } else if (name == QString::fromLatin1 ("z-index")) {
        if (region_node)
            region_node->z_order = val.toInt ();
        need_repaint = true;
    } else if (setSizeParam (name, val)) {
        if (active && rn && element) {
            if (rn->parentNode ())
                rn->parentNode ()->calculateChildBounds ();
            else {
                RegionNodePtr rootrn = element->document ()->rootLayout;
                if (rootrn && rootrn->region_element)
                    convertNode<RegionBase>(rootrn->region_element)->updateLayout ();
            }
            QRect nr (rn->x, rn->y, rn->w, rn->h);
            if (rect.width () == nr.width () && rect.height () == nr.height()) {
                PlayListNotify * n = element->document()->notify_listener;
                if (n && (rect.x () != nr.x () || rect.y () != nr.y ()))
                    n->moveRect (rect.x(), rect.y(), rect.width (), rect.height (), nr.x(), nr.y());
            } else {
                rect = rect.unite (nr);
                need_repaint = true;
            }
        }
    }
    if (need_repaint && active && rn && element) {
        PlayListNotify * n = element->document()->notify_listener;
        if (n)
            n->repaintRect (rect.x(), rect.y(), rect.width (), rect.height ());
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

KDE_NO_CDTOR_EXPORT ParRuntime::ParRuntime (NodePtr e) : TimedRuntime (e) {}

KDE_NO_EXPORT void ParRuntime::started () {
    if (element && element->firstChild ()) // activate all children
        for (NodePtr e = element->firstChild (); e; e = e->nextSibling ())
            e->activate ();
    else if (durations[(int)TimedRuntime::duration_time].durval==duration_media)
        durations[(int)TimedRuntime::duration_time].durval = 0;
    TimedRuntime::started ();
}

KDE_NO_EXPORT void ParRuntime::stopped () {
    if (element) // reset all children
        for (NodePtr e = element->firstChild (); e; e = e->nextSibling ())
            // children are out of scope now, reset their ElementRuntime
            e->reset (); // will call deactivate() if necessary
    TimedRuntime::stopped ();
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT ExclRuntime::ExclRuntime (NodePtr e) : TimedRuntime (e) {
}

KDE_NO_EXPORT void ExclRuntime::begin () {
    if (element) // setup connections with eveent_to_be_started
        for (NodePtr e = element->firstChild (); e; e = e->nextSibling ()) {
            TimedRuntime *tr=dynamic_cast<TimedRuntime*>(e->getRuntime().ptr());
            if (tr) {
                ConnectionPtr c = tr->connectTo (m_self, event_to_be_started);
                started_event_list.append((new ConnectionStoreItem(c))->self());
            }
        }
    TimedRuntime::begin ();
}

KDE_NO_EXPORT void ExclRuntime::reset () {
    started_event_list.clear (); // auto disconnect on destruction of data items
    TimedRuntime::reset ();
}

KDE_NO_EXPORT bool ExclRuntime::handleEvent (EventPtr event) {
    if (event->id () == event_to_be_started) {
        ToBeStartedEvent * se = static_cast <ToBeStartedEvent *> (event.ptr ());
        kdDebug () << "ExclRuntime::handleEvent " << se->node->nodeName()<<endl;
        if (element) // stop all other child elements
            for (NodePtr e = element->firstChild (); e; e = e->nextSibling ()) {
                if (e == se->node)
                    continue;
                TimedRuntime *tr=dynamic_cast<TimedRuntime*>(e->getRuntime().ptr());
                if (tr && tr->state()>timings_reset && tr->state()<timings_stopped)
                    tr->propagateStop (true);
            }
        return true;
    } else
        return TimedRuntime::handleEvent (event);
}

//-----------------------------------------------------------------------------

QString AnimateGroupData::setParam (const QString & name, const QString & val) {
    //kdDebug () << "AnimateGroupData::setParam " << name << "=" << val << endl;
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

KDE_NO_CDTOR_EXPORT AnimateData::AnimateData (NodePtr e)
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
    calcMode = calc_linear;
    change_from.truncate (0);
    change_values.clear ();
    steps = 0;
    change_delta = change_to_val = change_from_val = 0.0;
    change_from_unit.truncate (0);
}

QString AnimateData::setParam (const QString & name, const QString & val) {
    //kdDebug () << "AnimateData::setParam " << name << "=" << val << endl;
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
    } else
        return AnimateGroupData::setParam (name, val);
    return ElementRuntime::setParam (name, val);
}

/**
 * start_timer timer expired, execute it
 */
KDE_NO_EXPORT void AnimateData::started () {
    kdDebug () << "AnimateData::started " << durations [duration_time].durval << endl;
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
        ElementRuntimePtr rt = target_element->getRuntime ();
        if (!rt)
            break;
        target_region = rt->region_node;
        if (calcMode == calc_linear) {
            QRegExp reg ("^\\s*([0-9\\.]+)(\\s*[%a-z]*)?");
            if (change_from.isEmpty ()) {
                if (change_values.size () > 0) // check 'values' attribute
                     change_from = change_values.first ();
                else
                    change_from = rt->param (changed_attribute); // take current
            }
            if (!change_from.isEmpty ()) {
                old_value = rt->setParam (changed_attribute, change_from);
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
                anim_timer = startTimer (40); // 25 ms for now FIXME
                change_delta = (change_to_val - change_from_val) / steps;
                kdDebug () << "AnimateData::started " << target_element->nodeName () << "." << changed_attribute << " " << change_from_val << "->" << change_to_val << " in " << steps << " using:" << change_delta << " inc" << endl;
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
            kdDebug () << "AnimateData::started " << target_element->nodeName () << "." << changed_attribute << " " << change_values.first () << "->" << change_values.last () << " in " << steps << " interval:" << interval << endl;
            anim_timer = startTimer (interval);
            old_value = rt->setParam (changed_attribute, change_values.first());
            success = true;
        }
        //if (target_region)
        //    target_region->repaint ();
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
    kdDebug () << "AnimateData::stopped " << element->state << endl;
    if (anim_timer) { // make sure timers are stopped
        killTimer (anim_timer);
        anim_timer = 0;
    }
    AnimateGroupData::stopped ();
}

/**
 * for animations
 */
KDE_NO_EXPORT void AnimateData::timerEvent (QTimerEvent * e) {
    if (e->timerId () == anim_timer) {
        if (steps-- > 0 && target_element && target_element->getRuntime ()) {
            ElementRuntimePtr rt = target_element->getRuntime ();
            if (calcMode == calc_linear) {
                change_from_val += change_delta;
                rt->setParam (changed_attribute, QString ("%1%2").arg (change_from_val).arg(change_from_unit));
            } else if (calcMode == calc_discrete) {
                 rt->setParam (changed_attribute, change_values[change_values.size () - steps -1]);
            }
            RegionNodePtr target_region = rt->region_node;
            //if (target_region)
            //    target_region->repaint ();
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

KDE_NO_CDTOR_EXPORT MediaTypeRuntime::MediaTypeRuntime (NodePtr e)
 : TimedRuntime (e), mt_d (new MediaTypeRuntimePrivate), fit (fit_hidden) {}

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
    if (name == QString::fromLatin1 ("src")) {
        source_url = val;
        if (element) {
            QString url = convertNode <SMIL::MediaType> (element)->src;
            if (!url.isEmpty ())
                source_url = url;
        }
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
    } else if (!setSizeParam (name, val)) {
        return TimedRuntime::setParam (name, val);
    }
    RegionNode * rn = region_node.ptr ();
    if (state () == timings_began && rn && element)
        rn->repaint ();
    return ElementRuntime::setParam (name, val);
}

/**
 * find region node and request a repaint of attached region then
 */
KDE_NO_EXPORT void MediaTypeRuntime::started () {
    if (element && element->document ()->rootLayout) {
        region_node = findRegion (element->document()->rootLayout,
                                  param (QString::fromLatin1 ("region")));
        if (region_node)
            region_node->element_users->append ((new NodeRefItem (element))->self ());
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

KDE_NO_CDTOR_EXPORT AudioVideoData::AudioVideoData (NodePtr e)
    : MediaTypeRuntime (e) {}

KDE_NO_EXPORT bool AudioVideoData::isAudioVideo () {
    return timingstate == timings_started;
}

/**
 * reimplement for request backend to play audio/video
 */
KDE_NO_EXPORT void AudioVideoData::started () {
    MediaTypeRuntime::started ();
    if (region_node && element) {
        kdDebug () << "AudioVideoData::started " << source_url << endl;
        PlayListNotify * n = element->document ()->notify_listener;
        if (n && !source_url.isEmpty ()) {
            n->requestPlayURL (element);
            element->setState (Element::state_activated);
        }
    }
}

KDE_NO_EXPORT
QString AudioVideoData::setParam (const QString & name, const QString & val) {
    //kdDebug () << "AudioVideoData::setParam " << name << "=" << val << endl;
    if (name == QString::fromLatin1 ("src")) {
        QString old_val = MediaTypeRuntime::setParam (name, val);
        if (timingstate == timings_started && element) {
            PlayListNotify * n = element->document ()->notify_listener;
            if (n && !source_url.isEmpty ()) {
                n->requestPlayURL (element);
                element->setState (Element::state_activated);
            }
        }
        return old_val;
    }
    return MediaTypeRuntime::setParam (name, val);
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
    // animation, textstream, ref, brush
    return 0L;
}

static Element * fromContentControlGroup (NodePtr & d, const QString & tag) {
    if (!strcmp (tag.latin1 (), "switch"))
        return new SMIL::Switch (d);
    return 0L;
}

//-----------------------------------------------------------------------------

KDE_NO_EXPORT NodePtr Smil::childFromTag (const QString & tag) {
    if (!strcmp (tag.latin1 (), "body"))
        return (new SMIL::Body (m_doc))->self ();
    else if (!strcmp (tag.latin1 (), "head"))
        return (new SMIL::Head (m_doc))->self ();
    return NodePtr ();
}

static void beginOrEndRegions (RegionNodePtr rn, bool b) {
    if (rn->region_element) {
        ElementRuntimePtr rt = rn->region_element->getRuntime ();
        if (rt) {
            if (b) {
                static_cast <RegionRuntime *> (rt.ptr ())->init ();
                rt->begin ();
            } else
                rt->end ();
        }
    }
    for (RegionNodePtr c = rn->firstChild (); c; c = c->nextSibling ())
        beginOrEndRegions (c, b);
}

KDE_NO_EXPORT void Smil::activate () {
    kdDebug () << "Smil::activate" << endl;
    current_av_media_type = NodePtr ();
    Element::activate ();
}

KDE_NO_EXPORT void Smil::deactivate () {
    if (document ()->rootLayout) {
        beginOrEndRegions (document ()->rootLayout, false);
        document ()->rootLayout->repaint ();
    }
    Mrl::deactivate ();
}

KDE_NO_EXPORT NodePtr Smil::realMrl () {
    return current_av_media_type;
}

KDE_NO_EXPORT bool Smil::isMrl () {
    return true;
}

//-----------------------------------------------------------------------------

KDE_NO_EXPORT NodePtr SMIL::Head::childFromTag (const QString & tag) {
    if (!strcmp (tag.latin1 (), "layout"))
        return (new SMIL::Layout (m_doc))->self ();
    return NodePtr ();
}

KDE_NO_EXPORT bool SMIL::Head::expose () {
    return false;
}

KDE_NO_EXPORT void SMIL::Head::closed () {
    for (NodePtr e = firstChild (); e; e = e->nextSibling ())
        if (!strcmp (e->nodeName (), "layout"))
            return;
    SMIL::Layout * layout = new SMIL::Layout (m_doc);
    appendChild (layout->self ());
    layout->closed (); // add root-layout and a region
}

//-----------------------------------------------------------------------------

KDE_NO_EXPORT NodePtr SMIL::Layout::childFromTag (const QString & tag) {
    if (!strcmp (tag.latin1 (), "root-layout")) {
        NodePtr e = (new SMIL::RootLayout (m_doc))->self ();
        rootLayout = e;
        return e;
    } else if (!strcmp (tag.latin1 (), "region"))
        return (new SMIL::Region (m_doc))->self ();
    return NodePtr ();
}

static void buildRegionNodes (NodePtr p, RegionNodePtr r) {
    for (NodePtr e = p->firstChild (); e; e = e->nextSibling ())
        if (!strcmp (e->nodeName (), "region")) {
            r->appendChild ((new RegionNode (e))->self ());
            buildRegionNodes (e, r->lastChild ());
        }
}

KDE_NO_EXPORT void SMIL::Layout::closed () {
    RegionBase * smilroot = convertNode <SMIL::RootLayout> (rootLayout);
    bool has_root (smilroot);
    if (!has_root) { // just add one if non there
        smilroot = new SMIL::RootLayout (m_doc);
        appendChild (smilroot->self ());
        rootLayout = smilroot->self ();
    }
    regionRootLayout = (new RegionNode (rootLayout))->self ();
    buildRegionNodes (m_self, regionRootLayout);
    if (!has_root) {
        int w =0, h = 0;
        if (!regionRootLayout->hasChildNodes ()) {
            w = 100; h = 100; // have something to start with
            SMIL::Region * r = new SMIL::Region (m_doc);
            appendChild (r->self ());
        } else {
            for (RegionNodePtr rn = regionRootLayout->firstChild (); rn; rn = rn->nextSibling ()) {
                SMIL::Region *rb =convertNode<SMIL::Region>(rn->region_element);
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
    } else if (!regionRootLayout->hasChildNodes ()) {
        SMIL::Region * r = new SMIL::Region (m_doc);
        appendChild (r->self ());
        regionRootLayout->appendChild ((new RegionNode (r->self ()))->self ());
    }
    smilroot->updateLayout ();
    if (smilroot->w <= 0 || smilroot->h <= 0) {
        kdError () << "Root layout not having valid dimensions" << endl;
        return;
    }
    document ()->rootLayout = regionRootLayout;
}

KDE_NO_EXPORT void SMIL::Layout::activate () {
    kdDebug () << "SMIL::Layout::activate" << endl;
    setState (state_activated);
    RegionNodePtr rn = document ()->rootLayout;
    if (rn && rn->region_element) {
        beginOrEndRegions (rn, true);
        convertNode <RegionBase> (rn->region_element)->updateLayout ();
        document ()->rootLayout->repaint ();
    }
    deactivate (); // that's fast :-)
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
        w = rr->width.size ();
        h = rr->height.size ();
        kdDebug () << "RegionBase::updateLayout " << w << "," << h << endl;
        if (rr->region_node)
            rr->region_node->calculateChildBounds ();
    }
}

//-----------------------------------------------------------------------------

KDE_NO_EXPORT NodePtr SMIL::Region::childFromTag (const QString & tag) {
    if (!strcmp (tag.latin1 (), "region"))
        return (new SMIL::Region (m_doc))->self ();
    return NodePtr ();
}

/**
 * calculates dimensions of this regions with _w and _h as width and height
 * of parent Region (representing 100%)
 */
KDE_NO_EXPORT void SMIL::Region::calculateBounds (int _w, int _h) {
    ElementRuntimePtr rt = getRuntime ();
    if (rt) {
        RegionRuntime * rr = static_cast <RegionRuntime *> (rt.ptr ());
        rr->calcSizes (_w, _h, x, y, w, h);
        //kdDebug () << "Region::calculateBounds " << x << "," << y << " " << w << "x" << h << endl;
    }
}

//-----------------------------------------------------------------------------

KDE_NO_EXPORT void SMIL::TimedMrl::activate () {
    kdDebug () << "SMIL::TimedMrl(" << nodeName() << ")::activate" << endl;
    setState (state_activated);
    ElementRuntimePtr rt = getRuntime ();
    if (rt) {
        rt->init ();
        rt->begin ();
    }
}

KDE_NO_EXPORT void SMIL::TimedMrl::deactivate () {
    Mrl::deactivate ();
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
KDE_NO_EXPORT void SMIL::TimedMrl::childDone (NodePtr c) {
    if (c->nextSibling ())
        c->nextSibling ()->activate ();
    else { // check if Runtime still running
        TimedRuntime * tr = static_cast <TimedRuntime *> (getRuntime ().ptr ());
        if (tr && tr->state () < TimedRuntime::timings_stopped) {
            if (tr->state () == TimedRuntime::timings_started)
                tr->propagateStop (false);
            return; // still running, wait for runtime to finish
        }
        deactivate ();
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

KDE_NO_EXPORT void SMIL::TimedElement::activate () {
    setState (state_activated);
    ElementRuntimePtr rt = getRuntime ();
    rt->init ();
    rt->begin ();
}

KDE_NO_EXPORT void SMIL::TimedElement::deactivate () {
    Element::deactivate ();
}

KDE_NO_EXPORT void SMIL::TimedElement::reset () {
    getRuntime ()->end ();
}

//-----------------------------------------------------------------------------

KDE_NO_EXPORT bool SMIL::GroupBase::isMrl () {
    return false;
}

KDE_NO_EXPORT ElementRuntimePtr SMIL::GroupBase::getNewRuntime () {
    return (new TimedRuntime (m_self))->self ();
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
        return elm->self ();
    return NodePtr ();
}

KDE_NO_EXPORT void SMIL::Par::activate () {
    kdDebug () << "SMIL::Par::activate" << endl;
    GroupBase::activate (); // calls init() and begin() on runtime
}

KDE_NO_EXPORT void SMIL::Par::deactivate () {
    kdDebug () << "SMIL::Par::deactivate" << endl;
    setState (state_deactivated); // prevent recursion via childDone
    TimedRuntime * tr = static_cast <TimedRuntime *> (getRuntime ().ptr ());
    if (tr && tr->state () == TimedRuntime::timings_started) {
        tr->propagateStop (true);
        return; // wait for runtime to call deactivate()
    }
    GroupBase::deactivate ();
}

KDE_NO_EXPORT void SMIL::Par::reset () {
    GroupBase::reset ();
    for (NodePtr e = firstChild (); e; e = e->nextSibling ())
        e->reset ();
}

KDE_NO_EXPORT void SMIL::Par::childDone (NodePtr) {
    if (state != state_deactivated) {
        kdDebug () << "SMIL::Par::childDone" << endl;
        for (NodePtr e = firstChild (); e; e = e->nextSibling ()) {
            if (e->state != state_deactivated)
                return; // not all done
        }
        TimedRuntime * tr = static_cast <TimedRuntime *> (getRuntime ().ptr ());
        if (tr && tr->state () == TimedRuntime::timings_started) {
            if (tr->durations[(int)TimedRuntime::duration_time].durval == duration_media)
                tr->propagateStop (false);
            return; // still running, wait for runtime to finish
        }
        deactivate (); // we're done
    }
}

KDE_NO_EXPORT ElementRuntimePtr SMIL::Par::getNewRuntime () {
    return (new ParRuntime (m_self))->self ();
}
//-----------------------------------------------------------------------------

KDE_NO_EXPORT NodePtr SMIL::Seq::childFromTag (const QString & tag) {
    Element * elm = fromScheduleGroup (m_doc, tag);
    if (!elm) elm = fromMediaContentGroup (m_doc, tag);
    if (!elm) elm = fromContentControlGroup (m_doc, tag);
    if (!elm) elm = fromAnimateGroup (m_doc, tag);
    if (elm)
        return elm->self ();
    return NodePtr ();
}

KDE_NO_EXPORT void SMIL::Seq::activate () {
    GroupBase::activate ();
    Element::activate ();
}

//-----------------------------------------------------------------------------

KDE_NO_EXPORT NodePtr SMIL::Excl::childFromTag (const QString & tag) {
    Element * elm = fromScheduleGroup (m_doc, tag);
    if (!elm) elm = fromMediaContentGroup (m_doc, tag);
    if (!elm) elm = fromContentControlGroup (m_doc, tag);
    if (!elm) elm = fromAnimateGroup (m_doc, tag);
    if (elm)
        return elm->self ();
    return NodePtr ();
}

KDE_NO_EXPORT void SMIL::Excl::activate () {
    kdDebug () << "SMIL::Excl::activate" << endl;
    setState (state_activated);
    TimedRuntime * tr = static_cast <TimedRuntime *> (getRuntime ().ptr ());
    if (tr)
        tr->init ();
    if (tr && firstChild ()) { // init children
        for (NodePtr e = firstChild (); e; e = e->nextSibling ())
            e->activate ();
        tr->begin (); // sets up aboutToStart connection with children
    } else { // no children, deactivate if runtime started and no duration set
        if (tr && tr->state () == TimedRuntime::timings_started) {
            if (tr->durations[(int)TimedRuntime::duration_time].durval == duration_media)
                tr->propagateStop (false);
            return; // still running, wait for runtime to finish
        }
        deactivate (); // no children to run
    }
}

KDE_NO_EXPORT void SMIL::Excl::childDone (NodePtr /*child*/) {
    // do nothing
}

KDE_NO_EXPORT ElementRuntimePtr SMIL::Excl::getNewRuntime () {
    return (new ExclRuntime (m_self))->self ();
}

//-----------------------------------------------------------------------------

KDE_NO_EXPORT NodePtr SMIL::Switch::childFromTag (const QString & tag) {
    Element * elm = fromContentControlGroup (m_doc, tag);
    if (!elm) elm = fromMediaContentGroup (m_doc, tag);
    if (elm)
        return elm->self ();
    return NodePtr ();
}

KDE_NO_EXPORT void SMIL::Switch::activate () {
    kdDebug () << "SMIL::Switch::activate" << endl;
    setState (state_activated);
    if (firstChild ())
        firstChild ()->activate (); // activate only the first for now FIXME: condition
    else
        deactivate ();
}

KDE_NO_EXPORT void SMIL::Switch::deactivate () {
    Element::deactivate ();
    for (NodePtr e = firstChild (); e; e = e->nextSibling ())
        if (e->state == state_activated) {
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

KDE_NO_EXPORT void SMIL::Switch::childDone (NodePtr) {
    kdDebug () << "SMIL::Switch::childDone" << endl;
    deactivate (); // only one child can run
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT SMIL::MediaType::MediaType (NodePtr &d, const QString &t)
    : TimedMrl (d), m_type (t), bitrate (0) {}

KDE_NO_EXPORT NodePtr SMIL::MediaType::childFromTag (const QString & tag) {
    Element * elm = fromContentControlGroup (m_doc, tag);
    if (!elm) elm = fromParamGroup (m_doc, tag);
    if (!elm) elm = fromAnimateGroup (m_doc, tag);
    if (elm)
        return elm->self ();
    return NodePtr ();
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
    ElementRuntimePtr rt = getRuntime ();
    if (rt) {
        rt->init (); // sets all attributes
        if (firstChild ())
            firstChild ()->activate ();
        rt->begin ();
    }
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT
SMIL::AVMediaType::AVMediaType (NodePtr & d, const QString & t)
 : SMIL::MediaType (d, t) {}

KDE_NO_EXPORT void SMIL::AVMediaType::activate () {
    if (!isMrl ()) { // turned out this URL points to a playlist file
        Element::activate ();
        return;
    }
    kdDebug () << "SMIL::AVMediaType::activate" << endl;
    NodePtr p = parentNode ();
    while (p && strcmp (p->nodeName (), "smil"))
        p = p->parentNode ();
    if (p) { // this works only because we can only play one at a time FIXME
        convertNode <Smil> (p)->current_av_media_type = m_self;
        MediaType::activate ();
    } else {
        kdError () << nodeName () << " playing and current is not Smil" << endl;
        deactivate ();
    }
}

KDE_NO_EXPORT void SMIL::AVMediaType::deactivate () {
    TimedMrl::deactivate ();
    TimedRuntime * tr = static_cast <TimedRuntime *> (getRuntime ().ptr ());
    if (tr && tr->state () == TimedRuntime::timings_started)
        tr->propagateEvent ((new Event (event_stopped))->self ()); // called from backends
    // TODO stop backend player
}

KDE_NO_EXPORT ElementRuntimePtr SMIL::AVMediaType::getNewRuntime () {
    return (new AudioVideoData (m_self))->self ();
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT
SMIL::ImageMediaType::ImageMediaType (NodePtr & d)
    : SMIL::MediaType (d, "img") {}

KDE_NO_EXPORT ElementRuntimePtr SMIL::ImageMediaType::getNewRuntime () {
    isMrl (); // hack to get relative paths right
    return (new ImageData (m_self))->self ();
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT
SMIL::TextMediaType::TextMediaType (NodePtr & d)
    : SMIL::MediaType (d, "text") {}

KDE_NO_EXPORT ElementRuntimePtr SMIL::TextMediaType::getNewRuntime () {
    isMrl (); // hack to get relative paths right
    return (new TextData (m_self))->self ();
}

//-----------------------------------------------------------------------------

KDE_NO_EXPORT ElementRuntimePtr SMIL::Set::getNewRuntime () {
    return (new SetData (m_self))->self ();
}

KDE_NO_EXPORT void SMIL::Set::activate () {
    TimedElement::activate ();
    deactivate (); // no livetime of itself
}

//-----------------------------------------------------------------------------

KDE_NO_EXPORT ElementRuntimePtr SMIL::Animate::getNewRuntime () {
    return (new AnimateData (m_self))->self ();
}

//-----------------------------------------------------------------------------

KDE_NO_EXPORT void SMIL::Param::activate () {
    QString name = getAttribute ("name");
    if (!name.isEmpty () && parentNode ()) {
        ElementRuntimePtr rt = parentNode ()->getRuntime ();
        if (rt)
            rt->setParam (name, getAttribute ("value"));
    }
    deactivate (); // no livetime of itself
}

//-----------------------------------------------------------------------------

namespace KMPlayer {
    class ImageDataPrivate {
        public:
            ImageDataPrivate () : image (0L), cache_image (0) {}
            ~ImageDataPrivate () {
                delete image;
                delete cache_image;
            }
            QPixmap * image;
            QPixmap * cache_image; // scaled cache
            int olddur;
    };
}

KDE_NO_CDTOR_EXPORT ImageData::ImageData (NodePtr e)
 : MediaTypeRuntime (e), d (new ImageDataPrivate) {
}

KDE_NO_CDTOR_EXPORT ImageData::~ImageData () {
    delete d;
}

KDE_NO_EXPORT
QString ImageData::setParam (const QString & name, const QString & val) {
    //kdDebug () << "ImageData::param " << name << "=" << val << endl;
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
                    delete d->cache_image;
                    d->cache_image = 0;
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
        int xoff, yoff, w = r->w, h = r->h;
        calcSizes (w, h, xoff, yoff, w, h);
        xoff = int (xoff * r->xscale);
        yoff = int (yoff * r->yscale);
        if (fit == fit_hidden) {
            w = int (d->image->width () * r->xscale);
            h = int (d->image->height () * r->yscale);
        } else if (fit == fit_meet) { // scale in region, keeping aspects
            if (h > 0 && d->image->height () > 0) {
                int a = 100 * d->image->width () / d->image->height ();
                int w1 = a * h / 100;
                if (w1 > w)
                    h = 100 * w / a;
                else
                    w = w1;
            }
        } else if (fit == fit_slice) { // scale in region, keeping aspects
            if (h > 0 && d->image->height () > 0) {
                int a = 100 * d->image->width () / d->image->height ();
                int w1 = a * h / 100;
                if (w1 > w)
                    w = w1;
                else
                    h = 100 * w / a;
            }
        } //else if (fit == fit_fill) { // scale in region
        // else fit_scroll
        if (w == d->image->width () && h == d->image->height ())
            p.drawPixmap (QRect (r->x+xoff, r->y+yoff, w, h), *d->image);
        else {
            if (!d->cache_image || w != d->cache_image->width () || h != d->cache_image->height ()) {
                delete d->cache_image;
                QImage img;
                img = *d->image;
                d->cache_image = new QPixmap (img.scale (w, h));
            }
            p.drawPixmap (QRect (r->x+xoff, r->y+yoff, w, h), *d->cache_image);
        }
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
            delete d->cache_image;
            d->cache_image = 0;
            if (region_node && (timingstate == timings_started ||
                 (timingstate == timings_stopped && fill == fill_freeze)))
                region_node->repaint ();
        } else
            delete pix;
    }
    if (timingstate == timings_started &&
            durations [duration_time].durval == duration_data_download) {
        durations [duration_time].durval = d->olddur;
        propagateStart ();
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
            font_size = font.pointSize ();
            transparent = false;
        }
        QByteArray data;
        unsigned int background_color;
        unsigned int foreground_color;
        int olddur;
        QTextCodec * codec;
        QFont font;
        int font_size;
        bool transparent;
    };
}

KDE_NO_CDTOR_EXPORT TextData::TextData (NodePtr e)
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
    //kdDebug () << "TextData::setParam " << name << "=" << val << endl;
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
        d->font_size = val.toInt ();
    } else if (name == QString ("fontSize")) {
        d->font_size += val.toInt ();
    // TODO: expandTabs fontBackgroundColor fontSize fontStyle fontWeight hAlig vAlign wordWrap
    } else
        return MediaTypeRuntime::setParam (name, val);
    RegionNode * rn = region_node.ptr ();
    if (rn && (timingstate == timings_started ||
                (timingstate == timings_stopped && fill == fill_freeze)))
        rn->repaint ();
    return ElementRuntime::setParam (name, val);
}

KDE_NO_EXPORT void TextData::paint (QPainter & p) {
    if (region_node && (timingstate == timings_started ||
                (timingstate == timings_stopped && fill == fill_freeze))) {
        RegionNode * r = region_node.ptr ();
        int xoff, yoff, w = r->w, h = r->h;
        calcSizes (w, h, xoff, yoff, w, h);
        int x = r->x + int (xoff * r->xscale);
        int y = r->y + int (yoff * r->yscale);
        if (!d->transparent)
            p.fillRect (x, y, w, h, QColor (QRgb (d->background_color)));
        d->font.setPointSize (int (r->xscale * d->font_size));
        QFontMetrics metrics (d->font);
        QPainter::TextDirection direction = QApplication::reverseLayout () ?
            QPainter::RTL : QPainter::LTR;
        if (direction == QPainter::RTL)
            x += w;
        yoff = metrics.lineSpacing ();
        p.setFont (d->font);
        p.setPen (QRgb (d->foreground_color));
        QTextStream text (d->data, IO_ReadOnly);
        if (d->codec)
            text.setCodec (d->codec);
        QString line = text.readLine (); // FIXME word wrap
        while (!line.isNull () && yoff < h) {
            p.drawText (x, y+yoff, line, w, direction);
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
        propagateStart ();
    }
}

#include "kmplayer_smil.moc"
