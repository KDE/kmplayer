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

KMPlayerProcess::KMPlayerProcess (KMPlayer * player)
    : QObject (player), m_player (player), m_source (0L), m_process (0L) {}

KMPlayerProcess::~KMPlayerProcess () {
    stop ();
    delete m_process;
}

void KMPlayerProcess::init () {
}

void KMPlayerProcess::initProcess () {
    delete m_process;
    m_process = new KProcess;
    m_process->setUseShell (true);
    if (m_source) m_source->setPosition (0);
    m_url.truncate (0);
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

bool KMPlayerProcess::seek (int /*pos*/, bool /*absolute*/) {
    return false;
}

bool KMPlayerProcess::volume (int /*pos*/, bool /*absolute*/) {
    return false;
}

bool KMPlayerProcess::saturation (int /*pos*/, bool /*absolute*/) {
    return false;
}

bool KMPlayerProcess::hue (int /*pos*/, bool /*absolute*/) {
    return false;
}

bool KMPlayerProcess::contrast (int /*pos*/, bool /*absolute*/) {
    return false;
}

bool KMPlayerProcess::brightness (int /*pos*/, bool /*absolute*/) {
    return false;
}

bool KMPlayerProcess::grabPicture (const KURL & /*url*/, int /*pos*/) {
    return false;
}

bool KMPlayerProcess::stop () {
    if (!playing ()) return true;
    do {
        m_process->kill (SIGTERM);
        KProcessController::theKProcessController->waitForProcessExit (1);
        if (!m_process->isRunning ())
            break;
        m_process->kill (SIGKILL);
        KProcessController::theKProcessController->waitForProcessExit (1);
        if (m_process->isRunning ()) {
            KMessageBox::error (m_player->view (), i18n ("Failed to end Player process."), i18n ("KMPlayer: Error"));
        }
    } while (false);
    return !m_process->isRunning ();
}

void KMPlayerProcess::setSource (KMPlayerSource * source) {
    stop ();
    m_urls.clear();
    m_source = source;
}
//-----------------------------------------------------------------------------

static bool proxyForURL (const KURL& url, QString& proxy) {
    KProtocolManager::slaveProtocol (url, proxy);
    return !proxy.isNull ();
}

//-----------------------------------------------------------------------------

MPlayerBase::MPlayerBase (KMPlayer * player)
    : KMPlayerProcess (player), m_use_slave (true) {
    m_process = new KProcess;
}

MPlayerBase::~MPlayerBase () {
}

void MPlayerBase::initProcess () {
    KMPlayerProcess::initProcess ();
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

bool MPlayerBase::sendCommand (const QString & cmd) {
    if (playing () && m_use_slave) {
        commands.push_front (cmd + "\n");
        printf ("eval %s", commands.last ().latin1 ());
        m_process->writeStdin (QFile::encodeName(commands.last ()),
                               commands.last ().length ());
        return true;
    }
    return false;
}

bool MPlayerBase::stop () {
    if (!source () || !m_process || !m_process->isRunning ()) return true;
    disconnect (m_process, SIGNAL (processExited (KProcess *)),
                this, SLOT (processStopped (KProcess *)));
    if (!m_use_slave) {
        void (*oldhandler)(int) = signal(SIGTERM, SIG_IGN);
        ::kill (-1 * ::getpid (), SIGTERM);
        signal(SIGTERM, oldhandler);
    }
    QTime t;
    t.start ();
    do {
        KProcessController::theKProcessController->waitForProcessExit (2);
    } while (t.elapsed () < 2000 && m_process->isRunning ());
    if (m_process->isRunning ())
        KMPlayerProcess::stop ();
    processStopped (0L);
    return true;
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
    if (playing ())
        return sendCommand (QString ("gui_play"));
    stop ();
    initProcess ();
    source ()->setPosition (0);
    QString args = source ()->options () + ' ';
    KURL url = m_source->url ();
    if (m_urls.count () > 0) {
        url = KURL (m_urls.front ());
        m_urls.pop_front ();
    }
    if (!url.isEmpty ()) {
        m_url = url.url ();
        if (url.isLocalFile ()) {
            QFileInfo fi (url.path ());
            m_process->setWorkingDirectory (fi.dirPath (true));
            m_url = fi.fileName ();
        }
        args += KProcess::quote (QString (QFile::encodeName (m_url)));
    }
    m_tmpURL.truncate (0);
    m_urls.clear ();
    if (!source ()->identified ()) {
        m_refURLRegExp.setPattern (m_player->settings ()->referenceurlpattern);
        m_refRegExp.setPattern (m_player->settings ()->referencepattern);
        args += QString (" -quiet -nocache -identify -frames 0 ");
    } else {
        if (m_player->settings ()->loop)
            args += QString (" -loop 0");
        if (!m_source->subUrl ().isEmpty ()) {
            args += QString (" -sub ");
            const KURL & sub_url (source ()->subUrl ());
            if (!sub_url.isEmpty ()) {
                QString myurl (sub_url.isLocalFile () ? sub_url.path () : sub_url.url ());
                args += KProcess::quote (QString (QFile::encodeName (myurl)));
            }
        }
    }
    return run (args.ascii (), source ()->pipeCmd ().ascii ());
}

bool MPlayer::stop () {
    if (!source () || !m_process || !m_process->isRunning ()) return true;
    kdDebug () << "MPlayer::stop ()" << endl;
    if (m_use_slave)
        sendCommand (QString ("quit"));
    return MPlayerBase::stop ();
}

bool MPlayer::pause () {
    return sendCommand (QString ("pause"));
}

bool MPlayer::seek (int pos, bool absolute) {
    if (!m_source || !m_source->hasLength ()) return false;
    QString cmd;
    cmd.sprintf ("seek %d %d", pos/10, absolute ? 2 : 0);
    return sendCommand (cmd);
}

bool MPlayer::volume (int incdec, bool absolute) {
    if (!source ()) return false;
    if (!absolute)
        return sendCommand (QString ("volume ") + QString::number (incdec));
    return false;
}

bool MPlayer::saturation (int val, bool absolute) {
    if (!source ()) return false;
    QString cmd;
    cmd.sprintf ("saturation %d %d", val, absolute ? 1 : 0);
    return sendCommand (cmd);
}

bool MPlayer::hue (int val, bool absolute) {
    if (!source ()) return false;
    QString cmd;
    cmd.sprintf ("hue %d %d", val, absolute ? 1 : 0);
    return sendCommand (cmd);
}

bool MPlayer::contrast (int val, bool /*absolute*/) {
    if (!source ()) return false;
    QString cmd;
    cmd.sprintf ("contrast %d 1", val);
    return sendCommand (cmd);
}

bool MPlayer::brightness (int val, bool /*absolute*/) {
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
    KMPlayerSettings *settings = m_player->settings ();
    m_cacheRegExp.setPattern (settings->cachepattern);
    m_indexRegExp.setPattern (settings->indexpattern);
    m_posRegExp.setPattern (m_player->settings ()->positionpattern);

    m_use_slave = !(pipe && pipe[0]);
    if (!m_use_slave) {
        printf ("%s | ", pipe);
        *m_process << pipe << " | ";
    }
    printf ("mplayer -wid %lu ", (unsigned long) widget ()->winId ());
    *m_process << "mplayer -wid " << QString::number (widget ()->winId ());

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
    if (strAudioDriver != "") {
        printf (" -ao %s", strAudioDriver.lower().ascii());
        *m_process << " -ao " << strAudioDriver.lower();
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
        *m_process << " " << settings->additionalarguments;
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

bool MPlayer::grabPicture (const KURL & url, int pos) {
    stop ();
    initProcess ();
    QString outdir = locateLocal ("data", "kmplayer/");
    m_grabfile = outdir + QString ("00000001.jpg");
    unlink (m_grabfile.ascii ());
    QString myurl (url.isLocalFile () ? url.path () : url.url ());
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

void MPlayer::processOutput (KProcess *, char * str, int slen) {
    KMPlayerView * v = static_cast <KMPlayerView *> (m_player->view ());
    if (!v || slen <= 0) return;

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
                int pos = int (10.0 * m_posRegExp.cap (1).toFloat ());
                source ()->setPosition (pos);
                emit positionChanged (pos);
            } else if (m_cacheRegExp.search (out) > -1) {
                emit loading (int (m_cacheRegExp.cap (1).toDouble ()));
            }
        } else if (!source ()->identified () && m_refURLRegExp.search (out) > -1) {
            kdDebug () << "Reference mrl " << m_refURLRegExp.cap (1) << endl;
            if (!m_tmpURL.isEmpty ())
                m_urls.push_back (m_tmpURL);
            m_tmpURL = m_refURLRegExp.cap (1);
            if (m_source->url () == m_tmpURL || m_url == m_tmpURL)
                m_tmpURL.truncate (0);
        } else if (!source ()->identified () && m_refRegExp.search (out) > -1) {
            kdDebug () << "Reference File " << endl;
            m_tmpURL.truncate (0);
        } else {
            v->addText (out + '\n');
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
                        m_source->setAspect (1.0*movie_width/movie_height);
                        if (m_player->settings ()->sizeratio)
                            v->viewer ()->setAspect (m_source->aspect ());
                    }
                } else if (startRegExp.search (out) > -1) {
                        emit startPlaying ();
                }
            }
        }
    } while (slen > 0);
}

