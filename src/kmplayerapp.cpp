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

// include files for QT
#undef Always
#include <qdir.h>
#include <qdatastream.h>
#include <qregexp.h>
#include <qiodevice.h>
#include <qprinter.h>
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

// application specific includes
#include "kmplayer.h"
#include "kmplayerview.h"
#include "kmplayer_part.h"
#include "kmplayersource.h"
#include "kmplayerconfig.h"

#define ID_STATUS_MSG 1

KMPlayerApp::KMPlayerApp(QWidget* , const char* name)
    : KMainWindow(0, name),
      config (kapp->config ()),
      m_player (new KMPlayer (this, config)),
      m_dvdmenu (new QPopupMenu (this)),
      m_vcdmenu (new QPopupMenu (this)),
      m_urlsource (new KMPlayerAppURLSource (this)),
      m_dvdsource (new KMPlayerDVDSource (this, m_dvdmenu)),
      m_vcdsource (new KMPlayerVCDSource (this, m_vcdmenu)),
      m_pipesource (new KMPlayerPipeSource (this))
{
    initStatusBar();
    initActions();
    initView();

    readOptions();
}

KMPlayerApp::~KMPlayerApp () {
    delete m_player;
    if (!m_dcopName.isEmpty ()) {
        QCString replytype;
        QByteArray data, replydata;
        kapp->dcopClient ()->call (m_dcopName, "MainApplication-Interface", "quit()", data, replytype, replydata);
    }
}

