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
#include <QAbstractNativeEventFilter>
typedef QWidget QX11EmbedContainer;
#include <QList>

#include "mediaobject.h"
#include "surface.h"

class QPaintEngine;
class KActionCollection;

namespace KMPlayer {

class View;
class IViewer;
class ViewerAreaPrivate;
class VideoOutput;

/*
 * The area in which the video widget and controlpanel are laid out
 */
class KMPLAYER_EXPORT ViewArea : public QWidget, public QAbstractNativeEventFilter {
    friend class VideoOutput;
    Q_OBJECT
public:
    ViewArea(QWidget* parent, View *view, bool paint_bg) KDE_NO_CDTOR_EXPORT;
    ~ViewArea() KDE_NO_CDTOR_EXPORT;
    KDE_NO_EXPORT bool isFullScreen () const { return m_fullscreen; }
    KDE_NO_EXPORT bool isMinimalMode () const { return m_minimal; }
    KDE_NO_EXPORT KActionCollection * actionCollection () const { return m_collection; }
    KDE_NO_EXPORT QRect topWindowRect () const { return m_topwindow_rect; }
    Surface *getSurface(Mrl* mrl) KDE_NO_EXPORT;
    void mouseMoved() KDE_NO_EXPORT;
    void scheduleRepaint(const IRect& rect) KDE_NO_EXPORT;
    ConnectionList* updaters() KDE_NO_EXPORT;
    void resizeEvent(QResizeEvent*) KDE_NO_EXPORT;
    void enableUpdaters(bool enable, unsigned int off_time) KDE_NO_EXPORT;
    void minimalMode ();
    IViewer *createVideoWidget ();
    void destroyVideoWidget (IViewer *widget);
    void setVideoWidgetVisible (bool show);
signals:
    void fullScreenChanged ();
public slots:
    void fullScreen() KDE_NO_EXPORT;
    void accelActivated() KDE_NO_EXPORT;
    void scale(int) KDE_NO_EXPORT;
protected:
    void showEvent(QShowEvent*) KDE_NO_EXPORT;
    void keyPressEvent(QKeyEvent*) KDE_NO_EXPORT;
    void mouseMoveEvent(QMouseEvent*) KDE_NO_EXPORT;
    void mousePressEvent(QMouseEvent*) KDE_NO_EXPORT;
    void mouseDoubleClickEvent(QMouseEvent*) KDE_NO_EXPORT;
    void dragEnterEvent(QDragEnterEvent*) KDE_NO_EXPORT;
    void dropEvent(QDropEvent*) KDE_NO_EXPORT;
    void contextMenuEvent(QContextMenuEvent* e) KDE_NO_EXPORT;
    void paintEvent(QPaintEvent*) KDE_NO_EXPORT;
    void timerEvent(QTimerEvent* e) KDE_NO_EXPORT;
    void closeEvent(QCloseEvent* e) KDE_NO_EXPORT;
    bool nativeEventFilter(const QByteArray& eventType, void * message, long *result);
    QPaintEngine *paintEngine () const;
private:
    void syncVisual() KDE_NO_EXPORT;
    void updateSurfaceBounds() KDE_NO_EXPORT;
    void stopTimers() KDE_NO_EXPORT;

    ConnectionList m_updaters;
    ViewerAreaPrivate *d;
    View * m_view;
    KActionCollection * m_collection;
    SurfacePtr surface;
    IRect m_repaint_rect;
    IRect m_update_rect;
    QRect m_topwindow_rect;
    typedef QList <IViewer *> VideoWidgetList;
    VideoWidgetList video_widgets;
    int m_mouse_invisible_timer;
    int m_repaint_timer;
    int m_restore_fullscreen_timer;
    bool m_fullscreen;
    bool m_minimal;
    bool m_updaters_enabled;
    bool m_paint_background;
};

/*
 * The video widget
 */
class KMPLAYER_NO_EXPORT VideoOutput : public QX11EmbedContainer, public IViewer {
    Q_OBJECT
public:
    VideoOutput(QWidget* parent, View* view) KDE_NO_CDTOR_EXPORT;
    ~VideoOutput() KDE_NO_CDTOR_EXPORT;

    int heightForWidth(int w) const KDE_NO_EXPORT;

    virtual WindowId windowHandle ();
    virtual WindowId clientHandle ();
    virtual WindowId ownHandle();
    using QWidget::setGeometry;
    virtual void setGeometry (const IRect &rect);
    virtual void setAspect (float a);
    virtual float aspect () { return m_aspect; }
    virtual void useIndirectWidget (bool);
    virtual void setMonitoring(Monitor m) KDE_NO_EXPORT;
    virtual void map() KDE_NO_EXPORT;
    virtual void unmap() KDE_NO_EXPORT;

    KDE_NO_EXPORT long inputMask () const { return m_input_mask; }
    void setBackgroundColor(const QColor& c) KDE_NO_EXPORT;
    void resetBackgroundColor() KDE_NO_EXPORT;
    void setCurrentBackgroundColor(const QColor& c) KDE_NO_EXPORT;
    KDE_NO_EXPORT View * view () const { return m_view; }

    WindowId clientWinId() { return m_client_window; }
    void discardClient() {}
    void embedded(WindowId handle) KDE_NO_EXPORT;
public slots:
    void fullScreenChanged() KDE_NO_EXPORT;
protected:
    void resizeEvent(QResizeEvent*) KDE_NO_EXPORT;
    void timerEvent(QTimerEvent*) KDE_NO_EXPORT;
    void dragEnterEvent(QDragEnterEvent*) KDE_NO_EXPORT;
    void dropEvent(QDropEvent*) KDE_NO_EXPORT;
    void contextMenuEvent(QContextMenuEvent* e) KDE_NO_EXPORT;
    //virtual void windowChanged( WId w );
private:
    WId m_plain_window;
    WId m_client_window;
    int resized_timer;
    unsigned int m_bgcolor;
    float m_aspect;
    View * m_view;
    long m_input_mask;
};

} // namespace KMPlayer

#endif
