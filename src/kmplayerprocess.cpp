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
#include <qdom.h>
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

#include "kmplayerpartbase.h"
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
        kdDebug () << "getPath " << p.mid (i-1) << endl;
        if (i > 0)
            return p.mid (i-1);
        return QString (QChar ('/') + p);
    }
    return p;
}

Process::Process (PartBase * player, const char * n)
    : QObject (player, n), m_player (player), m_source (0L), m_process (0L),
      m_supported_sources (default_supported) {}

Process::~Process () {
    stop ();
    delete m_process;
}

void Process::init () {
}

QString Process::menuName () const {
    return QString (className ());
}

void Process::initProcess () {
    delete m_process;
    m_process = new KProcess;
    m_process->setUseShell (true);
    if (m_source) m_source->setPosition (0);
    m_url.truncate (0);
}

WId Process::widget () {
    return 0;
}

View * Process::view () {
    return static_cast <View *> (m_player->view ());
}

bool Process::playing () const {
    return m_process && m_process->isRunning ();
}

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
        m_process->kill (SIGTERM);
        KProcessController::theKProcessController->waitForProcessExit (1);
        if (!m_process->isRunning ())
            break;
        m_process->kill (SIGKILL);
        KProcessController::theKProcessController->waitForProcessExit (1);
        if (m_process->isRunning ()) {
            KMessageBox::error (m_player->view (), i18n ("Failed to end player process."), i18n ("Error"));
        }
    } while (false);
    return !playing ();
}

bool Process::quit () {
    return stop ();
}

KDE_NO_EXPORT bool Process::play () {
    if (!source ()) return false;
    stop ();
    connect (m_source, SIGNAL (currentURL (const QString &)), this, SLOT (urlForPlaying (const QString &)));
    m_source->getCurrent ();
    return true;
}

void Process::urlForPlaying (const QString &) {
}

void Process::setSource (Source * source) {
    if (source != m_source) {
        stop ();
        m_source = source;
    }
}

//-----------------------------------------------------------------------------

static bool proxyForURL (const KURL& url, QString& proxy) {
    KProtocolManager::slaveProtocol (url, proxy);
    return !proxy.isNull ();
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT MPlayerBase::MPlayerBase (PartBase * player, const char * n)
    : Process (player, n), m_use_slave (true) {
    m_process = new KProcess;
}

KDE_NO_CDTOR_EXPORT MPlayerBase::~MPlayerBase () {
}

KDE_NO_EXPORT void MPlayerBase::initProcess () {
    Process::initProcess ();
    const KURL & url (source ()->url ());
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
        commands.push_front (cmd + "\n");
        printf ("eval %s", commands.last ().latin1 ());
        if (commands.size () < 2)
            m_process->writeStdin (QFile::encodeName(commands.last ()),
                    commands.last ().length ());
        return true;
    }
    return false;
}

KDE_NO_EXPORT bool MPlayerBase::stop () {
    if (!source () || !m_process || !m_process->isRunning ()) return true;
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
    return true;
}

KDE_NO_EXPORT bool MPlayerBase::quit () {
    if (!m_process || !m_process->isRunning ()) return true;
    disconnect (m_process, SIGNAL (processExited (KProcess *)),
                this, SLOT (processStopped (KProcess *)));
    return stop ();
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
    disconnect (m_source, SIGNAL (currentURL (const QString &)), this, SLOT (urlForPlaying (const QString &)));
    QTimer::singleShot (0, this, SLOT (emitFinished ()));
}

//-----------------------------------------------------------------------------

static const char * mplayer_supports [] = {
    "dvdsource", "hrefsource", "pipesource", "tvsource", "urlsource", "vcdsource", 0L
};

KDE_NO_CDTOR_EXPORT MPlayer::MPlayer (PartBase * player)
 : MPlayerBase (player, "mplayer"),
   m_widget (0L),
   m_configpage (new MPlayerPreferencesPage (this)) {
       m_supported_sources = mplayer_supports;
    m_player->settings ()->addPage (m_configpage);
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
    return view()->viewer()->embeddedWinId ();
}

KDE_NO_EXPORT bool MPlayer::play () {
    if (playing ())
        return sendCommand (QString ("gui_play"));
    return Process::play ();
}

void MPlayer::urlForPlaying (const QString & urlstr) {
    KURL url (urlstr);
    stop ();
    initProcess ();
    source ()->setPosition (0);
    m_request_seek = -1;
    QString args = source ()->options () + ' ';
    m_url = url.url ();
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
            if (cache > 3 && url.protocol () != QString ("dvd") &&
                    url.protocol () != QString ("vcd") &&
                    !url.url ().startsWith (QString ("tv://")))
                args += QString ("-cache %1 ").arg (cache); 
        }
        args += KProcess::quote (QString (QFile::encodeName (m_url)));
    }
    m_tmpURL.truncate (0);
    if (!source ()->identified () && !m_player->settings ()->mplayerpost090) {
        args += QString (" -quiet -nocache -identify -frames 0 ");
    } else {
        if (m_player->settings ()->loop)
            args += QString (" -loop 0");
        if (m_player->settings ()->mplayerpost090)
            args += QString (" -identify");
        if (!m_source->subUrl ().isEmpty ()) {
            args += QString (" -sub ");
            const KURL & sub_url (source ()->subUrl ());
            if (!sub_url.isEmpty ()) {
                QString myurl (sub_url.isLocalFile () ? getPath (sub_url) : sub_url.url ());
                args += KProcess::quote (QString (QFile::encodeName (myurl)));
            }
        }
    }
    run (args.ascii (), source ()->pipeCmd ().ascii ());
}

