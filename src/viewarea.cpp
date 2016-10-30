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
#include <qmap.h>
#include <QPalette>
#include <QDesktopWidget>
#include <QtX11Extras/QX11Info>
#include <QPainter>
#include <QMainWindow>
#include <QWidgetAction>
#include <QTextBlock>
#include <QTextDocument>
#include <QAbstractTextDocumentLayout>
#include <QImage>
#include <QAbstractNativeEventFilter>

#include <kactioncollection.h>
#include <kstatusbar.h>
#include <kshortcut.h>
#include <klocale.h>
#include <kdebug.h>

#include "kmplayerview.h"
#include "kmplayercontrolpanel.h"
#include "playlistview.h"
#include "viewarea.h"
#ifdef KMPLAYER_WITH_CAIRO
# include <cairo-xcb.h>
#endif
#include "mediaobject.h"
#include "kmplayer_smil.h"
#include "kmplayer_rp.h"
#include "mediaobject.h"

#include <xcb/xcb.h>

#include <kurl.h>

using namespace KMPlayer;

#if QT_VERSION >= 0x050600
static qreal pixel_device_ratio;
#endif
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
            QImage qi = image->convertToFormat (QImage::Format_RGB32);
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
        zoom->calcSizes (NULL, NULL, width, height, zx, zy, zw, zh);
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

struct KMPLAYER_NO_EXPORT PaintContext
{
    PaintContext (const Matrix& m, const IRect& c)
        : matrix (m)
        , clip (c)
        , fit (fit_default)
        , bg_repeat (SMIL::RegionBase::BgRepeat)
        , bg_image (NULL)
    {}
    Matrix matrix;
    IRect clip;
    Fit fit;
    SMIL::RegionBase::BackgroundRepeat bg_repeat;
    ImageData *bg_image;
};

class KMPLAYER_NO_EXPORT CairoPaintVisitor : public Visitor, public PaintContext {
    cairo_surface_t * cairo_surface;
    // stack vars need for transitions
    TransitionModule *cur_transition;
    cairo_pattern_t * cur_pat;
    cairo_matrix_t cur_mat;
    float opacity;
    bool toplevel;

    void traverseRegion (Node *reg, Surface *s);
    void updateExternal (SMIL::MediaType *av, SurfacePtr s);
    void paint (TransitionModule *trans, MediaOpacity mopacity, Surface *s,
                const IPoint &p, const IRect &);
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
 : PaintContext (m, rect), cairo_surface (cs), toplevel (top)
{
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
    if (layout->root_layout)
        layout->root_layout->accept (this);
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
        PaintContext ctx_save = *(PaintContext *) this;
        matrix = Matrix (rect.x(), rect.y(), s->xscale, s->yscale);
        matrix.transform (ctx_save.matrix);
        clip = clip.intersect (scr);
        if (SMIL::RegionBase::BgInherit != reg->bg_repeat)
            bg_repeat = reg->bg_repeat;
        cairo_save (cr);

        Surface *cs = s->firstChild ();
        if (!s->virtual_size.isEmpty ())
            matrix.translate (-s->x_scroll, -s->y_scroll);

        if (fit_default != reg->fit)
            fit = reg->fit;

        ImageMedia *im = reg->media_info
            ? (ImageMedia *) reg->media_info->media
            : NULL;

        ImageData *bg_img = im && !im->isEmpty() ? im->cached_img.ptr () : NULL;
        if (reg->background_image == "inherit")
            bg_img = bg_image;
        else
            bg_image = bg_img;
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
                int cw = clip.width ();
                int ch = clip.height ();
                switch (bg_repeat) {
                case SMIL::RegionBase::BgRepeatX:
                    if (h < ch)
                        ch = h;
                    break;
                case SMIL::RegionBase::BgRepeatY:
                    if (w < cw)
                        cw = w;
                    break;
                case SMIL::RegionBase::BgNoRepeat:
                    if (w < cw)
                        cw = w;
                    if (h < ch)
                        ch = h;
                    break;
                default:
                    break;
                }
                cairo_rectangle (cr, clip.x (), clip.y (), cw, ch);
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
        *(PaintContext *) this = ctx_save;
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
    float perc = trans->start_progress + (trans->end_progress - trans->start_progress)*cur_transition->trans_gain;
    if (cur_transition->trans_out_active)
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
            (MediaManager::Audio == m->media_info->type ||
             MediaManager::AudioVideo == m->media_info->type)) {
        AudioVideoMedia *avm = static_cast<AudioVideoMedia *> (m->media_info->media);
        if (avm->viewer ()) {
            if (s &&
                    avm->process &&
                    avm->process->state () > IProcess::Ready &&
                    strcmp (m->nodeName (), "audio")) {
                s->xscale = s->yscale = 1; // either scale width/height or use bounds
                avm->viewer ()->setGeometry (s->toScreen (s->bounds.size));
            } else {
                avm->viewer ()->setGeometry (IRect (-60, -60, 50, 50));
            }
        }
    }
}

KDE_NO_EXPORT void CairoPaintVisitor::visit (SMIL::RefMediaType *ref) {
    Surface *s = ref->surface ();
    if (s && ref->external_tree) {
        updateExternal (ref, s);
        return;
    }
    if (!ref->media_info)
        return;
    if (fit_default != fit
            && fit_default == ref->fit
            && fit != ref->effective_fit) {
        ref->effective_fit = fit;
        s->resize (ref->calculateBounds(), false);
    }
    if (ref->media_info->media &&
            ref->media_info->media->type () == MediaManager::Image) {
        if (!s)
            return;

        IRect scr = matrix.toScreen (s->bounds);
        IRect clip_rect = clip.intersect (scr);
        if (clip_rect.isEmpty ())
            return;

        ImageMedia *im = static_cast <ImageMedia *> (ref->media_info->media);
        ImageData *id = im ? im->cached_img.ptr () : NULL;
        if (id && id->flags == ImageData::ImageScalable)
            im->render (scr.size);
        if (!id || im->isEmpty () || ref->size.isEmpty ()) {
            s->remove();
            return;
        }
        if (!s->surface || s->dirty)
            id->copyImage (s, SSize (scr.width (), scr.height ()), cairo_surface, ref->pan_zoom);
        paint (&ref->transition, ref->media_opacity, s, scr.point, clip_rect);
        s->dirty = false;
    } else {
        video (ref, s);
    }
}

