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
#include <qlayout.h>
#include <qwhatsthis.h>
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
#include <dcopclient.h>
#include <kpopupmenu.h>
#include <kurlrequester.h>
#include <klineedit.h>
#include <kkeydialog.h>

// application specific includes
#include "kmplayer.h"
#include "kmplayerview.h"
#include "kmplayerpartbase.h"
#include "kmplayerprocess.h"
#include "kmplayerappsource.h"
#include "kmplayertvsource.h"
#include "kmplayerbroadcast.h"
#include "kmplayerconfig.h"

#define ID_STATUS_MSG 1
const int DVDNav_start = 1;
const int DVDNav_previous = 2;
const int DVDNav_next = 3;
const int DVDNav_root = 4;
const int DVDNav_up = 5;

extern const char * strMPlayerGroup;

KDE_NO_CDTOR_EXPORT KMPlayerApp::KMPlayerApp(QWidget* , const char* name)
    : KMainWindow(0, name),
      config (kapp->config ()),
      m_player (new KMPlayer (this, 0L, 0L, 0L, config)),
      m_dvdmenu (new QPopupMenu (this)),
      m_dvdnavmenu (new QPopupMenu (this)),
      m_vcdmenu (new QPopupMenu (this)),
      m_tvmenu (new QPopupMenu (this)),
      m_ffserverconfig (new KMPlayerFFServerConfig),
      m_broadcastconfig (new KMPlayerBroadcastConfig (m_player, m_ffserverconfig))
{
    connect (m_broadcastconfig, SIGNAL (broadcastStarted()), this, SLOT (broadcastStarted()));
    connect (m_broadcastconfig, SIGNAL (broadcastStopped()), this, SLOT (broadcastStopped()));
    initStatusBar();
    m_player->init (actionCollection ());
    m_player->setProcess ("mplayer");
    m_player->setRecorder ("mencoder");
    m_player->sources () ["dvdsource"] = new KMPlayerDVDSource(this, m_dvdmenu);
    m_player->sources () ["dvdnavsource"] = new KMPlayerDVDNavSource (this, m_dvdnavmenu);
    m_player->sources () ["vcdsource"] = new KMPlayerVCDSource(this, m_vcdmenu);
    m_player->sources () ["pipesource"] = new KMPlayerPipeSource (this);
    m_player->sources () ["tvsource"] = new KMPlayerTVSource (this, m_tvmenu);
    initActions();
    initView();

    readOptions();
}

KDE_NO_CDTOR_EXPORT KMPlayerApp::~KMPlayerApp () {
    delete m_broadcastconfig;
    delete m_player;
    if (!m_dcopName.isEmpty ()) {
        QCString replytype;
        QByteArray data, replydata;
        kapp->dcopClient ()->call (m_dcopName, "MainApplication-Interface", "quit()", data, replytype, replydata);
    }
}

KDE_NO_EXPORT void KMPlayerApp::initActions()
{
    fileNewWindow = new KAction(i18n("New &Window"), 0, 0, this, SLOT(slotFileNewWindow()), actionCollection(),"new_window");
    fileOpen = KStdAction::open(this, SLOT(slotFileOpen()), actionCollection(), "open");
    fileOpenRecent = KStdAction::openRecent(this, SLOT(slotFileOpenRecent(const KURL&)), actionCollection());
    fileClose = KStdAction::close(this, SLOT(slotFileClose()), actionCollection());
    fileQuit = KStdAction::quit(this, SLOT(slotFileQuit()), actionCollection());
    
    new KAction (i18n ("V&ideo"), QString ("video"), KShortcut (), m_player, SLOT (showVideoWindow ()), actionCollection (), "view_video");
    new KAction (i18n ("Pla&y List"), QString ("player_playlist"), KShortcut (), m_player, SLOT (showPlayListWindow ()), actionCollection (), "view_playlist");
    new KAction (i18n ("C&onsole"), QString ("konsole"), KShortcut (), m_player, SLOT (showConsoleWindow ()), actionCollection (), "view_console");
    /*KAction *preference =*/ KStdAction::preferences (m_player, SLOT (showConfigDialog ()), actionCollection(), "configure");
    new KAction (i18n ("50%"), 0, 0, this, SLOT (zoom50 ()), actionCollection (), "view_zoom_50");
    new KAction (i18n ("100%"), 0, 0, this, SLOT (zoom100 ()), actionCollection (), "view_zoom_100");
    new KAction (i18n ("150%"), 0, 0, this, SLOT (zoom150 ()), actionCollection (), "view_zoom_150");
    viewKeepRatio = new KToggleAction (i18n ("&Keep Width/Height Ratio"), 0, this, SLOT (keepSizeRatio ()), actionCollection (), "view_keep_ratio");
#if KDE_IS_VERSION(3,1,90)
    /*KAction *fullscreenact =*/ KStdAction::fullScreen( this, SLOT(fullScreen ()), actionCollection (), 0 );
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
    KStdAction::keyBindings( this, SLOT( slotConfigureKeys() ), actionCollection() );

// use the absolute path to your kmplayerui.rc file for testing purpose in createGUI();
    createGUI();
}

