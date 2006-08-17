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
 * the Free Software Foundation, Inc., 51 Franklin Steet, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */
#include <math.h>
#include <config.h>
#include <qstring.h>
#include <qfile.h>
#include <qfileinfo.h>
#include <qtimer.h>
#include <qlayout.h>
#include <qtable.h>
#include <qlineedit.h>
#include <qslider.h>
#include <qcombobox.h>
#include <qcheckbox.h>
#include <qspinbox.h>
#include <qlabel.h>
#include <qfontmetrics.h>
#include <qwhatsthis.h>

#include <dcopobject.h>
#include <dcopclient.h>
#include <kprocess.h>
#include <kdebug.h>
#include <kprocctrl.h>
#include <kprotocolmanager.h>
#include <kfiledialog.h>
#include <kmessagebox.h>
#include <klocale.h>
#include <kapplication.h>
#include <kstandarddirs.h>
#include <kio/job.h>

#include "kmplayerview.h"
#include "kmplayercontrolpanel.h"
#include "kmplayerprocess.h"
#include "kmplayersource.h"
#include "kmplayerconfig.h"
#include "kmplayer_callback.h"
#include "kmplayer_backend_stub.h"

using namespace KMPlayer;

static const char * default_supported [] = { 0L };

static QString getPath (const KURL & url) {
    QString p = KURL::decode_string (url.url ());
    if (p.startsWith (QString ("file:/"))) {
        p = p.mid (5);
        unsigned int i = 0;
        for (; i < p.length () && p[i] == QChar ('/'); ++i)
            ;
        //kdDebug () << "getPath " << p.mid (i-1) << endl;
        if (i > 0)
            return p.mid (i-1);
        return QString (QChar ('/') + p);
    }
    return p;
}

Process::Process (QObject * parent, Settings * settings, const char * n)
    : QObject (parent, n), m_source (0L), m_settings (settings),
      m_state (NotRunning), m_old_state (NotRunning), m_process (0L), m_job(0L),
      m_supported_sources (default_supported), m_viewer (0L) {}

Process::~Process () {
    stop ();
    delete m_process;
}

void Process::init () {
}

QString Process::menuName () const {
    return QString (className ());
}

void Process::initProcess (Viewer * viewer) {
    m_viewer = viewer;
    delete m_process;
    m_process = new KProcess;
    m_process->setUseShell (true);
    m_process->setEnvironment (QString::fromLatin1 ("SESSION_MANAGER"), QString::fromLatin1 (""));
    if (m_source) m_source->setPosition (0);
}

WId Process::widget () {
    return 0;
}

bool Process::playing () const {
    return m_process && m_process->isRunning ();
}

void Process::setAudioLang (int, const QString &) {}

void Process::setSubtitle (int, const QString &) {}

bool Process::pause () {
    return false;
}

bool Process::seek (int /*pos*/, bool /*absolute*/) {
    return false;
}

bool Process::volume (int /*pos*/, bool /*absolute*/) {
    return false;
}

bool Process::saturation (int /*pos*/, bool /*absolute*/) {
    return false;
}

bool Process::hue (int /*pos*/, bool /*absolute*/) {
    return false;
}

bool Process::contrast (int /*pos*/, bool /*absolute*/) {
    return false;
}

bool Process::brightness (int /*pos*/, bool /*absolute*/) {
    return false;
}

bool Process::grabPicture (const KURL & /*url*/, int /*pos*/) {
    return false;
}

bool Process::supports (const char * source) const {
    for (const char ** s = m_supported_sources; s[0]; ++s) {
        if (!strcmp (s[0], source))
            return true;
    }
    return false;
}

bool Process::stop () {
    if (!playing ()) return true;
    do {
        if (m_source && !m_source->pipeCmd ().isEmpty ()) {
            void (*oldhandler)(int) = signal(SIGTERM, SIG_IGN);
            ::kill (-1 * ::getpid (), SIGTERM);
            signal(SIGTERM, oldhandler);
        } else
            m_process->kill (SIGTERM);
        KProcessController::theKProcessController->waitForProcessExit (1);
        if (!m_process->isRunning ())
            break;
        m_process->kill (SIGKILL);
        KProcessController::theKProcessController->waitForProcessExit (1);
        if (m_process->isRunning ()) {
            KMessageBox::error (viewer (), i18n ("Failed to end player process."), i18n ("Error"));
        }
    } while (false);
    return !playing ();
}

bool Process::quit () {
    stop ();
    setState (NotRunning);
    return !playing ();
}

void Process::setState (State newstate) {
    if (m_state != newstate) {
        m_old_state = m_state;
        m_state = newstate;
        if (m_source)
            QTimer::singleShot (0, this, SLOT (rescheduledStateChanged ()));
    }
}

KDE_NO_EXPORT void Process::rescheduledStateChanged () {
    m_source->stateChange (this, m_old_state, m_state);
}

bool Process::play (Source * src, NodePtr _mrl) {
    m_source = src;
    m_mrl = _mrl;
    Mrl * m = _mrl ? _mrl->mrl () : 0L;
    QString url = m ? m->absolutePath () : QString ();
    bool changed = m_url != url;
    m_url = url;
#if KDE_IS_VERSION(3,3,91)
    if (!changed || KURL (m_url).isLocalFile ())
        return deMediafiedPlay ();
    m_url = url;
    m_job = KIO::stat (m_url, false);
    connect(m_job, SIGNAL (result(KIO::Job *)), this, SLOT(result(KIO::Job *)));
    return true;
#else
    return deMediafiedPlay ();
#endif
}

bool Process::deMediafiedPlay () {
    return false;
}

void Process::result (KIO::Job * job) {
#if KDE_IS_VERSION(3,3,91)
    KIO::UDSEntry entry = static_cast <KIO::StatJob *> (job)->statResult ();
    KIO::UDSEntry::iterator e = entry.end ();
    for (KIO::UDSEntry::iterator it = entry.begin (); it != e; ++it)
        if ((*it).m_uds == KIO::UDS_LOCAL_PATH) {
            m_url = KURL::fromPathOrURL ((*it).m_str).url ();
            break;
        }
    m_job = 0L;
    deMediafiedPlay ();
#endif
}

KDE_NO_EXPORT void Process::terminateJob () {
    if (m_job) {
        m_job->kill ();
        m_job = 0L;
    }
}

bool Process::ready (Viewer * viewer) {
    m_viewer = viewer;
    setState (Ready);
    return true;
}

Viewer * Process::viewer () const {
    return (m_viewer ? (Viewer*)m_viewer :
        (m_settings->defaultView() ? m_settings->defaultView()->viewer() : 0L));
}

//-----------------------------------------------------------------------------

