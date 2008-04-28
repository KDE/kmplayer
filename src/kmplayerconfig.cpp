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

#include <algorithm>
#include <functional>
#include "config-kmplayer.h"
#include <qcheckbox.h>
#include <qtextedit.h>
#include <qpushbutton.h>
#include <qradiobutton.h>
#include <qtabwidget.h>
#include <qslider.h>
#include <qspinbox.h>
#include <qlabel.h>
#include <qfileinfo.h>
#include <Q3ListBox>
#include <Q3ButtonGroup>

#include <kurlrequester.h>
#include <klineedit.h>
#include <kstatusbar.h>

#include <kconfig.h>
#include <kconfiggroup.h>
#include <kapplication.h>
#include <kurl.h>
#include <kdebug.h>
#include <klocale.h>
#include <kcombobox.h>
#include <kmessagebox.h>
#include <kglobalsettings.h>
#include <kcolorscheme.h>

#include "kmplayersource.h"
#include "kmplayerconfig.h"
#include "kmplayerpartbase.h"
#include "kmplayerprocess.h"
#include "playlistview.h"
#include "viewarea.h"
#include "pref.h"

using namespace KMPlayer;

static OutputDriver _ads[] = {
    { "alsa,oss,sdl,arts", i18n ("Auto") },
    { "oss", i18n ("Open Sound System") },
    { "sdl", i18n ("Simple DirectMedia Layer") },
    { "alsa", i18n ("Advanced Linux Sound Architecture") },
    { "arts", i18n ("Analog Real-Time Synthesizer") },
    { "jack", i18n ("JACK Audio Connection Kit") },
    { "openal", i18n ("OpenAL") },
    { "esd", i18n ("Enlightened Sound Daemon") },
    { "alsa5", i18n ("Advanced Linux Sound Architecture v0.5") },
    { "alsa9", i18n ("Advanced Linux Sound Architecture v0.9") },
    { "", i18n ("Use back-end defaults") },
    { 0, QString () }
};

static OutputDriver _vds [] = {
    { "xv,sdl,x11", i18n ("Auto") },
    { "x11", i18n ("X11Shm") },
    { "xvidix", i18n ("XVidix") },
    { "xvmc,xv", i18n ("XvMC") },
    { "sdl", i18n ("SDL") },
    { "gl", i18n ("OpenGL") },
    { "gl2", i18n ("OpenGL MT") },
    { "xv", i18n ("XVideo") },
    { 0, QString () }
};

static const int ADRIVER_ARTS_INDEX = 4;


KDE_NO_CDTOR_EXPORT Settings::Settings (PartBase * player, KSharedConfigPtr config)
  : pagelist (0L), configdialog (0L), m_config (config), m_player (player) {
    audiodrivers = _ads;
    videodrivers = _vds;
    colors [ColorSetting::playlist_background].title = i18n ("Playlist background");
    colors [ColorSetting::playlist_background].option = "PlaylistBackground";
    colors [ColorSetting::playlist_background].color =
        KColorScheme(QPalette::Active, KColorScheme::View).background().color();
    colors [ColorSetting::playlist_foreground].title = i18n ("Playlist foreground");
    colors [ColorSetting::playlist_foreground].option = "PlaylistForeground";
    colors [ColorSetting::playlist_foreground].color =
        KColorScheme(QPalette::Active, KColorScheme::View).foreground().color();
    colors [ColorSetting::console_background].title =i18n("Console background");
    colors [ColorSetting::playlist_active].title = i18n("Playlist active item");
    colors [ColorSetting::playlist_active].option = "PlaylistActive";
    colors [ColorSetting::playlist_active].color =
        KColorScheme(QPalette::Active, KColorScheme::Selection).foreground().color();
    colors [ColorSetting::console_background].option = "ConsoleBackground";
    colors [ColorSetting::console_background].color = QColor (0, 0, 0);
    colors [ColorSetting::console_foreground].title = i18n ("Console foreground");
    colors [ColorSetting::console_foreground].option = "ConsoleForeground";
    colors [ColorSetting::console_foreground].color = QColor (0xB2, 0xB2, 0xB2);
    colors [ColorSetting::video_background].title = i18n ("Video background");
    colors [ColorSetting::video_background].option = "VideoBackground";
    colors [ColorSetting::video_background].color = QColor (0, 0, 0);
    colors [ColorSetting::area_background].title = i18n ("Viewing area background");
    colors [ColorSetting::area_background].option = "ViewingAreaBackground";
    colors [ColorSetting::area_background].color = QColor (0, 0, 0);
    colors [ColorSetting::infowindow_background].title = i18n ("Info window background");
    colors [ColorSetting::infowindow_background].option ="InfoWindowBackground";
    colors [ColorSetting::infowindow_background].color =
        KColorScheme(QPalette::Active, KColorScheme::View).background().color();
    colors [ColorSetting::infowindow_foreground].title = i18n ("Info window foreground");
    colors [ColorSetting::infowindow_foreground].option ="InfoWindowForeground";
    colors [ColorSetting::infowindow_foreground].color =
        colors [ColorSetting::playlist_foreground].color;
    fonts [FontSetting::playlist].title = i18n ("Playlist");
    fonts [FontSetting::playlist].option = "PlaylistFont";
    fonts [FontSetting::playlist].font = KGlobalSettings::generalFont();
    fonts [FontSetting::playlist].font.setItalic (true);
    fonts [FontSetting::infowindow].title = i18n ("Info window");
    fonts [FontSetting::infowindow].option = "InfoWindowFont";
    fonts [FontSetting::infowindow].font = KGlobalSettings::generalFont();
}