KDE_NO_EXPORT void KMPlayerApp::slotConfigureKeys()
{
  KKeyDialog::configure( actionCollection(), this );
}


KDE_NO_EXPORT void KMPlayerApp::initStatusBar()
{
    statusBar()->insertItem(i18n("Ready."), ID_STATUS_MSG);
}

KDE_NO_EXPORT void KMPlayerApp::initView ()
{
    m_view = static_cast <KMPlayerView*> (m_player->view());
    m_view->docArea ()->readDockConfig (config, QString ("Window Layout"));
    setCentralWidget (m_view);
    QPopupMenu * bookmarkmenu = m_view->buttonBar()->bookmarkMenu ();
    m_view->buttonBar()->popupMenu ()->removeItem (KMPlayerControlPanel::menu_bookmark);
    menuBar ()->insertItem (i18n ("&Bookmarks"), bookmarkmenu, -1, 2);
    m_sourcemenu = menuBar ()->findItem (menuBar ()->idAt (0));
    m_sourcemenu->setText (i18n ("S&ource"));
    m_sourcemenu->popup ()->insertItem (KGlobal::iconLoader ()->loadIconSet (QString ("dvd_mount"), KIcon::Small, 0, true), i18n ("&DVD"), m_dvdmenu, -1, 4);
#ifdef HAVE_XINE
    m_dvdnavmenu->insertItem (i18n ("&Start"), this, SLOT (dvdNav ()));
    m_dvdmenu->insertItem (i18n ("&DVD Navigator"), m_dvdnavmenu, -1, 1);
    m_dvdmenu->insertItem (i18n ("&Open DVD"), this, SLOT(openDVD ()), 0,-1, 2);
#else
    m_dvdmenu->insertItem (i18n ("&Open DVD"), this, SLOT(openDVD ()), 0,-1, 1);
#endif
    m_sourcemenu->popup ()->insertItem (KGlobal::iconLoader ()->loadIconSet (QString ("cdrom_mount"), KIcon::Small, 0, true), i18n ("V&CD"), m_vcdmenu, -1, 5);
    m_sourcemenu->popup ()->insertItem (KGlobal::iconLoader ()->loadIconSet (QString ("tv"), KIcon::Small, 0, true), i18n ("&TV"), m_tvmenu, -1, 6);
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
    connect (m_player, SIGNAL (titleChanged (const QString &)), this,
             SLOT (setCaption (const QString &)));
    m_view->buttonBar ()->zoomMenu ()->connectItem (KMPlayerControlPanel::menu_zoom50,
            this, SLOT (zoom50 ()));
    m_view->buttonBar ()->zoomMenu ()->connectItem (KMPlayerControlPanel::menu_zoom100,
            this, SLOT (zoom100 ()));
    m_view->buttonBar ()->zoomMenu ()->connectItem (KMPlayerControlPanel::menu_zoom150,
            this, SLOT (zoom150 ()));
    connect (m_view->buttonBar()->broadcastButton (), SIGNAL (clicked ()),
            this, SLOT (broadcastClicked ()));
    connect (m_view->viewer (), SIGNAL (aspectChanged ()),
            this, SLOT (zoom100 ()));
    connect (m_view, SIGNAL (fullScreenChanged ()),
            this, SLOT (fullScreen ()));
    /*QPopupMenu * viewmenu = new QPopupMenu;
    viewmenu->insertItem (i18n ("Full Screen"), this, SLOT(fullScreen ()),
                          QKeySequence ("CTRL + Key_F"));
    menuBar ()->insertItem (i18n ("&View"), viewmenu, -1, 2);*/
    //toolBar("mainToolBar")->hide();
    setAcceptDrops (true);
}

