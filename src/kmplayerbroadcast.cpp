/* this file is part of the kmplayer application
   copyright (c) 2003 koos vriezen <koos.vriezen@xs4all.nl>

   this program is free software; you can redistribute it and/or
   modify it under the terms of the gnu general public
   license as published by the free software foundation; either
   version 2 of the license, or (at your option) any later version.

   this program is distributed in the hope that it will be useful,
   but without any warranty; without even the implied warranty of
   merchantability or fitness for a particular purpose.  see the gnu
    general public license for more details.

   you should have received a copy of the gnu general public license
   along with this program; see the file copying.  if not, write to
   the free software foundation, inc., 59 temple place - suite 330,
   boston, ma 02111-1307, usa.
*/

#include <qlayout.h>
#include <qlabel.h>
#include <qpushbutton.h>
#include <qtable.h>
#include <qstringlist.h>
#include <qcombobox.h>
#include <qlistbox.h>
#include <qlineedit.h>
#include <qtooltip.h>
#include <qtabwidget.h>
#include <qcursor.h>
#include <qdir.h>
#include <qfile.h>
#include <qtimer.h>

#include <klocale.h>
#include <kdebug.h>
#include <kled.h>
#include <kconfig.h>
#include <kprocess.h>
#include <kprocctrl.h>
#include <kmessagebox.h>

#include "kmplayerbroadcast.h"
#include "kmplayerprocess.h"
#include "kmplayerview.h"
#include "kmplayerpartbase.h"

static const char * strBroadcast = "Broadcast";
static const char * strBindAddress = "Bind Address";
static const char * strFFServerPort = "FFServer Port";
static const char * strMaxClients = "Maximum Connections";
static const char * strMaxBandwidth = "Maximum Bandwidth";
static const char * strFeedFile = "Feed File";
static const char * strFeedFileSize = "Feed File Size";
//static const char * strFFServerSetting = "FFServer Setting";
static const char * strFFServerCustomSetting = "Custom Setting";
static const char * strFFServerProfiles = "Profiles";


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
    if (sl.count () < 11) {
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
    acl.clear ();
    for (; it != sl.end (); ++it)
        acl.push_back (*it);
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
    QStringList::const_iterator it = acl.begin ();
    for (; it != acl.end (); ++it)
        sl.push_back (*it);
    return sl;
}

//-----------------------------------------------------------------------------

KMPlayerPrefBroadcastPage::KMPlayerPrefBroadcastPage (QWidget *parent) : QFrame (parent) {
    QVBoxLayout *layout = new QVBoxLayout (this, 5);
    QGridLayout *gridlayout = new QGridLayout (layout, 6, 2, 2);
    QLabel *label = new QLabel (i18n ("Bind address:"), this);
    bindaddress = new QLineEdit ("", this);
    QToolTip::add (bindaddress, i18n ("If you have multiple network devices, you can limit access"));
    gridlayout->addWidget (label, 0, 0);
    gridlayout->addWidget (bindaddress, 0, 1);
    label = new QLabel (i18n ("Listen port:"), this);
    port = new QLineEdit ("", this);
    gridlayout->addWidget (label, 1, 0);
    gridlayout->addWidget (port, 1, 1);
    label = new QLabel (i18n ("Maximum connections:"), this);
    maxclients = new QLineEdit ("", this);
    gridlayout->addWidget (label, 2, 0);
    gridlayout->addWidget (maxclients, 2, 1);
    label = new QLabel (i18n ("Maximum bandwidth (kbit):"), this);
    maxbandwidth = new QLineEdit ("", this);
    gridlayout->addWidget (label, 3, 0);
    gridlayout->addWidget (maxbandwidth, 3, 1);
    label = new QLabel (i18n ("Temporary feed file:"), this);
    feedfile = new QLineEdit ("", this);
    gridlayout->addWidget (label, 4, 0);
    gridlayout->addWidget (feedfile, 4, 1);
    label = new QLabel (i18n ("Feed file size (kB):"), this);
    feedfilesize = new QLineEdit ("", this);
    gridlayout->addWidget (label, 5, 0);
    gridlayout->addWidget (feedfilesize, 5, 1);
    layout->addItem (new QSpacerItem (0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding));
}

//-----------------------------------------------------------------------------

#undef ADDPROPERTY
#define ADDPROPERTY(label,qedit,gridlayout,row,parent)                  \
    qedit = new QLineEdit ("", parent);                                 \
    gridlayout->addWidget (new QLabel (qedit, label, parent), row, 0);  \
    gridlayout->addWidget (qedit, row, 1);

