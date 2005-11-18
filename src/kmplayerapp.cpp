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
#include <qfile.h>
#include <qmetaobject.h>

// include files for KDE
#include <kdeversion.h>
#include <kstandarddirs.h>
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
#include <ksystemtray.h>

// application specific includes
#include "kmplayer.h"
#include "kmplayerview.h"
#include "kmplayercontrolpanel.h"
#include "kmplayerpartbase.h"
#include "kmplayerprocess.h"
#include "kmplayerappsource.h"
#include "kmplayertvsource.h"
#include "kmplayerbroadcast.h"
#include "kmplayervdr.h"
#include "kmplayerconfig.h"

static const int DVDNav_start = 1;
static const int DVDNav_previous = 2;
static const int DVDNav_next = 3;
static const int DVDNav_root = 4;
static const int DVDNav_up = 5;

extern const char * strMPlayerGroup;

KDE_NO_CDTOR_EXPORT KMPlayerApp::KMPlayerApp(QWidget* , const char* name)
    : KMainWindow(0, name),
      config (kapp->config ()),
      m_systray (0L),
      m_player (new KMPlayer::PartBase (this, 0L, 0L, 0L, config)),
      m_dvdmenu (new QPopupMenu (this)),
      m_dvdnavmenu (new QPopupMenu (this)),
      m_vcdmenu (new QPopupMenu (this)),
      m_tvmenu (new QPopupMenu (this)),
      m_ffserverconfig (new KMPlayerFFServerConfig),
      m_broadcastconfig (new KMPlayerBroadcastConfig (m_player, m_ffserverconfig)),
      last_time_left (0),
      m_played_intro (false),
      m_played_exit (false),
      m_minimal_mode (false)
{
    connect (m_broadcastconfig, SIGNAL (broadcastStarted()), this, SLOT (broadcastStarted()));
    connect (m_broadcastconfig, SIGNAL (broadcastStopped()), this, SLOT (broadcastStopped()));
    initStatusBar();
    m_player->init (actionCollection ());
    m_player->players () ["xvideo"] = new XVideo(m_player,m_player->settings());
    m_player->setProcess ("mplayer");
    m_player->setRecorder ("mencoder");
    m_player->sources () ["dvdsource"] = new ::KMPlayerDVDSource(this, m_dvdmenu);
    m_player->sources () ["dvdnavsource"] = new KMPlayerDVDNavSource (this, m_dvdnavmenu);
    m_player->sources () ["vcdsource"] = new KMPlayerVCDSource(this, m_vcdmenu);
    m_player->sources () ["pipesource"] = new KMPlayerPipeSource (this);
    m_player->sources () ["tvsource"] = new KMPlayerTVSource (this, m_tvmenu);
    m_player->sources () ["vdrsource"] = new KMPlayerVDRSource (this);
    m_player->setSource (m_player->sources () ["urlsource"]);
    m_view = static_cast <KMPlayer::View*> (m_player->view());
    initActions();
    initView();

    readOptions();
}

KDE_NO_CDTOR_EXPORT KMPlayerApp::~KMPlayerApp () {
    delete m_broadcastconfig;
}


KDE_NO_EXPORT void KMPlayerApp::initActions () {
    KActionCollection * ac = actionCollection ();
    fileNewWindow = new KAction(i18n("New &Window"), 0, 0, this, SLOT(slotFileNewWindow()), ac, "new_window");
    fileOpen = KStdAction::open(this, SLOT(slotFileOpen()), ac, "open");
    fileOpenRecent = KStdAction::openRecent(this, SLOT(slotFileOpenRecent(const KURL&)), ac, "open_recent");
    KStdAction::saveAs (this, SLOT (slotSaveAs ()), ac, "save_as");
    new KAction (i18n ("Clear &History"), 0, 0, this, SLOT (slotClearHistory ()), ac, "clear_history");
    fileClose = KStdAction::close (this, SLOT (slotFileClose ()), ac);
    fileQuit = KStdAction::quit (this, SLOT (slotFileQuit ()), ac);
    new KAction (i18n ("&Open Pipe..."), QString ("pipe"), KShortcut (), this, SLOT(openPipe ()), ac, "source_pipe");
    //KGlobal::iconLoader ()->loadIconSet (QString ("tv"), KIcon::Small, 0,true)
    new KAction (i18n ("&Connect"), QString ("connect_established"), KShortcut (), this, SLOT (openVDR ()), ac, "vdr_connect");
    editVolumeInc = new KAction (i18n ("Increase Volume"), QString ("player_volume"), KShortcut (), m_player, SLOT (increaseVolume ()), ac, "edit_volume_up");
    editVolumeDec = new KAction (i18n ("Decrease Volume"), QString ("player_volume"), KShortcut (), m_player, SLOT(decreaseVolume ()), ac, "edit_volume_down");
    toggleView = new KAction (i18n ("C&onsole"), QString ("konsole"), KShortcut (), m_player->view(), SLOT (toggleVideoConsoleWindow ()), ac, "view_video");
    //new KAction (i18n ("V&ideo"), QString ("video"), KShortcut (), m_view, SLOT (toggleVideoConsoleWindow ()), ac, "view_video");
    new KAction (i18n ("Pla&y List"), QString ("player_playlist"), KShortcut (), m_player, SLOT (showPlayListWindow ()), ac, "view_playlist");
    new KAction (i18n ("Minimal mode"), QString ("empty"), KShortcut (), this, SLOT (slotMinimalMode ()), ac, "view_minimal");
    /*KAction *preference =*/ KStdAction::preferences (m_player, SLOT (showConfigDialog ()), ac, "configure");
    new KAction (i18n ("50%"), 0, 0, this, SLOT (zoom50 ()), ac, "view_zoom_50");
    new KAction (i18n ("100%"), QString ("viewmagfit"), KShortcut (), this, SLOT (zoom100 ()), ac, "view_zoom_100");
    new KAction (i18n ("150%"), 0, 0, this, SLOT (zoom150 ()), ac, "view_zoom_150");
    new KAction (i18n ("Show Popup Menu"), KShortcut (), m_view->controlPanel (), SLOT (showPopupMenu ()), ac, "view_show_popup_menu");
    new KAction (i18n ("Show Language Menu"), KShortcut (Qt::Key_L), m_view->controlPanel (), SLOT (showLanguageMenu ()), ac, "view_show_lang_menu");
    viewKeepRatio = new KToggleAction (i18n ("&Keep Width/Height Ratio"), 0, this, SLOT (keepSizeRatio ()), ac, "view_keep_ratio");
#if KDE_IS_VERSION(3,1,90)
    /*KAction *fullscreenact =*/ KStdAction::fullScreen( this, SLOT(fullScreen ()), ac, 0 );
#else
    /*KAction *fullscreenact =*/ new KAction (i18n("&Full Screen"), 0, 0, this, SLOT(fullScreen ()), ac, "fullscreen");
#endif
    /*KAction *playact =*/ new KAction (i18n ("P&lay"), QString ("player_play"), KShortcut (), m_player, SLOT (play ()), ac, "play");
    /*KAction *pauseact =*/ new KAction (i18n ("&Pause"), QString ("player_pause"), KShortcut (), m_player, SLOT (pause ()), ac, "pause");
    /*KAction *stopact =*/ new KAction (i18n ("&Stop"), QString ("player_stop"), KShortcut (), m_player, SLOT (stop ()), ac, "stop");
    /*KAction *artsctrl =*/ new KAction (i18n ("&Arts Control"), QString ("player_volume"), KShortcut (), this, SLOT (startArtsControl ()), ac, "view_arts_control");
    viewToolBar = KStdAction::showToolbar(this, SLOT(slotViewToolBar()), ac);
    viewStatusBar =KStdAction::showStatusbar(this,SLOT(slotViewStatusBar()),ac);
    viewMenuBar = KStdAction::showMenubar(this, SLOT(slotViewMenuBar()), ac);
    fileNewWindow->setStatusText(i18n("Opens a new application window"));
    fileOpen->setStatusText(i18n("Opens an existing file"));
    fileOpenRecent->setStatusText(i18n("Opens a recently used file"));
    fileClose->setStatusText(i18n("Closes the actual source"));
    fileQuit->setStatusText(i18n("Quits the application"));
    //viewToolBar->setStatusText(i18n("Enables/disables the toolbar"));
    viewStatusBar->setStatusText(i18n("Enables/disables the statusbar"));
    viewMenuBar->setStatusText(i18n("Enables/disables the menubar"));
    KStdAction::keyBindings( this, SLOT( slotConfigureKeys() ), ac);
}

