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

#include <stdio.h>
#include <math.h>

// include files for Qt
#include <qprinter.h>
#include <qtimer.h>
#include <qpainter.h>
#include <qmetaobject.h>
#include <qlayout.h>
#include <qpixmap.h>
#include <qmultilineedit.h>
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

#include "kmplayerview.h"
#include "kmplayersource.h"

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

static const int button_height_with_slider = 15;
static const int button_height_only_buttons = 11;

// application specific includes
//#include "kmplayer.h"

#include <kstaticdeleter.h>
#include <kdebug.h>
#include <klocale.h>
#include <kapplication.h>
#include <kactioncollection.h>
#include <kshortcut.h>
#include <kurldrag.h>
#include <dcopclient.h>

//-------------------------------------------------------------------------

static const char * const stop_xpm[] = {
    "5 7 2 1",
    "       c None",
    ".      c #000000",
    "     ",
    ".....",
    ".....",
    ".....",
    ".....",
    ".....",
    "     "};

static const char * const play_xpm[] = {
    "5 9 2 1",
    "       c None",
    ".      c #000000",
    ".    ",
    "..   ",
    "...  ",
    ".... ",
    ".....",
    ".... ",
    "...  ",
    "..   ",
    ".    "};

static const char * const pause_xpm[] = {
    "7 9 2 1",
    "       c None",
    ".      c #000000",
    "       ",
    "..   ..",
    "..   ..",
    "..   ..",
    "..   ..",
    "..   ..",
    "..   ..",
    "..   ..",
    "       "};

static const char * const forward_xpm[] = {
    "11 9 2 1",
    "       c None",
    ".      c #000000",
    ".     .    ",
    "..    ..   ",
    "...   ...  ",
    "....  .... ",
    "..... .....",
    "....  .... ",
    "...   ...  ",
    "..    ..   ",
    ".     .    "};

static const char * const back_xpm[] = {
    "11 9 2 1",
    "       c None",
    ".      c #000000",
    "    .     .",
    "   ..    ..",
    "  ...   ...",
    " ....  ....",
    "..... .....",
    " ....  ....",
    "  ...   ...",
    "   ..    ..",
    "    .     ."};

static const char * const config_xpm[] = {
    "11 8 2 1",
    "       c None",
    ".      c #000000",
    "           ",
    "           ",
    "...........",
    " ......... ",
    "  .......  ",
    "   .....   ",
    "    ...    ",
    "     .     "};

static const char * const record_xpm[] = {
    "7 7 3 1",
    "       c None",
    ".      c #000000",
    "+      c #FF0000",
    "       ",
    ".......",
    ".+++++.",
    ".+++++.",
    ".+++++.",
    ".......",
    "       "};

static const char * const broadcast_xpm[] = {
"21 9 2 1",
"       c None",
".      c #000000",
"                     ",
" ..  ..       ..  .. ",
"..  ..   ...   ..  ..",
"..  ..  .....  ..  ..",
"..  ..  .....  ..  ..",
"..  ..  .....  ..  ..",
"..  ..   ...   ..  ..",
" ..  ..       ..  .. ",
"                     "};

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT KMPlayerViewLayer::KMPlayerViewLayer (QWidget * parent, KMPlayerView * view)
 : QWidget (parent),
   m_parent (parent),
   m_view (view),
   m_collection (new KActionCollection (this)),
   m_fullscreen (false) {
    new KAction (i18n ("Escape"), KShortcut (Qt::Key_Escape), this, SLOT (accelActivated ()), m_collection, "view_fullscreen_escape");
    new KAction (i18n ("Fullscreen"), KShortcut (Qt::Key_F), this, SLOT (accelActivated ()), m_collection, "view_fullscreen_toggle");
}

KDE_NO_EXPORT void KMPlayerViewLayer::fullScreen () {
    if (m_fullscreen) {
        showNormal ();
        reparent (m_parent, 0, QPoint (0, 0), true);
        static_cast <KDockWidget *> (m_parent)->setWidget (this);
        for (int i = 0; i < m_collection->count (); ++i)
            m_collection->action (i)->setEnabled (false);
    } else {
        reparent (0L, 0, qApp->desktop()->screenGeometry(this).topLeft(), true);
        showFullScreen ();
        for (int i = 0; i < m_collection->count (); ++i)
            m_collection->action (i)->setEnabled (true);

        //m_accel = new QAccel (this);
        //int id = m_accel->insertItem (QKeySequence (Qt::Key_Escape));
        //m_accel->connectItem (id, this, SLOT (accelActivated ()));
    }
    m_fullscreen = !m_fullscreen;
    m_view->buttonBar()->popupMenu ()->setItemChecked (KMPlayerControlPanel::menu_fullscreen, m_fullscreen);
}

KDE_NO_EXPORT void KMPlayerViewLayer::accelActivated () {
    m_view->buttonBar()->popupMenu ()->activateItemAt (m_view->buttonBar()->popupMenu ()->indexOf (KMPlayerControlPanel::menu_fullscreen)); 
}
//-----------------------------------------------------------------------------

class KMPlayerViewerHolder : public QWidget {
    friend class KMPlayerView;
public:
    KMPlayerViewerHolder (QWidget * parent, KMPlayerView * view);
protected:
    void resizeEvent (QResizeEvent *);
    void mouseMoveEvent (QMouseEvent *);
    void dragEnterEvent (QDragEnterEvent *);
    void dropEvent (QDropEvent *);
    void contextMenuEvent (QContextMenuEvent * e);
private:
    KMPlayerView * m_view;
};

