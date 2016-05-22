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
#include <qpainter.h>
#include <qstringlist.h>
#include <QCursor>
#include <QPalette>
#include <QSlider>
#include <QLabel>
#include <QWheelEvent>
#include <QWidgetAction>

#include <kiconloader.h>
#include <kicon.h>
#include <klocale.h>
#include <kdebug.h>

#include "kmplayerview.h"
#include "kmplayercontrolpanel.h"

static const int button_height_with_slider = 16;
static const int button_height_only_buttons = 16;
static float dpi_scale = 1.0;
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

static QIcon makeIcon(const char** xpm) {
    QPixmap pix(xpm);
    if (dpi_scale > 1.01)
        pix = pix.scaledToHeight(pix.height() * dpi_scale, Qt::SmoothTransformation);
    return QIcon(pix);
}

static QPushButton *ctrlButton(QBoxLayout* l, const char **p, int key = 0) {
    QPushButton* b = new QPushButton(makeIcon(p), QString());
    b->setFocusPolicy (Qt::NoFocus);
    b->setFlat (true);
    b->setAutoFillBackground (true);
    if (key)
        b->setShortcut (QKeySequence (key));
    l->addWidget (b);
    return b;
}

KDE_NO_CDTOR_EXPORT
KMPlayerMenuButton::KMPlayerMenuButton(QWidget*, QBoxLayout * l, const char ** p, int key)
 : QPushButton(makeIcon(p), QString()) {
    setFocusPolicy (Qt::NoFocus);
    setFlat (true);
    setAutoFillBackground (true);
    if (key)
        setShortcut (QKeySequence (key));
    l->addWidget (this);
}

