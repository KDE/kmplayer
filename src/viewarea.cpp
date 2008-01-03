/**
  This file belong to the KMPlayer project, a movie player plugin for Konqueror
  Copyright (C) 2007  Koos Vriezen <koos.vriezen@gmail.com>

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

#include <stdlib.h>
#include <math.h>

#include <qapplication.h>
#include <qslider.h>
#include <qcursor.h>
#include <qimage.h>
#include <qmap.h>
#include <QPalette>
#include <QDesktopWidget>
#include <QX11Info>
#include <QPainter>
#include <QMainWindow>

#include <kactioncollection.h>
#include <kapplication.h>
#include <kstatusbar.h>
#include <kstdaction.h>
#include <kshortcut.h>
#include <klocale.h>
#include <kdebug.h>

#include "kmplayerview.h"
#include "kmplayercontrolpanel.h"
#include "playlistview.h"
#include "viewarea.h"
#ifdef KMPLAYER_WITH_CAIRO
# include <cairo-xlib.h>
# include <cairo-xlib-xrender.h>
#endif
#include "mediaobject.h"
#include "kmplayer_smil.h"
#include "kmplayer_rp.h"
#include "mediaobject.h"

#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
static const int XKeyPress = KeyPress;
#undef KeyPress
#undef Always
#undef Never
#undef Status
#undef Unsorted
#undef Bool

using namespace KMPlayer;

extern const char * normal_window_xpm[];
extern const char * playlist_xpm[];

//-------------------------------------------------------------------------

#ifdef KMPLAYER_WITH_CAIRO
static void copyImage (Surface *s, int w, int h, QImage *img, cairo_surface_t *similar) {
    int iw = img->width ();
    int ih = img->height ();

    if (img->depth () < 24) {
        QImage qi = img->convertDepth (32, 0);
        *img = qi;
    }
    cairo_surface_t *sf = cairo_image_surface_create_for_data (
            img->bits (),
            img->hasAlphaBuffer () ? CAIRO_FORMAT_ARGB32 : CAIRO_FORMAT_RGB24,
            iw, ih, img->bytesPerLine ());
    cairo_pattern_t *img_pat = cairo_pattern_create_for_surface (sf);
    cairo_pattern_set_extend (img_pat, CAIRO_EXTEND_NONE);
    if (w != iw && h != ih) {
        cairo_matrix_t mat;
        cairo_matrix_init_scale (&mat, 1.0 * iw/w, 1.0 * ih/h);
        cairo_pattern_set_matrix (img_pat, &mat);
    }
    if (!s->surface)
        s->surface = cairo_surface_create_similar (similar,
                img->hasAlphaBuffer () ?
                CAIRO_CONTENT_COLOR_ALPHA : CAIRO_CONTENT_COLOR, w, h);
    cairo_t *cr = cairo_create (s->surface);
    cairo_set_source (cr, img_pat);
    cairo_paint (cr);
    cairo_destroy (cr);

    cairo_pattern_destroy (img_pat);
    cairo_surface_destroy (sf);
}
#endif

//-------------------------------------------------------------------------

namespace KMPlayer {

class KMPLAYER_NO_EXPORT ViewSurface : public Surface {
public:
    ViewSurface (ViewArea * widget);
    ViewSurface (ViewArea * widget, NodePtr owner, const SRect & rect);
    ~ViewSurface ();

    void clear () { m_first_child = 0L; }

    SurfacePtr createSurface (NodePtr owner, const SRect & rect);
    IRect toScreen (Single x, Single y, Single w, Single h);
    void resize (const SRect & rect);
    void repaint ();
    void repaint (const SRect &rect);
    void video (Mrl *mt);

    NodePtrW current_video;
    ViewArea * view_widget;
};

} // namespace

KDE_NO_CDTOR_EXPORT ViewSurface::ViewSurface (ViewArea * widget)
  : Surface (NULL, SRect (0, 0, widget->width (), widget->height ())),
    view_widget (widget)
{}

KDE_NO_CDTOR_EXPORT
ViewSurface::ViewSurface (ViewArea * widget, NodePtr owner, const SRect & rect)
  : Surface (owner, rect), view_widget (widget) {}

KDE_NO_CDTOR_EXPORT ViewSurface::~ViewSurface() {
    //kDebug() << "~ViewSurface";
}

SurfacePtr ViewSurface::createSurface (NodePtr owner, const SRect & rect) {
    SurfacePtr surface = new ViewSurface (view_widget, owner, rect);
    appendChild (surface);
    return surface;
}

KDE_NO_EXPORT void ViewSurface::resize (const SRect &r) {
    bounds = r;
#ifdef KMPLAYER_WITH_CAIRO
    if (surface)
        cairo_xlib_surface_set_size (surface, (int)r.width(), (int)r.height ());
#endif
    /*if (rect == nrect)
        ;//return;
    SRect pr = rect.unite (nrect); // for repaint
    rect = nrect;*/
}

KDE_NO_EXPORT IRect ViewSurface::toScreen (Single x, Single y, Single w, Single h) {
    Matrix matrix (0, 0, xscale, yscale);
    matrix.translate (bounds.x (), bounds.y ());
    for (SurfacePtr s = parentNode(); s; s = s->parentNode()) {
        matrix.transform(Matrix (0, 0, s->xscale, s->yscale));
        matrix.translate (s->bounds.x (), s->bounds.y ());
    }
    matrix.getXYWH (x, y, w, h);
    return IRect (x, y, w, h);
}

KDE_NO_EXPORT
void ViewSurface::repaint (const SRect &r) {
    markDirty ();
    view_widget->scheduleRepaint (toScreen (r.x (), r.y (), r.width (), r.height ()));
    //kDebug() << "Surface::repaint x:" << (int)x << " y:" << (int)y << " w:" << (int)w << " h:" << (int)h;
}

KDE_NO_EXPORT
void ViewSurface::repaint () {
    markDirty ();
    view_widget->scheduleRepaint (toScreen (0, 0, bounds.width (), bounds.height ()));
}

KDE_NO_EXPORT void ViewSurface::video (Mrl *mt) {
    xscale = yscale = 1; // either scale width/heigt or use bounds
    if (mt->media_object &&
            MediaManager::AudioVideo == mt->media_object->type ()) {
        AudioVideoMedia *avm = static_cast <AudioVideoMedia*>(mt->media_object);
        if (avm->viewer &&
                avm->process &&
                avm->process->state () > IProcess::Ready &&
                strcmp (mt->nodeName (), "audio"))
            avm->viewer->setGeometry (toScreen (
                        0, 0, bounds.width(), bounds.height ()));
    }
}

//-------------------------------------------------------------------------

#ifdef KMPLAYER_WITH_CAIRO

static cairo_surface_t * cairoCreateSurface (Window id, int w, int h) {
    Display * display = QX11Info::display ();
    return cairo_xlib_surface_create (display, id,
            DefaultVisual (display, DefaultScreen (display)), w, h);
    /*return cairo_xlib_surface_create_with_xrender_format (
            QX11Info::display (),
            id,
            DefaultScreenOfDisplay (QX11Info::display ()),
            XRenderFindVisualFormat (QX11Info::display (),
                DefaultVisual (QX11Info::display (),
                    DefaultScreen (QX11Info::display ()))),
            w, h);*/
}

# define CAIRO_SET_SOURCE_RGB(cr,c)           \
    cairo_set_source_rgb ((cr),               \
            1.0 * (((c) >> 16) & 0xff) / 255, \
            1.0 * (((c) >> 8) & 0xff) / 255,  \
            1.0 * (((c)) & 0xff) / 255)

class KMPLAYER_NO_EXPORT CairoPaintVisitor : public Visitor {
    IRect clip;
    cairo_surface_t * cairo_surface;
    Matrix matrix;
    // stack vars need for transitions
    SMIL::MediaType *cur_media;
    cairo_pattern_t * cur_pat;
    cairo_matrix_t cur_mat;
    float opacity;
    bool toplevel;

    void traverseRegion (SMIL::RegionBase * reg);
    void updateExternal (SMIL::MediaType *av, SurfacePtr s);
    void paint(SMIL::MediaType *, Surface *, int x, int y, const IRect &);
public:
    cairo_t * cr;
    CairoPaintVisitor (cairo_surface_t * cs, Matrix m,
            const IRect & rect, QColor c=QColor(), bool toplevel=false);
    ~CairoPaintVisitor ();
    using Visitor::visit;
    void visit (Node * n);
    void visit (SMIL::Layout *);
    void visit (SMIL::Region *);
    void visit (SMIL::Transition *);
    void visit (SMIL::ImageMediaType *);
    void visit (SMIL::TextMediaType *);
    void visit (SMIL::Brush *);
    void visit (SMIL::RefMediaType *);
    void visit (SMIL::AVMediaType *);
    void visit (RP::Imfl *);
    void visit (RP::Fill *);
    void visit (RP::Fadein *);
    void visit (RP::Fadeout *);
    void visit (RP::Crossfade *);
    void visit (RP::Wipe *);
    void visit (RP::ViewChange *);
};

KDE_NO_CDTOR_EXPORT
CairoPaintVisitor::CairoPaintVisitor (cairo_surface_t * cs, Matrix m,
        const IRect & rect, QColor c, bool top)
 : clip (rect), cairo_surface (cs), matrix (m), toplevel (top) {
    cr = cairo_create (cs);
    if (toplevel) {
        cairo_rectangle (cr, rect.x, rect.y, rect.w, rect.h);
        cairo_clip (cr);
        //cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
        cairo_set_tolerance (cr, 0.5 );
        cairo_push_group (cr);
        cairo_set_source_rgb (cr,
           1.0 * c.red () / 255, 1.0 * c.green () / 255, 1.0 * c.blue () / 255);
        cairo_rectangle (cr, rect.x, rect.y, rect.w, rect.h);
        cairo_fill (cr);
    } else {
        cairo_save (cr);
        cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
        cairo_rectangle (cr, rect.x, rect.y, rect.w, rect.h);
        cairo_fill (cr);
        cairo_restore (cr);
    }
}