inline
KMPlayerViewerHolder::KMPlayerViewerHolder (QWidget * pa, KMPlayerView * view)
 : QWidget (pa), m_view (view) {
    setEraseColor (QColor (0, 0, 0));
    setAcceptDrops (true);
}

KDE_NO_EXPORT void KMPlayerViewerHolder::mouseMoveEvent (QMouseEvent * e) {
    if (e->state () == Qt::NoButton) {
        int vert_buttons_pos = height ();
        int cp_height = m_view->buttonBar ()->maximumSize ().height ();
        m_view->delayedShowButtons (e->y() > vert_buttons_pos-cp_height &&
                                    e->y() < vert_buttons_pos);
    }
}

KDE_NO_EXPORT void KMPlayerViewerHolder::resizeEvent (QResizeEvent *) {
    int x =0, y = 0;
    int w = width ();
    int h = height ();
    if (m_view->keepSizeRatio ()) {
        int hfw = m_view->viewer ()->heightForWidth (w);
        if (hfw > 0) {
            if (hfw > h)
                w = int ((1.0 * h * w)/(1.0 * hfw));
            else
                h = hfw;
        }
        x = (width () - w) / 2;
        y = (height () - h) / 2;
    }
    m_view->widgetStack ()->setGeometry (x, y, w, h);
}

KDE_NO_EXPORT void KMPlayerViewerHolder::dropEvent (QDropEvent * de) {
    m_view->dropEvent (de);
}

KDE_NO_EXPORT void KMPlayerViewerHolder::dragEnterEvent (QDragEnterEvent* dee) {
    m_view->dragEnterEvent (dee);
}

KDE_NO_EXPORT void KMPlayerViewerHolder::contextMenuEvent (QContextMenuEvent * e) {
    m_view->buttonBar ()->popupMenu ()->exec (e->globalPos ());
}

//-----------------------------------------------------------------------------

static QPushButton * ctrlButton (QWidget * w, QBoxLayout * l, const char * const * p, int key = 0) {
    QPushButton * b = new QPushButton (QIconSet (QPixmap(p)), QString::null, w);
    b->setFocusPolicy (QWidget::NoFocus);
    b->setFlat (true);
    if (key)
        b->setAccel (QKeySequence (key));
    l->addWidget (b);
    return b;
}

KDE_NO_CDTOR_EXPORT
KMPlayerControlButton::KMPlayerControlButton (QWidget * parent, QBoxLayout * l, const char * const * p, int key)
 : QPushButton (QIconSet (QPixmap(p)), QString::null, parent, "kde_kmplayer_control_button") {
   setFocusPolicy (QWidget::NoFocus);
   setFlat (true);
   if (key)
       setAccel (QKeySequence (key));
   l->addWidget (this);
}

KDE_NO_EXPORT void KMPlayerControlButton::enterEvent (QEvent *) {
    emit mouseEntered ();
}
        
