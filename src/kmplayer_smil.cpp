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

#include "kmplayer_smil.h"

using namespace KMPlayer;

static const unsigned int duration_infinite = (unsigned int) -1;
static const unsigned int duration_media = (unsigned int) -2;
static const unsigned int duration_element_activated = (unsigned int) -3;
static const unsigned int duration_element_inbounds = (unsigned int) -4;
static const unsigned int duration_element_outbounds = (unsigned int) -5;
static const unsigned int duration_last_option = (unsigned int) -6;

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

KDE_NO_CDTOR_EXPORT RegionNode::RegionNode (ElementPtr e)
 : x (0), y (0), w (0), h (0), regionElement (e) {}

KDE_NO_CDTOR_EXPORT void RegionNode::clearAll () {
    kdDebug () << "RegionNode::clearAll " << endl;
    attached_element = ElementPtr ();
    for (RegionNodePtr r = firstChild; r; r = r->nextSibling)
        r->clearAll ();
}

KDE_NO_CDTOR_EXPORT
ElementRuntime::ElementRuntime (ElementPtr & e)
 : media_element (e), start_timer (0), dur_timer (0), isstarted (false) {}

KDE_NO_CDTOR_EXPORT ElementRuntime::~ElementRuntime () {}

KDE_NO_EXPORT
void ElementRuntime::setDurationItem (DurationTime item, const QString & val) {
    unsigned int dur = 0; // also 0 for 'media' duration, so it will not update then
    QRegExp reg ("\\s*([0-9\\.]+)\\s*([a-z]*)");
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
    if (!dur && media_element) {
        int pos = vl.find (QChar ('.'));
        if (pos > 0) {
            ElementPtr e = media_element->document()->getElementById (vl.left(pos));
            if (e) {
                kdDebug () << "getElementById " << vl.left (pos) << " " << e->nodeName () << endl;
                ElementRuntimePtr rt = e->getRuntime ();
                if (rt) {
                    if (vl.find ("activateevent") > -1) {
                        dur = duration_element_activated;
                        connect (rt.ptr (), SIGNAL (activateEvent ()),
                                 this, SLOT (elementActivateEvent ()));
                    } else if (vl.find ("inboundsevent") > -1) {
                        dur = duration_element_inbounds;
                        connect (rt.ptr (), SIGNAL (inBoundsEvent ()),
                                 this, SLOT (elementInBoundsEvent ()));
                    } else if (vl.find ("outofboundsevent") > -1) {
                        dur = duration_element_outbounds;
                        connect (rt.ptr (), SIGNAL (outOfBoundsEvent ()),
                                 this, SLOT (elementOutOfBoundsEvent ()));
                    }
                    durations [(int) item].connection = rt;
                }
            }
        }
    }
    durations [(int) item].durval = dur;
}

KDE_NO_EXPORT void ElementRuntime::begin () {
    kdDebug () << "ElementRuntime::begin " << (media_element ? media_element->nodeName() : "-") << endl; 
    if (start_timer || dur_timer)
        end ();
    // setup timings ..
    setDurationItem (begin_time, media_element->getAttribute ("begin"));
    setDurationItem (end_time, media_element->getAttribute ("end"));
    QString durvalstr = media_element->getAttribute ("dur").stripWhiteSpace ();
    if (durvalstr.isEmpty ()) { // update dur if not set
        if (durations [end_time].durval < duration_last_option &&
            durations [end_time].durval > durations [begin_time].durval)
            durations [duration_time].durval = durations [end_time].durval - durations [begin_time].durval;
        // else use intrinsic time duration
    } else
        setDurationItem (duration_time, durvalstr);
    if (durations [begin_time].durval > 0) {
        if (durations [begin_time].durval < duration_last_option)
            start_timer = startTimer (1000 * durations [begin_time].durval);
    } else {
        isstarted = true;
        QTimer::singleShot (0, this, SLOT (started ()));
    }
}
    
KDE_NO_EXPORT void ElementRuntime::end () {
    kdDebug () << "ElementRuntime::end " << (media_element ? media_element->nodeName() : "-") << endl; 
    killTimers ();
    start_timer = 0;
    dur_timer = 0;
    isstarted = false;
    for (int i = 0; i < (int) durtime_last; i++) {
        if (durations [i].connection) {
            disconnect (durations [i].connection, 0, this, 0);
            durations [i].connection = ElementRuntimePtr ();
        }
        durations [i].durval = 0;
    }
}

