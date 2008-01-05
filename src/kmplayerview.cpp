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

#include <config.h>
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
#include <qcursor.h>
#include <qclipboard.h>

#include <kiconloader.h>
#include <kstatusbar.h>
#include <kdebug.h>
#include <klocale.h>
#include <kapplication.h>
#include <kactioncollection.h>
#include <kstdaction.h>
#include <kshortcut.h>
#include <kurldrag.h>
#include <kfinddialog.h>
#include <dcopclient.h>
#include <kglobalsettings.h>
#include <kstaticdeleter.h>

#include "kmplayerview.h"
#include "kmplayercontrolpanel.h"
#include "kmplayersource.h"
#include "playlistview.h"
#include "viewarea.h"

extern const char * normal_window_xpm[];
extern const char * playlist_xpm[];


/* mouse invisible: define the time (in 1/1000 seconds) before mouse goes invisible */


using namespace KMPlayer;

//-------------------------------------------------------------------------

namespace KMPlayer {

class KMPLAYER_NO_EXPORT PictureWidget : public QWidget {
    View * m_view;
public:
    KDE_NO_CDTOR_EXPORT PictureWidget (QWidget * parent, View * view)
        : QWidget (parent), m_view (view) {}
    KDE_NO_CDTOR_EXPORT ~PictureWidget () {}
protected:
    void mousePressEvent (QMouseEvent *);
};

} // namespace

