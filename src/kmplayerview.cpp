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
#include <qpainter.h>
#include <qmetaobject.h>
#include <qlayout.h>
#include <qpushbutton.h>
#include <qpixmap.h>
#include <qmultilineedit.h>
#include <qapplication.h>
#include <qiconset.h>
#include <qaccel.h>
#include <qcursor.h>
#include <qpopupmenu.h>
#include <qkeysequence.h>
#include <qslider.h>
#include <qlabel.h>
#include <qdatastream.h>
#include <qpixmap.h>
#include <qpainter.h>

#include <X11/Xlib.h>
#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>

static const int XKeyPress = KeyPress;
#undef KeyPress
#undef Always
#undef Never
#undef Status
#undef Unsorted

static const int button_height = 11;

// application specific includes
#include "kmplayerview.h"
//#include "kmplayer.h"

#include <kstaticdeleter.h>
#include <kdebug.h>
#include <klocale.h>
#include <kapplication.h>
#include <kurldrag.h>
#include <kpopupmenu.h>
#include <dcopclient.h>
#ifdef USE_ARTS
#include <arts/kartsdispatcher.h>
#include <arts/soundserver.h>
#include <arts/kartsserver.h>
#include <arts/kartsfloatwatch.h>
#endif

typedef int (*QX11EventFilter) (XEvent*);
static QX11EventFilter oldFilter = 0;
extern QX11EventFilter qt_set_x11_event_filter (QX11EventFilter filter);

static int qxembed_x11_event_filter (XEvent* e) {
    switch (e->type) {
        case MapRequest: {
            //printf("maprequest 0x%x 0x%x\n", e->xmap.event, e->xmap.window);
            QWidget* w = QWidget::find (e->xmap.window);
            if (w && w->metaObject ()->inherits ("KMPlayerViewer")) {
                w->show ();
                return true;
            }
        }
    }
    if (oldFilter && oldFilter != (QX11EventFilter) qt_set_x11_event_filter)
        return oldFilter (e);
    return false;
}

class KMPlayerViewStatic {
public:
    KMPlayerViewStatic ();
    ~KMPlayerViewStatic ();

#ifdef USE_ARTS
    void getDispatcher();
    void releaseDispatcher();
private:
    KArtsDispatcher *dispatcher;
#endif
    int use_count;
};

static KMPlayerViewStatic * kmplayerview_static = 0L;

