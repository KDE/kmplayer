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
#include <qguardedptr.h>

#include <kurl.h>
#include <qxembed.h>
#include <kmediaplayer/view.h>

class KMPlayerView;
class KMPlayerViewer;
class KMPlayerViewerHolder;
class KMPlayerControlPanel;
class QMultiLineEdit;
class QWidgetStack;
class QPixmap;
class QPushButton;
class QPopupMenu;
class QBoxLayout;
class QSlider;
class QLabel;
class QAccel;
class KPopupMenu;
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
    friend class KMPlayerPictureWidget;
public:
    KMPlayerView(QWidget *parent, const char *name = (char*) 0);
    ~KMPlayerView();

    void addText (const QString &);
    void init ();
    void reset ();
    //void print(QPrinter *pPrinter);

    //QMultiLineEdit * consoleOutput () const { return m_multiedit; }
    KMPlayerViewer * viewer () const { return m_viewer; }
    KMPlayerControlPanel * buttonBar () const { return m_buttonbar; }
    QWidgetStack * widgetStack () const { return m_widgetstack; }
    bool keepSizeRatio () const { return m_keepsizeratio; }
    void setKeepSizeRatio (bool b) { m_keepsizeratio = b; }
    bool useArts () const { return m_use_arts; }
    void setUseArts (bool b);
    bool showConsoleOutput () const { return m_show_console_output; }
    void setShowConsoleOutput (bool b);
    void setAutoHideButtons (bool b);
    //void setAutoHideSlider (bool b);
    bool autoHideButtons () const { return m_auto_hide_buttons; }
    void delayedShowButtons (bool show);
    bool isFullScreen () const { return m_layer->isFullScreen (); }
    bool setPicture (const QString & path);
    QPixmap * image () const { return m_image; }
    bool playing () const { return m_playing; }
    void setForeignViewer (KMPlayerView *);
public slots:
    void startsToPlay ();
    void showPopupMenu ();
    void setVolume (int);
    void updateVolume (float);
    void fullScreen ();
    void updateLayout ();
signals:
    void urlDropped (const KURL & url);
    void pictureClicked ();
    void fullScreenChanged ();
protected:
    void leaveEvent (QEvent *);
    void timerEvent (QTimerEvent *);
    void dragEnterEvent (QDragEnterEvent *);
    void dropEvent (QDropEvent *);
    bool x11Event (XEvent *);
private:
    void updateUseArts ();
    void emitPictureClicked () { emit pictureClicked (); }
    // widget for player's output
    QGuardedPtr<KMPlayerViewer> m_viewer;
    // console output
    QMultiLineEdit * m_multiedit;
    // click-to-play widget
    QWidget * m_picturewidget;
    // widget stack contains m_viewer, m_multiedit and m_picturewidget
    QWidgetStack * m_widgetstack;
    // widget that layouts m_widgetstack for ratio setting
    KMPlayerViewerHolder * m_holder;
    // widget that contains m_holder, m_buttonbar and m_posSlider
    KMPlayerViewLayer * m_layer;
    QString tmplog;
    QPixmap * m_image;
    KMPlayerControlPanel * m_buttonbar;
    QLabel * m_arts_label;
    QSlider * m_slider;
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
    bool m_revert_fullscreen : 1;
};

class KMPlayerControlPanel : public QWidget {
public:
    enum MenuID {
        menu_config = 0, menu_player, menu_fullscreen, menu_volume, 
        menu_bookmark, menu_zoom, menu_zoom50, menu_zoom100, menu_zoom150
    };
    KMPlayerControlPanel (QWidget * parent);
    QSlider * positionSlider () const { return m_posSlider; }
    QSlider * contrastSlider () const { return m_contrastSlider; }
    QSlider * brightnessSlider () const { return m_brightnessSlider; }
    QSlider * hueSlider () const { return m_hueSlider; }
    QSlider * saturationSlider () const { return m_saturationSlider; }
    QPushButton * backButton () const { return m_backButton; }
    QPushButton * playButton () const { return m_playButton; }
    QPushButton * forwardButton () const { return m_forwardButton; }
    QPushButton * pauseButton () const { return m_pauseButton; }
    QPushButton * stopButton () const { return m_stopButton; }
    QPushButton * configButton () const { return m_configButton; }
    QPushButton * recordButton () const { return m_recordButton; }
    QPushButton * broadcastButton () const { return m_broadcastButton; }
    QPopupMenu * popupMenu () const { return m_popupMenu; }
    KPopupMenu * bookmarkMenu () const { return m_bookmarkMenu; }
    QPopupMenu * zoomMenu () const { return m_zoomMenu; }
    QPopupMenu * playerMenu () const { return m_playerMenu; }
private:
    QSlider * m_posSlider;
    QSlider * m_contrastSlider;
    QSlider * m_brightnessSlider;
    QSlider * m_hueSlider;
    QSlider * m_saturationSlider;
    QPushButton * m_backButton;
    QPushButton * m_playButton;
    QPushButton * m_forwardButton;
    QPushButton * m_stopButton;
    QPushButton * m_pauseButton;
    QPushButton * m_configButton;
    QPushButton * m_recordButton;
    QPushButton * m_broadcastButton;
    QPopupMenu * m_popupMenu;
    KPopupMenu * m_bookmarkMenu;
    QPopupMenu * m_zoomMenu;
    QPopupMenu * m_playerMenu;
};

class KMPlayerViewer : public QXEmbed {
    Q_OBJECT
public:
    KMPlayerViewer(QWidget *parent, KMPlayerView * view);
    ~KMPlayerViewer();

    int heightForWidth (int w) const;

    void setAspect (float a);
    float aspect () { return m_aspect; }
    void sendKeyEvent (int key);
public slots:
    void sendConfigureEvent ();
signals:
    void aspectChanged ();
protected:
    void dragEnterEvent (QDragEnterEvent *);
    void dropEvent (QDropEvent *);
    void mouseMoveEvent (QMouseEvent * e);
private:
    float m_aspect;
    KMPlayerView * m_view;
};

#endif // KMPLAYERVIEW_H