//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT KMPlayerControlPanel::KMPlayerControlPanel(QWidget * parent)
 : QWidget (parent),
   m_progress_mode (progress_playing),
   m_progress_length (0),
   m_auto_controls (true) {
    m_buttonbox = new QHBoxLayout (this, 5, 4);
    m_buttons[button_config] = new KMPlayerControlButton (this, m_buttonbox, config_xpm);
    m_buttons[button_back] = ctrlButton (this, m_buttonbox, back_xpm);
    m_buttons[button_play] = ctrlButton (this, m_buttonbox, play_xpm, Qt::Key_R);
    m_buttons[button_forward] = ctrlButton (this, m_buttonbox, forward_xpm);
    m_buttons[button_stop] = ctrlButton (this, m_buttonbox, stop_xpm, Qt::Key_S);
    m_buttons[button_pause] = ctrlButton (this, m_buttonbox, pause_xpm, Qt::Key_P);
    m_buttons[button_record] = ctrlButton (this, m_buttonbox, record_xpm);
    m_buttons[button_broadcast] = ctrlButton (this, m_buttonbox, broadcast_xpm);
    m_buttons[button_play]->setToggleButton (true);
    m_buttons[button_stop]->setToggleButton (true);
    m_buttons[button_record]->setToggleButton (true);
    m_buttons[button_broadcast]->setToggleButton (true);
    m_buttons[button_broadcast]->hide ();
    m_posSlider = new QSlider (Qt::Horizontal, this);
    m_posSlider->setEnabled (false);
    m_buttonbox->addWidget (m_posSlider);
    showPositionSlider (true);
    m_popupMenu = new KMPlayerPopupMenu (this);
    m_playerMenu = new KMPlayerPopupMenu (this);
    m_playerMenu->setEnabled (false);
    m_popupMenu->insertItem (i18n ("&Play with"), m_playerMenu, menu_player);
    m_popupMenu->setItemVisible (menu_player, false);
    m_bookmarkMenu = new KMPlayerPopupMenu (this);
    m_popupMenu->insertItem (i18n("&Bookmarks"), m_bookmarkMenu, menu_bookmark);
    m_viewMenu = new KMPlayerPopupMenu (this);
    m_viewMenu->insertItem (KGlobal::iconLoader ()->loadIconSet (QString ("video"), KIcon::Small, 0, true), i18n ("V&ideo"), menu_video);
    m_viewMenu->insertItem (KGlobal::iconLoader ()->loadIconSet (QString ("player_playlist"), KIcon::Small, 0, true), i18n ("Pla&y List"), menu_playlist);
    m_viewMenu->insertItem (KGlobal::iconLoader ()->loadIconSet (QString ("konsole"), KIcon::Small, 0, true), i18n ("Con&sole"), menu_console);
    m_popupMenu->insertItem (KGlobal::iconLoader ()->loadIconSet (QString ("window"), KIcon::Small, 0, false), i18n ("&View"), m_viewMenu, menu_view);
    m_zoomMenu = new KMPlayerPopupMenu (this);
    m_zoomMenu->insertItem (i18n ("50%"), menu_zoom50);
    m_zoomMenu->insertItem (i18n ("100%"), menu_zoom100);
    m_zoomMenu->insertItem (i18n ("150%"), menu_zoom150);
    m_popupMenu->insertItem (KGlobal::iconLoader ()->loadIconSet (QString ("viewmag"), KIcon::Small, 0, false), i18n ("&Zoom"), m_zoomMenu, menu_zoom);
    m_popupMenu->insertItem (KGlobal::iconLoader ()->loadIconSet (QString ("window_fullscreen"), KIcon::Small, 0, true), i18n ("&Full Screen"), menu_fullscreen);
    m_popupMenu->setAccel (QKeySequence (Qt::Key_F), menu_fullscreen);
    m_popupMenu->insertSeparator ();
    m_colorMenu = new KMPlayerPopupMenu (this);
    QLabel * label = new QLabel (i18n ("Contrast:"), m_colorMenu);
    m_colorMenu->insertItem (label);
    m_contrastSlider = new QSlider (-100, 100, 10, 0, Qt::Horizontal, m_colorMenu);
    m_colorMenu->insertItem (m_contrastSlider);
    label = new QLabel (i18n ("Brightness:"), m_colorMenu);
    m_colorMenu->insertItem (label);
    m_brightnessSlider = new QSlider (-100, 100, 10, 0, Qt::Horizontal, m_colorMenu);
    m_colorMenu->insertItem (m_brightnessSlider);
    label = new QLabel (i18n ("Hue:"), m_colorMenu);
    m_colorMenu->insertItem (label);
    m_hueSlider = new QSlider (-100, 100, 10, 0, Qt::Horizontal, m_colorMenu);
    m_colorMenu->insertItem (m_hueSlider);
    label = new QLabel (i18n ("Saturation:"), m_colorMenu);
    m_colorMenu->insertItem (label);
    m_saturationSlider = new QSlider (-100, 100, 10, 0, Qt::Horizontal, m_colorMenu);
    m_colorMenu->insertItem (m_saturationSlider);
    m_popupMenu->insertItem (KGlobal::iconLoader ()->loadIconSet (QString ("colorize"), KIcon::Small, 0, true), i18n ("Co&lors"), m_colorMenu);
    m_popupMenu->insertSeparator ();
    m_popupMenu->insertItem (KGlobal::iconLoader ()->loadIconSet (QString ("configure"), KIcon::Small, 0, true), i18n ("&Configure KMPlayer..."), menu_config);
}

void KMPlayerControlPanel::setAutoControls (bool b) {
    m_auto_controls = b;
    if (m_auto_controls) {
        for (int i = 0; i < (int) button_last; i++)
            m_buttons [i]->show ();
        m_posSlider->show ();
    } else { // hide everything
        for (int i = 0; i < (int) button_last; i++)
            m_buttons [i]->hide ();
        m_posSlider->hide ();
    }
}

void KMPlayerControlPanel::showPositionSlider (bool show) {
    if (!m_auto_controls) return;
    int h = show ? button_height_with_slider : button_height_only_buttons;
    m_posSlider->setEnabled (false);
    m_posSlider->setValue (0);
    if (show) {
        m_posSlider->show ();
        m_buttonbox->setMargin (4);
        m_buttonbox->setSpacing (4);
        setEraseColor (parentWidget ()->paletteBackgroundColor ());
    } else {
        m_posSlider->hide ();
        m_buttonbox->setMargin (1);
        m_buttonbox->setSpacing (1);
        setEraseColor (QColor (0, 0, 0));
    }
    for (int i = 0; i < (int) button_last; i++) {
        m_buttons[i]->setMinimumSize (15, h-1);
        m_buttons[i]->setMaximumSize (750, h);
    }
    setMaximumSize (2500, h + (show ? 8 : 2 ));
}

void KMPlayerControlPanel::enableSeekButtons (bool enable) {
    if (!m_auto_controls) return;
    if (enable) {
        m_buttons[button_back]->show ();
        m_buttons[button_forward]->show ();
    } else {
        m_buttons[button_back]->hide ();
        m_buttons[button_forward]->hide ();
    }
}

KDE_NO_EXPORT void KMPlayerControlPanel::setPlaying (bool play) {
    if (play != m_buttons[button_play]->isOn ())
        m_buttons[button_play]->toggle ();
    m_posSlider->setEnabled (false);
    m_posSlider->setValue (0);
    if (!play) {
        showPositionSlider (true);
        enableSeekButtons (true);
    }
}

KDE_NO_EXPORT void KMPlayerControlPanel::setRecording (bool record) {
    if (record != m_buttons[button_record]->isOn ())
        m_buttons[button_record]->toggle ();
}