KMPlayerViewStatic::KMPlayerViewStatic () 
#ifdef USE_ARTS
    : dispatcher ((KArtsDispatcher*) 0), use_count(0) {
#else
    : use_count(0) {
#endif
    printf ("KMPlayerViewStatic::KMPlayerViewStatic\n");
    oldFilter = qt_set_x11_event_filter (qxembed_x11_event_filter);
}

KMPlayerViewStatic::~KMPlayerViewStatic () {
    printf ("KMPlayerViewStatic::~KMPlayerViewStatic\n");
#ifdef USE_ARTS
    delete dispatcher;
#endif
    qt_set_x11_event_filter (oldFilter);
    kmplayerview_static = 0L;
}

#ifdef USE_ARTS
void KMPlayerViewStatic::getDispatcher() {
    if (!dispatcher) {
        dispatcher = new KArtsDispatcher;
        use_count = 1;
    } else
        use_count++;
}

void KMPlayerViewStatic::releaseDispatcher() {
    if (--use_count <= 0) {
        delete dispatcher;
        dispatcher = 0L;
    }
}
#endif

static KStaticDeleter <KMPlayerViewStatic> kmplayerViewStatic;

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

KMPlayerViewLayer::KMPlayerViewLayer (KMPlayerView * parent, QBoxLayout * b)
 : QWidget (parent),
   m_view (parent),
   m_box (b),
   m_accel (0L),
   m_fullscreen (false) {
    setEraseColor (QColor (255, 0, 0));
}

void KMPlayerViewLayer::fullScreen () {
    if (m_fullscreen) {
        showNormal ();
        reparent (m_view, 0, QPoint (0, 0), true);
        m_box->addWidget (this);
        delete m_accel;
        m_accel = 0L;
    } else {
        reparent (0L, 0, QPoint (0, 0), true);
        showFullScreen ();
        m_accel = new QAccel (this);
        int id = m_accel->insertItem (QKeySequence (Qt::Key_Escape));
        m_accel->connectItem (id, this, SLOT (accelActivated ()));
    }
    m_fullscreen = !m_fullscreen;
    m_view->popupMenu ()->setItemChecked (KMPlayerView::menu_fullscreen, 
                                          m_fullscreen);
}

void KMPlayerViewLayer::accelActivated () {
    m_view->popupMenu ()->activateItemAt (m_view->popupMenu ()->indexOf (KMPlayerView::menu_fullscreen)); 
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
private:
    KMPlayerView * m_view;
};

inline
KMPlayerViewerHolder::KMPlayerViewerHolder (QWidget * pa, KMPlayerView * view)
 : QWidget (pa), m_view (view) {
    setEraseColor (QColor (0, 0, 0));
    setAcceptDrops (true);
}

void KMPlayerViewerHolder::mouseMoveEvent (QMouseEvent * e) {
    if (e->state () == Qt::NoButton) {
        int vert_buttons_pos = height ();
        m_view->delayedShowButtons (e->y() > vert_buttons_pos - button_height &&
                                    e->y() < vert_buttons_pos);
    }
}

void KMPlayerViewerHolder::resizeEvent (QResizeEvent *) {
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
    m_view->viewer ()->setGeometry (x, y, w, h);
}

void KMPlayerViewerHolder::dropEvent (QDropEvent * de) {
    m_view->dropEvent (de);
}

void KMPlayerViewerHolder::dragEnterEvent (QDragEnterEvent* dee) {
    m_view->dragEnterEvent (dee);
}
//-----------------------------------------------------------------------------

class KMPlayerSlider : public QSlider {
public:
    KMPlayerSlider (Qt::Orientation orient, QWidget *parent, KMPlayerView *view)
        : QSlider (orient, parent), m_view (view) {
    }
protected:
    void enterEvent (QEvent *);
private:
    KMPlayerView * m_view;
};

void KMPlayerSlider::enterEvent (QEvent *) {
    m_view->delayedShowButtons (false);
}

//-----------------------------------------------------------------------------

static QPushButton * ctrlButton (QWidget * w, QBoxLayout * l, const char * const * p, int key = 0) {
    QPushButton * b = new QPushButton (QIconSet (QPixmap(p)), QString::null, w);
    b->setMinimumSize (15, button_height-1);
    b->setMaximumSize (750, button_height);
    b->setFocusPolicy (QWidget::NoFocus);
    b->setFlat (true);
    if (key)
        b->setAccel (QKeySequence (key));
    l->addWidget (b);
    return b;
}

KMPlayerView::KMPlayerView (QWidget *parent, const char *name)
  : KMediaPlayer::View (parent, name),
    m_image (0L),
    m_playerMenu (0L),
    m_posSlider (0L),
    m_artsserver (0L),
    m_svc (0L),
    m_watch (0L),
    delayed_timer (0),
    m_keepsizeratio (false),
    m_show_console_output (false),
    m_auto_hide_buttons (false),
    m_playing (false),
    m_use_arts (false),
    m_inVolumeUpdate (false)
{
    if (!kmplayerview_static)
        kmplayerview_static = kmplayerViewStatic.setObject (new KMPlayerViewStatic());
    setEraseColor (QColor (0, 0, 255));
}

void KMPlayerView::dropEvent (QDropEvent * de) {
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
        emit urlDropped (url);
        de->accept ();
    }
}

void KMPlayerView::dragEnterEvent (QDragEnterEvent* dee) {
    if (KURLDrag::canDecode (dee)) {
        dee->accept ();
    } else if (QTextDrag::canDecode (dee)) {
        QString text;
        if (KURL (QTextDrag::decode (dee, text)).isValid ())
            dee->accept ();
    }
}