KDE_NO_EXPORT void PictureWidget::mousePressEvent (QMouseEvent *) {
    m_view->emitPictureClicked ();
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT TextEdit::TextEdit (QWidget * parent, View * view) : QTextEdit (parent, "kde_kmplayer_console"), m_view (view) {
    setReadOnly (true);
    setPaper (QBrush (QColor (0, 0, 0)));
    setColor (QColor (0xB2, 0xB2, 0xB2));
}

KDE_NO_EXPORT void TextEdit::contextMenuEvent (QContextMenuEvent * e) {
    m_view->controlPanel ()->popupMenu ()->exec (e->globalPos ());
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT InfoWindow::InfoWindow (QWidget * parent, View * view) : QTextEdit (parent, "kde_kmplayer_console"), m_view (view) {
    setReadOnly (true);
    setLinkUnderline (false);
}

KDE_NO_EXPORT void InfoWindow::contextMenuEvent (QContextMenuEvent * e) {
    m_view->controlPanel ()->popupMenu ()->exec (e->globalPos ());
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT View::View (QWidget *parent, const char *name)
  : KMediaPlayer::View (parent, name),
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
    KURL::List sl;
    if (KURLDrag::canDecode (de)) {
        KURLDrag::decode (de, sl);
    } else if (QTextDrag::canDecode (de)) {
        QString text;
        QTextDrag::decode (de, text);
        sl.push_back (KURL (text));
    }
    if (sl.size () > 0) {
        for (unsigned i = 0; i < sl.size (); i++)
            sl [i] = KURL::decode_string (sl [i].url ());
        //m_widgetstack->visibleWidget ()->setFocus ();
        emit urlDropped (sl);
        de->accept ();
    }
}

KDE_NO_EXPORT void View::dragEnterEvent (QDragEnterEvent* dee) {
    if (isDragValid (dee))
        dee->accept ();
}

KDE_NO_EXPORT void View::init (KActionCollection * action_collection) {
    setBackgroundMode(Qt::NoBackground); // prevents flashing
    //m_dockarea->setEraseColor (QColor (0, 0, 0));
    QPalette pal (QColor (64, 64,64), QColor (32, 32, 32));
    QVBoxLayout * viewbox = new QVBoxLayout (this, 0, 0);
    m_dockarea = new KDockArea (this, "kde_kmplayer_dock_area");
    m_dock_video = new KDockWidget (m_dockarea->manager (), 0, KGlobal::iconLoader ()->loadIcon (QString ("kmplayer"), KIcon::Small), m_dockarea);
    m_dock_video->setEraseColor (QColor (0, 0, 255));
    m_dock_video->setDockSite (KDockWidget::DockLeft | KDockWidget::DockBottom | KDockWidget::DockRight | KDockWidget::DockTop);
    m_dock_video->setEnableDocking(KDockWidget::DockNone);
    m_view_area = new ViewArea (m_dock_video, this);
    m_dock_video->setWidget (m_view_area);
    m_dockarea->setMainDockWidget (m_dock_video);
    m_dock_playlist = m_dockarea->createDockWidget (i18n ("Play List"), KGlobal::iconLoader ()->loadIcon (QString ("player_playlist"), KIcon::Small));
    m_playlist = new PlayListView (m_dock_playlist, this, action_collection);
    m_dock_playlist->setWidget (m_playlist);
    viewbox->addWidget (m_dockarea);
    m_control_panel = new ControlPanel (m_view_area, this);
    m_control_panel->setMaximumSize (2500, controlPanel ()->maximumSize ().height ());
    m_status_bar = new StatusBar (m_view_area);
    m_status_bar->insertItem (QString (""), 0);
    QSize sbsize = m_status_bar->sizeHint ();
    m_status_bar->hide ();
    m_status_bar->setMaximumSize (2500, sbsize.height ());
#if KDE_IS_VERSION(3,1,90)
    setVideoWidget (m_view_area);
#endif

    m_multiedit = new TextEdit (m_view_area, this);
    m_multiedit->setTextFormat (Qt::PlainText);
    QFont fnt = KGlobalSettings::fixedFont ();
    m_multiedit->setFont (fnt);
    m_multiedit->hide ();

    m_picture = new PictureWidget (m_view_area, this);
    m_picture->hide ();

    m_dock_infopanel = m_dockarea->createDockWidget ("infopanel", KGlobal::iconLoader ()->loadIcon (QString ("info"), KIcon::Small));
    m_infopanel = new InfoWindow (m_dock_infopanel, this);
    m_dock_infopanel->setWidget (m_infopanel);

    setFocusPolicy (QWidget::ClickFocus);

    setAcceptDrops (true);
    m_view_area->resizeEvent (0L);
}

KDE_NO_CDTOR_EXPORT View::~View () {
    delete m_image;
    if (m_view_area->parent () != this)
        delete m_view_area;
}

KDE_NO_EXPORT void View::setEraseColor (const QColor & color) {
    KMediaPlayer::View::setEraseColor (color);
    if (statusBar ()) {
        statusBar ()->setEraseColor (color);
        controlPanel ()->setEraseColor (color);
    }
}

void View::setInfoMessage (const QString & msg) {
    bool ismain = m_dockarea->getMainDockWidget () == m_dock_infopanel;
    if (msg.isEmpty ()) {
        if (!ismain && !m_edit_mode && !infopanel_timer)
            infopanel_timer = startTimer (0);
       m_infopanel->clear ();
    } else if (ismain || !m_no_info) {
        if (!m_edit_mode && m_dock_infopanel->mayBeShow ())
          m_dock_infopanel->manualDock(m_dock_video,KDockWidget::DockBottom,80);
        m_infopanel->setText (msg);
    }
}

void View::setStatusMessage (const QString & msg) {
    if (m_statusbar_mode != SB_Hide)
        m_status_bar->changeItem (msg, 0);
}

void View::toggleShowPlaylist () {
    if (m_controlpanel_mode == CP_Only)
        return;
    if (m_dock_playlist->mayBeShow ()) {
        if (m_dock_playlist->isDockBackPossible ())
            m_dock_playlist->dockBack ();
        else {
            bool horz = true;
            QStyle & style = m_playlist->style ();
            int h = style.pixelMetric (QStyle::PM_ScrollBarExtent, m_playlist);
            h += style.pixelMetric(QStyle::PM_DockWindowFrameWidth, m_playlist);
            h +=style.pixelMetric(QStyle::PM_DockWindowHandleExtent,m_playlist);
            for (QListViewItem *i=m_playlist->firstChild();i;i=i->itemBelow()) {
                h += i->height ();
                if (h > int (0.25 * height ())) {
                    horz = false;
                    break;
                }
            }
            int perc = 30;
            if (horz && 100 * h / height () < perc)
                perc = 100 * h / height ();
            m_dock_playlist->manualDock (m_dock_video, horz ? KDockWidget::DockTop : KDockWidget::DockLeft, perc);
        }
    } else
        m_dock_playlist->undock ();
}

void View::setViewOnly () {
    if (m_dock_playlist->mayBeHide ())
        m_dock_playlist->undock ();
    if (m_dock_infopanel->mayBeHide ())
       m_dock_infopanel->undock ();
}

void View::setInfoPanelOnly () {
    if (m_dock_playlist->mayBeHide ())
        m_dock_playlist->undock ();
    m_dock_video->setEnableDocking (KDockWidget::DockCenter);
    m_dock_video->undock ();
    m_dock_infopanel->setEnableDocking (KDockWidget::DockNone);
    m_dockarea->setMainDockWidget (m_dock_infopanel);
}

void View::setPlaylistOnly () {
    if (m_dock_infopanel->mayBeHide ())
       m_dock_infopanel->undock ();
    m_dock_video->setEnableDocking (KDockWidget::DockCenter);
    m_dock_video->undock ();
    m_dock_playlist->setEnableDocking (KDockWidget::DockNone);
    m_dockarea->setMainDockWidget (m_dock_playlist);
}

void View::setEditMode (RootPlayListItem *ri, bool enable) {
    m_edit_mode = enable;
    m_infopanel->setReadOnly (!m_edit_mode);
    m_infopanel->setTextFormat (enable ? Qt::PlainText : Qt::AutoText);
    if (m_edit_mode && m_dock_infopanel->mayBeShow ())
        m_dock_infopanel->manualDock(m_dock_video,KDockWidget::DockBottom,50);
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
            kdDebug() << "View::setPicture failed " << path << endl;
        }
    }
    if (!m_image) {
        m_view_area->setVideoWidgetVisible (true);
        m_picture->hide ();
    } else {
        m_picture->setPaletteBackgroundPixmap (*m_image);
        m_view_area->setVideoWidgetVisible (false);
        m_picture->show ();
        setControlPanelMode (CP_AutoHide);
    }
    return m_image;
}