KDE_NO_EXPORT void KMPlayerMenuButton::enterEvent (QEvent *) {
    emit mouseEntered ();
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT
KMPlayerPopupMenu::KMPlayerPopupMenu (QWidget *parent, const QString &title)
 : KMenu (title, parent) {}

KDE_NO_EXPORT void KMPlayerPopupMenu::leaveEvent (QEvent *) {
    emit mouseLeft ();
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT VolumeBar::VolumeBar(QWidget*, View * view)
 : m_view(view), m_value(100) {
    setSizePolicy( QSizePolicy (QSizePolicy::Minimum, QSizePolicy::Fixed));
    setMinimumSize (QSize (51, button_height_only_buttons + 2));
    setToolTip (i18n ("Volume is ") + QString::number (m_value));
    setAutoFillBackground (true);
    QPalette palette;
    palette.setColor (backgroundRole (), m_view->palette ().color (QPalette::Background));
    setPalette (palette);
}

KDE_NO_CDTOR_EXPORT VolumeBar::~VolumeBar () {
}

void VolumeBar::setValue (int v) {
    m_value = v;
    if (m_value < 0) m_value = 0;
    if (m_value > 100) m_value = 100;
    setToolTip (i18n ("Volume is ") + QString::number (m_value));
    repaint ();
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
    QColor color = palette ().color (foregroundRole ());
    p.setPen (color);
    int w = width () - 6 * dpi_scale;
    int vx = m_value * w / 100;
    p.fillRect (3 * dpi_scale, 3 * dpi_scale, vx, 7 * dpi_scale, color);
    p.drawRect (vx + 3 * dpi_scale, 3 * dpi_scale, w - vx, 7 * dpi_scale);
    p.end ();
    //kDebug () << "w=" << w << " vx=" << vx;
}

void VolumeBar::mousePressEvent (QMouseEvent * e) {
    setValue (100 * (e->x () - 3 * dpi_scale) / (width () - 6 * dpi_scale));
    e->accept ();
}

void VolumeBar::mouseMoveEvent (QMouseEvent * e) {
    setValue (100 * (e->x () - 3 * dpi_scale) / (width () - 6 * dpi_scale));
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
#if QT_VERSION > 0x040399
    setAttribute (Qt::WA_NativeWindow);
    setAttribute(Qt::WA_DontCreateNativeAncestors);
#endif
    dpi_scale = qMax(1.0, logicalDpiX() / 120.0);
    m_buttonbox = new QHBoxLayout (this);
    m_buttonbox->setSpacing (4 * dpi_scale);
    m_buttonbox->setContentsMargins (5 * dpi_scale, 5 * dpi_scale, 5 * dpi_scale, 5 * dpi_scale);
    setAutoFillBackground (true);
    QColor c = palette ().color (foregroundRole ());
    strncpy (xpm_fg_color, QString().sprintf(".      c #%02x%02x%02x", c.red(), c.green(),c.blue()).toAscii().constData(), 31);
    xpm_fg_color[31] = 0;
    m_buttons[button_config] = new KMPlayerMenuButton (this, m_buttonbox, config_xpm);
    m_buttons[button_playlist] = ctrlButton(m_buttonbox, playlist_xpm);
    m_buttons[button_back] = ctrlButton(m_buttonbox, back_xpm);
    m_buttons[button_play] = ctrlButton(m_buttonbox, play_xpm, Qt::Key_R);
    m_buttons[button_forward] = ctrlButton(m_buttonbox, forward_xpm);
    m_buttons[button_stop] = ctrlButton(m_buttonbox, stop_xpm, Qt::Key_S);
    m_buttons[button_pause]=ctrlButton(m_buttonbox, pause_xpm, Qt::Key_P);
    m_buttons[button_record] = ctrlButton(m_buttonbox, record_xpm);
    m_buttons[button_broadcast] = ctrlButton(m_buttonbox, broadcast_xpm);
    m_buttons[button_language] = new KMPlayerMenuButton (this, m_buttonbox, language_xpm);
    m_buttons[button_red] = ctrlButton(m_buttonbox, red_xpm);
    m_buttons[button_green] = ctrlButton(m_buttonbox, green_xpm);
    m_buttons[button_yellow] = ctrlButton(m_buttonbox, yellow_xpm);
    m_buttons[button_blue] = ctrlButton(m_buttonbox, blue_xpm);
    m_buttons[button_play]->setCheckable (true);
    m_buttons[button_stop]->setCheckable (true);
    m_buttons[button_record]->setCheckable (true);
    m_buttons[button_broadcast]->setCheckable (true);
    m_posSlider = new QSlider;
    m_posSlider->setOrientation (Qt::Horizontal);
    m_posSlider->setMaximum (100);
    m_posSlider->setPageStep (1);
    m_posSlider->setEnabled (false);
    m_buttonbox->addWidget (m_posSlider);
    setupPositionSlider (true);
    m_volume = new VolumeBar (this, m_view);
    m_buttonbox->addWidget (m_volume);

    popupMenu = new KMPlayerPopupMenu (this, QString ());

    playerMenu = new KMPlayerPopupMenu (this, i18n ("&Play with"));
    playersAction = popupMenu->addMenu (playerMenu);

    videoConsoleAction = popupMenu->addAction (KIcon ("utilities-terminal"), i18n ("Con&sole"));

    playlistAction = popupMenu->addAction (KIcon ("view-media-playlist"), i18n ("Play&list"));

    zoomMenu = new KMPlayerPopupMenu (this, i18n ("&Zoom"));
    zoomAction = popupMenu->addMenu (zoomMenu);
    zoomAction->setIcon (KIcon ("zoom-fit-best"));
    zoom50Action = zoomMenu->addAction (i18n ("50%"));
    zoom100Action = zoomMenu->addAction (i18n ("100%"));
    zoom150Action = zoomMenu->addAction (i18n ("150%"));

    fullscreenAction = popupMenu->addAction (KIcon ("view-fullscreen"), i18n ("&Full Screen"));
    fullscreenAction->setShortcut (QKeySequence (Qt::Key_F));

    popupMenu->addSeparator ();

    colorMenu = new KMPlayerPopupMenu (this, i18n ("Co&lors"));
    colorAction = popupMenu->addMenu (colorMenu);
    colorAction->setIcon (KIcon ("format-fill-color"));
    /*QLabel * label = new QLabel (i18n ("Contrast:"), colorMenu);
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
    colorMenu->insertItem (m_saturationSlider);*/
    colorAction->setVisible (false);

    bookmarkMenu = new KMPlayerPopupMenu (this, i18n("&Bookmarks"));
    bookmarkAction = popupMenu->addMenu (bookmarkMenu);
    bookmarkAction->setVisible (false);

    languageMenu = new KMPlayerPopupMenu (this, i18n ("&Audio languages"));
    languageAction = popupMenu->addMenu (languageMenu);
    audioMenu = new KMPlayerPopupMenu (this, i18n ("&Audio languages"));
    subtitleMenu = new KMPlayerPopupMenu (this, i18n ("&Subtitles"));
    QAction *audioAction = languageMenu->addMenu (audioMenu);
    QAction *subtitleAction = languageMenu->addMenu (subtitleMenu);
    audioAction->setIcon (KIcon ("audio-x-generic"));
    subtitleAction->setIcon (KIcon ("view-list-text"));
    languageAction->setVisible (false);

    scaleLabelAction = new QWidgetAction (popupMenu);
    scaleLabelAction->setDefaultWidget (new QLabel (i18n ("Scale:")));
    popupMenu->addAction (scaleLabelAction);
    scaleAction = new QWidgetAction (popupMenu);
    scale_slider = new QSlider;
    scale_slider->setOrientation (Qt::Horizontal);
    scale_slider->setMinimum (50);
    scale_slider->setMaximum (150);
    scale_slider->setPageStep (10);
    scale_slider->setSliderPosition (100);
    scaleAction->setDefaultWidget (scale_slider);
    popupMenu->addAction (scaleAction);

    configureAction = popupMenu->addAction (KIcon ("configure"), i18n ("&Configure KMPlayer..."));

    QPalette pal = palette ();
    pal.setColor(backgroundRole(), view->palette().color(QPalette::Background));
    setPalette (pal);
    setAutoControls (true);
    connect (m_buttons [button_config], SIGNAL (clicked ()),
            this, SLOT (buttonClicked ()));
    connect (m_buttons [button_language], SIGNAL (clicked ()),
            this, SLOT (buttonClicked ()));
    connect (m_buttons [button_config], SIGNAL (mouseEntered ()),
             this, SLOT (buttonMouseEntered ()));
    connect (m_buttons [button_language], SIGNAL (mouseEntered ()),
             this, SLOT (buttonMouseEntered ()));
    connect (popupMenu, SIGNAL (mouseLeft ()), this, SLOT (menuMouseLeft ()));
    connect (playerMenu, SIGNAL (mouseLeft ()), this, SLOT(menuMouseLeft ()));
    connect (zoomMenu, SIGNAL (mouseLeft ()), this, SLOT (menuMouseLeft ()));
    connect (colorMenu, SIGNAL (mouseLeft ()), this, SLOT (menuMouseLeft ()));
    connect (languageMenu, SIGNAL(mouseLeft ()), this, SLOT(menuMouseLeft()));
    connect (subtitleMenu, SIGNAL(mouseLeft ()), this, SLOT(menuMouseLeft()));
    connect (audioMenu, SIGNAL (mouseLeft ()), this, SLOT (menuMouseLeft ()));
}

KDE_NO_EXPORT void ControlPanel::setPalette (const QPalette & pal) {
    QWidget::setPalette (pal);
    QColor c = palette ().color (foregroundRole ());
    if (c == Qt::black)
        return;
    strncpy (xpm_fg_color, QString().sprintf(".      c #%02x%02x%02x", c.red(), c.green(),c.blue()).toAscii().constData(), 31);
    xpm_fg_color[31] = 0;
    m_buttons[button_config]->setIcon(makeIcon(config_xpm));
    m_buttons[button_playlist]->setIcon(makeIcon(playlist_xpm));
    m_buttons[button_back]->setIcon(makeIcon(back_xpm));
    m_buttons[button_play]->setIcon(makeIcon(play_xpm));
    m_buttons[button_forward]->setIcon(makeIcon(forward_xpm));
    m_buttons[button_stop]->setIcon(makeIcon(stop_xpm));
    m_buttons[button_pause]->setIcon(makeIcon(pause_xpm));
    m_buttons[button_record]->setIcon(makeIcon(record_xpm));
    m_buttons[button_broadcast]->setIcon(makeIcon(broadcast_xpm));
    m_buttons[button_language]->setIcon(makeIcon(language_xpm));
    m_buttons[button_red]->setIcon(makeIcon(red_xpm));
    m_buttons[button_green]->setIcon(makeIcon(green_xpm));
    m_buttons[button_yellow]->setIcon(makeIcon(yellow_xpm));
    m_buttons[button_blue]->setIcon(makeIcon(blue_xpm));
}

KDE_NO_EXPORT void ControlPanel::timerEvent (QTimerEvent * e) {
    if (e->timerId () == m_popup_timer) {
        m_popup_timer = 0;
        if (m_button_monitored == button_config) {
            if (m_buttons [button_config]->testAttribute(Qt::WA_UnderMouse) &&
                    !popupMenu->isVisible ())
                showPopupMenu ();
        } else if (m_buttons [button_language]->testAttribute(Qt::WA_UnderMouse) &&
                    !languageMenu->isVisible ()) {
            showLanguageMenu ();
        }
    } else if (e->timerId () == m_popdown_timer) {
        m_popdown_timer = 0;
        QPoint mpos = QCursor::pos();
#define HAS_MOUSE(menu) ((menu)->isVisible() && (menu)->rect().contains((menu)->mapFromGlobal(mpos)))
        if (popupMenu->isVisible () &&
                !popupMenu->underMouse() &&
                !HAS_MOUSE(playerMenu) &&
                !HAS_MOUSE(zoomMenu) &&
                !HAS_MOUSE(colorMenu) &&
                !HAS_MOUSE(bookmarkMenu)) {
            if (!(bookmarkMenu->isVisible () &&
                        static_cast <QWidget *> (bookmarkMenu) != QWidget::keyboardGrabber ())) {
                // not if user entered the bookmark sub menu or if I forgot one
                popupMenu->hide ();
                if (m_buttons [button_config]->isChecked ())
                    m_buttons [button_config]->toggle ();
            }
        } else if (languageMenu->isVisible () &&
                !HAS_MOUSE(languageMenu) &&
                !HAS_MOUSE(audioMenu) &&
                !HAS_MOUSE(subtitleMenu)) {
            languageMenu->hide ();
            if (m_buttons [button_language]->isChecked ())
                m_buttons [button_language]->toggle ();
        }
#undef HAS_MOUSE
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
        showPositionSlider (false);
        m_volume->show ();
        if (m_buttons [button_broadcast]->isChecked ()) // still broadcasting
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
    popupMenu->exec (m_buttons [button_config]->mapToGlobal (QPoint (0, maximumSize ().height ())));
}

KDE_NO_EXPORT void ControlPanel::showLanguageMenu () {
    languageMenu->exec (m_buttons [button_language]->mapToGlobal (QPoint (0, maximumSize ().height ())));
}

void ControlPanel::showPositionSlider (bool show) {
    if (!m_auto_controls || show == m_posSlider->isVisible ())
        return;
    setupPositionSlider (show);
    if (isVisible ())
        m_view->updateLayout ();
}

KDE_NO_EXPORT void ControlPanel::setupPositionSlider (bool show) {
    int h = show ? button_height_with_slider : button_height_only_buttons;
    m_posSlider->setEnabled (false);
    m_posSlider->setValue (0);
    m_posSlider->setVisible (show);
    for (int i = 0; i < (int) button_last; i++) {
        m_buttons[i]->setMinimumSize (15 * dpi_scale, (h-1) * dpi_scale);
        m_buttons[i]->setMaximumHeight(h * dpi_scale);
    }
    setMaximumHeight((h + 6) * dpi_scale);
}

KDE_NO_EXPORT int ControlPanel::preferredHeight () {
    return dpi_scale * (m_posSlider->isVisible () ?
            button_height_with_slider + 8 : button_height_only_buttons + 2);
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
    if (play != m_buttons[button_play]->isChecked ())
        m_buttons[button_play]->toggle ();
    m_posSlider->setEnabled (false);
    m_posSlider->setValue (0);
    if (!play) {
        showPositionSlider (false);
        enableSeekButtons (true);
    }
}

KDE_NO_EXPORT void ControlPanel::setRecording (bool record) {
    if (record != m_buttons[button_record]->isChecked ())
        m_buttons[button_record]->toggle ();
}

KDE_NO_EXPORT void ControlPanel::setPlayingProgress (int pos, int len) {
    m_posSlider->setEnabled (false);
    m_progress_length = len;
    showPositionSlider (len > 0);
    if (m_progress_mode != progress_playing) {
        m_posSlider->setMaximum (m_progress_length);
        m_progress_mode = progress_playing;
    }
    if (pos < len && len > 0 && len != m_posSlider->maximum ())
        m_posSlider->setMaximum (m_progress_length);
    else if (m_progress_length <= 0 && pos > 7 * m_posSlider->maximum ()/8)
        m_posSlider->setMaximum (m_posSlider->maximum() * 2);
    else if (m_posSlider->maximum() < pos)
        m_posSlider->setMaximum (int (1.4 * m_posSlider->maximum()));
    m_posSlider->setValue (pos);
    m_posSlider->setEnabled (true);
}

KDE_NO_EXPORT void ControlPanel::setLoadingProgress (int pos) {
    if (pos > 0 && pos < 100 && !m_posSlider->isVisible ())
        showPositionSlider (true);
    else if (pos >= 100 && m_posSlider->isVisible ())
        showPositionSlider (false);
    m_posSlider->setEnabled (false);
    if (m_progress_mode != progress_loading) {
        m_posSlider->setMaximum (100);
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
            if (!popupMenu->isVisible ()) {
                m_button_monitored = button_config;
                m_popup_clicked = false;
                m_popup_timer = startTimer (400);
            }
        } else if (!languageMenu->isVisible ()) {
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
    audioMenu->clear ();
    for (int i = 0; i < sz; i++)
        audioMenu->addAction (alang [i])->setCheckable(true);
    sz = (int) slang.size ();
    showbutton |= (sz > 0);
    subtitleMenu->clear ();
    for (int i = 0; i < sz; i++)
        subtitleMenu->addAction (slang [i])->setCheckable(true);
    if (showbutton)
        m_buttons [button_language]->show ();
    else
        m_buttons [button_language]->hide ();
}

KDE_NO_EXPORT void ControlPanel::actionToggled (QAction* act) {
    if (act->isChecked ())
        return;
    int size = act->parentWidget()->actions().count();
    for (int i = 0; i < size; i++)
        if (act->parentWidget()->actions().at(i)->isChecked ()) {
            act->parentWidget()->actions().at(i)->setChecked (false);
            break;
        }
    act->setChecked (true);
}

//-----------------------------------------------------------------------------

#include "kmplayercontrolpanel.moc"
