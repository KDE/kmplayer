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

#include <iostream>

#include <qcheckbox.h>
#include <qtextedit.h>
#include <qpushbutton.h>
#include <qradiobutton.h>
#include <qmultilineedit.h>
#include <qtabwidget.h>
#include <qslider.h>
#include <qtable.h>
#include <kurlrequester.h>
#include <klineedit.h>

#include <kconfig.h>
#include <kapplication.h>
#include <kurl.h>
#include <kdebug.h>
#include <klocale.h>

#include "kmplayerconfig.h"
#include "kmplayer_part.h"
#include "kmplayerprocess.h"
#include "kmplayerview.h"
//#include "configdialog.h"
#include "pref.h"

static MPlayerAudioDriver _ads[] = {
    { "", i18n ("Default from MPlayer Config file") },
    { "oss", i18n ("Open Sound System") },
    { "sdl", i18n ("Simple DirectMedia Layer") },
    { "alsa", i18n ("Advanced Linux Sound Architecture") },
    { "arts", i18n ("Analog Real-time Synthesizer") },
    { "esd", i18n ("Enlightened Sound Daemon") },
    { 0, QString::null }
};

static const int ADRIVER_ARTS_INDEX = 4;

FFServerSetting::FFServerSetting (int i, const QString & n, const QString & f, const QString & ac, int abr, int asr, const QString & vc, int vbr, int q, int fr, int gs, int w, int h)
 : index (i), name (n), format (f), audiocodec (ac),
   audiobitrate (abr > 0 ? QString::number (abr) : QString ()),
   audiosamplerate (asr > 0 ? QString::number (asr) : QString ()),
   videocodec (vc),
   videobitrate (vbr > 0 ? QString::number (vbr) : QString ()),
   quality (q > 0 ? QString::number (q) : QString ()),
   framerate (fr > 0 ? QString::number (fr) : QString ()),
   gopsize (gs > 0 ? QString::number (gs) : QString ()),
   width (w > 0 ? QString::number (w) : QString ()),
   height (h > 0 ? QString::number (h) : QString ()) {}

FFServerSetting & FFServerSetting::operator = (const FFServerSetting & fs) {
    format = fs.format;
    audiocodec = fs.audiocodec;
    audiobitrate = fs.audiobitrate;
    audiosamplerate = fs.audiosamplerate;
    videocodec = fs.videocodec;
    videobitrate = fs.videobitrate;
    quality = fs.quality;
    framerate = fs.framerate;
    gopsize = fs.gopsize;
    width = fs.width;
    height = fs.height;
    return *this;
}

FFServerSetting & FFServerSetting::operator = (const QStringList & sl) {
    if (sl.count () != 11) {
        return *this;
    }
    QStringList::const_iterator it = sl.begin ();
    format = *it++;
    audiocodec = *it++;
    audiobitrate = *it++;
    audiosamplerate = *it++;
    videocodec = *it++;
    videobitrate = *it++;
    quality = *it++;
    framerate = *it++;
    gopsize = *it++;
    width = *it++;
    height = *it++;
    return *this;
}

QString & FFServerSetting::ffconfig (QString & buf) {
    QString nl ("\n");
    buf = QString ("Format ") + format + nl;
    if (!audiocodec.isEmpty ())
        buf += QString ("AudioCodec ") + audiocodec + nl;
    if (!audiobitrate.isEmpty ())
        buf += QString ("AudioBitRate ") + audiobitrate + nl;
    if (!audiosamplerate.isEmpty () > 0)
        buf += QString ("AudioSampleRate ") + audiosamplerate + nl;
    if (!videocodec.isEmpty ())
        buf += QString ("VideoCodec ") + videocodec + nl;
    if (!videobitrate.isEmpty ())
        buf += QString ("VideoBitRate ") + videobitrate + nl;
    if (!quality.isEmpty ())
        buf += QString ("VideoQMin ") + quality + nl;
    if (!framerate.isEmpty ())
        buf += QString ("VideoFrameRate ") + framerate + nl;
    if (!gopsize.isEmpty ())
        buf += QString ("VideoGopSize ") + gopsize + nl;
    if (!width.isEmpty () && !height.isEmpty ())
        buf += QString ("VideoSize ") + width + QString ("x") + height + nl;
    return buf;
}

const QStringList FFServerSetting::list () {
    QStringList sl;
    sl.push_back (format);
    sl.push_back (audiocodec);
    sl.push_back (audiobitrate);
    sl.push_back (audiosamplerate);
    sl.push_back (videocodec);
    sl.push_back (videobitrate);
    sl.push_back (quality);
    sl.push_back (framerate);
    sl.push_back (gopsize);
    sl.push_back (width);
    sl.push_back (height);
    return sl;
}

