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
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>

#include <iostream>

#ifdef KDE_USE_FINAL
#undef Always
#include <qdir.h>
#endif
#include <qapplication.h>
#include <qcstring.h>
#include <qregexp.h>
#include <qcursor.h>
#include <qtimer.h>
#include <qmultilineedit.h>
#include <qpair.h>
#include <qpushbutton.h>
#include <qpopupmenu.h>
#include <qslider.h>
#include <qvaluelist.h>

#include <klibloader.h>
#include <kmessagebox.h>
#include <kaboutdata.h>
#include <kdebug.h>
#include <kconfig.h>
#include <kaction.h>
#include <kstandarddirs.h>
#include <kprocess.h>

#include "kmplayer_part.h"
#include "kmplayer_koffice_part.h"
#include "kmplayerview.h"
#include "kmplayerconfig.h"
#include "kmplayerprocess.h"


K_EXPORT_COMPONENT_FACTORY (kparts_kmplayer, KMPlayerFactory);

KInstance *KMPlayerFactory::s_instance = 0;

KMPlayerFactory::KMPlayerFactory () {
    s_instance = new KInstance ("KMPlayer");
}

KMPlayerFactory::~KMPlayerFactory () {
    delete s_instance;
}

KParts::Part *KMPlayerFactory::createPartObject
  (QWidget *wparent, const char *wname,
   QObject *parent, const char * name,
   const char * cls, const QStringList & args) {
      kdDebug() << "KMPlayerFactory::createPartObject " << cls << endl;
#ifdef HAVE_KOFFICE
    if (strstr (cls, "KoDocument"))
        return new KOfficeMPlayer (wparent, wname, parent, name);
    else
#endif //HAVE_KOFFICE
        return new KMPlayer (wparent, wname, parent, name, args);
}

//-----------------------------------------------------------------------------

KMPlayer::KMPlayer (QWidget * parent, KConfig * config)
 : KMediaPlayer::Player (parent, ""),
   m_config (config),
   m_view (new KMPlayerView (parent)),
   m_settings (new KMPlayerSettings (this, config)),
   m_process (0L),
   m_mplayer (new MPlayer (this)),
   m_mencoder (new MEncoder (this)),
   m_xine (new Xine (this)),
   m_urlsource (new KMPlayerURLSource (this)),
   m_hrefsource (new KMPlayerHRefSource (this)),
   m_liveconnectextension (0L),
   movie_width (0),
   movie_height (0),
   m_autoplay (true),
   m_ispart (false),
   m_havehref (false) {
    m_view->init ();
    init();
}

KMPlayer::KMPlayer (QWidget * wparent, const char *wname,
                    QObject * parent, const char *name, const QStringList &args)
 : KMediaPlayer::Player (wparent, wname, parent, name),
   m_config (new KConfig ("kmplayerrc")),
   m_view (new KMPlayerView (wparent, wname)),
   m_settings (new KMPlayerSettings (this, m_config)),
   m_process (0L),
   m_mplayer (new MPlayer (this)),
   m_mencoder (new MEncoder (this)),
   m_xine (new Xine (this)),
   m_urlsource (new KMPlayerURLSource (this)),
   m_hrefsource (new KMPlayerHRefSource (this)),
   m_liveconnectextension (new KMPlayerLiveConnectExtension (this)),
   movie_width (0),
   movie_height (0),
   m_autoplay (true),
   m_ispart (true),
   m_havehref (false) {
    printf("MPlayer::KMPlayer ()\n");
    setInstance (KMPlayerFactory::instance ());
    /*KAction *playact =*/ new KAction(i18n("P&lay"), 0, 0, this, SLOT(play ()), actionCollection (), "view_play");
    /*KAction *pauseact =*/ new KAction(i18n("&Pause"), 0, 0, this, SLOT(pause ()), actionCollection (), "view_pause");
    /*KAction *stopact =*/ new KAction(i18n("&Stop"), 0, 0, this, SLOT(stop ()), actionCollection (), "view_stop");
    QStringList::const_iterator it = args.begin ();
    for ( ; it != args.end (); ++it) {
        int equalPos = (*it).find("=");
        if (equalPos > 0) {
            QString name = (*it).left (equalPos).upper ();
            QString value = (*it).right ((*it).length () - equalPos - 1);
            if (value.at(0)=='\"')
                value = value.right (value.length () - 1);
            if (value.at (value.length () - 1) == '\"')
                value.truncate (value.length () - 1);
            kdDebug () << "name=" << name << " value=" << value << endl;
            if (name.lower () == "href") {
                m_urlsource->setURL (KURL (value));
                m_havehref = true;
            } else if (name.lower()==QString::fromLatin1("width"))
                movie_width = value.toInt();
            else if (name.lower()==QString::fromLatin1("height"))
                movie_height = value.toInt();
            else if (name.lower()==QString::fromLatin1("autostart"))
                m_autoplay = !(value.lower() == QString::fromLatin1("false") ||
                               value.lower() == QString::fromLatin1("0"));
        }
    }
    m_view->init ();
    m_view->zoomMenu ()->connectItem (KMPlayerView::menu_zoom50,
                                      this, SLOT (setMenuZoom (int)));
    m_view->zoomMenu ()->connectItem (KMPlayerView::menu_zoom100,
                                      this, SLOT (setMenuZoom (int)));
    m_view->zoomMenu ()->connectItem (KMPlayerView::menu_zoom150,
                                      this, SLOT (setMenuZoom (int)));
    KParts::Part::setWidget (m_view);
    setXMLFile("kmplayerpartui.rc");
    init ();
}

void KMPlayer::showConfigDialog () {
    m_settings->show ();
}