KDE_NO_CDTOR_EXPORT Settings::~Settings () {
    // configdialog should be destroyed when the view is destroyed
    //delete configdialog;
}

KDE_EXPORT const char * strMPlayerGroup = "MPlayer";
const char * strGeneralGroup = "General Options";
static const char * strKeepSizeRatio = "Keep Size Ratio";
static const char * strRememberSize = "Remember Size";
static const char * strAutoResize = "Auto Resize";
static const char * strDockSysTray = "Dock in System Tray";
static const char * strNoIntro = "No Intro";
static const char * strVolume = "Volume";
static const char * strContrast = "Contrast";
static const char * strBrightness = "Brightness";
static const char * strHue = "Hue";
static const char * strSaturation = "Saturation";
static const char * strURLList = "URL List";
static const char * strSubURLList = "URL Sub Title List";
static const char * strPrefBitRate = "Prefered Bitrate";
static const char * strMaxBitRate = "Maximum Bitrate";
//static const char * strUseArts = "Use aRts";
static const char * strVoDriver = "Video Driver";
static const char * strAoDriver = "Audio Driver";
static const char * strLoop = "Loop";
static const char * strFrameDrop = "Frame Drop";
static const char * strAdjustVolume = "Auto Adjust Volume";
static const char * strAdjustColors = "Auto Adjust Colors";
static const char * strAddConfigButton = "Add Configure Button";
static const char * strAddPlaylistButton = "Add Playlist Button";
static const char * strAddRecordButton = "Add Record Button";
static const char * strAddBroadcastButton = "Add Broadcast Button";
static const char * strPostMPlayer090 = "Post MPlayer 0.90";
//static const char * strAutoHideSlider = "Auto Hide Slider";
static const char * strSeekTime = "Forward/Backward Seek Time";
static const char * strDVDDevice = "DVD Device";
//static const char * strShowDVD = "Show DVD Menu";
//static const char * strShowVCD = "Show VCD Menu";
static const char * strVCDDevice = "VCD Device";
const char * strUrlBackend = "URL Backend";
static const char * strClickToPlay = "Click to Play";
static const char * strAllowHref = "Allow HREF";
// postproc thingies
static const char * strPPGroup = "Post processing options";
static const char * strPostProcessing = "Post processing";
static const char * strDisablePPauto = "Automaticly disable post processing";
static const char * strPP_Default = "Default preset";
static const char * strPP_Fast = "Fast preset";
static const char * strPP_Custom = "Custom preset";

static const char * strCustom_Hz = "Horizontal deblocking";
static const char * strCustom_Hz_Aq = "Horizontal deblocking auto quality";
static const char * strCustom_Hz_Ch = "Horizontal deblocking chrominance";

static const char * strCustom_Vt = "Vertical deblocking";
static const char * strCustom_Vt_Aq = "Vertical deblocking auto quality";
static const char * strCustom_Vt_Ch = "Vertical deblocking chrominance";

static const char * strCustom_Dr = "Dering filter";
static const char * strCustom_Dr_Aq = "Dering auto quality";
static const char * strCustom_Dr_Ch = "Dering chrominance";

static const char * strCustom_Al = "Autolevel";
static const char * strCustom_Al_F = "Autolevel full range";

static const char * strCustom_Tn = "Temporal Noise Reducer";
static const char * strCustom_Tn_S = "Temporal Noise Reducer strength";

static const char * strPP_Lin_Blend_Int = "Linear Blend Deinterlacer";
static const char * strPP_Lin_Int = "Linear Interpolating Deinterlacer";
static const char * strPP_Cub_Int = "Cubic Interpolating Deinterlacer";
static const char * strPP_Med_Int = "Median Interpolating Deinterlacer";
static const char * strPP_FFmpeg_Int = "FFmpeg Interpolating Deinterlacer";
// end of postproc
// recording
static const char * strRecordingGroup = "Recording";
static const char * strRecorder = "Recorder";
static const char * strMencoderArgs = "Mencoder Arguments";
static const char * strFFMpegArgs = "FFMpeg Arguments";
static const char * strRecordingFile = "Last Recording Ouput File";
static const char * strAutoPlayAfterRecording = "Auto Play After Recording";
static const char * strAutoPlayAfterTime = "Auto Play After Recording Time";
static const char * strRecordingCopy = "Recording Is Copy";

