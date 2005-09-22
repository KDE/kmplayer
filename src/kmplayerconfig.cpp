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
 *  the Free Software Foundation, Inc., 51 Franklin Steet, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 **/

#include <algorithm>
#include <functional>
#include <config.h>
#include <qcheckbox.h>
#include <qtextedit.h>
#include <qpushbutton.h>
#include <qradiobutton.h>
#include <qmultilineedit.h>
#include <qtabwidget.h>
#include <qslider.h>
#include <qspinbox.h>
#include <qlabel.h>
#include <qbuttongroup.h>
#include <qfileinfo.h>

#include <kurlrequester.h>
#include <klineedit.h>

#include <kconfig.h>
#include <kapplication.h>
#include <kurl.h>
#include <kdebug.h>
#include <klocale.h>
#include <kcombobox.h>
#include <kmessagebox.h>
#include <kglobalsettings.h>

#include "kmplayersource.h"
#include "kmplayerconfig.h"
#include "kmplayerpartbase.h"
#include "kmplayerprocess.h"
#include "pref.h"

using namespace KMPlayer;

static OutputDriver _ads[] = {
    { "", i18n ("Use back-end defaults") },
    { "oss", i18n ("Open Sound System") },
    { "sdl", i18n ("Simple DirectMedia Layer") },
    { "alsa", i18n ("Advanced Linux Sound Architecture") },
    { "arts", i18n ("Analog Real-Time Synthesizer") },
    { "esd", i18n ("Enlightened Sound Daemon") },
    { "alsa5", i18n ("Advanced Linux Sound Architecture v0.5") },
    { "alsa9", i18n ("Advanced Linux Sound Architecture v0.9") },
    { 0, QString::null }
};

static OutputDriver _vds [] = {
    { "xv", i18n ("XVideo") },
    { "x11", i18n ("X11Shm") },
    { "xvidix", i18n ("XVidix") },
    { 0, QString::null }
};

static const int ADRIVER_ARTS_INDEX = 4;