static bool proxyForURL (const KURL& url, QString& proxy) {
    KProtocolManager::slaveProtocol (url, proxy);
    return !proxy.isNull ();
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT MPlayerBase::MPlayerBase (QObject * parent, Settings * settings, const char * n)
    : Process (parent, settings, n), m_use_slave (true) {
    m_process = new KProcess;
}

KDE_NO_CDTOR_EXPORT MPlayerBase::~MPlayerBase () {
}

KDE_NO_EXPORT void MPlayerBase::initProcess (Viewer * viewer) {
    Process::initProcess (viewer);
    const KURL & url (m_source->url ());
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

KDE_NO_EXPORT bool MPlayerBase::sendCommand (const QString & cmd) {
    if (playing () && m_use_slave) {
        commands.push_front (cmd + '\n');
        fprintf (stderr, "eval %s", commands.last ().latin1 ());
        if (commands.size () < 2)
            m_process->writeStdin (QFile::encodeName(commands.last ()),
                    commands.last ().length ());
        return true;
    }
    return false;
}

KDE_NO_EXPORT bool MPlayerBase::stop () {
    terminateJob ();
    if (!m_source || !m_process || !m_process->isRunning ()) return true;
    if (!m_use_slave) {
        void (*oldhandler)(int) = signal(SIGTERM, SIG_IGN);
        ::kill (-1 * ::getpid (), SIGTERM);
        signal(SIGTERM, oldhandler);
    }
#if KDE_IS_VERSION(3, 1, 90)
    m_process->wait(2);
#else
    QTime t;
    t.start ();
    do {
        KProcessController::theKProcessController->waitForProcessExit (2);
    } while (t.elapsed () < 2000 && m_process->isRunning ());
#endif
    if (m_process->isRunning ())
        Process::stop ();
    processStopped (0L);
    commands.clear ();
    return true;
}

KDE_NO_EXPORT bool MPlayerBase::quit () {
    if (playing ()) {
        disconnect (m_process, SIGNAL (processExited (KProcess *)),
                this, SLOT (processStopped (KProcess *)));
        stop ();
    }
    setState (NotRunning);
    return !playing ();
}

KDE_NO_EXPORT void MPlayerBase::dataWritten (KProcess *) {
    if (!commands.size ()) return;
    kdDebug() << "eval done " << commands.last () << endl;
    commands.pop_back ();
    if (commands.size ())
        m_process->writeStdin (QFile::encodeName(commands.last ()),
                             commands.last ().length ());
}

KDE_NO_EXPORT void MPlayerBase::processStopped (KProcess *) {
    kdDebug() << "process stopped" << endl;
    commands.clear ();
    setState (Ready);
}

//-----------------------------------------------------------------------------

static const char * mplayer_supports [] = {
    "dvdsource", "exitsource", "hrefsource", "introsource", "pipesource", "tvscanner", "tvsource", "urlsource", "vcdsource", "audiocdsource", 0L
};

KDE_NO_CDTOR_EXPORT MPlayer::MPlayer (QObject * parent, Settings * settings)
 : MPlayerBase (parent, settings, "mplayer"),
   m_widget (0L),
   m_configpage (new MPlayerPreferencesPage (this)),
   aid (-1), sid (-1),
   m_needs_restarted (false) {
       m_supported_sources = mplayer_supports;
    m_settings->addPage (m_configpage);
}

KDE_NO_CDTOR_EXPORT MPlayer::~MPlayer () {
    if (m_widget && !m_widget->parent ())
        delete m_widget;
    delete m_configpage;
}

KDE_NO_EXPORT void MPlayer::init () {
}

QString MPlayer::menuName () const {
    return i18n ("&MPlayer");
}

KDE_NO_EXPORT WId MPlayer::widget () {
    return viewer ()->embeddedWinId ();
}

KDE_NO_EXPORT bool MPlayer::deMediafiedPlay () {
    if (playing ())
        return sendCommand (QString ("gui_play"));
    if (!m_needs_restarted)
        stop ();
    initProcess (viewer ());
    m_source->setPosition (0);
    if (!m_needs_restarted) {
        aid = sid = -1;
    } else
        m_needs_restarted = false;
    alanglist = 0L;
    slanglist = 0L;
    m_request_seek = -1;
    QString args = m_source->options () + ' ';
    KURL url (m_url);
    if (!url.isEmpty ()) {
        if (m_source->url ().isLocalFile ())
            m_process->setWorkingDirectory
                (QFileInfo (m_source->url ().path ()).dirPath (true));
        if (url.isLocalFile ()) {
            m_url = getPath (url);
            if (m_configpage->alwaysbuildindex &&
                    (m_url.lower ().endsWith (".avi") ||
                     m_url.lower ().endsWith (".divx")))
                args += QString (" -idx ");
        } else {
            int cache = m_configpage->cachesize;
            if (cache > 3 && !url.url ().startsWith (QString ("dvd")) &&
                    !url.url ().startsWith (QString ("vcd")) &&
                    !url.url ().startsWith (QString ("tv://")))
                args += QString ("-cache %1 ").arg (cache);
            if (m_url.startsWith (QString ("cdda:/")) && 
                    !m_url.startsWith (QString ("cdda://")))
                m_url = QString ("cdda://") + m_url.mid (6);
        }
        if (url.protocol () != QString ("stdin"))
            args += KProcess::quote (QString (QFile::encodeName (m_url)));
    }
    m_tmpURL.truncate (0);
    if (!m_source->identified () && !m_settings->mplayerpost090) {
        args += QString (" -quiet -nocache -identify -frames 0 ");
    } else {
        if (m_settings->loop)
            args += QString (" -loop 0");
        if (m_settings->mplayerpost090)
            args += QString (" -identify");
        if (!m_source->subUrl ().isEmpty ()) {
            args += QString (" -sub ");
            const KURL & sub_url (m_source->subUrl ());
            if (!sub_url.isEmpty ()) {
                QString myurl (sub_url.isLocalFile () ? getPath (sub_url) : sub_url.url ());
                args += KProcess::quote (QString (QFile::encodeName (myurl)));
            }
        }
    }
    return run (args.ascii (), m_source->pipeCmd ().ascii ());
}

KDE_NO_EXPORT bool MPlayer::stop () {
    terminateJob ();
    if (!m_source || !m_process || !m_process->isRunning ()) return true;
    if (m_use_slave)
        sendCommand (QString ("quit"));
    return MPlayerBase::stop ();
}

KDE_NO_EXPORT bool MPlayer::pause () {
    return sendCommand (QString ("pause"));
}

KDE_NO_EXPORT bool MPlayer::seek (int pos, bool absolute) {
    if (!m_source || !m_source->hasLength () ||
            (absolute && m_source->position () == pos))
        return false;
    if (m_request_seek >= 0 && commands.size () > 1) {
        QStringList::iterator i = commands.begin ();
        QStringList::iterator end ( commands.end () );
        for (++i; i != end; ++i)
            if ((*i).startsWith (QString ("seek"))) {
                i = commands.erase (i);
                m_request_seek = -1;
                break;
            }
    }
    if (m_request_seek >= 0) {
        //m_request_seek = pos;
        return false;
    }
    m_request_seek = pos;
    QString cmd;
    cmd.sprintf ("seek %d %d", pos/10, absolute ? 2 : 0);
    if (!absolute)
        pos = m_source->position () + pos;
    m_source->setPosition (pos);
    return sendCommand (cmd);
}

KDE_NO_EXPORT bool MPlayer::volume (int incdec, bool absolute) {
    if (absolute)
        incdec -= old_volume;
    if (incdec == 0)
        return true;
    old_volume += incdec;
    return sendCommand (QString ("volume ") + QString::number (incdec));
}

KDE_NO_EXPORT bool MPlayer::saturation (int val, bool absolute) {
    QString cmd;
    cmd.sprintf ("saturation %d %d", val, absolute ? 1 : 0);
    return sendCommand (cmd);
}

KDE_NO_EXPORT bool MPlayer::hue (int val, bool absolute) {
    QString cmd;
    cmd.sprintf ("hue %d %d", val, absolute ? 1 : 0);
    return sendCommand (cmd);
}

KDE_NO_EXPORT bool MPlayer::contrast (int val, bool /*absolute*/) {
    QString cmd;
    cmd.sprintf ("contrast %d 1", val);
    return sendCommand (cmd);
}

KDE_NO_EXPORT bool MPlayer::brightness (int val, bool /*absolute*/) {
    QString cmd;
    cmd.sprintf ("brightness %d 1", val);
    return sendCommand (cmd);
}

bool MPlayer::run (const char * args, const char * pipe) {
    //m_view->consoleOutput ()->clear ();
    m_process_output = QString::null;
    connect (m_process, SIGNAL (receivedStdout (KProcess *, char *, int)),
            this, SLOT (processOutput (KProcess *, char *, int)));
    connect (m_process, SIGNAL (receivedStderr (KProcess *, char *, int)),
            this, SLOT (processOutput (KProcess *, char *, int)));
    m_use_slave = !(pipe && pipe[0]);
    if (!m_use_slave) {
        fprintf (stderr, "%s | ", pipe);
        *m_process << pipe << " | ";
    }
    fprintf (stderr, "mplayer -wid %lu ", (unsigned long) widget ());
    *m_process << "mplayer -wid " << QString::number (widget ());

    if (m_use_slave) {
        fprintf (stderr, "-slave ");
        *m_process << "-slave ";
    }

    QString strVideoDriver = QString (m_settings->videodrivers[m_settings->videodriver].driver);
    if (!strVideoDriver.isEmpty ()) {
        fprintf (stderr, " -vo %s", strVideoDriver.lower().ascii());
        *m_process << " -vo " << strVideoDriver.lower();
        if (viewer ()->view ()->keepSizeRatio () &&
                strVideoDriver.lower() == QString::fromLatin1 ("x11")) {
            fprintf (stderr, " -zoom");
            *m_process << " -zoom";
        }
    }
    QString strAudioDriver = QString (m_settings->audiodrivers[m_settings->audiodriver].driver);
    if (!strAudioDriver.isEmpty ()) {
        fprintf (stderr, " -ao %s", strAudioDriver.lower().ascii());
        *m_process << " -ao " << strAudioDriver.lower();
    }
    if (m_settings->framedrop) {
        fprintf (stderr, " -framedrop");
        *m_process << " -framedrop";
    }

    if (m_configpage->additionalarguments.length () > 0) {
        fprintf (stderr, " %s", m_configpage->additionalarguments.ascii());
        *m_process << " " << m_configpage->additionalarguments;
    }
    // postproc thingies

    fprintf (stderr, " %s", m_source->filterOptions ().ascii ());
    *m_process << " " << m_source->filterOptions ();

    if (m_settings->autoadjustcolors) {
        fprintf (stderr, " -contrast %d", m_settings->contrast);
        *m_process << " -contrast " << QString::number (m_settings->contrast);

        fprintf (stderr, " -brightness %d", m_settings->brightness);
        *m_process << " -brightness " <<QString::number(m_settings->brightness);

        fprintf (stderr, " -hue %d", m_settings->hue);
        *m_process << " -hue " << QString::number (m_settings->hue);

        fprintf (stderr, " -saturation %d", m_settings->saturation);
        *m_process << " -saturation " <<QString::number(m_settings->saturation);
    }
    if (aid > -1) {
        fprintf (stderr, " -aid %d", aid);
        *m_process << " -aid " << QString::number (aid);
    }

    if (sid > -1) {
        fprintf (stderr, " -sid %d", sid);
        *m_process << " -sid " << QString::number (sid);
    }

    fprintf (stderr, " %s\n", args);
    *m_process << " " << args;

    QValueList<QCString>::const_iterator it;
    QString sMPArgs;
    QValueList<QCString>::const_iterator end( m_process->args().end() );
    for ( it = m_process->args().begin(); it != end; ++it ){
        sMPArgs += (*it);
    }

    m_process->start (KProcess::NotifyOnExit, KProcess::All);

    old_volume = viewer ()->view ()->controlPanel ()->volumeBar ()->value ();

    if (m_process->isRunning ()) {
        setState (Buffering); // wait for start regexp for state Playing
        return true;
    }
    return false;
}

KDE_NO_EXPORT bool MPlayer::grabPicture (const KURL & url, int pos) {
    stop ();
    initProcess (viewer ());
    QString outdir = locateLocal ("data", "kmplayer/");
    m_grabfile = outdir + QString ("00000001.jpg");
    unlink (m_grabfile.ascii ());
    QString myurl (url.isLocalFile () ? getPath (url) : url.url ());
    QString args ("mplayer ");
    if (m_settings->mplayerpost090)
        args += "-vo jpeg:outdir=";
    else
        args += "-vo jpeg -jpeg outdir=";
    args += KProcess::quote (outdir);
    args += QString (" -frames 1 -nosound -quiet ");
    if (pos > 0)
        args += QString ("-ss %1 ").arg (pos);
    args += KProcess::quote (QString (QFile::encodeName (myurl)));
    *m_process << args;
    kdDebug () << args << endl;
    m_process->start (KProcess::NotifyOnExit, KProcess::NoCommunication);
    return m_process->isRunning ();
}

KDE_NO_EXPORT void MPlayer::processOutput (KProcess *, char * str, int slen) {
    if (!viewer () || slen <= 0) return;
    View * v = viewer ()->view ();

    bool ok;
    QRegExp * patterns = m_configpage->m_patterns;
    QRegExp & m_refURLRegExp = patterns[MPlayerPreferencesPage::pat_refurl];
    QRegExp & m_refRegExp = patterns[MPlayerPreferencesPage::pat_ref];
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
            QRegExp & m_posRegExp = patterns[MPlayerPreferencesPage::pat_pos];
            QRegExp & m_cacheRegExp = patterns[MPlayerPreferencesPage::pat_cache];
            if (m_source->hasLength () && m_posRegExp.search (out) > -1) {
                int pos = int (10.0 * m_posRegExp.cap (1).toFloat ());
                m_source->setPosition (pos);
                m_request_seek = -1;
            } else if (m_cacheRegExp.search (out) > -1) {
                m_source->setLoading (int (m_cacheRegExp.cap(1).toDouble()));
            }
        } else if (!m_source->identified () && out.startsWith ("ID_LENGTH")) {
            int pos = out.find ('=');
            if (pos > 0) {
                int l = (int) out.mid (pos + 1).toDouble (&ok);
                if (ok && l >= 0) {
                    m_source->setLength (m_mrl, 10 * l);
                }
            }
        } else if (!m_source->identified() && m_refURLRegExp.search(out) > -1) {
            kdDebug () << "Reference mrl " << m_refURLRegExp.cap (1) << endl;
            if (!m_tmpURL.isEmpty () && m_url != m_tmpURL)
                m_source->insertURL (m_mrl, m_tmpURL);;
            m_tmpURL = KURL::fromPathOrURL (m_refURLRegExp.cap (1)).url ();
            if (m_source->url () == m_tmpURL || m_url == m_tmpURL)
                m_tmpURL.truncate (0);
        } else if (!m_source->identified () && m_refRegExp.search (out) > -1) {
            kdDebug () << "Reference File " << endl;
            m_tmpURL.truncate (0);
        } else if (out.startsWith ("ID_VIDEO_WIDTH")) {
            int pos = out.find ('=');
            if (pos > 0) {
                int w = out.mid (pos + 1).toInt ();
                m_source->setDimensions (m_mrl, w, m_source->height ());
            }
        } else if (out.startsWith ("ID_VIDEO_HEIGHT")) {
            int pos = out.find ('=');
            if (pos > 0) {
                int h = out.mid (pos + 1).toInt ();
                m_source->setDimensions (m_mrl, m_source->width (), h);
            }
        } else if (out.startsWith ("ID_VIDEO_ASPECT")) {
            int pos = out.find ('=');
            if (pos > 0) {
                bool ok;
                QString val = out.mid (pos + 1);
                float a = val.toFloat (&ok);
                if (!ok) {
                    val.replace (',', '.');
                    a = val.toFloat (&ok);
                }
                if (ok && a > 0.001)
                    m_source->setAspect (m_mrl, a);
            }
        } else if (out.startsWith ("ID_AID_")) {
            int pos = out.find ('_', 7);
            if (pos > 0) {
                int id = out.mid (7, pos - 7).toInt ();
                pos = out.find ('=', pos);
                if (pos > 0) {
                    if (!alanglist_end) {
                        alanglist = new LangInfo (id, out.mid (pos + 1));
                        alanglist_end = alanglist;
                    } else {
                        alanglist_end->next = new LangInfo (id, out.mid(pos+1));
                        alanglist_end = alanglist_end->next;
                    }
                    kdDebug () << "lang " << id << " " << alanglist_end->name <<endl;
                }
            }
        } else if (out.startsWith ("ID_SID_")) {
            int pos = out.find ('_', 7);
            if (pos > 0) {
                int id = out.mid (7, pos - 7).toInt ();
                pos = out.find ('=', pos);
                if (pos > 0) {
                    if (!slanglist_end) {
                        slanglist = new LangInfo (id, out.mid (pos + 1));
                        slanglist_end = slanglist;
                    } else {
                        slanglist_end->next = new LangInfo (id, out.mid(pos+1));
                        slanglist_end = slanglist_end->next;
                    }
                    kdDebug () << "sid " << id << " " << slanglist_end->name <<endl;
                }
            }
        } else {
            QRegExp & m_startRegExp = patterns[MPlayerPreferencesPage::pat_start];
            QRegExp & m_sizeRegExp = patterns[MPlayerPreferencesPage::pat_size];
            v->addText (out, true);
            if (!m_source->processOutput (out)) {
                // int movie_width = m_source->width ();
                if (/*movie_width <= 0 &&*/ m_sizeRegExp.search (out) > -1) {
                    int movie_width = m_sizeRegExp.cap (1).toInt (&ok);
                    int movie_height = ok ? m_sizeRegExp.cap (2).toInt (&ok) : 0;
                    if (ok && movie_width > 0 && movie_height > 0) {
                        m_source->setDimensions(m_mrl,movie_width,movie_height);
                        m_source->setAspect (m_mrl, 1.0*movie_width/movie_height);
                    }
                } else if (m_startRegExp.search (out) > -1) {
                    if (m_settings->mplayerpost090) {
                        if (!m_tmpURL.isEmpty () && m_url != m_tmpURL) {
                            m_source->insertURL (m_mrl, m_tmpURL);;
                            m_tmpURL.truncate (0);
                        }
                        m_source->setIdentified ();
                    }
                    QStringList alst, slst;
                    for (SharedPtr <LangInfo> li = alanglist; li; li = li->next)
                        alst.push_back (li->name);
                    for (SharedPtr <LangInfo> li = slanglist; li; li = li->next)
                        slst.push_back (li->name);
                    m_source->setLanguages (alst, slst);
                    setState (Playing);
                }
            }
        }
    } while (slen > 0);
}

