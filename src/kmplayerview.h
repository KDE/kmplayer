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
 *  the Free Software Foundation, Inc., 51 Franklin Steet, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 **/

#ifndef KMPLAYERVIEW_H
#define KMPLAYERVIEW_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif 

#include <qwidget.h>
#include <qpushbutton.h>
#include <qguardedptr.h>
#include <qtextedit.h>
#include <qtextbrowser.h>

#include <kdockwidget.h>
#include <kpopupmenu.h>
#include <klistview.h>
#include <kurl.h>
#include <qxembed.h>
#include <kmediaplayer/view.h>

#include "kmplayersource.h"

class QWidgetStack;
class QPixmap;
class QPaintDevice;
class QPainter;
class QPopupMenu;
class QBoxLayout;
class QSlider;
class QLabel;
class QAccel;
class KPopupMenu;
class KActionCollection;
class KAction;
class KShortcut;

namespace KMPlayer {

class View;
class Viewer;
class ControlPanel;
class VolumeBar;
class Console;
class PlayListView;
class ViewAreaPrivate;

/*
 * An item in the playlist
 */
class KMPLAYER_EXPORT ListViewItem : public QListViewItem {
public:
    ListViewItem (QListViewItem *p, const NodePtr & e, PlayListView * lv);
    ListViewItem (QListViewItem *p, const AttributePtr & e, PlayListView * lv);
    ListViewItem (PlayListView *v, const NodePtr & e);
    KDE_NO_CDTOR_EXPORT ~ListViewItem () {}
    void paintCell (QPainter * p, const QColorGroup & cg, int column, int width, int align);
    NodePtrW m_elm;
    AttributePtrW m_attr;
    PlayListView * listview;
};

/*
 * The playlist GUI
 */
class KMPLAYER_EXPORT PlayListView : public KListView {
    Q_OBJECT
public:
    PlayListView (QWidget * parent, View * view);
    ~PlayListView ();
    void updateTree (NodePtr root, NodePtr active);
    void selectItem (const QString & txt);
    void setActiveForegroundColor (const QColor & c) { m_active_color = c; }
    const QColor & activeColor () const { return m_active_color; }
signals:
    void addBookMark (const QString & title, const QString & url);
protected:
    bool acceptDrag (QDropEvent* event) const;
public slots:
    void editCurrent ();
    void rename (QListViewItem * item, int c);
private slots:
    void contextMenuItem (QListViewItem *, const QPoint &, int);
    void itemExpanded (QListViewItem *);
    void copyToClipboard ();
    void addBookMark ();
    void toggleShowAllNodes ();
    void itemDropped (QDropEvent * e, QListViewItem * after);
    void itemIsRenamed (QListViewItem * item);
private:
    void populate (NodePtr e, NodePtr focus, QListViewItem * item, QListViewItem ** curitem);
    View * m_view;
    QPopupMenu * m_itemmenu;
    QPixmap folder_pix;
    QPixmap auxiliary_pix;
    QPixmap video_pix;
    QPixmap unknown_pix;
    QPixmap menu_pix;
    QPixmap config_pix;
    QPixmap url_pix;
    QColor m_active_color;
    bool m_show_all_nodes;
    bool m_have_dark_nodes;
    bool m_ignore_expanded;
};

/*
 * The area in which the video widget and controlpanel are laid out
 */
class ViewArea : public QWidget {
    Q_OBJECT
public:
    ViewArea (QWidget * parent, View * view);
    ~ViewArea ();
    bool isFullScreen () const { return m_fullscreen; }
    KActionCollection * actionCollection () const { return m_collection; }
    void setEventListener (NodePtr rl);
    void setAudioVideoGeometry (int x, int y, int w, int y, unsigned int * bg);
    void mouseMoved ();
    void scheduleRepaint (int x, int y, int w, int y);
    void moveRect (int x, int y, int w, int h, int x1, int y1);
    void resizeEvent (QResizeEvent *);
public slots:
    void fullScreen ();
    void accelActivated ();
    void scale (int);
protected:
    void showEvent (QShowEvent *);
    void mouseMoveEvent (QMouseEvent *);
    void mousePressEvent (QMouseEvent *);
    void dragEnterEvent (QDragEnterEvent *);
    void dropEvent (QDropEvent *);
    void contextMenuEvent (QContextMenuEvent * e);
    void paintEvent (QPaintEvent *);
    void timerEvent (QTimerEvent * e);
private:
    void syncVisual (QRect rect);
    ViewAreaPrivate * d;
    QWidget * m_parent;
    View * m_view;
    QPainter * m_painter;
    QPaintDevice * m_paint_buffer;
    KActionCollection * m_collection;
    NodePtrW eventListener;
    QRect m_av_geometry;
    QRect m_repaint_rect;
    int m_mouse_invisible_timer;
    int m_repaint_timer;
    int m_fullscreen_scale;
    int scale_lbl_id;
    int scale_slider_id;
    bool m_fullscreen;
};

/*
 * The console GUI
 */
class TextEdit : public QTextEdit {
public:
    TextEdit (QWidget * parent, View * view);
protected:
    void contextMenuEvent (QContextMenuEvent * e);
private:
    View * m_view;
};

/*
 * The infowindow GUI
 */
class InfoWindow : public QTextBrowser {
public:
    InfoWindow (QWidget * parent, View * view);
protected:
    void contextMenuEvent (QContextMenuEvent * e);
private:
    View * m_view;
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
 * The view containing ViewArea and playlist
 */
class KMPLAYER_EXPORT View : public KMediaPlayer::View {
    Q_OBJECT
public:
    enum ControlPanelMode {
        CP_Hide, CP_AutoHide, CP_Show, CP_Only /* no video widget */
    };
    enum WidgetType {
        WT_Video, WT_Console, WT_Picture, WT_Last
    };