KMPlayerPrefBroadcastFormatPage::KMPlayerPrefBroadcastFormatPage (QWidget *parent, FFServerSettingList & ffs) : QFrame (parent, "BroadcastPage"), profiles (ffs)
{
    QHBoxLayout *layout = new QHBoxLayout (this, 5);
    QGridLayout *formatlayout = new QGridLayout (11, 2, 2);
    formatlayout->setAlignment (Qt::AlignTop);
    QVBoxLayout *leftlayout = new QVBoxLayout (15);
    QHBoxLayout *ledlayout = new QHBoxLayout (5);
    format = new QComboBox (this);
    QLabel * label = new QLabel (format, i18n ("Format:"), this);
    format->clear ();
    format->insertItem (QString ("asf"));
    format->insertItem (QString ("avi"));
    format->insertItem (QString ("mpjpeg"));
    format->insertItem (QString ("mpeg"));
    format->insertItem (QString ("rm"));
    format->insertItem (QString ("swf"));
    QToolTip::add (format, i18n ("Only avi, mpeg and rm work for mplayer playback"));
    formatlayout->addWidget (label, 0, 0);
    formatlayout->addWidget (format, 0, 1);
    ADDPROPERTY (i18n ("Audio codec:"), audiocodec, formatlayout, 1, this);
    ADDPROPERTY (i18n ("Audio bit rate (kbit):"), audiobitrate, formatlayout, 2, this);
    ADDPROPERTY (i18n ("Audio sample rate (Hz):"), audiosamplerate, formatlayout, 3, this);
    ADDPROPERTY (i18n ("Video codec:"), videocodec, formatlayout, 4, this);
    ADDPROPERTY (i18n ("Video bit rate (kbit):"), videobitrate, formatlayout, 5, this);
    ADDPROPERTY (i18n ("Quality (1-31):"), quality, formatlayout, 6, this);
    ADDPROPERTY (i18n ("Frame rate (Hz):"), framerate, formatlayout, 7, this);
    ADDPROPERTY (i18n ("Gop size:"), gopsize, formatlayout, 8, this);
    ADDPROPERTY (i18n ("Width (pixels):"), moviewidth, formatlayout, 9, this);
    ADDPROPERTY (i18n ("Height (pixels):"), movieheight, formatlayout, 10, this);
    label = new QLabel (i18n ("Allow access from:"), this);
    accesslist = new QTable (40, 1, this);
    accesslist->verticalHeader ()->hide ();
    accesslist->setLeftMargin (0);
    accesslist->setColumnWidth (0, 250);
    QToolTip::add (accesslist, i18n ("'Single IP' or 'start-IP end-IP' for IP ranges"));
    QHeader *header = accesslist->horizontalHeader ();
    header->setLabel (0, i18n ("Host/IP or IP Range"));
    QFrame *profileframe = new QFrame (this);
    QGridLayout *profileslayout = new QGridLayout (profileframe, 5, 2, 2);
    profile = new QLineEdit ("", profileframe);
    connect (profile, SIGNAL(textChanged (const QString &)),
             this, SLOT (slotTextChanged (const QString &)));
    profilelist = new QListBox (profileframe);
    for (int i = 0; i < (int) profiles.size (); i++)
        profilelist->insertItem (profiles[i]->name, i);
    connect (profilelist, SIGNAL (selected (int)),
             this, SLOT (slotIndexChanged (int)));
    connect (profilelist, SIGNAL (highlighted (int)),
             this, SLOT (slotItemHighlighted (int)));
    load = new QPushButton (i18n ("Load"), profileframe);
    save = new QPushButton (i18n ("Save"), profileframe);
    del = new QPushButton (i18n ("Delete"), profileframe);
    load->setEnabled (false);
    save->setEnabled (false);
    del->setEnabled (false);
    connect (load, SIGNAL (clicked ()), this, SLOT (slotLoad ()));
    connect (save, SIGNAL (clicked ()), this, SLOT (slotSave ()));
    connect (del, SIGNAL (clicked ()), this, SLOT (slotDelete ()));
    profileslayout->addWidget (profile, 0, 0);
#if (QT_VERSION < 0x030200)
    profileslayout->addRowSpacing (4, 60);
#else
    profileslayout->setRowSpacing (4, 60);
#endif
    profileslayout->addMultiCellWidget (profilelist, 1, 4, 0, 0);
    profileslayout->addWidget (load, 1, 1);
    profileslayout->addWidget (save, 2, 1);
    profileslayout->addWidget (del, 3, 1);
    leftlayout->addWidget (profileframe);
    startbutton = new QPushButton (i18n ("Start"), this);
    serverled = new KLed (Qt::green, KLed::Off, KLed::Raised, KLed::Circular, this);
    feedled = new KLed (Qt::green, KLed::Off, KLed::Raised, KLed::Circular, this);
    ledlayout->addWidget (startbutton);
    ledlayout->addItem (new QSpacerItem (0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum));
    ledlayout->addWidget (serverled);
    ledlayout->addWidget (feedled);
    leftlayout->addLayout (ledlayout);
    QFrame * line = new QFrame (this);
    line->setFrameShape (QFrame::HLine);
    leftlayout->addWidget (line);
    leftlayout->addWidget (label);
    leftlayout->addWidget (accesslist);
    leftlayout->addItem (new QSpacerItem (0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding));
    layout->addLayout (leftlayout);
    line = new QFrame (this);
    line->setFrameShape (QFrame::VLine);
    layout->addWidget (line);
    layout->addLayout (formatlayout);
    layout->addItem (new QSpacerItem (0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding));
}

