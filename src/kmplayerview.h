/***************************************************************************
  kmplayerview.h  -  description
  -------------------
begin                : Sat Dec  7 16:14:51 CET 2002
copyright            : (C) 2002 by Koos Vriezen
email                : 
 ***************************************************************************/

/***************************************************************************
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
 ***************************************************************************/

#ifndef KMPLAYERVIEW_H
#define KMPLAYERVIEW_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif 

#include <qwidget.h>
#include <kmediaplayer/view.h>

class KMPlayerDoc;
class KMPlayerView;
class KMPlayerViewer;
class QMultiLineEdit;
class QPushButton;
class QPopupMenu;
class QBoxLayout;
class QSlider;
class QLabel;
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
private:
    KMPlayerView * m_view;
    QBoxLayout * m_box;
    bool m_fullscreen : 1;
};

class KMPlayerView : public KMediaPlayer::View {
    Q_OBJECT
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
    void print(QPrinter *pPrinter);

    QMultiLineEdit * consoleOutput () const { return m_multiedit; }
    KMPlayerViewer * viewer () const { return m_viewer; }
    QWidget * buttonBar () const { return m_buttonbar; }
    QPushButton * backButton () const { return m_backButton; }
    QPushButton * playButton () const { return m_playButton; }
    QPushButton * forwardButton () const { return m_forwardButton; }
    QPushButton * pauseButton () const { return m_pauseButton; }
    QPushButton * stopButton () const { return m_stopButton; }
    QPushButton * configButton () const { return m_configButton; }
    QSlider * positionSlider () const { return m_posSlider; }
    QPopupMenu * popupMenu () const { return m_popupMenu; }
    QPopupMenu * zoomMenu () const { return m_zoomMenu; }
    bool keepSizeRatio () const { return m_keepsizeratio; }
    void setKeepSizeRatio (bool b) { m_keepsizeratio = b; }
    bool useArts () const { return m_use_arts; }
    void setUseArts (bool b);
    bool showConsoleOutput () const { return m_show_console_output; }
    void setShowConsoleOutput (bool b) { m_show_console_output = b; }
    void setAutoHideButtons (bool b);
    bool autoHideButtons () const { return m_auto_hide_buttons; }
    void delayedShowButtons (bool show);
    bool isFullScreen () const { return m_layer->isFullScreen (); }
public slots:
    void startsToPlay ();
    void showPopupMenu ();
    void setVolume (int);
    void updateVolume (float);
    void fullScreen ();
protected:
    void leaveEvent (QEvent *);
    void timerEvent (QTimerEvent *);
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
    QPopupMenu * m_popupMenu;
    QPopupMenu * m_zoomMenu;
    QLabel * m_arts_label;
    QSlider * m_slider;
    QSlider * m_posSlider;
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
private:
    KMPlayerView * m_view;
};

#endif // KMPLAYERVIEW_H