void KMPlayerApp::initActions()
{
    fileNewWindow = new KAction(i18n("New &Window"), 0, 0, this, SLOT(slotFileNewWindow()), actionCollection(),"file_new_window");
    fileOpen = KStdAction::open(this, SLOT(slotFileOpen()), actionCollection());
    fileOpenRecent = KStdAction::openRecent(this, SLOT(slotFileOpenRecent(const KURL&)), actionCollection());
    fileClose = KStdAction::close(this, SLOT(slotFileClose()), actionCollection());
    fileQuit = KStdAction::quit(this, SLOT(slotFileQuit()), actionCollection());
    /*KAction *preference =*/ KStdAction::preferences (m_player, SLOT (showConfigDialog ()), actionCollection());
    new KAction (i18n ("50%"), 0, 0, this, SLOT (zoom50 ()), actionCollection (), "view_zoom_50");
    new KAction (i18n ("100%"), 0, 0, this, SLOT (zoom100 ()), actionCollection (), "view_zoom_100");
    new KAction (i18n ("150%"), 0, 0, this, SLOT (zoom150 ()), actionCollection (), "view_zoom_150");
    viewKeepRatio = new KToggleAction (i18n ("&Keep Width/Height Ratio"), 0, this, SLOT (keepSizeRatio ()), actionCollection (), "view_keep_ratio");
    viewShowConsoleOutput = new KToggleAction (i18n ("&Show Console Output"), 0, this, SLOT (showConsoleOutput ()), actionCollection (), "view_show_console");
    /*KAction *fullscreenact =*/ new KAction (i18n("&Full Screen"), 0, 0, this, SLOT(fullScreen ()), actionCollection (), "view_fullscreen");
    /*KAction *playact =*/ new KAction (i18n ("P&lay"), 0, 0, this, SLOT (play ()), actionCollection (), "view_play");
    /*KAction *pauseact =*/ new KAction (i18n ("&Pause"), 0, 0, m_player, SLOT (pause ()), actionCollection (), "view_pause");
    /*KAction *stopact =*/ new KAction (i18n ("&Stop"), 0, 0, m_player, SLOT (stop ()), actionCollection (), "view_stop");
    /*KAction *artsctrl =*/ new KAction (i18n ("&Arts Control"), 0, 0, this, SLOT (startArtsControl ()), actionCollection (), "view_arts_control");
    //viewToolBar = KStdAction::showToolbar(this, SLOT(slotViewToolBar()), actionCollection());
    viewStatusBar = KStdAction::showStatusbar(this, SLOT(slotViewStatusBar()), actionCollection());
    viewMenuBar = KStdAction::showMenubar(this, SLOT(slotViewMenuBar()), actionCollection());
    fileNewWindow->setStatusText(i18n("Opens a new application window"));
    fileOpen->setStatusText(i18n("Opens an existing document"));
    fileOpenRecent->setStatusText(i18n("Opens a recently used file"));
    fileClose->setStatusText(i18n("Closes the actual document"));
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
    view = static_cast <KMPlayerView*> (m_player->view());
    setCentralWidget (view);
    m_sourcemenu = menuBar ()->findItem (menuBar ()->idAt (0));
    m_sourcemenu->setText (i18n ("S&ource"));
    m_dvdmenuId = m_sourcemenu->popup ()->insertItem (i18n ("&DVD"), m_dvdmenu, -1, 3);
    m_havedvdmenu = true;
    m_dvdmenu->insertItem (i18n ("&Open DVD"), this, SLOT(openDVD ()), 0,-1, 0);
    m_vcdmenuId = m_sourcemenu->popup ()->insertItem (i18n ("V&CD"), m_vcdmenu, -1, 4);
    m_havevcdmenu = true;
    m_vcdmenu->insertItem (i18n ("&Open VCD"), this, SLOT(openVCD ()), 0,-1, 0);
    m_sourcemenu->popup ()->insertItem (i18n ("&Open Pipe..."), this, SLOT(openPipe ()), 0, -1, 5);
    connect (m_player->configDialog (), SIGNAL (configChanged ()),
             this, SLOT (configChanged ()));
    connect (m_player->browserextension (), SIGNAL (loadingProgress (int)),
             this, SLOT (loadingProgress (int)));
    view->zoomMenu ()->connectItem (KMPlayerView::menu_zoom50,
            this, SLOT (zoom50 ()));
    view->zoomMenu ()->connectItem (KMPlayerView::menu_zoom100,
            this, SLOT (zoom100 ()));
    view->zoomMenu ()->connectItem (KMPlayerView::menu_zoom150,
            this, SLOT (zoom150 ()));
    view->popupMenu ()->connectItem (KMPlayerView::menu_fullscreen,
            this, SLOT (fullScreen ()));
    /*QPopupMenu * viewmenu = new QPopupMenu;
    viewmenu->insertItem (i18n ("Full Screen"), this, SLOT(fullScreen ()),
                          QKeySequence ("CTRL + Key_F"));
    menuBar ()->insertItem (i18n ("&View"), viewmenu, -1, 2);*/
    toolBar("mainToolBar")->hide();

}

void KMPlayerApp::loadingProgress (int percentage) {
    if (percentage >= 100)
        slotStatusMsg(i18n("Ready"));
    else
        slotStatusMsg (QString::number (percentage) + "%");
}

void KMPlayerApp::openDVD () {
    setCaption (i18n ("DVD"), false);
    slotStatusMsg(i18n("Opening DVD..."));
    m_player->setSource (m_dvdsource);
}

void KMPlayerApp::openVCD () {
    setCaption (i18n ("VCD"), false);
    slotStatusMsg(i18n("Opening VCD..."));
    m_player->setSource (m_vcdsource);
}

void KMPlayerApp::openPipe () {
    slotStatusMsg(i18n("Opening pipe..."));
    bool ok;
    QString cmd = KLineEditDlg::getText (i18n("Read From Pipe"),
      i18n ("Enter command:"), m_pipesource->command (), &ok, m_player->view());
    if (!ok) {
        slotStatusMsg (i18n ("Ready."));
        return;
    }
    setCaption (i18n ("Pipe - %1").arg (cmd), false);
    m_pipesource->setCommand (cmd);
    m_player->setSource (m_pipesource);
}