void KMPlayerView::init () {
    //setBackgroundMode(Qt::NoBackground);
    QVBoxLayout * viewbox = new QVBoxLayout (this, 0, 0);
    m_layer = new KMPlayerViewLayer (this, viewbox);
    viewbox->addWidget (m_layer);
    QVBoxLayout * layerbox = new QVBoxLayout (m_layer, 0, 0);
    m_buttonbar = new QWidget (m_layer);
    m_holder = new KMPlayerViewerHolder (m_layer, this);
    m_viewer = new KMPlayerViewer (m_holder, this);
    layerbox->addWidget (m_holder);
#if KDE_IS_VERSION(3,1,90)
    setVideoWidget (m_layer);
#endif
    
    layerbox->addWidget (m_buttonbar);
    m_posSlider = new KMPlayerSlider (Qt::Horizontal, m_layer, this);
    layerbox->addWidget (m_posSlider);

    QHBoxLayout * buttonbox = new QHBoxLayout (m_buttonbar, 1);
    m_buttonbar->setMaximumSize (2500, button_height);
    m_buttonbar->setEraseColor (QColor (0, 0, 0));
    m_configButton = ctrlButton (m_buttonbar, buttonbox, config_xpm);
    m_backButton = ctrlButton (m_buttonbar, buttonbox, back_xpm);
    m_playButton = ctrlButton (m_buttonbar, buttonbox, play_xpm, Qt::Key_R);
    m_forwardButton = ctrlButton (m_buttonbar, buttonbox, forward_xpm);
    m_stopButton = ctrlButton (m_buttonbar, buttonbox, stop_xpm, Qt::Key_S);
    m_pauseButton = ctrlButton (m_buttonbar, buttonbox, pause_xpm, Qt::Key_P);
    m_recordButton = ctrlButton (m_buttonbar, buttonbox, record_xpm);
    m_broadcastButton = ctrlButton (m_buttonbar, buttonbox, broadcast_xpm);
    m_playButton->setToggleButton (true);
    m_stopButton->setToggleButton (true);
    m_recordButton->setToggleButton (true);
    m_broadcastButton->setToggleButton (true);
    m_broadcastButton->hide ();

    m_popupMenu = new QPopupMenu (m_layer);
    m_playerMenu = new QPopupMenu (m_layer);
    m_playerMenu->setEnabled (false);
    m_popupMenu->insertItem (i18n ("&Play with"), m_playerMenu, menu_player);
    m_popupMenu->setItemVisible (menu_player, false);
    m_bookmarkMenu = new KPopupMenu (m_layer);
    m_popupMenu->insertItem (i18n("&Bookmarks"), m_bookmarkMenu, menu_bookmark);
    m_zoomMenu = new QPopupMenu (m_layer);
    m_zoomMenu->insertItem (i18n ("50%"), menu_zoom50);
    m_zoomMenu->insertItem (i18n ("100%"), menu_zoom100);
    m_zoomMenu->insertItem (i18n ("150%"), menu_zoom150);
    m_popupMenu->insertItem (i18n ("&Zoom"), m_zoomMenu, menu_zoom);
    m_popupMenu->insertItem (i18n ("&Full Screen"),
          this, SLOT (fullScreen()), QKeySequence (Qt::Key_F), menu_fullscreen);
    m_popupMenu->insertSeparator ();
    QPopupMenu * colorMenu = new QPopupMenu (m_layer);
    QLabel * label = new QLabel (i18n ("Contrast:"), colorMenu);
    colorMenu->insertItem (label);
    m_contrastSlider = new QSlider (-100, 100, 10, 0, Qt::Horizontal, colorMenu);
    colorMenu->insertItem (m_contrastSlider);
    label = new QLabel (i18n ("Brightness:"), colorMenu);
    colorMenu->insertItem (label);
    m_brightnessSlider = new QSlider (-100, 100, 10, 0, Qt::Horizontal, colorMenu);
    colorMenu->insertItem (m_brightnessSlider);
    label = new QLabel (i18n ("Hue:"), colorMenu);
    colorMenu->insertItem (label);
    m_hueSlider = new QSlider (-100, 100, 10, 0, Qt::Horizontal, colorMenu);
    colorMenu->insertItem (m_hueSlider);
    label = new QLabel (i18n ("Saturation:"), colorMenu);
    colorMenu->insertItem (label);
    m_saturationSlider = new QSlider (-100, 100, 10, 0, Qt::Horizontal, colorMenu);
    colorMenu->insertItem (m_saturationSlider);
    m_popupMenu->insertItem (i18n ("Co&lors"), colorMenu);
    m_popupMenu->insertSeparator ();
    m_popupMenu->insertItem (i18n ("&Configure KMPlayer..."), menu_config);

    QVBoxLayout * viewerbox = new QVBoxLayout (m_viewer, 0, 0);
    m_multiedit = new QMultiLineEdit (m_viewer, "ConsoleOutput");
    m_multiedit->setReadOnly (true);
    m_multiedit->setFamily ("courier");
    m_multiedit->setPaper (QBrush (QColor (0, 0, 0)));
    m_multiedit->setColor (QColor (0xB2, 0xB2, 0xB2));
    QPalette pal (QColor (64, 64,64), QColor (32, 32, 32));
    m_multiedit->horizontalScrollBar ()->setPalette (pal);
    m_multiedit->verticalScrollBar ()->setPalette (pal);
    viewerbox->addWidget (m_multiedit);
    m_multiedit->hide ();

    setFocusPolicy (QWidget::ClickFocus);

    connect (m_viewer, SIGNAL (aboutToPlay ()), this, SLOT (startsToPlay ()));
    connect (m_configButton, SIGNAL (clicked ()), this, SLOT (showPopupMenu()));

    setAcceptDrops (true);
    m_holder->resizeEvent (0L);
    kdDebug() << "KMPlayerView " << (unsigned long) (m_viewer->winId()) << endl;

}