void KMPlayer::init () {
    m_settings->readConfig ();
    setProcess (m_mplayer);
    m_started_emited = false;
    m_browserextension = new KMPlayerBrowserExtension (this);
    m_movie_position = 0;
    m_bPosSliderPressed = false;
    m_view->contrastSlider ()->setValue (m_settings->contrast);
    m_view->brightnessSlider ()->setValue (m_settings->brightness);
    m_view->hueSlider ()->setValue (m_settings->hue);
    m_view->saturationSlider ()->setValue (m_settings->saturation);
    connect (m_view->backButton (), SIGNAL (clicked ()), this, SLOT (back ()));
    connect (m_view->playButton (), SIGNAL (clicked ()), this, SLOT (play ()));
    connect (m_view->forwardButton (), SIGNAL (clicked ()), this, SLOT (forward ()));
    connect (m_view->pauseButton (), SIGNAL (clicked ()), this, SLOT (pause ()));
    connect (m_view->stopButton (), SIGNAL (clicked ()), this, SLOT (stop ()));
    connect (m_view->recordButton(), SIGNAL (clicked()), this, SLOT (record()));
    connect (m_view->positionSlider (), SIGNAL (valueChanged (int)), this, SLOT (positonValueChanged (int)));
    connect (m_view->positionSlider (), SIGNAL (sliderPressed()), this, SLOT (posSliderPressed()));
    connect (m_view->positionSlider (), SIGNAL (sliderReleased()), this, SLOT (posSliderReleased()));
    connect (m_view->contrastSlider (), SIGNAL (valueChanged(int)), this, SLOT (contrastValueChanged(int)));
    connect (m_view->brightnessSlider (), SIGNAL (valueChanged(int)), this, SLOT (brightnessValueChanged(int)));
    connect (m_view->hueSlider (), SIGNAL (valueChanged(int)), this, SLOT (hueValueChanged(int)));
    connect (m_view->saturationSlider (), SIGNAL (valueChanged(int)), this, SLOT (saturationValueChanged(int)));
    connect (m_view, SIGNAL (urlDropped (const KURL &)), this, SLOT (openURL (const KURL &)));
    m_view->popupMenu ()->connectItem (KMPlayerView::menu_config,
                                       m_settings, SLOT (show ()));
    connect (m_mencoder, SIGNAL (finished()), this, SLOT (recordingFinished()));
    setMovieLength (0);
    //connect (m_view->configButton (), SIGNAL (clicked ()), m_settings, SLOT (show ()));
}

KMPlayer::~KMPlayer () {
    kdDebug() << "KMPlayer::~KMPlayer" << endl;
    if (!m_ispart)
        delete (KMPlayerView*) m_view;
    m_view = (KMPlayerView*) 0;
    stop ();
    delete m_settings;
    if (m_ispart)
        delete m_config;
}

KMediaPlayer::View* KMPlayer::view () {
    return m_view;
}

void KMPlayer::setProcess (KMPlayerProcess * process) {
    if (m_process == process)
        return;
    KMPlayerSource * source = m_urlsource;
    if (m_process) {
        m_process->stop ();
        source = m_process->source ();
        disconnect (m_process, SIGNAL (started ()),
                    this, SLOT (processStarted ()));
        disconnect (m_process, SIGNAL (finished ()),
                    this, SLOT (processFinished ()));
        disconnect (m_process, SIGNAL (positionChanged (int)),
                    this, SLOT (processPosition (int)));
        disconnect (m_process, SIGNAL (loading (int)),
                    this, SLOT (processLoading (int)));
        disconnect (m_process, SIGNAL (startPlaying ()),
                    this, SLOT (processPlaying ()));
        disconnect (m_process, SIGNAL (output (const QString &)),
                    this, SLOT (processOutput (const QString &)));
    }
    m_process = process;
    m_process->setSource (source);
    connect (m_process, SIGNAL (started ()), this, SLOT (processStarted ()));
    connect (m_process, SIGNAL (finished ()), this, SLOT (processFinished ()));
    connect (m_process, SIGNAL (positionChanged (int)),
             this, SLOT (processPosition (int)));
    connect (m_process, SIGNAL (loading (int)),
             this, SLOT (processLoading (int)));
    connect (m_process, SIGNAL (startPlaying ()),
             this, SLOT (processPlaying ()));
    connect (m_process, SIGNAL (output (const QString &)),
             this, SLOT (processOutput (const QString &)));
}

extern const char * strUrlBackend;

void KMPlayer::setXine () {
    bool playing = m_process->playing ();
    if (playing)
        m_process->source ()->deactivate ();
    m_settings->urlbackend = QString ("Xine");
    m_config->writeEntry (strUrlBackend, m_settings->urlbackend);
    m_config->sync ();
    setProcess (m_xine);
    if (playing)
        m_process->source ()->activate ();
}

void KMPlayer::setMPlayer () {
    bool playing = m_process->playing ();
    if (playing)
        m_process->source ()->deactivate ();
    m_settings->urlbackend = QString ("MPlayer");
    m_config->writeEntry (strUrlBackend, m_settings->urlbackend);
    m_config->sync ();
    setProcess (m_mplayer);
    if (playing)
        m_process->source ()->activate ();
}

void KMPlayer::setSource (KMPlayerSource * source, bool keepsizes) {
    KMPlayerSource * oldsource = m_process->source ();
    int w = movie_width;
    int h = movie_height;
    if (oldsource) {
        oldsource->deactivate ();
        closeURL ();
    }
    m_process->setSource (source);
    if (!oldsource)
        setMovieLength (0);
    if (source->hasLength () && m_settings->showposslider)
        m_view->positionSlider()->show ();
    else
        m_view->positionSlider()->hide ();
    if (source->isSeekable ()) {
        m_view->forwardButton ()->show ();
        m_view->backButton ()->show ();
    } else {
        m_view->forwardButton ()->hide ();
        m_view->backButton ()->hide ();
    }
    if (keepsizes) {
        movie_width = w;
        movie_height = h;
    }
    if (source) QTimer::singleShot (0, source, SLOT (activate ()));
}