void KMPlayerApp::openDocumentFile (const KURL& url)
{
    slotStatusMsg(i18n("Opening file..."));
    m_urlsource->setURL (url);
    m_player->setSource (m_urlsource);
    setCaption (url.fileName (), false);
}

void KMPlayerApp::resizePlayer (int percentage) {
    KMPlayerSource * source = m_player->source ();
    kdDebug () << "KMPlayerApp::resizePlayer " << source << endl;
    int w = source->width ();
    int h = source->height ();
    kdDebug () << "KMPlayerApp::resizePlayer (" << w << "," << h << ")" << endl;
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
        KMPlayerView * kview = static_cast <KMPlayerView*> (m_player->view());
        view->viewer()->setAspect (view->keepSizeRatio() ? source->aspect() : 0.0);
        int h = source->height () + 2 + kview->buttonBar()->frameSize ().height ();
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

void KMPlayerApp::play () {
    m_player->play ();
}

void KMPlayerApp::saveOptions()
{
    config->setGroup ("General Options");
    config->writeEntry ("Geometry", size());
    //config->writeEntry ("Show Toolbar", viewToolBar->isChecked());
    config->writeEntry ("Show Statusbar",viewStatusBar->isChecked());
    config->writeEntry ("Show Menubar",viewMenuBar->isChecked());
    if (!m_pipesource->command ().isEmpty ()) {
        config->setGroup ("Pipe Command");
        config->writeEntry ("Command1", m_pipesource->command ());
    }
    //config->writeEntry ("ToolBarPos", (int) toolBar("mainToolBar")->barPos());
    fileOpenRecent->saveEntries (config,"Recent Files");
    m_player->configDialog ()->writeConfig ();
}


void KMPlayerApp::readOptions() {

    config->setGroup("General Options");

    QSize size=config->readSizeEntry("Geometry");
    if (!size.isEmpty ())
        resize(size);

    // bar status settings
    //bool bViewToolbar = config->readBoolEntry("Show Toolbar", false);
    //viewToolBar->setChecked(bViewToolbar);
    //slotViewToolBar();

    bool bViewStatusbar = config->readBoolEntry("Show Statusbar", false);
    viewStatusBar->setChecked(bViewStatusbar);
    slotViewStatusBar();

    bool bViewMenubar = config->readBoolEntry("Show Menubar", true);
    viewMenuBar->setChecked(bViewMenubar);
    slotViewMenuBar();

    config->setGroup ("Pipe Command");
    m_pipesource->setCommand (config->readEntry ("Command1", ""));

    // bar position settings
    /*KToolBar::BarPosition toolBarPos;
    toolBarPos=(KToolBar::BarPosition) config->readNumEntry("ToolBarPos", KToolBar::Top);
    toolBar("mainToolBar")->setBarPos(toolBarPos);*/

    m_player->configDialog ()->readConfig ();
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

    // however implemented this should fix it too, work around ..
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
    KMPlayerView * kview = static_cast <KMPlayerView*> (m_player->view());
    if (sender ()->metaObject ()->inherits ("KAction"))
        kview->fullScreen();
    if (kview->isFullScreen())
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
    if (m_player->configDialog ()->showdvdmenu && !m_havedvdmenu) {
        m_dvdmenuId = m_sourcemenu->popup ()->insertItem (i18n ("&DVD"), m_dvdmenu, -1, 3);
        m_havedvdmenu = true;
    } else if (!m_player->configDialog ()->showdvdmenu && m_havedvdmenu) {
        menuBar ()->removeItem (m_dvdmenuId);
        m_havedvdmenu = false;
    }
    if (m_player->configDialog ()->showvcdmenu && !m_havevcdmenu) {
        m_vcdmenuId = m_sourcemenu->popup ()->insertItem (i18n ("V&CD"), m_vcdmenu, -1, m_havedvdmenu ? 4 : 3);
        m_havevcdmenu = true;
    } else if (!m_player->configDialog ()->showvcdmenu && m_havevcdmenu) {
        menuBar ()->removeItem (m_vcdmenuId);
        m_havevcdmenu = false;
    }
    viewKeepRatio->setChecked (m_player->configDialog ()->sizeratio);
    viewShowConsoleOutput->setChecked (m_player->configDialog ()->showconsole);
}

void KMPlayerApp::keepSizeRatio () {
    view->setKeepSizeRatio (!view->keepSizeRatio ());
    if (m_player->source () && view->keepSizeRatio ())
        view->viewer ()->setAspect (m_player->source ()->aspect ());
    else
        view->viewer ()->setAspect (0.0);
    viewKeepRatio->setChecked (view->keepSizeRatio ());
}

void KMPlayerApp::showConsoleOutput () {
    view->setShowConsoleOutput (!view->showConsoleOutput ());
    viewShowConsoleOutput->setChecked (view->showConsoleOutput ());
    if (view->showConsoleOutput ()) {
        if (!m_player->playing ())
            view->consoleOutput ()->show ();
    } else
        view->consoleOutput ()->hide ();
}

//-----------------------------------------------------------------------------

KMPlayerAppURLSource::KMPlayerAppURLSource (KMPlayerApp * a)
    : KMPlayerURLSource (a->player ()), app (a) {
}

KMPlayerAppURLSource::~KMPlayerAppURLSource () {
}

bool KMPlayerAppURLSource::processOutput (const QString & str) {
    return KMPlayerSource::processOutput (str);
}

void KMPlayerAppURLSource::activate () {
    init ();
    bool loop = m_player->configDialog ()->loop;
    m_player->configDialog ()->loop = false;
    if (!url ().isEmpty ()) {
        QString args ("-quiet -nocache -identify -frames 0 ");
        QString myurl (url ().isLocalFile () ? url ().path () : m_url.url ());
        args += KProcess::quote (myurl);
        if (m_player->run (args.ascii ()))
            connect (m_player, SIGNAL (finished ()), this, SLOT (finished ()));
        else
            app->slotStatusMsg (i18n ("Ready."));
    }
    m_player->configDialog ()->loop = loop;
}

void KMPlayerAppURLSource::finished () {
    disconnect (m_player, SIGNAL (finished ()), this, SLOT (finished ()));
    m_player->setMovieLength (10 * length ());
    app->resizePlayer (100);
    play ();
    app->recentFiles ()->addURL (url ());
    app->slotStatusMsg (i18n ("Ready."));
}

void KMPlayerAppURLSource::deactivate () {
}

//-----------------------------------------------------------------------------

KMPlayerDiscSource::KMPlayerDiscSource (KMPlayerApp * a, QPopupMenu * m)
    : KMPlayerSource (a->player ()), m_menu (m), app (a) {
}

KMPlayerDiscSource::~KMPlayerDiscSource () {
}

void KMPlayerDiscSource::menuItemClicked (QPopupMenu * menu, int id) {
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
    if (m_player->configDialog ()->playdvd) {
        m_player->stop ();
        play ();
    }
}

//-----------------------------------------------------------------------------

KMPlayerDVDSource::KMPlayerDVDSource (KMPlayerApp * a, QPopupMenu * m)
    : KMPlayerDiscSource (a, m) {
    m_menu->insertTearOffHandle ();
    m_dvdtitlemenu = new QPopupMenu (app);
    m_dvdsubtitlemenu = new QPopupMenu (app);
    m_dvdchaptermenu = new QPopupMenu (app);
    m_dvdlanguagemenu = new QPopupMenu (app);
    m_dvdtitlemenu->setCheckable (true);
    m_dvdsubtitlemenu->setCheckable (true);
    m_dvdchaptermenu->setCheckable (true);
    m_dvdlanguagemenu->setCheckable (true);
    m_menu->insertItem (i18n ("&Titles"), m_dvdtitlemenu);
    m_menu->insertItem (i18n ("&Chapters"), m_dvdchaptermenu);
    m_menu->insertItem (i18n ("Audio &Language"), m_dvdlanguagemenu);
    m_menu->insertItem (i18n ("&SubTitles"), m_dvdsubtitlemenu);
}

KMPlayerDVDSource::~KMPlayerDVDSource () {
}

bool KMPlayerDVDSource::processOutput (const QString & str) {
    if (KMPlayerSource::processOutput (str))
        return true;
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
    init ();
    deactivate (); // clearMenus ?
    QString args ("-v dvd:// -identify -frames 0 -quiet -nocache");
    if (m_player->configDialog ()->dvddevice.length () > 0)
        args += QString(" -dvd-device ") + m_player->configDialog ()->dvddevice;
    langRegExp.setPattern (m_player->configDialog ()->langpattern);
    subtitleRegExp.setPattern (m_player->configDialog ()->subtitlespattern);
    titleRegExp.setPattern (m_player->configDialog ()->titlespattern);
    chapterRegExp.setPattern (m_player->configDialog ()->chapterspattern);
    bool loop = m_player->configDialog ()->loop;
    m_player->configDialog ()->loop = false;
    if (m_player->run (args.ascii()))
        connect (m_player, SIGNAL (finished()), this, SLOT(finished ()));
    else
        app->slotStatusMsg (i18n ("Ready."));
    m_player->configDialog ()->loop = loop;
}

void KMPlayerDVDSource::deactivate () {
    m_dvdtitlemenu->clear ();
    m_dvdsubtitlemenu->clear ();
    m_dvdchaptermenu->clear ();
    m_dvdlanguagemenu->clear ();
}

void KMPlayerDVDSource::finished () {
    disconnect (m_player, SIGNAL (finished()), this, SLOT (finished ()));
    m_player->setMovieLength (10 * length ());
    if (m_dvdtitlemenu->count ()) m_dvdtitlemenu->setItemChecked (0, true);
    if (m_dvdchaptermenu->count ()) m_dvdchaptermenu->setItemChecked (0, true);
    if (m_dvdlanguagemenu->count()) m_dvdlanguagemenu->setItemChecked (m_dvdlanguagemenu->idAt (0), true);
    app->resizePlayer (100);
    if (m_player->configDialog ()->playdvd)
        play ();
    app->slotStatusMsg (i18n ("Ready."));
}

void KMPlayerDVDSource::play () {
    QString args;
    unsigned i;
    args.sprintf ("-slave");
    for (i = 0; i < m_dvdsubtitlemenu->count(); i++)
        if (m_dvdsubtitlemenu->isItemChecked (m_dvdsubtitlemenu->idAt (i)))
            args += " -sid " + QString::number (m_dvdsubtitlemenu->idAt (i));
    for (i = 0; i < m_dvdtitlemenu->count(); i++)
        if (m_dvdtitlemenu->isItemChecked (i)) {
            args += " -dvd " + m_dvdtitlemenu->findItem (i)->text ();
            break;
        }
    if (i == m_dvdtitlemenu->count())
        args += " dvd:// ";
    for (i = 0; i < m_dvdchaptermenu->count(); i++)
        if (m_dvdchaptermenu->isItemChecked (i))
            args += " -chapter " + m_dvdchaptermenu->findItem (i)->text ();
    for (i = 0; i < m_dvdlanguagemenu->count(); i++)
        if (m_dvdlanguagemenu->isItemChecked (m_dvdlanguagemenu->idAt (i)))
            args += " -aid " + QString::number (m_dvdlanguagemenu->idAt (i));
    if (m_player->configDialog ()->dvddevice.length () > 0)
        args += QString(" -dvd-device ") + m_player->configDialog ()->dvddevice;
    m_player->run (args.ascii());
}

void KMPlayerDVDSource::titleMenuClicked (int id) {
    menuItemClicked (m_dvdtitlemenu, id);
}

void KMPlayerDVDSource::subtitleMenuClicked (int id) {
    menuItemClicked (m_dvdsubtitlemenu, id);
}

void KMPlayerDVDSource::languageMenuClicked (int id) {
    menuItemClicked (m_dvdlanguagemenu, id);
}

void KMPlayerDVDSource::chapterMenuClicked (int id) {
    menuItemClicked (m_dvdchaptermenu, id);
}

//-----------------------------------------------------------------------------

KMPlayerVCDSource::KMPlayerVCDSource (KMPlayerApp * a, QPopupMenu * m)
    : KMPlayerDiscSource (a, m) {
    m_menu->insertTearOffHandle ();
    m_vcdtrackmenu = new QPopupMenu (app);
    m_vcdtrackmenu->setCheckable (true);
    m_menu->insertItem (i18n ("&Tracks"), m_vcdtrackmenu);
}

KMPlayerVCDSource::~KMPlayerVCDSource () {
}

bool KMPlayerVCDSource::processOutput (const QString & str) {
    if (KMPlayerSource::processOutput (str))
        return true;
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
    init ();
    deactivate (); // clearMenus ?
    QString args ("-v vcd:// -identify -frames 0 -quiet -nocache");
    if (m_player->configDialog ()->vcddevice.length () > 0)
        args += QString(" -cdrom-device ")+m_player->configDialog ()->vcddevice;
    trackRegExp.setPattern (m_player->configDialog ()->trackspattern);
    bool loop = m_player->configDialog ()->loop;
    m_player->configDialog ()->loop = false;
    if (m_player->run (args.ascii()))
        connect (m_player, SIGNAL (finished()), this, SLOT(finished ()));
    else
        app->slotStatusMsg (i18n ("Ready."));
    m_player->configDialog ()->loop = loop;
}

void KMPlayerVCDSource::deactivate () {
    m_vcdtrackmenu->clear ();
}

void KMPlayerVCDSource::finished () {
    disconnect (m_player, SIGNAL (finished()), this, SLOT (finished ()));
    m_player->setMovieLength (10 * length ());
    if (m_vcdtrackmenu->count()) m_vcdtrackmenu->setItemChecked (0, true);
    app->resizePlayer (100);
    if (m_player->configDialog ()->playdvd)
        play ();
    app->slotStatusMsg (i18n ("Ready."));
}

void KMPlayerVCDSource::play () {
    QString args;
    unsigned i;
    args.sprintf ("-slave");
    for (i = 0; i < m_vcdtrackmenu->count(); i++)
        if (m_vcdtrackmenu->isItemChecked (i)) {
            args += " -vcd " + m_vcdtrackmenu->findItem (i)->text ();
            break;
        }
    if (i == m_vcdtrackmenu->count())
        args += " vcd:// ";
    if (m_player->configDialog ()->vcddevice.length () > 0)
        args += QString(" -cdrom-device") + m_player->configDialog()->vcddevice;
    m_player->run (args.ascii());
}

void KMPlayerVCDSource::trackMenuClicked (int id) {
    menuItemClicked (m_vcdtrackmenu, id);
}

//-----------------------------------------------------------------------------

KMPlayerPipeSource::KMPlayerPipeSource (KMPlayerApp * a)
    : KMPlayerSource (a->player ()), app (a) {
}

KMPlayerPipeSource::~KMPlayerPipeSource () {
}

void KMPlayerPipeSource::activate () {
    init ();
    play ();
    app->slotStatusMsg (i18n ("Ready."));
}

void KMPlayerPipeSource::play () {
    QString args ("-");
    m_player->run (args.ascii(), m_pipe.ascii());
    m_player->setMovieLength (10 * length ());
    app->resizePlayer (100);
}

void KMPlayerPipeSource::deactivate () {
}


#include "kmplayer.moc"
#include "kmplayersource.moc"