static FFServerSetting _ffs[] = {
    FFServerSetting (0, i18n ("Modem (32k)"), QString ("avi"), QString ("mp3"), 8, 11025, QString ("mpeg4"), 50, 19, 3, 3, 160, 128 ),
    FFServerSetting (1, i18n ("ISDN (64k)"), QString ("avi"), QString ("mp3"), 8, 11025, QString ("mpeg4"), 50, 16, 3, 3, 320, 240 ),
    FFServerSetting (2, i18n ("ISDN2 (128k)"), QString ("avi"), QString ("mp3"), 32, 22050, QString ("mpeg4"), 80, 10, 10, 12, 320, 240 ),
    FFServerSetting (3, i18n ("LAN (1024k)"), QString ("mpeg"), QString::null, 64, 44100, QString::null, 512, 5, 25, 12, 320, 240 ),
    FFServerSetting (4, i18n ("Custom"), QString::null, QString::null, 0, 0, QString::null, 0, 0, 0, 0, 0, 0 ),
    FFServerSetting (-1, QString::null, QString::null, QString::null, 0, 0, QString::null, 0, 0, 0, 0, 0, 0 )
};

TVChannel::TVChannel (const QString & n, int f) : name (n), frequency (f) {}

TVInput::TVInput (const QString & n, int _id) : name (n), id (_id) {
    channels.setAutoDelete (true);
}

TVDevice::TVDevice (const QString & d, const QSize & s)
    : device (d), size (s), noplayback (false) {
    inputs.setAutoDelete (true);
}

KMPlayerSettings::KMPlayerSettings (KMPlayer * player, KConfig * config)
  : configdialog (0L), m_config (config), m_player (player) {
    tvdevices.setAutoDelete (true);
    audiodrivers = _ads;
    ffserversettings = _ffs;
}

KMPlayerSettings::~KMPlayerSettings () {
    // configdialog should be destroyed when the view is destroyed
    //delete configdialog;
}

static const char * strMPlayerGroup = "MPlayer";
static const char * strGeneralGroup = "General Options";
static const char * strMPlayerPatternGroup = "MPlayer Output Matching";
static const char * strKeepSizeRatio = "Keep Size Ratio";
static const char * strContrast = "Contrast";
static const char * strBrightness = "Brightness";
static const char * strHue = "Hue";
static const char * strSaturation = "Saturation";
//static const char * strUseArts = "Use aRts";
static const char * strVoDriver = "Video Driver";
static const char * strAoDriver = "Audio Driver";
static const char * strAddArgs = "Additional Arguments";
static const char * strMencoderArgs = "Mencoder Arguments";
static const char * strSize = "Movie Size";
static const char * strCache = "Cache Fill";
static const char * strPosPattern = "Movie Position";
static const char * strIndexPattern = "Index Pattern";
static const char * strStart = "Start Playing";
static const char * strShowConsole = "Show Console Output";
static const char * strLoop = "Loop";
static const char * strFrameDrop = "Frame Drop";
static const char * strShowControlButtons = "Show Control Buttons";
static const char * strShowPositionSlider = "Show Position Slider";
static const char * strAddConfigButton = "Add Configure Button";
static const char * strAddRecordButton = "Add Record Button";
static const char * strAddBroadcastButton = "Add Broadcast Button";
static const char * strAutoHideButtons = "Auto Hide Control Buttons";
static const char * strAutoPlayAfterRecording = "Auto Play After Recording";
//static const char * strAutoHideSlider = "Auto Hide Slider";
static const char * strSeekTime = "Forward/Backward Seek Time";
static const char * strCacheSize = "Cache Size for Streaming";
static const char * strPlayDVD = "Immediately Play DVD";
//static const char * strShowDVD = "Show DVD Menu";
static const char * strDVDDevice = "DVD Device";
static const char * strLanguagePattern = "DVD Language";
static const char * strSubtitlePattern = "DVD Sub Title";
static const char * strTitlePattern = "DVD Titles";
static const char * strChapterPattern = "DVD Chapters";
static const char * strPlayVCD = "Immediately Play VCD";
//static const char * strShowVCD = "Show VCD Menu";
static const char * strVCDDevice = "VCD Device";
static const char * strTrackPattern = "VCD Tracks";
static const char * strAlwaysBuildIndex = "Always build index";
static const char * strUrlBackend = "URL Backend";
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
static const char * strTV = "TV";
static const char * strTVDevices = "Devices";
static const char * strTVDeviceName = "Name";
static const char * strTVAudioDevice = "Audio Device";
static const char * strTVInputs = "Inputs";
static const char * strTVSize = "Size";
static const char * strTVMinSize = "Minimum Size";
static const char * strTVMaxSize = "Maximum Size";
static const char * strTVNoPlayback = "No Playback";
static const char * strTVNorm = "Norm";
static const char * strTVDriver = "Driver";
// ffserver
static const char * strBroadcast = "Broadcast";
static const char * strBindAddress = "Bind Address";
static const char * strFFServerPort = "FFServer Port";
static const char * strMaxClients = "Maximum Connections";
static const char * strMaxBandwidth = "Maximum Bandwidth";
static const char * strFeedFile = "Feed File";
static const char * strFeedFileSize = "Feed File Size";
static const char * strFFServerSetting = "FFServer Setting";
static const char * strFFServerCustomSetting = "Custom Setting";
static const char * strFFServerACL = "FFServer ACL";