KDE_NO_EXPORT void KMPlayerApp::loadingProgress (int percentage) {
    if (percentage >= 100)
        slotStatusMsg(i18n("Ready"));
    else
        slotStatusMsg (QString::number (percentage) + "%");
}

KDE_NO_EXPORT void KMPlayerApp::playerStarted () {
    KMPlayerSource * source = m_player->process ()->source ();
    if (source->inherits ("KMPlayerURLSource"))
        recentFiles ()->addURL (source->url ());
}

KDE_NO_EXPORT void KMPlayerApp::slotSourceChanged (KMPlayerSource * source) {
    setCaption (source->prettyName (), false);
}

KDE_NO_EXPORT void KMPlayerApp::dvdNav () {
    slotStatusMsg(i18n("DVD Navigation ..."));
    m_player->setSource (m_player->sources () ["dvdnavsource"]);
    slotStatusMsg(i18n("Ready"));
}

KDE_NO_EXPORT void KMPlayerApp::openDVD () {
    slotStatusMsg(i18n("Opening DVD..."));
    m_player->setSource (m_player->sources () ["dvdsource"]);
}

KDE_NO_EXPORT void KMPlayerApp::openVCD () {
    slotStatusMsg(i18n("Opening VCD..."));
    m_player->setSource (m_player->sources () ["vcdsource"]);
}

KDE_NO_EXPORT void KMPlayerApp::openPipe () {
    slotStatusMsg(i18n("Opening pipe..."));
    bool ok;
    QString cmd = KLineEditDlg::getText (i18n("Read From Pipe"),
      i18n ("Enter command:"), m_player->sources () ["pipesource"]->pipeCmd (), &ok, m_player->view());
    if (!ok) {
        slotStatusMsg (i18n ("Ready."));
        return;
    }
    static_cast <KMPlayerPipeSource *> (m_player->sources () ["pipesource"])->setCommand (cmd);
    m_player->setSource (m_player->sources () ["pipesource"]);
}

KDE_NO_EXPORT void KMPlayerApp::openDocumentFile (const KURL& url)
{
    slotStatusMsg(i18n("Opening file..."));
    m_player->openURL (url);
    if (m_broadcastconfig->broadcasting () && url.url() == m_broadcastconfig->serverURL ()) {
        // speed up replay
        FFServerSetting & ffs = m_broadcastconfig->ffserversettings;
        KMPlayerSource * source = m_player->process ()->source ();
        if (!ffs.width.isEmpty () && !ffs.height.isEmpty ()) {
            source->setWidth (ffs.width.toInt ());
            source->setHeight (ffs.height.toInt ());
        }
        source->setIdentified ();
    }
    slotStatusMsg (i18n ("Ready."));
}

KDE_NO_EXPORT void KMPlayerApp::resizePlayer (int percentage) {
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
        //m_view->viewer()->setAspect (m_view->keepSizeRatio() ? source->aspect() : 0.0);
        if (m_view->controlPanelMode () == KMPlayerView::CP_Show)
            h += 2 + m_view->buttonBar()->frameSize ().height ();
        w = int (1.0 * w * percentage/100.0);
        h = int (1.0 * h * percentage/100.0);
        kdDebug () << "resizePlayer (" << w << "," << h << ")" << endl;
        QSize s = sizeForCentralWidgetSize (QSize (w, h));
        resize (s);
    }
}

KDE_NO_EXPORT void KMPlayerApp::zoom50 () {
    resizePlayer (50);
}

KDE_NO_EXPORT void KMPlayerApp::zoom100 () {
    resizePlayer (100);
}

KDE_NO_EXPORT void KMPlayerApp::zoom150 () {
    resizePlayer (150);
}

KDE_NO_EXPORT void KMPlayerApp::showBroadcastConfig () {
    m_player->settings ()->addPage (m_broadcastconfig);
    m_player->settings ()->addPage (m_ffserverconfig);
}

KDE_NO_EXPORT void KMPlayerApp::hideBroadcastConfig () {
    m_player->settings ()->removePage (m_broadcastconfig);
    m_player->settings ()->removePage (m_ffserverconfig);
}

