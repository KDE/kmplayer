/***************************************************************************
                          kmplayerapp.cpp  -  description
                             -------------------
    begin                : Sat Dec  7 16:14:51 CET 2002
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

#undef Always

// include files for QT
#include <qdir.h>
#include <qfile.h>
#include <qdatastream.h>
#include <qregexp.h>
#include <qiodevice.h>
#include <qprinter.h>
#include <qcursor.h>
#include <qpainter.h>
#include <qcheckbox.h>
#include <qmultilineedit.h>
#include <qpushbutton.h>
#include <qkeysequence.h>
#include <qapplication.h>
#include <qslider.h>
#include <qtimer.h>
#include <qmetaobject.h>

// include files for KDE
#include <kdeversion.h>
#include <kiconloader.h>
#include <kmessagebox.h>
#include <kfiledialog.h>
#include <klineeditdlg.h>
#include <kmenubar.h>
#include <kstatusbar.h>
#include <klocale.h>
#include <kconfig.h>
#include <kstdaction.h>
#include <kdebug.h>
#include <kprocess.h>
#include <kprocctrl.h>
#include <dcopclient.h>
#include <kpopupmenu.h>

// application specific includes
#include "kmplayer.h"
#include "kmplayerview.h"
#include "kmplayerpartbase.h"
#include "kmplayerprocess.h"
#include "kmplayerappsource.h"
#include "kmplayerconfig.h"

#define ID_STATUS_MSG 1
const int DVDNav_start = 1;
const int DVDNav_previous = 2;
const int DVDNav_next = 3;
const int DVDNav_root = 4;
const int DVDNav_up = 5;

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

KMPlayerApp::KMPlayerApp(QWidget* , const char* name)
    : KMainWindow(0, name),
      config (kapp->config ()),
      m_player (new KMPlayer (this, 0L, 0L, 0L, config)),
      m_dvdmenu (new QPopupMenu (this)),
      m_dvdnavmenu (new QPopupMenu (this)),
      m_vcdmenu (new QPopupMenu (this)),
      m_tvmenu (new QPopupMenu (this)),
      m_urlsource (new KMPlayerAppURLSource (this)),
      m_dvdsource (new KMPlayerDVDSource (this, m_dvdmenu)),
      m_dvdnavsource (new KMPlayerDVDNavSource (this, m_dvdnavmenu)),
      m_vcdsource (new KMPlayerVCDSource (this, m_vcdmenu)),
      m_pipesource (new KMPlayerPipeSource (this)),
      m_tvsource (new KMPlayerTVSource (this, m_tvmenu)),
      m_ffmpeg_process (0L),
      m_ffserver_process (0L),
      m_endserver (true)
{
    initStatusBar();
    m_player->init (actionCollection ());
    initActions();
    initView();

    readOptions();
}

KMPlayerApp::~KMPlayerApp () {
    m_endserver = false;
    stopProcess (m_ffmpeg_process, "q");
    stopProcess (m_ffserver_process);
    delete m_ffmpeg_process;
    delete m_ffserver_process;
    delete m_player;
    if (!m_dcopName.isEmpty ()) {
        QCString replytype;
        QByteArray data, replydata;
        kapp->dcopClient ()->call (m_dcopName, "MainApplication-Interface", "quit()", data, replytype, replydata);
    }
}

void KMPlayerApp::initActions()
{
    fileNewWindow = new KAction(i18n("New &Window"), 0, 0, this, SLOT(slotFileNewWindow()), actionCollection(),"new_window");
    fileOpen = KStdAction::open(this, SLOT(slotFileOpen()), actionCollection(), "open");
    fileOpenRecent = KStdAction::openRecent(this, SLOT(slotFileOpenRecent(const KURL&)), actionCollection());
    fileClose = KStdAction::close(this, SLOT(slotFileClose()), actionCollection());
    fileQuit = KStdAction::quit(this, SLOT(slotFileQuit()), actionCollection());
    /*KAction *preference =*/ KStdAction::preferences (m_player, SLOT (showConfigDialog ()), actionCollection(), "configure");
    new KAction (i18n ("50%"), 0, 0, this, SLOT (zoom50 ()), actionCollection (), "view_zoom_50");
    new KAction (i18n ("100%"), 0, 0, this, SLOT (zoom100 ()), actionCollection (), "view_zoom_100");
    new KAction (i18n ("150%"), 0, 0, this, SLOT (zoom150 ()), actionCollection (), "view_zoom_150");
    viewKeepRatio = new KToggleAction (i18n ("&Keep Width/Height Ratio"), 0, this, SLOT (keepSizeRatio ()), actionCollection (), "view_keep_ratio");
    viewShowConsoleOutput = new KToggleAction (i18n ("&Show Console Output"), 0, this, SLOT (showConsoleOutput ()), actionCollection (), "view_show_console");
#if KDE_IS_VERSION(3,1,90)
    /*KAction *fullscreenact =*/ KStdAction::fullScreen( this, SLOT(fullScreen ()), actionCollection () );
#else
    /*KAction *fullscreenact =*/ new KAction (i18n("&Full Screen"), 0, 0, this, SLOT(fullScreen ()), actionCollection (), "fullscreen");
#endif
    /*KAction *playact =*/ new KAction (i18n ("P&lay"), 0, 0, m_player, SLOT (play ()), actionCollection (), "play");
    /*KAction *pauseact =*/ new KAction (i18n ("&Pause"), 0, 0, m_player, SLOT (pause ()), actionCollection (), "pause");
    /*KAction *stopact =*/ new KAction (i18n ("&Stop"), 0, 0, m_player, SLOT (stop ()), actionCollection (), "stop");
    /*KAction *artsctrl =*/ new KAction (i18n ("&Arts Control"), 0, 0, this, SLOT (startArtsControl ()), actionCollection (), "view_arts_control");
    viewToolBar = KStdAction::showToolbar(this, SLOT(slotViewToolBar()), actionCollection());
    viewStatusBar = KStdAction::showStatusbar(this, SLOT(slotViewStatusBar()), actionCollection());
    viewMenuBar = KStdAction::showMenubar(this, SLOT(slotViewMenuBar()), actionCollection());
    fileNewWindow->setStatusText(i18n("Opens a new application window"));
    fileOpen->setStatusText(i18n("Opens an existing file"));
    fileOpenRecent->setStatusText(i18n("Opens a recently used file"));
    fileClose->setStatusText(i18n("Closes the actual source"));
    fileQuit->setStatusText(i18n("Quits the application"));
    //viewToolBar->setStatusText(i18n("Enables/disables the toolbar"));
    viewStatusBar->setStatusText(i18n("Enables/disables the statusbar"));
    viewMenuBar->setStatusText(i18n("Enables/disables the menubar"));
    // use the absolute path to your kmplayerui.rc file for testing purpose in createGUI();
    createGUI();
}