void MPlayer::processStopped (KProcess * p) {
    if (p && !m_grabfile.isEmpty ()) {
        emit grabReady (m_grabfile);
        m_grabfile.truncate (0);
    } else if (p && !source ()->identified ()) {
        if (!m_tmpURL.isEmpty () && m_tmpURL != m_source->url ().url ()) {
            m_urls.push_back (m_tmpURL);
            m_tmpURL.truncate (0);
        }
        source ()->setIdentified ();
        QTimer::singleShot (0, this, SLOT (play ()));
    } else
        MPlayerBase::processStopped (p);
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
    if (!m_source || m_source->recordCmd ().isNull ())
        return false;
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
    const KURL & url (source ()->url ());
    QString myurl = url.isLocalFile () ? url.path () : url.url ();
    bool post090 = m_player->settings ()->mplayerpost090;
    if (!myurl.isEmpty ()) {
        if (!post090 && myurl.startsWith (QString ("tv://")))
            ; // skip it
        else if (!post090 && myurl.startsWith (QString ("vcd://")))
            args += myurl.replace (0, 6, QString (" -vcd "));
        else if (!post090 && myurl.startsWith (QString ("dvd://")))
            args += myurl.replace (0, 6, QString (" -dvd "));
        else {
            QString myurl = url.isLocalFile () ? url.path () : url.url ();
            args += ' ' + KProcess::quote (QString (QFile::encodeName (myurl)));
        }
    }
    QString outurl = KProcess::quote (QString (QFile::encodeName (m_recordurl.isLocalFile () ? m_recordurl.path () : m_recordurl.url ())));
    kdDebug () << args << " -o " << outurl << endl;
    *m_process << args << " -o " << outurl;
    m_process->start (KProcess::NotifyOnExit, KProcess::NoCommunication);
    success = m_process->isRunning ();
    if (success)
        emitStarted ();
    return success;
}

