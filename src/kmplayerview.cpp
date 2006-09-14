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
#include <qaccel.h>
#include <qcursor.h>
#include <qkeysequence.h>
#include <qslider.h>
#include <qlabel.h>
#include <qdatastream.h>
#include <qpixmap.h>
#include <qpainter.h>
#include <qwidgetstack.h>
#include <qheader.h>
#include <qcursor.h>
#include <qclipboard.h>

#include <kiconloader.h>
#include <kstaticdeleter.h>
#include <kstatusbar.h>
#include <kdebug.h>
#include <klocale.h>
#include <kapplication.h>
#include <kactioncollection.h>
#include <kstdaction.h>
#include <kshortcut.h>
#include <kurldrag.h>
#include <klistview.h>
#include <kfinddialog.h>
#include <dcopclient.h>
#include <kglobalsettings.h>

#include "kmplayerview.h"
#include "kmplayercontrolpanel.h"
#include "kmplayersource.h"
#include "playlistview.h"

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
#define MOUSE_INVISIBLE_DELAY 2000


using namespace KMPlayer;

//-------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT ViewArea::ViewArea (QWidget * parent, View * view)
 : QWidget (parent, "kde_kmplayer_viewarea", WResizeNoErase | WRepaintNoErase),
   m_parent (parent),
   m_view (view),
   m_painter (0L),
   m_paint_buffer (0L),
   m_collection (new KActionCollection (this)),
   m_mouse_invisible_timer (0),
   m_repaint_timer (0),
   m_fullscreen_scale (100),
   scale_lbl_id (-1),
   scale_slider_id (-1),
   m_fullscreen (false),
   m_minimal (false) {
    setEraseColor (QColor (0, 0, 0));
    setAcceptDrops (true);
    new KAction (i18n ("Fullscreen"), KShortcut (Qt::Key_F), this, SLOT (accelActivated ()), m_collection, "view_fullscreen_toggle");
    setMouseTracking (true);
}

KDE_NO_CDTOR_EXPORT ViewArea::~ViewArea () {
    delete m_painter;
    delete m_paint_buffer;
}

KDE_NO_EXPORT void ViewArea::fullScreen () {
    killTimers ();
    m_mouse_invisible_timer = m_repaint_timer = 0;
    if (m_fullscreen) {
        showNormal ();
        reparent (m_parent, 0, QPoint (0, 0), true);
        static_cast <KDockWidget *> (m_parent)->setWidget (this);
        for (unsigned i = 0; i < m_collection->count (); ++i)
            m_collection->action (i)->setEnabled (false);
        if (scale_lbl_id != -1) {
            m_view->controlPanel ()->popupMenu ()->removeItem (scale_lbl_id);
            m_view->controlPanel ()->popupMenu ()->removeItem (scale_slider_id);
            scale_lbl_id = scale_slider_id = -1;
        }
        m_view->controlPanel ()->button (ControlPanel::button_playlist)->setIconSet (QIconSet (QPixmap (playlist_xpm)));
    } else {
        m_topwindow_rect = topLevelWidget ()->geometry ();
        reparent (0L, 0, qApp->desktop()->screenGeometry(this).topLeft(), true);
        showFullScreen ();
        for (unsigned i = 0; i < m_collection->count (); ++i)
            m_collection->action (i)->setEnabled (true);
        QPopupMenu * menu = m_view->controlPanel ()->popupMenu ();
        QLabel * lbl = new QLabel (i18n ("Scale:"), menu);
        scale_lbl_id = menu->insertItem (lbl, -1, 4);
        QSlider * slider = new QSlider (50, 150, 10, m_fullscreen_scale, Qt::Horizontal, menu);
        connect (slider, SIGNAL (valueChanged (int)), this, SLOT (scale (int)));
        scale_slider_id = menu->insertItem (slider, -1, 5);
        m_view->controlPanel ()->button (ControlPanel::button_playlist)->setIconSet (QIconSet (QPixmap (normal_window_xpm)));
    }
    m_fullscreen = !m_fullscreen;
    m_view->controlPanel()->popupMenu ()->setItemChecked (ControlPanel::menu_fullscreen, m_fullscreen);

    if (m_fullscreen) {
        m_mouse_invisible_timer = startTimer(MOUSE_INVISIBLE_DELAY);
    } else {
        if (m_mouse_invisible_timer) {
            killTimer (m_mouse_invisible_timer);
            m_mouse_invisible_timer = 0;
        }
        unsetCursor();
    }
}