KDE_NO_EXPORT void View::updateVolume () {
    if (m_mixer_init && !m_volume_slider)
        return;
    QByteArray data, replydata;
    QCString replyType;
    int volume;
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
    }
    m_mixer_init = true;
}

void View::toggleVideoConsoleWindow () {
    if (m_multiedit->isVisible ()) {
        m_multiedit->hide ();
        m_view_area->setVideoWidgetVisible (true);
        m_control_panel->popupMenu ()->changeItem (ControlPanel::menu_video, KGlobal::iconLoader ()->loadIconSet (QString ("konsole"), KIcon::Small, 0, true), i18n ("Con&sole"));
        delayedShowButtons (false);
    } else {
        m_control_panel->popupMenu ()->changeItem (ControlPanel::menu_video, KGlobal::iconLoader ()->loadIconSet (QString ("video"), KIcon::Small, 0, true), i18n ("V&ideo"));
        m_multiedit->show ();
        m_view_area->setVideoWidgetVisible (false);
        addText (QString (""), false);
        if (m_controlpanel_mode == CP_AutoHide && m_playing)
            m_control_panel->show();
    }
    updateLayout ();
    emit windowVideoConsoleToggled (m_multiedit->isVisible ());
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
        if (m_playing && !m_multiedit->isVisible ())
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
            (m_playing || m_picture->isVisible ()) &&
            !m_multiedit->isVisible () && !controlbar_timer) {
        controlbar_timer = startTimer (500);
    }
}

KDE_NO_EXPORT void View::setVolume (int vol) {
    if (m_inVolumeUpdate) return;
    QByteArray data;
    QDataStream arg( data, IO_WriteOnly );
    arg << vol;
    if (!kapp->dcopClient()->send (m_mixer_object, "Mixer0", "setMasterVolume(int)", data))
        kdWarning() << "Failed to update volume" << endl;
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
        if (m_playing || m_picture->isVisible ()) {
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
        if (m_infopanel->text ().isEmpty ())
            m_dock_infopanel->undock ();
        infopanel_timer  = 0;
    }
    killTimer (e->timerId ());
}