KDE_NO_EXPORT void CairoPaintVisitor::paint (TransitionModule *trans,
        MediaOpacity mopacity, Surface *s,
        const IPoint &point, const IRect &rect) {
    cairo_save (cr);
    opacity = 1.0;
    cairo_matrix_init_translate (&cur_mat, -point.x, -point.y);
    cur_pat = cairo_pattern_create_for_surface (s->surface);
    if (trans->active_trans) {
        IRect clip_save = clip;
        clip = rect;
        cur_transition = trans;
        trans->active_trans->accept (this);
        clip = clip_save;
    } else {
        cairo_pattern_set_extend (cur_pat, CAIRO_EXTEND_NONE);
        cairo_pattern_set_matrix (cur_pat, &cur_mat);
        cairo_pattern_set_filter (cur_pat, CAIRO_FILTER_FAST);
        cairo_set_source (cr, cur_pat);
        cairo_rectangle (cr, rect.x(), rect.y(), rect.width(), rect.height());
    }
    opacity *= mopacity.opacity / 100.0;
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
                 (MediaManager::Audio == mrl->media_info->type ||
                  MediaManager::AudioVideo == mrl->media_info->type)))
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
        m.scale (s->xscale, s->yscale);
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
    paint (&av->transition, av->media_opacity, s.ptr (), scr.point, clip_rect);
}

static void setAlignment (QTextDocument &td, unsigned char align) {
    QTextOption opt = td.defaultTextOption();
    if (SmilTextProperties::AlignLeft == align)
        opt.setAlignment (Qt::AlignLeft);
    else if (SmilTextProperties::AlignCenter == align)
        opt.setAlignment (Qt::AlignCenter);
    else if (SmilTextProperties::AlignRight == align)
        opt.setAlignment (Qt::AlignRight);
    td.setDefaultTextOption (opt);
}

static void calculateTextDimensions (const QFont& font,
        const QString& text, Single w, Single h, Single maxh,
        int *pxw, int *pxh, bool markup_text,
        unsigned char align = SmilTextProperties::AlignLeft) {
    QTextDocument td;
    td.setDefaultFont( font );
    td.setDocumentMargin (0);
    QImage img (QSize ((int)w, (int)h), QImage::Format_RGB32);
    td.setPageSize (QSize ((int)w, (int)maxh));
    td.documentLayout()->setPaintDevice (&img);
    if (markup_text)
        td.setHtml( text );
    else
        td.setPlainText( text );
    setAlignment (td, align);
    QRectF r = td.documentLayout()->blockBoundingRect (td.lastBlock());
    *pxw = (int)td.idealWidth ();
    *pxh = (int)(r.y() + r.height());
#if QT_VERSION >= 0x050600
    *pxw = qMin( (int)(*pxw + pixel_device_ratio), (int)w);
#endif
}

static cairo_t *createContext (cairo_surface_t *similar, Surface *s, int w, int h) {
    unsigned int bg_alpha = s->background_color & 0xff000000;
    bool clear = s->surface;
    if (!s->surface)
        s->surface = cairo_surface_create_similar (similar,
                bg_alpha < 0xff000000
                ? CAIRO_CONTENT_COLOR_ALPHA
                : CAIRO_CONTENT_COLOR,
                w, h);
    cairo_t *cr = cairo_create (s->surface);
    if (clear)
        clearSurface (cr, IRect (0, 0, w, h));
    cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);

    if (bg_alpha) {
        if (bg_alpha < 0xff000000)
            CAIRO_SET_SOURCE_ARGB (cr, s->background_color);
        else
            CAIRO_SET_SOURCE_RGB (cr, s->background_color);
        cairo_paint (cr);
    }
    return cr;
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
        bool clear = s->surface;

        QFont font (txt->font_name);
        font.setPixelSize(ft_size);
        if (clear) {
            pxw = scr.width ();
            pxh = scr.height ();
        } else {
            calculateTextDimensions (font, tm->text,
                    w, 2 * ft_size, scr.height (), &pxw, &pxh, false);
        }
        QTextDocument td;
        td.setDocumentMargin (0);
        td.setDefaultFont (font);
        bool have_alpha = (s->background_color & 0xff000000) < 0xff000000;
        QImage img (QSize (pxw, pxh), have_alpha ? QImage::Format_ARGB32 : QImage::Format_RGB32);
        img.fill (s->background_color);
        td.setPageSize (QSize (pxw, pxh + (int)ft_size));
        td.documentLayout()->setPaintDevice (&img);
        setAlignment (td, 1 + (int)txt->halign);
        td.setPlainText (tm->text);
        QPainter painter;
        painter.begin (&img);
        QAbstractTextDocumentLayout::PaintContext ctx;
        ctx.clip = QRect (0, 0, pxw, pxh);
        ctx.palette.setColor (QPalette::Text, QColor (QRgb (txt->font_color)));
        td.documentLayout()->draw (&painter, ctx);
        painter.end();

        cairo_t *cr_txt = createContext (cairo_surface, s, pxw, pxh);
        cairo_surface_t *src_sf = cairo_image_surface_create_for_data (
                img.bits (),
                have_alpha ? CAIRO_FORMAT_ARGB32:CAIRO_FORMAT_RGB24,
                img.width(), img.height(), img.bytesPerLine ());
        cairo_pattern_t *pat = cairo_pattern_create_for_surface (src_sf);
        cairo_pattern_set_extend (pat, CAIRO_EXTEND_NONE);
        cairo_set_operator (cr_txt, CAIRO_OPERATOR_SOURCE);
        cairo_set_source (cr_txt, pat);
        cairo_paint (cr_txt);
        cairo_pattern_destroy (pat);
        cairo_surface_destroy (src_sf);
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
        paint (&txt->transition, txt->media_opacity, s, scr.point, clip_rect);
    s->dirty = false;
}

KDE_NO_EXPORT void CairoPaintVisitor::visit (SMIL::Brush * brush) {
    Surface *s = brush->surface ();
    if (s) {
        opacity = 1.0;
        IRect clip_rect = clip.intersect (matrix.toScreen (s->bounds));
        if (clip_rect.isEmpty ())
            return;
        cairo_save (cr);
        if (brush->transition.active_trans) {
            cur_transition = &brush->transition;
            cur_pat = NULL;
            brush->transition.active_trans->accept (this);
        } else {
            cairo_rectangle (cr, clip_rect.x (), clip_rect.y (),
                    clip_rect.width (), clip_rect.height ());
        }
        unsigned int color = brush->color.color;
        if (!color) {
            color = brush->background_color.color;
            opacity *= brush->background_color.opacity / 100.0;
        } else {
            opacity *= brush->color.opacity / 100.0;
        }
        opacity *= brush->media_opacity.opacity / 100.0;
        if (opacity < 0.99) {
            cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
            cairo_set_source_rgba (cr,
                    1.0 * ((color >> 16) & 0xff) / 255,
                    1.0 * ((color >> 8) & 0xff) / 255,
                    1.0 * (color & 0xff) / 255,
                    opacity);
        } else {
            CAIRO_SET_SOURCE_RGB (cr, color);
        }
        cairo_fill (cr);
        if (opacity < 0.99)
            cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
        s->dirty = false;
        cairo_restore (cr);
    }
}

