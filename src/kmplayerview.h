/**
 * Copyright (C) 2002-2003 by Koos Vriezen <koos ! vriezen ? xs4all ! nl>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License version 2 as published by the Free Software Foundation.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 *  Boston, MA 02111-1307, USA.
 **/

#ifndef KMPLAYERVIEW_H
#define KMPLAYERVIEW_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif 

#include <qwidget.h>
#include <kurl.h>
#include <kmediaplayer/view.h>

class KMPlayerView;
class KMPlayerViewer;
class QMultiLineEdit;
class QPushButton;
class QPopupMenu;
class QBoxLayout;
class QSlider;
class QLabel;
class QAccel;
class KArtsFloatWatch;
namespace Arts {
    class SoundServerV2;
    class StereoVolumeControl;
}

class KMPlayerViewLayer : public QWidget {
    Q_OBJECT
public:
    KMPlayerViewLayer (KMPlayerView * parent, QBoxLayout * b);
    bool isFullScreen () const { return m_fullscreen; }
public slots:
    void fullScreen ();
    void accelActivated ();
private:
    KMPlayerView * m_view;
    QBoxLayout * m_box;
    QAccel * m_accel;
    bool m_fullscreen : 1;
};

class KMPlayerView : public KMediaPlayer::View {
    Q_OBJECT
    friend class KMPlayerViewerHolder;
    friend class KMPlayerViewer;
public:
    enum MenuID {
        menu_config = 0, menu_fullscreen, menu_volume, 
        menu_zoom, menu_zoom50, menu_zoom100, menu_zoom150
    };
    KMPlayerView(QWidget *parent, const char *name = (char*) 0);
    ~KMPlayerView();

    void addText (const QString &);
    void init ();
    void reset ();
    //void print(QPrinter *pPrinter);

    QMultiLineEdit * consoleOutput () const { return m_multiedit; }
    KMPlayerViewer * viewer () const { return m_viewer; }
    QWidget * buttonBar () const { return m_buttonbar; }
    QPushButton * backButton () const { return m_backButton; }
    QPushButton * playButton () const { return m_playButton; }
    QPushButton * forwardButton () const { return m_forwardButton; }
    QPushButton * pauseButton () const { return m_pauseButton; }
    QPushButton * stopButton () const { return m_stopButton; }
    QPushButton * configButton () const { return m_configButton; }
    QPushButton * recordButton () const { return m_recordButton; }
    QPushButton * broadcastButton () const { return m_broadcastButton; }
    QSlider * positionSlider () const { return m_posSlider; }
    QSlider * contrastSlider () const { return m_contrastSlider; }
    QSlider * brightnessSlider () const { return m_brightnessSlider; }
    QSlider * hueSlider () const { return m_hueSlider; }
    QSlider * saturationSlider () const { return m_saturationSlider; }
    QPopupMenu * popupMenu () const { return m_popupMenu; }
    QPopupMenu * zoomMenu () const { return m_zoomMenu; }
    bool keepSizeRatio () const { return m_keepsizeratio; }
    void setKeepSizeRatio (bool b) { m_keepsizeratio = b; }
    bool useArts () const { return m_use_arts; }
    void setUseArts (bool b);
    bool showConsoleOutput () const { return m_show_console_output; }
    void setShowConsoleOutput (bool b) { m_show_console_output = b; }
    void setAutoHideButtons (bool b);
    //void setAutoHideSlider (bool b);
    bool autoHideButtons () const { return m_auto_hide_buttons; }
    void delayedShowButtons (bool show);
    bool isFullScreen () const { return m_layer->isFullScreen (); }
public slots:
    void startsToPlay ();
    void showPopupMenu ();
    void setVolume (int);
    void updateVolume (float);
    void fullScreen ();
signals:
    void urlDropped (const KURL & url);
protected:
    void leaveEvent (QEvent *);
    void timerEvent (QTimerEvent *);
    void dragEnterEvent (QDragEnterEvent *);
    void dropEvent (QDropEvent *);
private:
    KMPlayerViewer * m_viewer;
    KMPlayerViewLayer * m_layer;
    QMultiLineEdit * m_multiedit;
    QWidget * m_buttonbar;
    QPushButton * m_backButton;
    QPushButton * m_playButton;
    QPushButton * m_forwardButton;
    QPushButton * m_stopButton;
    QPushButton * m_pauseButton;
    QPushButton * m_configButton;
    QPushButton * m_recordButton;
    QPushButton * m_broadcastButton;
    QPopupMenu * m_popupMenu;
    QPopupMenu * m_zoomMenu;
    QLabel * m_arts_label;
    QSlider * m_slider;
    QSlider * m_posSlider;
    QSlider * m_contrastSlider;
    QSlider * m_brightnessSlider;
    QSlider * m_hueSlider;
    QSlider * m_saturationSlider;
    Arts::SoundServerV2 * m_artsserver;
    Arts::StereoVolumeControl * m_svc;
    KArtsFloatWatch * m_watch;
    int delayed_timer;
    bool m_keepsizeratio : 1;
    bool m_show_console_output : 1;
    bool m_auto_hide_buttons : 1;
    bool m_playing : 1;
    bool m_use_arts : 1;
    bool m_inVolumeUpdate : 1;
    bool m_sreensaver_disabled : 1;
};

class KMPlayerViewer : public QWidget {
    Q_OBJECT
public:
    KMPlayerViewer(QWidget *parent, KMPlayerView * view);
    ~KMPlayerViewer();

    int heightForWidth (int w) const;

    void setAspect (float a);
    float aspect () { return m_aspect; }
signals:
    void aboutToPlay ();
protected:
    void showEvent (QShowEvent *);
    void hideEvent (QHideEvent *);
    void dragEnterEvent (QDragEnterEvent *);
    void dropEvent (QDropEvent *);
    bool x11Event (XEvent *);
    void mouseMoveEvent (QMouseEvent * e);
private:
    float m_aspect;
    KMPlayerView * m_view;
};

class KMPlayerViewerHolder : public QWidget {
    Q_OBJECT
public:
    KMPlayerViewerHolder (QWidget * parent, KMPlayerView * view);
protected:
    void resizeEvent (QResizeEvent *);
    void mouseMoveEvent (QMouseEvent *);
    void dragEnterEvent (QDragEnterEvent *);
    void dropEvent (QDropEvent *);
private:
    KMPlayerView * m_view;
};

#endif // KMPLAYERVIEW_H