void KMPlayerApp::initStatusBar()
{
    statusBar()->insertItem(i18n("Ready."), ID_STATUS_MSG);
}

void KMPlayerApp::initView ()
{
    m_view = static_cast <KMPlayerView*> (m_player->view());
    setCentralWidget (m_view);
    QPopupMenu * bookmarkmenu = m_view->bookmarkMenu ();
    m_view->popupMenu ()->removeItem (KMPlayerView::menu_bookmark);
    menuBar ()->insertItem (i18n ("&Bookmarks"), bookmarkmenu, -1, 2);
    m_sourcemenu = menuBar ()->findItem (menuBar ()->idAt (0));
    m_sourcemenu->setText (i18n ("S&ource"));
    m_sourcemenu->popup ()->insertItem (i18n ("&DVD"), m_dvdmenu, -1, 4);
#ifdef HAVE_XINE
    m_dvdnavmenu->insertItem (i18n ("&Start"), this, SLOT (dvdNav ()));
    m_dvdmenu->insertItem (i18n ("&DVD Navigator"), m_dvdnavmenu, -1, 1);
    m_dvdmenu->insertItem (i18n ("&Open DVD"), this, SLOT(openDVD ()), 0,-1, 2);
#else
    m_dvdmenu->insertItem (i18n ("&Open DVD"), this, SLOT(openDVD ()), 0,-1, 1);
#endif
    m_sourcemenu->popup ()->insertItem (i18n ("V&CD"), m_vcdmenu, -1, 5);
    m_sourcemenu->popup ()->insertItem (i18n ("&TV"), m_tvmenu, -1, 6);
    m_vcdmenu->insertItem (i18n ("&Open VCD"), this, SLOT(openVCD ()), 0,-1, 1);
    m_sourcemenu->popup ()->insertItem (i18n ("&Open Pipe..."), this, SLOT(openPipe ()), 0, -1, 5);
    connect (m_player->settings (), SIGNAL (configChanged ()),
             this, SLOT (configChanged ()));
    connect (m_player, SIGNAL (startPlaying ()),
             this, SLOT (playerStarted ()));
    connect (m_player, SIGNAL (loading (int)),
             this, SLOT (loadingProgress (int)));
    connect (m_player, SIGNAL (sourceChanged (KMPlayerSource *)), this,
             SLOT (slotSourceChanged (KMPlayerSource *)));
    m_view->zoomMenu ()->connectItem (KMPlayerView::menu_zoom50,
            this, SLOT (zoom50 ()));
    m_view->zoomMenu ()->connectItem (KMPlayerView::menu_zoom100,
            this, SLOT (zoom100 ()));
    m_view->zoomMenu ()->connectItem (KMPlayerView::menu_zoom150,
            this, SLOT (zoom150 ()));
    m_view->popupMenu ()->connectItem (KMPlayerView::menu_fullscreen,
            this, SLOT (fullScreen ()));
    connect (m_view->broadcastButton (), SIGNAL (clicked ()),
            this, SLOT (broadcastClicked ()));
    /*QPopupMenu * viewmenu = new QPopupMenu;
    viewmenu->insertItem (i18n ("Full Screen"), this, SLOT(fullScreen ()),
                          QKeySequence ("CTRL + Key_F"));
    menuBar ()->insertItem (i18n ("&View"), viewmenu, -1, 2);*/
    //toolBar("mainToolBar")->hide();

}

void KMPlayerApp::loadingProgress (int percentage) {
    if (percentage >= 100)
        slotStatusMsg(i18n("Ready"));
    else
        slotStatusMsg (QString::number (percentage) + "%");
}

void KMPlayerApp::playerStarted () {
    if (m_player->process ()->source () != m_tvsource && 
        m_player->settings ()->sizeratio)
        resizePlayer (100);
}

void KMPlayerApp::slotSourceChanged (KMPlayerSource * source) {
    setCaption (source->prettyName (), false);
}

void KMPlayerApp::dvdNav () {
    slotStatusMsg(i18n("DVD Navigation ..."));
    m_player->setSource (m_dvdnavsource);
    slotStatusMsg(i18n("Ready"));
}

void KMPlayerApp::openDVD () {
    slotStatusMsg(i18n("Opening DVD..."));
    m_player->setSource (m_dvdsource);
}

void KMPlayerApp::openVCD () {
    slotStatusMsg(i18n("Opening VCD..."));
    m_player->setSource (m_vcdsource);
}

void KMPlayerApp::openPipe () {
    slotStatusMsg(i18n("Opening pipe..."));
    bool ok;
    QString cmd = KLineEditDlg::getText (i18n("Read From Pipe"),
      i18n ("Enter command:"), m_pipesource->pipeCmd (), &ok, m_player->view());
    if (!ok) {
        slotStatusMsg (i18n ("Ready."));
        return;
    }
    m_pipesource->setCommand (cmd);
    m_player->setSource (m_pipesource);
}

void KMPlayerApp::openDocumentFile (const KURL& url)
{
    slotStatusMsg(i18n("Opening file..."));
    m_urlsource->setURL (url);
    m_player->setSource (m_urlsource);
}

void KMPlayerApp::resizePlayer (int percentage) {
    KMPlayerSource * source = m_player->process ()->source ();
    int w = source->width ();
    int h = source->height ();
    if (w <= 0 || h <= 0) {
        m_player->sizes (w, h);
        source->setWidth (w);
        source->setHeight (h);
    }
    kdDebug () << "KMPlayerApp::resizePlayer (" << w << "," << h << ")" << endl;
    if (w > 0 && h > 0) {
        if (source->aspect () > 0.01) {
            w = int (source->aspect () * source->height ());
            w += w % 2;
            source->setWidth (w);
        } else
            source->setAspect (1.0 * w/h);
        m_view->viewer()->setAspect (m_view->keepSizeRatio() ? source->aspect() : 0.0);
        if (m_player->settings ()->showbuttons &&
            !m_player->settings ()->autohidebuttons)
            h += 2 + m_view->buttonBar()->frameSize ().height ();
        if (source->hasLength () && 
            m_player->settings ()->showposslider)
            h += 2 + m_view->positionSlider ()->height ();
        w = int (1.0 * w * percentage/100.0);
        h = int (1.0 * h * percentage/100.0);
        kdDebug () << "resizePlayer (" << w << "," << h << ")" << endl;
        QSize s = sizeForCentralWidgetSize (QSize (w, h));
        resize (s);
    }
}

