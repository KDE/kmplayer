/* This file is part of the KDE project
 *
 * Copyright (C) 2003 Koos Vriezen <koos.vriezen@xs4all.nl>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <qstring.h>

#include <qfile.h>
#include <qtimer.h>

#include <kprocess.h>
#include <kdebug.h>
#include <kprocctrl.h>
#include <kprotocolmanager.h>
#include <kfiledialog.h>
#include <kmessagebox.h>
#include <klocale.h>

#include "kmplayer_part.h"
#include "kmplayerprocess.h"
#include "kmplayersource.h"
#include "kmplayerconfig.h"
#include "kmplayer_callback.h"

KMPlayerProcess::KMPlayerProcess (KMPlayer * player)
    : m_player (player), m_source (0L), m_process (0L) {}

KMPlayerProcess::~KMPlayerProcess () {
    delete m_process;
}

void KMPlayerProcess::init () {
}

QWidget * KMPlayerProcess::widget () {
    return 0L;
}

bool KMPlayerProcess::playing () const {
    return m_process && m_process->isRunning ();
}

bool KMPlayerProcess::pause () {
    return false;
}

bool KMPlayerProcess::seek (int /*pos*/, int /*absolute*/) {
    return false;
}

bool KMPlayerProcess::volume (int /*pos*/, int /*absolute*/) {
    return false;
}

bool KMPlayerProcess::saturation (int /*pos*/, int /*absolute*/) {
    return false;
}

bool KMPlayerProcess::hue (int /*pos*/, int /*absolute*/) {
    return false;
}

bool KMPlayerProcess::contrast (int /*pos*/, int /*absolute*/) {
    return false;
}

bool KMPlayerProcess::brightness (int /*pos*/, int /*absolute*/) {
    return false;
}

//-----------------------------------------------------------------------------

static bool proxyForURL (const KURL& url, QString& proxy) {
    KProtocolManager::slaveProtocol (url, proxy);
    return !proxy.isNull ();
}

//-----------------------------------------------------------------------------

MPlayerBase::MPlayerBase (KMPlayer * player)
    : KMPlayerProcess (player), m_use_slave (true) {
}

MPlayerBase::~MPlayerBase () {
}

void MPlayerBase::initProcess () {
    delete m_process;
    m_process = new KProcess;
    m_process->setUseShell (true);
    KURL url = m_player->urlSource ()->url ();
    if (!url.isEmpty ()) {
        QString proxy_url;
        if (KProtocolManager::useProxy () && proxyForURL (url, proxy_url))
            m_process->setEnvironment("http_proxy", proxy_url);
    }
    connect (m_process, SIGNAL (wroteStdin (KProcess *)),
            this, SLOT (dataWritten (KProcess *)));
    connect (m_process, SIGNAL (processExited (KProcess *)),
            this, SLOT (processStopped (KProcess *)));
}

bool MPlayerBase::sendCommand (const QString & cmd) {
    if (m_process->isRunning () && m_use_slave) {
        commands.push_front (cmd + "\n");
        printf ("eval %s", commands.last ().latin1 ());
        m_process->writeStdin (QFile::encodeName(commands.last ()),
                               commands.last ().length ());
        return true;
    }
    return false;
}

void MPlayerBase::dataWritten (KProcess *) {
    if (!commands.size ()) return;
    kdDebug() << "eval done " << commands.last () << endl;
    commands.pop_back ();
    if (commands.size ())
        m_process->writeStdin (QFile::encodeName(commands.last ()),
                             commands.last ().length ());
}

void MPlayerBase::processStopped (KProcess *) {
    kdDebug() << "process stopped" << endl;
    commands.clear ();
    QTimer::singleShot (0, this, SLOT (emitFinished ()));
}


//-----------------------------------------------------------------------------

MPlayer::MPlayer (KMPlayer * player)
    : MPlayerBase (player), m_widget (0L) {
}

MPlayer::~MPlayer () {
    if (m_widget && !m_widget->parent ())
        delete m_widget;
}

void MPlayer::init () {
}

QWidget * MPlayer::widget () {
    return static_cast <KMPlayerView *> (m_player->view())->viewer();
}

bool MPlayer::play () {
    if (!source ()) return false;
    if (m_process->isRunning ())
        return sendCommand (QString ("gui_play"));
    m_source->play ();
    return m_process->isRunning ();
}

bool MPlayer::stop () {
    kdDebug () << "MPlayer::stop ()" << endl;
    if (!source ()) return false;
    if (!m_process || !m_process->isRunning ()) return true;
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
            KMessageBox::error (m_player->view (), i18n ("Failed to end MPlayer process."), i18n ("KMPlayer: Error"));
        }
    } while (false);
    return true;
}

bool MPlayer::pause () {
    return sendCommand (QString ("pause"));
}

bool MPlayer::seek (int pos, int absolute) {
    if (!m_source || !m_source->hasLength ()) return false;
    QString cmd;
    cmd.sprintf ("seek -%d %d", pos, absolute ? 2 : 0);
    return sendCommand (cmd);
}