bool KMPlayer::isSeekable (void) const {
    return m_process->source ()->isSeekable ();
}

bool KMPlayer::hasLength () const {
    return m_process->source ()->hasLength (); 
}

unsigned long KMPlayer::length () const {
    return m_process->source ()->length ();
}
            
bool KMPlayer::openURL (const KURL & url) {
    kdDebug () << "KMPlayer::openURL " << url.url() << endl;
    if (!m_view || !url.isValid ()) return false;
    if (m_havehref) {
        m_havehref = false;
        m_hrefsource->setURL (url);
        setSource (m_hrefsource);
    } else {
        m_urlsource->setURL (url);
        m_hrefsource->setURL (KURL ());
        setSource (m_urlsource);
        //play ();
    }
    return true;
}

bool KMPlayer::closeURL () {
    stop ();
    movie_height = movie_width = 0;
    if (!m_view) return false;
    setMovieLength (0);
    m_view->viewer ()->setAspect (0.0);
    m_view->reset ();
    return true;
}

bool KMPlayer::openFile () {
    return false;
}

void KMPlayer::keepMovieAspect (bool b) {
    m_view->setKeepSizeRatio (b);
    if (b) {
        if (m_view->viewer ()->aspect () < 0.01 && movie_height > 0)
            m_view->viewer ()->setAspect (1.0 * movie_width / movie_height);
    } else
        m_view->viewer ()->setAspect (0.0);
}

void KMPlayer::recordingFinished () {
    if (m_view && m_view->recordButton ()->isOn ()) 
        m_view->recordButton ()->toggle ();
    if (m_settings->autoplayafterrecording)
        openURL (m_mencoder->recordURL ());
}

void KMPlayer::processFinished () {
    kdDebug () << "process finished" << endl;
    if (m_movie_position > m_process->source ()->length ())
        setMovieLength (m_movie_position);
    m_movie_position = 0;
    if (m_started_emited) {
        m_started_emited = false;
        m_browserextension->setLoadingProgress (100);
        emit completed ();
    } else {
        emit canceled (i18n ("Could not start MPlayer"));
    }
    if (m_view && m_view->playButton ()->isOn ()) {
        m_view->playButton ()->toggle ();
        m_view->positionSlider()->setEnabled (false);
        m_view->positionSlider()->setValue (0);
    }
    if (m_view) {
        m_view->reset ();
        if (m_browserextension)
            m_browserextension->infoMessage (i18n ("KMPlayer: Stop Playing"));
        emit finished ();
    }
}

void KMPlayer::processStarted () {
    kdDebug () << "process started" << endl;
    if (m_settings->showposslider && m_process->source ()->hasLength ())
        m_view->positionSlider()->show();
    else
        m_view->positionSlider()->hide();
    if (!m_view->playButton ()->isOn ()) m_view->playButton ()->toggle ();
    emit started (0L);
    m_started_emited = true;
    m_view->positionSlider()->setEnabled (true);
}

void KMPlayer::processPosition (int pos) {
    m_movie_position = pos;
    QSlider *slider = m_view->positionSlider ();
    if (m_process->source ()->length () <= 0 && pos > 7 * slider->maxValue ()/8)
        slider->setMaxValue (slider->maxValue() * 2);
    else if (slider->maxValue() < pos)
        slider->setMaxValue (int (1.4 * slider->maxValue()));
    if (!m_bPosSliderPressed)
        slider->setValue (pos);
}

void KMPlayer::processLoading (int percentage) {
    if (m_browserextension) {
        m_browserextension->setLoadingProgress (percentage);
        m_browserextension->infoMessage 
            (QString::number (percentage) + i18n ("% Cache fill"));
    }
}

void KMPlayer::processPlaying () {
    if (movie_width <= 0) {
        movie_width = m_process->source ()->width ();
        movie_height = m_process->source ()->height ();
    }
    kdDebug () << "KMPlayer::processPlaying " << movie_width << "," << movie_height << endl;
    m_view->viewer ()->setAspect (1.0 * movie_width / movie_height);
    if (m_liveconnectextension)
        m_liveconnectextension->setSize (movie_width, movie_height);
    if (m_browserextension) {
        m_browserextension->setLoadingProgress (100);
        emit completed ();
        m_started_emited = false;
        m_browserextension->infoMessage (i18n("KMPlayer: Playing"));
    }
}

void KMPlayer::processOutput (const QString & msg) {
    if (m_view)
        m_view->addText (msg + QString ("\n"));
}

void KMPlayer::setMovieLength (int len) {
    if (!m_process->source ())
        return;
    m_process->source ()->setLength (len);
    if (m_view)
        m_view->positionSlider()->setMaxValue (len > 0 ? m_process->source ()->length () + 9 : 300);
}

void KMPlayer::pause () {
    m_process->pause ();
}

void KMPlayer::back () {
    m_process->seek (-1 * m_seektime, false);
}

void KMPlayer::forward () {
    m_process->seek (m_seektime, false);
}

void KMPlayer::record () {
    if (m_mencoder->playing ()) {
        m_mencoder->stop ();
        return;
    }
    m_process->stop ();
    m_mencoder->setSource (m_process->source ());
    if (m_mencoder->play ()) {
        if (!m_view->recordButton ()->isOn ()) 
            m_view->recordButton ()->toggle ();
    } else if (m_view->recordButton ()->isOn ()) 
        m_view->recordButton ()->toggle ();
}

void KMPlayer::play () {
    m_process->play ();
    if (m_process->playing () && !m_view->playButton ()->isOn ())
        m_view->playButton ()->toggle ();
    else if (!m_process->playing () && m_view->playButton ()->isOn ())
        m_view->playButton ()->toggle ();
}

bool KMPlayer::playing () const {
    return m_process && m_process->playing ();
}

