/**
 * Copyright (C) 2002-2003 by Koos Vriezen <koos.vriezen@gmail.com>
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

#include <stdio.h>
#include <math.h>

#include "config-kmplayer.h"
// include files for Qt
#include <qstyle.h>
#include <qtimer.h>
#include <qpainter.h>
#include <qmetaobject.h>
#include <qlayout.h>
#include <qpixmap.h>
#include <qtextedit.h>
#include <qtooltip.h>
#include <qapplication.h>
#include <qiconset.h>
#include <qcursor.h>
#include <qkeysequence.h>
#include <qslider.h>
#include <qlabel.h>
#include <qdatastream.h>
#include <QStackedWidget>
#include <QContextMenuEvent>
#include <Q3TextDrag>
#include <QDropEvent>
#include <QAction>
#include <QDragEnterEvent>
#include <QTextDocument>
#include <QTextCursor>
#include <qcursor.h>
#include <qclipboard.h>
#include <QMainWindow>
#include <QDockWidget>

#include <kiconloader.h>
#include <kstatusbar.h>
#include <kdebug.h>
#include <klocale.h>
#include <kapplication.h>
#include <kactioncollection.h>
#include <kstdaction.h>
#include <kshortcut.h>
#include <kfinddialog.h>
#include <kglobalsettings.h>
#include <k3staticdeleter.h>

#include "kmplayerview.h"
#include "kmplayercontrolpanel.h"
#include "kmplayersource.h"
#include "playlistview.h"
#include "viewarea.h"

#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
static const int XKeyPress = KeyPress;
#undef KeyPress
#undef Always
#undef Never
#undef Status
#undef Unsorted
#undef Bool

extern const char * normal_window_xpm[];
extern const char * playlist_xpm[];


/* mouse invisible: define the time (in 1/1000 seconds) before mouse goes invisible */


using namespace KMPlayer;

//-------------------------------------------------------------------------

namespace KMPlayer {

class KMPlayerPictureWidget : public QWidget {
    View * m_view;
public:
    KDE_NO_CDTOR_EXPORT KMPlayerPictureWidget (QWidget * parent, View * view)
        : QWidget (parent), m_view (view) {}
    KDE_NO_CDTOR_EXPORT ~KMPlayerPictureWidget () {}
protected:
    void mousePressEvent (QMouseEvent *);
};

} // namespace