void View::addText (const QString & str, bool eol) {
    if (m_tmplog_needs_eol)
        tmplog += QChar ('\n');
    tmplog += str;
    m_tmplog_needs_eol = eol;
    if (!m_multiedit->isVisible () && tmplog.length () < 7500)
        return;
    if (eol) {
        m_multiedit->append (tmplog);
        tmplog.truncate (0);
        m_tmplog_needs_eol = false;
    } else {
        int pos = tmplog.findRev (QChar ('\n'));
        if (pos >= 0) {
            m_multiedit->append (tmplog.left (pos));
            tmplog = tmplog.mid (pos+1);
        }
    }
    int p = m_multiedit->paragraphs ();
    if (5000 < p) {
        m_multiedit->setSelection (0, 0, p - 4499, 0);
        m_multiedit->removeSelectedText ();
    }
    m_multiedit->setCursorPosition (m_multiedit->paragraphs () - 1, 0);
}

/* void View::print (QPrinter *pPrinter)
{
    QPainter printpainter;
    printpainter.begin (pPrinter);

    // TODO: add your printing code here

    printpainter.end ();
}*/

KDE_NO_EXPORT void View::videoStart () {
    if (!isFullScreen () && m_dockarea->getMainDockWidget () != m_dock_video) {
        // restore from an info or playlist only setting
        KDockWidget * dw = m_dockarea->getMainDockWidget ();
        dw->setEnableDocking (KDockWidget::DockCenter);
        dw->undock ();
        m_dock_video->setEnableDocking (KDockWidget::DockNone);
        m_dockarea->setMainDockWidget (m_dock_video);
        m_view_area->resizeEvent (0L);
    }
    if (m_controlpanel_mode == CP_Only) {
        m_control_panel->setMaximumSize(2500, controlPanel()->preferedHeight());
        setControlPanelMode (CP_Show);
    }
}

KDE_NO_EXPORT void View::playingStart () {
    if (m_playing) return; //FIXME: make symetric with playingStop
    if (m_picture->isVisible ()) {
        m_picture->hide ();
        m_view_area->setVideoWidgetVisible (true);
    }
    m_playing = true;
    m_revert_fullscreen = !isFullScreen();
    setControlPanelMode (m_old_controlpanel_mode);
}

KDE_NO_EXPORT void View::playingStop () {
    if (m_controlpanel_mode == CP_AutoHide && !m_picture->isVisible ()) {
        m_control_panel->show ();
        //m_view_area->setMouseTracking (false);
    }
    killTimer (controlbar_timer);
    controlbar_timer = 0;
    m_playing = false;
    m_view_area->resizeEvent (0L);
}

KDE_NO_EXPORT void View::leaveEvent (QEvent *) {
    delayedShowButtons (false);
}

KDE_NO_EXPORT void View::reset () {
    if (m_revert_fullscreen && isFullScreen())
        m_control_panel->popupMenu ()->activateItemAt (m_control_panel->popupMenu ()->indexOf (ControlPanel::menu_fullscreen));
        //m_view_area->fullScreen ();
    playingStop ();
}

bool View::isFullScreen () const {
    return m_view_area->isFullScreen ();
}

void View::fullScreen () {
    if (!m_view_area->isFullScreen()) {
        m_sreensaver_disabled = false;
        QByteArray data, replydata;
        QCString replyType;
        if (kapp->dcopClient ()->call ("kdesktop", "KScreensaverIface",
                    "isEnabled()", data, replyType, replydata)) {
            bool enabled;
            QDataStream replystream (replydata, IO_ReadOnly);
            replystream >> enabled;
            if (enabled)
                m_sreensaver_disabled = kapp->dcopClient()->send
                    ("kdesktop", "KScreensaverIface", "enable(bool)", "false");
        }
        //if (m_keepsizeratio && m_viewer->aspect () < 0.01)
        //    m_viewer->setAspect (1.0 * m_viewer->width() / m_viewer->height());
        m_view_area->fullScreen();
        m_control_panel->popupMenu ()->setItemVisible (ControlPanel::menu_zoom, false);
        //if (m_viewer->isVisible ())
        //    m_viewer->setFocus ();
    } else {
        if (m_sreensaver_disabled)
            m_sreensaver_disabled = !kapp->dcopClient()->send
                ("kdesktop", "KScreensaverIface", "enable(bool)", "true");
        m_view_area->fullScreen();
        m_control_panel->popupMenu ()->setItemVisible (ControlPanel::menu_zoom, true);
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

#include "kmplayerview.moc"
