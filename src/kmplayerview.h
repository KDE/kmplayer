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
#include <kdemacros.h>
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
    enum ControlPanelMode {
        CP_Hide, CP_AutoHide, CP_Show
    };

    KMPlayerView(QWidget *parent, const char *name = (char*) 0);
    ~KMPlayerView();

    void addText (const QString &);
    void init ();
    void reset ();
    //void print(QPrinter *pPrinter);

    //QMultiLineEdit * consoleOutput () const { return m_multiedit; }
    KDE_NO_EXPORT KMPlayerViewer * viewer () const { return m_viewer; }
    KDE_NO_EXPORT KMPlayerControlPanel * buttonBar () const { return m_buttonbar; }
    KDE_NO_EXPORT QWidgetStack * widgetStack () const { return m_widgetstack; }
    KDE_NO_EXPORT bool keepSizeRatio () const { return m_keepsizeratio; }
    KDE_NO_EXPORT void setKeepSizeRatio (bool b) { m_keepsizeratio = b; }
    KDE_NO_EXPORT bool showConsoleOutput () const { return m_show_console_output; }
    void setShowConsoleOutput (bool b);
    void setControlPanelMode (ControlPanelMode m);
    //void setAutoHideSlider (bool b);
    KDE_NO_EXPORT ControlPanelMode controlPanelMode () const { return m_controlpanel_mode; }
    void delayedShowButtons (bool show);
    bool isFullScreen () const;
    bool setPicture (const QString & path);
    QPixmap * image () const { return m_image; }
    bool playing () const { return m_playing; }
    void setForeignViewer (KMPlayerView *);
public slots:
    /* raise video widget, might (auto) hides panel */
    void videoStart ();
    /* might raise console widget, shows panel */
    void videoStop ();
    void showPopupMenu ();
    void setVolume (int);
    void updateVolume ();
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
    KDE_NO_EXPORT void emitPictureClicked () { emit pictureClicked (); }
    // widget for player's output
    QGuardedPtr<KMPlayerViewer> m_viewer;
    QGuardedPtr<KMPlayerView> m_foreign_view;
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
    QLabel * m_mixer_label;
    QSlider * m_volume_slider;
    const char * m_mixer_object;
    ControlPanelMode m_controlpanel_mode;
    ControlPanelMode m_old_controlpanel_mode;
    int delayed_timer;
    bool m_keepsizeratio : 1;
    bool m_show_console_output : 1;
    bool m_playing : 1;
    bool m_mixer_init : 1;
    bool m_inVolumeUpdate : 1;
    bool m_sreensaver_disabled : 1;
    bool m_revert_fullscreen : 1;
};

static const int KMPlayerControlPanelButtons = 8;

class KMPlayerControlPanel : public QWidget {
public:
    enum MenuID {
        menu_config = 0, menu_player, menu_fullscreen, menu_volume, 
        menu_bookmark, menu_zoom, menu_zoom50, menu_zoom100, menu_zoom150
    };
    enum Button {
        button_config = 0, button_back, button_play, button_forward,
        button_stop, button_pause, button_record, button_broadcast
    };
    KMPlayerControlPanel (QWidget * parent);
    void showPositionSlider (bool show);
    void enableSeekButtons (bool enable);
    void setPlaying (bool play);
    void setRecording (bool record);
    void setPlayingProgress (int pos);
    void setLoadingProgress (int pos);
    void setPlayingLength (int len);
    KDE_NO_EXPORT QSlider * positionSlider () const { return m_posSlider; }
    KDE_NO_EXPORT QSlider * contrastSlider () const { return m_contrastSlider; }
    KDE_NO_EXPORT QSlider * brightnessSlider () const { return m_brightnessSlider; }
    KDE_NO_EXPORT QSlider * hueSlider () const { return m_hueSlider; }
    KDE_NO_EXPORT QSlider * saturationSlider () const { return m_saturationSlider; }
    KDE_NO_EXPORT QPushButton * backButton () const { return m_buttons[button_back]; }
    KDE_NO_EXPORT QPushButton * playButton () const { return m_buttons[button_play]; }
    KDE_NO_EXPORT QPushButton * forwardButton () const { return m_buttons[button_forward]; }
    KDE_NO_EXPORT QPushButton * pauseButton () const { return m_buttons[button_pause]; }
    KDE_NO_EXPORT QPushButton * stopButton () const { return m_buttons[button_stop]; }
    KDE_NO_EXPORT QPushButton * configButton () const { return m_buttons[button_config]; }
    KDE_NO_EXPORT QPushButton * recordButton () const { return m_buttons[button_record]; }
    KDE_NO_EXPORT QPushButton * broadcastButton () const { return m_buttons[button_broadcast]; }
    KDE_NO_EXPORT QPopupMenu * popupMenu () const { return m_popupMenu; }
    KDE_NO_EXPORT KPopupMenu * bookmarkMenu () const { return m_bookmarkMenu; }
    KDE_NO_EXPORT QPopupMenu * zoomMenu () const { return m_zoomMenu; }
    KDE_NO_EXPORT QPopupMenu * playerMenu () const { return m_playerMenu; }
private:
    enum { progress_loading, progress_playing } m_progress_mode;
    int m_progress_length;
    QBoxLayout * m_buttonbox;
    QSlider * m_posSlider;
    QSlider * m_contrastSlider;
    QSlider * m_brightnessSlider;
    QSlider * m_hueSlider;
    QSlider * m_saturationSlider;
    QPushButton * m_buttons[KMPlayerControlPanelButtons];
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