KDE_NO_EXPORT void Settings::applyColorSetting (bool only_changed_ones) {
    View *view = static_cast <View *> (m_player->view ());
    if (!view) return;
    for (int i = 0; i < int (ColorSetting::last_target); i++)
        if (!only_changed_ones || colors[i].color != colors[i].newcolor) {
            colors[i].color = colors[i].newcolor;
            QPalette palette;
            switch (ColorSetting::Target (i)) {
                case ColorSetting::playlist_background:
                   palette.setColor (view->playList()->backgroundRole(), colors[i].color);
                   view->playList()->setPalette (palette);
                   break;
                case ColorSetting::playlist_foreground:
                   palette.setColor (view->playList()->foregroundRole(), colors[i].color);
                   view->playList()->setPalette (palette);
                   break;
                case ColorSetting::playlist_active:
                   view->playList()->setActiveForegroundColor (colors[i].color);
                   break;
                case ColorSetting::console_background:
                   palette.setColor (view->console()->backgroundRole(), colors[i].color);
                   view->console()->setPalette (palette);
                   break;
                case ColorSetting::console_foreground:
                   palette.setColor (view->console()->foregroundRole(), colors[i].color);
                   view->console()->setPalette (palette);
                   break;
                case ColorSetting::video_background:
                   //palette.setColor (view->viewer()->backgroundRole(), colors[i].color);
                   //view->viewer()->setPalette (palette);
                   break;
                case ColorSetting::area_background:
                   palette.setColor (view->viewArea()->backgroundRole(), colors[i].color);
                   view->viewArea()->setPalette (palette);
                   break;
                case ColorSetting::infowindow_background:
                   palette.setColor(view->infoPanel()->backgroundRole(), colors[i].color);
                   view->infoPanel()->setPalette (palette);
                   break;
                case ColorSetting::infowindow_foreground:
                   palette.setColor(view->infoPanel()->foregroundRole(), colors[i].color);
                   view->infoPanel()->setPalette (palette);
                   break;
                default:
                    ;
            }
        }
    for (int i = 0; i < int (FontSetting::last_target); i++)
        if (!only_changed_ones || fonts[i].font != fonts[i].newfont) {
            fonts[i].font = fonts[i].newfont;
            switch (FontSetting::Target (i)) {
                case FontSetting::playlist:
                   view->playList ()->setFont (fonts[i].font);
                   break;
                case FontSetting::infowindow:
                   view->infoPanel ()->setFont (fonts[i].font);
                   break;
                default:
                    ;
            }
        }
}

View * Settings::defaultView () {
    return static_cast <View *> (m_player->view ());
}

KDE_NO_EXPORT void Settings::readConfig () {
    KConfigGroup general (m_config, strGeneralGroup);
    no_intro = general.readEntry (strNoIntro, false);
    urllist = general.readEntry (strURLList, QStringList());
    sub_urllist = general.readEntry (strSubURLList, QStringList());
    prefbitrate = general.readEntry (strPrefBitRate, 512);
    maxbitrate = general.readEntry (strMaxBitRate, 1024);
    volume = general.readEntry (strVolume, 20);
    contrast = general.readEntry (strContrast, 0);
    brightness = general.readEntry (strBrightness, 0);
    hue = general.readEntry (strHue, 0);
    saturation = general.readEntry (strSaturation, 0);
    const QMap <QString, Source*>::const_iterator e = m_player->sources ().end ();
    QMap <QString, Source *>::const_iterator i = m_player->sources().begin ();
    for (; i != e; ++i)
        backends[i.data()->name ()] = general.readEntry (i.data()->name ());
    for (int i = 0; i < int (ColorSetting::last_target); i++)
        colors[i].newcolor = colors[i].color = general.readEntry (colors[i].option, colors[i].color);
    for (int i = 0; i < int (FontSetting::last_target); i++)
        fonts[i].newfont = fonts[i].font = general.readEntry (fonts[i].option, fonts[i].font);

    KConfigGroup mplayer (m_config, strMPlayerGroup);
    sizeratio = mplayer.readEntry (strKeepSizeRatio, true);
    remembersize = mplayer.readEntry (strRememberSize, true);
    autoresize = mplayer.readEntry (strAutoResize, true);
    docksystray = mplayer.readEntry (strDockSysTray, true);
    loop = mplayer.readEntry (strLoop, false);
    framedrop = mplayer.readEntry (strFrameDrop, true);
    autoadjustvolume = mplayer.readEntry (strAdjustVolume, true);
    autoadjustcolors = mplayer.readEntry (strAdjustColors, true);
    mplayerpost090 = mplayer.readEntry (strPostMPlayer090, true);
    showcnfbutton = mplayer.readEntry (strAddConfigButton, true);
    showrecordbutton = mplayer.readEntry (strAddRecordButton, true);
    showbroadcastbutton = mplayer.readEntry (strAddBroadcastButton, true);
    showplaylistbutton = mplayer.readEntry (strAddPlaylistButton, true);
    seektime = mplayer.readEntry (strSeekTime, 10);
    dvddevice = mplayer.readEntry (strDVDDevice, "/dev/dvd");
    vcddevice = mplayer.readEntry (strVCDDevice, "/dev/cdrom");
    videodriver = mplayer.readEntry (strVoDriver, 0);
    audiodriver = mplayer.readEntry (strAoDriver, 0);
    clicktoplay = mplayer.readEntry (strClickToPlay, false);
    grabhref = mplayer.readEntry (strAllowHref, false);

    // recording
    KConfigGroup rec_cfg (m_config, strRecordingGroup);
    mencoderarguments = rec_cfg.readEntry (strMencoderArgs, QString ("-oac mp3lame -ovc lavc"));
    ffmpegarguments = rec_cfg.readEntry (strFFMpegArgs, QString ("-f avi -acodec mp3 -vcodec mpeg4"));
    recordfile = rec_cfg.readPathEntry(strRecordingFile, QDir::homeDirPath () + "/record.avi");
    recorder = Recorder (rec_cfg.readEntry (strRecorder, int (MEncoder)));
    replayoption = ReplayOption (rec_cfg.readEntry (strAutoPlayAfterRecording, int (ReplayFinished)));
    replaytime = rec_cfg.readEntry (strAutoPlayAfterTime, 60);
    recordcopy = rec_cfg.readEntry(strRecordingCopy, true);

    // postproc
    KConfigGroup pp_cfg (m_config, strPPGroup);
    postprocessing = pp_cfg.readEntry (strPostProcessing, false);
    disableppauto = pp_cfg.readEntry (strDisablePPauto, true);

    pp_default = pp_cfg.readEntry (strPP_Default, true);
    pp_fast = pp_cfg.readEntry (strPP_Fast, false);
    pp_custom = pp_cfg.readEntry (strPP_Custom, false);
    // default these to default preset
    pp_custom_hz = pp_cfg.readEntry (strCustom_Hz, true);
    pp_custom_hz_aq = pp_cfg.readEntry (strCustom_Hz_Aq, true);
    pp_custom_hz_ch = pp_cfg.readEntry (strCustom_Hz_Ch, false);

    pp_custom_vt = pp_cfg.readEntry (strCustom_Vt, true);
    pp_custom_vt_aq = pp_cfg.readEntry (strCustom_Vt_Aq, true);
    pp_custom_vt_ch = pp_cfg.readEntry (strCustom_Vt_Ch, false);

    pp_custom_dr = pp_cfg.readEntry (strCustom_Dr, true);
    pp_custom_dr_aq = pp_cfg.readEntry (strCustom_Dr_Aq, true);
    pp_custom_dr_ch = pp_cfg.readEntry (strCustom_Dr_Ch, false);

    pp_custom_al = pp_cfg.readEntry (strCustom_Al, true);
    pp_custom_al_f = pp_cfg.readEntry (strCustom_Al_F, false);

    pp_custom_tn = pp_cfg.readEntry (strCustom_Tn, true);
    pp_custom_tn_s = pp_cfg.readEntry (strCustom_Tn_S, 0);

    pp_lin_blend_int = pp_cfg.readEntry (strPP_Lin_Blend_Int, false);
    pp_lin_int = pp_cfg.readEntry (strPP_Lin_Int, false);
    pp_cub_int = pp_cfg.readEntry (strPP_Cub_Int, false);
    pp_med_int = pp_cfg.readEntry (strPP_Med_Int, false);
    pp_ffmpeg_int = pp_cfg.readEntry (strPP_FFmpeg_Int, false);

    for (PreferencesPage * p = pagelist; p; p = p->next)
        p->read (m_config);
    emit configChanged ();
}

