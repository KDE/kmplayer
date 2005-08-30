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
 *  the Free Software Foundation, Inc., 51 Franklin Steet, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 **/

#include <config.h>
#include <qtextstream.h>
#include <qcolor.h>
#include <qpainter.h>
#include <qpixmap.h>
#include <qmovie.h>
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
}

/* Intrinsic duration 
 *  duration_time   |    end_time    |
 *  =======================================================================
 *  duration_media  | duration_media | wait for event
 *       0          | duration_media | only wait for child elements
 */
//-----------------------------------------------------------------------------

static SMIL::Region * findRegion (NodePtr p, const QString & id) {
    for (NodePtr c = p->firstChild (); c; c = c->nextSibling ()) {
        if (c->id == SMIL::id_node_region) {
            SMIL::Region * r = convertNode <SMIL::Region> (c);
            QString a = r->getAttribute ("id");
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

ElementRuntimePtr Node::getRuntime () {
    return ElementRuntimePtr ();
}

//-----------------------------------------------------------------------------

namespace KMPlayer {
    struct ParamValue {
        QString val;
        QStringList  * modifications;
        ParamValue (const QString & v) : val (v), modifications (0L) {}
        ~ParamValue () { delete modifications; }
        QString value () { return modifications ? modifications->back() : val; }
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
                    pv->modifications->back () == QString::null)
                pv->modifications->pop_back ();
        }
        if (pv->modifications->size () == 0) {
            delete pv->modifications;
            pv->modifications = 0L;
            if (pv->value () == QString::null) {
                delete pv;
                d->params.remove (name);
                return;
            }
        }
        parseParam (name, pv->value ());
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
    region_node = 0L;
    d->clear ();
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT ToBeStartedEvent::ToBeStartedEvent (NodePtr n)
 : Event (event_to_be_started), node (n) {}

KDE_NO_CDTOR_EXPORT
PaintEvent::PaintEvent (QPainter & p, int _x, int _y, int _w, int _h)
 : Event (event_paint), painter (p), x (_x), y (_y), w (_w), h (_h) {}

KDE_NO_CDTOR_EXPORT SizeEvent::SizeEvent(int _x, int _y, int _w, int _h, bool b)
 : Event (event_sized), x (_x), y (_y), w (_w), h (_h), keep_ratio (b) {}

PointerEvent::PointerEvent (unsigned int event_id, int _x, int _y)
 : Event (event_id), x (_x), y (_y) {}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT TimedRuntime::TimedRuntime (NodePtr e)
 : ElementRuntime (e) {
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
        durations [i].durval = 0;
    }
    durations [end_time].durval = duration_media;
    ElementRuntime::reset ();
}

KDE_NO_EXPORT
void TimedRuntime::setDurationItem (DurationTime item, const QString & val) {
    unsigned int dur = 0; // also 0 for 'media' duration, so it will not update then
    QRegExp reg ("^\\s*([0-9\\.]+)\\s*([a-z]*)");
    QString vl = val.lower ();
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
    //kdDebug () << "TimedRuntime::end " << (element ? element->nodeName() : "-") << endl; 
    if (region_node)
        region_node = 0L;
    killTimers ();
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
        else if (durations [end_time].durval > duration_last_option)
            durations [duration_time].durval = duration_media; // event
    } else if (name == QString::fromLatin1 ("endsync")) {
        if ((durations [duration_time].durval == duration_media ||
                    durations [duration_time].durval == 0) &&
                durations [end_time].durval == duration_media) {
            NodePtr e = element->document ()->getElementById (val);
            if (e) {
                SMIL::TimedMrl * tm = dynamic_cast <SMIL::TimedMrl *> (e.ptr());
                if (tm) {
                    durations [(int) end_time].connection = tm->connectTo (element, event_stopped);
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
    ElementRuntime::parseParam (name, val);
}

KDE_NO_EXPORT void TimedRuntime::timerEvent (QTimerEvent * e) {
    //kdDebug () << "TimedRuntime::timerEvent " << (element ? element->nodeName() : "-") << endl; 
    if (e->timerId () == start_timer) {
        killTimer (start_timer);
        start_timer = 0;
        propagateStart ();
    } else if (e->timerId () == dur_timer)
        propagateStop (true);
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
    if (dur_timer) {
        killTimer (dur_timer);
        dur_timer = 0;
    }
    if (timingstate == timings_started)
        QTimer::singleShot (0, this, SLOT (stopped ()));
    timingstate = timings_stopped;
}

KDE_NO_EXPORT void TimedRuntime::propagateStart () {
    SMIL::TimedMrl * tm = convertNode <SMIL::TimedMrl> (element);
    if (tm)
        tm->propagateEvent ((new ToBeStartedEvent (element))->self ());
    timingstate = timings_started;
    QTimer::singleShot (0, this, SLOT (started ()));
}

/**
 * start_timer timer expired
 */
KDE_NO_EXPORT void TimedRuntime::started () {
    //kdDebug () << "TimedRuntime::started " << (element ? element->nodeName() : "-") << endl; 
    if (durations [duration_time].durval > 0) {
        if (durations [duration_time].durval < duration_last_option) {
            dur_timer = startTimer (100 * durations [duration_time].durval);
     //       kdDebug () << "TimedRuntime::started set dur timer " << durations [duration_time].durval << endl;
        }
        element->begin ();
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
    } else if (element->active ()) {
        element->finish ();
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
    percentage = false;
    QString strval (s);
    int p = strval.find (QChar ('%'));
    if (p > -1) {
        percentage = true;
        strval.truncate (p);
    }
    bool ok;
    int size = int (strval.toDouble (&ok));
    if (ok) {
        m_size = size;
        isset = true;
    }
    return *this;
}

int SizeType::size (int relative_to) {
    if (percentage)
        return m_size * relative_to / 100;
    return m_size;
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

static bool regPoints (const QString & str, int & x, int & y) {
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

KDE_NO_EXPORT void CalculatedSizer::calcSizes (Node * node, int w, int h, int & xoff, int & yoff, int & w1, int & h1) {
    while (!reg_point.isEmpty ()) {
        int rpx, rpy, rax, ray;
        if (!regPoints (reg_point, rpx, rpy)) {
            while (node && node->id != SMIL::id_node_smil)
                node = node->parentNode ().ptr ();
            if (!node)
                break;
            node = static_cast <SMIL::Smil *> (node)->layout_node.ptr ();
            if (!node)
                break;
            NodePtr c = node->firstChild ();
            for (; c; c = c->nextSibling ()) {
                if (!c->isElementNode ())
                    continue;
                if (c->id == SMIL::id_node_regpoint && convertNode <Element> (c)->getAttribute ("id") == reg_point) {
                    RegPointRuntime *rprt = static_cast <RegPointRuntime*> (c->getRuntime ().ptr ());
                    if (rprt) {
                        int i1, i2; // dummies
                        rprt->sizes.calcSizes (0L, 100, 100, rpx, rpy, i1, i2);
                        QString ra = convertNode <Element> (c)->getAttribute ("regAlign");
                        if (!ra.isEmpty () && reg_align.isEmpty ())
                            reg_align = ra;
                        break;
                    }
                }
            }
            if (!c)
                break; // not found
        }
        if (!regPoints (reg_align, rax, ray))
            rax = ray = 0; // default back to topLeft
        xoff = w * (rpx - rax) / 100;
        yoff = h * (rpy - ray) / 100;
        w1 = w - w * (rpx > rax ? (rpx - rax) : (rax - rpx)) / 100;
        h1 = h - h * (rpy > ray ? (rpy - ray) : (ray - rpy)) / 100;
        // kdDebug () << "calc rp:" << reg_point << " ra:" << reg_align << " xoff:" << xoff << " yoff:" << yoff << " w1:" << w1 << " h1:" << h1 << endl;
        return; // success getting sizes based on regPoint
    }
    xoff = left.size (w);
    yoff = top.size (h);
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

KDE_NO_EXPORT bool CalculatedSizer::setSizeParam (const QString & name, const QString & val) {
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
    region_node = e;
    init ();
}

KDE_NO_EXPORT void RegionRuntime::reset () {
    // Keep region_node, so no ElementRuntime::reset (); or set it back again
    have_bg_color = false;
    active = false;
    sizes.resetSizes ();
}

KDE_NO_EXPORT
void RegionRuntime::parseParam (const QString & name, const QString & val) {
    //kdDebug () << "RegionRuntime::parseParam " << convertNode <Element> (element)->getAttribute ("id") << " " << name << "=" << val << endl;
    SMIL::RegionBase * rb = convertNode <SMIL::RegionBase> (region_node);
    QRect rect;
    bool need_repaint = false;
    if (rb) {
        Matrix m = rb->transform ();
        int rx = 0, ry = 0, rw = rb->w, rh = rb->h;
        m.getXYWH (rx, ry, rw, rh);
        rect = QRect (rx, ry, rw, rh);
    }
    if (name == QString::fromLatin1 ("background-color") ||
            name == QString::fromLatin1 ("background-color")) {
        background_color = QColor (val).rgb ();
        have_bg_color = true;
        need_repaint = true;
    } else if (name == QString::fromLatin1 ("z-index")) {
        if (rb)
            rb->z_order = val.toInt ();
        need_repaint = true;
    } else if (sizes.setSizeParam (name, val)) {
        if (active && rb && element) {
            SMIL::RegionBase * pr = dynamic_cast<SMIL::RegionBase*>(rb->parentNode().ptr());
            if (pr)
                pr->calculateChildBounds ();
            else {
                for (NodePtr e = element; e; e = e->parentNode ())
                    if (e->id == SMIL::id_node_layout) {
                        convertNode<SMIL::Layout> (e)->updateLayout ();
                        break;
                    }
            }
            Matrix m = rb->transform ();
            int rx = 0, ry = 0, rw = rb->w, rh = rb->h;
            m.getXYWH (rx, ry, rw, rh);
            if (rect.width () == rw && rect.height () == rh) {
                PlayListNotify * n = element->document()->notify_listener;
                if (n && (rect.x () != rx || rect.y () != ry))
                    n->moveRect (rect.x(), rect.y(), rect.width (), rect.height (), rx, ry);
            } else {
                rect = rect.unite (QRect (rx, ry, rw, rh));
                need_repaint = true;
            }
        }
    }
    if (need_repaint && active && rb && element) {
        PlayListNotify * n = element->document()->notify_listener;
        if (n)
            n->repaintRect (rect.x(), rect.y(), rect.width (), rect.height ());
    }
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

KDE_NO_EXPORT void AnimateGroupData::begin () {
    if (durations [begin_time].durval > duration_last_option) {
        if (element)
            element->finish ();
    } else
        TimedRuntime::begin ();
}

/**
 * animation finished
 */
KDE_NO_EXPORT void AnimateGroupData::stopped () {
    //kdDebug () << "SetData::stopped " << durations [duration_time].durval << endl;
    if (fill == fill_freeze)
        ; // keep it 
    else
        restoreModification ();
    TimedRuntime::stopped ();
}

KDE_NO_EXPORT void AnimateGroupData::reset () {
    restoreModification ();
}

KDE_NO_EXPORT void AnimateGroupData::restoreModification () {
    if (modification_id > -1 && target_element) {
        ElementRuntimePtr rt = target_element->getRuntime ();
        if (rt) {
            //kdDebug () << "AnimateGroupData::restoreModificatio " <<modification_id << endl;
            rt->resetParam (changed_attribute, modification_id);
            if (target_region)
                convertNode <SMIL::RegionBase> (target_region)->repaint ();
        }
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
            ElementRuntimePtr rt = target_element->getRuntime ();
            if (rt) {
                target_region = rt->region_node;
                rt->setParam (changed_attribute, change_to, &modification_id);
                //kdDebug () << "SetData::started " << target_element->nodeName () << "." << changed_attribute << " " << old_value << "->" << change_to << endl;
                if (target_region)
                    convertNode <SMIL::RegionBase> (target_region)->repaint ();
            }
        } else
            kdWarning () << "target element not found" << endl;
    } else
        kdWarning () << "set element disappeared" << endl;
    AnimateGroupData::started ();
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
            steps = 10 * durations [duration_time].durval / 2; // 50 per sec
            if (steps > 0) {
                anim_timer = startTimer (20); // 50 /s for now FIXME
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
            anim_timer = startTimer (interval);
            rt->setParam (changed_attribute, change_values.first (), &modification_id);
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
    // kdDebug () << "AnimateData::stopped " << element->state << endl;
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
                rt->setParam (changed_attribute, QString ("%1%2").arg (change_from_val).arg(change_from_unit), &modification_id);
            } else if (calcMode == calc_discrete) {
                 rt->setParam (changed_attribute, change_values[change_values.size () - steps -1], &modification_id);
            }
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
    //kdDebug () << "MediaTypeRuntime::wget " << url.url () << endl;
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
void MediaTypeRuntime::parseParam (const QString & name, const QString & val) {
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
    } else if (!sizes.setSizeParam (name, val)) {
        TimedRuntime::parseParam (name, val);
        return;
    }
    SMIL::RegionBase * rb = convertNode <SMIL::RegionBase> (region_node);
    if (state () == timings_began && rb && element)
        rb->repaint ();
    ElementRuntime::parseParam (name, val);
}

/**
 * find region node and request a repaint of attached region then
 */
KDE_NO_EXPORT void MediaTypeRuntime::started () {
    NodePtr e = element;
    for (; e; e = e->parentNode ())
        if (e->id == SMIL::id_node_smil) {
            e = convertNode <SMIL::Smil> (e)->layout_node;
            break;
        }
    if (e) {
        SMIL::Region * r = findRegion (e, param(QString::fromLatin1("region")));
        if (r) {
            region_node = r->self ();
            r->addRegionUser (element);
            r->repaint ();
        } else
            kdWarning () << "MediaTypeRuntime::started no region found" << endl;
    }
    TimedRuntime::started ();
}

/**
 * will request a repaint of attached region
 */
KDE_NO_EXPORT void MediaTypeRuntime::stopped () {
    if (region_node)
        convertNode <SMIL::RegionBase> (region_node)->repaint ();
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
    if (durations [duration_time].durval == 0 &&
            durations [end_time].durval == duration_media)
        durations [duration_time].durval = duration_media; // duration of clip
    MediaTypeRuntime::started ();
    if (element) {
        for (NodePtr r = region_node; r; r = r->parentNode ())
            if (r->id == SMIL::id_node_layout) {
                sized_connection = r->connectTo (element, event_sized);
                break;
            }
        //kdDebug () << "AudioVideoData::started " << source_url << endl;
        PlayListNotify * n = element->document ()->notify_listener;
        if (n && !source_url.isEmpty ()) {
            convertNode <SMIL::AVMediaType> (element)->positionVideoWidget ();
            for (NodePtr e = element; e; e = e->parentNode ())
                if (e->id == SMIL::id_node_smil) {
                    n->requestPlayURL (e); // keep <smil> current
                    break;
                }
            element->setState (Element::state_began);
        }
    }
}

KDE_NO_EXPORT void AudioVideoData::stopped () {
    //kdDebug () << "AudioVideoData::stopped " << endl;
    sized_connection = 0L;
    MediaTypeRuntime::stopped ();
}

KDE_NO_EXPORT
void AudioVideoData::parseParam (const QString & name, const QString & val) {
    //kdDebug () << "AudioVideoData::parseParam " << name << "=" << val << endl;
    MediaTypeRuntime::parseParam (name, val);
    if (name == QString::fromLatin1 ("src")) {
        if (timingstate == timings_started && element) {
            PlayListNotify * n = element->document ()->notify_listener;
            if (n && !source_url.isEmpty ()) {
                for (NodePtr e = element; e; e = e->parentNode ())
                    if (e->id == SMIL::id_node_smil) {
                        n->requestPlayURL (e); // keep <smil> current
                        break;
                    }
                element->setState (Node::state_began);
            }
        }
    }
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

KDE_NO_EXPORT NodePtr SMIL::Smil::childFromTag (const QString & tag) {
    if (!strcmp (tag.latin1 (), "body"))
        return (new SMIL::Body (m_doc))->self ();
    else if (!strcmp (tag.latin1 (), "head"))
        return (new SMIL::Head (m_doc))->self ();
    return NodePtr ();
}

static void beginOrEndLayout (Node * node, bool b) {
    ElementRuntime * rt = node ? node->getRuntime ().ptr () : 0L;
    if (rt) { // note rb can be a Region/Layout/RootLayout object
        if (b) {
            rt->init ();
            rt->begin ();
        } else
            rt->end ();
        for (NodePtr c = node->firstChild (); c; c = c->nextSibling ())
            beginOrEndLayout (c.ptr (), b);
    }
}

KDE_NO_EXPORT void SMIL::Smil::activate () {
    //kdDebug () << "Smil::activate" << endl;
    current_av_media_type = NodePtr ();
    PlayListNotify * n = document()->notify_listener;
    if (n) {
        n->setEventDispatcher (layout_node);
        n->setCurrent (m_self);
    }
    Element::activate ();
}

KDE_NO_EXPORT void SMIL::Smil::deactivate () {
    if (layout_node) {
        SMIL::Layout * rb = convertNode <SMIL::Layout> (layout_node);
        if (rb) {
            beginOrEndLayout (rb, false);
            rb->repaint ();
        }
    }
    PlayListNotify * n = document()->notify_listener;
    if (n)
        n->setEventDispatcher (NodePtr ());
    Mrl::deactivate ();
}

KDE_NO_EXPORT void SMIL::Smil::closed () {
    width = 320; // something to start with
    height = 240;
    NodePtr head;
    for (NodePtr e = firstChild (); e; e = e->nextSibling ())
        if (e->id == id_node_head) {
            head = e;
            break;
        }
    if (!head) {
        SMIL::Head * h = new SMIL::Head (m_doc);
        insertBefore (h->self (), firstChild ());
        h->setAuxiliaryNode (true);
        h->closed ();
        head = h->self ();
    }
    for (NodePtr e = head->firstChild (); e; e = e->nextSibling ()) {
        if (e->id == id_node_layout) {
            layout_node = e;
        } else if (e->id == id_node_title) {
            QString str = e->innerText ();
            pretty_name = str.left (str.find (QChar ('\n')));
        }
    }
    if (!layout_node) {
        kdError () << "no <root-layout>" << endl;
        return;
    }
    SMIL::RegionBase * rb = convertNode <SMIL::RegionBase> (layout_node);
    if (rb) {
        width = rb->w;
        height = rb->h;
    }
}

KDE_NO_EXPORT NodePtr SMIL::Smil::realMrl () {
    return current_av_media_type ? current_av_media_type : m_self;
}

KDE_NO_EXPORT bool SMIL::Smil::isMrl () {
    return true;
}

KDE_NO_EXPORT bool SMIL::Smil::expose () const {
    return !pretty_name.isEmpty () || //return false if no title and only one
        previousSibling () || nextSibling ();
}

//-----------------------------------------------------------------------------

KDE_NO_EXPORT NodePtr SMIL::Head::childFromTag (const QString & tag) {
    if (!strcmp (tag.latin1 (), "layout"))
        return (new SMIL::Layout (m_doc))->self ();
    else if (!strcmp (tag.latin1 (), "title"))
        return (new DarkNode (m_doc, tag, id_node_title))->self ();
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
    appendChild (layout->self ());
    layout->setAuxiliaryNode (true);
    layout->closed (); // add root-layout and a region
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT SMIL::Layout::Layout (NodePtr & d)
 : RegionBase (d, id_node_layout),
   m_SizeListeners ((new NodeRefList)->self ()) {}

KDE_NO_EXPORT NodePtr SMIL::Layout::childFromTag (const QString & tag) {
    if (!strcmp (tag.latin1 (), "root-layout")) {
        NodePtr e = (new SMIL::RootLayout (m_doc))->self ();
        rootLayout = e;
        return e;
    } else if (!strcmp (tag.latin1 (), "region"))
        return (new SMIL::Region (m_doc))->self ();
    else if (!strcmp (tag.latin1 (), "regPoint"))
        return (new SMIL::RegPoint (m_doc))->self ();
    return NodePtr ();
}

KDE_NO_EXPORT void SMIL::Layout::closed () {
    SMIL::RegionBase * smilroot = convertNode <SMIL::RootLayout> (rootLayout);
    bool has_root (smilroot);
    if (!has_root) { // just add one if none there
        smilroot = new SMIL::RootLayout (m_doc);
        NodePtr sr = smilroot->self (); // protect against destruction
        smilroot->setAuxiliaryNode (true);
        rootLayout = smilroot->self ();
        int w_root =0, h_root = 0, reg_count = 0;
        for (NodePtr n = firstChild (); n; n = n->nextSibling ()) {
            if (n->id == id_node_region) {
                SMIL::Region * rb =convertNode <SMIL::Region> (n);
                ElementRuntimePtr rt = rb->getRuntime ();
                static_cast <RegionRuntime *> (rt.ptr ())->init ();
                rb->calculateBounds (0, 0);
                if (rb->x + rb->w > w_root)
                    w_root = rb->x + rb->w;
                if (rb->y + rb->h > h_root)
                    h_root = rb->y + rb->h;
                reg_count++;
            }
        }
        if (!reg_count) {
            w_root = 320; h_root = 240; // have something to start with
            SMIL::Region * r = new SMIL::Region (m_doc);
            appendChild (r->self ());
            r->setAuxiliaryNode (true);
        }
        smilroot->setAttribute ("width", QString::number (w_root));
        smilroot->setAttribute ("height", QString::number (h_root));
        insertBefore (sr, firstChild ());
    } else if (childNodes ()->length () < 2) { // only a root-layout
        SMIL::Region * r = new SMIL::Region (m_doc);
        appendChild (r->self ());
        r->setAuxiliaryNode (true);
    }
    updateLayout ();
    if (w <= 0 || h <= 0) {
        kdError () << "Root layout not having valid dimensions" << endl;
        return;
    }
}

KDE_NO_EXPORT void SMIL::Layout::activate () {
    //kdDebug () << "SMIL::Layout::activate" << endl;
    setState (state_activated);
    beginOrEndLayout (this, true);
    updateLayout ();
    repaint ();
    finish (); // that's fast :-)
}

KDE_NO_EXPORT void SMIL::Layout::updateLayout () {
    x = y = 0;
    ElementRuntimePtr rt = rootLayout->getRuntime ();
    if (rt) {
        RegionRuntime * rr = static_cast <RegionRuntime *> (rt.ptr ());
        w = rr->sizes.width.size ();
        h = rr->sizes.height.size ();
        //kdDebug () << "RegionBase::updateLayout " << w << "," << h << endl;
        calculateChildBounds ();
    }
}

KDE_NO_EXPORT bool SMIL::Layout::handleEvent (EventPtr event) {
    bool handled = false;
    switch (event->id ()) {
        case event_paint:
            if (rootLayout) {
                PaintEvent * p = static_cast <PaintEvent*>(event.ptr ());
                RegionRuntime * rr = static_cast <RegionRuntime *> (rootLayout->getRuntime ().ptr ());
                if (rr && rr->have_bg_color) {
                    Matrix m = transform ();
                    int rx = 0, ry = 0, rw = w, rh = h;
                    m.getXYWH (rx, ry, rw, rh);
                    p->painter.fillRect (rx, ry, rw, rh, QColor (QRgb (rr->background_color)));
                }
            }
            return RegionBase::handleEvent (event);
        case event_sized: {
            SizeEvent * e = static_cast <SizeEvent *> (event.ptr ());
            float xscale = 1.0, yscale = 1.0;
            if (w > 0 && h > 0) {
                xscale += 1.0 * (e->w - w) / w;
                yscale += 1.0 * (e->h - h) / h;
                if (e->keep_ratio)
                    if (xscale > yscale) {
                        xscale = yscale;
                        e->x = (e->w - int ((xscale - 1.0) * w + w)) / 2;
                    } else {
                        yscale = xscale;
                        e->y = (e->h - int ((yscale - 1.0) * h + h)) / 2;
                    }
            }
            m_transform = Matrix (0, 0, xscale, yscale);
            m_transform.translate (e->x, e->y);
            propagateEvent (event); // trigger size updates for av widgets
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

NodeRefListPtr SMIL::Layout::listeners (unsigned int eid) {
    if (eid == event_sized)
        return m_SizeListeners;
    return Element::listeners (eid);
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT SMIL::RegionBase::RegionBase (NodePtr & d, short id)
 : Element (d, id), x (0), y (0), w (0), h (0),
   z_order (1),
   m_PaintListeners ((new NodeRefList)->self ()) {}

KDE_NO_EXPORT ElementRuntimePtr SMIL::RegionBase::getRuntime () {
    if (!runtime)
        runtime = ElementRuntimePtr (new RegionRuntime (m_self));
    return runtime;
}

KDE_NO_EXPORT void SMIL::RegionBase::repaint () {
    PlayListNotify * n = document()->notify_listener;
    Matrix m = transform ();
    int rx = 0, ry = 0, rw = w, rh = h;
    m.getXYWH (rx, ry, rw, rh);
    if (n)
        n->repaintRect (rx, ry, rw, rh);
}

KDE_NO_EXPORT void SMIL::RegionBase::calculateChildBounds () {
    for (NodePtr r = firstChild (); r; r = r->nextSibling ())
        if (r->id == id_node_region) {
            SMIL::Region * cr = static_cast <SMIL::Region *> (r.ptr ());
            cr->calculateBounds (w, h);
            cr->calculateChildBounds ();
        }
}

KDE_NO_EXPORT Matrix SMIL::RegionBase::transform () {
    Matrix m (m_transform);
    for (NodePtr p = parentNode ();
            p && (p->id ==SMIL::id_node_region || p->id ==SMIL::id_node_layout);
            p = p->parentNode ())
        m.transform (convertNode <SMIL::RegionBase> (p)->getTransform ());
    return m;
}

bool SMIL::RegionBase::handleEvent (EventPtr event) {
    switch (event->id ()) {
        case event_paint: {// event passed from Region/Layout::handleEvent
            // paint children, accounting for z-order FIXME optimize
            NodeRefList sorted;
            for (NodePtr n = firstChild (); n; n = n->nextSibling ()) {
                if (n->id != id_node_region)
                    continue;
                Region * r = static_cast <Region *> (n.ptr ());
                NodeRefItemPtr rn = sorted.first ();
                for (; rn; rn = rn->nextSibling ())
                    if (r->z_order < static_cast <Region *> (rn->data.ptr ())->z_order) {
                        sorted.insertBefore((new NodeRefItem (n))->self (), rn);
                        break;
                    }
                if (!rn)
                    sorted.append ((new NodeRefItem (n))->self ());
            }
            for (NodeRefItemPtr r = sorted.first (); r; r = r->nextSibling ())
                static_cast <Region *> (r->data.ptr ())->handleEvent (event);
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
    return Element::listeners (eid);
}

KDE_NO_CDTOR_EXPORT SMIL::Region::Region (NodePtr & d)
 : RegionBase (d, id_node_region),
   m_ActionListeners ((new NodeRefList)->self ()),
   m_OutOfBoundsListeners ((new NodeRefList)->self ()),
   m_InBoundsListeners ((new NodeRefList)->self ()),
   has_mouse (false) {}

KDE_NO_EXPORT NodePtr SMIL::Region::childFromTag (const QString & tag) {
    if (!strcmp (tag.latin1 (), "region"))
        return (new SMIL::Region (m_doc))->self ();
    return NodePtr ();
}

/**
 * calculates dimensions of this regions with _w and _h as width and height
 * of parent Region (representing 100%)
 */
KDE_NO_EXPORT void SMIL::Region::calculateBounds (int pw, int ph) {
    ElementRuntimePtr rt = getRuntime ();
    if (rt) {
        RegionRuntime * rr = static_cast <RegionRuntime *> (rt.ptr ());
        rr->sizes.calcSizes (this, pw, ph, x, y, w, h);
        m_transform = Matrix (x, y, 1.0, 1.0);
        //kdDebug () << "Region::calculateBounds " << x << "," << y << " " << w << "x" << h << endl;
    }
}

bool SMIL::Region::handleEvent (EventPtr event) {
    Matrix m = transform ();
    int rx = 0, ry = 0, rw = w, rh = h;
    m.getXYWH (rx, ry, rw, rh);
    switch (event->id ()) {
        case event_paint: {
            PaintEvent * p = static_cast <PaintEvent *> (event.ptr ());
            if (rx + rw < p->x || p->x + p->w < rx ||
                    ry + rh < p->y || p->y + p->h < ry)
                return false;
            p->painter.setClipRect (rx, ry, rw, rh, QPainter::CoordPainter);
        //kdDebug () << "Region::calculateBounds " << getAttribute ("id") << " (" << x << "," << y << " " << w << "x" << h << ") -> (" << x1 << "," << y1 << " " << w1 << "x" << h1 << ")" << endl;
            RegionRuntime *rr = static_cast<RegionRuntime*>(getRuntime().ptr());
            if (rr && rr->have_bg_color)
                p->painter.fillRect (rx, ry, rw, rh, QColor (QRgb (rr->background_color)));
            for (NodeRefItemPtr n = users.first (); n; n = n->nextSibling ())
                if (n->data)
                    n->data->handleEvent (event); // for MediaType listeners
            p->painter.setClipping (false);
            return RegionBase::handleEvent (event);
        }
        case event_pointer_clicked: {
            PointerEvent * e = static_cast <PointerEvent *> (event.ptr ());
            bool inside = e->x > rx && e->x < rx+rw && e->y > ry && e->y< ry+rh;
            if (!inside)
                return false;
            bool handled = false;
            for (NodePtr r = firstChild (); r; r = r->nextSibling ())
                handled |= r->handleEvent (event);
            if (!handled) { // handle it ..
                EventPtr evt = (new Event (event_activated))->self ();
                propagateEvent (evt);
                for (NodeRefItemPtr n = users.first (); n; n = n->nextSibling())
                    if (n->data)
                        n->data->propagateEvent (evt); //for MediaType listeners
            }
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
                evt = (new Event (event_outbounds))->self ();
            } else if (inside && !handled && !has_mouse) { // InBoundsEvent
                has_mouse = true;
                evt = (new Event (event_inbounds))->self ();
            }
            if (evt) {
                propagateEvent (evt);
                for (NodeRefItemPtr n = users.first (); n; n = n->nextSibling())
                    if (n->data)
                        n->data->propagateEvent (evt); //for MediaType listeners
            }
            return inside;
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

KDE_NO_EXPORT void SMIL::Region::addRegionUser (NodePtr mt) {
    for (NodeRefItemPtr n = users.first (); n; n = n->nextSibling ())
        if (n->data == mt)
            return;
    users.append ((new NodeRefItem (mt))->self ());
}

//-----------------------------------------------------------------------------

KDE_NO_EXPORT ElementRuntimePtr SMIL::RegPoint::getRuntime () {
    if (!runtime)
        runtime = (new RegPointRuntime (self ()))->self ();
    return runtime;
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT SMIL::TimedMrl::TimedMrl (NodePtr & d, short id)
 : Mrl (d, id),
   m_StartedListeners ((new NodeRefList)->self ()),
   m_StoppedListeners ((new NodeRefList)->self ()) {}

KDE_NO_EXPORT void SMIL::TimedMrl::activate () {
    //kdDebug () << "SMIL::TimedMrl(" << nodeName() << ")::activate" << endl;
    setState (state_activated);
    ElementRuntimePtr rt = getRuntime ();
    if (rt) {
        rt->init ();
        rt->begin ();
    }
}

KDE_NO_EXPORT void SMIL::TimedMrl::deactivate () {
    if (unfinished ())
        finish ();
    Mrl::deactivate ();
}

KDE_NO_EXPORT void SMIL::TimedMrl::finish () {
    Mrl::finish ();
    propagateEvent ((new Event (event_stopped))->self ());
}

KDE_NO_EXPORT void SMIL::TimedMrl::reset () {
    //kdDebug () << "SMIL::TimedMrl::reset " << endl;
    Mrl::reset ();
    if (runtime)
        runtime->end ();
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
    if (c->state == state_finished)
        c->deactivate ();
    if (c->nextSibling ())
        c->nextSibling ()->activate ();
    else { // check if Runtime still running
        TimedRuntime * tr = static_cast <TimedRuntime *> (getRuntime ().ptr ());
        if (tr && tr->state () < TimedRuntime::timings_stopped) {
            if (tr->state () == TimedRuntime::timings_started)
                tr->propagateStop (false);
            return; // still running, wait for runtime to finish
        }
        finish ();
    }
}

KDE_NO_EXPORT ElementRuntimePtr SMIL::TimedMrl::getRuntime () {
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
    TimedRuntime * te = static_cast <TimedRuntime *> (getRuntime ().ptr ());
    if (te)
        te->processEvent (event->id ());
    return true;
}

KDE_NO_EXPORT ElementRuntimePtr SMIL::TimedMrl::getNewRuntime () {
    return (new TimedRuntime (m_self))->self ();
}

//-----------------------------------------------------------------------------

KDE_NO_EXPORT bool SMIL::GroupBase::isMrl () {
    return false;
}

KDE_NO_EXPORT bool SMIL::GroupBase::expose () const {
    return !pretty_name.isEmpty () || //return false if no title and only one
        previousSibling () || nextSibling ();
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
    //kdDebug () << "SMIL::Par::activate" << endl;
    GroupBase::activate (); // calls init() and begin() on runtime
}

KDE_NO_EXPORT void SMIL::Par::deactivate () {
    if (unfinished ())
        finish ();
    GroupBase::deactivate ();
}

KDE_NO_EXPORT void SMIL::Par::finish () {
    setState (state_finished); // prevent recursion via childDone
    TimedRuntime * tr = static_cast <TimedRuntime *> (getRuntime ().ptr ());
    if (tr && tr->state () == TimedRuntime::timings_started) {
        tr->propagateStop (true);
        return; // wait for runtime to call deactivate()
    }
    for (NodePtr e = firstChild (); e; e = e->nextSibling ())
        if (e->active ())
            e->deactivate ();
    GroupBase::finish ();
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
        TimedRuntime * tr = static_cast <TimedRuntime *> (getRuntime ().ptr ());
        if (tr && tr->state () == TimedRuntime::timings_started) {
            unsigned dv =tr->durations[(int)TimedRuntime::duration_time].durval;
            if (dv == 0 || dv == duration_media)
                tr->propagateStop (false);
            return; // still running, wait for runtime to finish
        }
        finish (); // we're done
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
    //kdDebug () << "SMIL::Excl::activate" << endl;
    setState (state_activated);
    TimedRuntime * tr = static_cast <TimedRuntime *> (getRuntime ().ptr ());
    if (tr)
        tr->init ();
    if (tr && firstChild ()) { // init children
        for (NodePtr e = firstChild (); e; e = e->nextSibling ())
            e->activate ();
        for (NodePtr e = firstChild (); e; e = e->nextSibling ()) {
            SMIL::TimedMrl * tm = dynamic_cast <SMIL::TimedMrl *> (e.ptr ());
            if (tm) { // sets up aboutToStart connection with TimedMrl children
                ConnectionPtr c = tm->connectTo (m_self, event_to_be_started);
                started_event_list.append((new ConnectionStoreItem(c))->self());
            }
        }
        tr->begin ();
    } else { // no children, deactivate if runtime started and no duration set
        if (tr && tr->state () == TimedRuntime::timings_started) {
            if (tr->durations[(int)TimedRuntime::duration_time].durval == duration_media)
                tr->propagateStop (false);
            return; // still running, wait for runtime to finish
        }
        finish (); // no children to run
    }
}

KDE_NO_EXPORT void SMIL::Excl::deactivate () {
    started_event_list.clear (); // auto disconnect on destruction of data items
    GroupBase::deactivate ();
}

KDE_NO_EXPORT void SMIL::Excl::childDone (NodePtr /*child*/) {
    // do nothing
}

KDE_NO_EXPORT bool SMIL::Excl::handleEvent (EventPtr event) {
    if (event->id () == event_to_be_started) {
        ToBeStartedEvent * se = static_cast <ToBeStartedEvent *> (event.ptr ());
        //kdDebug () << "Excl::handleEvent " << se->node->nodeName()<<endl;
        for (NodePtr e = firstChild (); e; e = e->nextSibling ()) {
            if (e == se->node) // stop all _other_ child elements
                continue;
            TimedRuntime *tr=dynamic_cast<TimedRuntime*>(e->getRuntime().ptr());
            tr->propagateStop (true);
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
        return elm->self ();
    return NodePtr ();
}

KDE_NO_EXPORT void SMIL::Switch::activate () {
    //kdDebug () << "SMIL::Switch::activate" << endl;
    setState (state_activated);
    if (firstChild ())
        firstChild ()->activate (); // activate only the first for now FIXME: condition
    else
        finish ();
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

KDE_NO_EXPORT void SMIL::Switch::childDone (NodePtr) {
    //kdDebug () << "SMIL::Switch::childDone" << endl;
    finish (); // only one child can run
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT SMIL::MediaType::MediaType (NodePtr &d, const QString &t, short id)
 : TimedMrl (d, id), m_type (t), bitrate (0),
   m_ActionListeners ((new NodeRefList)->self ()),
   m_OutOfBoundsListeners ((new NodeRefList)->self ()),
   m_InBoundsListeners ((new NodeRefList)->self ()) {}

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

bool SMIL::MediaType::handleEvent (EventPtr event) {
    switch (event->id ()) {
        case event_paint: {
            MediaTypeRuntime * mr = static_cast <MediaTypeRuntime *> (getRuntime ().ptr ());
            if (mr)
                mr->paint (static_cast <PaintEvent *> (event.ptr ())->painter);
            return true;
        }
        case event_sized:
            return true; // ignored for now
        case event_activated:
        case event_outbounds:
        case event_inbounds:
            propagateEvent (event);
            // fall through // return true;
        default:
            return TimedMrl::handleEvent (event);
    }
    return false;
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

KDE_NO_EXPORT void SMIL::AVMediaType::positionVideoWidget () {
    //kdDebug () << "AVMediaType::sized " << endl;
    PlayListNotify * n = document()->notify_listener;
    MediaTypeRuntime * mtr = static_cast <MediaTypeRuntime *> (getRuntime ().ptr ());
    if (n && mtr && mtr->region_node) {
        RegionBase * rb = convertNode <RegionBase> (mtr->region_node);
        int x, y, w, h;
        mtr->sizes.calcSizes (this, rb->w, rb->h, x, y, w, h);
        Matrix matrix (x, y, 1.0, 1.0);
        matrix.transform (rb->transform ());
        int xoff = 0, yoff = 0;
        matrix.getXYWH (xoff, yoff, w, h);
        unsigned int * bg_color = 0L;
        if (mtr->region_node) {
            RegionRuntime * rr = static_cast <RegionRuntime *>(mtr->region_node->getRuntime ().ptr ());
            if (rr && rr->have_bg_color)
                bg_color = &rr->background_color;
        }
        n->avWidgetSizes (xoff, yoff, w, h, bg_color);
    }
}

KDE_NO_EXPORT void SMIL::AVMediaType::activate () {
    if (!isMrl ()) { // turned out this URL points to a playlist file
        Element::activate ();
        return;
    }
    //kdDebug () << "SMIL::AVMediaType::activate" << endl;
    NodePtr p = parentNode ();
    while (p && p->id != id_node_smil)
        p = p->parentNode ();
    if (p) { // this works only because we can only play one at a time FIXME
        convertNode <Smil> (p)->current_av_media_type = m_self;
        MediaType::activate ();
    } else {
        kdError () << nodeName () << " playing and current is not Smil" << endl;
        finish ();
    }
}

KDE_NO_EXPORT void SMIL::AVMediaType::deactivate () {
    TimedRuntime * tr = static_cast <TimedRuntime *> (getRuntime ().ptr ());
    if (tr && tr->state () == TimedRuntime::timings_started) {
        tr->propagateStop (false);
        return; // movie has stopped, wait for runtime
    }
    TimedMrl::deactivate ();
    // TODO stop backend player
}

KDE_NO_EXPORT ElementRuntimePtr SMIL::AVMediaType::getNewRuntime () {
    return (new AudioVideoData (m_self))->self ();
}

bool SMIL::AVMediaType::handleEvent (EventPtr event) {
    if (event->id () == event_sized) {
        // when started, connected to Layout's size event
        positionVideoWidget ();
    } else
        return SMIL::MediaType::handleEvent (event);
    return true;
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT
SMIL::ImageMediaType::ImageMediaType (NodePtr & d)
    : SMIL::MediaType (d, "img", id_node_img) {}

KDE_NO_EXPORT ElementRuntimePtr SMIL::ImageMediaType::getNewRuntime () {
    isMrl (); // hack to get relative paths right
    return (new ImageData (m_self))->self ();
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT
SMIL::TextMediaType::TextMediaType (NodePtr & d)
    : SMIL::MediaType (d, "text", id_node_text) {}

KDE_NO_EXPORT ElementRuntimePtr SMIL::TextMediaType::getNewRuntime () {
    isMrl (); // hack to get relative paths right
    return (new TextData (m_self))->self ();
}

//-----------------------------------------------------------------------------

KDE_NO_EXPORT ElementRuntimePtr SMIL::Set::getNewRuntime () {
    return (new SetData (m_self))->self ();
}

//-----------------------------------------------------------------------------

KDE_NO_EXPORT ElementRuntimePtr SMIL::Animate::getNewRuntime () {
    return (new AnimateData (m_self))->self ();
}

//-----------------------------------------------------------------------------

KDE_NO_EXPORT void SMIL::Param::activate () {
    setState (state_activated);
    QString name = getAttribute ("name");
    if (!name.isEmpty () && parentNode ()) {
        ElementRuntimePtr rt = parentNode ()->getRuntime ();
        if (rt)
            rt->setParam (name, getAttribute ("value"));
    }
    Element::activate (); //finish (); // no livetime of itself, will deactivate
}

//-----------------------------------------------------------------------------

namespace KMPlayer {
    class ImageDataPrivate {
        public:
            ImageDataPrivate () : image (0L), cache_image (0), img_movie (0L) {}
            ~ImageDataPrivate () {
                delete image;
                delete cache_image;
            }
            QPixmap * image;
            QPixmap * cache_image; // scaled cache
            QMovie * img_movie;
            QString url;
            int olddur;
            bool have_frame;
    };
}

KDE_NO_CDTOR_EXPORT ImageData::ImageData (NodePtr e)
 : MediaTypeRuntime (e), d (new ImageDataPrivate) {
}

KDE_NO_CDTOR_EXPORT ImageData::~ImageData () {
    delete d;
}

KDE_NO_EXPORT
void ImageData::parseParam (const QString & name, const QString & val) {
    //kdDebug () << "ImageData::param " << name << "=" << val << endl;
    MediaTypeRuntime::parseParam (name, val);
    if (name == QString::fromLatin1 ("src")) {
        killWGet ();
        if (!val.isEmpty () && d->url != val) {
            KURL url (source_url);
            d->url = val;
            if (url.isLocalFile ()) {
                QPixmap *pix = new QPixmap (url.path ());
                if (pix->isNull ()) {
                    delete pix;
                    delete d->image;
                    d->image = 0L;
                } else {
                    d->image = pix;
                    delete d->img_movie;
                    QMovie * mov = new QMovie (url.path ());
                    if (mov->isNull ()) {
                        delete mov;
                        d->img_movie = 0L;
                    } else {
                      mov->connectUpdate(this,SLOT(movieUpdated(const QRect&)));
                      mov->connectStatus (this, SLOT (movieStatus (int)));
                      mov->connectResize(this, SLOT(movieResize(const QSize&)));
                      d->img_movie = mov;
                    }
                    delete d->cache_image;
                    d->cache_image = 0;
                    d->have_frame = false;
                }
            } else
                wget (url);
        }
    }
}

KDE_NO_EXPORT void ImageData::paint (QPainter & p) {
    if (((d->image && !d->image->isNull ()) ||
                (d->img_movie && !d->img_movie->isNull ())) &&
            region_node && (timingstate == timings_started ||
                (timingstate == timings_stopped && fill == fill_freeze))) {
        SMIL::RegionBase * rb = convertNode <SMIL::RegionBase> (region_node);
        if (rb->w <= 0 || rb->h <= 0)
            return;
        const QPixmap &img = (d->image ? *d->image:d->img_movie->framePixmap());
        int x, y, w0, h0;
        sizes.calcSizes (element.ptr (), rb->w, rb->h, x, y, w0, h0);
        Matrix matrix (x, y, 1.0, 1.0);
        matrix.transform (rb->transform ());
        int xoff = 0, yoff = 0, w = w0, h = h0;
        matrix.getXYWH (xoff, yoff, w, h);
        if (fit == fit_hidden) {
            w = int (img.width () * 1.0 * w / w0);
            h = int (img.height () * 1.0 * h / h0);
        } else if (fit == fit_meet) { // scale in region, keeping aspects
            if (h > 0 && img.height () > 0) {
                int a = 100 * img.width () / img.height ();
                int w1 = a * h / 100;
                if (w1 > w)
                    h = 100 * w / a;
                else
                    w = w1;
            }
        } else if (fit == fit_slice) { // scale in region, keeping aspects
            if (h > 0 && img.height () > 0) {
                int a = 100 * img.width () / img.height ();
                int w1 = a * h / 100;
                if (w1 > w)
                    w = w1;
                else
                    h = 100 * w / a;
            }
        } //else if (fit == fit_fill) { // scale in region
        // else fit_scroll
        if (w == img.width () && h == img.height ())
            p.drawPixmap (QRect (xoff, yoff, w, h), img);
        else {
            if (!d->cache_image || w != d->cache_image->width () || h != d->cache_image->height ()) {
                delete d->cache_image;
                QImage img2;
                img2 = img;
                d->cache_image = new QPixmap (img2.scale (w, h));
            }
            p.drawPixmap (QRect (xoff, yoff, w, h), *d->cache_image);
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
    //if (durations [duration_time].durval == 0)
    //    durations [duration_time].durval = duration_media; // intrinsic duration of 0 FIXME gif movies
    if (d->img_movie) {
        d->img_movie->restart ();
        if (d->img_movie->paused ())
            d->img_movie->unpause ();
    }
    MediaTypeRuntime::started ();
}

KDE_NO_EXPORT void ImageData::stopped () {
    if (d->img_movie && d->have_frame)
        d->img_movie->pause ();
    MediaTypeRuntime::stopped ();
}

KDE_NO_EXPORT void ImageData::slotResult (KIO::Job * job) {
    //kdDebug () << "ImageData::slotResult" << endl;
    MediaTypeRuntime::slotResult (job);
    if (mt_d->data.size () && element) {
        QPixmap *pix = new QPixmap (mt_d->data);
        if (!pix->isNull ()) {
            d->image = pix;
            delete d->cache_image;
            d->cache_image = 0;
            delete d->img_movie;
            d->img_movie = new QMovie (mt_d->data);
            d->have_frame = false;
            d->img_movie->connectUpdate(this, SLOT(movieUpdated(const QRect&)));
            d->img_movie->connectStatus (this, SLOT (movieStatus (int)));
            d->img_movie->connectResize(this, SLOT (movieResize(const QSize&)));
            if (region_node && (timingstate == timings_started ||
                 (timingstate == timings_stopped && fill == fill_freeze)))
                convertNode <SMIL::RegionBase> (region_node)->repaint ();
        } else
            delete pix;
    }
    if (timingstate == timings_started &&
            durations [duration_time].durval == duration_data_download) {
        durations [duration_time].durval = d->olddur;
        propagateStart ();
    }
}

KDE_NO_EXPORT void ImageData::movieUpdated (const QRect &) {
    d->have_frame = true;
    if (region_node && (timingstate == timings_started ||
                (timingstate == timings_stopped && fill == fill_freeze))) {
        delete d->cache_image;
        d->cache_image = 0;
        delete d->image;
        d->image = 0;
        convertNode <SMIL::RegionBase> (region_node)->repaint ();
        //kdDebug () << "movieUpdated" << endl;
    }
    if (timingstate != timings_started && d->img_movie)
        d->img_movie->pause ();
}

KDE_NO_EXPORT void ImageData::movieStatus (int s) {
    if (region_node && (timingstate == timings_started ||
                (timingstate == timings_stopped && fill == fill_freeze))) {
        if (s == QMovie::EndOfMovie)
            propagateStop (false);
    }
}

KDE_NO_EXPORT void ImageData::movieResize (const QSize & s) {
    if (!(d->cache_image && d->cache_image->width () == s.width () && d->cache_image->height () == s.height ()) &&
            region_node && (timingstate == timings_started ||
                (timingstate == timings_stopped && fill == fill_freeze))) {
        convertNode <SMIL::RegionBase> (region_node)->repaint ();
        //kdDebug () << "movieResize" << endl;
    }
}

//-----------------------------------------------------------------------------
#include <qtextedit.h>

namespace KMPlayer {
    class TextDataPrivate {
    public:
        TextDataPrivate () : edit (0L) {
            reset ();
        }
        void reset () {
            codec = 0L;
            font = QApplication::font ();
            font_size = font.pointSize ();
            transparent = false;
            delete edit;
            edit = new QTextEdit;
            edit->setReadOnly (true);
            edit->setHScrollBarMode (QScrollView::AlwaysOff);
            edit->setVScrollBarMode (QScrollView::AlwaysOff);
            edit->setFrameShape (QFrame::NoFrame);
            edit->setFrameShadow (QFrame::Plain);
        }
        QByteArray data;
        int olddur;
        QTextCodec * codec;
        QFont font;
        int font_size;
        bool transparent;
        QTextEdit * edit;
    };
}

KDE_NO_CDTOR_EXPORT TextData::TextData (NodePtr e)
 : MediaTypeRuntime (e), d (new TextDataPrivate) {
}

KDE_NO_CDTOR_EXPORT TextData::~TextData () {
    delete d->edit;
    delete d;
}

KDE_NO_EXPORT void TextData::end () {
    d->reset ();
    MediaTypeRuntime::end ();
}

KDE_NO_EXPORT
void TextData::parseParam (const QString & name, const QString & val) {
    //kdDebug () << "TextData::parseParam " << name << "=" << val << endl;
    if (name == QString::fromLatin1 ("src")) {
        if (source_url == val)
            return;
        MediaTypeRuntime::parseParam (name, val);
        d->data.resize (0);
        killWGet ();
        if (!val.isEmpty ()) {
            KURL url (source_url);
            if (url.isLocalFile ()) {
                QFile file (url.path ());
                file.open (IO_ReadOnly);
                d->data = file.readAll ();
            } else
                wget (url);
        }
        return;
    }
    MediaTypeRuntime::parseParam (name, val);
    if (name == QString::fromLatin1 ("backgroundColor")) {
        d->edit->setPaper (QBrush (QColor (val)));
    } else if (name == QString ("fontColor")) {
        d->edit->setPaletteForegroundColor (QColor (val));
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
        return;
    if (region_node && (timingstate == timings_started ||
                (timingstate == timings_stopped && fill == fill_freeze)))
        convertNode <SMIL::RegionBase> (region_node)->repaint ();
}

KDE_NO_EXPORT void TextData::paint (QPainter & p) {
    if (region_node && (timingstate == timings_started ||
                (timingstate == timings_stopped && fill == fill_freeze))) {
        SMIL::RegionBase * rb = convertNode <SMIL::RegionBase> (region_node);
        int x, y, w0, h0;
        sizes.calcSizes (element.ptr (), rb->w, rb->h, x, y, w0, h0);
        Matrix matrix (x, y, 1.0, 1.0);
        matrix.transform (rb->transform ());
        int xoff = 0, yoff = 0, w = w0, h = h0;
        matrix.getXYWH (xoff, yoff, w, h);
        d->edit->setGeometry (0, 0, w, h);
        if (d->edit->length () == 0) {
            QTextStream text (d->data, IO_ReadOnly);
            if (d->codec)
                text.setCodec (d->codec);
            d->edit->setText (text.read ());
        }
        if (w0 > 0)
            d->font.setPointSize (int (1.0 * w * d->font_size / w0));
        d->edit->setFont (d->font);
        QRect rect = p.clipRegion (QPainter::CoordPainter).boundingRect ();
        rect = rect.intersect (QRect (xoff, yoff, w, h));
        QPixmap pix = QPixmap::grabWidget (d->edit, rect.x () - xoff, rect.y () - yoff, rect.width (), rect.height ());
        //kdDebug () << "text paint " << x << "," << y << " " << w << "x" << h << " clip: " << rect.x () << "," << rect.y () << endl;
        p.drawPixmap (rect.x (), rect.y (), pix);
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
            convertNode <SMIL::RegionBase> (region_node)->repaint ();
    }
    if (timingstate == timings_started &&
            durations [duration_time].durval == duration_data_download) {
        durations [duration_time].durval = d->olddur;
        propagateStart ();
    }
}

#include "kmplayer_smil.moc"
