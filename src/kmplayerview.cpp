/***************************************************************************
  kmplayerview.cpp  -  description
  -------------------
begin                : Sat Dec  7 16:14:51 CET 2002
copyright            : (C) 2002 by Koos Vriezen
email                : 
 ***************************************************************************/

/***************************************************************************
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
 ***************************************************************************/

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
#include <qdragobject.h>

#include <X11/Xlib.h>
#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>

static const int XKeyPress = KeyPress;
#undef KeyPress
static const int button_height = 11;

// application specific includes
#include "kmplayerview.h"
//#include "kmplayer.h"

#include <kstaticdeleter.h>
#include <kdebug.h>
#include <klocale.h>
#include <kapplication.h>
#include <dcopclient.h>
#include <arts/kartsdispatcher.h>
#include <arts/soundserver.h>
#include <arts/kartsserver.h>
#include <arts/kartsfloatwatch.h>

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

    void getDispatcher();
    void releaseDispatcher();
private:
    KArtsDispatcher *dispatcher;
    int use_count;
};

static KMPlayerViewStatic * kmplayerview_static = 0L;

KMPlayerViewStatic::KMPlayerViewStatic () 
    : dispatcher ((KArtsDispatcher*) 0), use_count(0) {
    printf ("KMPlayerViewStatic::KMPlayerViewStatic\n");
    oldFilter = qt_set_x11_event_filter (qxembed_x11_event_filter);
}

KMPlayerViewStatic::~KMPlayerViewStatic () {
    printf ("KMPlayerViewStatic::~KMPlayerViewStatic\n");
    delete dispatcher;
    qt_set_x11_event_filter (oldFilter);
    kmplayerview_static = 0L;
}

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
    setEraseColor (QColor (0, 0, 0));
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
    m_artsserver (0L),
    m_svc (0L),
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
}

