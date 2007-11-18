/**
 * Copyright (C) 2006 by Koos Vriezen <koos.vriezen@gmail.com>
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

#include <qcolor.h>
#include <qimage.h>
#include <qtimer.h>

#include <kdebug.h>

#include "kmplayer_rp.h"
#include "kmplayer_smil.h"
#include "mediaobject.h"

using namespace KMPlayer;


KDE_NO_CDTOR_EXPORT RP::Imfl::Imfl (NodePtr & d)
  : Mrl (d, id_node_imfl),
    fit (fit_hidden),
    duration (0),
    needs_scene_img (0) {}

KDE_NO_CDTOR_EXPORT RP::Imfl::~Imfl () {
}

KDE_NO_EXPORT void RP::Imfl::closed () {
    for (NodePtr n = firstChild (); n; n = n->nextSibling ())
        if (RP::id_node_head == n->id) {
            AttributePtr a = convertNode <Element> (n)->attributes ()->first ();
            for (; a; a = a->nextSibling ()) {
                if (StringPool::attr_width == a->name ()) {
                    width = a->value ().toInt ();
                } else if (StringPool::attr_height == a->name ()) {
                    height = a->value ().toInt ();
                } else if (a->name () == "duration") {
                    int dur;
                    parseTime (a->value ().lower (), dur);
                    duration = dur;
                }
            }
        }
}

KDE_NO_EXPORT void RP::Imfl::defer () {
    kdDebug () << "RP::Imfl::defer " << endl;
    setState (state_deferred);
    for (Node * n = firstChild ().ptr (); n; n = n->nextSibling ().ptr ())
        if (n->id == RP::id_node_image && !n->active ())
            n->activate ();
}

KDE_NO_EXPORT void RP::Imfl::activate () {
    kdDebug () << "RP::Imfl::activate " << endl;
    resolved = true;
    setState (state_activated);
    int timings_count = 0;
    for (NodePtr n = firstChild (); n; n = n->nextSibling ())
        switch (n->id) {
            case RP::id_node_crossfade:
            case RP::id_node_fadein:
            case RP::id_node_fadeout:
            case RP::id_node_fill:
            case RP::id_node_wipe:
            case RP::id_node_viewchange:
                n->activate (); // set their start timers
                timings_count++;
                break;
            case RP::id_node_image:
                if (!n->active ())
                    n->activate ();
                break;
        }
    if (duration > 0)
        duration_timer = document ()->setTimeout (this, duration * 100);
    else if (!timings_count)
        finish ();
}

KDE_NO_EXPORT void RP::Imfl::finish () {
    kdDebug () << "RP::Imfl::finish " << endl;
    Mrl::finish ();
    if (duration_timer) {
        document ()->cancelTimer (duration_timer);
        duration_timer = 0;
    }
    for (NodePtr n = firstChild (); n; n = n->nextSibling ())
        if (n->unfinished ())
            n->finish ();
}

KDE_NO_EXPORT void RP::Imfl::childDone (NodePtr) {
    if (unfinished () && !duration_timer) {
        for (NodePtr n = firstChild (); n; n = n->nextSibling ())
            switch (n->id) {
                case RP::id_node_crossfade:
                case RP::id_node_fadein:
                case RP::id_node_fadeout:
                case RP::id_node_fill:
                    if (n->unfinished ())
                        return;
            }
        finish ();
    }
}

KDE_NO_EXPORT void RP::Imfl::deactivate () {
    kdDebug () << "RP::Imfl::deactivate " << endl;
    if (unfinished ())
        finish ();
    if (!active ())
        return; // calling finish might call deactivate() as well
    setState (state_deactivated);
    for (NodePtr n = firstChild (); n; n = n->nextSibling ())
        if (n->active ())
            n->deactivate ();
    rp_surface = Mrl::getSurface (0L);
}

KDE_NO_EXPORT bool RP::Imfl::handleEvent (EventPtr event) {
    if (event->id () == event_timer) {
        TimerEvent * te = static_cast <TimerEvent *> (event.ptr ());
        if (te->timer_info == duration_timer) {
            kdDebug () << "RP::Imfl timer " << duration << endl;
            duration_timer = 0;
            if (unfinished ())
                finish ();
        }
    }
    return true;
}

KDE_NO_EXPORT void RP::Imfl::accept (Visitor * v) {
    v->visit (this);
}

KDE_NO_EXPORT Surface *RP::Imfl::surface () {
    if (!rp_surface) {
        rp_surface = Mrl::getSurface (this);
        if (rp_surface) {
            if (width <= 0 || width > 32000)
                width = rp_surface->bounds.width ();
            if (height <= 0 || height > 32000)
                height = rp_surface->bounds.height ();
        }
    }
    return rp_surface.ptr ();
}

KDE_NO_EXPORT NodePtr RP::Imfl::childFromTag (const QString & tag) {
    const char * ctag = tag.latin1 ();
    if (!strcmp (ctag, "head"))
        return new DarkNode (m_doc, "head", RP::id_node_head);
    else if (!strcmp (ctag, "image"))
        return new RP::Image (m_doc);
    else if (!strcmp (ctag, "fill"))
        return new RP::Fill (m_doc);
    else if (!strcmp (ctag, "wipe"))
        return new RP::Wipe (m_doc);
    else if (!strcmp (ctag, "viewchange"))
        return new RP::ViewChange (m_doc);
    else if (!strcmp (ctag, "crossfade"))
        return new RP::Crossfade (m_doc);
    else if (!strcmp (ctag, "fadein"))
        return new RP::Fadein (m_doc);
    else if (!strcmp (ctag, "fadeout"))
        return new RP::Fadeout (m_doc);
    return 0L;
}

KDE_NO_EXPORT void RP::Imfl::repaint () {
    if (!active ())
        kdWarning () << "Spurious Imfl repaint" << endl;
    else if (surface () && width > 0 && height > 0)
        rp_surface->repaint (SRect (0, 0, width, height));
}

KDE_NO_CDTOR_EXPORT RP::Image::Image (NodePtr & doc)
 : Mrl (doc, id_node_image)
{}

KDE_NO_CDTOR_EXPORT RP::Image::~Image () {
}

KDE_NO_EXPORT void RP::Image::closed () {
    src = getAttribute (StringPool::attr_name);
}

KDE_NO_EXPORT void RP::Image::activate () {
    kdDebug () << "RP::Image::activate" << endl;
    setState (state_activated);
    isPlayable (); // update src attribute
    if (!media_object) {
        PlayListNotify *n = document()->notify_listener;
        if (n)
            media_object = n->mediaManager ()->createMedia (MediaManager::Image, this);
    }
    media_object->wget (absolutePath ());
}

KDE_NO_EXPORT void RP::Image::begin () {
    Node::begin ();
}

KDE_NO_EXPORT void RP::Image::deactivate () {
    if (img_surface) {
        img_surface->remove ();
        img_surface = NULL;
    }
    setState (state_deactivated);
    postpone_lock = 0L;
    if (media_object) {
        media_object->destroy ();
        media_object = NULL;
    }
}

bool RP::Image::handleEvent (EventPtr event) {
    if (event->id () != event_data_arrived)
        return Mrl::handleEvent (event);
    dataArrived ();
    return true;
}

KDE_NO_EXPORT void RP::Image::dataArrived () {
    kdDebug () << "RP::Image::remoteReady" << endl;
    ImageMedia *im = static_cast <ImageMedia *> (media_object);
    if (!im->isEmpty ()) {
        width = im->cached_img->image->width ();
        height = im->cached_img->image->height ();
    }
    postpone_lock = 0L;
}

KDE_NO_EXPORT bool RP::Image::isReady (bool postpone_if_not) {
    if (media_object->downloading () && postpone_if_not)
        postpone_lock = document ()->postpone ();
    return !media_object->downloading ();
}

KDE_NO_EXPORT Surface *RP::Image::surface () {
    ImageMedia *im = static_cast <ImageMedia *> (media_object);
    if (im && !img_surface && !im->isEmpty ()) {
        Node * p = parentNode ().ptr ();
        if (p && p->id == RP::id_node_imfl) {
            Surface *ps = static_cast <RP::Imfl *> (p)->surface ();
            if (ps)
                img_surface = ps->createSurface (this,
                        SRect (0, 0, width, height));
        }
    }
    return img_surface;
}

KDE_NO_CDTOR_EXPORT RP::TimingsBase::TimingsBase (NodePtr & d, const short i)
 : Element (d, i), x (0), y (0), w (0), h (0), start (0), duration (0) {}

KDE_NO_EXPORT void RP::TimingsBase::activate () {
    setState (state_activated);
    x = y = w = h = 0;
    srcx = srcy = srcw = srch = 0;
    for (Attribute * a= attributes ()->first ().ptr (); a; a = a->nextSibling ().ptr ()) {
        if (a->name () == StringPool::attr_target) {
            for (NodePtr n = parentNode()->firstChild(); n; n= n->nextSibling())
                if (convertNode <Element> (n)->
                        getAttribute ("handle") == a->value ())
                    target = n;
        } else if (a->name () == "start") {
            int dur;
            parseTime (a->value ().lower (), dur);
            start = dur;
        } else if (a->name () == "duration") {
            int dur;
            parseTime (a->value ().lower (), dur);
            duration = dur;
        } else if (a->name () == "dstx") {
            x = a->value ().toInt ();
        } else if (a->name () == "dsty") {
            y = a->value ().toInt ();
        } else if (a->name () == "dstw") {
            w = a->value ().toInt ();
        } else if (a->name () == "dsth") {
            h = a->value ().toInt ();
        } else if (a->name () == "srcx") {
            srcx = a->value ().toInt ();
        } else if (a->name () == "srcy") {
            srcy = a->value ().toInt ();
        } else if (a->name () == "srcw") {
            srcw = a->value ().toInt ();
        } else if (a->name () == "srch") {
            srch = a->value ().toInt ();
        }
    }
    start_timer = document ()->setTimeout (this, start *100);
}

KDE_NO_EXPORT void RP::TimingsBase::deactivate () {
    if (unfinished ())
        finish ();
    setState (state_deactivated);
}

KDE_NO_EXPORT bool RP::TimingsBase::handleEvent (EventPtr event) {
    if (event->id () == event_timer) {
        TimerEvent * te = static_cast <TimerEvent *> (event.ptr ());
        if (te->timer_info == update_timer && duration > 0) {
            update (100 * ++curr_step / duration);
            te->interval = true;
        } else if (te->timer_info == start_timer) {
            start_timer = 0;
            duration_timer = document ()->setTimeout (this, duration * 100);
            begin ();
        } else if (te->timer_info == duration_timer) {
            duration_timer = 0;
            update (100);
            finish ();
        } else
            return false;
        return true;
    } else if (event->id () == event_postponed) {
        if (!static_cast <PostponedEvent *> (event.ptr ())->is_postponed) {
            document_postponed = 0L; // disconnect
            update (duration > 0 ? 0 : 100);
        }
    }
    return false;
}

KDE_NO_EXPORT void RP::TimingsBase::begin () {
    progress = 0;
    setState (state_began);
    if (target)
        target->begin ();
    if (duration > 0) {
        steps = duration; // 10/s updates
        update_timer = document ()->setTimeout (this, 100); // 50ms
        curr_step = 1;
    }
}

KDE_NO_EXPORT void RP::TimingsBase::update (int percentage) {
    progress = percentage;
    Node * p = parentNode ().ptr ();
    if (p->id == RP::id_node_imfl)
        static_cast <RP::Imfl *> (p)->repaint ();
}

KDE_NO_EXPORT void RP::TimingsBase::finish () {
    progress = 100;
    if (start_timer) {
        document ()->cancelTimer (start_timer);
        start_timer = 0;
    } else if (duration_timer) {
        document ()->cancelTimer (duration_timer);
        duration_timer = 0;
    }
    if (update_timer) {
        document ()->cancelTimer (update_timer);
        update_timer = 0;
    }
    document_postponed = 0L; // disconnect
    Element::finish ();
}

KDE_NO_EXPORT void RP::Crossfade::activate () {
    TimingsBase::activate ();
}

KDE_NO_EXPORT void RP::Crossfade::begin () {
    //kdDebug () << "RP::Crossfade::begin" << endl;
    TimingsBase::begin ();
    if (target && target->id == id_node_image) {
        RP::Image * img = static_cast <RP::Image *> (target.ptr ());
        if (!img->isReady (true))
            document_postponed = document()->connectTo (this, event_postponed);
        else
            update (duration > 0 ? 0 : 100);
    }
}

KDE_NO_EXPORT void RP::Crossfade::accept (Visitor * v) {
    v->visit (this);
}

KDE_NO_EXPORT void RP::Fadein::activate () {
    // pickup color from Fill that should be declared before this node
    from_color = 0;
    TimingsBase::activate ();
}

KDE_NO_EXPORT void RP::Fadein::begin () {
    //kdDebug () << "RP::Fadein::begin" << endl;
    TimingsBase::begin ();
    if (target && target->id == id_node_image) {
        RP::Image * img = static_cast <RP::Image *> (target.ptr ());
        if (!img->isReady (true))
            document_postponed = document()->connectTo (this, event_postponed);
        else
            update (duration > 0 ? 0 : 100);
    }
}

KDE_NO_EXPORT void RP::Fadein::accept (Visitor * v) {
    v->visit (this);
}

KDE_NO_EXPORT void RP::Fadeout::activate () {
    to_color = QColor (getAttribute ("color")).rgb ();
    TimingsBase::activate ();
}

KDE_NO_EXPORT void RP::Fadeout::begin () {
    //kdDebug () << "RP::Fadeout::begin" << endl;
    TimingsBase::begin ();
}

KDE_NO_EXPORT void RP::Fadeout::accept (Visitor * v) {
    v->visit (this);
}

KDE_NO_EXPORT void RP::Fill::activate () {
    color = QColor (getAttribute ("color")).rgb ();
    TimingsBase::activate ();
}

KDE_NO_EXPORT void RP::Fill::begin () {
    setState (state_began);
    update (0);
}

KDE_NO_EXPORT void RP::Fill::accept (Visitor * v) {
    v->visit (this);
}

KDE_NO_EXPORT void RP::Wipe::activate () {
    //TODO implement 'type="push"'
    QString dir = getAttribute ("direction").lower ();
    direction = dir_right;
    if (dir == QString::fromLatin1 ("left"))
        direction = dir_left;
    else if (dir == QString::fromLatin1 ("up"))
        direction = dir_up;
    else if (dir == QString::fromLatin1 ("down"))
        direction = dir_down;
    TimingsBase::activate ();
}

KDE_NO_EXPORT void RP::Wipe::begin () {
    //kdDebug () << "RP::Wipe::begin" << endl;
    TimingsBase::begin ();
    if (target && target->id == id_node_image) {
        RP::Image * img = static_cast <RP::Image *> (target.ptr ());
        if (!img->isReady (true))
            document_postponed = document()->connectTo (this, event_postponed);
        else
            update (duration > 0 ? 0 : 100);
    }
}

KDE_NO_EXPORT void RP::Wipe::accept (Visitor * v) {
    v->visit (this);
}

KDE_NO_EXPORT void RP::ViewChange::activate () {
    TimingsBase::activate ();
}

KDE_NO_EXPORT void RP::ViewChange::begin () {
    kdDebug () << "RP::ViewChange::begin" << endl;
    setState (state_began);
    Node * p = parentNode ().ptr ();
    if (p->id == RP::id_node_imfl)
        static_cast <RP::Imfl *> (p)->needs_scene_img++;
    update (0);
}

KDE_NO_EXPORT void RP::ViewChange::finish () {
    Node * p = parentNode ().ptr ();
    if (p && p->id == RP::id_node_imfl)
        static_cast <RP::Imfl *> (p)->needs_scene_img--;
    TimingsBase::finish ();
}

KDE_NO_EXPORT void RP::ViewChange::accept (Visitor * v) {
    v->visit (this);
}