KDE_NO_EXPORT bool MPlayer::stop () {
    if (!source () || !m_process || !m_process->isRunning ()) return true;
    kdDebug () << "MPlayer::stop ()" << endl;
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
        for (++i; i != commands.end (); ++i)
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
    if (!source ()) return false;
    if (!absolute)
        return sendCommand (QString ("volume ") + QString::number (incdec));
    return false;
}

KDE_NO_EXPORT bool MPlayer::saturation (int val, bool absolute) {
    if (!source ()) return false;
    QString cmd;
    cmd.sprintf ("saturation %d %d", val, absolute ? 1 : 0);
    return sendCommand (cmd);
}

KDE_NO_EXPORT bool MPlayer::hue (int val, bool absolute) {
    if (!source ()) return false;
    QString cmd;
    cmd.sprintf ("hue %d %d", val, absolute ? 1 : 0);
    return sendCommand (cmd);
}

KDE_NO_EXPORT bool MPlayer::contrast (int val, bool /*absolute*/) {
    if (!source ()) return false;
    QString cmd;
    cmd.sprintf ("contrast %d 1", val);
    return sendCommand (cmd);
}

KDE_NO_EXPORT bool MPlayer::brightness (int val, bool /*absolute*/) {
    if (!source ()) return false;
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
    Settings *settings = m_player->settings ();
    m_use_slave = !(pipe && pipe[0]);
    if (!m_use_slave) {
        printf ("%s | ", pipe);
        *m_process << pipe << " | ";
    }
    printf ("mplayer -wid %lu ", (unsigned long) widget ());
    *m_process << "mplayer -wid " << QString::number (widget ());

    if (m_use_slave) {
        printf ("-slave ");
        *m_process << "-slave ";
    }

    QString strVideoDriver = QString (settings->videodrivers[settings->videodriver].driver);
    if (!strVideoDriver.isEmpty ()) {
        printf (" -vo %s", strVideoDriver.lower().ascii());
        *m_process << " -vo " << strVideoDriver.lower();
    }
    QString strAudioDriver = QString (settings->audiodrivers[settings->audiodriver].driver);
    if (!strAudioDriver.isEmpty ()) {
        printf (" -ao %s", strAudioDriver.lower().ascii());
        *m_process << " -ao " << strAudioDriver.lower();
    }
    if (settings->framedrop) {
        printf (" -framedrop");
        *m_process << " -framedrop";
    }

    if (m_configpage->additionalarguments.length () > 0) {
        printf (" %s", m_configpage->additionalarguments.ascii());
        *m_process << " " << m_configpage->additionalarguments;
    }
    // postproc thingies

    printf (" %s", source ()->filterOptions ().ascii ());
    *m_process << " " << source ()->filterOptions ();

    printf (" -contrast %d", settings->contrast);
    *m_process << " -contrast " << QString::number (settings->contrast);

    printf (" -brightness %d", settings->brightness);
    *m_process << " -brightness " << QString::number(settings->brightness);

    printf (" -hue %d", settings->hue);
    *m_process << " -hue " << QString::number (settings->hue);

    printf (" -saturation %d", settings->saturation);
    *m_process << " -saturation " << QString::number(settings->saturation);

    printf (" %s\n", args);
    *m_process << " " << args;

    QValueList<QCString>::const_iterator it;
    QString sMPArgs;
    for ( it = m_process->args().begin(); it != m_process->args().end(); ++it ){
        sMPArgs += (*it);
    }

    m_process->start (KProcess::NotifyOnExit, KProcess::All);

    if (m_process->isRunning ()) {
        QTimer::singleShot (0, this, SLOT (emitStarted ()));
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
    QString args ("mplayer -vo jpeg -jpeg outdir=");
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
    View * v = view ();
    if (!v || slen <= 0) return;

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
                source ()->setPosition (pos);
                m_request_seek = -1;
                emit positioned (pos);
            } else if (m_cacheRegExp.search (out) > -1) {
                emit loaded (int (m_cacheRegExp.cap(1).toDouble()));
            }
        } else if (!source ()->identified () && out.startsWith ("ID_LENGTH")) {
            int pos = out.find ('=');
            if (pos > 0) {
                int l = out.mid (pos + 1).toInt (&ok);
                if (ok && l >= 0) {
                    m_source->setLength (10 * l);
                    emit lengthFound (10 * l);
                }
            }
        } else if (!source ()->identified () && m_refURLRegExp.search (out) > -1) {
            kdDebug () << "Reference mrl " << m_refURLRegExp.cap (1) << endl;
            if (!m_tmpURL.isEmpty () && m_url != m_tmpURL)
                m_source->insertURL (m_tmpURL);;
            m_tmpURL = KURL::fromPathOrURL (m_refURLRegExp.cap (1)).url ();
            if (m_source->url () == m_tmpURL || m_url == m_tmpURL)
                m_tmpURL.truncate (0);
        } else if (!source ()->identified () && m_refRegExp.search (out) > -1) {
            kdDebug () << "Reference File " << endl;
            m_tmpURL.truncate (0);
        } else {
            QRegExp & m_startRegExp = patterns[MPlayerPreferencesPage::pat_start];
            QRegExp & m_sizeRegExp = patterns[MPlayerPreferencesPage::pat_size];
            v->addText (out, true);
            if (!m_source->processOutput (out)) {
                int movie_width = m_source->width ();
                if (movie_width <= 0 && m_sizeRegExp.search (out) > -1) {
                    movie_width = m_sizeRegExp.cap (1).toInt (&ok);
                    int movie_height = ok ? m_sizeRegExp.cap (2).toInt (&ok) : 0;
                    if (ok && movie_width > 0 && movie_height > 0) {
                        m_source->setWidth (movie_width);
                        m_source->setHeight (movie_height);
                        m_source->setAspect (1.0*movie_width/movie_height);
                        if (m_player->settings ()->sizeratio)
                            v->viewer ()->setAspect (m_source->aspect ());
                    }
                } else if (m_startRegExp.search (out) > -1) {
                    if (m_player->settings ()->mplayerpost090) {
                        if (!m_tmpURL.isEmpty () && m_url != m_tmpURL) {
                            m_source->insertURL (m_tmpURL);;
                            m_tmpURL.truncate (0);
                        }
                        source ()->setIdentified ();
                    }
                    emit startedPlaying ();
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
                m_source->insertURL (m_tmpURL);;
                m_tmpURL.truncate (0);
            }
            url = m_source->first ();
        } else
            url = m_source->next ();
        if (url.isEmpty ()) {
            MPlayerBase::processStopped (p);
            m_source->first ();
        } else
            QTimer::singleShot (0, m_source, SLOT (getCurrent ()));
    }
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
    { i18n ("Postion pattern"), "Movie Position", "V:\\s*([0-9\\.]+)" },
    { i18n ("Index pattern"), "Index Pattern", "Generating Index: +([0-9]+)%" },
    { i18n ("Reference URL pattern"), "Reference URL Pattern", "Playing\\s+(.*[^\\.])\\.?\\s*$" },
    { i18n ("Reference pattern"), "Reference Pattern", "Reference Media file" },
    { i18n ("Start pattern"), "Start Playing", "Start[^ ]* play" },
    { i18n ("DVD language pattern"), "DVD Language", "\\[open].*audio.*language: ([A-Za-z]+).*aid.*[^0-9]([0-9]+)" },
    { i18n ("DVD subtitle pattern"), "DVD Sub Title", "\\[open].*subtitle.*[^0-9]([0-9]+).*language: ([A-Za-z]+)" },
    { i18n ("DVD titles pattern"), "DVD Titles", "There are ([0-9]+) titles" },
    { i18n ("DVD chapters pattern"), "DVD Chapters", "There are ([0-9]+) chapters" },
    { i18n ("VCD track pattern"), "VCD Tracks", "track ([0-9]+):" }
};

