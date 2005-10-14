/**
 * Copyright (C) 2005 by Koos Vriezen <koos ! vriezen ? gmail ! com>
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

#include <qlayout.h>
#include <qpixmap.h>
#include <qslider.h>
#include <qlabel.h>
#include <qtooltip.h>
#include <qpainter.h>
#include <qstringlist.h>

#include <kiconloader.h>
#include <klocale.h>
#include <kdebug.h>

#include "kmplayerview.h"
#include "kmplayercontrolpanel.h"
#include "kmplayersource.h"

static const int button_height_with_slider = 15;
static const int button_height_only_buttons = 11;
extern const char * normal_window_xpm[];
extern const char * playlist_xpm[];
#include "kmplayerview.h"
#include "kmplayercontrolpanel.h"

using namespace KMPlayer;

static char xpm_fg_color [32] = ".      c #000000";

static const char * stop_xpm[] = {
    "5 7 2 1",
    "       c None",
    xpm_fg_color,
    "     ",
    ".....",
    ".....",
    ".....",
    ".....",
    ".....",
    "     "};

static const char * play_xpm[] = {
    "5 9 2 1",
    "       c None",
    xpm_fg_color,
    ".    ",
    "..   ",
    "...  ",
    ".... ",
    ".....",
    ".... ",
    "...  ",
    "..   ",
    ".    "};

static const char * pause_xpm[] = {
    "7 9 2 1",
    "       c None",
    xpm_fg_color,
    "       ",
    "..   ..",
    "..   ..",
    "..   ..",
    "..   ..",
    "..   ..",
    "..   ..",
    "..   ..",
    "       "};

static const char * forward_xpm[] = {
    "11 9 2 1",
    "       c None",
    xpm_fg_color,
    ".     .    ",
    "..    ..   ",
    "...   ...  ",
    "....  .... ",
    "..... .....",
    "....  .... ",
    "...   ...  ",
    "..    ..   ",
    ".     .    "};

static const char * back_xpm[] = {
    "11 9 2 1",
    "       c None",
    xpm_fg_color,
    "    .     .",
    "   ..    ..",
    "  ...   ...",
    " ....  ....",
    "..... .....",
    " ....  ....",
    "  ...   ...",
    "   ..    ..",
    "    .     ."};

static const char * config_xpm[] = {
    "11 8 2 1",
    "       c None",
    xpm_fg_color,
    "           ",
    "           ",
    "...........",
    " ......... ",
    "  .......  ",
    "   .....   ",
    "    ...    ",
    "     .     "};

const char * playlist_xpm[] = {
    "8 9 2 1",
    "       c None",
    xpm_fg_color,
    "        ",
    "        ",
    "........",
    "........",
    "        ",
    "        ",
    "........",
    "........",
    "        "};

const char * normal_window_xpm[] = {
    "7 9 2 1",
    "       c None",
    xpm_fg_color,
    "       ",
    ".......",
    ".......",
    ".     .",
    ".     .",
    ".     .",
    ".     .",
    ".......",
    "       "};

static const char * record_xpm[] = {
    "7 7 3 1",
    "       c None",
    xpm_fg_color,
    "+      c #FF0000",
    "       ",
    ".......",
    ".+++++.",
    ".+++++.",
    ".+++++.",
    ".......",
    "       "};

static const char * broadcast_xpm[] = {
"21 9 2 1",
"       c None",
xpm_fg_color,
"                     ",
" ..  ..       ..  .. ",
"..  ..   ...   ..  ..",
"..  ..  .....  ..  ..",
"..  ..  .....  ..  ..",
"..  ..  .....  ..  ..",
"..  ..   ...   ..  ..",
" ..  ..       ..  .. ",
"                     "};

static const char * language_xpm [] = {
    "12 9 2 1",
    "       c None",
    xpm_fg_color,
    "            ",
    "            ",
    "            ",
    "            ",
    "            ",
    "....  ......",
    "....  ......",
    "....  ......",
    "            "};

static const char * red_xpm[] = {
    "7 9 3 1",
    "       c None",
    xpm_fg_color,
    "+      c #FF0000",
    "       ",
    ".......",
    ".+++++.",
    ".+++++.",
    ".+++++.",
    ".+++++.",
    ".+++++.",
    ".......",
    "       "};

static const char * green_xpm[] = {
    "7 9 3 1",
    "       c None",
    xpm_fg_color,
    "+      c #00FF00",
    "       ",
    ".......",
    ".+++++.",
    ".+++++.",
    ".+++++.",
    ".+++++.",
    ".+++++.",
    ".......",
    "       "};

static const char * yellow_xpm[] = {
    "7 9 3 1",
    "       c None",
    xpm_fg_color,
    "+      c #FFFF00",
    "       ",
    ".......",
    ".+++++.",
    ".+++++.",
    ".+++++.",
    ".+++++.",
    ".+++++.",
    ".......",
    "       "};

static const char * blue_xpm[] = {
    "7 9 3 1",
    "       c None",
    xpm_fg_color,
    "+      c #0080FF00",
    "       ",
    ".......",
    ".+++++.",
    ".+++++.",
    ".+++++.",
    ".+++++.",
    ".+++++.",
    ".......",
    "       "};

//-----------------------------------------------------------------------------

static QPushButton * ctrlButton (QWidget * w, QBoxLayout * l, const char ** p, int key = 0) {
    QPushButton * b = new QPushButton (QIconSet (QPixmap(p)), QString::null, w);
    b->setFocusPolicy (QWidget::NoFocus);
    b->setFlat (true);
    if (key)
        b->setAccel (QKeySequence (key));
    l->addWidget (b);
    return b;
}

KDE_NO_CDTOR_EXPORT
KMPlayerMenuButton::KMPlayerMenuButton (QWidget * parent, QBoxLayout * l, const char ** p, int key)
 : QPushButton (QIconSet (QPixmap(p)), QString::null, parent, "kde_kmplayer_control_button") {
   setFocusPolicy (QWidget::NoFocus);
   setFlat (true);
   if (key)
       setAccel (QKeySequence (key));
   l->addWidget (this);
}

KDE_NO_EXPORT void KMPlayerMenuButton::enterEvent (QEvent *) {
    emit mouseEntered ();
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT KMPlayerPopupMenu::KMPlayerPopupMenu (QWidget * parent)
 : KPopupMenu (parent, "kde_kmplayer_popupmenu") {}

KDE_NO_EXPORT void KMPlayerPopupMenu::leaveEvent (QEvent *) {
    emit mouseLeft ();
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT VolumeBar::VolumeBar (QWidget * parent, View * view)
 : QWidget (parent), m_view (view), m_value (100) {
    setSizePolicy( QSizePolicy (QSizePolicy::Minimum, QSizePolicy::Fixed));
    setMinimumSize (QSize (51, button_height_only_buttons + 2));
    QToolTip::add (this, i18n ("Volume is %1").arg (m_value));
}

KDE_NO_CDTOR_EXPORT VolumeBar::~VolumeBar () {
}

void VolumeBar::setValue (int v) {
    m_value = v;
    if (m_value < 0) m_value = 0;
    if (m_value > 100) m_value = 100;
    QToolTip::remove (this);
    QToolTip::add (this, i18n ("Volume is %1").arg (m_value));
    repaint (true);
    emit volumeChanged (m_value);
}

void VolumeBar::wheelEvent (QWheelEvent * e) {
    setValue (m_value + (e->delta () > 0 ? 2 : -2));
    e->accept ();
}

void VolumeBar::paintEvent (QPaintEvent * e) {
    QWidget::paintEvent (e);
    QPainter p;
    p.begin (this);
    QColor color = paletteForegroundColor ();
    p.setPen (color);
    int w = width () - 6;
    int vx = m_value * w / 100;
    p.fillRect (3, 3, vx, 7, color);
    p.drawRect (vx + 3, 3, w - vx, 7);
    p.end ();
    //kdDebug () << "w=" << w << " vx=" << vx << endl;
}

void VolumeBar::mousePressEvent (QMouseEvent * e) {
    setValue (100 * (e->x () - 3) / (width () - 6));
    e->accept ();
}

void VolumeBar::mouseMoveEvent (QMouseEvent * e) {
    setValue (100 * (e->x () - 3) / (width () - 6));
    e->accept ();
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT ControlPanel::ControlPanel(QWidget * parent, View * view)
 : QWidget (parent),
   m_progress_mode (progress_playing),
   m_progress_length (0),
   m_popup_timer (0),
   m_popdown_timer (0),
   m_view (view),
   m_auto_controls (true),
   m_popup_clicked (false) {
    m_buttonbox = new QHBoxLayout (this, 5, 4);
    QColor c = paletteForegroundColor ();
    strncpy (xpm_fg_color, QString().sprintf(".      c #%02x%02x%02x", c.red(), c.green(),c.blue()).ascii(), 31);
    xpm_fg_color[31] = 0;
    m_buttons[button_config] = new KMPlayerMenuButton (this, m_buttonbox, config_xpm);
    m_buttons[button_playlist] = ctrlButton (this, m_buttonbox, playlist_xpm);
    m_buttons[button_back] = ctrlButton (this, m_buttonbox, back_xpm);
    m_buttons[button_play] = ctrlButton (this, m_buttonbox, play_xpm, Qt::Key_R);
    m_buttons[button_forward] = ctrlButton (this, m_buttonbox, forward_xpm);
    m_buttons[button_stop] = ctrlButton (this, m_buttonbox, stop_xpm, Qt::Key_S);
    m_buttons[button_pause] = ctrlButton (this, m_buttonbox, pause_xpm, Qt::Key_P);
    m_buttons[button_record] = ctrlButton (this, m_buttonbox, record_xpm);
    m_buttons[button_broadcast] = ctrlButton (this, m_buttonbox, broadcast_xpm);
    m_buttons[button_language] = new KMPlayerMenuButton (this, m_buttonbox, language_xpm);
    m_buttons[button_red] = ctrlButton (this, m_buttonbox, red_xpm);
    m_buttons[button_green] = ctrlButton (this, m_buttonbox, green_xpm);
    m_buttons[button_yellow] = ctrlButton (this, m_buttonbox, yellow_xpm);
    m_buttons[button_blue] = ctrlButton (this, m_buttonbox, blue_xpm);
    m_buttons[button_play]->setToggleButton (true);
    m_buttons[button_stop]->setToggleButton (true);
    m_buttons[button_record]->setToggleButton (true);
    m_buttons[button_broadcast]->setToggleButton (true);
    m_posSlider = new QSlider (Qt::Horizontal, this);
    m_posSlider->setEnabled (false);
    m_buttonbox->addWidget (m_posSlider);
    showPositionSlider (true);
    m_volume = new VolumeBar (this, m_view);
    m_buttonbox->addWidget (m_volume);
    m_popupMenu = new KMPlayerPopupMenu (this);
    m_playerMenu = new KMPlayerPopupMenu (this);
    m_popupMenu->insertItem (i18n ("&Play with"), m_playerMenu, menu_player);
    m_bookmarkMenu = new KMPlayerPopupMenu (this);
    m_popupMenu->insertItem (i18n("&Bookmarks"), m_bookmarkMenu, menu_bookmark);
    m_popupMenu->insertItem (KGlobal::iconLoader ()->loadIconSet (QString ("konsole"), KIcon::Small, 0, true), i18n ("Con&sole"), menu_video);
    m_popupMenu->insertItem (KGlobal::iconLoader ()->loadIconSet (QString ("player_playlist"), KIcon::Small, 0, true), i18n ("Play&list"), menu_playlist);
    m_zoomMenu = new KMPlayerPopupMenu (this);
    m_zoomMenu->insertItem (i18n ("50%"), menu_zoom50);
    m_zoomMenu->insertItem (i18n ("100%"), menu_zoom100);
    m_zoomMenu->insertItem (i18n ("150%"), menu_zoom150);
    m_popupMenu->insertItem (KGlobal::iconLoader ()->loadIconSet (QString ("viewmag"), KIcon::Small, 0, false), i18n ("&Zoom"), m_zoomMenu, menu_zoom);
    m_popupMenu->insertItem (KGlobal::iconLoader ()->loadIconSet (QString ("window_fullscreen"), KIcon::Small, 0, true), i18n ("&Full Screen"), menu_fullscreen);
    m_popupMenu->setAccel (QKeySequence (Qt::Key_F), menu_fullscreen);
    m_popupMenu->insertSeparator ();
    m_colorMenu = new KMPlayerPopupMenu (this);
    m_languageMenu = new KMPlayerPopupMenu (this);
    m_audioMenu = new KMPlayerPopupMenu (this);
    m_subtitleMenu = new KMPlayerPopupMenu (this);
    m_languageMenu->insertItem (KGlobal::iconLoader ()->loadIconSet (QString ("mime-sound"), KIcon::Small, 0, true), i18n ("&Audio languages"), m_audioMenu);
    m_languageMenu->insertItem (KGlobal::iconLoader ()->loadIconSet (QString ("view_text"), KIcon::Small, 0, true), i18n ("&Subtitles"), m_subtitleMenu);
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
    setAutoControls (true);
    connect (m_buttons [button_config], SIGNAL (clicked ()),
            this, SLOT (buttonClicked ()));
    connect (m_buttons [button_language], SIGNAL (clicked ()),
            this, SLOT (buttonClicked ()));
    connect (m_buttons [button_config], SIGNAL (mouseEntered ()),
             this, SLOT (buttonMouseEntered ()));
    connect (m_buttons [button_language], SIGNAL (mouseEntered ()),
             this, SLOT (buttonMouseEntered ()));
    connect (m_popupMenu, SIGNAL (mouseLeft ()), this, SLOT (menuMouseLeft ()));
    connect (m_playerMenu, SIGNAL (mouseLeft ()), this, SLOT(menuMouseLeft ()));
    connect (m_zoomMenu, SIGNAL (mouseLeft ()), this, SLOT (menuMouseLeft ()));
    connect (m_colorMenu, SIGNAL (mouseLeft ()), this, SLOT (menuMouseLeft ()));
    connect (m_languageMenu, SIGNAL(mouseLeft ()), this, SLOT(menuMouseLeft()));
    connect (m_subtitleMenu, SIGNAL(mouseLeft ()), this, SLOT(menuMouseLeft()));
    connect (m_audioMenu, SIGNAL (mouseLeft ()), this, SLOT (menuMouseLeft ()));
}

KDE_NO_EXPORT void ControlPanel::timerEvent (QTimerEvent * e) {
    if (e->timerId () == m_popup_timer) {
        m_popup_timer = 0;
        if (m_button_monitored == button_config) {
            if (m_buttons [button_config]->hasMouse() &&
                    !m_popupMenu->isVisible ())
                showPopupMenu ();
        } else if (m_buttons [button_language]->hasMouse() && 
                    !m_languageMenu->isVisible ()) {
            showLanguageMenu ();
        }
    } else if (e->timerId () == m_popdown_timer) {
        m_popdown_timer = 0;
        if (m_popupMenu->isVisible () &&
                !m_popupMenu->hasMouse () &&
                !m_playerMenu->hasMouse () &&
                !m_zoomMenu->hasMouse () &&
                !m_colorMenu->hasMouse () &&
                !m_bookmarkMenu->hasMouse ()) {
            if (!(m_bookmarkMenu->isVisible () && 
                        static_cast <QWidget *> (m_bookmarkMenu) != QWidget::keyboardGrabber ())) {
                // not if user entered the bookmark sub menu or if I forgot one
                m_popupMenu->hide ();
                if (m_buttons [button_config]->isOn ())
                    m_buttons [button_config]->toggle ();
            }
        } else if (m_languageMenu->isVisible () &&
                !m_languageMenu->hasMouse () &&
                !m_audioMenu->hasMouse () &&
                !m_subtitleMenu->hasMouse ()) {
            m_languageMenu->hide ();
            if (m_buttons [button_language]->isOn ())
                m_buttons [button_language]->toggle ();
        }
    }
    killTimer (e->timerId ());
}

void ControlPanel::setAutoControls (bool b) {
    m_auto_controls = b;
    if (m_auto_controls) {
        for (int i = 0; i < (int) button_broadcast; i++)
            m_buttons [i]->show ();
        for (int i = button_broadcast; i < (int) button_last; i++)
            m_buttons [i]->hide ();
        showPositionSlider (true);
        m_volume->show ();
        if (m_buttons [button_broadcast]->isOn ()) // still broadcasting
            m_buttons [button_broadcast]->show ();
    } else { // hide everything
        for (int i = 0; i < (int) button_last; i++)
            m_buttons [i]->hide ();
        m_posSlider->hide ();
        m_volume->hide ();
    }
    m_view->updateLayout ();
}

KDE_NO_EXPORT void ControlPanel::showPopupMenu () {
    m_view->updateVolume ();
    m_popupMenu->exec (m_buttons [button_config]->mapToGlobal (QPoint (0, maximumSize ().height ())));
}

KDE_NO_EXPORT void ControlPanel::showLanguageMenu () {
    m_languageMenu->exec (m_buttons [button_language]->mapToGlobal (QPoint (0, maximumSize ().height ())));
}

void ControlPanel::showPositionSlider (bool show) {
    if (!m_auto_controls) return;
    int h = show ? button_height_with_slider : button_height_only_buttons;
    m_posSlider->setEnabled (false);
    m_posSlider->setValue (0);
    if (show) {
        m_posSlider->show ();
        m_buttonbox->setMargin (4);
        m_buttonbox->setSpacing (4);
        setEraseColor (m_view->topLevelWidget ()->paletteBackgroundColor ());
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
    m_view->updateLayout ();
}

void ControlPanel::enableSeekButtons (bool enable) {
    if (!m_auto_controls) return;
    if (enable) {
        m_buttons[button_back]->show ();
        m_buttons[button_forward]->show ();
    } else {
        m_buttons[button_back]->hide ();
        m_buttons[button_forward]->hide ();
    }
}

void ControlPanel::enableRecordButtons (bool enable) {
    if (!m_auto_controls) return;
    if (enable)
        m_buttons[button_record]->show ();
    else
        m_buttons[button_record]->hide ();
}

void ControlPanel::setPlaying (bool play) {
    if (play != m_buttons[button_play]->isOn ())
        m_buttons[button_play]->toggle ();
    m_posSlider->setEnabled (false);
    m_posSlider->setValue (0);
    if (!play) {
        showPositionSlider (true);
        enableSeekButtons (true);
    }
}

KDE_NO_EXPORT void ControlPanel::setRecording (bool record) {
    if (record != m_buttons[button_record]->isOn ())
        m_buttons[button_record]->toggle ();
}

KDE_NO_EXPORT void ControlPanel::setPlayingProgress (int pos, int len) {
    m_posSlider->setEnabled (false);
    m_progress_length = len;
    if (len > 0 && !m_posSlider->isVisible ())
        showPositionSlider (true);
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

KDE_NO_EXPORT void ControlPanel::setLoadingProgress (int pos) {
    m_posSlider->setEnabled (false);
    if (m_progress_mode != progress_loading) {
        m_posSlider->setMaxValue (100);
        m_progress_mode = progress_loading;
    }
    m_posSlider->setValue (pos);
}

KDE_NO_EXPORT void ControlPanel::buttonClicked () {
    if (m_popup_timer) {
        killTimer (m_popup_timer);
        m_popup_timer = 0;
    }
    m_popup_clicked = true;
    if (sender () == m_buttons [button_language])
        showLanguageMenu ();
    else
        showPopupMenu ();
}

KDE_NO_EXPORT void ControlPanel::buttonMouseEntered () {
    if (!m_popup_timer) {
        if (sender () == m_buttons [button_config]) {
            if (!m_popupMenu->isVisible ()) {
                m_button_monitored = button_config;
                m_popup_clicked = false;
                m_popup_timer = startTimer (400);
            }
        } else if (!m_languageMenu->isVisible ()) {
            m_button_monitored = button_language;
            m_popup_clicked = false;
            m_popup_timer = startTimer (400);
        }
    }
}

KDE_NO_EXPORT void ControlPanel::menuMouseLeft () {
    if (!m_popdown_timer && !m_popup_clicked)
        m_popdown_timer = startTimer (400);
}

KDE_NO_EXPORT void ControlPanel::setLanguages (const QStringList & alang, const QStringList & slang) {
    int sz = (int) alang.size ();
    bool showbutton = (sz > 0);
    m_audioMenu->clear ();
    for (int i = 0; i < sz; i++)
        m_audioMenu->insertItem (alang [i], i);
    sz = (int) slang.size ();
    showbutton |= (sz > 0);
    m_subtitleMenu->clear ();
    for (int i = 0; i < sz; i++)
        m_subtitleMenu->insertItem (slang [i], i);
    if (showbutton)
        m_buttons [button_language]->show ();
    else
        m_buttons [button_language]->hide ();
}

KDE_NO_EXPORT void ControlPanel::selectSubtitle (int id) {
    if (m_subtitleMenu->isItemChecked (id))
        return;
    int size = m_subtitleMenu->count ();
    for (int i = 0; i < size; i++)
        if (m_subtitleMenu->isItemChecked (i)) {
            m_subtitleMenu->setItemChecked (i, false);
            break;
        }
    m_subtitleMenu->setItemChecked (id, true);
}

KDE_NO_EXPORT void ControlPanel::selectAudioLanguage (int id) {
    kdDebug () << "ControlPanel::selectAudioLanguage " << id << endl;
    if (m_audioMenu->isItemChecked (id))
        return;
    int sz = m_audioMenu->count ();
    for (int i = 0; i < sz; i++)
        if (m_audioMenu->isItemChecked (i)) {
            m_audioMenu->setItemChecked (i, false);
            break;
        }
    m_audioMenu->setItemChecked (id, true);
}

//-----------------------------------------------------------------------------

#include "kmplayercontrolpanel.moc"
