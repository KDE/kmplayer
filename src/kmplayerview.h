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
#include <qpushbutton.h>
#include <qguardedptr.h>

#include <kdockwidget.h>
#include <kpopupmenu.h>
#include <klistview.h>
#include <kurl.h>
#include <qxembed.h>
#include <kmediaplayer/view.h>

#include "kmplayersource.h"

class KMPlayerView;
class KMPlayerViewer;
class KMPlayerViewerHolder;
class KMPlayerControlPanel;
class QMultiLineEdit;
class QWidgetStack;
class QPixmap;
class QPopupMenu;
class QBoxLayout;
class QSlider;
class QLabel;
class QAccel;
class KPopupMenu;
class KActionCollection;
class KAction;
class KShortcut;
class KMPlayerPlayListView;

class KMPlayerListViewItem : public QListViewItem {
public:
    KMPlayerListViewItem (QListViewItem *p, const ElementPtr & e, KMPlayerPlayListView * lv);
    KMPlayerListViewItem (KMPlayerPlayListView *v, const ElementPtr & e);
    KDE_NO_CDTOR_EXPORT ~KMPlayerListViewItem () {}
    void init ();
    ElementPtrW m_elm;
    KMPlayerPlayListView * listview;
};

class KMPlayerPlayListView : public KListView {
    friend class KMPlayerListViewItem;
    Q_OBJECT
public:
    KMPlayerPlayListView (QWidget * parent, KMPlayerView * view);
    ~KMPlayerPlayListView ();
    void updateTree (ElementPtr root, ElementPtr active);
    void selectItem (const QString & txt);
signals:
    void addBookMark (const QString & title, const QString & url);
protected:
    void dragEnterEvent (QDragEnterEvent *);
    void dropEvent (QDropEvent *);
private slots:
    void contextMenuItem (QListViewItem *, const QPoint &, int);
    void copyToClipboard ();
    void addBookMark ();
private:
    KMPlayerView * m_view;
    QPopupMenu * m_itemmenu;
    QPixmap folder_pix;
    QPixmap video_pix;
};

class KMPlayerViewLayer : public QWidget {
    friend class KMPlayerView;
    Q_OBJECT
public:
    KMPlayerViewLayer (QWidget * parent, KMPlayerView * view);
    bool isFullScreen () const { return m_fullscreen; }
public slots:
    void fullScreen ();
    void accelActivated ();
private:
    QWidget * m_parent;
    KMPlayerView * m_view;
    KActionCollection * m_collection;
    bool m_fullscreen : 1;
};

class KMPlayerPopupMenu : public KPopupMenu {
    Q_OBJECT
public:
    KMPlayerPopupMenu (QWidget *);
    KDE_NO_CDTOR_EXPORT ~KMPlayerPopupMenu () {}
signals:
    void mouseLeft ();
protected:
    void leaveEvent (QEvent *);
};

class KMPlayerView : public KMediaPlayer::View {
    Q_OBJECT
    friend class KMPlayerViewerHolder;
    friend class KMPlayerViewer;
    friend class KMPlayerPlayListView;
    friend class KMPlayerPictureWidget;
public:
    enum ControlPanelMode {
        CP_Hide, CP_AutoHide, CP_Show
    };
    enum WidgetType {
        WT_Video, WT_Console, WT_Picture, WT_Last
    };

    KMPlayerView (QWidget *parent, const char *);
    ~KMPlayerView();

    void addText (const QString &);
    void init ();
    void reset ();
    //void print(QPrinter *pPrinter);

    //QMultiLineEdit * consoleOutput () const { return m_multiedit; }
    KDE_NO_EXPORT KMPlayerViewer * viewer () const { return m_viewer; }
    KDE_NO_EXPORT KMPlayerControlPanel * buttonBar () const { return m_buttonbar; }
    KDE_NO_EXPORT KMPlayerPlayListView * playList () const { return m_playlist; }
    KDE_NO_EXPORT QWidgetStack * widgetStack () const { return m_widgetstack; }
    KDE_NO_EXPORT KDockArea * docArea () const { return m_dockarea; }
    KDE_NO_EXPORT bool keepSizeRatio () const { return m_keepsizeratio; }
    KDE_NO_EXPORT void setKeepSizeRatio (bool b) { m_keepsizeratio = b; }
    void showWidget (WidgetType w);
    void setControlPanelMode (ControlPanelMode m);
    //void setAutoHideSlider (bool b);
    KDE_NO_EXPORT ControlPanelMode controlPanelMode () const { return m_controlpanel_mode; }
    void delayedShowButtons (bool show);
    void addFullscreenAction (const QString &, const KShortcut &, QObject *, const char *, const char *);
    bool isFullScreen () const;
    bool setPicture (const QString & path);
    QPixmap * image () const { return m_image; }
    bool playing () const { return m_playing; }
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
    void showPlaylist ();
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
private slots:
    void ctrlButtonMouseEntered ();
    void ctrlButtonClicked ();
    void popupMenuMouseLeft ();
private:
    KDE_NO_EXPORT void emitPictureClicked () { emit pictureClicked (); }
    // widget for player's output
    KMPlayerViewer * m_viewer;
    // console output
    QMultiLineEdit * m_multiedit;
    // widget stack contains m_viewer, m_multiedit and m_picturewidget
    QWidgetStack * m_widgetstack;
    // widget that layouts m_widgetstack for ratio setting
    KMPlayerViewerHolder * m_holder;
    // widget that contains m_holder, m_buttonbar and m_posSlider
    KMPlayerViewLayer * m_layer;
    // playlist widget
    KMPlayerPlayListView * m_playlist;
    // all widget types
    QWidget * m_widgettypes [WT_Last];
    KDockArea * m_dockarea;
    KDockWidget * m_dock_video;
    KDockWidget * m_dock_playlist;
    QString tmplog;
    QPixmap * m_image;
    KMPlayerControlPanel * m_buttonbar;
    QLabel * m_mixer_label;
    QSlider * m_volume_slider;
    const char * m_mixer_object;
    ControlPanelMode m_controlpanel_mode;
    ControlPanelMode m_old_controlpanel_mode;
    int controlbar_timer;
    int popup_timer;
    int popdown_timer;
    bool m_keepsizeratio : 1;
    bool m_playing : 1;
    bool m_mixer_init : 1;
    bool m_inVolumeUpdate : 1;
    bool m_sreensaver_disabled : 1;
    bool m_revert_fullscreen : 1;
    bool m_popup_clicked : 1;
};