void KMPlayerSettings::readConfig () {
    KMPlayerView *view = static_cast <KMPlayerView *> (m_player->view ());

    m_config->setGroup (strGeneralGroup);
    contrast = m_config->readNumEntry (strContrast, 0);
    brightness = m_config->readNumEntry (strBrightness, 0);
    hue = m_config->readNumEntry (strHue, 0);
    saturation = m_config->readNumEntry (strSaturation, 0);

    m_config->setGroup (strMPlayerGroup);
    sizeratio = m_config->readBoolEntry (strKeepSizeRatio, true);
    view->setKeepSizeRatio (sizeratio);
    showconsole = m_config->readBoolEntry (strShowConsole, false);
    view->setShowConsoleOutput (showconsole);
    loop = m_config->readBoolEntry (strLoop, false);
    framedrop = m_config->readBoolEntry (strFrameDrop, true);
    showbuttons = m_config->readBoolEntry (strShowControlButtons, true);
    autohidebuttons = m_config->readBoolEntry (strAutoHideButtons, false);
    autoplayafterrecording = m_config->readBoolEntry (strAutoPlayAfterRecording, true);
    view->setAutoHideButtons (showbuttons && autohidebuttons);
    if (!showbuttons) {
        view->buttonBar ()->hide ();
    }
    showposslider = m_config->readBoolEntry(strShowPositionSlider, true);
    if (!showposslider)
        view->positionSlider ()->hide ();
    else
        view->positionSlider ()->show ();
    showcnfbutton = m_config->readBoolEntry (strAddConfigButton, true);
    if (showcnfbutton)
        view->configButton ()->show ();
    else
        view->configButton ()->hide ();
    showrecordbutton = m_config->readBoolEntry (strAddRecordButton, true);
    showbroadcastbutton = m_config->readBoolEntry (strAddBroadcastButton, true);
    if (showrecordbutton)
        view->recordButton ()->show ();
    else
        view->recordButton ()->hide ();
    seektime = m_config->readNumEntry (strSeekTime, 10);
    m_player->setSeekTime (seektime);
    alwaysbuildindex = m_config->readBoolEntry (strAlwaysBuildIndex, false);
    playdvd = m_config->readBoolEntry (strPlayDVD, true);
    dvddevice = m_config->readEntry (strDVDDevice, "/dev/dvd");
    playvcd = m_config->readBoolEntry (strPlayVCD, true);
    vcddevice = m_config->readEntry (strVCDDevice, "/dev/cdrom");
    videodriver = m_config->readNumEntry (strVoDriver, VDRIVER_XV_INDEX);
    audiodriver = m_config->readNumEntry (strAoDriver, 0);
    urlbackend = m_config->readEntry (strUrlBackend, "mplayer");
    view->setUseArts (audiodriver == ADRIVER_ARTS_INDEX);
    additionalarguments = m_config->readEntry (strAddArgs, "");
    mencoderarguments = m_config->readEntry (strMencoderArgs, "-oac copy -ovc copy");
    cachesize = m_config->readNumEntry (strCacheSize, 0);
    m_config->setGroup (strMPlayerPatternGroup);
    sizepattern = m_config->readEntry (strSize, "VO:.*[^0-9]([0-9]+)x([0-9]+)");
    cachepattern = m_config->readEntry (strCache, "Cache fill:[^0-9]*([0-9\\.]+)%");
    positionpattern = m_config->readEntry (strPosPattern, "V:\\s*([0-9\\.]+)");
    indexpattern = m_config->readEntry (strIndexPattern, "Generating Index: +([0-9]+)%");
    startpattern = m_config->readEntry (strStart, "Start[^ ]* play");
    langpattern = m_config->readEntry (strLanguagePattern, "\\[open].*audio.*language: ([A-Za-z]+).*aid.*[^0-9]([0-9]+)");
    subtitlespattern = m_config->readEntry (strSubtitlePattern, "\\[open].*subtitle.*[^0-9]([0-9]+).*language: ([A-Za-z]+)");
    titlespattern = m_config->readEntry (strTitlePattern, "There are ([0-9]+) titles");
    chapterspattern = m_config->readEntry (strChapterPattern, "There are ([0-9]+) chapters");
    trackspattern = m_config->readEntry (strTrackPattern, "track ([0-9]+):");


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

    // TV stuff
    tvdevices.clear ();
    m_config->setGroup(strTV);
    tvdriver = m_config->readEntry (strTVDriver, "v4l");
    QStrList devlist;
    int deviceentries = m_config->readListEntry (strTVDevices, devlist, ';');
    for (int i = 0; i < deviceentries; i++) {
        m_config->setGroup (devlist.at (i));
        TVDevice * device = new TVDevice (devlist.at (i),
                                          m_config->readSizeEntry (strTVSize));
        device->name = m_config->readEntry (strTVDeviceName, "/dev/video");
        device->audiodevice = m_config->readEntry (strTVAudioDevice, "");
        device->minsize = m_config->readSizeEntry (strTVMinSize);
        device->maxsize = m_config->readSizeEntry (strTVMaxSize);
        device->noplayback = m_config->readBoolEntry (strTVNoPlayback, false);
        QStrList inputlist;
        int inputentries = m_config->readListEntry (strTVInputs, inputlist,';');
        kdDebug() << device->device << " has " << inputentries << " inputs" << endl;
        for (int j = 0; j < inputentries; j++) {
            QString inputstr = inputlist.at (j);
            int pos = inputstr.find (':');
            if (pos < 0) {
                kdError () << "Wrong input: " << inputstr << endl;
                continue;
            }
            TVInput * input = new TVInput (inputstr.mid (pos + 1),
                                           inputstr.left (pos).toInt ());
            QStrList freqlist;
            int freqentries = m_config->readListEntry(input->name,freqlist,';');
            kdDebug() << input->name<< " has " << freqentries << " freqs" << endl;
            input->hastuner = (freqentries > 0);
            for (int k = 0; k < freqentries; k++) {
                QString freqstr = freqlist.at (k);
                int pos = freqstr.find (':');
                if (pos < 0) {
                    kdWarning () << "Wrong frequency or none defined: " << freqstr << endl;
                    continue;
                }
                TVChannel * channel = new TVChannel (freqstr.left (pos),
                                                  freqstr.mid (pos+1).toInt ());
                kdDebug() << freqstr.left (pos) << " at " << freqstr.mid (pos+1).toInt() << endl;
                input->channels.append (channel);
            }
            if (input->hastuner) // what if multible tuners?
                input->norm = m_config->readEntry (strTVNorm, "PAL");
            device->inputs.append (input);
        }
        tvdevices.append (device);
    }
    m_config->setGroup (strBroadcast);
    bindaddress = m_config->readEntry (strBindAddress, "0.0.0.0");
    ffserverport = m_config->readNumEntry (strFFServerPort, 8090);
    maxclients = m_config->readNumEntry (strMaxClients, 10);
    maxbandwidth = m_config->readNumEntry (strMaxBandwidth, 1000);
    feedfile = m_config->readEntry (strFeedFile, "/tmp/kmplayer.ffm");
    feedfilesize = m_config->readNumEntry (strFeedFileSize, 512);
    ffserversetting = m_config->readNumEntry (strFFServerSetting, 0);
    if (ffserversetting == 4)
        ffserversettings[4] = m_config->readListEntry (strFFServerCustomSetting, ';');
    ffserveracl = m_config->readListEntry (strFFServerACL, ';');
    if (!ffserveracl.count ()) {
        ffserveracl.push_back (QString ("127.0.0.1"));
        ffserveracl.push_back (QString ("10.0.0.0 10.255.255.255"));
        ffserveracl.push_back (QString ("192.168.0.0 192.168.255.255"));
    }
}