KDE_NO_EXPORT void MPlayer::processStopped (KProcess * p) {
    if (p && !m_grabfile.isEmpty ()) {
        emit grabReady (m_grabfile);
        m_grabfile.truncate (0);
    } else if (p) {
        QString url;
        if (!m_source->identified ()) {
            m_source->setIdentified ();
            if (!m_tmpURL.isEmpty () && m_url != m_tmpURL) {
                m_source->insertURL (m_mrl, m_tmpURL);;
                m_tmpURL.truncate (0);
            }
        }
        if (m_source && m_needs_restarted) {
            commands.clear ();
            int pos = m_source->position ();
            play (m_source, m_mrl);
            seek (pos, true);
        } else
            MPlayerBase::processStopped (p);
    }
}

void MPlayer::setAudioLang (int id, const QString &) {
    SharedPtr <LangInfo> li = alanglist;
    for (; id > 0 && li; li = li->next)
        id--;
    if (li)
        aid = li->id;
    m_needs_restarted = true;
    sendCommand (QString ("quit"));
}

void MPlayer::setSubtitle (int id, const QString &) {
    SharedPtr <LangInfo> li = slanglist;
    for (; id > 0 && li; li = li->next)
        id--;
    if (li)
        sid = li->id;
    m_needs_restarted = true;
    sendCommand (QString ("quit"));
}