void KMPlayer::stop () {
    if (m_view && !m_view->stopButton ()->isOn ())
        m_view->stopButton ()->toggle ();
    if (m_view) m_view->setCursor (QCursor (Qt::WaitCursor));
    m_process->stop ();
    if (m_view) m_view->setCursor (QCursor (Qt::ArrowCursor));
    if (m_view && m_view->stopButton ()->isOn ())
        m_view->stopButton ()->toggle ();
}

void KMPlayer::seek (unsigned long msec) {
    m_process->seek (msec/1000, true);
}

void KMPlayer::adjustVolume (int incdec) {
    m_process->volume (incdec, false);
}

void KMPlayer::sizes (int & w, int & h) const {
    w = movie_width == 0 ? m_view->viewer ()->width () : movie_width;
    h = movie_height == 0 ? m_view->viewer ()->height () : movie_height;
}

void KMPlayer::setMenuZoom (int id) {
    sizes (movie_width, movie_height);
    if (id == KMPlayerView::menu_zoom100) {
        m_liveconnectextension->setSize (movie_width, movie_height);
        return;
    }
    float scale = 1.5;
    if (id == KMPlayerView::menu_zoom50)
        scale = 0.5;
    m_liveconnectextension->setSize (int (scale * m_view->viewer ()->width ()),
                                     int (scale * m_view->viewer ()->height()));
}

void KMPlayer::posSliderPressed () {
    m_bPosSliderPressed=true;
}

void KMPlayer::posSliderReleased () {
    m_bPosSliderPressed=false;
    seek (m_view->positionSlider()->value());
}

void KMPlayer::contrastValueChanged (int val) {
    m_settings->contrast = val;
    m_process->contrast (val, true);
}

void KMPlayer::brightnessValueChanged (int val) {
    m_settings->brightness = val;
    m_process->brightness (val, true);
}

void KMPlayer::hueValueChanged (int val) {
    m_settings->hue = val;
    m_process->hue (val, true);
}

void KMPlayer::saturationValueChanged (int val) {
    m_settings->saturation = val;
    m_process->saturation (val, true);
}

void KMPlayer::positonValueChanged (int /*pos*/) {
    if (!m_bPosSliderPressed)
        return;
}

KAboutData* KMPlayer::createAboutData () {
    KMessageBox::error(0L, "createAboutData", "KMPlayer");
    return 0;
}

//---------------------------------------------------------------------

KMPlayerBrowserExtension::KMPlayerBrowserExtension (KMPlayer * parent)
  : KParts::BrowserExtension (parent, "KMPlayer Browser Extension") {
}

void KMPlayerBrowserExtension::urlChanged (const QString & url) {
    emit setLocationBarURL (url);
}

void KMPlayerBrowserExtension::setLoadingProgress (int percentage) {
    emit loadingProgress (percentage);
}

void KMPlayerBrowserExtension::setURLArgs (const KParts::URLArgs & /*args*/) {
}

void KMPlayerBrowserExtension::saveState (QDataStream & stream) {
    stream << static_cast <KMPlayer *> (parent ())->url ().url ();
}

void KMPlayerBrowserExtension::restoreState (QDataStream & stream) {
    QString url;
    stream >> url;
    static_cast <KMPlayer *> (parent ())->openURL (url);
}
//---------------------------------------------------------------------

enum JSCommand {
    notsupported,
    canpause, canplay, canstop, canseek, 
    isfullscreen, isloop, isaspect,
    length, width, height, playstate,
    gotourl, nextentry, jsc_pause, play, preventry, stop, volume
};

struct JSCommandEntry {
    const char * name;
    JSCommand command;
    const char * defaultvalue;
    const KParts::LiveConnectExtension::Type rettype;
};

const int jscommandentries = 63;