void KMPlayerApp::zoom50 () {
    resizePlayer (50);
}

void KMPlayerApp::zoom100 () {
    resizePlayer (100);
}

void KMPlayerApp::zoom150 () {
    resizePlayer (150);
}

#include <kglobal.h>
#include <kstandarddirs.h>

bool KMPlayerApp::broadcasting () const {
    return m_ffserver_process && m_ffserver_process->isRunning ();
}

static const char ffserverconf[] =
"Port %d\nBindAddress %s\nMaxClients %d\nMaxBandwidth %d\n"
"CustomLog -\nNoDaemon\n"
"<Feed kmplayer.ffm>\nFile %s\nFileMaxSize %dK\nACL allow 127.0.0.1\n</Feed>\n"
"<Stream video.%s>\nFeed kmplayer.ffm\n%s\n%s%s\n</Stream>\n"
"<Stream stat.html>\nFormat status\nACL allow localhost\n</Stream>\n";

void KMPlayerApp::broadcastClicked () {
    setCursor (QCursor (Qt::WaitCursor));
    m_endserver = false;
    if (!stopProcess (m_ffmpeg_process, "q"))
        KMessageBox::error (this, i18n ("Failed to end ffmpeg process."), i18n ("Error"));
    if (!stopProcess (m_ffserver_process))
        KMessageBox::error (this, i18n ("Failed to end ffserver process."), i18n ("Error"));
    m_endserver = true;
    if (!m_view->broadcastButton ()->isOn ()) {
        setCursor (QCursor (Qt::ArrowCursor));
        return;
    }
    if (m_player->process ()->source ()->ffmpegCommand ().isEmpty ()) {
        m_view->broadcastButton ()->toggle ();
        setCursor (QCursor (Qt::ArrowCursor));
        return;
    }
    if (!m_ffmpeg_process) {
        m_ffmpeg_process = new KProcess;
        m_ffmpeg_process->setUseShell (true);
        connect (m_ffmpeg_process, SIGNAL (processExited (KProcess *)),
                 this, SLOT (processStopped (KProcess *)));
        m_ffserver_process = new KProcess;
        m_ffserver_process->setUseShell (true);
        connect (m_ffserver_process, SIGNAL (processExited (KProcess *)),
                 this, SLOT (processStopped (KProcess *)));
        //connect (m_process, SIGNAL(receivedStdout(KProcess *, char *, int)),
        //        this, SLOT (processOutput (KProcess *, char *, int)));
    } else {
        m_ffserver_process->clearArguments();
    }
    QString conffile = locateLocal ("data", "kmplayer/ffserver.conf");
    KMPlayerTVSource::TVSource * source = 0L;
    if (m_player->process ()->source () == m_tvsource)
        source = m_tvsource->tvsource ();
    const char * noaudio = source && source->audiodevice.isEmpty () ? "NoAudio" : "";
    KMPlayerSettings * conf = m_player->settings ();
    QString acl;
    QStringList::iterator it = conf->ffserveracl.begin ();
    for (; it != conf->ffserveracl.end (); ++it)
        acl += QString ("ACL allow ") + *it + QString ("\n");
    unlink (conf->feedfile.ascii ());
    FFServerSetting & ffs = conf->ffserversettings[conf->ffserversetting];
    QFile qfile (conffile);
    qfile.open (IO_WriteOnly);
    QString configdata;
    QString buf;
    configdata.sprintf (ffserverconf, conf->ffserverport, conf->bindaddress.ascii (), conf->maxclients, conf->maxbandwidth, conf->feedfile.ascii (), conf->feedfilesize, ffs.format.ascii (), acl.ascii (), ffs.ffconfig (buf).ascii (), noaudio);
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
    QTimer::singleShot (500, this, SLOT (startFeed ()));
}

void KMPlayerApp::processOutput (KProcess * p, char * s, int) {
    if (p == m_ffserver_process)
        m_ffserver_out += QString (s);
}

void KMPlayerApp::startFeed () {
    QString ffmpegcmd = m_player->process ()->source ()->ffmpegCommand ();
    KMPlayerSettings * conf = m_player->settings ();
    FFServerSetting & ffs = conf->ffserversettings[conf->ffserversetting];
    do {
        if (!m_ffserver_process->isRunning ()) {
            KMessageBox::error (this, i18n ("Failed to start ffserver.\n") + m_ffserver_out, i18n ("Error"));
            break;
        }
        disconnect (m_ffserver_process,
                    SIGNAL (receivedStderr (KProcess *, char *, int)),
                    this, SLOT (processOutput (KProcess *, char *, int)));
        if (m_ffmpeg_process && m_ffmpeg_process->isRunning ()) {
            m_endserver = false;
            if (!stopProcess (m_ffmpeg_process, "q"))
                KMessageBox::error (this, i18n ("Failed to end ffmpeg process."), i18n ("Error"));
            else
                QTimer::singleShot (100, this, SLOT (startFeed ()));
            m_endserver = true;
            return;
        }
        if (m_player->process ()->source () == m_tvsource) {
            KMPlayerTVSource::TVSource * tvsource = m_tvsource->tvsource ();
            if (tvsource->frequency >= 0) {
                KProcess process;
                process.setUseShell (true);
                process << "v4lctl -c " << tvsource->videodevice << " setnorm " << tvsource->norm.ascii ();
                kdDebug () << "v4lctl -c " << tvsource->videodevice << " setnorm " << tvsource->norm << endl;
                process.start (KProcess::Block);
                process.clearArguments();
                process << "v4lctl -c " << tvsource->videodevice << " setfreq " << QString::number (tvsource->frequency).ascii ();
                kdDebug () << "v4lctl -c " << tvsource->videodevice << " setfreq " << tvsource->frequency << endl;
                process.start (KProcess::Block);
            }
        }
        m_player->stop ();
        QString ffurl;
        m_ffmpeg_process->clearArguments();
        ffurl.sprintf (" http://localhost:%d/kmplayer.ffm", conf->ffserverport);
        kdDebug () << ffmpegcmd << ffurl <<endl;
        *m_ffmpeg_process << ffmpegcmd << ffurl.ascii ();
        m_ffmpeg_process->start (KProcess::NotifyOnExit, KProcess::Stdin);
        if (!m_ffmpeg_process->isRunning ()) {
            KMessageBox::error (this, i18n ("Failed to start ffmpeg."), i18n ("Error"));
            stopProcess (m_ffserver_process);
            break;
        }
    } while (false);
    if (!m_ffmpeg_process->isRunning () && m_view->broadcastButton ()->isOn ())
        m_view->broadcastButton ()->toggle ();
    if (m_ffmpeg_process->isRunning ()) {
        if (!m_view->broadcastButton ()->isOn ())
            m_view->broadcastButton ()->toggle ();
        QString ffurl;
        ffurl.sprintf ("http://localhost:%d/video.%s", conf->ffserverport, ffs.format.ascii ());
        openDocumentFile (KURL (ffurl));
        //QTimer::singleShot (500, this, SLOT (zoom100 ()));
    }
    setCursor (QCursor (Qt::ArrowCursor));
}