//-----------------------------------------------------------------------------

extern const char * strMPlayerGroup;
static const char * strMPlayerPatternGroup = "MPlayer Output Matching";
static const char * strAddArgs = "Additional Arguments";
static const char * strCacheSize = "Cache Size for Streaming";
static const char * strAlwaysBuildIndex = "Always build index";
static const int non_patterns = 3;

static struct MPlayerPattern {
    QString caption;
    const char * name;
    const char * pattern;
} _mplayer_patterns [] = {
    { i18n ("Size pattern"), "Movie Size", "VO:.*[^0-9]([0-9]+)x([0-9]+)" },
    { i18n ("Cache pattern"), "Cache Fill", "Cache fill:[^0-9]*([0-9\\.]+)%" },
    { i18n ("Position pattern"), "Movie Position", "V:\\s*([0-9\\.]+)" },
    { i18n ("Index pattern"), "Index Pattern", "Generating Index: +([0-9]+)%" },
    { i18n ("Reference URL pattern"), "Reference URL Pattern", "Playing\\s+(.*[^\\.])\\.?\\s*$" },
    { i18n ("Reference pattern"), "Reference Pattern", "Reference Media file" },
    { i18n ("Start pattern"), "Start Playing", "Start[^ ]* play" },
    { i18n ("DVD language pattern"), "DVD Language", "\\[open].*audio.*language: ([A-Za-z]+).*aid.*[^0-9]([0-9]+)" },
    { i18n ("DVD subtitle pattern"), "DVD Sub Title", "\\[open].*subtitle.*[^0-9]([0-9]+).*language: ([A-Za-z]+)" },
    { i18n ("DVD titles pattern"), "DVD Titles", "There are ([0-9]+) titles" },
    { i18n ("DVD chapters pattern"), "DVD Chapters", "There are ([0-9]+) chapters" },
    { i18n ("VCD track pattern"), "VCD Tracks", "track ([0-9]+):" },
    { i18n ("Audio CD tracks pattern"), "CDROM Tracks", "Audio CD[^0-9]+([0-9]+)[^0-9]tracks" }
};

namespace KMPlayer {

class KMPLAYER_NO_EXPORT MPlayerPreferencesFrame : public QFrame {
public:
    MPlayerPreferencesFrame (QWidget * parent);
    QTable * table;
};

} // namespace

KDE_NO_CDTOR_EXPORT MPlayerPreferencesFrame::MPlayerPreferencesFrame (QWidget * parent)
 : QFrame (parent) {
    QVBoxLayout * layout = new QVBoxLayout (this);
    table = new QTable (int (MPlayerPreferencesPage::pat_last)+non_patterns, 2, this);
    table->verticalHeader ()->hide ();
    table->setLeftMargin (0);
    table->horizontalHeader ()->hide ();
    table->setTopMargin (0);
    table->setColumnReadOnly (0, true);
    table->setText (0, 0, i18n ("Additional command line arguments:"));
    table->setText (1, 0, QString("%1 (%2)").arg (i18n ("Cache size:")).arg (i18n ("kB"))); // FIXME for new translations
    table->setCellWidget (1, 1, new QSpinBox (0, 32767, 32, table->viewport()));
    table->setText (2, 0, i18n ("Build new index when possible"));
    table->setCellWidget (2, 1, new QCheckBox (table->viewport()));
    QWhatsThis::add (table->cellWidget (2, 1), i18n ("Allows seeking in indexed files (AVIs)"));
    for (int i = 0; i < int (MPlayerPreferencesPage::pat_last); i++)
        table->setText (i+non_patterns, 0, _mplayer_patterns[i].caption);
    QFontMetrics metrics (table->font ());
    int first_column_width = 50;
    for (int i = 0; i < int (MPlayerPreferencesPage::pat_last+non_patterns); i++) {
        int strwidth = metrics.boundingRect (table->text (i, 0)).width ();
        if (strwidth > first_column_width)
            first_column_width = strwidth + 4;
    }
    table->setColumnWidth (0, first_column_width);
    table->setColumnStretchable (1, true);
    layout->addWidget (table);
}

KDE_NO_CDTOR_EXPORT MPlayerPreferencesPage::MPlayerPreferencesPage (MPlayer * p)
 : m_process (p), m_configframe (0L) {
}

KDE_NO_EXPORT void MPlayerPreferencesPage::write (KConfig * config) {
    config->setGroup (strMPlayerPatternGroup);
    for (int i = 0; i < int (pat_last); i++)
        config->writeEntry
            (_mplayer_patterns[i].name, m_patterns[i].pattern ());
    config->setGroup (strMPlayerGroup);
    config->writeEntry (strAddArgs, additionalarguments);
    config->writeEntry (strCacheSize, cachesize);
    config->writeEntry (strAlwaysBuildIndex, alwaysbuildindex);
}

KDE_NO_EXPORT void MPlayerPreferencesPage::read (KConfig * config) {
    config->setGroup (strMPlayerPatternGroup);
    for (int i = 0; i < int (pat_last); i++)
        m_patterns[i].setPattern (config->readEntry
                (_mplayer_patterns[i].name, _mplayer_patterns[i].pattern));
    config->setGroup (strMPlayerGroup);
    additionalarguments = config->readEntry (strAddArgs);
    cachesize = config->readNumEntry (strCacheSize, 384);
    alwaysbuildindex = config->readBoolEntry (strAlwaysBuildIndex, false);
}

KDE_NO_EXPORT void MPlayerPreferencesPage::sync (bool fromUI) {
    QTable * table = m_configframe->table;
    QSpinBox * cacheSize = static_cast<QSpinBox *>(table->cellWidget (1, 1));
    QCheckBox * buildIndex = static_cast<QCheckBox *>(table->cellWidget (2, 1));
    if (fromUI) {
        additionalarguments = table->text (0, 1);
        for (int i = 0; i < int (pat_last); i++)
            m_patterns[i].setPattern (table->text (i+non_patterns, 1));
        cachesize = cacheSize->value();
        alwaysbuildindex = buildIndex->isChecked ();
    } else {
        table->setText (0, 1, additionalarguments);
        for (int i = 0; i < int (pat_last); i++)
            table->setText (i+non_patterns, 1, m_patterns[i].pattern ());
        if (cachesize > 0)
            cacheSize->setValue(cachesize);
        buildIndex->setChecked (alwaysbuildindex);
    }
}

KDE_NO_EXPORT void MPlayerPreferencesPage::prefLocation (QString & item, QString & icon, QString & tab) {
    item = i18n ("General Options");
    icon = QString ("kmplayer");
    tab = i18n ("MPlayer");
}

KDE_NO_EXPORT QFrame * MPlayerPreferencesPage::prefPage (QWidget * parent) {
    m_configframe = new MPlayerPreferencesFrame (parent);
    return m_configframe;
}

