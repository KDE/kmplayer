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
#include <QWidgetAction>

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
# include <pango/pangocairo.h>
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
static void clearSurface (cairo_t *cr, const IRect &rect) {
    cairo_save (cr);
    cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
    cairo_rectangle (cr, rect.x (), rect.y (), rect.width (), rect.height ());
    cairo_fill (cr);
    cairo_restore (cr);
}

void ImageData::copyImage (Surface *s, const SSize &sz, cairo_surface_t *similar, CalculatedSizer *zoom) {
    cairo_surface_t *src_sf;
    bool clear = false;
    int w = sz.width;
    int h = sz.height;

    if (surface) {
        src_sf = surface;
    } else {
        if (image->depth () < 24) {
            QImage qi = image->convertDepth (32, 0);
            *image = qi;
        }
        src_sf = cairo_image_surface_create_for_data (
                image->bits (),
                has_alpha ? CAIRO_FORMAT_ARGB32:CAIRO_FORMAT_RGB24,
                width, height, image->bytesPerLine ());
        if (flags & ImagePixmap && !(flags & ImageAnimated)) {
            surface = cairo_surface_create_similar (similar,
                    has_alpha ? CAIRO_CONTENT_COLOR_ALPHA : CAIRO_CONTENT_COLOR,
                    width, height);
            cairo_pattern_t *pat = cairo_pattern_create_for_surface (src_sf);
            cairo_pattern_set_extend (pat, CAIRO_EXTEND_NONE);
            cairo_t *cr = cairo_create (surface);
            cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
            cairo_set_source (cr, pat);
            cairo_paint (cr);
            cairo_destroy (cr);
            cairo_pattern_destroy (pat);
            cairo_surface_destroy (src_sf);
            src_sf = surface;
            delete image;
            image = NULL;
        }
    }

    cairo_pattern_t *img_pat = cairo_pattern_create_for_surface (src_sf);
    cairo_pattern_set_extend (img_pat, CAIRO_EXTEND_NONE);
    if (zoom) {
        cairo_matrix_t mat;
        Single zx, zy, zw, zh;
        zoom->calcSizes (NULL, width, height, zx, zy, zw, zh);
        cairo_matrix_init_translate (&mat, zx, zy);
        cairo_matrix_scale (&mat, 1.0 * zw/w, 1.0 * zh/h);
        cairo_pattern_set_matrix (img_pat, &mat);
    } else if (w != width && h != height) {
        cairo_matrix_t mat;
        cairo_matrix_init_scale (&mat, 1.0 * width/w, 1.0 * height/h);
        cairo_pattern_set_matrix (img_pat, &mat);
    }
    if (!s->surface)
        s->surface = cairo_surface_create_similar (similar,
                has_alpha ?
                CAIRO_CONTENT_COLOR_ALPHA : CAIRO_CONTENT_COLOR, w, h);
    else
        clear = true;
    cairo_t *cr = cairo_create (s->surface);
    if (clear)
        clearSurface (cr, IRect (0, 0, w, h));
    cairo_set_source (cr, img_pat);
    cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
    cairo_paint (cr);
    cairo_destroy (cr);

    cairo_pattern_destroy (img_pat);
    if (!surface)
        cairo_surface_destroy (src_sf);
}
#endif

//-------------------------------------------------------------------------

#define REGION_SCROLLBAR_WIDTH 20

#ifdef KMPLAYER_WITH_CAIRO

# define CAIRO_SET_SOURCE_RGB(cr,c)           \
    cairo_set_source_rgb ((cr),               \
            1.0 * (((c) >> 16) & 0xff) / 255, \
            1.0 * (((c) >> 8) & 0xff) / 255,  \
            1.0 * (((c)) & 0xff) / 255)

# define CAIRO_SET_SOURCE_ARGB(cr,c)          \
    cairo_set_source_rgba ((cr),              \
            1.0 * (((c) >> 16) & 0xff) / 255, \
            1.0 * (((c) >> 8) & 0xff) / 255,  \
            1.0 * (((c)) & 0xff) / 255,       \
            1.0 * (((c) >> 24) & 0xff) / 255)

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

    void traverseRegion (Node *reg, Surface *s);
    void updateExternal (SMIL::MediaType *av, SurfacePtr s);
    void paint(SMIL::MediaType *, Surface *, const IPoint &p, const IRect &);
    void video (Mrl *mt, Surface *s);
public:
    cairo_t * cr;
    CairoPaintVisitor (cairo_surface_t * cs, Matrix m,
            const IRect & rect, QColor c=QColor(), bool toplevel=false);
    ~CairoPaintVisitor ();
    using Visitor::visit;
    void visit (Node *);
    void visit (SMIL::Smil *);
    void visit (SMIL::Layout *);
    void visit (SMIL::RegionBase *);
    void visit (SMIL::Transition *);
    void visit (SMIL::ImageMediaType *);
    void visit (SMIL::TextMediaType *);
    void visit (SMIL::Brush *);
    void visit (SMIL::SmilText *);
    void visit (SMIL::RefMediaType *);
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
 : clip (rect), cairo_surface (cs),
   matrix (m), toplevel (top) {
    cr = cairo_create (cs);
    if (toplevel) {
        cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
        cairo_set_tolerance (cr, 0.5 );
        //cairo_push_group (cr);
        cairo_set_source_rgb (cr,
           1.0 * c.red () / 255, 1.0 * c.green () / 255, 1.0 * c.blue () / 255);
        cairo_rectangle (cr, rect.x(), rect.y(), rect.width(), rect.height());
        cairo_fill (cr);
    } else {
        clearSurface (cr, rect);
    }
}

KDE_NO_CDTOR_EXPORT CairoPaintVisitor::~CairoPaintVisitor () {
    /*if (toplevel) {
        cairo_pattern_t * pat = cairo_pop_group (cr);
        //cairo_pattern_set_extend (pat, CAIRO_EXTEND_NONE);
        cairo_set_source (cr, pat);
        cairo_rectangle (cr, clip.x, clip.y, clip.w, clip.h);
        cairo_fill (cr);
        cairo_pattern_destroy (pat);
    }*/
    cairo_destroy (cr);
}

KDE_NO_EXPORT void CairoPaintVisitor::visit (Node * n) {
    kWarning() << "Paint called on " << n->nodeName();
}

KDE_NO_EXPORT void CairoPaintVisitor::visit (SMIL::Smil *s) {
    if (s->active () && s->layout_node)
        s->layout_node->accept (this);
}

KDE_NO_EXPORT void CairoPaintVisitor::traverseRegion (Node *node, Surface *s) {
    ConnectionList *nl = nodeMessageReceivers (node, MsgSurfaceAttach);
    if (nl) {
        for (Connection *c = nl->first(); c; c = nl->next ())
            if (c->connecter)
                c->connecter->accept (this);
    }
    /*for (SurfacePtr c = s->lastChild (); c; c = c->previousSibling ()) {
        if (c->node && c->node->id != SMIL::id_node_region &&
        c->node && c->node->id != SMIL::id_node_root_layout)
            c->node->accept (this);
        else
            break;
    }*/
    // finally visit region children
    for (SurfacePtr c = s->firstChild (); c; c = c->nextSibling ()) {
        if (c->node && c->node->id == SMIL::id_node_region)
            c->node->accept (this);
        else
            break;
    }
    s->dirty = false;
}

KDE_NO_EXPORT void CairoPaintVisitor::visit (SMIL::Layout *layout) {
    //kDebug() << "Visit " << layout->nodeName();
    if (layout->root_layout) {
        Surface *s = (Surface *)layout->root_layout->role (RoleDisplay);
        if (s) {
            //cairo_save (cr);
            Matrix m = matrix;

            IRect scr = matrix.toScreen (SRect (0, 0, s->bounds.size));

            IRect clip_save = clip;
            clip = clip.intersect (scr);

            if (s->background_color & 0xff000000) {
                CAIRO_SET_SOURCE_RGB (cr, s->background_color);
                cairo_rectangle (cr,
                        clip.x (), clip.y (), clip.width (), clip.height ());
                cairo_fill (cr);
            }

            matrix = Matrix (0, 0, s->xscale, s->yscale);
            matrix.transform (m);
            traverseRegion (layout->root_layout, s);
            //cairo_restore (cr);
            matrix = m;
            clip = clip_save;
        }
    }
}


static void cairoDrawRect (cairo_t *cr, unsigned int color,
        int x, int y, int w, int h) {
    CAIRO_SET_SOURCE_ARGB (cr, color);
    cairo_rectangle (cr, x, y, w, h);
    cairo_fill (cr);
}