KDE_NO_EXPORT void KMPlayerControlPanel::setPlayingProgress (int pos) {
    m_posSlider->setEnabled (false);
    if (m_progress_mode != progress_playing) {
        m_posSlider->setMaxValue (m_progress_length);
        m_progress_mode = progress_playing;
    }
    if (m_progress_length <= 0 && pos > 7 * m_posSlider->maxValue ()/8)
        m_posSlider->setMaxValue (m_posSlider->maxValue() * 2);
    else if (m_posSlider->maxValue() < pos)
        m_posSlider->setMaxValue (int (1.4 * m_posSlider->maxValue()));
    m_posSlider->setValue (pos);
    m_posSlider->setEnabled (true);
}

KDE_NO_EXPORT void KMPlayerControlPanel::setLoadingProgress (int pos) {
    m_posSlider->setEnabled (false);
    if (m_progress_mode != progress_loading) {
        m_posSlider->setMaxValue (100);
        m_progress_mode = progress_loading;
    }
    m_posSlider->setValue (pos);
}

KDE_NO_EXPORT void KMPlayerControlPanel::setPlayingLength (int len) {
    m_posSlider->setEnabled (false);
    m_progress_length = len;
    m_posSlider->setMaxValue (m_progress_length);
    m_progress_mode = progress_playing;
    m_posSlider->setEnabled (true);
}

//-----------------------------------------------------------------------------

class KMPlayerPictureWidget : public QWidget {
    KMPlayerView * m_view;
public:
    KDE_NO_CDTOR_EXPORT KMPlayerPictureWidget (QWidget * parent, KMPlayerView * view)
        : QWidget (parent), m_view (view) {}
    KDE_NO_CDTOR_EXPORT ~KMPlayerPictureWidget () {}
protected:
    void mousePressEvent (QMouseEvent *);
};

