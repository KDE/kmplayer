/*
    SPDX-FileCopyrightText: 2002-2003 Koos Vriezen <koos.vriezen@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include <cstdio>
#include <cmath>

#include "config-kmplayer.h"
// include files for Qt
#include <QStyle>
#include <QTimer>
#include <QPainter>
#include <QMetaObject>
#include <QLayout>
#include <QPixmap>
#include <QTextEdit>
#include <QToolTip>
#include <QApplication>
#include <QCursor>
#include <QKeySequence>
#include <QSlider>
#include <QLabel>
#include <QDataStream>
#include <QContextMenuEvent>
#include <QMimeData>
#include <QDropEvent>
#include <QAction>
#include <QDragEnterEvent>
#include <QFontDatabase>
#include <QTextDocument>
#include <QTextCursor>
#include <QCursor>
#include <QClipboard>
#include <QMainWindow>
#include <QStatusBar>
#include <QDockWidget>

#include <KIconLoader>
#include <KLocalizedString>
#include <KActionCollection>

#include "kmplayercommon_log.h"
#include "kmplayerview.h"
#include "kmplayercontrolpanel.h"
#include "playlistview.h"
#include "viewarea.h"

/* mouse invisible: define the time (in 1/1000 seconds) before mouse goes invisible */


using namespace KMPlayer;

//-------------------------------------------------------------------------

PictureWidget::PictureWidget (QWidget * parent, View * view)
 : QWidget (parent), m_view (view) {
    setAutoFillBackground (true);
}

void PictureWidget::mousePressEvent (QMouseEvent *) {
    m_view->emitPictureClicked ();
}

void PictureWidget::mouseMoveEvent (QMouseEvent *e) {
    if (e->buttons () == Qt::NoButton)
        m_view->mouseMoved (e->x (), e->y ());
}

//-----------------------------------------------------------------------------

TextEdit::TextEdit (QWidget * parent, View * view) : QTextEdit (parent), m_view (view) {
    setAttribute (Qt::WA_NativeWindow);
    setAttribute(Qt::WA_DontCreateNativeAncestors);
    setReadOnly (true);
    QPalette p=palette();
    p.setColor (QPalette::Active, QPalette::Base, QColor (Qt::black));
    p.setColor (QPalette::Active, QPalette::Foreground, (QColor (0xB2, 0xB2, 0xB2)));
    setPalette (p);
}

void TextEdit::contextMenuEvent (QContextMenuEvent * e) {
    m_view->controlPanel ()->popupMenu->exec (e->globalPos ());
}

//-----------------------------------------------------------------------------

InfoWindow::InfoWindow (QWidget *, View * view)
 : m_view (view) {
    setReadOnly (true);
    //setLinkUnderline (false);
}

void InfoWindow::contextMenuEvent (QContextMenuEvent * e) {
    m_view->controlPanel ()->popupMenu->exec (e->globalPos ());
}

//-----------------------------------------------------------------------------

View::View (QWidget *parent)
  : KMediaPlayer::View (parent),
    m_control_panel (nullptr),
    m_status_bar (nullptr),
    m_controlpanel_mode (CP_Show),
    m_old_controlpanel_mode (CP_Show),
    m_statusbar_mode (SB_Hide),
    controlbar_timer (0),
    infopanel_timer (0),
    m_restore_state_timer(0),
    m_powerManagerStopSleep( -1 ),
    m_inhibitIface("org.freedesktop.PowerManagement.Inhibit",
                   "/org/freedesktop/PowerManagement/Inhibit",
                   QDBusConnection::sessionBus()),
    m_keepsizeratio (false),
    m_playing (false),
    m_tmplog_needs_eol (false),
    m_revert_fullscreen (false),
    m_no_info (false),
    m_edit_mode (false)
{
    setAttribute (Qt::WA_NoSystemBackground, true);
    setAutoFillBackground (false);
    setAcceptDrops(true);
}

void View::dropEvent (QDropEvent * de) {
    QList<QUrl> uris = de->mimeData()->urls();
    if (uris.isEmpty() || !uris[0].isValid()) {
        const QUrl url = QUrl::fromUserInput(de->mimeData()->text());
        if (url.isValid ())
            uris.push_back(url);
    }
    if (uris.size () > 0) {
        //m_widgetstack->currentWidget ()->setFocus ();
        Q_EMIT urlDropped (uris);
        de->accept ();
    }
}