void ViewArea::minimalMode () {
    m_minimal = !m_minimal;
    killTimers ();
    m_mouse_invisible_timer = m_repaint_timer = 0;
    if (m_minimal) {
        m_view->setViewOnly ();
        m_view->setControlPanelMode (KMPlayer::View::CP_AutoHide);
        m_view->setNoInfoMessages (true);
        m_view->controlPanel ()->button (ControlPanel::button_playlist)->setIconSet (QIconSet (QPixmap (normal_window_xpm)));
    } else {
        m_view->setControlPanelMode (KMPlayer::View::CP_Show);
        m_view->setNoInfoMessages (false);
        m_view->controlPanel ()->button (ControlPanel::button_playlist)->setIconSet (QIconSet (QPixmap (playlist_xpm)));
    }
    m_topwindow_rect = topLevelWidget ()->geometry ();
}

KDE_NO_EXPORT void ViewArea::accelActivated () {
    m_view->controlPanel()->popupMenu ()->activateItemAt (m_view->controlPanel()->popupMenu ()->indexOf (ControlPanel::menu_fullscreen)); 
}

KDE_NO_EXPORT void ViewArea::mousePressEvent (QMouseEvent * e) {
    if (eventListener && eventListener->handleEvent(new PointerEvent(event_pointer_clicked,e->x(), e->y())))
        e->accept ();
}

KDE_NO_EXPORT void ViewArea::mouseDoubleClickEvent (QMouseEvent *) {
    m_view->fullScreen (); // screensaver stuff
}

KDE_NO_EXPORT void ViewArea::mouseMoveEvent (QMouseEvent * e) {
    if (e->state () == Qt::NoButton) {
        int vert_buttons_pos = height ();
        int cp_height = m_view->controlPanel ()->maximumSize ().height ();
        m_view->delayedShowButtons (e->y() > vert_buttons_pos-cp_height &&
                                    e->y() < vert_buttons_pos);
    }
    if (eventListener && eventListener->handleEvent(new PointerEvent(event_pointer_moved,e->x(), e->y())))
        e->accept ();
    mouseMoved (); // for m_mouse_invisible_timer
}

KDE_NO_EXPORT void ViewArea::syncVisual (QRect rect) {
    if (!eventListener) {
        repaint (rect, false);
        return;
    }
#define PAINT_BUFFER_HEIGHT 128
    if (!m_paint_buffer) {
        m_paint_buffer = new QPixmap (width (), PAINT_BUFFER_HEIGHT);
        m_painter = new QPainter ();
    } else if (((QPixmap *)m_paint_buffer)->width () < width ())
        ((QPixmap *)m_paint_buffer)->resize (width (), PAINT_BUFFER_HEIGHT);
    int py=0;
    int ex = rect.x ();
    int ey = rect.y ();
    int ew = rect.width ();
    int eh = rect.height ();
    while (py < eh) {
        int ph = eh-py < PAINT_BUFFER_HEIGHT ? eh-py : PAINT_BUFFER_HEIGHT;
        m_painter->begin (m_paint_buffer);
        m_painter->translate(-ex, -ey-py);
        m_painter->fillRect (ex, ey+py, ew, ph, QBrush (paletteBackgroundColor ()));
        eventListener->handleEvent(new PaintEvent(*m_painter, ex, ey+py,ew,ph));
        m_painter->end();
        bitBlt (this, ex, ey+py, m_paint_buffer, 0, 0, ew, ph);
        py += PAINT_BUFFER_HEIGHT;
    }
    XFlush (qt_xdisplay ());
}

KDE_NO_EXPORT void ViewArea::paintEvent (QPaintEvent * pe) {
    if (eventListener)
        scheduleRepaint (pe->rect ().x (), pe->rect ().y (), pe->rect ().width (), pe->rect ().height ());
    else
        QWidget::paintEvent (pe);
}

KDE_NO_EXPORT void ViewArea::scale (int val) {
    m_fullscreen_scale = val;
    resizeEvent (0L);
}

