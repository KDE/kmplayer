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

#include <kprocess.h>
#include <kprocctrl.h>
#include <klibloader.h>
#include <kmessagebox.h>
#include <kaboutdata.h>
#include <kdebug.h>
#include <kconfig.h>
#include <kaction.h>
#include <kprotocolmanager.h>
#include <kfiledialog.h>
#include <kstandarddirs.h>

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
#ifdef HAVE_KOFFICE

#include <qdom.h>
#include <qmetaobject.h>
#include <qptrlist.h>
#include <qpainter.h>
#include <koFrame.h>

KOfficeMPlayer::KOfficeMPlayer (QWidget *parentWidget, const char *widgetName, QObject* parent, const char* name, bool singleViewMode) 
  : KoDocument (parentWidget, widgetName, parent, name, singleViewMode),
    m_config (new KConfig ("kmplayerrc")),
    m_player (new KMPlayer (parentWidget, m_config))
{
    setInstance (KMPlayerFactory::instance (), false);
    setReadWrite (false);
    m_player->setSource (m_player->urlSource ());
    //setWidget (view);
}

KOfficeMPlayer::~KOfficeMPlayer () {
    kdDebug() << "KOfficeMPlayer::~KOfficeMPlayer" << /*kdBacktrace() <<*/ endl;
}

void KOfficeMPlayer::paintContent (QPainter& p, const QRect& r, bool, double, double) {
    p.fillRect (r, QBrush (QColor (0, 0, 0)));
}

bool KOfficeMPlayer::initDoc() {
    kdDebug() << "KOfficeMPlayer::initDoc" << endl;
    return true;
}

bool KOfficeMPlayer::loadXML (QIODevice *, const QDomDocument & doc) {
    QDomNode node = doc.firstChild ();
    if (node.isNull ()) return true;
    kdDebug() << "KOfficeMPlayer::loadXML " << node.nodeName () << endl; 
    node = node.firstChild ();
    if (node.isNull ()) return true;
    kdDebug() << "KOfficeMPlayer::loadXML " << node.nodeName () << endl; 
    node = node.firstChild ();
    if (node.isNull () || !node.isText ()) return true;
    m_player->setURL (KURL (node.toText ().data ()));
    return true;
}

QDomDocument KOfficeMPlayer::saveXML() {
    QDomDocument doc = createDomDocument ("kmplayer", QString::number(1.0));
    QDomElement docelm = doc.documentElement();
    docelm.setAttribute ("editor", "KMPlayer");
    docelm.setAttribute ("mime", "application/x-kmplayer");
    QDomElement url = doc.createElement ("url");
    url.appendChild (doc.createTextNode (m_player->url ().url ()));
    doc.appendChild (url);
    return doc;
}

KoView* KOfficeMPlayer::createViewInstance (QWidget* parent, const char* name) {
    kdDebug() << "KOfficeMPlayer::createViewInstance" << endl;
    return new KOfficeMPlayerView (this, parent);
}

#include <qlayout.h>
KOfficeMPlayerView::KOfficeMPlayerView (KOfficeMPlayer* part, QWidget* parent, const char* name)
    : KoView (part, parent, name),
      m_view (static_cast <KMPlayerView*> (part->player ()->view ())) {
    kdDebug() << "KOfficeMPlayerView::KOfficeMPlayerView this:" << this << " parent:" << parent << endl;
    m_oldparent = static_cast <QWidget*> (m_view->parent());
    m_view->reparent (this, QPoint (0, 0));
    QVBoxLayout * box = new QVBoxLayout (this, 0, 0);
    box->addWidget (m_view);
}

KOfficeMPlayerView::~KOfficeMPlayerView () {
    kdDebug() << "KOfficeMPlayerView::~KOfficeMPlayerView this:" << this << endl;
    m_view->reparent (m_oldparent, QPoint (0, 0));
}

#endif //HAVE_KOFFICE
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
   m_source (0L),
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
   m_source (0L),
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
    m_process = 0L;
    m_use_slave = false;
    m_recording = false;
    m_started_emited = false;
    initProcess ();
    m_browserextension = new KMPlayerBrowserExtension (this);
    m_movie_position = 0;
    m_bPosSliderPressed = false;
    m_view->contrastSlider ()->setValue (m_settings->contrast);
    m_view->brightnessSlider ()->setValue (m_settings->brightness);
    m_view->hueSlider ()->setValue (m_settings->hue);
    m_view->saturationSlider ()->setValue (m_settings->saturation);
    m_posRegExp.setPattern (m_settings->positionpattern);
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
    setMovieLength (0);
    //connect (m_view->configButton (), SIGNAL (clicked ()), m_settings, SLOT (show ()));
}