void View::dragEnterEvent (QDragEnterEvent* dee) {
    if (m_playlist->isDragValid (dee))
        dee->accept ();
}

void View::initDock (QWidget *central) {
    m_dockarea = new QMainWindow;
    m_dockarea->setDockNestingEnabled(true);
    m_dockarea->setCentralWidget (central);
    central->setVisible (true);

    m_dock_playlist = new QDockWidget (i18n ("Playlist"));
    if (central != m_playlist)
        m_dock_playlist->setWidget (m_playlist);
    m_dock_playlist->setObjectName ("playlist");

    m_dock_infopanel = new QDockWidget (i18n ("Information"));
    if (central != m_infopanel)
        m_dock_infopanel->setWidget (m_infopanel);
    m_dock_infopanel->setObjectName ("infopanel");

    m_dock_playlist->hide ();
    m_dock_infopanel->hide ();

    m_dockarea->addDockWidget (Qt::BottomDockWidgetArea, m_dock_infopanel);
    m_dockarea->addDockWidget (Qt::LeftDockWidgetArea, m_dock_playlist);

    layout ()->addWidget (m_dockarea);

    m_dockarea->setWindowFlags (Qt::SubWindow);
    m_dockarea->show ();

    m_view_area->resizeEvent (nullptr);
}

void View::init (KActionCollection *action_collection, bool transparent) {
    QVBoxLayout * viewbox = new QVBoxLayout;
    viewbox->setContentsMargins (0, 0, 0, 0);
    setLayout (viewbox);
    m_view_area = new ViewArea (nullptr, this, !transparent);
    m_playlist = new PlayListView (nullptr, this, action_collection);

    m_picture = new PictureWidget (m_view_area, this);
    m_picture->hide ();
    m_control_panel = new ControlPanel (m_view_area, this);
    m_control_panel->setMaximumHeight(controlPanel ()->maximumSize ().height ());
    m_status_bar = new StatusBar (m_view_area);
    m_status_bar->clearMessage();
    m_status_bar->setAutoFillBackground (true);
    QSize sbsize = m_status_bar->sizeHint ();
    m_status_bar->hide ();
    m_status_bar->setMaximumHeight(sbsize.height ());
    setVideoWidget (m_view_area);

    m_multiedit = new TextEdit (m_view_area, this);
    QFont fnt = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    m_multiedit->setFont (fnt);
    m_multiedit->hide ();

    m_infopanel = new InfoWindow (nullptr, this);

    connect (m_control_panel->scale_slider, SIGNAL (valueChanged (int)),
             m_view_area, SLOT (scale (int)));
    setFocusPolicy (Qt::ClickFocus);

    setAcceptDrops (true);
}

View::~View () {
    if (m_view_area->parent () != this)
        delete m_view_area;
}