bool MPlayer::volume (int incdec, int absolute) {
    if (!source ()) return false;
    if (!absolute)
        return sendCommand (QString ("volume ") + QString::number (incdec));
    return false;
}

bool MPlayer::saturation (int val, int absolute) {
    if (!source ()) return false;
    if (!m_widget) return false;
    QString cmd;
    cmd.sprintf ("saturation %d %d", val, absolute ? 1 : 0);
    return sendCommand (cmd);
}

bool MPlayer::hue (int val, int absolute) {
    if (!source ()) return false;
    if (!m_widget) return false;
    QString cmd;
    cmd.sprintf ("hue %d %d", val, absolute ? 1 : 0);
    return sendCommand (cmd);
}

bool MPlayer::contrast (int val, int /*absolute*/) {
    if (!source ()) return false;
    if (!m_widget) return false;
    QString cmd;
    cmd.sprintf ("contrast %d 1", val);
    return sendCommand (cmd);
}

bool MPlayer::brightness (int val, int /*absolute*/) {
    if (!source ()) return false;
    if (!m_widget) return false;
    QString cmd;
    cmd.sprintf ("brightness %d 1", val);
    return sendCommand (cmd);
}

bool MPlayer::run (const char * args, const char * pipe) {
    stop ();
    //m_view->consoleOutput ()->clear ();
    m_process_output = QString::null;
    //m_started_emited = false;
    initProcess ();
    connect (m_process, SIGNAL (receivedStdout (KProcess *, char *, int)),
            this, SLOT (processOutput (KProcess *, char *, int)));
    connect (m_process, SIGNAL (receivedStderr (KProcess *, char *, int)),
            this, SLOT (processOutput (KProcess *, char *, int)));
    KMPlayerSettings *settings = m_player->settings ();
    m_cacheRegExp.setPattern (settings->cachepattern);
    m_indexRegExp.setPattern (settings->indexpattern);
    m_posRegExp.setPattern (m_player->settings ()->positionpattern);

    m_use_slave = !(pipe && pipe[0]);
    if (!m_use_slave) {
        printf ("%s | ", pipe);
        *m_process << pipe << " | ";
    }
    printf ("mplayer -wid %lu", (unsigned long) widget ()->winId ());
    *m_process << "mplayer -wid " << QString::number (widget ()->winId ());

    QString strVideoDriver;

    switch( settings->videodriver ){
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
    strAudioDriver = QString (settings->audiodrivers[settings->audiodriver].audiodriver);
    if (strAudioDriver != "") {
        printf (" -ao %s", strAudioDriver.lower().ascii());
        *m_process << " -ao " << strAudioDriver.lower().ascii();
    }
    if (settings->loop) {
        printf (" -loop 0");
        *m_process << " -loop 0";
    }
    if (settings->framedrop) {
        printf (" -framedrop");
        *m_process << " -framedrop";
    }

    /*if (!settings->audiodriver.contains("default", false)){
      printf (" -ao %s", settings->audiodriver.lower().latin1());
     *m_process << " -ao " << settings->audiodriver.lower().latin1();
     }*/
    if (settings->additionalarguments.length () > 0) {
        printf (" %s", settings->additionalarguments.ascii());
        *m_process << " " << settings->additionalarguments.ascii();
    }
    // postproc thingies

    printf (" %s", source ()->filterOptions ().ascii ());
    *m_process << " " << source ()->filterOptions ().ascii ();

    printf (" -contrast %d", settings->contrast);
    *m_process << " -contrast " << QString::number (settings->contrast);

    printf (" -brightness %d", settings->brightness);
    *m_process << " -brightness " << QString::number(settings->brightness);

    printf (" -hue %d", settings->hue);
    *m_process << " -hue " << QString::number (settings->hue);

    printf (" -saturation %d", settings->saturation);
    *m_process << " -saturation " << QString::number(settings->saturation);

    printf (" %s", args);
    *m_process << " " << args;

    QValueList<QCString>::const_iterator it;
    QString sMPArgs;
    for ( it = m_process->args().begin(); it != m_process->args().end(); ++it ){
        sMPArgs += (*it);
    }
    //m_view->addText( sMPArgs.simplifyWhiteSpace() );

    m_process->start (KProcess::NotifyOnExit, KProcess::All);

    if (m_process->isRunning ()) {
        QTimer::singleShot (0, this, SLOT (emitStarted ()));
        return true;
    }
    QTimer::singleShot (0, this, SLOT (emitFinished ()));
    return false;
}

void MPlayer::processOutput (KProcess *, char * str, int slen) {
    if (!m_player->view () || slen <= 0) return;

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
                emit positionChanged (int(10.0 * m_posRegExp.cap(1).toFloat()));
            } else if (m_cacheRegExp.search (out) > -1) {
                emit loading (int (m_cacheRegExp.cap (1).toDouble ()));
            }
        } else {
            emit output (out);
            if (!m_source->processOutput (out)) {
                QRegExp sizeRegExp (m_player->settings ()->sizepattern);
                QRegExp startRegExp (m_player->settings ()->startpattern);
                bool ok;
                int movie_width = m_source->width ();
                if (movie_width <= 0 && sizeRegExp.search (out) > -1) {
                    movie_width = sizeRegExp.cap (1).toInt (&ok);
                    int movie_height = ok ? sizeRegExp.cap (2).toInt (&ok) : 0;
                    if (ok && movie_width > 0 && movie_height > 0) {
                        m_source->setWidth (movie_width);
                        m_source->setHeight (movie_height);
                    }
                } else if (startRegExp.search (out) > -1) {
                        emit startPlaying ();
                }
            }
        }
    } while (slen > 0);
}