bool MEncoder::stop () {
    kdDebug () << "MEncoder::stop ()" << endl;
    if (!source () || !m_process || !m_process->isRunning ()) return true;
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

void KMPlayerCallback::started (QByteArray data) {
    m_process->setStarted (data);
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

KMPlayerCallbackProcess::KMPlayerCallbackProcess (KMPlayer * player)
 : KMPlayerProcess (player),
   m_callback (new KMPlayerCallback (this)),
   m_configpage (new KMPlayerXMLPreferencesPage (this)),
   m_have_config (unknown) {
#ifdef HAVE_XINE
    m_player->settings ()->pagelist.push_back (m_configpage);
#endif
}

KMPlayerCallbackProcess::~KMPlayerCallbackProcess () {
    delete m_callback;
    delete m_configpage;
}

void KMPlayerCallbackProcess::setURL (const QString & url) {
    kdDebug () << "Reference mrl " << url << endl;
    if (m_source->url ().url () != url && m_url != url)
        m_urls.push_back (url);
}

void KMPlayerCallbackProcess::setStatusMessage (const QString & /*msg*/) {
}

void KMPlayerCallbackProcess::setErrorMessage (int /*code*/, const QString & /*msg*/) {
}

void KMPlayerCallbackProcess::setFinished () {
    QTimer::singleShot (0, this, SLOT (emitFinished ()));
}

void KMPlayerCallbackProcess::setPlaying () {
    KMPlayerView * v = static_cast <KMPlayerView *> (m_player->view ());
    if (!v) return;
    QTimer::singleShot (0, v, SLOT (startsToPlay ())); // FIXME
    emit startPlaying (); // FIXME TOO :-)
}

void KMPlayerCallbackProcess::setStarted (QByteArray &) {
}

void KMPlayerCallbackProcess::setMovieParams (int len, int w, int h, float a) {
    kdDebug () << "setMovieParams " << len << " " << w << "," << h << endl;
    m_source->setWidth (w);
    m_source->setHeight (h);
    m_source->setAspect (a);
    m_source->setLength (len);
    if (m_player->settings ()->sizeratio) {
        KMPlayerView * v = static_cast <KMPlayerView *> (m_player->view ());
        if (!v) return;
        v->viewer ()->setAspect (a);
        v->updateLayout ();
    }
}

void KMPlayerCallbackProcess::setMoviePosition (int position) {
    m_source->setPosition (position);
    emit positionChanged (position);
}

void KMPlayerCallbackProcess::setLoadingProgress (int percentage) {
    emit loading (percentage);
}

bool KMPlayerCallbackProcess::getConfigData (QByteArray & data) {
    if (m_have_config == unknown) {
        if (playing ())
            return false; // it will come ..
        m_have_config = probe;
        play ();
    } else if (m_have_config == yes) {
        data = m_configdata;
        return true;
    }
    return false;
}
//-----------------------------------------------------------------------------
#include <qlayout.h>
#include <qtable.h>
#include <qlineedit.h>
#include <qslider.h>
#include <qcombobox.h>
#include <qcheckbox.h>
#include <qdom.h>

class KMPlayerXMLPreferencesFrame : public QFrame {
public:
    KMPlayerXMLPreferencesFrame (QWidget * parent);
    QTable * table;
};

KMPlayerXMLPreferencesFrame::KMPlayerXMLPreferencesFrame (QWidget * parent)
 : QFrame (parent) {
    QVBoxLayout * layout = new QVBoxLayout (this);
    table = new QTable (this);
    layout->addWidget (table);
}

KMPlayerXMLPreferencesPage::KMPlayerXMLPreferencesPage (KMPlayerCallbackProcess * p)
 : m_process (p), m_configframe (0L) {
}

void KMPlayerXMLPreferencesPage::write (KConfig *) {
}

void KMPlayerXMLPreferencesPage::read (KConfig *) {
}

void KMPlayerXMLPreferencesPage::sync (bool fromUI) {
    if (fromUI) {
        ; // TODO: send changes to Xine
    } else {
        QByteArray data;
        if (!m_process->getConfigData (data))
            return;
        // if not have data, get it from Xine
        if (!data.size ())
            return;
        if (m_configframe->table->numCols () < 1) { // not yet created
            QDomDocument dom;
            QString err;
            int line, column;
            if (!dom.setContent (data, false, &err, &line, &column)) {
                kdDebug () << "Config data error " << err << " l:" << line << " c:" << column << endl;
                return;
            }
            if (dom.childNodes().length() != 1 || dom.firstChild().childNodes().length() < 1) {
                kdDebug () << "No valid data" << endl;
                return;
            }
            QTable * table = m_configframe->table;
            table->setNumCols (2);
            table->setNumRows (dom.firstChild().childNodes().length());
            table->verticalHeader ()->hide ();
            table->setLeftMargin (0);
            table->horizontalHeader ()->hide ();
            table->setTopMargin (0);
            table->setColumnReadOnly (0, true);
            table->setColumnWidth (0, 250);
            table->setColumnWidth (1, 250);
            // set up the table fields
            int row = 0;
            for (QDomNode node = dom.firstChild().firstChild(); !node.isNull (); node = node.nextSibling (), row++) {
                QDomNamedNodeMap attr = node.attributes ();
                QDomNode n = attr.namedItem (QString ("NAME"));
                QDomNode t = attr.namedItem (QString ("TYPE"));
                if (!n.isNull () && !t.isNull ()) {
                    m_configframe->table->setText (row, 0, n.nodeValue ());
                    if (t.nodeValue () == QString ("num") || t.nodeValue () == QString ("string")) {
                        QString v = attr.namedItem (QString ("VALUE")).nodeValue ();
                        table->setText (row, 1, v);
                    } else if (t.nodeValue () == QString ("range")) {
                        QString v = attr.namedItem (QString ("VALUE")).nodeValue ();
                        QString s = attr.namedItem (QString ("START")).nodeValue ();
                        QString e = attr.namedItem (QString ("END")).nodeValue ();
                        QSlider * slider = new QSlider (s.toInt (), e.toInt (), 1, v.toInt (), Qt::Horizontal, table);
                        table->setCellWidget (row, 1, slider);
                    } else if (t.nodeValue () == QString ("bool")) {
                        QString v = attr.namedItem (QString ("VALUE")).nodeValue ();
                        QCheckBox * checkbox = new QCheckBox (table);
                        checkbox->setChecked (v.toInt ());
                        table->setCellWidget (row, 1, checkbox);
                    } else if (t.nodeValue () == QString ("enum")) {
                        QString v = attr.namedItem (QString ("VALUE")).nodeValue ();
                        QComboBox * combobox = new QComboBox (table);
                        for (QDomNode d = node.firstChild(); !d.isNull (); d = d.nextSibling ())
                            combobox->insertItem (d.attributes ().namedItem (QString ("VALUE")).nodeValue ());
                        combobox->setCurrentItem (v.toInt ());
                        table->setCellWidget (row, 1, combobox);
                    }
                }
            }
        }
    }
}

void KMPlayerXMLPreferencesPage::prefLocation (QString & item, QString & icon, QString & tab) {
    item = i18n ("General Options");
    icon = QString ("kmplayer");
    tab = i18n ("Xine");
}

QFrame * KMPlayerXMLPreferencesPage::prefPage (QWidget * parent) {
    m_configframe = new KMPlayerXMLPreferencesFrame (parent);
    return m_configframe;
}

//-----------------------------------------------------------------------------

Xine::Xine (KMPlayer * player)
    : KMPlayerCallbackProcess (player), m_backend (0L) {
}

Xine::~Xine () {}

QWidget * Xine::widget () {
    return static_cast <KMPlayerView *> (m_player->view())->viewer();
}

void Xine::initProcess () {
    KMPlayerProcess::initProcess ();
    connect (m_process, SIGNAL (processExited (KProcess *)),
            this, SLOT (processStopped (KProcess *)));
    connect (m_process, SIGNAL (receivedStdout (KProcess *, char *, int)),
            this, SLOT (processOutput (KProcess *, char *, int)));
    connect (m_process, SIGNAL (receivedStderr (KProcess *, char *, int)),
            this, SLOT (processOutput (KProcess *, char *, int)));
}

bool Xine::play () {
    if (playing ()) {
        if (m_backend)
            m_backend->play ();
        return true;
    }
    QString cbname;
    cbname.sprintf ("%s/%s", QString (kapp->dcopClient ()->appId ()).ascii (),
                             QString (m_callback->objId ()).ascii ());
    if (m_have_config == probe) {
        initProcess ();
        printf ("kxineplayer -wid %lu", (unsigned long) widget ()->winId ());
        *m_process << "kxineplayer -wid " << QString::number (widget ()->winId ());
        printf (" -cb %s -c nomovie\n", cbname.ascii());
        *m_process << " -cb " << cbname << " -c nomovie";
        m_process->start (KProcess::NotifyOnExit, KProcess::All);
        return m_process->isRunning ();
    }
    KURL url = m_source->url ();
    if (m_urls.count () > 0) {
        url = KURL (m_urls.front ());
        m_urls.pop_front ();
    }
    kdDebug() << "Xine::play (" << url.url() << ")" << endl;
    if (url.isEmpty ())
        return false;
    m_urls.clear ();
    KMPlayerSettings *settings = m_player->settings ();
    initProcess ();
    printf ("kxineplayer -wid %lu", (unsigned long) widget ()->winId ());
    *m_process << "kxineplayer -wid " << QString::number (widget ()->winId ());

    QString strVideoDriver = QString (settings->videodrivers[settings->videodriver].driver);
    if (strVideoDriver == QString ("x11"))
        strVideoDriver = QString ("xshm");
    if (strVideoDriver != "") {
        printf (" -vo %s", strVideoDriver.lower().ascii());
        *m_process << " -vo " << strVideoDriver.lower();
    }
    QString strAudioDriver = QString (settings->audiodrivers[settings->audiodriver].driver);
    if (strAudioDriver != "") {
        printf (" -ao %s", strAudioDriver.lower().ascii());
        *m_process << " -ao " << strAudioDriver.lower();
    }
    printf (" -cb %s", cbname.ascii());
    *m_process << " -cb " << cbname;
    if (m_have_config == unknown) {
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
                   QFileInfo (sub_url.path ()).absFilePath () :
                   sub_url.url ())));
        printf (" -sub %s ", surl.ascii ());
        *m_process <<" -sub " << surl;
    }
    m_url = url.url ();
    if (url.isLocalFile ()) {
        QFileInfo fi (url.path ());
        m_process->setWorkingDirectory (fi.dirPath (true));
        m_url = fi.fileName ();
    }
    QString myurl = KProcess::quote (m_url);
    printf (" %s\n", myurl.ascii ());
    *m_process << " " << myurl;
    m_process->start (KProcess::NotifyOnExit, KProcess::All);
    if (m_process->isRunning ()) {
        QTimer::singleShot (0, this, SLOT (emitStarted ()));
        return true;
    }
    return false;
}