KDE_NO_CDTOR_EXPORT CairoPaintVisitor::~CairoPaintVisitor () {
    if (toplevel) {
        cairo_pattern_t * pat = cairo_pop_group (cr);
        //cairo_pattern_set_extend (pat, CAIRO_EXTEND_NONE);
        cairo_set_source (cr, pat);
        cairo_rectangle (cr, clip.x, clip.y, clip.w, clip.h);
        cairo_fill (cr);
        cairo_pattern_destroy (pat);
    }
    cairo_destroy (cr);
}

KDE_NO_EXPORT void CairoPaintVisitor::visit (Node * n) {
    kWarning() << "Paint called on " << n->nodeName();
}

KDE_NO_EXPORT void CairoPaintVisitor::traverseRegion (SMIL::RegionBase * reg) {
    // next visit listeners
    NodeRefListPtr nl = reg->listeners (mediatype_attached);
    if (nl) {
        for (NodeRefItemPtr c = nl->first(); c; c = c->nextSibling ())
            if (c->data)
                c->data->accept (this);
    }
    // finally visit children, accounting for z-order FIXME optimize
    NodeRefList sorted;
    for (NodePtr n = reg->firstChild (); n; n = n->nextSibling ()) {
        if (n->id != SMIL::id_node_region)
            continue;
        SMIL::Region * r = static_cast <SMIL::Region *> (n.ptr ());
        NodeRefItemPtr rn = sorted.first ();
        for (; rn; rn = rn->nextSibling ())
            if (r->z_order < convertNode <SMIL::Region> (rn->data)->z_order) {
                sorted.insertBefore (new NodeRefItem (n), rn);
                break;
            }
        if (!rn)
            sorted.append (new NodeRefItem (n));
    }
    for (NodeRefItemPtr r = sorted.first (); r; r = r->nextSibling ())
        r->data->accept (this);
}

KDE_NO_EXPORT void CairoPaintVisitor::visit (SMIL::Layout * reg) {
    //kDebug() << "Visit " << reg->nodeName();
    SMIL::RegionBase *rb = convertNode <SMIL::RegionBase> (reg->rootLayout);
    if (reg->surface () && rb) {
        //cairo_save (cr);
        Matrix m = matrix;

        SRect rect = reg->region_surface->bounds;
        Single x, y, w = rect.width(), h = rect.height();
        matrix.getXYWH (x, y, w, h);

        IRect clip_save = clip;
        clip = clip.intersect (IRect (x, y, w, h));

        rb->region_surface = reg->region_surface;
        rb->region_surface->background_color = rb->background_color;

        if (reg->region_surface->background_color & 0xff000000) {
            CAIRO_SET_SOURCE_RGB (cr, reg->region_surface->background_color);
            cairo_rectangle (cr, clip.x, clip.y, clip.w, clip.h);
            cairo_fill (cr);
        }
        //cairo_rectangle (cr, xoff, yoff, w, h);
        //cairo_clip (cr);

        matrix = Matrix (0, 0, reg->region_surface->xscale, reg->region_surface->yscale);
        matrix.transform (m);
        traverseRegion (reg);
        //cairo_restore (cr);
        matrix = m;
        clip = clip_save;

        rb->region_surface = 0L;
    }
}

KDE_NO_EXPORT void CairoPaintVisitor::visit (SMIL::Region * reg) {
    Surface *s = reg->surface ();
    if (s) {
        SRect rect = s->bounds;

        Matrix m = matrix;
        Single x = rect.x(), y = rect.y(), w = rect.width(), h = rect.height();
        matrix.getXYWH (x, y, w, h);
        if (clip.intersect (IRect (x, y, w, h)).isEmpty ())
            return;
        matrix = Matrix (rect.x(), rect.y(), 1.0, 1.0);
        matrix.transform (m);
        IRect clip_save = clip;
        clip = clip.intersect (IRect (x, y, w, h));
        cairo_save (cr);
        QImage *bg_img = reg->bg_image &&
            !reg->bg_image->isEmpty()
                ? reg->bg_image->cached_img->image
                : NULL;
        if ((SMIL::RegionBase::ShowAlways == reg->show_background ||
                    reg->m_AttachedMediaTypes->first ()) &&
                (s->background_color & 0xff000000 || bg_img)) {
            cairo_save (cr);
            if (s->background_color & 0xff000000) {
                CAIRO_SET_SOURCE_RGB (cr, s->background_color);
                cairo_rectangle (cr, clip.x, clip.y, clip.w, clip.h);
                cairo_fill (cr);
            }
            if (bg_img) {
                Single x1, y1;
                Single w = bg_img->width ();
                Single h = bg_img->height();
                matrix.getXYWH (x1, y1, w, h);
                if (!s->surface)
                    copyImage (s, w, h, bg_img, cairo_surface);
                cairo_pattern_t *pat = cairo_pattern_create_for_surface (s->surface);
                cairo_pattern_set_extend (pat, CAIRO_EXTEND_REPEAT);
                cairo_matrix_t mat;
                cairo_matrix_init_translate (&mat, (int) -x, (int) -y);
                cairo_pattern_set_matrix (pat, &mat);
                cairo_set_source (cr, pat);
                cairo_rectangle (cr, clip.x, clip.y, clip.w, clip.h);
                cairo_fill (cr);
                cairo_pattern_destroy (pat);
            }
            cairo_restore (cr);
        }
        traverseRegion (reg);
        cairo_restore (cr);
        matrix = m;
        clip = clip_save;
    }
}

#define CAIRO_SET_PATTERN_COND(cr,pat,mat)                      \
    if (pat) {                                                  \
        cairo_pattern_set_extend (cur_pat, CAIRO_EXTEND_NONE);  \
        cairo_pattern_set_matrix (pat, &mat);                   \
        cairo_pattern_set_filter (pat, CAIRO_FILTER_FAST);      \
        cairo_set_source (cr, pat);                             \
    }