void KMPlayerView::dropEvent (QDropEvent * de) {
    KURL url;
    if (QUriDrag::canDecode (de)) {
        QStrList sl;
        QUriDrag::decode (de, sl);
        if (sl.count () > 0)
            url = KURL (sl.at (0));
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
    if (QUriDrag::canDecode (dee)) {
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
    KMPlayerViewerHolder * w1 = new KMPlayerViewerHolder (m_layer, this);
    m_viewer = new KMPlayerViewer (w1, this);
    //w1->setEraseColor (QColor (0, 0, 0));
    layerbox->addWidget (w1);
    
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
    m_zoomMenu = new QPopupMenu (m_layer);
    m_zoomMenu->insertItem (i18n ("50%"), menu_zoom50);
    m_zoomMenu->insertItem (i18n ("100%"), menu_zoom100);
    m_zoomMenu->insertItem (i18n ("150%"), menu_zoom150);
    m_popupMenu->insertItem (i18n ("&Zoom"), m_zoomMenu, menu_zoom);
    m_popupMenu->insertItem (i18n ("&Full Screen"),
          this, SLOT (fullScreen()), QKeySequence (Qt::Key_F), menu_fullscreen);
    m_popupMenu->insertSeparator ();
    QLabel * label = new QLabel (i18n ("Contrast:"), m_popupMenu);
    m_popupMenu->insertItem (label);
    m_contrastSlider = new QSlider (-100, 100, 10, 0, Qt::Horizontal, m_popupMenu);
    m_popupMenu->insertItem (m_contrastSlider);
    label = new QLabel (i18n ("Brightness:"), m_popupMenu);
    m_popupMenu->insertItem (label);
    m_brightnessSlider = new QSlider (-100, 100, 10, 0, Qt::Horizontal, m_popupMenu);
    m_popupMenu->insertItem (m_brightnessSlider);
    label = new QLabel (i18n ("Hue:"), m_popupMenu);
    m_popupMenu->insertItem (label);
    m_hueSlider = new QSlider (-100, 100, 10, 0, Qt::Horizontal, m_popupMenu);
    m_popupMenu->insertItem (m_hueSlider);
    label = new QLabel (i18n ("Saturation:"), m_popupMenu);
    m_popupMenu->insertItem (label);
    m_saturationSlider = new QSlider (-100, 100, 10, 0, Qt::Horizontal, m_popupMenu);
    m_popupMenu->insertItem (m_saturationSlider);
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
    XSelectInput (qt_xdisplay (), m_viewer->winId (), 
            ExposureMask | StructureNotifyMask | KeyPressMask);
    printf ("KMPlayerView %u %u\n", m_viewer->winId(), kmplayerview_static);

}

KMPlayerView::~KMPlayerView () {
    setUseArts (false);
    if (m_layer->parent () != this)
        delete m_layer;
}

void KMPlayerView::setUseArts (bool b) {
    if (!m_use_arts && b) {
        kmplayerview_static->getDispatcher();
        m_artsserver = new Arts::SoundServerV2;
        *m_artsserver = KArtsServer().server();
        m_svc = new Arts::StereoVolumeControl;
        if (m_artsserver && !m_artsserver->isNull()) {
            m_arts_label = new QLabel (i18n ("Volume:"), m_popupMenu);
            m_popupMenu->insertItem (m_arts_label, -1, 3);
            m_slider = new QSlider (0, 100, 10, 40, Qt::Horizontal, m_popupMenu);
            connect(m_slider, SIGNAL(valueChanged(int)), this,SLOT(setVolume(int)));
            *m_svc = m_artsserver->outVolume();
            m_watch = new KArtsFloatWatch(*m_svc, "scaleFactor_changed", this);
            connect (m_watch, SIGNAL (valueChanged (float)), 
                    this, SLOT (updateVolume (float)));
            m_popupMenu->insertItem (m_slider, menu_volume, 4);
            m_popupMenu->insertSeparator (5);
        }
    } else if (m_use_arts && !b) {
        m_popupMenu->removeItemAt (5);
        m_popupMenu->removeItemAt (4);
        m_popupMenu->removeItemAt (3);
        delete m_watch;
        delete m_artsserver;
        delete m_svc;
        kmplayerview_static->releaseDispatcher();
    }
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

/*void KMPlayerView::setAutoHideSlider (bool b) {
    killTimers ();
    m_auto_hide_slider = b;
    if (b && m_playing)
    	m_slider->hide();
    else
    	m_slider->show();
*/
void KMPlayerView::delayedShowButtons (bool show) {
    if (!m_auto_hide_buttons || delayed_timer ||
        (show && m_buttonbar->isVisible ()) || 
        (!show && !m_buttonbar->isVisible ()))
        return;
    delayed_timer = startTimer (300);
}

void KMPlayerView::setVolume (int vol) {
    if (m_inVolumeUpdate) return;
    float volume = float (0.0004*vol*vol);
    printf("setVolume %d -> %.4f\n", vol, volume);
    m_svc->scaleFactor (volume);
}

void KMPlayerView::updateVolume (float v) {
    m_inVolumeUpdate = true;
    printf("updateVolume %.4f\n", v);
    m_slider->setValue (int (sqrt(v*10000.0/4)));
    m_inVolumeUpdate = false;
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
    m_multiedit->append (str);
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
    if (m_use_arts)
        updateVolume(m_svc->scaleFactor());
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
        m_layer->fullScreen();
    } else {
        if (m_sreensaver_disabled)
            m_sreensaver_disabled = !kapp->dcopClient()->send
                ("kdesktop", "KScreensaverIface", "enable(bool)", "true");
        m_layer->fullScreen();
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
    setFocusPolicy (QWidget::NoFocus);
    setAcceptDrops (true);
}

KMPlayerViewer::~KMPlayerViewer () {
}
    
void KMPlayerViewer::showEvent (QShowEvent *) {
    printf ("show\n");
    QWidget * w = static_cast <QWidget *> (parent ());
    QResizeEvent ev (w->size (), w->size ());
    QApplication::sendEvent (w, &ev);
    QWidget * parent = parentWidget ();
    XConfigureEvent c = {
        ConfigureNotify, 0UL, True,
        qt_xdisplay (), winId (), parent->winId (),
        0, 0, parent->width (), parent->height (),
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

void KMPlayerViewer::setAspect (float a) {
    m_aspect = a;
    QWidget * w = static_cast <QWidget *> (parent ());
    QResizeEvent ev (w->size (), w->size ());
    QApplication::sendEvent (w, &ev);
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

bool KMPlayerViewer::x11Event (XEvent * e) {
    switch (e->type) {
        case UnmapNotify:
            if (e->xunmap.event == winId ()) {
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