KDE_NO_EXPORT void KMPlayerApp::slotConfigureKeys()
{
  KKeyDialog::configure( actionCollection(), this );
}


KDE_NO_EXPORT void KMPlayerApp::initStatusBar () {
    KStatusBar *sb = statusBar ();
    sb->insertItem (i18n ("Ready."), id_status_msg);
    sb->insertItem (QString ("--:--"), id_status_timer, 0, true);
}

KDE_NO_EXPORT void KMPlayerApp::initMenu () {
    createGUI (); // first build the one from the kmplayerui.rc
    QPopupMenu * bookmarkmenu = m_view->controlPanel()->bookmarkMenu ();
    m_view->controlPanel()->popupMenu ()->removeItem (KMPlayer::ControlPanel::menu_bookmark);
    menuBar ()->insertItem (i18n ("&Bookmarks"), bookmarkmenu, -1, 2);
    m_sourcemenu = menuBar ()->findItem (menuBar ()->idAt (0));
    m_sourcemenu->setText (i18n ("S&ource"));
    m_sourcemenu->popup ()->insertItem (KGlobal::iconLoader ()->loadIconSet (QString ("dvd_mount"), KIcon::Small, 0, true), i18n ("&DVD"), m_dvdmenu, -1, 5);
    //m_dvdmenu->clear ();
#ifdef HAVE_XINE
    m_dvdnavmenu->clear ();
    m_dvdnavmenu->insertItem (i18n ("&Start"), this, SLOT (dvdNav ()));
    m_dvdmenu->insertItem (i18n ("&DVD Navigator"), m_dvdnavmenu, -1, 1);
    m_dvdmenu->insertItem (i18n ("&Open DVD"), this, SLOT(openDVD ()), 0,-1, 2);
#else
    m_dvdmenu->insertItem (i18n ("&Open DVD"), this, SLOT(openDVD ()), 0,-1, 1);
#endif
    m_sourcemenu->popup ()->insertItem (KGlobal::iconLoader ()->loadIconSet (QString ("cdrom_mount"), KIcon::Small, 0, true), i18n ("V&CD"), m_vcdmenu, -1, 6);
    m_vcdmenu->clear ();
    m_sourcemenu->popup ()->insertItem (KGlobal::iconLoader ()->loadIconSet (QString ("tv"), KIcon::Small, 0, true), i18n ("&TV"), m_tvmenu, -1, 7);
    m_vcdmenu->insertItem (i18n ("&Open VCD"), this, SLOT(openVCD ()), 0,-1, 1);
}

