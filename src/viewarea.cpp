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

#include <config.h>

#include <qapplication.h>
#include <qwidgetstack.h>
#include <qslider.h>
#include <qcursor.h>
#include <qimage.h>
#include <qmap.h>

#include <kactioncollection.h>
#include <kstaticdeleter.h>
#include <kstatusbar.h>
#include <kstdaction.h>
#include <kshortcut.h>
#include <klocale.h>
#include <kdebug.h>

#include "kmplayerview.h"
#include "kmplayercontrolpanel.h"
#include "playlistview.h"
#include "viewarea.h"
#ifdef HAVE_CAIRO
# include <cairo-xlib.h>
# include <cairo-xlib-xrender.h>
#endif
#include "kmplayer_smil.h"
#include "kmplayer_rp.h"

using namespace KMPlayer;

extern const char * normal_window_xpm[];
extern const char * playlist_xpm[];

//-------------------------------------------------------------------------

namespace KMPlayer {
    typedef QMap <QString, ImageDataPtrW> ImageDataMap;
    static KStaticDeleter <ImageDataMap> imageCacheDeleter;
    static ImageDataMap * image_data_map;
}

ImageData::ImageData( const QString & img) :
#ifdef HAVE_CAIRO
    cairo_image (0L),
#endif
    image (0L),
    url (img) {
        if (img.isEmpty ())
            kdDebug() << "New ImageData for " << this << endl;
        else
            kdDebug() << "New ImageData for " << img << endl;
    }

ImageData::~ImageData() {
    if (url.isEmpty ())
        kdDebug() << "Delete ImageData for " << this << endl;
    else
        kdDebug() << "Delete ImageData for " << url << endl;
    if (!url.isEmpty ())
        image_data_map->erase (url);
#ifdef HAVE_CAIRO
    if (cairo_image)
        cairo_pattern_destroy (cairo_image);
#endif
    delete image;
}

#ifdef HAVE_CAIRO
cairo_pattern_t *
ImageData::cairoImage (Single sw, Single sh, cairo_surface_t * similar) {
    if (cairo_image) {
        if (sw == w && sh == h)
            return cairo_image;
        cairo_pattern_destroy (cairo_image);
        cairo_image = 0L;
    }
    if (!image || sw <= 0 || sh <= 0)
        return 0L;

    int iw = image->width ();
    int ih = image->height ();
    w = sw;
    h = sh;

    cairo_surface_t * sf = cairo_image_surface_create_for_data (
            image->bits (),
            image->hasAlphaBuffer () ? CAIRO_FORMAT_ARGB32 : CAIRO_FORMAT_RGB24,
            iw, ih, iw*4);
    cairo_pattern_t * cp = cairo_pattern_create_for_surface (sf);
    cairo_surface_destroy (sf);
    cairo_pattern_set_extend (cp, CAIRO_EXTEND_NONE);
    cairo_matrix_t mat;
    cairo_matrix_init_scale (&mat, 1.0 * iw/w, 1.0 * ih/h);
    cairo_pattern_set_matrix (cp, &mat);

    cairo_surface_t * img_surface = cairo_surface_create_similar (similar,
            image->hasAlphaBuffer () ?
                CAIRO_CONTENT_COLOR_ALPHA : CAIRO_CONTENT_COLOR, w, h);
    cairo_t * c = cairo_create (img_surface);
    cairo_set_source (c, cp);
    cairo_paint (c);
    cairo_pattern_destroy (cp);
    cairo_destroy (c);
    cairo_image = cairo_pattern_create_for_surface (img_surface);
    cairo_surface_destroy (img_surface);
    return cairo_image;
}

cairo_pattern_t * ImageData::cairoImage (cairo_surface_t * similar) {
    return cairoImage (width (), height (), similar);
}
#endif

bool ImageData::isEmpty () {
    return !(image
#ifdef HAVE_CAIRO
            || cairo_image
#endif
    );
}

Single ImageData::width () {
    if (image)
        return image->width ();
    return w;
}

Single ImageData::height () {
    if (image)
        return image->height ();
    return h;
}

void CachedImage::setUrl (const QString & url) {
    if (url.isEmpty ()) {
        data = ImageDataPtr (new ImageData (url));
    } else {
        ImageDataMap::iterator i = image_data_map->find (url);
        if (i == image_data_map->end ()) {
            data = ImageDataPtr (new ImageData (url));
            image_data_map->insert (url, ImageDataPtrW (data));
        } else {
            data = i.data ();
        }
    }
}

//-------------------------------------------------------------------------

namespace KMPlayer {

class KMPLAYER_NO_EXPORT ViewSurface : public Surface {
public:
    ViewSurface (ViewArea * widget);
    ViewSurface (ViewArea * widget, NodePtr owner, const SRect & rect);
    ~ViewSurface ();

    void clear () { m_first_child = 0L; }

    SurfacePtr createSurface (NodePtr owner, const SRect & rect);
    void toScreen (Single & x, Single & y, Single & w, Single & h);
    void resize (const SRect & rect);
    void repaint (Single x, Single y, Single w, Single h);
    void video (Single x, Single y, Single w, Single h);

    ViewArea * view_widget;
};

} // namespace

KDE_NO_CDTOR_EXPORT ViewSurface::ViewSurface (ViewArea * widget)
  : Surface (SRect (0, 0, widget->width (), widget->height ())),
    view_widget (widget)
{}

KDE_NO_CDTOR_EXPORT
ViewSurface::ViewSurface (ViewArea * widget, NodePtr owner, const SRect & rect)
  : Surface (owner, rect), view_widget (widget) {}