KDE_NO_EXPORT void KMPlayerPictureWidget::mousePressEvent (QMouseEvent *) {
    m_view->emitPictureClicked ();
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT KMPlayerPopupMenu::KMPlayerPopupMenu (QWidget * parent)
 : KPopupMenu (parent, "kde_kmplayer_popupmenu") {}

KDE_NO_EXPORT void KMPlayerPopupMenu::leaveEvent (QEvent *) {
    emit mouseLeft ();
}

//-----------------------------------------------------------------------------


KDE_NO_CDTOR_EXPORT KMPlayerListViewItem::KMPlayerListViewItem (QListViewItem *p, const ElementPtr & e, KMPlayerPlayListView * lv) : QListViewItem (p), m_elm (e), listview (lv) {
    init ();
}

KDE_NO_CDTOR_EXPORT KMPlayerListViewItem::KMPlayerListViewItem (KMPlayerPlayListView *v, const ElementPtr & e) : QListViewItem (v), m_elm (e), listview (v) {
    init ();
}

KDE_NO_EXPORT void KMPlayerListViewItem::init () {
    setPixmap (0, m_elm->isMrl() ? listview->video_pix : listview->folder_pix);
}


KDE_NO_CDTOR_EXPORT KMPlayerPlayListView::KMPlayerPlayListView (QWidget * parent, KMPlayerView * view) : KListView (parent, "kde_kmplayer_playlist"), m_view (view) {
    addColumn (QString::null);
    header()->hide ();
    setTreeStepSize (15);
    setRootIsDecorated (true);
    setSorting (-1);
    setAcceptDrops (true);
    m_itemmenu = new QPopupMenu (this);
    folder_pix = KGlobal::iconLoader ()->loadIcon (QString ("folder"), KIcon::Small);
    video_pix = KGlobal::iconLoader ()->loadIcon (QString ("video"), KIcon::Small);
    m_itemmenu->insertItem (KGlobal::iconLoader ()->loadIconSet (QString ("editcopy"), KIcon::Small, 0, true), i18n ("&Copy to Clipboard"), this, SLOT (copyToClipboard ()), 0, 0);
    m_itemmenu->insertItem (KGlobal::iconLoader ()->loadIconSet (QString ("bookmark_add"), KIcon::Small, 0, true), i18n ("&Add Bookmark"), this, SLOT (addBookMark ()), 0, 1);
    connect (this, SIGNAL (contextMenuRequested (QListViewItem *, const QPoint &, int)), this, SLOT (contextMenuItem (QListViewItem *, const QPoint &, int)));
}

KDE_NO_CDTOR_EXPORT KMPlayerPlayListView::~KMPlayerPlayListView () {
}

static void populateTree (ElementPtr e, ElementPtr focus, KMPlayerPlayListView * tree, QListViewItem *item) {
    Mrl * mrl = e->mrl ();
    item->setText(0, mrl ? KURL(mrl->src).prettyURL() : QString(e->nodeName()));
    if (focus == e) {
        for (QListViewItem * p = item->parent (); p; p = p->parent ())
            p->setOpen (true);
        tree->setSelected (item, true);
    }
    for (ElementPtr c = e->lastChild (); c; c = c->previousSibling ())
        if (strcmp (c->nodeName (), "#text"))
            populateTree (c, focus, tree, new KMPlayerListViewItem (item, c, tree));
}

void KMPlayerPlayListView::updateTree (ElementPtr root, ElementPtr active) {
    clear ();
    if (!root) return;
    populateTree (root, active, this, new KMPlayerListViewItem (this, root));
}

KDE_NO_EXPORT void KMPlayerPlayListView::contextMenuItem (QListViewItem * vi, const QPoint & p, int) {
    if (vi) {
        KMPlayerListViewItem * item = static_cast <KMPlayerListViewItem *> (vi);
        m_itemmenu->setItemEnabled (1, item->m_elm && (item->m_elm->isMrl () || item->m_elm->isDocument ()));
        m_itemmenu->exec (p);
    } else
        m_view->buttonBar ()->popupMenu ()->exec (p);
}

void KMPlayerPlayListView::copyToClipboard () {
    KMPlayerListViewItem * item = static_cast <KMPlayerListViewItem *> (currentItem ());
    if (item->m_elm) {
        Mrl * mrl = item->m_elm->mrl ();
        QApplication::clipboard()->setText (mrl ? mrl->src : QString (item->m_elm->nodeName ()));
    }
}

void KMPlayerPlayListView::addBookMark () {
    KMPlayerListViewItem * item = static_cast <KMPlayerListViewItem *> (currentItem ());
    if (item->m_elm) {
        Mrl * mrl = item->m_elm->mrl ();
        KURL url (mrl ? mrl->src : QString (item->m_elm->nodeName ()));
        emit addBookMark (url.prettyURL (), url.url ());
    }
}

KDE_NO_EXPORT void KMPlayerPlayListView::dropEvent (QDropEvent * de) {
    m_view->dropEvent (de);
}

KDE_NO_EXPORT void KMPlayerPlayListView::dragEnterEvent (QDragEnterEvent* dee) {
    m_view->dragEnterEvent (dee);
}

//-----------------------------------------------------------------------------

class KMPlayerConsole : public QMultiLineEdit {
public:
    KMPlayerConsole (QWidget * parent, KMPlayerView * view);
protected:
    void contextMenuEvent (QContextMenuEvent * e);
private:
    KMPlayerView * m_view;
};

KDE_NO_CDTOR_EXPORT KMPlayerConsole::KMPlayerConsole (QWidget * parent, KMPlayerView * view) : QMultiLineEdit (parent, "kde_kmplayer_console"), m_view (view) {}

KDE_NO_EXPORT void KMPlayerConsole::contextMenuEvent (QContextMenuEvent * e) {
    m_view->buttonBar ()->popupMenu ()->exec (e->globalPos ());
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT KMPlayerView::KMPlayerView (QWidget *parent, const char *name)
  : KMediaPlayer::View (parent, name),
    m_image (0L),
    m_buttonbar (0L),
    m_volume_slider (0L),
    m_mixer_object ("kicker"),
    m_controlpanel_mode (CP_Show),
    m_old_controlpanel_mode (CP_Show),
    controlbar_timer (0),
    popup_timer (0),
    popdown_timer (0),
    m_keepsizeratio (false),
    m_playing (false),
    m_mixer_init (false),
    m_inVolumeUpdate (false),
    m_revert_fullscreen (false),
    m_popup_clicked (false)
{
    setEraseColor (QColor (0, 0, 255));
}

KDE_NO_EXPORT void KMPlayerView::dropEvent (QDropEvent * de) {
    KURL url;
    if (KURLDrag::canDecode (de)) {
        KURL::List sl;
        KURLDrag::decode (de, sl);
        if (sl.count () > 0)
            url = sl.first();
    } else if (QTextDrag::canDecode (de)) {
        QString text;
        QTextDrag::decode (de, text);
        url = KURL (text);
    }
    if (url.isValid ()) {
        m_widgetstack->visibleWidget ()->setFocus ();
        emit urlDropped (url);
        de->accept ();
    }
}

KDE_NO_EXPORT void KMPlayerView::ctrlButtonMouseEntered () {
    if (!popup_timer && !m_buttonbar->popupMenu ()->isVisible ()) {
        m_popup_clicked = false;
        popup_timer = startTimer (400);
    }
}

KDE_NO_EXPORT void KMPlayerView::ctrlButtonClicked () {
    if (popup_timer) {
        killTimer (popup_timer);
        popup_timer = 0;
    }
    m_popup_clicked = true;
    showPopupMenu ();
}

KDE_NO_EXPORT void KMPlayerView::popupMenuMouseLeft () {
    if (!popdown_timer && !m_popup_clicked)
        popdown_timer = startTimer (400);
}

KDE_NO_EXPORT void KMPlayerView::dragEnterEvent (QDragEnterEvent* dee) {
    if (KURLDrag::canDecode (dee)) {
        dee->accept ();
    } else if (QTextDrag::canDecode (dee)) {
        QString text;
        if (KURL (QTextDrag::decode (dee, text)).isValid ())
            dee->accept ();
    }
}

KDE_NO_EXPORT void KMPlayerView::init () {
    //setBackgroundMode(Qt::NoBackground);
    QPalette pal (QColor (64, 64,64), QColor (32, 32, 32));
    QVBoxLayout * viewbox = new QVBoxLayout (this, 0, 0);
    m_dockarea = new KDockArea (this, "kde_kmplayer_dock_area");
    m_dock_video = new KDockWidget (m_dockarea->manager (), 0, KGlobal::iconLoader ()->loadIcon (QString ("kmplayer"), KIcon::Small), m_dockarea);
    m_dock_video->setDockSite (KDockWidget::DockLeft | KDockWidget::DockBottom | KDockWidget::DockRight | KDockWidget::DockTop);
    m_dock_video->setEnableDocking(KDockWidget::DockNone);
    m_layer = new KMPlayerViewLayer (m_dock_video, this);
    m_dock_video->setWidget (m_layer);
    m_dockarea->setMainDockWidget (m_dock_video);
    m_dock_playlist = m_dockarea->createDockWidget (QString ("PlayList"), KGlobal::iconLoader ()->loadIcon (QString ("player_playlist"), KIcon::Small));
    m_playlist = new KMPlayerPlayListView (m_dock_playlist, this);
    m_playlist->horizontalScrollBar ()->setPalette (pal);
    m_playlist->verticalScrollBar ()->setPalette (pal);
    m_playlist->setPaletteBackgroundColor (QColor (0, 0, 0));
    m_playlist->setPaletteForegroundColor (QColor (0xB2, 0xB2, 0xB2));
    m_dock_playlist->setWidget (m_playlist);
    viewbox->addWidget (m_dockarea);
    QVBoxLayout * layerbox = new QVBoxLayout (m_layer, 0, 0);
    m_buttonbar = new KMPlayerControlPanel (m_layer);
    m_buttonbar->setMaximumSize (2500, buttonBar ()->maximumSize ().height ());
    m_holder = new KMPlayerViewerHolder (m_layer, this);
    m_widgetstack = new QWidgetStack (m_holder);
    m_viewer = new KMPlayerViewer (m_widgetstack, this);
    m_widgettypes [WT_Video] = m_viewer;
    layerbox->addWidget (m_holder);
    layerbox->addWidget (m_buttonbar);
#if KDE_IS_VERSION(3,1,90)
    setVideoWidget (m_layer);
#endif

    m_multiedit = new KMPlayerConsole (m_widgetstack, this);
    m_multiedit->setReadOnly (true);
    m_multiedit->setFamily ("courier");
    m_multiedit->setPaper (QBrush (QColor (0, 0, 0)));
    m_multiedit->setColor (QColor (0xB2, 0xB2, 0xB2));
    m_widgettypes[WT_Console] = m_multiedit;
    m_multiedit->horizontalScrollBar ()->setPalette (pal);
    m_multiedit->verticalScrollBar ()->setPalette (pal);
    m_widgettypes[WT_Picture] = new KMPlayerPictureWidget (m_widgetstack, this);

    m_widgetstack->addWidget (m_viewer);
    m_widgetstack->addWidget (m_multiedit);
    m_widgetstack->addWidget (m_widgettypes[WT_Picture]);

    setFocusPolicy (QWidget::ClickFocus);

    connect (m_buttonbar->button (KMPlayerControlPanel::button_config), SIGNAL (clicked ()), this, SLOT (ctrlButtonClicked ()));
    connect (m_buttonbar->button (KMPlayerControlPanel::button_config), SIGNAL (mouseEntered ()), this, SLOT (ctrlButtonMouseEntered ()));
    connect (m_buttonbar->popupMenu(), SIGNAL (mouseLeft ()), this, SLOT (popupMenuMouseLeft ()));
    connect (m_buttonbar->playerMenu(), SIGNAL (mouseLeft ()), this, SLOT (popupMenuMouseLeft ()));
    connect (m_buttonbar->zoomMenu(), SIGNAL (mouseLeft ()), this, SLOT (popupMenuMouseLeft ()));
    connect (m_buttonbar->viewMenu(), SIGNAL (mouseLeft ()), this, SLOT (popupMenuMouseLeft ()));
    connect (m_buttonbar->colorMenu(), SIGNAL (mouseLeft ()), this, SLOT (popupMenuMouseLeft ()));
    setAcceptDrops (true);
    m_holder->resizeEvent (0L);
    kdDebug() << "KMPlayerView " << (unsigned long) (m_viewer->winId()) << endl;

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

KDE_NO_CDTOR_EXPORT KMPlayerView::~KMPlayerView () {
    delete m_image;
    if (m_layer->parent () != this)
        delete m_layer;
}

void KMPlayerView::showPlaylist () {
    m_dock_playlist->manualDock (m_dock_video, KDockWidget::DockLeft, 30);
}

bool KMPlayerView::setPicture (const QString & path) {
    delete m_image;
    if (path.isEmpty ())
        m_image = 0L;
    else {
        m_image = new QPixmap (path);
        if (m_image->isNull ()) {
            delete m_image;
            m_image = 0L;
            kdDebug() << "KMPlayerView::setPicture failed " << path << endl;
        }
    }
    if (!m_image) {
        m_widgetstack->raiseWidget (m_viewer);
    } else {
        m_widgettypes[WT_Picture]->setPaletteBackgroundPixmap (*m_image);
        m_widgetstack->raiseWidget (m_widgettypes[WT_Picture]);
    }
    return m_image;
}

KDE_NO_EXPORT void KMPlayerView::updateVolume () {
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
            m_mixer_label = new QLabel (i18n ("Volume:"), m_buttonbar->popupMenu ());
            m_buttonbar->popupMenu ()->insertItem (m_mixer_label, -1, 4);
            m_volume_slider = new QSlider (0, 100, 10, volume, Qt::Horizontal, m_buttonbar->popupMenu ());
            connect(m_volume_slider, SIGNAL(valueChanged(int)), this,SLOT(setVolume(int)));
            m_buttonbar->popupMenu ()->insertItem (m_volume_slider, KMPlayerControlPanel::menu_volume, 5);
            m_buttonbar->popupMenu ()->insertSeparator (6);
        } else {
            m_inVolumeUpdate = true;
            m_volume_slider->setValue (volume);
            m_inVolumeUpdate = false;
        }
    } else if (m_volume_slider) {
        m_buttonbar->popupMenu ()->removeItemAt (6);
        m_buttonbar->popupMenu ()->removeItemAt (5);
        m_buttonbar->popupMenu ()->removeItemAt (4);
        m_volume_slider = 0L;
    }
    m_mixer_init = true;
}

void KMPlayerView::showWidget (WidgetType wt) {
    m_widgetstack->raiseWidget (m_widgettypes [wt]);
}

void KMPlayerView::setControlPanelMode (ControlPanelMode m) {
    killTimer (controlbar_timer);
    m_old_controlpanel_mode = m_controlpanel_mode = m;
    if (m_playing && isFullScreen())
        m_controlpanel_mode = CP_AutoHide;
    if (m_buttonbar)
        if (m_controlpanel_mode == CP_Show)
            m_buttonbar->show ();
        else if (m_controlpanel_mode == CP_AutoHide) { 
            if (m_playing)
                delayedShowButtons (false);
            else
                m_buttonbar->show ();
        } else
            m_buttonbar->hide ();
    m_holder->setMouseTracking (m_controlpanel_mode == CP_AutoHide && m_playing);
}

KDE_NO_EXPORT void KMPlayerView::delayedShowButtons (bool show) {
    if (m_controlpanel_mode != CP_AutoHide || controlbar_timer ||
        (m_buttonbar &&
         (show && m_buttonbar->isVisible ()) || 
         (!show && !m_buttonbar->isVisible ())))
        return;
    controlbar_timer = startTimer (500);
}

KDE_NO_EXPORT void KMPlayerView::setVolume (int vol) {
    if (m_inVolumeUpdate) return;
    QByteArray data;
    QDataStream arg( data, IO_WriteOnly );
    arg << vol;
    if (!kapp->dcopClient()->send (m_mixer_object, "Mixer0", "setMasterVolume(int)", data))
        kdWarning() << "Failed to update volume" << endl;
}

KDE_NO_EXPORT void  KMPlayerView::updateLayout () {
    m_holder->resizeEvent (0l);
}

KDE_NO_EXPORT void KMPlayerView::timerEvent (QTimerEvent * e) {
    if (e->timerId () == controlbar_timer) {
        controlbar_timer = 0;
        if (!m_playing)
            return;
        int vert_buttons_pos = m_layer->height ();
        int mouse_pos = m_layer->mapFromGlobal (QCursor::pos ()).y();
        int cp_height = m_buttonbar->maximumSize ().height ();
        bool mouse_on_buttons = (//m_layer->hasMouse () && 
                mouse_pos >= vert_buttons_pos-cp_height &&
                mouse_pos <= vert_buttons_pos);
        if (m_buttonbar)
            if (mouse_on_buttons && !m_buttonbar->isVisible ())
                m_buttonbar->show ();
            else if (!mouse_on_buttons && m_buttonbar->isVisible ())
                m_buttonbar->hide ();
    } else if (e->timerId () == popup_timer) {
        popup_timer = 0;
        if (m_buttonbar->button (KMPlayerControlPanel::button_config)->hasMouse () && !m_buttonbar->popupMenu ()->isVisible ())
            showPopupMenu ();
    } else if (e->timerId () == popdown_timer) {
        popdown_timer = 0;
        if (m_buttonbar->popupMenu ()->isVisible () && !m_buttonbar->popupMenu ()->hasMouse () && !m_buttonbar->playerMenu ()->hasMouse () && !m_buttonbar->viewMenu ()->hasMouse () && !m_buttonbar->zoomMenu ()->hasMouse () && !m_buttonbar->colorMenu ()->hasMouse () && !m_buttonbar->bookmarkMenu ()->hasMouse ())
            if (!(m_buttonbar->bookmarkMenu ()->isVisible () && static_cast <QWidget *> (m_buttonbar->bookmarkMenu ()) != QWidget::keyboardGrabber ()))
                // not if user entered the bookmark sub menu or if I forgot one
                m_buttonbar->popupMenu ()->hide ();
    }
    killTimer (e->timerId ());
}

void KMPlayerView::addText (const QString & str) {
    tmplog += str;
    int pos = tmplog.findRev (QChar ('\n'));
    if (pos >= 0) {
        m_multiedit->append (tmplog.left (pos));
        tmplog = tmplog.mid (pos+1);
        while (5000 < m_multiedit->numLines ())
            m_multiedit->removeLine (0);
    }
}

void KMPlayerView::addFullscreenAction (const QString & title, const KShortcut & c, QObject * o, const char * s, const char * name) {
    KAction * action = m_layer->m_collection->action (name);
    if (action)
        m_layer->m_collection->remove (action);
    new KAction (title, c, o, s, m_layer->m_collection, name);
}
/* void KMPlayerView::print (QPrinter *pPrinter)
{
    QPainter printpainter;
    printpainter.begin (pPrinter);

    // TODO: add your printing code here

    printpainter.end ();
}*/

KDE_NO_EXPORT void KMPlayerView::videoStart () {
    if (m_playing) return; //FIXME: make symetric with videoStop
    if (m_widgetstack->visibleWidget () == m_widgettypes[WT_Picture])
        m_widgetstack->raiseWidget (m_viewer);
    m_playing = true;
    m_revert_fullscreen = !isFullScreen();
    setControlPanelMode (m_old_controlpanel_mode);
}

KDE_NO_EXPORT void KMPlayerView::videoStop () {
    if (m_buttonbar && m_controlpanel_mode == CP_AutoHide) {
        m_buttonbar->show ();
        m_holder->setMouseTracking (false);
    }
    m_playing = false;
    XClearWindow (qt_xdisplay(), m_viewer->embeddedWinId ());
}

KDE_NO_EXPORT void KMPlayerView::showPopupMenu () {
    updateVolume ();
    int cp_height = m_buttonbar->maximumSize ().height ();
    m_buttonbar->popupMenu ()->exec (m_buttonbar->button (KMPlayerControlPanel::button_config)->mapToGlobal (QPoint (0, cp_height)));
}

KDE_NO_EXPORT void KMPlayerView::leaveEvent (QEvent *) {
    if (m_controlpanel_mode == CP_AutoHide && m_playing)
        delayedShowButtons (false);
}

KDE_NO_EXPORT void KMPlayerView::reset () {
    if (m_revert_fullscreen && isFullScreen())
        m_buttonbar->popupMenu ()->activateItemAt (m_buttonbar->popupMenu ()->indexOf (KMPlayerControlPanel::menu_fullscreen)); 
        //m_layer->fullScreen ();
    videoStop ();
    m_viewer->show ();
}

bool KMPlayerView::isFullScreen () const {
    return m_layer->isFullScreen ();
}

void KMPlayerView::fullScreen () {
    if (!m_layer->isFullScreen()) {
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
        m_layer->fullScreen();
        m_buttonbar->popupMenu ()->setItemVisible (KMPlayerControlPanel::menu_zoom, false);
        m_widgetstack->visibleWidget ()->setFocus ();
    } else {
        if (m_sreensaver_disabled)
            m_sreensaver_disabled = !kapp->dcopClient()->send
                ("kdesktop", "KScreensaverIface", "enable(bool)", "true");
        m_layer->fullScreen();
        m_buttonbar->popupMenu ()->setItemVisible (KMPlayerControlPanel::menu_zoom, true);
    }
    setControlPanelMode (m_old_controlpanel_mode);
    emit fullScreenChanged ();
}

KDE_NO_EXPORT bool KMPlayerView::x11Event (XEvent * e) {
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
            printf ("colormap notify\n");
            return true;*/
        case MotionNotify:
            if (m_playing && e->xmotion.window == m_viewer->embeddedWinId ())
                delayedShowButtons (e->xmotion.y > m_viewer->height () -
                                    m_buttonbar->maximumSize ().height ());
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

KDE_NO_CDTOR_EXPORT KMPlayerViewer::KMPlayerViewer (QWidget *parent, KMPlayerView * view)
  : QXEmbed (parent), m_aspect (0.0),
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
    embed (XCreateSimpleWindow (qt_xdisplay(), view->winId (), 0, 0, width(), height(), 1, 0, 0));
    XClearWindow (qt_xdisplay(), embeddedWinId ());
}

KDE_NO_CDTOR_EXPORT KMPlayerViewer::~KMPlayerViewer () {
}
    
KDE_NO_EXPORT void KMPlayerViewer::mouseMoveEvent (QMouseEvent * e) {
    if (e->state () == Qt::NoButton) {
        int cp_height = m_view->buttonBar ()->maximumSize ().height ();
        m_view->delayedShowButtons (e->y () > height () - cp_height);
    }
}

void KMPlayerViewer::setAspect (float a) {
    float da = m_aspect - a;
    if (da < 0) da *= -1;
    if (da < 0.0001)
        return;
    m_aspect = a;
    QWidget * w = static_cast <QWidget *> (parent ());
    QResizeEvent ev (w->size (), w->size ());
    QApplication::sendEvent (w, &ev);
    emit aspectChanged ();
}

KDE_NO_EXPORT int KMPlayerViewer::heightForWidth (int w) const {
    if (m_aspect <= 0.01)
        return 0;
    return int (w/m_aspect); 
}

KDE_NO_EXPORT void KMPlayerViewer::dropEvent (QDropEvent * de) {
    m_view->dropEvent (de);
}

KDE_NO_EXPORT void KMPlayerViewer::dragEnterEvent (QDragEnterEvent* dee) {
    m_view->dragEnterEvent (dee);
}
/*
*/
void KMPlayerViewer::sendKeyEvent (int key) {
    char buf[2] = { char (key), '\0' }; 
    KeySym keysym = XStringToKeysym (buf);
    XKeyEvent event = {
        XKeyPress, 0, true,
        qt_xdisplay (), embeddedWinId (), qt_xrootwin(), embeddedWinId (),
        /*time*/ 0, 0, 0, 0, 0,
        0, XKeysymToKeycode (qt_xdisplay (), keysym), true
    };
    XSendEvent (qt_xdisplay(), embeddedWinId (), FALSE, KeyPressMask, (XEvent *) &event);
    XFlush (qt_xdisplay ());
}

KDE_NO_EXPORT void KMPlayerViewer::sendConfigureEvent () {
    XConfigureEvent c = {
        ConfigureNotify, 0UL, True,
        qt_xdisplay (), embeddedWinId (), winId (),
        x (), y (), width (), height (),
        0, None, False
    };
    XSendEvent(qt_xdisplay(), c.event, TRUE, StructureNotifyMask, (XEvent*) &c);
    XFlush (qt_xdisplay ());
}

KDE_NO_EXPORT void KMPlayerViewer::contextMenuEvent (QContextMenuEvent * e) {
    m_view->buttonBar ()->popupMenu ()->exec (e->globalPos ());
}

#include "kmplayerview.moc"