//-----------------------------------------------------------------------------

MEncoder::MEncoder (KMPlayer * player)
    : MPlayerBase (player) {
}

MEncoder::~MEncoder () {
}

void MEncoder::init () {
}

bool MEncoder::play () {
    if (!m_source || m_source->recordCommand ().isEmpty ())
        return false;
    bool success = false;
    KFileDialog *dlg = new KFileDialog (QString::null, QString::null, m_player->view (), "", true);
    if (dlg->exec ()) {
        stop ();
        initProcess ();
        m_recordurl = dlg->selectedURL().url ();
        QString myurl = KProcess::quote (m_recordurl.isLocalFile () ? m_recordurl.path () : m_recordurl.url ());
        kdDebug () << m_source->recordCommand () << " -o " << myurl << endl;
        *m_process << m_source->recordCommand () << " -o " << myurl;
        m_process->start (KProcess::NotifyOnExit, KProcess::NoCommunication);
        success = m_process->isRunning ();
    }
    delete dlg;
    return success;
}

bool MEncoder::stop () {
    if (!m_process || !m_process->isRunning ())
        return true;
    do {
        m_process->kill (SIGINT);
        KProcessController::theKProcessController->waitForProcessExit (3);
        if (!m_process->isRunning ())
            break;
        m_process->kill (SIGTERM);
        KProcessController::theKProcessController->waitForProcessExit (1);
        if (!m_process->isRunning ())
            break;
        m_process->kill (SIGKILL);
        KProcessController::theKProcessController->waitForProcessExit (1);
        if (m_process->isRunning ()) {
            processStopped (0L); // give up
            KMessageBox::error (m_player->view (), i18n ("Failed to end MPlayer process."), i18n ("KMPlayer: Error"));
        }
    } while (false);
    return true;
}

//-----------------------------------------------------------------------------

static int callback_counter = 0;

KMPlayerCallback::KMPlayerCallback (KMPlayerCallbackProcess * process)
    : DCOPObject (QString (QString ("KMPlayerCallback-") +
                           QString::number (callback_counter++)).ascii ()),
      m_process (process) {}

void KMPlayerCallback::setURL (QString url) {
    m_process->setURL (url);
}

void KMPlayerCallback::statusMessage (QString msg) {
    m_process->setStatusMessage (msg);
}

void KMPlayerCallback::errorMessage (int code, QString msg) {
    m_process->setErrorMessage (code, msg);
}

void KMPlayerCallback::finished () {
    m_process->setFinished ();
}

void KMPlayerCallback::playing () {
    m_process->setPlaying ();
}

void KMPlayerCallback::started () {
    m_process->setStarted ();
}

void KMPlayerCallback::movieParams (int length, int w, int h, float aspect) {
    m_process->setMovieParams (length, w, h, aspect);
}

//-----------------------------------------------------------------------------

KMPlayerCallbackProcess::KMPlayerCallbackProcess (KMPlayer * player)
    : KMPlayerProcess (player), m_callback (new KMPlayerCallback (this)) {
}

KMPlayerCallbackProcess::~KMPlayerCallbackProcess () {
    delete m_callback;
}

void KMPlayerCallbackProcess::setURL (const QString & url) {
}

void KMPlayerCallbackProcess::setStatusMessage (const QString & msg) {
}

void KMPlayerCallbackProcess::setErrorMessage (int code, const QString & msg) {
}

void KMPlayerCallbackProcess::setFinished () {
}

void KMPlayerCallbackProcess::setPlaying () {
}

void KMPlayerCallbackProcess::setStarted () {
}

void KMPlayerCallbackProcess::setMovieParams (int len, int w, int h, float a) {
    m_source->setWidth (w);
    m_source->setHeight (h);
    m_source->setAspect (a);
    m_source->setLength (len);
}

//-----------------------------------------------------------------------------

FFMpeg::FFMpeg (KMPlayer * player)
    : KMPlayerProcess (player) {
}

FFMpeg::~FFMpeg () {
}

void FFMpeg::init () {
}

bool FFMpeg::play () {
    return true;
}

bool FFMpeg::stop () {
    return true;
}

void FFMpeg::processStopped (KProcess *) {
}

#include "kmplayerprocess.moc"