KDE_NO_EXPORT void ElementRuntime::timerEvent (QTimerEvent * e) {
    kdDebug () << "ElementRuntime::timerEvent " << (media_element ? media_element->nodeName() : "-") << endl; 
    if (e->timerId () == start_timer) {
        killTimer (start_timer);
        start_timer = 0;
        isstarted = true;
        QTimer::singleShot (0, this, SLOT (started ()));
    } else if (e->timerId () == dur_timer) {
        end ();
        QTimer::singleShot (0, this, SLOT (stopped ()));
    }
}

KDE_NO_EXPORT void ElementRuntime::elementActivateEvent () {
    processEvent (duration_element_activated);
}

KDE_NO_EXPORT void ElementRuntime::elementInBoundsEvent () {
    processEvent (duration_element_inbounds);
}

KDE_NO_EXPORT void ElementRuntime::elementOutOfBoundsEvent () {
    processEvent (duration_element_outbounds);
}

KDE_NO_EXPORT void ElementRuntime::processEvent (unsigned int event) {
    kdDebug () << "ElementRuntime::processEvent " << event << " " << (media_element ? media_element->nodeName() : "-") << endl; 
    if (!isstarted && durations [begin_time].durval == event) {
        isstarted = true;
        QTimer::singleShot (0, this, SLOT (started ()));
    } else if (isstarted && durations [end_time].durval == event) {
        end ();
        QTimer::singleShot (0, this, SLOT (stopped ()));
    }
}

KDE_NO_EXPORT void ElementRuntime::started () {
    kdDebug () << "ElementRuntime::started " << (media_element ? media_element->nodeName() : "-") << " dur:" << durations [duration_time].durval << endl; 
    if (durations [duration_time].durval > 0) {
        if (durations [duration_time].durval < duration_last_option)
            dur_timer = startTimer (1000 * durations [duration_time].durval);
    } else if (durations [end_time].durval < duration_last_option)
        // no duration set and no special end, so mark us finished
        QTimer::singleShot (0, this, SLOT (stopped ()));
}

KDE_NO_EXPORT void ElementRuntime::stopped () {
    end ();
}

//-----------------------------------------------------------------------------

KDE_NO_EXPORT void MediaTypeRuntime::stopped () {
    SMIL::MediaType * mt = convertNode <SMIL::MediaType> (media_element);
    if (!mt)
        end ();
    else
        mt->timed_end ();
}

//-----------------------------------------------------------------------------

KDE_NO_EXPORT void SetData::started () {
    kdDebug () << "SetData::started" << endl;
    if (media_element) {
        QString elm_id = media_element->getAttribute ("targetElement");
        target_element = media_element->document ()->getElementById (elm_id);
        changed_attribute = media_element->getAttribute ("attributeName");
        QString to = media_element->getAttribute ("to");
        if (target_element) {
            old_value = target_element->getAttribute (changed_attribute);
            target_element->setAttribute (changed_attribute, to);
            PlayListNotify * n = target_element->document ()->notify_listener;
            ElementRuntimePtr rt = target_element->getRuntime ();
            if (rt)
                target_region = rt->region_node;
            else if (!strcmp (target_element->nodeName (), "region"))
                target_region = findRegion (target_element->document ()->rootLayout, elm_id);
    kdDebug () << "SetData::started " << target_element->nodeName () << "." << changed_attribute << " " << old_value << "->" << to << endl;
            if (n && target_region)
                n->repaintRegion (target_region);
        } else
            kdWarning () << "target element not found" << endl;
    } else
        kdWarning () << "set element disappeared" << endl;
}