namespace KMPlayer {
    
class MPlayerPreferencesFrame : public QFrame {
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

KDE_NO_CDTOR_EXPORT MEncoder::MEncoder (PartBase * player)
    : MPlayerBase (player, "mencoder") {
    }

KDE_NO_CDTOR_EXPORT MEncoder::~MEncoder () {
}

KDE_NO_EXPORT void MEncoder::init () {
}

void MEncoder::urlForPlaying (const QString & urlstr) {
    bool success = false;
    stop ();
    initProcess ();
    source ()->setPosition (0);
    QString args;
    m_use_slave = m_source->pipeCmd ().isEmpty ();
    if (!m_use_slave)
        args = m_source->pipeCmd () + QString (" | ");
    QString margs = m_player->settings()->mencoderarguments;
    if (m_player->settings()->recordcopy)
        margs = QString ("-oac copy -ovc copy");
    args += QString ("mencoder ") + margs + ' ' + m_source->recordCmd ();
    KURL url (urlstr);
    QString myurl = url.isLocalFile () ? getPath (url) : url.url ();
    bool post090 = m_player->settings ()->mplayerpost090;
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
        emitStarted ();
}

KDE_NO_EXPORT bool MEncoder::stop () {
    if (!source () || !m_process || !m_process->isRunning ()) return true;
    kdDebug () << "MEncoder::stop ()" << endl;
    if (m_use_slave)
        m_process->kill (SIGINT);
    return MPlayerBase::stop ();
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT
MPlayerDumpstream::MPlayerDumpstream (PartBase * player)
    : MPlayerBase (player, "mplayerdumpstream") {
    }

KDE_NO_CDTOR_EXPORT MPlayerDumpstream::~MPlayerDumpstream () {
}

KDE_NO_EXPORT void MPlayerDumpstream::init () {
}

void MPlayerDumpstream::urlForPlaying (const QString & urlstr) {
    bool success = false;
    stop ();
    initProcess ();
    source ()->setPosition (0);
    QString args;
    m_use_slave = m_source->pipeCmd ().isEmpty ();
    if (!m_use_slave)
        args = m_source->pipeCmd () + QString (" | ");
    args += QString ("mplayer ") + m_source->recordCmd ();
    KURL url (urlstr);
    QString myurl = url.isLocalFile () ? getPath (url) : url.url ();
    bool post090 = m_player->settings ()->mplayerpost090;
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
        emitStarted ();
}

KDE_NO_EXPORT bool MPlayerDumpstream::stop () {
    if (!source () || !m_process || !m_process->isRunning ()) return true;
    kdDebug () << "MPlayerDumpstream::stop ()" << endl;
    if (m_use_slave)
        m_process->kill (SIGINT);
    return MPlayerBase::stop ();
}

//-----------------------------------------------------------------------------

static int callback_counter = 0;

KMPlayerCallback::KMPlayerCallback (KMPlayerCallbackProcess * process)
    : DCOPObject (QString (QString ("KMPlayerCallback-") +
                           QString::number (callback_counter++)).ascii ()),
      m_process (process) {}

void KMPlayerCallback::statusMessage (int code, QString msg) {
    switch ((StatusCode) code) {
        case stat_newtitle:
            m_process->player ()->changeTitle (msg);
            break;
        case stat_addurl:
            m_process->source ()->insertURL (KURL::fromPathOrURL (msg).url ());
            break;
        default:
            m_process->setStatusMessage (msg);
    };
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

void KMPlayerCallback::started (QCString dcopname, QByteArray data) {
    m_process->setStarted (dcopname, data);
}

void KMPlayerCallback::movieParams (int length, int w, int h, float aspect) {
    m_process->setMovieParams (length, w, h, aspect);
}

void KMPlayerCallback::moviePosition (int position) {
    m_process->setMoviePosition (position);
}

void KMPlayerCallback::loadingProgress (int percentage) {
    m_process->setLoadingProgress (percentage);
}

//-----------------------------------------------------------------------------

KMPlayerCallbackProcess::KMPlayerCallbackProcess (PartBase * player, const char * n)
 : Process (player, n),
   m_callback (new KMPlayerCallback (this)),
   m_backend (0L),
   m_configpage (new KMPlayerXMLPreferencesPage (this)),
   in_gui_update (false),
   m_have_config (config_unknown),
   m_send_config (send_no),
   m_status (status_stop) {
}

KMPlayerCallbackProcess::~KMPlayerCallbackProcess () {
    delete m_callback;
    delete m_configpage;
}

void KMPlayerCallbackProcess::setStatusMessage (const QString & /*msg*/) {
}

void KMPlayerCallbackProcess::setErrorMessage (int code, const QString & msg) {
    kdDebug () << "setErrorMessage " << code << " " << msg << endl;
    if (code == 0 && m_send_config != send_no) {
        if (m_send_config == send_new)
            stop ();
        m_send_config = send_no;
    }
}

void KMPlayerCallbackProcess::setFinished () {
    m_status = status_stop;
    //QTimer::singleShot (0, this, SLOT (emitFinished ()));
}

void KMPlayerCallbackProcess::setPlaying () {
    m_status = status_play;
    emit startedPlaying ();
}

void KMPlayerCallbackProcess::setStarted (QCString dcopname, QByteArray & data) {
    m_status = status_start;
    if (data.size ())
        m_configdata = data;
    kdDebug () << "up and running " << dcopname << endl;
    m_backend = new KMPlayerBackend_stub (dcopname, "KMPlayerBackend");
    if (m_send_config == send_new) {
        m_backend->setConfig (m_changeddata);
    }
    if (m_have_config == config_probe || m_have_config == config_unknown) {
        bool was_probe = m_have_config == config_probe;
        m_have_config = data.size () ? config_yes : config_no;
        emit configReceived ();
        if (m_configpage)
            m_configpage->sync (false);
        if (was_probe) {
            quit ();
            return;
        }
    }
    Settings * settings = m_player->settings ();
    saturation (settings->saturation, true);
    hue (settings->hue, true);
    brightness (settings->brightness, true);
    contrast (settings->contrast, true);
    m_backend->play ();
}

void KMPlayerCallbackProcess::setMovieParams (int len, int w, int h, float a) {
    kdDebug () << "setMovieParams " << len << " " << w << "," << h << " " << a << endl;
    in_gui_update = true;
    m_source->setWidth (w);
    m_source->setHeight (h);
    m_source->setAspect (a);
    m_source->setLength (len);
    emit lengthFound (len);
    if (m_player->settings ()->sizeratio) {
        View * v = view ();
        if (!v) return;
        v->viewer ()->setAspect (a);
        v->updateLayout ();
    }
    in_gui_update = false;
}

void KMPlayerCallbackProcess::setMoviePosition (int position) {
    in_gui_update = true;
    m_source->setPosition (position);
    m_request_seek = -1;
    emit positioned (position);
    in_gui_update = false;
}

void KMPlayerCallbackProcess::setLoadingProgress (int percentage) {
    in_gui_update = true;
    emit loaded (percentage);
    in_gui_update = false;
}

bool KMPlayerCallbackProcess::getConfigData () {
    if (m_have_config == config_no)
        return false;
    if (m_have_config == config_unknown && !playing ()) {
        m_have_config = config_probe;
        runForConfig ();
    }
    return true;
}

void KMPlayerCallbackProcess::setChangedData (const QByteArray & data) {
    m_changeddata = data;
    m_send_config = playing () ? send_try : send_new;
    if (m_send_config == send_try)
        m_backend->setConfig (data);
    else
        play ();
}

bool KMPlayerCallbackProcess::stop () {
    if (!m_process || !m_process->isRunning ()) return true;
    if (m_backend)
        m_backend->stop ();
    return true;
}

bool KMPlayerCallbackProcess::pause () {
    if (!playing () || !m_backend) return false;
    m_backend->pause ();
    return true;
}

bool KMPlayerCallbackProcess::saturation (int val, bool b) {
    if (m_backend)
        m_backend->saturation (val, b);
    return !!m_backend;
}

bool KMPlayerCallbackProcess::hue (int val, bool b) {
    if (m_backend)
        m_backend->hue (val, b);
    return !!m_backend;
}

bool KMPlayerCallbackProcess::brightness (int val, bool b) {
    if (m_backend)
        m_backend->brightness (val, b);
    return !!m_backend;
}

bool KMPlayerCallbackProcess::contrast (int val, bool b) {
    if (m_backend)
        m_backend->contrast (val, b);
    return !!m_backend;
}

QString KMPlayerCallbackProcess::dcopName () {
    QString cbname;
    cbname.sprintf ("%s/%s", QString (kapp->dcopClient ()->appId ()).ascii (),
                             QString (m_callback->objId ()).ascii ());
    return cbname;
}

void KMPlayerCallbackProcess::runForConfig() {}

//-----------------------------------------------------------------------------

namespace KMPlayer {

class KMPlayerXMLPreferencesFrame : public QFrame {
public:
    KMPlayerXMLPreferencesFrame (QWidget * parent, KMPlayerCallbackProcess *);
    QTable * table;
    QDomDocument dom;
protected:
    void showEvent (QShowEvent *);
private:
    KMPlayerCallbackProcess * m_process;
};

} // namespace

KDE_NO_CDTOR_EXPORT KMPlayerXMLPreferencesFrame::KMPlayerXMLPreferencesFrame
(QWidget * parent, KMPlayerCallbackProcess * p)
 : QFrame (parent), m_process (p){
    QVBoxLayout * layout = new QVBoxLayout (this);
    table = new QTable (this);
    layout->addWidget (table);
}

KDE_NO_CDTOR_EXPORT KMPlayerXMLPreferencesPage::KMPlayerXMLPreferencesPage (KMPlayerCallbackProcess * p)
 : m_process (p), m_configframe (0L) {
}

KDE_NO_EXPORT void KMPlayerXMLPreferencesFrame::showEvent (QShowEvent *) {
    if (!m_process->haveConfig ())
        m_process->getConfigData ();
}

KDE_NO_EXPORT void KMPlayerXMLPreferencesPage::write (KConfig *) {
}

KDE_NO_EXPORT void KMPlayerXMLPreferencesPage::read (KConfig *) {
}

static QString attname ("NAME");
static QString atttype ("TYPE");
static QString attvalue ("VALUE");
static QString attstart ("START");
static QString attend ("END");
static QString valrange ("range");
static QString valnum ("num");
static QString valbool ("bool");
static QString valenum ("enum");
static QString valstring ("string");
static QString valtree ("tree");

KDE_NO_EXPORT void KMPlayerXMLPreferencesPage::sync (bool fromUI) {
    if (!m_configframe) return;
    QTable * table = m_configframe->table;
    QDomDocument & dom = m_configframe->dom;
    int row = 0;
    if (fromUI) {
        if (m_configframe->table->numCols () < 1) // not yet created
            return;
        if (dom.childNodes().length() != 1 || dom.firstChild().childNodes().length() < 1) {
            kdDebug () << "No valid data" << endl;
            return;
        }
        QDomDocument changeddom;
        QDomElement changedroot = changeddom.createElement (QString ("document"));
        for (QDomNode node = dom.firstChild().firstChild(); !node.isNull (); node = node.nextSibling (), row++) {
            QDomNamedNodeMap attr = node.attributes ();
            QDomNode n = attr.namedItem (attname);
            QDomNode t = attr.namedItem (atttype);
            if (!n.isNull () && !t.isNull ()) {
                if (m_configframe->table->text (row, 0) != n.nodeValue ()) {
                    kdDebug () << "Unexpected table text found at row " << row << endl;
                    return;
                }
                QDomNode v = attr.namedItem (attvalue);
                bool changed = false;
                if (t.nodeValue () == valnum || t.nodeValue () == valstring) {
                    QLineEdit * lineedit = static_cast<QLineEdit *>(table->cellWidget (row, 1));
                    if (lineedit->text () != v.nodeValue ()) {
                        v.setNodeValue (lineedit->text ());
                        changed = true;
                    }
                } else if (t.nodeValue () == valrange) {
                    int i = v.nodeValue ().toInt ();
                    QSlider * slider = static_cast<QSlider *>(table->cellWidget (row, 1));
                    if (slider->value () != i) {
                        v.setNodeValue (QString::number (slider->value ()));
                        changed = true;
                    }
                } else if (t.nodeValue () == valbool) {
                    bool b = attr.namedItem (attvalue).nodeValue ().toInt ();
                    QCheckBox * checkbox = static_cast<QCheckBox *>(table->cellWidget (row, 1));
                    if (checkbox->isChecked () != b) {
                        //node.toElement ().setAttribute (attvalue, QString::number (b ? 1 : 0));
                        v.setNodeValue (QString::number (b ? 0 : 1));
                        changed = true;
                    }
                } else if (t.nodeValue () == valenum) {
                    int i = v.nodeValue ().toInt ();
                    QComboBox * combobox = static_cast<QComboBox *>(table->cellWidget (row, 1));
                    if (combobox->currentItem () != i) {
                        v.setNodeValue (QString::number (combobox->currentItem ()));
                        changed = true;
                    }
                }
                if (changed)
                    changedroot.appendChild (node.cloneNode ());
            }
        }
        if (changedroot.childNodes().length() > 0) {
            changeddom.appendChild (changedroot);
            QCString str = changeddom.toCString ();
            kdDebug () << str << endl;
            QByteArray changeddata = str;
            changeddata.resize (str.length ());
            m_process->setChangedData (changeddata);
        }
    } else {
        if (!m_process->haveConfig ())
            return;
        QByteArray & data = m_process->configData ();
        if (!data.size ())
            return;
        if (m_configframe->table->numCols () < 1) { // not yet created
            QString err;
            int line, column;
            int first_column_width = 50;
            if (!dom.setContent (data, false, &err, &line, &column)) {
                kdDebug () << "Config data error " << err << " l:" << line << " c:" << column << endl;
                return;
            }
            if (dom.childNodes().length() != 1 || dom.firstChild().childNodes().length() < 1) {
                kdDebug () << "No valid data" << endl;
                return;
            }
            table->setNumCols (2);
            table->setNumRows (dom.firstChild().childNodes().length());
            table->verticalHeader ()->hide ();
            table->setLeftMargin (0);
            table->horizontalHeader ()->hide ();
            table->setTopMargin (0);
            table->setColumnReadOnly (0, true);
            QFontMetrics metrics (table->font ());
            dom.firstChild().normalize ();
            // set up the table fields
            for (QDomNode node = dom.firstChild().firstChild(); !node.isNull (); node = node.nextSibling (), row++) {
                QDomNamedNodeMap attr = node.attributes ();
                QDomNode n = attr.namedItem (attname);
                QDomNode t = attr.namedItem (atttype);
                QWidget * w = 0L;
                if (!n.isNull () && !t.isNull ()) {
                    m_configframe->table->setText (row, 0, n.nodeValue ());
                    int strwid = metrics.boundingRect (n.nodeValue ()).width ();
                    if (strwid > first_column_width)
                        first_column_width = strwid + 4;
                    if (t.nodeValue () == valnum || t.nodeValue () == valstring) {
                        w = new QLineEdit (attr.namedItem (attvalue).nodeValue (), table->viewport ());
                    } else if (t.nodeValue () == valrange) {
                        QString v = attr.namedItem (attvalue).nodeValue ();
                        QString s = attr.namedItem (attstart).nodeValue ();
                        QString e = attr.namedItem (attend).nodeValue ();
                        w = new QSlider (s.toInt (), e.toInt (), 1, v.toInt (), Qt::Horizontal, table->viewport());
                    } else if (t.nodeValue () == valbool) {
                        QString v = attr.namedItem (attvalue).nodeValue ();
                        QCheckBox * checkbox = new QCheckBox(table->viewport());
                        checkbox->setChecked (v.toInt ());
                        w = checkbox;
                    } else if (t.nodeValue () == valenum) {
                        QString v = attr.namedItem (attvalue).nodeValue ();
                        QComboBox * combobox = new QComboBox(table->viewport());
                        for (QDomNode d = node.firstChild(); !d.isNull (); d = d.nextSibling ())
                            if (d.nodeType () != QDomNode::TextNode)
                                combobox->insertItem (d.attributes ().namedItem (attvalue).nodeValue ());
                        combobox->setCurrentItem (v.toInt ());
                        w = combobox;
                    }
                    if (w) {
                        table->setCellWidget (row, 1, w);
                        for (QDomNode d = node.firstChild(); !d.isNull (); d = d.nextSibling ())
                            if (d.nodeType () == QDomNode::TextNode) {
                                QWhatsThis::add (w, d.nodeValue ());
                                break;
                            }
                    }
                }
            }
            table->setColumnWidth (0, first_column_width);
            table->setColumnStretchable (1, true);
        }
    }
}

KDE_NO_EXPORT void KMPlayerXMLPreferencesPage::prefLocation (QString & item, QString & icon, QString & tab) {
    item = i18n ("General Options");
    icon = QString ("kmplayer");
    tab = m_process->menuName ();
}

KDE_NO_EXPORT QFrame * KMPlayerXMLPreferencesPage::prefPage (QWidget * parent) {
    m_configframe = new KMPlayerXMLPreferencesFrame (parent, m_process);
    return m_configframe;
}

//-----------------------------------------------------------------------------

static const char * xine_supported [] = {
    "dvdnavsource", "urlsource", "vcdsource", 0L
};

KDE_NO_CDTOR_EXPORT Xine::Xine (PartBase * player)
    : KMPlayerCallbackProcess (player, "xine") {
#ifdef HAVE_XINE
    m_supported_sources = xine_supported;
    m_player->settings ()->addPage (m_configpage);
#endif
}

KDE_NO_CDTOR_EXPORT Xine::~Xine () {}

KDE_NO_EXPORT QString Xine::menuName () const {
    return i18n ("&Xine");
}

KDE_NO_EXPORT WId Xine::widget () {
    return view()->viewer()->embeddedWinId ();
}

KDE_NO_EXPORT void Xine::initProcess () {
    Process::initProcess ();
    connect (m_process, SIGNAL (processExited (KProcess *)),
            this, SLOT (processStopped (KProcess *)));
    connect (m_process, SIGNAL (receivedStdout (KProcess *, char *, int)),
            this, SLOT (processOutput (KProcess *, char *, int)));
    connect (m_process, SIGNAL (receivedStderr (KProcess *, char *, int)),
            this, SLOT (processOutput (KProcess *, char *, int)));
}

KDE_NO_EXPORT bool Xine::play () {
    if (playing () && m_backend && m_status != status_stop) { // paused
        m_backend->play ();
        return true;
    }
    return Process::play ();
}

void Xine::runForConfig () {
    initProcess ();
    QString xine_config = KProcess::quote (QString (QFile::encodeName (locateLocal ("data", "kmplayer/") + QString ("xine_config"))));
    printf ("kxineplayer -wid %lu", (unsigned long) widget ());
    *m_process << "kxineplayer -wid " << QString::number (widget ());
    if (m_have_config == config_probe) {
        printf (" -c");
        *m_process << " -c ";
    }
    printf (" -f %s", xine_config.ascii ());
    *m_process << " -f " << xine_config;
    printf (" -cb %s nomovie\n", dcopName ().ascii());
    *m_process << " -cb " << dcopName () << " nomovie";
    m_process->start (KProcess::NotifyOnExit, KProcess::All);
    return;
}

// TODO:input.v4l_video_device_path input.v4l_radio_device_path
// v4l:/Webcam/0   v4l:/Television/21600  v4l:/Radio/96
void Xine::urlForPlaying (const QString & urlstr) {
    KURL url (urlstr);
    QString myurl = url.isLocalFile () ? getPath (url) : url.url ();
    if (playing ()) {
        if (m_backend) {
            m_backend->setURL (myurl);
            m_backend->play ();
        }
        return;
    }
    if (m_have_config == config_probe || m_send_config == send_new) {
        runForConfig ();
        return;
    }
    initProcess ();
    QString xine_config = KProcess::quote (QString (QFile::encodeName (locateLocal ("data", "kmplayer/") + QString ("xine_config"))));
    m_request_seek = -1;
    kdDebug() << "Xine::play (" << myurl << ")" << endl;
    if (url.isEmpty ())
        return;
    Settings *settings = m_player->settings ();
    initProcess ();
    printf ("kxineplayer -wid %lu", (unsigned long) widget ());
    *m_process << "kxineplayer -wid " << QString::number (widget ());
    printf (" -f %s", xine_config.ascii ());
    *m_process << " -f " << xine_config;

    QString strVideoDriver = QString (settings->videodrivers[settings->videodriver].driver);
    if (strVideoDriver == QString ("x11"))
        strVideoDriver = QString ("xshm");
    if (!strVideoDriver.isEmpty ()) {
        printf (" -vo %s", strVideoDriver.lower().ascii());
        *m_process << " -vo " << strVideoDriver.lower();
    }
    QString strAudioDriver = QString (settings->audiodrivers[settings->audiodriver].driver);
    if (!strAudioDriver.isEmpty ()) {
        if (strAudioDriver.startsWith (QString ("alsa")))
            strAudioDriver = QString ("alsa");
        printf (" -ao %s", strAudioDriver.lower().ascii());
        *m_process << " -ao " << strAudioDriver.lower();
    }
    printf (" -cb %s", dcopName ().ascii());
    *m_process << " -cb " << dcopName ();
    if (m_have_config == config_unknown) {
        printf (" -c");
        *m_process << " -c";
    }
    if (url.url ().startsWith (QString::fromLatin1 ("dvd://")) &&
            !settings->dvddevice.isEmpty ()) {
        printf (" -dvd-device %s", settings->dvddevice.ascii ());
        *m_process << " -dvd-device " << settings->dvddevice;
    } else if (url.url ().startsWith (QString::fromLatin1 ("vcd://")) &&
            !settings->vcddevice.isEmpty ()) {
        printf (" -vcd-device %s", settings->vcddevice.ascii ());
        *m_process << " -vcd-device " << settings->vcddevice;
    }
    const KURL & sub_url = m_source->subUrl ();
    if (!sub_url.isEmpty ()) {
        QString surl = KProcess::quote (QString (QFile::encodeName
                  (sub_url.isLocalFile () ?
                   QFileInfo (getPath (sub_url)).absFilePath () :
                   sub_url.url ())));
        printf (" -sub %s ", surl.ascii ());
        *m_process <<" -sub " << surl;
    }
    if (m_source->url ().isLocalFile ()) {
        m_process->setWorkingDirectory 
            (QFileInfo (getPath (m_source->url ())).dirPath (true));
    }
    m_url = url.url ();
    myurl = KProcess::quote (QString (QFile::encodeName (myurl)));
    printf (" %s\n", myurl.ascii ());
    *m_process << " " << myurl;
    m_process->start (KProcess::NotifyOnExit, KProcess::All);
    if (m_process->isRunning ())
        QTimer::singleShot (0, this, SLOT (emitStarted ()));
}

KDE_NO_EXPORT bool Xine::quit () {
    kdDebug () << "Xine::quit ()" << endl;
    if (m_source)
        disconnect (m_source, SIGNAL (currentURL (const QString &)), this, SLOT (urlForPlaying (const QString &)));
    if (m_have_config == config_probe)
        m_have_config = config_unknown; // hmm
    if (m_send_config == send_new)
        m_send_config = send_no; // oh well
    if (!m_process || !m_process->isRunning ()) return true;
    if (m_backend) {
        m_backend->quit ();
        QTime t;
        t.start ();
        do {
            KProcessController::theKProcessController->waitForProcessExit (2);
        } while (t.elapsed () < 2000 && m_process->isRunning ());
        kdDebug () << "DCOP quit " << t.elapsed () << endl;
    }
    if (m_process->isRunning () && !Process::stop ())
        processStopped (0L); // give up
    return true;
}

KDE_NO_EXPORT void Xine::setFinished () {
    KMPlayerCallbackProcess::setFinished ();
    if (!m_source) return; // initial case?
    kdDebug () << "Xine::finished () " << endl;
    if (m_source->next ().isEmpty ()) {
        quit ();
        m_source->first ();
    } else
        QTimer::singleShot (0, m_source, SLOT (getCurrent ()));
}

KDE_NO_EXPORT bool Xine::seek (int pos, bool absolute) {
    if (in_gui_update || !playing () ||
            !m_backend ||
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

KDE_NO_EXPORT void Xine::processOutput (KProcess *, char * str, int slen) {
    View * v = view ();
    if (v && slen > 0)
        v->addText (QString::fromLocal8Bit (str, slen));
}

KDE_NO_EXPORT void Xine::processStopped (KProcess *) {
    delete m_backend;
    m_backend = 0L;
    QTimer::singleShot (0, this, SLOT (emitFinished ()));
    if (m_send_config == send_try) {
        m_send_config = send_new; // we failed, retry ..
        play ();
    }
}

//-----------------------------------------------------------------------------

static const char * gst_supported [] = {
    "urlsource", 0L
};

KDE_NO_CDTOR_EXPORT GStreamer::GStreamer (PartBase * player)
    : KMPlayerCallbackProcess (player, "gst") {
#ifdef HAVE_GSTREAMER
    m_supported_sources = gst_supported;
#endif
}

KDE_NO_CDTOR_EXPORT GStreamer::~GStreamer () {}

KDE_NO_EXPORT QString GStreamer::menuName () const {
    return i18n ("&GStreamer");
}

KDE_NO_EXPORT WId GStreamer::widget () {
    return view()->viewer()->embeddedWinId ();
}

KDE_NO_EXPORT void GStreamer::initProcess () {
    Process::initProcess ();
    connect (m_process, SIGNAL (processExited (KProcess *)),
            this, SLOT (processStopped (KProcess *)));
    connect (m_process, SIGNAL (receivedStdout (KProcess *, char *, int)),
            this, SLOT (processOutput (KProcess *, char *, int)));
    connect (m_process, SIGNAL (receivedStderr (KProcess *, char *, int)),
            this, SLOT (processOutput (KProcess *, char *, int)));
}

KDE_NO_EXPORT bool GStreamer::play () {
    if (playing () && m_backend && m_status != status_stop) { // paused
        m_backend->play ();
        return true;
    }
    return Process::play ();
}

void GStreamer::urlForPlaying (const QString & urlstr) {
    KURL url (urlstr);
    if (url.isEmpty ()) {
        quit ();
        return;
    }
    QString myurl = url.isLocalFile () ? QString("file://%1").arg (KURL::encode_string (getPath (url))) : url.url ();
    if (playing ()) {
        if (m_backend) {
            m_backend->setURL (myurl);
            m_backend->play ();
        }
        return;
    }
    initProcess ();
    m_request_seek = -1;
    kdDebug() << "GStreamer::play (" << myurl << ")" << endl;
    Settings *settings = m_player->settings ();
    initProcess ();
    printf ("kgstplayer -wid %lu", (unsigned long) widget ());
    *m_process << "kgstplayer -wid " << QString::number (widget ());

    QString strVideoDriver = QString (settings->videodrivers[settings->videodriver].driver);
    if (!strVideoDriver.isEmpty ()) {
        printf (" -vo %s", strVideoDriver.lower().ascii());
        *m_process << " -vo " << strVideoDriver.lower();
    }
    QString strAudioDriver = QString (settings->audiodrivers[settings->audiodriver].driver);
    if (!strAudioDriver.isEmpty ()) {
        if (strAudioDriver.startsWith (QString ("alsa")))
            strAudioDriver = QString ("alsa");
        printf (" -ao %s", strAudioDriver.lower().ascii());
        *m_process << " -ao " << strAudioDriver.lower();
    }
    printf (" -cb %s", dcopName ().ascii());
    *m_process << " -cb " << dcopName ();
    if (m_source->url ().isLocalFile ()) {
        m_process->setWorkingDirectory 
            (QFileInfo (getPath (m_source->url ())).dirPath (true));
    }
    m_url = url.url ();
    myurl = KProcess::quote (QString (QFile::encodeName (myurl)));
    printf (" %s\n", myurl.ascii ());
    *m_process << " " << myurl;
    m_process->start (KProcess::NotifyOnExit, KProcess::All);
    if (m_process->isRunning ())
        QTimer::singleShot (0, this, SLOT (emitStarted ()));
}

KDE_NO_EXPORT bool GStreamer::quit () {
    kdDebug () << "GStreamer::quit ()" << endl;
    disconnect (m_source, SIGNAL (currentURL (const QString &)), this, SLOT (urlForPlaying (const QString &)));
    if (!m_process || !m_process->isRunning ()) return true;
    if (m_backend) {
        m_backend->quit ();
        QTime t;
        t.start ();
        do {
            KProcessController::theKProcessController->waitForProcessExit (2);
        } while (t.elapsed () < 2000 && m_process->isRunning ());
        kdDebug () << "DCOP quit " << t.elapsed () << endl;
    }
    if (m_process->isRunning () && !Process::stop ())
        processStopped (0L); // give up
    return true;
}

KDE_NO_EXPORT void GStreamer::setFinished () {
    KMPlayerCallbackProcess::setFinished ();
    if (!m_source) return; // initial case?
    kdDebug () << "GStreamer::finished () " << endl;
    if (m_source->next ().isEmpty ()) {
        quit ();
        m_source->first ();
    } else
        QTimer::singleShot (0, m_source, SLOT (getCurrent ()));
}

KDE_NO_EXPORT bool GStreamer::seek (int pos, bool absolute) {
    if (in_gui_update || !playing () ||
            !m_backend ||
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

KDE_NO_EXPORT void GStreamer::processOutput (KProcess *, char * str, int slen) {
    View * v = view ();
    if (v && slen > 0)
        v->addText (QString::fromLocal8Bit (str, slen));
}

KDE_NO_EXPORT void GStreamer::processStopped (KProcess *) {
    delete m_backend;
    m_backend = 0L;
    QTimer::singleShot (0, this, SLOT (emitFinished ()));
}

//-----------------------------------------------------------------------------

FFMpeg::FFMpeg (PartBase * player)
    : Process (player, "ffmpeg") {
}

KDE_NO_CDTOR_EXPORT FFMpeg::~FFMpeg () {
}

KDE_NO_EXPORT void FFMpeg::init () {
}

void FFMpeg::urlForPlaying (const QString & urlstr) {
    initProcess ();
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
        if (m_source->frequency () >= 0) {
            KProcess process;
            process.setUseShell (true);
            process << "v4lctl -c " << m_source->videoDevice () << " setnorm " << m_source->videoNorm ();
            kdDebug () << "v4lctl -c " << m_source->videoDevice () << " setnorm " << m_source->videoNorm () << endl;
            process.start (KProcess::Block);
            process.clearArguments();
            process << "v4lctl -c " << m_source->videoDevice () << " setfreq " << QString::number (m_source->frequency ());
            kdDebug () << "v4lctl -c " << m_source->videoDevice () << " setfreq " << m_source->frequency () << endl;
            process.start (KProcess::Block);
            cmd += QString (" -tvstd ") + m_source->videoNorm ();
        }
    } else {
        KURL url (urlstr);
        cmd += QString ("-i ") + KProcess::quote (QString (QFile::encodeName (url.isLocalFile () ? getPath (url) : url.url ())));
    }
    cmd += QChar (' ') + arguments;
    cmd += QChar (' ') + KProcess::quote (QString (QFile::encodeName (outurl)));
    printf ("%s\n", (const char *) cmd.local8Bit ());
    *m_process << cmd;
    m_process->start (KProcess::NotifyOnExit, KProcess::All);
    if (m_process->isRunning ())
        QTimer::singleShot (0, this, SLOT (emitStarted ()));
}

KDE_NO_EXPORT bool FFMpeg::stop () {
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
    disconnect (m_source, SIGNAL (currentURL (const QString &)), this, SLOT (urlForPlaying (const QString &)));
    QTimer::singleShot (0, this, SLOT (emitFinished ()));
}

#include "kmplayerprocess.moc"