KDE_NO_CDTOR_EXPORT ViewSurface::~ViewSurface() {
    kdDebug() << "~ViewSurface" << endl;
}

KDE_NO_EXPORT
SurfacePtr ViewSurface::createSurface (NodePtr owner, const SRect & rect) {
    SurfacePtr surface = new ViewSurface (view_widget, owner, rect);
    appendChild (surface);
    return surface;
}

KDE_NO_EXPORT void ViewSurface::resize (const SRect & ) {
    /*if (rect == nrect)
        ;//return;
    SRect pr = rect.unite (nrect); // for repaint
    rect = nrect;*/
}

KDE_NO_EXPORT
void ViewSurface::toScreen (Single & x, Single & y, Single & w, Single & h) {
    Matrix matrix (xoffset, yoffset, xscale, yscale);
    matrix.translate (bounds.x (), bounds.y ());
    for (SurfacePtr s = parentNode(); s; s = s->parentNode()) {
        matrix.transform(Matrix (s->xoffset, s->yoffset, s->xscale, s->yscale));
        matrix.translate (s->bounds.x (), s->bounds.y ());
    }
    matrix.getXYWH (x, y, w, h);
}

KDE_NO_EXPORT
void ViewSurface::repaint (Single x, Single y, Single w, Single h) {
    toScreen (x, y, w, h);
    view_widget->scheduleRepaint (x, y, w, h);
    //kdDebug() << "Surface::repaint x:" << (int)x << " y:" << (int)y << " w:" << (int)w << " h:" << (int)h << endl;
}

KDE_NO_EXPORT void ViewSurface::video (Single x, Single y, Single w, Single h) {
    toScreen (x, y, w, h);
    kdDebug() << "Surface::video:" << background_color << " " << (background_color & 0xff000000) << endl;
    view_widget->setAudioVideoGeometry (x, y, w, h,
            (background_color & 0xff000000 ? &background_color : 0));
}

//-------------------------------------------------------------------------

#ifdef HAVE_CAIRO

//# define USE_CAIRO_GLITZ

static cairo_surface_t * cairoCreateX11Surface (Window id, int w, int h) {
    Display * display = qt_xdisplay ();
    return cairo_xlib_surface_create (display, id,
            DefaultVisual (display, DefaultScreen (display)), w, h);
    /*return cairo_xlib_surface_create_with_xrender_format (
            qt_xdisplay (),
            id,
            DefaultScreenOfDisplay (qt_xdisplay ()),
            XRenderFindVisualFormat (qt_xdisplay (),
                DefaultVisual (qt_xdisplay (),
                    DefaultScreen (qt_xdisplay ()))),
            w, h);*/
}
# ifndef USE_CAIRO_GLITZ
cairo_surface_t * (*cairoCreateSurface)(Window, int, int)=cairoCreateX11Surface;
# else

#  include <cairo-glitz.h>
#  include <glitz.h>
#  include <glitz-glx.h>

glitz_drawable_t * glitz_drawable = 0L; // FIXME add in ViewArea

static cairo_surface_t * cairoCreateSurface (Window id, int w, int h) {
    glitz_drawable_format_t  formatDrawableTemplate;
    formatDrawableTemplate.doublebuffer = 1;
    unsigned long mask = GLITZ_FORMAT_DOUBLEBUFFER_MASK;
    glitz_drawable_format_t* draw_fmt = NULL;
    glitz_drawable_format_t* fmt = 0L;
    XVisualInfo* pVisualInfo;
    VisualID vis_id = XVisualIDFromVisual (DefaultVisual (qt_xdisplay (),
                DefaultScreen (qt_xdisplay ())));
    int i = 0;
    do {
        fmt = glitz_glx_find_window_format (
                qt_xdisplay (),
                DefaultScreen (qt_xdisplay ()),
                mask,
                &formatDrawableTemplate,
                i++);
        if (fmt) {
            pVisualInfo = glitz_glx_get_visual_info_from_format (
                    qt_xdisplay (), DefaultScreen (qt_xdisplay()), fmt);
            kdDebug() << "depth = " << pVisualInfo->depth << " id:" << pVisualInfo->visualid << endl;
            if (pVisualInfo->visualid == vis_id) {
            //if (pVisualInfo->depth == 32) {
                  //DefaultDepth(qt_xdisplay(), DefaultScreen(qt_xdisplay()))) {
                draw_fmt = fmt;
                break;
            } else if (!draw_fmt)
                draw_fmt = fmt;
        }
    } while (fmt);
    if (!draw_fmt)
        return cairoCreateX11Surface (id, w, h);

    glitz_drawable_t * drawable = glitz_glx_create_drawable_for_window (
            qt_xdisplay (), DefaultScreen (qt_xdisplay()), draw_fmt, id, w, h);
    if (!drawable) {
        kdWarning() << "failed to create glitz drawable on screen " << DefaultScreen (qt_xdisplay()) << endl;
        return cairoCreateX11Surface (id, w, h);
    }

    glitz_format_t formatTemplate;
    formatTemplate.color = draw_fmt->color;
    formatTemplate.color.fourcc = GLITZ_FOURCC_RGB;

    glitz_format_t* format = glitz_find_format (drawable,
            GLITZ_FORMAT_RED_SIZE_MASK   |
            GLITZ_FORMAT_GREEN_SIZE_MASK |
            GLITZ_FORMAT_BLUE_SIZE_MASK  |
            GLITZ_FORMAT_ALPHA_SIZE_MASK |
            GLITZ_FORMAT_FOURCC_MASK,
            &formatTemplate,
            0);
    //glitz_format_t* format = glitz_find_standard_format (
    //        drawable, GLITZ_STANDARD_ARGB32);
    glitz_surface_t * pGlitzSurface = glitz_surface_create (
            drawable, format, w, h, 0, NULL);
    if (draw_fmt->doublebuffer)
        glitz_surface_attach (pGlitzSurface,
                drawable,
                GLITZ_DRAWABLE_BUFFER_BACK_COLOR);
    else
        glitz_surface_attach (pGlitzSurface,
                drawable,
                GLITZ_DRAWABLE_BUFFER_FRONT_COLOR);
    cairo_surface_t* pCairoSurface = cairo_glitz_surface_create (pGlitzSurface);
    glitz_surface_destroy (pGlitzSurface);
    glitz_drawable = drawable;
    return pCairoSurface;
}
# endif

