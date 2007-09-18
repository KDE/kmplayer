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

#ifndef KMPLAYER_VIEW_AREA_H
#define KMPLAYER_VIEW_AREA_H

#include <qwidget.h>

class KActionCollection;

namespace KMPlayer {

class View;
class ViewAreaPrivate;

/*
 * The area in which the video widget and controlpanel are laid out
 */
class KMPLAYER_EXPORT ViewArea : public QWidget {
    Q_OBJECT
public:
    ViewArea (QWidget * parent, View * view);
    ~ViewArea ();
    KDE_NO_EXPORT bool isFullScreen () const { return m_fullscreen; }
    KDE_NO_EXPORT bool isMinimalMode () const { return m_minimal; }
    KDE_NO_EXPORT KActionCollection * actionCollection () const { return m_collection; }
    KDE_NO_EXPORT QRect topWindowRect () const { return m_topwindow_rect; }
    SurfacePtr getSurface (NodePtr node);
    void setAudioVideoGeometry (int x, int y, int w, int h, unsigned int * bg);
    void setAudioVideoNode (NodePtr n);
    void mouseMoved ();
    void scheduleRepaint (int x, int y, int w, int y);
    void resizeEvent (QResizeEvent *);
    void minimalMode ();
public slots:
    void fullScreen ();
    void accelActivated ();
    void scale (int);
protected:
    void showEvent (QShowEvent *);
    void mouseMoveEvent (QMouseEvent *);
    void mousePressEvent (QMouseEvent *);
    void mouseDoubleClickEvent (QMouseEvent *);
    void dragEnterEvent (QDragEnterEvent *);
    void dropEvent (QDropEvent *);
    void contextMenuEvent (QContextMenuEvent * e);
    void paintEvent (QPaintEvent *);
    void timerEvent (QTimerEvent * e);
    void closeEvent (QCloseEvent * e);
private:
    void syncVisual (const IRect & rect);
    void updateSurfaceBounds ();
    ViewAreaPrivate * d;
    QWidget * m_parent;
    View * m_view;
    KActionCollection * m_collection;
    SurfacePtr surface;
    NodePtrW video_node;
    QRect m_av_geometry;
    IRect m_repaint_rect;
    QRect m_topwindow_rect;
    int m_mouse_invisible_timer;
    int m_repaint_timer;
    int m_fullscreen_scale;
    int scale_lbl_id;
    int scale_slider_id;
    bool m_fullscreen;
    bool m_minimal;
};

} // namespace KMPlayer

#endif
