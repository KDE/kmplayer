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

#ifndef _KMPLAYER_SURFACE_H_
#define _KMPLAYER_SURFACE_H_

#include <config-kmplayer.h>

#include "kmplayer_def.h"
#include "kmplayerplaylist.h"

#ifdef KMPLAYER_WITH_CAIRO
typedef struct _cairo_surface cairo_surface_t;
#endif

namespace KMPlayer {

class ViewArea;

class KMPLAYER_NO_EXPORT Surface : public TreeNode <Surface> {
public:
    Surface (ViewArea * widget);
    ~Surface();

    void clear ();
    Surface *createSurface (NodePtr owner, const SRect & rect);
    IRect toScreen (Single x, Single y, Single w, Single h);
    IRect clipToScreen (Single x, Single y, Single w, Single h);
    void resize (const SRect & rect, bool parent_resized=false);
    void repaint ();
    void repaint (const SRect &rect);
    void remove ();                // remove from parent, mark ancestors dirty
    void markDirty ();             // mark this and ancestors dirty
    void updateChildren (bool parent_resized=false);

    NodePtrW node;
    SRect bounds;                  // bounds in in parent coord.
    float xscale, yscale;          // internal scaling
    unsigned int background_color; // rgba background color
#ifdef KMPLAYER_WITH_CAIRO
    cairo_surface_t *surface;
#endif
    bool dirty;                    // a decendant is removed

private:
    NodePtrW current_video;
    ViewArea *view_widget;
};

typedef Item<Surface>::SharedType SurfacePtr;
typedef Item<Surface>::WeakType SurfacePtrW;
ITEM_AS_POINTER(KMPlayer::Surface)

} // namespace

#endif