# define CAIRO_SET_SOURCE_RGB(cr,c)           \
    cairo_set_source_rgb ((cr),               \
            1.0 * (((c) >> 16) & 0xff) / 255, \
            1.0 * (((c) >> 8) & 0xff) / 255,  \
            1.0 * (((c)) & 0xff) / 255)

class KMPLAYER_NO_EXPORT CairoPaintVisitor : public Visitor {
    const SRect clip;
    cairo_surface_t * cairo_surface;
    Matrix matrix;
    void traverseRegion (SMIL::RegionBase * reg);
public:
    cairo_t * cr;
    CairoPaintVisitor (cairo_surface_t * cs, const SRect & rect);
    ~CairoPaintVisitor ();
    using Visitor::visit;
    void visit (Node * n);
    void visit (SMIL::Layout *);
    void visit (SMIL::Region *);
    void visit (SMIL::ImageMediaType *);
    void visit (SMIL::TextMediaType *);
    void visit (SMIL::Brush *);
    KDE_NO_EXPORT void visit (SMIL::RefMediaType *) {}
    KDE_NO_EXPORT void visit (SMIL::AVMediaType *) {}
    void visit (RP::Imfl *);
    void visit (RP::Fill *);
    void visit (RP::Fadein *);
    void visit (RP::Fadeout *);
    void visit (RP::Crossfade *);
    void visit (RP::Wipe *);
    void visit (RP::ViewChange *);
};

KDE_NO_CDTOR_EXPORT
CairoPaintVisitor::CairoPaintVisitor (cairo_surface_t * cs, const SRect & rect)
 : clip (rect), cairo_surface (cs) {
    cr = cairo_create (cs);
    cairo_rectangle (cr, rect.x(), rect.y(), rect.width(), rect.height());
    cairo_clip_preserve (cr);
    cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
    cairo_set_tolerance (cr, 0.5 );
# ifndef USE_CAIRO_GLITZ
    cairo_push_group (cr);
#endif
    cairo_set_source_rgb (cr, 0, 0, 0);
    cairo_fill (cr);
}

KDE_NO_CDTOR_EXPORT CairoPaintVisitor::~CairoPaintVisitor () {
# ifndef USE_CAIRO_GLITZ
    cairo_pattern_t * pat = cairo_pop_group (cr);
    //cairo_pattern_set_extend (pat, CAIRO_EXTEND_NONE);
    cairo_set_source (cr, pat);
    cairo_rectangle (cr, clip.x(), clip.y(), clip.width(), clip.height());
    cairo_fill (cr);
    cairo_pattern_destroy (pat);
# else
    cairo_surface_flush (cairo_surface);
    if (glitz_drawable)
        glitz_drawable_swap_buffers (glitz_drawable);
#endif
    cairo_destroy (cr);
}

KDE_NO_EXPORT void CairoPaintVisitor::visit (Node * n) {
    kdWarning() << "Paint called on " << n->nodeName() << endl;
}

KDE_NO_EXPORT void CairoPaintVisitor::traverseRegion (SMIL::RegionBase * reg) {
    // next visit listeners
    NodeRefListPtr nl = reg->listeners (event_paint);
    if (nl) {
        for (NodeRefItemPtr c = nl->first(); c; c = c->nextSibling ())
            if (c->data)
                c->data->accept (this);
    }
    if (reg->surface && reg->surface->node && reg->surface->node.ptr () != reg)
        reg->surface->node->accept (this);

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
    //kdDebug() << "Visit " << reg->nodeName() << endl;
    SMIL::RegionBase *rb = convertNode <SMIL::RegionBase> (reg->rootLayout);
    if (reg->surface && rb) {
        //cairo_save (cr);
        Matrix m = matrix;

        SRect rect = reg->surface->bounds;
        Single xoff = reg->surface->xoffset;
        Single yoff = reg->surface->yoffset;
        Single w = rect.width() - 2 * xoff, h = rect.height() - 2 * yoff;
        matrix.getXYWH (xoff, yoff, w, h);

        rb->surface = reg->surface;
        rb->surface->background_color = rb->background_color;

        if (reg->surface && (reg->surface->background_color & 0xff000000)) {
            CAIRO_SET_SOURCE_RGB (cr, reg->surface->background_color);
            SRect clip_rect = clip.intersect (SRect (xoff, yoff, w, h));
            cairo_rectangle (cr, clip_rect.x (), clip_rect.y(),
                    clip_rect.width (), clip_rect.height ());
            cairo_fill (cr);
        }
        //cairo_rectangle (cr, xoff, yoff, w, h);
        //cairo_clip (cr);

        matrix = Matrix (reg->surface->xoffset, reg->surface->yoffset,
                reg->surface->xscale, reg->surface->yscale);
        matrix.transform (m);
        traverseRegion (reg);
        //cairo_restore (cr);
        matrix = m;

        rb->surface = 0L;
    }
}

