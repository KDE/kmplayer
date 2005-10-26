/**
 * Copyright (C) 2005 by Koos Vriezen <koos ! vriezen ? gmail ! com>
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
 *  the Free Software Foundation, Inc., 51 Franklin Steet, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 **/

#ifndef KMPLAYER_CONTROLPANEL_H
#define KMPLAYER_CONTROLPANEL_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif 

#include <qwidget.h>
#include <qpushbutton.h>

#include <kpopupmenu.h>

class QSlider;
//class QPushButton;
class QBoxLayout;
class QStringList;
class KPopupMenu;

namespace KMPlayer {

class View;

/*
 * A button from the controlpanel
 */
class KMPlayerMenuButton : public QPushButton {
    Q_OBJECT
public:
    KMPlayerMenuButton (QWidget *, QBoxLayout *, const char **, int = 0);
    KDE_NO_CDTOR_EXPORT ~KMPlayerMenuButton () {}
signals:
    void mouseEntered ();
protected:
    void enterEvent (QEvent *);
};

/*
 * The pop down menu from the controlpanel
 */
class KMPLAYER_EXPORT KMPlayerPopupMenu : public KPopupMenu {
    Q_OBJECT
public:
    KMPlayerPopupMenu (QWidget *);
    KDE_NO_CDTOR_EXPORT ~KMPlayerPopupMenu () {}
signals:
    void mouseLeft ();
protected:
    void leaveEvent (QEvent *);
};

/*
 * The volume bar from the controlpanel
 */
class KMPLAYER_EXPORT VolumeBar : public QWidget {
    Q_OBJECT
public:
    VolumeBar (QWidget * parent, View * view);
    ~VolumeBar ();
    KDE_NO_EXPORT int value () const { return m_value; }
    void setValue (int v);
signals:
    void volumeChanged (int); // 0 - 100
protected:
    void wheelEvent (QWheelEvent * e);
    void paintEvent (QPaintEvent *);
    void mousePressEvent (QMouseEvent * e);
    void mouseMoveEvent (QMouseEvent * e);
private:
    View * m_view;
    int m_value;
};

/*
 * The controlpanel GUI
 */
class KMPLAYER_EXPORT ControlPanel : public QWidget {
    Q_OBJECT
public:
    enum MenuID {
        menu_config = 0, menu_player, menu_fullscreen, menu_volume, 
        menu_bookmark, menu_zoom, menu_zoom50, menu_zoom100, menu_zoom150,
        menu_view, menu_video, menu_playlist
    };
    enum Button {
        button_config = 0, button_playlist,
        button_back, button_play, button_forward,
        button_stop, button_pause, button_record,
        button_broadcast, button_language,
        button_red, button_green, button_yellow, button_blue,
        button_last
    };
    ControlPanel (QWidget * parent, View * view);
    KDE_NO_CDTOR_EXPORT ~ControlPanel () {}
    void showPositionSlider (bool show);
    void enableSeekButtons (bool enable);
    void enableRecordButtons (bool enable);
    void setPlaying (bool play);
    void setRecording (bool record);
    void setAutoControls (bool b);
    KDE_NO_EXPORT bool autoControls () const { return m_auto_controls; }
    KDE_NO_EXPORT QSlider * positionSlider () const { return m_posSlider; }
    KDE_NO_EXPORT QSlider * contrastSlider () const { return m_contrastSlider; }
    KDE_NO_EXPORT QSlider * brightnessSlider () const { return m_brightnessSlider; }
    KDE_NO_EXPORT QSlider * hueSlider () const { return m_hueSlider; }
    KDE_NO_EXPORT QSlider * saturationSlider () const { return m_saturationSlider; }
    QPushButton * button (Button b) const { return m_buttons [(int) b]; }
    KDE_NO_EXPORT QPushButton * broadcastButton () const { return m_buttons[button_broadcast]; }
    KDE_NO_EXPORT VolumeBar * volumeBar () const { return m_volume; }
    KDE_NO_EXPORT KMPlayerPopupMenu * popupMenu () const { return m_popupMenu; }
    KDE_NO_EXPORT KPopupMenu * bookmarkMenu () const { return m_bookmarkMenu; }
    KDE_NO_EXPORT QPopupMenu * zoomMenu () const { return m_zoomMenu; }
    KDE_NO_EXPORT QPopupMenu * playerMenu () const { return m_playerMenu; }
    KDE_NO_EXPORT QPopupMenu * colorMenu () const { return m_colorMenu; }
    KDE_NO_EXPORT QPopupMenu * audioMenu () const { return m_audioMenu; }
    KDE_NO_EXPORT QPopupMenu * subtitleMenu () const { return m_subtitleMenu; }
    KDE_NO_EXPORT View * view () const { return m_view; }
public slots:
    void setLanguages (const QStringList & al, const QStringList & sl);
    void selectSubtitle (int id);
    void selectAudioLanguage (int id);
    void showPopupMenu ();
    void showLanguageMenu ();
    void setPlayingProgress (int position, int length);
    void setLoadingProgress (int pos);
protected:
    void timerEvent (QTimerEvent * e);
    void setupPositionSlider (bool show);
private slots:
    void buttonMouseEntered ();
    void buttonClicked ();
    void menuMouseLeft ();
private:
    enum { progress_loading, progress_playing } m_progress_mode;
    int m_progress_length;
    int m_popup_timer;
    int m_popdown_timer;
    int m_button_monitored;
    View * m_view;
    QBoxLayout * m_buttonbox;
    QSlider * m_posSlider;
    QSlider * m_contrastSlider;
    QSlider * m_brightnessSlider;
    QSlider * m_hueSlider;
    QSlider * m_saturationSlider;
    QPushButton * m_buttons [button_last];
    VolumeBar * m_volume;
    KMPlayerPopupMenu * m_popupMenu;
    KMPlayerPopupMenu * m_bookmarkMenu;
    KMPlayerPopupMenu * m_zoomMenu;
    KMPlayerPopupMenu * m_playerMenu;
    KMPlayerPopupMenu * m_colorMenu;
    KMPlayerPopupMenu * m_languageMenu;
    KMPlayerPopupMenu * m_audioMenu;
    KMPlayerPopupMenu * m_subtitleMenu;
    bool m_auto_controls; // depending on source caps
    bool m_popup_clicked;
};

}

#endif // KMPLAYER_CONTROLPANEL_H