void View::setEraseColor (const QColor & /*color*/) {
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
        m_status_bar->showMessage(msg);
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

void View::setEditMode (TopPlayItem *ri, bool enable) {
    m_edit_mode = enable;
    m_infopanel->setReadOnly (!m_edit_mode);
    if (m_edit_mode && !m_dock_infopanel->isVisible ())
        m_dock_infopanel->show ();
    //if (m_edit_mode && m_dock_infopanel->mayBeShow ())
    //    m_dock_infopanel->manualDock(m_dock_video,K3DockWidget::DockBottom,50);
    m_playlist->showAllNodes (ri, m_edit_mode);
}

#ifndef KMPLAYER_WITH_CAIRO
bool View::setPicture (const QString & path) {
    if (path.isEmpty ())
        m_image = QImage ();
    else {
        m_image = QImage (path);
        if (m_image.isNull ())
            qCDebug(LOG_KMPLAYER_COMMON) << "View::setPicture failed " << path;
        else if (m_image.depth () < 24)
            m_image = m_image.convertToFormat (QImage::Format_RGB32);
    }
    m_picture->setVisible (!m_image.isNull ());
    if (m_image.isNull ()) {
        m_view_area->setVideoWidgetVisible (true);
    } else {
        QPalette palette = m_picture->palette ();
        palette.setColor (m_picture->backgroundRole(), viewArea()->palette ().color (backgroundRole ()));
        palette.setBrush (m_picture->backgroundRole(), QBrush (m_image));
        m_picture->setPalette (palette);
        m_view_area->setVideoWidgetVisible (false);
        controlPanel ()->raise ();
        setControlPanelMode (CP_AutoHide);
    }
    return !m_image.isNull ();
}
#endif

void View::toggleVideoConsoleWindow () {
    if (m_multiedit->isVisible ()) {
        m_multiedit->hide ();
        m_view_area->setVideoWidgetVisible (true);
        m_control_panel->videoConsoleAction->setIcon(QIcon::fromTheme(("konsole")));
        m_control_panel->videoConsoleAction->setText (i18n ("Con&sole"));
        delayedShowButtons (false);
    } else {
        m_control_panel->videoConsoleAction->setIcon(QIcon::fromTheme("video"));
        m_control_panel->videoConsoleAction->setText (i18n ("V&ideo"));
        m_multiedit->show ();
        m_multiedit->raise ();
        m_view_area->setVideoWidgetVisible (false);
        addText (QString (""), false);
        if (m_controlpanel_mode == CP_AutoHide && m_playing)
            m_control_panel->show();
    }
    updateLayout ();
    Q_EMIT windowVideoConsoleToggled (m_multiedit->isVisible ());
}

void View::setControlPanelMode (ControlPanelMode m) {
    if (controlbar_timer) {
        killTimer (controlbar_timer);
        controlbar_timer = 0L;
    }
    m_old_controlpanel_mode = m_controlpanel_mode = m;
    if (m_playing && isFullScreen())
        m_controlpanel_mode = CP_AutoHide;
    if ((m_controlpanel_mode == CP_Show || m_controlpanel_mode == CP_Only) &&
            !m_control_panel->isVisible ()) {
        m_control_panel->show ();
    } else if (m_controlpanel_mode == CP_AutoHide) {
        if (!m_image.isNull () || (m_playing && !m_multiedit->isVisible ()))
            delayedShowButtons (false);
        else if (!m_control_panel->isVisible ()) {
            m_control_panel->show ();
        }
    } else if (m_controlpanel_mode == CP_Hide) {
        bool vis = m_control_panel->isVisible();
        m_control_panel->hide ();
        if (vis)
            m_view_area->resizeEvent (nullptr);
    }
    m_view_area->resizeEvent (nullptr);
}

void View::setStatusBarMode (StatusBarMode m) {
    m_statusbar_mode = m;
    m_status_bar->setVisible (m != SB_Hide);
    m_view_area->resizeEvent (nullptr);
}

void View::delayedShowButtons (bool show) {
    if ((show && m_control_panel->isVisible ()) ||
            (!show && !m_control_panel->isVisible ())) {
        if (controlbar_timer) {
            killTimer (controlbar_timer);
            controlbar_timer = 0;
        }
        if (!show)
            m_control_panel->hide (); // for initial race
    } else if (m_controlpanel_mode == CP_AutoHide &&
            (m_playing || !m_image.isNull ()) &&
            !m_multiedit->isVisible () && !controlbar_timer) {
        controlbar_timer = startTimer (500);
    }
}

void View::mouseMoved (int, int y) {
    int h = m_view_area->height ();
    int vert_buttons_pos = h - statusBarHeight ();
    int cp_height = controlPanel ()->maximumSize ().height ();
    if (cp_height > int (0.25 * h))
        cp_height = int (0.25 * h);
    delayedShowButtons (y > vert_buttons_pos-cp_height && y < vert_buttons_pos);
}

void View::updateLayout () {
    if (m_controlpanel_mode == CP_Only)
        m_control_panel->setMaximumHeight(height());
    m_view_area->resizeEvent (nullptr);
}

void View::setKeepSizeRatio (bool b) {
    if (m_keepsizeratio != b) {
        m_keepsizeratio = b;
        updateLayout ();
        m_view_area->update ();
    }
}

void View::timerEvent (QTimerEvent * e) {
    if (e->timerId () == controlbar_timer) {
        controlbar_timer = 0;
        if (m_playing || !m_image.isNull ()) {
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
                m_view_area->resizeEvent (nullptr);
            } else if (!mouse_on_buttons && m_control_panel->isVisible ()) {
                m_control_panel->hide ();
                m_view_area->resizeEvent (nullptr);
            }
        }
    } else if (e->timerId () == infopanel_timer) {
        if (m_infopanel->document ()->isEmpty ())
            m_dock_infopanel->hide ();
            //m_dock_infopanel->undock ();
        infopanel_timer  = 0;
    } else if (e->timerId () == m_restore_state_timer) {
        m_view_area->setVisible(true);
        setControlPanelMode (m_old_controlpanel_mode);
        m_dockarea->restoreState(m_dock_state);
        m_restore_state_timer = 0;
    }
    killTimer (e->timerId ());
}