void KMPlayerApp::processStopped (KProcess * process) {
    if (process == m_ffmpeg_process) {
        kdDebug () << "ffmpeg process stopped " << m_endserver << endl; 
        if (m_endserver && !stopProcess (m_ffserver_process)) {
            disconnect (m_ffserver_process,
                        SIGNAL (receivedStderr (KProcess *, char *, int)),
                        this, SLOT (processOutput (KProcess *, char *, int)));
            KMessageBox::error (this, i18n ("Failed to end ffserver process."), i18n ("Error"));
        }
    } else {
        kdDebug () << "ffserver process stopped" << endl; 
        if (m_view && m_view->broadcastButton ()->isOn ())
            m_view->broadcastButton ()->toggle ();
    }
    if (!m_ffserver_process->isRunning () && m_player->process ()->source () != m_tvsource)
        m_view->broadcastButton ()->hide ();
}

void KMPlayerApp::saveOptions()
{
    config->setGroup ("General Options");
    config->writeEntry ("Geometry", size());
    config->writeEntry ("Show Toolbar", viewToolBar->isChecked());
    config->writeEntry ("ToolBarPos", (int) toolBar("mainToolBar")->barPos());
    config->writeEntry ("Show Statusbar",viewStatusBar->isChecked());
    config->writeEntry ("Show Menubar",viewMenuBar->isChecked());
    if (!m_pipesource->pipeCmd ().isEmpty ()) {
        config->setGroup ("Pipe Command");
        config->writeEntry ("Command1", m_pipesource->pipeCmd ());
    }
    fileOpenRecent->saveEntries (config,"Recent Files");
    disconnect (m_player->settings (), SIGNAL (configChanged ()),
                this, SLOT (configChanged ()));
    m_player->settings ()->writeConfig ();
}


void KMPlayerApp::readOptions() {

    config->setGroup("General Options");

    QSize size=config->readSizeEntry("Geometry");
    if (!size.isEmpty ())
        resize(size);

    // bar status settings
    bool bViewToolbar = config->readBoolEntry("Show Toolbar", false);
    viewToolBar->setChecked(bViewToolbar);
    slotViewToolBar();

    // bar position settings
    KToolBar::BarPosition toolBarPos;
    toolBarPos=(KToolBar::BarPosition) config->readNumEntry("ToolBarPos", KToolBar::Top);
    toolBar("mainToolBar")->setBarPos(toolBarPos);

    bool bViewStatusbar = config->readBoolEntry("Show Statusbar", false);
    viewStatusBar->setChecked(bViewStatusbar);
    slotViewStatusBar();

    bool bViewMenubar = config->readBoolEntry("Show Menubar", true);
    viewMenuBar->setChecked(bViewMenubar);
    slotViewMenuBar();

    config->setGroup ("Pipe Command");
    m_pipesource->setCommand (config->readEntry ("Command1", ""));

    keepSizeRatio ();
    keepSizeRatio (); // Lazy, I know :)
    showConsoleOutput ();
    showConsoleOutput ();

    // initialize the recent file list
    fileOpenRecent->loadEntries(config,"Recent Files");

    configChanged ();
}

bool KMPlayerApp::queryClose () {
    return true;
}

bool KMPlayerApp::queryExit()
{
    saveOptions();
    return true;
}

void KMPlayerApp::slotFileNewWindow()
{
    slotStatusMsg(i18n("Opening a new application window..."));

    KMPlayerApp *new_window= new KMPlayerApp();
    new_window->show();

    slotStatusMsg(i18n("Ready."));
}

void KMPlayerApp::slotFileOpen()
{
    slotStatusMsg(i18n("Opening file..."));

    KURL url=KFileDialog::getOpenURL(QString::null,
            i18n("*|All Files"), this, i18n("Open File"));
    if(!url.isEmpty())
    {
        openDocumentFile (url);
    }
}

void KMPlayerApp::slotFileOpenRecent(const KURL& url)
{
    slotStatusMsg(i18n("Opening file..."));

    openDocumentFile (url);

}

void KMPlayerApp::slotFileClose()
{
    slotStatusMsg(i18n("Closing file..."));

    m_player->stop ();

    slotStatusMsg(i18n("Ready."));
}

void KMPlayerApp::slotFileQuit()
{
    slotStatusMsg(i18n("Exiting..."));
    saveOptions();

    // whoever implemented this should fix it too, work around ..
    if (memberList->count () > 1)
        deleteLater ();
    else {
        delete this;
        qApp->quit ();
    }
    // close the first window, the list makes the next one the first again.
    // This ensures that queryClose() is called on each window to ask for closing
    /*KMainWindow* w;
    if(memberList)
    {
        for(w=memberList->first(); w!=0; w=memberList->first())
        {
            // only close the window if the closeEvent is accepted. If the user presses Cancel on the saveModified() dialog,
            // the window and the application stay open.
            if(!w->close())
                break;
        }
    }*/
}

void KMPlayerApp::slotPreferences () {
    m_player->showConfigDialog ();
}

void KMPlayerApp::slotViewToolBar() {
    m_showToolbar = viewToolBar->isChecked();
    if(m_showToolbar)
        toolBar("mainToolBar")->show();
    else
        toolBar("mainToolBar")->hide();
}

void KMPlayerApp::slotViewStatusBar() {
    m_showStatusbar = viewStatusBar->isChecked();
    if(m_showStatusbar)
        statusBar()->show();
    else
        statusBar()->hide();
}