KDE_NO_EXPORT void ViewArea::resizeEvent (QResizeEvent *) {
    if (!m_view->controlPanel ()) return;
    int x =0, y = 0;
    int w = width ();
    int h = height ();
    int hsb = m_view->statusBar ()->isVisible () && !isFullScreen () ? (m_view->statusBarMode () == View::SB_Only ? h : m_view->statusBar()->maximumSize ().height ()) : 0;
    int hcp = m_view->controlPanel ()->isVisible () ? (m_view->controlPanelMode () == View::CP_Only ? h-hsb: m_view->controlPanel()->maximumSize ().height ()) : 0;
    int wws = w;
    // move controlpanel over video when autohiding and playing
    int hws = h - (m_view->controlPanelMode () == View::CP_AutoHide && m_view->widgetStack ()->visibleWidget () == m_view->viewer () ? 0 : hcp) - hsb;
    // now scale the regions and check if video region is already sized
    bool av_geometry_changed = false;
    if (eventListener && wws > 0 && hws > 0) {
        m_av_geometry = QRect (0, 0, 0, 0);
        eventListener->handleEvent (new SizeEvent (x, y, wws, hws, m_view->keepSizeRatio () ? fit_meet : fit_fill));
        av_geometry_changed = (m_av_geometry != QRect (0, 0, 0, 0));
        x = m_av_geometry.x ();
        y = m_av_geometry.y ();
        wws = m_av_geometry.width ();
        hws = m_av_geometry.height ();
            //m_view->viewer ()->setAspect (region->w / region->h);
    } else
        m_av_geometry = QRect (x, y, wws, hws);

    // finally resize controlpanel and video widget
    if (m_view->controlPanel ()->isVisible ())
        m_view->controlPanel ()->setGeometry (0, h-hcp-hsb, w, hcp);
    if (m_view->statusBar ()->isVisible ())
        m_view->statusBar ()->setGeometry (0, h-hsb, w, hsb);
    if (m_fullscreen && wws == w && hws == h) {
        wws = wws * m_fullscreen_scale / 100;
        hws = hws * m_fullscreen_scale / 100;
        x += (w - wws) / 2;
        y += (h - hws) / 2;
    }
    if (!av_geometry_changed)
        setAudioVideoGeometry (x, y, wws, hws, 0L);
}

KDE_NO_EXPORT
void ViewArea::setAudioVideoGeometry (int x, int y, int w, int h, unsigned int * bg_color) {
    if (m_view->controlPanelMode() == View::CP_Only) {
        w = h = 0;
    } else if (m_view->keepSizeRatio ()) { // scale video widget inside region
        int hfw = m_view->viewer ()->heightForWidth (w);
        if (hfw > 0)
            if (hfw > h) {
                int old_w = w;
                w = int ((1.0 * h * w)/(1.0 * hfw));
                x += (old_w - w) / 2;
            } else {
                y += (h - hfw) / 2;
                h = hfw;
            }
    }
    m_av_geometry = QRect (x, y, w, h);
    QRect rect = m_view->widgetStack ()->geometry ();
    if (m_av_geometry != rect) {
        m_view->widgetStack ()->setGeometry (x, y, w, h);
        rect.unite (m_av_geometry);
        scheduleRepaint (rect.x (), rect.y (), rect.width (), rect.height ());
    }
    if (bg_color)
        if (QColor (QRgb (*bg_color)) != (m_view->viewer ()->paletteBackgroundColor ())) {
            m_view->viewer()->setCurrentBackgroundColor (QColor (QRgb (*bg_color)));
            scheduleRepaint (x, y, w, h);
        }
}

KDE_NO_EXPORT void ViewArea::setEventListener (NodePtr el) {
    if (eventListener != el) {
        eventListener = el;
        resizeEvent (0L);
        if (m_repaint_timer) {
            killTimer (m_repaint_timer);
            m_repaint_timer = 0;
        }
        m_view->viewer()->resetBackgroundColor ();
        repaint ();
    }
}

KDE_NO_EXPORT void ViewArea::showEvent (QShowEvent *) {
    resizeEvent (0L);
}

KDE_NO_EXPORT void ViewArea::dropEvent (QDropEvent * de) {
    m_view->dropEvent (de);
}

KDE_NO_EXPORT void ViewArea::dragEnterEvent (QDragEnterEvent* dee) {
    m_view->dragEnterEvent (dee);
}

KDE_NO_EXPORT void ViewArea::contextMenuEvent (QContextMenuEvent * e) {
    m_view->controlPanel ()->popupMenu ()->exec (e->globalPos ());
}

KDE_NO_EXPORT void ViewArea::mouseMoved () {
    if (m_fullscreen) {
        if (m_mouse_invisible_timer)
            killTimer (m_mouse_invisible_timer);
        unsetCursor ();
        m_mouse_invisible_timer = startTimer (MOUSE_INVISIBLE_DELAY);
    }
}