bool Xine::stop () {
    kdDebug () << "Xine::stop ()" << endl;
    if (m_have_config == probe)
        m_have_config = unknown; // hmm
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
    if (m_process->isRunning () && !KMPlayerCallbackProcess::stop ())
        processStopped (0L); // give up
    return true;
}

void Xine::setFinished () {
    kdDebug () << "Xine::finished () " << m_urls.count () << endl;
    if (m_urls.count ()) {
        QString url = m_urls.front ();
        m_urls.pop_front ();
        m_backend->setURL (url);
        m_backend->play ();
        return;
    }
    stop ();
}

bool Xine::pause () {
    if (!playing () || !m_backend) return false;
    m_backend->pause ();
    return true;
}

bool Xine::seek (int pos, bool absolute) {
    if (!playing () || !m_backend || !m_source->hasLength ()) return false;
    m_backend->seek (absolute ? pos : m_source->position () + pos, true);
    return true;
}

void Xine::processOutput (KProcess *, char * str, int slen) {
    KMPlayerView * v = static_cast <KMPlayerView *> (m_player->view ());
    if (v && slen > 0)
        v->addText (QString (str));
}

void Xine::processStopped (KProcess *) {
    delete m_backend;
    m_backend = 0L;
    QTimer::singleShot (0, this, SLOT (emitFinished ()));
}

