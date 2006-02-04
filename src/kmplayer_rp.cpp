/**
 * Copyright (C) 2006 by Koos Vriezen <koos ! vriezen ? xs4all ! nl>
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
#include <qpainter.h>
#include <qpixmap.h>
#include <qimage.h>
#include <qtimer.h>

#include <kdebug.h>
#include <kurl.h>

#include "kmplayer_rp.h"

using namespace KMPlayer;


KDE_NO_CDTOR_EXPORT RP::Imfl::Imfl (NodePtr & d)
  : Mrl (d, id_node_imfl),
    x (0), y (0), w (0), h (0),
    fit (fit_hidden),
    width (0), height (0), duration (0), image (0L) {}

KDE_NO_CDTOR_EXPORT RP::Imfl::~Imfl () {
    delete image;
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
    setState (state_activated);
    int timings_count = 0;
    for (NodePtr n = firstChild (); n; n = n->nextSibling ())
        switch (n->id) {
            case RP::id_node_crossfade:
            case RP::id_node_fadein:
            case RP::id_node_fadeout:
            case RP::id_node_fill:
            case RP::id_node_wipe:
                n->activate (); // set their start timers
                timings_count++;
                break;
            case RP::id_node_image:
                if (!n->active ())
                    n->activate ();
            case RP::id_node_head:
                for (AttributePtr a= convertNode <Element> (n)->attributes ()->first (); a; a = a->nextSibling ()) {
                    if (!strcmp (a->nodeName (), "width")) {
                        width = a->nodeValue ().toInt ();
                    } else if (!strcmp (a->nodeName (), "height")) {
                        height = a->nodeValue ().toInt ();
                    } else if (!strcmp (a->nodeName (), "duration")) {
                        parseTime (a->nodeValue ().lower (), duration);
                    }
                }
                break;
        }
    if (width <= 0 || width > 32000)
        width = w;
    if (height <= 0 || height > 32000)
        height = h;
    if (width > 0 && height > 0) {
        image = new QPixmap (width, height);
        image->fill ();
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
    setState (state_deactivated);
    for (NodePtr n = firstChild (); n; n = n->nextSibling ())
        if (n->active ())
            n->deactivate ();
    delete image;
    image = 0L;
}

KDE_NO_EXPORT bool RP::Imfl::handleEvent (EventPtr event) {
    if (event->id () == event_sized) {
        SizeEvent * e = static_cast <SizeEvent *> (event.ptr ());
        x = e->x ();
        y = e->y ();
        w = e->w ();
        h = e->h ();
        fit = e->fit;
        matrix = e->matrix;
        kdDebug () << "RP::Imfl sized: " << x << "," << y << " " << w << "x" << h << endl;
    } else if (event->id () == event_paint) {
        if (active () && image) {
            PaintEvent * p = static_cast <PaintEvent *> (event.ptr ());
            kdDebug () << "RP::Imfl paint: " << x << "," << y << " " << w << "x" << h << endl;
            if (w == width && h == height) {
                p->painter.drawPixmap (x, y, *image);
            } else {
                int x1=0, y1=0, w1=width, h1=height;
                if (fit == fit_fill) {
                    w1 = w;
                    h1 = h;
                } else
                    matrix.getXYWH (x1, y1, w1, h1);
                QImage img;
                img = *image;
                p->painter.drawImage (x, y, img.scale (w1, h1));
            }
        }
    } else if (event->id () == event_timer) {
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
    else if (!strcmp (ctag, "crossfade"))
        return new RP::Crossfade (m_doc);
    else if (!strcmp (ctag, "fadein"))
        return new RP::Fadein (m_doc);
    else if (!strcmp (ctag, "fadeout"))
        return new RP::Fadeout (m_doc);
    return 0L;
}

KDE_NO_EXPORT void RP::Imfl::repaint () {
    PlayListNotify * n = document()->notify_listener;
    if (!active ())
        kdWarning () << "Spurious Imfl repaint" << endl;
    else if (n && w > 0 && h > 0)
        n->repaintRect (x, y, w, h);
}

KDE_NO_CDTOR_EXPORT RP::Image::Image (NodePtr & doc)
 : Mrl (doc, id_node_image), image (0L) {}

KDE_NO_CDTOR_EXPORT RP::Image::~Image () {
    delete image;
}

KDE_NO_EXPORT void RP::Image::closed () {
    src = getAttribute ("name");
}

KDE_NO_EXPORT void RP::Image::activate () {
    kdDebug () << "RP::Image::activate" << endl;
    setState (state_activated);
    isMrl (); // update src attribute
    wget (src);
}

KDE_NO_EXPORT void RP::Image::deactivate () {
    setState (state_deactivated);
    if (ready_waiter) {
        document ()->proceed ();
        ready_waiter = 0L;
    }
}


KDE_NO_EXPORT void RP::Image::remoteReady () {
    kdDebug () << "RP::Image::remoteReady" << endl;
    if (!m_data.isEmpty ()) {
        QPixmap *pix = new QPixmap (m_data);
        if (!pix->isNull ()) {
            image = pix;
        } else
            delete pix;
    }
    if (ready_waiter) {
        document ()->proceed ();
        ready_waiter->begin ();
        ready_waiter = 0L;
    }
    kdDebug () << "RP::Image::remoteReady " << (void *) image << endl;
}

KDE_NO_EXPORT bool RP::Image::isReady () {
    return !m_job;
}

KDE_NO_CDTOR_EXPORT RP::TimingsBase::TimingsBase (NodePtr & d, const short i)
 : Element (d, i), start (0), duration (0), x (0), y (0), w (0), h (0) {}

KDE_NO_EXPORT void RP::TimingsBase::activate () {
    setState (state_activated);
    for (Attribute * a= attributes ()->first ().ptr (); a; a = a->nextSibling ().ptr ()) {
        if (!strcasecmp (a->nodeName (), "start"))
            parseTime (a->nodeValue ().lower (), start);
        else if (!strcasecmp (a->nodeName (), "duration"))
            parseTime (a->nodeValue ().lower (), duration);
        else if (!strcasecmp (a->nodeName (), "target")) {
            for (NodePtr n = parentNode()->firstChild(); n; n= n->nextSibling())
                if (convertNode <Element> (n)->getAttribute ("handle") == a->nodeValue ())
                    target = n;
        } else if (!strcasecmp (a->nodeName (), "dstx")) {
            x = a->nodeValue ().toInt ();
        } else if (!strcasecmp (a->nodeName (), "dsth")) {
            h = a->nodeValue ().toInt ();
        } else if (!strcasecmp (a->nodeName (), "dstw")) {
            w = a->nodeValue ().toInt ();
        } else if (!strcasecmp (a->nodeName (), "dsth")) {
            h = a->nodeValue ().toInt ();
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
        if (te->timer_info == start_timer) {
            start_timer = 0;
            duration_timer = document ()->setTimeout (this, duration * 100);
            begin ();
        } else if (te->timer_info == duration_timer) {
            duration_timer = 0;
            finish ();
        } else
            return false;
        return true;
    }
    return false;
}

KDE_NO_EXPORT void RP::TimingsBase::begin () {
    setState (state_began);
    if (target)
        target->begin ();
}

KDE_NO_EXPORT void RP::TimingsBase::finish () {
    if (start_timer) {
        document ()->cancelTimer (start_timer);
        start_timer = 0;
    }
    Element::finish ();
}

KDE_NO_EXPORT void RP::Crossfade::activate () {
    TimingsBase::activate ();
}

KDE_NO_EXPORT void RP::Crossfade::begin () {
    kdDebug () << "RP::Crossfade::begin" << endl;
    TimingsBase::begin ();
    Node * p = parentNode ().ptr ();
    if (p->id != RP::id_node_imfl) {
        kdWarning () << "crossfade begin: no imfl parent found" << endl;
        return;
    }
    RP::Imfl * imfl = static_cast <RP::Imfl *> (p);
    if (imfl->image && target && target->id == id_node_image) {
        RP::Image * img = static_cast <RP::Image *> (target.ptr ());
    kdDebug () << "RP::Crossfade::begin img ready:" << img->isReady () << endl;
        if (img->isReady ()) {
            if (img->image) {
                QPainter * painter = new QPainter ();
                painter->begin (imfl->image);
                painter->drawPixmap (x, y, *img->image);
                painter->end ();
                imfl->repaint ();
            }
        } else {
            document ()->postpone ();
            img->ready_waiter = this;
        }
    }
}

KDE_NO_EXPORT void RP::Fadein::activate () {
    TimingsBase::activate ();
}

KDE_NO_EXPORT void RP::Fadein::begin () {
    kdDebug () << "RP::Fadein::begin" << endl;
    TimingsBase::begin ();
    Node * p = parentNode ().ptr ();
    if (p->id != RP::id_node_imfl) {
        kdWarning () << "fadein begin: no imfl parent found" << endl;
        return;
    }
    RP::Imfl * imfl = static_cast <RP::Imfl *> (p);
    if (imfl->image && target && target->id == id_node_image) {
        RP::Image * img = static_cast <RP::Image *> (target.ptr ());
    kdDebug () << "RP::Fadein::begin img ready:" << img->isReady () << endl;
        if (img->isReady ()) {
            if (img->image) {
                QPainter painter;
                painter.begin (imfl->image);
                painter.drawPixmap (x, y, *img->image);
                painter.end ();
                imfl->repaint ();
            }
        } else {
            document ()->postpone ();
            img->ready_waiter = this;
        }
    }
}

KDE_NO_EXPORT void RP::Fadeout::activate () {
    TimingsBase::activate ();
}

KDE_NO_EXPORT void RP::Fadeout::begin () {
    kdDebug () << "RP::Fadeout::begin" << endl;
    TimingsBase::begin ();
    Node * p = parentNode ().ptr ();
    if (p->id == RP::id_node_imfl) {
        RP::Imfl * imfl = static_cast <RP::Imfl *> (p);
        if (imfl->image) {
            if (!w || !h) {
                imfl->image->fill (QColor (getAttribute ("color")).rgb ());
            } else {
                QPainter painter;
                painter.begin (imfl->image);
                painter.fillRect (x, y, w, h, QBrush (QColor (getAttribute ("color"))));
                painter.end ();
            }
            imfl->repaint ();
        }
    }
}

KDE_NO_EXPORT void RP::Fill::activate () {
    TimingsBase::activate ();
}

KDE_NO_EXPORT void RP::Fill::begin () {
    TimingsBase::begin ();
    Node * p = parentNode ().ptr ();
    if (p->id == RP::id_node_imfl) {
        RP::Imfl * imfl = static_cast <RP::Imfl *> (p);
        if (imfl->image) {
            if (!w || !h) {
                imfl->image->fill (QColor (getAttribute ("color")).rgb ());
            } else {
                QPainter painter;
                painter.begin (imfl->image);
                painter.fillRect (x, y, w, h, QBrush (QColor (getAttribute ("color"))));
                painter.end ();
            }
            imfl->repaint ();
        }
    }
}

KDE_NO_EXPORT void RP::Wipe::activate () {
    TimingsBase::activate ();
}

KDE_NO_EXPORT void RP::Wipe::begin () {
    kdDebug () << "RP::Wipe::begin" << endl;
    TimingsBase::begin ();
    Node * p = parentNode ().ptr ();
    if (p->id != RP::id_node_imfl) {
        kdWarning () << "wipe begin: no imfl parent found" << endl;
        return;
    }
    RP::Imfl * imfl = static_cast <RP::Imfl *> (p);
    if (imfl->image && target && target->id == id_node_image) {
        RP::Image * img = static_cast <RP::Image *> (target.ptr ());
    kdDebug () << "RP::Wipe::begin img ready:" << img->isReady () << endl;
        if (img->isReady ()) {
            if (img->image) {
                QPainter * painter = new QPainter ();
                painter->begin (imfl->image);
                painter->drawPixmap (x, y, *img->image);
                painter->end ();
                imfl->repaint ();
            }
        } else {
            document ()->postpone ();
            img->ready_waiter = this;
        }
    }
}

#include "kmplayer_rp.moc"