void KMPlayer::initProcess () {
    delete m_process;
    m_process = new KProcess;
    m_process->setUseShell (true);
    if (!m_urlsource->url ().isEmpty ()) {
        QString proxy_url;
        KURL url = m_urlsource->url();
        if (KProtocolManager::useProxy () && proxyForURL (url, proxy_url))
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
    kdDebug() << "KMPlayer::~KMPlayer" << kdBacktrace() << endl;
    if (!m_ispart)
        delete (KMPlayerView*) m_view;
    m_view = (KMPlayerView*) 0;
    stop ();
    delete m_settings;
    delete m_process;
    if (m_ispart)
        delete m_config;
}

KMediaPlayer::View* KMPlayer::view () {
    return m_view;
}

void KMPlayer::setSource (KMPlayerSource * source, bool keepsizes) {
    KMPlayerSource * oldsource = m_source;
    int w = movie_width;
    int h = movie_height;
    if (oldsource) {
        oldsource->deactivate ();
        closeURL ();
    }
    m_source = source;
    if (!oldsource)
        setMovieLength (0);
    if (m_source->hasLength () && m_settings->showposslider)
        m_view->positionSlider()->show ();
    else
        m_view->positionSlider()->hide ();
    if (m_source->isSeekable ()) {
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
    if (m_source) QTimer::singleShot (0, m_source, SLOT (activate ()));
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
            if (m_source->hasLength () && m_posRegExp.search (out) > -1) {
                m_movie_position = int (10.0 * m_posRegExp.cap (1).toFloat ());
                QSlider *slider = m_view->positionSlider ();
                if (m_source->length () <= 0 &&
                    m_movie_position > 7 * slider->maxValue () / 8)
                    slider->setMaxValue (slider->maxValue() * 2);
                else if (slider->maxValue() < m_movie_position)
                    slider->setMaxValue (int (1.4 * slider->maxValue()));
                if (!m_bPosSliderPressed)
                    slider->setValue (m_movie_position);
            } else if (m_browserextension) {
                if (m_cacheRegExp.search (out) > -1) {
                    double p = m_cacheRegExp.cap (1).toDouble ();
                    m_browserextension->setLoadingProgress (int (p));
                    m_browserextension->infoMessage 
                      (QString (m_cacheRegExp.cap (1)) + i18n ("% Cache fill"));
                }
            }
	    
        } else {
            m_view->addText (out + QString ("\n"));
            if (m_source->processOutput (out))
                ;
            else {
                QRegExp sizeRegExp (m_settings->sizepattern);
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
                    QRegExp startRegExp (m_settings->startpattern);
                    if (startRegExp.search (out) > -1) {
                        m_browserextension->setLoadingProgress (100);
                        emit completed ();
                        m_started_emited = false;
                        m_browserextension->infoMessage (i18n("KMPlayer: Playing"));
                    }
                }
            }
        }
    // WHERE SHOULD THIS BE?
    // how can we write to statusbar from here?
    
    /*if (m_indexRegExp.search (out) > -1) {
			double p = m_indexRegExp.cap (1).toDouble();
			//parent->setLoadingProgress (int (p));
			parent->slotStatusMsg (QString (i18n("Opening file...") + m_indexRegExp.cap (1) + "%");
		}*/
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
    if (m_process->isRunning () && m_use_slave && !m_recording) {
        commands.push_front (cmd + "\n");
        printf ("eval %s", commands.last ().latin1 ());
        m_process->writeStdin (QFile::encodeName(commands.last ()),
                               commands.last ().length ());
    }
}

void KMPlayer::processDataWritten (KProcess *) {
    if (!commands.size ()) return;
    printf ("eval done %s", commands.last ().latin1 ());
    commands.pop_back ();
    if (commands.size ())
        m_process->writeStdin (QFile::encodeName(commands.last ()),
                             commands.last ().length ());
}

