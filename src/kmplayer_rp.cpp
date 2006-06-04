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
#include "kmplayer_smil.h"

using namespace KMPlayer;


KDE_NO_CDTOR_EXPORT RP::Imfl::Imfl (NodePtr & d)
  : Mrl (d, id_node_imfl),
    x (0), y (0), w (0), h (0),
    fit (fit_hidden),
    width (0), height (0), duration (0),
    image (0L), cached_image (0L) {}

KDE_NO_CDTOR_EXPORT RP::Imfl::~Imfl () {
    delete image;
    delete cached_image;
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
    if (parentNode ())
        parentNode ()->registerEventHandler (this);
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
    delete image;
    image = 0L;
    invalidateCachedImage ();
    if (parentNode ())
        parentNode ()->deregisterEventHandler (this);
}

KDE_NO_EXPORT void RP::Imfl::invalidateCachedImage () {
    delete cached_image;
    cached_image = 0L;
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
        //kdDebug () << "RP::Imfl sized: " << x << "," << y << " " << w << "x" << h << endl;
    } else if (event->id () == event_paint) {
        if (active () && image) {
            PaintEvent * p = static_cast <PaintEvent *> (event.ptr ());
            //kdDebug () << "RP::Imfl paint: " << x << "," << y << " " << w << "x" << h << endl;
            if (w == width && h == height) {
                p->painter.drawPixmap (x, y, *image);
            } else {
                int x1=0, y1=0, w1=width, h1=height;
                if (fit == fit_fill) {
                    w1 = w;
                    h1 = h;
                } else
                    matrix.getXYWH (x1, y1, w1, h1);
                if (!cached_image ||
                        cached_image->width () != w1 ||
                        cached_image->height () != h1) {
                    delete cached_image;
                    QImage img;
                    img = *image;
                    cached_image = new QPixmap (img.scale (w1, h1));
                }
                p->painter.drawPixmap (x, y, *cached_image);
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
 : Mrl (doc, id_node_image), proceed_on_ready (false), image (0L) {}

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
    wget (absolutePath ());
}

KDE_NO_EXPORT void RP::Image::deactivate () {
    setState (state_deactivated);
    if (proceed_on_ready) {
        proceed_on_ready = false;
        document ()->proceed ();
    }
}


KDE_NO_EXPORT void RP::Image::remoteReady (QByteArray & data) {
    kdDebug () << "RP::Image::remoteReady" << endl;
    if (!data.isEmpty ()) {
        QImage * img = new QImage (data);
        if (!img->isNull ()) {
            image = img;
            image->setAlphaBuffer (true);
        } else
            delete img;
    }
    if (proceed_on_ready) {
        proceed_on_ready = false;
        document ()->proceed ();
    }
    kdDebug () << "RP::Image::remoteReady " << (void *) image << endl;
}

KDE_NO_EXPORT bool RP::Image::isReady (bool postpone_if_not) {
    if (downloading () && !proceed_on_ready && postpone_if_not) {
        proceed_on_ready = true;
        document ()->postpone ();
    }
    return !downloading ();
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
        } else if (!strcasecmp (a->nodeName (), "dsty")) {
            y = a->nodeValue ().toInt ();
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
    setState (state_began);
    if (target)
        target->begin ();
    if (duration > 0) {
        steps = duration; // 10/s updates
        update_timer = document ()->setTimeout (this, 100); // 50ms
        curr_step = 1;
    }
}

KDE_NO_EXPORT void RP::TimingsBase::update (int /*percentage*/) {
}

KDE_NO_EXPORT void RP::TimingsBase::finish () {
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
    kdDebug () << "RP::Crossfade::begin" << endl;
    TimingsBase::begin ();
    if (target && target->id == id_node_image) {
        RP::Image * img = static_cast <RP::Image *> (target.ptr ());
        if (!img->isReady (true))
            document_postponed = document()->connectTo (this, event_postponed);
        else
            update (duration > 0 ? 0 : 100);
    }
}

KDE_NO_EXPORT void RP::Crossfade::update (int percentage) {
    if (percentage > 0 && percentage < 100)
        return; // we do no crossfading yet, just paint the first and last time
    Node * p = parentNode ().ptr ();
    if (p->id != RP::id_node_imfl) {
        kdWarning () << "crossfade update: no imfl parent found" << endl;
        return;
    }
    RP::Imfl * imfl = static_cast <RP::Imfl *> (p);
    if (imfl->image && target && target->id == id_node_image) {
        RP::Image * img = static_cast <RP::Image *> (target.ptr ());
        if (img->image) {
            QPainter painter;
            painter.begin (imfl->image);
            painter.drawImage (x, y, *img->image);
            painter.end ();
            imfl->invalidateCachedImage ();
            imfl->repaint ();
        }
    }
}

KDE_NO_EXPORT void RP::Fadein::activate () {
    // pickup color from Fill that should be declared before this node
    from_color = 0;
    for (NodePtr n = previousSibling (); n; n = n->previousSibling ())
        if (n->id == id_node_fill) {
            from_color = convertNode <RP::Fill> (n)->fillColor ();
            break;
        }
    TimingsBase::activate ();
}

KDE_NO_EXPORT void RP::Fadein::begin () {
    kdDebug () << "RP::Fadein::begin" << endl;
    TimingsBase::begin ();
    if (target && target->id == id_node_image) {
        RP::Image * img = static_cast <RP::Image *> (target.ptr ());
        if (!img->isReady (true))
            document_postponed = document()->connectTo (this, event_postponed);
        else
            update (duration > 0 ? 0 : 100);
    }
}

KDE_NO_EXPORT void RP::Fadein::update (int percentage) {
    Node * p = parentNode ().ptr ();
    if (p->id != RP::id_node_imfl) {
        kdWarning () << "fadein begin: no imfl parent found" << endl;
        return;
    }
    RP::Imfl * imfl = static_cast <RP::Imfl *> (p);
    if (imfl->image && target && target->id == id_node_image) {
        RP::Image * img = static_cast <RP::Image *> (target.ptr ());
        if (img->image) {
            QPainter painter;
            painter.begin (imfl->image);
            painter.drawImage (x, y, *img->image);
            //QImage alpha (img->image->width(), img->image->height(), img->image->depth ());
            //alpha.setAlphaBuffer (true);
            //alpha.fill (((0x00ff * (100 - percentage) / 100) << 24) | (0xffffff & from_color));
            //painter.fillRect (x, y, img->image->width(), img->image->height(), QBrush (QColor (((0x00ff * (100 - percentage) / 100) << 24) | (0xffffff & from_color)), Qt::Dense7Pattern));
            if (percentage < 90) {
                int brush_pat = ((int) Qt::SolidPattern) + 10 * percentage / 125;
                painter.fillRect (x, y, img->image->width(), img->image->height(), QBrush (QColor (from_color), (Qt::BrushStyle) brush_pat));
            }
            //painter.drawImage (x, y, alpha, Qt::OrderedAlphaDither);
            painter.end ();
            imfl->invalidateCachedImage ();
            imfl->repaint ();
        }
    }
}

KDE_NO_EXPORT void RP::Fadeout::activate () {
    to_color = QColor (getAttribute ("color")).rgb ();
    TimingsBase::activate ();
}

KDE_NO_EXPORT void RP::Fadeout::begin () {
    kdDebug () << "RP::Fadeout::begin" << endl;
    TimingsBase::begin ();
}

KDE_NO_EXPORT void RP::Fadeout::update (int percentage) {
    Node * p = parentNode ().ptr ();
    if (p->id == RP::id_node_imfl) {
        RP::Imfl * imfl = static_cast <RP::Imfl *> (p);
        if (imfl->image) {
            int brush_pat = ((int) Qt::Dense7Pattern) - 10 * percentage / 126;
            int pw = w;
            int ph = h;
            if (!w || !h) {
                pw = imfl->image->width ();
                ph = imfl->image->height ();
            }
            QPainter painter;
            painter.begin (imfl->image);
            painter.fillRect (x, y, pw, ph, QBrush (QColor (to_color), (Qt::BrushStyle) brush_pat));
            painter.end ();
            imfl->invalidateCachedImage ();
            imfl->repaint ();
        }
    }
}

KDE_NO_EXPORT void RP::Fill::activate () {
    color = QColor (getAttribute ("color")).rgb ();
    TimingsBase::activate ();
}

KDE_NO_EXPORT void RP::Fill::begin () {
    TimingsBase::begin ();
    Node * p = parentNode ().ptr ();
    if (p->id == RP::id_node_imfl) {
        RP::Imfl * imfl = static_cast <RP::Imfl *> (p);
        if (imfl->image) {
            if (!w || !h) {
                imfl->image->fill (color);
            } else {
                QPainter painter;
                painter.begin (imfl->image);
                painter.fillRect (x, y, w, h, QBrush (color));
                painter.end ();
            }
            imfl->invalidateCachedImage ();
            imfl->repaint ();
        }
    }
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
    kdDebug () << "RP::Wipe::begin" << endl;
    TimingsBase::begin ();
    if (target && target->id == id_node_image) {
        RP::Image * img = static_cast <RP::Image *> (target.ptr ());
        if (!img->isReady (true))
            document_postponed = document()->connectTo (this, event_postponed);
        else
            update (duration > 0 ? 0 : 100);
    }
}

KDE_NO_EXPORT void RP::Wipe::update (int percentage) {
    Node * p = parentNode ().ptr ();
    if (p->id != RP::id_node_imfl) {
        kdWarning () << "wipe update: no imfl parent found" << endl;
        return;
    }
    RP::Imfl * imfl = static_cast <RP::Imfl *> (p);
    if (imfl->image && target && target->id == id_node_image) {
        RP::Image * img = static_cast <RP::Image *> (target.ptr ());
        if (img->image) {
            QPainter painter;
            painter.begin (imfl->image);
            int dx = x, dy = y;
            int sx = 0, sy = 0;
            int sw = img->image->width ();
            int sh = img->image->height ();
            if (direction == dir_right) {
                int iw = sw * percentage / 100;
                sx = sw - iw;
                sw = iw;
            } else if (direction == dir_left) {
                int iw = sw * percentage / 100;
                dx += sw - iw;
                sw = iw;
            } else if (direction == dir_down) {
                int ih = sh * percentage / 100;
                sy = sh - ih;
                sh = ih;
            } else if (direction == dir_up) {
                int ih = sh * percentage / 100;
                dy += sh - ih;
                sh = ih;
            }
            painter.drawImage (dx, dy, *img->image, sx, sy, sw, sh);
            painter.end ();
            imfl->invalidateCachedImage ();
            imfl->repaint ();
        }
    }
}
