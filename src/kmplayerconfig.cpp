/***************************************************************************
                          kmplayerconfig.cpp  -  description
                             -------------------
    begin                : 2002/12/30
    copyright            : (C) 2002 by Koos Vriezen
    email                : |EMAIL|
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <qcheckbox.h>
#include <qtextedit.h>
#include <qpushbutton.h>
#include <qradiobutton.h>
#include <qmultilineedit.h>
#include <qtabwidget.h>
#include <qslider.h>

#include <kconfig.h>
#include <kfiledialog.h>
#include <kapplication.h>
#include <kurl.h>
#include <kdebug.h>

#include "kmplayerconfig.h"
#include "kmplayer_part.h"
#include "kmplayerview.h"
#include "configdialog.h"

KMPlayerConfig::KMPlayerConfig (KMPlayer * player, KConfig * config)
  : configdialog (0L), m_config (config), m_player (player) {
}

KMPlayerConfig::~KMPlayerConfig () {
    // configdialog should be destroyed when the view is destroyed
    //delete configdialog;
}

static const char * strMPlayerGroup = "MPlayer";
static const char * strMPlayerPatternGroup = "MPlayer Output Matching";
static const char * strKeepSizeRatio = "Keep Size Ratio";
static const char * strVoDriver = "Video Driver";
static const char * strAddArgs = "Additional Arguments";
static const char * strSize = "Movie Size";
static const char * strCache = "Cache Fill";
static const char * strStart = "Start Playing";
static const char * strShowConsole = "Show Console Output";
static const char * strLoop = "Loop";
static const char * strShowControlButtons = "Show Control Buttons";
static const char * strAddConfigButton = "Add Configure Button";
static const char * strAutoHideButtons = "Auto Hide Control Buttons";
static const char * strSeekTime = "Forward/Backward Seek Time";
static const char * strCacheSize = "Cache Size for Streaming";
static const char * strPlayDVD = "Immediately Play DVD";
static const char * strShowDVD = "Show DVD Menu";
static const char * strDVDDevice = "DVD Device";
static const char * strLanguagePattern = "DVD Language";
static const char * strSubtitlePattern = "DVD Sub Title";
static const char * strTitlePattern = "DVD Titles";
static const char * strChapterPattern = "DVD Chapters";
static const char * strPlayVCD = "Immediately Play VCD";
static const char * strShowVCD = "Show VCD Menu";
static const char * strVCDDevice = "VCD Device";
static const char * strTrackPattern = "VCD Tracks";
// postproc thingies
static const char * strPPGroup = "Post processing options";
static const char * strPostProcessing = "Post processing";
static const char * strPP_Default = "Default preset used";
static const char * strPP_Fast = "Fast preset used";
static const char * strPP_Custom = "Custom preset used";

static const char * strCustom_Hz = "Horizontal deblocking";
static const char * strCustom_Hz_Aq = "Hb Auto quality";
static const char * strCustom_Hz_Ch = "Hb Chrominance";

static const char * strCustom_Vt = "Vertical deblocking";
static const char * strCustom_Vt_Aq = "Vb Auto quality";
static const char * strCustom_Vt_Ch = "Vb Chrominance";

static const char * strCustom_Dr = "Dering filter";
static const char * strCustom_Dr_Aq = "Dr Auto quality";
static const char * strCustom_Dr_Ch = "Dr Chrominance";

static const char * strCustom_Al = "Autolevel";
static const char * strCustom_Al_F = "Full range";

static const char * strCustom_Tn = "Temporal Noise Reducer";
static const char * strCustom_Tn_S = "Strength";

static const char * strPP_Lin_Blend_Int = "Linear Blend Deinterlacer";
static const char * strPP_Lin_Int = "Linear Interpolating Deinterlacer";
static const char * strPP_Cub_Int = "Cubic Interpolating Deinterlacer";
static const char * strPP_Med_Int = "Median Interpolating Deinterlacer";
static const char * strPP_FFmpeg_Int = "FFmpeg Interpolating Deinterlacer";
// end of postproc

void KMPlayerConfig::readConfig () {
    KMPlayerView *view = static_cast <KMPlayerView *> (m_player->view ());
    m_config->setGroup (strMPlayerGroup);

    sizeratio = m_config->readBoolEntry (strKeepSizeRatio, true);
    view->setKeepSizeRatio (sizeratio);
    showconsole = m_config->readBoolEntry (strShowConsole, false);
    view->setShowConsoleOutput (showconsole);
    loop = m_config->readBoolEntry (strLoop, false);
    showbuttons = m_config->readBoolEntry (strShowControlButtons, true);
    autohidebuttons = m_config->readBoolEntry (strAutoHideButtons, false);
    view->setAutoHideButtons (showbuttons && autohidebuttons);
    if (!showbuttons) {
        view->buttonBar ()->hide ();
    }
    showcnfbutton = m_config->readBoolEntry (strAddConfigButton, true);
    if (showcnfbutton)
        view->configButton ()->show ();
    else
        view->configButton ()->hide ();
    seektime = m_config->readNumEntry (strSeekTime, 10);
    m_player->setSeekTime (seektime);
    playdvd = m_config->readBoolEntry (strPlayDVD, true);
    dvddevice = m_config->readEntry (strDVDDevice, "/dev/dvd");
    showdvdmenu = m_config->readBoolEntry (strShowDVD, true);
    playvcd = m_config->readBoolEntry (strPlayVCD, true);
    vcddevice = m_config->readEntry (strVCDDevice, "/dev/cdrom");
    showvcdmenu = m_config->readBoolEntry (strShowVCD, true);
    videodriver = m_config->readEntry (strVoDriver, "");
    additionalarguments = m_config->readEntry (strAddArgs, "");
    cachesize = m_config->readNumEntry (strCacheSize, 64);
    m_player->setCacheSize (cachesize);
    m_config->setGroup (strMPlayerPatternGroup);
    sizepattern = m_config->readEntry (strSize, "VO:.*[^0-9]([0-9]+)x([0-9]+)");
    cachepattern = m_config->readEntry (strCache, "Cache fill:[^0-9]*([0-9\\.]+)%");
    startpattern = m_config->readEntry (strStart, "Start[^ ]* play");
    langpattern = m_config->readEntry (strLanguagePattern, "\\[open].*audio.*language: ([A-Za-z]+).*aid.*[^0-9]([0-9]+)");
    subtitlespattern = m_config->readEntry (strSubtitlePattern, "\\[open].*subtitle.*[^0-9]([0-9]+).*language: ([A-Za-z]+)");
    titlespattern = m_config->readEntry (strTitlePattern, "There are ([0-9]+) titles");
    chapterspattern = m_config->readEntry (strChapterPattern, "There are ([0-9]+) chapters");
    trackspattern = m_config->readEntry (strTrackPattern, "track ([0-9]+):");

    // postproc
    m_config->setGroup (strPPGroup);
    postprocessing = m_config->readBoolEntry (strPostProcessing, false);
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

    pp_custom_tn = m_config->readBoolEntry (strCustom_Al, true);
    pp_custom_tn_s = m_config->readNumEntry (strCustom_Al_F, 0);

    pp_lin_blend_int = m_config->readBoolEntry (strPP_Lin_Blend_Int, false);
    pp_lin_int = m_config->readBoolEntry (strPP_Lin_Int, false);
    pp_cub_int = m_config->readBoolEntry (strPP_Cub_Int, false);
    pp_med_int = m_config->readBoolEntry (strPP_Med_Int, false);
    pp_ffmpeg_int = m_config->readBoolEntry (strPP_FFmpeg_Int, false);

}

void KMPlayerConfig::show () {
    if (!configdialog) {
        configdialog = new ConfigDialog (m_player->view ());
        connect (configdialog->buttonOk, SIGNAL (clicked ()), 
                this, SLOT (okPressed ()));
        connect (configdialog->buttonApply, SIGNAL (clicked ()), 
                this, SLOT (okPressed ()));
        connect (configdialog->openFile, SIGNAL (clicked ()), 
                this, SLOT (fileOpen ()));
        connect (configdialog->showDVDMenu, SIGNAL (toggled (bool)), 
                configdialog->dvdTab, SLOT (setEnabled (bool)));
        connect (configdialog->showVCDMenu, SIGNAL (toggled (bool)), 
                configdialog->vcdTab, SLOT (setEnabled (bool)));
        connect (configdialog->showControlButtons, SIGNAL (toggled (bool)), 
                configdialog->addConfigButton, SLOT (setEnabled (bool)));
        connect (configdialog->showControlButtons, SIGNAL (toggled (bool)), 
                configdialog->autoHideControlButtons, SLOT (setEnabled (bool)));
        connect (configdialog->haveVideoDriver, SIGNAL (toggled (bool)), 
                configdialog->videoDriver, SLOT (setEnabled (bool)));
        connect (configdialog->haveCache, SIGNAL (toggled (bool)), 
                configdialog->cacheSize, SLOT (setEnabled (bool)));
        connect (configdialog->haveArguments, SIGNAL (toggled (bool)), 
                configdialog->additionalArguments, SLOT (setEnabled (bool)));
        if (KApplication::kApplication())
            connect (configdialog->buttonHelp, SIGNAL (clicked ()),
                     this, SLOT (getHelp ()));
        else
            configdialog->buttonHelp->hide ();
    }
    configdialog->url->setText (m_player->url ().url ());
    configdialog->keepSizeRatio->setChecked (sizeratio);
    configdialog->showConsoleOutput->setChecked (showconsole);
    configdialog->loop->setChecked (loop);
    configdialog->showControlButtons->setChecked (showbuttons);
    configdialog->addConfigButton->setChecked (showcnfbutton);
    configdialog->addConfigButton->setEnabled (showbuttons);
    configdialog->autoHideControlButtons->setChecked (autohidebuttons);
    configdialog->autoHideControlButtons->setEnabled (showbuttons);
    configdialog->seekTime->setText (QString::number (seektime));
    configdialog->dvdPlay->setChecked (playdvd);
    configdialog->showDVDMenu->setChecked (showdvdmenu);
    configdialog->dvdTab->setEnabled (showdvdmenu);
    configdialog->dvdDevice->setText (dvddevice);
    configdialog->vcdPlay->setChecked (playvcd);
    configdialog->showVCDMenu->setChecked (showvcdmenu);
    configdialog->vcdTab->setEnabled (showvcdmenu);
    configdialog->vcdDevice->setText (vcddevice);
    configdialog->haveVideoDriver->setChecked (videodriver.length () > 0);
    configdialog->videoDriver->setEnabled (videodriver.length () > 0);
    if (videodriver.length () > 0)
        configdialog->videoDriver->setText (videodriver);
    configdialog->haveCache->setChecked (cachesize > 0);
    configdialog->cacheSize->setEnabled (cachesize > 0);
    if (cachesize > 0)
        configdialog->cacheSize->setText (QString::number (cachesize));
    bool haveArgs = additionalarguments.length () > 0;
    configdialog->haveArguments->setChecked (haveArgs);
    configdialog->additionalArguments->setEnabled (haveArgs);
    if (haveArgs)
        configdialog->additionalArguments->setText (additionalarguments);
    configdialog->sizePattern->setText (sizepattern);
    configdialog->cachePattern->setText (cachepattern);
    configdialog->startPattern->setText (startpattern);
    configdialog->langPattern->setText (langpattern);
    configdialog->subtitlesPattern->setText (subtitlespattern);
    configdialog->titlesPattern->setText (titlespattern);
    configdialog->chaptersPattern->setText (chapterspattern);
    configdialog->tracksPattern->setText (trackspattern);

    // postproc
    configdialog->postProcessing->setChecked (postprocessing);
    if (postprocessing)
    {
        //configdialog->PostprocessingOptions->setEnabled (true);
    }
    else {
        configdialog->PostprocessingOptions->setEnabled (false);
    }
    configdialog->defaultPreset->setChecked (pp_default);
    configdialog->fastPreset->setChecked (pp_fast);
    configdialog->customPreset->setChecked (pp_custom);

    configdialog->HzDeblockFilter->setChecked (pp_custom_hz);
    configdialog->HzDeblockAQuality->setChecked (pp_custom_hz_aq);
    configdialog->HzDeblockCFiltering->setChecked (pp_custom_hz_ch);

    configdialog->VtDeblockFilter->setChecked (pp_custom_vt);
    configdialog->VtDeblockAQuality->setChecked (pp_custom_vt_aq);
    configdialog->VtDeblockCFiltering->setChecked (pp_custom_vt_ch);

    configdialog->DeringFilter->setChecked (pp_custom_dr);
    configdialog->DeringAQuality->setChecked (pp_custom_dr_aq);
    configdialog->DeringCFiltering->setChecked (pp_custom_dr_ch);

    configdialog->AutolevelsFilter->setChecked (pp_custom_al);
    configdialog->AutolevelsFullrange->setChecked (pp_custom_al_f);
    configdialog->TmpNoiseFilter->setChecked (pp_custom_tn);
    configdialog->TmpNoiseSlider->setValue (pp_custom_tn_s);

    configdialog->LinBlendDeinterlacer->setChecked (pp_lin_blend_int);
    configdialog->LinIntDeinterlacer->setChecked (pp_lin_int);
    configdialog->CubicIntDeinterlacer->setChecked (pp_cub_int);
    configdialog->MedianDeinterlacer->setChecked (pp_med_int);
    configdialog->FfmpegDeinterlacer->setChecked (pp_ffmpeg_int);


    configdialog->show ();
}

void KMPlayerConfig::writeConfig () {
    KMPlayerView *view = static_cast <KMPlayerView *> (m_player->view ());
    m_config->setGroup (strMPlayerGroup);
    m_config->writeEntry (strKeepSizeRatio, view->keepSizeRatio ());
    m_config->writeEntry (strShowConsole, view->showConsoleOutput());
    m_config->writeEntry (strLoop, loop);
    m_config->writeEntry (strSeekTime, m_player->seekTime ());
    m_config->writeEntry (strVoDriver, videodriver);
    m_config->writeEntry (strAddArgs, additionalarguments);
    m_config->writeEntry (strCacheSize, m_player->cacheSize ());
    m_config->writeEntry (strShowControlButtons, showbuttons);
    m_config->writeEntry (strAddConfigButton, showcnfbutton);
    m_config->writeEntry (strAutoHideButtons, autohidebuttons);
    m_config->writeEntry (strPlayDVD, playdvd);
    m_config->writeEntry (strShowDVD, showdvdmenu);
    m_config->writeEntry (strDVDDevice, dvddevice);
    m_config->writeEntry (strPlayVCD, playvcd);
    m_config->writeEntry (strShowVCD, showvcdmenu);
    m_config->writeEntry (strVCDDevice, vcddevice);
    m_config->setGroup (strMPlayerPatternGroup);
    m_config->writeEntry (strSize, sizepattern);
    m_config->writeEntry (strCache, cachepattern);
    m_config->writeEntry (strStart, startpattern);
    m_config->writeEntry (strLanguagePattern, langpattern);
    m_config->writeEntry (strSubtitlePattern, subtitlespattern);
    m_config->writeEntry (strTitlePattern, titlespattern);
    m_config->writeEntry (strChapterPattern, chapterspattern);
    m_config->writeEntry (strTrackPattern, trackspattern);
    //postprocessing stuff
    m_config->setGroup (strPPGroup);
    m_config->writeEntry (strPostProcessing, postprocessing);
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

    m_config->sync ();
}

void KMPlayerConfig::okPressed () {
    KMPlayerView *view = static_cast <KMPlayerView *> (m_player->view ());
    if (!view)
        return;
    bool urlchanged = m_player->url () != KURL (configdialog->url->text ());
    if (m_player->url ().isEmpty () && configdialog->url->text ().isEmpty ())
        urlchanged = false; // hmmm aren't these URLs the same?
    if (urlchanged)
        m_player->setURL (configdialog->url->text ());
    sizeratio = configdialog->keepSizeRatio->isChecked ();
    m_player->keepMovieAspect (sizeratio);
    showconsole = configdialog->showConsoleOutput->isChecked ();
    view->setShowConsoleOutput (showconsole);
    loop = configdialog->loop->isChecked ();
    if (showconsole && !m_player->playing ())
        view->consoleOutput ()->show ();
    else
        view->consoleOutput ()->hide ();
    showbuttons = configdialog->showControlButtons->isChecked ();
    autohidebuttons = configdialog->autoHideControlButtons->isChecked ();
    view->setAutoHideButtons (showbuttons && autohidebuttons);
    if (showbuttons)
        view->buttonBar ()->show ();
    else
        view->buttonBar ()->hide ();
    showcnfbutton = configdialog->addConfigButton->isChecked ();
    if (showcnfbutton)
        view->configButton ()->show ();
    else
        view->configButton ()->hide ();
    playdvd = configdialog->dvdPlay->isChecked ();
    showdvdmenu = configdialog->showDVDMenu->isChecked ();
    dvddevice = configdialog->dvdDevice->text ();
    playvcd = configdialog->vcdPlay->isChecked ();
    showvcdmenu = configdialog->showVCDMenu->isChecked ();
    vcddevice = configdialog->vcdDevice->text ();
    seektime = configdialog->seekTime->text ().toInt ();
    m_player->setSeekTime (seektime);
    videodriver = configdialog->haveVideoDriver->isChecked () ?
        configdialog->videoDriver->text () : QString ("");
    additionalarguments = configdialog->haveArguments->isChecked () ?
        configdialog->additionalArguments->text () : QString ("");
    cachesize = configdialog->haveCache->isChecked () ?
        configdialog->cacheSize->text ().toInt () : 0; 
    m_player->setCacheSize (cachesize);
    sizepattern = configdialog->sizePattern->text ();
    cachepattern = configdialog->cachePattern->text ();
    startpattern = configdialog->startPattern->text ();
    langpattern = configdialog->langPattern->text ();
    titlespattern = configdialog->titlesPattern->text ();
    subtitlespattern = configdialog->subtitlesPattern->text ();
    chapterspattern = configdialog->chaptersPattern->text ();
    trackspattern = configdialog->tracksPattern->text ();
    //postproc
    postprocessing = configdialog->postProcessing->isChecked();

    pp_default = configdialog->defaultPreset->isChecked();
    pp_fast = configdialog->fastPreset->isChecked();
    pp_custom = configdialog->customPreset->isChecked();

    pp_custom_hz = configdialog->HzDeblockFilter->isChecked();
    pp_custom_hz_aq = configdialog->HzDeblockAQuality->isChecked();
    pp_custom_hz_ch = configdialog->HzDeblockCFiltering->isChecked();

    pp_custom_vt = configdialog->VtDeblockFilter->isChecked();
    pp_custom_vt_aq = configdialog->VtDeblockAQuality->isChecked();
    pp_custom_vt_ch = configdialog->VtDeblockCFiltering->isChecked();

    pp_custom_dr = configdialog->DeringFilter->isChecked();
    pp_custom_dr_aq = configdialog->DeringAQuality->isChecked();
    pp_custom_dr_ch = configdialog->DeringCFiltering->isChecked();

    pp_custom_al = configdialog->AutolevelsFilter->isChecked();
    pp_custom_al_f = configdialog->AutolevelsFullrange->isChecked();

    pp_custom_tn = configdialog->TmpNoiseFilter->isChecked();
    pp_custom_tn_s = configdialog->TmpNoiseSlider->value();

    pp_lin_blend_int = configdialog->LinBlendDeinterlacer->isChecked();
    pp_lin_int = configdialog->LinIntDeinterlacer->isChecked();
    pp_cub_int = configdialog->CubicIntDeinterlacer->isChecked();
    pp_med_int = configdialog->MedianDeinterlacer->isChecked();
    pp_ffmpeg_int = configdialog->FfmpegDeinterlacer->isChecked();

    writeConfig ();

    emit configChanged ();

    if (urlchanged) {
        m_player->stop ();
        if (m_player->browserextension ())
            m_player->browserextension ()->urlChanged (m_player->url ().url ());
        m_player->play ();
    }
}

void KMPlayerConfig::fileOpen () {
    KFileDialog *dlg = new KFileDialog (QString::null, QString::null, configdialog, "", true);
    if (dlg->exec ())
        configdialog->url->setText (dlg->selectedURL().url ());
    delete dlg;
}

void KMPlayerConfig::getHelp () {
    KApplication::kApplication()->invokeBrowser ("man:/mplayer");
}

#include "kmplayerconfig.moc"

