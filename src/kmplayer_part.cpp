/***************************************************************************
                          kmplayer_part.cpp  -  description
                             -------------------
    begin                : Sun Dec 8 2002
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
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>

#ifdef KDE_USE_FINAL
#undef Always
#include <qdir.h>
#endif
#include <qapplication.h>
#include <qcstring.h>
#include <qregexp.h>
#include <qmultilineedit.h>
#include <qpair.h>
#include <qpushbutton.h>
#include <qpopupmenu.h>
#include <qslider.h>
#include <qvaluelist.h>

#include <kprocess.h>
#include <kprocctrl.h>
#include <klibloader.h>
#include <kmessagebox.h>
#include <kaboutdata.h>
#include <kdebug.h>
#include <kconfig.h>
#include <kaction.h>
#include <kprotocolmanager.h>

#include "kmplayer_part.h"
#include "kmplayerview.h"
#include "kmplayerconfig.h"
#include <qfile.h>

//merge KProtocolManager::proxyForURL and KProtocolManager::slaveProtocol
static bool revmatch(const char *host, const char *nplist) {
    if (host == 0) return false;

    const char *hptr = host + strlen( host ) - 1;
    const char *nptr = nplist + strlen( nplist ) - 1;
    const char *shptr = hptr;

    while (nptr >= nplist) {
        if (*hptr != *nptr) {
            hptr = shptr;

            // Try to find another domain or host in the list
            while (--nptr >= nplist && *nptr != ',' && *nptr != ' ');

            // Strip out multiple spaces and commas
            while (--nptr >= nplist && (*nptr == ',' || *nptr == ' '));
        } else {
            if (nptr == nplist || nptr[-1] == ',' || nptr[-1] == ' ')
                return true;
            hptr--;
            nptr--;
        }
    }
    return false;
}

static bool proxyForURL (KURL & url, QString & proxy) {
    QString protocol = url.protocol ();
    bool protocol_hack = false;
    if (protocol != "http" || protocol != "https" || protocol != "ftp") {
        protocol_hack = true;
        url.setProtocol ("http");
    }
    proxy = KProtocolManager::proxyForURL (url);
    if (protocol_hack)
        url.setProtocol (protocol);
    if (!proxy.isEmpty() && proxy != QString::fromLatin1 ("DIRECT")) {
        QString noProxy = KProtocolManager::noProxyFor ();
        KProtocolManager::ProxyType type = KProtocolManager::proxyType();
        bool useRevProxy = ((type == KProtocolManager::ManualProxy ||
                             type == KProtocolManager::EnvVarProxy) &&
                            KProtocolManager::useReverseProxy ());
        bool isRevMatch = false;

        if (!noProxy.isEmpty()) {
            QString qhost = url.host().lower();
            const char *host = qhost.latin1();
            QString qno_proxy = noProxy.stripWhiteSpace().lower();
            const char *no_proxy = qno_proxy.latin1();
            isRevMatch = revmatch(host, no_proxy);
            // If the hostname does not contain a dot, check if
            // <local> is part of noProxy.
            if (!isRevMatch && host && (strchr(host, '.') == NULL))
                isRevMatch = revmatch("<local>", no_proxy);
        }
        if ((!useRevProxy && !isRevMatch) || (useRevProxy && isRevMatch))
            return true;
    }
    return false;
}

//-----------------------------------------------------------------------------

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
   QObject *parent, const char * name, const char *, const QStringList & args) {
    return new KMPlayer (wparent, wname, parent, name, args);
}

//-----------------------------------------------------------------------------

KMPlayer::KMPlayer (QWidget * parent, KConfig * config)
 : KMediaPlayer::Player (parent, ""),
   m_config (config),
   m_view (new KMPlayerView (parent)),
   m_configdialog (new KMPlayerConfig (this, config)),
   m_liveconnectextension (0L),
   movie_width (0),
   movie_height (0),
   m_ispart (false) {
    m_view->init ();
    init();
}

KMPlayer::KMPlayer (QWidget * wparent, const char *wname,
                    QObject * parent, const char *name, const QStringList &args)
 : KMediaPlayer::Player (wparent, wname, parent, name),
   m_config (new KConfig ("kmplayerrc")),
   m_view (new KMPlayerView (wparent, wname)),
   m_configdialog (new KMPlayerConfig (this, m_config)),
   m_liveconnectextension (new KMPlayerLiveConnectExtension (this)),
   movie_width (0),
   movie_height (0),
   m_ispart (true) {
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
                value = value.left(value.length()-1);
            kdDebug () << "name=" << name << " value=" << value << endl;
            if (name.lower () == "href")
                m_href = value;
            else if (name.lower()==QString::fromLatin1("width"))
                movie_width = value.toInt();
            else if (name.lower()==QString::fromLatin1("height"))
                movie_height = value.toInt();
        }
    }
    m_view->init ();
    m_configdialog->readConfig ();
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
    m_configdialog->show ();
}

void KMPlayer::init () {
    m_process = 0L;
    m_use_slave = false;
    initProcess ();
    m_browserextension = new KMPlayerBrowserExtension (this);
    m_movie_position = 0;
    setMovieLength (0);
    m_bPosSliderPressed = false;
    m_posRegExp.setPattern ("V:\\s*([0-9\\.]+)");
    connect (m_view->backButton (), SIGNAL (clicked ()), this, SLOT (back ()));
    connect (m_view->playButton (), SIGNAL (clicked ()), this, SLOT (play ()));
    connect (m_view->forwardButton (), SIGNAL (clicked ()), this, SLOT (forward ()));
    connect (m_view->pauseButton (), SIGNAL (clicked ()), this, SLOT (pause ()));
    connect (m_view->stopButton (), SIGNAL (clicked ()), this, SLOT (stop ()));
    connect (m_view->positionSlider (), SIGNAL (sliderMoved (int)), SLOT (posSliderChanged (int)));
    connect (m_view->positionSlider (), SIGNAL (sliderPressed()), SLOT (posSliderPressed()));
    connect (m_view->positionSlider (), SIGNAL (sliderReleased()), SLOT (posSliderReleased()));
    m_view->popupMenu ()->connectItem (KMPlayerView::menu_config,
                                       m_configdialog, SLOT (show ()));
    //connect (m_view->configButton (), SIGNAL (clicked ()), m_configdialog, SLOT (show ()));
}

void KMPlayer::initProcess () {
    delete m_process;
    m_process = new KProcess;
    m_process->setUseShell (true);
    if (!m_url.isEmpty ()) {
        QString proxy_url;
        if (KProtocolManager::useProxy () && proxyForURL (m_url, proxy_url))
            m_process->setEnvironment("http_proxy", proxy_url);
    }
    connect (m_process, SIGNAL (receivedStdout (KProcess *, char *, int)),
            this, SLOT (processOutput (KProcess *, char *, int)));
    connect (m_process, SIGNAL (receivedStderr (KProcess *, char *, int)),
            this, SLOT (processOutput (KProcess *, char *, int)));
    connect (m_process, SIGNAL (wroteStdin (KProcess *)),
            this, SLOT (processDataWritten (KProcess *)));
    connect (m_process, SIGNAL (processExited (KProcess *)),
            this, SLOT (processStopped (KProcess *)));
}

KMPlayer::~KMPlayer () {
    if (!m_ispart)
        delete (KMPlayerView*) m_view;
    m_view = (KMPlayerView*) 0;
    stop ();
    delete m_configdialog;
    delete m_process;
    delete m_browserextension;
    delete m_liveconnectextension;
    if (m_ispart)
        delete m_config;
}

KMediaPlayer::View* KMPlayer::view () {
    return m_view;
}

void KMPlayer::setURL (const KURL &url) {
    if (!m_url.isEmpty () && m_url != url)
        closeURL ();
    m_url = url;
}

bool KMPlayer::openURL (const KURL & _url) {
    if (!m_view) return false;
    KURL url = _url;
    if (!m_href.isEmpty ())
        url = m_href;
    if (url.isValid ()) {
        setURL (url);
        play ();
        m_href == QString::null;
        if (m_process->isRunning ()) {
            emit started ((KIO::Job*) 0);
            return true;
        }
    }
    return false;
}

bool KMPlayer::closeURL () {
    stop ();
    m_href = QString::null;
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

void KMPlayer::processOutput (KProcess *, char * str, int slen) {
    if (!m_view || slen <= 0) return;

    do {
        int len = strcspn (str, "\r\n");
        QString out = m_process_output + QString::fromLocal8Bit (str, len);
        m_process_output = QString::null;
        str += len;
        slen -= len;
        if (slen <= 0) {
            m_process_output = out;
            break;
        }
        bool process_stats = false;
        if (str[0] == '\r') {
            if (slen > 1 && str[1] == '\n') {
                str++;
                slen--;
            } else
                process_stats = true;
        }
        str++;
        slen--;

        if (process_stats) {
            if (m_posRegExp.search (out) > -1) {
                m_movie_position = int (10.0 * m_posRegExp.cap (1).toFloat ());
                QSlider *slider = m_view->positionSlider ();
                if (m_movie_length <= 0 &&
                    m_movie_position > 7 * slider->maxValue () / 8)
                    slider->setMaxValue (slider->maxValue() * 2);
                else if (slider->maxValue() < m_movie_position)
                    slider->setMaxValue (int (1.4 * slider->maxValue()));
                if (!m_bPosSliderPressed)
                    slider->setValue (m_movie_position);
            }
        } else {
            m_view->addText (out + QString ("\n"));
            QRegExp sizeRegExp (m_configdialog->sizepattern);
            bool ok;
            if (movie_width <= 0 && sizeRegExp.search (out) > -1) {
                movie_width = sizeRegExp.cap (1).toInt (&ok);
                movie_height = ok ? sizeRegExp.cap (2).toInt (&ok) : 0;
                if (ok && movie_width > 0 && movie_height > 0 && m_view->viewer ()->aspect () < 0.01) {
                    m_view->viewer ()->setAspect (1.0 * movie_width / movie_height);
                    if (m_liveconnectextension)
                        m_liveconnectextension->setSize (movie_width, movie_height);
                }
            } else if (m_browserextension) {
                QRegExp cacheRegExp (m_configdialog->cachepattern);
                QRegExp startRegExp (m_configdialog->startpattern);
                if (cacheRegExp.search (out) > -1) {
                    double p = cacheRegExp.cap (1).toDouble (&ok);
                    if (ok) {
                        m_browserextension->setLoadingProgress (int (p));
                        m_browserextension->infoMessage 
                            (QString (cacheRegExp.cap (1)) + i18n ("% Cache fill"));
                    }
                } else if (startRegExp.search (out) > -1) {
                    m_browserextension->setLoadingProgress (100);
                    emit completed ();
                    m_started_emited = false;
                    m_browserextension->infoMessage (i18n("KMPlayer: Playing"));
                }
            }
        }
    } while (slen > 0);
}

void KMPlayer::keepMovieAspect (bool b) {
    m_view->setKeepSizeRatio (b);
    if (b) {
        if (m_view->viewer ()->aspect () < 0.01 && movie_height > 0)
            m_view->viewer ()->setAspect (1.0 * movie_width / movie_height);
    } else
        m_view->viewer ()->setAspect (0.0);
}

void KMPlayer::sendCommand (const QString & cmd) {
    if (m_process->isRunning () && m_use_slave) {
        commands.push_front (cmd + "\n");
        printf ("eval %s", commands.last ().latin1 ());
        m_process->writeStdin (QFile::encodeName(commands.last ()),
                               commands.last ().length ());
    }
}

void KMPlayer::processDataWritten (KProcess *) {
    printf ("eval done %s", commands.last ().latin1 ());
    commands.pop_back ();
    if (commands.size ())
        m_process->writeStdin (QFile::encodeName(commands.last ()),
                             commands.last ().length ());
}

void KMPlayer::processStopped (KProcess *) {
    printf("process stopped\n");
    if (m_movie_position > m_movie_length)
        setMovieLength (m_movie_position);
    m_movie_position = 0;
    if (m_started_emited) {
        m_started_emited = false;
        m_browserextension->setLoadingProgress (100);
        emit completed ();
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

void KMPlayer::setMovieLength (int len) {
    m_movie_length = len;
    if (m_view)
        m_view->positionSlider()->setMaxValue (len > 0 ? m_movie_length : 300);
}

void KMPlayer::pause () {
    sendCommand (QString ("pause"));
}

void KMPlayer::back () {
    QString cmd;
    cmd.sprintf ("seek -%d 0", m_seektime);
    sendCommand (cmd);
}

void KMPlayer::forward () {
    QString cmd;
    cmd.sprintf ("seek %d 0", m_seektime);
    sendCommand (cmd);
}

bool KMPlayer::run (const char * args, const char * pipe) {
    m_movie_position = 0;
    m_view->consoleOutput ()->clear ();
    m_process_output = QString::null;
    m_started_emited = false;
    initProcess ();

    if (!m_configdialog->showposslider)
        m_view->positionSlider()->hide();
    else
        m_view->positionSlider()->show();

    m_use_slave = !(pipe && pipe[0]);
    if (!m_use_slave) {
        printf ("%s | ", pipe);
        *m_process << pipe << " | ";
    }
    printf ("mplayer -wid %lu", (unsigned long) m_view->viewer ()->winId ());
    *m_process << "mplayer -wid " << QString::number (m_view->viewer ()->winId());
    if (m_configdialog->videodriver.length () > 0) {
        printf (" -vo %s", m_configdialog->videodriver.latin1 ());
        *m_process << " -vo " << m_configdialog->videodriver;
    }
    if (m_configdialog->loop) {
        printf (" -loop 0");
        *m_process << " -loop 0 ";
    }
    if (m_configdialog->usearts) {
        printf (" -ao arts");
        *m_process << " -ao arts";
    }
    if (m_configdialog->additionalarguments.length () > 0) {
        printf (" %s", m_configdialog->additionalarguments.latin1 ());
        *m_process << " " << m_configdialog->additionalarguments;
    }
    // postproc thingies
    if (m_configdialog->postprocessing)
    {
        QString PPargs;
        if (m_configdialog->pp_default)
            PPargs = "-vop pp=de";
        else if (m_configdialog->pp_fast)
            PPargs = "-vop pp=fa";
        else if (m_configdialog->pp_custom) {
            PPargs = "-vop pp=";
            if (m_configdialog->pp_custom_hz) {
                PPargs += "hb";
                if (m_configdialog->pp_custom_hz_aq && \
                        m_configdialog->pp_custom_hz_ch)
                    PPargs += ":ac";
                else if (m_configdialog->pp_custom_hz_aq)
                    PPargs += ":a";
                else if (m_configdialog->pp_custom_hz_ch)
                    PPargs += ":c";
                PPargs += "/";
            }
            if (m_configdialog->pp_custom_vt) {
                PPargs += "vb";
                if (m_configdialog->pp_custom_vt_aq && \
                        m_configdialog->pp_custom_vt_ch)
                    PPargs += ":ac";
                else if (m_configdialog->pp_custom_vt_aq)
                    PPargs += ":a";
                else if (m_configdialog->pp_custom_vt_ch)
                    PPargs += ":c";
                PPargs += "/";
            }
            if (m_configdialog->pp_custom_dr) {
                PPargs += "dr";
                if (m_configdialog->pp_custom_dr_aq && \
                        m_configdialog->pp_custom_dr_ch)
                    PPargs += ":ac";
                else if (m_configdialog->pp_custom_dr_aq)
                    PPargs += ":a";
                else if (m_configdialog->pp_custom_dr_ch)
                    PPargs += ":c";
                PPargs += "/";
            }
            if (m_configdialog->pp_custom_al) {
                PPargs += "al";
                if (m_configdialog->pp_custom_al_f)
                    PPargs += ":f";
                PPargs += "/";
            }
            if (m_configdialog->pp_custom_tn) {
                PPargs += "tn";
                if (1 <= m_configdialog->pp_custom_tn_s <= 3)
                    PPargs += m_configdialog->pp_custom_tn_s;
                PPargs += "/";
            }
            if (m_configdialog->pp_lin_blend_int) {
                PPargs += "lb";
                PPargs += "/";
            }
            if (m_configdialog->pp_lin_int) {
                PPargs += "li";
                PPargs += "/";
            }
            if (m_configdialog->pp_cub_int) {
                PPargs += "ci";
                PPargs += "/";
            }
            if (m_configdialog->pp_med_int) {
                PPargs += "md";
                PPargs += "/";
            }
            if (m_configdialog->pp_ffmpeg_int) {
                PPargs += "fd";
                PPargs += "/";
            }
        }
        if (PPargs.endsWith("/"))
            PPargs.truncate(PPargs.length()-1);

        printf (" %s", PPargs.ascii());
        *m_process << " " << PPargs;
    }
    printf (" %s", args);
    *m_process << " " << args;
    if (!m_url.isEmpty () && m_url.isValid ()) {
        QString myurl;
        myurl = m_url.isLocalFile () ? m_url.path () : m_url.url ();
        printf (" %s\n", KProcess::quote (myurl).latin1 ());
        *m_process << " " << KProcess::quote (myurl);
    } else
        printf ("\n");

    QValueList<QCString>::const_iterator it;
    QString sMPArgs;
    for ( it = m_process->args().begin(); it != m_process->args().end(); ++it )
        sMPArgs += (*it);

    m_view->addText( sMPArgs.simplifyWhiteSpace() );

    m_process->start (KProcess::NotifyOnExit, KProcess::All);

    if (m_process->isRunning ()) {
        if (!m_view->playButton ()->isOn ()) m_view->playButton ()->toggle ();
        emit started (0L);
        m_started_emited = true;
        m_view->positionSlider()->setEnabled (true);
        return true;
    } else {
        if (m_view->playButton ()->isOn ()) m_view->playButton ()->toggle ();
        emit canceled (i18n ("Could not start MPlayer"));
        return false;
    }

}

void KMPlayer::play () {
    if (m_process->isRunning ()) {
        sendCommand (QString ("gui_play"));
        if (!m_view->playButton ()->isOn ()) m_view->playButton ()->toggle ();
        return;
    }
    if (!m_url.isValid () || m_url.isEmpty ()) {
        if (m_view->playButton ()->isOn ()) m_view->playButton ()->toggle ();
        return;
    }
    QCString args;
    if (m_url.isLocalFile () || m_cachesize <= 0)
        args.sprintf ("-slave");
    else
        args.sprintf ("-slave -cache %d", m_cachesize);
    run (args);
    emit running ();
}

bool KMPlayer::playing () const {
    return m_process->isRunning ();
}

void KMPlayer::stop () {
    if (m_process->isRunning ()) {
        if (m_view && !m_view->stopButton ()->isOn ())
            m_view->stopButton ()->toggle ();
        do {
            if (m_use_slave) {
                sendCommand (QString ("quit"));
                KProcessController::theKProcessController->waitForProcessExit (1);
                if (!m_process->isRunning ())
                    break;
                m_process->kill (SIGTERM);
            } else {
                void (*oldhandler)(int) = signal(SIGTERM, SIG_IGN);
                ::kill (-1 * ::getpid (), SIGTERM);
                signal(SIGTERM, oldhandler);
            }
            KProcessController::theKProcessController->waitForProcessExit (1);
            if (!m_process->isRunning ())
                break;
            m_process->kill (SIGKILL);
            KProcessController::theKProcessController->waitForProcessExit (1);
            if (m_process->isRunning ()) {
                processStopped (0L); // give up
                KMessageBox::error (m_view, i18n ("Failed to end MPlayer process."), i18n ("KMPlayer: Error"));
            }
        } while (false);
    }
    if (m_view && m_view->stopButton ()->isOn ())
        m_view->stopButton ()->toggle ();
}

void KMPlayer::timerEvent (QTimerEvent *) {
}

void KMPlayer::seek (unsigned long msec) {
    QString cmd;
    cmd.sprintf ("seek %lu 2", msec/1000);
    sendCommand (cmd);
}

void KMPlayer::seekPercent (float per) {
    QString cmd;
    cmd.sprintf ("seek %f 1", per);
    sendCommand (cmd);
}

void KMPlayer::adjustVolume (int incdec) {
    sendCommand (QString ("volume ") + QString::number (incdec));
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
}

void KMPlayer::posSliderChanged (int pos) {
    seekPercent (100.0 * pos / m_view->positionSlider()->maxValue());
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

KMPlayerLiveConnectExtension::KMPlayerLiveConnectExtension (KMPlayer * parent)
  : KParts::LiveConnectExtension (parent), player (parent), m_started (false) {
      connect (parent, SIGNAL (running ()), this, SLOT (started ()));
      connect (parent, SIGNAL (finished ()), this, SLOT (finished ()));
}

KMPlayerLiveConnectExtension::~KMPlayerLiveConnectExtension() {
}

void KMPlayerLiveConnectExtension::started () {
    m_started = true;
}

void KMPlayerLiveConnectExtension::finished () {
    if (m_started) {
        KParts::LiveConnectExtension::ArgList args;
        args.push_back (qMakePair (KParts::LiveConnectExtension::TypeString, QString("if (window.onFinished) onFinished();")));
        emit partEvent (0, "eval", args);
        m_started = true;
    }
}

bool KMPlayerLiveConnectExtension::get
  (const unsigned long id, const QString & name,
   KParts::LiveConnectExtension::Type & type, unsigned long & rid, QString &) {
    printf("get %s\n", name.latin1());
    if (name == "play" || name == "stop" || name == "pause" || name == "volume") {
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
   unsigned long & rid, QString &) {
    if (name == "play" || name == "stop" || name == "pause" || name == "volume") {
        type = KParts::LiveConnectExtension::TypeVoid;
        rid = id;
        if (name == "play")
            player->play ();
        else if (name == "stop")
            player->stop ();
        else if (name == "pause")
            player->pause ();
        else if (name == "volume" && args.size () > 0)
            player->adjustVolume (args.first ().toInt ());
        return true;
    }
    return false;
}

void KMPlayerLiveConnectExtension::unregister (const unsigned long) {
}

void KMPlayerLiveConnectExtension::setSize (int w, int h) {
    KMPlayerView * view = static_cast <KMPlayerView*> (player->view ());
    if (view->buttonBar()->isVisible())
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

#include "kmplayer_part.moc"