void KMPlayerApp::slotViewMenuBar() {
    m_showMenubar = viewMenuBar->isChecked();
    if (m_showMenubar) {
        menuBar()->show();
        slotStatusMsg(i18n("Ready"));
    } else {
        menuBar()->hide();
        slotStatusMsg (i18n ("Show Menubar with %1").arg(viewMenuBar->shortcutText()));
        if (!m_showStatusbar) {
            statusBar()->show();
            QTimer::singleShot (3000, statusBar(), SLOT (hide ()));
        }
    }
}

void KMPlayerApp::slotStatusMsg(const QString &text) {
    statusBar()->clear();
    statusBar()->changeItem(text, ID_STATUS_MSG);
}

void KMPlayerApp::fullScreen () {
    if (sender ()->metaObject ()->inherits ("KAction"))
        m_view->fullScreen();
        
#if KDE_IS_VERSION(3,1,90)
    KToggleAction *fullScreenAction = static_cast<KToggleAction*>(action("fullscreen"));
    if (fullScreenAction)
       fullScreenAction->setChecked(m_view->isFullScreen());
#endif
        
    if (m_view->isFullScreen())
        hide ();
    else
        show ();
}

void KMPlayerApp::startArtsControl () {
    QCString fApp, fObj;
    QByteArray data, replydata;
    QCStringList apps = kapp->dcopClient ()->registeredApplications();
    for( QCStringList::ConstIterator it = apps.begin(); it != apps.end(); ++it)
        if (!strncmp ((*it).data (), "artscontrol", 11)) {
            kapp->dcopClient ()->findObject
                (*it, "artscontrol-mainwindow#1", "raise()", data, fApp, fObj);
            return;
        }
    QStringList args;
    QCString replytype;
    QDataStream stream (data, IO_WriteOnly);
    stream << QString ("aRts Control Tool") << args;
    if (kapp->dcopClient ()->call ("klauncher", "klauncher", "start_service_by_name(QString,QStringList)", data, replytype, replydata)) {
        int result;
        QDataStream replystream (replydata, IO_ReadOnly);
        replystream >> result >> m_dcopName;
    }
}

void KMPlayerApp::configChanged () {
    viewKeepRatio->setChecked (m_player->settings ()->sizeratio);
    viewShowConsoleOutput->setChecked (m_player->settings ()->showconsole);
    m_tvsource->buildMenu ();
}

void KMPlayerApp::keepSizeRatio () {
    m_view->setKeepSizeRatio (!m_view->keepSizeRatio ());
    if (m_player->process ()->source () && m_view->keepSizeRatio ())
        m_view->viewer ()->setAspect (m_player->process ()->source ()->aspect ());
    else
        m_view->viewer ()->setAspect (0.0);
    viewKeepRatio->setChecked (m_view->keepSizeRatio ());
}

void KMPlayerApp::showConsoleOutput () {
    m_view->setShowConsoleOutput (!m_view->showConsoleOutput ());
    viewShowConsoleOutput->setChecked (m_view->showConsoleOutput ());
    if (m_view->showConsoleOutput ()) {
        if (!m_player->playing ())
            m_view->consoleOutput ()->show ();
    } else
        m_view->consoleOutput ()->hide ();
}

//-----------------------------------------------------------------------------

KMPlayerAppURLSource::KMPlayerAppURLSource (KMPlayerApp * a)
    : KMPlayerURLSource (a->player ()), m_app (a) {
}

KMPlayerAppURLSource::~KMPlayerAppURLSource () {
}

void KMPlayerAppURLSource::activate () {
    if (m_app->broadcasting ()) {
        init ();
        KMPlayerSettings * conf = m_player->settings ();
        FFServerSetting & ffs = conf->ffserversettings[conf->ffserversetting];
        if (!ffs.width.isEmpty () && !ffs.height.isEmpty ()) {
            setWidth (ffs.width.toInt ());
            setHeight (ffs.height.toInt ());
        }
        kdDebug () << "KMPlayerAppURLSource::activate()" << endl;
        setIdentified ();
        QTimer::singleShot (0, m_player, SLOT (play ()));
    } else
        KMPlayerURLSource::activate ();
    m_app->slotStatusMsg (i18n ("Ready."));
}

void KMPlayerAppURLSource::setIdentified (bool b) {
    KMPlayerURLSource::setIdentified (b);
    if (b)
        m_app->recentFiles ()->addURL (url ());
}

//-----------------------------------------------------------------------------

KMPlayerMenuSource::KMPlayerMenuSource (KMPlayerApp * a, QPopupMenu * m)
    : KMPlayerSource (a->player ()), m_menu (m), m_app (a) {
}

KMPlayerMenuSource::~KMPlayerMenuSource () {
}

void KMPlayerMenuSource::menuItemClicked (QPopupMenu * menu, int id) {
    int unsetmenuid = -1;
    for (unsigned i = 0; i < menu->count(); i++) {
        int menuid = menu->idAt (i);
        if (menu->isItemChecked (menuid)) {
            menu->setItemChecked (menuid, false);
            unsetmenuid = menuid;
            break;
        }
    }
    if (unsetmenuid != id)
        menu->setItemChecked (id, true);
}

//-----------------------------------------------------------------------------

KMPlayerDVDSource::KMPlayerDVDSource (KMPlayerApp * a, QPopupMenu * m)
    : KMPlayerMenuSource (a, m) {
    m_menu->insertTearOffHandle ();
    m_dvdtitlemenu = new QPopupMenu (m_app);
    m_dvdsubtitlemenu = new QPopupMenu (m_app);
    m_dvdchaptermenu = new QPopupMenu (m_app);
    m_dvdlanguagemenu = new QPopupMenu (m_app);
    m_dvdtitlemenu->setCheckable (true);
    m_dvdsubtitlemenu->setCheckable (true);
    m_dvdchaptermenu->setCheckable (true);
    m_dvdlanguagemenu->setCheckable (true);
    m_menu->insertItem (i18n ("&Titles"), m_dvdtitlemenu);
    m_menu->insertItem (i18n ("&Chapters"), m_dvdchaptermenu);
    m_menu->insertItem (i18n ("Audio &Language"), m_dvdlanguagemenu);
    m_menu->insertItem (i18n ("&SubTitles"), m_dvdsubtitlemenu);
    m_url = KURL ("dvd://");
}

KMPlayerDVDSource::~KMPlayerDVDSource () {
}