KDE_NO_EXPORT void ViewArea::scheduleRepaint (int x, int y, int w, int h) {
    if (m_repaint_timer)
        m_repaint_rect = m_repaint_rect.unite (QRect (x, y, w, h));
    else {
        m_repaint_rect = QRect (x, y, w, h);
        m_repaint_timer = startTimer (10); // 100 per sec should do
    }
}

KDE_NO_EXPORT
void ViewArea::moveRect (int x, int y, int w, int h, int x1, int y1) {
    QRect r (x, y, w, h);
    if (m_repaint_timer && m_repaint_rect.intersects (r)) {
        m_repaint_rect = m_repaint_rect.unite (QRect (x1, y1, w, h).unite (r));
    } else if (m_view->viewer()->frameGeometry ().intersects (r)) {
        QRect r2 (QRect (x1, y1, w, h).unite (r));
        scheduleRepaint (r.x (), r.y (), r.width (), r.height ());
    } else {
        bitBlt (this, x1, y1, this, x, y, w, h);
        if (x1 > x)
            syncVisual (QRect (x, y, x1 - x, h));
        else if (x > x1)
            syncVisual (QRect (x1 + w, y, x - x1, h));
        if (y1 > y)
            syncVisual (QRect (x, y, w, y1 - y));
        else if (y > y1)
            syncVisual (QRect (x, y1 + h, w, y - y1));
    }
}

KDE_NO_EXPORT void ViewArea::timerEvent (QTimerEvent * e) {
    if (e->timerId () == m_mouse_invisible_timer) {
        killTimer (m_mouse_invisible_timer);
        m_mouse_invisible_timer = 0;
        if (m_fullscreen)
            setCursor (BlankCursor);
    } else if (e->timerId () == m_repaint_timer) {
        killTimer (m_repaint_timer);
        m_repaint_timer = 0;
        //repaint (m_repaint_rect, false);
        syncVisual (m_repaint_rect);
    } else {
        kdError () << "unknown timer " << e->timerId () << " " << m_repaint_timer << endl;
        killTimer (e->timerId ());
    }
}

KDE_NO_EXPORT void ViewArea::closeEvent (QCloseEvent * e) {
    //kdDebug () << "closeEvent" << endl;
    if (m_fullscreen) {
        fullScreen ();
        if (!m_parent->topLevelWidget ()->isVisible ())
            m_parent->topLevelWidget ()->show ();
        e->ignore ();
    } else
        QWidget::closeEvent (e);
}

//-----------------------------------------------------------------------------

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
        m_widgetstack->visibleWidget ()->setFocus ();
        emit urlDropped (sl);
        de->accept ();
    }
}

KDE_NO_EXPORT void View::dragEnterEvent (QDragEnterEvent* dee) {
    if (isDragValid (dee))
        dee->accept ();
}

