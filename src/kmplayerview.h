/*
    SPDX-FileCopyrightText: 2002-2003 Koos Vriezen <koos.vriezen@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#ifndef KMPLAYERVIEW_H
#define KMPLAYERVIEW_H

#include "config-kmplayer.h"
#include <qwidget.h>
#include <qtextedit.h>
#include <QImage>
#include <QList>
#include <QUrl>

#include <kmediaplayer/view.h>

#include "kmplayercommon_export.h"

#define MOUSE_INVISIBLE_DELAY 2000

class QSlider;
class QStatusBar;
class QMainWindow;
class QDockWidget;
class KActionCollection;

namespace KMPlayer {

class View;
class ViewArea;
class ControlPanel;
class Console;
class PlayListView;
class PlayListView;
class TopPlayItem;

typedef QStatusBar StatusBar;

/*
 * The console GUI
 */
class TextEdit : public QTextEdit
{
public:
    TextEdit (QWidget * parent, View * view);
protected:
    void contextMenuEvent (QContextMenuEvent * e) override;
private:
    View * m_view;
};

/*
 * The infowindow GUI
 */
class InfoWindow : public QTextEdit
{
public:
    InfoWindow (QWidget * parent, View * view);
    View * view () const { return m_view; }
protected:
    void contextMenuEvent (QContextMenuEvent * e) override;
private:
    View * m_view;
};

class PictureWidget : public QWidget
{
    View * m_view;
public:
    PictureWidget (QWidget * parent, View * view);
    ~PictureWidget () override {}
protected:
    void mousePressEvent (QMouseEvent *) override;
    void mouseMoveEvent (QMouseEvent *) override;
};

/*
 * The view containing ViewArea and playlist
 */
class KMPLAYERCOMMON_EXPORT View : public KMediaPlayer::View
{
    Q_OBJECT
public:
    enum ControlPanelMode {
        CP_Hide, CP_AutoHide, CP_Show, CP_Only /* no video widget */
    };
    enum StatusBarMode {
        SB_Hide, SB_Show, SB_Only /* no video widget */
    };

    View(QWidget* parent);
    ~View() override;

    void addText (const QString &, bool eol=false);
    void init(KActionCollection* ac, bool transparent) KMPLAYERCOMMON_NO_EXPORT;
    void initDock (QWidget *central);
    void reset() KMPLAYERCOMMON_NO_EXPORT;
    //void print(QPrinter *pPrinter);

    TextEdit * console () const { return m_multiedit; }
    PictureWidget *picture () const KMPLAYERCOMMON_NO_EXPORT { return m_picture; }
    KMPLAYERCOMMON_NO_EXPORT ControlPanel * controlPanel () const {return m_control_panel;}
    KMPLAYERCOMMON_NO_EXPORT StatusBar * statusBar () const {return m_status_bar;}
    KMPLAYERCOMMON_NO_EXPORT PlayListView * playList () const { return m_playlist; }
    KMPLAYERCOMMON_NO_EXPORT InfoWindow * infoPanel () const { return m_infopanel; }
    KMPLAYERCOMMON_NO_EXPORT QMainWindow *dockArea () const { return m_dockarea; }
    KMPLAYERCOMMON_NO_EXPORT QDockWidget *dockPlaylist () const { return m_dock_playlist; }
    KMPLAYERCOMMON_NO_EXPORT ViewArea * viewArea () const { return m_view_area; }
    KMPLAYERCOMMON_NO_EXPORT bool keepSizeRatio () const { return m_keepsizeratio; }
    void setKeepSizeRatio (bool b);
    void setControlPanelMode (ControlPanelMode m);
    void setStatusBarMode (StatusBarMode m);
    void setEraseColor(const QColor&) KMPLAYERCOMMON_NO_EXPORT;
    KMPLAYERCOMMON_NO_EXPORT ControlPanelMode controlPanelMode () const { return m_controlpanel_mode; }
    KMPLAYERCOMMON_NO_EXPORT StatusBarMode statusBarMode () const { return m_statusbar_mode; }
    void delayedShowButtons(bool show) KMPLAYERCOMMON_NO_EXPORT;
    bool isFullScreen () const;
    int statusBarHeight() const KMPLAYERCOMMON_NO_EXPORT;
    KMPLAYERCOMMON_NO_EXPORT bool editMode () const { return m_edit_mode; }
#ifndef KMPLAYER_WITH_CAIRO
    bool setPicture (const QString & path);
#endif
    void setNoInfoMessages (bool b) { m_no_info = b; }
    void setViewOnly ();
    void setEditMode (TopPlayItem *, bool enable=true);
    void dragEnterEvent(QDragEnterEvent*) override KMPLAYERCOMMON_NO_EXPORT;
    void dropEvent(QDropEvent*) override KMPLAYERCOMMON_NO_EXPORT;
    KMPLAYERCOMMON_NO_EXPORT void emitPictureClicked () { emit pictureClicked (); }
    /* raise video widget, might (auto) hides panel */
    void videoStart() KMPLAYERCOMMON_NO_EXPORT;
    void playingStart() KMPLAYERCOMMON_NO_EXPORT;
    /* shows panel */
    void playingStop() KMPLAYERCOMMON_NO_EXPORT;
    void mouseMoved(int x, int y) KMPLAYERCOMMON_NO_EXPORT;
public slots:
    void fullScreen ();
    void updateLayout() KMPLAYERCOMMON_NO_EXPORT;
    void toggleShowPlaylist ();
    void toggleVideoConsoleWindow ();
    void setInfoMessage (const QString & msg);
    void setStatusMessage (const QString & msg);
signals:
    void urlDropped (const QList<QUrl>& urls);
    void pictureClicked ();
    void fullScreenChanged ();
    void windowVideoConsoleToggled (bool show);
protected:
    void leaveEvent (QEvent *) override KMPLAYERCOMMON_NO_EXPORT;
    void timerEvent(QTimerEvent*) override KMPLAYERCOMMON_NO_EXPORT;
private:
    QByteArray m_dock_state;
    // console output
    TextEdit * m_multiedit;
    PictureWidget *m_picture;
    // widget that layouts m_widgetstack for ratio setting and m_control_panel
    ViewArea * m_view_area;
    // playlist widget
    PlayListView * m_playlist;
    // infopanel widget
    InfoWindow * m_infopanel;
    QMainWindow *m_dockarea;
    QDockWidget *m_dock_playlist;
    QDockWidget *m_dock_infopanel;
    QString tmplog;
    QImage m_image;
    ControlPanel * m_control_panel;
    StatusBar * m_status_bar;
    ControlPanelMode m_controlpanel_mode;
    ControlPanelMode m_old_controlpanel_mode;
    StatusBarMode m_statusbar_mode;
    int controlbar_timer;
    int infopanel_timer;
    int m_restore_state_timer;
    int m_powerManagerStopSleep;
    bool m_keepsizeratio;
    bool m_playing;
    bool m_sreensaver_disabled;
    bool m_tmplog_needs_eol;
    bool m_revert_fullscreen;
    bool m_no_info;
    bool m_edit_mode;
};

} // namespace

#endif // KMPLAYERVIEW_H