void KMPlayerSettings::show () {
    if (!configdialog) {
        configdialog = new KMPlayerPreferences (m_player->view (), _ads, _ffs);
        configdialog->m_SourcePageTV->scanner = new TVDeviceScannerSource (m_player);
        connect (configdialog, SIGNAL (okClicked ()),
                this, SLOT (okPressed ()));
        connect (configdialog, SIGNAL (applyClicked ()),
                this, SLOT (okPressed ()));
        if (KApplication::kApplication())
            connect (configdialog, SIGNAL (helpClicked ()),
                     this, SLOT (getHelp ()));
    }
    configdialog->m_GeneralPageGeneral->keepSizeRatio->setChecked (sizeratio);
    configdialog->m_GeneralPageGeneral->showConsoleOutput->setChecked (showconsole);
    configdialog->m_GeneralPageGeneral->loop->setChecked (loop);
    configdialog->m_GeneralPageGeneral->framedrop->setChecked (framedrop);
    configdialog->m_GeneralPageGeneral->showControlButtons->setChecked (showbuttons);
    configdialog->m_GeneralPageGeneral->showPositionSlider->setChecked (showposslider);
    configdialog->m_GeneralPageGeneral->alwaysBuildIndex->setChecked (alwaysbuildindex);
    //configdialog->m_GeneralPageGeneral->autoHideSlider->setChecked (autohideslider);
    //configdialog->addConfigButton->setChecked (showcnfbutton);	//not
    configdialog->m_GeneralPageGeneral->showRecordButton->setChecked (showrecordbutton);
    configdialog->m_GeneralPageGeneral->showBroadcastButton->setChecked (showbroadcastbutton);
    configdialog->m_GeneralPageGeneral->autoHideControlButtons->setChecked (autohidebuttons); //works
    configdialog->m_GeneralPageGeneral->seekTime->setValue(seektime);
    configdialog->m_SourcePageURL->url->setText (m_player->url ().url ());
    configdialog->m_GeneralPageDVD->autoPlayDVD->setChecked (playdvd); //works if autoplay?
    configdialog->m_GeneralPageDVD->dvdDevicePath->lineEdit()->setText (dvddevice);
    configdialog->m_GeneralPageVCD->autoPlayVCD->setChecked (playvcd);
    configdialog->m_GeneralPageVCD->vcdDevicePath->lineEdit()->setText (vcddevice);
    configdialog->m_SourcePageTV->driver->setText (tvdriver);
    configdialog->m_SourcePageTV->setTVDevices (&tvdevices);

    configdialog->m_GeneralPageOutput->videoDriver->setCurrentItem (videodriver);
    configdialog->m_GeneralPageOutput->audioDriver->setCurrentItem (audiodriver);
    configdialog->m_SourcePageURL->backend->setCurrentText (urlbackend);


    if (cachesize > 0)
        configdialog->m_GeneralPageAdvanced->cacheSize->setValue(cachesize);
    configdialog->m_GeneralPageAdvanced->additionalArguments->setText (additionalarguments);
    configdialog->m_GeneralPageAdvanced->sizePattern->setText (sizepattern);
    configdialog->m_GeneralPageAdvanced->cachePattern->setText (cachepattern);
    configdialog->m_GeneralPageAdvanced->startPattern->setText (startpattern);
    configdialog->m_GeneralPageAdvanced->indexPattern->setText (indexpattern);
    configdialog->m_GeneralPageAdvanced->dvdLangPattern->setText (langpattern);
    configdialog->m_GeneralPageAdvanced->dvdSubPattern->setText (subtitlespattern);
    configdialog->m_GeneralPageAdvanced->dvdTitlePattern->setText (titlespattern);
    configdialog->m_GeneralPageAdvanced->dvdChapPattern->setText (chapterspattern);
    configdialog->m_GeneralPageAdvanced->vcdTrackPattern->setText (trackspattern);

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

    configdialog->m_BroadcastPage->bindaddress->setText (bindaddress);
    configdialog->m_BroadcastPage->port->setText (QString::number (ffserverport));
    configdialog->m_BroadcastPage->maxclients->setText (QString::number (maxclients));
    configdialog->m_BroadcastPage->maxbandwidth->setText (QString::number (maxbandwidth));
    configdialog->m_BroadcastPage->feedfile->setText (feedfile);
    configdialog->m_BroadcastPage->feedfilesize->setText (QString::number (feedfilesize));
    configdialog->m_BroadcastPage->optimize->setCurrentItem (ffserversetting);
    configdialog->m_BroadcastPage->custom = ffserversettings[4];
    configdialog->m_BroadcastPage->format->insertItem (QString (""));
    configdialog->m_BroadcastPage->format->setCurrentText (QString (""));
    configdialog->m_BroadcastPage->slotIndexChanged (ffserversetting);
    QTable *accesslist = configdialog->m_BroadcastACLPage->accesslist;
    accesslist->setNumRows (0);
    accesslist->setNumRows (50);
    QStringList::iterator it = ffserveracl.begin ();
    for (int i = 0; it != ffserveracl.end (); ++i, ++it)
        accesslist->setItem (i, 0, new QTableItem (accesslist, QTableItem::Always, *it));
    configdialog->show ();
}

