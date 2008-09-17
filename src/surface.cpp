/**
  This file belong to the KMPlayer project, a movie player plugin for Konqueror
  Copyright (C) 2008  Koos Vriezen <koos.vriezen@gmail.com>

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
**/

#include "config-kmplayer.h"

#ifdef KMPLAYER_WITH_CAIRO
# include <cairo.h>
#endif

#include "surface.h"
#include "viewarea.h"

using namespace KMPlayer;


KDE_NO_CDTOR_EXPORT Surface::Surface (ViewArea * widget)
  : bounds (SRect (0, 0, widget->width (), widget->height ())),
    xscale (1.0), yscale (1.0),
    background_color (0),
#ifdef KMPLAYER_WITH_CAIRO
    surface (0L),
#endif
    dirty (false),
    view_widget (widget)
{}

Surface::~Surface() {
#ifdef KMPLAYER_WITH_CAIRO
    if (surface)
        cairo_surface_destroy (surface);
#endif
}

void Surface::clear () {
    m_first_child = 0L;
}

void Surface::remove () {
    Surface *sp = parentNode ().ptr ();
    if (sp) {
        sp->markDirty ();
        sp->removeChild (this);
    }
}

void Surface::markDirty () {
    for (Surface *s = this; s; s = s->parentNode ().ptr ())
        s->dirty = true;
}

Surface *Surface::createSurface (NodePtr owner, const SRect & rect) {
    Surface *surface = new Surface (view_widget);
    surface->node = owner;
    surface->bounds = rect;
    appendChild (surface);
    return surface;
}

KDE_NO_EXPORT void Surface::resize (const SRect &r) {
    bounds = r;
    /*if (rect == nrect)
        ;//return;
    SRect pr = rect.unite (nrect); // for repaint
    rect = nrect;*/
}

KDE_NO_EXPORT IRect Surface::toScreen (Single x, Single y, Single w, Single h) {
    Matrix matrix (0, 0, xscale, yscale);
    matrix.translate (bounds.x (), bounds.y ());
    for (SurfacePtr s = parentNode(); s; s = s->parentNode()) {
        matrix.transform(Matrix (0, 0, s->xscale, s->yscale));
        matrix.translate (s->bounds.x (), s->bounds.y ());
    }
    matrix.getXYWH (x, y, w, h);
    return IRect (x, y, w, h);
}

KDE_NO_EXPORT IRect Surface::clipToScreen (Single x, Single y, Single w, Single h) {
    Matrix m (0, 0, xscale, yscale);
    m.translate (bounds.x (), bounds.y ());
    m.getXYWH (x, y, w, h);
    SRect r = bounds.intersect (SRect (x, y, w, h));
    x= r.x (), y = r.y (), w = r.width (), h = r.height ();
    for (SurfacePtr s = parentNode (); s; s = s->parentNode ()) {
        m = Matrix (0, 0, s->xscale, s->yscale);
        m.translate (s->bounds.x (), s->bounds.y ());
        m.getXYWH (x, y, w, h);
        r = SRect (s->bounds).intersect (SRect (x, y, w, h));
        x= r.x (); y = r.y (); w = r.width (); h = r.height ();
    }
    return IRect (x, y, w, h);
}

KDE_NO_EXPORT void Surface::repaint (const SRect &r) {
    markDirty ();
    view_widget->scheduleRepaint (clipToScreen (r.x (), r.y (), r.width (), r.height ()));
    //kDebug() << "Surface::repaint x:" << (int)x << " y:" << (int)y << " w:" << (int)w << " h:" << (int)h;
}

KDE_NO_EXPORT void Surface::repaint () {
    markDirty ();
    view_widget->scheduleRepaint (clipToScreen (0, 0, bounds.width (), bounds.height ()));
}