KDE_NO_EXPORT void KMPlayerApp::initView () {
    //m_view->docArea ()->readDockConfig (config, QString ("Window Layout"));
    m_player->connectPanel (m_view->controlPanel ());
    setCentralWidget (m_view);
    initMenu ();
    new KAction (i18n ("Increase Volume"), editVolumeInc->shortcut (), m_player, SLOT (increaseVolume ()), m_view->viewArea ()->actionCollection (), "edit_volume_up");
    new KAction (i18n ("Decrease Volume"), editVolumeDec->shortcut (), m_player, SLOT(decreaseVolume ()), m_view->viewArea ()->actionCollection (), "edit_volume_down");
    connect (m_player->settings (), SIGNAL (configChanged ()),
             this, SLOT (configChanged ()));
    connect (m_player, SIGNAL (loading (int)),
             this, SLOT (loadingProgress (int)));
    connect (m_player, SIGNAL (positioned (int, int)),
             this, SLOT (positioned (int, int)));
    connect (m_player, SIGNAL (statusUpdated (const QString &)),
             this, SLOT (slotStatusMsg (const QString &)));
    connect (m_view, SIGNAL (windowVideoConsoleToggled (int)),
             this, SLOT (windowVideoConsoleToggled (int)));
    connect (m_player, SIGNAL (sourceChanged (KMPlayer::Source *, KMPlayer::Source *)), this, SLOT (slotSourceChanged(KMPlayer::Source *, KMPlayer::Source *)));
    m_view->controlPanel ()->zoomMenu ()->connectItem (KMPlayer::ControlPanel::menu_zoom50,
            this, SLOT (zoom50 ()));
    m_view->controlPanel ()->zoomMenu ()->connectItem (KMPlayer::ControlPanel::menu_zoom100,
            this, SLOT (zoom100 ()));
    m_view->controlPanel ()->zoomMenu ()->connectItem (KMPlayer::ControlPanel::menu_zoom150,
            this, SLOT (zoom150 ()));
    connect (m_view->controlPanel()->broadcastButton (), SIGNAL (clicked ()),
            this, SLOT (broadcastClicked ()));
    m_auto_resize = m_player->settings ()->autoresize;
    if (m_auto_resize)
        connect (m_player, SIGNAL (sourceDimensionChanged ()),
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

KDE_NO_EXPORT void KMPlayerApp::loadingProgress (int perc) {
    if (perc < 100)
        statusBar ()->changeItem (QString ("%1%").arg (perc), id_status_timer);
}

KDE_NO_EXPORT void KMPlayerApp::positioned (int pos, int length) {
    int left = (length - pos) / 10;
    if (left != last_time_left) {
        last_time_left = left;
        QString text ("--:--");
        if (left > 0) {
            int h = left / 3600;
            int m = (left % 3600) / 60;
            int s = left % 60;
            if (h > 0)
                text.sprintf ("%d:%02d:%02d", h, m, s);
            else
                text.sprintf ("%02d:%02d", m, s);
        }
        statusBar ()->changeItem (text, id_status_timer);
    }
}

KDE_NO_EXPORT void KMPlayerApp::windowVideoConsoleToggled (int wt) {
    if (wt == int (KMPlayer::View::WT_Video)) {
        toggleView->setText (i18n ("C&onsole"));
        toggleView->setIcon (QString ("konsole"));
    } else {
        toggleView->setText (i18n ("V&ideo"));
        toggleView->setIcon (QString ("video"));
    }
}

KDE_NO_EXPORT void KMPlayerApp::playerStarted () {
    KMPlayer::Source * source = m_player->source ();
    if (!strcmp (source->name (), "urlsource"))
        recentFiles ()->addURL (source->url ());
}

KDE_NO_EXPORT void KMPlayerApp::slotSourceChanged (KMPlayer::Source *olds, KMPlayer::Source * news) {
    if (olds) {
        disconnect (olds, SIGNAL (titleChanged (const QString &)), this,
                    SLOT (setCaption (const QString &)));
        disconnect (olds, SIGNAL (startPlaying ()),
                    this, SLOT (playerStarted ()));
    }
    if (news) {
        setCaption (news->prettyName (), false);
        connect (news, SIGNAL (titleChanged (const QString &)),
                 this, SLOT (setCaption (const QString &)));
        connect (news, SIGNAL (startPlaying ()),
                 this, SLOT (playerStarted ()));
    }
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
      i18n ("Enter a command that will output an audio/video stream\nto the stdout. This will be piped to a player's stdin.\n\nCommand:"), m_player->sources () ["pipesource"]->pipeCmd (), &ok, m_player->view());
    if (!ok) {
        slotStatusMsg (i18n ("Ready."));
        return;
    }
    static_cast <KMPlayerPipeSource *> (m_player->sources () ["pipesource"])->setCommand (cmd);
    m_player->setSource (m_player->sources () ["pipesource"]);
}

KDE_NO_EXPORT void KMPlayerApp::openVDR () {
    slotStatusMsg(i18n("Opening VDR..."));
    if (!strcmp (m_player->source ()->name (), "vdrsource") && m_player->process ()->playing ())
        static_cast<KMPlayerVDRSource *>(m_player->source())->toggleConnected();
    else
        m_player->setSource (m_player->sources () ["vdrsource"]);
}

struct IntroSource : public KMPlayer::Source {
    KMPlayerApp * m_app;
    IntroSource (KMPlayer::PartBase *p, KMPlayerApp * a)
        : KMPlayer::Source (i18n ("Intro"), p, "introsource"), m_app (a) {}
    KDE_NO_EXPORT bool hasLength () { return false; }
    KDE_NO_EXPORT bool isSeekable () { return false; }
    KDE_NO_EXPORT QString prettyName () { return i18n ("Intro"); }
    void activate ();
    void deactivate ();
    void stateElementChanged (KMPlayer::NodePtr node);
    bool deactivated;
    bool finished;
};

KDE_NO_EXPORT void IntroSource::activate () {
    if (m_player->settings ()->autoresize)
        m_app->disconnect(m_player, SIGNAL(sourceDimensionChanged()),m_app,SLOT(zoom100()));
    m_document = new KMPlayer::Document (QString (""), this);
    QString introfile = locate ("data", "kmplayer/intro.xml");
    QFile file (introfile);
    if (file.exists () && file.open (IO_ReadOnly)) {
        QTextStream ts (&file);
        KMPlayer::readXML (m_document, ts, QString::null);
    } else {
        QString smil = QString::fromLatin1 ("<smil><head><layout>"
          "<root-layout width='320' height='240' background-color='black'/>"
          "<region id='image1' left='31.25%' top='25%' width='37.5%' height='50%' z-order='1'/>"
          "<region id='reg1' top='10%' height='80%' z-order='2'>"
          "<region id='image2' left='128' top='136' width='64' bottom='56'/>"
          "</region>"
          "</layout></head><body><excl><seq>"
          "<img src='%1' region='image1' dur='0.5s' fit='fill'/>"
          "<par>"
          "<animate target='image1' attribute='width' from='37.5%' to='1%' dur='0.5' fill='freeze'/>"
          "<animate target='image1' attribute='left' from='31.25%' to='50%' dur='0.5' fill='freeze'/>"
          "<animate target='image1' attribute='height' from='50%' to='1%' dur='0.5' fill='freeze'/>"
          "<animate target='image1' attribute='top' from='25%' to='50%' dur='0.5' fill='freeze'/>"
          "<set target='image1' attribute='background-color' to='white' dur='0.5'/>"
          "</par>"
          "<par>"
          "<animate target='reg1' attribute='background-color' calcMode='discrete' values='#000000;#000000;#020202;#060606;#0B0B0B;#111111;#191919;#222222;#2D2D2D;#393939;#464646;#555555;#656565;#777777;#8A8A8A;#9E9E9E;#B4B4B4;#CCCCCC;#E4E4E4;#FFFFFF' dur='0.6'/>"
          "<animate target='image2' attribute='top' from='136' to='72' dur='0.4' fill='freeze'/>"
          "<img src='%2' region='image2' dur='0.6' fit='hidden'/>"
          "</par></seq><seq begin='reg1.activateEvent'/>"
          "</excl></body></smil>").arg (locate ("data", "kmplayer/noise.gif")).arg (KGlobal::iconLoader()->iconPath (QString::fromLatin1 ("kmplayer"), -64));
        QTextStream ts (smil.utf8 (), IO_ReadOnly);
        KMPlayer::readXML (m_document, ts, QString::null);
    }
    //m_document->normalize ();
    m_current = m_document; //mrl->self ();
    if (m_document && m_document->firstChild ()) {
        KMPlayer::Mrl * mrl = m_document->firstChild ()->mrl ();
        if (mrl) {
            Source::setDimensions (m_document->firstChild (), mrl->width, mrl->height);
            m_player->updateTree ();
            m_current->activate ();
            emit startPlaying ();
        }
    }
    deactivated = finished = false;
}

KDE_NO_EXPORT void IntroSource::stateElementChanged (KMPlayer::NodePtr node) {
    if (node->state == KMPlayer::Node::state_deactivated && node == m_document){
        m_document->reset ();
        finished = true;
        if (m_player->view ())
            m_app->restoreFromConfig ();
        emit stopPlaying ();
        if (!deactivated) // replace introsource with urlsource
            m_player->openURL (KURL ());
    }
}

KDE_NO_EXPORT void IntroSource::deactivate () {
    deactivated = true;
    if (m_player->settings ()->autoresize)
        m_app->connect(m_player, SIGNAL(sourceDimensionChanged()),m_app,SLOT(zoom100()));
    if (!finished && m_document) // user opens a source while introducing
        m_document->reset ();
}

KDE_NO_EXPORT void KMPlayerApp::restoreFromConfig () {
    if (m_player->view ()) {
        m_view->docArea ()->hide ();
        m_view->docArea ()->readDockConfig (m_player->config (), QString ("Window Layout"));
        m_view->docArea ()->show ();
        m_view->layout ()->activate ();
    }
}

KDE_NO_EXPORT void KMPlayerApp::openDocumentFile (const KURL& url)
{
    if (!m_played_intro) {
        m_played_intro = true;
        if (url.isEmpty () && m_player->source () &&
                m_player->source ()->document () &&
                m_player->source ()->document ()->hasChildNodes ()) {
            restoreFromConfig ();
            m_player->play ();
            return;
        } else if (!m_player->settings ()->no_intro && url.isEmpty ()) {
            m_player->setSource (new IntroSource (m_player, this));
            return;
        } else
            restoreFromConfig ();
    }
    slotStatusMsg(i18n("Opening file..."));
    m_player->openURL (url);
    if (m_broadcastconfig->broadcasting () && url.url() == m_broadcastconfig->serverURL ()) {
        // speed up replay
        FFServerSetting & ffs = m_broadcastconfig->ffserversettings;
        KMPlayer::Source * source = m_player->source ();
        if (!ffs.width.isEmpty () && !ffs.height.isEmpty ()) {
            source->setWidth (ffs.width.toInt ());
            source->setHeight (ffs.height.toInt ());
        }
        source->setIdentified ();
    }
    slotStatusMsg (i18n ("Ready."));
}

KDE_NO_EXPORT void KMPlayerApp::addURL (const KURL& url) {
    KMPlayer::Source * src = m_player->sources () ["urlsource"];
    if (m_player->source () != src)
        m_player->setSource (src);
    KMPlayer::NodePtr d = src->document ();
    if (d)
        d->appendChild (new KMPlayer::GenericURL (d, url.url ()));
}

KDE_NO_EXPORT void KMPlayerApp::saveProperties (KConfig * config) {
    config->writeEntry ("URL", m_player->source ()->url ().url ());
    config->writeEntry ("Visible", isVisible ());
}

KDE_NO_EXPORT void KMPlayerApp::readProperties (KConfig * config) {
    KURL url (config->readEntry ("URL", QString ()));
    openDocumentFile (url);
    if (!config->readBoolEntry ("Visible", true) && m_systray)
        hide ();
}

KDE_NO_EXPORT void KMPlayerApp::resizePlayer (int percentage) {
    KMPlayer::Source * source = m_player->source ();
    if (!source)
        return;
    int w, h;
    source->dimensions (w, h);
    if (w == 0 && h == 0) {
        w = 320;
        h = 240;
    } else
        h = m_view->viewer ()->heightForWidth (w);
    //kdDebug () << "KMPlayerApp::resizePlayer (" << w << "," << h << ")" << endl;
    if (w > 0 && h > 0) {
        if (m_view->controlPanel ()->isVisible ())
            h += m_view->controlPanel ()->size ().height ();
        QSize s1 = size ();
        QSize s2 = m_view->viewArea ()->size ();
        w += s1.width () - s2.width ();
        h += s1.height () - s2.height ();
        w = int (1.0 * w * percentage/100.0);
        h = int (1.0 * h * percentage/100.0);
        QSize s = sizeForCentralWidgetSize (QSize (w, h));
        if (s.width () != width () || s.height () != height ()) {
            QSize oldsize = m_view->viewArea ()->size ();
            resize (s);
        }
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
        m_view->controlPanel()->broadcastButton ()->toggle ();
    }
}

KDE_NO_EXPORT void KMPlayerApp::broadcastStarted () {
    if (!m_view->controlPanel()->broadcastButton ()->isOn ())
        m_view->controlPanel()->broadcastButton ()->toggle ();
}

KDE_NO_EXPORT void KMPlayerApp::broadcastStopped () {
    if (m_view->controlPanel()->broadcastButton ()->isOn ())
        m_view->controlPanel()->broadcastButton ()->toggle ();
    if (m_player->source () != m_player->sources () ["tvsource"])
        m_view->controlPanel()->broadcastButton ()->hide ();
    setCursor (QCursor (Qt::ArrowCursor));
}

KDE_NO_EXPORT bool KMPlayerApp::broadcasting () const {
    return m_broadcastconfig->broadcasting ();
}

KDE_NO_EXPORT void KMPlayerApp::saveOptions()
{
    config->setGroup ("General Options");
    if (m_player->settings ()->remembersize)
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
    m_view->setInfoMessage (QString::null);
    m_view->docArea ()->writeDockConfig (config, QString ("Window Layout"));
}


KDE_NO_EXPORT void KMPlayerApp::readOptions() {

    config->setGroup("General Options");

    // bar position settings
    KToolBar::BarPosition toolBarPos;
    toolBarPos=(KToolBar::BarPosition) config->readNumEntry("ToolBarPos", KToolBar::Top);
    toolBar("mainToolBar")->setBarPos(toolBarPos);

    // bar status settings
    viewToolBar->setChecked (config->readBoolEntry("Show Toolbar", true));
    slotViewToolBar();

    bool bViewStatusbar = config->readBoolEntry("Show Statusbar", true);
    viewStatusBar->setChecked(bViewStatusbar);
    slotViewStatusBar();

    viewMenuBar->setChecked (config->readBoolEntry("Show Menubar", true));
    slotViewMenuBar();

    QSize size = config->readSizeEntry ("Geometry");
    if (!size.isEmpty ())
        resize (size);

    config->setGroup ("Pipe Command");
    static_cast <KMPlayerPipeSource *> (m_player->sources () ["pipesource"])->setCommand (config->readEntry ("Command1", ""));
    // initialize the recent file list
    fileOpenRecent->loadEntries(config,"Recent Files");

    configChanged ();
}

#include <netwm.h>
#undef Always
#undef Never
#undef Status
#undef Unsorted
#undef Bool

KDE_NO_EXPORT void KMPlayerApp::minimalMode (bool by_user) {
    unsigned long props = NET::WMWindowType;
    NETWinInfo winfo (qt_xdisplay (), winId (), qt_xrootwin (), props);
    if (m_minimal_mode) {
        winfo.setWindowType (NET::Normal);
        readOptions ();
        if (by_user)
            disconnect (m_view->controlPanel ()->button (KMPlayer::ControlPanel::button_playlist), SIGNAL (clicked ()), this, SLOT (slotMinimalMode ()));
        restoreFromConfig ();
    } else {
        saveOptions ();
        menuBar()->hide();
        toolBar("mainToolBar")->hide();
        statusBar()->hide();
        if (by_user)
            connect (m_view->controlPanel ()->button (KMPlayer::ControlPanel::button_playlist), SIGNAL (clicked ()), this, SLOT (slotMinimalMode ()));
        if (by_user)
#if KDE_IS_VERSION(3, 1, 90)
            winfo.setWindowType (NET::Utility);
#else
            winfo.setWindowType (NET::Menu);
#endif
    }
    m_view->viewArea ()->minimalMode ();
    if (by_user) {
        QRect rect = m_view->viewArea ()->topWindowRect ();
        hide ();
        QTimer::singleShot (0, this, SLOT (zoom100 ()));
        show ();
        move (rect.x (), rect.y ());
    }
    m_minimal_mode = !m_minimal_mode;
}

KDE_NO_EXPORT void KMPlayerApp::slotMinimalMode () {
    minimalMode (true);
}

struct ExitSource : public KMPlayer::Source {
    KDE_NO_CDTOR_EXPORT ExitSource (KMPlayer::PartBase *p)
        : KMPlayer::Source (i18n ("Exit"), p, "exitsource") {}
    KDE_NO_EXPORT QString prettyName () { return i18n ("Exit"); }
    KDE_NO_EXPORT bool hasLength () { return false; }
    KDE_NO_EXPORT bool isSeekable () { return false; }
    void activate ();
    KDE_NO_EXPORT void deactivate () {}
    void stateElementChanged (KMPlayer::NodePtr node);
};

KDE_NO_EXPORT void ExitSource::activate () {
    m_document = new KMPlayer::Document (QString (""), this);
    QString exitfile = locate ("data", "kmplayer/exit.xml");
    QFile file (exitfile);
    if (file.exists () && file.open (IO_ReadOnly)) {
        QTextStream ts (&file);
        KMPlayer::readXML (m_document, ts, QString::null);
    } else {
        QString smil = QString::fromLatin1 ("<smil><head><layout>"
          "<root-layout width='320' height='240' background-color='black'/>"
          "<region id='reg1' top='10%' height='80%' z-order='2'>"
          "<region id='image' left='128' top='72' width='64' bottom='56'/>"
          "</region>"
          "</layout></head><body>"
          "<par>"
          "<animate target='reg1' attribute='background-color' calcMode='discrete' values='#FFFFFF;#E4E4E4;#CCCCCC;#B4B4B4;#9E9E9E;#8A8A8A;#777777;#656565;#555555;#464646;#393939;#2D2D2D;#222222;#191919;#111111;#0B0B0B;#060606;#020202;#000000;#000000' dur='0.6'/>"
          "<animate target='image' attribute='top' from='72' to='136' dur='0.4' fill='freeze'/>"
          "<img src='%2' region='image' dur='0.6' fit='hidden'/>"
          "</par>"
          "</body></smil>").arg (KGlobal::iconLoader()->iconPath (QString::fromLatin1 ("kmplayer"), -64));
        QTextStream ts (smil.utf8 (), IO_ReadOnly);
        KMPlayer::readXML (m_document, ts, QString::null);
    }
    //m_document->normalize ();
    m_current = m_document;
    if (m_document && m_document->firstChild ()) {
        KMPlayer::Mrl * mrl = m_document->firstChild ()->mrl ();
        if (mrl) {
            setDimensions (m_document->firstChild (), mrl->width, mrl->height);
            m_player->updateTree ();
            m_current->activate ();
            emit startPlaying ();
            return;
        }
    }
    qApp->quit ();
}

KDE_NO_EXPORT void ExitSource::stateElementChanged (KMPlayer::NodePtr node) {
    if (node->state == KMPlayer::Node::state_deactivated &&
            node == m_document &&
            m_player->view ())
       m_player->view ()->topLevelWidget ()->close ();
}

KDE_NO_EXPORT bool KMPlayerApp::queryClose () {
    // KMPlayerVDRSource has to wait for pending commands like mute and quit
    m_player->stop ();
    static_cast <KMPlayerVDRSource *> (m_player->sources () ["vdrsource"])->waitForConnectionClose ();
    // is arts control still running
    if (!m_dcopName.isEmpty ()) {
        QCString replytype;
        QByteArray data, replydata;
        kapp->dcopClient ()->call (m_dcopName, "MainApplication-Interface", "quit()", data, replytype, replydata);
    }
    if (m_played_exit || m_player->settings ()->no_intro)
        return true;
    if (m_auto_resize)
        disconnect(m_player, SIGNAL(sourceDimensionChanged()),this,SLOT(zoom100()));
    m_played_exit = true;
    if (!m_minimal_mode)
        minimalMode (false);
    m_player->setSource (new ExitSource (m_player));
    return false;
}

KDE_NO_EXPORT bool KMPlayerApp::queryExit()
{
    if (!m_minimal_mode)
        saveOptions();
    disconnect (m_player->settings (), SIGNAL (configChanged ()),
                this, SLOT (configChanged ()));
    m_player->settings ()->writeConfig ();
    return true;
}

KDE_NO_EXPORT void KMPlayerApp::slotFileNewWindow()
{
    slotStatusMsg(i18n("Opening a new application window..."));

    KMPlayerApp *new_window= new KMPlayerApp();
    new_window->show();

    slotStatusMsg(i18n("Ready."));
}

KDE_NO_EXPORT void KMPlayerApp::slotFileOpen () {
    KURL::List urls = KFileDialog::getOpenURLs (QString::null, i18n ("*|All Files"), this, i18n ("Open File"));
    if (urls.size () == 1) {
        openDocumentFile (urls [0]);
    } else if (urls.size () > 1) {
        m_player->openURL (KURL ());
        for (unsigned int i = 0; i < urls.size (); i++)
            addURL (urls [i]);
    }
}

KDE_NO_EXPORT void KMPlayerApp::slotFileOpenRecent(const KURL& url)
{
    slotStatusMsg(i18n("Opening file..."));

    openDocumentFile (url);

}

KDE_NO_EXPORT void KMPlayerApp::slotSaveAs () {
    QString url = KFileDialog::getSaveFileName (QString::null, QString::null, this, i18n ("Save File"));
    if (!url.isEmpty ()) {
        QFile file (url);
        if (!file.open (IO_WriteOnly)) {
            KMessageBox::error (this, i18n ("Error opening file %1.\n%2.").arg (url).arg (file.errorString ()), i18n("Error"));
            return;
        }
        if (m_player->source ()) {
            KMPlayer::NodePtr doc = m_player->source ()->document ();
            if (doc) {
                QTextStream ts (&file);
                ts.setEncoding (QTextStream::UnicodeUTF8);
                ts << QString ("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
                if (doc->childNodes ()->length () == 1)
                    ts << doc->innerXML ();
                else
                    ts << doc->outerXML ();
            }
        }
        file.close ();
    }
}

KDE_NO_EXPORT void KMPlayerApp::slotClearHistory () {
    //fileOpenRecent->clearURLList ();
    int mi = fileOpenRecent->maxItems ();
    fileOpenRecent->setMaxItems (0);
    fileOpenRecent->setMaxItems (mi);
    m_player->settings ()->urllist.clear ();
    m_player->settings ()->sub_urllist.clear ();
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

    // whoever implemented this should fix it too, work around ..
    if (memberList->count () > 1)
        deleteLater ();
    else
        qApp->quit ();
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

KDE_NO_EXPORT void KMPlayerApp::slotStatusMsg (const QString &text) {
    KStatusBar * sb = statusBar ();
    sb->clear ();
    sb->changeItem (text, id_status_msg);
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
    else {
        show ();
        setGeometry (m_view->viewArea ()->topWindowRect ());
    }
}

KDE_NO_EXPORT void KMPlayerApp::startArtsControl () {
    QCString fApp, fObj;
    QByteArray data, replydata;
    QCStringList apps = kapp->dcopClient ()->registeredApplications();
    QCStringList::ConstIterator end( apps.end() );
    for( QCStringList::ConstIterator it = apps.begin(); it != end; ++it)
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
    if (m_player->settings ()->docksystray && !m_systray) {
        m_systray = new KSystemTray (this);
        m_systray->setPixmap (KGlobal::iconLoader ()->loadIcon (QString ("kmplayer"), KIcon::NoGroup, 22));
        m_systray->show ();
    } else if (!m_player->settings ()->docksystray && m_systray) {
        delete m_systray;
        m_systray = 0L;
    }
    if (m_player->settings ()->autoresize && !m_auto_resize)
        connect(m_player,SIGNAL(sourceDimensionChanged()),this,SLOT(zoom100()));
    else if (!m_player->settings ()->autoresize && m_auto_resize)
        disconnect(m_player, SIGNAL(sourceDimensionChanged()),this,SLOT(zoom100()));
    m_auto_resize = m_player->settings ()->autoresize;
    static_cast <KMPlayerTVSource *> (m_player->sources () ["tvsource"])->buildMenu ();
}

KDE_NO_EXPORT void KMPlayerApp::keepSizeRatio () {
    m_view->setKeepSizeRatio (!m_view->keepSizeRatio ());
    m_player->settings ()->sizeratio = m_view->keepSizeRatio ();
    viewKeepRatio->setChecked (m_view->keepSizeRatio ());
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT KMPlayerMenuSource::KMPlayerMenuSource (const QString & n, KMPlayerApp * a, QPopupMenu * m, const char * src)
    : KMPlayer::Source (n, a->player (), src), m_menu (m), m_app (a) {
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
    setURL (KURL ("dvd://"));
    m_player->settings ()->addPage (this);
}

KDE_NO_CDTOR_EXPORT KMPlayerDVDSource::~KMPlayerDVDSource () {
}

KDE_NO_EXPORT bool KMPlayerDVDSource::processOutput (const QString & str) {
    if (KMPlayer::Source::processOutput (str))
        return true;
    if (m_identified)
        return false;
    //kdDebug () << "scanning " << cstr << endl;
    QRegExp * patterns = static_cast <KMPlayer::MPlayer *> (m_player->players () ["mplayer"])->configPage ()->m_patterns;
    QRegExp & langRegExp = patterns[KMPlayer::MPlayerPreferencesPage::pat_dvdlang];
    QRegExp & subtitleRegExp = patterns[KMPlayer::MPlayerPreferencesPage::pat_dvdsub];
    QRegExp & titleRegExp = patterns[KMPlayer::MPlayerPreferencesPage::pat_dvdtitle];
    QRegExp & chapterRegExp = patterns[KMPlayer::MPlayerPreferencesPage::pat_dvdchapter];
    bool post090 = m_player->settings ()->mplayerpost090;
    if (!post090 && subtitleRegExp.search (str) > -1) {
        bool ok;
        int sub_id = subtitleRegExp.cap (1).toInt (&ok);
        QString sub_title = ok ? subtitleRegExp.cap (2) : subtitleRegExp.cap(1);
        if (!ok)
            sub_id = subtitleRegExp.cap (2).toInt (&ok);
        m_dvdsubtitlemenu->insertItem (sub_title, sub_id);
        kdDebug () << "subtitle sid:" << sub_id << " lang:" << sub_title <<endl;
    } else if (!post090 && langRegExp.search (str) > -1) {
        bool ok;
        int lang_id = langRegExp.cap (1).toInt (&ok);
        QString lang_title = ok ? langRegExp.cap (2) : langRegExp.cap (1);
        if (!ok)
            lang_id = langRegExp.cap (2).toInt (&ok);
        m_dvdlanguagemenu->insertItem (lang_title, lang_id);
        kdDebug () << "lang aid:" << lang_id << " lang:" << lang_title << endl;
    } else if (titleRegExp.search (str) > -1) {
        kdDebug () << "title " << titleRegExp.cap (1) << endl;
        unsigned ts = titleRegExp.cap (1).toInt ();
        if ( ts > 100) ts = 100;
        for (unsigned t = 1; t <= ts; t++)
            m_dvdtitlemenu->insertItem (QString::number (t), t);
    } else if (chapterRegExp.search (str) > -1) {
        kdDebug () << "chapter " << chapterRegExp.cap (1) << endl;
        unsigned chs = chapterRegExp.cap (1).toInt ();
        if ( chs > 100) chs = 100;
        for (unsigned c = 1; c <= chs; c++)
            m_dvdchaptermenu->insertItem (QString::number (c), c);
    } else
        return false;
    return true;
}

KDE_NO_EXPORT void KMPlayerDVDSource::activate () {
    m_start_play = m_auto_play;
    m_current_title = -1;
    setURL (KURL ("dvd://"));
    buildArguments ();
    m_menu->insertItem (i18n ("&Titles"), m_dvdtitlemenu);
    m_menu->insertItem (i18n ("&Chapters"), m_dvdchaptermenu);
    if (!m_player->settings ()->mplayerpost090) {
        m_menu->insertItem (i18n ("Audio &Language"), m_dvdlanguagemenu);
        m_menu->insertItem (i18n ("&SubTitles"), m_dvdsubtitlemenu);
        connect (m_dvdsubtitlemenu, SIGNAL (activated (int)),
                 this, SLOT (subtitleMenuClicked (int)));
        connect (m_dvdlanguagemenu, SIGNAL (activated (int)),
                 this, SLOT (languageMenuClicked (int)));
    }
    connect (m_dvdtitlemenu, SIGNAL (activated (int)),
             this, SLOT (titleMenuClicked (int)));
    connect (m_dvdchaptermenu, SIGNAL (activated (int)),
             this, SLOT (chapterMenuClicked (int)));
    if (m_start_play)
        QTimer::singleShot (0, m_player, SLOT (play ()));
}

KDE_NO_EXPORT void KMPlayerDVDSource::setIdentified (bool b) {
    KMPlayer::Source::setIdentified (b);
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
    if (m_player->view ()) {
        m_dvdtitlemenu->clear ();
        m_dvdsubtitlemenu->clear ();
        m_dvdchaptermenu->clear ();
        m_dvdlanguagemenu->clear ();
        m_menu->removeItemAt (m_menu->count () - 1);
        m_menu->removeItemAt (m_menu->count () - 1);
        if (!m_player->settings ()->mplayerpost090) {
            m_menu->removeItemAt (m_menu->count () - 1);
            m_menu->removeItemAt (m_menu->count () - 1);
            disconnect (m_dvdsubtitlemenu, SIGNAL (activated (int)),
                        this, SLOT (subtitleMenuClicked (int)));
            disconnect (m_dvdlanguagemenu, SIGNAL (activated (int)),
                        this, SLOT (languageMenuClicked (int)));
        }
        disconnect (m_dvdtitlemenu, SIGNAL (activated (int)),
                    this, SLOT (titleMenuClicked (int)));
        disconnect (m_dvdchaptermenu, SIGNAL (activated (int)),
                    this, SLOT (chapterMenuClicked (int)));
    }
}

KDE_NO_EXPORT void KMPlayerDVDSource::buildArguments () {
    QString url ("dvd://");
    if (m_document) {
        if (m_current_title > 0)
            url += QString::number (m_current_title);
        m_document->mrl ()->src = url;
    } else
        setURL (KURL (url));
    m_options = QString (m_identified ? "" : "-v ");
    if (m_identified) {
        for (unsigned i = 0; i < m_dvdsubtitlemenu->count (); i++)
            if (m_dvdsubtitlemenu->isItemChecked (m_dvdsubtitlemenu->idAt (i)))
                m_options += "-sid " + QString::number (m_dvdsubtitlemenu->idAt(i));
        for (unsigned i = 0; i < m_dvdchaptermenu->count (); i++)
            if (m_dvdchaptermenu->isItemChecked (i))
                m_options += QString (" -chapter %1").arg (i);
        for (unsigned i = 0; i < m_dvdlanguagemenu->count (); i++)
            if (m_dvdlanguagemenu->isItemChecked (m_dvdlanguagemenu->idAt (i)))
                m_options += " -aid " + QString::number(m_dvdlanguagemenu->idAt(i));
        if (m_player->settings ()->dvddevice.length () > 0)
            m_options += QString(" -dvd-device ") + m_player->settings()->dvddevice;
    }
    m_recordcmd = m_options + QString (" -vop scale -zoom");
}

KDE_NO_EXPORT QString KMPlayerDVDSource::filterOptions () {
    KMPlayer::Settings * settings = m_player->settings ();
    if (!settings->disableppauto)
        return KMPlayer::Source::filterOptions ();
    return QString ("");
}

KDE_NO_EXPORT void KMPlayerDVDSource::titleMenuClicked (int id) {
    if (m_current_title != id) {
        m_player->stop ();
        m_current_title = id;
        m_identified = false;
        buildArguments ();
        m_dvdtitlemenu->clear ();
        m_dvdsubtitlemenu->clear ();
        m_dvdchaptermenu->clear ();
        m_dvdlanguagemenu->clear ();
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
    setURL (KURL ("dvd://"));
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
    QTimer::singleShot (0, m_player, SLOT (play ()));
    connect (this, SIGNAL (stopPlaying ()), this, SLOT(finished ()));
}

KDE_NO_EXPORT void KMPlayerDVDNavSource::finished () {
    disconnect (this, SIGNAL (stopPlaying ()), this, SLOT(finished ()));
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
    m_player->settings ()->addPage (this);
    setURL (KURL ("vcd://"));
}

KDE_NO_CDTOR_EXPORT KMPlayerVCDSource::~KMPlayerVCDSource () {
}

KDE_NO_EXPORT bool KMPlayerVCDSource::processOutput (const QString & str) {
    if (KMPlayer::Source::processOutput (str))
        return true;
    if (m_identified)
        return false;
    //kdDebug () << "scanning " << cstr << endl;
    QRegExp * patterns = static_cast<KMPlayer::MPlayer *> (m_player->players () ["mplayer"])->configPage ()->m_patterns;
    QRegExp & trackRegExp = patterns [KMPlayer::MPlayerPreferencesPage::pat_vcdtrack];
    if (trackRegExp.search (str) > -1) {
        if (m_document->state != KMPlayer::Element::state_deferred)
            m_document->defer ();
        m_document->appendChild (new KMPlayer::GenericMrl (m_document, QString ("vcd://") + trackRegExp.cap (1), i18n ("Track ") + trackRegExp.cap (1)));
        kdDebug () << "track " << trackRegExp.cap (1) << endl;
        return true;
    }
    return false;
}

KDE_NO_EXPORT void KMPlayerVCDSource::activate () {
    m_player->stop ();
    init ();
    m_start_play = m_auto_play;
    setURL (KURL ("vcd://"));
    buildArguments ();
    if (m_start_play)
        QTimer::singleShot (0, m_player, SLOT (play ()));
}

KDE_NO_EXPORT void KMPlayerVCDSource::deactivate () {
}

KDE_NO_EXPORT void KMPlayerVCDSource::setIdentified (bool b) {
    KMPlayer::Source::setIdentified (b);
    if (!m_document->hasChildNodes ())
        m_current = m_document;
    m_player->updateTree ();
    buildArguments ();
    if (m_current->state == KMPlayer::Element::state_deferred)
        m_current->undefer ();
    m_app->slotStatusMsg (i18n ("Ready."));
}

KDE_NO_EXPORT void KMPlayerVCDSource::buildArguments () {
    QString url ("vcd://");
    if (m_current != m_document)
        url += m_current->mrl ()->src;
    m_options.truncate (0);
    if (m_player->settings ()->vcddevice.length () > 0)
        m_options+=QString(" -cdrom-device ") + m_player->settings()->vcddevice;
    m_recordcmd = m_options;
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
 : KMPlayer::Source (i18n ("Pipe"), a->player (), "pipesource"), m_app (a) {
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
    // dangerous !! if (!m_url.protocol ().compare ("kmplayer"))
    //    m_pipecmd = KURL::decode_string (m_url.path ()).mid (1);
    setURL (KURL ("stdin://"));
    KMPlayer::GenericMrl * gen = new KMPlayer::GenericMrl (m_document, QString ("stdin://"), m_pipecmd);
    gen->bookmarkable = false;
    m_document->appendChild (gen);
    m_recordcmd = m_options = QString ("-"); // or m_url?
    m_identified = true;
    reset ();
    QTimer::singleShot (0, m_player, SLOT (play ()));
    m_app->slotStatusMsg (i18n ("Ready."));
}

KDE_NO_EXPORT void KMPlayerPipeSource::deactivate () {
}

KDE_NO_EXPORT QString KMPlayerPipeSource::prettyName () {
    return i18n ("Pipe - %1").arg (m_pipecmd);
}

KDE_NO_EXPORT void KMPlayerPipeSource::setCommand (const QString & cmd) {
    m_pipecmd = cmd;
    if (m_document)
        m_document->mrl ()->pretty_name = cmd;
}

#include "kmplayer.moc"
#include "kmplayerappsource.moc"