KDE_NO_EXPORT void CairoPaintVisitor::visit (SMIL::Transition *trans) {
    float perc = trans->start_progress + (trans->end_progress - trans->start_progress)*cur_media->trans_step / cur_media->trans_steps;
    if (cur_media->trans_out_active)
        perc = 1.0 - perc;
    if (SMIL::Transition::Fade == trans->type) {
        CAIRO_SET_PATTERN_COND(cr, cur_pat, cur_mat)
        cairo_rectangle (cr, clip.x, clip.y, clip.w, clip.h);
        opacity = perc;
    } else if (SMIL::Transition::BarWipe == trans->type) {
        IRect rect;
        if (SMIL::Transition::SubTopToBottom == trans->sub_type) {
            if (SMIL::Transition::dir_reverse == trans->direction) {
                int dy = (int) ((1.0 - perc) * clip.h);
                rect = IRect (clip.x, clip.y + dy, clip.w, clip.h - dy);
            } else {
                rect = IRect (clip.x, clip.y, clip.w, (int) (perc * clip.h));
            }
        } else {
            if (SMIL::Transition::dir_reverse == trans->direction) {
                int dx = (int) ((1.0 - perc) * clip.w);
                rect = IRect (clip.x + dx, clip.y, clip.w - dx, clip.h);
            } else {
                rect = IRect (clip.x, clip.y, (int) (perc * clip.w), clip.h);
            }
        }
        cairo_rectangle (cr, rect.x, rect.y, rect.w, rect.h);
        CAIRO_SET_PATTERN_COND(cr, cur_pat, cur_mat)
    } else if (SMIL::Transition::PushWipe == trans->type) {
        int dx = 0, dy = 0;
        if (SMIL::Transition::SubFromTop == trans->sub_type)
            dy = -(int) ((1.0 - perc) * clip.h);
        else if (SMIL::Transition::SubFromRight == trans->sub_type)
            dx = (int) ((1.0 - perc) * clip.w);
        else if (SMIL::Transition::SubFromBottom == trans->sub_type)
            dy = (int) ((1.0 - perc) * clip.h);
        else //if (SMIL::Transition::SubFromLeft == trans->sub_type)
            dx = -(int) ((1.0 - perc) * clip.w);
        cairo_matrix_translate (&cur_mat, -dx, -dy);
        IRect rect = clip.intersect (IRect (clip.x + dx, clip.y + dy,
                    clip.w - dx, clip.h - dy));
        cairo_rectangle (cr, rect.x, rect.y, rect.w, rect.h);
        CAIRO_SET_PATTERN_COND(cr, cur_pat, cur_mat)
    } else if (SMIL::Transition::IrisWipe == trans->type) {
        CAIRO_SET_PATTERN_COND(cr, cur_pat, cur_mat)
        if (SMIL::Transition::SubDiamond == trans->sub_type) {
            cairo_rectangle (cr, clip.x, clip.y, clip.w, clip.h);
            cairo_clip (cr);
            int dx = (int) (perc * clip.w);
            int dy = (int) (perc * clip.h);
            int mx = clip.x + clip.w/2;
            int my = clip.y + clip.h/2;
            cairo_new_path (cr);
            cairo_move_to (cr, mx, my - dy);
            cairo_line_to (cr, mx + dx, my);
            cairo_line_to (cr, mx, my + dy);
            cairo_line_to (cr, mx - dx, my);
            cairo_close_path (cr);
        } else { // SubRectangle
            int dx = (int) (0.5 * (1 - perc) * clip.w);
            int dy = (int) (0.5 * (1 - perc) * clip.h);
            cairo_rectangle (cr, clip.x + dx, clip.y + dy,
                    clip.w - 2 * dx, clip.h -2 * dy);
        }
    } else if (SMIL::Transition::ClockWipe == trans->type) {
        cairo_rectangle (cr, clip.x, clip.y, clip.w, clip.h);
        cairo_clip (cr);
        int mx = clip.x + clip.w/2;
        int my = clip.y + clip.h/2;
        cairo_new_path (cr);
        cairo_move_to (cr, mx, my);
        float hw = 1.0 * clip.w/2;
        float hh = 1.0 * clip.h/2;
        float radius = sqrtf (hw * hw + hh * hh);
        float phi;
        switch (trans->sub_type) {
            case SMIL::Transition::SubClockwiseThree:
                phi = 0;
                break;
            case SMIL::Transition::SubClockwiseSix:
                phi = M_PI / 2;
                break;
            case SMIL::Transition::SubClockwiseNine:
                phi = M_PI;
                break;
            default: // Twelve
                phi = -M_PI / 2;
                break;
        }
        if (SMIL::Transition::dir_reverse == trans->direction)
            cairo_arc_negative (cr, mx, my, radius, phi, phi - 2 * M_PI * perc);
        else
            cairo_arc (cr, mx, my, radius, phi, phi + 2 * M_PI * perc);
        cairo_close_path (cr);
        CAIRO_SET_PATTERN_COND(cr, cur_pat, cur_mat)
    } else if (SMIL::Transition::BowTieWipe == trans->type) {
        cairo_rectangle (cr, clip.x, clip.y, clip.w, clip.h);
        cairo_clip (cr);
        int mx = clip.x + clip.w/2;
        int my = clip.y + clip.h/2;
        cairo_new_path (cr);
        cairo_move_to (cr, mx, my);
        float hw = 1.0 * clip.w/2;
        float hh = 1.0 * clip.h/2;
        float radius = sqrtf (hw * hw + hh * hh);
        float phi;
        switch (trans->sub_type) {
            case SMIL::Transition::SubHorizontal:
                phi = 0;
                break;
            default: // Vertical
                phi = -M_PI / 2;
                break;
        }
        float dphi = 0.5 * M_PI * perc;
        cairo_arc (cr, mx, my, radius, phi - dphi, phi + dphi);
        cairo_close_path (cr);
        cairo_new_sub_path (cr);
        cairo_move_to (cr, mx, my);
        if (SMIL::Transition::SubHorizontal == trans->sub_type)
            cairo_arc (cr, mx, my, radius, M_PI + phi - dphi, M_PI + phi +dphi);
        else
            cairo_arc (cr, mx, my, radius, -phi - dphi, -phi + dphi);
        cairo_close_path (cr);
        CAIRO_SET_PATTERN_COND(cr, cur_pat, cur_mat)
    } else if (SMIL::Transition::EllipseWipe == trans->type) {
        cairo_rectangle (cr, clip.x, clip.y, clip.w, clip.h);
        cairo_clip (cr);
        int mx = clip.x + clip.w/2;
        int my = clip.y + clip.h/2;
        float hw = (double) clip.w/2;
        float hh = (double) clip.h/2;
        float radius = sqrtf (hw * hw + hh * hh);
        cairo_save (cr);
        cairo_new_path (cr);
        cairo_translate (cr, (int) mx, (int) my);
        cairo_move_to (cr, - Single (radius), 0);
        if (SMIL::Transition::SubHorizontal == trans->sub_type)
            cairo_scale (cr, 1.0, 0.6);
        else if (SMIL::Transition::SubVertical == trans->sub_type)
            cairo_scale (cr, 0.6, 1.0);
        cairo_arc (cr, 0, 0, perc * radius, 0, 2 * M_PI);
        cairo_close_path (cr);
        cairo_restore (cr);
        CAIRO_SET_PATTERN_COND(cr, cur_pat, cur_mat)
    }
}

KDE_NO_EXPORT void CairoPaintVisitor::visit (SMIL::RefMediaType *ref) {
    Surface *s = ref->surface ();
    if (s) {
        if (ref->external_tree)
            updateExternal (ref, s);
        else
            s->video (ref);
    }
}

KDE_NO_EXPORT void CairoPaintVisitor::paint (SMIL::MediaType *mt, Surface *s,
        int x, int y, const IRect &rect) {
    cairo_save (cr);
    opacity = 1.0;
    cairo_matrix_init_translate (&cur_mat, -x, -y);
    cur_pat = cairo_pattern_create_for_surface (s->surface);
    if (mt->active_trans) {
        IRect clip_save = clip;
        clip = rect;
        cur_media = mt;
        mt->active_trans->accept (this);
        clip = clip_save;
    } else {
        cairo_pattern_set_extend (cur_pat, CAIRO_EXTEND_NONE);
        cairo_pattern_set_matrix (cur_pat, &cur_mat);
        cairo_pattern_set_filter (cur_pat, CAIRO_FILTER_FAST);
        cairo_set_source (cr, cur_pat);
        cairo_rectangle (cr, rect.x, rect.y, rect.w, rect.h);
    }
    opacity *= mt->opacity / 100.0;
    if (opacity < 0.99) {
        cairo_clip (cr);
        cairo_paint_with_alpha (cr, opacity);
    } else {
        cairo_fill (cr);
    }
    cairo_pattern_destroy (cur_pat);
    cairo_restore (cr);
}

KDE_NO_EXPORT
void CairoPaintVisitor::updateExternal (SMIL::MediaType *av, SurfacePtr s) {
    SRect rect = s->bounds;
    Single x = rect.x ();
    Single y = rect.y ();
    Single w = rect.width();
    Single h = rect.height();
    matrix.getXYWH (x, y, w, h);
    IRect clip_rect = clip.intersect (IRect (x, y, w, h));
    if (!clip_rect.isValid ())
        return;
    if (!s->surface || s->dirty) {
        Matrix m = matrix;
        m.translate (-x, -y);
        IRect r (clip_rect.x - (int) x - 1, clip_rect.y - (int) y - 1,
                clip_rect.w + 3, clip_rect.h + 3);
        if (!s->surface) {
            s->surface = cairo_surface_create_similar (cairo_surface,
                    CAIRO_CONTENT_COLOR_ALPHA, (int) w, (int) h);
            r = IRect (0, 0, w, h);
        }
        CairoPaintVisitor visitor (s->surface, m, r);
        av->external_tree->accept (&visitor);
        s->dirty = false;
    }
    paint (av, s.ptr (), x, y, clip_rect);
}

KDE_NO_EXPORT void CairoPaintVisitor::visit (SMIL::AVMediaType *av) {
    Surface *s = av->surface ();
    if (s) {
        if (av->external_tree)
            updateExternal (av, s);
        else
            s->video (av);
    }
}

KDE_NO_EXPORT void CairoPaintVisitor::visit (SMIL::ImageMediaType * img) {
    //kDebug() << "Visit " << img->nodeName() << " " << img->src;
    Surface *s = img->surface ();
    if (!s)
        return;
    if (img->external_tree) {
        updateExternal (img, s);
        return;
    }
    ImageMedia *im = static_cast <ImageMedia *> (img->media_object);
    ImageData *id = im ? im->cached_img.ptr () : NULL;
    if (!id || !id->image || img->width <= 0 || img->height <= 0) {
        s->remove();
        return;
    }
    SRect rect = s->bounds;
    Single x = rect.x ();
    Single y = rect.y ();
    Single w = rect.width();
    Single h = rect.height();
    matrix.getXYWH (x, y, w, h);
    IRect clip_rect = clip.intersect (IRect (x, y, w, h));
    if (clip_rect.isEmpty ())
        return;
    if (!s->surface || s->dirty)
        copyImage (s, w, h, id->image, cairo_surface);
    paint (img, s, x, y, clip_rect);
    s->dirty = false;
}