void View::addText (const QString & str, bool eol) {
    if (m_tmplog_needs_eol)
        tmplog += QChar ('\n');
    tmplog += str;
    m_tmplog_needs_eol = eol;
    if (!m_multiedit->isVisible () && tmplog.size () < 7500)
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

void View::videoStart () {
    if (!isFullScreen () && m_dockarea->centralWidget () != m_view_area) {
        // restore from an info or playlist only setting
        if (m_dockarea->centralWidget () == m_playlist)
            m_dock_playlist->setWidget (m_playlist);
        else if (m_dockarea->centralWidget () == m_infopanel)
            m_dock_infopanel->setWidget (m_infopanel);
        else
            m_status_bar->setVisible (false);
        m_dockarea->setCentralWidget (m_view_area);
    }
    if (m_controlpanel_mode == CP_Only) {
        m_control_panel->setMaximumHeight(controlPanel()->preferredHeight());
        setControlPanelMode (CP_Show);
    }
}

void View::playingStart () {
    if (m_playing)
        return; //FIXME: make symetric with playingStop
    m_playing = true;
    m_revert_fullscreen = !isFullScreen();
    setControlPanelMode (m_old_controlpanel_mode);
}

void View::playingStop () {
    if (m_controlpanel_mode == CP_AutoHide && m_image.isNull ()) {
        m_control_panel->show ();
        //m_view_area->setMouseTracking (false);
    }
    if (controlbar_timer) {
        killTimer (controlbar_timer);
        controlbar_timer = 0;
    }
    m_playing = false;
    m_view_area->resizeEvent (nullptr);
}

void View::leaveEvent (QEvent *) {
    delayedShowButtons (false);
}

void View::reset () {
    if (m_revert_fullscreen && isFullScreen ())
        m_control_panel->fullscreenAction->activate (QAction::Trigger);
        //m_view_area->fullScreen ();
    playingStop ();
}

bool View::isFullScreen () const {
    return m_view_area->isFullScreen ();
}

void View::fullScreen () {
    if (m_restore_state_timer) {
        killTimer (m_restore_state_timer);
        m_restore_state_timer = 0;
    }
    if (!m_view_area->isFullScreen()) {
        m_sreensaver_disabled = false;
        QDBusReply<uint> reply = m_inhibitIface.Inhibit(QCoreApplication::applicationName(), "KMplayer: watching a film");

        m_powerManagerStopSleep = reply.isValid() ? reply : -1;

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
        m_dock_state = m_dockarea->saveState();
        m_dock_playlist->hide();
        m_dock_infopanel->hide();
        m_view_area->fullScreen();
        m_control_panel->zoomAction->setVisible (false);
        //if (m_viewer->isVisible ())
        //    m_viewer->setFocus ();
    } else {
        m_inhibitIface.UnInhibit(m_powerManagerStopSleep);
       // if (m_sreensaver_disabled)
       //     m_sreensaver_disabled = !kapp->dcopClient()->send
       //         ("kdesktop", "KScreensaverIface", "enable(bool)", "true");
        m_view_area->fullScreen();
        m_control_panel->zoomAction->setVisible (true);
        m_restore_state_timer = startTimer(100); //dockArea()->restoreState(m_dock_state);
    }
    setControlPanelMode (m_old_controlpanel_mode);
    Q_EMIT fullScreenChanged ();
}

int View::statusBarHeight () const {
    if (statusBar()->isVisible () && !viewArea()->isFullScreen ()) {
        if (statusBarMode () == SB_Only)
            return height ();
        else
            return statusBar()->maximumSize ().height ();
    }
    return 0;
}