KDE_NO_EXPORT void CairoPaintVisitor::visit (SMIL::Region * reg) {
    SRect rect = reg->surface->bounds;

    Matrix m = matrix;
    Single x = rect.x (), y = rect.y (), w = rect.width(), h = rect.height();
    matrix.getXYWH (x, y, w, h);
    if (!clip.intersect (SRect (x, y, w, h)).isValid ())
        return;
    matrix = Matrix (rect.x(), rect.y(), 1.0, 1.0);
    matrix.transform (m);
    cairo_save (cr);
    if (reg->surface && (reg->surface->background_color & 0xff000000)) {
        CAIRO_SET_SOURCE_RGB (cr, reg->surface->background_color);
        SRect clip_rect = clip.intersect (SRect (x, y, w, h));
        cairo_rectangle (cr, clip_rect.x (), clip_rect.y(),
                clip_rect.width (), clip_rect.height ());
        //cairo_rectangle (cr, x, y, w, h);
        cairo_fill (cr);
    }
    //cairo_rectangle (cr, x, y, w, h);
    //cairo_clip (cr);
    traverseRegion (reg);
    cairo_restore (cr);
    matrix = m;
}

KDE_NO_EXPORT void CairoPaintVisitor::visit (SMIL::ImageMediaType * img) {
    //kdDebug() << "Visit " << img->nodeName() << endl;
    ImageRuntime * ir = static_cast <ImageRuntime *> (img->timedRuntime ());
    SMIL::RegionBase * rb = convertNode <SMIL::RegionBase> (img->region_node);
    ImageData * id = ir->cached_img.data.ptr ();
    if (rb && rb->surface && id && !id->isEmpty () &&
            (ir->timingstate == TimedRuntime::timings_started ||
             (ir->timingstate == TimedRuntime::timings_stopped &&
              ir->fill == TimedRuntime::fill_freeze))) {
        SRect rect = rb->surface->bounds;
        Single x, y, w = rect.width(), h = rect.height();
        if (id && !id->isEmpty () && id->width() > 0 && id->height() > 0 &&
                (int)w > 0 && (int)h > 0) {
            float xs = 1.0, ys = 1.0;
            if (ir->fit == fit_meet) {
                float pasp = 1.0 * id->width() / id->height();
                float rasp = 1.0 * w / h;
                if (pasp > rasp)
                    xs = ys = 1.0 * w / id->width();
                else
                    xs = ys = 1.0 * h / id->height();
            } else if (ir->fit == fit_fill) {
                xs = 1.0 * w / id->width();
                ys = 1.0 * h / id->height();
            } else if (ir->fit == fit_slice) {
                float pasp = 1.0 * id->width() / id->height();
                float rasp = 1.0 * w / h;
                if (pasp > rasp)
                    xs = ys = 1.0 * h / id->height();
                else
                    xs = ys = 1.0 * w / id->width();
            } // else fit_hidden
            matrix.getXYWH (x, y, w, h);
            SRect reg_rect (x, y, w, h);
            x = y = 0;
            w = xs * id->width();
            h = ys * id->height();
            ir->sizes.applyRegPoints(img, rect.width(), rect.height(), x,y,w,h);
            matrix.getXYWH (x, y, w, h);
            SRect clip_rect = reg_rect.intersect (SRect (x, y, w, h));
            cairo_pattern_t * pat = id->cairoImage (w, h, cairo_surface);
            if (pat) {
                cairo_matrix_t mat;
                cairo_matrix_init_identity (&mat);
                cairo_matrix_translate (&mat, -x, -y);
                cairo_pattern_set_matrix (pat, &mat);
                cairo_pattern_set_filter (pat, CAIRO_FILTER_FAST);
                cairo_set_source (cr, pat);
                cairo_rectangle (cr, clip_rect.x (), clip_rect.y (),
                        clip_rect.width (), clip_rect.height ());
                cairo_fill (cr);
            }
        }
    }
}

KDE_NO_EXPORT void CairoPaintVisitor::visit (SMIL::TextMediaType * txt) {
    TextRuntime * td = static_cast <TextRuntime *> (txt->timedRuntime ());
    //kdDebug() << "Visit " << txt->nodeName() << " " << td->font_size << endl;
    SMIL::RegionBase * rb = convertNode <SMIL::RegionBase> (txt->region_node);
    if (rb && rb->surface) {
        SRect rect = rb->surface->bounds;
        Single x, y, w = rect.width(), h = rect.height();
        td->sizes.applyRegPoints (txt, rect.width(), rect.height(), x, y, w, h);
        matrix.getXYWH (x, y, w, h);
        if (!td->transparent) {
            CAIRO_SET_SOURCE_RGB (cr, td->background_color);
            cairo_rectangle (cr, x, y, w, h);
            cairo_fill (cr);
        }
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

        CAIRO_SET_SOURCE_RGB (cr, td->font_color);
        float scale = 1.0 * w / rect.width (); // TODO: make an image
        cairo_set_font_size (cr, scale * td->font_size);
        int margin = (int) scale * (1 + (td->font_size >> 2));
        cairo_move_to (cr, x + margin, y + margin + (int)scale * td->font_size);
        cairo_show_text (cr, td->text.utf8 ().data ());
        //cairo_stroke (cr);
    }
}