void KMPlayerSettings::writeConfig () {
    KMPlayerView *view = static_cast <KMPlayerView *> (m_player->view ());

    m_config->setGroup (strGeneralGroup);
    m_config->writeEntry (strContrast, contrast);
    m_config->writeEntry (strBrightness, brightness);
    m_config->writeEntry (strHue, hue);
    m_config->writeEntry (strSaturation, saturation);
    m_config->setGroup (strMPlayerGroup);
    m_config->writeEntry (strKeepSizeRatio, view->keepSizeRatio ());
    m_config->writeEntry (strShowConsole, view->showConsoleOutput());
    m_config->writeEntry (strLoop, loop);
    m_config->writeEntry (strFrameDrop, framedrop);
    m_config->writeEntry (strSeekTime, m_player->seekTime ());
    m_config->writeEntry (strVoDriver, videodriver);
    m_config->writeEntry (strAoDriver, audiodriver);
    m_config->writeEntry (strUrlBackend, urlbackend);
    m_config->writeEntry (strAddArgs, additionalarguments);
    m_config->writeEntry (strCacheSize, cachesize);
    m_config->writeEntry (strShowControlButtons, showbuttons);
    m_config->writeEntry (strShowPositionSlider, showposslider);
    m_config->writeEntry (strAlwaysBuildIndex, alwaysbuildindex);
    m_config->writeEntry (strAddConfigButton, showcnfbutton);
    m_config->writeEntry (strAddRecordButton, showrecordbutton);
    m_config->writeEntry (strAddBroadcastButton, showbroadcastbutton);
    m_config->writeEntry (strAutoHideButtons, autohidebuttons);
    m_config->writeEntry (strPlayDVD, playdvd);

    m_config->writeEntry (strDVDDevice, dvddevice);
    m_config->writeEntry (strPlayVCD, playvcd);

    m_config->writeEntry (strVCDDevice, vcddevice);
    m_config->setGroup (strMPlayerPatternGroup);
    m_config->writeEntry (strSize, sizepattern);
    m_config->writeEntry (strCache, cachepattern);
    m_config->writeEntry (strIndexPattern, indexpattern);
    m_config->writeEntry (strStart, startpattern);
    m_config->writeEntry (strLanguagePattern, langpattern);
    m_config->writeEntry (strSubtitlePattern, subtitlespattern);
    m_config->writeEntry (strTitlePattern, titlespattern);
    m_config->writeEntry (strChapterPattern, chapterspattern);
    m_config->writeEntry (strTrackPattern, trackspattern);
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

    //TV stuff
    m_config->setGroup (strTV);
    QStringList devicelist = m_config->readListEntry (strTVDevices, ';');
    for (unsigned i = 0; i < devicelist.size (); i++)
        m_config->deleteGroup (*devicelist.at (i));
    devicelist.clear ();
    if (configdialog)
        configdialog->m_SourcePageTV->updateTVDevices ();
    TVDevice * device;
    QString sep = QString (":");
    for (tvdevices.first(); (device = tvdevices.current ()); tvdevices.next()) {
        kdDebug() << " writing " << device->device << endl;
        devicelist.append (device->device);
        m_config->setGroup (device->device);
        m_config->writeEntry (strTVSize, device->size);
        m_config->writeEntry (strTVMinSize, device->minsize);
        m_config->writeEntry (strTVMaxSize, device->maxsize);
        m_config->writeEntry (strTVNoPlayback, device->noplayback);
        m_config->writeEntry (strTVDeviceName, device->name);
        m_config->writeEntry (strTVAudioDevice, device->audiodevice);
        QStringList inputlist;
        TVInput * input;
        for (device->inputs.first (); (input = device->inputs.current ()); device->inputs.next ()) {
            inputlist.append (QString::number (input->id) + sep + input->name);
            if (input->hastuner) {
                TVChannel * channel;
                QStringList channellist;
                for (input->channels.first (); (channel = input->channels.current()); input->channels.next ()) {
                    channellist.append (channel->name + sep + QString::number (channel->frequency));
                }
                if (!channellist.size ())
                    channellist.append (QString ("none"));
                m_config->writeEntry (input->name, channellist, ';');
                m_config->writeEntry (strTVNorm, input->norm);
            }
        }
        m_config->writeEntry (strTVInputs, inputlist, ';');
    }
    m_config->setGroup (strTV);
    m_config->writeEntry (strTVDevices, devicelist, ';');
    m_config->writeEntry (strTVDriver, tvdriver);
    // end TV stuff
    m_config->setGroup (strBroadcast);
    m_config->writeEntry (strBindAddress, bindaddress);
    m_config->writeEntry (strFFServerPort, ffserverport);
    m_config->writeEntry (strMaxClients, maxclients);
    m_config->writeEntry (strMaxBandwidth, maxbandwidth);
    m_config->writeEntry (strFeedFile, feedfile);
    m_config->writeEntry (strFeedFileSize, feedfilesize);
    m_config->writeEntry (strFFServerSetting, ffserversetting);
    if (ffserversetting == 4)
        m_config->writeEntry (strFFServerCustomSetting, ffserversettings[4].list (), ';');
    m_config->writeEntry (strFFServerACL, ffserveracl, ';');
    m_config->sync ();
}