#undef ADDPROPERTY

void KMPlayerPrefBroadcastFormatPage::setSettings (const FFServerSetting & fs) {
    if (!fs.format.isEmpty ())
        format->setCurrentText (fs.format);
    audiocodec->setText (fs.audiocodec);
    audiobitrate->setText (fs.audiobitrate);
    audiosamplerate->setText (fs.audiosamplerate);
    videocodec->setText (fs.videocodec);
    videobitrate->setText (fs.videobitrate);
    quality->setText (fs.quality);
    framerate->setText (fs.framerate);
    gopsize->setText (fs.gopsize);
    moviewidth->setText (fs.width);
    movieheight->setText (fs.height);
    accesslist->setNumRows (0);
    accesslist->setNumRows (50);
    QStringList::const_iterator it = fs.acl.begin ();
    for (int i = 0; it != fs.acl.end (); ++i, ++it)
        accesslist->setItem (i, 0, new QTableItem (accesslist, QTableItem::Always, *it));
}

void KMPlayerPrefBroadcastFormatPage::getSettings (FFServerSetting & fs) {
    fs.format = format->currentText ();
    fs.audiocodec = audiocodec->text ();
    fs.audiobitrate = audiobitrate->text ();
    fs.audiosamplerate = audiosamplerate->text ();
    fs.videocodec = videocodec->text ();
    fs.videobitrate = videobitrate->text ();
    fs.quality = quality->text ();
    fs.framerate = framerate->text ();
    fs.gopsize = gopsize->text ();
    fs.width = moviewidth->text ();
    fs.height = movieheight->text ();
    fs.acl.clear ();
    for (int i = 0; i < accesslist->numRows (); ++i) {
        if (accesslist->item (i, 0) && !accesslist->item (i, 0)->text ().isEmpty ())
            fs.acl.push_back (accesslist->item (i, 0)->text ());
    }
}

void KMPlayerPrefBroadcastFormatPage::slotIndexChanged (int index) {
    slotItemHighlighted (index);
    if (index >= 0 && index < (int) profiles.size ())
        setSettings (*profiles[index]);
}

void KMPlayerPrefBroadcastFormatPage::slotTextChanged (const QString & txt) {
    save->setEnabled (txt.length ());
}

void KMPlayerPrefBroadcastFormatPage::slotItemHighlighted (int index) {
    if (index < 0 || index >= (int) profiles.size ()) {
        load->setEnabled (false);
        del->setEnabled (false);
    } else {
        profile->setText (profiles[profilelist->currentItem ()]->name);
        load->setEnabled (true);
        del->setEnabled (true);
        slotTextChanged (profilelist->currentText ());
    }
}

void KMPlayerPrefBroadcastFormatPage::slotSave () {
    for (int i = 0; i < (int) profiles.size (); ++i)
        if (profiles[i]->name == profile->text ()) {
            getSettings (*profiles[i]);
            return;
        }
    FFServerSetting * fs = new FFServerSetting;
    fs->name = profile->text ();
    getSettings (*fs);
    profiles.push_back (fs);
    profilelist->insertItem (fs->name);
}

void KMPlayerPrefBroadcastFormatPage::slotLoad () {
    setSettings (*profiles[profilelist->currentItem ()]);
}