KDE_NO_EXPORT void CairoPaintVisitor::visit (SMIL::TextMediaType * txt) {
    TextMedia *tm = static_cast <TextMedia *> (txt->media_object);
    Surface *s = tm ? txt->surface () : NULL;
    //kDebug() << "Visit " << txt->nodeName() << " " << td->text << endl;
    if (!s)
        return;
    SRect rect = s->bounds;
    Single x = rect.x (), y = rect.y(), w = rect.width(), h = rect.height();
    matrix.getXYWH (x, y, w, h);
    if (!s->surface) {
        //kDebug() << "new txt surface " << td->text;
        /* QTextEdit * edit = new QTextEdit;
        edit->setReadOnly (true);
        edit->setHScrollBarMode (QScrollView::AlwaysOff);
        edit->setVScrollBarMode (QScrollView::AlwaysOff);
        edit->setFrameShape (QFrame::NoFrame);
        edit->setFrameShadow (QFrame::Plain);
        edit->setGeometry (0, 0, w, h);
        if (edit->length () == 0)
            edit->setText (text);
        if (w0 > 0)
            font.setPointSize (int (1.0 * w * font_size / w0));
        edit->setFont (font);
        QRect rect = p.clipRegion (QPainter::CoordPainter).boundingRect ();
        rect = rect.intersect (QRect (xoff, yoff, w, h));
        QPixmap pix = QPixmap::grabWidget (edit, rect.x () - (int) xoff,
                rect.y () - (int) yoff, rect.width (), rect.height ());*/

        float scale = 1.0 * w / rect.width (); // TODO: make an image
        cairo_set_font_size (cr, scale * txt->font_size);
        cairo_font_extents_t txt_fnt;
        cairo_font_extents (cr, &txt_fnt);
        QString str = tm->text;
        struct Line {
            Line (const QString & ln) : txt (ln), next(0) {}
            QString txt;
            cairo_text_extents_t txt_ext;
            Single xoff;
            Line * next;
        } *lines = 0, *last_line = 0;
        Single y1 = y;
        Single max_width;
        int line_count = 0;
        Single min_xoff = w;
        while (!str.isEmpty ()) {
            int len = str.find (QChar ('\n'));
            bool skip_cr = false;
            if (len > 1 && str[len-1] == QChar ('\r')) {
                --len;
                skip_cr = true;
            }
            QString para = len > -1 ? str.left (len) : str;
            Line * line = new Line (para);
            ++line_count;
            if (!lines)
                lines = line;
            else
                last_line->next = line;
            last_line = line;
            int ppos = 0;
            while (true) {
                cairo_text_extents (cr, line->txt.utf8 ().data (), &line->txt_ext);
                float frag = line->txt_ext.width > 0.1
                    ? w / line->txt_ext.width : 1.1;
                if (frag < 1.0) {
                    int br_pos = int (line->txt.length () * frag); //educated guess
                    while (br_pos > 0) {
                        line->txt.truncate (br_pos);
                        br_pos = line->txt.lastIndexOf (QChar (' '));
                        if (br_pos < 1)
                            break;
                        line->txt.truncate (br_pos);
                        cairo_text_extents (cr, line->txt.utf8 ().data (), &line->txt_ext);
                        if (line->txt_ext.width < (double)w)
                            break;
                    }
                }
                if (line->txt_ext.width > (double)max_width)
                    max_width = line->txt_ext.width;

                if (txt->halign == SMIL::TextMediaType::align_center)
                    line->xoff = (w - Single (line->txt_ext.width)) / 2;
                else if (txt->halign == SMIL::TextMediaType::align_right)
                    line->xoff = w - Single (line->txt_ext.width);
                if (line->xoff < min_xoff)
                    min_xoff = line->xoff;

                y1 += Single (txt_fnt.height);
                ppos += line->txt.length () + 1;
                if (ppos >= para.length ())
                    break;

                line->next = new Line (para.mid (ppos));
                ++line_count;
                line = line->next;
                last_line = line;
            }
            if (len < 0)
                break;
            str = str.mid (len + (skip_cr ? 2 : 1));
        }
        // new coord in screen space
        x += min_xoff;
        w = (double)max_width + txt_fnt.max_x_advance / 2;
        h = y1 - y /*txt_fnt.height + txt_fnt.descent*/;

        s->surface = cairo_surface_create_similar (cairo_surface,
                CAIRO_CONTENT_COLOR, (int) w, (int) h);
        cairo_t * cr_txt = cairo_create (s->surface);
        cairo_set_font_size (cr_txt, scale * txt->font_size);
        if (txt->bg_opacity) { // TODO real alpha
            CAIRO_SET_SOURCE_RGB (cr_txt, txt->background_color);
            cairo_paint (cr_txt);
        }
        CAIRO_SET_SOURCE_RGB (cr_txt, txt->font_color);
        y1 = 0;
        while (lines) {
            Line * line = lines;
            line->xoff += Single (txt_fnt.max_x_advance / 4);
            cairo_move_to (cr_txt, line->xoff - min_xoff, y1 + Single (txt_fnt.ascent));
            cairo_show_text (cr_txt, line->txt.utf8 ().data ());
            y1 += Single (txt_fnt.height);
            lines = lines->next;
            delete line;
        }
        //cairo_stroke (cr);
        cairo_destroy (cr_txt);

        // update bounds rect
        Single sx = x, sy = y, sw = w, sh = h;
        matrix.invXYWH (sx, sy, sw, sh);
        txt->width = sw;
        txt->height = sh;
        s->bounds = txt->calculateBounds ();

        // update coord. for painting below
        x = s->bounds.x ();
        y = s->bounds.y();
        w = s->bounds.width();
        h = s->bounds.height();
        matrix.getXYWH (x, y, w, h);
    }
    IRect clip_rect = clip.intersect (IRect (x, y, w, h));
    if (!clip_rect.isEmpty ())
        paint (txt, s, x, y, clip_rect);
    s->dirty = false;
}

KDE_NO_EXPORT void CairoPaintVisitor::visit (SMIL::Brush * brush) {
    //kDebug() << "Visit " << brush->nodeName();
    Surface *s = brush->surface ();
    if (s) {
        cairo_save (cr);
        opacity = 1.0;
        SRect rect = s->bounds;
        Single x, y, w = rect.width(), h = rect.height();
        matrix.getXYWH (x, y, w, h);
        unsigned int color = QColor (brush->param ("color")).rgb ();
        if (brush->active_trans) {
            cur_media = brush;
            cur_pat = NULL;
            brush->active_trans->accept (this);
        } else {
            cairo_rectangle (cr, (int) x, (int) y, (int) w, (int) h);
        }
        opacity *= brush->opacity / 100.0;
        if (opacity < 0.99)
            cairo_set_source_rgba (cr,
                    1.0 * ((color >> 16) & 0xff) / 255,
                    1.0 * ((color >> 8) & 0xff) / 255,
                    1.0 * (color & 0xff) / 255,
                    opacity);
        else
            CAIRO_SET_SOURCE_RGB (cr, color);
        cairo_fill (cr);
        s->dirty = false;
        cairo_restore (cr);
    }
}

KDE_NO_EXPORT void CairoPaintVisitor::visit (RP::Imfl * imfl) {
    if (imfl->surface ()) {
        cairo_save (cr);
        Matrix m = matrix;
        Single x, y;
        Single w = imfl->rp_surface->bounds.width();
        Single h = imfl->rp_surface->bounds.height();
        matrix.getXYWH (x, y, w, h);
        cairo_rectangle (cr, x, y, w, h);
        //cairo_clip (cr);
        cairo_translate (cr, x, y);
        cairo_scale (cr, w/imfl->width, h/imfl->height);
        if (imfl->needs_scene_img)
            cairo_push_group (cr);
        for (NodePtr n = imfl->firstChild (); n; n = n->nextSibling ())
            if (n->state >= Node::state_began &&
                    n->state < Node::state_deactivated) {
                RP::TimingsBase * tb = convertNode<RP::TimingsBase>(n);
                switch (n->id) {
                    case RP::id_node_viewchange:
                        if (!(int)tb->srcw)
                            tb->srcw = imfl->width;
                        if (!(int)tb->srch)
                            tb->srch = imfl->height;
                        // fall through
                    case RP::id_node_crossfade:
                    case RP::id_node_fadein:
                    case RP::id_node_fadeout:
                    case RP::id_node_fill:
                    case RP::id_node_wipe:
                        if (!(int)tb->w)
                            tb->w = imfl->width;
                        if (!(int)tb->h)
                            tb->h = imfl->height;
                        n->accept (this);
                        break;
                }
            }
        if (imfl->needs_scene_img) {
            cairo_pattern_t * pat = cairo_pop_group (cr);
            cairo_pattern_set_extend (pat, CAIRO_EXTEND_NONE);
            cairo_set_source (cr, pat);
            cairo_paint (cr);
            cairo_pattern_destroy (pat);
        }
        cairo_restore (cr);
        matrix = m;
    }
}

KDE_NO_EXPORT void CairoPaintVisitor::visit (RP::Fill * fi) {
    //kDebug() << "Visit " << fi->nodeName();
    CAIRO_SET_SOURCE_RGB (cr, fi->color);
    if ((int)fi->w && (int)fi->h) {
        cairo_rectangle (cr, fi->x, fi->y, fi->w, fi->h);
        cairo_fill (cr);
    }
}

KDE_NO_EXPORT void CairoPaintVisitor::visit (RP::Fadein * fi) {
    //kDebug() << "Visit " << fi->nodeName();
    if (fi->target && fi->target->id == RP::id_node_image) {
        RP::Image *img = convertNode <RP::Image> (fi->target);
        ImageMedia *im = img ? static_cast<ImageMedia*>(img->media_object):NULL;
        if (im && img->surface ()) {
            Single sx = fi->srcx, sy = fi->srcy, sw = fi->srcw, sh = fi->srch;
            if (!(int)sw)
                sw = img->width;
            if (!(int)sh)
                sh = img->height;
            if ((int)fi->w && (int)fi->h && (int)sw && (int)sh) {
                if (!img->img_surface->surface)
                    copyImage (img->img_surface, img->width, img->height,
                            im->cached_img->image, cairo_surface);
                cairo_matrix_t matrix;
                cairo_matrix_init_identity (&matrix);
                float scalex = 1.0 * sw / fi->w;
                float scaley = 1.0 * sh / fi->h;
                cairo_matrix_scale (&matrix, scalex, scaley);
                cairo_matrix_translate (&matrix,
                        1.0*sx/scalex - (double)fi->x,
                        1.0*sy/scaley - (double)fi->y);
                cairo_save (cr);
                cairo_rectangle (cr, fi->x, fi->y, fi->w, fi->h);
                cairo_pattern_t *pat = cairo_pattern_create_for_surface (img->img_surface->surface);
                cairo_pattern_set_extend (pat, CAIRO_EXTEND_NONE);
                cairo_pattern_set_matrix (pat, &matrix);
                cairo_set_source (cr, pat);
                cairo_clip (cr);
                cairo_paint_with_alpha (cr, 1.0 * fi->progress / 100);
                cairo_restore (cr);
                cairo_pattern_destroy (pat);
            }
        }
    }
}

KDE_NO_EXPORT void CairoPaintVisitor::visit (RP::Fadeout * fo) {
    //kDebug() << "Visit " << fo->nodeName();
    if (fo->progress > 0) {
        CAIRO_SET_SOURCE_RGB (cr, fo->to_color);
        if ((int)fo->w && (int)fo->h) {
            cairo_save (cr);
            cairo_rectangle (cr, fo->x, fo->y, fo->w, fo->h);
            cairo_clip (cr);
            cairo_paint_with_alpha (cr, 1.0 * fo->progress / 100);
            cairo_restore (cr);
        }
    }
}