bool KMPlayerDVDSource::processOutput (const QString & str) {
    if (KMPlayerSource::processOutput (str))
        return true;
    if (m_identified)
        return false;
    //kdDebug () << "scanning " << cstr << endl;
    if (subtitleRegExp.search (str) > -1) {
        m_dvdsubtitlemenu->insertItem (subtitleRegExp.cap (2), this,
                SLOT (subtitleMenuClicked (int)), 0,
                subtitleRegExp.cap (1).toInt ());
        kdDebug () << "subtitle sid:" << subtitleRegExp.cap (1) <<
            " lang:" << subtitleRegExp.cap (2) << endl;
    } else if (langRegExp.search (str) > -1) {
        m_dvdlanguagemenu->insertItem (langRegExp.cap (1), this,
                SLOT (languageMenuClicked (int)), 0,
                langRegExp.cap (2).toInt ());
        kdDebug () << "lang aid:" << langRegExp.cap (2) <<
            " lang:" << langRegExp.cap (1) << endl;
    } else if (titleRegExp.search (str) > -1) {
        kdDebug () << "title " << titleRegExp.cap (1) << endl;
        unsigned ts = titleRegExp.cap (1).toInt ();
        if ( ts > 100) ts = 100;
        for (unsigned t = 0; t < ts; t++)
            m_dvdtitlemenu->insertItem (QString::number (t + 1), this,
                    SLOT (titleMenuClicked(int)), 0, t);
    } else if (chapterRegExp.search (str) > -1) {
        kdDebug () << "chapter " << chapterRegExp.cap (1) << endl;
        unsigned chs = chapterRegExp.cap (1).toInt ();
        if ( chs > 100) chs = 100;
        for (unsigned c = 0; c < chs; c++)
            m_dvdchaptermenu->insertItem (QString::number (c + 1), this,
                    SLOT (chapterMenuClicked(int)), 0, c);
    } else
        return false;
    return true;
}

void KMPlayerDVDSource::activate () {
    m_player->stop ();
    init ();
    m_player->setProcess (m_player->mplayer ());
    m_start_play = m_player->settings ()->playdvd;
    langRegExp.setPattern (m_player->settings ()->langpattern);
    subtitleRegExp.setPattern (m_player->settings ()->subtitlespattern);
    titleRegExp.setPattern (m_player->settings ()->titlespattern);
    chapterRegExp.setPattern (m_player->settings ()->chapterspattern);
    m_current_title = -1;
    buildArguments ();
    if (m_start_play)
        QTimer::singleShot (0, m_player, SLOT (play ()));
}

void KMPlayerDVDSource::setIdentified (bool b) {
    KMPlayerSource::setIdentified (b);
    m_start_play = true;
    if (m_current_title < 0 || m_current_title >= int (m_dvdtitlemenu->count()))
        m_current_title = 0;
    if (m_dvdtitlemenu->count ())
        m_dvdtitlemenu->setItemChecked (m_current_title, true);
    else
        m_current_title = -1; // hmmm
    if (m_dvdchaptermenu->count ()) m_dvdchaptermenu->setItemChecked (0, true);
    // TODO remember lang/subtitles settings
    if (m_dvdlanguagemenu->count())
        m_dvdlanguagemenu->setItemChecked (m_dvdlanguagemenu->idAt (0), true);
    buildArguments ();
    m_app->slotStatusMsg (i18n ("Ready."));
}

void KMPlayerDVDSource::deactivate () {
    m_dvdtitlemenu->clear ();
    m_dvdsubtitlemenu->clear ();
    m_dvdchaptermenu->clear ();
    m_dvdlanguagemenu->clear ();
}

void KMPlayerDVDSource::buildArguments () {
    QString url ("dvd://");
    if (m_current_title >= 0)
        url += m_dvdtitlemenu->findItem (m_current_title)->text ();
    m_url = KURL (url);
    m_options = QString (m_identified ? "" : "-v ");
    if (m_identified) {
        for (unsigned i = 0; i < m_dvdsubtitlemenu->count (); i++)
            if (m_dvdsubtitlemenu->isItemChecked (m_dvdsubtitlemenu->idAt (i)))
                m_options += "-sid " + QString::number (m_dvdsubtitlemenu->idAt(i));
        for (unsigned i = 0; i < m_dvdchaptermenu->count (); i++)
            if (m_dvdchaptermenu->isItemChecked (i))
                m_options += " -chapter " + m_dvdchaptermenu->findItem (i)->text ();
        for (unsigned i = 0; i < m_dvdlanguagemenu->count (); i++)
            if (m_dvdlanguagemenu->isItemChecked (m_dvdlanguagemenu->idAt (i)))
                m_options += " -aid " + QString::number(m_dvdlanguagemenu->idAt(i));
        if (m_player->settings ()->dvddevice.length () > 0)
            m_options += QString(" -dvd-device ") + m_player->settings()->dvddevice;
    }
    m_recordcmd = m_options + QString (" -vop scale -zoom");
}

QString KMPlayerDVDSource::filterOptions () {
    KMPlayerSettings * settings = m_player->settings ();
    if (!settings->disableppauto)
        return KMPlayerSource::filterOptions ();
    return QString ("");
}

void KMPlayerDVDSource::titleMenuClicked (int id) {
    if (m_current_title != id) {
        m_player->stop ();
        m_current_title = id;
        m_identified = false;
        buildArguments ();
        deactivate (); // clearMenus ?
        if (m_start_play)
            QTimer::singleShot (0, m_player, SLOT (play ()));
    }
}

void KMPlayerDVDSource::play () {
    buildArguments ();
    if (m_start_play) {
        m_player->stop ();
        QTimer::singleShot (0, m_player, SLOT (play ()));
    }
}

void KMPlayerDVDSource::subtitleMenuClicked (int id) {
    menuItemClicked (m_dvdsubtitlemenu, id);
    play ();
}

void KMPlayerDVDSource::languageMenuClicked (int id) {
    menuItemClicked (m_dvdlanguagemenu, id);
    play ();
}

void KMPlayerDVDSource::chapterMenuClicked (int id) {
    menuItemClicked (m_dvdchaptermenu, id);
    play ();
}

QString KMPlayerDVDSource::prettyName () {
    return QString (i18n ("DVD"));
}
//-----------------------------------------------------------------------------

KMPlayerDVDNavSource::KMPlayerDVDNavSource (KMPlayerApp * app, QPopupMenu * m)
    : KMPlayerMenuSource (app, m) {
    m_menu->insertTearOffHandle (-1, 0);
    m_url = KURL ("dvd://");
}

KMPlayerDVDNavSource::~KMPlayerDVDNavSource () {}