class KMPlayerControlButton : public QPushButton {
    Q_OBJECT
public:
    KMPlayerControlButton (QWidget *, QBoxLayout *,const char * const *, int=0);
    KDE_NO_CDTOR_EXPORT ~KMPlayerControlButton () {}
signals:
    void mouseEntered ();
protected:
    void enterEvent (QEvent *);
};

class KMPlayerControlPanel : public QWidget {
public:
    enum MenuID {
        menu_config = 0, menu_player, menu_fullscreen, menu_volume, 
        menu_bookmark, menu_zoom, menu_zoom50, menu_zoom100, menu_zoom150,
        menu_view, menu_video, menu_playlist, menu_console
    };
    enum Button {
        button_config = 0, button_back, button_play, button_forward,
        button_stop, button_pause, button_record, button_broadcast,
        button_last
    };
    KMPlayerControlPanel (QWidget * parent);
    KDE_NO_CDTOR_EXPORT ~KMPlayerControlPanel () {}
    void showPositionSlider (bool show);
    void enableSeekButtons (bool enable);
    void setPlaying (bool play);
    void setRecording (bool record);
    void setPlayingProgress (int pos);
    void setLoadingProgress (int pos);
    void setPlayingLength (int len);
    void setAutoControls (bool b);
    KDE_NO_EXPORT bool autoControls () const { return m_auto_controls; }
    KDE_NO_EXPORT QSlider * positionSlider () const { return m_posSlider; }
    KDE_NO_EXPORT QSlider * contrastSlider () const { return m_contrastSlider; }
    KDE_NO_EXPORT QSlider * brightnessSlider () const { return m_brightnessSlider; }
    KDE_NO_EXPORT QSlider * hueSlider () const { return m_hueSlider; }
    KDE_NO_EXPORT QSlider * saturationSlider () const { return m_saturationSlider; }
    QPushButton * button (Button b) const { return m_buttons [(int) b]; }
    KDE_NO_EXPORT QPushButton * broadcastButton () const { return m_buttons[button_broadcast]; }
    KDE_NO_EXPORT KMPlayerPopupMenu * popupMenu () const { return m_popupMenu; }
    KDE_NO_EXPORT KPopupMenu * bookmarkMenu () const { return m_bookmarkMenu; }
    KDE_NO_EXPORT QPopupMenu * zoomMenu () const { return m_zoomMenu; }
    KDE_NO_EXPORT QPopupMenu * playerMenu () const { return m_playerMenu; }
    KDE_NO_EXPORT QPopupMenu * viewMenu () const { return m_viewMenu; }
    KDE_NO_EXPORT QPopupMenu * colorMenu () const { return m_colorMenu; }
private:
    enum { progress_loading, progress_playing } m_progress_mode;
    int m_progress_length;
    QBoxLayout * m_buttonbox;
    QSlider * m_posSlider;
    QSlider * m_contrastSlider;
    QSlider * m_brightnessSlider;
    QSlider * m_hueSlider;
    QSlider * m_saturationSlider;
    QPushButton * m_buttons [button_last];
    KMPlayerPopupMenu * m_popupMenu;
    KMPlayerPopupMenu * m_bookmarkMenu;
    KMPlayerPopupMenu * m_viewMenu;
    KMPlayerPopupMenu * m_zoomMenu;
    KMPlayerPopupMenu * m_playerMenu;
    KMPlayerPopupMenu * m_colorMenu;
    bool m_auto_controls; // depending on source caps
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
    void contextMenuEvent (QContextMenuEvent * e);
private:
    float m_aspect;
    KMPlayerView * m_view;
};

#endif // KMPLAYERVIEW_H