void KMPlayerPrefBroadcastFormatPage::slotDelete () {
    FFServerSettingList::iterator it = profiles.begin();
    for (int i = 0; i < profilelist->currentItem (); i++)
        ++it;
    delete *it;
    profiles.erase (it);
    profilelist->removeItem (profilelist->currentItem ());
    load->setEnabled (false);
    del->setEnabled (false);
}

//-----------------------------------------------------------------------------

static bool stopProcess (KProcess * process, const char * cmd = 0L) {
    if (!process || !process->isRunning ()) return true;
    do {
        if (cmd)
            process->writeStdin (cmd, strlen (cmd));
        KProcessController::theKProcessController->waitForProcessExit (1);
        if (!process->isRunning ())
            break;
        process->kill (SIGINT);
        KProcessController::theKProcessController->waitForProcessExit (3);
        if (!process->isRunning ())
            break;
        process->kill (SIGTERM);
        KProcessController::theKProcessController->waitForProcessExit (1);
        if (!process->isRunning ())
            break;
        process->kill (SIGKILL);
        KProcessController::theKProcessController->waitForProcessExit (1);
        if (process->isRunning ()) {
            return false; // give up
        }
    } while (false);
    return true;
}


KMPlayerBroadcastConfig::KMPlayerBroadcastConfig (KMPlayer * player, KMPlayerFFServerConfig * fsc)
 : m_player (player),
   m_ffserverconfig (fsc),
   m_configpage (0L),
   m_ffmpeg_process (0L),
   m_ffserver_process (0L),
   m_endserver (true) {
}

KMPlayerBroadcastConfig::~KMPlayerBroadcastConfig () {
    stopServer ();
}

void KMPlayerBroadcastConfig::write (KConfig * config) {
    config->setGroup (strBroadcast);
    config->writeEntry (strFFServerCustomSetting, ffserversettings.list (), ';');
    QStringList sl;
    for (int i = 0; i < (int) ffserversettingprofiles.size (); i++) {
        sl.push_back (ffserversettingprofiles[i]->name);
        config->writeEntry (QString ("Profile_") + ffserversettingprofiles[i]->name, ffserversettingprofiles[i]->list(), ';');
    }
    config->writeEntry (strFFServerProfiles, sl, ';');
}

void KMPlayerBroadcastConfig::read (KConfig * config) {
    config->setGroup (strBroadcast);
    ffserversettings = config->readListEntry (strFFServerCustomSetting, ';');
    QStringList profiles = config->readListEntry (strFFServerProfiles, ';');
    QStringList::iterator pr_it = profiles.begin ();
    for (; pr_it != profiles.end (); ++pr_it) {
        QStringList sl = config->readListEntry (QString ("Profile_") + *pr_it, ';');
        if (sl.size () > 10) {
            FFServerSetting * ffs = new FFServerSetting (sl);
            ffs->name = *pr_it;
            ffserversettingprofiles.push_back (ffs);
        }
    }
}

void KMPlayerBroadcastConfig::sync (bool fromUI) {
    if (fromUI) {
        m_configpage->getSettings(ffserversettings);
    } else {
        m_configpage->setSettings (ffserversettings);
        m_configpage->profile->setText (QString::null);
    }
}

void KMPlayerBroadcastConfig::prefLocation (QString & item, QString & icon, QString & tab) {
    item = i18n ("Broadcasting");
    icon = QString ("share");
    tab = i18n ("Profiles");
}

QFrame * KMPlayerBroadcastConfig::prefPage (QWidget * parent) {
    m_configpage = new KMPlayerPrefBroadcastFormatPage (parent, ffserversettingprofiles);
    connect (m_configpage->startbutton, SIGNAL (clicked ()), this, SLOT (startServer ()));
    connect (m_player, SIGNAL (sourceChanged (KMPlayerSource *)),
             this, SLOT (sourceChanged (KMPlayerSource *)));
    m_configpage->startbutton->setEnabled
        (!m_player->process ()->source ()->videoDevice ().isEmpty ());
    return m_configpage;
}

bool KMPlayerBroadcastConfig::broadcasting () const {
    return m_ffserver_process && m_ffserver_process->isRunning ();
}
#include <kglobal.h>
#include <kstandarddirs.h>