// keep this list in alphabetic order
static const JSCommandEntry JSCommandList [jscommandentries] = {
    { "CanPause", canpause, "true", KParts::LiveConnectExtension::TypeBool },
    { "CanPlay", canplay, "true", KParts::LiveConnectExtension::TypeBool },
    { "CanStop", canstop, "true", KParts::LiveConnectExtension::TypeBool },
    { "DoGotoURL", gotourl, "true", KParts::LiveConnectExtension::TypeBool },
    { "DoNextEntry", notsupported, "true", KParts::LiveConnectExtension::TypeBool },
    { "DoPause", jsc_pause, "true", KParts::LiveConnectExtension::TypeBool },
    { "DoPlay", play, "true", KParts::LiveConnectExtension::TypeBool },
    { "DoPrevEntry", notsupported, "true", KParts::LiveConnectExtension::TypeBool },
    { "DoStop", stop, "true", KParts::LiveConnectExtension::TypeBool },
    { "GetAuthor", notsupported, "noname", KParts::LiveConnectExtension::TypeString },
    { "GetAutoGoToURL", notsupported, "false", KParts::LiveConnectExtension::TypeBool },
    { "GetBackgroundColor", notsupported, "ffffff", KParts::LiveConnectExtension::TypeNumber },
    { "GetBandwidthAverage", notsupported, "64", KParts::LiveConnectExtension::TypeNumber },
    { "GetBandwidthCurrent", notsupported, "64", KParts::LiveConnectExtension::TypeNumber },
    { "GetBufferingTimeElapsed", notsupported, "0", KParts::LiveConnectExtension::TypeNumber },
    { "GetBufferingTimeRemaining", notsupported, "0", KParts::LiveConnectExtension::TypeNumber },
    { "GetCanSeek", canseek, "false", KParts::LiveConnectExtension::TypeBool },
    { "GetCenter", notsupported, "true", KParts::LiveConnectExtension::TypeBool },
    { "GetClipHeight", height, 0L, KParts::LiveConnectExtension::TypeNumber }, 
    { "GetClipWidth", width, 0L, KParts::LiveConnectExtension::TypeNumber }, 
    { "GetConnectionBandwidth", notsupported, "64", KParts::LiveConnectExtension::TypeNumber },
    { "GetConsole", notsupported, "unknown", KParts::LiveConnectExtension::TypeString },
    { "GetConsoleEvents", notsupported, "false", KParts::LiveConnectExtension::TypeBool },
    { "GetControls", notsupported, "buttons", KParts::LiveConnectExtension::TypeString },
    { "GetCopyright", notsupported, "(c) whoever", KParts::LiveConnectExtension::TypeString },
    { "GetCurrentEntry", notsupported, "1", KParts::LiveConnectExtension::TypeNumber },
    { "GetDRMInfo", notsupported, "RNBA", KParts::LiveConnectExtension::TypeString },
    { "GetDoubleSize", notsupported, "false", KParts::LiveConnectExtension::TypeBool },
    { "GetEntryAbstract", notsupported, "abstract", KParts::LiveConnectExtension::TypeString },
    { "GetEntryAuthor", notsupported, "noname", KParts::LiveConnectExtension::TypeString },
    { "GetEntryCopyright", notsupported, "(c)", KParts::LiveConnectExtension::TypeString },
    { "GetEntryTitle", notsupported, "title", KParts::LiveConnectExtension::TypeString },
    { "GetFullScreen", isfullscreen, "false", KParts::LiveConnectExtension::TypeBool }, 
    { "GetImageStatus", notsupported, "false", KParts::LiveConnectExtension::TypeBool },
    { "GetLastErrorMoreInfoURL", notsupported, "no error", KParts::LiveConnectExtension::TypeString },
    { "GetLastErrorSeverity", notsupported, "6", KParts::LiveConnectExtension::TypeNumber },
    { "GetLastErrorUserCode", notsupported, "0", KParts::LiveConnectExtension::TypeNumber },
    { "GetLastErrorUserString", notsupported, "no error", KParts::LiveConnectExtension::TypeString },
    { "GetLastMessage", notsupported, "no error", KParts::LiveConnectExtension::TypeString },
    { "GetLastStatus", notsupported, "no error", KParts::LiveConnectExtension::TypeString },
    { "GetLength", length, 0L, KParts::LiveConnectExtension::TypeNumber },
    { "GetLiveState", notsupported, "false", KParts::LiveConnectExtension::TypeBool },
    { "GetLoop", isloop, 0L, KParts::LiveConnectExtension::TypeBool },
    { "GetMaintainAspect", isaspect, 0L, KParts::LiveConnectExtension::TypeBool },
    { "GetPlayState", length, 0L, KParts::LiveConnectExtension::TypeNumber },
    { "Pause", jsc_pause, 0L, KParts::LiveConnectExtension::TypeBool },
    { "Play", play, 0L, KParts::LiveConnectExtension::TypeBool },
    { "Stop", stop, 0L, KParts::LiveConnectExtension::TypeBool },
    { "Volume", volume, 0L, KParts::LiveConnectExtension::TypeBool },
    { "pause", jsc_pause, 0L, KParts::LiveConnectExtension::TypeBool },
    { "play", play, 0L, KParts::LiveConnectExtension::TypeBool },
    { "stop", stop, 0L, KParts::LiveConnectExtension::TypeBool },
    { "volume", volume, 0L, KParts::LiveConnectExtension::TypeBool },
};

const JSCommandEntry * getJSCommandEntry (const char * name, int start = 0, int end = jscommandentries) {
    if (end - start < 2) {
        if (start != end && !strcmp (JSCommandList[start].name, name))
            return &JSCommandList[start];
        return 0L;
    }
    int mid = (start + end) / 2;
    int cmp = strcmp (JSCommandList[mid].name, name);
    if (cmp < 0)
        return getJSCommandEntry (name, mid + 1, end);
    if (cmp > 0)
        return getJSCommandEntry (name, start, mid);
    return &JSCommandList[mid];
}

KMPlayerLiveConnectExtension::KMPlayerLiveConnectExtension (KMPlayer * parent)
  : KParts::LiveConnectExtension (parent), player (parent),
    lastJSCommandEntry (0L),
    m_started (false),
    m_enablefinish (false) {
      connect (parent, SIGNAL (started (KIO::Job *)), this, SLOT (started ()));
      connect (parent, SIGNAL (finished ()), this, SLOT (finished ()));
}

KMPlayerLiveConnectExtension::~KMPlayerLiveConnectExtension() {
    kdDebug () << "KMPlayerLiveConnectExtension::~KMPlayerLiveConnectExtension()" << endl;
}

void KMPlayerLiveConnectExtension::started () {
    m_started = true;
}

void KMPlayerLiveConnectExtension::finished () {
    if (m_started && m_enablefinish) {
        KParts::LiveConnectExtension::ArgList args;
        args.push_back (qMakePair (KParts::LiveConnectExtension::TypeString, QString("if (window.onFinished) onFinished();")));
        emit partEvent (0, "eval", args);
        m_started = true;
        m_enablefinish = false;
    }
}

bool KMPlayerLiveConnectExtension::get
  (const unsigned long id, const QString & name,
   KParts::LiveConnectExtension::Type & type, unsigned long & rid, QString &) {
    const char * str = name.ascii ();
    printf("get %s\n", str);
    lastJSCommandEntry = getJSCommandEntry (str);
    if (lastJSCommandEntry) {
        type = KParts::LiveConnectExtension::TypeFunction;
        rid = id;
        return true;
    }
    return false;
}

bool KMPlayerLiveConnectExtension::put
  (const unsigned long, const QString &, const QString &) {
    return false;
}