KDE_NO_EXPORT void SetData::stopped () {
    if (target_element) {
        target_element->setAttribute (changed_attribute, old_value);
        PlayListNotify * n = target_element->document ()->notify_listener;
        if (n && target_region)
            n->repaintRegion (target_region);
    } else
        kdWarning () << "target element not found" << endl;
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT AudioVideoData::AudioVideoData (ElementPtr e)
    : MediaTypeRuntime (e) {}

KDE_NO_EXPORT bool AudioVideoData::isAudioVideo () {
    return isstarted;
}

KDE_NO_EXPORT void AudioVideoData::started () {
    kdDebug () << "AudioVideoData::started " << endl; 
    SMIL::MediaType * mt = convertNode <SMIL::MediaType> (media_element);
    if (mt)
        mt->timed_start ();
    ElementRuntime::started ();
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

static Element * fromMediaContentGroup (ElementPtr & d, const QString & tag) {
    const char * taglatin = tag.latin1 ();
    if (!strcmp (taglatin, "video") || !strcmp (taglatin, "audio"))
        return new SMIL::AVMediaType (d, tag);
    else if (!strcmp (taglatin, "img"))
        return new SMIL::ImageMediaType (d);
    else if (!strcmp (taglatin, "text"))
        return new SMIL::TextMediaType (d);
    else if (!strcmp (tag.latin1 (), "set")) // not sure if correct ..
        return new SMIL::Set (d);
    // animation, textstream, ref, brush
    return 0L;
}

static Element * fromContentControlGroup (ElementPtr & d, const QString & tag) {
    if (!strcmp (tag.latin1 (), "switch"))
        return new SMIL::Switch (d);
    else if (!strcmp (tag.latin1 (), "set"))
        return new SMIL::Set (d);
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

KDE_NO_EXPORT void Smil::start () {
    kdDebug () << "Smil::start" << endl;
    current_av_media_type = ElementPtr ();
    setState (state_started);
    for (ElementPtr e = firstChild (); e; e = e->nextSibling ())
        if (!strcmp (e->nodeName (), "body")) {
            e->start ();
            return;
        }
    stop (); //source->emitEndOfPlayItems ();
}

KDE_NO_EXPORT ElementPtr Smil::realMrl () {
    return current_av_media_type;
}

KDE_NO_EXPORT bool Smil::isMrl () {
    return true;
}

KDE_NO_EXPORT void Smil::childDone (ElementPtr child) {
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
                last_region->nextSibling = RegionNodePtr (new ::RegionNode (e));
                last_region = last_region->nextSibling;
            } else {
                region = last_region = RegionNodePtr (new RegionNode (e));
                r->firstChild = region;
            }
            buildRegionNodes (e, last_region);
        }
}

static void sizeRegionNodes (RegionNodePtr p) {
    SMIL::RegionBase * rb = convertNode <SMIL::RegionBase> (p->regionElement);
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
            root = RegionNodePtr (new RegionNode (e));
            if (region)
                root->firstChild = region;
            smilroot = convertNode <SMIL::RootLayout> (e);
        } else if (!strcmp (name, "region")) {
            if (region) {
                last_region->nextSibling = RegionNodePtr (new ::RegionNode (e));
                last_region = last_region->nextSibling;
            } else {
                region = last_region = RegionNodePtr (new RegionNode (e));
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

KDE_NO_EXPORT ElementPtr SMIL::Region::childFromTag (const QString & tag) {
    if (!strcmp (tag.latin1 (), "region"))
        return (new SMIL::Region (m_doc))->self ();
    return ElementPtr ();
}

//-----------------------------------------------------------------------------

KDE_NO_EXPORT ElementPtr SMIL::Body::childFromTag (const QString & tag) {
    Element * elm = fromScheduleGroup (m_doc, tag);
    if (!elm) elm = fromMediaContentGroup (m_doc, tag);
    if (!elm) elm = fromContentControlGroup (m_doc, tag);
    if (elm)
        return elm->self ();
    return ElementPtr ();
}

//-----------------------------------------------------------------------------

KDE_NO_EXPORT ElementPtr SMIL::Par::childFromTag (const QString & tag) {
    Element * elm = fromScheduleGroup (m_doc, tag);
    if (!elm) elm = fromMediaContentGroup (m_doc, tag);
    if (!elm) elm = fromContentControlGroup (m_doc, tag);
    if (elm)
        return elm->self ();
    return ElementPtr ();
}

KDE_NO_EXPORT void SMIL::Par::start () {
    kdDebug () << "SMIL::Par::start" << endl;
    setState (state_started);
    if (firstChild ()) {
        for (ElementPtr e = firstChild (); e; e = e->nextSibling ())
            e->start ();
    } else
        stop (); // no children to run in parallel
}

KDE_NO_EXPORT void SMIL::Par::stop () {
    for (ElementPtr e = firstChild (); e; e = e->nextSibling ())
        // children are out of scope now, reset their ElementRuntime
        e->reset (); // will call stop() if necessary
    Element::stop ();
}

KDE_NO_EXPORT void SMIL::Par::reset () {
    Element::reset ();
    for (ElementPtr e = firstChild (); e; e = e->nextSibling ())
        e->reset ();
}

KDE_NO_EXPORT void SMIL::Par::childDone (ElementPtr) {
    kdDebug () << "SMIL::Par::childDone" << endl;
    for (ElementPtr e = firstChild (); e; e = e->nextSibling ()) {
        if (e->state != state_finished)
            return; // not all done
    }
    stop (); // we're done
}

//-----------------------------------------------------------------------------

KDE_NO_EXPORT ElementPtr SMIL::Seq::childFromTag (const QString & tag) {
    Element * elm = fromScheduleGroup (m_doc, tag);
    if (!elm) elm = fromMediaContentGroup (m_doc, tag);
    if (!elm) elm = fromContentControlGroup (m_doc, tag);
    if (elm)
        return elm->self ();
    return ElementPtr ();
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
    : Mrl (d), m_type (t), bitrate (0), begin_time (0), end_time (0), duration_time (0) {}

KDE_NO_EXPORT ElementPtr SMIL::MediaType::childFromTag (const QString & tag) {
    Element * elm = fromContentControlGroup (m_doc, tag);
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
        else if (!strcmp (cname, "region")) {
            RegionNodePtr root = document ()->rootLayout;
            if (root)
                region = findRegion (root, a->nodeValue ());
            if (!region)
                kdWarning() << "region " << a->nodeValue()<<" not found"<< endl;
        }
        else
            kdWarning () << "unhandled MediaType attr: " << cname << "=" << a->nodeValue () << endl;
    }
    kdDebug () << "MediaType attr found bitrate: " << bitrate << " src: " << (src.isEmpty() ? "-" : src) << " type: " << (mimetype.isEmpty() ? "-" : mimetype) << endl;
}

KDE_NO_EXPORT void SMIL::MediaType::start () {
    kdDebug () << "SMIL::MediaType(" << nodeName() << ")::start " << !!region << endl;
    setState (state_started);
    if (region) {
        region->clearAll ();
        region->attached_element = m_self;
        ElementRuntimePtr rt = getRuntime ();
        if (rt) {
            rt->region_node = region;
            rt->durations [ElementRuntime::duration_time].durval =duration_time;
            rt->begin ();
        }
    }
}

KDE_NO_EXPORT void SMIL::MediaType::timed_start () {
    if (document ()->notify_listener && !src.isEmpty ())
        document ()->notify_listener->requestPlayURL (m_self, region);
}

KDE_NO_EXPORT void SMIL::MediaType::timed_end () {
    kdDebug () << "SMIL::MediaType::timed_end " << duration_time << " " << nodeName() << endl;
    stop ();
}

KDE_NO_EXPORT void SMIL::MediaType::reset () {
    kdDebug () << "SMIL::MediaType::reset " << endl;
    Mrl::reset ();
    if (runtime)
        runtime->end ();
    if (region)
        region->clearAll ();
}

KDE_NO_EXPORT ElementRuntimePtr SMIL::MediaType::getRuntime () {
    if (!runtime)
        runtime = getNewRuntime ();
    return runtime;
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT
SMIL::AVMediaType::AVMediaType (ElementPtr & d, const QString & t)
 : SMIL::MediaType (d, t) {
    duration_time = duration_media; // default stop when movie ends
}

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
    Element::stop ();
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
    getRuntime ()->begin ();
}

KDE_NO_EXPORT void SMIL::Set::stop () {
    getRuntime ()->end ();
}

//-----------------------------------------------------------------------------

void RegionNode::paint (QPainter & p) {
    if (regionElement) {
        QString colstr = regionElement->getAttribute ("background-color");
        if (!colstr.isEmpty ())
            p.fillRect (x, y, w, h, QColor (colstr));
    }
    if (attached_element) {
        ElementRuntimePtr rt = attached_element->getRuntime ();
        if (rt)
            rt->paint (p);
    }
}

void RegionNode::pointerClicked () {
    if (attached_element) {
        kdDebug () << "pointerClicked " << attached_element->nodeName () << endl;
        ElementRuntimePtr rt = attached_element->getRuntime ();
        if (rt)
            rt->emitActivateEvent ();
    }
}

void RegionNode::pointerEntered () {
    has_mouse = true;
    if (attached_element) {
        kdDebug () << "pointerEntered " << attached_element->nodeName () << endl;
        ElementRuntimePtr rt = attached_element->getRuntime ();
        if (rt)
            rt->emitInBoundsEvent ();
    }
}

void RegionNode::pointerLeft () {
    if (attached_element) {
        kdDebug () << "pointerLeft " << attached_element->nodeName () << endl;
        ElementRuntimePtr rt = attached_element->getRuntime ();
        if (rt)
            rt->emitOutOfBoundsEvent ();
    }
    has_mouse = false;
}

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
    Mrl * mrl = media_element ? media_element->mrl () : 0L;
    kdDebug () << "ImageData::ImageData " << (mrl ? mrl->src : QString()) << endl;
    if (mrl && !mrl->src.isEmpty ()) {
        KURL url (mrl->src);
        if (url.isLocalFile ()) {
            QPixmap *pix = new QPixmap (url.path ());
            if (pix->isNull ())
                delete pix;
            else
                d->image = pix;
        }
    }
}

KDE_NO_CDTOR_EXPORT ImageData::~ImageData () {
    delete d;
}

KDE_NO_EXPORT void ImageData::paint (QPainter & p) {
    if (isstarted && d->image && region_node) {
        RegionNode * r = region_node.ptr ();
        p.drawPixmap (QRect (r->x, r->y, r->w, r->h), *d->image);
    }
}

KDE_NO_EXPORT void ImageData::started () {
    if (media_element && d->image) {
        PlayListNotify * n = media_element->document ()->notify_listener;
        if (n && region_node)
            n->repaintRegion (region_node);
    }
    ElementRuntime::started ();
}

KDE_NO_EXPORT void ImageData::slotResult (KIO::Job*) {
}

KDE_NO_EXPORT void ImageData::slotData (KIO::Job*, const QByteArray& qb) {
}

//-----------------------------------------------------------------------------

namespace KMPlayer {
    class TextDataPrivate {
    public:
        TextDataPrivate () : background_color (0xFFFFFF), foreground_color (0), codec (0L), font (QApplication::font ()), transparent (false) {
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
 : MediaTypeRuntime (e) {
    d = new TextDataPrivate;
    Mrl * mrl = media_element ? media_element->mrl () : 0L;
    kdDebug () << "TextData::TextData " << (mrl ? mrl->src : QString()) << endl;
    if (mrl && !mrl->src.isEmpty ()) {
        KURL url (mrl->src);
        if (url.protocol () == "data") {
            QString s = KURL::decode_string (url.url ());
            int pos = s.find (QString ("data:"));
            if (pos > -1) {
                s = s.mid (pos + 5);
                pos = s.find (QChar (','));
                if (pos > -1) {
                    d->data.resize (s.length () - pos);
                    memcpy (d->data.data (), s.mid (pos + 1).ascii (), s.length ()- pos - 1);
                    if (pos > 0)
                        d->codec =QTextCodec::codecForName(s.left(pos).ascii());
                }
            }
        } else if (url.isLocalFile ()) {
            QFile file (url.path ());
            file.open (IO_ReadOnly);
            d->data = file.readAll ();
        } else
            ; // FIXME
    }
    for (ElementPtr p = media_element->firstChild (); p; p = p->nextSibling())
        if (!strcmp (p->nodeName (), "param")) {
            QString name = p->getAttribute ("name");
            QString value = p->getAttribute ("value");
            kdDebug () << "TextData param " << name << "=" << value << endl;
            if (name == QString ("backgroundColor"))
                d->background_color = QColor (value).rgb ();
            else if (name == QString ("fontColor"))
                d->foreground_color = QColor (value).rgb ();
            else if (name == QString ("charset"))
                d->codec = QTextCodec::codecForName (value.ascii ());
            else if (name == QString ("fontFace"))
                ; //FIXME
            else if (name == QString ("fontPtSize"))
                d->font.setPointSize (value.toInt ());
            else if (name == QString ("fontSize"))
                d->font.setPointSize (d->font.pointSize () + value.toInt ());
            // TODO: expandTabs fontBackgroundColor fontSize fontStyle fontWeight hAlig vAlign wordWrap
        }
}

KDE_NO_CDTOR_EXPORT TextData::~TextData () {
    delete d;
}

KDE_NO_EXPORT void TextData::paint (QPainter & p) {
    if (isstarted && region_node) {
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
    if (media_element && d->data.size () > 0) {
        PlayListNotify * n = media_element->document ()->notify_listener;
        if (n && region_node)
            n->repaintRegion (region_node);
    }
    ElementRuntime::started ();
}

KDE_NO_EXPORT void TextData::slotResult (KIO::Job*) {
}

KDE_NO_EXPORT void TextData::slotData (KIO::Job*, const QByteArray& qb) {
}

#include "kmplayer_smil.moc"