KDE_NO_CDTOR_EXPORT Settings::Settings (PartBase * player, KConfig * config)
  : pagelist (0L), configdialog (0L), m_config (config), m_player (player) {
    audiodrivers = _ads;
    videodrivers = _vds;
    colors [ColorSetting::playlist_background].title = i18n ("Playlist background");
    colors [ColorSetting::playlist_background].option = "PlaylistBackground";
    colors [ColorSetting::playlist_background].color = QColor (0, 0, 0);
    colors [ColorSetting::playlist_foreground].title = i18n ("Playlist foreground");
    colors [ColorSetting::playlist_foreground].option = "PlaylistForeground";
    colors [ColorSetting::playlist_foreground].color = QColor(0xB2, 0xB2, 0xB2);
    colors [ColorSetting::console_background].title =i18n("Console background");
    colors [ColorSetting::playlist_active].title = i18n("Playlist active item");
    colors [ColorSetting::playlist_active].option = "PlaylistActive";
    colors [ColorSetting::playlist_active].color = QColor (0xFF, 0xFF, 0xFF);
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
    colors [ColorSetting::infowindow_background].color = QColor (0, 0, 0);
    colors [ColorSetting::infowindow_foreground].title = i18n ("Info window foreground");
    colors [ColorSetting::infowindow_foreground].option ="InfoWindowForeground";
    colors [ColorSetting::infowindow_foreground].color=QColor(0xB2, 0xB2, 0xB2);
    fonts [FontSetting::playlist].title = i18n ("Playlist");
    fonts [FontSetting::playlist].option = "PlaylistFont";
    fonts [FontSetting::playlist].font = KGlobalSettings::generalFont();
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
        if (colors[i].color != colors[i].newcolor || !only_changed_ones) {
            colors[i].color = colors[i].newcolor;
            switch (ColorSetting::Target (i)) {
                case ColorSetting::playlist_background:
                   view->playList()->setPaletteBackgroundColor(colors[i].color);
                   break;
                case ColorSetting::playlist_foreground:
                   view->playList()->setPaletteForegroundColor(colors[i].color);
                   break;
                case ColorSetting::playlist_active:
                   view->playList()->setActiveForegroundColor (colors[i].color);
                   break;
                case ColorSetting::console_background:
                   view->console()->setPaper (QBrush (colors[i].color));
                   break;
                case ColorSetting::console_foreground:
                   view->console()->setColor(colors[i].color);
                   break;
                case ColorSetting::video_background:
                   view->viewer ()->setBackgroundColor (colors[i].color);
                   break;
                case ColorSetting::area_background:
                   view->viewArea()->setPaletteBackgroundColor(colors[i].color);
                   break;
                case ColorSetting::infowindow_background:
                   view->infoPanel ()->setPaper (QBrush (colors[i].color));
                   break;
                case ColorSetting::infowindow_foreground:
                  view->infoPanel()->setPaletteForegroundColor(colors[i].color);
                  view->infoPanel ()->setColor (colors[i].color);
                  break;
                default:
                    ;
            }
        }
    for (int i = 0; i < int (FontSetting::last_target); i++)
        if (fonts[i].font != fonts[i].newfont || !only_changed_ones) {
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
    m_config->setGroup (strGeneralGroup);
    no_intro = m_config->readBoolEntry (strNoIntro, false);
    urllist = m_config->readListEntry (strURLList, ';');
    sub_urllist = m_config->readListEntry (strSubURLList, ';');
    prefbitrate = m_config->readNumEntry (strPrefBitRate, 512);
    maxbitrate = m_config->readNumEntry (strMaxBitRate, 1024);
    volume = m_config->readNumEntry (strVolume, 80);
    contrast = m_config->readNumEntry (strContrast, 0);
    brightness = m_config->readNumEntry (strBrightness, 0);
    hue = m_config->readNumEntry (strHue, 0);
    saturation = m_config->readNumEntry (strSaturation, 0);
    const QMap <QString, Source*>::const_iterator e = m_player->sources ().end ();
    QMap <QString, Source *>::const_iterator i = m_player->sources().begin ();
    for (; i != e; ++i)
        backends[i.data()->name ()] = m_config->readEntry (i.data()->name ());
    for (int i = 0; i < int (ColorSetting::last_target); i++)
        colors[i].newcolor = colors[i].color = m_config->readColorEntry (colors[i].option, &colors[i].color);
    for (int i = 0; i < int (FontSetting::last_target); i++)
        fonts[i].newfont = fonts[i].font = m_config->readFontEntry (fonts[i].option, &fonts[i].font);

    m_config->setGroup (strMPlayerGroup);
    sizeratio = m_config->readBoolEntry (strKeepSizeRatio, true);
    remembersize = m_config->readBoolEntry (strRememberSize, true);
    autoresize = m_config->readBoolEntry (strAutoResize, true);
    docksystray = m_config->readBoolEntry (strDockSysTray, true);
    loop = m_config->readBoolEntry (strLoop, false);
    framedrop = m_config->readBoolEntry (strFrameDrop, true);
    autoadjustvolume = m_config->readBoolEntry (strAdjustVolume, true);
    mplayerpost090 = m_config->readBoolEntry (strPostMPlayer090, true);
    showcnfbutton = m_config->readBoolEntry (strAddConfigButton, true);
    showrecordbutton = m_config->readBoolEntry (strAddRecordButton, true);
    showbroadcastbutton = m_config->readBoolEntry (strAddBroadcastButton, true);
    showplaylistbutton = m_config->readBoolEntry (strAddPlaylistButton, true);
    seektime = m_config->readNumEntry (strSeekTime, 10);
    dvddevice = m_config->readEntry (strDVDDevice, "/dev/dvd");
    vcddevice = m_config->readEntry (strVCDDevice, "/dev/cdrom");
    videodriver = m_config->readNumEntry (strVoDriver, 0);
    audiodriver = m_config->readNumEntry (strAoDriver, 0);
    allowhref = m_config->readBoolEntry(strAllowHref, false);

    // recording
    m_config->setGroup (strRecordingGroup);
    mencoderarguments = m_config->readEntry (strMencoderArgs, "-oac mp3lame -ovc lavc");
    ffmpegarguments = m_config->readEntry (strFFMpegArgs, "-f avi -acodec mp3 -vcodec mpeg4");
    recordfile = m_config->readPathEntry(strRecordingFile, QDir::homeDirPath () + "/record.avi");
    recorder = Recorder (m_config->readNumEntry (strRecorder, int (MEncoder)));
    replayoption = ReplayOption (m_config->readNumEntry (strAutoPlayAfterRecording, ReplayFinished));
    replaytime = m_config->readNumEntry (strAutoPlayAfterTime, 60);
    recordcopy = m_config->readBoolEntry(strRecordingCopy, true);

    // postproc
    m_config->setGroup (strPPGroup);
    postprocessing = m_config->readBoolEntry (strPostProcessing, false);
    disableppauto = m_config->readBoolEntry (strDisablePPauto, true);

    pp_default = m_config->readBoolEntry (strPP_Default, true);
    pp_fast = m_config->readBoolEntry (strPP_Fast, false);
    pp_custom = m_config->readBoolEntry (strPP_Custom, false);
    // default these to default preset
    pp_custom_hz = m_config->readBoolEntry (strCustom_Hz, true);
    pp_custom_hz_aq = m_config->readBoolEntry (strCustom_Hz_Aq, true);
    pp_custom_hz_ch = m_config->readBoolEntry (strCustom_Hz_Ch, false);

    pp_custom_vt = m_config->readBoolEntry (strCustom_Vt, true);
    pp_custom_vt_aq = m_config->readBoolEntry (strCustom_Vt_Aq, true);
    pp_custom_vt_ch = m_config->readBoolEntry (strCustom_Vt_Ch, false);

    pp_custom_dr = m_config->readBoolEntry (strCustom_Dr, true);
    pp_custom_dr_aq = m_config->readBoolEntry (strCustom_Dr_Aq, true);
    pp_custom_dr_ch = m_config->readBoolEntry (strCustom_Dr_Ch, false);

    pp_custom_al = m_config->readBoolEntry (strCustom_Al, true);
    pp_custom_al_f = m_config->readBoolEntry (strCustom_Al_F, false);

    pp_custom_tn = m_config->readBoolEntry (strCustom_Tn, true);
    pp_custom_tn_s = m_config->readNumEntry (strCustom_Tn_S, 0);

    pp_lin_blend_int = m_config->readBoolEntry (strPP_Lin_Blend_Int, false);
    pp_lin_int = m_config->readBoolEntry (strPP_Lin_Int, false);
    pp_cub_int = m_config->readBoolEntry (strPP_Cub_Int, false);
    pp_med_int = m_config->readBoolEntry (strPP_Med_Int, false);
    pp_ffmpeg_int = m_config->readBoolEntry (strPP_FFmpeg_Int, false);

    for (PreferencesPage * p = pagelist; p; p = p->next)
        p->read (m_config);
    emit configChanged ();
}

KDE_NO_EXPORT bool Settings::createDialog () {
    if (configdialog) return false;
    configdialog = new Preferences (m_player, this);
    int id = 0;
    const PartBase::ProcessMap::const_iterator e = m_player->players ().end ();
    for (PartBase::ProcessMap::const_iterator i = m_player->players ().begin(); i != e; ++i) {
        Process * p = i.data ();
        if (p->supports ("urlsource"))
            configdialog->m_SourcePageURL->backend->insertItem (p->menuName ().remove (QChar ('&')), id++);
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
    configdialog->m_SourcePageURL->urllist->setCurrentText (m_player->source ()->url ().prettyURL ());
    configdialog->m_SourcePageURL->sub_urllist->clear ();
    configdialog->m_SourcePageURL->sub_urllist->insertStringList (sub_urllist);
    configdialog->m_SourcePageURL->sub_urllist->setCurrentText (m_player->source ()->subUrl ().prettyURL ());
    configdialog->m_SourcePageURL->changed = false;
    configdialog->m_SourcePageURL->prefBitRate->setText (QString::number (prefbitrate));
    configdialog->m_SourcePageURL->maxBitRate->setText (QString::number (maxbitrate));

    configdialog->m_GeneralPageOutput->videoDriver->setCurrentItem (videodriver);
    configdialog->m_GeneralPageOutput->audioDriver->setCurrentItem (audiodriver);
    configdialog->m_SourcePageURL->backend->setCurrentItem (configdialog->m_SourcePageURL->backend->findItem (backends["urlsource"]));
    int id = 0;
    const PartBase::ProcessMap::const_iterator e = m_player->players ().end ();
    for (PartBase::ProcessMap::const_iterator i = m_player->players ().begin(); i != e; ++i) {
        Process * p = i.data ();
        if (p->supports ("urlsource")) {
            if (backends["urlsource"] == QString (p->name()))
                configdialog->m_SourcePageURL->backend->setCurrentItem (id);
            id++;
        }
    }
    configdialog->m_SourcePageURL->allowhref->setChecked (allowhref);

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
    m_config->setGroup (strGeneralGroup);
    m_config->writeEntry (strURLList, urllist, ';');
    m_config->writeEntry (strSubURLList, sub_urllist, ';');
    m_config->writeEntry (strPrefBitRate, prefbitrate);
    m_config->writeEntry (strMaxBitRate, maxbitrate);
    m_config->writeEntry (strVolume, volume);
    m_config->writeEntry (strContrast, contrast);
    m_config->writeEntry (strBrightness, brightness);
    m_config->writeEntry (strHue, hue);
    m_config->writeEntry (strSaturation, saturation);
    const QMap<QString,QString>::iterator b_end = backends.end ();
    for (QMap<QString,QString>::iterator i = backends.begin(); i != b_end; ++i)
        m_config->writeEntry (i.key (), i.data ());
    for (int i = 0; i < int (ColorSetting::last_target); i++)
        m_config->writeEntry (colors[i].option, colors[i].color);
    for (int i = 0; i < int (FontSetting::last_target); i++)
        m_config->writeEntry (fonts[i].option, fonts[i].font);
    m_config->setGroup (strMPlayerGroup);
    m_config->writeEntry (strKeepSizeRatio, sizeratio);
    m_config->writeEntry (strAutoResize, autoresize);
    m_config->writeEntry (strRememberSize, remembersize);
    m_config->writeEntry (strDockSysTray, docksystray);
    m_config->writeEntry (strLoop, loop);
    m_config->writeEntry (strFrameDrop, framedrop);
    m_config->writeEntry (strAdjustVolume, autoadjustvolume);
    m_config->writeEntry (strSeekTime, seektime);
    m_config->writeEntry (strVoDriver, videodriver);
    m_config->writeEntry (strAoDriver, audiodriver);
    m_config->writeEntry (strAllowHref, allowhref);
    m_config->writeEntry (strAddConfigButton, showcnfbutton);
    m_config->writeEntry (strAddPlaylistButton, showplaylistbutton);
    m_config->writeEntry (strAddRecordButton, showrecordbutton);
    m_config->writeEntry (strAddBroadcastButton, showbroadcastbutton);

    m_config->writeEntry (strDVDDevice, dvddevice);
    m_config->writeEntry (strVCDDevice, vcddevice);

    //postprocessing stuff
    m_config->setGroup (strPPGroup);
    m_config->writeEntry (strPostProcessing, postprocessing);
    m_config->writeEntry (strDisablePPauto, disableppauto);
    m_config->writeEntry (strPP_Default, pp_default);
    m_config->writeEntry (strPP_Fast, pp_fast);
    m_config->writeEntry (strPP_Custom, pp_custom);

    m_config->writeEntry (strCustom_Hz, pp_custom_hz);
    m_config->writeEntry (strCustom_Hz_Aq, pp_custom_hz_aq);
    m_config->writeEntry (strCustom_Hz_Ch, pp_custom_hz_ch);

    m_config->writeEntry (strCustom_Vt, pp_custom_vt);
    m_config->writeEntry (strCustom_Vt_Aq, pp_custom_vt_aq);
    m_config->writeEntry (strCustom_Vt_Ch, pp_custom_vt_ch);

    m_config->writeEntry (strCustom_Dr, pp_custom_dr);
    m_config->writeEntry (strCustom_Dr_Aq, pp_custom_vt_aq);
    m_config->writeEntry (strCustom_Dr_Ch, pp_custom_vt_ch);

    m_config->writeEntry (strCustom_Al, pp_custom_al);
    m_config->writeEntry (strCustom_Al_F, pp_custom_al_f);

    m_config->writeEntry (strCustom_Tn, pp_custom_tn);
    m_config->writeEntry (strCustom_Tn_S, pp_custom_tn_s);

    m_config->writeEntry (strPP_Lin_Blend_Int, pp_lin_blend_int);
    m_config->writeEntry (strPP_Lin_Int, pp_lin_int);
    m_config->writeEntry (strPP_Cub_Int, pp_cub_int);
    m_config->writeEntry (strPP_Med_Int, pp_med_int);
    m_config->writeEntry (strPP_FFmpeg_Int, pp_ffmpeg_int);

    // recording
    m_config->setGroup (strRecordingGroup);
    m_config->writePathEntry (strRecordingFile, recordfile);
    m_config->writeEntry (strAutoPlayAfterRecording, int (replayoption));
    m_config->writeEntry (strAutoPlayAfterTime, replaytime);
    m_config->writeEntry (strRecorder, int (recorder));
    m_config->writeEntry (strRecordingCopy, recordcopy);
    m_config->writeEntry (strMencoderArgs, mencoderarguments);
    m_config->writeEntry (strFFMpegArgs, ffmpegarguments);

    //dynamic stuff
    for (PreferencesPage * p = pagelist; p; p = p->next)
        p->write (m_config);
    //\dynamic stuff
    m_config->sync ();
}

KDE_NO_EXPORT void Settings::okPressed () {
    bool urlchanged = configdialog->m_SourcePageURL->changed;
    bool playerchanged = false;
    if (urlchanged) {
        if (configdialog->m_SourcePageURL->url->url ().isEmpty ())
            urlchanged = false;
        else {
            if (KURL::fromPathOrURL (configdialog->m_SourcePageURL->url->url ()).isLocalFile () ||
                    KURL::isRelativeURL (configdialog->m_SourcePageURL->url->url ())) {
                QFileInfo fi (configdialog->m_SourcePageURL->url->url ());
                int hpos = configdialog->m_SourcePageURL->url->url ().findRev ('#');
                QString xine_directives ("");
                while (!fi.exists () && hpos > -1) {
                    xine_directives = configdialog->m_SourcePageURL->url->url ().mid (hpos);
                    fi.setFile (configdialog->m_SourcePageURL->url->url ().left (hpos));
                    hpos = configdialog->m_SourcePageURL->url->url ().findRev ('#', hpos-1);
                }
                if (!fi.exists ()) {
                    urlchanged = false;
                    KMessageBox::error (m_player->view (), i18n ("File %1 does not exist.").arg (configdialog->m_SourcePageURL->url->url ()), i18n ("Error"));
                } else
                    configdialog->m_SourcePageURL->url->setURL (fi.absFilePath () + xine_directives);
            }
            if (urlchanged &&
                    !configdialog->m_SourcePageURL->sub_url->url ().isEmpty () &&
                    (KURL::fromPathOrURL (configdialog->m_SourcePageURL->sub_url->url ()).isLocalFile () ||
                     KURL::isRelativeURL (configdialog->m_SourcePageURL->sub_url->url ()))) {
                QFileInfo sfi (configdialog->m_SourcePageURL->sub_url->url ());
                if (!sfi.exists ()) {
                    KMessageBox::error (m_player->view (), i18n ("Sub title file %1 does not exist.").arg (configdialog->m_SourcePageURL->sub_url->url ()), i18n ("Error"));
                    configdialog->m_SourcePageURL->sub_url->setURL (QString::null);
                } else
                    configdialog->m_SourcePageURL->sub_url->setURL (sfi.absFilePath ());
            }
        }
    }
    if (urlchanged) {
        KURL url = KURL::fromPathOrURL (configdialog->m_SourcePageURL->url->url ());
        m_player->setURL (url);
        if (urllist.find (url.prettyURL ()) == urllist.end ())
            configdialog->m_SourcePageURL->urllist->insertItem (url.prettyURL (), 0);
        KURL sub_url = KURL::fromPathOrURL (configdialog->m_SourcePageURL->sub_url->url ());
        if (sub_urllist.find (sub_url.prettyURL ()) == sub_urllist.end ())
            configdialog->m_SourcePageURL->sub_urllist->insertItem (sub_url.prettyURL (), 0);
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
    showcnfbutton = configdialog->m_GeneralPageGeneral->showConfigButton->isChecked ();
    showplaylistbutton = configdialog->m_GeneralPageGeneral->showPlaylistButton->isChecked ();
    showrecordbutton = configdialog->m_GeneralPageGeneral->showRecordButton->isChecked ();
    showbroadcastbutton = configdialog->m_GeneralPageGeneral->showBroadcastButton->isChecked ();
    seektime = configdialog->m_GeneralPageGeneral->seekTime->value();

    videodriver = configdialog->m_GeneralPageOutput->videoDriver->currentItem();
    audiodriver = configdialog->m_GeneralPageOutput->audioDriver->currentItem();
    int backend = configdialog->m_SourcePageURL->backend->currentItem ();
    const PartBase::ProcessMap::const_iterator e = m_player->players ().end();
    for (PartBase::ProcessMap::const_iterator i = m_player->players ().begin(); backend >=0 && i != e; ++i) {
        Process * proc = i.data ();
        if (proc->supports ("urlsource") && backend-- == 0) {
            backends["urlsource"] = proc->name ();
            if (proc != m_player->process ()) {
                m_player->setProcess (proc->name ());
                playerchanged = true;
            }
        }
    }
    allowhref = configdialog->m_SourcePageURL->allowhref->isChecked ();
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
            (KURL(configdialog->m_SourcePageURL->sub_url->url()));
        m_player->openURL (KURL::fromPathOrURL (configdialog->m_SourcePageURL->url->url ()));
        m_player->source ()->setSubURL (KURL::fromPathOrURL (configdialog->m_SourcePageURL->sub_url->url ()));
    }
}

KDE_NO_EXPORT void Settings::getHelp () {
    KApplication::kApplication()->invokeBrowser ("man:/mplayer");
}

#include "kmplayerconfig.moc"