KMPlayerView::~KMPlayerView () {
    delete m_image;
    setUseArts (false);
    if (m_layer->parent () != this)
        delete m_layer;
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
        } else
            m_multiedit->hide ();
    }
    m_viewer->setMouseTracking (m_viewer->hasMouseTracking ());
    m_viewer->repaint ();
    return m_image;
}

void KMPlayerView::updateUseArts () {
#ifdef USE_ARTS
    if (m_use_arts && !m_artsserver) {
        kmplayerview_static->getDispatcher();
        m_artsserver = new Arts::SoundServerV2;
        *m_artsserver = KArtsServer().server();
        m_svc = new Arts::StereoVolumeControl;
        if (m_artsserver && !m_artsserver->isNull()) {
            m_arts_label = new QLabel (i18n ("Volume:"), m_popupMenu);
            m_popupMenu->insertItem (m_arts_label, -1, 4);
            m_slider = new QSlider (0, 100, 10, 40, Qt::Horizontal, m_popupMenu);
            connect(m_slider, SIGNAL(valueChanged(int)), this,SLOT(setVolume(int)));
            *m_svc = m_artsserver->outVolume();
            m_watch = new KArtsFloatWatch(*m_svc, "scaleFactor_changed", this);
            connect (m_watch, SIGNAL (valueChanged (float)), 
                    this, SLOT (updateVolume (float)));
            m_popupMenu->insertItem (m_slider, menu_volume, 5);
            m_popupMenu->insertSeparator (6);
        }
    }
#endif
}

void KMPlayerView::setUseArts (bool b) {
#ifdef USE_ARTS
    if (m_use_arts && !b && m_artsserver) {
        m_popupMenu->removeItemAt (6);
        m_popupMenu->removeItemAt (5);
        m_popupMenu->removeItemAt (4);
        delete m_watch;
        delete m_artsserver;
        delete m_svc;
        m_watch = 0L;
        m_artsserver = 0L;
        m_svc = 0L;
        kmplayerview_static->releaseDispatcher();
    }
#endif
    m_use_arts = b;
}

void KMPlayerView::setAutoHideButtons (bool b) {
    killTimers ();
    m_auto_hide_buttons = b;
    if (b && m_playing)
        m_buttonbar->hide ();
    else
        m_buttonbar->show ();
    m_viewer->setMouseTracking (b && m_playing);
    m_viewer->parentWidget ()->setMouseTracking (b && m_playing);
    m_posSlider->setMouseTracking (b && m_playing);
}