void Xine::setStarted (QByteArray & data) {
    QString dcopname;
    dcopname.sprintf ("kxineplayer-%u", m_process->pid ());
    kdDebug () << "up and running " << dcopname << endl;
    m_backend = new KMPlayerBackend_stub (dcopname.ascii (), "KMPlayerBackend");
    if (m_have_config == probe || m_have_config == unknown) {
        m_configdata = data;
        if (m_have_config == probe) {
            m_have_config = data.size () ? yes : no;
            stop ();
            m_configpage->sync (false);
            return;
        }
        m_have_config = data.size () ? yes : no;
    }
    KMPlayerSettings * settings = m_player->settings ();
    saturation (settings->saturation, true);
    hue (settings->hue, true);
    brightness (settings->brightness, true);
    contrast (settings->contrast, true);
    //const KURL & url = m_source->subUrl ();
    //if (!url.isEmpty ())
    //    m_backend->setSubTitleURL (url.isLocalFile () ? url.path () : url.url ());
    m_backend->play ();
}

bool Xine::saturation (int val, bool) {
    if (m_backend)
        m_backend->saturation (65535 * (val + 100) / 200, true);
    return !!m_backend;
}

bool Xine::hue (int val, bool) {
    if (m_backend)
        m_backend->hue (65535 * (val + 100) / 200, true);
    return !!m_backend;
}

