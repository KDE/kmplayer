/*
    SPDX-FileCopyrightText: 2003 Koos Vriezen <koos.vriezen@xs4all.nl>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include <cmath>
#include "config-kmplayer.h"
#include <unistd.h>
#include <QString>
#include <QFile>
#include <QFileInfo>
#include <QTimer>
#include <QLayout>
#include <QTableWidget>
#include <QLineEdit>
#include <QSlider>
#include <QComboBox>
#include <QCheckBox>
#include <QSpinBox>
#include <QLabel>
#include <QFontMetrics>
#include <QWhatsThis>
#include <QList>
#include <QDir>
#include <QUrl>
#include <QHeaderView>
#include <QNetworkCookie>

#include <KProtocolManager>
#include <KMessageBox>
#include <KLocalizedString>
#include <KShell>
#include <KIO/Job>
#include <KIO/AccessManager>

#include "kmplayercommon_log.h"
#include "kmplayerconfig.h"
#include "kmplayerview.h"
#include "kmplayercontrolpanel.h"
#include "kmplayerprocess.h"
#include "kmplayerpartbase.h"
#include "masteradaptor.h"
#include "streammasteradaptor.h"
#ifdef KMPLAYER_WITH_NPP
# include "callbackadaptor.h"
# include "streamadaptor.h"
#endif

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

static QString getPath (const QUrl & url) {
    QString p = QUrl::fromPercentEncoding (url.url ().toLatin1 ());
    if (p.startsWith (QString ("file:/"))) {
        int i = 0;
        p = p.mid (5);
        for (; i < p.size () && p[i] == QChar ('/'); ++i)
            ;
        //qCDebug(LOG_KMPLAYER_COMMON) << "getPath " << p.mid (i-1);
        if (i > 0)
            return p.mid (i-1);
        return QString (QChar ('/') + p);
    }
    return p;
}

static QString encodeFileOrUrl (const QUrl &url)
{
    return url.isEmpty ()
        ? QString ()
        : QString::fromLocal8Bit (QFile::encodeName (
                    url.isLocalFile ()
                    ? url.toLocalFile ()
                    : QUrl::fromPercentEncoding (url.url ().toLocal8Bit ())));
}

static QString encodeFileOrUrl (const QString &str)
{
    if (!str.startsWith (QString ("dvd:")) &&
            !str.startsWith (QString ("vcd:")) &&
            !str.startsWith (QString ("tv:")) &&
            !str.startsWith (QString ("cdda:")))
        return encodeFileOrUrl (QUrl::fromUserInput(str));
    return str;
}

static void setupProcess (QProcess **process)
{
    delete *process;
    *process = new QProcess;
    QStringList env = (*process)->systemEnvironment ();
    const QStringList::iterator e = env.end ();
    for (QStringList::iterator i = env.begin (); i != e; ++i)
        if ((*i).startsWith ("SESSION_MANAGER")) {
            env.erase (i);
            break;
        }
    (*process)->setEnvironment (env);
}

static void killProcess (QProcess *process, QWidget *widget) {
    if (!process || !process->pid ())
        return;
    process->terminate ();
    if (!process->waitForFinished (1000)) {
        process->kill ();
        if (!process->waitForFinished (1000) && widget)
            KMessageBox::error (widget,
                    i18n ("Failed to end player process."), i18n ("Error"));
    }
}

static void outputToView (View *view, const QByteArray &ba)
{
    if (view && ba.size ())
        view->addText (QString::fromLocal8Bit (ba.constData ()));
}

Process::Process (QObject *parent, ProcessInfo *pinfo, Settings *settings)
 : QObject (parent),
   IProcess (pinfo),
   m_source (nullptr),
   m_settings (settings),
   m_old_state (IProcess::NotRunning),
   m_process (nullptr),
   m_job(nullptr),
   m_process_state (QProcess::NotRunning)
{}

Process::~Process () {
    quit ();
    delete m_process;
    if (user)
        user->processDestroyed (this);
}

void Process::init () {
}

void Process::initProcess () {
    setupProcess (&m_process);
    m_process_state = QProcess::NotRunning;
    connect (m_process, SIGNAL (stateChanged (QProcess::ProcessState)),
            this, SLOT (processStateChanged (QProcess::ProcessState)));
    if (m_source) m_source->setPosition (0);
}

WId Process::widget () {
    return view () && user && user->viewer ()
        ? user->viewer ()->windowHandle ()
        : 0;
}

Mrl *Process::mrl () const {
    if (user)
        return user->getMrl ();
    return nullptr;
}

static bool processRunning (QProcess *process) {
    return process && process->state () > QProcess::NotRunning;
}

bool Process::running () const {
    return processRunning (m_process);
}

void Process::setAudioLang (int) {}

void Process::setSubtitle (int) {}

void Process::pause () {
}

void Process::unpause () {
}

bool Process::seek (int /*pos*/, bool /*absolute*/) {
    return false;
}