KDE_NO_EXPORT void CairoPaintVisitor::visit (SMIL::Brush * brush) {
    //kdDebug() << "Visit " << brush->nodeName() << endl;
    SMIL::RegionBase * rb = convertNode <SMIL::RegionBase> (brush->region_node);
    if (rb && rb->surface) {
        SRect rect = rb->surface->bounds;
        Single x, y, w = rect.width(), h = rect.height();
        matrix.getXYWH (x, y, w, h);
        unsigned int color = QColor (brush->param ("color")).rgb ();
        CAIRO_SET_SOURCE_RGB (cr, color);
        cairo_rectangle (cr, x, y, w, h);
        cairo_fill (cr);
    }
}

KDE_NO_EXPORT void CairoPaintVisitor::visit (RP::Imfl * imfl) {
    if (imfl->surface) {
        cairo_save (cr);
        Matrix m = matrix;
        Single xoff = imfl->surface->xoffset, yoff = imfl->surface->yoffset;
        Single w = imfl->surface->bounds.width() - 2 * xoff;
        Single h = imfl->surface->bounds.height() - 2 * yoff;
        matrix.getXYWH (xoff, yoff, w, h);
        cairo_rectangle (cr, xoff, yoff, w, h);
        //cairo_clip (cr);
        cairo_translate (cr, xoff, yoff);
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
    //kdDebug() << "Visit " << fi->nodeName() << endl;
    CAIRO_SET_SOURCE_RGB (cr, fi->color);
    if ((int)fi->w && (int)fi->h) {
        cairo_rectangle (cr, fi->x, fi->y, fi->w, fi->h);
        cairo_fill (cr);
    }
}

KDE_NO_EXPORT void CairoPaintVisitor::visit (RP::Fadein * fi) {
    //kdDebug() << "Visit " << fi->nodeName() << endl;
    if (fi->target && fi->target->id == RP::id_node_image) {
        ImageData *id=convertNode<RP::Image>(fi->target)->cached_img.data.ptr();
        if (id && !id->isEmpty ()) {
            Single sx = fi->srcx, sy = fi->srcy, sw = fi->srcw, sh = fi->srch;
            if (!(int)sw)
                sw = id->width();
            if (!(int)sh)
                sh = id->height();
            if ((int)fi->w && (int)fi->h && (int)sw && (int)sh) {
                cairo_pattern_t * pat = id->cairoImage (cairo_surface);
                if (pat) {
                    cairo_pattern_set_extend (pat, CAIRO_EXTEND_NONE);
                    cairo_matrix_t matrix;
                    cairo_matrix_init_identity (&matrix);
                    float scalex = 1.0 * sw / fi->w;
                    float scaley = 1.0 * sh / fi->h;
                    cairo_matrix_scale (&matrix, scalex, scaley);
                    cairo_matrix_translate (&matrix,
                            1.0*sx/scalex - (double)fi->x,
                            1.0*sy/scaley - (double)fi->y);
                    cairo_pattern_set_matrix (pat, &matrix);
                    cairo_save (cr);
                    cairo_rectangle (cr, fi->x, fi->y, fi->w, fi->h);
                    cairo_set_source (cr, pat);
                    cairo_clip (cr);
                    cairo_paint_with_alpha (cr, 1.0 * fi->progress / 100);
                    cairo_restore (cr);
                }
            }
        }
    }
}

KDE_NO_EXPORT void CairoPaintVisitor::visit (RP::Fadeout * fo) {
    //kdDebug() << "Visit " << fo->nodeName() << endl;
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
    //kdDebug() << "Visit " << cf->nodeName() << endl;
    if (cf->target && cf->target->id == RP::id_node_image) {
        ImageData *id=convertNode<RP::Image>(cf->target)->cached_img.data.ptr();
        if (id && !id->isEmpty ()) {
            Single sx = cf->srcx, sy = cf->srcy, sw = cf->srcw, sh = cf->srch;
            if (!(int)sw)
                sw = id->width();
            if (!(int)sh)
                sh = id->height();
            if ((int)cf->w && (int)cf->h && (int)sw && (int)sh) {
                cairo_pattern_t * pat = id->cairoImage (cairo_surface);
                if (pat) {
                    cairo_pattern_set_extend (pat, CAIRO_EXTEND_NONE);
                    cairo_save (cr);
                    cairo_matrix_t matrix;
                    cairo_matrix_init_identity (&matrix);
                    float scalex = 1.0 * sw / cf->w;
                    float scaley = 1.0 * sh / cf->h;
                    cairo_matrix_scale (&matrix, scalex, scaley);
                    cairo_matrix_translate (&matrix,
                            1.0*sx/scalex - (double)cf->x,
                            1.0*sy/scaley - (double)cf->y);
                    cairo_pattern_set_matrix (pat, &matrix);
                    cairo_rectangle (cr, cf->x, cf->y, cf->w, cf->h);
                    cairo_set_source (cr, pat);
                    cairo_clip (cr);
                    cairo_paint_with_alpha (cr, 1.0 * cf->progress / 100);
                    cairo_restore (cr);
                }
            }
        }
    }
}

KDE_NO_EXPORT void CairoPaintVisitor::visit (RP::Wipe * wipe) {
    //kdDebug() << "Visit " << wipe->nodeName() << endl;
    if (wipe->target && wipe->target->id == RP::id_node_image) {
        ImageData *id=convertNode<RP::Image>(wipe->target)->cached_img.data.ptr();
        if (id && !id->isEmpty ()) {
            Single x = wipe->x, y = wipe->y;
            Single tx = x, ty = y;
            Single w = wipe->w, h = wipe->h;
            Single sx = wipe->srcx, sy = wipe->srcy, sw = wipe->srcw, sh = wipe->srch;
            if (!(int)sw)
                sw = id->width();
            if (!(int)sh)
                sh = id->height();
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
                cairo_pattern_t * pat = id->cairoImage (cairo_surface);
                if (pat) {
                    cairo_pattern_set_extend (pat, CAIRO_EXTEND_NONE);
                    cairo_matrix_t matrix;
                    cairo_matrix_init_identity (&matrix);
                    float scalex = 1.0 * sw / wipe->w;
                    float scaley = 1.0 * sh / wipe->h;
                    cairo_matrix_scale (&matrix, scalex, scaley);
                    cairo_matrix_translate (&matrix,
                            1.0*sx/scalex - (double)tx,
                            1.0*sy/scaley - (double)ty);
                    cairo_pattern_set_matrix (pat, &matrix);
                    cairo_set_source (cr, pat);
                    cairo_rectangle (cr, x, y, w, h);
                    cairo_fill (cr);
                }
            }
        }
    }
}