bool KMPlayerLiveConnectExtension::call
  (const unsigned long id, const QString & name,
   const QStringList & args, KParts::LiveConnectExtension::Type & type,
   unsigned long & rid, QString & rval) {
    const JSCommandEntry * entry = lastJSCommandEntry;
    const char * str = name.ascii ();
    if (!entry || strcmp (entry->name, str))
        entry = getJSCommandEntry (str);
    if (!entry)
        return false;
    kdDebug () << "entry " << entry->name << endl;
    rid = id;
    type = entry->rettype;
    switch (entry->command) {
        case notsupported:
            rval = entry->defaultvalue;
            break;
        case play:
            player->play ();
            rval = "true";
            break;
        case stop:
            player->stop ();
            rval = "true";
            break;
        case jsc_pause:
            player->pause ();
            rval = "true";
            break;
        case volume:
            if (!args.size ())
                return false;
            player->adjustVolume (args.first ().toInt ());
            rval = "true";
            break;
        case isloop:
            rval = player->settings ()->loop ? "true" : "false";
            break;
        case isaspect:
            rval = player->settings ()->sizeratio ? "true" : "false";
            break;
        case isfullscreen:
            rval = static_cast <KMPlayerView*> (player->view ())->isFullScreen () ? "true" : "false";
            break;
        case canseek:
            rval = player->process ()->source ()->isSeekable () ? "true" : "false";
            break;
        case length:
            rval.setNum (player->process ()->source ()->length ());
            break;
        case width:
            rval.setNum (player->process ()->source ()->width ());
            break;
        case height:
            rval.setNum (player->process ()->source ()->height ());
            break;
        case playstate: // FIXME 0-6
            rval = player->process ()->playing () ? "3" : "0";
            break;
        default:
            return false;
    }
    return true;
}

void KMPlayerLiveConnectExtension::unregister (const unsigned long) {
}

void KMPlayerLiveConnectExtension::setSize (int w, int h) {
    KMPlayerView * view = static_cast <KMPlayerView*> (player->view ());
    if (view->buttonBar ()->isVisible () &&
            !player->settings ()->autohidebuttons)
        h += view->buttonBar()->height();
    if (view->positionSlider()->isVisible())
        h += view->positionSlider()->height();
    QCString jscode;
    //jscode.sprintf("this.width=%d;this.height=%d;kmplayer", w, h);
    KParts::LiveConnectExtension::ArgList args;
    args.push_back (qMakePair (KParts::LiveConnectExtension::TypeString, QString("width")));
    args.push_back (qMakePair (KParts::LiveConnectExtension::TypeNumber, QString::number (w)));
    emit partEvent (0, "this.setAttribute", args);
    args.clear();
    args.push_back (qMakePair (KParts::LiveConnectExtension::TypeString, QString("height")));
    args.push_back (qMakePair (KParts::LiveConnectExtension::TypeNumber, QString::number (h)));
    emit partEvent (0, "this.setAttribute", args);
}

//-----------------------------------------------------------------------------

KMPlayerSource::KMPlayerSource (KMPlayer * player)
    : QObject (player), m_player (player) {
    kdDebug () << "KMPlayerSource::KMPlayerSource" << endl;
}

KMPlayerSource::~KMPlayerSource () {
    kdDebug () << "KMPlayerSource::~KMPlayerSource" << endl;
}

void KMPlayerSource::init () {
    m_width = 0;
    m_height = 0;
    m_aspect = 0.0;
    m_length = 0;
    m_identified = false;
    m_recordCommand.truncate (0);
}

const KURL & KMPlayerSource::url () const {
    return m_url;
}

bool KMPlayerSource::processOutput (const QString & str) {
    if (m_identified)
        return false;
    if (str.startsWith ("ID_VIDEO_WIDTH")) {
        int pos = str.find ('=');
        if (pos > 0)
            setWidth (str.mid (pos + 1).toInt());
        kdDebug () << "KMPlayerSource::processOutput " << width() << endl;
    } else if (str.startsWith ("ID_VIDEO_HEIGHT")) {
        int pos = str.find ('=');
        if (pos > 0)
            setHeight (str.mid (pos + 1).toInt());
    } else if (str.startsWith ("ID_VIDEO_ASPECT")) {
        int pos = str.find ('=');
        if (pos > 0)
            setAspect (str.mid (pos + 1).replace (',', '.').toFloat ());
    } else if (str.startsWith ("ID_LENGTH")) {
        int pos = str.find ('=');
        if (pos > 0)
            setLength (str.mid (pos + 1).toInt());
    } else
        return false;
    return true;
}

QString KMPlayerSource::filterOptions () {
    KMPlayerSettings* m_settings = m_player->settings ();
    QString PPargs ("");
    if (m_settings->postprocessing)
    {
        if (m_settings->pp_default)
            PPargs = "-vop pp=de";
        else if (m_settings->pp_fast)
            PPargs = "-vop pp=fa";
        else if (m_settings->pp_custom) {
            PPargs = "-vop pp=";
            if (m_settings->pp_custom_hz) {
                PPargs += "hb";
                if (m_settings->pp_custom_hz_aq && \
                        m_settings->pp_custom_hz_ch)
                    PPargs += ":ac";
                else if (m_settings->pp_custom_hz_aq)
                    PPargs += ":a";
                else if (m_settings->pp_custom_hz_ch)
                    PPargs += ":c";
                PPargs += "/";
            }
            if (m_settings->pp_custom_vt) {
                PPargs += "vb";
                if (m_settings->pp_custom_vt_aq && \
                        m_settings->pp_custom_vt_ch)
                    PPargs += ":ac";
                else if (m_settings->pp_custom_vt_aq)
                    PPargs += ":a";
                else if (m_settings->pp_custom_vt_ch)
                    PPargs += ":c";
                PPargs += "/";
            }
            if (m_settings->pp_custom_dr) {
                PPargs += "dr";
                if (m_settings->pp_custom_dr_aq && \
                        m_settings->pp_custom_dr_ch)
                    PPargs += ":ac";
                else if (m_settings->pp_custom_dr_aq)
                    PPargs += ":a";
                else if (m_settings->pp_custom_dr_ch)
                    PPargs += ":c";
                PPargs += "/";
            }
            if (m_settings->pp_custom_al) {
                PPargs += "al";
                if (m_settings->pp_custom_al_f)
                    PPargs += ":f";
                PPargs += "/";
            }
            if (m_settings->pp_custom_tn) {
                PPargs += "tn";
                /*if (1 <= m_settings->pp_custom_tn_s <= 3){
                    PPargs += ":";
                    PPargs += m_settings->pp_custom_tn_s;
                    }*/ //disabled 'cos this is wrong
                PPargs += "/";
            }
            if (m_settings->pp_lin_blend_int) {
                PPargs += "lb";
                PPargs += "/";
            }
            if (m_settings->pp_lin_int) {
                PPargs += "li";
                PPargs += "/";
            }
            if (m_settings->pp_cub_int) {
                PPargs += "ci";
                PPargs += "/";
            }
            if (m_settings->pp_med_int) {
                PPargs += "md";
                PPargs += "/";
            }
            if (m_settings->pp_ffmpeg_int) {
                PPargs += "fd";
                PPargs += "/";
            }
        }
        if (PPargs.endsWith("/"))
            PPargs.truncate(PPargs.length()-1);
    }
    return PPargs;
}