//-----------------------------------------------------------------------------

static const char * mencoder_supports [] = {
    "dvdsource", "pipesource", "tvscanner", "tvsource", "urlsource", "vcdsource", "audiocdsource", 0L
};

KDE_NO_CDTOR_EXPORT MEncoder::MEncoder (QObject * parent, Settings * settings)
 : MPlayerBase (parent, settings, "mencoder") {
    m_supported_sources = mencoder_supports;
}

KDE_NO_CDTOR_EXPORT MEncoder::~MEncoder () {
}

KDE_NO_EXPORT void MEncoder::init () {
}

bool MEncoder::deMediafiedPlay () {
    bool success = false;
    stop ();
    initProcess (viewer ());
    KURL url (m_url);
    m_source->setPosition (0);
    QString args;
    m_use_slave = m_source->pipeCmd ().isEmpty ();
    if (!m_use_slave)
        args = m_source->pipeCmd () + QString (" | ");
    QString margs = m_settings->mencoderarguments;
    if (m_settings->recordcopy)
        margs = QString ("-oac copy -ovc copy");
    args += QString ("mencoder ") + margs + ' ' + m_source->recordCmd ();
    // FIXME if (m_player->source () == source) // ugly
    //    m_player->stop ();
    QString myurl = url.isLocalFile () ? getPath (url) : url.url ();
    bool post090 = m_settings->mplayerpost090;
    if (!myurl.isEmpty ()) {
        if (!post090 && myurl.startsWith (QString ("tv://")))
            ; // skip it
        else if (!post090 && myurl.startsWith (QString ("vcd://")))
            args += myurl.replace (0, 6, QString (" -vcd "));
        else if (!post090 && myurl.startsWith (QString ("dvd://")))
            args += myurl.replace (0, 6, QString (" -dvd "));
        else
            args += ' ' + KProcess::quote (QString (QFile::encodeName (myurl)));
    }
    QString outurl = KProcess::quote (QString (QFile::encodeName (m_recordurl.isLocalFile () ? getPath (m_recordurl) : m_recordurl.url ())));
    kdDebug () << args << " -o " << outurl << endl;
    *m_process << args << " -o " << outurl;
    m_process->start (KProcess::NotifyOnExit, KProcess::NoCommunication);
    success = m_process->isRunning ();
    if (success)
        setState (Playing);
    return success;
}

KDE_NO_EXPORT bool MEncoder::stop () {
    terminateJob ();
    if (!m_source || !m_process || !m_process->isRunning ()) return true;
    kdDebug () << "MEncoder::stop ()" << endl;
    if (m_use_slave)
        m_process->kill (SIGINT);
    return MPlayerBase::stop ();
}

//-----------------------------------------------------------------------------

static const char * mplayerdump_supports [] = {
    "dvdsource", "pipesource", "tvscanner", "tvsource", "urlsource", "vcdsource", "audiocdsource", 0L
};

KDE_NO_CDTOR_EXPORT
MPlayerDumpstream::MPlayerDumpstream (QObject * parent, Settings * settings)
    : MPlayerBase (parent, settings, "mplayerdumpstream") {
    m_supported_sources = mplayerdump_supports;
    }

KDE_NO_CDTOR_EXPORT MPlayerDumpstream::~MPlayerDumpstream () {
}

KDE_NO_EXPORT void MPlayerDumpstream::init () {
}

bool MPlayerDumpstream::deMediafiedPlay () {
    bool success = false;
    stop ();
    initProcess (viewer ());
    KURL url (m_url);
    m_source->setPosition (0);
    QString args;
    m_use_slave = m_source->pipeCmd ().isEmpty ();
    if (!m_use_slave)
        args = m_source->pipeCmd () + QString (" | ");
    args += QString ("mplayer ") + m_source->recordCmd ();
    // FIXME if (m_player->source () == source) // ugly
    //    m_player->stop ();
    QString myurl = url.isLocalFile () ? getPath (url) : url.url ();
    bool post090 = m_settings->mplayerpost090;
    if (!myurl.isEmpty ()) {
        if (!post090 && myurl.startsWith (QString ("tv://")))
            ; // skip it
        else if (!post090 && myurl.startsWith (QString ("vcd://")))
            args += myurl.replace (0, 6, QString (" -vcd "));
        else if (!post090 && myurl.startsWith (QString ("dvd://")))
            args += myurl.replace (0, 6, QString (" -dvd "));
        else
            args += ' ' + KProcess::quote (QString (QFile::encodeName (myurl)));
    }
    QString outurl = KProcess::quote (QString (QFile::encodeName (m_recordurl.isLocalFile () ? getPath (m_recordurl) : m_recordurl.url ())));
    kdDebug () << args << " -dumpstream -dumpfile " << outurl << endl;
    *m_process << args << " -dumpstream -dumpfile " << outurl;
    m_process->start (KProcess::NotifyOnExit, KProcess::NoCommunication);
    success = m_process->isRunning ();
    if (success)
        setState (Playing);
    return success;
}

KDE_NO_EXPORT bool MPlayerDumpstream::stop () {
    terminateJob ();
    if (!m_source || !m_process || !m_process->isRunning ()) return true;
    kdDebug () << "MPlayerDumpstream::stop ()" << endl;
    if (m_use_slave)
        m_process->kill (SIGINT);
    return MPlayerBase::stop ();
}

//-----------------------------------------------------------------------------

static int callback_counter = 0;

Callback::Callback (CallbackProcess * process)
    : DCOPObject (QString (QString ("KMPlayerCallback-") +
                           QString::number (callback_counter++)).ascii ()),
      m_process (process) {}

void Callback::statusMessage (int code, QString msg) {
    if (!m_process->m_source) return;
    switch ((StatusCode) code) {
        case stat_newtitle:
            //m_process->source ()->setTitle (msg);
            if (m_process->viewer ())
                ((PlayListNotify *) m_process->source ())->setInfoMessage (msg);
            break;
        case stat_hasvideo:
            if (m_process->viewer ())
                m_process->viewer ()->view ()->videoStart ();
            break;
        default:
            m_process->setStatusMessage (msg);
    };
}

void Callback::subMrl (QString mrl, QString title) {
    if (!m_process->m_source) return;
    m_process->m_source->insertURL (m_process->m_mrl, KURL::fromPathOrURL (mrl).url (), title);
    if (m_process->m_mrl && m_process->m_mrl->active ())
        m_process->m_mrl->defer (); // Xine detected this is a playlist
}

void Callback::errorMessage (int code, QString msg) {
    m_process->setErrorMessage (code, msg);
}

void Callback::finished () {
    m_process->setFinished ();
}

void Callback::playing () {
    m_process->setPlaying ();
}

void Callback::started (QCString dcopname, QByteArray data) {
    m_process->setStarted (dcopname, data);
}

void Callback::movieParams (int length, int w, int h, float aspect, QStringList alang, QStringList slang) {
    m_process->setMovieParams (length, w, h, aspect, alang, slang);
}

void Callback::moviePosition (int position) {
    m_process->setMoviePosition (position);
}

void Callback::loadingProgress (int percentage) {
    m_process->setLoadingProgress (percentage);
}

void Callback::toggleFullScreen () {
    Viewer * v = m_process->viewer ();
    if (v)
        v->view ()->fullScreen ();
}

//-----------------------------------------------------------------------------

CallbackProcess::CallbackProcess (QObject * parent, Settings * settings, const char * n, const QString & menuname)
 : Process (parent, settings, n),
   m_callback (new Callback (this)),
   m_backend (0L),
   m_menuname (menuname),
   m_configpage (new XMLPreferencesPage (this)),
   in_gui_update (false),
   m_have_config (config_unknown),
   m_send_config (send_no) {
}

CallbackProcess::~CallbackProcess () {
    delete m_callback;
    delete m_configpage;
    if (configdoc)
        configdoc->document()->dispose ();
}

void CallbackProcess::setStatusMessage (const QString & /*msg*/) {
}

QString CallbackProcess::menuName () const {
    return m_menuname;
}

void CallbackProcess::setErrorMessage (int code, const QString & msg) {
    kdDebug () << "setErrorMessage " << code << " " << msg << endl;
    if (code == 0 && m_send_config != send_no) {
        if (m_send_config == send_new)
            stop ();
        m_send_config = send_no;
    }
}

void CallbackProcess::setFinished () {
    setState (Ready);
}