void Process::volume (int /*pos*/, bool /*absolute*/) {
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

bool Process::grabPicture (const QString &/*file*/, int /*pos*/) {
    m_old_state = m_state = Buffering;
    setState (Ready);
    return false;
}

void Process::stop () {
}

void Process::quit () {
    killProcess (m_process, view ());
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

void Process::rescheduledStateChanged () {
    IProcess::State old_state = m_old_state;
    m_old_state = m_state;
    if (user) {
        user->stateChange (this, old_state, m_state);
    } else {
        if (m_state > IProcess::Ready)
            qCCritical(LOG_KMPLAYER_COMMON) << "Process running, mrl disappeared" << endl;
        delete this;
    }
}

bool Process::play () {
    Mrl *m = mrl ();
    if (!m)
        return false;
    bool nonstdurl = m->src.startsWith ("tv:/") ||
        m->src.startsWith ("dvd:") ||
        m->src.startsWith ("cdda:") ||
        m->src.startsWith ("vcd:");
    QString url = nonstdurl ? m->src : m->absolutePath ();
    bool changed = m_url != url;
    m_url = url;
    if (user) // FIXME: remove check
        user->starting (this);
    const QUrl u = QUrl::fromUserInput(m_url);
    if (!changed ||
            u.isLocalFile () ||
            nonstdurl ||
            (m_source && m_source->avoidRedirects ()))
        return deMediafiedPlay ();
    m_job = KIO::stat (u, KIO::HideProgressInfo);
    connect (m_job, SIGNAL (result (KJob *)), this, SLOT (result (KJob *)));
    return true;
}

bool Process::deMediafiedPlay () {
    return false;
}

void Process::result (KJob * job) {
    KIO::UDSEntry entry = static_cast <KIO::StatJob *> (job)->statResult ();
    QString url = entry.stringValue (KIO::UDSEntry::UDS_LOCAL_PATH);
    if (!url.isEmpty ())
        m_url = url;
    m_job = nullptr;
    deMediafiedPlay ();
}

void Process::terminateJobs () {
    if (m_job) {
        m_job->kill ();
        m_job = nullptr;
    }
}

bool Process::ready () {
    setState (IProcess::Ready);
    return true;
}

void Process::processStateChanged (QProcess::ProcessState nstate)
{
    if (QProcess::Starting == m_process_state) {
        if (QProcess::NotRunning == nstate)
            setState (IProcess::NotRunning);
        else if (state () == IProcess::Ready)
            setState (IProcess::Buffering);
        m_process_state = nstate;
    }
}

void Process::startProcess (const QString &program, const QStringList &args)
{
    m_process_state = QProcess::Starting;
    m_process->start (program, args);
}

View *Process::view () const {
    return m_source ? m_source->player ()->viewWidget () : nullptr;
}

//-----------------------------------------------------------------------------

RecordDocument::RecordDocument (const QString &url, const QString &rurl,
        const QString &rec, Source *src)
 : SourceDocument (src, url),
   record_file (rurl),
   recorder (rec) {
    id = id_node_record_document;
}

void RecordDocument::begin () {
    if (!media_info) {
        media_info = new MediaInfo (this, MediaManager::AudioVideo);
        media_info->create ();
    }
    media_info->media->play ();
}

void RecordDocument::message (MessageType msg, void *content) {
    switch (msg) {
    case MsgMediaFinished:
        deactivate ();
        break;
    default:
        SourceDocument::message (msg, content);
    }
}

void RecordDocument::deactivate () {
    state = state_deactivated;
    ((MediaManager *) role (RoleMediaManager))->player ()->recorderStopped ();
    Document::deactivate ();
}

static RecordDocument *recordDocument (ProcessUser *user) {
    Mrl *mrl = user ? user->getMrl () : nullptr;
    return mrl && id_node_record_document == mrl->id
        ? static_cast <RecordDocument *> (mrl) : nullptr;
}

//-----------------------------------------------------------------------------

static bool proxyForURL (const QUrl &url, QString &proxy) {
    KProtocolManager::slaveProtocol (url, proxy);
    return !proxy.isNull ();
}

//-----------------------------------------------------------------------------

MPlayerBase::MPlayerBase (QObject *parent, ProcessInfo *pinfo, Settings * settings)
    : Process (parent, pinfo, settings),
      m_needs_restarted (false) {
    m_process = new QProcess;
}

MPlayerBase::~MPlayerBase () {
}

void MPlayerBase::initProcess () {
    Process::initProcess ();
    const QUrl &url  = m_source->url ();
    if (!url.isEmpty ()) {
        QString proxy_url;
        if (KProtocolManager::useProxy () && proxyForURL (url, proxy_url)) {
            QStringList env = m_process->environment ();
            env << (QString ("http_proxy=") + proxy_url);
            m_process->setEnvironment (env);
        }
    }
    connect (m_process, SIGNAL (bytesWritten (qint64)),
            this, SLOT (dataWritten (qint64)));
    connect (m_process, SIGNAL (finished (int, QProcess::ExitStatus)),
            this, SLOT (processStopped (int, QProcess::ExitStatus)));
}

bool MPlayerBase::removeQueued (const char *cmd) {
    for (QList<QByteArray>::iterator i = commands.begin ();
            i != commands.end ();
            ++i)
        if (!strncmp ((*i).data (), cmd, strlen (cmd))) {
            commands.erase (i);
            return true;
        }
    return false;
}

bool MPlayerBase::sendCommand (const QString & cmd) {
    if (running ()) {
        commands.push_front (QString (cmd + '\n').toLatin1 ());
        fprintf (stderr, "eval %s", commands.last ().constData ());
        if (commands.size () < 2)
            m_process->write (commands.last ());
        return true;
    }
    return false;
}

void MPlayerBase::stop () {
    terminateJobs ();
}

void MPlayerBase::quit () {
    if (running ()) {
        qCDebug(LOG_KMPLAYER_COMMON) << "MPlayerBase::quit";
        stop ();
        disconnect (m_process, SIGNAL (finished (int, QProcess::ExitStatus)),
                this, SLOT (processStopped (int, QProcess::ExitStatus)));
        m_process->waitForFinished (2000);
        if (running ())
            Process::quit ();
        commands.clear ();
        m_needs_restarted = false;
        processStopped ();
    }
    Process::quit ();
}

void MPlayerBase::dataWritten (qint64) {
    if (!commands.size ()) return;
    qCDebug(LOG_KMPLAYER_COMMON) << "eval done " << commands.last ().data ();
    commands.pop_back ();
    if (commands.size ())
        m_process->write (commands.last ());
}

void MPlayerBase::processStopped () {
    setState (IProcess::Ready);
}

void MPlayerBase::processStopped (int, QProcess::ExitStatus) {
    qCDebug(LOG_KMPLAYER_COMMON) << "process stopped" << endl;
    commands.clear ();
    processStopped ();
}

//-----------------------------------------------------------------------------

static const char *mplayer_supports [] = {
    "dvdsource", "exitsource", "introsource", "pipesource", "tvscanner", "tvsource", "urlsource", "vcdsource", "audiocdsource", nullptr
};

MPlayerProcessInfo::MPlayerProcessInfo (MediaManager *mgr)
 : ProcessInfo ("mplayer", i18n ("&MPlayer"), mplayer_supports,
         mgr, new MPlayerPreferencesPage ()) {}

IProcess *MPlayerProcessInfo::create (PartBase *part, ProcessUser *usr) {
    MPlayer *m = new MPlayer (part, this, part->settings ());
    m->setSource (part->source ());
    m->user = usr;
    part->processCreated (m);
    return m;
}

MPlayer::MPlayer (QObject *parent, ProcessInfo *pinfo, Settings *settings)
 : MPlayerBase (parent, pinfo, settings),
   m_widget (nullptr),
   m_transition_state (NotRunning),
   aid (-1), sid (-1)
{}

MPlayer::~MPlayer () {
    if (m_widget && !m_widget->parent ())
        delete m_widget;
}

void MPlayer::init () {
}

bool MPlayer::ready () {
    Process::ready ();
    if (user && user->viewer ())
        user->viewer ()->useIndirectWidget (true);
    return false;
}

bool MPlayer::deMediafiedPlay () {
    if (running ())
        return sendCommand (QString ("gui_play"));

    m_transition_state = NotRunning;
    if (!m_needs_restarted && running ())
        quit (); // rescheduling of setState will reset state just-in-time

    initProcess ();
    connect (m_process, SIGNAL (readyReadStandardOutput ()),
            this, SLOT (processOutput ()));
    connect (m_process, SIGNAL (readyReadStandardError ()),
            this, SLOT (processOutput ()));

    m_process_output = QString ();
    m_source->setPosition (0);
    if (!m_needs_restarted) {
        if (m_source->identified ()) {
            aid = m_source->audioLangId ();
            sid = m_source->subTitleId ();
        } else {
            aid = sid = -1;
        }
    } else {
        m_needs_restarted = false;
    }
    alanglist = nullptr;
    slanglist = nullptr;
    slanglist_end = nullptr;
    alanglist_end = nullptr;
    m_request_seek = -1;
    m_tmpURL.truncate (0);

    QStringList args;
    //m_view->consoleOutput ()->clear ();
    MPlayerPreferencesPage *cfg_page = static_cast <MPlayerPreferencesPage *>(process_info->config_page);
    QString exe = cfg_page->mplayer_path;
    if (exe.isEmpty ())
        exe = "mplayer";

    args << "-wid" << QString::number (widget ());
    args << "-slave";

    QString strVideoDriver = QString (m_settings->videodrivers[m_settings->videodriver].driver);
    if (!strVideoDriver.isEmpty ()) {
        args << "-vo" << strVideoDriver.toLower();
        if (view () && view ()->keepSizeRatio () &&
                strVideoDriver.toLower() == QString::fromLatin1 ("x11"))
            args << "-zoom";
    }

    QString strAudioDriver = QString (m_settings->audiodrivers[m_settings->audiodriver].driver);
    if (!strAudioDriver.isEmpty ())
        args << "-ao" << strAudioDriver.toLower();

    if (m_settings->framedrop)
        args << "-framedrop";

    if (cfg_page->additionalarguments.length () > 0)
        args << KShell::splitArgs (cfg_page->additionalarguments);

    // postproc thingies
    args << KShell::splitArgs (m_source->filterOptions ());

    if (m_settings->autoadjustcolors) {
        args << "-contrast" << QString::number (m_settings->contrast);
        args << "-brightness" <<QString::number(m_settings->brightness);
        args << "-hue" << QString::number (m_settings->hue);
        args << "-saturation" <<QString::number(m_settings->saturation);
    }

    if (aid > -1)
        args << "-aid" << QString::number (aid);

    if (sid > -1)
        args << "-sid" << QString::number (sid);

    for (Node *n = mrl (); n; n = n->parentNode ()) {
        if (n->id != id_node_group_node && n->id != id_node_playlist_item)
            break;
        QString plops = static_cast<Element *>(n)->getAttribute ("mplayeropts");
        if (!plops.isNull ()) {
            QStringList sl = plops.split (QChar (' '));
            for (int i = 0; i < sl.size (); ++i)
                args << sl[i];
            break;
        }
    }

    args << KShell::splitArgs (m_source->options ());

    const QUrl url = QUrl::fromUserInput(m_url);
    if (!url.isEmpty ()) {
        if (m_source->url ().isLocalFile ())
            m_process->setWorkingDirectory
                (QFileInfo (m_source->url ().path ()).absolutePath ());
        if (url.isLocalFile ()) {
            m_url = url.toLocalFile ();
            if (cfg_page->alwaysbuildindex &&
                    (m_url.toLower ().endsWith (".avi") ||
                     m_url.toLower ().endsWith (".divx")))
                args << "-idx";
        } else {
            int cache = cfg_page->cachesize;
            if (cache > 3 && !url.url ().startsWith (QString ("dvd")) &&
                    !url.url ().startsWith (QString ("vcd")) &&
                    !m_url.startsWith (QString ("tv://")))
                args << "-cache" << QString::number (cache);
            if (m_url.startsWith (QString ("cdda:/")) &&
                    !m_url.startsWith (QString ("cdda://")))
                m_url = QString ("cdda://") + m_url.mid (6);
        }
        if (url.scheme () != QString ("stdin"))
            args << encodeFileOrUrl (m_url);
    }
    Mrl *m = mrl ();
    if (m && m->repeat > 0)
        args << "-loop" << QString::number (m->repeat);
    else if (m_settings->loop)
        args << "-loop" << nullptr;
    args << "-identify";
    const QString surl = encodeFileOrUrl (m_source->subUrl ());
    if (!surl.isEmpty ())
        args << "-sub" << surl;
    qCDebug(LOG_KMPLAYER_COMMON, "mplayer %s\n", args.join (" ").toLocal8Bit ().constData ());

    startProcess (exe, args);

    old_volume = view () ? view ()->controlPanel ()->volumeBar ()->value () : 0;

    return true;
}

void MPlayer::stop () {
    terminateJobs ();
    if (!m_source || !running ())
        return;
    sendCommand (QString ("quit"));
    MPlayerBase::stop ();
}

void MPlayer::pause () {
    if (Paused != m_transition_state) {
        m_transition_state = Paused;
        if (!removeQueued ("pause"))
            sendCommand (QString ("pause"));
    }
}

void MPlayer::unpause () {
    if (m_transition_state == Paused
            || (Paused == m_state
                && m_transition_state != Playing)) {
        m_transition_state = Playing;
        if (!removeQueued ("pause"))
            sendCommand (QString ("pause"));
    }
}

bool MPlayer::seek (int pos, bool absolute) {
    if (!m_source || !m_source->hasLength () ||
            (absolute && m_source->position () == pos))
        return false;
    if (m_request_seek >= 0 && commands.size () > 1) {
        QList<QByteArray>::iterator i = commands.begin ();
        for (++i; i != commands.end (); ++i)
            if (!strncmp ((*i).data (), "seek", 4)) {
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

void MPlayer::volume (int incdec, bool absolute) {
    if (absolute)
        incdec -= old_volume;
    if (incdec == 0)
        return;
    old_volume += incdec;
    sendCommand (QString ("volume ") + QString::number (incdec));
}

bool MPlayer::saturation (int val, bool absolute) {
    QString cmd;
    cmd.sprintf ("saturation %d %d", val, absolute ? 1 : 0);
    return sendCommand (cmd);
}

bool MPlayer::hue (int val, bool absolute) {
    QString cmd;
    cmd.sprintf ("hue %d %d", val, absolute ? 1 : 0);
    return sendCommand (cmd);
}

bool MPlayer::contrast (int val, bool /*absolute*/) {
    QString cmd;
    cmd.sprintf ("contrast %d 1", val);
    return sendCommand (cmd);
}

bool MPlayer::brightness (int val, bool /*absolute*/) {
    QString cmd;
    cmd.sprintf ("brightness %d 1", val);
    return sendCommand (cmd);
}

bool MPlayer::grabPicture (const QString &file, int pos) {
    Mrl *m = mrl ();
    if (m_state > Ready || !m || m->src.isEmpty ())
        return false; //FIXME
    initProcess ();
    m_old_state = m_state = Buffering;
    unlink (file.toLatin1 ().constData ());
    QByteArray ba = file.toLocal8Bit ();
    ba.append ("XXXXXX");
    if (mkdtemp ((char *) ba.constData ())) {
        m_grab_dir = QString::fromLocal8Bit (ba.constData ());
        QString exe ("mplayer");
        QStringList args;
        QString jpgopts ("jpeg:outdir=");
        jpgopts += KShell::quoteArg (m_grab_dir);
        args << "-vo" << jpgopts;
        args << "-frames" << "1" << "-nosound" << "-quiet";
        if (pos > 0)
            args << "-ss" << QString::number (pos);
        args << encodeFileOrUrl (m->src);
        qCDebug(LOG_KMPLAYER_COMMON) << args.join (" ");
        m_process->start (exe, args);
        if (m_process->waitForStarted ()) {
            m_grab_file = file;
            setState (Playing);
            return true;
        } else {
            rmdir (ba.constData ());
            m_grab_dir.truncate (0);
        }
    } else {
        qCCritical(LOG_KMPLAYER_COMMON) << "mkdtemp failure";
    }
    setState (Ready);
    return false;
}

void MPlayer::processOutput () {
    const QByteArray ba = m_process->readAllStandardOutput ();
    const char *str = ba.constData ();
    int slen = ba.size ();
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
            if (m_posRegExp.indexIn (out) > -1) {
                if (m_source->hasLength ()) {
                    int pos = int (10.0 * m_posRegExp.cap (1).toFloat ());
                    m_source->setPosition (pos);
                    m_request_seek = -1;
                }
                if (Playing == m_transition_state) {
                    m_transition_state = NotRunning;
                    setState (Playing);
                }
            } else if (m_cacheRegExp.indexIn (out) > -1) {
                m_source->setLoading (int (m_cacheRegExp.cap(1).toDouble()));
            }
        } else if (out.startsWith ("ID_LENGTH")) {
            int pos = out.indexOf ('=');
            if (pos > 0) {
                int l = (int) out.mid (pos + 1).toDouble (&ok);
                if (ok && l >= 0) {
                    m_source->setLength (mrl (), 10 * l);
                }
            }
        } else if (out.startsWith ("ID_PAUSED")) {
            if (Paused == m_transition_state) {
                m_transition_state = NotRunning;
                setState (Paused);
            }
        } else if (m_refURLRegExp.indexIn(out) > -1) {
            qCDebug(LOG_KMPLAYER_COMMON) << "Reference mrl " << m_refURLRegExp.cap (1);
            if (!m_tmpURL.isEmpty () &&
                    (m_url.endsWith (m_tmpURL) || m_tmpURL.endsWith (m_url)))
                m_source->insertURL (mrl (), m_tmpURL);;
            const QUrl tmp = QUrl::fromUserInput(m_refURLRegExp.cap (1));
            m_tmpURL = tmp.isLocalFile () ? tmp.toLocalFile () : tmp.url ();
            if (m_source->url () == tmp ||
                    m_url.endsWith (m_tmpURL) || m_tmpURL.endsWith (m_url))
                m_tmpURL.truncate (0);
        } else if (m_refRegExp.indexIn (out) > -1) {
            qCDebug(LOG_KMPLAYER_COMMON) << "Reference File ";
            m_tmpURL.truncate (0);
        } else if (out.startsWith ("ID_VIDEO_WIDTH")) {
            int pos = out.indexOf ('=');
            if (pos > 0) {
                int w = out.mid (pos + 1).toInt ();
                m_source->setDimensions (mrl (), w, m_source->height ());
            }
        } else if (out.startsWith ("ID_VIDEO_HEIGHT")) {
            int pos = out.indexOf ('=');
            if (pos > 0) {
                int h = out.mid (pos + 1).toInt ();
                m_source->setDimensions (mrl (), m_source->width (), h);
            }
        } else if (out.startsWith ("ID_VIDEO_ASPECT")) {
            int pos = out.indexOf ('=');
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
            int pos = out.indexOf ('_', 7);
            if (pos > 0) {
                int id = out.mid (7, pos - 7).toInt ();
                pos = out.indexOf ('=', pos);
                if (pos > 0) {
                    if (!alanglist_end) {
                        alanglist = new Source::LangInfo (id, out.mid (pos + 1));
                        alanglist_end = alanglist;
                    } else {
                        alanglist_end->next = new Source::LangInfo (id, out.mid(pos+1));
                        alanglist_end = alanglist_end->next;
                    }
                    qCDebug(LOG_KMPLAYER_COMMON) << "lang " << id << " " << alanglist_end->name;
                }
            }
        } else if (out.startsWith ("ID_SID_")) {
            int pos = out.indexOf ('_', 7);
            if (pos > 0) {
                int id = out.mid (7, pos - 7).toInt ();
                pos = out.indexOf ('=', pos);
                if (pos > 0) {
                    if (!slanglist_end) {
                        slanglist = new Source::LangInfo (id, out.mid (pos + 1));
                        slanglist_end = slanglist;
                    } else {
                        slanglist_end->next = new Source::LangInfo (id, out.mid(pos+1));
                        slanglist_end = slanglist_end->next;
                    }
                    qCDebug(LOG_KMPLAYER_COMMON) << "sid " << id << " " << slanglist_end->name;
                }
            }
        } else if (out.startsWith ("ICY Info")) {
            int p = out.indexOf ("StreamTitle=", 8);
            if (p > -1) {
                p += 12;
                int e = out.indexOf (';', p);
                if (e > -1)
                    e -= p;
                QString inf = out.mid (p, e);
                mrl ()->document ()->message (MsgInfoString, &inf);
            }
        } else if (v) {
            QRegExp & m_startRegExp = patterns[MPlayerPreferencesPage::pat_start];
            QRegExp & m_sizeRegExp = patterns[MPlayerPreferencesPage::pat_size];
            v->addText (out, true);
            if (!m_source->processOutput (out)) {
                // int movie_width = m_source->width ();
                if (/*movie_width <= 0 &&*/ m_sizeRegExp.indexIn (out) > -1) {
                    int movie_width = m_sizeRegExp.cap (1).toInt (&ok);
                    int movie_height = ok ? m_sizeRegExp.cap (2).toInt (&ok) : 0;
                    if (ok && movie_width > 0 && movie_height > 0) {
                        m_source->setDimensions(mrl(),movie_width,movie_height);
                        m_source->setAspect (mrl(), 1.0*movie_width/movie_height);
                    }
                } else if (m_startRegExp.indexIn (out) > -1) {
                    if (!m_tmpURL.isEmpty () && m_url != m_tmpURL) {
                        m_source->insertURL (mrl (), m_tmpURL);;
                        m_tmpURL.truncate (0);
                    }
                    m_source->setIdentified ();
                    m_source->setLanguages (alanglist, slanglist);
                    m_source->setLoading (100);
                    setState (IProcess::Playing);
                    m_source->setPosition (0);
                }
            }
        }
    } while (slen > 0);
}

void MPlayer::processStopped () {
    if (mrl ()) {
        QString url;
        if (!m_grab_dir.isEmpty ()) {
            QDir dir (m_grab_dir);
            QStringList files = dir.entryList ();
            bool renamed = false;
            for (int i = 0; i < files.size (); ++i) {
                qCDebug(LOG_KMPLAYER_COMMON) << files[i];
                if (files[i] == "." || files[i] == "..")
                    continue;
                if (!renamed) {
                    qCDebug(LOG_KMPLAYER_COMMON) << "rename " << dir.filePath (files[i]) << "->" << m_grab_file;
                    renamed = true;
                    ::rename (dir.filePath (files[i]).toLocal8Bit().constData(),
                            m_grab_file.toLocal8Bit ().constData ());
                } else {
                    qCDebug(LOG_KMPLAYER_COMMON) << "rm " << files[i];
                    dir.remove (files[i]);
                }
            }
            QString dirname = dir.dirName ();
            dir.cdUp ();
            qCDebug(LOG_KMPLAYER_COMMON) << m_grab_dir << " " << files.size () << " rmdir " << dirname;
            dir.rmdir (dirname);
        }
        if (m_source && m_needs_restarted) {
            commands.clear ();
            int pos = m_source->position ();
            play ();
            seek (pos, true);
            return;
        }
    }
    setState (IProcess::Ready);
}

void MPlayer::setAudioLang (int id) {
    aid = id;
    m_needs_restarted = true;
    sendCommand (QString ("quit"));
}

void MPlayer::setSubtitle (int id) {
    sid = id;
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
    KLocalizedString caption;
    const char * name;
    const char * pattern;
} _mplayer_patterns [] = {
    { ki18n ("Size pattern"), "Movie Size", "VO:.*[^0-9]([0-9]+)x([0-9]+)" },
    { ki18n ("Cache pattern"), "Cache Fill", "Cache fill:[^0-9]*([0-9\\.]+)%" },
    { ki18n ("Position pattern"), "Movie Position", "[AV]:\\s*([0-9\\.]+)" },
    { ki18n ("Index pattern"), "Index Pattern", "Generating Index: +([0-9]+)%" },
    { ki18n ("Reference URL pattern"), "Reference URL Pattern", "Playing\\s+(.*[^\\.])\\.?\\s*$" },
    { ki18n ("Reference pattern"), "Reference Pattern", "Reference Media file" },
    { ki18n ("Start pattern"), "Start Playing", "Start[^ ]* play" },
    { ki18n ("VCD track pattern"), "VCD Tracks", "track ([0-9]+):" },
    { ki18n ("Audio CD tracks pattern"), "CDROM Tracks", "[Aa]udio CD[^0-9]+([0-9]+)[^0-9]tracks" }
};

namespace KMPlayer {

class MPlayerPreferencesFrame : public QFrame
{
public:
    MPlayerPreferencesFrame (QWidget * parent);
    QTableWidget * table;
};

} // namespace

MPlayerPreferencesFrame::MPlayerPreferencesFrame (QWidget * parent)
 : QFrame (parent) {
    QVBoxLayout * layout = new QVBoxLayout (this);
    table = new QTableWidget (int (MPlayerPreferencesPage::pat_last)+non_patterns, 2, this);
    table->verticalHeader ()->setVisible (false);
    table->horizontalHeader ()->setVisible (false);
    table->setContentsMargins (0, 0, 0, 0);
    table->setItem (0, 0, new QTableWidgetItem (i18n ("MPlayer command:")));
    table->setItem (0, 1, new QTableWidgetItem ());
    table->setItem (1, 0, new QTableWidgetItem (i18n ("Additional command line arguments:")));
    table->setItem (1, 1, new QTableWidgetItem ());
    table->setItem (2, 0, new QTableWidgetItem (QString("%1 (%2)").arg (i18n ("Cache size:")).arg (i18n ("kB")))); // FIXME for new translations
    QSpinBox* spin = new QSpinBox(table->viewport());
    spin->setMaximum(32767);
    spin->setSingleStep(32);
    table->setCellWidget (2, 1, spin);
    table->setItem (3, 0, new QTableWidgetItem (i18n ("Build new index when possible")));
    table->setCellWidget (3, 1, new QCheckBox (table->viewport()));
    table->cellWidget (3, 1)->setWhatsThis(i18n ("Allows seeking in indexed files (AVIs)"));
    for (int i = 0; i < int (MPlayerPreferencesPage::pat_last); i++) {
        table->setItem (i+non_patterns, 0, new QTableWidgetItem (_mplayer_patterns[i].caption.toString()));
        table->setItem (i+non_patterns, 1, new QTableWidgetItem ());
    }
    for (int i = 0; i < non_patterns + int (MPlayerPreferencesPage::pat_last); i++) {
        QTableWidgetItem *item = table->itemAt (i, 0);
        item->setFlags (item->flags () ^ Qt::ItemIsEditable);
    }
    table->horizontalHeader ()->setSectionResizeMode (QHeaderView::ResizeToContents);
    table->horizontalHeader ()->setStretchLastSection (true);
    table->resizeRowsToContents ();
    layout->addWidget (table);
}

MPlayerPreferencesPage::MPlayerPreferencesPage ()
 : m_configframe (nullptr) {
}

void MPlayerPreferencesPage::write (KSharedConfigPtr config) {
    KConfigGroup patterns_cfg (config, strMPlayerPatternGroup);
    for (int i = 0; i < int (pat_last); i++)
        patterns_cfg.writeEntry
            (_mplayer_patterns[i].name, m_patterns[i].pattern ());
    KConfigGroup mplayer_cfg (config, strMPlayerGroup);
    mplayer_cfg.writeEntry (strMPlayerPath, mplayer_path);
    mplayer_cfg.writeEntry (strAddArgs, additionalarguments);
    mplayer_cfg.writeEntry (strCacheSize, cachesize);
    mplayer_cfg.writeEntry (strAlwaysBuildIndex, alwaysbuildindex);
}

void MPlayerPreferencesPage::read (KSharedConfigPtr config) {
    KConfigGroup patterns_cfg (config, strMPlayerPatternGroup);
    for (int i = 0; i < int (pat_last); i++)
        m_patterns[i].setPattern (patterns_cfg.readEntry
                (_mplayer_patterns[i].name, _mplayer_patterns[i].pattern));
    KConfigGroup mplayer_cfg (config, strMPlayerGroup);
    mplayer_path = mplayer_cfg.readEntry (strMPlayerPath, QString ("mplayer"));
    additionalarguments = mplayer_cfg.readEntry (strAddArgs, QString ());
    cachesize = mplayer_cfg.readEntry (strCacheSize, 384);
    alwaysbuildindex = mplayer_cfg.readEntry (strAlwaysBuildIndex, false);
}

void MPlayerPreferencesPage::sync (bool fromUI) {
    QTableWidget * table = m_configframe->table;
    QSpinBox * cacheSize = static_cast<QSpinBox *>(table->cellWidget (2, 1));
    QCheckBox * buildIndex = static_cast<QCheckBox *>(table->cellWidget (3, 1));
    if (fromUI) {
        mplayer_path = table->item (0, 1)->text ();
        additionalarguments = table->item (1, 1)->text ();
        for (int i = 0; i < int (pat_last); i++)
            m_patterns[i].setPattern (table->item (i+non_patterns, 1)->text ());
        cachesize = cacheSize->value();
        alwaysbuildindex = buildIndex->isChecked ();
    } else {
        table->item (0, 1)->setText (mplayer_path);
        table->item (1, 1)->setText (additionalarguments);
        for (int i = 0; i < int (pat_last); i++)
            table->item (i+non_patterns, 1)->setText (m_patterns[i].pattern ());
        if (cachesize > 0)
            cacheSize->setValue(cachesize);
        buildIndex->setChecked (alwaysbuildindex);
    }
}

void MPlayerPreferencesPage::prefLocation (QString & item, QString & icon, QString & tab) {
    item = i18n ("General Options");
    icon = QString ("kmplayer");
    tab = i18n ("MPlayer");
}

QFrame * MPlayerPreferencesPage::prefPage (QWidget * parent) {
    m_configframe = new MPlayerPreferencesFrame (parent);
    return m_configframe;
}

//-----------------------------------------------------------------------------

static const char * mencoder_supports [] = {
    "dvdsource", "pipesource", "tvscanner", "tvsource", "urlsource",
    "vcdsource", "audiocdsource", nullptr
};

MEncoderProcessInfo::MEncoderProcessInfo (MediaManager *mgr)
 : ProcessInfo ("mencoder", i18n ("M&Encoder"), mencoder_supports,
         mgr, nullptr) {}

IProcess *MEncoderProcessInfo::create (PartBase *part, ProcessUser *usr) {
    MEncoder *m = new MEncoder (part, this, part->settings ());
    m->setSource (part->source ());
    m->user = usr;
    part->processCreated (m);
    return m;
}

MEncoder::MEncoder (QObject * parent, ProcessInfo *pinfo, Settings * settings)
 : MPlayerBase (parent, pinfo, settings) {}

MEncoder::~MEncoder () {
}

void MEncoder::init () {
}

bool MEncoder::deMediafiedPlay () {
    stop ();
    RecordDocument *rd = recordDocument (user);
    if (!rd)
        return false;
    initProcess ();
    QString exe ("mencoder");
    QString margs = m_settings->mencoderarguments;
    if (m_settings->recordcopy)
        margs = QString ("-oac copy -ovc copy");
    QStringList args = KShell::splitArgs (margs);
    if (m_source)
        args << KShell::splitArgs (m_source->recordCmd ());
    // FIXME if (m_player->source () == source) // ugly
    //    m_player->stop ();
    QString myurl = encodeFileOrUrl (m_url);
    if (!myurl.isEmpty ())
        args << myurl;
    args << "-o" << encodeFileOrUrl (rd->record_file);
    startProcess (exe, args);
    qCDebug(LOG_KMPLAYER_COMMON, "mencoder %s\n", args.join (" ").toLocal8Bit ().constData ());
    if (m_process->waitForStarted ()) {
        setState (Playing);
        return true;
    }
    stop ();
    return false;
}

void MEncoder::stop () {
    terminateJobs ();
    if (running ()) {
        qCDebug(LOG_KMPLAYER_COMMON) << "MEncoder::stop ()";
        Process::quit ();
        MPlayerBase::stop ();
    }
}

//-----------------------------------------------------------------------------

static const char * mplayerdump_supports [] = {
    "dvdsource", "pipesource", "tvscanner", "tvsource", "urlsource", "vcdsource", "audiocdsource", nullptr
};

MPlayerDumpProcessInfo::MPlayerDumpProcessInfo (MediaManager *mgr)
 : ProcessInfo ("mplayerdumpstream", i18n ("&MPlayerDumpstream"),
         mplayerdump_supports, mgr, nullptr) {}

IProcess *MPlayerDumpProcessInfo::create (PartBase *p, ProcessUser *usr) {
    MPlayerDumpstream *m = new MPlayerDumpstream (p, this, p->settings ());
    m->setSource (p->source ());
    m->user = usr;
    p->processCreated (m);
    return m;
}

MPlayerDumpstream::MPlayerDumpstream (QObject *p, ProcessInfo *pi, Settings *s)
 : MPlayerBase (p, pi, s) {}

MPlayerDumpstream::~MPlayerDumpstream () {
}

void MPlayerDumpstream::init () {
}

bool MPlayerDumpstream::deMediafiedPlay () {
    stop ();
    RecordDocument *rd = recordDocument (user);
    if (!rd)
        return false;
    initProcess ();
    QString exe ("mplayer");
    QStringList args;
    args << KShell::splitArgs (m_source->recordCmd ());
    // FIXME if (m_player->source () == source) // ugly
    //    m_player->stop ();
    QString myurl = encodeFileOrUrl (m_url);
    if (!myurl.isEmpty ())
        args << myurl;
    args << "-dumpstream" << "-dumpfile" << encodeFileOrUrl (rd->record_file);
    qCDebug(LOG_KMPLAYER_COMMON, "mplayer %s\n", args.join (" ").toLocal8Bit ().constData ());
    startProcess (exe, args);
    if (m_process->waitForStarted ()) {
        setState (Playing);
        return true;
    }
    stop ();
    return false;
}

void MPlayerDumpstream::stop () {
    terminateJobs ();
    if (!m_source || !running ())
        return;
    qCDebug(LOG_KMPLAYER_COMMON) << "MPlayerDumpstream::stop";
    if (running ())
        Process::quit ();
    MPlayerBase::stop ();
}

//-----------------------------------------------------------------------------

MasterProcessInfo::MasterProcessInfo (const char *nm, const QString &lbl,
            const char **supported, MediaManager *mgr, PreferencesPage *pp)
 : ProcessInfo (nm, lbl, supported, mgr, pp),
   m_slave (nullptr) {}

MasterProcessInfo::~MasterProcessInfo () {
    stopSlave ();
}

void MasterProcessInfo::initSlave () {
    if (m_path.isEmpty ()) {
        static int count = 0;
        m_path = QString ("/master_%1").arg (count++);
        (void) new MasterAdaptor (this);
        QDBusConnection::sessionBus().registerObject (m_path, this);
        m_service = QDBusConnection::sessionBus().baseService ();
    }
    setupProcess (&m_slave);
    connect (m_slave, SIGNAL (finished (int, QProcess::ExitStatus)),
            this, SLOT (slaveStopped (int, QProcess::ExitStatus)));
    connect (m_slave, SIGNAL (readyReadStandardOutput ()),
            this, SLOT (slaveOutput ()));
    connect (m_slave, SIGNAL (readyReadStandardError ()),
            this, SLOT (slaveOutput ()));
}

void MasterProcessInfo::quitProcesses () {
    stopSlave ();
}

void MasterProcessInfo::stopSlave () {
    if (!m_slave_service.isEmpty ()) {
        QDBusMessage msg = QDBusMessage::createMethodCall (
                m_slave_service, QString ("/%1").arg (ProcessInfo::name),
                "org.kde.kmplayer.Slave", "quit");
        //msg << m_url << mime << plugin << param_len;
        msg.setDelayedReply (false);
        QDBusConnection::sessionBus().send (msg);
    }
    if (processRunning (m_slave)) {
        m_slave->waitForFinished (1000);
        killProcess (m_slave, manager->player ()->view ());
    }
}

void MasterProcessInfo::running (const QString &srv) {
    qCDebug(LOG_KMPLAYER_COMMON) << "MasterProcessInfo::running " << srv;
    m_slave_service = srv;
    MediaManager::ProcessList &pl = manager->processes ();
    const MediaManager::ProcessList::iterator e = pl.end ();
    for (MediaManager::ProcessList::iterator i = pl.begin (); i != e; ++i)
        if (this == (*i)->process_info)
            static_cast <Process *> (*i)->setState (IProcess::Ready);
}

void MasterProcessInfo::slaveStopped (int, QProcess::ExitStatus) {
    m_slave_service.truncate (0);
    MediaManager::ProcessList &pl = manager->processes ();
    const MediaManager::ProcessList::iterator e = pl.end ();
    for (MediaManager::ProcessList::iterator i = pl.begin (); i != e; ++i)
        if (this == (*i)->process_info)
            static_cast <Process *> (*i)->setState (IProcess::NotRunning);
}

void MasterProcessInfo::slaveOutput () {
    outputToView(manager->player()->viewWidget(), m_slave->readAllStandardOutput());
    outputToView(manager->player()->viewWidget(), m_slave->readAllStandardError ());
}

MasterProcess::MasterProcess (QObject *parent, ProcessInfo *pinfo, Settings *settings)
 : Process (parent, pinfo, settings) {}

MasterProcess::~MasterProcess () {
}

void MasterProcess::init () {
}

bool MasterProcess::deMediafiedPlay () {
    WindowId wid = user->viewer ()->windowHandle ();
    m_slave_path = QString ("/stream_%1").arg (wid);
    MasterProcessInfo *mpi = static_cast <MasterProcessInfo *>(process_info);
    qCDebug(LOG_KMPLAYER_COMMON) << "MasterProcess::deMediafiedPlay " << m_url << " " << wid;

    (void) new StreamMasterAdaptor (this);
    QDBusConnection::sessionBus().registerObject (
            QString ("%1/stream_%2").arg (mpi->m_path).arg (wid), this);

    QDBusMessage msg = QDBusMessage::createMethodCall (
            mpi->m_slave_service, QString ("/%1").arg (process_info->name),
                "org.kde.kmplayer.Slave", "newStream");
    if (!m_url.startsWith ("dvd:") ||
            !m_url.startsWith ("vcd:") ||
            !m_url.startsWith ("cdda:")) {
        const QUrl url = QUrl::fromUserInput(m_url);
        if (url.isLocalFile ())
            m_url = getPath (url);
    }
    msg << m_url << (qulonglong)wid;
    msg.setDelayedReply (false);
    QDBusConnection::sessionBus().send (msg);
    setState (IProcess::Buffering);
    return true;
}

bool MasterProcess::running () const {
    MasterProcessInfo *mpi = static_cast <MasterProcessInfo *>(process_info);
    return processRunning (mpi->m_slave);
}

void MasterProcess::loading (int perc) {
    process_info->manager->player ()->setLoaded (perc);
}

void MasterProcess::streamInfo (uint64_t length, double aspect) {
    qCDebug(LOG_KMPLAYER_COMMON) << length;
    m_source->setLength (mrl (), length);
    m_source->setAspect (mrl (), aspect);
}

void MasterProcess::streamMetaInfo (QString info) {
    m_source->document ()->message (MsgInfoString, &info);
}

void MasterProcess::playing () {
    process_info->manager->player ()->setLoaded (100);
    setState (IProcess::Playing);
}

void MasterProcess::progress (uint64_t pos) {
    m_source->setPosition (pos);
}

void MasterProcess::pause () {
    if (IProcess::Playing == m_state) {
        MasterProcessInfo *mpi = static_cast<MasterProcessInfo *>(process_info);
        QDBusMessage msg = QDBusMessage::createMethodCall (
                mpi->m_slave_service,
                m_slave_path,
                "org.kde.kmplayer.StreamSlave",
                "pause");
        msg.setDelayedReply (false);
        QDBusConnection::sessionBus().send (msg);
    }
}

void MasterProcess::unpause () {
    pause ();
}

bool MasterProcess::seek (int pos, bool) {
    if (IProcess::Playing == m_state) {
        MasterProcessInfo *mpi = static_cast<MasterProcessInfo *>(process_info);
        QDBusMessage msg = QDBusMessage::createMethodCall (
                mpi->m_slave_service,
                m_slave_path,
                "org.kde.kmplayer.StreamSlave",
                "seek");
        msg << (qulonglong) pos << true;
        msg.setDelayedReply (false);
        QDBusConnection::sessionBus().send (msg);
        return true;
    }
    return false;
}

void MasterProcess::volume (int incdec, bool) {
    if (IProcess::Playing == m_state) {
        MasterProcessInfo *mpi = static_cast<MasterProcessInfo *>(process_info);
        QDBusMessage msg = QDBusMessage::createMethodCall (
                mpi->m_slave_service,
                m_slave_path,
                "org.kde.kmplayer.StreamSlave",
                "volume");
        msg << incdec;
        msg.setDelayedReply (false);
        QDBusConnection::sessionBus().send (msg);
    }
}

void MasterProcess::eof () {
    setState (IProcess::Ready);
}

void MasterProcess::stop () {
    if (m_state > IProcess::Ready) {
        MasterProcessInfo *mpi = static_cast<MasterProcessInfo *>(process_info);
        QDBusMessage msg = QDBusMessage::createMethodCall (
                mpi->m_slave_service,
                m_slave_path,
                "org.kde.kmplayer.StreamSlave",
                "stop");
        msg.setDelayedReply (false);
        QDBusConnection::sessionBus().send (msg);
    }
}

//-------------------------%<--------------------------------------------------

static const char *phonon_supports [] = {
    "urlsource", "dvdsource", "vcdsource", "audiocdsource", nullptr
};

PhononProcessInfo::PhononProcessInfo (MediaManager *mgr)
  : MasterProcessInfo ("phonon", i18n ("&Phonon"), phonon_supports, mgr, nullptr)
{}

IProcess *PhononProcessInfo::create (PartBase *part, ProcessUser *usr) {
    if (!processRunning (m_slave))
        startSlave ();
    Phonon *p = new Phonon (part, this, part->settings ());
    p->setSource (part->source ());
    p->user = usr;
    part->processCreated (p);
    return p;
}

bool PhononProcessInfo::startSlave () {
    initSlave ();
    QString exe ("kphononplayer");
    QStringList args;
    args << "-cb" << (m_service + m_path);
    qCDebug(LOG_KMPLAYER_COMMON, "kphononplayer %s", args.join (" ").toLocal8Bit ().constData ());
    m_slave->start (exe, args);
    return true;
}

Phonon::Phonon (QObject *parent, ProcessInfo *pinfo, Settings *settings)
 : MasterProcess (parent, pinfo, settings) {}

bool Phonon::ready () {
    if (user && user->viewer ())
        user->viewer ()->useIndirectWidget (false);
    qCDebug(LOG_KMPLAYER_COMMON) << "Phonon::ready " << state () << endl;
    PhononProcessInfo *ppi = static_cast <PhononProcessInfo *>(process_info);
    if (running ()) {
        if (!ppi->m_slave_service.isEmpty ())
            setState (IProcess::Ready);
        return true;
    } else {
        return ppi->startSlave ();
    }
}

//-----------------------------------------------------------------------------

ConfigDocument::ConfigDocument ()
    : Document (QString ()) {}

ConfigDocument::~ConfigDocument () {
    qCDebug(LOG_KMPLAYER_COMMON) << "~ConfigDocument";
}

namespace KMPlayer {
    /*
     * Element for ConfigDocument
     */
    struct SomeNode : public ConfigNode {
        SomeNode (NodePtr & d, const QString & t)
            : ConfigNode (d, t) {}
        ~SomeNode () override {}
        Node *childFromTag (const QString & t) override;
    };
} // namespace

ConfigNode::ConfigNode (NodePtr & d, const QString & t)
    : DarkNode (d, t.toUtf8 ()), w (nullptr) {}

Node *ConfigDocument::childFromTag (const QString & tag) {
    if (tag.toLower () == QString ("document"))
        return new ConfigNode (m_doc, tag);
    return nullptr;
}

Node *ConfigNode::childFromTag (const QString & t) {
    return new TypeNode (m_doc, t);
}

TypeNode::TypeNode (NodePtr & d, const QString & t)
 : ConfigNode (d, t), tag (t) {}

Node *TypeNode::childFromTag (const QString & tag) {
    return new SomeNode (m_doc, tag);
}

Node *SomeNode::childFromTag (const QString & t) {
    return new SomeNode (m_doc, t);
}

QWidget * TypeNode::createWidget (QWidget * parent) {
    QByteArray ba = getAttribute (Ids::attr_type).toLatin1 ();
    const char *ctype = ba.constData ();
    QString value = getAttribute (Ids::attr_value);
    if (!strcmp (ctype, "range")) {
        QSlider* slider = new QSlider (parent);
        slider->setMinimum(getAttribute (QString ("START")).toInt ());
        slider->setMaximum(getAttribute (Ids::attr_end).toInt ());
        slider->setPageStep(1);
        slider->setOrientation(Qt::Horizontal);
        slider->setValue(value.toInt ());
        w = slider;
    } else if (!strcmp (ctype, "num") || !strcmp (ctype,  "string")) {
        w = new QLineEdit (value, parent);
    } else if (!strcmp (ctype, "bool")) {
        QCheckBox * checkbox = new QCheckBox (parent);
        checkbox->setChecked (value.toInt ());
        w = checkbox;
    } else if (!strcmp (ctype, "enum")) {
        QComboBox * combo = new QComboBox (parent);
        for (Node *e = firstChild (); e; e = e->nextSibling ())
            if (e->isElementNode () && !strcmp (e->nodeName (), "item"))
                combo->addItem (static_cast <Element *> (e)->getAttribute (Ids::attr_value));
        combo->setCurrentIndex (value.toInt ());
        w = combo;
    } else if (!strcmp (ctype, "tree")) {
    } else
        qCDebug(LOG_KMPLAYER_COMMON) << "Unknown type:" << ctype;
    return w;
}

void TypeNode::changedXML (QTextStream & out) {
    if (!w) return;
    QByteArray ba = getAttribute (Ids::attr_type).toLatin1 ();
    const char *ctype = ba.constData ();
    QString value = getAttribute (Ids::attr_value);
    QString newvalue;
    if (!strcmp (ctype, "range")) {
        newvalue = QString::number (static_cast <QSlider *> (w)->value ());
    } else if (!strcmp (ctype, "num") || !strcmp (ctype,  "string")) {
        newvalue = static_cast <QLineEdit *> (w)->text ();
    } else if (!strcmp (ctype, "bool")) {
        newvalue = QString::number (static_cast <QCheckBox *> (w)->isChecked());
    } else if (!strcmp (ctype, "enum")) {
        newvalue = QString::number (static_cast<QComboBox *>(w)->currentIndex());
    } else if (!strcmp (ctype, "tree")) {
    } else
        qCDebug(LOG_KMPLAYER_COMMON) << "Unknown type:" << ctype;
    if (value != newvalue) {
        value = newvalue;
        setAttribute (Ids::attr_value, newvalue);
        out << outerXML ();
    }
}

//-----------------------------------------------------------------------------

static const char * ffmpeg_supports [] = {
    "tvsource", "urlsource", nullptr
};

FFMpegProcessInfo::FFMpegProcessInfo (MediaManager *mgr)
 : ProcessInfo ("ffmpeg", i18n ("&FFMpeg"), ffmpeg_supports, mgr, nullptr) {}

IProcess *FFMpegProcessInfo::create (PartBase *p, ProcessUser *usr) {
    FFMpeg *m = new FFMpeg (p, this, p->settings ());
    m->setSource (p->source ());
    m->user = usr;
    p->processCreated (m);
    return m;
}

FFMpeg::FFMpeg (QObject *parent, ProcessInfo *pinfo, Settings * settings)
 : Process (parent, pinfo, settings) {
}

FFMpeg::~FFMpeg () {
}

void FFMpeg::init () {
}

bool FFMpeg::deMediafiedPlay () {
    RecordDocument *rd = recordDocument (user);
    if (!rd)
        return false;
    initProcess ();
    connect (m_process, SIGNAL (finished (int, QProcess::ExitStatus)),
            this, SLOT (processStopped (int, QProcess::ExitStatus)));
    QString outurl = encodeFileOrUrl (rd->record_file);
    if (outurl.startsWith (QChar ('/')))
        QFile (outurl).remove ();
    QString exe ("ffmpeg ");
    QStringList args;
    if (!m_source->videoDevice ().isEmpty () ||
        !m_source->audioDevice ().isEmpty ()) {
        if (!m_source->videoDevice ().isEmpty ())
            args << "-vd" << m_source->videoDevice ();
        else
            args << "-vn";
        if (!m_source->audioDevice ().isEmpty ())
            args << "-ad" << m_source->audioDevice ();
        else
            args << "-an";
        QProcess process;
        QString ctl_exe ("v4lctl");
        QStringList ctl_args;
        if (!m_source->videoNorm ().isEmpty ()) {
            ctl_args << "-c" << m_source->videoDevice () << "setnorm" << m_source->videoNorm ();
            process.start (ctl_exe, ctl_args);
            process.waitForFinished (5000);
            args << "-tvstd" << m_source->videoNorm ();
        }
        if (m_source->frequency () > 0) {
            ctl_args.clear ();
            ctl_args << "-c" << m_source->videoDevice () << "setfreq" << QString::number (m_source->frequency ());
            process.start (ctl_exe, ctl_args);
            process.waitForFinished (5000);
        }
    } else {
        args << "-i" << encodeFileOrUrl (m_url);
    }
    args << KShell::splitArgs (m_settings->ffmpegarguments);
    args << outurl;
    qCDebug(LOG_KMPLAYER_COMMON, "ffmpeg %s\n", args.join (" ").toLocal8Bit().constData ());
    // FIXME if (m_player->source () == source) // ugly
    //    m_player->stop ();
    m_process->start (exe, args);
    if (m_process->waitForStarted ()) {
        setState (Playing);
        return true;
    }
    stop ();
    return false;
}

void FFMpeg::stop () {
    terminateJobs ();
    if (!running ())
        return;
    qCDebug(LOG_KMPLAYER_COMMON) << "FFMpeg::stop";
    m_process->write ("q");
}

void FFMpeg::quit () {
    stop ();
    if (!running ())
        return;
    if (m_process->waitForFinished (2000))
        Process::quit ();
}

void FFMpeg::processStopped (int, QProcess::ExitStatus) {
    setState (IProcess::NotRunning);
}

//-----------------------------------------------------------------------------

static const char *npp_supports [] = { "urlsource", nullptr };

NppProcessInfo::NppProcessInfo (MediaManager *mgr)
 : ProcessInfo ("npp", i18n ("&Ice Ape"), npp_supports, mgr, nullptr) {}

IProcess *NppProcessInfo::create (PartBase *p, ProcessUser *usr) {
    NpPlayer *n = new NpPlayer (p, this, p->settings());
    n->setSource (p->source ());
    n->user = usr;
    p->processCreated (n);
    return n;
}

#ifdef KMPLAYER_WITH_NPP

NpStream::NpStream (NpPlayer *p, uint32_t sid, const QString &u, const QByteArray &ps)
 : QObject (p),
   url (u),
   post (ps),
   job (nullptr), bytes (0),
   stream_id (sid),
   content_length (0),
   finish_reason (NoReason),
   received_data (false) {
    data_arrival.tv_sec = 0;
    (void) new StreamAdaptor (this);
    QString objpath = QString ("%1/stream_%2").arg (p->objectPath ()).arg (sid);
    QDBusConnection::sessionBus().registerObject (objpath, this);
}

NpStream::~NpStream () {
    close ();
}

void NpStream::open () {
    qCDebug(LOG_KMPLAYER_COMMON) << "NpStream " << stream_id << " open " << url;
    if (url.startsWith ("javascript:")) {
        NpPlayer *npp = static_cast <NpPlayer *> (parent ());
        QString result = npp->evaluate (url.mid (11), false);
        if (!result.isEmpty ()) {
            QByteArray cr = result.toLocal8Bit ();
            int len = strlen (cr.constData ());
            pending_buf.resize (len + 1);
            memcpy (pending_buf.data (), cr.constData (), len);
            pending_buf.data ()[len] = 0;
            gettimeofday (&data_arrival, nullptr);
        }
        qCDebug(LOG_KMPLAYER_COMMON) << "result is " << pending_buf.constData ();
        finish_reason = BecauseDone;
        Q_EMIT stateChanged ();
    } else {
        if (!post.size ()) {
            job = KIO::get (QUrl::fromUserInput (url), KIO::NoReload, KIO::HideProgressInfo);
            job->addMetaData ("PropagateHttpHeader", "true");
        } else {
            QStringList name;
            QStringList value;
            QString buf;
            int data_pos = -1;
            for (int i = 0; i < post.size () && data_pos < 0; ++i) {
                char c = post.at (i);
                switch (c) {
                case ':':
                    if (name.size () == value.size ()) {
                        name << buf;
                        buf.truncate (0);
                    } else
                        buf += QChar (':');
                    break;
                case '\r':
                    break;
                case '\n':
                    if (name.size () == value.size ()) {
                        if (buf.isEmpty ()) {
                            data_pos = i + 1;
                        } else {
                            name << buf;
                            value << QString ("");
                        }
                    } else {
                        value << buf;
                    }
                    buf.truncate (0);
                    break;
                default:
                    buf += QChar (c);
                }
            }
            job = KIO::http_post (QUrl::fromUserInput (url), post.mid (data_pos), KIO::HideProgressInfo);
            for (int i = 0; i < name.size (); ++i)
                job->addMetaData (name[i].trimmed (), value[i].trimmed ());
        }
        job->addMetaData ("errorPage", "false");
        connect (job, SIGNAL (data (KIO::Job *, const QByteArray &)),
                this, SLOT (slotData (KIO::Job *, const QByteArray &)));
        connect (job, SIGNAL (result (KJob *)),
                this, SLOT (slotResult (KJob *)));
        connect (job, SIGNAL(redirection(KIO::Job*, const QUrl&)),
                this, SLOT (redirection (KIO::Job *, const QUrl &)));
        connect (job, SIGNAL (mimetype (KIO::Job *, const QString &)),
                SLOT (slotMimetype (KIO::Job *, const QString &)));
        connect (job, SIGNAL (totalSize (KJob *, qulonglong)),
                SLOT (slotTotalSize (KJob *, qulonglong)));
    }
}

void NpStream::close () {
    if (job) {
        job->kill (); // quiet, no result signal
        job = nullptr;
        finish_reason = BecauseStopped;
        // don't emit stateChanged(), because always triggered from NpPlayer
    }
}

void NpStream::destroy () {
    pending_buf.clear ();
    static_cast <NpPlayer *> (parent ())->destroyStream (stream_id);
}

void NpStream::slotResult (KJob *jb) {
    qCDebug(LOG_KMPLAYER_COMMON) << "slotResult " << stream_id << " " << bytes << " err:" << jb->error ();
    finish_reason = jb->error () ? BecauseError : BecauseDone;
    job = nullptr; // signal KIO::Job::result deletes itself
    Q_EMIT stateChanged ();
}

void NpStream::slotData (KIO::Job*, const QByteArray& qb) {
    if (job) {
        int sz = pending_buf.size ();
        if (sz) {
            pending_buf.resize (sz + qb.size ());
            memcpy (pending_buf.data () + sz, qb.constData (), qb.size ());
        } else {
            pending_buf = qb;
        }
        if (sz + qb.size () > 64000 &&
                !job->isSuspended () && !job->suspend ())
            qCCritical(LOG_KMPLAYER_COMMON) << "suspend not supported" << endl;
        if (!sz)
            gettimeofday (&data_arrival, nullptr);
        if (!received_data) {
            received_data = true;
            http_headers = job->queryMetaData ("HTTP-Headers");
            if (!http_headers.isEmpty() && !http_headers.endsWith (QChar ('\n')))
                http_headers += QChar ('\n');
        }
        if (sz + qb.size ())
            Q_EMIT stateChanged ();
    }
}

void NpStream::redirection (KIO::Job*, const QUrl& kurl) {
    url = kurl.url ();
    Q_EMIT redirected (stream_id, kurl);
}

void NpStream::slotMimetype (KIO::Job *, const QString &mime) {
    mimetype = mime.indexOf("adobe.flash") > -1 ? "application/x-shockwave-flash" : mime;
}

void NpStream::slotTotalSize (KJob *, qulonglong sz) {
    content_length = sz;
}

NpPlayer::NpPlayer (QObject *parent, ProcessInfo *pinfo, Settings *settings)
 : Process (parent, pinfo, settings),
   write_in_progress (false),
   in_process_stream (false) {
}

NpPlayer::~NpPlayer () {
}

void NpPlayer::init () {
}

void NpPlayer::initProcess () {
    setupProcess (&m_process);
    m_process_state = QProcess::NotRunning;
    connect (m_process, SIGNAL (finished (int, QProcess::ExitStatus)),
            this, SLOT (processStopped (int, QProcess::ExitStatus)));
    connect (m_process, SIGNAL (readyReadStandardError ()),
            this, SLOT (processOutput ()));
    connect (m_process, SIGNAL (bytesWritten (qint64)),
            this, SLOT (wroteStdin (qint64)));
    if (iface.isEmpty ()) {
        iface = QString ("org.kde.kmplayer.callback");
        static int count = 0;
        path = QString ("/npplayer%1").arg (count++);
        (void) new CallbackAdaptor (this);
        QDBusConnection::sessionBus().registerObject (path, this);
        filter = QString ("type='method_call',interface='org.kde.kmplayer.callback'");
        service = QDBusConnection::sessionBus().baseService ();
        //service = QString (dbus_bus_get_unique_name (conn));
        qCDebug(LOG_KMPLAYER_COMMON) << "using service " << service << " interface " << iface << " filter:" << filter;
    }
}

bool NpPlayer::deMediafiedPlay () {
    qCDebug(LOG_KMPLAYER_COMMON) << "NpPlayer::play '" << m_url << "' state " << m_state;
    // if we change from XPLAIN to XEMBED, the DestroyNotify may come later
    if (!view ())
        return false;
    if (!m_url.isEmpty () && m_url != "about:empty") {
        QDBusMessage msg = QDBusMessage::createMethodCall (
                remote_service, "/plugin", "org.kde.kmplayer.backend", "play");
        msg << m_url;
        msg.setDelayedReply (false);
        QDBusConnection::sessionBus().send (msg);
        setState (IProcess::Buffering);
    }
    return true;
}

bool NpPlayer::ready () {
    Mrl *node = mrl ();
    if (!node || !user || !user->viewer ())
        return false;

    user->viewer ()->useIndirectWidget (false);
    user->viewer ()->setMonitoring (IViewer::MonitorNothing);

    if (state () == IProcess::Ready)
        return true;

    initProcess ();
    QString exe ("knpplayer");
    QStringList args;
    args << "-cb" << (service + path);
    args << "-wid" << QString::number (user->viewer ()->windowHandle ());
    startProcess (exe, args);
    if (m_process->waitForStarted (5000)) {
        QString s;
        for (int i = 0; i < 2 && remote_service.isEmpty (); ++i) {
            if (!m_process->waitForReadyRead (5000))
                return false;
            const QByteArray ba = m_process->readAllStandardOutput ();
            s += QString::fromLatin1 (ba.data (), ba.size ());
            int nl = s.indexOf (QChar ('\n'));
            if (nl > 0) {
                int p = s.indexOf ("NPP_DBUS_SRV=");
                if (p > -1)
                    remote_service = s.mid (p + 13, nl - p - 13);
            }
        }
    }
    connect (m_process, SIGNAL (readyReadStandardOutput ()),
            this, SLOT (processOutput ()));
    if (!remote_service.isEmpty ()) {
        QString mime = "text/plain";
        QString plugin;
        Element *elm = node;
        if (elm->id == id_node_html_object) {
            // this sucks to have to do this here ..
            for (Node *n = elm->firstChild (); n; n = n->nextSibling ())
                if (n->id == KMPlayer::id_node_html_embed) {
                    elm = static_cast <Element *> (n);
                    break;
                }
        }
        for (Node *n = mrl (); n; n = n->parentNode ()) {
            Mrl *mrl = n->mrl ();
            if (mrl && !mrl->mimetype.isEmpty ()) {
                plugin = m_source->plugin (mrl->mimetype);
                qCDebug(LOG_KMPLAYER_COMMON) << "search plugin " << mrl->mimetype << "->" << plugin;
                if (!plugin.isEmpty ()) {
                    mime = mrl->mimetype;
                    if ( mime.indexOf("adobe.flash") > -1 )
                        mime = "application/x-shockwave-flash";
                    break;
                }
            }
        }
        if (!plugin.isEmpty ()) {
            QDBusMessage msg = QDBusMessage::createMethodCall (
                    remote_service, "/plugin", "org.kde.kmplayer.backend", "setup");
            msg << mime << plugin;
            QMap <QString, QVariant> urlargs;
            for (AttributePtr a = elm->attributes ().first (); a; a = a->nextSibling ())
                urlargs.insert (a->name ().toString (), a->value ());
            msg << urlargs;
            msg.setDelayedReply (false);
            QDBusConnection::sessionBus().call (msg, QDBus::BlockWithGui);
            setState (IProcess::Ready);
            return true;
        }
    }
    m_old_state = m_state = Ready;
    stop ();

    return false;
}

void NpPlayer::running (const QString &srv) {
    remote_service = srv;
    qCDebug(LOG_KMPLAYER_COMMON) << "NpPlayer::running " << srv;
    setState (Ready);
}

void NpPlayer::plugged () {
    view ()->videoStart ();
}

static int getStreamId (const QString &path) {
    int p = path.lastIndexOf (QChar ('_'));
    if (p < 0) {
        qCCritical(LOG_KMPLAYER_COMMON) << "wrong object path " << path << endl;
        return -1;
    }
    bool ok;
    qint32 sid = path.mid (p+1).toInt (&ok);
    if (!ok) {
        qCCritical(LOG_KMPLAYER_COMMON) << "wrong object path suffix " << path.mid (p+1) << endl;
        return -1;
    }
    return sid;
}

void NpPlayer::request_stream (const QString &path, const QString &url, const QString &target, const QByteArray &post) {
    QString uri (url);
    qCDebug(LOG_KMPLAYER_COMMON) << "NpPlayer::request " << path << " '" << url << "' " << " tg:" << target << "post" << post.size ();
    bool js = url.startsWith ("javascript:");
    if (!js) {
        const QUrl base = process_info->manager->player ()->docBase ();
        uri = (base.isEmpty () ? QUrl::fromUserInput(m_url) : base).resolved(QUrl(url)).url ();
    }
    qCDebug(LOG_KMPLAYER_COMMON) << "NpPlayer::request " << path << " '" << uri << "'" << m_url << "->" << url;
    qint32 sid = getStreamId (path);
    if ((int)sid >= 0) {
        if (!target.isEmpty ()) {
            qCDebug(LOG_KMPLAYER_COMMON) << "new page request " << target;
            if (js) {
                QString result = evaluate (url.mid (11), false);
                qCDebug(LOG_KMPLAYER_COMMON) << "result is " << result;
                if (result == "undefined")
                    uri = QString ();
                else
                    uri = QUrl::fromUserInput (m_url).resolved(QUrl(result)).url (); // probably wrong ..
            }
            QUrl kurl = QUrl::fromUserInput(uri);
            if (kurl.isValid ())
                process_info->manager->player ()->openUrl (kurl, target, QString ());
            sendFinish (sid, 0, NpStream::BecauseDone);
        } else {
            NpStream * ns = new NpStream (this, sid, uri, post);
            connect (ns, SIGNAL (stateChanged ()), this, SLOT (streamStateChanged ()));
            streams[sid] = ns;
            if (url != uri)
                streamRedirected (sid, QUrl(uri));
            if (!write_in_progress)
                processStreams ();
        }
    }
}

QString NpPlayer::evaluate (const QString &script, bool store) {
    QString result ("undefined");
    Q_EMIT evaluate (script, store, result);
    //qCDebug(LOG_KMPLAYER_COMMON) << "evaluate " << script << " => " << result;
    return result;
}

void NpPlayer::dimension (int w, int h) {
    source ()->setAspect (mrl (), 1.0 * w/ h);
}

void NpPlayer::destroyStream (uint32_t sid) {
    if (streams.contains (sid)) {
        NpStream *ns = streams[sid];
        ns->close ();
        if (!write_in_progress)
            processStreams ();
    } else {
        qCWarning(LOG_KMPLAYER_COMMON) << "Object " << sid << " not found";
    }
    if (!sid)
        Q_EMIT loaded ();
}

void NpPlayer::sendFinish (quint32 sid, quint32 bytes, NpStream::Reason because) {
    qCDebug(LOG_KMPLAYER_COMMON) << "NpPlayer::sendFinish " << sid << " bytes:" << bytes;
    if (running ()) {
        uint32_t reason = (int) because;
        QString objpath = QString ("/stream_%1").arg (sid);
        QDBusMessage msg = QDBusMessage::createMethodCall (
                remote_service, objpath, "org.kde.kmplayer.backend", "eof");
        msg << bytes << reason;
        msg.setDelayedReply (false);
        QDBusConnection::sessionBus().send (msg);
    }
    if (!sid)
        Q_EMIT loaded ();
}

void NpPlayer::terminateJobs () {
    Process::terminateJobs ();
    const StreamMap::iterator e = streams.end ();
    for (StreamMap::iterator i = streams.begin (); i != e; ++i)
        delete i.value ();
    streams.clear ();
}

void NpPlayer::stop () {
    terminateJobs ();
    if (!running ())
        return;
    qCDebug(LOG_KMPLAYER_COMMON) << "NpPlayer::stop ";
    QDBusMessage msg = QDBusMessage::createMethodCall (
            remote_service, "/plugin", "org.kde.kmplayer.backend", "quit");
    msg.setDelayedReply (false);
    QDBusConnection::sessionBus().send (msg);
}

void NpPlayer::quit () {
    if (running () && !m_process->waitForFinished (2000))
        Process::quit ();
}

void NpPlayer::processOutput () {
    if (!remote_service.isEmpty ())
        outputToView (view (), m_process->readAllStandardOutput ());
    outputToView (view (), m_process->readAllStandardError ());
}

void NpPlayer::processStopped (int, QProcess::ExitStatus) {
    terminateJobs ();
    if (m_source)
        m_source->document ()->message (MsgInfoString, nullptr);
    setState (IProcess::NotRunning);
}

void NpPlayer::streamStateChanged () {
    setState (IProcess::Playing); // hmm, this doesn't really fit in current states
    if (!write_in_progress)
        processStreams ();
}

void NpPlayer::streamRedirected(uint32_t sid, const QUrl& u) {
    if (running ()) {
        qCDebug(LOG_KMPLAYER_COMMON) << "redirected " << sid << " to " << u.url();
        QString objpath = QString ("/stream_%1").arg (sid);
        QDBusMessage msg = QDBusMessage::createMethodCall (
                remote_service, objpath, "org.kde.kmplayer.backend", "redirected");
        msg << u.url ();
        msg.setDelayedReply (false);
        QDBusConnection::sessionBus().send (msg);
    }
}

void NpPlayer::requestGet (const uint32_t id, const QString &prop, QString *res) {
    if (!remote_service.isEmpty ()) {
        QDBusMessage msg = QDBusMessage::createMethodCall (
                remote_service, "/plugin", "org.kde.kmplayer.backend", "get");
        msg << id << prop;
        QDBusMessage rmsg = QDBusConnection::sessionBus().call (msg, QDBus::BlockWithGui);
        if (rmsg.type () == QDBusMessage::ReplyMessage) {
            //qCDebug(LOG_KMPLAYER_COMMON) << "get " << prop << rmsg.arguments ().size ();
            if (rmsg.arguments ().size ()) {
                QString s = rmsg.arguments ().first ().toString ();
                if (s != "error")
                    *res = s;
            }
        } else {
            qCCritical(LOG_KMPLAYER_COMMON) << "get" << prop << rmsg.type () << rmsg.errorMessage ();
        }
    }
}

void NpPlayer::requestCall (const uint32_t id, const QString &func,
        const QStringList &args, QString *res) {
    QDBusMessage msg = QDBusMessage::createMethodCall (
            remote_service, "/plugin", "org.kde.kmplayer.backend", "call");
    msg << id << func << args;
    QDBusMessage rmsg = QDBusConnection::sessionBus().call (msg, QDBus::BlockWithGui);
    //qCDebug(LOG_KMPLAYER_COMMON) << "call " << func << rmsg.arguments ().size ();
    if (rmsg.arguments ().size ()) {
        QString s = rmsg.arguments ().first ().toString ();
        if (s != "error")
            *res = s;
    }
}

void NpPlayer::processStreams () {
    NpStream *stream = nullptr;
    qint32 stream_id;
    timeval tv = { 0x7fffffff, 0 };
    const StreamMap::iterator e = streams.end ();
    int active_count = 0;

    if (in_process_stream || write_in_progress) {
        qCCritical(LOG_KMPLAYER_COMMON) << "wrong call"; // TODO: port << kBacktrace();
        return;
    }
    in_process_stream = true;

    //qCDebug(LOG_KMPLAYER_COMMON) << "NpPlayer::processStreams " << streams.size ();
    for (StreamMap::iterator i = streams.begin (); i != e;) {
        NpStream *ns = i.value ();
        if (ns->job) {
            active_count++;
        } else if (active_count < 5 &&
                ns->finish_reason == NpStream::NoReason) {
            write_in_progress = true; // javascript: urls emit stateChange
            ns->open ();
            write_in_progress = false;
            if (ns->job) {
                connect(ns, SIGNAL(redirected(uint32_t, const QUrl&)),
                        this, SLOT(streamRedirected(uint32_t, const QUrl&)));
                active_count++;
            }
        }
        if (ns->finish_reason == NpStream::BecauseStopped ||
                ns->finish_reason == NpStream::BecauseError ||
                (ns->finish_reason == NpStream::BecauseDone &&
                 ns->pending_buf.size () == 0)) {
            sendFinish (i.key(), ns->bytes, ns->finish_reason);
            i = streams.erase (i);
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
    //qCDebug(LOG_KMPLAYER_COMMON) << "NpPlayer::processStreams " << stream;
    if (stream) {
        if (stream->finish_reason != NpStream::BecauseStopped &&
                stream->finish_reason != NpStream::BecauseError &&
                !stream->bytes &&
                (!stream->mimetype.isEmpty() || stream->content_length)) {
            QString objpath = QString ("/stream_%1").arg (stream->stream_id);
            QDBusMessage msg = QDBusMessage::createMethodCall (
                    remote_service, objpath, "org.kde.kmplayer.backend", "streamInfo");
            msg << stream->mimetype
                << stream->content_length
                << stream->http_headers;
            msg.setDelayedReply (false);
            QDBusConnection::sessionBus().send (msg);
        }
        const int header_len = 2 * sizeof (qint32);
        qint32 chunk = stream->pending_buf.size ();
        send_buf.resize (chunk + header_len);
        memcpy (send_buf.data (), &stream_id, sizeof (qint32));
        memcpy (send_buf.data() + sizeof (qint32), &chunk, sizeof (qint32));
        memcpy (send_buf.data ()+header_len,
                stream->pending_buf.constData (), chunk);
        stream->pending_buf = QByteArray ();
        /*fprintf (stderr, " => %d %d\n", (long)stream_id, chunk);*/
        stream->bytes += chunk;
        write_in_progress = true;
        m_process->write (send_buf);
        if (stream->finish_reason == NpStream::NoReason)
            stream->job->resume ();
    }
    in_process_stream = false;
}

void NpPlayer::wroteStdin (qint64) {
    if (!m_process->bytesToWrite ()) {
        write_in_progress = false;
        if (running ())
            processStreams ();
    }
}

QString NpPlayer::cookie (const QString &url)
{
    QString s;
    View *v = view ();
    if (v) {
        KIO::Integration::CookieJar jar (v);
        jar.setWindowId (v->topLevelWidget()->winId ());
        QList<QNetworkCookie> c = jar.cookiesForUrl (QUrl(url));
        QList<QNetworkCookie>::const_iterator e = c.constEnd ();
        for (QList<QNetworkCookie>::const_iterator i = c.constBegin (); i != e; ++i)
            s += (s.isEmpty() ? "" : ";") + QString::fromUtf8 ((*i).toRawForm());
    }
    return s;
}

#else

NpStream::NpStream (NpPlayer *p, uint32_t sid, const QString &u, const QByteArray &/*ps*/)
    : QObject (p) {}

NpStream::~NpStream () {}
void NpStream::slotResult (KJob*) {}
void NpStream::slotData (KIO::Job*, const QByteArray&) {}
void NpStream::redirection(KIO::Job*, const QUrl&) {}
void NpStream::slotMimetype (KIO::Job *, const QString &) {}
void NpStream::slotTotalSize (KJob *, KIO::filesize_t) {}

NpPlayer::NpPlayer (QObject *parent, ProcessInfo *pinfo, Settings *settings)
 : Process (parent, pinfo, settings) {}
NpPlayer::~NpPlayer () {}
void NpPlayer::init () {}
bool NpPlayer::deMediafiedPlay () { return false; }
void NpPlayer::initProcess () {}
void NpPlayer::stop () {}
void NpPlayer::quit () { }
bool NpPlayer::ready () { return false; }
void NpPlayer::requestGet (const uint32_t, const QString &, QString *) {}
void NpPlayer::requestCall (const uint32_t, const QString &, const QStringList &, QString *) {}
void NpPlayer::processOutput () {}
void NpPlayer::processStopped (int, QProcess::ExitStatus) {}
void NpPlayer::wroteStdin (qint64) {}
void NpPlayer::streamStateChanged () {}
void NpPlayer::streamRedirected(uint32_t, const QUrl&) {}
void NpPlayer::terminateJobs () {}

#endif