KDE_NO_EXPORT void CairoPaintVisitor::visit (RP::Crossfade * cf) {
    //kDebug() << "Visit " << cf->nodeName();
    if (cf->target && cf->target->id == RP::id_node_image) {
        RP::Image *img = convertNode <RP::Image> (cf->target);
        ImageMedia *im = img ? static_cast<ImageMedia*>(img->media_object):NULL;
        if (im && img->surface ()) {
            Single sx = cf->srcx, sy = cf->srcy, sw = cf->srcw, sh = cf->srch;
            if (!(int)sw)
                sw = img->width;
            if (!(int)sh)
                sh = img->height;
            if ((int)cf->w && (int)cf->h && (int)sw && (int)sh) {
                if (!img->img_surface->surface)
                    copyImage (img->img_surface, img->width, img->height,
                            im->cached_img->image, cairo_surface);
                cairo_save (cr);
                cairo_matrix_t matrix;
                cairo_matrix_init_identity (&matrix);
                float scalex = 1.0 * sw / cf->w;
                float scaley = 1.0 * sh / cf->h;
                cairo_matrix_scale (&matrix, scalex, scaley);
                cairo_matrix_translate (&matrix,
                        1.0*sx/scalex - (double)cf->x,
                        1.0*sy/scaley - (double)cf->y);
                cairo_rectangle (cr, cf->x, cf->y, cf->w, cf->h);
                cairo_pattern_t *pat = cairo_pattern_create_for_surface (img->img_surface->surface);
                cairo_pattern_set_extend (pat, CAIRO_EXTEND_NONE);
                cairo_pattern_set_matrix (pat, &matrix);
                cairo_set_source (cr, pat);
                cairo_clip (cr);
                cairo_paint_with_alpha (cr, 1.0 * cf->progress / 100);
                cairo_restore (cr);
                cairo_pattern_destroy (pat);
            }
        }
    }
}

KDE_NO_EXPORT void CairoPaintVisitor::visit (RP::Wipe * wipe) {
    //kDebug() << "Visit " << wipe->nodeName();
    if (wipe->target && wipe->target->id == RP::id_node_image) {
        RP::Image *img = convertNode <RP::Image> (wipe->target);
        ImageMedia *im = img ? static_cast<ImageMedia*>(img->media_object):NULL;
        if (im && img->surface ()) {
            Single x = wipe->x, y = wipe->y;
            Single tx = x, ty = y;
            Single w = wipe->w, h = wipe->h;
            Single sx = wipe->srcx, sy = wipe->srcy, sw = wipe->srcw, sh = wipe->srch;
            if (!(int)sw)
                sw = img->width;
            if (!(int)sh)
                sh = img->height;
            if (wipe->direction == RP::Wipe::dir_right) {
                Single dx = w * 1.0 * wipe->progress / 100;
                tx = x -w + dx;
                w = dx;
            } else if (wipe->direction == RP::Wipe::dir_left) {
                Single dx = w * 1.0 * wipe->progress / 100;
                tx = x + w - dx;
                x = tx;
                w = dx;
            } else if (wipe->direction == RP::Wipe::dir_down) {
                Single dy = h * 1.0 * wipe->progress / 100;
                ty = y - h + dy;
                h = dy;
            } else if (wipe->direction == RP::Wipe::dir_up) {
                Single dy = h * 1.0 * wipe->progress / 100;
                ty = y + h - dy;
                y = ty;
                h = dy;
            }

            if ((int)w && (int)h) {
                if (!img->img_surface->surface)
                    copyImage (img->img_surface, img->width, img->height,
                            im->cached_img->image, cairo_surface);
                cairo_matrix_t matrix;
                cairo_matrix_init_identity (&matrix);
                float scalex = 1.0 * sw / wipe->w;
                float scaley = 1.0 * sh / wipe->h;
                cairo_matrix_scale (&matrix, scalex, scaley);
                cairo_matrix_translate (&matrix,
                        1.0*sx/scalex - (double)tx,
                        1.0*sy/scaley - (double)ty);
                cairo_pattern_t *pat = cairo_pattern_create_for_surface (img->img_surface->surface);
                cairo_pattern_set_extend (pat, CAIRO_EXTEND_NONE);
                cairo_pattern_set_matrix (pat, &matrix);
                cairo_set_source (cr, pat);
                cairo_rectangle (cr, x, y, w, h);
                cairo_fill (cr);
                cairo_pattern_destroy (pat);
            }
        }
    }
}

KDE_NO_EXPORT void CairoPaintVisitor::visit (RP::ViewChange * vc) {
    //kDebug() << "Visit " << vc->nodeName();
    if (vc->unfinished () || vc->progress < 100) {
        cairo_pattern_t * pat = cairo_pop_group (cr); // from imfl
        cairo_pattern_set_extend (pat, CAIRO_EXTEND_NONE);
        cairo_push_group (cr);
        cairo_save (cr);
        cairo_set_source (cr, pat);
        cairo_paint (cr);
        if ((int)vc->w && (int)vc->h && (int)vc->srcw && (int)vc->srch) {
            cairo_matrix_t matrix;
            cairo_matrix_init_identity (&matrix);
            float scalex = 1.0 * vc->srcw / vc->w;
            float scaley = 1.0 * vc->srch / vc->h;
            cairo_matrix_scale (&matrix, scalex, scaley);
            cairo_matrix_translate (&matrix,
                    1.0*vc->srcx/scalex - (double)vc->x,
                    1.0*vc->srcy/scaley - (double)vc->y);
            cairo_pattern_set_matrix (pat, &matrix);
            cairo_set_source (cr, pat);
            cairo_rectangle (cr, vc->x, vc->y, vc->w, vc->h);
            cairo_fill (cr);
        }
        cairo_pattern_destroy (pat);
        cairo_restore (cr);
    }
}

#endif

//-----------------------------------------------------------------------------

namespace KMPlayer {

class KMPLAYER_NO_EXPORT MouseVisitor : public Visitor {
    Matrix matrix;
    NodePtr node;
    unsigned int event;
    int x, y;
    bool handled;
    bool bubble_up;
public:
    MouseVisitor (unsigned int evt, int x, int y);
    KDE_NO_CDTOR_EXPORT ~MouseVisitor () {}
    using Visitor::visit;
    void visit (Node * n);
    void visit (SMIL::Layout *);
    void visit (SMIL::Region *);
    void visit (SMIL::TimedMrl * n);
    void visit (SMIL::MediaType * n);
    void visit (SMIL::Anchor *);
    void visit (SMIL::Area *);
    QCursor cursor;
};

} // namespace

KDE_NO_CDTOR_EXPORT
MouseVisitor::MouseVisitor (unsigned int evt, int a, int b)
    : event (evt), x (a), y (b), handled (false), bubble_up (false) {
}

KDE_NO_EXPORT void MouseVisitor::visit (Node * n) {
    kDebug () << "Mouse event ignored for " << n->nodeName ();
}

KDE_NO_EXPORT void MouseVisitor::visit (SMIL::Layout * layout) {
    if (layout->surface ()) {
        Matrix m = matrix;
        SRect rect = layout->region_surface->bounds;
        matrix = Matrix (rect.x(), rect.y(),
                layout->region_surface->xscale, layout->region_surface->yscale);
        matrix.transform (m);

        NodePtr node_save = node;
        node = layout;
        for (NodePtr r = layout->firstChild (); r; r = r->nextSibling ()) {
            if (r->id == SMIL::id_node_region)
                r->accept (this);
            if (!node->active ())
                break;
        }
        node = node_save;

        matrix = m;
    }
}

KDE_NO_EXPORT void MouseVisitor::visit (SMIL::Region * region) {
    if (region->surface ()) {
        SRect rect = region->region_surface->bounds;
        Single rx = rect.x(), ry = rect.y(), rw = rect.width(), rh = rect.height();
        matrix.getXYWH (rx, ry, rw, rh);
        handled = false;
        bool inside = x > rx && x < rx+rw && y > ry && y< ry+rh;
        if (!inside && (event == event_pointer_clicked || !region->has_mouse))
            return;
        Matrix m = matrix;
        matrix = Matrix (rect.x(), rect.y(), 1.0, 1.0);
        matrix.transform (m);
        bubble_up = false;

        bool child_handled = false;
        if (inside)
            for (NodePtr r = region->firstChild (); r; r = r->nextSibling ()) {
                r->accept (this);
                child_handled |= handled;
                if (!node->active ())
                    break;
            }
        child_handled &= !bubble_up;
        bubble_up = false;

        int saved_event = event;
        if (node->active ()) {
            bool propagate_listeners = !child_handled;
            if (event == event_pointer_moved) {
                propagate_listeners = true; // always pass move events
                if (region->has_mouse && (!inside || child_handled)) {
                    region->has_mouse = false;
                    event = event_outbounds;
                } else if (inside && !child_handled && !region->has_mouse) {
                    region->has_mouse = true;
                    event = event_inbounds;
                }
            }// else // event_pointer_clicked
            if (propagate_listeners) {
                NodeRefListPtr nl = region->listeners (
                        event == event_pointer_moved ? mediatype_attached : event);
                if (nl) {
                    for (NodeRefItemPtr c = nl->first(); c; c = c->nextSibling ()) {
                        if (c->data)
                            c->data->accept (this);
                        if (!node->active ())
                            break;
                    }
                }
            }
        }
        event = saved_event;
        handled = inside;
        matrix = m;
    }
}

static void followLink (SMIL::LinkingBase * link) {
    kDebug() << "link to " << link->href << " clicked";
    NodePtr n = link;
    if (link->href.startsWith ("#")) {
        SMIL::Smil * s = SMIL::Smil::findSmilNode (link);
        if (s)
            s->jump (link->href.mid (1));
        else
            kError() << "In document jumps smil not found" << endl;
    } else
        for (NodePtr p = link->parentNode (); p; p = p->parentNode ()) {
            if (n->mrl () && n->mrl ()->opener == p) {
                p->setState (Node::state_deferred);
                p->mrl ()->setParam (StringPool::attr_src, link->href, 0L);
                p->activate ();
                break;
            }
            n = p;
        }
}