void KMPlayerDVDNavSource::activate () {
    m_player->stop ();
    m_player->setProcess (m_player->xine ());
    play ();
}

void KMPlayerDVDNavSource::deactivate () {
}

void KMPlayerDVDNavSource::play () {
    if (!m_menu->findItem (DVDNav_previous)) {
        m_menu->insertItem (i18n ("&Previous"), this, SLOT (navMenuClicked (int)), 0, DVDNav_previous);
        m_menu->insertItem (i18n ("&Next"), this, SLOT (navMenuClicked (int)), 0, DVDNav_next);
        m_menu->insertItem (i18n ("&Root"), this, SLOT (navMenuClicked (int)), 0, DVDNav_root);
        m_menu->insertItem (i18n ("&Up"), this, SLOT (navMenuClicked (int)), 0, DVDNav_up);
    }
    QTimer::singleShot (0, m_player->process (), SLOT (play ()));
    connect (m_player, SIGNAL (stopPlaying ()), this, SLOT(finished ()));
}

void KMPlayerDVDNavSource::finished () {
    disconnect (m_player, SIGNAL (stopPlaying ()), this, SLOT(finished ()));
    m_menu->removeItem (DVDNav_previous);
    m_menu->removeItem (DVDNav_next);
    m_menu->removeItem (DVDNav_root);
    m_menu->removeItem (DVDNav_up);
}

void KMPlayerDVDNavSource::navMenuClicked (int id) {
    switch (id) {
        case DVDNav_start:
            break;
        case DVDNav_previous:
            m_app->view ()->viewer ()->sendKeyEvent ('p');
            break;
        case DVDNav_next:
            m_app->view ()->viewer ()->sendKeyEvent ('n');
            break;
        case DVDNav_root:
            m_app->view ()->viewer ()->sendKeyEvent ('r');
            break;
        case DVDNav_up:
            m_app->view ()->viewer ()->sendKeyEvent ('u');
            break;
    } 
}

QString KMPlayerDVDNavSource::prettyName () {
    return QString (i18n ("DVD"));
}

//-----------------------------------------------------------------------------
                        
KMPlayerVCDSource::KMPlayerVCDSource (KMPlayerApp * a, QPopupMenu * m)
    : KMPlayerMenuSource (a, m) {
    m_menu->insertTearOffHandle ();
    m_vcdtrackmenu = new QPopupMenu (m_app);
    m_vcdtrackmenu->setCheckable (true);
    m_menu->insertItem (i18n ("&Tracks"), m_vcdtrackmenu);
    m_url = KURL ("vcd://");
}

KMPlayerVCDSource::~KMPlayerVCDSource () {
}

bool KMPlayerVCDSource::processOutput (const QString & str) {
    if (KMPlayerSource::processOutput (str))
        return true;
    if (m_identified)
        return false;
    //kdDebug () << "scanning " << cstr << endl;
    if (trackRegExp.search (str) > -1) {
        m_vcdtrackmenu->insertItem (trackRegExp.cap (1), this,
                                    SLOT (trackMenuClicked(int)), 0,
                                    m_vcdtrackmenu->count ());
        kdDebug () << "track " << trackRegExp.cap (1) << endl;
        return true;
    }
    return false;
}

void KMPlayerVCDSource::activate () {
    m_player->stop ();
    init ();
    m_player->setProcess (m_player->mplayer ());
    m_start_play = m_player->settings ()->playvcd;
    trackRegExp.setPattern (m_player->settings ()->trackspattern);
    m_current_title = -1;
    buildArguments ();
    if (m_start_play)
        QTimer::singleShot (0, m_player, SLOT (play ()));
}

void KMPlayerVCDSource::deactivate () {
    m_vcdtrackmenu->clear ();
}

void KMPlayerVCDSource::setIdentified (bool b) {
    KMPlayerSource::setIdentified (b);
    if (m_current_title < 0 || m_current_title >= int (m_vcdtrackmenu->count()))
        m_current_title = 0;
    if (m_vcdtrackmenu->count ())
        m_vcdtrackmenu->setItemChecked (m_current_title, true);
    else
        m_current_title = -1; // hmmm
    buildArguments ();
    m_app->slotStatusMsg (i18n ("Ready."));
}

void KMPlayerVCDSource::buildArguments () {
    QString url ("vcd://");
    if (m_current_title >= 0)
        url += m_vcdtrackmenu->findItem (m_current_title)->text ();
    m_url = KURL (url);
    m_options.truncate (0);
    if (m_player->settings ()->vcddevice.length () > 0)
        m_options+=QString(" -cdrom-device ") + m_player->settings()->vcddevice;
    m_recordcmd = m_options;
}

void KMPlayerVCDSource::trackMenuClicked (int id) {
    menuItemClicked (m_vcdtrackmenu, id);
    if (m_current_title != id) {
        m_player->stop ();
        m_current_title = id;
        m_identified = false;
        buildArguments ();
        m_vcdtrackmenu->clear ();
        if (m_start_play)
            QTimer::singleShot (0, m_player, SLOT (play ()));
    }
}

QString KMPlayerVCDSource::prettyName () {
    return QString (i18n ("VCD"));
}

//-----------------------------------------------------------------------------

KMPlayerPipeSource::KMPlayerPipeSource (KMPlayerApp * a)
    : KMPlayerSource (a->player ()), m_app (a) {
}

KMPlayerPipeSource::~KMPlayerPipeSource () {
}

bool KMPlayerPipeSource::hasLength () {
    return false;
}

bool KMPlayerPipeSource::isSeekable () {
    return false;
}

void KMPlayerPipeSource::activate () {
    m_player->setProcess (m_player->mplayer ());
    init ();
    m_recordcmd = m_options = QString ("-"); // or m_url?
    m_identified = true;
    QTimer::singleShot (0, m_player, SLOT (play ()));
    m_app->slotStatusMsg (i18n ("Ready."));
}

void KMPlayerPipeSource::deactivate () {
}

QString KMPlayerPipeSource::prettyName () {
    return i18n ("Pipe - %1").arg (m_pipecmd);
}

//-----------------------------------------------------------------------------
/*
 * [TV]
 * Devices=/dev/video0;/dev/video1
 * Driver=v4l
 *
 * [/dev/video0]
 * Inputs=0:Television;1:Composite1;2:S-Video;3:Composite3
 * Size=768,576
 * Television=Ned1:216;Ned2:184;Ned3:192
 *
 * [/dev/video1]
 * Inputs=0:Webcam
 * Size=640,480
 */

