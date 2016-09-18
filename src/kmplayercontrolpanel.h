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

#include "config-kmplayer.h"

#include <qwidget.h>
#include <qmenu.h>
#include <qpushbutton.h>

class QSlider;
//class QPushButton;
class QAction;
class QWidgetAction;
class QBoxLayout;
class QStringList;

namespace KMPlayer {

class View;

/*
 * A button from the controlpanel
 */
class KMPLAYER_NO_EXPORT KMPlayerMenuButton : public QPushButton {
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
class KMPLAYER_EXPORT KMPlayerPopupMenu : public QMenu {
    Q_OBJECT
public:
    KMPlayerPopupMenu(QWidget*, const QString& title) KDE_NO_CDTOR_EXPORT;
    KDE_NO_CDTOR_EXPORT ~KMPlayerPopupMenu () {}
signals:
    void mouseLeft ();
protected:
    void leaveEvent(QEvent*) KDE_NO_EXPORT;
};

/*
 * The volume bar from the controlpanel
 */
class KMPLAYER_EXPORT VolumeBar : public QWidget {
    Q_OBJECT
public:
    VolumeBar(QWidget* parent, View* view) KDE_NO_CDTOR_EXPORT;
    ~VolumeBar() KDE_NO_CDTOR_EXPORT;
    KDE_NO_EXPORT int value () const { return m_value; }
    void setValue (int v);
signals:
    void volumeChanged (int); // 0 - 100
protected:
    void wheelEvent(QWheelEvent* e) KDE_NO_EXPORT;
    void paintEvent(QPaintEvent*) KDE_NO_EXPORT;
    void mousePressEvent(QMouseEvent* e) KDE_NO_EXPORT;
    void mouseMoveEvent(QMouseEvent* e) KDE_NO_EXPORT;
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
    enum Button {
        button_config = 0, button_playlist,
        button_back, button_play, button_forward,
        button_stop, button_pause, button_record,
        button_broadcast, button_language,
        button_red, button_green, button_yellow, button_blue,
        button_last
    };
    ControlPanel(QWidget* parent, View* view) KDE_NO_CDTOR_EXPORT;
    KDE_NO_CDTOR_EXPORT ~ControlPanel () {}
    void showPositionSlider (bool show);
    void enableSeekButtons (bool enable);
    void enableRecordButtons (bool enable);
    void enableFullscreenButton(bool enable);
    void setPlaying (bool play);
    void setRecording (bool record) KDE_NO_EXPORT;
    void setAutoControls (bool b);
    void setPalette (const QPalette &) KDE_NO_EXPORT;
    int preferredHeight () KDE_NO_EXPORT;
    KDE_NO_EXPORT bool autoControls () const { return m_auto_controls; }
    KDE_NO_EXPORT QSlider * positionSlider () const { return m_posSlider; }
    KDE_NO_EXPORT QSlider * contrastSlider () const { return m_contrastSlider; }
    KDE_NO_EXPORT QSlider * brightnessSlider () const { return m_brightnessSlider; }
    KDE_NO_EXPORT QSlider * hueSlider () const { return m_hueSlider; }
    KDE_NO_EXPORT QSlider * saturationSlider () const { return m_saturationSlider; }
    QPushButton * button (Button b) const { return m_buttons [(int) b]; }
    KDE_NO_EXPORT QPushButton * broadcastButton () const { return m_buttons[button_broadcast]; }
    KDE_NO_EXPORT VolumeBar * volumeBar () const { return m_volume; }
    KDE_NO_EXPORT View * view () const { return m_view; }
    QAction *playersAction;
    QAction *videoConsoleAction;
    QAction *playlistAction;
    QAction *zoomAction;
    QAction *zoom50Action;
    QAction *zoom100Action;
    QAction *zoom150Action;
    QAction *fullscreenAction;
    QAction *colorAction;
    QAction *configureAction;
    QAction *bookmarkAction;
    QAction *languageAction;
    QWidgetAction *scaleLabelAction;
    QWidgetAction *scaleAction;
    QSlider *scale_slider;
    KMPlayerPopupMenu *popupMenu;
    KMPlayerPopupMenu *bookmarkMenu;
    KMPlayerPopupMenu *zoomMenu;
    KMPlayerPopupMenu *playerMenu;
    KMPlayerPopupMenu *colorMenu;
    KMPlayerPopupMenu *languageMenu;
    KMPlayerPopupMenu *audioMenu;
    KMPlayerPopupMenu *subtitleMenu;
public slots:
    void setLanguages(const QStringList& al, const QStringList& sl) KDE_NO_EXPORT;
    void actionToggled(QAction*) KDE_NO_EXPORT;
    void showPopupMenu() KDE_NO_EXPORT;
    void showLanguageMenu() KDE_NO_EXPORT;
    void setPlayingProgress(int position, int length) KDE_NO_EXPORT;
    void setLoadingProgress(int pos) KDE_NO_EXPORT;
protected:
    void timerEvent(QTimerEvent* e) KDE_NO_EXPORT;
    void setupPositionSlider(bool show) KDE_NO_EXPORT;
private slots:
    void buttonMouseEntered() KDE_NO_EXPORT;
    void buttonClicked() KDE_NO_EXPORT;
    void menuMouseLeft() KDE_NO_EXPORT;
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
    bool m_auto_controls; // depending on source caps
    bool m_popup_clicked;
};

}

#endif // KMPLAYER_CONTROLPANEL_H