void CallbackProcess::setPlaying () {
    setState (Playing);
}

void CallbackProcess::setStarted (QCString dcopname, QByteArray & data) {
    if (data.size ())
        m_configdata = data;
    kdDebug () << "up and running " << dcopname << endl;
    m_backend = new Backend_stub (dcopname, "Backend");
    if (m_send_config == send_new) {
        m_backend->setConfig (m_changeddata);
    }
    if (m_have_config == config_probe || m_have_config == config_unknown) {
        bool was_probe = m_have_config == config_probe;
        m_have_config = data.size () ? config_yes : config_no;
        if (m_have_config == config_yes) {
            configdoc = new ConfigDocument ();
            QTextStream ts (data, IO_ReadOnly);
            readXML (configdoc, ts, QString::null);
            configdoc->normalize ();
            //kdDebug () << mydoc->innerText () << endl;
        }
        emit configReceived ();
        if (m_configpage)
            m_configpage->sync (false);
        if (was_probe) {
            quit ();
            return;
        }
    }
    if (m_settings->autoadjustcolors) {
        saturation (m_settings->saturation, true);
        hue (m_settings->hue, true);
        brightness (m_settings->brightness, true);
        contrast (m_settings->contrast, true);
    }
    setState (Ready);
}

void CallbackProcess::setMovieParams (int len, int w, int h, float a, const QStringList & alang, const QStringList & slang) {
    kdDebug () << "setMovieParams " << len << " " << w << "," << h << " " << a << endl;
    if (!m_source) return;
    in_gui_update = true;
    m_source->setDimensions (m_mrl, w, h);
    m_source->setAspect (m_mrl, a);
    m_source->setLength (m_mrl, len);
    m_source->setLanguages (alang, slang);
    in_gui_update = false;
}

void CallbackProcess::setMoviePosition (int position) {
    if (!m_source) return;
    in_gui_update = true;
    m_source->setPosition (position);
    m_request_seek = -1;
    in_gui_update = false;
}

void CallbackProcess::setLoadingProgress (int percentage) {
    in_gui_update = true;
    m_source->setLoading (percentage);
    in_gui_update = false;
}

bool CallbackProcess::getConfigData () {
    if (m_have_config == config_no)
        return false;
    if (m_have_config == config_unknown && !playing ()) {
        m_have_config = config_probe;
        ready (viewer ());
    }
    return true;
}

void CallbackProcess::setChangedData (const QByteArray & data) {
    m_changeddata = data;
    m_send_config = playing () ? send_try : send_new;
    if (m_send_config == send_try)
        m_backend->setConfig (data);
    else
        ready (viewer ());
}

bool CallbackProcess::deMediafiedPlay () {
    if (!m_backend)
        return false;
    kdDebug () << "CallbackProcess::play " << m_url << endl;
    KURL url (m_url);
    QString myurl = url.isLocalFile () ? getPath (url) : url.url ();
    m_backend->setURL (myurl);
    const KURL & sub_url = m_source->subUrl ();
    if (!sub_url.isEmpty ())
        m_backend->setSubTitleURL (QString (QFile::encodeName (sub_url.isLocalFile () ? QFileInfo (getPath (sub_url)).absFilePath () : sub_url.url ())));
    if (m_source->frequency () > 0)
        m_backend->frequency (m_source->frequency ());
    m_backend->play ();
    setState (Buffering);
    return true;
}

bool CallbackProcess::stop () {
    terminateJob ();
    if (!m_process || !m_process->isRunning () || m_state < Buffering)
        return true;
    kdDebug () << "CallbackProcess::stop ()" << m_backend << endl;
    if (m_backend)
        m_backend->stop ();
    return true;
}

bool CallbackProcess::quit () {
    if (m_have_config == config_probe)
        m_have_config = config_unknown; // hmm
    if (m_send_config == send_new)
        m_send_config = send_no; // oh well
    if (playing ()) {
        kdDebug () << "CallbackProcess::quit ()" << endl;
        if (m_backend)
            m_backend->quit ();
        else if (viewer ())
            viewer ()->sendKeyEvent ('q');
#if KDE_IS_VERSION(3, 1, 90)
        m_process->wait(1);
#else
        QTime t;
        t.start ();
        do {
            KProcessController::theKProcessController->waitForProcessExit (2);
        } while (t.elapsed () < 1000 && m_process->isRunning ());
#endif
    }
    return Process::quit ();
}

bool CallbackProcess::pause () {
    if (!playing () || !m_backend) return false;
    m_backend->pause ();
    return true;
}

void CallbackProcess::setAudioLang (int id, const QString & al) {
    if (!m_backend) return;
    m_backend->setAudioLang (id, al);
}

void CallbackProcess::setSubtitle (int id, const QString & sl) {
    if (!m_backend) return;
    m_backend->setSubtitle (id, sl);
}

bool CallbackProcess::seek (int pos, bool absolute) {
    if (in_gui_update || !playing () ||
            !m_backend || !m_source ||
            !m_source->hasLength () ||
            (absolute && m_source->position () == pos))
        return false;
    if (!absolute)
        pos = m_source->position () + pos;
    m_source->setPosition (pos);
    if (m_request_seek < 0)
        m_backend->seek (pos, true);
    m_request_seek = pos;
    return true;
}

bool CallbackProcess::volume (int val, bool b) {
    if (m_backend)
        m_backend->volume (int (sqrt (val*100)), b);
    //m_backend->volume (100 * log (1.0*val) / log (100.0), b);
    return !!m_backend;
}

bool CallbackProcess::saturation (int val, bool b) {
    if (m_backend)
        m_backend->saturation (val, b);
    return !!m_backend;
}

bool CallbackProcess::hue (int val, bool b) {
    if (m_backend)
        m_backend->hue (val, b);
    return !!m_backend;
}

bool CallbackProcess::brightness (int val, bool b) {
    if (m_backend)
        m_backend->brightness (val, b);
    return !!m_backend;
}

bool CallbackProcess::contrast (int val, bool b) {
    if (m_backend)
        m_backend->contrast (val, b);
    return !!m_backend;
}

QString CallbackProcess::dcopName () {
    QString cbname;
    cbname.sprintf ("%s/%s", QString (kapp->dcopClient ()->appId ()).ascii (),
                             QString (m_callback->objId ()).ascii ());
    return cbname;
}

void CallbackProcess::initProcess (Viewer * viewer) {
    Process::initProcess (viewer);
    connect (m_process, SIGNAL (processExited (KProcess *)),
            this, SLOT (processStopped (KProcess *)));
    connect (m_process, SIGNAL (receivedStdout (KProcess *, char *, int)),
            this, SLOT (processOutput (KProcess *, char *, int)));
    connect (m_process, SIGNAL (receivedStderr (KProcess *, char *, int)),
            this, SLOT (processOutput (KProcess *, char *, int)));
}

KDE_NO_EXPORT void CallbackProcess::processOutput (KProcess *, char * str, int slen) {
    if (viewer () && slen > 0)
        viewer ()->view ()->addText (QString::fromLocal8Bit (str, slen));
}

KDE_NO_EXPORT void CallbackProcess::processStopped (KProcess *) {
    if (m_source)
        ((PlayListNotify *) m_source)->setInfoMessage (QString::null);
    delete m_backend;
    m_backend = 0L;
    setState (NotRunning);
    if (m_send_config == send_try) {
        m_send_config = send_new; // we failed, retry ..
        ready (viewer ());
    }
}