void KMPlayerSettings::okPressed () {
    KMPlayerView *view = static_cast <KMPlayerView *> (m_player->view ());
    if (!view)
        return;
    bool urlchanged = m_player->url () != KURL (configdialog->m_SourcePageURL->url->text ());
    if (m_player->url ().isEmpty () && configdialog->m_SourcePageURL->url->text ().isEmpty ())
        urlchanged = false; // hmmm aren't these URLs the same?

    if (urlchanged)
        m_player->setURL (configdialog->m_SourcePageURL->url->text ());

    sizeratio = configdialog->m_GeneralPageGeneral->keepSizeRatio->isChecked ();
    m_player->keepMovieAspect (sizeratio);
    showconsole = configdialog->m_GeneralPageGeneral->showConsoleOutput->isChecked ();
    view->setShowConsoleOutput (showconsole);
    alwaysbuildindex = configdialog->m_GeneralPageGeneral->alwaysBuildIndex->isChecked();
    loop = configdialog->m_GeneralPageGeneral->loop->isChecked ();
    framedrop = configdialog->m_GeneralPageGeneral->framedrop->isChecked ();
    if (showconsole && !m_player->playing ())
        view->consoleOutput ()->show ();
    else
        view->consoleOutput ()->hide ();
    showbuttons = configdialog->m_GeneralPageGeneral->showControlButtons->isChecked ();
    autohidebuttons = configdialog->m_GeneralPageGeneral->autoHideControlButtons->isChecked ();
    view->setAutoHideButtons (showbuttons && autohidebuttons);
    if (!showbuttons)
        view->buttonBar ()->hide ();
    showposslider = configdialog->m_GeneralPageGeneral->showPositionSlider->isChecked ();
    if (showposslider && m_player->process ()->source ()->hasLength ())
        view->positionSlider ()->show ();
    else
        view->positionSlider ()->hide ();
    //showcnfbutton = configdialog->m_GeneralPageGeneral->addConfigButton->isChecked ();
    showcnfbutton = true;
    if (showcnfbutton)
	view->configButton ()->show ();
    else
        view->configButton ()->hide ();
    showrecordbutton = configdialog->m_GeneralPageGeneral->showRecordButton->isChecked ();
    if (showrecordbutton)
	view->recordButton ()->show ();
    else
        view->recordButton ()->hide ();
    showbroadcastbutton = configdialog->m_GeneralPageGeneral->showBroadcastButton->isChecked ();
    if (!showbroadcastbutton)
        view->broadcastButton ()->hide ();
    playdvd = configdialog->m_GeneralPageDVD->autoPlayDVD->isChecked ();
    dvddevice = configdialog->m_GeneralPageDVD->dvdDevicePath->lineEdit()->text ();
    playvcd = configdialog->m_GeneralPageVCD->autoPlayVCD->isChecked ();
    vcddevice = configdialog->m_GeneralPageVCD->vcdDevicePath->lineEdit()->text ();
    seektime = configdialog->m_GeneralPageGeneral->seekTime->value();
    m_player->setSeekTime (seektime);

    additionalarguments = configdialog->m_GeneralPageAdvanced->additionalArguments->text();
    cachesize = configdialog->m_GeneralPageAdvanced->cacheSize->value();
    sizepattern = configdialog->m_GeneralPageAdvanced->sizePattern->text ();
    cachepattern = configdialog->m_GeneralPageAdvanced->cachePattern->text ();
    startpattern = configdialog->m_GeneralPageAdvanced->startPattern->text ();
    indexpattern = configdialog->m_GeneralPageAdvanced->indexPattern->text ();
    langpattern = configdialog->m_GeneralPageAdvanced->dvdLangPattern->text ();
    titlespattern = configdialog->m_GeneralPageAdvanced->dvdTitlePattern->text ();
    subtitlespattern = configdialog->m_GeneralPageAdvanced->dvdSubPattern->text ();
    chapterspattern = configdialog->m_GeneralPageAdvanced->dvdChapPattern->text ();
    trackspattern = configdialog->m_GeneralPageAdvanced->vcdTrackPattern->text ();

    videodriver = configdialog->m_GeneralPageOutput->videoDriver->currentItem();
    audiodriver = configdialog->m_GeneralPageOutput->audioDriver->currentItem();
    urlbackend = configdialog->m_SourcePageURL->backend->currentText ();
    view->setUseArts(audiodriver == ADRIVER_ARTS_INDEX);
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
    bindaddress = configdialog->m_BroadcastPage->bindaddress->text ();
    ffserverport = configdialog->m_BroadcastPage->port->text ().toInt ();
    maxclients = configdialog->m_BroadcastPage->maxclients->text ().toInt ();
    maxbandwidth = configdialog->m_BroadcastPage->maxbandwidth->text ().toInt();
    feedfile = configdialog->m_BroadcastPage->feedfile->text ();
    feedfilesize = configdialog->m_BroadcastPage->feedfilesize->text ().toInt();
    ffserversetting = configdialog->m_BroadcastPage->optimize->currentItem ();
    configdialog->m_BroadcastPage->slotIndexChanged (ffserversetting);
    if (ffserversetting == 4)
        ffserversettings[4] = configdialog->m_BroadcastPage->custom;
    ffserveracl.clear ();
    QTable *accesslist = configdialog->m_BroadcastACLPage->accesslist;
    for (int i = 0; i < accesslist->numRows (); ++i) {
        if (accesslist->item (i, 0) && !accesslist->item (i, 0)->text ().isEmpty ())
            ffserveracl.push_back (accesslist->item (i, 0)->text ());
    }
    writeConfig ();

    emit configChanged ();

    if (urlchanged)
        m_player->openURL (KURL (configdialog->m_SourcePageURL->url->text ()));
}