QString KMPlayerSource::recordCommand () {
    if (m_recordCommand.isEmpty ())
        return QString::null;
    return QString ("mencoder ") + m_player->settings()->mencoderarguments +
           QString (" ") + m_recordCommand;
}

QString KMPlayerSource::ffmpegCommand () {
    if (m_ffmpegCommand.isEmpty ())
        return QString::null;
    return QString ("ffmpeg ") + QString (" ") + m_ffmpegCommand;
}

bool KMPlayerSource::hasLength () {
    return true;
}

bool KMPlayerSource::isSeekable () {
    return true;
}

//-----------------------------------------------------------------------------

KMPlayerURLSource::KMPlayerURLSource (KMPlayer * player, const KURL & url)
    : KMPlayerSource (player) {
    m_url = url;
    kdDebug () << "KMPlayerURLSource::KMPlayerURLSource" << endl;
}

KMPlayerURLSource::~KMPlayerURLSource () {
    kdDebug () << "KMPlayerURLSource::~KMPlayerURLSource" << endl;
}

void KMPlayerURLSource::init () {
    KMPlayerSource::init ();
    isreference = false;
    m_urls.clear ();
    m_urlother = KURL ();
}

bool KMPlayerURLSource::hasLength () {
    return !!length ();
}

void KMPlayerURLSource::setURL (const KURL & url) { 
    m_url = url;
    kdDebug () << "KMPlayerURLSource::setURL:" << url.url () << endl;
    isreference = false;
    m_identified = false;
    m_urls.clear ();
    m_urlother = KURL ();
}

bool KMPlayerURLSource::processOutput (const QString & str) {
    if (m_identified)
        return false;
    if (str.startsWith ("ID_FILENAME")) {
        int pos = str.find ('=');
        if (pos < 0) 
            return false;
        KURL url (str.mid (pos + 1));
        if (url.isValid ())
            m_urls.push_front (url);
        kdDebug () << "KMPlayerURLSource::processOutput " << url.url () << endl;
        return true;
    } else if (str.startsWith ("Playing")) {
        KURL url(str.mid (8));
        if (url.isValid ()) {
            if (!isreference && !m_urlother.isEmpty ()) {
                m_urls.push_back (m_urlother);
                return true;
            }
            m_urlother = url;
            isreference = false;
            kdDebug () << "KMPlayerURLSource::processOutput " << m_url.url () << endl;
            return true;
        }
    } else if (str.find ("Reference Media file") >= 0) {
        isreference = true;
    }
    return KMPlayerSource::processOutput (str);
}

void KMPlayerURLSource::play () {
    kdDebug () << "KMPlayerURLSource::play() " << m_url.url() << endl;
    KURL url = m_url;
    if (m_urls.count () > 0)
        url = *m_urls.begin ();
    if (!url.isValid () || url.isEmpty ())
        return;
    QString args;
    m_recordCommand.truncate (0);
    m_ffmpegCommand.truncate (0);
    if (m_player->settings ()->urlbackend == QString ("Xine")) {
        QTimer::singleShot (0, m_player, SLOT (play ()));
        return;
    }
    int cache = m_player->settings ()->cachesize;
    if (url.isLocalFile () || cache <= 0)
        args.sprintf ("-slave ");
    else
        args.sprintf ("-slave -cache %d ", cache);

    if (m_player->settings ()->alwaysbuildindex && url.isLocalFile ()) {
        if (url.path ().lower ().endsWith (".avi") ||
            url.path ().lower ().endsWith (".divx")) {
            args += QString (" -idx ");
            m_recordCommand = QString (" -idx ");
        }
    }
    QString myurl (url.isLocalFile () ? url.path () : url.url ());
    m_recordCommand += myurl;
    if (url.isLocalFile ())
        m_ffmpegCommand = QString ("-i ") + url.path ();
    args += KProcess::quote (myurl);
    m_player->mplayer ()->run (args.latin1 ());
    if (m_player->liveconnectextension ())
        m_player->liveconnectextension ()->enableFinishEvent ();
}