KDE_NO_EXPORT bool Settings::createDialog () {
    if (configdialog) return false;
    configdialog = new Preferences (m_player, this);
    int id = 0;
    const MediaManager::ProcessInfoMap::const_iterator e = m_player->mediaManager()->processInfos ().end ();
    for (MediaManager::ProcessInfoMap::const_iterator i = m_player->mediaManager()->processInfos ().begin(); i != e; ++i) {
        ProcessInfo *p = i.data ();
        if (p->supports ("urlsource")) {
            QString lbl = p->label.remove (QChar ('&'));
            configdialog->m_SourcePageURL->backend->insertItem (lbl, id++);
        }
    }
    connect (configdialog, SIGNAL (okClicked ()),
            this, SLOT (okPressed ()));
    connect (configdialog, SIGNAL (applyClicked ()),
            this, SLOT (okPressed ()));
    if (KApplication::kApplication())
        connect (configdialog, SIGNAL (helpClicked ()),
                this, SLOT (getHelp ()));
    return true;
}

void Settings::addPage (PreferencesPage * page) {
    for (PreferencesPage * p = pagelist; p; p = p->next)
        if (p == page)
            return;
    page->read (m_config);
    if (configdialog) {
        configdialog->addPrefPage (page);
        page->sync (false);
    }
    page->next = pagelist;
    pagelist = page;
}

void Settings::removePage (PreferencesPage * page) {
    if (configdialog)
        configdialog->removePrefPage (page);
    PreferencesPage * prev = 0L;
    for (PreferencesPage * p = pagelist; p; prev = p, p = p->next)
        if (p == page) {
            if (prev)
                prev->next = p->next;
            else
                pagelist = p->next;
            break;
        }
}