struct SmilTextBlock {
    SmilTextBlock (const QFont& f, const QString &t,
            IRect r, unsigned char a)
        : font (f), rich_text (t), rect (r), align (a), next (NULL) {}

    QFont font;
    QString rich_text;
    IRect rect;
    unsigned char align;

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
    void visit (SMIL::TemporalMoment *);

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
    QString s = "<span style=\"";
    if (props.font_size.size () > -1)
        s += "font-size:" + QString::number ((int)(scale * props.font_size.size ())) + "px;";
    s += "font-family:" + props.font_family + ";";
    if (props.font_color > -1)
        s += QString().sprintf ("color:#%06x;", props.font_color);
    if (props.background_color > -1)
        s += QString().sprintf ("background-color:#%06x;", props.background_color);
    if (SmilTextProperties::StyleInherit != props.font_style) {
        s += "font-style:";
        switch (props.font_style) {
            case SmilTextProperties::StyleOblique:
                s += "oblique;";
                break;
            case SmilTextProperties::StyleItalic:
                s += "italic;";
                break;
            default:
                s += "normal;";
                break;
        }
    }
    if (SmilTextProperties::WeightInherit != props.font_weight) {
        s += "font-weight:";
        switch (props.font_weight) {
            case SmilTextProperties::WeightBold:
                s += "bold;";
                break;
            default:
                s += "normal;";
                break;
        }
    }
    s += "\">";
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
        float fs = info.props.font_size.size ();
        if (fs < 0)
            fs = TextMedia::defaultFontSize ();
        float maxfs = max_font_size;
        if (maxfs < 1.0)
            maxfs = fs;
        fs *= scale;
        maxfs *= scale;

        QFont font ("Sans");
        font.setPixelSize((int)fs);
        calculateTextDimensions (font, rich_text.toUtf8 ().constData (),
                width, 2 * maxfs, 1024, &pxw, &pxh, true, info.props.text_align);
        int x = 0;
        if (SmilTextProperties::AlignCenter == info.props.text_align)
            x = (width - pxw) / 2;
        else if (SmilTextProperties::AlignRight == info.props.text_align)
            x = width - pxw;
        SmilTextBlock *block = new SmilTextBlock (font, rich_text,
                IRect (x, voffset, pxw, pxh), info.props.text_align);
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
    QTextStream out (&buffer, QIODevice::WriteOnly);
    out << XMLStringlet (text->nodeValue ());
    addRichText (buffer);
    if (text->nextSibling ())
        text->nextSibling ()->accept (this);
}

void SmilTextVisitor::visit (SMIL::TextFlow *flow) {
    bool new_block = SMIL::id_node_p == flow->id ||
        SMIL::id_node_br == flow->id ||
        SMIL::id_node_div == flow->id;
    if ((new_block && !rich_text.isEmpty ()) || flow->firstChild ()) {
        float fs = info.props.font_size.size ();
        if (fs < 0)
            fs = TextMedia::defaultFontSize ();
        int par_extra = SMIL::id_node_p == flow->id
            ? (int)(scale * fs) : 0;
        voffset += par_extra;

        SmilTextInfo saved_info = info;
        if (new_block)
            push ();

        info.props.mask (flow->props);
        if ((float)info.props.font_size.size () > max_font_size)
            max_font_size = info.props.font_size.size ();
        info.span (scale);

        if (flow->firstChild ())
            flow->firstChild ()->accept (this);

        if (rich_text.isEmpty ())
            par_extra = 0;
        if (new_block && flow->firstChild ())
            push ();
        voffset += par_extra;

        info = saved_info;
    }
    if (flow->nextSibling ())
        flow->nextSibling ()->accept (this);
}

void SmilTextVisitor::visit (SMIL::TemporalMoment *tm) {
    if (tm->state >= Node::state_began
            && tm->nextSibling ())
        tm->nextSibling ()->accept (this);
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

        Node *first = txt->firstChild ();
        for (Node *n = first; n; n = n->nextSibling ())
            if (SMIL::id_node_clear == n->id) {
                if (n->state >= Node::state_began)
                    first = n->nextSibling ();
                else
                    break;
            }
        if (first)
            first->accept (&info);

        info.push ();
        if (info.first) {
            cairo_t *cr_txt = createContext (cairo_surface, s, (int) w, info.voffset);

            CAIRO_SET_SOURCE_RGB (cr_txt, 0);
            SmilTextBlock *b = info.first;
            int hoff = 0;
            int voff = 0;
            while (b) {
                cairo_translate (cr_txt, b->rect.x() - hoff, b->rect.y() - voff);
                QTextDocument td;
                td.setDocumentMargin (0);
                td.setDefaultFont (b->font);
                bool have_alpha = (s->background_color & 0xff000000) < 0xff000000;
                QImage img (QSize (b->rect.width(), b->rect.height()), have_alpha ? QImage::Format_ARGB32 : QImage::Format_RGB32);
                img.fill (s->background_color);
                td.setPageSize (QSize (b->rect.width(), b->rect.height() + 10));
                setAlignment (td, b->align);
                td.documentLayout()->setPaintDevice (&img);
                td.setHtml (b->rich_text);
                QPainter painter;
                painter.begin (&img);
                QAbstractTextDocumentLayout::PaintContext ctx;
                ctx.clip = QRect (QPoint (0, 0), img.size ());
                td.documentLayout()->draw (&painter, ctx);
                painter.end();

                cairo_surface_t *src_sf = cairo_image_surface_create_for_data (
                        img.bits (),
                        have_alpha ? CAIRO_FORMAT_ARGB32:CAIRO_FORMAT_RGB24,
                        img.width(), img.height(), img.bytesPerLine ());
                cairo_pattern_t *pat = cairo_pattern_create_for_surface (src_sf);
                cairo_pattern_set_extend (pat, CAIRO_EXTEND_NONE);
                cairo_set_operator (cr_txt, CAIRO_OPERATOR_SOURCE);
                cairo_set_source (cr_txt, pat);
                cairo_rectangle (cr_txt, 0, 0, b->rect.width(), b->rect.height());
                cairo_fill (cr_txt);
                cairo_pattern_destroy (pat);
                cairo_surface_destroy (src_sf);

                hoff = b->rect.x ();
                voff = b->rect.y ();
                SmilTextBlock *tmp = b;
                b = b->next;
                delete tmp;
            }
            cairo_destroy (cr_txt);

            // update bounds rect
            s->bounds = matrix.toUser (IRect (scr.point, ISize (w, info.voffset)));
            txt->size = s->bounds.size;
            txt->updateBounds (false);

            // update coord. for painting below
            scr = matrix.toScreen (s->bounds);
        }
    }
    IRect clip_rect = clip.intersect (scr);
    if (s->surface && !clip_rect.isEmpty ())
        paint (&txt->transition, txt->media_opacity, s, scr.point, clip_rect);
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
    CAIRO_SET_SOURCE_RGB (cr, fi->color);
    if ((int)fi->w && (int)fi->h) {
        cairo_rectangle (cr, fi->x, fi->y, fi->w, fi->h);
        cairo_fill (cr);
    }
}