KDE_NO_EXPORT void MouseVisitor::visit (SMIL::Anchor * anchor) {
    if (event == event_pointer_moved)
        cursor.setShape (Qt::PointingHandCursor);
    else if (event == event_pointer_clicked)
        followLink (anchor);
}

KDE_NO_EXPORT void MouseVisitor::visit (SMIL::Area * area) {
    NodePtr n = area->parentNode ();
    if (n->id >= SMIL::id_node_first_mediatype &&
            n->id < SMIL::id_node_last_mediatype) {
        SMIL::MediaType * mt = convertNode <SMIL::MediaType> (n);
        Surface *s = mt->surface ();
        if (s) {
            SRect rect = s->bounds;
            Single x1 = rect.x (), x2 = rect.y ();
            Single w = rect.width (), h = rect.height ();
            matrix.getXYWH (x1, x2, w, h);
            if (area->nr_coords > 1) {
                Single left = area->coords[0].size (rect.width ());
                Single top = area->coords[1].size (rect.height ());
                matrix.getXY (left, top);
                if (x < left || x > left + w || y < top || y > top + h)
                    return;
                if (area->nr_coords > 3) {
                    Single right = area->coords[2].size (rect.width ());
                    Single bottom = area->coords[3].size (rect.height ());
                    matrix.getXY (right, bottom);
                    if (x > right || y > bottom)
                        return;
                }
            }
            if (event == event_pointer_moved)
                cursor.setShape (Qt::PointingHandCursor);
            else {
                NodeRefListPtr nl = area->listeners (event);
                if (nl)
                    for (NodeRefItemPtr c = nl->first(); c; c = c->nextSibling ()) {
                        if (c->data)
                            c->data->accept (this);
                        if (!node->active ())
                            return;
                    }
                if (event == event_pointer_clicked && !area->href.isEmpty ())
                    followLink (area);
            }
        }
    }
}

KDE_NO_EXPORT void MouseVisitor::visit (SMIL::TimedMrl * timedmrl) {
    timedmrl->runtime ()->processEvent (event);
}