void KMPlayer::processStopped (KProcess *) {
    printf("process stopped\n");
    commands.clear ();
    if (m_recording) {
        m_recording = false;
        if (m_view && m_view->recordButton ()->isOn ()) 
            m_view->recordButton ()->toggle ();
        if (m_settings->autoplayafterrecording)
            openURL (m_recordurl);
        return;
    }
    if (m_movie_position > m_source->length ())
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
    if (!m_source)
        return;
    m_source->setLength (len);
    if (m_view)
        m_view->positionSlider()->setMaxValue (len > 0 ? m_source->length () + 9 : 300);
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

void KMPlayer::record () {
    if (!m_source || m_source->recordCommand ().isEmpty ()) {
        if (m_view->recordButton ()->isOn ()) 
            m_view->recordButton ()->toggle ();
        return;
    }
    if (m_recording) {
        stop ();
        return;
    }
    KFileDialog *dlg = new KFileDialog (QString::null, QString::null, m_view, "", true);
    if (dlg->exec ()) {
        stop ();
        initProcess ();
        m_recording = true;
        m_recordurl = dlg->selectedURL().url ();
        QString myurl = KProcess::quote (m_recordurl.isLocalFile () ? m_recordurl.path () : m_recordurl.url ());
        kdDebug () << m_source->recordCommand () << " -o " << myurl << endl;
        *m_process << m_source->recordCommand () << " -o " << myurl;
        m_process->start (KProcess::NotifyOnExit, KProcess::NoCommunication);
        if (!m_process->isRunning () && m_view->recordButton ()->isOn ()) 
            m_view->recordButton ()->toggle ();
    } else
        m_view->recordButton ()->toggle ();
    delete dlg;
}

bool KMPlayer::run (const char * args, const char * pipe) {
    stop ();
    m_movie_position = 0;
    m_view->consoleOutput ()->clear ();
    m_process_output = QString::null;
    m_started_emited = false;
    initProcess ();
    m_cacheRegExp.setPattern (m_settings->cachepattern);
    m_indexRegExp.setPattern (m_settings->indexpattern);

    if (m_settings->showposslider && m_source->hasLength ())
        m_view->positionSlider()->show();
    else
        m_view->positionSlider()->hide();

    m_use_slave = !(pipe && pipe[0]);
    if (!m_use_slave) {
        printf ("%s | ", pipe);
        *m_process << pipe << " | ";
    }
    printf ("mplayer -wid %lu", (unsigned long) m_view->viewer ()->winId ());
    *m_process << "mplayer -wid " << QString::number (m_view->viewer ()->winId());

    QString strVideoDriver;

    switch( m_settings->videodriver ){
        case VDRIVER_XV_INDEX:
            strVideoDriver = VDRIVER_XV;
            break;
        case VDRIVER_X11_INDEX:
            strVideoDriver = VDRIVER_X11;
            strVideoDriver.truncate(3);
            break;
        case VDRIVER_XVIDIX_INDEX:
            strVideoDriver = VDRIVER_XVIDIX;
            break;
        default:		
            strVideoDriver = VDRIVER_XV;
            break;
    }
    printf (" -vo %s", strVideoDriver.lower().ascii());
    *m_process << " -vo " << strVideoDriver.lower().ascii();

    QString strAudioDriver;
    strAudioDriver = QString (m_settings->audiodrivers[m_settings->audiodriver].audiodriver);
    if (strAudioDriver != "") {
        printf (" -ao %s", strAudioDriver.lower().ascii());
        *m_process << " -ao " << strAudioDriver.lower().ascii();
    }
    if (m_settings->loop) {
        printf (" -loop 0");
        *m_process << " -loop 0";
    }
    if (m_settings->framedrop) {
        printf (" -framedrop");
        *m_process << " -framedrop";
    }

    /*if (!m_settings->audiodriver.contains("default", false)){
      printf (" -ao %s", m_settings->audiodriver.lower().latin1());
     *m_process << " -ao " << m_settings->audiodriver.lower().latin1();
     }*/
    if (m_settings->additionalarguments.length () > 0) {
        printf (" %s", m_settings->additionalarguments.ascii());
        *m_process << " " << m_settings->additionalarguments.ascii();
    }
    // postproc thingies

    printf (" %s", source ()->filterOptions ().ascii ());
    *m_process << " " << source ()->filterOptions ().ascii ();

    printf (" -contrast %d", m_settings->contrast);
    *m_process << " -contrast " << QString::number (m_settings->contrast);

    printf (" -brightness %d", m_settings->brightness);
    *m_process << " -brightness " << QString::number(m_settings->brightness);

    printf (" -hue %d", m_settings->hue);
    *m_process << " -hue " << QString::number (m_settings->hue);

    printf (" -saturation %d", m_settings->saturation);
    *m_process << " -saturation " << QString::number(m_settings->saturation);

    printf (" %s", args);
    *m_process << " " << args;

    QValueList<QCString>::const_iterator it;
    QString sMPArgs;
    for ( it = m_process->args().begin(); it != m_process->args().end(); ++it ){
        sMPArgs += (*it);
    }
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
    } else if (m_source) {
        m_source->play ();
        if (m_view->playButton ()->isOn () && !playing ())
            m_view->playButton ()->toggle ();
    }
}

