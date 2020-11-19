/*
    This file belong to the KMPlayer project, a movie player plugin for Konqueror
    SPDX-FileCopyrightText: 2008 Koos Vriezen <koos.vriezen@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef _KMPLAYER_SURFACE_H_
#define _KMPLAYER_SURFACE_H_

#include <config-kmplayer.h>

#include "kmplayerplaylist.h"

#ifdef KMPLAYER_WITH_CAIRO
typedef struct _cairo_surface cairo_surface_t;
#endif

namespace KMPlayer {

class ViewArea;

class Surface : public TreeNode <Surface>
{
public:
    Surface (ViewArea *widget);
    ~Surface();

    void clear ();
    Surface *createSurface (NodePtr owner, const SRect & rect);
    IRect toScreen (const SSize &size);
    void resize (const SRect & rect, bool parent_resized=false);
    void repaint ();
    void repaint (const SRect &rect);
    void remove ();                // remove from parent, mark ancestors dirty
    void markDirty ();             // mark this and ancestors dirty
    void updateChildren (bool parent_resized=false);
    void setBackgroundColor (unsigned int argb);

    NodePtrW node;
    SRect bounds;                  // bounds in parent coord.
    SSize virtual_size;            // virtual size in screen coord.
    float xscale, yscale;          // internal scaling
    unsigned int background_color; // rgba background color
    unsigned short x_scroll;       // top of horizontal knob
    unsigned short y_scroll;       // top of vertical knob
#ifdef KMPLAYER_WITH_CAIRO
    cairo_surface_t *surface;
#endif
    bool dirty;                    // a decendant is removed
    bool scroll;
    bool has_mouse;

private:
    NodePtrW current_video;
    ViewArea *view_widget;
};

typedef Item<Surface>::SharedType SurfacePtr;
typedef Item<Surface>::WeakType SurfacePtrW;
ITEM_AS_POINTER(KMPlayer::Surface)

template <> void TreeNode<Surface>::appendChild (Surface *c);
template <> void TreeNode<Surface>::insertBefore (Surface *c, Surface *b);
template <> void TreeNode<Surface>::removeChild (SurfacePtr c);

} // namespace

#endif