KDE_NO_EXPORT void CairoPaintVisitor::visit (RP::ViewChange * vc) {
    //kdDebug() << "Visit " << vc->nodeName() << endl;
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

};

} // namespace

KDE_NO_CDTOR_EXPORT
MouseVisitor::MouseVisitor (unsigned int evt, int a, int b)
    : event (evt), x (a), y (b), handled (false) {
}

KDE_NO_EXPORT void MouseVisitor::visit (Node * n) {
    kdDebug () << "Mouse event ignored for " << n->nodeName () << endl;
}

KDE_NO_EXPORT void MouseVisitor::visit (SMIL::Layout * layout) {
    Matrix m = matrix;
    matrix = Matrix (layout->surface->xoffset, layout->surface->yoffset,
            layout->surface->xscale, layout->surface->yscale);
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

KDE_NO_EXPORT void MouseVisitor::visit (SMIL::Region * region) {
    SRect rect = region->surface->bounds;
    Single rx = rect.x (), ry = rect.y(), rw = rect.width(), rh = rect.height();
    matrix.getXYWH (rx, ry, rw, rh);
    handled = false;
    bool inside = x > rx && x < rx+rw && y > ry && y< ry+rh;
    if (!inside && event == event_pointer_clicked)
        return;
    Matrix m = matrix;
    matrix = Matrix (rect.x(), rect.y(), 1.0, 1.0);
    matrix.transform (m);

    bool child_handled = false;
    if (inside)
        for (NodePtr r = region->firstChild (); r; r = r->nextSibling ()) {
            r->accept (this);
            child_handled |= handled;
            if (!node->active ())
                break;
        }
    int saved_event = event;
    if (node->active ()) {
        if (event == event_pointer_moved) {
            if (region->has_mouse && (!inside || child_handled)) {
                region->has_mouse = false;
                event = event_outbounds;
                child_handled = false;
            } else if (inside && !child_handled && !region->has_mouse) {
                region->has_mouse = true;
                event = event_inbounds;
                child_handled = false;
            }
        }
        if (!child_handled) {
            NodeRefListPtr nl = region->listeners (event);
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

KDE_NO_EXPORT void MouseVisitor::visit (SMIL::Anchor * anchor) {
    kdDebug() << "anchor to " << anchor->href << " clicked" << endl;
    NodePtr n = anchor;
    for (NodePtr p = anchor->parentNode (); p; p = p->parentNode ()) {
        if (n->mrl () && n->mrl ()->opener == p) {
            p->setState (Node::state_deferred);
            p->mrl ()->setParam (QString ("src"), anchor->href, 0L);
            p->activate ();
            break;
        }
        n = p;
    }
}

KDE_NO_EXPORT void MouseVisitor::visit (SMIL::TimedMrl * timedmrl) {
    timedmrl->timedRuntime ()->processEvent (event);
}

KDE_NO_EXPORT void MouseVisitor::visit (SMIL::MediaType * mediatype) {
    NodeRefListPtr nl = mediatype->listeners (event);
    if (nl)
        for (NodeRefItemPtr c = nl->first(); c; c = c->nextSibling ()) {
            if (c->data)
                c->data->accept (this);
            if (!node->active ())
                return;
        }
    visit (static_cast <SMIL::TimedMrl *> (mediatype));

    int save_event = event;
    if (event == event_inbounds || event == event_outbounds)
        event = event_pointer_moved;
    SMIL::RegionBase *r = convertNode<SMIL::RegionBase>(mediatype->region_node);
    if (r && r->surface && r->surface->node && r != r->surface->node)
        return r->surface->node->accept (this);
    event = save_event;
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT ViewArea::ViewArea (QWidget * parent, View * view)
 : QWidget (parent, "kde_kmplayer_viewarea", WResizeNoErase | WRepaintNoErase),
   m_parent (parent),
   m_view (view),
#ifdef HAVE_CAIRO
    cairo_surface (0L),
#endif
   m_collection (new KActionCollection (this)),
   surface (new ViewSurface (this)),
   m_mouse_invisible_timer (0),
   m_repaint_timer (0),
   m_fullscreen_scale (100),
   scale_lbl_id (-1),
   scale_slider_id (-1),
   m_fullscreen (false),
   m_minimal (false) {
    setEraseColor (QColor (0, 0, 0));
    setAcceptDrops (true);
    new KAction (i18n ("Fullscreen"), KShortcut (Qt::Key_F), this, SLOT (accelActivated ()), m_collection, "view_fullscreen_toggle");
    setMouseTracking (true);
    if (!image_data_map)
        imageCacheDeleter.setObject (image_data_map, new ImageDataMap);
}

KDE_NO_CDTOR_EXPORT ViewArea::~ViewArea () {
#ifdef HAVE_CAIRO
    if (cairo_surface)
        cairo_surface_destroy (cairo_surface);
#endif
}

KDE_NO_EXPORT void ViewArea::fullScreen () {
    killTimers ();
    m_mouse_invisible_timer = m_repaint_timer = 0;
    if (m_fullscreen) {
        showNormal ();
        reparent (m_parent, 0, QPoint (0, 0), true);
        static_cast <KDockWidget *> (m_parent)->setWidget (this);
        for (unsigned i = 0; i < m_collection->count (); ++i)
            m_collection->action (i)->setEnabled (false);
        if (scale_lbl_id != -1) {
            m_view->controlPanel ()->popupMenu ()->removeItem (scale_lbl_id);
            m_view->controlPanel ()->popupMenu ()->removeItem (scale_slider_id);
            scale_lbl_id = scale_slider_id = -1;
        }
        m_view->controlPanel ()->button (ControlPanel::button_playlist)->setIconSet (QIconSet (QPixmap (playlist_xpm)));
    } else {
        m_topwindow_rect = topLevelWidget ()->geometry ();
        reparent (0L, 0, qApp->desktop()->screenGeometry(this).topLeft(), true);
        showFullScreen ();
        for (unsigned i = 0; i < m_collection->count (); ++i)
            m_collection->action (i)->setEnabled (true);
        QPopupMenu * menu = m_view->controlPanel ()->popupMenu ();
        QLabel * lbl = new QLabel (i18n ("Scale:"), menu);
        scale_lbl_id = menu->insertItem (lbl, -1, 4);
        QSlider * slider = new QSlider (50, 150, 10, m_fullscreen_scale, Qt::Horizontal, menu);
        connect (slider, SIGNAL (valueChanged (int)), this, SLOT (scale (int)));
        scale_slider_id = menu->insertItem (slider, -1, 5);
        m_view->controlPanel ()->button (ControlPanel::button_playlist)->setIconSet (QIconSet (QPixmap (normal_window_xpm)));
    }
    m_fullscreen = !m_fullscreen;
    m_view->controlPanel()->popupMenu ()->setItemChecked (ControlPanel::menu_fullscreen, m_fullscreen);

#ifdef HAVE_CAIRO
    if (cairo_surface) {
        cairo_surface_destroy (cairo_surface);
        cairo_surface = 0L;
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
    killTimers ();
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
    m_view->controlPanel()->popupMenu ()->activateItemAt (m_view->controlPanel()->popupMenu ()->indexOf (ControlPanel::menu_fullscreen)); 
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
    }
    e->accept ();
    mouseMoved (); // for m_mouse_invisible_timer
}

KDE_NO_EXPORT void ViewArea::syncVisual (const SRect & rect) {
    if (!surface->node) {
        repaint (QRect(rect.x(), rect.y(), rect.width(), rect.height()), false);
        return;
    }
#ifdef HAVE_CAIRO
    int ex = rect.x ();
    if (ex > 0)
        ex--;
    int ey = rect.y ();
    if (ey > 0)
        ey--;
    int ew = rect.width () + 2;
    int eh = rect.height () + 2;
    if (!cairo_surface)
        cairo_surface = cairoCreateSurface (winId (), width (), height ());
    CairoPaintVisitor visitor (cairo_surface, SRect (ex, ey, ew, eh));
    surface->node->accept (&visitor);
#endif
    //XFlush (qt_xdisplay ());
}

KDE_NO_EXPORT void ViewArea::paintEvent (QPaintEvent * pe) {
#ifdef HAVE_CAIRO
    if (surface->node)
        scheduleRepaint (pe->rect ().x (), pe->rect ().y (), pe->rect ().width (), pe->rect ().height ());
    else
#endif
        QWidget::paintEvent (pe);
}

KDE_NO_EXPORT void ViewArea::scale (int val) {
    m_fullscreen_scale = val;
    resizeEvent (0L);
}

KDE_NO_EXPORT void ViewArea::resizeEvent (QResizeEvent *) {
    if (!m_view->controlPanel ()) return;
    int x =0, y = 0;
    int w = width ();
    int h = height ();
#ifdef HAVE_CAIRO
    if (cairo_surface)
        cairo_xlib_surface_set_size (cairo_surface, width (), height ());
# ifdef USE_CAIRO_GLITZ
    if (glitz_drawable)
        glitz_drawable_update_size (glitz_drawable, width (), height ());
# endif
#endif
    int hsb = m_view->statusBarHeight ();
    int hcp = m_view->controlPanel ()->isVisible () ? (m_view->controlPanelMode () == View::CP_Only ? h-hsb: m_view->controlPanel()->maximumSize ().height ()) : 0;
    int wws = w;
    // move controlpanel over video when autohiding and playing
    int hws = h - (m_view->controlPanelMode () == View::CP_AutoHide && m_view->widgetStack ()->visibleWidget () == m_view->viewer () ? 0 : hcp) - hsb;
    // now scale the regions and check if video region is already sized
    bool av_geometry_changed = false;
    surface->bounds = SRect (x, y, wws, hws);
    if (surface->node && wws > 0 && hws > 0) {
        m_av_geometry = QRect (0, 0, 0, 0);
        surface->node->handleEvent (new SizeEvent (x, y, wws, hws, m_view->keepSizeRatio () ? fit_meet : fit_fill));
        av_geometry_changed = (m_av_geometry != QRect (0, 0, 0, 0));
        x = m_av_geometry.x ();
        y = m_av_geometry.y ();
        wws = m_av_geometry.width ();
        hws = m_av_geometry.height ();
        scheduleRepaint (0, 0, wws, hws);
            //m_view->viewer ()->setAspect (region->w / region->h);
    } else
        m_av_geometry = QRect (x, y, wws, hws);

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
    if (!av_geometry_changed)
        setAudioVideoGeometry (x, y, wws, hws, 0L);
}

KDE_NO_EXPORT
void ViewArea::setAudioVideoGeometry (int x, int y, int w, int h, unsigned int * bg_color) {
    if (m_view->controlPanelMode() == View::CP_Only) {
        w = h = 0;
    } else if (m_view->keepSizeRatio ()) { // scale video widget inside region
        int hfw = m_view->viewer ()->heightForWidth (w);
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
    m_av_geometry = QRect (x, y, w, h);
    QRect rect = m_view->widgetStack ()->geometry ();
    if (m_av_geometry != rect) {
        m_view->widgetStack ()->setGeometry (x, y, w, h);
        rect.unite (m_av_geometry);
        scheduleRepaint (rect.x (), rect.y (), rect.width (), rect.height ());
    }
    if (bg_color)
        if (QColor (QRgb (*bg_color)) != (m_view->viewer ()->paletteBackgroundColor ())) {
            m_view->viewer()->setCurrentBackgroundColor (QColor (QRgb (*bg_color)));
            scheduleRepaint (x, y, w, h);
        }
}

KDE_NO_EXPORT SurfacePtr ViewArea::getSurface (NodePtr node) {
    surface->node = node;
    qApp->postEvent (this, new QResizeEvent (size (), QSize (0, 0)));
    if (m_repaint_timer) {
        killTimer (m_repaint_timer);
        m_repaint_timer = 0;
    }
    m_view->viewer()->resetBackgroundColor ();
    if (node)
        return surface;
    static_cast <ViewSurface *> (surface.ptr ())->clear ();
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
    m_view->controlPanel ()->popupMenu ()->exec (e->globalPos ());
}

KDE_NO_EXPORT void ViewArea::mouseMoved () {
    if (m_fullscreen) {
        if (m_mouse_invisible_timer)
            killTimer (m_mouse_invisible_timer);
        unsetCursor ();
        m_mouse_invisible_timer = startTimer (MOUSE_INVISIBLE_DELAY);
    }
}

KDE_NO_EXPORT void ViewArea::scheduleRepaint (Single x, Single y, Single w, Single h) {
    if (m_repaint_timer)
        m_repaint_rect = m_repaint_rect.unite (SRect (x, y, w, h));
    else {
        m_repaint_rect = SRect (x, y, w, h);
        m_repaint_timer = startTimer (10); // 100 per sec should do
    }
}

KDE_NO_EXPORT
void ViewArea::moveRect (Single x, Single y, Single w, Single h, Single x1, Single y1) {
    SRect r (x, y, w, h);
    if (m_repaint_timer && m_repaint_rect.intersect (r).isValid ()) {
        m_repaint_rect = m_repaint_rect.unite (SRect (x1, y1, w, h).unite (r));
    } else if (m_view->viewer()->frameGeometry ().intersects (QRect(x,y,w,h))) {
        SRect r2 (SRect (x1, y1, w, h).unite (r));
        scheduleRepaint (r.x (), r.y (), r.width (), r.height ());
    } else {
        bitBlt (this, x1, y1, this, x, y, w, h);
        if (x1 > x)
            syncVisual (SRect (x, y, x1 - x, h));
        else if (x > x1)
            syncVisual (SRect (x1 + w, y, x - x1, h));
        if (y1 > y)
            syncVisual (SRect (x, y, w, y1 - y));
        else if (y > y1)
            syncVisual (SRect (x, y1 + h, w, y - y1));
    }
}

KDE_NO_EXPORT void ViewArea::timerEvent (QTimerEvent * e) {
    if (e->timerId () == m_mouse_invisible_timer) {
        killTimer (m_mouse_invisible_timer);
        m_mouse_invisible_timer = 0;
        if (m_fullscreen)
            setCursor (BlankCursor);
    } else if (e->timerId () == m_repaint_timer) {
        killTimer (m_repaint_timer);
        m_repaint_timer = 0;
        //repaint (m_repaint_rect, false);
        syncVisual (m_repaint_rect);
    } else {
        kdError () << "unknown timer " << e->timerId () << " " << m_repaint_timer << endl;
        killTimer (e->timerId ());
    }
}

KDE_NO_EXPORT void ViewArea::closeEvent (QCloseEvent * e) {
    //kdDebug () << "closeEvent" << endl;
    if (m_fullscreen) {
        fullScreen ();
        if (!m_parent->topLevelWidget ()->isVisible ())
            m_parent->topLevelWidget ()->show ();
        e->ignore ();
    } else
        QWidget::closeEvent (e);
}


#include "viewarea.moc"