KDE_NO_EXPORT void CairoPaintVisitor::visit (RP::Fadein * fi) {
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

    bool deliverAndForward (Node *n, Surface *s, bool inside, bool deliver);
    void surfaceEvent (Node *mt, Surface *s);
public:
    MouseVisitor (ViewArea *v, MessageType evt, Matrix m, int x, int y);
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
MouseVisitor::MouseVisitor (ViewArea *v, MessageType evt, Matrix m, int a, int b)
  : view_area (v), matrix (m), event (evt), x (a), y (b),
    handled (false), bubble_up (false) {
}

KDE_NO_EXPORT void MouseVisitor::visit (Node * n) {
    kDebug () << "Mouse event ignored for " << n->nodeName ();
}

KDE_NO_EXPORT void MouseVisitor::visit (SMIL::Smil *s) {
    if (s->active () && s->layout_node)
        s->layout_node->accept (this);
}

KDE_NO_EXPORT void MouseVisitor::visit (SMIL::Layout * layout) {
    if (layout->root_layout)
        layout->root_layout->accept (this);
}

KDE_NO_EXPORT void MouseVisitor::visit (SMIL::RegionBase *region) {
    Surface *s = (Surface *) region->role (RoleDisplay);
    if (s) {
        SRect rect = s->bounds;
        IRect scr = matrix.toScreen (rect);
        int rx = scr.x(), ry = scr.y(), rw = scr.width(), rh = scr.height();
        handled = false;
        bool inside = x > rx && x < rx+rw && y > ry && y< ry+rh;
        if (!inside && (event == MsgEventClicked || !s->has_mouse))
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

        NodePtrW src = source;
        source = region;
        Matrix m = matrix;
        matrix = Matrix (rect.x(), rect.y(), 1.0, 1.0);
        matrix.transform (m);
        if (!s->virtual_size.isEmpty ())
            matrix.translate (-s->x_scroll, -s->y_scroll);
        bubble_up = false;

        bool child_handled = false;
        if (inside || s->has_mouse)
            for (SurfacePtr c = s->firstChild (); c; c = c->nextSibling ()) {
                if (c->node && c->node->id == SMIL::id_node_region) {
                    c->node->accept (this);
                    child_handled |= handled;
                    if (!source || !source->active ())
                        break;
                } else {
                    break;
                }
            }
        child_handled &= !bubble_up;
        bubble_up = false;
        if (source && source->active ())
            deliverAndForward (region, s, inside, !child_handled);

        handled = inside;
        matrix = m;
        source = src;
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
            notify->openUrl(KUrl(link->href), link->target, QString());
        } else {
            NodePtr n = link;
            for (NodePtr p = link->parentNode (); p; p = p->parentNode ()) {
                if (n->mrl () && n->mrl ()->opener == p) {
                    p->setState (Node::state_deferred);
                    p->mrl ()->setParam (Ids::attr_src, link->href, 0L);
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

bool MouseVisitor::deliverAndForward (Node *node, Surface *s, bool inside, bool deliver) {
    bool forward = deliver;
    MessageType user_event = event;
    if (event == MsgEventPointerMoved) {
        forward = true; // always pass move events
        if (inside && !s->has_mouse) {
            deliver = true;
            user_event = MsgEventPointerInBounds;
        } else if (!inside && s->has_mouse) {
            deliver = true;
            user_event = MsgEventPointerOutBounds;
        } else if (!inside) {
            return false;
        } else {
            deliver = false;
        }
    }
    s->has_mouse = inside;

    if (event != MsgEventPointerMoved && !inside)
        return false;

    NodePtrW node_save = node;

    if (forward) {
        ConnectionList *nl = nodeMessageReceivers (node, MsgSurfaceAttach);
        if (nl) {
            NodePtr node_save = source;
            source = node;

            for (Connection *c = nl->first(); c; c = nl->next ()) {
                if (c->connecter)
                    c->connecter->accept (this);
                if (!source || !source->active ())
                    break;
            }
            source = node_save;
        }
    }
    if (!node_save || !node->active ())
        return false;
    if (deliver) {
        Posting mouse_event (node, user_event);
        node->deliver (user_event, &mouse_event);
    }
    if (!node_save || !node->active ())
        return false;
    return true;
}

void MouseVisitor::surfaceEvent (Node *node, Surface *s) {
    if (!s)
        return;
    if (s->node && s->node.ptr () != node) {
        s->node->accept (this);
        return;
    }
    SRect rect = s->bounds;
    IRect scr = matrix.toScreen (rect);
    int rx = scr.x(), ry = scr.y(), rw = scr.width(), rh = scr.height();
    const bool inside = x > rx && x < rx+rw && y > ry && y< ry+rh;
    const bool had_mouse = s->has_mouse;
    if (deliverAndForward (node, s, inside, true) &&
            (inside || had_mouse) &&
            s->firstChild () && s->firstChild ()->node) {
        Matrix m = matrix;
        matrix = Matrix (rect.x(), rect.y(), s->xscale, s->yscale);
        matrix.transform (m);
        s->firstChild ()->node->accept (this);
        matrix = m;
    }
}

KDE_NO_EXPORT void MouseVisitor::visit (SMIL::MediaType *mt) {
    if (mt->sensitivity == SMIL::MediaType::sens_transparent)
        bubble_up = true;
    else
        surfaceEvent (mt, mt->surface ());
}

KDE_NO_EXPORT void MouseVisitor::visit (SMIL::SmilText *st) {
    surfaceEvent (st, st->surface ());
}

//-----------------------------------------------------------------------------

namespace KMPlayer {
class KMPLAYER_NO_EXPORT ViewerAreaPrivate {
public:
    ViewerAreaPrivate (ViewArea *v)
        : m_view_area (v), backing_store (0), gc(0),
          screen(NULL), visual(NULL), width(0), height(0)
    {}
    ~ViewerAreaPrivate() {
        destroyBackingStore ();
        if (gc) {
            xcb_connection_t* connection = QX11Info::connection();
            xcb_free_gc(connection, gc);
        }
    }
    void clearSurface (Surface *s) {
#ifdef KMPLAYER_WITH_CAIRO
        if (s->surface) {
            cairo_surface_destroy (s->surface);
            s->surface = 0L;
        }
        destroyBackingStore ();
#endif
    }
    void resizeSurface (Surface *s) {
#ifdef KMPLAYER_WITH_CAIRO
#if QT_VERSION >= 0x050600
        int w = (int)(m_view_area->width() * m_view_area->devicePixelRatioF());
        int h = (int)(m_view_area->height() * m_view_area->devicePixelRatioF());
#else
        int w = m_view_area->width ();
        int h = m_view_area->height ();
#endif
        if ((w != width || h != height) && s->surface) {
            clearSurface (s);
            width = w;
            height = h;
        }
#endif
    }
#ifdef KMPLAYER_WITH_CAIRO
    cairo_surface_t *createSurface (int w, int h) {
        xcb_connection_t* connection = QX11Info::connection();
        destroyBackingStore ();
        xcb_screen_t* scr = screen_of_display(connection, QX11Info::appScreen());
        backing_store = xcb_generate_id(connection);
        xcb_void_cookie_t cookie = xcb_create_pixmap_checked(connection, scr->root_depth, backing_store, m_view_area->winId(), w, h);
        xcb_generic_error_t* error = xcb_request_check(connection, cookie);
        if (error) {
            qDebug("failed to create pixmap");
            return NULL;
        }
        return cairo_xcb_surface_create(connection, backing_store, visual_of_screen(connection, scr), w, h);
    }
    void swapBuffer (const IRect &sr, int dx, int dy) {
        xcb_connection_t* connection = QX11Info::connection();
        if (!gc) {
            gc = xcb_generate_id(connection);
            uint32_t values[] = { XCB_GX_COPY, XCB_FILL_STYLE_SOLID,
                XCB_SUBWINDOW_MODE_CLIP_BY_CHILDREN, 0 };
            xcb_create_gc(connection, gc, backing_store,
                    XCB_GC_FUNCTION | XCB_GC_FILL_STYLE |
                    XCB_GC_SUBWINDOW_MODE | XCB_GC_GRAPHICS_EXPOSURES, values);
        }
        xcb_copy_area(connection, backing_store, m_view_area->winId(),
                gc, sr.x(), sr.y(), dx, dy, sr.width (), sr.height ());
        xcb_flush(connection);
    }
#endif
    void destroyBackingStore () {
#ifdef KMPLAYER_WITH_CAIRO
        if (backing_store) {
            xcb_connection_t* connection = QX11Info::connection();
            xcb_free_pixmap(connection, backing_store);
        }
#endif
        backing_store = 0;
    }
    xcb_screen_t *screen_of_display(xcb_connection_t* c, int num)
    {
        if (!screen) {
            xcb_screen_iterator_t iter;

            iter = xcb_setup_roots_iterator (xcb_get_setup (c));
            for (; iter.rem; --num, xcb_screen_next (&iter))
                if (num == 0) {
                    screen = iter.data;
                    break;
                }
        }
        return screen;
    }

    xcb_visualtype_t* visual_of_screen(xcb_connection_t* c, xcb_screen_t* screen)
    {
        if (!visual) {
            xcb_depth_iterator_t depth_iter = xcb_screen_allowed_depths_iterator (screen);
            for (; depth_iter.rem; xcb_depth_next (&depth_iter)) {
                xcb_visualtype_iterator_t visual_iter = xcb_depth_visuals_iterator (depth_iter.data);
                for (; visual_iter.rem; xcb_visualtype_next (&visual_iter)) {
                    if (screen->root_visual == visual_iter.data->visual_id) {
                        visual = visual_iter.data;
                        break;
                    }
                }
            }
        }
        return visual;
    }
    ViewArea *m_view_area;
    xcb_drawable_t backing_store;
    xcb_gcontext_t gc;
    xcb_screen_t* screen;
    xcb_visualtype_t* visual;
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

KDE_NO_CDTOR_EXPORT ViewArea::ViewArea (QWidget *, View * view, bool paint_bg)
// : QWidget (parent, "kde_kmplayer_viewarea", WResizeNoErase | WRepaintNoErase),
 : //QWidget (parent),
   d (new ViewerAreaPrivate (this)),
   m_view (view),
   m_collection (new KActionCollection (this)),
   surface (new Surface (this)),
   m_mouse_invisible_timer (0),
   m_repaint_timer (0),
   m_restore_fullscreen_timer (0),
   m_fullscreen (false),
   m_minimal (false),
   m_updaters_enabled (true),
   m_paint_background (paint_bg) {
    if (!paint_bg)
        setAttribute (Qt::WA_NoSystemBackground, true);
    QPalette palette;
    palette.setColor (backgroundRole(), QColor (0, 0, 0));
    setPalette (palette);
    setAcceptDrops (true);
    //new KAction (i18n ("Fullscreen"), KShortcut (Qt::Key_F), this, SLOT (accelActivated ()), m_collection, "view_fullscreen_toggle");
    setMouseTracking (true);
    setFocusPolicy (Qt::ClickFocus);
    QCoreApplication::instance()->installNativeEventFilter(this);
}

KDE_NO_CDTOR_EXPORT ViewArea::~ViewArea () {
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
        setVisible(false);
        setWindowState(windowState() & ~Qt::WindowFullScreen); // reset
        if (!m_restore_fullscreen_timer)
            m_restore_fullscreen_timer = startTimer(25);
        for (int i = 0; i < m_collection->count (); ++i)
            m_collection->action (i)->setEnabled (false);
        m_view->controlPanel()->enableFullscreenButton(false);
        unsetCursor();
    } else {
        m_topwindow_rect = topLevelWidget ()->geometry ();
#if QT_VERSION >= 0x050200
        m_view->dockArea()->takeCentralWidget();
#else
        setParent (0L);
#endif
        move(qApp->desktop()->screenGeometry(this).topLeft());
        setVisible(true);
        setWindowState( windowState() ^ Qt::WindowFullScreen ); // set
        for (int i = 0; i < m_collection->count (); ++i)
            m_collection->action (i)->setEnabled (true);
        m_view->controlPanel()->enableFullscreenButton(true);
        m_mouse_invisible_timer = startTimer(MOUSE_INVISIBLE_DELAY);
    }
    m_fullscreen = !m_fullscreen;
    m_view->controlPanel()->fullscreenAction->setChecked (m_fullscreen);

    d->clearSurface (surface.ptr ());
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
        m_view->controlPanel()->enableFullscreenButton(true);
    } else {
        m_view->setControlPanelMode (KMPlayer::View::CP_Show);
        m_view->setNoInfoMessages (false);
        m_view->controlPanel()->enableFullscreenButton(false);
    }
    m_topwindow_rect = topLevelWidget ()->geometry ();
}

KDE_NO_EXPORT void ViewArea::accelActivated () {
    m_view->controlPanel()->fullscreenAction->trigger ();
}

KDE_NO_EXPORT void ViewArea::keyPressEvent (QKeyEvent *e) {
    if (surface->node) {
        QString txt = e->text ();
        if (!txt.isEmpty ())
            surface->node->document ()->message (MsgAccessKey,
                    (void *)(long) txt[0].unicode ());
    }
}

KDE_NO_EXPORT void ViewArea::mousePressEvent (QMouseEvent * e) {
#if QT_VERSION >= 0x050600
    int devicex = (int)(e->x() * devicePixelRatioF());
    int devicey = (int)(e->y() * devicePixelRatioF());
#else
    int devicex = e->x();
    int devicey = e->y();
#endif
    if (surface->node) {
        MouseVisitor visitor (this, MsgEventClicked,
                Matrix (surface->bounds.x (), surface->bounds.y (),
                    surface->xscale, surface->yscale),
                devicex, devicey);
        surface->node->accept (&visitor);
    }
}

KDE_NO_EXPORT void ViewArea::mouseDoubleClickEvent (QMouseEvent *) {
    m_view->fullScreen (); // screensaver stuff
}

KDE_NO_EXPORT void ViewArea::mouseMoveEvent (QMouseEvent * e) {
    if (e->buttons () == Qt::NoButton)
        m_view->mouseMoved (e->x (), e->y ());
    if (surface->node) {
#if QT_VERSION >= 0x050600
        int devicex = (int)(e->x() * devicePixelRatioF());
        int devicey = (int)(e->y() * devicePixelRatioF());
#else
        int devicex = e->x();
        int devicey = e->y();
#endif
        MouseVisitor visitor (this, MsgEventPointerMoved,
                Matrix (surface->bounds.x (), surface->bounds.y (),
                    surface->xscale, surface->yscale),
                devicex, devicey);
        surface->node->accept (&visitor);
        setCursor (visitor.cursor);
    }
    e->accept ();
    mouseMoved (); // for m_mouse_invisible_timer
}

KDE_NO_EXPORT void ViewArea::syncVisual () {
#if QT_VERSION >= 0x050600
    pixel_device_ratio = devicePixelRatioF();
    int w = (int)(width() * devicePixelRatioF());
    int h = (int)(height() * devicePixelRatioF());
#else
    int w = width();
    int h = heigth();
#endif
    IRect rect = m_repaint_rect.intersect (IRect (0, 0, w, h));
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
            surface->surface = d->createSurface(w, h);
            swap_rect = IRect (ex, ey, ew, eh);
            CairoPaintVisitor visitor (surface->surface,
                    Matrix (surface->bounds.x(), surface->bounds.y(),
                        surface->xscale, surface->yscale),
                    swap_rect,
                    palette ().color (backgroundRole ()), true);
            surface->node->accept (&visitor);
            m_update_rect = IRect ();
        } else if (!rect.isEmpty ()) {
            merge = cairo_surface_create_similar (surface->surface,
                    CAIRO_CONTENT_COLOR, ew, eh);
            {
                CairoPaintVisitor visitor (merge,
                        Matrix (surface->bounds.x()-ex, surface->bounds.y()-ey,
                            surface->xscale, surface->yscale),
                        IRect (0, 0, ew, eh),
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
    {
        m_update_rect = IRect ();
#if QT_VERSION >= 0x050600
        repaint(QRect(rect.x() / devicePixelRatioF(),
                      rect.y() / devicePixelRatioF(),
                      rect.width() / devicePixelRatioF(),
                      rect.height() / devicePixelRatioF()));
#else
        repaint (QRect(rect.x(), rect.y(), rect.width(), rect.height()));
#endif
    }
}

KDE_NO_EXPORT void ViewArea::paintEvent (QPaintEvent * pe) {
#ifdef KMPLAYER_WITH_CAIRO
    if (surface->node) {
#if QT_VERSION >= 0x050600
        int x = (int)(pe->rect().x() * devicePixelRatioF());
        int y = (int)(pe->rect().y() * devicePixelRatioF());
        int w = (int)(pe->rect().width() * devicePixelRatioF());
        int h = (int)(pe->rect().height() * devicePixelRatioF());
#else
        int x = pe->rect().x();
        int y = pe->rect().y();
        int w = pe->rect().width();
        int h = pe->rect().height();
#endif
        scheduleRepaint(IRect(x, y, w, h));
    } else
#endif
        if (m_fullscreen || m_paint_background)
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
#if QT_VERSION >= 0x050600
    int devicew = (int)(width() * devicePixelRatioF());
    int deviceh = (int)(height() * devicePixelRatioF());
#else
    int devicew = width, deviceh = height()
#endif
    Single x, y, w = devicew, h = deviceh;
    h -= m_view->statusBarHeight ();
    h -= m_view->controlPanel ()->isVisible () && !m_fullscreen
        ? (m_view->controlPanelMode () == View::CP_Only
                ? h
                : (Single) m_view->controlPanel()->maximumSize ().height ())
        : Single (0);

    int scale = m_view->controlPanel ()->scale_slider->sliderPosition ();
    if (scale != 100) {
        int nw = w * 1.0 * scale / 100;
        int nh = h * 1.0 * scale / 100;
        x += (w - nw) / 2;
        y += (h - nh) / 2;
        w = nw;
        h = nh;
    }
    if (surface->node) {
        d->resizeSurface (surface.ptr ());
        surface->resize (SRect (x, y, w, h));
        surface->node->message (MsgSurfaceBoundsUpdate, (void *) true);
    }
    scheduleRepaint (IRect (0, 0, devicew, deviceh));
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
    updateSurfaceBounds ();

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
    if (!surface->node && video_widgets.size () == 1) {
#if QT_VERSION >= 0x050600
        x *= devicePixelRatioF();
        y *= devicePixelRatioF();
        ws *= devicePixelRatioF();
        hs *= devicePixelRatioF();
#endif
        video_widgets.first ()->setGeometry (IRect (x, y, ws, hs));
    }
}

KDE_NO_EXPORT Surface *ViewArea::getSurface (Mrl *mrl) {
    surface->clear ();
    surface->node = mrl;
    kDebug() << mrl;
    //m_view->viewer()->resetBackgroundColor ();
    if (mrl) {
        updateSurfaceBounds ();
#ifdef KMPLAYER_WITH_CAIRO
        setAttribute (Qt::WA_OpaquePaintEvent, true);
        setAttribute (Qt::WA_PaintOnScreen, true);
#endif
        return surface.ptr ();
    } else {
#ifdef KMPLAYER_WITH_CAIRO
        setAttribute (Qt::WA_OpaquePaintEvent, false);
        setAttribute (Qt::WA_PaintOnScreen, false);
        d->clearSurface (surface.ptr ());
#endif
    }
#if QT_VERSION >= 0x050600
    int devicew = (int)(width() * devicePixelRatioF());
    int deviceh = (int)(height() * devicePixelRatioF());
    scheduleRepaint (IRect (0, 0, devicew, deviceh));
#else
    scheduleRepaint (IRect (0, 0, width (), height ()));
#endif
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

KDE_NO_EXPORT ConnectionList *ViewArea::updaters () {
    if (!m_repaint_timer)
        m_repaint_timer = startTimer (25);
    return &m_updaters;
}

KDE_NO_EXPORT
void ViewArea::enableUpdaters (bool enable, unsigned int skip) {
    m_updaters_enabled = enable;
    Connection *connect = m_updaters.first ();
    if (enable && connect) {
        UpdateEvent event (connect->connecter->document (), skip);
        for (; connect; connect = m_updaters.next ())
            if (connect->connecter)
                connect->connecter->message (MsgSurfaceUpdate, &event);
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
        Connection *connect = m_updaters.first ();
        int count = 0;
        if (m_updaters_enabled && connect) {
            UpdateEvent event (connect->connecter->document (), 0);
            for (; connect; count++, connect = m_updaters.next ())
                if (connect->connecter)
                    connect->connecter->message (MsgSurfaceUpdate, &event);
        }
        //repaint (m_repaint_rect, false);
        if (!m_repaint_rect.isEmpty () || !m_update_rect.isEmpty ()) {
            syncVisual ();
            m_repaint_rect = IRect ();
        }
        if (m_update_rect.isEmpty () &&
                (!m_updaters_enabled || !m_updaters.first ())) {
            killTimer (m_repaint_timer);
            m_repaint_timer = 0;
        }
    } else if (e->timerId () == m_restore_fullscreen_timer) {
        xcb_connection_t* connection = QX11Info::connection();
        xcb_get_window_attributes_cookie_t cookie = xcb_get_window_attributes(connection, winId());
        xcb_get_window_attributes_reply_t* attrs = xcb_get_window_attributes_reply(connection, cookie, NULL);
        if (attrs->map_state == XCB_MAP_STATE_UNMAPPED) {
            m_view->dockArea ()->setCentralWidget (this);
            killTimer(m_restore_fullscreen_timer);
            m_restore_fullscreen_timer = 0;
        }
        free(attrs);
    } else {
        kError () << "unknown timer " << e->timerId () << " " << m_repaint_timer << endl;
        killTimer (e->timerId ());
    }
}

KDE_NO_EXPORT void ViewArea::closeEvent (QCloseEvent * e) {
    //kDebug () << "closeEvent";
    if (m_fullscreen) {
        m_view->fullScreen();
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
    int i = video_widgets.indexOf(widget);
    if (i >= 0) {
        IViewer *viewer = widget;
        delete viewer;
        video_widgets.removeAt(i);
    } else {
        kWarning () << "destroyVideoWidget widget not found" << endl;
    }
}

void ViewArea::setVideoWidgetVisible (bool show) {
    const VideoWidgetList::iterator e = video_widgets.end ();
    for (VideoWidgetList::iterator it = video_widgets.begin (); it != e; ++it)
        static_cast <VideoOutput *> (*it)->setVisible (show);
}

static void setXSelectInput(WId wid, uint32_t mask) {
    xcb_connection_t* connection = QX11Info::connection();
    const uint32_t values[] = { mask };
    xcb_change_window_attributes(connection, wid, XCB_CW_EVENT_MASK, values);
    xcb_query_tree_cookie_t biscuit = xcb_query_tree(connection, wid);
    xcb_query_tree_reply_t *reply = xcb_query_tree_reply(connection, biscuit, NULL);
    if (reply) {
        xcb_window_t *chlds = xcb_query_tree_children(reply);
        for (int i = 0; i < xcb_query_tree_children_length(reply); i++)
            setXSelectInput(chlds[i], mask);
        free(reply);
    } else {
        qDebug("failed to get x children");
    }
}

bool ViewArea::nativeEventFilter(const QByteArray& eventType, void * message, long *result) {
    if (eventType != "xcb_generic_event_t")
        return false;

    xcb_generic_event_t* event = (xcb_generic_event_t*)message;
    switch (event->response_type & ~0x80) {
    case XCB_UNMAP_NOTIFY: {
        xcb_unmap_notify_event_t* ev = (xcb_unmap_notify_event_t*)event;
        if (ev->event != ev->window) {
            const VideoWidgetList::iterator e = video_widgets.end ();
            for (VideoWidgetList::iterator i=video_widgets.begin(); i != e; ++i) {
                if (ev->event == (*i)->ownHandle()) {
                    (*i)->embedded(0);
                    break;
                }
            }
        }
        break;
    }
    case XCB_MAP_NOTIFY: {
        xcb_map_notify_event_t* ev = (xcb_map_notify_event_t*)event;
        if (!ev->override_redirect && ev->event != ev->window) {
            xcb_connection_t* connection = QX11Info::connection();
            const VideoWidgetList::iterator e = video_widgets.end ();
            for (VideoWidgetList::iterator i=video_widgets.begin(); i != e; ++i) {
                if (ev->event == (*i)->ownHandle()) {
                    (*i)->embedded(ev->window);
                    return false;
                }
                xcb_window_t p = ev->event;
                xcb_window_t w = ev->window;
                xcb_window_t v = (*i)->clientHandle ();
                xcb_window_t va = winId ();
                xcb_window_t root = 0;
                while (p != v) {
                    xcb_query_tree_cookie_t cookie = xcb_query_tree(connection, w);
                    xcb_query_tree_reply_t *reply = xcb_query_tree_reply(connection, cookie, NULL);
                    if (reply) {
                        p = reply->parent;
                        root = reply->root;
                        free(reply);
                    } else {
                        qDebug("failed to get x parent");
                        break;
                    }
                    if (p == va || p == v || p == root)
                        break;
                    w = p;
                }
                if (p == v) {
                    setXSelectInput (ev->window,
                            static_cast <VideoOutput *>(*i)->inputMask ());
                    break;
                }
            }
        }
        break;
    }
    case XCB_MOTION_NOTIFY: {
        xcb_motion_notify_event_t* ev = (xcb_motion_notify_event_t*)event;
        if (m_view->controlPanelMode () == View::CP_AutoHide) {
            const VideoWidgetList::iterator e = video_widgets.end ();
            for (VideoWidgetList::iterator i=video_widgets.begin(); i != e; ++i) {
                QPoint p = mapToGlobal (QPoint (0, 0));
                int x = ev->root_x - p.x ();
                int y = ev->root_y - p.y ();
#if QT_VERSION >= 0x050600
                m_view->mouseMoved(x / devicePixelRatioF(), y / devicePixelRatioF());
                int devicew = (int)(width() * devicePixelRatioF());
                int deviceh = (int)(height() * devicePixelRatioF());
#else
                m_view->mouseMoved (x, y);
                int devicew = width();
                int deviceh = height();
#endif
                if (x > 0 && x < devicew && y > 0 && y < deviceh)
                    mouseMoved ();
            }
        }
        break;
    }
    case XCB_KEY_PRESS: {
        xcb_key_press_event_t* ev = (xcb_key_press_event_t*)event;
        const VideoWidgetList::iterator e = video_widgets.end ();
        for (VideoWidgetList::iterator i=video_widgets.begin(); i != e; ++i)
            if ((*i)->clientHandle () == ev->event &&
                    static_cast <VideoOutput *>(*i)->inputMask() & XCB_EVENT_MASK_KEY_PRESS) {
                if (ev->detail == 41 /*FIXME 'f'*/)
                    m_view->fullScreen ();
                break;
            }
        break;
    }
    default:
        break;
    }
    return false;
}

//----------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT VideoOutput::VideoOutput (QWidget *parent, View * view)
  : QX11EmbedContainer (parent),
    m_plain_window(0), m_client_window(0), resized_timer(0),
    m_bgcolor (0), m_aspect (0.0),
    m_view (view)
{
    setAcceptDrops (true);
    connect (view->viewArea (), SIGNAL (fullScreenChanged ()),
             this, SLOT (fullScreenChanged ()));
    kDebug() << "VideoOutput::VideoOutput" << endl;
    setMonitoring (MonitorAll);
    setAttribute (Qt::WA_NoSystemBackground, true);

    xcb_connection_t* connection = QX11Info::connection();
    xcb_get_window_attributes_cookie_t cookie = xcb_get_window_attributes(connection, winId());
    xcb_get_window_attributes_reply_t* attrs = xcb_get_window_attributes_reply(connection, cookie, NULL);
    if (!(attrs->your_event_mask & XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY))
        setXSelectInput(winId(), attrs->your_event_mask | XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY);
    free(attrs);
    //setProtocol (QXEmbed::XPLAIN);
}

KDE_NO_CDTOR_EXPORT VideoOutput::~VideoOutput () {
    kDebug() << "VideoOutput::~VideoOutput" << endl;
    if (m_plain_window) {
        xcb_connection_t* connection = QX11Info::connection();
        xcb_destroy_window(connection, m_plain_window);
        xcb_flush(connection);
        m_plain_window = 0;
    }
}

void VideoOutput::useIndirectWidget (bool inderect) {
    kDebug () << "setIntermediateWindow " << !!m_plain_window << "->" << inderect;
    if (!clientWinId () || !!m_plain_window != inderect) {
        xcb_connection_t* connection = QX11Info::connection();
        if (inderect) {
            if (!m_plain_window) {
                xcb_screen_t* scr = m_view->viewArea()->d->screen_of_display(connection, QX11Info::appScreen());
                m_plain_window = xcb_generate_id(connection);
                uint32_t values[] = { scr->black_pixel, m_input_mask };
#if QT_VERSION >= 0x050600
                int devicew = (int)(width() * devicePixelRatioF());
                int deviceh = (int)(height() * devicePixelRatioF());
#else
                int devicew = width();
                int deviceh = height();
#endif
                xcb_create_window(connection,
                        XCB_COPY_FROM_PARENT, m_plain_window, winId(),
                        0, 0, devicew, deviceh,
                        1, XCB_WINDOW_CLASS_INPUT_OUTPUT, XCB_COPY_FROM_PARENT,
                        XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK, values);
                xcb_map_window(connection, m_plain_window);
                xcb_flush(connection);
                //XSync (QX11Info::display (), false);
                //embedClient (m_plain_window);
            }
            //XClearWindow (QX11Info::display(), m_plain_window);
        } else {
            if (m_plain_window) {
                xcb_unmap_window(connection, m_plain_window);
                discardClient ();
                xcb_destroy_window(connection, m_plain_window);
                xcb_flush(connection);
                m_plain_window = 0;
                //XSync (QX11Info::display (), false);
            }
        }
    }
}

KDE_NO_EXPORT void VideoOutput::embedded(WindowId handle) {
    kDebug () << "[01;35mwindowChanged[00m " << (int)clientWinId ();
    m_client_window = handle;
    if (clientWinId () && !resized_timer)
         resized_timer = startTimer (50);
    if (clientWinId())
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
        if (clientWinId ()) {
            xcb_connection_t* connection = QX11Info::connection();
#if QT_VERSION >= 0x050600
            uint32_t devicew = (uint32_t)(width() * devicePixelRatioF());
            uint32_t deviceh = (uint32_t)(height() * devicePixelRatioF());
#else
            uint32_t devicew = width();
            uint32_t deviceh = height();
#endif
            uint32_t values[] = { 0, 0, devicew, deviceh };
            xcb_configure_window(connection, clientWinId(),
                    XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y |
                    XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT,
                    values);
            xcb_flush(connection);
        }
    }
}

WindowId VideoOutput::windowHandle () {
    //return m_plain_window ? clientWinId () : winId ();
    return m_plain_window ? m_plain_window : winId ();
}

WindowId VideoOutput::ownHandle () {
    return winId ();
}

WindowId VideoOutput::clientHandle () {
    return clientWinId ();
}

void VideoOutput::setGeometry (const IRect &rect) {
#if QT_VERSION >= 0x050600
    int x = (int)(rect.x() / devicePixelRatioF());
    int y = (int)(rect.y() / devicePixelRatioF());
    int w = (int)(rect.width() / devicePixelRatioF());
    int h = (int)(rect.height() / devicePixelRatioF());
#else
    int x = rect.x (), y = rect.y (), w = rect.width (), h = rect.height ();
#endif
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
#if QT_VERSION >= 0x050600
    int x = (int)(r.x() * devicePixelRatioF());
    int y = (int)(r.y() * devicePixelRatioF());
    int w = (int)(r.width() * devicePixelRatioF());
    int h = (int)(r.height() * devicePixelRatioF());
#else
    int x = r.x();
    int y = r.y();
    int w = r.width();
    int h = r.height();
#endif
    m_view->viewArea()->scheduleRepaint(IRect(x, y, w, h));
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
        XCB_EVENT_MASK_EXPOSURE |
        //StructureNotifyMask |
        XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY;
    if (m & MonitorMouse)
        m_input_mask |= XCB_EVENT_MASK_POINTER_MOTION;
    if (m & MonitorKey)
        m_input_mask |= XCB_EVENT_MASK_KEY_PRESS;
    if (clientWinId ())
        setXSelectInput (clientWinId (), m_input_mask);
}

KDE_NO_EXPORT void VideoOutput::fullScreenChanged () {
    if (!(m_input_mask & XCB_EVENT_MASK_KEY_PRESS)) { // FIXME: store monitor when needed
        if (m_view->isFullScreen ())
            m_input_mask |= XCB_EVENT_MASK_POINTER_MOTION;
        else
            m_input_mask &= ~XCB_EVENT_MASK_POINTER_MOTION;
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
    if (clientWinId()) {
        xcb_connection_t* connection = QX11Info::connection();
        const uint32_t values[] = { c.rgb() };
        xcb_change_window_attributes(connection, clientWinId(), XCB_CW_BACK_PIXEL, values);
        xcb_flush(connection);
    }
}

#include "viewarea.moc"
