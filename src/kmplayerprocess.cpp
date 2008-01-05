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

#ifdef HAVE_DBUS
# include <kstaticdeleter.h>
# include <dbus/connection.h>
#endif

#include "kmplayerview.h"
#include "kmplayercontrolpanel.h"
#include "kmplayerprocess.h"
#include "kmplayersource.h"
#include "kmplayerpartbase.h"
#include "kmplayerconfig.h"
#include "kmplayer_callback.h"
#include "kmplayer_backend_stub.h"

using namespace KMPlayer;

ProcessInfo::ProcessInfo (const char *nm, const QString &lbl,
        const char **supported, MediaManager* mgr, PreferencesPage *prefs)
 : name (nm),
   label (lbl),
   supported_sources (supported),
   manager (mgr),
   config_page (prefs) {
    if (config_page)
        manager->player ()->settings ()->addPage (config_page);
}

ProcessInfo::~ProcessInfo () {
    delete config_page;
}

bool ProcessInfo::supports (const char *source) const {
    for (const char ** s = supported_sources; s[0]; ++s) {
        if (!strcmp (s[0], source))
            return true;
    }
    return false;
}

//------------------------%<----------------------------------------------------

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

static void setupProcess (KProcess **process) {
    delete *process;
    *process = new KProcess;
    (*process)->setUseShell (true);
    (*process)->setEnvironment (QString::fromLatin1 ("SESSION_MANAGER"), QString::fromLatin1 (""));
}

static void killProcess (KProcess *process, QWidget *widget, bool group) {
    if (!process || !process->isRunning ())
        return;
    if (group) {
        void (*oldhandler)(int) = signal(SIGTERM, SIG_IGN);
        ::kill (-1 * ::getpid (), SIGTERM);
        signal(SIGTERM, oldhandler);
    } else
        process->kill (SIGTERM);
#if KDE_IS_VERSION(3, 1, 90)
    process->wait(1);
#else
    KProcessController::theKProcessController->waitForProcessExit (1);
#endif
    if (!process->isRunning ())
        return;
    process->kill (SIGKILL);
#if KDE_IS_VERSION(3, 1, 90)
    process->wait(1);
#else
    KProcessController::theKProcessController->waitForProcessExit (1);
#endif
    if (process->isRunning ())
        KMessageBox::error (widget,
                i18n ("Failed to end player process."), i18n ("Error"));
}

Process::Process (QObject *parent, ProcessInfo *pinfo, Settings *settings, const char * n)
 : QObject (parent, n),
   IProcess (pinfo),
   m_source (0L),
   m_settings (settings),
   m_old_state (IProcess::NotRunning),
   m_process (0L),
   m_job(0L) {
    kdDebug() << "new Process " << name () << endl;
}

Process::~Process () {
    quit ();
    delete m_process;
    if (media_object)
        media_object->process = NULL;
    if (process_info) // FIXME: remove
        process_info->manager->processDestroyed (this);
    kdDebug() << "~Process " << name () << endl;
}

void Process::init () {
}

void Process::initProcess () {
    setupProcess (&m_process);
    if (m_source) m_source->setPosition (0);
}

WId Process::widget () {
    return media_object && media_object->viewer
        ? media_object->viewer->windowHandle ()
        : 0;
}

Mrl *Process::mrl () const {
    if (media_object)
        return media_object->mrl ();
    return NULL;
}

bool Process::running () const {
    return m_process && m_process->isRunning ();
}

void Process::setAudioLang (int, const QString &) {}

void Process::setSubtitle (int, const QString &) {}