static const char ffserverconf[] =
"Port %d\nBindAddress %s\nMaxClients %d\nMaxBandwidth %d\n"
"CustomLog -\nNoDaemon\n"
"<Feed kmplayer.ffm>\nFile %s\nFileMaxSize %dK\nACL allow 127.0.0.1\n</Feed>\n"
"<Stream video.%s>\nFeed kmplayer.ffm\n%s\n%s%s\n</Stream>\n"
"<Stream stat.html>\nFormat status\nACL allow localhost\n</Stream>\n";

void KMPlayerBroadcastConfig::startServer () {
    if (broadcasting ()) {
        stopServer ();
        return;
    }
    m_configpage->setCursor (QCursor (Qt::WaitCursor));
    m_ffserver_process = new KProcess;
    m_ffserver_process->setUseShell (true);
    connect (m_ffserver_process, SIGNAL (processExited (KProcess *)),
             this, SLOT (processStopped (KProcess *)));
    QString conffile = locateLocal ("data", "kmplayer/ffserver.conf");
    const char * noaudio = m_player->process ()->source ()->audioDevice ().isEmpty () ? "NoAudio" : "";
    FFServerSetting & ffs = ffserversettings;
    QString acl;
    QStringList::iterator it = ffs.acl.begin ();
    for (; it != ffs.acl.end (); ++it)
        acl += QString ("ACL allow ") + *it + QString ("\n");
    unlink (m_ffserverconfig->feedfile.ascii ());
    QFile qfile (conffile);
    qfile.open (IO_WriteOnly);
    QString configdata;
    QString buf;
    configdata.sprintf (ffserverconf, m_ffserverconfig->ffserverport, m_ffserverconfig->bindaddress.ascii (), m_ffserverconfig->maxclients, m_ffserverconfig->maxbandwidth, m_ffserverconfig->feedfile.ascii (), m_ffserverconfig->feedfilesize, ffs.format.ascii (), acl.ascii (), ffs.ffconfig (buf).ascii (), noaudio);
    qfile.writeBlock (configdata.ascii (), configdata.length ());
    qfile.close ();
    kdDebug () << configdata << endl;
    kdDebug () << "ffserver -f " << conffile << endl;
    *m_ffserver_process << "ffserver -f " << conffile;
    m_ffserver_out.truncate (0);
    connect (m_ffserver_process,
             SIGNAL (receivedStderr (KProcess *, char *, int)),
             this, SLOT (processOutput (KProcess *, char *, int)));
    m_ffserver_process->start (KProcess::NotifyOnExit, KProcess::Stderr);
    if (m_ffserver_process->isRunning ()) {
        m_configpage->startbutton->setText (i18n ("Stop"));
        m_configpage->serverled->setState (KLed::On);
        emit broadcastStarted ();
    }
    QTimer::singleShot (500, this, SLOT (startFeed ()));
}

void KMPlayerBroadcastConfig::stopServer () {
    m_endserver = true;
    if (m_ffmpeg_process)
        m_ffmpeg_process->stop ();
    if (!stopProcess (m_ffserver_process))
        KMessageBox::error (m_configpage, i18n ("Failed to end ffserver process."), i18n ("Error"));
}

void KMPlayerBroadcastConfig::processOutput (KProcess * p, char * s, int) {
    if (p == m_ffserver_process)
        m_ffserver_out += QString (s);
}

void KMPlayerBroadcastConfig::startFeed () {
    FFServerSetting & ffs = ffserversettings;
    QString ffurl;
    if (!m_ffserver_process || !m_ffserver_process->isRunning ()) {
        KMessageBox::error (m_configpage, i18n ("Failed to start ffserver.\n") + m_ffserver_out, i18n ("Error"));
        goto bail_out;
    }
    disconnect (m_ffserver_process, SIGNAL (receivedStderr (KProcess *, char *, int)),
                this, SLOT (processOutput (KProcess *, char *, int)));
    if (m_ffmpeg_process)
        m_ffmpeg_process->stop ();
    delete m_ffmpeg_process;
    m_ffmpeg_process = new FFMpeg (m_player);
    m_ffmpeg_process->setSource (m_player->process ()->source ());
    connect (m_ffmpeg_process, SIGNAL (finished ()),
             this, SLOT (feedFinished ()));
    ffurl.sprintf ("http://localhost:%d/kmplayer.ffm", m_ffserverconfig->ffserverport);
    m_ffmpeg_process->setURL (ffurl.ascii ());
    if (!m_ffmpeg_process->play ()) {
        KMessageBox::error (m_configpage, i18n ("Failed to start ffmpeg."), i18n ("Error"));
        stopProcess (m_ffserver_process);
        goto bail_out;
    }
    if (m_ffmpeg_process->playing ()) {
        m_ffserver_url.sprintf ("http://localhost:%d/video.%s", m_ffserverconfig->ffserverport, ffs.format.ascii ());
        m_endserver = false;
        m_configpage->feedled->setState (KLed::On);
        m_player->openURL (KURL (m_ffserver_url));
    } else
        stopServer ();
bail_out:
    m_configpage->setCursor (QCursor (Qt::ArrowCursor));
}

