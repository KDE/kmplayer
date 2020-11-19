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
class KMPLAYERCOMMON_EXPORT ViewArea : public QWidget, public QAbstractNativeEventFilter
{
    friend class VideoOutput;
    Q_OBJECT
public:
    ViewArea(QWidget* parent, View *view, bool paint_bg);
    ~ViewArea() override;
    KMPLAYERCOMMON_NO_EXPORT bool isFullScreen () const { return m_fullscreen; }
    KMPLAYERCOMMON_NO_EXPORT bool isMinimalMode () const { return m_minimal; }
    KMPLAYERCOMMON_NO_EXPORT KActionCollection * actionCollection () const { return m_collection; }
    KMPLAYERCOMMON_NO_EXPORT QRect topWindowRect () const { return m_topwindow_rect; }
    Surface *getSurface(Mrl* mrl) KMPLAYERCOMMON_NO_EXPORT;
    void mouseMoved() KMPLAYERCOMMON_NO_EXPORT;
    void scheduleRepaint(const IRect& rect) KMPLAYERCOMMON_NO_EXPORT;
    ConnectionList* updaters() KMPLAYERCOMMON_NO_EXPORT;
    void resizeEvent(QResizeEvent*) override KMPLAYERCOMMON_NO_EXPORT;
    void enableUpdaters(bool enable, unsigned int off_time) KMPLAYERCOMMON_NO_EXPORT;
    void minimalMode ();
    IViewer *createVideoWidget ();
    void destroyVideoWidget (IViewer *widget);
    void setVideoWidgetVisible (bool show);
signals:
    void fullScreenChanged ();
public slots:
    void fullScreen() KMPLAYERCOMMON_NO_EXPORT;
    void accelActivated() KMPLAYERCOMMON_NO_EXPORT;
    void scale(int) KMPLAYERCOMMON_NO_EXPORT;
protected:
    void showEvent(QShowEvent*) override KMPLAYERCOMMON_NO_EXPORT;
    void keyPressEvent(QKeyEvent*) override KMPLAYERCOMMON_NO_EXPORT;
    void mouseMoveEvent(QMouseEvent*) override KMPLAYERCOMMON_NO_EXPORT;
    void mousePressEvent(QMouseEvent*) override KMPLAYERCOMMON_NO_EXPORT;
    void mouseDoubleClickEvent(QMouseEvent*) override KMPLAYERCOMMON_NO_EXPORT;
    void dragEnterEvent(QDragEnterEvent*) override KMPLAYERCOMMON_NO_EXPORT;
    void dropEvent(QDropEvent*) override KMPLAYERCOMMON_NO_EXPORT;
    void contextMenuEvent(QContextMenuEvent* e) override KMPLAYERCOMMON_NO_EXPORT;
    void paintEvent(QPaintEvent*) override KMPLAYERCOMMON_NO_EXPORT;
    void timerEvent(QTimerEvent* e) override KMPLAYERCOMMON_NO_EXPORT;
    void closeEvent(QCloseEvent* e) override KMPLAYERCOMMON_NO_EXPORT;
    bool nativeEventFilter(const QByteArray& eventType, void * message, long *result) override;
    QPaintEngine *paintEngine () const override;
private:
    void syncVisual() KMPLAYERCOMMON_NO_EXPORT;
    void updateSurfaceBounds() KMPLAYERCOMMON_NO_EXPORT;
    void stopTimers() KMPLAYERCOMMON_NO_EXPORT;

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
class VideoOutput : public QX11EmbedContainer, public IViewer
{
    Q_OBJECT
public:
    VideoOutput(QWidget* parent, View* view);
    ~VideoOutput() override;

    int heightForWidth(int w) const override;

    WindowId windowHandle () override;
    WindowId clientHandle () override;
    WindowId ownHandle() override;
    using QWidget::setGeometry;
    void setGeometry (const IRect &rect) override;
    void setAspect (float a) override;
    float aspect () override { return m_aspect; }
    void useIndirectWidget (bool) override;
    void setMonitoring(Monitor m) override;
    void map() override;
    void unmap() override;

    long inputMask () const { return m_input_mask; }
    void setBackgroundColor(const QColor& c);
    void resetBackgroundColor();
    void setCurrentBackgroundColor(const QColor& c);
    View * view () const { return m_view; }

    WindowId clientWinId() { return m_client_window; }
    void discardClient() {}
    void embedded(WindowId handle) override;
public slots:
    void fullScreenChanged();
protected:
    void resizeEvent(QResizeEvent*) override;
    void timerEvent(QTimerEvent*) override;
    void dragEnterEvent(QDragEnterEvent*) override;
    void dropEvent(QDropEvent*) override;
    void contextMenuEvent(QContextMenuEvent* e) override;
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