void KMPlayerSettings::getHelp () {
    KApplication::kApplication()->invokeBrowser ("man:/mplayer");
}

//-----------------------------------------------------------------------------
TVDeviceScannerSource::TVDeviceScannerSource (KMPlayer * player)
    : KMPlayerSource (player), m_tvdevice (0) {}

void TVDeviceScannerSource::init () {
    ;
}

bool TVDeviceScannerSource::processOutput (const QString & line) {
    if (m_nameRegExp.search (line) > -1) {
        m_tvdevice->name = m_nameRegExp.cap (1);
        kdDebug() << "Name " << m_tvdevice->name << endl;
    } else if (m_sizesRegExp.search (line) > -1) {
        m_tvdevice->minsize = QSize (m_sizesRegExp.cap (1).toInt (),
                                     m_sizesRegExp.cap (2).toInt ());
        m_tvdevice->maxsize = QSize (m_sizesRegExp.cap (3).toInt (),
                                     m_sizesRegExp.cap (4).toInt ());
        kdDebug() << "MinSize (" << m_tvdevice->minsize.width () << "," << m_tvdevice->minsize.height () << ")" << endl;
    } else if (m_inputRegExp.search (line) > -1) {
        TVInput * input = new TVInput (m_inputRegExp.cap (2).stripWhiteSpace (),
                                       m_inputRegExp.cap (1).toInt ());
        input->hastuner = m_inputRegExp.cap (3).toInt () == 1;
        m_tvdevice->inputs.append (input);
        kdDebug() << "Input " << input->id << ": " << input->name << endl;
    } else
        return false;
    return true;
}