KDE_NO_EXPORT void KMPlayerPictureWidget::mousePressEvent (QMouseEvent *) {
    m_view->emitPictureClicked ();
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT TextEdit::TextEdit (QWidget * parent, View * view) : QTextEdit (parent), m_view (view) {
    setReadOnly (true);
    QPalette p=palette();
    p.setColor (QPalette::Active, QPalette::Base, QColor (Qt::black));
    p.setColor (QPalette::Active, QPalette::Foreground, (QColor (0xB2, 0xB2, 0xB2)));
    setPalette (p);
}

KDE_NO_EXPORT void TextEdit::contextMenuEvent (QContextMenuEvent * e) {
    m_view->controlPanel ()->popupMenu->exec (e->globalPos ());
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT InfoWindow::InfoWindow (QWidget * parent, View * view)
 : /*QTextEdit (parent),*/ m_view (view) {
    setReadOnly (true);
    //setLinkUnderline (false);
}

KDE_NO_EXPORT void InfoWindow::contextMenuEvent (QContextMenuEvent * e) {
    m_view->controlPanel ()->popupMenu->exec (e->globalPos ());
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT View::View (QWidget *parent)
  : KMediaPlayer::View (parent),
    m_image (0L),
    m_control_panel (0L),
    m_status_bar (0L),
    m_volume_slider (0L),
    m_mixer_object ("kicker"),
    m_controlpanel_mode (CP_Show),
    m_old_controlpanel_mode (CP_Show),
    m_statusbar_mode (SB_Hide),
    controlbar_timer (0),
    infopanel_timer (0),
    m_keepsizeratio (false),
    m_playing (false),
    m_mixer_init (false),
    m_inVolumeUpdate (false),
    m_tmplog_needs_eol (false),
    m_revert_fullscreen (false),
    m_no_info (false),
    m_edit_mode (false)
{}

KDE_NO_EXPORT void View::dropEvent (QDropEvent * de) {
    KUrl::List uris = KUrl::List::fromMimeData( de->mimeData() );
    if (uris.isEmpty() && Q3TextDrag::canDecode (de)) {
        QString text;
        Q3TextDrag::decode (de, text);
        uris.push_back (KURL (text));
    }
    if (uris.size () > 0) {
        for (int i = 0; i < uris.size (); i++)
            uris [i] = KURL::decode_string (uris [i].url ());
        m_widgetstack->currentWidget ()->setFocus ();
        emit urlDropped (uris);
        de->accept ();
    }
}

KDE_NO_EXPORT void View::dragEnterEvent (QDragEnterEvent* dee) {
    if (isDragValid (dee))
        dee->accept ();
}

KDE_NO_EXPORT void View::init (KActionCollection * action_collection) {
    setAutoFillBackground (false); // prevents flashing
    QPalette pal (QColor (64, 64,64), QColor (32, 32, 32));
    QVBoxLayout * viewbox = new QVBoxLayout;
    viewbox->setContentsMargins (0, 0, 0, 0);
    setLayout (viewbox);
    m_dockarea = new QMainWindow (this);
    m_view_area = new ViewArea (m_dock_video, this);
    m_dockarea->setCentralWidget (m_view_area);
    m_dock_playlist = new QDockWidget (i18n ("Playlist"));
    m_dockarea->addDockWidget (Qt::LeftDockWidgetArea, m_dock_playlist);
    m_playlist = new PlayListView (m_dock_playlist, this, action_collection);
    m_dock_playlist->setWidget (m_playlist);
    viewbox->addWidget (m_dockarea);
    m_widgetstack = new QStackedWidget (m_view_area);
    m_control_panel = new ControlPanel (m_view_area, this);
    m_control_panel->setMaximumSize (2500, controlPanel ()->maximumSize ().height ());
    m_status_bar = new StatusBar (m_view_area);
    m_status_bar->insertItem (QString (""), 0);
    QSize sbsize = m_status_bar->sizeHint ();
    m_status_bar->hide ();
    m_status_bar->setMaximumSize (2500, sbsize.height ());
    m_viewer = new Viewer (m_widgetstack, this);
    m_widgettypes [WT_Video] = m_viewer;
#if KDE_IS_VERSION(3,1,90)
    setVideoWidget (m_view_area);
#endif

    m_multiedit = new TextEdit (m_widgetstack, this);
    QFont fnt = KGlobalSettings::fixedFont ();
    m_multiedit->setFont (fnt);
    m_widgettypes[WT_Console] = m_multiedit;

    m_widgettypes[WT_Picture] = new KMPlayerPictureWidget (m_widgetstack, this);

    m_dock_infopanel = new QDockWidget (i18n ("Information"));
    m_dockarea->addDockWidget (Qt::BottomDockWidgetArea, m_dock_infopanel);
    m_infopanel = new InfoWindow (m_dock_infopanel, this);
    m_dock_infopanel->setWidget (m_infopanel);

    m_widgetstack->addWidget (m_viewer);
    m_widgetstack->addWidget (m_multiedit);
    m_widgetstack->addWidget (m_widgettypes[WT_Picture]);

    setFocusPolicy (Qt::ClickFocus);

    setAcceptDrops (true);
    m_view_area->resizeEvent (0L);

    m_dockarea->setWindowFlags (Qt::SubWindow);
    m_dockarea->show ();
    kapp->installX11EventFilter (this);
}

KDE_NO_CDTOR_EXPORT View::~View () {
    delete m_image;
    if (m_view_area->parent () != this)
        delete m_view_area;
}

KDE_NO_EXPORT void View::setEraseColor (const QColor & color) {
    /*KMediaPlayer::View::setEraseColor (color);
    if (statusBar ()) {
        statusBar ()->setEraseColor (color);
        controlPanel ()->setEraseColor (color);
    }*/
}

void View::setInfoMessage (const QString & msg) {
    bool ismain = m_dockarea->centralWidget () == m_infopanel;
    if (msg.isEmpty ()) {
        if (!ismain && !m_edit_mode && !infopanel_timer)
            infopanel_timer = startTimer (0);
       m_infopanel->clear ();
    } else if (ismain || !m_no_info) {
        if (!ismain && !m_edit_mode && !m_dock_infopanel->isVisible ())
            m_dock_infopanel->show ();
        if (m_edit_mode)
            m_infopanel->setPlainText (msg);
        else
            m_infopanel->setHtml (msg);
    }
}

void View::setStatusMessage (const QString & msg) {
    if (m_statusbar_mode != SB_Hide)
        m_status_bar->changeItem (msg, 0);
}

void View::toggleShowPlaylist () {
    if (m_controlpanel_mode == CP_Only)
        return;
    if (m_dock_playlist->isVisible ())
        m_dock_playlist->hide ();
    else
        m_dock_playlist->show();
    /*if (m_dock_playlist->mayBeShow ()) {
        if (m_dock_playlist->isDockBackPossible ())
            m_dock_playlist->dockBack ();
        else {
            bool horz = true;
            QStyle *style = m_playlist->style ();
            int h = style->pixelMetric (QStyle::PM_ScrollBarExtent, NULL, m_playlist);
            h += style->pixelMetric (QStyle::PM_DockWidgetFrameWidth, NULL, m_playlist);
            h +=style->pixelMetric (QStyle::PM_DockWidgetHandleExtent, NULL, m_playlist);
            for (Q3ListViewItem *i=m_playlist->firstChild();i;i=i->itemBelow()) {
                h += i->height ();
                if (h > int (0.25 * height ())) {
                    horz = false;
                    break;
                }
            }
            int perc = 30;
            if (horz && 100 * h / height () < perc)
                perc = 100 * h / height ();
            m_dock_playlist->manualDock (m_dock_video, horz ? K3DockWidget::DockTop : K3DockWidget::DockLeft, perc);
        }
    } else
        m_dock_playlist->undock ();*/
}

void View::setViewOnly () {
    m_dock_playlist->hide ();
    m_dock_infopanel->hide ();
}

void View::setInfoPanelOnly () {
    /*if (m_dock_playlist->mayBeHide ())
        m_dock_playlist->undock ();
    m_dock_video->setEnableDocking (K3DockWidget::DockCenter);
    m_dock_video->undock ();
    m_dock_infopanel->setEnableDocking (K3DockWidget::DockNone);
    m_dockarea->setMainDockWidget (m_dock_infopanel);*/
}

void View::setPlaylistOnly () {
    /*if (m_dock_infopanel->mayBeHide ())
       m_dock_infopanel->undock ();
    m_dock_video->setEnableDocking (K3DockWidget::DockCenter);
    m_dock_video->undock ();
    m_dock_playlist->setEnableDocking (K3DockWidget::DockNone);
    m_dockarea->setMainDockWidget (m_dock_playlist);*/
}

void View::setEditMode (RootPlayListItem *ri, bool enable) {
    m_edit_mode = enable;
    m_infopanel->setReadOnly (!m_edit_mode);
    if (m_edit_mode && !m_dock_infopanel->isVisible ())
        m_dock_infopanel->show ();
    //if (m_edit_mode && m_dock_infopanel->mayBeShow ())
    //    m_dock_infopanel->manualDock(m_dock_video,K3DockWidget::DockBottom,50);
    m_playlist->showAllNodes (ri, m_edit_mode);
}

bool View::setPicture (const QString & path) {
    delete m_image;
    if (path.isEmpty ())
        m_image = 0L;
    else {
        m_image = new QPixmap (path);
        if (m_image->isNull ()) {
            delete m_image;
            m_image = 0L;
            kDebug() << "View::setPicture failed " << path << endl;
        }
    }
    if (!m_image) {
        m_widgetstack->setCurrentWidget (m_viewer);
    } else {
        QPalette palette;
        palette.setBrush (m_widgettypes[WT_Picture]->backgroundRole(), QBrush (*m_image));
        m_widgettypes[WT_Picture]->setPalette(palette);
        m_widgetstack->setCurrentWidget (m_widgettypes[WT_Picture]);
        setControlPanelMode (CP_AutoHide);
    }
    return m_image;
}

KDE_NO_EXPORT void View::updateVolume () {
    /*if (m_mixer_init && !m_volume_slider)
        return;
    QByteArray data, replydata;
    QCString replyType;
    int volume;
    bool has_mixer = false;
    bool has_mixer = kapp->dcopClient ()->call (m_mixer_object, "Mixer0",
            "masterVolume()", data, replyType, replydata);
    if (!has_mixer) {
        m_mixer_object = "kmix";
        has_mixer = kapp->dcopClient ()->call (m_mixer_object, "Mixer0",
                "masterVolume()", data, replyType, replydata);
    }
    if (has_mixer) {
        QDataStream replystream (replydata, IO_ReadOnly);
        replystream >> volume;
        if (!m_mixer_init) {
            QLabel * mixer_label = new QLabel (i18n ("Volume:"), m_control_panel->popupMenu ());
            m_control_panel->popupMenu ()->insertItem (mixer_label, -1, 4);
            m_volume_slider = new QSlider (0, 100, 10, volume, Qt::Horizontal, m_control_panel->popupMenu ());
            connect(m_volume_slider, SIGNAL(valueChanged(int)), this,SLOT(setVolume(int)));
            m_control_panel->popupMenu ()->insertItem (m_volume_slider, ControlPanel::menu_volume, 5);
            m_control_panel->popupMenu ()->insertSeparator (6);
        } else {
            m_inVolumeUpdate = true;
            m_volume_slider->setValue (volume);
            m_inVolumeUpdate = false;
        }
    } else if (m_volume_slider) {
        m_control_panel->popupMenu ()->removeItemAt (6);
        m_control_panel->popupMenu ()->removeItemAt (5);
        m_control_panel->popupMenu ()->removeItemAt (4);
        m_volume_slider = 0L;
    }*/
    m_mixer_init = true;
}

void View::showWidget (WidgetType wt) {
    m_widgetstack->setCurrentWidget (m_widgettypes [wt]);
    if (m_widgetstack->currentWidget () == m_widgettypes[WT_Console]) {
        addText (QString (""), false);
        if (m_controlpanel_mode == CP_AutoHide && m_playing)
            m_control_panel->show();
    } else
        delayedShowButtons (false);
    updateLayout ();
}

void View::toggleVideoConsoleWindow () {
    WidgetType wt = WT_Console;
    if (m_widgetstack->currentWidget () == m_widgettypes[WT_Console]) {
        wt = WT_Video;
        m_control_panel->videoConsoleAction->setIcon (
                KIconLoader::global ()->loadIconSet (
                    QString ("konsole"), KIconLoader::Small, 0, true));
        m_control_panel->videoConsoleAction->setText (i18n ("Con&sole"));
    } else {
        m_control_panel->videoConsoleAction->setIcon (
                KIconLoader::global ()->loadIconSet (
                    QString ("video"), KIconLoader::Small, 0, true));
        m_control_panel->videoConsoleAction->setText (i18n ("V&ideo"));
    }
    showWidget (wt);
    emit windowVideoConsoleToggled (int (wt));
}

void View::setControlPanelMode (ControlPanelMode m) {
    killTimer (controlbar_timer);
    controlbar_timer = 0L;
    m_old_controlpanel_mode = m_controlpanel_mode = m;
    if (m_playing && isFullScreen())
        m_controlpanel_mode = CP_AutoHide;
    if ((m_controlpanel_mode == CP_Show || m_controlpanel_mode == CP_Only) &&
            !m_control_panel->isVisible ()) {
        m_control_panel->show ();
        m_view_area->resizeEvent (0L);
    } else if (m_controlpanel_mode == CP_AutoHide) {
        if ((m_playing &&
                m_widgetstack->currentWidget () != m_widgettypes[WT_Console]))
            delayedShowButtons (false);
        else if (!m_control_panel->isVisible ()) {
            m_control_panel->show ();
            m_view_area->resizeEvent (0L);
        }
    } else if (m_controlpanel_mode == CP_Hide && m_control_panel->isVisible()) {
        m_control_panel->hide ();
        m_view_area->resizeEvent (0L);
    }
}

void View::setStatusBarMode (StatusBarMode m) {
    m_statusbar_mode = m;
    if (m == SB_Hide)
        m_status_bar->hide ();
    else
        m_status_bar->show ();
    m_view_area->resizeEvent (0L);
}

KDE_NO_EXPORT void View::delayedShowButtons (bool show) {
    if ((show && m_control_panel->isVisible ()) ||
            (!show && !m_control_panel->isVisible ())) {
        if (controlbar_timer) {
            killTimer (controlbar_timer);
            controlbar_timer = 0;
        }
        if (!show)
            m_control_panel->hide (); // for initial race
    } else if (m_controlpanel_mode == CP_AutoHide &&
            (m_playing ||
             m_widgetstack->currentWidget () == m_widgettypes[WT_Picture]) &&
            m_widgetstack->currentWidget () != m_widgettypes[WT_Console] &&
            !controlbar_timer) {
        controlbar_timer = startTimer (500);
    }
}

KDE_NO_EXPORT void View::setVolume (int vol) {
    if (m_inVolumeUpdate) return;
    //QByteArray data;
    //QDataStream arg( data, IO_WriteOnly );
    //arg << vol;
    //if (!kapp->dcopClient()->send (m_mixer_object, "Mixer0", "setMasterVolume(int)", data))
    //    kWarning() << "Failed to update volume" << endl;
}

KDE_NO_EXPORT void View::updateLayout () {
    if (m_controlpanel_mode == CP_Only)
        m_control_panel->setMaximumSize (2500, height ());
    m_view_area->resizeEvent (0L);
}

void View::setKeepSizeRatio (bool b) {
    if (m_keepsizeratio != b) {
        m_keepsizeratio = b;
        updateLayout ();
        m_view_area->update ();
    }
}

KDE_NO_EXPORT void View::timerEvent (QTimerEvent * e) {
    if (e->timerId () == controlbar_timer) {
        controlbar_timer = 0;
        if (m_playing ||
                m_widgetstack->currentWidget () == m_widgettypes[WT_Picture]) {
            int vert_buttons_pos = m_view_area->height()-statusBarHeight ();
            QPoint mouse_pos = m_view_area->mapFromGlobal (QCursor::pos ());
            int cp_height = m_control_panel->maximumSize ().height ();
            bool mouse_on_buttons = (//m_view_area->hasMouse () && 
                    mouse_pos.y () >= vert_buttons_pos-cp_height &&
                    mouse_pos.y ()<= vert_buttons_pos &&
                    mouse_pos.x () > 0 &&
                    mouse_pos.x () < m_control_panel->width());
            if (mouse_on_buttons && !m_control_panel->isVisible ()) {
                m_control_panel->show ();
                m_view_area->resizeEvent (0L);
            } else if (!mouse_on_buttons && m_control_panel->isVisible ()) {
                m_control_panel->hide ();
                m_view_area->resizeEvent (0L);
            }
        }
    } else if (e->timerId () == infopanel_timer) {
        if (m_infopanel->document ()->isEmpty ())
            m_dock_infopanel->hide ();
            //m_dock_infopanel->undock ();
        infopanel_timer  = 0;
    }
    killTimer (e->timerId ());
}

void View::addText (const QString & str, bool eol) {
    if (m_tmplog_needs_eol)
        tmplog += QChar ('\n');
    tmplog += str;
    m_tmplog_needs_eol = eol;
    if (m_widgetstack->currentWidget () != m_widgettypes[WT_Console] &&
            tmplog.length () < 7500)
        return;
    if (eol) {
        if (m_multiedit->document ()->isEmpty ())
            m_multiedit->setPlainText (tmplog);
        else
            m_multiedit->append (tmplog);
        tmplog.truncate (0);
        m_tmplog_needs_eol = false;
    } else {
        int pos = tmplog.lastIndexOf (QChar ('\n'));
        if (pos >= 0) {
            m_multiedit->append (tmplog.left (pos));
            tmplog = tmplog.mid (pos+1);
        }
    }
    QTextCursor cursor = m_multiedit->textCursor ();
    cursor.movePosition (QTextCursor::End);
    cursor.movePosition (QTextCursor::PreviousBlock, QTextCursor::MoveAnchor, 5000);
    cursor.movePosition (QTextCursor::Start, QTextCursor::KeepAnchor);
    cursor.removeSelectedText ();
    cursor.movePosition (QTextCursor::End);
    m_multiedit->setTextCursor (cursor);
}

KDE_NO_EXPORT void View::videoStart () {
    if (m_dockarea->centralWidget () != m_view_area) {
        // restore from an info or playlist only setting
        m_dockarea->setCentralWidget (m_view_area);
    }
    if (m_controlpanel_mode == CP_Only) {
        m_control_panel->setMaximumSize(2500, controlPanel()->preferedHeight());
        setControlPanelMode (CP_Show);
    }
}

KDE_NO_EXPORT void View::playingStart () {
    if (m_playing) return; //FIXME: make symetric with playingStop
    if (m_widgetstack->currentWidget () == m_widgettypes[WT_Picture])
        m_widgetstack->setCurrentWidget (m_viewer);
    m_playing = true;
    m_revert_fullscreen = !isFullScreen();
    setControlPanelMode (m_old_controlpanel_mode);
}

KDE_NO_EXPORT void View::playingStop () {
    if (m_controlpanel_mode == CP_AutoHide &&
            m_widgetstack->currentWidget () != m_widgettypes[WT_Picture]) {
        m_control_panel->show ();
        //m_view_area->setMouseTracking (false);
    }
    killTimer (controlbar_timer);
    controlbar_timer = 0;
    m_playing = false;
    WId w = m_viewer->clientWinId ();
    if (w)
        XClearWindow (QX11Info::display(), w);
    m_view_area->resizeEvent (0L);
}

KDE_NO_EXPORT void View::leaveEvent (QEvent *) {
    delayedShowButtons (false);
}

KDE_NO_EXPORT void View::reset () {
    if (m_revert_fullscreen && isFullScreen ())
        m_control_panel->fullscreenAction->activate (QAction::Trigger);
        //m_view_area->fullScreen ();
    playingStop ();
    m_viewer->show ();
}

bool View::isFullScreen () const {
    return m_view_area->isFullScreen ();
}

void View::fullScreen () {
    if (!m_view_area->isFullScreen()) {
        m_sreensaver_disabled = false;
        /*QByteArray data, replydata;
        QCString replyType;
        if (kapp->dcopClient ()->call ("kdesktop", "KScreensaverIface",
                    "isEnabled()", data, replyType, replydata)) {
            bool enabled;
            QDataStream replystream (replydata, IO_ReadOnly);
            replystream >> enabled;
            if (enabled)
                m_sreensaver_disabled = kapp->dcopClient()->send
                    ("kdesktop", "KScreensaverIface", "enable(bool)", "false");
        }*/
        //if (m_keepsizeratio && m_viewer->aspect () < 0.01)
        //    m_viewer->setAspect (1.0 * m_viewer->width() / m_viewer->height());
        m_view_area->fullScreen();
        m_control_panel->zoomAction->setVisible (false);
        m_widgetstack->currentWidget ()->setFocus ();
    } else {
       // if (m_sreensaver_disabled)
       //     m_sreensaver_disabled = !kapp->dcopClient()->send
       //         ("kdesktop", "KScreensaverIface", "enable(bool)", "true");
        m_view_area->fullScreen();
        m_control_panel->zoomAction->setVisible (true);
    }
    setControlPanelMode (m_old_controlpanel_mode);
    emit fullScreenChanged ();
}

KDE_NO_EXPORT int View::statusBarHeight () const {
    if (statusBar()->isVisible () && !viewArea()->isFullScreen ()) {
        if (statusBarMode () == SB_Only)
            return height ();
        else
            return statusBar()->maximumSize ().height ();
    }
    return 0;
}

bool View::x11Event (XEvent * e) {
    switch (e->type) {
        case UnmapNotify:
            if (e->xunmap.event == m_viewer->clientWinId ()) {
                videoStart ();
                //hide();
            }
            break;
        case XKeyPress:
            if (e->xkey.window == m_viewer->clientWinId ()) {
                KeySym ksym;
                char kbuf[16];
                XLookupString (&e->xkey, kbuf, sizeof(kbuf), &ksym, NULL);
                switch (ksym) {
                    case XK_f:
                    case XK_F:
                        //fullScreen ();
                        break;
                };
            }
            break;
        /*case ColormapNotify:
            fprintf (stderr, "colormap notify\n");
            return true;*/
        case MotionNotify:
            if (e->xmotion.window == m_viewer->clientWinId ())
                delayedShowButtons (e->xmotion.y > m_view_area->height () -
                        statusBarHeight () -
                        m_control_panel->maximumSize ().height ());
            m_view_area->mouseMoved ();
            break;
        case MapNotify:
            if (e->xmap.event == m_viewer->clientWinId ()) {
                show ();
                QTimer::singleShot (10, m_viewer, SLOT (sendConfigureEvent ()));
            }
            break;
        /*case ConfigureNotify:
            break;
            //return true;*/
        default:
            break;
    }
    return false;
}

//----------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT Viewer::Viewer (QWidget *parent, View * view)
  : QX11EmbedContainer (parent), m_plain_window (0), m_bgcolor (0), m_aspect (0.0),
    m_view (view) {
    /*XWindowAttributes xwa;
    XGetWindowAttributes (QX11Info::display(), winId (), &xwa);
    XSetWindowAttributes xswa;
    xswa.background_pixel = 0;
    xswa.border_pixel = 0;
    xswa.colormap = xwa.colormap;
    create (XCreateWindow (QX11Info::display()), parent->winId (), 0, 0, 10, 10, 0, 
                           x11Depth (), InputOutput, (Visual*)x11Visual (),
                           CWBackPixel | CWBorderPixel | CWColormap, &xswa));*/
    setAcceptDrops (true);
    connect (this, SIGNAL (clientIsEmbedded ()), this, SLOT (embedded ()));
    //setProtocol (QXEmbed::XPLAIN);
}

KDE_NO_CDTOR_EXPORT Viewer::~Viewer () {
}

KDE_NO_EXPORT void Viewer::setIntermediateWindow (bool set) {
    kDebug () << "setIntermediateWindow " << !!m_plain_window << "->" << set << endl;
    if (!clientWinId () || !!m_plain_window != set) {
        if (set) {
            if (!m_plain_window) {
                int scr = DefaultScreen (QX11Info::display ());
                m_plain_window = XCreateSimpleWindow (
                        QX11Info::display(),
                        m_view->winId (),
                        0, 0, width(), height(),
                        1,
                        BlackPixel (QX11Info::display(), scr),
                        BlackPixel (QX11Info::display(), scr));
                embedClient (m_plain_window);
            }
            XClearWindow (QX11Info::display(), m_plain_window);
        } else {
            if (m_plain_window) {
                XUnmapWindow (QX11Info::display(), m_plain_window);
                XFlush (QX11Info::display());
                discardClient ();
                XDestroyWindow (QX11Info::display(), m_plain_window);
                m_plain_window = 0;
                //XSync (QX11Info::display (), false);
            }
        }
    }
}

KDE_NO_EXPORT void Viewer::embedded () {
    kDebug () << "windowChanged " << (int)clientWinId () << endl;
    if (clientWinId () && m_plain_window)
        XSelectInput (QX11Info::display (), clientWinId (),
                //KeyPressMask | KeyReleaseMask |
                KeyPressMask |
                //EnterWindowMask | LeaveWindowMask |
                //FocusChangeMask |
                ExposureMask |
                StructureNotifyMask |
                PointerMotionMask);
}

KDE_NO_EXPORT void Viewer::mouseMoveEvent (QMouseEvent * e) {
    if (e->buttons () == Qt::NoButton) {
        int cp_height = m_view->controlPanel ()->maximumSize ().height ();
        m_view->delayedShowButtons (e->y () > height () - cp_height);
    }
    m_view->viewArea ()->mouseMoved ();
}

void Viewer::setAspect (float a) {
    m_aspect = a;
}

KDE_NO_EXPORT int Viewer::heightForWidth (int w) const {
    if (m_aspect <= 0.01)
        return 0;
    return int (w/m_aspect);
}

KDE_NO_EXPORT void Viewer::dropEvent (QDropEvent * de) {
    m_view->dropEvent (de);
}

KDE_NO_EXPORT void Viewer::dragEnterEvent (QDragEnterEvent* dee) {
    m_view->dragEnterEvent (dee);
}
/*
*/
void Viewer::sendKeyEvent (int key) {
    WId w = clientWinId ();
    if (w) {
        char buf[2] = { char (key), '\0' };
        KeySym keysym = XStringToKeysym (buf);
        XKeyEvent event = {
            XKeyPress, 0, true,
            QX11Info::display (), w, QX11Info::appRootWindow(), w,
            /*time*/ 0, 0, 0, 0, 0,
            0, XKeysymToKeycode (QX11Info::display (), keysym), true
        };
        XSendEvent (QX11Info::display(), w, false, KeyPressMask, (XEvent *) &event);
        XFlush (QX11Info::display ());
    }
}

KDE_NO_EXPORT void Viewer::sendConfigureEvent () {
    WId w = clientWinId ();
    if (w) {
        XConfigureEvent c = {
            ConfigureNotify, 0UL, True,
            QX11Info::display (), w, winId (),
            x (), y (), width (), height (),
            0, None, False
        };
        XSendEvent(QX11Info::display(),c.event,true,StructureNotifyMask,(XEvent*)&c);
        XFlush (QX11Info::display ());
    }
}

KDE_NO_EXPORT void Viewer::contextMenuEvent (QContextMenuEvent * e) {
    m_view->controlPanel ()->popupMenu->exec (e->globalPos ());
}

KDE_NO_EXPORT void Viewer::setBackgroundColor (const QColor & c) {
    if (m_bgcolor != c.rgb ()) {
        m_bgcolor = c.rgb ();
        setCurrentBackgroundColor (c);
    }
}

KDE_NO_EXPORT void Viewer::resetBackgroundColor () {
    setCurrentBackgroundColor (m_bgcolor);
}

KDE_NO_EXPORT void Viewer::setCurrentBackgroundColor (const QColor & c) {
    QPalette palette;
    palette.setColor (backgroundRole(), c);
    setPalette (palette);
    WId w = clientWinId ();
    if (w) {
        XSetWindowBackground (QX11Info::display (), w, c.rgb ());
        XFlush (QX11Info::display ());
    }
}

#include "kmplayerview.moc"