KDE_NO_EXPORT void View::init (KActionCollection * action_collection) {
    //setBackgroundMode(Qt::NoBackground);
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
    m_playlist->setPaletteBackgroundColor (QColor (0, 0, 0));
    m_playlist->setPaletteForegroundColor (QColor (0xB2, 0xB2, 0xB2));
    m_dock_playlist->setWidget (m_playlist);
    viewbox->addWidget (m_dockarea);
    m_widgetstack = new QWidgetStack (m_view_area);
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
    m_multiedit->setTextFormat (Qt::PlainText);
    QFont fnt = KGlobalSettings::fixedFont ();
    m_multiedit->setFont (fnt);
    m_widgettypes[WT_Console] = m_multiedit;

    m_widgettypes[WT_Picture] = new KMPlayerPictureWidget (m_widgetstack, this);

    m_dock_infopanel = m_dockarea->createDockWidget ("infopanel", KGlobal::iconLoader ()->loadIcon (QString ("info"), KIcon::Small));
    m_infopanel = new InfoWindow (m_dock_infopanel, this);
    m_dock_infopanel->setWidget (m_infopanel);

    m_widgetstack->addWidget (m_viewer);
    m_widgetstack->addWidget (m_multiedit);
    m_widgetstack->addWidget (m_widgettypes[WT_Picture]);

    setFocusPolicy (QWidget::ClickFocus);

    setAcceptDrops (true);
    m_view_area->resizeEvent (0L);
    kdDebug() << "View " << (unsigned long) (m_viewer->embeddedWinId()) << endl;

    XSelectInput (qt_xdisplay (), m_viewer->embeddedWinId (), 
               //KeyPressMask | KeyReleaseMask |
               KeyPressMask |
               //EnterWindowMask | LeaveWindowMask |
               //FocusChangeMask |
               ExposureMask |
               StructureNotifyMask |
               PointerMotionMask
              );
    kapp->installX11EventFilter (this);
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
        if (!ismain && !m_edit_mode)
            m_dock_infopanel->undock ();
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
                if (h > int (0.5 * height ())) {
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
        m_widgetstack->raiseWidget (m_viewer);
    } else {
        m_widgettypes[WT_Picture]->setPaletteBackgroundPixmap (*m_image);
        m_widgetstack->raiseWidget (m_widgettypes[WT_Picture]);
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

void View::showWidget (WidgetType wt) {
    m_widgetstack->raiseWidget (m_widgettypes [wt]);
    if (m_widgetstack->visibleWidget () == m_widgettypes[WT_Console])
        addText (QString (""), false);
    updateLayout ();
}

void View::toggleVideoConsoleWindow () {
    WidgetType wt = WT_Console;
    if (m_widgetstack->visibleWidget () == m_widgettypes[WT_Console]) {
        wt = WT_Video;
        m_control_panel->popupMenu ()->changeItem (ControlPanel::menu_video, KGlobal::iconLoader ()->loadIconSet (QString ("konsole"), KIcon::Small, 0, true), i18n ("Con&sole"));
    } else
        m_control_panel->popupMenu ()->changeItem (ControlPanel::menu_video, KGlobal::iconLoader ()->loadIconSet (QString ("video"), KIcon::Small, 0, true), i18n ("V&ideo"));
    showWidget (wt);
    emit windowVideoConsoleToggled (int (wt));
}

void View::setControlPanelMode (ControlPanelMode m) {
    killTimer (controlbar_timer);
    controlbar_timer = 0L;
    m_old_controlpanel_mode = m_controlpanel_mode = m;
    if (m_playing && isFullScreen())
        m_controlpanel_mode = CP_AutoHide;
    if (m_control_panel)
        if (m_controlpanel_mode == CP_Show || m_controlpanel_mode == CP_Only)
            m_control_panel->show ();
        else if (m_controlpanel_mode == CP_AutoHide) { 
            if (m_playing || m_widgetstack->visibleWidget () == m_widgettypes[WT_Picture])
                delayedShowButtons (false);
            else
                m_control_panel->show ();
        } else
            m_control_panel->hide ();
    //m_view_area->setMouseTracking (m_controlpanel_mode == CP_AutoHide && m_playing);
    m_view_area->resizeEvent (0L);
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
    if (m_controlpanel_mode != CP_AutoHide || controlbar_timer ||
        (m_control_panel &&
         (show && m_control_panel->isVisible ()) || 
         (!show && !m_control_panel->isVisible ())))
        return;
    controlbar_timer = startTimer (500);
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
        if (!m_playing && m_widgetstack->visibleWidget () != m_widgettypes[WT_Picture])
            return;
        int vert_buttons_pos = m_view_area->height ();
        int mouse_pos = m_view_area->mapFromGlobal (QCursor::pos ()).y();
        int cp_height = m_control_panel->maximumSize ().height ();
        bool mouse_on_buttons = (//m_view_area->hasMouse () && 
                mouse_pos >= vert_buttons_pos-cp_height &&
                mouse_pos <= vert_buttons_pos);
        if (m_control_panel)
            if (mouse_on_buttons && !m_control_panel->isVisible ())
                m_control_panel->show ();
            else if (!mouse_on_buttons && m_control_panel->isVisible ())
                m_control_panel->hide ();
    }
    killTimer (e->timerId ());
    m_view_area->resizeEvent (0L);
}

void View::addText (const QString & str, bool eol) {
    if (m_tmplog_needs_eol)
        tmplog += QChar ('\n');
    tmplog += str;
    m_tmplog_needs_eol = eol;
    if (m_widgetstack->visibleWidget () != m_widgettypes[WT_Console] &&
            tmplog.length () < 7500)
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
    if (m_dockarea->getMainDockWidget () != m_dock_video) {
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
    if (m_widgetstack->visibleWidget () == m_widgettypes[WT_Picture])
        m_widgetstack->raiseWidget (m_viewer);
    m_playing = true;
    m_revert_fullscreen = !isFullScreen();
    setControlPanelMode (m_old_controlpanel_mode);
}

KDE_NO_EXPORT void View::playingStop () {
    if (m_control_panel && m_controlpanel_mode == CP_AutoHide) {
        m_control_panel->show ();
        //m_view_area->setMouseTracking (false);
    }
    m_playing = false;
    XClearWindow (qt_xdisplay(), m_viewer->embeddedWinId ());
    m_view_area->resizeEvent (0L);
}

KDE_NO_EXPORT void View::leaveEvent (QEvent *) {
    if (m_controlpanel_mode == CP_AutoHide && m_playing)
        delayedShowButtons (false);
}

KDE_NO_EXPORT void View::reset () {
    if (m_revert_fullscreen && isFullScreen())
        m_control_panel->popupMenu ()->activateItemAt (m_control_panel->popupMenu ()->indexOf (ControlPanel::menu_fullscreen)); 
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
        m_widgetstack->visibleWidget ()->setFocus ();
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

KDE_NO_EXPORT bool View::x11Event (XEvent * e) {
    switch (e->type) {
        case UnmapNotify:
            if (e->xunmap.event == m_viewer->embeddedWinId ()) {
                videoStart ();
                //hide();
            }
            break;
        case XKeyPress:
            if (e->xkey.window == m_viewer->embeddedWinId ()) {
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
            if (m_playing && e->xmotion.window == m_viewer->embeddedWinId ())
                delayedShowButtons (e->xmotion.y > m_view_area->height () -
                                    m_control_panel->maximumSize ().height ());
            m_view_area->mouseMoved ();
            break;
        case MapNotify:
            if (e->xmap.event == m_viewer->embeddedWinId ()) {
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
  : QXEmbed (parent), m_bgcolor (0), m_aspect (0.0),
    m_view (view) {
    /*XWindowAttributes xwa;
    XGetWindowAttributes (qt_xdisplay(), winId (), &xwa);
    XSetWindowAttributes xswa;
    xswa.background_pixel = 0;
    xswa.border_pixel = 0;
    xswa.colormap = xwa.colormap;
    create (XCreateWindow (qt_xdisplay (), parent->winId (), 0, 0, 10, 10, 0, 
                           x11Depth (), InputOutput, (Visual*)x11Visual (),
                           CWBackPixel | CWBorderPixel | CWColormap, &xswa));*/
    setAcceptDrops (true);
#if KDE_IS_VERSION(3,1,1)
    setProtocol(QXEmbed::XPLAIN);
#endif
    int scr = DefaultScreen (qt_xdisplay ());
    embed (XCreateSimpleWindow (qt_xdisplay(), view->winId (), 0, 0, width(), height(), 1, BlackPixel (qt_xdisplay(), scr), BlackPixel (qt_xdisplay(), scr)));
    XClearWindow (qt_xdisplay(), embeddedWinId ());
}

KDE_NO_CDTOR_EXPORT Viewer::~Viewer () {
}
    
KDE_NO_EXPORT void Viewer::mouseMoveEvent (QMouseEvent * e) {
    if (e->state () == Qt::NoButton) {
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
    char buf[2] = { char (key), '\0' }; 
    KeySym keysym = XStringToKeysym (buf);
    XKeyEvent event = {
        XKeyPress, 0, true,
        qt_xdisplay (), embeddedWinId (), qt_xrootwin(), embeddedWinId (),
        /*time*/ 0, 0, 0, 0, 0,
        0, XKeysymToKeycode (qt_xdisplay (), keysym), true
    };
    XSendEvent (qt_xdisplay(), embeddedWinId (), false, KeyPressMask, (XEvent *) &event);
    XFlush (qt_xdisplay ());
}

KDE_NO_EXPORT void Viewer::sendConfigureEvent () {
    XConfigureEvent c = {
        ConfigureNotify, 0UL, True,
        qt_xdisplay (), embeddedWinId (), winId (),
        x (), y (), width (), height (),
        0, None, False
    };
    XSendEvent(qt_xdisplay(), c.event, true, StructureNotifyMask, (XEvent*) &c);
    XFlush (qt_xdisplay ());
}

KDE_NO_EXPORT void Viewer::contextMenuEvent (QContextMenuEvent * e) {
    m_view->controlPanel ()->popupMenu ()->exec (e->globalPos ());
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
    setPaletteBackgroundColor (c);
    XSetWindowBackground (qt_xdisplay (), embeddedWinId (), c.rgb ());
    XFlush (qt_xdisplay ());
}

#include "kmplayerview.moc"