KDE_NO_EXPORT void KMPlayerApp::broadcastClicked () {
    if (m_broadcastconfig->broadcasting ())
        m_broadcastconfig->stopServer ();
    else {
        m_player->settings ()->show ("BroadcastPage");
        m_view->buttonBar()->broadcastButton ()->toggle ();
    }
}

KDE_NO_EXPORT void KMPlayerApp::broadcastStarted () {
    if (!m_view->buttonBar()->broadcastButton ()->isOn ())
        m_view->buttonBar()->broadcastButton ()->toggle ();
}

KDE_NO_EXPORT void KMPlayerApp::broadcastStopped () {
    if (m_view->buttonBar()->broadcastButton ()->isOn ())
        m_view->buttonBar()->broadcastButton ()->toggle ();
    if (m_player->process ()->source () != m_player->sources () ["tvsource"])
        m_view->buttonBar()->broadcastButton ()->hide ();
    setCursor (QCursor (Qt::ArrowCursor));
}

KDE_NO_EXPORT bool KMPlayerApp::broadcasting () const {
    return m_broadcastconfig->broadcasting ();
}

KDE_NO_EXPORT void KMPlayerApp::saveOptions()
{
    config->setGroup ("General Options");
    config->writeEntry ("Geometry", size());
    config->writeEntry ("Show Toolbar", viewToolBar->isChecked());
    config->writeEntry ("ToolBarPos", (int) toolBar("mainToolBar")->barPos());
    config->writeEntry ("Show Statusbar",viewStatusBar->isChecked());
    config->writeEntry ("Show Menubar",viewMenuBar->isChecked());
    if (!m_player->sources () ["pipesource"]->pipeCmd ().isEmpty ()) {
        config->setGroup ("Pipe Command");
        config->writeEntry ("Command1", m_player->sources () ["pipesource"]->pipeCmd ());
    }
    fileOpenRecent->saveEntries (config,"Recent Files");
    disconnect (m_player->settings (), SIGNAL (configChanged ()),
                this, SLOT (configChanged ()));
    m_player->settings ()->writeConfig ();
    m_view->docArea ()->writeDockConfig (config, QString ("Window Layout"));
}


KDE_NO_EXPORT void KMPlayerApp::readOptions() {

    config->setGroup("General Options");

    QSize size=config->readSizeEntry("Geometry");
    if (!size.isEmpty ())
        resize(size);

    // bar position settings
    KToolBar::BarPosition toolBarPos;
    toolBarPos=(KToolBar::BarPosition) config->readNumEntry("ToolBarPos", KToolBar::Top);
    toolBar("mainToolBar")->setBarPos(toolBarPos);

    // bar status settings
    viewToolBar->setChecked (config->readBoolEntry("Show Toolbar", false));
    slotViewToolBar();

    bool bViewStatusbar = config->readBoolEntry("Show Statusbar", false);
    viewStatusBar->setChecked(bViewStatusbar);
    slotViewStatusBar();

    viewMenuBar->setChecked (config->readBoolEntry("Show Menubar", true));
    slotViewMenuBar();

    config->setGroup ("Pipe Command");
    static_cast <KMPlayerPipeSource *> (m_player->sources () ["pipesource"])->setCommand (config->readEntry ("Command1", ""));

    keepSizeRatio ();
    keepSizeRatio (); // Lazy, I know :)

    // initialize the recent file list
    fileOpenRecent->loadEntries(config,"Recent Files");

    configChanged ();
}

KDE_NO_EXPORT bool KMPlayerApp::queryClose () {
    return true;
}

KDE_NO_EXPORT bool KMPlayerApp::queryExit()
{
    saveOptions();
    return true;
}

KDE_NO_EXPORT void KMPlayerApp::slotFileNewWindow()
{
    slotStatusMsg(i18n("Opening a new application window..."));

    KMPlayerApp *new_window= new KMPlayerApp();
    new_window->show();

    slotStatusMsg(i18n("Ready."));
}

KDE_NO_EXPORT void KMPlayerApp::slotFileOpen()
{
    m_player->settings ()->show ("URLPage");
}

KDE_NO_EXPORT void KMPlayerApp::slotFileOpenRecent(const KURL& url)
{
    slotStatusMsg(i18n("Opening file..."));

    openDocumentFile (url);

}