bool KMPlayer::playing () const {
    return m_process->isRunning ();
}

void KMPlayer::stop () {
    kdDebug () << "KMPlayer::stop ()" << endl;
    if (m_process->isRunning ()) {
        if (m_view && !m_view->stopButton ()->isOn ())
            m_view->stopButton ()->toggle ();
        if (m_view)
            m_view->setCursor (QCursor (Qt::WaitCursor));
        do {
            if (m_recording) {
                m_process->kill (SIGINT);
                KProcessController::theKProcessController->waitForProcessExit (3);
                if (!m_process->isRunning ())
                    break;
                m_process->kill (SIGTERM);
            } else if (m_use_slave) {
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
        if (m_view)
            m_view->setCursor (QCursor (Qt::ArrowCursor));
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
    seekPercent (100.0 * m_view->positionSlider()->value() / m_view->positionSlider()->maxValue());
}

void KMPlayer::contrastValueChanged (int val) {
    m_settings->contrast = val;
    QString cmd;
    cmd.sprintf ("contrast %d 1", val);
    sendCommand (cmd);
}

void KMPlayer::brightnessValueChanged (int val) {
    m_settings->brightness = val;
    QString cmd;
    cmd.sprintf ("brightness %d 1", val);
    sendCommand (cmd);
}

void KMPlayer::hueValueChanged (int val) {
    m_settings->hue = val;
    QString cmd;
    cmd.sprintf ("hue %d 1", val);
    sendCommand (cmd);
}

void KMPlayer::saturationValueChanged (int val) {
    m_settings->saturation = val;
    QString cmd;
    cmd.sprintf ("saturation %d 1", val);
    sendCommand (cmd);
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

KMPlayerLiveConnectExtension::KMPlayerLiveConnectExtension (KMPlayer * parent)
  : KParts::LiveConnectExtension (parent), player (parent),
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
  (const unsigned long id, const QString & _name,
   KParts::LiveConnectExtension::Type & type, unsigned long & rid, QString &) {
    QString name = _name.lower ();
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
  (const unsigned long id, const QString & _name,
   const QStringList & args, KParts::LiveConnectExtension::Type & type,
   unsigned long & rid, QString &) {
    QString name = _name.lower ();
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
    : KMPlayerSource (player), m_url (url) {
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
    m_player->run (args.latin1 ());
    if (m_player->liveconnectextension ())
        m_player->liveconnectextension ()->enableFinishEvent ();
}

void KMPlayerURLSource::activate () {
    init ();
    bool loop = m_player->settings ()->loop;
    m_player->settings ()->loop = false;
    if (!url ().isEmpty ()) {
        QString args ("-quiet -nocache -identify -frames 0 ");
        QString myurl (url ().isLocalFile () ? url ().path () : m_url.url ());
        args += KProcess::quote (myurl);
        if (m_player->run (args.ascii ()))
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

bool KMPlayerHRefSource::processOutput (const QString & str) {
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
    if (m_finished) {
        QTimer::singleShot (0, this, SLOT (finished ()));
        return;
    }
    KMPlayerView * view = static_cast <KMPlayerView*> (m_player->view ());
    init ();
    m_player->stop ();
    m_player->initProcess ();
    view->consoleOutput ()->clear ();
    unlink (locateLocal ("data", "kmplayer/00000001.jpg").ascii ());
    QString outdir = locateLocal ("data", "kmplayer/");
    QString myurl (m_url.isLocalFile () ? m_url.path () : m_url.url ());
    QString args;
    args.sprintf ("mplayer -vo jpeg -jpeg outdir=%s -frames 1 -nosound -quiet %s", KProcess::quote (outdir).ascii (), KProcess::quote (myurl).ascii ());
    *m_player->process () << args;
    kdDebug () << args << endl;
    m_player->process ()->start (KProcess::NotifyOnExit, KProcess::NoCommunication);

    if (m_player->process ()->isRunning ())
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