void Process::pause () {
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

void Process::stop () {
}

void Process::quit () {
    killProcess (m_process, view(), m_source && !m_source->pipeCmd().isEmpty());
    setState (IProcess::NotRunning);
}

void Process::setState (IProcess::State newstate) {
    if (m_state != newstate) {
        bool need_timer = m_old_state == m_state;
        m_old_state = m_state;
        m_state = newstate;
        if (need_timer)
            QTimer::singleShot (0, this, SLOT (rescheduledStateChanged ()));
    }
}

KDE_NO_EXPORT void Process::rescheduledStateChanged () {
    IProcess::State old_state = m_old_state;
    m_old_state = m_state;
    if (media_object) {
        process_info->manager->stateChange (media_object, old_state, m_state);
    } else {
        if (m_state > IProcess::Ready)
            kdError() << "Process running, mrl disappeared" << endl;
        delete this;
    }
}

bool Process::play () {
    Mrl *m = mrl ();
    QString url = m ? m->absolutePath () : QString ();
    bool changed = m_url != url;
    m_url = url;
    if (media_object) // FIXME: remove check
        media_object->request = AudioVideoMedia::ask_nothing;
#if KDE_IS_VERSION(3,3,91)
    if (!changed || KURL (m_url).isLocalFile () || m_url.startsWith ("tv:/"))
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

void Process::terminateJobs () {
    if (m_job) {
        m_job->kill ();
        m_job = 0L;
    }
}

bool Process::ready () {
    setState (IProcess::Ready);
    return true;
}

View *Process::view () const {
    return m_source ? m_source->player ()->viewWidget () : NULL;
}

//-----------------------------------------------------------------------------

RecordDocument::RecordDocument (const QString &url, const QString &rurl,
        const QString &rec, bool video, PlayListNotify *n)
 : Document (url, n),
   record_file (rurl),
   recorder (rec),
   has_video (video) {
    id = id_node_record_document;
}

void RecordDocument::begin () {
    media_object = notify_listener->mediaManager ()->createMedia (
            MediaManager::AudioVideo, this);
    media_object->play ();
}

void RecordDocument::endOfFile () {
    deactivate ();
}

void RecordDocument::deactivate () {
    state = state_deactivated;
    notify_listener->mediaManager ()->player ()->stopRecording ();
    Document::deactivate ();
}

static RecordDocument *recordDocument (AudioVideoMedia *media_object) {
    Mrl *mrl = media_object ? media_object->mrl () : NULL;
    return mrl && id_node_record_document == mrl->id
        ? static_cast <RecordDocument *> (mrl) : NULL;
}

//-----------------------------------------------------------------------------

static bool proxyForURL (const KURL& url, QString& proxy) {
    KProtocolManager::slaveProtocol (url, proxy);
    return !proxy.isNull ();
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT MPlayerBase::MPlayerBase (QObject *parent, ProcessInfo *pinfo, Settings * settings, const char * n)
    : Process (parent, pinfo, settings, n), m_use_slave (true) {
    m_process = new KProcess;
}

KDE_NO_CDTOR_EXPORT MPlayerBase::~MPlayerBase () {
}

KDE_NO_EXPORT void MPlayerBase::initProcess () {
    Process::initProcess ();
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
    if (running () && m_use_slave) {
        commands.push_front (cmd + '\n');
        fprintf (stderr, "eval %s", commands.last ().latin1 ());
        if (commands.size () < 2)
            m_process->writeStdin (QFile::encodeName(commands.last ()),
                    commands.last ().length ());
        return true;
    }
    return false;
}

KDE_NO_EXPORT void MPlayerBase::stop () {
    terminateJobs ();
}

KDE_NO_EXPORT void MPlayerBase::quit () {
    if (running ()) {
        kdDebug() << "PlayerBase::quit" << endl;
        stop ();
        disconnect (m_process, SIGNAL (processExited (KProcess *)),
                this, SLOT (processStopped (KProcess *)));
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
            Process::quit ();
        processStopped (0L);
        commands.clear ();
    }
    Process::quit ();
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
    setState (IProcess::Ready);
}

//-----------------------------------------------------------------------------

static const char *mplayer_supports [] = {
    "dvdsource", "exitsource", "hrefsource", "introsource", "pipesource", "tvscanner", "tvsource", "urlsource", "vcdsource", "audiocdsource", 0L
};

MPlayerProcessInfo::MPlayerProcessInfo (MediaManager *mgr)
 : ProcessInfo ("mplayer", i18n ("&MPlayer"), mplayer_supports,
         mgr, new MPlayerPreferencesPage ()) {}

IProcess *MPlayerProcessInfo::create (PartBase *part, AudioVideoMedia *media) {
    MPlayer *m = new MPlayer (part, this, part->settings ());
    m->setSource (part->source ());
    m->media_object = media;
    part->processCreated (m);
    return m;
}

KDE_NO_CDTOR_EXPORT
MPlayer::MPlayer (QObject *parent, ProcessInfo *pinfo, Settings *settings)
 : MPlayerBase (parent, pinfo, settings, "mplayer"),
   m_widget (0L),
   aid (-1), sid (-1),
   m_needs_restarted (false) {
}

KDE_NO_CDTOR_EXPORT MPlayer::~MPlayer () {
    if (m_widget && !m_widget->parent ())
        delete m_widget;
}

KDE_NO_EXPORT void MPlayer::init () {
}

KDE_NO_EXPORT bool MPlayer::ready () {
    Process::ready ();
    if (media_object && media_object->viewer)
        media_object->viewer->useIndirectWidget (true);
    return false;
}

KDE_NO_EXPORT bool MPlayer::deMediafiedPlay () {
    if (running ())
        return sendCommand (QString ("gui_play"));
    if (!m_needs_restarted && running ())
        quit (); // rescheduling of setState will reset state just-in-time
    initProcess ();
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
    MPlayerPreferencesPage *cfg_page = static_cast <MPlayerPreferencesPage *>(process_info->config_page);
    if (!url.isEmpty ()) {
        if (m_source->url ().isLocalFile ())
            m_process->setWorkingDirectory
                (QFileInfo (m_source->url ().path ()).dirPath (true));
        if (url.isLocalFile ()) {
            m_url = getPath (url);
            if (cfg_page->alwaysbuildindex &&
                    (m_url.lower ().endsWith (".avi") ||
                     m_url.lower ().endsWith (".divx")))
                args += QString (" -idx ");
        } else {
            int cache = cfg_page->cachesize;
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
        Mrl *m = mrl ();
        if (m && m->repeat > 0)
            args += QString (" -loop " + QString::number (m->repeat + 1));
        else if (m_settings->loop)
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

KDE_NO_EXPORT void MPlayer::stop () {
    terminateJobs ();
    if (!m_source || !m_process || !m_process->isRunning ())
        return;
    if (m_use_slave)
        sendCommand (QString ("quit"));
    MPlayerBase::stop ();
}

KDE_NO_EXPORT void MPlayer::pause () {
    sendCommand (QString ("pause"));
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
    m_process_output = QString ();
    connect (m_process, SIGNAL (receivedStdout (KProcess *, char *, int)),
            this, SLOT (processOutput (KProcess *, char *, int)));
    connect (m_process, SIGNAL (receivedStderr (KProcess *, char *, int)),
            this, SLOT (processOutput (KProcess *, char *, int)));
    m_use_slave = !(pipe && pipe[0]);
    if (!m_use_slave) {
        fprintf (stderr, "%s | ", pipe);
        *m_process << pipe << " | ";
    }
    MPlayerPreferencesPage *cfg_page = static_cast <MPlayerPreferencesPage *>(process_info->config_page);
    QString exe = cfg_page->mplayer_path;
    if (exe.isEmpty ())
        exe = "mplayer";
    fprintf (stderr, "%s -wid %lu ", exe.ascii(), (unsigned long) widget ());
    *m_process << exe << " -wid " << QString::number (widget ());

    if (m_use_slave) {
        fprintf (stderr, "-slave ");
        *m_process << "-slave ";
    }

    QString strVideoDriver = QString (m_settings->videodrivers[m_settings->videodriver].driver);
    if (!strVideoDriver.isEmpty ()) {
        fprintf (stderr, " -vo %s", strVideoDriver.lower().ascii());
        *m_process << " -vo " << strVideoDriver.lower();
        if (view () && view ()->keepSizeRatio () &&
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

    if (cfg_page->additionalarguments.length () > 0) {
        fprintf (stderr, " %s", cfg_page->additionalarguments.ascii());
        *m_process << " " << cfg_page->additionalarguments;
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
    for (NodePtr n = mrl (); n; n = n->parentNode ()) {
        if (n->id != id_node_group_node && n->id != id_node_playlist_item)
            break;
        QString plops = convertNode <Element> (n)->getAttribute ("mplayeropts");
        if (!plops.isNull ()) {
            QStringList sl = QStringList::split (QChar (' '), plops);
            for (int i = 0; i < sl.size (); ++i) {
                QString plop = KProcess::quote (sl[i]);
                fprintf (stderr, " %s", plop.ascii ());
                *m_process << " " << plop;
            }
            break;
        }
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

    old_volume = view () ? view ()->controlPanel ()->volumeBar ()->value () : 0;

    if (m_process->isRunning ()) {
        setState (IProcess::Buffering); // wait for start regexp for state Playing
        return true;
    }
    return false;
}

KDE_NO_EXPORT bool MPlayer::grabPicture (const KURL & url, int pos) {
    stop ();
    initProcess ();
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
    if (!mrl () || slen <= 0) return;
    View *v = view ();

    bool ok;
    QRegExp *patterns = static_cast<MPlayerPreferencesPage *>(process_info->config_page)->m_patterns;
    QRegExp & m_refURLRegExp = patterns[MPlayerPreferencesPage::pat_refurl];
    QRegExp & m_refRegExp = patterns[MPlayerPreferencesPage::pat_ref];
    do {
        int len = strcspn (str, "\r\n");
        QString out = m_process_output + QString::fromLocal8Bit (str, len);
        m_process_output = QString ();
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
        } else if (out.startsWith ("ID_LENGTH")) {
            int pos = out.find ('=');
            if (pos > 0) {
                int l = (int) out.mid (pos + 1).toDouble (&ok);
                if (ok && l >= 0) {
                    m_source->setLength (mrl (), 10 * l);
                }
            }
        } else if (m_refURLRegExp.search(out) > -1) {
            kdDebug () << "Reference mrl " << m_refURLRegExp.cap (1) << endl;
            if (!m_tmpURL.isEmpty () && m_url != m_tmpURL)
                m_source->insertURL (mrl (), m_tmpURL);;
            m_tmpURL = KURL::fromPathOrURL (m_refURLRegExp.cap (1)).url ();
            if (m_source->url () == m_tmpURL || m_url == m_tmpURL)
                m_tmpURL.truncate (0);
        } else if (m_refRegExp.search (out) > -1) {
            kdDebug () << "Reference File " << endl;
            m_tmpURL.truncate (0);
        } else if (out.startsWith ("ID_VIDEO_WIDTH")) {
            int pos = out.find ('=');
            if (pos > 0) {
                int w = out.mid (pos + 1).toInt ();
                m_source->setDimensions (mrl (), w, m_source->height ());
            }
        } else if (out.startsWith ("ID_VIDEO_HEIGHT")) {
            int pos = out.find ('=');
            if (pos > 0) {
                int h = out.mid (pos + 1).toInt ();
                m_source->setDimensions (mrl (), m_source->width (), h);
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
                    m_source->setAspect (mrl (), a);
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
        } else if (out.startsWith ("ICY Info")) {
            int p = out.find ("StreamTitle=", 8);
            if (p > -1) {
                p += 12;
                int e = out.find (';', p);
                if (e > -1)
                    e -= p;
                ((PlayListNotify *)m_source)->setInfoMessage (out.mid (p, e));
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
                        m_source->setDimensions(mrl(),movie_width,movie_height);
                        m_source->setAspect (mrl(), 1.0*movie_width/movie_height);
                    }
                } else if (m_startRegExp.search (out) > -1) {
                    if (m_settings->mplayerpost090) {
                        if (!m_tmpURL.isEmpty () && m_url != m_tmpURL) {
                            m_source->insertURL (mrl (), m_tmpURL);;
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
                    setState (IProcess::Playing);
                }
            }
        }
    } while (slen > 0);
}

KDE_NO_EXPORT void MPlayer::processStopped (KProcess * p) {
    if (p && !m_grabfile.isEmpty ()) {
        emit grabReady (m_grabfile);
        m_grabfile.truncate (0);
    } else if (mrl ()) {
        QString url;
        if (!m_source->identified ()) {
            m_source->setIdentified ();
            if (!m_tmpURL.isEmpty () && m_url != m_tmpURL) {
                m_source->insertURL (mrl (), m_tmpURL);;
                m_tmpURL.truncate (0);
            }
        }
        if (p && m_source && m_needs_restarted) {
            commands.clear ();
            int pos = m_source->position ();
            play ();
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
static const char * strMPlayerPath = "MPlayer Path";
static const char * strAddArgs = "Additional Arguments";
static const char * strCacheSize = "Cache Size for Streaming";
static const char * strAlwaysBuildIndex = "Always build index";
static const int non_patterns = 4;

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
    { i18n ("Audio CD tracks pattern"), "CDROM Tracks", "[Aa]udio CD[^0-9]+([0-9]+)[^0-9]tracks" }
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
    table->setText (0, 0, i18n ("MPlayer command:"));
    table->setText (1, 0, i18n ("Additional command line arguments:"));
    table->setText (2, 0, QString("%1 (%2)").arg (i18n ("Cache size:")).arg (i18n ("kB"))); // FIXME for new translations
    table->setCellWidget (2, 1, new QSpinBox (0, 32767, 32, table->viewport()));
    table->setText (3, 0, i18n ("Build new index when possible"));
    table->setCellWidget (3, 1, new QCheckBox (table->viewport()));
    QWhatsThis::add (table->cellWidget (3, 1), i18n ("Allows seeking in indexed files (AVIs)"));
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

KDE_NO_CDTOR_EXPORT MPlayerPreferencesPage::MPlayerPreferencesPage ()
 : m_configframe (0L) {
}

KDE_NO_EXPORT void MPlayerPreferencesPage::write (KConfig * config) {
    config->setGroup (strMPlayerPatternGroup);
    for (int i = 0; i < int (pat_last); i++)
        config->writeEntry
            (_mplayer_patterns[i].name, m_patterns[i].pattern ());
    config->setGroup (strMPlayerGroup);
    config->writeEntry (strMPlayerPath, mplayer_path);
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
    mplayer_path = config->readEntry (strMPlayerPath, "mplayer");
    additionalarguments = config->readEntry (strAddArgs);
    cachesize = config->readNumEntry (strCacheSize, 384);
    alwaysbuildindex = config->readBoolEntry (strAlwaysBuildIndex, false);
}

KDE_NO_EXPORT void MPlayerPreferencesPage::sync (bool fromUI) {
    QTable * table = m_configframe->table;
    QSpinBox * cacheSize = static_cast<QSpinBox *>(table->cellWidget (2, 1));
    QCheckBox * buildIndex = static_cast<QCheckBox *>(table->cellWidget (3, 1));
    if (fromUI) {
        mplayer_path = table->text (0, 1);
        additionalarguments = table->text (1, 1);
        for (int i = 0; i < int (pat_last); i++)
            m_patterns[i].setPattern (table->text (i+non_patterns, 1));
        cachesize = cacheSize->value();
        alwaysbuildindex = buildIndex->isChecked ();
    } else {
        table->setText (0, 1, mplayer_path);
        table->setText (1, 1, additionalarguments);
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
    "dvdsource", "pipesource", "tvscanner", "tvsource", "urlsource",
    "vcdsource", "audiocdsource", NULL
};

MEncoderProcessInfo::MEncoderProcessInfo (MediaManager *mgr)
 : ProcessInfo ("mencoder", i18n ("M&Encoder"), mencoder_supports,
         mgr, NULL) {}

IProcess *MEncoderProcessInfo::create (PartBase *part, AudioVideoMedia *media) {
    MEncoder *m = new MEncoder (part, this, part->settings ());
    m->setSource (part->source ());
    m->media_object = media;
    part->processCreated (m);
    return m;
}

KDE_NO_CDTOR_EXPORT
MEncoder::MEncoder (QObject * parent, ProcessInfo *pinfo, Settings * settings)
 : MPlayerBase (parent, pinfo, settings, "mencoder") {}

KDE_NO_CDTOR_EXPORT MEncoder::~MEncoder () {
}

KDE_NO_EXPORT void MEncoder::init () {
}

bool MEncoder::deMediafiedPlay () {
    bool success = false;
    stop ();
    RecordDocument *rd = recordDocument (media_object);
    if (!rd)
        return false;
    initProcess ();
    KURL url (m_url);
    QString args;
    m_use_slave = m_source ? m_source->pipeCmd ().isEmpty () : true;
    //if (!m_use_slave)
    //    args = m_source->pipeCmd () + QString (" | ");
    QString margs = m_settings->mencoderarguments;
    if (m_settings->recordcopy)
        margs = QString ("-oac copy -ovc copy");
    args += QString ("mencoder ") + margs;
    if (m_source)
        args += + ' ' + m_source->recordCmd ();
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
    KURL out (rd->record_file);
    QString outurl = KProcess::quote (QString (QFile::encodeName (
                    out.isLocalFile () ? getPath (out) : out.url ())));
    kdDebug () << args << " -o " << outurl << endl;
    *m_process << args << " -o " << outurl;
    m_process->start (KProcess::NotifyOnExit, KProcess::All);
    success = m_process->isRunning ();
    if (success)
        setState (IProcess::Playing);
    return success;
}

KDE_NO_EXPORT void MEncoder::stop () {
    terminateJobs ();
    if (!m_process || !m_process->isRunning ())
        return;
    kdDebug () << "MEncoder::stop ()" << endl;
    if (m_use_slave)
        m_process->kill (SIGINT);
    MPlayerBase::stop ();
}

//-----------------------------------------------------------------------------

static const char * mplayerdump_supports [] = {
    "dvdsource", "pipesource", "tvscanner", "tvsource", "urlsource", "vcdsource", "audiocdsource", 0L
};

MPlayerDumpProcessInfo::MPlayerDumpProcessInfo (MediaManager *mgr)
 : ProcessInfo ("mplayerdumpstream", i18n ("&MPlayerDumpstream"),
         mplayerdump_supports, mgr, NULL) {}

IProcess *MPlayerDumpProcessInfo::create (PartBase *p, AudioVideoMedia *media) {
    MPlayerDumpstream *m = new MPlayerDumpstream (p, this, p->settings ());
    m->setSource (p->source ());
    m->media_object = media;
    p->processCreated (m);
    return m;
}

KDE_NO_CDTOR_EXPORT
MPlayerDumpstream::MPlayerDumpstream (QObject *p, ProcessInfo *pi, Settings *s)
 : MPlayerBase (p, pi, s, "mplayerdumpstream") {}

KDE_NO_CDTOR_EXPORT MPlayerDumpstream::~MPlayerDumpstream () {
}

KDE_NO_EXPORT void MPlayerDumpstream::init () {
}

bool MPlayerDumpstream::deMediafiedPlay () {
    bool success = false;
    stop ();
    RecordDocument *rd = recordDocument (media_object);
    if (!rd)
        return false;
    initProcess ();
    KURL url (m_url);
    QString args;
    m_use_slave = m_source ? m_source->pipeCmd ().isEmpty () : true;
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
    KURL out (rd->record_file);
    QString outurl = KProcess::quote (QString (QFile::encodeName (
                    out.isLocalFile () ? getPath (out) : out.url ())));
    kdDebug () << args << " -dumpstream -dumpfile " << outurl << endl;
    *m_process << args << " -dumpstream -dumpfile " << outurl;
    m_process->start (KProcess::NotifyOnExit, KProcess::NoCommunication);
    success = m_process->isRunning ();
    if (success)
        setState (IProcess::Playing);
    return success;
}

KDE_NO_EXPORT void MPlayerDumpstream::stop () {
    terminateJobs ();
    if (!m_source || !m_process || !m_process->isRunning ())
        return;
    kdDebug () << "MPlayerDumpstream::stop ()" << endl;
    if (m_use_slave)
        m_process->kill (SIGINT);
    MPlayerBase::stop ();
}

//-----------------------------------------------------------------------------

static int callback_counter = 0;

Callback::Callback (CallbackProcessInfo *pinfo, const char *pname)
 : DCOPObject (QString (QString ("KMPlayerCallback-") +
             QString::number (callback_counter++)).ascii ()),
   process_info (pinfo),
   proc_name (pname) {}

CallbackProcess *Callback::process (unsigned long id) {
    MediaManager::MediaList &ml = process_info->manager->medias ();
    const MediaManager::MediaList::iterator e = ml.end ();
    for (MediaManager::MediaList::iterator i = ml.begin (); i != e; ++i)
        if ((*i)->type () == MediaManager::AudioVideo) {
            AudioVideoMedia *av = static_cast <AudioVideoMedia *> (*i);
            if (av->viewer && av->viewer->windowHandle () == id)
                return static_cast <CallbackProcess *> (av->process);
        }
    kdWarning() << "process " << id << " not found" << kdBacktrace() << endl;
    return NULL;
}

void Callback::statusMessage (unsigned long id, int code, QString msg) {
    CallbackProcess *proc = process (id);
    if (!proc || proc->m_source)
        return;
    switch ((StatusCode) code) {
        case stat_newtitle:
            //proc->source ()->setTitle (msg);
            if (proc->view ())
                ((PlayListNotify *) proc->source ())->setInfoMessage (msg);
            break;
        case stat_hasvideo:
            if (proc->view ())
                proc->view ()->videoStart ();
            break;
        default:
            proc->setStatusMessage (msg);
    };
}

void Callback::subMrl (unsigned long id, QString url, QString title) {
    CallbackProcess *proc = process (id);
    Mrl *m = proc ? proc->mrl () : NULL;
    if (!m || !proc->m_source)
        return;
    proc->m_source->insertURL (m, KURL::fromPathOrURL(url).url(), title);
    if (m->active ())
        m->defer (); // Xine detected this is a playlist
}

void Callback::errorMessage (unsigned long id, int code, QString msg) {
    if (!id && !code) {
        process_info->changesReceived ();
    } else {
        CallbackProcess *proc = process (id);
        if (proc)
            proc->setErrorMessage (code, msg);
    }
}

void Callback::finished (unsigned long id) {
    CallbackProcess *proc = process (id);
    if (proc)
        proc->setFinished ();
}

void Callback::playing (unsigned long id) {
    CallbackProcess *proc = process (id);
    if (proc)
        proc->setPlaying ();
}

void Callback::started (QCString dcopname, QByteArray data) {
    static_cast <CallbackProcessInfo *> (process_info)->backendStarted (
            dcopname, data);
}

void Callback::movieParams (unsigned long id, int length, int w, int h, float aspect, QStringList alang, QStringList slang) {
    CallbackProcess *proc = process (id);
    if (proc)
        proc->setMovieParams (length, w, h, aspect, alang, slang);
}

void Callback::moviePosition (unsigned long id, int position) {
    CallbackProcess *proc = process (id);
    if (proc)
        proc->setMoviePosition (position);
}

void Callback::loadingProgress (unsigned long id, int percentage) {
    CallbackProcess *proc = process (id);
    if (proc)
        proc->setLoadingProgress (percentage);
}

void Callback::toggleFullScreen (unsigned long id) {
    CallbackProcess *proc = process (id);
    if (proc && proc->view ())
        proc->view ()->fullScreen ();
}

//-----------------------------------------------------------------------------

CallbackProcessInfo::CallbackProcessInfo (const char *nm, const QString &lbl,
        const char **supported, MediaManager *mgr, PreferencesPage *pref)
 : ProcessInfo (nm, lbl, supported, mgr, pref),
   backend (NULL),
   callback (NULL),
   m_process (NULL),
   have_config (config_unknown),
   send_config (send_no) {}

CallbackProcessInfo::~CallbackProcessInfo () {
    delete callback;
    if (config_doc)
        config_doc->document()->dispose ();
    delete m_process;
}

QString CallbackProcessInfo::dcopName () {
    QString cbname;
    if (callback) {
        kdError() << "backend still avaible" << endl;
        delete callback;
    }
    callback = new Callback (this, ProcessInfo::name);
    cbname.sprintf ("%s/%s", QString (kapp->dcopClient ()->appId ()).ascii (),
                             QString (callback->objId ()).ascii ());
    return cbname;
}

void CallbackProcessInfo::backendStarted (QCString dcopname, QByteArray &data) {
    MediaManager::ProcessList &pl = manager->processes ();
    bool kill_backend = false;
    kdDebug () << "up and running " << dcopname << " " << have_config << endl;

    backend = new Backend_stub (dcopname, "Backend");

    if (send_config == send_new)
        backend->setConfig (changed_data);

    if (have_config == config_probe || have_config == config_unknown) {
        bool was_probe = have_config == config_probe;
        have_config = data.size () ? config_yes : config_no;
        if (have_config == config_yes) {
            if (config_doc)
                config_doc->document ()->dispose ();
            config_doc = new ConfigDocument ();
            QTextStream ts (data, IO_ReadOnly);
            readXML (config_doc, ts, QString ());
            config_doc->normalize ();
            //kdDebug () << mydoc->innerText () << endl;
        }
        emit configReceived ();
        if (config_page)
            config_page->sync (false);
        if (was_probe)
            kill_backend = true;
    }

    const MediaManager::ProcessList::iterator e = pl.end ();
    for (MediaManager::ProcessList::iterator i = pl.begin (); i != e; ++i) {
        Process *proc = static_cast <Process *> (*i);
        if (!strcmp (ProcessInfo::name, proc->name ())) {
             static_cast <CallbackProcess *>(proc)->setState (IProcess::Ready);
             kill_backend = false;
        }
    }
    if (kill_backend)
        backend->quit ();
}

KDE_NO_EXPORT
void CallbackProcessInfo::processOutput (KProcess *, char *str, int slen) {
    if (manager->player ()->viewWidget () && slen > 0)
        manager->player ()->viewWidget ()->addText (QString::fromLocal8Bit (str, slen));
}

KDE_NO_EXPORT void CallbackProcessInfo::processStopped (KProcess *) {
    MediaManager::ProcessList &pl = manager->processes ();
    kdDebug () << "ProcessInfo " << ProcessInfo::name << " stopped" << endl;

    delete m_process;
    m_process = NULL;

    const MediaManager::ProcessList::iterator e = pl.end ();
    for (MediaManager::ProcessList::iterator i = pl.begin (); i != e; ++i) {
        if (this == (*i)->process_info)
             static_cast <CallbackProcess *>(*i)->setState (
                     IProcess::NotRunning);
    }
    manager->player ()->updateInfo (QString ());
    delete backend;
    backend = NULL;
    delete callback;
    callback = NULL;
    /*if (m_send_config == send_try) {
        m_send_config = send_new; // we failed, retry ..
        ready ();
    }*/
}

void CallbackProcessInfo::quitProcesses () {
    stopBackend ();
}

void CallbackProcessInfo::stopBackend () {
    if (have_config == config_probe)
        have_config = config_unknown; // hmm
    if (send_config == send_new)
        send_config = send_no; // oh well
    if (m_process && m_process->isRunning ()) {
        kdDebug () << "CallbackProcessInfo::quit ()" << endl;
        if (backend)
            backend->quit ();
        //else if (media_object && media_object->viewer)
        //    media_object && media_object->viewer->sendKeyEvent ('q');
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
    killProcess (m_process, manager->player ()->view(), false);
}

bool CallbackProcessInfo::getConfigData () {
    if (have_config == config_no)
        return false;
    if (have_config == config_unknown &&
            (!m_process || !m_process->isRunning())) {
        have_config = config_probe;
        startBackend ();
    }
    return true;
}

void CallbackProcessInfo::setChangedData (const QByteArray & data) {
    // lookup a Xine process, or create one
    changed_data.duplicate (data);
    if (m_process && m_process->isRunning ()) {
        send_config = send_try;
        backend->setConfig (data);
    } else {
        send_config = send_new;
        startBackend ();
    }
}

void CallbackProcessInfo::changesReceived () {
    if (send_config != send_no) {
        if (send_new == send_config)
            backend->quit ();
        send_config = send_no;
    }
}

void CallbackProcessInfo::initProcess () {
    setupProcess (&m_process);
    connect (m_process, SIGNAL (processExited (KProcess *)),
            this, SLOT (processStopped (KProcess *)));
    connect (m_process, SIGNAL (receivedStdout (KProcess *, char *, int)),
            this, SLOT (processOutput (KProcess *, char *, int)));
    connect (m_process, SIGNAL (receivedStderr (KProcess *, char *, int)),
            this, SLOT (processOutput (KProcess *, char *, int)));
}

CallbackProcess::CallbackProcess (QObject *parent, ProcessInfo *pi, Settings *settings, const char *n)
 : Process (parent, pi, settings, n),
   in_gui_update (false) {
}

CallbackProcess::~CallbackProcess () {
    kdDebug() << "~CallbackProcess " << process_info->name << endl;
}

void CallbackProcess::setStatusMessage (const QString & /*msg*/) {
}

void CallbackProcess::setErrorMessage (int code, const QString & msg) {
    kdDebug () << "setErrorMessage " << code << " " << msg << endl;
}

void CallbackProcess::setFinished () {
    setState (IProcess::Ready);
}

void CallbackProcess::setPlaying () {
    setState (IProcess::Playing);
}

void CallbackProcess::setMovieParams (int len, int w, int h, float a, const QStringList & alang, const QStringList & slang) {
    Mrl *m = mrl ();
    kdDebug () << "setMovieParams " << len << " " << w << "," << h << " " << a << endl;
    if (!m || !m_source) return;
    in_gui_update = true;
    m_source->setDimensions (m, w, h);
    m_source->setAspect (m, a);
    m_source->setLength (m, len);
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

bool CallbackProcess::deMediafiedPlay () {
    CallbackProcessInfo *cb = static_cast <CallbackProcessInfo *>(process_info);
    if (!cb->backend)
        return false;
    kdDebug () << "CallbackProcess::play " << m_url << endl;
    QString u = m_url;
    m_request_seek = -1;
    if (u == "tv://" && !m_source->tuner ().isEmpty ()) {
        u = "v4l:/" + m_source->tuner ();
        if (m_source->frequency () > 0)
            u += QChar ('/') + QString::number (m_source->frequency ());
    }
    KURL url (u);
    QString myurl = url.isLocalFile () ? getPath (url) : url.url ();
    cb->backend->setURL (widget (), myurl);
    const KURL & sub_url = m_source->subUrl ();
    if (!sub_url.isEmpty ())
        cb->backend->setSubTitleURL (widget (), QString (QFile::encodeName (sub_url.isLocalFile () ? QFileInfo (getPath (sub_url)).absFilePath () : sub_url.url ())));

    Mrl *link = mrl ();
    if (link && Mrl::WindowMode == link->view_mode)
        cb->backend->property (widget (),
                QString ("audiovisualization"), QString ("0"));
    if (m_source->frequency () > 0)
        cb->backend->property (widget (),
                QString ("frequency"), QString::number(m_source->frequency ()));
    if (m_settings->autoadjustcolors) {
        saturation (m_settings->saturation, true);
        hue (m_settings->hue, true);
        brightness (m_settings->brightness, true);
        contrast (m_settings->contrast, true);
    }
    cb->backend->play (widget (), link ? link->repeat : 0);
    setState (IProcess::Buffering);
    return true;
}

bool CallbackProcess::running () const {
    CallbackProcessInfo *cb = static_cast <CallbackProcessInfo *>(process_info);
    return cb->m_process && cb->m_process->isRunning ();
}

void CallbackProcess::stop () {
    CallbackProcessInfo *cb = static_cast <CallbackProcessInfo *>(process_info);
    terminateJobs ();
    if (!running () || m_state < IProcess::Buffering)
        return;
    kdDebug () << "CallbackProcess::stop ()" << cb->backend << endl;
    if (cb->backend)
        cb->backend->stop (widget ());
}

void CallbackProcess::quit () {
    static_cast <CallbackProcessInfo *>(process_info)->stopBackend ();
}

void CallbackProcess::pause () {
    kdDebug() << "pause " << process_info->name << endl;
    CallbackProcessInfo *cb = static_cast <CallbackProcessInfo *>(process_info);
    if (running () && cb->backend)
        cb->backend->pause (widget ());
}

void CallbackProcess::setAudioLang (int id, const QString & al) {
    CallbackProcessInfo *cb = static_cast <CallbackProcessInfo *>(process_info);
    if (cb->backend)
        cb->backend->setAudioLang (widget (), id, al);
}

void CallbackProcess::setSubtitle (int id, const QString & sl) {
    CallbackProcessInfo *cb = static_cast <CallbackProcessInfo *>(process_info);
    if (cb->backend)
        cb->backend->setSubtitle (widget (), id, sl);
}

bool CallbackProcess::seek (int pos, bool absolute) {
    CallbackProcessInfo *cb = static_cast <CallbackProcessInfo *>(process_info);
    if (in_gui_update || !running () ||
            !cb->backend || !m_source ||
            !m_source->hasLength () ||
            (absolute && m_source->position () == pos))
        return false;
    if (!absolute)
        pos = m_source->position () + pos;
    m_source->setPosition (pos);
    if (m_request_seek < 0)
        cb->backend->seek (widget (), pos, true);
    m_request_seek = pos;
    return true;
}

bool CallbackProcess::volume (int val, bool b) {
    CallbackProcessInfo *cb = static_cast <CallbackProcessInfo *>(process_info);
    if (cb->backend)
        cb->backend->volume (widget (), int (sqrt (val*100)), b);
    //cb->backend->volume (100 * log (1.0*val) / log (100.0), b);
    return !!cb->backend;
}

bool CallbackProcess::saturation (int val, bool b) {
    CallbackProcessInfo *cb = static_cast <CallbackProcessInfo *>(process_info);
    if (cb->backend)
        cb->backend->saturation (widget (), val, b);
    return !!cb->backend;
}

bool CallbackProcess::hue (int val, bool b) {
    CallbackProcessInfo *cb = static_cast <CallbackProcessInfo *>(process_info);
    if (cb->backend)
        cb->backend->hue (widget (), val, b);
    return !!cb->backend;
}

bool CallbackProcess::brightness (int val, bool b) {
    CallbackProcessInfo *cb = static_cast <CallbackProcessInfo *>(process_info);
    if (cb->backend)
        cb->backend->brightness (widget (), val, b);
    return !!cb->backend;
}

bool CallbackProcess::contrast (int val, bool b) {
    CallbackProcessInfo *cb = static_cast <CallbackProcessInfo *>(process_info);
    if (cb->backend)
        cb->backend->contrast (widget (), val, b);
    return !!cb->backend;
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT ConfigDocument::ConfigDocument ()
    : Document (QString ()) {}

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
    QString type_attr = getAttribute (StringPool::attr_type);
    const char * ctype = type_attr.ascii ();
    QString value = getAttribute (StringPool::attr_value);
    if (!strcmp (ctype, "range")) {
        w = new QSlider (getAttribute (QString ("START")).toInt (),
                getAttribute (StringPool::attr_end).toInt (),
                1, value.toInt (), Qt::Horizontal, parent);
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
                combo->insertItem (convertNode <Element> (e)->getAttribute (StringPool::attr_value));
        combo->setCurrentItem (value.toInt ());
        w = combo;
    } else if (!strcmp (ctype, "tree")) {
    } else
        kdDebug() << "Unknown type:" << ctype << endl;
    return w;
}

void TypeNode::changedXML (QTextStream & out) {
    if (!w) return;
    QString type_attr = getAttribute (StringPool::attr_type);
    const char * ctype = type_attr.ascii ();
    QString value = getAttribute (StringPool::attr_value);
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
        setAttribute (StringPool::attr_value, newvalue);
        out << outerXML ();
    }
}

//-----------------------------------------------------------------------------

namespace KMPlayer {

class KMPLAYER_NO_EXPORT XMLPreferencesFrame : public QFrame {
public:
    XMLPreferencesFrame (QWidget * parent, CallbackProcessInfo *);
    KDE_NO_CDTOR_EXPORT ~XMLPreferencesFrame () {}
    QTable * table;
protected:
    void showEvent (QShowEvent *);
private:
    CallbackProcessInfo *m_process_info;
};

} // namespace

KDE_NO_CDTOR_EXPORT XMLPreferencesFrame::XMLPreferencesFrame
(QWidget * parent, CallbackProcessInfo *p)
 : QFrame (parent), m_process_info (p){
    QVBoxLayout * layout = new QVBoxLayout (this);
    table = new QTable (this);
    layout->addWidget (table);
}

KDE_NO_CDTOR_EXPORT
XMLPreferencesPage::XMLPreferencesPage (CallbackProcessInfo *pi)
 : m_process_info (pi), m_configframe (NULL) {
}

KDE_NO_CDTOR_EXPORT XMLPreferencesPage::~XMLPreferencesPage () {
}

KDE_NO_EXPORT void XMLPreferencesFrame::showEvent (QShowEvent *) {
    if (!m_process_info->haveConfig ())
        m_process_info->getConfigData ();
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
        NodePtr configdoc = m_process_info->config_doc;
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
            m_process_info->setChangedData (changeddata);
        }
    } else {
        //if (!m_process->haveConfig ())
        //    return;
        NodePtr configdoc = m_process_info->config_doc;
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
                QString name = tn->getAttribute (StringPool::attr_name);
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
    tab = m_process_info->label;
}

KDE_NO_EXPORT QFrame * XMLPreferencesPage::prefPage (QWidget * parent) {
    m_configframe = new XMLPreferencesFrame (parent, m_process_info);
    return m_configframe;
}

//-----------------------------------------------------------------------------

static const char *xine_supports [] = {
    "dvdnavsource", "dvdsource", "exitsource", "introsource", "pipesource",
    "tvsource", "urlsource", "vcdsource", "audiocdsource", 0L
};

XineProcessInfo::XineProcessInfo (MediaManager *mgr)
 : CallbackProcessInfo ("xine", i18n ("&Xine"), xine_supports, mgr,
#ifdef HAVE_XINE
            new XMLPreferencesPage (this)
#else
            NULL
#endif
         ) {}

IProcess *XineProcessInfo::create (PartBase *part, AudioVideoMedia *media) {
    Xine *x = new Xine (part, this, part->settings ());
    x->setSource (part->source ());
    x->media_object = media;
    part->processCreated (x);
    return x;
}

bool XineProcessInfo::startBackend () {
    if (m_process && m_process->isRunning ())
        return true;
    Settings *cfg = manager->player ()->settings();

    initProcess ();

    QString xine_config = KProcess::quote (QString (QFile::encodeName (locateLocal ("data", "kmplayer/") + QString ("xine_config"))));
    fprintf (stderr, "kxineplayer ");
    *m_process << "kxineplayer ";
    fprintf (stderr, " -f %s", xine_config.ascii ());
    *m_process << " -f " << xine_config;

    QString vo = QString (cfg->videodrivers[cfg->videodriver].driver);
    if (!vo.isEmpty ()) {
        fprintf (stderr, " -vo %s", vo.lower().ascii());
        *m_process << " -vo " << vo.lower();
    }
    QString ao = QString (cfg->audiodrivers[cfg->audiodriver].driver);
    if (!ao.isEmpty ()) {
        if (ao.startsWith (QString ("alsa")))
            ao = QString ("alsa");
        fprintf (stderr, " -ao %s", ao.lower().ascii());
        *m_process << " -ao " << ao.lower();
    }
    QString service = dcopName ();
    fprintf (stderr, " -cb %s", service.ascii());
    *m_process << " -cb " << service;
    if (have_config == config_unknown || have_config == config_probe) {
        fprintf (stderr, " -c");
        *m_process << " -c";
    }
    // if (m_source->url ().url ().startsWith (QString ("vcd://")) &&
    // if (m_source->url ().url ().startsWith (QString ("tv://")) &
    fprintf (stderr, " -dvd-device %s", cfg->dvddevice.ascii ());
    *m_process << " -dvd-device " << cfg->dvddevice;
    fprintf (stderr, " -vcd-device %s", cfg->vcddevice.ascii ());
    *m_process << " -vcd-device " << cfg->vcddevice;
    //fprintf (stderr, " -vd %s", m_source->videoDevice ().ascii ());
    //*m_process << " -vd " << m_source->videoDevice ();
    /*if (!m_recordurl.isEmpty ()) {
        QString rf = KProcess::quote (
                QString (QFile::encodeName (getPath (m_recordurl))));
        fprintf (stderr, " -rec %s", rf.ascii ());
        *m_process << " -rec " << rf;
    }*/
    fprintf (stderr, "\n");
    m_process->start (KProcess::NotifyOnExit, KProcess::All);
    return m_process->isRunning ();
}

KDE_NO_CDTOR_EXPORT
Xine::Xine (QObject *parent, ProcessInfo *pinfo, Settings *settings)
 : CallbackProcess (parent, pinfo, settings, "xine") {}

KDE_NO_CDTOR_EXPORT Xine::~Xine () {}

bool Xine::ready () {
    if (media_object && media_object->viewer)
        media_object->viewer->useIndirectWidget (true);
    kdDebug() << "Xine::ready " << state () << endl;
    if (running ()) {
        setState (IProcess::Ready);
        return true;
    }
    return static_cast <XineProcessInfo *> (process_info)->startBackend ();
}

// TODO:input.v4l_video_device_path input.v4l_radio_device_path
// v4l:/Webcam/0   v4l:/Television/21600  v4l:/Radio/96

//-----------------------------------------------------------------------------

static const char * gst_supported [] = {
    "exitsource", "introsource", "urlsource", "vcdsource", "audiocdsource", 0L
};

KDE_NO_CDTOR_EXPORT GStreamer::GStreamer (QObject * parent, Settings * settings)
    : CallbackProcess (parent, NULL, settings, "gstreamer") {
#ifdef HAVE_GSTREAMER
#endif
}

KDE_NO_CDTOR_EXPORT GStreamer::~GStreamer () {}

KDE_NO_EXPORT bool GStreamer::ready () {
    if (media_object && media_object->viewer)
        media_object->viewer->useIndirectWidget (true);
    if (state () >= IProcess::Ready)
        return true;
    initProcess ();
    /*m_request_seek = -1;
    fprintf (stderr, "kgstplayer");
    *m_process << "kgstplayer";

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
    m_process->start (KProcess::NotifyOnExit, KProcess::All);*/
    return m_process->isRunning ();
}

//-----------------------------------------------------------------------------

static const char * ffmpeg_supports [] = {
    "tvsource", "urlsource", 0L
};

FFMpegProcessInfo::FFMpegProcessInfo (MediaManager *mgr)
 : ProcessInfo ("ffmpeg", i18n ("&FFMpeg"), ffmpeg_supports, mgr, NULL) {}

IProcess *FFMpegProcessInfo::create (PartBase *p, AudioVideoMedia *media) {
    FFMpeg *m = new FFMpeg (p, this, p->settings ());
    m->setSource (p->source ());
    m->media_object = media;
    p->processCreated (m);
    return m;
}

FFMpeg::FFMpeg (QObject *parent, ProcessInfo *pinfo, Settings * settings)
 : Process (parent, pinfo, settings, "ffmpeg") {
}

KDE_NO_CDTOR_EXPORT FFMpeg::~FFMpeg () {
}

KDE_NO_EXPORT void FFMpeg::init () {
}

bool FFMpeg::deMediafiedPlay () {
    RecordDocument *rd = recordDocument (media_object);
    if (!rd)
        return false;
    initProcess ();
    KURL url (m_url);
    connect (m_process, SIGNAL (processExited (KProcess *)),
            this, SLOT (processStopped (KProcess *)));
    KURL out (rd->record_file);
    QString outurl = QString (QFile::encodeName (out.isLocalFile ()
                ? getPath (out) : out.url ()));
    if (out.isLocalFile ())
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
        setState (IProcess::Playing);
    return m_process->isRunning ();
}

KDE_NO_EXPORT void FFMpeg::stop () {
    terminateJobs ();
    if (!running ())
        return;
    kdDebug () << "FFMpeg::stop" << endl;
    m_process->writeStdin ("q", 1);
}

KDE_NO_EXPORT void FFMpeg::quit () {
    stop ();
    if (!running ())
        return;
    QTime t;
    t.start ();
    do {
        KProcessController::theKProcessController->waitForProcessExit (2);
    } while (t.elapsed () < 2000 && m_process->isRunning ());
    Process::quit ();
}

KDE_NO_EXPORT void FFMpeg::processStopped (KProcess *) {
    setState (IProcess::NotRunning);
}

//-----------------------------------------------------------------------------

#ifdef HAVE_NSPR

struct KMPLAYER_NO_EXPORT DBusStatic {
    DBusStatic ();
    ~DBusStatic ();
    DBusQt::Connection *connection; // FIXME find a way to detect if already connected
    DBusConnection *dbus_connnection;
};

static DBusStatic * dbus_static = 0L;

DBusStatic::DBusStatic ()
 : connection (new DBusQt::Connection (DBUS_BUS_SESSION, 0L)),
   dbus_connnection (0L) {}

DBusStatic::~DBusStatic () {
    dbus_connection_unref (dbus_connnection);
    delete connection;
    dbus_static = 0L;
}

static KStaticDeleter <DBusStatic> dbus_static_deleter;

//------------------%<---------------------------------------------------------

static DBusHandlerResult
dbusFilter (DBusConnection *conn, DBusMessage *msg, void *data) {
    DBusMessageIter args;
    //const char *iface = "org.kde.kmplayer.backend";
    NpPlayer *process = (NpPlayer *) data;
    const char * iface = process->interface ().ascii ();
    const char * path = dbus_message_get_path (msg);

    if (dbus_message_has_destination (msg, process->destination ().ascii ()) &&
                dbus_message_has_interface (msg, iface) &&
            QString (path).startsWith(process->objectPath ()))
    {
        //kdDebug () << "dbusFilter " << sender <<
        //    " iface:" << dbus_message_get_interface (msg) <<
        //    " member:" << dbus_message_get_member (msg) <<
        //    " dest:" << dbus_message_get_destination (msg) << endl;

        if (dbus_message_is_method_call (msg, iface, "getUrl")) {
            char *param = 0;
            QString url, target;
            if (dbus_message_iter_init (msg, &args) &&
                    DBUS_TYPE_STRING == dbus_message_iter_get_arg_type(&args)) {
                dbus_message_iter_get_basic (&args, &param);
                url = QString::fromLocal8Bit (param);
                if (dbus_message_iter_next (&args) &&
                      DBUS_TYPE_STRING==dbus_message_iter_get_arg_type(&args)) {
                    dbus_message_iter_get_basic (&args, &param);
                    target = QString::fromLocal8Bit (param);
                }
                process->requestStream (path, url, target);
            }
            //kdDebug () << "getUrl " << param << endl;

        } else if (dbus_message_is_method_call (msg, iface, "evaluate")) {
            char *param = 0;
            if (dbus_message_iter_init (msg, &args) &&
                    DBUS_TYPE_STRING == dbus_message_iter_get_arg_type(&args)) {
                dbus_message_iter_get_basic (&args, &param);
                QString r = process->evaluateScript (QString::fromUtf8 (param));
                DBusMessage * rmsg = dbus_message_new_method_return (msg);
                char *res = strdup (r.utf8 ().data ());
                //kdDebug () << "evaluate => " << res << endl;
                dbus_message_append_args (rmsg,
                        DBUS_TYPE_STRING, &res,
                        DBUS_TYPE_INVALID);
                dbus_connection_send (conn, rmsg, NULL);
                dbus_connection_flush (conn);
                dbus_message_unref (rmsg);
                free (res);
            }

        } else if (dbus_message_is_method_call (msg, iface, "destroy")) {
            QString stream =QString(path).mid(process->objectPath().length()+1);
            process->destroyStream (stream);

        } else if (dbus_message_is_method_call (msg, iface, "running")) {
            char *param = 0;
            if (dbus_message_iter_init (msg, &args) &&
                    DBUS_TYPE_STRING == dbus_message_iter_get_arg_type(&args)) {
                dbus_message_iter_get_basic (&args, &param);
                process->setStarted (QString (param));
            }

        } else if (dbus_message_is_method_call (msg, iface, "plugged") &&
                process->view ()) {
            process->view ()->videoStart ();
        } else if (dbus_message_is_method_call (msg, iface, "dimension")) {
            Q_UINT32 w, h;
            if (dbus_message_iter_init (msg, &args) &&
                    DBUS_TYPE_UINT32 == dbus_message_iter_get_arg_type(&args)) {
                dbus_message_iter_get_basic (&args, &w);
                if (dbus_message_iter_next (&args) &&
                  DBUS_TYPE_UINT32 == dbus_message_iter_get_arg_type(&args)) {
                    dbus_message_iter_get_basic (&args, &h);
                    if (h > 0)
                        process->source ()->setAspect (process->mrl(), 1.0*w/h);
                }
            }
        }
        return DBUS_HANDLER_RESULT_HANDLED;
    }
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

KDE_NO_CDTOR_EXPORT
NpStream::NpStream (QObject *p, Q_UINT32 sid, const KURL & u)
 : QObject (p),
   url (u),
   job (0L), bytes (0), content_length (0),
   stream_id (sid),
   finish_reason (NoReason) {
    data_arrival.tv_sec = 0;
}

KDE_NO_CDTOR_EXPORT NpStream::~NpStream () {
    close ();
}

KDE_NO_EXPORT void NpStream::open () {
    kdDebug () << "NpStream " << stream_id << " open " << url.url () << endl;
    if (url.url().startsWith ("javascript:")) {
        NpPlayer *npp = static_cast <NpPlayer *> (parent ());
        QString result = npp->evaluateScript (url.url().mid (11));
        if (!result.isEmpty ()) {
            QCString cr = result.local8Bit ();
            int len = cr.length ();
            pending_buf.resize (len + 1);
            memcpy (pending_buf.data (), cr.data (), len);
            pending_buf.data ()[len] = 0;
            gettimeofday (&data_arrival, 0L);
        }
        kdDebug () << "result is " << pending_buf.data () << endl;
        finish_reason = BecauseDone;
        emit stateChanged ();
    } else {
        job = KIO::get (url, false, false);
        job->addMetaData ("errorPage", "false");
        connect (job, SIGNAL (data (KIO::Job *, const QByteArray &)),
                this, SLOT (slotData (KIO::Job *, const QByteArray &)));
        connect (job, SIGNAL (result (KIO::Job *)),
                this, SLOT (slotResult (KIO::Job *)));
        connect (job, SIGNAL (redirection (KIO::Job *, const KURL &)),
                this, SLOT (redirection (KIO::Job *, const KURL &)));
        connect (job, SIGNAL (mimetype (KIO::Job *, const QString &)),
                SLOT (slotMimetype (KIO::Job *, const QString &)));
        connect (job, SIGNAL (totalSize (KIO::Job *, KIO::filesize_t)),
                SLOT (slotTotalSize (KIO::Job *, KIO::filesize_t)));
    }
}

KDE_NO_EXPORT void NpStream::close () {
    if (job) {
        job->kill (); // quiet, no result signal
        job = 0L;
        finish_reason = BecauseStopped;
        // don't emit stateChanged(), because always triggered from NpPlayer
    }
}

KDE_NO_EXPORT void NpStream::slotResult (KIO::Job *jb) {
    kdDebug() << "slotResult " << bytes << " err:" << jb->error () << endl;
    finish_reason = jb->error () ? BecauseError : BecauseDone;
    job = 0L; // signal KIO::Job::result deletes itself
    emit stateChanged ();
}

KDE_NO_EXPORT void NpStream::slotData (KIO::Job*, const QByteArray& qb) {
    pending_buf = qb; // we suspend job, so qb should be valid until resume
    if (qb.size()) {
        job->suspend ();
        gettimeofday (&data_arrival, 0L);
        emit stateChanged ();
    }
}

KDE_NO_EXPORT void NpStream::redirection (KIO::Job *, const KURL &u) {
    url = u;
    emit redirected (stream_id, url);
}

void NpStream::slotMimetype (KIO::Job *, const QString &mime) {
    mimetype = mime;
}

void NpStream::slotTotalSize (KIO::Job *, KIO::filesize_t sz) {
    content_length = sz;
}

static const char *npp_supports [] = { "urlsource", 0L };

NppProcessInfo::NppProcessInfo (MediaManager *mgr)
 : ProcessInfo ("npp", i18n ("&Ice Ape"), npp_supports, mgr, NULL) {}

IProcess *NppProcessInfo::create (PartBase *p, AudioVideoMedia *media) {
    NpPlayer *n = new NpPlayer (p, this, p->settings(), p->serviceName ());
    n->setSource (p->source ());
    n->media_object = media;
    p->processCreated (n);
    return n;
}

KDE_NO_CDTOR_EXPORT
NpPlayer::NpPlayer (QObject *parent, ProcessInfo *pinfo, Settings *settings, const QString & srv)
 : Process (parent, pinfo, settings, "npp"),
   service (srv),
   write_in_progress (false) {
}

KDE_NO_CDTOR_EXPORT NpPlayer::~NpPlayer () {
    if (!iface.isEmpty ()) {
        DBusError dberr;
        dbus_error_init (&dberr);
        DBusConnection *conn = dbus_static->dbus_connnection;
        if (conn) {
            dbus_bus_remove_match (conn, filter.ascii(), &dberr);
            if (dbus_error_is_set (&dberr))
                dbus_error_free (&dberr);
            dbus_connection_remove_filter (conn, dbusFilter, this);
            dbus_connection_flush (conn);
        }
    }
}

KDE_NO_EXPORT void NpPlayer::init () {
}

KDE_NO_EXPORT void NpPlayer::initProcess () {
    Process::initProcess ();
    connect (m_process, SIGNAL (processExited (KProcess *)),
            this, SLOT (processStopped (KProcess *)));
    connect (m_process, SIGNAL (receivedStdout (KProcess *, char *, int)),
            this, SLOT (processOutput (KProcess *, char *, int)));
    connect (m_process, SIGNAL (receivedStderr (KProcess *, char *, int)),
            this, SLOT (processOutput (KProcess *, char *, int)));
    connect (m_process, SIGNAL (wroteStdin (KProcess *)),
            this, SLOT (wroteStdin (KProcess *)));
    if (!dbus_static)
        dbus_static = dbus_static_deleter.setObject (new DBusStatic ());
    if (iface.isEmpty ()) {
        DBusError dberr;
        iface = QString ("org.kde.kmplayer.callback");
        static int count = 0;
        path = QString ("/npplayer%1").arg (count++);
        filter = QString ("type='method_call',interface='org.kde.kmplayer.callback'");

        dbus_error_init (&dberr);
        DBusConnection *conn = dbus_bus_get (DBUS_BUS_SESSION, &dberr);
        if (dbus_error_is_set (&dberr))
            dbus_error_free (&dberr);
        if (!conn) {
            kdError () << "Failed to get dbus connection: " << dberr.message << endl;
            return;
        }
        bool has_service = !service.isEmpty();
        if (has_service) { // standalone kmplayer
            dbus_bus_request_name (conn, service.ascii(),
                    DBUS_NAME_FLAG_DO_NOT_QUEUE, &dberr);
            if (dbus_error_is_set (&dberr)) {
                kdError () << "Failed to register name " << service << ": " << dberr.message;
                dbus_error_free (&dberr);
                has_service = false;
            }
        }
        if (!has_service) // plugin, accept what-is [sic]
            service = QString (dbus_bus_get_unique_name (conn));
        kdDebug() << "using service " << service << " interface " << iface << endl;
        dbus_bus_add_match (conn, filter.ascii(), &dberr);
        if (dbus_error_is_set (&dberr)) {
            kdError () << "Failed to set match " << filter << ": " <<  dberr.message << endl;
            dbus_error_free (&dberr);
        }
        dbus_connection_add_filter (conn, dbusFilter, this, 0L);
        dbus_connection_flush (conn);
        dbus_static->dbus_connnection = conn;
    }
}

KDE_NO_EXPORT bool NpPlayer::deMediafiedPlay () {
    kdDebug() << "NpPlayer::play '" << m_url << "'" << endl;
    // if we change from XPLAIN to XEMBED, the DestroyNotify may come later
    Mrl *node = mrl ();
    if (!view ())
        return false;
    if (media_object && media_object->viewer) {
        media_object->viewer->useIndirectWidget (false);
        media_object->viewer->setMonitoring (IViewer::MonitorNothing);
    }
    if (node && !m_url.isEmpty () && dbus_static->dbus_connnection) {
        QString mime = "text/plain";
        QString plugin;
        Element *elm = node;
        if (elm->id == id_node_html_object) {
            // this sucks to have to do this here ..
            for (NodePtr n = elm->firstChild (); n; n = n->nextSibling ())
                if (n->id == KMPlayer::id_node_html_embed) {
                    elm = convertNode <Element> (n);
                    break;
                }
        }
        for (NodePtr n = node; n; n = n->parentNode ()) {
            Mrl *m = n->mrl ();
            if (m && m_base_url.isEmpty ())
                m_base_url = m->getAttribute ("pluginbaseurl");
            if (m && !m->mimetype.isEmpty ()) {
                plugin = m_source->plugin (m->mimetype);
                kdDebug() << "search plugin " << m->mimetype << "->" << plugin << endl;
                if (!plugin.isEmpty ()) {
                    mime = m->mimetype;
                    break;
                }
            }
        }
        if (!plugin.isEmpty ()) {
            DBusMessage *msg = dbus_message_new_method_call (
                    remote_service.ascii(),
                    "/plugin",
                    "org.kde.kmplayer.backend",
                    "play");
            char *c_url = strdup (m_url.local8Bit().data ());
            char *c_mime = strdup (mime.ascii ());
            char *c_plugin = strdup (plugin.ascii ());
            DBusMessageIter it;
            dbus_message_iter_init_append (msg, &it);
            dbus_message_iter_append_basic (&it, DBUS_TYPE_STRING, &c_url);
            dbus_message_iter_append_basic (&it, DBUS_TYPE_STRING, &c_mime);
            dbus_message_iter_append_basic (&it, DBUS_TYPE_STRING, &c_plugin);
            unsigned int param_len = elm->attributes ()->length ();
            char **argn = (char **) malloc (param_len * sizeof (char *));
            char **argv = (char **) malloc (param_len * sizeof (char *));
            dbus_message_iter_append_basic (&it, DBUS_TYPE_UINT32, &param_len);
            DBusMessageIter ait;
            dbus_message_iter_open_container (&it, DBUS_TYPE_ARRAY,"{ss}",&ait);
            AttributePtr a = elm->attributes ()->first ();
            for (int i = 0; i < param_len && a; i++, a = a->nextSibling ()) {
                DBusMessageIter dit;
                dbus_message_iter_open_container (&ait,
                        DBUS_TYPE_DICT_ENTRY,
                        NULL,
                        &dit);
                argn[i] = strdup (a->name ().toString ().local8Bit().data ());
                argv[i] = strdup (a->value ().local8Bit().data ());
                dbus_message_iter_append_basic (&dit, DBUS_TYPE_STRING, &argn[i]);
                dbus_message_iter_append_basic (&dit, DBUS_TYPE_STRING, &argv[i]);
                dbus_message_iter_close_container (&ait, &dit);
            }
            dbus_message_iter_close_container (&it, &ait);
            dbus_message_set_no_reply (msg, TRUE);
            dbus_connection_send (dbus_static->dbus_connnection, msg, NULL);
            dbus_message_unref (msg);
            dbus_connection_flush (dbus_static->dbus_connnection);
            free (c_url);
            free (c_mime);
            free (c_plugin);
            for (int i = 0; i < param_len; i++) {
                free (argn[i]);
                free (argv[i]);
            }
            free (argn);
            free (argv);
            setState (IProcess::Buffering);
            return true;
        }
    }
    stop ();
    return false;
}

KDE_NO_EXPORT bool NpPlayer::ready () {
    if (!media_object || !media_object->viewer)
        return false;
    // FIXME wait for callback
    media_object->viewer->useIndirectWidget (false);
    if (state () == IProcess::Ready)
        return true;
    initProcess ();
    kdDebug() << "NpPlayer::ready" << endl;
    QString cmd ("knpplayer");
    cmd += QString (" -cb ");
    cmd += service;
    cmd += path;
    cmd += QString (" -wid ");
    cmd += QString::number (media_object->viewer->windowHandle ());
    fprintf (stderr, "%s\n", cmd.local8Bit ().data ());
    *m_process << cmd;
    m_process->start (KProcess::NotifyOnExit, KProcess::All);
    return m_process->isRunning ();
}

KDE_NO_EXPORT void NpPlayer::setStarted (const QString & srv) {
    remote_service = srv;
    kdDebug () << "NpPlayer::setStarted " << srv << endl;
    setState (IProcess::Ready);
}

KDE_NO_EXPORT QString NpPlayer::evaluateScript (const QString & script) {
    QString result;
    emit evaluate (script, result);
    //kdDebug () << "evaluateScript " << script << " => " << result << endl;
    return result;
}

static int getStreamId (const QString &path) {
    int p = path.findRev (QChar ('_'));
    if (p < 0) {
        kdError() << "wrong object path " << path << endl;
        return -1;
    }
    bool ok;
    Q_UINT32 sid = path.mid (p+1).toInt (&ok);
    if (!ok) {
        kdError() << "wrong object path suffix " << path.mid (p+1) << endl;
        return -1;
    }
    return sid;
}

KDE_NO_EXPORT
void NpPlayer::requestStream (const QString &path, const QString & url, const QString & target) {
    KURL uri (m_base_url.isEmpty () ? m_url : m_base_url, url);
    kdDebug () << "NpPlayer::request " << path << " '" << uri << "'" << endl;
    Q_UINT32 sid = getStreamId (path);
    if (sid >= 0) {
        if (!target.isEmpty ()) {
            kdDebug () << "new page request " << target << endl;
            if (url.startsWith ("javascript:")) {
                QString result = evaluateScript (url.mid (11));
                kdDebug() << "result is " << result << endl;
                if (result == "undefined")
                    uri = KURL ();
                else
                    uri = KURL (m_url, result); // probably wrong ..
            }
            if (uri.isValid ())
                emit openUrl (uri, target);
            sendFinish (sid, 0, NpStream::BecauseDone);
        } else {
            NpStream * ns = new NpStream (this, sid, uri);
            connect (ns, SIGNAL (stateChanged ()),
                    this, SLOT (streamStateChanged ()));
            streams[sid] = ns;
            if (url != uri.url ())
                streamRedirected (sid, uri.url ());
            if (!write_in_progress)
                processStreams ();
        }
    }
}

KDE_NO_EXPORT void NpPlayer::destroyStream (const QString &s) {
    int sid = getStreamId (s);
    if (sid >= 0 && streams.contains ((Q_UINT32) sid)) {
        NpStream *ns = streams[(Q_UINT32) sid];
        ns->close ();
        if (!write_in_progress)
            processStreams ();
    } else {
        kdWarning () << "Object " << s << " not found" << endl;
    }
}

KDE_NO_EXPORT
void NpPlayer::sendFinish (Q_UINT32 sid, Q_UINT32 bytes, NpStream::Reason because) {
    if (running () && dbus_static->dbus_connnection) {
        Q_UINT32 reason = (int) because;
        QString objpath = QString ("/plugin/stream_%1").arg (sid);
        DBusMessage *msg = dbus_message_new_method_call (
                remote_service.ascii(),
                objpath.ascii (),
                "org.kde.kmplayer.backend",
                "eof");
        dbus_message_append_args(msg,
                DBUS_TYPE_UINT32, &bytes,
                DBUS_TYPE_UINT32, &reason,
                DBUS_TYPE_INVALID);
        dbus_message_set_no_reply (msg, TRUE);
        dbus_connection_send (dbus_static->dbus_connnection, msg, NULL);
        dbus_message_unref (msg);
        dbus_connection_flush (dbus_static->dbus_connnection);
    }
}

KDE_NO_EXPORT void NpPlayer::terminateJobs () {
    Process::terminateJobs ();
    const StreamMap::iterator e = streams.end ();
    for (StreamMap::iterator i = streams.begin (); i != e; ++i)
        delete i.data ();
    streams.clear ();
}

KDE_NO_EXPORT void NpPlayer::stop () {
    terminateJobs ();
    if (!running ())
        return;
    kdDebug () << "NpPlayer::stop " << endl;
    if (dbus_static->dbus_connnection) {
        DBusMessage *msg = dbus_message_new_method_call (
                remote_service.ascii(),
                "/plugin",
                "org.kde.kmplayer.backend",
                "quit");
        dbus_message_set_no_reply (msg, TRUE);
        dbus_connection_send (dbus_static->dbus_connnection, msg, NULL);
        dbus_message_unref (msg);
        dbus_connection_flush (dbus_static->dbus_connnection);
    }
}

KDE_NO_EXPORT void NpPlayer::quit () {
    if (running ()) {
        stop ();
        QTime t;
        t.start ();
        do {
            KProcessController::theKProcessController->waitForProcessExit (2);
        } while (t.elapsed () < 2000 && m_process->isRunning ());
        return Process::quit ();
    }
}

KDE_NO_EXPORT void NpPlayer::processOutput (KProcess *, char * str, int slen) {
    if (view () && slen > 0)
        view ()->addText (QString::fromLocal8Bit (str, slen));
}

KDE_NO_EXPORT void NpPlayer::processStopped (KProcess *) {
    terminateJobs ();
    if (m_source)
        ((PlayListNotify *) m_source)->setInfoMessage (QString ());
    setState (IProcess::NotRunning);
}

KDE_NO_EXPORT void NpPlayer::streamStateChanged () {
    setState (IProcess::Playing); // hmm, this doesn't really fit in current states
    if (!write_in_progress)
        processStreams ();
}

KDE_NO_EXPORT void NpPlayer::streamRedirected (Q_UINT32 sid, const KURL &u) {
    if (running () && dbus_static->dbus_connnection) {
        kdDebug() << "redirected " << sid << " to " << u.url() << endl;
        char *cu = strdup (u.url ().local8Bit().data ());
        QString objpath = QString ("/plugin/stream_%1").arg (sid);
        DBusMessage *msg = dbus_message_new_method_call (
                remote_service.ascii(),
                objpath.ascii (),
                "org.kde.kmplayer.backend",
                "redirected");
        dbus_message_append_args(msg, DBUS_TYPE_STRING, &cu, DBUS_TYPE_INVALID);
        dbus_message_set_no_reply (msg, TRUE);
        dbus_connection_send (dbus_static->dbus_connnection, msg, NULL);
        dbus_message_unref (msg);
        dbus_connection_flush (dbus_static->dbus_connnection);
        free (cu);
    }
}

KDE_NO_EXPORT void NpPlayer::processStreams () {
    NpStream *stream = 0L;
    Q_UINT32 stream_id;
    timeval tv = { 0x7fffffff, 0 };
    const StreamMap::iterator e = streams.end ();
    int active_count = 0;
    //kdDebug() << "NpPlayer::processStreams " << streams.size() << endl;
    for (StreamMap::iterator i = streams.begin (); i != e;) {
        NpStream *ns = i.data ();
        if (ns->job) {
            active_count++;
        } else if (active_count < 5 &&
                ns->finish_reason == NpStream::NoReason) {
            write_in_progress = true; // javascript: urls emit stateChange
            ns->open ();
            write_in_progress = false;
            if (ns->job) {
                connect (ns, SIGNAL (redirected (Q_UINT32, const KURL&)),
                        this, SLOT (streamRedirected (Q_UINT32, const KURL&)));
                active_count++;
            }
        }
        if (ns->finish_reason == NpStream::BecauseStopped ||
                ns->finish_reason == NpStream::BecauseError ||
                (ns->finish_reason == NpStream::BecauseDone &&
                 ns->pending_buf.size () == 0)) {
            sendFinish (i.key(), ns->bytes, ns->finish_reason);
            StreamMap::iterator ii = i;
            ++ii;
            streams.erase (i);
            i = ii;
            delete ns;
        } else {
            if (ns->pending_buf.size () > 0 &&
                    (ns->data_arrival.tv_sec < tv.tv_sec ||
                     (ns->data_arrival.tv_sec == tv.tv_sec &&
                      ns->data_arrival.tv_usec < tv.tv_usec))) {
                tv = ns->data_arrival;
                stream = ns;
                stream_id = i.key();
            }
            ++i;
        }
    }
    //kdDebug() << "NpPlayer::processStreams " << stream << endl;
    if (stream) {
        if (dbus_static->dbus_connnection &&
                !stream->bytes &&
                (!stream->mimetype.isEmpty() || stream->content_length)) {
            char *mt = strdup (stream->mimetype.isEmpty ()
                    ? ""
                    : stream->mimetype.utf8 ().data ());
            QString objpath=QString("/plugin/stream_%1").arg(stream->stream_id);
            DBusMessage *msg = dbus_message_new_method_call (
                    remote_service.ascii(),
                    objpath.ascii (),
                    "org.kde.kmplayer.backend",
                    "streamInfo");
            dbus_message_append_args (msg,
                    DBUS_TYPE_STRING, &mt,
                    DBUS_TYPE_UINT32, &stream->content_length,
                    DBUS_TYPE_INVALID);
            dbus_message_set_no_reply (msg, TRUE);
            dbus_connection_send (dbus_static->dbus_connnection, msg, NULL);
            dbus_message_unref (msg);
            dbus_connection_flush (dbus_static->dbus_connnection);
            free (mt);
        }
        const int header_len = 2 * sizeof (Q_UINT32);
        Q_UINT32 chunk = stream->pending_buf.size();
        send_buf.resize (chunk + header_len);
        memcpy (send_buf.data (), &stream_id, sizeof (Q_UINT32));
        memcpy (send_buf.data() + sizeof (Q_UINT32), &chunk, sizeof (Q_UINT32));
        memcpy (send_buf.data()+header_len, stream->pending_buf.data (), chunk);
        stream->pending_buf = QByteArray ();
        /*fprintf (stderr, " => %d %d\n", (long)stream_id, chunk);*/
        stream->bytes += chunk;
        write_in_progress = true;
        m_process->writeStdin (send_buf.data (), send_buf.size ());
        if (stream->finish_reason == NpStream::NoReason)
            stream->job->resume ();
    }
}

KDE_NO_EXPORT void NpPlayer::wroteStdin (KProcess *) {
    write_in_progress = false;
    if (running ())
        processStreams ();
}

#else

KDE_NO_CDTOR_EXPORT
NpStream::NpStream (QObject *p, Q_UINT32, const KURL & url)
    : QObject (p) {}

KDE_NO_CDTOR_EXPORT NpStream::~NpStream () {}
void NpStream::slotResult (KIO::Job*) {}
void NpStream::slotData (KIO::Job*, const QByteArray&) {}
void NpStream::redirection (KIO::Job *, const KURL &) {}
void NpStream::slotMimetype (KIO::Job *, const QString &) {}
void NpStream::slotTotalSize (KIO::Job *, KIO::filesize_t) {}

KDE_NO_CDTOR_EXPORT
NpPlayer::NpPlayer (QObject * parent, Settings * settings, const QString &)
 : Process (parent, settings, "npp") {}
KDE_NO_CDTOR_EXPORT NpPlayer::~NpPlayer () {}
KDE_NO_EXPORT void NpPlayer::init () {}
KDE_NO_EXPORT bool NpPlayer::deMediafiedPlay () { return false; }
KDE_NO_EXPORT void NpPlayer::initProcess () {}
KDE_NO_EXPORT void NpPlayer::setStarted (const QString &) {}
KDE_NO_EXPORT void NpPlayer::stop () {}
KDE_NO_EXPORT void NpPlayer::quit () { return false; }
KDE_NO_EXPORT bool NpPlayer::ready () { return false; }
KDE_NO_EXPORT void NpPlayer::processOutput (KProcess *, char *, int) {}
KDE_NO_EXPORT void NpPlayer::processStopped (KProcess *) {}
KDE_NO_EXPORT void NpPlayer::wroteStdin (KProcess *) {}
KDE_NO_EXPORT void NpPlayer::streamStateChanged () {}
KDE_NO_EXPORT void NpPlayer::streamRedirected (Q_UINT32, const KURL &) {}
KDE_NO_EXPORT void NpPlayer::terminateJobs () {}

#endif

#include "kmplayerprocess.moc"