void KMPlayerBroadcastConfig::feedFinished () {
    m_configpage->feedled->setState (KLed::Off);
    m_ffmpeg_process->deleteLater ();
    m_ffmpeg_process = 0L;
    kdDebug () << "ffmpeg process stopped " << m_endserver << endl;
    if (m_endserver && !stopProcess (m_ffserver_process)) {
        disconnect (m_ffserver_process,
                SIGNAL (receivedStderr (KProcess *, char *, int)),
                this, SLOT (processOutput (KProcess *, char *, int)));
        KMessageBox::error (m_configpage, i18n ("Failed to end ffserver process."), i18n ("Error"));
        processStopped (0L);
    }
}

void KMPlayerBroadcastConfig::processStopped (KProcess *) {
    kdDebug () << "ffserver process stopped" << endl;
    m_configpage->serverled->setState (KLed::Off);
    m_configpage->startbutton->setText (i18n ("Start"));
    m_configpage->startbutton->setEnabled
        (!m_player->process ()->source ()->videoDevice ().isEmpty ());
    m_ffserver_process->deleteLater ();
    m_ffserver_process = 0L;
    emit broadcastStopped ();
}

void KMPlayerBroadcastConfig::sourceChanged (KMPlayerSource * source) {
    m_configpage->startbutton->setEnabled (broadcasting () || !source->videoDevice ().isEmpty ());
}
//-----------------------------------------------------------------------------

KMPlayerFFServerConfig::KMPlayerFFServerConfig () : m_configpage (0L) {
}

void KMPlayerFFServerConfig::write (KConfig * config) {
    config->setGroup (strBroadcast);
    config->writeEntry (strBindAddress, bindaddress);
    config->writeEntry (strFFServerPort, ffserverport);
    config->writeEntry (strMaxClients, maxclients);
    config->writeEntry (strMaxBandwidth, maxbandwidth);
    config->writePathEntry (strFeedFile, feedfile);
    config->writeEntry (strFeedFileSize, feedfilesize);
}

void KMPlayerFFServerConfig::read (KConfig * config) {
    config->setGroup (strBroadcast);
    bindaddress = config->readEntry (strBindAddress, "0.0.0.0");
    ffserverport = config->readNumEntry (strFFServerPort, 8090);
    maxclients = config->readNumEntry (strMaxClients, 10);
    maxbandwidth = config->readNumEntry (strMaxBandwidth, 1000);
    feedfile = config->readPathEntry (strFeedFile, "/tmp/kmplayer.ffm");
    feedfilesize = config->readNumEntry (strFeedFileSize, 512);
}

void KMPlayerFFServerConfig::sync (bool fromUI) {
    if (fromUI) {
        bindaddress = m_configpage->bindaddress->text ();
        ffserverport = m_configpage->port->text ().toInt ();
        maxclients = m_configpage->maxclients->text ().toInt ();
        maxbandwidth = m_configpage->maxbandwidth->text ().toInt();
        feedfile = m_configpage->feedfile->text ();
        feedfilesize = m_configpage->feedfilesize->text ().toInt();
    } else {
        m_configpage->bindaddress->setText (bindaddress);
        m_configpage->port->setText (QString::number (ffserverport));
        m_configpage->maxclients->setText (QString::number (maxclients));
        m_configpage->maxbandwidth->setText (QString::number (maxbandwidth));
        m_configpage->feedfile->setText (feedfile);
        m_configpage->feedfilesize->setText (QString::number (feedfilesize));
    }
}

void KMPlayerFFServerConfig::prefLocation (QString & item, QString & icon, QString & tab) {
    item = i18n ("Broadcasting");
    icon = QString ("share");
    tab = i18n ("FFServer");
}

QFrame *KMPlayerFFServerConfig:: prefPage (QWidget * parent) {
    if (!m_configpage)
        m_configpage = new KMPlayerPrefBroadcastPage (parent);
    return m_configpage;
}


#include "kmplayerbroadcast.moc"