KMPlayerTVSource::KMPlayerTVSource (KMPlayerApp * a, QPopupMenu * m)
    : KMPlayerMenuSource (a, m) {
    m_tvsource = 0L;
    m_menu->insertTearOffHandle ();
    m_url = KURL ("tv://");
}

KMPlayerTVSource::~KMPlayerTVSource () {
}

void KMPlayerTVSource::activate () {
    init ();
    m_identified = true;
    m_player->setProcess (m_player->mplayer ());
    buildArguments ();
    if (m_player->settings ()->showbroadcastbutton)
        m_app->view()->broadcastButton ()->show ();
}
/* TODO: playback by
 * ffmpeg -vd /dev/video0 -r 25 -s 768x576 -f rawvideo - |mplayer -nocache -ao arts -rawvideo on:w=768:h=576:fps=25 -quiet -
 */
void KMPlayerTVSource::buildArguments () {
    if (!m_tvsource)
        return;
    m_identified = true;
    KMPlayerSettings * config = m_player->settings ();
    m_app->setCaption (QString (i18n ("TV: ")) + m_tvsource->title, false);
    setWidth (m_tvsource->size.width ());
    setHeight (m_tvsource->size.height ());
    m_options.sprintf ("-tv noaudio:driver=%s:%s:width=%d:height=%d -slave -nocache -quiet", config->tvdriver.ascii (), m_tvsource->command.ascii (), width (), height ());
    if (config->mplayerpost090)
        m_recordcmd.sprintf ("-tv %s:driver=%s:%s:width=%d:height=%d", m_tvsource->audiodevice.isEmpty () ? "noaudio" : (QString ("forceaudio:adevice=") + m_tvsource->audiodevice).ascii(), config->tvdriver.ascii (), m_tvsource->command.ascii (), width (), height ());
    else
        m_recordcmd.sprintf ("-tv on:%s:driver=%s:%s:width=%d:height=%d", m_tvsource->audiodevice.isEmpty () ? "noaudio" : (QString ("forceaudio:adevice=") + m_tvsource->audiodevice).ascii(), config->tvdriver.ascii (), m_tvsource->command.ascii (), width (), height ());
    if (!m_app->broadcasting ())
        m_app->resizePlayer (100);
    m_ffmpegCommand = QString (" -vd ") + m_tvsource->videodevice;
    m_ffmpegCommand += m_tvsource->audiodevice.isEmpty () ?
        QString (" -an") : QString (" -ad ") + m_tvsource->audiodevice;
}

void KMPlayerTVSource::deactivate () {
    if (m_app->view () && !m_app->view ()->broadcastButton ()->isOn ())
        m_app->view ()->broadcastButton ()->hide ();
}

void KMPlayerTVSource::buildMenu () {
    KMPlayerSettings * config = m_player->settings ();
    QString currentcommand;
    if (m_tvsource)
        currentcommand = m_tvsource->command;
    CommandMap::iterator it = commands.begin ();
    for ( ; it != commands.end (); ++it)
        delete it.data ();
    commands.clear ();
    m_menu->clear ();
    m_menu->insertTearOffHandle ();
    m_tvsource = 0L;
    int counter = 0;
    TVDeviceList::iterator dit = config->tvdevices.begin ();
    for (; dit != config->tvdevices.end (); ++dit) {
        TVDevice * device = *dit;
        QPopupMenu * devmenu = new QPopupMenu (m_app);
        TVInputList::iterator iit = device->inputs.begin ();
        for (; iit != device->inputs.end (); ++iit) {
            TVInput * input = *iit;
            if (input->channels.size () <= 0) {
                TVSource * source = new TVSource;
                devmenu->insertItem (input->name, this, SLOT (menuClicked (int)), 0, counter);
                source->videodevice = device->device;
                source->audiodevice = device->audiodevice;
                source->noplayback = device->noplayback;
                source->frequency = -1;
                source->command.sprintf ("device=%s:input=%d", device->device.ascii (), input->id);
                if (currentcommand == source->command)
                    m_tvsource = source;
                source->size = device->size;
                source->title = device->name + QString ("-") + input->name;
                commands.insert (counter++, source);
            } else {
                QPopupMenu * inputmenu = new QPopupMenu (m_app);
                inputmenu->insertTearOffHandle ();
                TVChannelList::iterator it = input->channels.begin ();
                for (; it != input->channels.end (); ++it) {
                    TVSource * source = new TVSource;
                    source->videodevice = device->device;
                    source->audiodevice = device->audiodevice;
                    source->noplayback = device->noplayback;
                    source->frequency = (*it)->frequency;
                    source->size = device->size;
                    source->norm = input->norm;
                    inputmenu->insertItem ((*it)->name, this, SLOT(menuClicked (int)), 0, counter);
                    source->command.sprintf ("device=%s:input=%d:freq=%d", device->device.ascii (), input->id, (*it)->frequency);
                    source->title = device->name + QString("-") + (*it)->name;
                    if (currentcommand == source->command)
                        m_tvsource = source;
                    commands.insert (counter++, source);
                }
                devmenu->insertItem (input->name, inputmenu, 0, input->id);
            }
        }
        m_menu->insertItem (device->name, devmenu);
    }
}

void KMPlayerTVSource::menuClicked (int id) {
    CommandMap::iterator it = commands.find (id);
    if (it != commands.end ()) {
        TVSource * prevsource = m_tvsource;
        m_tvsource = it.data ();
        bool playing = prevsource && 
                       (prevsource->videodevice == m_tvsource->videodevice) &&
                       m_player->playing ();
        if (m_player->process ()->source () != this) {
            m_player->setSource (this);
            playing = false;
        }
        buildArguments ();
        if (m_app->broadcasting ())
            QTimer::singleShot (0, m_app, SLOT (startFeed ()));
        else {
            m_player->stop ();
            if (!m_tvsource->noplayback || playing)
                QTimer::singleShot (0, m_player, SLOT (play ()));
        }
    }
}

QString KMPlayerTVSource::filterOptions () {
    if (! m_player->settings ()->disableppauto)
        return KMPlayerSource::filterOptions ();
    return QString ("-vop pp=lb");
}

bool KMPlayerTVSource::hasLength () {
    return false;
}

bool KMPlayerTVSource::isSeekable () {
    return false;
}

QString KMPlayerTVSource::prettyName () {
    QString name (i18n ("TV"));
    if (m_tvsource)
        name += ' ' + m_tvsource->title;
    return name;
}

#include "kmplayer.moc"
#include "kmplayerappsource.moc"