void Settings::show (const char * pagename) {
    bool created = createDialog ();
    configdialog->m_GeneralPageGeneral->keepSizeRatio->setChecked (sizeratio);
    configdialog->m_GeneralPageGeneral->autoResize->setChecked (autoresize);
    configdialog->m_GeneralPageGeneral->sizesChoice->setButton (remembersize ? 0 : 1);
    configdialog->m_GeneralPageGeneral->dockSysTray->setChecked (docksystray);
    configdialog->m_GeneralPageGeneral->loop->setChecked (loop);
    configdialog->m_GeneralPageGeneral->framedrop->setChecked (framedrop);
    configdialog->m_GeneralPageGeneral->adjustvolume->setChecked (autoadjustvolume);
    configdialog->m_GeneralPageGeneral->adjustcolors->setChecked (autoadjustcolors);
    //configdialog->m_GeneralPageGeneral->autoHideSlider->setChecked (autohideslider);
    configdialog->m_GeneralPageGeneral->showConfigButton->setChecked (showcnfbutton);
    configdialog->m_GeneralPageGeneral->showPlaylistButton->setChecked (showplaylistbutton);
    configdialog->m_GeneralPageGeneral->showRecordButton->setChecked (showrecordbutton);
    configdialog->m_GeneralPageGeneral->showBroadcastButton->setChecked (showbroadcastbutton);
    configdialog->m_GeneralPageGeneral->seekTime->setValue(seektime);
    for (int i = 0; i < int (ColorSetting::last_target); i++)
        colors[i].newcolor = colors[i].color;
    for (int i = 0; i < int (FontSetting::last_target); i++)
        fonts[i].newfont = fonts[i].font;
    configdialog->m_SourcePageURL->urllist->clear ();
    configdialog->m_SourcePageURL->urllist->insertStringList (urllist);
    configdialog->m_SourcePageURL->urllist->setCurrentText (m_player->source ()->url ().prettyUrl ());
    configdialog->m_SourcePageURL->sub_urllist->clear ();
    configdialog->m_SourcePageURL->sub_urllist->insertStringList (sub_urllist);
    configdialog->m_SourcePageURL->sub_urllist->setCurrentText (m_player->source ()->subUrl ().prettyUrl ());
    configdialog->m_SourcePageURL->changed = false;
    configdialog->m_SourcePageURL->prefBitRate->setText (QString::number (prefbitrate));
    configdialog->m_SourcePageURL->maxBitRate->setText (QString::number (maxbitrate));

    configdialog->m_GeneralPageOutput->videoDriver->setCurrentItem (videodriver);
    configdialog->m_GeneralPageOutput->audioDriver->setCurrentItem (audiodriver);
    configdialog->m_SourcePageURL->backend->setCurrentItem (configdialog->m_SourcePageURL->backend->findItem (backends["urlsource"]));
    int id = 0;
    const MediaManager::ProcessInfoMap::const_iterator e = m_player->mediaManager()->processInfos ().end ();
    for (MediaManager::ProcessInfoMap::const_iterator i = m_player->mediaManager()->processInfos ().begin(); i != e; ++i) {
        ProcessInfo *p = i.data ();
        if (p->supports ("urlsource")) {
            if (backends["urlsource"] == QString (p->name))
                configdialog->m_SourcePageURL->backend->setCurrentItem (id);
            id++;
        }
    }
    configdialog->m_SourcePageURL->clicktoplay->setChecked (clicktoplay);
    configdialog->m_SourcePageURL->grabhref->setChecked (grabhref);

    // postproc
    configdialog->m_OPPagePostproc->postProcessing->setChecked (postprocessing);
    configdialog->m_OPPagePostproc->disablePPauto->setChecked (disableppauto);
    configdialog->m_OPPagePostproc->PostprocessingOptions->setEnabled (postprocessing);

    configdialog->m_OPPagePostproc->defaultPreset->setChecked (pp_default);
    configdialog->m_OPPagePostproc->fastPreset->setChecked (pp_fast);
    configdialog->m_OPPagePostproc->customPreset->setChecked (pp_custom);

    configdialog->m_OPPagePostproc->HzDeblockFilter->setChecked (pp_custom_hz);
    configdialog->m_OPPagePostproc->HzDeblockAQuality->setChecked (pp_custom_hz_aq);
    configdialog->m_OPPagePostproc->HzDeblockCFiltering->setChecked (pp_custom_hz_ch);

    configdialog->m_OPPagePostproc->VtDeblockFilter->setChecked (pp_custom_vt);
    configdialog->m_OPPagePostproc->VtDeblockAQuality->setChecked (pp_custom_vt_aq);
    configdialog->m_OPPagePostproc->VtDeblockCFiltering->setChecked (pp_custom_vt_ch);

    configdialog->m_OPPagePostproc->DeringFilter->setChecked (pp_custom_dr);
    configdialog->m_OPPagePostproc->DeringAQuality->setChecked (pp_custom_dr_aq);
    configdialog->m_OPPagePostproc->DeringCFiltering->setChecked (pp_custom_dr_ch);

    configdialog->m_OPPagePostproc->AutolevelsFilter->setChecked (pp_custom_al);
    configdialog->m_OPPagePostproc->AutolevelsFullrange->setChecked (pp_custom_al_f);
    configdialog->m_OPPagePostproc->TmpNoiseFilter->setChecked (pp_custom_tn);
    //configdialog->m_OPPagePostproc->TmpNoiseSlider->setValue (pp_custom_tn_s);

    configdialog->m_OPPagePostproc->LinBlendDeinterlacer->setChecked (pp_lin_blend_int);
    configdialog->m_OPPagePostproc->LinIntDeinterlacer->setChecked (pp_lin_int);
    configdialog->m_OPPagePostproc->CubicIntDeinterlacer->setChecked (pp_cub_int);
    configdialog->m_OPPagePostproc->MedianDeinterlacer->setChecked (pp_med_int);
    configdialog->m_OPPagePostproc->FfmpegDeinterlacer->setChecked (pp_ffmpeg_int);
    // recording
    configdialog->m_RecordPage->url->lineEdit()->setText (recordfile);
    configdialog->m_RecordPage->replay->setButton (int (replayoption));
    configdialog->m_RecordPage->recorder->setButton (int (recorder));
    configdialog->m_RecordPage->replayClicked (int (replayoption));
    configdialog->m_RecordPage->recorderClicked (int (recorder));
    configdialog->m_RecordPage->replaytime->setText (QString::number (replaytime));
    configdialog->m_MEncoderPage->arguments->setText (mencoderarguments);
    configdialog->m_MEncoderPage->format->setButton (recordcopy ? 0 : 1);
    configdialog->m_MEncoderPage->formatClicked (recordcopy ? 0 : 1);
    configdialog->m_FFMpegPage->arguments->setText (ffmpegarguments);

    //dynamic stuff
    for (PreferencesPage * p = pagelist; p; p = p->next)
        p->sync (false);
    //\dynamic stuff
    if (pagename)
        configDialog ()->setPage (pagename);
    if (created)
        configdialog->resize (configdialog->minimumSize ());
    configdialog->show ();
}