KDE_NO_EXPORT void KMPlayerApp::slotFileClose()
{
    slotStatusMsg(i18n("Closing file..."));

    m_player->stop ();

    slotStatusMsg(i18n("Ready."));
}

KDE_NO_EXPORT void KMPlayerApp::slotFileQuit()
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

KDE_NO_EXPORT void KMPlayerApp::slotPreferences () {
    m_player->showConfigDialog ();
}

KDE_NO_EXPORT void KMPlayerApp::slotViewToolBar() {
    m_showToolbar = viewToolBar->isChecked();
    if(m_showToolbar)
        toolBar("mainToolBar")->show();
    else
        toolBar("mainToolBar")->hide();
}

KDE_NO_EXPORT void KMPlayerApp::slotViewStatusBar() {
    m_showStatusbar = viewStatusBar->isChecked();
    if(m_showStatusbar)
        statusBar()->show();
    else
        statusBar()->hide();
}

KDE_NO_EXPORT void KMPlayerApp::slotViewMenuBar() {
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

KDE_NO_EXPORT void KMPlayerApp::slotStatusMsg(const QString &text) {
    statusBar()->clear();
    statusBar()->changeItem(text, ID_STATUS_MSG);
}

KDE_NO_EXPORT void KMPlayerApp::fullScreen () {
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

KDE_NO_EXPORT void KMPlayerApp::startArtsControl () {
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

KDE_NO_EXPORT void KMPlayerApp::configChanged () {
    viewKeepRatio->setChecked (m_player->settings ()->sizeratio);
    static_cast <KMPlayerTVSource *> (m_player->sources () ["tvsource"])->buildMenu ();
}

KDE_NO_EXPORT void KMPlayerApp::keepSizeRatio () {
    m_view->setKeepSizeRatio (!m_view->keepSizeRatio ());
    if (m_player->process ()->source () && m_view->keepSizeRatio ())
        m_view->viewer ()->setAspect (m_player->process ()->source ()->aspect ());
    else
        m_view->viewer ()->setAspect (0.0);
    m_player->settings ()->sizeratio = m_view->keepSizeRatio ();
    viewKeepRatio->setChecked (m_view->keepSizeRatio ());
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT KMPlayerMenuSource::KMPlayerMenuSource (const QString & n, KMPlayerApp * a, QPopupMenu * m, const char * src)
    : KMPlayerSource (n, a->player (), src), m_menu (m), m_app (a) {
}

KDE_NO_CDTOR_EXPORT KMPlayerMenuSource::~KMPlayerMenuSource () {
}

KDE_NO_EXPORT void KMPlayerMenuSource::menuItemClicked (QPopupMenu * menu, int id) {
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

KDE_NO_CDTOR_EXPORT KMPlayerPrefSourcePageDVD::KMPlayerPrefSourcePageDVD (QWidget * parent)
 : QFrame(parent) {
    QVBoxLayout *layout = new QVBoxLayout (this, 5, 2);
    autoPlayDVD = new QCheckBox (i18n ("Auto play after opening DVD"), this, 0);
    QWhatsThis::add(autoPlayDVD, i18n ("Start playing DVD right after opening DVD"));
    QLabel *dvdDevicePathLabel = new QLabel (i18n("DVD device:"), this, 0);
    dvddevice = new KURLRequester ("/dev/dvd", this, 0);
    QWhatsThis::add(dvddevice, i18n ("Path to your DVD device, you must have read rights to this device"));
    layout->addWidget (autoPlayDVD);
    layout->addItem (new QSpacerItem (0, 10, QSizePolicy::Minimum, QSizePolicy::Minimum));
    layout->addWidget (dvdDevicePathLabel);
    layout->addWidget (dvddevice);
    layout->addItem (new QSpacerItem (0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding));
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT KMPlayerDVDSource::KMPlayerDVDSource (KMPlayerApp * a, QPopupMenu * m)
    : KMPlayerMenuSource (i18n ("DVD"), a, m, "dvdsource"), m_configpage (0L) {
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
    setURL (KURL ("dvd://"));
    m_player->settings ()->pagelist.push_back (this);
}

KDE_NO_CDTOR_EXPORT KMPlayerDVDSource::~KMPlayerDVDSource () {
}

KDE_NO_EXPORT bool KMPlayerDVDSource::processOutput (const QString & str) {
    if (KMPlayerSource::processOutput (str))
        return true;
    if (m_identified)
        return false;
    //kdDebug () << "scanning " << cstr << endl;
    QRegExp * patterns = static_cast <MPlayer *> (m_player->players () ["mplayer"])->configPage ()->m_patterns;
    QRegExp & langRegExp = patterns[MPlayerPreferencesPage::pat_dvdlang];
    QRegExp & subtitleRegExp = patterns[MPlayerPreferencesPage::pat_dvdsub];
    QRegExp & titleRegExp = patterns[MPlayerPreferencesPage::pat_dvdtitle];
    QRegExp & chapterRegExp = patterns[MPlayerPreferencesPage::pat_dvdchapter];
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

KDE_NO_EXPORT void KMPlayerDVDSource::activate () {
    m_player->setProcess ("mplayer");
    m_start_play = m_auto_play;
    m_current_title = -1;
    buildArguments ();
    if (m_start_play)
        QTimer::singleShot (0, m_player, SLOT (play ()));
}

KDE_NO_EXPORT void KMPlayerDVDSource::setIdentified (bool b) {
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

KDE_NO_EXPORT void KMPlayerDVDSource::deactivate () {
    m_dvdtitlemenu->clear ();
    m_dvdsubtitlemenu->clear ();
    m_dvdchaptermenu->clear ();
    m_dvdlanguagemenu->clear ();
}

KDE_NO_EXPORT void KMPlayerDVDSource::buildArguments () {
    QString url ("dvd://");
    if (m_current_title >= 0)
        url += m_dvdtitlemenu->findItem (m_current_title)->text ();
    setURL (KURL (url));
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

KDE_NO_EXPORT QString KMPlayerDVDSource::filterOptions () {
    KMPlayerSettings * settings = m_player->settings ();
    if (!settings->disableppauto)
        return KMPlayerSource::filterOptions ();
    return QString ("");
}

KDE_NO_EXPORT void KMPlayerDVDSource::titleMenuClicked (int id) {
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

KDE_NO_EXPORT void KMPlayerDVDSource::play () {
    buildArguments ();
    if (m_start_play) {
        m_player->stop ();
        QTimer::singleShot (0, m_player, SLOT (play ()));
    }
}

KDE_NO_EXPORT void KMPlayerDVDSource::subtitleMenuClicked (int id) {
    menuItemClicked (m_dvdsubtitlemenu, id);
    play ();
}

KDE_NO_EXPORT void KMPlayerDVDSource::languageMenuClicked (int id) {
    menuItemClicked (m_dvdlanguagemenu, id);
    play ();
}

KDE_NO_EXPORT void KMPlayerDVDSource::chapterMenuClicked (int id) {
    menuItemClicked (m_dvdchaptermenu, id);
    play ();
}

KDE_NO_EXPORT QString KMPlayerDVDSource::prettyName () {
    return QString (i18n ("DVD"));
}

static const char * strPlayDVD = "Immediately Play DVD";

KDE_NO_EXPORT void KMPlayerDVDSource::write (KConfig * config) {
    config->setGroup (strMPlayerGroup);
    config->writeEntry (strPlayDVD, m_auto_play);
}

KDE_NO_EXPORT void KMPlayerDVDSource::read (KConfig * config) {
    config->setGroup (strMPlayerGroup);
    m_auto_play = config->readBoolEntry (strPlayDVD, true);
}

KDE_NO_EXPORT void KMPlayerDVDSource::sync (bool fromUI) {
    if (fromUI) {
        m_auto_play = m_configpage->autoPlayDVD->isChecked ();
        m_player->settings ()->dvddevice = m_configpage->dvddevice->lineEdit()->text ();
    } else {
        m_configpage->autoPlayDVD->setChecked (m_auto_play);
        m_configpage->dvddevice->lineEdit()->setText (m_player->settings ()->dvddevice);
    }
}

KDE_NO_EXPORT void KMPlayerDVDSource::prefLocation (QString & item, QString & icon, QString & tab) {
    item = i18n ("Source");
    icon = QString ("source");
    tab = i18n ("DVD");
}

KDE_NO_EXPORT QFrame * KMPlayerDVDSource::prefPage (QWidget * parent) {
    m_configpage = new KMPlayerPrefSourcePageDVD (parent);
    return m_configpage;
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT KMPlayerDVDNavSource::KMPlayerDVDNavSource (KMPlayerApp * app, QPopupMenu * m)
    : KMPlayerMenuSource (i18n ("DVDNav"), app, m, "dvdnavsource") {
    m_menu->insertTearOffHandle (-1, 0);
    setURL (KURL ("dvd://"));
}

KDE_NO_CDTOR_EXPORT KMPlayerDVDNavSource::~KMPlayerDVDNavSource () {}

KDE_NO_EXPORT void KMPlayerDVDNavSource::activate () {
    m_player->setProcess ("xine");
    play ();
}

KDE_NO_EXPORT void KMPlayerDVDNavSource::deactivate () {
}

KDE_NO_EXPORT void KMPlayerDVDNavSource::play () {
    if (!m_menu->findItem (DVDNav_previous)) {
        m_menu->insertItem (i18n ("&Previous"), this, SLOT (navMenuClicked (int)), 0, DVDNav_previous);
        m_menu->insertItem (i18n ("&Next"), this, SLOT (navMenuClicked (int)), 0, DVDNav_next);
        m_menu->insertItem (i18n ("&Root"), this, SLOT (navMenuClicked (int)), 0, DVDNav_root);
        m_menu->insertItem (i18n ("&Up"), this, SLOT (navMenuClicked (int)), 0, DVDNav_up);
    }
    QTimer::singleShot (0, m_player->process (), SLOT (play ()));
    connect (m_player, SIGNAL (stopPlaying ()), this, SLOT(finished ()));
}

KDE_NO_EXPORT void KMPlayerDVDNavSource::finished () {
    disconnect (m_player, SIGNAL (stopPlaying ()), this, SLOT(finished ()));
    m_menu->removeItem (DVDNav_previous);
    m_menu->removeItem (DVDNav_next);
    m_menu->removeItem (DVDNav_root);
    m_menu->removeItem (DVDNav_up);
}

KDE_NO_EXPORT void KMPlayerDVDNavSource::navMenuClicked (int id) {
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

KDE_NO_EXPORT QString KMPlayerDVDNavSource::prettyName () {
    return QString (i18n ("DVD"));
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT KMPlayerPrefSourcePageVCD::KMPlayerPrefSourcePageVCD (QWidget * parent)
 : QFrame (parent) {
     QVBoxLayout *layout = new QVBoxLayout (this, 5, 2);
     autoPlayVCD = new QCheckBox (i18n ("Auto play after opening a VCD"), this, 0);
     QWhatsThis::add(autoPlayVCD, i18n ("Start playing VCD right after opening VCD"));
     QLabel *vcdDevicePathLabel = new QLabel (i18n ("VCD (CDROM) device:"), this, 0);
     vcddevice= new KURLRequester ("/dev/cdrom", this, 0);
     QWhatsThis::add(vcddevice, i18n ("Path to your CDROM/DVD device, you must have read rights to this device"));
     layout->addWidget (autoPlayVCD);
     layout->addItem (new QSpacerItem (0, 10, QSizePolicy::Minimum, QSizePolicy::Minimum));
     layout->addWidget (vcdDevicePathLabel);
     layout->addWidget (vcddevice);
     layout->addItem (new QSpacerItem (0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding));
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT KMPlayerVCDSource::KMPlayerVCDSource (KMPlayerApp * a, QPopupMenu * m)
    : KMPlayerMenuSource (i18n ("VCD"), a, m, "vcdsource"), m_configpage (0L) {
    m_menu->insertTearOffHandle ();
    m_vcdtrackmenu = new QPopupMenu (m_app);
    m_vcdtrackmenu->setCheckable (true);
    m_menu->insertItem (i18n ("&Tracks"), m_vcdtrackmenu);
    setURL (KURL ("vcd://"));
    m_player->settings ()->pagelist.push_back (this);
}

KDE_NO_CDTOR_EXPORT KMPlayerVCDSource::~KMPlayerVCDSource () {
}

KDE_NO_EXPORT bool KMPlayerVCDSource::processOutput (const QString & str) {
    if (KMPlayerSource::processOutput (str))
        return true;
    if (m_identified)
        return false;
    //kdDebug () << "scanning " << cstr << endl;
    QRegExp * patterns = static_cast<MPlayer *> (m_player->players () ["mplayer"])->configPage ()->m_patterns;
    QRegExp & trackRegExp = patterns [MPlayerPreferencesPage::pat_vcdtrack];
    if (trackRegExp.search (str) > -1) {
        m_vcdtrackmenu->insertItem (trackRegExp.cap (1), this,
                                    SLOT (trackMenuClicked(int)), 0,
                                    m_vcdtrackmenu->count ());
        kdDebug () << "track " << trackRegExp.cap (1) << endl;
        return true;
    }
    return false;
}

KDE_NO_EXPORT void KMPlayerVCDSource::activate () {
    m_player->stop ();
    init ();
    m_player->enablePlayerMenu (true);
    m_start_play = m_auto_play;
    m_current_title = -1;
    buildArguments ();
    if (m_start_play)
        QTimer::singleShot (0, m_player, SLOT (play ()));
}

KDE_NO_EXPORT void KMPlayerVCDSource::deactivate () {
    m_vcdtrackmenu->clear ();
    m_player->enablePlayerMenu (false);
}

KDE_NO_EXPORT void KMPlayerVCDSource::setIdentified (bool b) {
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

KDE_NO_EXPORT void KMPlayerVCDSource::buildArguments () {
    QString url ("vcd://");
    if (m_current_title >= 0)
        url += m_vcdtrackmenu->findItem (m_current_title)->text ();
    setURL (KURL (url));
    m_options.truncate (0);
    if (m_player->settings ()->vcddevice.length () > 0)
        m_options+=QString(" -cdrom-device ") + m_player->settings()->vcddevice;
    m_recordcmd = m_options;
}

KDE_NO_EXPORT void KMPlayerVCDSource::trackMenuClicked (int id) {
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

KDE_NO_EXPORT QString KMPlayerVCDSource::prettyName () {
    return QString (i18n ("VCD"));
}

static const char * strPlayVCD = "Immediately Play VCD";

KDE_NO_EXPORT void KMPlayerVCDSource::write (KConfig * config) {
    config->setGroup (strMPlayerGroup);
    config->writeEntry (strPlayVCD, m_auto_play);
}

KDE_NO_EXPORT void KMPlayerVCDSource::read (KConfig * config) {
    config->setGroup (strMPlayerGroup);
    m_auto_play = config->readBoolEntry (strPlayVCD, true);
}

KDE_NO_EXPORT void KMPlayerVCDSource::sync (bool fromUI) {
    if (fromUI) {
        m_auto_play = m_configpage->autoPlayVCD->isChecked ();
        m_player->settings ()->vcddevice = m_configpage->vcddevice->lineEdit()->text ();
    } else {
        m_configpage->autoPlayVCD->setChecked (m_auto_play);
        m_configpage->vcddevice->lineEdit()->setText (m_player->settings ()->vcddevice);
    }
}

KDE_NO_EXPORT void KMPlayerVCDSource::prefLocation (QString & item, QString & icon, QString & tab) {
    item = i18n ("Source");
    icon = QString ("source");
    tab = i18n ("VCD");
}

KDE_NO_EXPORT QFrame * KMPlayerVCDSource::prefPage (QWidget * parent) {
    m_configpage = new KMPlayerPrefSourcePageVCD (parent);
    return m_configpage;
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT KMPlayerPipeSource::KMPlayerPipeSource (KMPlayerApp * a)
    : KMPlayerSource (i18n ("Pipe"), a->player (), "pipesource"), m_app (a) {
}

KDE_NO_CDTOR_EXPORT KMPlayerPipeSource::~KMPlayerPipeSource () {
}

KDE_NO_EXPORT bool KMPlayerPipeSource::hasLength () {
    return false;
}

KDE_NO_EXPORT bool KMPlayerPipeSource::isSeekable () {
    return false;
}

KDE_NO_EXPORT void KMPlayerPipeSource::activate () {
    m_player->setProcess ("mplayer");
    m_recordcmd = m_options = QString ("-"); // or m_url?
    m_identified = true;
    QTimer::singleShot (0, m_player, SLOT (play ()));
    m_app->slotStatusMsg (i18n ("Ready."));
}

KDE_NO_EXPORT void KMPlayerPipeSource::deactivate () {
}

KDE_NO_EXPORT QString KMPlayerPipeSource::prettyName () {
    return i18n ("Pipe - %1").arg (m_pipecmd);
}

#include "kmplayer.moc"
#include "kmplayerappsource.moc"