    View (QWidget *parent, const char *);
    ~View();

    void addText (const QString &, bool eol=false);
    void init ();
    void reset ();
    //void print(QPrinter *pPrinter);

    TextEdit * console () const { return m_multiedit; }
    KDE_NO_EXPORT Viewer * viewer () const { return m_viewer; }
    KDE_NO_EXPORT ControlPanel * controlPanel () const {return m_control_panel;}
    KDE_NO_EXPORT PlayListView * playList () const { return m_playlist; }
    KDE_NO_EXPORT InfoWindow * infoPanel () const { return m_infopanel; }
    KDE_NO_EXPORT QWidgetStack * widgetStack () const { return m_widgetstack; }
    KDE_NO_EXPORT KDockArea * docArea () const { return m_dockarea; }
    KDE_NO_EXPORT ViewArea * viewArea () const { return m_view_area; }
    KDE_NO_EXPORT bool keepSizeRatio () const { return m_keepsizeratio; }
    void setKeepSizeRatio (bool b);
    void showWidget (WidgetType w);
    void setControlPanelMode (ControlPanelMode m);
    KDE_NO_EXPORT ControlPanelMode controlPanelMode () const { return m_controlpanel_mode; }
    void delayedShowButtons (bool show);
    bool isFullScreen () const;
    bool setPicture (const QString & path);
    KDE_NO_EXPORT QPixmap * image () const { return m_image; }
    KDE_NO_EXPORT bool videoStarted () const { return m_playing; }
    void setInfoMessage (const QString & msg);
    void setNoInfoMessages (bool b) { m_no_info = b; }
    void setViewOnly ();
    void dragEnterEvent (QDragEnterEvent *);
    void dropEvent (QDropEvent *);
    KDE_NO_EXPORT void emitPictureClicked () { emit pictureClicked (); }
public slots:
    /* raise video widget, might (auto) hides panel */
    void videoStart ();
    /* shows panel */
    void videoStop ();
    void showPopupMenu ();
    void setVolume (int);
    void updateVolume ();
    void fullScreen ();
    void updateLayout ();
    void toggleShowPlaylist ();
    void toggleVideoConsoleWindow ();
signals:
    void urlDropped (const KURL::List & urls);
    void pictureClicked ();
    void fullScreenChanged ();
    void windowVideoConsoleToggled (int wt);
protected:
    void leaveEvent (QEvent *);
    void timerEvent (QTimerEvent *);
    bool x11Event (XEvent *);
private slots:
    void ctrlButtonMouseEntered ();
    void ctrlButtonClicked ();
    void popupMenuMouseLeft ();
private:
    // widget for player's output
    Viewer * m_viewer;
    // console output
    TextEdit * m_multiedit;
    // widget stack contains m_viewer, m_multiedit and m_picturewidget
    QWidgetStack * m_widgetstack;
    // widget that layouts m_widgetstack for ratio setting and m_control_panel
    ViewArea * m_view_area;
    // playlist widget
    PlayListView * m_playlist;
    // infopanel widget
    InfoWindow * m_infopanel;
    // all widget types
    QWidget * m_widgettypes [WT_Last];
    KDockArea * m_dockarea;
    KDockWidget * m_dock_video;
    KDockWidget * m_dock_playlist;
    KDockWidget * m_dock_infopanel;
    QString tmplog;
    QPixmap * m_image;
    ControlPanel * m_control_panel;
    QSlider * m_volume_slider;
    const char * m_mixer_object;
    ControlPanelMode m_controlpanel_mode;
    ControlPanelMode m_old_controlpanel_mode;
    int controlbar_timer;
    int popup_timer;
    int popdown_timer;
    bool m_keepsizeratio;
    bool m_playing;
    bool m_mixer_init;
    bool m_inVolumeUpdate;
    bool m_sreensaver_disabled;
    bool m_tmplog_needs_eol;
    bool m_revert_fullscreen;
    bool m_popup_clicked;
    bool m_no_info;
};

/*
 * A button from the controlpanel
 */
class KMPlayerControlButton : public QPushButton {
    Q_OBJECT
public:
    KMPlayerControlButton (QWidget *, QBoxLayout *, const char **, int = 0);
    KDE_NO_CDTOR_EXPORT ~KMPlayerControlButton () {}
signals:
    void mouseEntered ();
protected:
    void enterEvent (QEvent *);
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
public:
    enum MenuID {
        menu_config = 0, menu_player, menu_fullscreen, menu_volume, 
        menu_bookmark, menu_zoom, menu_zoom50, menu_zoom100, menu_zoom150,
        menu_view, menu_video
    };
    enum Button {
        button_config = 0, button_playlist,
        button_back, button_play, button_forward,
        button_stop, button_pause, button_record, button_broadcast,
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
    KDE_NO_EXPORT VolumeBar * volumeBar () const { return m_volume; }
    KDE_NO_EXPORT KMPlayerPopupMenu * popupMenu () const { return m_popupMenu; }
    KDE_NO_EXPORT KPopupMenu * bookmarkMenu () const { return m_bookmarkMenu; }
    KDE_NO_EXPORT QPopupMenu * zoomMenu () const { return m_zoomMenu; }
    KDE_NO_EXPORT QPopupMenu * playerMenu () const { return m_playerMenu; }
    KDE_NO_EXPORT QPopupMenu * colorMenu () const { return m_colorMenu; }
private:
    enum { progress_loading, progress_playing } m_progress_mode;
    int m_progress_length;
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
    bool m_auto_controls; // depending on source caps
};

/*
 * The video widget
 */
class KMPLAYER_EXPORT Viewer : public QXEmbed {
    Q_OBJECT
public:
    Viewer(QWidget *parent, View * view);
    ~Viewer();

    int heightForWidth (int w) const;

    void setAspect (float a);
    float aspect () { return m_aspect; }
    void sendKeyEvent (int key);
    void setBackgroundColor (const QColor & c);
    void resetBackgroundColor ();
    void setCurrentBackgroundColor (const QColor & c);
    KDE_NO_EXPORT View * view () const { return m_view; }
public slots:
    void sendConfigureEvent ();
protected:
    void dragEnterEvent (QDragEnterEvent *);
    void dropEvent (QDropEvent *);
    void mouseMoveEvent (QMouseEvent * e);
    void contextMenuEvent (QContextMenuEvent * e);
private:
    unsigned int m_bgcolor;
    float m_aspect;
    View * m_view;
};

} // namespace

#endif // KMPLAYERVIEW_H