void Settings::writeConfig () {
    KConfigGroup gen_cfg (m_config, strGeneralGroup);
    gen_cfg.writeEntry (strURLList, urllist);
    gen_cfg.writeEntry (strSubURLList, sub_urllist);
    gen_cfg.writeEntry (strPrefBitRate, prefbitrate);
    gen_cfg.writeEntry (strMaxBitRate, maxbitrate);
    gen_cfg.writeEntry (strVolume, volume);
    gen_cfg.writeEntry (strContrast, contrast);
    gen_cfg.writeEntry (strBrightness, brightness);
    gen_cfg.writeEntry (strHue, hue);
    gen_cfg.writeEntry (strSaturation, saturation);
    const QMap<QString,QString>::iterator b_end = backends.end ();
    for (QMap<QString,QString>::iterator i = backends.begin(); i != b_end; ++i)
        gen_cfg.writeEntry (i.key (), i.data ());
    for (int i = 0; i < int (ColorSetting::last_target); i++)
        gen_cfg.writeEntry (colors[i].option, colors[i].color);
    for (int i = 0; i < int (FontSetting::last_target); i++)
        gen_cfg.writeEntry (fonts[i].option, fonts[i].font);

    KConfigGroup mplayer_cfg (m_config, strMPlayerGroup);
    mplayer_cfg.writeEntry (strKeepSizeRatio, sizeratio);
    mplayer_cfg.writeEntry (strAutoResize, autoresize);
    mplayer_cfg.writeEntry (strRememberSize, remembersize);
    mplayer_cfg.writeEntry (strDockSysTray, docksystray);
    mplayer_cfg.writeEntry (strLoop, loop);
    mplayer_cfg.writeEntry (strFrameDrop, framedrop);
    mplayer_cfg.writeEntry (strAdjustVolume, autoadjustvolume);
    mplayer_cfg.writeEntry (strAdjustColors, autoadjustcolors);
    mplayer_cfg.writeEntry (strSeekTime, seektime);
    mplayer_cfg.writeEntry (strVoDriver, videodriver);
    mplayer_cfg.writeEntry (strAoDriver, audiodriver);
    mplayer_cfg.writeEntry (strClickToPlay, clicktoplay);
    mplayer_cfg.writeEntry (strAllowHref, grabhref);
    mplayer_cfg.writeEntry (strAddConfigButton, showcnfbutton);
    mplayer_cfg.writeEntry (strAddPlaylistButton, showplaylistbutton);
    mplayer_cfg.writeEntry (strAddRecordButton, showrecordbutton);
    mplayer_cfg.writeEntry (strAddBroadcastButton, showbroadcastbutton);
    mplayer_cfg.writeEntry (strDVDDevice, dvddevice);
    mplayer_cfg.writeEntry (strVCDDevice, vcddevice);

    //postprocessing stuff
    KConfigGroup pp_cfg (m_config, strPPGroup);
    pp_cfg.writeEntry (strPostProcessing, postprocessing);
    pp_cfg.writeEntry (strDisablePPauto, disableppauto);
    pp_cfg.writeEntry (strPP_Default, pp_default);
    pp_cfg.writeEntry (strPP_Fast, pp_fast);
    pp_cfg.writeEntry (strPP_Custom, pp_custom);

    pp_cfg.writeEntry (strCustom_Hz, pp_custom_hz);
    pp_cfg.writeEntry (strCustom_Hz_Aq, pp_custom_hz_aq);
    pp_cfg.writeEntry (strCustom_Hz_Ch, pp_custom_hz_ch);

    pp_cfg.writeEntry (strCustom_Vt, pp_custom_vt);
    pp_cfg.writeEntry (strCustom_Vt_Aq, pp_custom_vt_aq);
    pp_cfg.writeEntry (strCustom_Vt_Ch, pp_custom_vt_ch);

    pp_cfg.writeEntry (strCustom_Dr, pp_custom_dr);
    pp_cfg.writeEntry (strCustom_Dr_Aq, pp_custom_vt_aq);
    pp_cfg.writeEntry (strCustom_Dr_Ch, pp_custom_vt_ch);

    pp_cfg.writeEntry (strCustom_Al, pp_custom_al);
    pp_cfg.writeEntry (strCustom_Al_F, pp_custom_al_f);

    pp_cfg.writeEntry (strCustom_Tn, pp_custom_tn);
    pp_cfg.writeEntry (strCustom_Tn_S, pp_custom_tn_s);

    pp_cfg.writeEntry (strPP_Lin_Blend_Int, pp_lin_blend_int);
    pp_cfg.writeEntry (strPP_Lin_Int, pp_lin_int);
    pp_cfg.writeEntry (strPP_Cub_Int, pp_cub_int);
    pp_cfg.writeEntry (strPP_Med_Int, pp_med_int);
    pp_cfg.writeEntry (strPP_FFmpeg_Int, pp_ffmpeg_int);

    // recording
    KConfigGroup rec_cfg (m_config, strRecordingGroup);
    rec_cfg.writePathEntry (strRecordingFile, recordfile);
    rec_cfg.writeEntry (strAutoPlayAfterRecording, int (replayoption));
    rec_cfg.writeEntry (strAutoPlayAfterTime, replaytime);
    rec_cfg.writeEntry (strRecorder, int (recorder));
    rec_cfg.writeEntry (strRecordingCopy, recordcopy);
    rec_cfg.writeEntry (strMencoderArgs, mencoderarguments);
    rec_cfg.writeEntry (strFFMpegArgs, ffmpegarguments);

    //dynamic stuff
    for (PreferencesPage * p = pagelist; p; p = p->next)
        p->write (m_config);
    //\dynamic stuff
    m_config->sync ();
}