void KMPlayerView::delayedShowButtons (bool show) {
    if (!m_auto_hide_buttons || delayed_timer ||
        (show && m_buttonbar->isVisible ()) || 
        (!show && !m_buttonbar->isVisible ()))
        return;
    delayed_timer = startTimer (300);
}

void KMPlayerView::setVolume (int vol) {
#ifdef USE_ARTS
    if (m_inVolumeUpdate) return;
    float volume = float (0.0004*vol*vol);
    printf("setVolume %d -> %.4f\n", vol, volume);
    m_svc->scaleFactor (volume);
#endif
}

void KMPlayerView::updateVolume (float v) {
#ifdef USE_ARTS
    m_inVolumeUpdate = true;
    printf("updateVolume %.4f\n", v);
    m_slider->setValue (int (sqrt(v*10000.0/4)));
    m_inVolumeUpdate = false;
#endif
}

void  KMPlayerView::updateLayout () {
    m_holder->resizeEvent (0l);
}

void KMPlayerView::timerEvent (QTimerEvent * e) {
    killTimer (e->timerId ());
    delayed_timer = 0;
    if (!m_playing)
        return;
    int vert_buttons_pos = m_layer->height ();
    if (m_posSlider->isVisible ())
        vert_buttons_pos -= m_posSlider->height ();
    int mouse_pos = m_layer->mapFromGlobal (QCursor::pos ()).y();
    bool mouse_on_buttons = (m_layer->hasMouse () && 
                             mouse_pos >= vert_buttons_pos - button_height && 
                             mouse_pos <= vert_buttons_pos);
    if (mouse_on_buttons && !m_buttonbar->isVisible ())
        m_buttonbar->show ();
    else if (!mouse_on_buttons && m_buttonbar->isVisible ())
        m_buttonbar->hide ();
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

/* void KMPlayerView::print (QPrinter *pPrinter)
{
    QPainter printpainter;
    printpainter.begin (pPrinter);

    // TODO: add your printing code here

    printpainter.end ();
}*/

void KMPlayerView::startsToPlay () {
    m_multiedit->hide ();
    m_playing = true;
    if (m_auto_hide_buttons) {
        m_buttonbar->hide ();
        m_viewer->setMouseTracking (true);
        m_viewer->parentWidget ()->setMouseTracking (true);
        m_posSlider->setMouseTracking (true);
    }
}

void KMPlayerView::showPopupMenu () {
#ifdef USE_ARTS
    if (m_use_arts) {
        updateUseArts ();
        updateVolume(m_svc->scaleFactor());
    }
#endif
    m_popupMenu->exec (m_configButton->mapToGlobal (QPoint (0, button_height)));
}

void KMPlayerView::leaveEvent (QEvent *) {
    if (m_auto_hide_buttons && m_playing)
        delayedShowButtons (false);
}

void KMPlayerView::reset () {
    m_playing = false;
    if (m_auto_hide_buttons) {
        m_buttonbar->show ();
        m_viewer->setMouseTracking (false);
        m_viewer->parentWidget ()->setMouseTracking (false);
        m_posSlider->setMouseTracking (false);
    }
    if (m_layer->isFullScreen())
        popupMenu ()->activateItemAt (popupMenu ()->indexOf (KMPlayerView::menu_fullscreen)); 
        //m_layer->fullScreen ();
    m_multiedit->hide ();
    if (m_show_console_output) {
        m_multiedit->show ();
        m_multiedit->resize (m_viewer->width (), m_viewer->height ());//Qt bug?
    }
    m_viewer->show ();
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
        m_popupMenu->setItemVisible (menu_zoom, false);
    } else {
        if (m_sreensaver_disabled)
            m_sreensaver_disabled = !kapp->dcopClient()->send
                ("kdesktop", "KScreensaverIface", "enable(bool)", "true");
        m_layer->fullScreen();
        m_popupMenu->setItemVisible (menu_zoom, true);
    }
}
//----------------------------------------------------------------------

KMPlayerViewer::KMPlayerViewer (QWidget *parent, KMPlayerView * view)
  : QWidget (parent), m_aspect (0.0),
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
    setEraseColor (QColor (0, 0, 0));
    setAcceptDrops (true);
}