KDE_NO_EXPORT void CairoPaintVisitor::visit (SMIL::RegionBase *reg) {
    Surface *s = (Surface *) reg->role (RoleDisplay);
    if (s) {
        SRect rect = s->bounds;

        IRect scr = matrix.toScreen (rect);
        if (clip.intersect (scr).isEmpty ())
            return;
        Matrix m = matrix;
        matrix = Matrix (rect.x(), rect.y(), 1.0, 1.0);
        matrix.transform (m);
        IRect clip_save = clip;
        clip = clip.intersect (scr);
        cairo_save (cr);

        Surface *cs = s->firstChild ();
        if (!s->virtual_size.isEmpty ())
            matrix.translate (-s->x_scroll, -s->y_scroll);

        ImageMedia *im = reg->media_info
            ? (ImageMedia *) reg->media_info->media
            : NULL;
        ImageData *bg_img = im && !im->isEmpty() ? im->cached_img.ptr () : NULL;
        unsigned int bg_alpha = s->background_color & 0xff000000;
        if ((SMIL::RegionBase::ShowAlways == reg->show_background ||
                    reg->m_AttachedMediaTypes.first ()) &&
                (bg_alpha || bg_img)) {
            cairo_save (cr);
            if (bg_alpha) {
                cairo_rectangle (cr,
                        clip.x (), clip.y (), clip.width (), clip.height ());
                if (bg_alpha < 0xff000000) {
                    CAIRO_SET_SOURCE_ARGB (cr, s->background_color);
                    cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
                    cairo_fill (cr);
                    cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
                } else {
                    CAIRO_SET_SOURCE_RGB (cr, s->background_color);
                    cairo_fill (cr);
                }
            }
            if (bg_img) {
                Single w = bg_img->width;
                Single h = bg_img->height;
                matrix.getWH (w, h);
                if (!s->surface)
                    bg_img->copyImage (s, SSize (w, h), cairo_surface);
                if (bg_img->has_alpha)
                    cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
                cairo_pattern_t *pat = cairo_pattern_create_for_surface (s->surface);
                cairo_pattern_set_extend (pat, CAIRO_EXTEND_REPEAT);
                cairo_matrix_t mat;
                cairo_matrix_init_translate (&mat, -scr.x (), -scr.y ());
                cairo_pattern_set_matrix (pat, &mat);
                cairo_set_source (cr, pat);
                cairo_rectangle (cr,
                        clip.x (), clip.y (), clip.width (), clip.height ());
                cairo_fill (cr);
                cairo_pattern_destroy (pat);
                if (bg_img->has_alpha)
                    cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
            }
            cairo_restore (cr);
        }
        traverseRegion (reg, s);
        cs = s->firstChild ();
        if (cs && (s->scroll || cs->scroll) && cs == s->lastChild ()) {
            SRect r = cs->bounds;
            if (r.width () > rect.width () || r.height () > rect.height ()) {
                if (s->virtual_size.isEmpty ())
                    s->x_scroll = s->y_scroll = 0;
                s->virtual_size = r.size;
                matrix.getWH (s->virtual_size.width, s->virtual_size.height);
                s->virtual_size.width += REGION_SCROLLBAR_WIDTH;
                s->virtual_size.height += REGION_SCROLLBAR_WIDTH;
                const int vy = s->virtual_size.height;
                const int vw = s->virtual_size.width;
                int sbw = REGION_SCROLLBAR_WIDTH;
                int sbx = scr.x () + scr.width () - sbw;
                int sby = scr.y ();
                int sbh = scr.height () - REGION_SCROLLBAR_WIDTH;
                IRect sb_clip = clip.intersect (IRect (sbx, sby, sbw, sbh));
                if (!sb_clip.isEmpty ()) {
                    int knob_h = sbh * scr.height () / vy;
                    int knob_y = scr.y () + s->y_scroll * sbh / vy;
                    IRect knob (sbx, knob_y, sbw, knob_h);
                    cairo_save (cr);
                    cairo_rectangle (cr, sb_clip.x (), sb_clip.y (),
                            sb_clip.width (), sb_clip.height ());
                    cairo_clip (cr);
                    cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
                    cairo_set_line_width (cr, 2);
                    CAIRO_SET_SOURCE_ARGB (cr, 0x80A0A0A0);
                    cairo_rectangle (cr, sbx + 1, sby + 1, sbw - 2, sbh - 2);
                    cairo_stroke (cr);
                    if (s->y_scroll)
                        cairoDrawRect (cr, 0x80000000,
                                sbx + 2, sby + 2,
                                sbw - 4, knob.y() - sby - 2);
                    cairoDrawRect (cr, 0x80808080,
                            knob.x() + 2, knob.y(),
                            knob.width() - 4, knob.height());
                    if (sby + sbh - knob.y() - knob.height() - 2 > 0)
                        cairoDrawRect (cr, 0x80000000,
                                sbx + 2, knob.y() + knob.height(),
                                sbw - 4, sby + sbh - knob.y() -knob.height()-2);
                    cairo_restore (cr);
                }
                sbh = REGION_SCROLLBAR_WIDTH;
                sbx = scr.x ();
                sby = scr.y () + scr.height () - sbh;
                sbw = scr.width () - REGION_SCROLLBAR_WIDTH;
                sb_clip = clip.intersect (IRect (sbx, sby, sbw, sbh));
                if (!sb_clip.isEmpty ()) {
                    int knob_w = sbw * scr.width () / vw;
                    int knob_x = scr.x () + s->x_scroll * sbw / vw;
                    IRect knob (knob_x, sby, knob_w, sbh);
                    cairo_save (cr);
                    cairo_rectangle (cr, sb_clip.x (), sb_clip.y (),
                            sb_clip.width (), sb_clip.height ());
                    cairo_clip (cr);
                    cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
                    cairo_set_line_width (cr, 2);
                    CAIRO_SET_SOURCE_ARGB (cr, 0x80A0A0A0);
                    cairo_rectangle (cr, sbx + 1, sby + 1, sbw - 2, sbh - 2);
                    cairo_stroke (cr);
                    if (s->x_scroll)
                        cairoDrawRect (cr, 0x80000000,
                                sbx + 2, sby + 2,
                                knob.x() - sbx - 2, sbh - 4);
                    cairoDrawRect (cr, 0x80808080,
                            knob.x(), knob.y() + 2,
                            knob.width(), knob.height() - 4);
                    if (sbx + sbw - knob.x() - knob.width() - 2 > 0)
                        cairoDrawRect (cr, 0x80000000,
                                knob.x() + knob.width(), sby + 2,
                                sbx + sbw - knob.x() - knob.width()-2, sbh - 4);
                    cairo_restore (cr);
                }
            }
        }
        cairo_restore (cr);
        matrix = m;
        clip = clip_save;
        s->dirty = false;
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
    float perc = trans->start_progress + (trans->end_progress - trans->start_progress)*cur_media->trans_gain;
    if (cur_media->trans_out_active)
        perc = 1.0 - perc;
    if (SMIL::Transition::Fade == trans->type) {
        CAIRO_SET_PATTERN_COND(cr, cur_pat, cur_mat)
        cairo_rectangle (cr, clip.x(), clip.y(), clip.width(), clip.height());
        opacity = perc;
    } else if (SMIL::Transition::BarWipe == trans->type) {
        IRect rect;
        if (SMIL::Transition::SubTopToBottom == trans->sub_type) {
            if (SMIL::Transition::dir_reverse == trans->direction) {
                int dy = (int) ((1.0 - perc) * clip.height ());
                rect = IRect (clip.x (), clip.y () + dy,
                        clip.width (), clip.height () - dy);
            } else {
                rect = IRect (clip.x (), clip.y (),
                        clip.width (), (int) (perc * clip.height ()));
            }
        } else {
            if (SMIL::Transition::dir_reverse == trans->direction) {
                int dx = (int) ((1.0 - perc) * clip.width ());
                rect = IRect (clip.x () + dx, clip.y (),
                        clip.width () - dx, clip.height ());
            } else {
                rect = IRect (clip.x (), clip.y (),
                        (int) (perc * clip.width ()), clip.height ());
            }
        }
        cairo_rectangle (cr, rect.x(), rect.y(), rect.width(), rect.height());
        CAIRO_SET_PATTERN_COND(cr, cur_pat, cur_mat)
    } else if (SMIL::Transition::PushWipe == trans->type) {
        int dx = 0, dy = 0;
        if (SMIL::Transition::SubFromTop == trans->sub_type)
            dy = -(int) ((1.0 - perc) * clip.height ());
        else if (SMIL::Transition::SubFromRight == trans->sub_type)
            dx = (int) ((1.0 - perc) * clip.width ());
        else if (SMIL::Transition::SubFromBottom == trans->sub_type)
            dy = (int) ((1.0 - perc) * clip.height ());
        else //if (SMIL::Transition::SubFromLeft == trans->sub_type)
            dx = -(int) ((1.0 - perc) * clip.width ());
        cairo_matrix_translate (&cur_mat, -dx, -dy);
        IRect rect = clip.intersect (IRect (clip.x () + dx, clip.y () + dy,
                    clip.width (), clip.height ()));
        cairo_rectangle (cr, rect.x(), rect.y(), rect.width(), rect.height());
        CAIRO_SET_PATTERN_COND(cr, cur_pat, cur_mat)
    } else if (SMIL::Transition::IrisWipe == trans->type) {
        CAIRO_SET_PATTERN_COND(cr, cur_pat, cur_mat)
        if (SMIL::Transition::SubDiamond == trans->sub_type) {
            cairo_rectangle (cr, clip.x(), clip.y(),clip.width(),clip.height());
            cairo_clip (cr);
            int dx = (int) (perc * clip.width ());
            int dy = (int) (perc * clip.height ());
            int mx = clip.x () + clip.width ()/2;
            int my = clip.y () + clip.height ()/2;
            cairo_new_path (cr);
            cairo_move_to (cr, mx, my - dy);
            cairo_line_to (cr, mx + dx, my);
            cairo_line_to (cr, mx, my + dy);
            cairo_line_to (cr, mx - dx, my);
            cairo_close_path (cr);
        } else { // SubRectangle
            int dx = (int) (0.5 * (1 - perc) * clip.width ());
            int dy = (int) (0.5 * (1 - perc) * clip.height ());
            cairo_rectangle (cr, clip.x () + dx, clip.y () + dy,
                    clip.width () - 2 * dx, clip.height () -2 * dy);
        }
    } else if (SMIL::Transition::ClockWipe == trans->type) {
        cairo_rectangle (cr, clip.x(), clip.y(), clip.width(), clip.height());
        cairo_clip (cr);
        int mx = clip.x () + clip.width ()/2;
        int my = clip.y () + clip.height ()/2;
        cairo_new_path (cr);
        cairo_move_to (cr, mx, my);
        float hw = 1.0 * clip.width ()/2;
        float hh = 1.0 * clip.height ()/2;
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
        cairo_rectangle (cr, clip.x(), clip.y(), clip.width(), clip.height());
        cairo_clip (cr);
        int mx = clip.x () + clip.width ()/2;
        int my = clip.y () + clip.height ()/2;
        cairo_new_path (cr);
        cairo_move_to (cr, mx, my);
        float hw = 1.0 * clip.width ()/2;
        float hh = 1.0 * clip.height ()/2;
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
        cairo_rectangle (cr, clip.x(), clip.y(), clip.width(), clip.height());
        cairo_clip (cr);
        int mx = clip.x () + clip.width ()/2;
        int my = clip.y () + clip.height ()/2;
        float hw = (double) clip.width ()/2;
        float hh = (double) clip.height ()/2;
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

KDE_NO_EXPORT void CairoPaintVisitor::video (Mrl *m, Surface *s) {
    if (m->media_info &&
            m->media_info->media &&
            MediaManager::AudioVideo == m->media_info->type) {
        AudioVideoMedia *avm = static_cast<AudioVideoMedia *> (m->media_info->media);
        if (avm->viewer ()) {
            if (s &&
                    avm->process &&
                    avm->process->state () > IProcess::Ready &&
                    strcmp (m->nodeName (), "audio")) {
                s->xscale = s->yscale = 1; // either scale width/heigt or use bounds
                avm->viewer ()->setGeometry (s->toScreen (s->bounds.size));
            } else {
                avm->viewer ()->setGeometry (IRect (-60, -60, 50, 50));
            }
        }
    }
}

KDE_NO_EXPORT void CairoPaintVisitor::visit (SMIL::RefMediaType *ref) {
    Surface *s = ref->surface ();
    if (s && ref->external_tree)
        updateExternal (ref, s);
    else
        video (ref, s);
}

KDE_NO_EXPORT void CairoPaintVisitor::paint (SMIL::MediaType *mt, Surface *s,
        const IPoint &point, const IRect &rect) {
    cairo_save (cr);
    opacity = 1.0;
    cairo_matrix_init_translate (&cur_mat, -point.x, -point.y);
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
        cairo_rectangle (cr, rect.x(), rect.y(), rect.width(), rect.height());
    }
    opacity *= mt->opacity / 100.0;
    bool over = opacity < 0.99 ||
                CAIRO_CONTENT_COLOR != cairo_surface_get_content (s->surface);
    cairo_operator_t op;
    if (over) {
        op = cairo_get_operator (cr);
        cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
    }
    if (opacity < 0.99) {
        cairo_clip (cr);
        cairo_paint_with_alpha (cr, opacity);
    } else {
        cairo_fill (cr);
    }
    if (over)
        cairo_set_operator (cr, op);
    cairo_pattern_destroy (cur_pat);
    cairo_restore (cr);
}

static Mrl *findActiveMrl (Node *n, bool *rp_or_smil) {
    Mrl *mrl = n->mrl ();
    if (mrl) {
        *rp_or_smil = (mrl->id >= SMIL::id_node_first &&
                mrl->id < SMIL::id_node_last) ||
            (mrl->id >= RP::id_node_first &&
             mrl->id < RP::id_node_last);
        if (*rp_or_smil ||
                (mrl->media_info &&
                 MediaManager::AudioVideo == mrl->media_info->type))
            return mrl;
    }
    for (Node *c = n->firstChild (); c; c = c->nextSibling ())
        if (c->active ()) {
            Mrl *m = findActiveMrl (c, rp_or_smil);
            if (m)
                return m;
        }
    return NULL;
}

KDE_NO_EXPORT
void CairoPaintVisitor::updateExternal (SMIL::MediaType *av, SurfacePtr s) {
    bool rp_or_smil = false;
    Mrl *ext_mrl = findActiveMrl (av->external_tree.ptr (), &rp_or_smil);
    if (!ext_mrl)
        return;
    if (!rp_or_smil) {
        video (ext_mrl, s.ptr ());
        return;
    }
    IRect scr = matrix.toScreen (s->bounds);
    IRect clip_rect = clip.intersect (scr);
    if (clip_rect.isEmpty ())
        return;
    if (!s->surface || s->dirty) {
        Matrix m = matrix;
        m.translate (-scr.x (), -scr.y ());
        IRect r (clip_rect.x() - scr.x () - 1, clip_rect.y() - scr.y () - 1,
                clip_rect.width() + 3, clip_rect.height() + 3);
        if (!s->surface) {
            s->surface = cairo_surface_create_similar (cairo_surface,
                    CAIRO_CONTENT_COLOR_ALPHA, scr.width (), scr.height ());
            r = IRect (0, 0, scr.size);
        }
        CairoPaintVisitor visitor (s->surface, m, r);
        ext_mrl->accept (&visitor);
        s->dirty = false;
    }
    paint (av, s.ptr (), scr.point, clip_rect);
}

KDE_NO_EXPORT void CairoPaintVisitor::visit (SMIL::ImageMediaType * img) {
    //kDebug() << "Visit " << img->nodeName() << " " << img->src;
    if (!img->media_info)
        return;
    Surface *s = img->surface ();
    if (!s)
        return;
    if (img->external_tree) {
        updateExternal (img, s);
        return;
    }
    if (!img->media_info->media)
        return;

    IRect scr = matrix.toScreen (s->bounds);
    IRect clip_rect = clip.intersect (scr);
    if (clip_rect.isEmpty ())
        return;

    ImageMedia *im = static_cast <ImageMedia *> (img->media_info->media);
    ImageData *id = im ? im->cached_img.ptr () : NULL;
    if (id && id->flags == ImageData::ImageScalable)
        im->render (scr.size);
    if (!id || im->isEmpty () || img->size.isEmpty ()) {
        s->remove();
        return;
    }
    if (!s->surface || s->dirty)
        id->copyImage (s, SSize (scr.width (), scr.height ()), cairo_surface, img->pan_zoom);
    paint (img, s, scr.point, clip_rect);
    s->dirty = false;
}

static void calculateTextDimensions (PangoFontDescription *desc,
        const char *text, Single w, Single h,
        int *pxw, int *pxh, bool markup_text, int align = -1) {
    cairo_surface_t *img_surf = cairo_image_surface_create (
            CAIRO_FORMAT_RGB24, (int) w, h);
    cairo_t *cr_txt = cairo_create (img_surf);
    PangoLayout *layout = pango_cairo_create_layout (cr_txt);
    pango_layout_set_width (layout, 1.0 * w * PANGO_SCALE);
    pango_layout_set_wrap (layout, PANGO_WRAP_WORD_CHAR);
    if (markup_text)
        pango_layout_set_markup (layout, text, -1);
    else
        pango_layout_set_text (layout, text, -1);
    if (align > -1)
        pango_layout_set_alignment (layout, (PangoAlignment) align);
    pango_layout_set_font_description (layout, desc);

    pango_cairo_show_layout (cr_txt, layout);
    pango_layout_get_pixel_size (layout, pxw, pxh);

    g_object_unref (layout);
    cairo_destroy (cr_txt);
    cairo_surface_destroy (img_surf);
}

KDE_NO_EXPORT void CairoPaintVisitor::visit (SMIL::TextMediaType * txt) {
    if (!txt->media_info || !txt->media_info->media)
        return;
    TextMedia *tm = static_cast <TextMedia *> (txt->media_info->media);
    Surface *s = txt->surface ();
    if (!s)
        return;
    if (!s->surface) {
        txt->size = SSize ();
        s->bounds = txt->calculateBounds ();
    }
    IRect scr = matrix.toScreen (s->bounds);
    if (!s->surface || s->dirty) {

        int w = scr.width ();
        int pxw, pxh;
        Single ft_size = w * txt->font_size / (double)s->bounds.width ();
        const QByteArray text = tm->text.toUtf8 ();
        bool clear = s->surface;

        PangoFontDescription *desc = pango_font_description_new ();
        pango_font_description_set_family(desc, txt->font_name.toUtf8().data());
        pango_font_description_set_size (desc, PANGO_SCALE * ft_size);
        if (clear) {
            pxw = scr.width ();
            pxh = scr.height ();
        } else {
            calculateTextDimensions (desc, text.data (),
                    w, 2 * ft_size, &pxw, &pxh, false);
        }
        unsigned int bg_alpha = txt->background_color & 0xff000000;
        if (!s->surface)
            s->surface = cairo_surface_create_similar (cairo_surface,
                    bg_alpha < 0xff000000
                    ? CAIRO_CONTENT_COLOR_ALPHA
                    : CAIRO_CONTENT_COLOR,
                    pxw, pxh);
        cairo_t *cr_txt = cairo_create (s->surface);
        if (clear)
            clearSurface (cr_txt, IRect (0, 0, pxw, pxh));
        cairo_set_operator (cr_txt, CAIRO_OPERATOR_SOURCE);

        if (bg_alpha) {
            if (bg_alpha < 0xff000000)
                CAIRO_SET_SOURCE_ARGB (cr_txt, txt->background_color);
            else
                CAIRO_SET_SOURCE_RGB (cr_txt, txt->background_color);
            cairo_paint (cr_txt);
        }

        CAIRO_SET_SOURCE_RGB (cr_txt, txt->font_color);
        PangoLayout *layout = pango_cairo_create_layout (cr_txt);
        pango_layout_set_width (layout, 1.0 * w * PANGO_SCALE);
        pango_layout_set_wrap (layout, PANGO_WRAP_WORD_CHAR);
        pango_layout_set_text (layout, text.data (), -1);
        pango_layout_set_font_description (layout, desc);

        pango_cairo_show_layout (cr_txt, layout);

        pango_font_description_free (desc);
        g_object_unref (layout);
        cairo_destroy (cr_txt);

        // update bounds rect
        SRect rect = matrix.toUser (IRect (scr.point, ISize (pxw, pxh)));
        txt->size = rect.size;
        s->bounds = txt->calculateBounds ();

        // update coord. for painting below
        scr = matrix.toScreen (s->bounds);
    }
    IRect clip_rect = clip.intersect (scr);
    if (!clip_rect.isEmpty ())
        paint (txt, s, scr.point, clip_rect);
    s->dirty = false;
}

KDE_NO_EXPORT void CairoPaintVisitor::visit (SMIL::Brush * brush) {
    //kDebug() << "Visit " << brush->nodeName();
    Surface *s = brush->surface ();
    if (s) {
        opacity = 1.0;
        IRect clip_rect = clip.intersect (matrix.toScreen (s->bounds));
        if (clip_rect.isEmpty ())
            return;
        cairo_save (cr);
        if (brush->active_trans) {
            cur_media = brush;
            cur_pat = NULL;
            brush->active_trans->accept (this);
        } else {
            cairo_rectangle (cr, clip_rect.x (), clip_rect.y (),
                    clip_rect.width (), clip_rect.height ());
        }
        opacity *= brush->opacity / 100.0;
        if (opacity < 0.99) {
            cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
            cairo_set_source_rgba (cr,
                    1.0 * ((brush->color >> 16) & 0xff) / 255,
                    1.0 * ((brush->color >> 8) & 0xff) / 255,
                    1.0 * (brush->color & 0xff) / 255,
                    opacity);
        } else {
            CAIRO_SET_SOURCE_RGB (cr, brush->color);
        }
        cairo_fill (cr);
        if (opacity < 0.99)
            cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
        s->dirty = false;
        cairo_restore (cr);
    }
}

struct SmilTextBlock {
    SmilTextBlock (PangoFontDescription *d, const QString &t, IRect r, int a)
        : desc (d), rich_text (t), rect (r), align (a), next (NULL) {}

    ~SmilTextBlock () {
        pango_font_description_free (desc);
    }

    PangoFontDescription *desc;
    QString rich_text;
    IRect rect;
    int align;

    SmilTextBlock *next;
};

struct KMPLAYER_NO_EXPORT SmilTextInfo {
    SmilTextInfo (const SmilTextProperties &p) : props (p) {}

    void span (float scale);

    SmilTextProperties props;
    QString span_text;
};

class KMPLAYER_NO_EXPORT SmilTextVisitor : public Visitor {
public:
    SmilTextVisitor (int w, float s, const SmilTextProperties &p)
        : first (NULL), last (NULL), width (w), voffset (0),
          scale (s), max_font_size (0), info (p) {
         info.span (scale);
    }
    using Visitor::visit;
    void visit (TextNode *);
    void visit (SMIL::TextFlow *);

    void addRichText (const QString &txt);
    void push ();

    SmilTextBlock *first;
    SmilTextBlock *last;

    int width;
    int voffset;
    float scale;
    float max_font_size;
    SmilTextInfo info;
    QString rich_text;
};

void SmilTextInfo::span (float scale) {
    QString s = "<span";
    if (props.font_size > -1)
        s += " size='" + QString::number ((int)(1024 * scale * props.font_size)) + "'";
    s += " face='" + props.font_family + "'";
    if (props.font_color > -1)
        s += QString().sprintf (" foreground='#%06x'", props.font_color);
    if (props.background_color > -1)
        s += QString().sprintf (" background='#%06x'", props.background_color);
    if (SmilTextProperties::StyleInherit != props.font_style) {
        s += " style='";
        switch (props.font_style) {
            case SmilTextProperties::StyleOblique:
                s += "oblique'";
                break;
            case SmilTextProperties::StyleItalic:
                s += "italic'";
                break;
            default:
                s += "normal'";
                break;
        }
    }
    if (SmilTextProperties::WeightInherit != props.font_weight) {
        s += " weight='";
        switch (props.font_weight) {
            case SmilTextProperties::WeightBold:
                s += "bold'";
                break;
            default:
                s += "normal'";
                break;
        }
    }
    s += ">";
    span_text = s;
}

void SmilTextVisitor::addRichText (const QString &txt) {
    if (!info.span_text.isEmpty ())
        rich_text += info.span_text;
    rich_text += txt;
    if (!info.span_text.isEmpty ())
        rich_text += "</span>";
}

void SmilTextVisitor::push () {
    if (!rich_text.isEmpty ()) {
        int pxw, pxh;
        float fs = info.props.font_size;
        if (fs < 0)
            fs = TextMedia::defaultFontSize ();
        fs *= scale;
        float maxfs = max_font_size;
        if (maxfs < 1.0)
            maxfs = info.props.font_size;
        maxfs *= scale;

        int align = -1;
        if (SmilTextProperties::AlignLeft == info.props.text_align)
            align = (int) PANGO_ALIGN_LEFT;
        else if (SmilTextProperties::AlignCenter == info.props.text_align)
            align = (int) PANGO_ALIGN_CENTER;
        else if (SmilTextProperties::AlignRight == info.props.text_align)
            align = (int) PANGO_ALIGN_RIGHT;
        PangoFontDescription *desc = pango_font_description_new ();
        pango_font_description_set_family (desc, "Sans");
        pango_font_description_set_size (desc, PANGO_SCALE * fs);
        calculateTextDimensions (desc, rich_text.toUtf8 ().data (),
                width, 2 * maxfs, &pxw, &pxh, true, align);
        int x = 0;
        if (SmilTextProperties::AlignCenter == info.props.text_align)
            x = (width - pxw) / 2;
        else if (SmilTextProperties::AlignRight == info.props.text_align)
            x = width - pxw;
        SmilTextBlock *block = new SmilTextBlock (desc, rich_text,
                IRect (x, voffset, pxw, pxh), align);
        voffset += pxh;
        max_font_size = 0;
        rich_text.clear();
        if (!first) {
            first = last = block;
        } else {
            last->next = block;
            last = block;
        }
    }
}

void SmilTextVisitor::visit (TextNode *text) {
    QString buffer;
    QTextStream out (&buffer);
    out << XMLStringlet (text->nodeValue ());
    addRichText (buffer);
    if (text->nextSibling ())
        text->nextSibling ()->accept (this);
}

void SmilTextVisitor::visit (SMIL::TextFlow *flow) {
    if (flow->firstChild ()) {
        bool new_block = SMIL::id_node_p == flow->id ||
            SMIL::id_node_div == flow->id;
        float fs = info.props.font_size;
        if (fs < 0)
            fs = TextMedia::defaultFontSize ();
        int par_extra = SMIL::id_node_p == flow->id
            ? (int)(scale * fs) : 0;
        voffset += par_extra;

        SmilTextInfo saved_info = info;
        if (new_block)
            push ();

        info.props.mask (flow->props);
        if (info.props.font_size > max_font_size)
            max_font_size = info.props.font_size;
        info.span (scale);

        SmilTextBlock *block = last;

        flow->firstChild ()->accept (this);

        if (rich_text.isEmpty ())
            par_extra = 0;
        if (new_block)
            push ();
        voffset += par_extra;

        info = saved_info;
    }
    if (flow->nextSibling ())
        flow->nextSibling ()->accept (this);
}

KDE_NO_EXPORT void CairoPaintVisitor::visit (SMIL::SmilText *txt) {
    Surface *s = txt->surface ();
    if (!s)
        return;

    SRect rect = s->bounds;
    IRect scr = matrix.toScreen (rect);

    if (!s->surface) {

        int w = scr.width ();
        float scale = 1.0 * w / (double)s->bounds.width ();
        SmilTextVisitor info (w, scale, txt->props);

        if (txt->firstChild ())
            txt->firstChild ()->accept (&info);

        info.push ();
        if (info.first) {

            s->surface = cairo_surface_create_similar (cairo_surface,
                    CAIRO_CONTENT_COLOR_ALPHA, (int) w, info.voffset);
            cairo_t *cr_txt = cairo_create (s->surface);
            cairo_set_operator (cr_txt, CAIRO_OPERATOR_SOURCE);

            CAIRO_SET_SOURCE_RGB (cr_txt, 0);
            SmilTextBlock *b = info.first;
            int voff = 0;
            while (b) {
                const QByteArray text = b->rich_text.toUtf8 ();
                cairo_translate (cr_txt, 0, b->rect.y() - voff);
                voff = b->rect.y ();
                PangoLayout *layout = pango_cairo_create_layout (cr_txt);
                pango_layout_set_width (layout, 1.0 * w * PANGO_SCALE);
                pango_layout_set_wrap (layout, PANGO_WRAP_WORD_CHAR);
                pango_layout_set_markup (layout, text.data (), -1);
                pango_layout_set_font_description (layout, b->desc);
                if (b->align > -1)
                    pango_layout_set_alignment(layout,(PangoAlignment)b->align);

                pango_cairo_show_layout (cr_txt, layout);

                g_object_unref (layout);

                SmilTextBlock *tmp = b;
                b = b->next;
                delete tmp;
            }
            cairo_destroy (cr_txt);

            // update bounds rect
            s->bounds = matrix.toUser (IRect (scr.point, ISize (w, info.voffset)));

            // update coord. for painting below
            scr = matrix.toScreen (s->bounds);
        }
    }
    IRect clip_rect = clip.intersect (scr);
    if (!clip_rect.isEmpty ()) {
        cairo_save (cr);
        cairo_matrix_init_translate (&cur_mat, -scr.x (), -scr.y ());
        cairo_pattern_t *pat = cairo_pattern_create_for_surface (s->surface);
        cairo_pattern_set_extend (pat, CAIRO_EXTEND_NONE);
        cairo_pattern_set_matrix (pat, &cur_mat);
        cairo_pattern_set_filter (pat, CAIRO_FILTER_FAST);
        cairo_set_source (cr, pat);
        cairo_rectangle(cr, clip_rect.x (), clip_rect.y (),
                clip_rect.width (), clip_rect.height ());
        cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
        cairo_fill (cr);
        cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
        cairo_pattern_destroy (pat);
        cairo_restore (cr);
    }
    s->dirty = false;
}

KDE_NO_EXPORT void CairoPaintVisitor::visit (RP::Imfl * imfl) {
    if (imfl->surface ()) {
        cairo_save (cr);
        Matrix m = matrix;
        IRect scr = matrix.toScreen (SRect (0, 0, imfl->rp_surface->bounds.size));
        int w = scr.width ();
        int h = scr.height ();
        cairo_rectangle (cr, scr.x (), scr.y (), w, h);
        //cairo_clip (cr);
        cairo_translate (cr, scr.x (), scr.y ());
        cairo_scale (cr, 1.0*w/(double)imfl->size.width, 1.0*h/(double)imfl->size.height);
        if (imfl->needs_scene_img)
            cairo_push_group (cr);
        for (NodePtr n = imfl->firstChild (); n; n = n->nextSibling ())
            if (n->state >= Node::state_began &&
                    n->state < Node::state_deactivated) {
                RP::TimingsBase * tb = convertNode<RP::TimingsBase>(n);
                switch (n->id) {
                    case RP::id_node_viewchange:
                        if (!(int)tb->srcw)
                            tb->srcw = imfl->size.width;
                        if (!(int)tb->srch)
                            tb->srch = imfl->size.height;
                        // fall through
                    case RP::id_node_crossfade:
                    case RP::id_node_fadein:
                    case RP::id_node_fadeout:
                    case RP::id_node_fill:
                    case RP::id_node_wipe:
                        if (!(int)tb->w)
                            tb->w = imfl->size.width;
                        if (!(int)tb->h)
                            tb->h = imfl->size.height;
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
        ImageMedia *im = img && img->media_info
            ? static_cast <ImageMedia*> (img->media_info->media) : NULL;
        if (im && img->surface ()) {
            Single sx = fi->srcx, sy = fi->srcy, sw = fi->srcw, sh = fi->srch;
            if (!(int)sw)
                sw = img->size.width;
            if (!(int)sh)
                sh = img->size.height;
            if ((int)fi->w && (int)fi->h && (int)sw && (int)sh) {
                if (!img->img_surface->surface)
                    im->cached_img->copyImage (img->img_surface,
                            img->size, cairo_surface);
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
        ImageMedia *im = img && img->media_info
            ? static_cast <ImageMedia*> (img->media_info->media) : NULL;
        if (im && img->surface ()) {
            Single sx = cf->srcx, sy = cf->srcy, sw = cf->srcw, sh = cf->srch;
            if (!(int)sw)
                sw = img->size.width;
            if (!(int)sh)
                sh = img->size.height;
            if ((int)cf->w && (int)cf->h && (int)sw && (int)sh) {
                if (!img->img_surface->surface)
                    im->cached_img->copyImage (img->img_surface,
                            img->size, cairo_surface);
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
        ImageMedia *im = img && img->media_info
            ? static_cast <ImageMedia*> (img->media_info->media) : NULL;
        if (im && img->surface ()) {
            Single x = wipe->x, y = wipe->y;
            Single tx = x, ty = y;
            Single w = wipe->w, h = wipe->h;
            Single sx = wipe->srcx, sy = wipe->srcy, sw = wipe->srcw, sh = wipe->srch;
            if (!(int)sw)
                sw = img->size.width;
            if (!(int)sh)
                sh = img->size.height;
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
                    im->cached_img->copyImage (img->img_surface,
                            img->size, cairo_surface);
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
    ViewArea *view_area;
    Matrix matrix;
    NodePtrW source;
    const MessageType event;
    int x, y;
    bool handled;
    bool bubble_up;

    void dispatchSurfaceAttach (Node *n);
public:
    MouseVisitor (ViewArea *v, MessageType evt, int x, int y);
    KDE_NO_CDTOR_EXPORT ~MouseVisitor () {}
    using Visitor::visit;
    void visit (Node * n);
    void visit (Element *);
    void visit (SMIL::Smil *);
    void visit (SMIL::Layout *);
    void visit (SMIL::RegionBase *);
    void visit (SMIL::MediaType * n);
    void visit (SMIL::SmilText * n);
    void visit (SMIL::Anchor *);
    void visit (SMIL::Area *);
    QCursor cursor;
};

} // namespace

KDE_NO_CDTOR_EXPORT
MouseVisitor::MouseVisitor (ViewArea *v, MessageType evt, int a, int b)
  : view_area (v), event (evt), x (a), y (b),
    handled (false), bubble_up (false) {
}

void MouseVisitor::dispatchSurfaceAttach (Node *n) {
    ConnectionList *nl = nodeMessageReceivers (n, MsgSurfaceAttach);
    if (nl) {
        NodePtr node_save = source;
        source = n;

        for (Connection *c = nl->first(); c; c = nl->next ()) {
            if (c->connecter)
                c->connecter->accept (this);
            if (!source || !source->active ())
                break;
        }
        source = node_save;
    }
}

KDE_NO_EXPORT void MouseVisitor::visit (Node * n) {
    kDebug () << "Mouse event ignored for " << n->nodeName ();
}

KDE_NO_EXPORT void MouseVisitor::visit (SMIL::Smil *s) {
    if (s->active () && s->layout_node)
        s->layout_node->accept (this);
}

KDE_NO_EXPORT void MouseVisitor::visit (SMIL::Layout * layout) {
    Surface *s = (Surface *) layout->root_layout->role (RoleDisplay);
    if (s) {
        Matrix m = matrix;
        SRect rect = s->bounds;
        matrix = Matrix (rect.x(), rect.y(), s->xscale, s->yscale);
        matrix.transform (m);

        NodePtr node_save = source;
        source = layout;
        for (NodePtr r = layout->firstChild (); r; r = r->nextSibling ()) {
            if (r->id == SMIL::id_node_region)
                r->accept (this);
            if (!source || !source->active ())
                break;
        }
        source = node_save;

        matrix = m;
    }
}

KDE_NO_EXPORT void MouseVisitor::visit (SMIL::RegionBase *region) {
    Surface *s = (Surface *) region->role (RoleDisplay);
    if (s) {
        SRect rect = s->bounds;
        IRect scr = matrix.toScreen (rect);
        int rx = scr.x(), ry = scr.y(), rw = scr.width(), rh = scr.height();
        handled = false;
        bool inside = x > rx && x < rx+rw && y > ry && y< ry+rh;
        if (!inside && (event == MsgEventClicked || !region->has_mouse))
            return;

        if (event == MsgEventClicked && !s->virtual_size.isEmpty () &&
                x > rx + rw - REGION_SCROLLBAR_WIDTH) {
            const int sbh = rh - REGION_SCROLLBAR_WIDTH;
            const int vy = s->virtual_size.height;
            const int knob_h = sbh * rh / vy;
            int knob_y = y - ry - 0.5 * knob_h;
            if (knob_y < 0)
                knob_y = 0;
            else if (knob_y + knob_h > sbh)
                knob_y = sbh - knob_h;
            s->y_scroll = vy * knob_y / sbh;
            view_area->scheduleRepaint (scr);
            return;
        }
        if (event == MsgEventClicked && !s->virtual_size.isEmpty () &&
                y > ry + rh - REGION_SCROLLBAR_WIDTH) {
            const int sbw = rw - REGION_SCROLLBAR_WIDTH;
            const int vw = s->virtual_size.width;
            const int knob_w = sbw * rw / vw;
            int knob_x = x - rx - 0.5 * knob_w;
            if (knob_x < 0)
                knob_x = 0;
            else if (knob_x + knob_w > sbw)
                knob_x = sbw - knob_w;
            s->x_scroll = vw * knob_x / sbw;
            view_area->scheduleRepaint (scr);
            return;
        }

        Matrix m = matrix;
        matrix = Matrix (rect.x(), rect.y(), 1.0, 1.0);
        matrix.transform (m);
        if (!s->virtual_size.isEmpty ())
            matrix.translate (-s->x_scroll, -s->y_scroll);
        bubble_up = false;

        bool child_handled = false;
        if (inside || region->has_mouse)
            for (NodePtr r = region->firstChild (); r; r = r->nextSibling ()) {
                r->accept (this);
                child_handled |= handled;
                if (!source || !source->active ())
                    break;
            }
        child_handled &= !bubble_up;
        bubble_up = false;

        MessageType user_event = event;
        if (source && source->active ()) {
            bool notify_receivers = !child_handled;
            bool pass_event = !child_handled;
            if (event == MsgEventPointerMoved) {
                pass_event = true; // always pass move events
                if (region->has_mouse && !inside) {
                    notify_receivers = true;
                    region->has_mouse = false;
                    user_event = MsgEventPointerOutBounds;
                } else if (inside && !region->has_mouse) {
                    notify_receivers = true;
                    region->has_mouse = true;
                    user_event = MsgEventPointerInBounds;
                }
            }// else // MsgEventClicked
            if (notify_receivers) {
                Posting mouse_event (region, user_event);
                region->deliver (user_event, &mouse_event);
            }
            if (pass_event && source && source->active ())
                dispatchSurfaceAttach (region);
        }
        handled = inside;
        matrix = m;
    }
}

static void followLink (SMIL::LinkingBase * link) {
    kDebug() << "link to " << link->href << " clicked";
    if (link->href.startsWith ("#")) {
        SMIL::Smil * s = SMIL::Smil::findSmilNode (link);
        if (s)
            s->jump (link->href.mid (1));
        else
            kError() << "In document jumps smil not found" << endl;
    } else {
        PlayListNotify *notify = link->document ()->notify_listener;
        if (notify && !link->target.isEmpty ()) {
            notify->openUrl (link->href, link->target, QString ());
        } else {
            NodePtr n = link;
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
    }
}

KDE_NO_EXPORT void MouseVisitor::visit (SMIL::Anchor * anchor) {
    if (event == MsgEventPointerMoved)
        cursor.setShape (Qt::PointingHandCursor);
    else if (event == MsgEventClicked)
        followLink (anchor);
}

KDE_NO_EXPORT void MouseVisitor::visit (SMIL::Area * area) {
    NodePtr n = area->parentNode ();
    Surface *s = (Surface *) n->role (RoleDisplay);
    if (s) {
        SRect rect = s->bounds;
        IRect scr = matrix.toScreen (rect);
        int w = scr.width (), h = scr.height ();
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
        if (event == MsgEventPointerMoved)
            cursor.setShape (Qt::PointingHandCursor);
        else {
            ConnectionList *nl = nodeMessageReceivers (area, event);
            if (nl)
                for (Connection *c = nl->first(); c; c = nl->next ()) {
                    if (c->connecter)
                        c->connecter->accept (this);
                    if (!source || !source->active ())
                        return;
                }
            if (event == MsgEventClicked && !area->href.isEmpty ())
                followLink (area);
        }
    }
}

KDE_NO_EXPORT void MouseVisitor::visit (Element *elm) {
    Runtime *rt = (Runtime *) elm->role (RoleTiming);
    if (rt) {
        Posting mouse_event (source, event);
        rt->message (event, &mouse_event);
    }
}

KDE_NO_EXPORT void MouseVisitor::visit (SMIL::MediaType *mt) {
    if (mt->sensitivity == SMIL::MediaType::sens_transparent) {
        bubble_up = true;
        return;
    }
    Surface *s = mt->surface ();
    if (!s)
        return;
    if (s->node && s->node.ptr () != mt) {
        s->node->accept (this);
        return;
    }
    SRect rect = s->bounds;
    IRect scr = matrix.toScreen (rect);
    int rx = scr.x(), ry = scr.y(), rw = scr.width(), rh = scr.height();
    const bool inside = x > rx && x < rx+rw && y > ry && y< ry+rh;
    MessageType user_event = event;
    if (event == MsgEventPointerMoved) {
        if (inside && !mt->has_mouse)
            user_event = MsgEventPointerInBounds;
        else if (!inside && mt->has_mouse)
            user_event = MsgEventPointerOutBounds;
        else if (!inside)
            return;
    }
    mt->has_mouse = inside;

    NodePtrW node_save = mt;

    if (inside || event == MsgEventPointerMoved)
        dispatchSurfaceAttach (mt);
    if (!node_save || !mt->active ())
        return;
    if (MsgEventPointerMoved != user_event) {
        Posting mouse_event (mt, user_event);
        mt->deliver (user_event, &mouse_event);
    }
    if (!node_save || !mt->active ())
        return;

    SMIL::RegionBase *r=convertNode<SMIL::RegionBase>(mt->region_node);
    if (r && r->role (RoleDisplay) &&
            r->id != SMIL::id_node_smil &&
            r->region_surface->node && r != r->region_surface->node.ptr ())
        return r->region_surface->node->accept (this);
}

KDE_NO_EXPORT void MouseVisitor::visit (SMIL::SmilText *st) {
    if (MsgEventClicked == event) {
        ConnectionList *nl = nodeMessageReceivers (st, event);
        if (nl)
            for (Connection *c = nl->first(); c; c = nl->next ()) {
                if (c->connecter && c->connecter.ptr () != st)
                    c->connecter->accept (this);
                if (!source || !source->active ())
                    return;
            }
    }
}
//-----------------------------------------------------------------------------

namespace KMPlayer {
class KMPLAYER_NO_EXPORT ViewerAreaPrivate {
public:
    ViewerAreaPrivate (ViewArea *v)
        : m_view_area (v), backing_store (0), width (0), height (0) {
    }
    ~ViewerAreaPrivate() {
        destroyBackingStore ();
    }
    void resizeSurface (Surface *s) {
#ifdef KMPLAYER_WITH_CAIRO
        int w = m_view_area->width ();
        int h = m_view_area->height ();
        if ((w != width || h != height) && s->surface) {
            Display *d = QX11Info::display();
            //cairo_xlib_surface_set_size (s->surface, w, h);
            destroyBackingStore ();
            backing_store = XCreatePixmap (d, m_view_area->winId (),
                    w, h, QX11Info::appDepth ());
            cairo_xlib_surface_set_drawable(s->surface, backing_store, w,h);
            width = w;
            height = h;
        }
#endif
    }
#ifdef KMPLAYER_WITH_CAIRO
    cairo_surface_t *createSurface (int w, int h) {
        Display * display = QX11Info::display ();
        destroyBackingStore ();
        backing_store = XCreatePixmap (display,
                m_view_area->winId (), w, h, QX11Info::appDepth ());
        width = w;
        height = h;
        return cairo_xlib_surface_create (display, backing_store,
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
    void swapBuffer (const IRect &sr, int dx, int dy) {
        GC gc = XCreateGC (QX11Info::display(), backing_store, 0, NULL);
        XCopyArea (QX11Info::display(), backing_store, m_view_area->winId(),
                gc, sr.x(), sr.y(), sr.width (), sr.height (), dx, dy);
        XFreeGC (QX11Info::display(), gc);
        XFlush (QX11Info::display());
    }
#endif
    void destroyBackingStore () {
#ifdef KMPLAYER_WITH_CAIRO
        if (backing_store)
            XFreePixmap (QX11Info::display(), backing_store);
#endif
        backing_store = 0;
    }
    ViewArea *m_view_area;
    Drawable backing_store;
    int width;
    int height;
};

class KMPLAYER_NO_EXPORT RepaintUpdater {
public:
    RepaintUpdater (Node *n, RepaintUpdater *nx) : node (n), next (nx) {}

    NodePtrW node;
    RepaintUpdater *next;
};

}

KDE_NO_CDTOR_EXPORT ViewArea::ViewArea (QWidget *, View * view)
// : QWidget (parent, "kde_kmplayer_viewarea", WResizeNoErase | WRepaintNoErase),
 : //QWidget (parent),
   d (new ViewerAreaPrivate (this)),
   m_updaters (NULL),
   m_view (view),
   m_collection (new KActionCollection (this)),
   surface (new Surface (this)),
   m_mouse_invisible_timer (0),
   m_repaint_timer (0),
   m_fullscreen (false),
   m_minimal (false),
   m_updaters_enabled (true) {
#ifdef KMPLAYER_WITH_CAIRO
    setAttribute (Qt::WA_OpaquePaintEvent, true);
    setAttribute (Qt::WA_PaintOnScreen, true);
#endif
    QPalette palette;
    palette.setColor (backgroundRole(), QColor (0, 0, 0));
    setPalette (palette);
    setAcceptDrops (true);
    //new KAction (i18n ("Fullscreen"), KShortcut (Qt::Key_F), this, SLOT (accelActivated ()), m_collection, "view_fullscreen_toggle");
    setMouseTracking (true);
    kapp->installX11EventFilter (this);
}

KDE_NO_CDTOR_EXPORT ViewArea::~ViewArea () {
    while (m_updaters) {
        RepaintUpdater *tmp = m_updaters;
        m_updaters = m_updaters->next;
        delete tmp;
    }
    delete d;
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
    if (m_fullscreen) {
        setWindowState( windowState() & ~Qt::WindowFullScreen ); // reset
        m_view->dockArea ()->setCentralWidget (this);
        m_view->dockArea ()->restoreState (m_dock_state);
        for (unsigned i = 0; i < m_collection->count (); ++i)
            m_collection->action (i)->setEnabled (false);
        m_view->controlPanel ()->button (ControlPanel::button_playlist)->setIconSet (QIconSet (QPixmap (playlist_xpm)));
        unsetCursor();
    } else {
        m_dock_state = m_view->dockArea ()->saveState ();
        m_topwindow_rect = topLevelWidget ()->geometry ();
        reparent (0L, 0, qApp->desktop()->screenGeometry(this).topLeft(), true);
        setWindowState( windowState() | Qt::WindowFullScreen ); // set
        for (unsigned i = 0; i < m_collection->count (); ++i)
            m_collection->action (i)->setEnabled (true);
        m_view->controlPanel ()->button (ControlPanel::button_playlist)->setIconSet (QIconSet (QPixmap (normal_window_xpm)));
        m_mouse_invisible_timer = startTimer(MOUSE_INVISIBLE_DELAY);
    }
    m_fullscreen = !m_fullscreen;
    m_view->controlPanel()->fullscreenAction->setChecked (m_fullscreen);

#ifdef KMPLAYER_WITH_CAIRO
    if (surface->surface) {
        cairo_surface_destroy (surface->surface);
        surface->surface = 0L;
        d->destroyBackingStore ();
    }
#endif
    emit fullScreenChanged ();
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
        MouseVisitor visitor (this, MsgEventClicked, e->x(), e->y());
        surface->node->accept (&visitor);
    }
}

KDE_NO_EXPORT void ViewArea::mouseDoubleClickEvent (QMouseEvent *) {
    m_view->fullScreen (); // screensaver stuff
}

KDE_NO_EXPORT void ViewArea::mouseMoveEvent (QMouseEvent * e) {
    if (e->state () == Qt::NoButton)
        m_view->mouseMoved (e->x (), e->y ());
    if (surface->node) {
        MouseVisitor visitor (this, MsgEventPointerMoved, e->x(), e->y());
        surface->node->accept (&visitor);
        setCursor (visitor.cursor);
    }
    e->accept ();
    mouseMoved (); // for m_mouse_invisible_timer
}

KDE_NO_EXPORT void ViewArea::syncVisual () {
    IRect rect = m_repaint_rect.intersect (IRect (0, 0, width (), height ()));
#ifdef KMPLAYER_WITH_CAIRO
    if (surface->node) {
        int ex = rect.x ();
        if (ex > 0)
            ex--;
        int ey = rect.y ();
        if (ey > 0)
            ey--;
        int ew = rect.width () + 2;
        int eh = rect.height () + 2;
        IRect swap_rect;
        cairo_surface_t *merge = NULL;
        cairo_pattern_t *pat = NULL;
        cairo_t *cr = NULL;
        if (!surface->surface) {
            surface->surface = d->createSurface (width (), height ());
            swap_rect = IRect (ex, ey, ew, eh);
            CairoPaintVisitor visitor (surface->surface,
                    Matrix (surface->bounds.x(), surface->bounds.y(), 1.0, 1.0),
                    swap_rect,
                    palette ().color (backgroundRole ()), true);
            surface->node->accept (&visitor);
            m_update_rect = IRect ();
        } else if (!rect.isEmpty ()) {
            merge = cairo_surface_create_similar (surface->surface,
                    CAIRO_CONTENT_COLOR, ew, eh);
            Matrix m (surface->bounds.x(), surface->bounds.y(), 1.0, 1.0);
            m.translate (-ex, -ey);
            {
                CairoPaintVisitor visitor (merge, m, IRect (0, 0, ew, eh),
                        palette ().color (backgroundRole ()), true);
                surface->node->accept (&visitor);
            }
            cr = cairo_create (surface->surface);
            pat = cairo_pattern_create_for_surface (merge);
            cairo_pattern_set_extend (pat, CAIRO_EXTEND_NONE);
            cairo_matrix_t mat;
            cairo_matrix_init_translate (&mat, (int) -ex, (int) -ey);
            cairo_pattern_set_matrix (pat, &mat);
            cairo_set_source (cr, pat);
            //cairo_set_operator (cr, CAIRO_OPERATOR_ADD);
            cairo_rectangle (cr, ex, ey, ew, eh);
            //cairo_fill (cr);
            cairo_clip (cr);
            cairo_paint_with_alpha (cr, .8);
            swap_rect = IRect (ex, ey, ew, eh).unite (m_update_rect);
            m_update_rect = IRect (ex, ey, ew, eh);
        } else {
            swap_rect = m_update_rect;
            m_update_rect = IRect ();
        }
        d->swapBuffer (swap_rect, swap_rect.x (), swap_rect.y ());
        if (merge) {
            cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
            cairo_rectangle (cr, ex, ey, ew, eh);
            cairo_fill (cr);
            cairo_destroy (cr);
            cairo_pattern_destroy (pat);
            cairo_surface_destroy (merge);
        }
        cairo_surface_flush (surface->surface);
    } else
#endif
        repaint (QRect(rect.x(), rect.y(), rect.width(), rect.height()), false);
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

QPaintEngine *ViewArea::paintEngine () const {
#ifdef KMPLAYER_WITH_CAIRO
    if (surface->node)
        return NULL;
    else
#endif
        return QWidget::paintEngine ();
}

KDE_NO_EXPORT void ViewArea::scale (int) {
    resizeEvent (0L);
}

KDE_NO_EXPORT void ViewArea::updateSurfaceBounds () {
    Single x, y, w = width (), h = height ();
    h -= m_view->statusBarHeight ();
    h -= m_view->controlPanel ()->isVisible () && !m_fullscreen
        ? (m_view->controlPanelMode () == View::CP_Only
                ? h
                : (Single) m_view->controlPanel()->maximumSize ().height ())
        : Single (0);
    Mrl *mrl = surface->node ? surface->node->mrl () : NULL;
    int scale = m_view->controlPanel ()->scale_slider->sliderPosition ();
    int nw = w * scale / 100;
    int nh = h * scale / 100;
    x += (w - nw) / 2;
    y += (h - nh) / 2;
    w = nw;
    h = nh;
    if (m_view->keepSizeRatio () &&
            w > 0 && h > 0 && mrl && !mrl->size.isEmpty ()) {
        double wasp = (double) w / h;
        double masp = (double) mrl->size.width / mrl->size.height;
        if (wasp > masp) {
            Single tmp = w;
            w = masp * h;
            x += (tmp - w) / 2;
        } else {
            Single tmp = h;
            h = Single (w / masp);
            y += (tmp - h) / 2;
        }
        surface->xscale = 1.0 * w / mrl->size.width;
        surface->yscale = 1.0 * h / mrl->size.height;
    } else {
        surface->xscale = 1.0;
        surface->yscale = 1.0;
    }
    if (surface->node) {
        surface->bounds = SRect (x, y, w, h);
        surface->node->message (MsgSurfaceBoundsUpdate, (void *) true);
    } else {
        surface->resize (SRect (x, y, w, h), true);
    }
    scheduleRepaint (IRect (0, 0, width (), height ()));
}

KDE_NO_EXPORT void ViewArea::resizeEvent (QResizeEvent *) {
    if (!m_view->controlPanel ()) return;
    Single x, y, w = width (), h = height ();
    Single hsb = m_view->statusBarHeight ();
    int hcp = m_view->controlPanel ()->isVisible ()
        ? (m_view->controlPanelMode () == View::CP_Only
                ? h-hsb
                : (Single) m_view->controlPanel()->maximumSize ().height ())
        : Single (0);
    // move controlpanel over video when autohiding and playing
    bool auto_hide = m_view->controlPanelMode () == View::CP_AutoHide;
    h -= Single (auto_hide ? 0 : hcp) - hsb;
    // now scale the regions and check if video region is already sized
    if (surface->node)
        d->destroyBackingStore ();
    updateSurfaceBounds ();
    d->resizeSurface (surface.ptr ());

    // finally resize controlpanel and video widget
    if (m_view->controlPanel ()->isVisible ())
        m_view->controlPanel ()->setGeometry (0, h-(auto_hide ? hcp:0), w, hcp);
    if (m_view->statusBar ()->isVisible ())
        m_view->statusBar ()->setGeometry (0, h-hsb, w, hsb);
    int scale = m_view->controlPanel ()->scale_slider->sliderPosition ();
    Single ws = w * scale / 100;
    Single hs = h * scale / 100;
    x += (w - ws) / 2;
    y += (h - hs) / 2;
    m_view->console ()->setGeometry (0, 0, w, h);
    m_view->picture ()->setGeometry (0, 0, w, h);
    if (!surface->node && video_widgets.size () == 1)
        video_widgets.first ()->setGeometry (IRect (x, y, ws, hs));
}

KDE_NO_EXPORT Surface *ViewArea::getSurface (Mrl *mrl) {
    surface->clear ();
    surface->node = mrl;
    kDebug() << mrl;
    //m_view->viewer()->resetBackgroundColor ();
    if (mrl) {
        updateSurfaceBounds ();
        return surface.ptr ();
    }
#ifdef KMPLAYER_WITH_CAIRO
    else if (surface->surface) {
        cairo_surface_destroy (surface->surface);
        surface->surface = 0L;
        d->destroyBackingStore ();
    }
#endif
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
        m_repaint_timer = startTimer (25);
    }
}

KDE_NO_EXPORT void ViewArea::addUpdater (Node *node) {
    m_updaters = new RepaintUpdater (node, m_updaters);
    if (!m_repaint_timer)
        m_repaint_timer = startTimer (25);
}

KDE_NO_EXPORT void ViewArea::removeUpdater (Node *node) {
    RepaintUpdater *prev = NULL;
    for (RepaintUpdater *r = m_updaters; r; r = r->next) {
        if (r->node.ptr () == node) {
            if (prev)
                prev->next = r->next;
            else
                m_updaters = r->next;
            delete r;
            break;
        }
        prev = r;
    }
    if (m_repaint_timer &&
            (!m_updaters_enabled || !m_updaters) &&
            m_repaint_rect.isEmpty () &&
            m_update_rect.isEmpty ()) {
        killTimer (m_repaint_timer);
        m_repaint_timer = 0;
    }
}

static RepaintUpdater *getFirstUpdater (RepaintUpdater *updaters) {
    for (RepaintUpdater *r = updaters; r; r = updaters) {
        if (updaters->node)
            return updaters;
        updaters = r->next;
        delete r;
    }
    return NULL;
}

static void propagateUpdatersEvent (RepaintUpdater *updaters, void *event) {
    for (RepaintUpdater *r = updaters; r; ) {
        RepaintUpdater *next = r->next;
        if (r->node)
            r->node->message (MsgSurfaceUpdate, event); // may call removeUpdater()
        r = next;
    }
}

KDE_NO_EXPORT
void ViewArea::enableUpdaters (bool enable, unsigned int skip) {
    m_updaters_enabled = enable;
    m_updaters = getFirstUpdater (m_updaters);
    if (enable && m_updaters) {
        UpdateEvent event (m_updaters->node->document (), skip);
        propagateUpdatersEvent (m_updaters, &event);
        if (!m_repaint_timer)
            m_repaint_timer = startTimer (25);
    } else if (!enable && m_repaint_timer &&
            m_repaint_rect.isEmpty () && m_update_rect.isEmpty ()) {
        killTimer (m_repaint_timer);
        m_repaint_timer = 0;
    }
}

KDE_NO_EXPORT void ViewArea::timerEvent (QTimerEvent * e) {
    if (e->timerId () == m_mouse_invisible_timer) {
        killTimer (m_mouse_invisible_timer);
        m_mouse_invisible_timer = 0;
        if (m_fullscreen)
            setCursor (QCursor (Qt::BlankCursor));
    } else if (e->timerId () == m_repaint_timer) {
        m_updaters = getFirstUpdater (m_updaters);
        if (m_updaters_enabled && m_updaters) {
            UpdateEvent event (m_updaters->node->document (), 0);
            propagateUpdatersEvent (m_updaters, &event);
        }
        //repaint (m_repaint_rect, false);
        if (!m_repaint_rect.isEmpty () || !m_update_rect.isEmpty ()) {
            syncVisual ();
            m_repaint_rect = IRect ();
        }
        if (m_update_rect.isEmpty () && (!m_updaters_enabled || !m_updaters)) {
            killTimer (m_repaint_timer);
            m_repaint_timer = 0;
        }
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

static void setXSelectInput (WId wid, long mask) {
    WId r, p, *c;
    unsigned int nr;
    XSelectInput (QX11Info::display (), wid, mask);
    if (XQueryTree (QX11Info::display (), wid, &r, &p, &c, &nr)) {
        for (int i = 0; i < nr; ++i)
            setXSelectInput (c[i], mask);
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
                if ((*i)->clientHandle () == xe->xkey.window &&
                        static_cast <VideoOutput *>(*i)->inputMask() & KeyPressMask) {
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
                    m_view->mouseMoved (x, y);
                    if (x > 0 && x < width () && y > 0 && y < height ())
                        mouseMoved ();
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
                        setXSelectInput (xe->xmap.window,
                                static_cast <VideoOutput *>(*i)->inputMask ());
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
    connect (view->viewArea (), SIGNAL (fullScreenChanged ()),
             this, SLOT (fullScreenChanged ()));
    kDebug() << "VideoOutput::VideoOutput" << endl;
    setMonitoring (MonitorAll);
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
        setXSelectInput (clientWinId (), m_input_mask);
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

WindowId VideoOutput::windowHandle () {
    //return m_plain_window ? clientWinId () : winId ();
    return m_plain_window ? m_plain_window : winId ();
}

WindowId VideoOutput::clientHandle () {
    return clientWinId ();
}

void VideoOutput::setGeometry (const IRect &rect) {
    int x = rect.x (), y = rect.y (), w = rect.width (), h = rect.height ();
    if (m_view->keepSizeRatio ()) {
        // scale video widget inside region
        int hfw = heightForWidth (w);
        if (hfw > 0) {
            if (hfw > h) {
                int old_w = w;
                w = int ((1.0 * h * w)/(1.0 * hfw));
                x += (old_w - w) / 2;
            } else {
                y += (h - hfw) / 2;
                h = hfw;
            }
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

KDE_NO_EXPORT void VideoOutput::setMonitoring (Monitor m) {
    m_input_mask =
        //KeyPressMask | KeyReleaseMask |
        //EnterWindowMask | LeaveWindowMask |
        //FocusChangeMask |
        ExposureMask |
        //StructureNotifyMask |
        SubstructureNotifyMask;
    if (m & MonitorMouse)
        m_input_mask |= PointerMotionMask;
    if (m & MonitorKey)
        m_input_mask |= KeyPressMask;
    if (clientWinId ())
        setXSelectInput (clientWinId (), m_input_mask);
}

KDE_NO_EXPORT void VideoOutput::fullScreenChanged () {
    if (!(m_input_mask & KeyPressMask)) { // FIXME: store monitor when needed
        if (m_view->isFullScreen ())
            m_input_mask |= PointerMotionMask;
        else
            m_input_mask &= ~PointerMotionMask;
    }
    if (clientWinId ())
        setXSelectInput (clientWinId (), m_input_mask);
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