QString TVDeviceScannerSource::filterOptions () {
    return QString ("");
}

bool TVDeviceScannerSource::hasLength () {
    return false;
}

bool TVDeviceScannerSource::isSeekable () {
    return false;
}

bool TVDeviceScannerSource::scan (const QString & dev, const QString & dri) {
    if (m_tvdevice)
        return false;
    m_tvdevice = new TVDevice (dev, QSize ());
    m_driver = dri;
    m_source = m_player->process ()->source ();
    m_player->setSource (this);
    play ();
    return !!m_tvdevice;
}

void TVDeviceScannerSource::activate () {
    m_nameRegExp.setPattern ("Selected device:\\s*([^\\s].*)");
    m_sizesRegExp.setPattern ("Supported sizes:\\s*([0-9]+)x([0-9]+) => ([0-9]+)x([0-9]+)");
    m_inputRegExp.setPattern ("\\s*([0-9]+):\\s*([^:]+):[^\\(]*\\(tuner:([01]),\\s*norm:([^\\)]+)\\)");
}

void TVDeviceScannerSource::deactivate () {
    disconnect (m_player, SIGNAL (finished ()), this, SLOT (finished ()));
    if (m_tvdevice) {
        delete m_tvdevice;
        m_tvdevice = 0L;
        emit scanFinished (m_tvdevice);
    }
}

void TVDeviceScannerSource::play () {
    if (!m_tvdevice)
        return;
    QString args;
    args.sprintf ("-tv on:driver=%s:device=%s -identify -frames 0", m_driver.ascii (), m_tvdevice->device.ascii ());
    if (m_player->mplayer ()->run (args.ascii()))
        connect (m_player, SIGNAL (finished ()), this, SLOT (finished ()));
    else
        deactivate ();
}

void TVDeviceScannerSource::finished () {
    TVDevice * dev = 0L;
    if (!m_tvdevice->inputs.count ())
        delete m_tvdevice;
    else
        dev = m_tvdevice;
    m_tvdevice = 0L;
    m_player->setSource (m_source);
    emit scanFinished (dev);
}

#include "kmplayerconfig.moc"