KDE_NO_EXPORT void MouseVisitor::visit (SMIL::MediaType * mediatype) {
    if (mediatype->sensitivity == SMIL::MediaType::sens_transparent) {
        bubble_up = true;
        return;
    }
    Surface *s = mediatype->surface ();
    if (!s)
        return;
    if (s->node && s->node.ptr () != mediatype) {
        s->node->accept (this);
        return;
    }
    SRect rect = s->bounds;
    Single rx = rect.x(), ry = rect.y(), rw = rect.width(), rh = rect.height();
    matrix.getXYWH (rx, ry, rw, rh);
    bool inside = x > rx && x < rx+rw && y > ry && y< ry+rh;
    if (!inside && event == event_pointer_clicked)
        return; // FIXME, also in/outbounds are bounds related

    NodeRefListPtr nl = mediatype->listeners (
            event == event_pointer_moved ? mediatype_attached : event);
    if (nl)
        for (NodeRefItemPtr c = nl->first(); c; c = c->nextSibling ()) {
            if (c->data)
                c->data->accept (this);
            if (!node->active ())
                return;
        }
    if (event != event_pointer_moved)
        visit (static_cast <SMIL::TimedMrl *> (mediatype));
    if (event != event_inbounds && event != event_outbounds) {
      SMIL::RegionBase *r=convertNode<SMIL::RegionBase>(mediatype->region_node);
      if (r && r->surface () &&
              r->id != SMIL::id_node_smil &&
              r->region_surface->node && r != r->region_surface->node.ptr ())
          return r->region_surface->node->accept (this);
    }
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT ViewArea::ViewArea (QWidget * parent, View * view)
// : QWidget (parent, "kde_kmplayer_viewarea", WResizeNoErase | WRepaintNoErase),
 : //QWidget (parent),
   m_view (view),
   m_collection (new KActionCollection (this)),
   surface (new ViewSurface (this)),
   m_mouse_invisible_timer (0),
   m_repaint_timer (0),
   m_fullscreen_scale (100),
   scale_lbl_id (-1),
   scale_slider_id (-1),
   m_fullscreen (false),
   m_minimal (false) {
    setAttribute (Qt::WA_OpaquePaintEvent, true);
    //setAttribute (Qt::WA_PaintOnScreen, true);
    QPalette palette;
    palette.setColor (backgroundRole(), QColor (0, 0, 0));
    setPalette (palette);
    setAcceptDrops (true);
    //new KAction (i18n ("Fullscreen"), KShortcut (Qt::Key_F), this, SLOT (accelActivated ()), m_collection, "view_fullscreen_toggle");
    setMouseTracking (true);
    kapp->installX11EventFilter (this);
}

KDE_NO_CDTOR_EXPORT ViewArea::~ViewArea () {
}

KDE_NO_EXPORT void ViewArea::stopTimers () {
    if (m_mouse_invisible_timer) {
        killTimer (m_mouse_invisible_timer);
        m_mouse_invisible_timer = 0;
    }
    if (m_repaint_timer) {
        killTimer (m_repaint_timer);
        m_repaint_timer = 0;
    }
}

KDE_NO_EXPORT void ViewArea::fullScreen () {
    stopTimers ();
    kDebug() << "had :" << m_fullscreen;
    if (m_fullscreen) {
        showNormal ();
        m_view->dockArea ()->setCentralWidget (this);
        m_view->dockArea ()->restoreState (m_dock_state);
        for (unsigned i = 0; i < m_collection->count (); ++i)
            m_collection->action (i)->setEnabled (false);
        /*if (scale_lbl_id != -1) {
            m_view->controlPanel ()->popupMenu->removeItem (scale_lbl_id);
            m_view->controlPanel ()->popupMenu->removeItem (scale_slider_id);
            scale_lbl_id = scale_slider_id = -1;
        }*/
        m_view->controlPanel ()->button (ControlPanel::button_playlist)->setIconSet (QIconSet (QPixmap (playlist_xpm)));
    } else {
        m_dock_state = m_view->dockArea ()->saveState ();
        m_topwindow_rect = topLevelWidget ()->geometry ();
        reparent (0L, 0, qApp->desktop()->screenGeometry(this).topLeft(), true);
        showFullScreen ();
        for (unsigned i = 0; i < m_collection->count (); ++i)
            m_collection->action (i)->setEnabled (true);
        /*QPopupMenu * menu = m_view->controlPanel ()->popupMenu;
        QLabel * lbl = new QLabel (i18n ("Scale:"), menu);
        scale_lbl_id = menu->insertItem (lbl, -1, 4);
        QSlider * slider = new QSlider (50, 150, 10, m_fullscreen_scale, Qt::Horizontal, menu);
        connect (slider, SIGNAL (valueChanged (int)), this, SLOT (scale (int)));
        scale_slider_id = menu->insertItem (slider, -1, 5);*/
        m_view->controlPanel ()->button (ControlPanel::button_playlist)->setIconSet (QIconSet (QPixmap (normal_window_xpm)));
    }
    m_fullscreen = !m_fullscreen;
    m_view->controlPanel()->fullscreenAction->setChecked (m_fullscreen);

#ifdef KMPLAYER_WITH_CAIRO
    if (surface->surface) {
        cairo_surface_destroy (surface->surface);
        surface->surface = 0L;
    }
#endif
    if (m_fullscreen) {
        m_mouse_invisible_timer = startTimer(MOUSE_INVISIBLE_DELAY);
    } else {
        if (m_mouse_invisible_timer) {
            killTimer (m_mouse_invisible_timer);
            m_mouse_invisible_timer = 0;
        }
        unsetCursor();
    }
}

void ViewArea::minimalMode () {
    m_minimal = !m_minimal;
    stopTimers ();
    m_mouse_invisible_timer = m_repaint_timer = 0;
    if (m_minimal) {
        m_view->setViewOnly ();
        m_view->setControlPanelMode (KMPlayer::View::CP_AutoHide);
        m_view->setNoInfoMessages (true);
        m_view->controlPanel ()->button (ControlPanel::button_playlist)->setIconSet (QIconSet (QPixmap (normal_window_xpm)));
    } else {
        m_view->setControlPanelMode (KMPlayer::View::CP_Show);
        m_view->setNoInfoMessages (false);
        m_view->controlPanel ()->button (ControlPanel::button_playlist)->setIconSet (QIconSet (QPixmap (playlist_xpm)));
    }
    m_topwindow_rect = topLevelWidget ()->geometry ();
}

KDE_NO_EXPORT void ViewArea::accelActivated () {
    m_view->controlPanel()->fullscreenAction->trigger ();
}

KDE_NO_EXPORT void ViewArea::mousePressEvent (QMouseEvent * e) {
    if (surface->node) {
        MouseVisitor visitor (event_pointer_clicked, e->x(), e->y());
        surface->node->accept (&visitor);
    }
    e->accept ();
}

KDE_NO_EXPORT void ViewArea::mouseDoubleClickEvent (QMouseEvent *) {
    m_view->fullScreen (); // screensaver stuff
}

KDE_NO_EXPORT void ViewArea::mouseMoveEvent (QMouseEvent * e) {
    if (e->state () == Qt::NoButton) {
        int vert_buttons_pos = height () - m_view->statusBarHeight ();
        int cp_height = m_view->controlPanel ()->maximumSize ().height ();
        m_view->delayedShowButtons (e->y() > vert_buttons_pos-cp_height &&
                                    e->y() < vert_buttons_pos);
    }
    if (surface->node) {
        MouseVisitor visitor (event_pointer_moved, e->x(), e->y());
        surface->node->accept (&visitor);
        setCursor (visitor.cursor);
    }
    e->accept ();
    mouseMoved (); // for m_mouse_invisible_timer
}

KDE_NO_EXPORT void ViewArea::syncVisual (const IRect & rect) {
#ifdef KMPLAYER_WITH_CAIRO
    int ex = rect.x;
    if (ex > 0)
        ex--;
    int ey = rect.y;
    if (ey > 0)
        ey--;
    int ew = rect.w + 2;
    int eh = rect.h + 2;
    if (!surface->surface)
        surface->surface = cairoCreateSurface (winId (), width (), height ());
    CairoPaintVisitor visitor (surface->surface,
            Matrix (surface->bounds.x(), surface->bounds.y(), 1.0, 1.0),
            IRect (ex, ey, ew, eh), palette ().color (backgroundRole ()), true);
    if (surface->node)
        surface->node->accept (&visitor);
#else
    repaint (QRect(rect.x, rect.y, rect.w, rect.h), false);
#endif
    if (m_repaint_timer) {
        killTimer (m_repaint_timer);
        m_repaint_timer = 0;
    }
    //XFlush (QX11Info::display ());
}

KDE_NO_EXPORT void ViewArea::paintEvent (QPaintEvent * pe) {
#ifdef KMPLAYER_WITH_CAIRO
    if (surface->node)
        scheduleRepaint (IRect (pe->rect ().x (), pe->rect ().y (), pe->rect ().width (), pe->rect ().height ()));
    else
#endif
    {
        QPainter p (this);
        p.fillRect (pe->rect (), QBrush (palette ().color (backgroundRole ())));
        p.end ();
    }
}

KDE_NO_EXPORT void ViewArea::scale (int val) {
    m_fullscreen_scale = val;
    resizeEvent (0L);
}

KDE_NO_EXPORT void ViewArea::updateSurfaceBounds () {
    Single x, y, w = width (), h = height ();
    h -= m_view->statusBarHeight ();
    h -= m_view->controlPanel ()->isVisible ()
        ? (m_view->controlPanelMode () == View::CP_Only
                ? h
                : (Single) m_view->controlPanel()->maximumSize ().height ())
        : Single (0);
    surface->resize (SRect (x, y, w, h));
    Mrl *mrl = surface->node ? surface->node->mrl () : NULL;
    if (m_view->keepSizeRatio () &&
            w > 0 && h > 0 &&
            mrl && mrl->width > 0 && mrl->height > 0) {
        double wasp = (double) w / h;
        double masp = (double) mrl->width / mrl->height;
        if (wasp > masp) {
            Single tmp = w;
            w = masp * h;
            x += (tmp - w) / 2;
        } else {
            Single tmp = h;
            h = Single (w / masp);
            y += (tmp - h) / 2;
        }
        surface->xscale = 1.0 * w / mrl->width;
        surface->yscale = 1.0 * h / mrl->height;
    } else {
        surface->xscale = 1.0;
        surface->yscale = 1.0;
    }
    surface->bounds = SRect (x, y, w, h);
    scheduleRepaint (IRect (0, 0, width (), height ()));
}

KDE_NO_EXPORT void ViewArea::resizeEvent (QResizeEvent *) {
    if (!m_view->controlPanel ()) return;
    Single x, y, w = width (), h = height ();
    Single hsb = m_view->statusBarHeight ();
    Single hcp = m_view->controlPanel ()->isVisible ()
        ? (m_view->controlPanelMode () == View::CP_Only
                ? h-hsb
                : (Single) m_view->controlPanel()->maximumSize ().height ())
        : Single (0);
    Single wws = w;
    // move controlpanel over video when autohiding and playing
    Single hws = h - (m_view->controlPanelMode () == View::CP_AutoHide
            ? Single (0)
            : hcp) - hsb;
    // now scale the regions and check if video region is already sized
    if (surface->node) {
        NodePtr n = surface->node;
        surface = new ViewSurface (this);
        surface->node = n;
    }
    updateSurfaceBounds ();

    // finally resize controlpanel and video widget
    if (m_view->controlPanel ()->isVisible ())
        m_view->controlPanel ()->setGeometry (0, h-hcp-hsb, w, hcp);
    if (m_view->statusBar ()->isVisible ())
        m_view->statusBar ()->setGeometry (0, h-hsb, w, hsb);
    if (m_fullscreen && wws == w && hws == h) {
        wws = wws * m_fullscreen_scale / 100;
        hws = hws * m_fullscreen_scale / 100;
        x += (w - wws) / 2;
        y += (h - hws) / 2;
    }
    m_view->console ()->setGeometry (0, 0, w, h - hsb - hcp);
    if (!surface->node && video_widgets.size () == 1)
        video_widgets.first ()->setGeometry (IRect (x, y, wws, hws));
}

KDE_NO_EXPORT SurfacePtr ViewArea::getSurface (NodePtr node) {
    static_cast <ViewSurface *> (surface.ptr ())->clear ();
    surface->node = node;
    //m_view->viewer()->resetBackgroundColor ();
    if (node) {
        updateSurfaceBounds ();
        return surface;
    }
    scheduleRepaint (IRect (0, 0, width (), height ()));
    return 0L;
}

KDE_NO_EXPORT void ViewArea::showEvent (QShowEvent *) {
    resizeEvent (0L);
}

KDE_NO_EXPORT void ViewArea::dropEvent (QDropEvent * de) {
    m_view->dropEvent (de);
}

KDE_NO_EXPORT void ViewArea::dragEnterEvent (QDragEnterEvent* dee) {
    m_view->dragEnterEvent (dee);
}

KDE_NO_EXPORT void ViewArea::contextMenuEvent (QContextMenuEvent * e) {
    m_view->controlPanel ()->popupMenu->exec (e->globalPos ());
}

KDE_NO_EXPORT void ViewArea::mouseMoved () {
    if (m_fullscreen) {
        if (m_mouse_invisible_timer)
            killTimer (m_mouse_invisible_timer);
        unsetCursor ();
        m_mouse_invisible_timer = startTimer (MOUSE_INVISIBLE_DELAY);
    }
}

KDE_NO_EXPORT void ViewArea::scheduleRepaint (const IRect &rect) {
    if (m_repaint_timer) {
        m_repaint_rect = m_repaint_rect.unite (rect);
    } else {
        m_repaint_rect = rect;
        m_repaint_timer = startTimer (10); // 100 per sec should do
    }
}

KDE_NO_EXPORT void ViewArea::timerEvent (QTimerEvent * e) {
    if (e->timerId () == m_mouse_invisible_timer) {
        killTimer (m_mouse_invisible_timer);
        m_mouse_invisible_timer = 0;
        if (m_fullscreen)
            setCursor (QCursor (Qt::BlankCursor));
    } else if (e->timerId () == m_repaint_timer) {
        killTimer (m_repaint_timer);
        m_repaint_timer = 0;
        //repaint (m_repaint_rect, false);
        syncVisual (m_repaint_rect.intersect (IRect (0, 0, width (), height ())));
    } else {
        kError () << "unknown timer " << e->timerId () << " " << m_repaint_timer << endl;
        killTimer (e->timerId ());
    }
}

KDE_NO_EXPORT void ViewArea::closeEvent (QCloseEvent * e) {
    //kDebug () << "closeEvent";
    if (m_fullscreen) {
        fullScreen ();
        if (!m_view->topLevelWidget ()->isVisible ())
            m_view->topLevelWidget ()->setVisible (true);
        e->ignore ();
    } else
        QWidget::closeEvent (e);
}

IViewer *ViewArea::createVideoWidget () {
    VideoOutput *viewer = new VideoOutput (this, m_view);
    video_widgets.push_back (viewer);
    viewer->setGeometry (IRect (-60, -60, 50, 50));
    viewer->setVisible (true);
    m_view->controlPanel ()->raise ();
    return viewer;
}

void ViewArea::destroyVideoWidget (IViewer *widget) {
    VideoWidgetList::iterator it = video_widgets.find (widget);
    if (it != video_widgets.end ()) {
        IViewer *viewer = *it;
        delete viewer;
        video_widgets.erase (it);
    } else {
        kWarning () << "destroyVideoWidget widget not found" << endl;
    }
}

void ViewArea::setVideoWidgetVisible (bool show) {
    const VideoWidgetList::iterator e = video_widgets.end ();
    for (VideoWidgetList::iterator it = video_widgets.begin (); it != e; ++it)
        static_cast <VideoOutput *> (*it)->setVisible (show);
}

static void setXSelectInput (WId wid) {
    WId r, p, *c;
    unsigned int nr;
    XSelectInput (QX11Info::display (), wid,
            //KeyPressMask | KeyReleaseMask |
            KeyPressMask |
            //EnterWindowMask | LeaveWindowMask |
            //FocusChangeMask |
            ExposureMask |
            StructureNotifyMask |
            SubstructureNotifyMask |
            PointerMotionMask);
    if (XQueryTree (QX11Info::display (), wid, &r, &p, &c, &nr)) {
        for (int i = 0; i < nr; ++i)
            setXSelectInput (c[i]);
        XFree (c);
    }
}

bool ViewArea::x11Event (XEvent *xe) {
    switch (xe->type) {
        case UnmapNotify: {
            const VideoWidgetList::iterator e = video_widgets.end ();
            for (VideoWidgetList::iterator it = video_widgets.begin(); it != e; ++it) {
                if ((*it)->clientHandle () == xe->xunmap.event) {
                    m_view->videoStart ();
                    break;
                }
            }
            break;
        }
        case XKeyPress: {
            const VideoWidgetList::iterator e = video_widgets.end ();
            for (VideoWidgetList::iterator i=video_widgets.begin(); i != e; ++i)
                if ((*i)->clientHandle () == xe->xkey.window) {
                    KeySym ksym;
                    char kbuf[16];
                    XLookupString (&xe->xkey, kbuf, sizeof(kbuf), &ksym, NULL);
                    switch (ksym) {
                        case XK_f:
                        case XK_F:
                            m_view->fullScreen ();
                            break;
                    }
                }
            break;
        }
        /*case ColormapNotify:
            fprintf (stderr, "colormap notify\n");
            return true;*/
        case MotionNotify:
            if (m_view->controlPanelMode () == View::CP_AutoHide) {
                const VideoWidgetList::iterator e = video_widgets.end ();
                for (VideoWidgetList::iterator i=video_widgets.begin(); i != e; ++i) {
                    QPoint p = mapToGlobal (QPoint (0, 0));
                    int x = xe->xmotion.x_root - p.x ();
                    int y = xe->xmotion.y_root - p.y ();
                    if (x > 0 && x < width ()) {
                        m_view->delayedShowButtons (y > height () -
                                m_view->statusBarHeight () -
                                m_view->controlPanel()->maximumSize().height());
                        mouseMoved ();
                    }
                }
            }
            break;
        case MapNotify:
            if (!xe->xmap.override_redirect) {
                const VideoWidgetList::iterator e = video_widgets.end ();
                for (VideoWidgetList::iterator i=video_widgets.begin(); i != e; ++i) {
                    WId p = xe->xmap.event;
                    WId w = xe->xmap.window;
                    WId v = (*i)->clientHandle ();
                    WId va = winId ();
                    WId root = 0;
                    WId *children;
                    unsigned int nr;
                    while (p != v &&
                            XQueryTree (QX11Info::display (), w, &root,
                                &p, &children, &nr)) {
                        if (nr)
                            XFree (children);
                        if (p == va || p == v || p == root)
                            break;
                        w = p;
                    }
                    if (p == v)
                        setXSelectInput (xe->xmap.window);
                }
                /*if (e->xmap.event == m_viewer->clientWinId ()) {
                  show ();
                  QTimer::singleShot (10, m_viewer, SLOT (sendConfigureEvent ()));
                  }*/
            }
            break;
        /*case ConfigureNotify:
            break;
            //return true;*/
        default:
            break;
    }
    return false;
}

//----------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT VideoOutput::VideoOutput (QWidget *parent, View * view)
  : QX11EmbedContainer (parent),
    m_plain_window (0), resized_timer (0),
    m_bgcolor (0), m_aspect (0.0),
    m_view (view) {
    /*XWindowAttributes xwa;
    XGetWindowAttributes (QX11Info::display(), winId (), &xwa);
    XSetWindowAttributes xswa;
    xswa.background_pixel = 0;
    xswa.border_pixel = 0;
    xswa.colormap = xwa.colormap;
    create (XCreateWindow (QX11Info::display()), parent->winId (), 0, 0, 10, 10, 0, 
                           x11Depth (), InputOutput, (Visual*)x11Visual (),
                           CWBackPixel | CWBorderPixel | CWColormap, &xswa));*/
    setAcceptDrops (true);
    connect (this, SIGNAL (clientIsEmbedded ()), this, SLOT (embedded ()));
    kDebug() << "VideoOutput::VideoOutput" << endl;
    //setProtocol (QXEmbed::XPLAIN);
}

KDE_NO_CDTOR_EXPORT VideoOutput::~VideoOutput () {
    kDebug() << "VideoOutput::~VideoOutput" << endl;
}

void VideoOutput::useIndirectWidget (bool inderect) {
    kDebug () << "setIntermediateWindow " << !!m_plain_window << "->" << inderect;
    if (!clientWinId () || !!m_plain_window != inderect) {
        if (inderect) {
            if (!m_plain_window) {
                int scr = DefaultScreen (QX11Info::display ());
                m_plain_window = XCreateSimpleWindow (
                        QX11Info::display(),
                        winId (),
                        0, 0, width(), height(),
                        1,
                        BlackPixel (QX11Info::display(), scr),
                        BlackPixel (QX11Info::display(), scr));
                XMapWindow (QX11Info::display(), m_plain_window);
                XSync (QX11Info::display (), false);
                //embedClient (m_plain_window);
            }
            XClearWindow (QX11Info::display(), m_plain_window);
        } else {
            if (m_plain_window) {
                XUnmapWindow (QX11Info::display(), m_plain_window);
                XFlush (QX11Info::display());
                discardClient ();
                XDestroyWindow (QX11Info::display(), m_plain_window);
                m_plain_window = 0;
                //XSync (QX11Info::display (), false);
            }
        }
    }
}

KDE_NO_EXPORT void VideoOutput::embedded () {
    kDebug () << "[01;35mwindowChanged[00m " << (int)clientWinId ();
    //QTimer::singleShot (10, this, SLOT (sendConfigureEvent ()));
    if (clientWinId () && !resized_timer)
         resized_timer = startTimer (50);
    if (clientWinId ())
        setXSelectInput (clientWinId ());
}

KDE_NO_EXPORT void VideoOutput::resizeEvent (QResizeEvent *) {
    if (clientWinId () && !resized_timer)
         resized_timer = startTimer (50);
}

KDE_NO_EXPORT void VideoOutput::timerEvent (QTimerEvent *e) {
    if (e->timerId () == resized_timer) {
        killTimer (resized_timer);
        resized_timer = 0;
        if (clientWinId ())
            XMoveResizeWindow (QX11Info::display(), clientWinId (),
                    0, 0, width (), height ());
    }
}

KDE_NO_EXPORT void VideoOutput::mouseMoveEvent (QMouseEvent * e) {
    if (e->buttons () == Qt::NoButton) {
        int cp_height = m_view->controlPanel ()->maximumSize ().height ();
        m_view->delayedShowButtons (e->y () > height () - cp_height);
    }
    m_view->viewArea ()->mouseMoved ();
}

WindowId VideoOutput::windowHandle () {
    //return m_plain_window ? clientWinId () : winId ();
    return m_plain_window ? m_plain_window : winId ();
}

WindowId VideoOutput::clientHandle () {
    return clientWinId ();
}

void VideoOutput::setGeometry (const IRect &rect) {
    int x = rect.x, y = rect.y, w = rect.w, h = rect.h;
    if (m_view->keepSizeRatio ()) {
        // scale video widget inside region
        int hfw = heightForWidth (w);
        if (hfw > 0)
            if (hfw > h) {
                int old_w = w;
                w = int ((1.0 * h * w)/(1.0 * hfw));
                x += (old_w - w) / 2;
            } else {
                y += (h - hfw) / 2;
                h = hfw;
            }
    }
    setGeometry (x, y, w, h);
    setVisible (true);
}

void VideoOutput::setAspect (float a) {
    m_aspect = a;
    QRect r = geometry ();
    m_view->viewArea ()->scheduleRepaint (
            IRect (r.x (), r.y (), r.width (), r.height ()));
}

KDE_NO_EXPORT void VideoOutput::map () {
    setVisible (true);
}

KDE_NO_EXPORT void VideoOutput::unmap () {
    setVisible (false);
}

KDE_NO_EXPORT int VideoOutput::heightForWidth (int w) const {
    if (m_aspect <= 0.01)
        return 0;
    return int (w/m_aspect);
}

KDE_NO_EXPORT void VideoOutput::dropEvent (QDropEvent * de) {
    m_view->dropEvent (de);
}

KDE_NO_EXPORT void VideoOutput::dragEnterEvent (QDragEnterEvent* dee) {
    m_view->dragEnterEvent (dee);
}
/*
*/
void VideoOutput::sendKeyEvent (int key) {
    WId w = clientWinId ();
    if (w) {
        char buf[2] = { char (key), '\0' };
        KeySym keysym = XStringToKeysym (buf);
        XKeyEvent event = {
            XKeyPress, 0, true,
            QX11Info::display (), w, QX11Info::appRootWindow(), w,
            /*time*/ 0, 0, 0, 0, 0,
            0, XKeysymToKeycode (QX11Info::display (), keysym), true
        };
        XSendEvent (QX11Info::display(), w, false, KeyPressMask, (XEvent *) &event);
        XFlush (QX11Info::display ());
    }
}

KDE_NO_EXPORT void VideoOutput::sendConfigureEvent () {
    WId w = clientWinId ();
    kDebug() << "[01;35msendConfigureEvent[00m " << width ();
    if (w) {
        XConfigureEvent c = {
            ConfigureNotify, 0UL, True,
            QX11Info::display (), w, winId (),
            x (), y (), width (), height (),
            0, None, False
        };
        XSendEvent(QX11Info::display(),c.event,true,StructureNotifyMask,(XEvent*)&c);
        XFlush (QX11Info::display ());
    }
}

KDE_NO_EXPORT void VideoOutput::contextMenuEvent (QContextMenuEvent * e) {
    m_view->controlPanel ()->popupMenu->exec (e->globalPos ());
}

KDE_NO_EXPORT void VideoOutput::setBackgroundColor (const QColor & c) {
    if (m_bgcolor != c.rgb ()) {
        m_bgcolor = c.rgb ();
        setCurrentBackgroundColor (c);
    }
}

KDE_NO_EXPORT void VideoOutput::resetBackgroundColor () {
    setCurrentBackgroundColor (m_bgcolor);
}

KDE_NO_EXPORT void VideoOutput::setCurrentBackgroundColor (const QColor & c) {
    QPalette palette;
    palette.setColor (backgroundRole(), c);
    setPalette (palette);
    WId w = clientWinId ();
    if (w) {
        XSetWindowBackground (QX11Info::display (), w, c.rgb ());
        XFlush (QX11Info::display ());
    }
}

#include "viewarea.moc"