WId CallbackProcess::widget () {
    return viewer () ? viewer ()->embeddedWinId () : 0;
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT ConfigDocument::ConfigDocument ()
    : Document (QString::null) {}

KDE_NO_CDTOR_EXPORT ConfigDocument::~ConfigDocument () {
    kdDebug () << "~ConfigDocument" << endl;
}

namespace KMPlayer {
    /*
     * Element for ConfigDocument
     */
    struct KMPLAYER_NO_EXPORT SomeNode : public ConfigNode {
        KDE_NO_CDTOR_EXPORT SomeNode (NodePtr & d, const QString & t)
            : ConfigNode (d, t) {}
        KDE_NO_CDTOR_EXPORT ~SomeNode () {}
        NodePtr childFromTag (const QString & t);
    };
} // namespace

KDE_NO_CDTOR_EXPORT ConfigNode::ConfigNode (NodePtr & d, const QString & t)
    : DarkNode (d, t), w (0L) {}

NodePtr ConfigDocument::childFromTag (const QString & tag) {
    if (tag.lower () == QString ("document"))
        return new ConfigNode (m_doc, tag);
    return 0L;
}

NodePtr ConfigNode::childFromTag (const QString & t) {
    return new TypeNode (m_doc, t);
}

KDE_NO_CDTOR_EXPORT TypeNode::TypeNode (NodePtr & d, const QString & t)
 : ConfigNode (d, t), tag (t) {}

NodePtr TypeNode::childFromTag (const QString & tag) {
    return new SomeNode (m_doc, tag);
}

NodePtr SomeNode::childFromTag (const QString & t) {
    return new SomeNode (m_doc, t);
}

QWidget * TypeNode::createWidget (QWidget * parent) {
    const char * ctype = getAttribute (QString ("TYPE")).ascii ();
    QString value = getAttribute (QString ("VALUE"));
    if (!strcmp (ctype, "range")) {
        w = new QSlider (getAttribute (QString ("START")).toInt (), getAttribute (QString ("END")).toInt (), 1, value.toInt (), Qt::Horizontal, parent);
    } else if (!strcmp (ctype, "num") || !strcmp (ctype,  "string")) {
        w = new QLineEdit (value, parent);
    } else if (!strcmp (ctype, "bool")) {
        QCheckBox * checkbox = new QCheckBox (parent);
        checkbox->setChecked (value.toInt ());
        w = checkbox;
    } else if (!strcmp (ctype, "enum")) {
        QComboBox * combo = new QComboBox (parent);
        for (NodePtr e = firstChild (); e; e = e->nextSibling ())
            if (e->isElementNode () && !strcmp (e->nodeName (), "item"))
                combo->insertItem (convertNode <Element> (e)->getAttribute (QString ("VALUE")));
        combo->setCurrentItem (value.toInt ());
        w = combo;
    } else if (!strcmp (ctype, "tree")) {
    } else
        kdDebug() << "Unknown type:" << ctype << endl;
    return w;
}

void TypeNode::changedXML (QTextStream & out) {
    if (!w) return;
    const char * ctype = getAttribute (QString ("TYPE")).ascii ();
    QString value = getAttribute (QString ("VALUE"));
    QString newvalue;
    if (!strcmp (ctype, "range")) {
        newvalue = QString::number (static_cast <QSlider *> (w)->value ());
    } else if (!strcmp (ctype, "num") || !strcmp (ctype,  "string")) {
        newvalue = static_cast <QLineEdit *> (w)->text ();
    } else if (!strcmp (ctype, "bool")) {
        newvalue = QString::number (static_cast <QCheckBox *> (w)->isChecked());
    } else if (!strcmp (ctype, "enum")) {
        newvalue = QString::number (static_cast<QComboBox *>(w)->currentItem());
    } else if (!strcmp (ctype, "tree")) {
    } else
        kdDebug() << "Unknown type:" << ctype << endl;
    if (value != newvalue) {
        value = newvalue;
        setAttribute (QString ("VALUE"), newvalue);
        out << outerXML ();
    }
}

//-----------------------------------------------------------------------------

namespace KMPlayer {

class KMPLAYER_NO_EXPORT XMLPreferencesFrame : public QFrame {
public:
    XMLPreferencesFrame (QWidget * parent, CallbackProcess *);
    KDE_NO_CDTOR_EXPORT ~XMLPreferencesFrame () {}
    QTable * table;
protected:
    void showEvent (QShowEvent *);
private:
    CallbackProcess * m_process;
};

} // namespace

KDE_NO_CDTOR_EXPORT XMLPreferencesFrame::XMLPreferencesFrame
(QWidget * parent, CallbackProcess * p)
 : QFrame (parent), m_process (p){
    QVBoxLayout * layout = new QVBoxLayout (this);
    table = new QTable (this);
    layout->addWidget (table);
}

KDE_NO_CDTOR_EXPORT XMLPreferencesPage::XMLPreferencesPage (CallbackProcess * p)
 : m_process (p), m_configframe (0L) {
}

KDE_NO_CDTOR_EXPORT XMLPreferencesPage::~XMLPreferencesPage () {
}

KDE_NO_EXPORT void XMLPreferencesFrame::showEvent (QShowEvent *) {
    if (!m_process->haveConfig ())
        m_process->getConfigData ();
}

KDE_NO_EXPORT void XMLPreferencesPage::write (KConfig *) {
}

KDE_NO_EXPORT void XMLPreferencesPage::read (KConfig *) {
}

KDE_NO_EXPORT void XMLPreferencesPage::sync (bool fromUI) {
    if (!m_configframe) return;
    QTable * table = m_configframe->table;
    int row = 0;
    if (fromUI) {
        NodePtr configdoc = m_process->configDocument ();
        if (!configdoc || m_configframe->table->numCols () < 1) //not yet created
            return;
        NodePtr elm = configdoc->firstChild (); // document
        if (!elm || !elm->hasChildNodes ()) {
            kdDebug () << "No valid data" << endl;
            return;
        }
        QString str;
        QTextStream ts (&str, IO_WriteOnly);
        ts << "<document>";
        for (NodePtr e = elm->firstChild (); e; e = e->nextSibling ())
            convertNode <TypeNode> (e)->changedXML (ts);
        if (str.length () > 10) {
            ts << "</document>";
            QByteArray changeddata = QCString (str.ascii ());
            kdDebug () << str <<  " " << changeddata.size () << str.length () << endl;
            changeddata.resize (str.length ());
            m_process->setChangedData (changeddata);
        }
    } else {
        if (!m_process->haveConfig ())
            return;
        NodePtr configdoc = m_process->configDocument ();
        if (!configdoc)
            return;
        if (m_configframe->table->numCols () < 1) { // not yet created
            QString err;
            int first_column_width = 50;
            NodePtr elm = configdoc->firstChild (); // document
            if (!elm || !elm->hasChildNodes ()) {
                kdDebug () << "No valid data" << endl;
                return;
            }
            // set up the table fields
            table->setNumCols (2);
            table->setNumRows (elm->childNodes ()->length ());
            table->verticalHeader ()->hide ();
            table->setLeftMargin (0);
            table->horizontalHeader ()->hide ();
            table->setTopMargin (0);
            table->setColumnReadOnly (0, true);
            QFontMetrics metrics (table->font ());
            for (elm=elm->firstChild (); elm; elm=elm->nextSibling (), row++) {
                TypeNode * tn = convertNode <TypeNode> (elm);
                QString name = tn->getAttribute (QString ("NAME"));
                m_configframe->table->setText (row, 0, name);
                int strwid = metrics.boundingRect (name).width ();
                if (strwid > first_column_width)
                    first_column_width = strwid + 4;
                QWidget * w = tn->createWidget (table->viewport ());
                if (w) {
                    table->setCellWidget (row, 1, w);
                    QWhatsThis::add (w, elm->innerText ());
                } else
                    kdDebug () << "No widget for " << name;
            }
            table->setColumnWidth (0, first_column_width);
            table->setColumnStretchable (1, true);
        }
    }
}

KDE_NO_EXPORT void XMLPreferencesPage::prefLocation (QString & item, QString & icon, QString & tab) {
    item = i18n ("General Options");
    icon = QString ("kmplayer");
    tab = m_process->menuName ();
}

KDE_NO_EXPORT QFrame * XMLPreferencesPage::prefPage (QWidget * parent) {
    m_configframe = new XMLPreferencesFrame (parent, m_process);
    return m_configframe;
}

//-----------------------------------------------------------------------------

static const char * xine_supported [] = {
    "dvdnavsource", "dvdsource", "exitsource", "introsource", "pipesource", "urlsource", "vcdsource", "audiocdsource", 0L
};

KDE_NO_CDTOR_EXPORT Xine::Xine (QObject * parent, Settings * settings)
    : CallbackProcess (parent, settings, "xine", i18n ("&Xine")) {
#ifdef HAVE_XINE
    m_supported_sources = xine_supported;
    m_settings->addPage (m_configpage);
#endif
}

KDE_NO_CDTOR_EXPORT Xine::~Xine () {}

bool Xine::ready (Viewer * viewer) {
    initProcess (viewer);
    QString xine_config = KProcess::quote (QString (QFile::encodeName (locateLocal ("data", "kmplayer/") + QString ("xine_config"))));
    m_request_seek = -1;
    if (m_source && !m_source->pipeCmd ().isEmpty ()) {
        fprintf (stderr, "%s | ", m_source->pipeCmd ().ascii ());
        *m_process << m_source->pipeCmd ().ascii () << " | ";
    }
    fprintf (stderr, "kxineplayer -wid %lu", (unsigned long) widget ());
    *m_process << "kxineplayer -wid " << QString::number (widget ());
    fprintf (stderr, " -f %s", xine_config.ascii ());
    *m_process << " -f " << xine_config;

    QString strVideoDriver = QString (m_settings->videodrivers[m_settings->videodriver].driver);
    if (!strVideoDriver.isEmpty ()) {
        fprintf (stderr, " -vo %s", strVideoDriver.lower().ascii());
        *m_process << " -vo " << strVideoDriver.lower();
    }
    QString strAudioDriver = QString (m_settings->audiodrivers[m_settings->audiodriver].driver);
    if (!strAudioDriver.isEmpty ()) {
        if (strAudioDriver.startsWith (QString ("alsa")))
            strAudioDriver = QString ("alsa");
        fprintf (stderr, " -ao %s", strAudioDriver.lower().ascii());
        *m_process << " -ao " << strAudioDriver.lower();
    }
    fprintf (stderr, " -cb %s", dcopName ().ascii());
    *m_process << " -cb " << dcopName ();
    if (m_have_config == config_unknown || m_have_config == config_probe) {
        fprintf (stderr, " -c");
        *m_process << " -c";
    }
    if (m_source)
        if (m_source->url ().url ().startsWith (QString ("dvd://")) &&
                !m_settings->dvddevice.isEmpty ()) {
            fprintf (stderr, " -dvd-device %s", m_settings->dvddevice.ascii ());
            *m_process << " -dvd-device " << m_settings->dvddevice;
        } else if (m_source->url ().url ().startsWith (QString ("vcd://")) &&
                !m_settings->vcddevice.isEmpty ()) {
            fprintf (stderr, " -vcd-device %s", m_settings->vcddevice.ascii ());
            *m_process << " -vcd-device " << m_settings->vcddevice;
        }
    if (!m_recordurl.isEmpty ()) {
        QString rf = KProcess::quote (
                QString (QFile::encodeName (getPath (m_recordurl))));
        fprintf (stderr, " -rec %s", rf.ascii ());
        *m_process << " -rec " << rf;
    }
    fprintf (stderr, "\n");
    m_process->start (KProcess::NotifyOnExit, KProcess::All);
    return m_process->isRunning ();
}

// TODO:input.v4l_video_device_path input.v4l_radio_device_path
// v4l:/Webcam/0   v4l:/Television/21600  v4l:/Radio/96

//-----------------------------------------------------------------------------

static const char * gst_supported [] = {
    "exitsource", "introsource", "urlsource", "vcdsource", "audiocdsource", 0L
};

KDE_NO_CDTOR_EXPORT GStreamer::GStreamer (QObject * parent, Settings * settings)
    : CallbackProcess (parent, settings, "gstreamer", i18n ("&GStreamer")) {
#ifdef HAVE_GSTREAMER
    m_supported_sources = gst_supported;
#endif
}

KDE_NO_CDTOR_EXPORT GStreamer::~GStreamer () {}

KDE_NO_EXPORT bool GStreamer::ready (Viewer * viewer) {
    initProcess (viewer);
    m_request_seek = -1;
    fprintf (stderr, "kgstplayer -wid %lu", (unsigned long) widget ());
    *m_process << "kgstplayer -wid " << QString::number (widget ());

    QString strVideoDriver = QString (m_settings->videodrivers[m_settings->videodriver].driver);
    if (!strVideoDriver.isEmpty ()) {
        fprintf (stderr, " -vo %s", strVideoDriver.lower().ascii());
        *m_process << " -vo " << strVideoDriver.lower();
    }
    QString strAudioDriver = QString (m_settings->audiodrivers[m_settings->audiodriver].driver);
    if (!strAudioDriver.isEmpty ()) {
        if (strAudioDriver.startsWith (QString ("alsa")))
            strAudioDriver = QString ("alsa");
        fprintf (stderr, " -ao %s", strAudioDriver.lower().ascii());
        *m_process << " -ao " << strAudioDriver.lower();
    }
    fprintf (stderr, " -cb %s", dcopName ().ascii());
    *m_process << " -cb " << dcopName ();
    if (m_source)
        if (m_source->url ().url ().startsWith (QString ("dvd://")) &&
                !m_settings->dvddevice.isEmpty ()) {
            fprintf (stderr, " -dvd-device %s", m_settings->dvddevice.ascii ());
            *m_process << " -dvd-device " << m_settings->dvddevice;
        } else if (m_source->url ().url ().startsWith (QString ("vcd://")) &&
                !m_settings->vcddevice.isEmpty ()) {
            fprintf (stderr, " -vcd-device %s", m_settings->vcddevice.ascii ());
            *m_process << " -vcd-device " << m_settings->vcddevice;
        }
    fprintf (stderr, "\n");
    m_process->start (KProcess::NotifyOnExit, KProcess::All);
    return m_process->isRunning ();
}

//-----------------------------------------------------------------------------

static const char * ffmpeg_supports [] = {
    "tvsource", "urlsource", 0L
};

FFMpeg::FFMpeg (QObject * parent, Settings * settings)
 : Process (parent, settings, "ffmpeg") {
    m_supported_sources = ffmpeg_supports;
}

KDE_NO_CDTOR_EXPORT FFMpeg::~FFMpeg () {
}

KDE_NO_EXPORT void FFMpeg::init () {
}

bool FFMpeg::deMediafiedPlay () {
    initProcess (viewer ());
    KURL url (m_url);
    connect (m_process, SIGNAL (processExited (KProcess *)),
            this, SLOT (processStopped (KProcess *)));
    QString outurl = QString (QFile::encodeName (m_recordurl.isLocalFile () ? getPath (m_recordurl) : m_recordurl.url ()));
    if (m_recordurl.isLocalFile ())
        QFile (outurl).remove ();
    QString cmd ("ffmpeg ");
    if (!m_source->videoDevice ().isEmpty () ||
        !m_source->audioDevice ().isEmpty ()) {
        if (!m_source->videoDevice ().isEmpty ())
            cmd += QString ("-vd ") + m_source->videoDevice ();
        else
            cmd += QString ("-vn");
        if (!m_source->audioDevice ().isEmpty ())
            cmd += QString (" -ad ") + m_source->audioDevice ();
        else
            cmd += QString (" -an");
        KProcess process;
        process.setUseShell (true);
        if (!m_source->videoNorm ().isEmpty ()) {
            process << "v4lctl -c " << m_source->videoDevice () << " setnorm " << m_source->videoNorm ();
            kdDebug () << "v4lctl -c " << m_source->videoDevice () << " setnorm " << m_source->videoNorm () << endl;
            process.start (KProcess::Block);
            cmd += QString (" -tvstd ") + m_source->videoNorm ();
        }
        if (m_source->frequency () > 0) {
            process.clearArguments();
            process << "v4lctl -c " << m_source->videoDevice () << " setfreq " << QString::number (m_source->frequency ());
            kdDebug () << "v4lctl -c " << m_source->videoDevice () << " setfreq " << m_source->frequency () << endl;
            process.start (KProcess::Block);
        }
    } else {
        cmd += QString ("-i ") + KProcess::quote (QString (QFile::encodeName (url.isLocalFile () ? getPath (url) : url.url ())));
    }
    cmd += QChar (' ') + m_settings->ffmpegarguments;
    cmd += QChar (' ') + KProcess::quote (QString (QFile::encodeName (outurl)));
    fprintf (stderr, "%s\n", (const char *) cmd.local8Bit ());
    *m_process << cmd;
    // FIXME if (m_player->source () == source) // ugly
    //    m_player->stop ();
    m_process->start (KProcess::NotifyOnExit, KProcess::All);
    if (m_process->isRunning ())
        setState (Playing);
    return m_process->isRunning ();
}

KDE_NO_EXPORT bool FFMpeg::stop () {
    terminateJob ();
    if (!playing ()) return true;
    kdDebug () << "FFMpeg::stop" << endl;
    m_process->writeStdin ("q", 1);
    QTime t;
    t.start ();
    do {
        KProcessController::theKProcessController->waitForProcessExit (2);
    } while (t.elapsed () < 2000 && m_process->isRunning ());
    if (!playing ()) return true;
    return Process::stop ();
}

KDE_NO_EXPORT void FFMpeg::processStopped (KProcess *) {
    setState (NotRunning);
}

#include "kmplayerprocess.moc"