void KMPlayerURLSource::activate () {
    KMPlayerView * view = static_cast <KMPlayerView*> (m_player->view ());
    QPopupMenu * menu = view->playerMenu ();
    menu->clear ();
    menu->insertItem (i18n ("&MPlayer"), m_player, SLOT (setMPlayer ()));
    menu->insertItem (i18n ("&Xine"), m_player, SLOT (setXine ()));
    menu->setEnabled (true);
    if (m_player->settings ()->urlbackend == QString ("Xine")) {
        menu->setItemChecked (menu->idAt (1), true);
        m_player->setProcess (m_player->xine ());
        if (!url ().isEmpty ())
            QTimer::singleShot (0, m_player, SLOT (play ()));
        return;
    } else {
        m_player->setProcess (m_player->mplayer ());
        menu->setItemChecked (menu->idAt (0), true);
    }
    init ();
    bool loop = m_player->settings ()->loop;
    m_player->settings ()->loop = false;
    if (!url ().isEmpty ()) {
        QString args ("-quiet -nocache -identify -frames 0 ");
        QString myurl (url ().isLocalFile () ? url ().path () : m_url.url ());
        args += KProcess::quote (myurl);
        if (m_player->mplayer ()->run (args.ascii ()))
            connect (m_player, SIGNAL (finished ()), this, SLOT (finished ()));
    }
    m_player->settings ()->loop = loop;
}

void KMPlayerURLSource::finished () {
    kdDebug () << "KMPlayerURLSource::finished()" << m_identified << " "  <<  m_player->hrefSource ()->url ().url () << " " <<  m_player->hrefSource ()->url ().isValid () << endl;
    if (m_identified && m_player->hrefSource ()->url ().isValid ()) {
        disconnect (m_player, SIGNAL (finished ()), this, SLOT (finished ()));
        m_player->setSource (m_player->hrefSource (), true);
        return;
    }
    if (!m_player->hrefSource ()->url ().isValid ())
        disconnect (m_player, SIGNAL (finished ()), this, SLOT (finished ()));
    m_player->setMovieLength (10 * length ());
    if (!isreference && !m_urlother.isEmpty ())
        m_urls.push_back (m_urlother);
    m_urlother = KURL ();
    m_identified = true;
    kdDebug () << "KMPlayerURLSource::finished()" << m_player->autoPlay() << endl;
    if (m_player->autoPlay())
        QTimer::singleShot (0, this, SLOT (play ()));
}

void KMPlayerURLSource::deactivate () {
    disconnect (m_player, SIGNAL (finished ()), this, SLOT (finished ()));
    KMPlayerView * view = static_cast <KMPlayerView*> (m_player->view ());
    if (view)
        view->playerMenu ()->setEnabled (false);
}

//-----------------------------------------------------------------------------

KMPlayerHRefSource::KMPlayerHRefSource (KMPlayer * player)
    : KMPlayerSource (player) {
    kdDebug () << "KMPlayerHRefSource::KMPlayerHRefSource" << endl;
}

KMPlayerHRefSource::~KMPlayerHRefSource () {
    kdDebug () << "KMPlayerHRefSource::~KMPlayerHRefSource" << endl;
}

void KMPlayerHRefSource::init () {
    KMPlayerSource::init ();
}

bool KMPlayerHRefSource::hasLength () {
    return false;
}

bool KMPlayerHRefSource::processOutput (const QString & /*str*/) {
    //return KMPlayerSource::processOutput (str);
    return true;
}

void KMPlayerHRefSource::setURL (const KURL & url) { 
    m_url = url;
    m_finished = false;
    kdDebug () << "KMPlayerHRefSource::setURL " << m_url.url() << endl;
}

void KMPlayerHRefSource::play () {
    kdDebug () << "KMPlayerHRefSource::play " << m_url.url() << endl;
    m_player->setSource (m_player->urlSource (), true);
}

void KMPlayerHRefSource::activate () {
    m_player->setProcess (m_player->mplayer ());
    if (m_finished) {
        QTimer::singleShot (0, this, SLOT (finished ()));
        return;
    }
    KMPlayerView * view = static_cast <KMPlayerView*> (m_player->view ());
    init ();
    m_player->stop ();
    m_player->mplayer ()->initProcess ();
    view->consoleOutput ()->clear ();
    unlink (locateLocal ("data", "kmplayer/00000001.jpg").ascii ());
    QString outdir = locateLocal ("data", "kmplayer/");
    QString myurl (m_url.isLocalFile () ? m_url.path () : m_url.url ());
    QString args;
    args.sprintf ("mplayer -vo jpeg -jpeg outdir=%s -frames 1 -nosound -quiet %s", KProcess::quote (outdir).ascii (), KProcess::quote (myurl).ascii ());
    *m_player->mplayer ()->process () << args;
    kdDebug () << args << endl;
    m_player->mplayer ()->process ()->start (KProcess::NotifyOnExit, KProcess::NoCommunication);

    if (m_player->playing ())
        connect (m_player, SIGNAL (finished ()), this, SLOT (finished ()));
    else {
        setURL (KURL ());
        QTimer::singleShot (0, this, SLOT (play ()));
    }
}

void KMPlayerHRefSource::finished () {
    KMPlayerView * view = static_cast <KMPlayerView*> (m_player->view ());
    m_finished = true;
    disconnect (m_player, SIGNAL (finished ()), this, SLOT (finished ()));
    kdDebug () << "KMPlayerHRefSource::finished()" << endl;
    QString outfile = locateLocal ("data", "kmplayer/00000001.jpg");
    if (!view->setPicture (outfile)) {
        setURL (KURL ());
        QTimer::singleShot (0, this, SLOT (play ()));
        return;
    }
    view->positionSlider()->hide();
    connect (view->viewer (), SIGNAL (clicked ()), this, SLOT (play ()));
}

void KMPlayerHRefSource::deactivate () {
    kdDebug () << "KMPlayerHRefSource::deactivate()" << endl;
    KMPlayerView * view = static_cast <KMPlayerView*> (m_player->view ());
    view->setPicture (QString::null);
    disconnect (view->viewer (), SIGNAL (clicked ()), this, SLOT (play ()));
}

#include "kmplayer_part.moc"
#include "kmplayersource.moc"
