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
#include <QtGui/QX11EmbedContainer>
#include <QList>

#include "mediaobject.h"

class QPaintEngine;
class KActionCollection;

namespace KMPlayer {

class View;
class IViewer;

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
    Surface *getSurface (Mrl *mrl);
    void mouseMoved ();
    void scheduleRepaint (const IRect &rect);
    void resizeEvent (QResizeEvent *);
    void minimalMode ();
    IViewer *createVideoWidget ();
    void destroyVideoWidget (IViewer *widget);
    void setVideoWidgetVisible (bool show);
signals:
    void fullScreenChanged ();
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
    bool x11Event (XEvent *e);
    QPaintEngine *paintEngine () const;
private:
    void syncVisual (const IRect & rect);
    void updateSurfaceBounds ();
    void stopTimers ();

    QByteArray m_dock_state;
    View * m_view;
    KActionCollection * m_collection;
    SurfacePtr surface;
    IRect m_repaint_rect;
    QRect m_topwindow_rect;
    typedef QList <IViewer *> VideoWidgetList;
    VideoWidgetList video_widgets;
    int m_mouse_invisible_timer;
    int m_repaint_timer;
    bool m_fullscreen;
    bool m_minimal;
};

/*
 * The video widget
 */
class KMPLAYER_NO_EXPORT VideoOutput : public QX11EmbedContainer, public IViewer {
    Q_OBJECT
public:
    VideoOutput(QWidget *parent, View * view);
    ~VideoOutput();

    int heightForWidth (int w) const;

    virtual WindowId windowHandle ();
    virtual WindowId clientHandle ();
    using QWidget::setGeometry;
    virtual void setGeometry (const IRect &rect);
    virtual void setAspect (float a);
    virtual float aspect () { return m_aspect; }
    virtual void useIndirectWidget (bool);
    virtual void setMonitoring (Monitor m);
    virtual void map ();
    virtual void unmap ();

    KDE_NO_EXPORT long inputMask () const { return m_input_mask; }
    void sendKeyEvent (int key);
    void setBackgroundColor (const QColor & c);
    void resetBackgroundColor ();
    void setCurrentBackgroundColor (const QColor & c);
    KDE_NO_EXPORT View * view () const { return m_view; }
public slots:
    void sendConfigureEvent ();
    void embedded ();
    void fullScreenChanged ();
protected:
    void resizeEvent (QResizeEvent *);
    void timerEvent (QTimerEvent *) KDE_NO_EXPORT;
    void dragEnterEvent (QDragEnterEvent *);
    void dropEvent (QDropEvent *);
    void mouseMoveEvent (QMouseEvent * e);
    void contextMenuEvent (QContextMenuEvent * e);
    //virtual void windowChanged( WId w );
private:
    WId m_plain_window;
    int resized_timer;
    unsigned int m_bgcolor;
    float m_aspect;
    View * m_view;
    long m_input_mask;
};

} // namespace KMPlayer

#endif