bool Xine::brightness (int val, bool) {
    if (m_backend)
        m_backend->brightness (65535 * (val + 100) / 200, true);
    return !!m_backend;
}

bool Xine::contrast (int val, bool) {
    if (m_backend)
        m_backend->contrast (65535 * (val + 100) / 200, true);
    return !!m_backend;
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
    delete m_process;
    m_process = new KProcess;
    m_process->setUseShell (true);
    connect (m_process, SIGNAL (processExited (KProcess *)),
            this, SLOT (processStopped (KProcess *)));
    QString outurl = QString (QFile::encodeName (m_recordurl.isLocalFile () ? m_recordurl.path () : m_recordurl.url ()));
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
        cmd += QString ("-i ") + KProcess::quote (m_source->url ().isLocalFile () ? m_source->url ().path () : m_source->url ().url ());
    }
    cmd += QChar (' ') + arguments;
    cmd += QChar (' ') + KProcess::quote (outurl);
    printf ("%s\n", cmd.ascii ());
    *m_process << cmd;
    m_process->start (KProcess::NotifyOnExit, KProcess::Stdin);
    if (m_process->isRunning ())
        QTimer::singleShot (0, this, SLOT (emitStarted ()));
    return m_process->isRunning ();
}

bool FFMpeg::stop () {
    kdDebug () << "FFMpeg::stop" << endl;
    if (!playing ()) return true;
    m_process->writeStdin ("q", 1);
    QTime t;
    t.start ();
    do {
        KProcessController::theKProcessController->waitForProcessExit (2);
    } while (t.elapsed () < 2000 && m_process->isRunning ());
    if (!playing ()) return true;
    return KMPlayerProcess::stop ();
}

void FFMpeg::processStopped (KProcess *) {
    QTimer::singleShot (0, this, SLOT (emitFinished ()));
}

#include "kmplayerprocess.moc"