void Settings::okPressed () {
    bool urlchanged = configdialog->m_SourcePageURL->changed;
    bool playerchanged = false;
    KUrl url = configdialog->m_SourcePageURL->url->url ();
    KUrl sub_url = configdialog->m_SourcePageURL->sub_url->url ();
    if (urlchanged) {
        if (url.isEmpty ()) {
            urlchanged = false;
        } else {
            if (KUrl (url.url ()).isLocalFile () || KUrl::isRelativeUrl (url.url ())) {
                QFileInfo fi (url.path ());
                int hpos = url.url ().lastIndexOf ('#');
                QString xine_directives ("");
                while (!fi.exists () && hpos > -1) {
                    xine_directives = url.url ().mid (hpos);
                    fi.setFile (url.url ().left (hpos));
                    hpos = url.url ().lastIndexOf ('#', hpos-1);
                }
                if (!fi.exists ()) {
                    urlchanged = false;
                    KMessageBox::error (m_player->view (), i18n ("File %1 does not exist.",url.url ()), i18n ("Error"));
                } else {
                    configdialog->m_SourcePageURL->url->setUrl (fi.absFilePath () + xine_directives);
                }
            }
            if (urlchanged &&
                    !sub_url.url ().isEmpty () &&
                    (KUrl (sub_url.url ()).isLocalFile () ||
                     KUrl::isRelativeUrl (sub_url.url ()))) {
                QFileInfo sfi (sub_url.path ());
                if (!sfi.exists ()) {
                    KMessageBox::error (m_player->view (), i18n ("Sub title file %1 does not exist.",sub_url.url ()), i18n ("Error"));
                    configdialog->m_SourcePageURL->sub_url->setUrl (QString ());
                } else
                    configdialog->m_SourcePageURL->sub_url->setUrl (sfi.absFilePath ());
            }
        }
    }
    if (urlchanged) {
        KUrl uri (url.url ());
        m_player->setUrl (uri.url ());
        if (urllist.find (uri.prettyUrl ()) == urllist.end ())
            configdialog->m_SourcePageURL->urllist->insertItem (uri.prettyUrl (), 0);
        KUrl sub_uri (sub_url.url ());
        if (sub_urllist.find (sub_uri.prettyUrl ()) == sub_urllist.end ())
            configdialog->m_SourcePageURL->sub_urllist->insertItem (sub_uri.prettyUrl (), 0);
    }
    urllist.clear ();
    for (int i = 0; i < configdialog->m_SourcePageURL->urllist->count () && i < 20; ++i)
        // damnit why don't maxCount and setDuplicatesEnabled(false) work :(
        // and why can I put a qstringlist in it, but cannot get it out of it again..
        if (!configdialog->m_SourcePageURL->urllist->text (i).isEmpty ())
            urllist.push_back (configdialog->m_SourcePageURL->urllist->text (i));
    sub_urllist.clear ();
    for (int i = 0; i < configdialog->m_SourcePageURL->sub_urllist->count () && i < 20; ++i)
        if (!configdialog->m_SourcePageURL->sub_urllist->text (i).isEmpty ())
            sub_urllist.push_back (configdialog->m_SourcePageURL->sub_urllist->text (i));
    prefbitrate = configdialog->m_SourcePageURL->prefBitRate->text ().toInt ();
    maxbitrate = configdialog->m_SourcePageURL->maxBitRate->text ().toInt ();
    sizeratio = configdialog->m_GeneralPageGeneral->keepSizeRatio->isChecked ();
    autoresize = configdialog->m_GeneralPageGeneral->autoResize->isChecked ();
    remembersize=!configdialog->m_GeneralPageGeneral->sizesChoice->selectedId();
    docksystray = configdialog->m_GeneralPageGeneral->dockSysTray->isChecked ();
    loop = configdialog->m_GeneralPageGeneral->loop->isChecked ();
    framedrop = configdialog->m_GeneralPageGeneral->framedrop->isChecked ();
    autoadjustvolume = configdialog->m_GeneralPageGeneral->adjustvolume->isChecked ();
    autoadjustcolors = configdialog->m_GeneralPageGeneral->adjustcolors->isChecked ();
    showcnfbutton = configdialog->m_GeneralPageGeneral->showConfigButton->isChecked ();
    showplaylistbutton = configdialog->m_GeneralPageGeneral->showPlaylistButton->isChecked ();
    showrecordbutton = configdialog->m_GeneralPageGeneral->showRecordButton->isChecked ();
    showbroadcastbutton = configdialog->m_GeneralPageGeneral->showBroadcastButton->isChecked ();
    seektime = configdialog->m_GeneralPageGeneral->seekTime->value();

    videodriver = configdialog->m_GeneralPageOutput->videoDriver->currentItem();
    audiodriver = configdialog->m_GeneralPageOutput->audioDriver->currentItem();
    QString backend_name = configdialog->m_SourcePageURL->backend->currentText ();
    if (!backend_name.isEmpty ()) {
        const MediaManager::ProcessInfoMap::const_iterator e = m_player->mediaManager()->processInfos ().end ();
        for (MediaManager::ProcessInfoMap::const_iterator i = m_player->mediaManager()->processInfos ().begin(); i != e; ++i) {
            ProcessInfo *p = i.data ();
            if (p->supports ("urlsource") &&
                    p->label.remove (QChar ('&')) == backend_name) {
                backends["urlsource"] = p->name;
                break;
            }
        }
    }
    clicktoplay = configdialog->m_SourcePageURL->clicktoplay->isChecked ();
    grabhref = configdialog->m_SourcePageURL->grabhref->isChecked ();
    //postproc
    postprocessing = configdialog->m_OPPagePostproc->postProcessing->isChecked();
    disableppauto = configdialog->m_OPPagePostproc->disablePPauto->isChecked();
    pp_default = configdialog->m_OPPagePostproc->defaultPreset->isChecked();
    pp_fast = configdialog->m_OPPagePostproc->fastPreset->isChecked();
    pp_custom = configdialog->m_OPPagePostproc->customPreset->isChecked();

    pp_custom_hz = configdialog->m_OPPagePostproc->HzDeblockFilter->isChecked();
    pp_custom_hz_aq = configdialog->m_OPPagePostproc->HzDeblockAQuality->isChecked();
    pp_custom_hz_ch = configdialog->m_OPPagePostproc->HzDeblockCFiltering->isChecked();

    pp_custom_vt = configdialog->m_OPPagePostproc->VtDeblockFilter->isChecked();
    pp_custom_vt_aq = configdialog->m_OPPagePostproc->VtDeblockAQuality->isChecked();
    pp_custom_vt_ch = configdialog->m_OPPagePostproc->VtDeblockCFiltering->isChecked();

    pp_custom_dr = configdialog->m_OPPagePostproc->DeringFilter->isChecked();
    pp_custom_dr_aq = configdialog->m_OPPagePostproc->DeringAQuality->isChecked();
    pp_custom_dr_ch = configdialog->m_OPPagePostproc->DeringCFiltering->isChecked();

    pp_custom_al = configdialog->m_OPPagePostproc->AutolevelsFilter->isChecked();
    pp_custom_al_f = configdialog->m_OPPagePostproc->AutolevelsFullrange->isChecked();

    pp_custom_tn = configdialog->m_OPPagePostproc->TmpNoiseFilter->isChecked();
    pp_custom_tn_s = 0; // gotta fix this later
    //pp_custom_tn_s = configdialog->m_OPPagePostproc->TmpNoiseSlider->value();

    pp_lin_blend_int = configdialog->m_OPPagePostproc->LinBlendDeinterlacer->isChecked();
    pp_lin_int = configdialog->m_OPPagePostproc->LinIntDeinterlacer->isChecked();
    pp_cub_int = configdialog->m_OPPagePostproc->CubicIntDeinterlacer->isChecked();
    pp_med_int = configdialog->m_OPPagePostproc->MedianDeinterlacer->isChecked();
    pp_ffmpeg_int = configdialog->m_OPPagePostproc->FfmpegDeinterlacer->isChecked();
    // recording
#if (QT_VERSION < 0x030200)
    recorder = Recorder (configdialog->m_RecordPage->recorder->id (configdialog->m_RecordPage->recorder->selected ()));
#else
    recorder = Recorder (configdialog->m_RecordPage->recorder->selectedId ());
#endif
    replaytime = configdialog->m_RecordPage->replaytime->text ().toInt ();
    configdialog->m_RecordPage->replaytime->setText (QString::number (replaytime));
    recordfile = configdialog->m_RecordPage->url->lineEdit()->text ();
    mencoderarguments = configdialog->m_MEncoderPage->arguments->text ();
    ffmpegarguments = configdialog->m_FFMpegPage->arguments->text ();
#if (QT_VERSION < 0x030200)
    recordcopy = !configdialog->m_MEncoderPage->format->id (configdialog->m_MEncoderPage->format->selected ());
#else
    recordcopy = !configdialog->m_MEncoderPage->format->selectedId ();
#endif

    //dynamic stuff
    for (PreferencesPage * p = pagelist; p; p = p->next)
        p->sync (true);
    //\dynamic stuff

    writeConfig ();
    emit configChanged ();

    if (urlchanged || playerchanged) {
        m_player->sources () ["urlsource"]->setSubURL
            (KUrl(configdialog->m_SourcePageURL->sub_url->url()));
        m_player->openUrl (KUrl (configdialog->m_SourcePageURL->url->url ()));
        m_player->source ()->setSubURL (KUrl (configdialog->m_SourcePageURL->sub_url->url ()));
    }
}

KDE_NO_EXPORT void Settings::getHelp () {
   // KApplication::kApplication()->invokeBrowser ("man:/mplayer");
}

#include "kmplayerconfig.moc"

