/*
    SPDX-FileCopyrightText: 2005 Koos Vriezen <koos ! vriezen ? gmail ! com>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#ifndef KMPLAYER_CONTROLPANEL_H
#define KMPLAYER_CONTROLPANEL_H

#include "config-kmplayer.h"
#include "kmplayercommon_export.h"

#include <QWidget>
#include <QMenu>
#include <QPushButton>

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
class KMPlayerMenuButton : public QPushButton
{
    Q_OBJECT
public:
    KMPlayerMenuButton (QWidget *, QBoxLayout *, const char **, int = 0);
    ~KMPlayerMenuButton () override {}
Q_SIGNALS:
    void mouseEntered ();
protected:
    void enterEvent (QEvent *) override;
};

/*
 * The pop down menu from the controlpanel
 */
class KMPLAYERCOMMON_EXPORT KMPlayerPopupMenu : public QMenu
{
    Q_OBJECT
public:
    KMPlayerPopupMenu(QWidget*, const QString& title);
    ~KMPlayerPopupMenu () override {}
Q_SIGNALS:
    void mouseLeft ();
protected:
    void leaveEvent(QEvent*) override KMPLAYERCOMMON_NO_EXPORT;
};

/*
 * The volume bar from the controlpanel
 */
class KMPLAYERCOMMON_EXPORT VolumeBar : public QWidget
{
    Q_OBJECT
public:
    VolumeBar(QWidget* parent, View* view);
    ~VolumeBar() override;
    KMPLAYERCOMMON_NO_EXPORT int value () const { return m_value; }
    void setValue (int v);
Q_SIGNALS:
    void volumeChanged (int); // 0 - 100
protected:
    void wheelEvent(QWheelEvent* e) override KMPLAYERCOMMON_NO_EXPORT;
    void paintEvent(QPaintEvent*) override KMPLAYERCOMMON_NO_EXPORT;
    void mousePressEvent(QMouseEvent* e) override KMPLAYERCOMMON_NO_EXPORT;
    void mouseMoveEvent(QMouseEvent* e) override KMPLAYERCOMMON_NO_EXPORT;
private:
    View * m_view;
    int m_value;
};

/*
 * The controlpanel GUI
 */
class KMPLAYERCOMMON_EXPORT ControlPanel : public QWidget
{
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
    ControlPanel(QWidget* parent, View* view);
    ~ControlPanel () override {}
    void showPositionSlider (bool show);
    void enableSeekButtons (bool enable);
    void enableRecordButtons (bool enable);
    void enableFullscreenButton(bool enable);
    void setPlaying (bool play);
    void setRecording (bool record) KMPLAYERCOMMON_NO_EXPORT;
    void setAutoControls (bool b);
    void setPalette (const QPalette &) KMPLAYERCOMMON_NO_EXPORT;
    int preferredHeight () KMPLAYERCOMMON_NO_EXPORT;
    KMPLAYERCOMMON_NO_EXPORT bool autoControls () const { return m_auto_controls; }
    KMPLAYERCOMMON_NO_EXPORT QSlider * positionSlider () const { return m_posSlider; }
    KMPLAYERCOMMON_NO_EXPORT QSlider * contrastSlider () const { return m_contrastSlider; }
    KMPLAYERCOMMON_NO_EXPORT QSlider * brightnessSlider () const { return m_brightnessSlider; }
    KMPLAYERCOMMON_NO_EXPORT QSlider * hueSlider () const { return m_hueSlider; }
    KMPLAYERCOMMON_NO_EXPORT QSlider * saturationSlider () const { return m_saturationSlider; }
    QPushButton * button (Button b) const { return m_buttons [(int) b]; }
    KMPLAYERCOMMON_NO_EXPORT QPushButton * broadcastButton () const { return m_buttons[button_broadcast]; }
    KMPLAYERCOMMON_NO_EXPORT VolumeBar * volumeBar () const { return m_volume; }
    KMPLAYERCOMMON_NO_EXPORT View * view () const { return m_view; }
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
public Q_SLOTS:
    void setLanguages(const QStringList& al, const QStringList& sl) KMPLAYERCOMMON_NO_EXPORT;
    void actionToggled(QAction*) KMPLAYERCOMMON_NO_EXPORT;
    void showPopupMenu() KMPLAYERCOMMON_NO_EXPORT;
    void showLanguageMenu() KMPLAYERCOMMON_NO_EXPORT;
    void setPlayingProgress(int position, int length) KMPLAYERCOMMON_NO_EXPORT;
    void setLoadingProgress(int pos) KMPLAYERCOMMON_NO_EXPORT;
protected:
    void timerEvent(QTimerEvent* e) override KMPLAYERCOMMON_NO_EXPORT;
    void setupPositionSlider(bool show) KMPLAYERCOMMON_NO_EXPORT;
private Q_SLOTS:
    void buttonMouseEntered() KMPLAYERCOMMON_NO_EXPORT;
    void buttonClicked() KMPLAYERCOMMON_NO_EXPORT;
    void menuMouseLeft() KMPLAYERCOMMON_NO_EXPORT;
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