KMPlayerViewer::~KMPlayerViewer () {
}
    
void KMPlayerViewer::showEvent (QShowEvent *) {
    printf ("show\n");
    QWidget * w = static_cast <QWidget *> (parent ());
    QResizeEvent ev (w->size (), w->size ());
    QApplication::sendEvent (w, &ev);
    XConfigureEvent c = {
        ConfigureNotify, 0UL, True,
        qt_xdisplay (), winId (), parentWidget ()->winId (),
        x (), y (), width (), height (),
        0, None, False
    };
    XSendEvent(qt_xdisplay(), c.event, TRUE, StructureNotifyMask, (XEvent*) &c);
    XFlush (qt_xdisplay ());
}

void KMPlayerViewer::resizeEvent (QResizeEvent *) {
    XConfigureEvent c = {
        ConfigureNotify, 0UL, True,
        qt_xdisplay (), winId (), parentWidget ()->winId (),
        x (), y (), width (), height (),
        0, None, False
    };
    XSendEvent(qt_xdisplay(), c.event, TRUE, StructureNotifyMask, (XEvent*) &c);
    XFlush (qt_xdisplay ());
}

void KMPlayerViewer::hideEvent (QHideEvent *) {
    printf ("hide\n");
}

void KMPlayerViewer::mouseMoveEvent (QMouseEvent * e) {
    if (e->state () == Qt::NoButton)
        m_view->delayedShowButtons (e->y () > height () - button_height);
}

void KMPlayerViewer::mousePressEvent (QMouseEvent *) {
    emit clicked ();
}

void KMPlayerViewer::setMouseTracking (bool enable) {
    if (enable)
        setWState (WState_MouseTracking);
    else
        clearWState (WState_MouseTracking);
    XSelectInput (qt_xdisplay (), winId (), 
               KeyPressMask | KeyReleaseMask |
               ButtonMotionMask |
               EnterWindowMask | LeaveWindowMask |
               FocusChangeMask |
               ExposureMask |
               PropertyChangeMask |
               StructureNotifyMask |
               (m_view->image () ? ButtonPressMask : 0) |
               (enable ? PointerMotionMask : 0)
              );
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

int KMPlayerViewer::heightForWidth (int w) const {
    if (m_aspect <= 0.01)
        return 0;
    return int (w/m_aspect); 
}

void KMPlayerViewer::dropEvent (QDropEvent * de) {
    m_view->dropEvent (de);
}

void KMPlayerViewer::dragEnterEvent (QDragEnterEvent* dee) {
    m_view->dragEnterEvent (dee);
}

void KMPlayerViewer::paintEvent (QPaintEvent * e) {
    if (!m_view->image ()) {
        QWidget::paintEvent (e);
        return;
    }
    QPainter paint (this);
    paint.setClipRect (e->rect ());
    paint.drawPixmap (0, 0, *m_view->image ());
}

void KMPlayerViewer::sendKeyEvent (int key) {
    char buf[2] = { char (key), '\0' }; 
    KeySym keysym = XStringToKeysym (buf);
    XKeyEvent event = {
        XKeyPress, 0, true,
        qt_xdisplay (), winId (), qt_xrootwin(), winId (),
        /*time*/ 0, 0, 0, 0, 0,
        0, XKeysymToKeycode (qt_xdisplay (), keysym), true
    };
    XSendEvent (qt_xdisplay(), winId(), FALSE, KeyPressMask, (XEvent *) &event);
}

bool KMPlayerViewer::x11Event (XEvent * e) {
    switch (e->type) {
        case UnmapNotify:
            if (e->xunmap.window == winId ()) {
                emit aboutToPlay ();
                hide();
            }
            break;
        case XKeyPress:
            printf ("key\n");
            break;
        case ColormapNotify:
            printf ("colormap notify\n");
            return true;
        /*case MapNotify:
            show();
            return true;
        case ConfigureNotify:
            break;
            //return true;*/
        default:
            break;
    }
    return false;
}

#include "kmplayerview.moc"
