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
#include <qiodevice.h>
#include <qprinter.h>
#include <qpainter.h>
#include <qcheckbox.h>
#include <qmultilineedit.h>
#include <qpushbutton.h>
#include <qkeysequence.h>
#include <qapplication.h>

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
#include <kregexp.h>
#include <kprocess.h>
#include <dcopclient.h>

// application specific includes
#include "kmplayer.h"
#include "kmplayerview.h"
#include "kmplayerdoc.h"
#include "kmplayer_part.h"
#include "kmplayerconfig.h"

#define ID_STATUS_MSG 1

KMPlayerApp::KMPlayerApp(QWidget* , const char* name)
    : KMainWindow(0, name),
      config (kapp->config ()),
      m_player (new KMPlayer (this, config)), 
      m_opendvd (false),
      m_openvcd (false),
      m_openpipe (false)
{
    initStatusBar();
    initActions();
    initDocument();
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
    fileNew = KStdAction::openNew(this, SLOT(slotFileNew()), actionCollection());
    fileOpen = KStdAction::open(this, SLOT(slotFileOpen()), actionCollection());
    fileOpenRecent = KStdAction::openRecent(this, SLOT(slotFileOpenRecent(const KURL&)), actionCollection());
    fileClose = KStdAction::close(this, SLOT(slotFileClose()), actionCollection());
    fileQuit = KStdAction::quit(this, SLOT(slotFileQuit()), actionCollection());
    /*KAction *preference =*/ KStdAction::preferences (m_player, SLOT (showConfigDialog ()), actionCollection());
    new KAction (i18n ("50%"), 0, 0, this, SLOT (zoom50 ()), actionCollection (), "view_zoom_50");
    new KAction (i18n ("100%"), 0, 0, this, SLOT (zoom100 ()), actionCollection (), "view_zoom_100");
    new KAction (i18n ("150%"), 0, 0, this, SLOT (zoom150 ()), actionCollection (), "view_zoom_150");
    viewKeepRatio = new KToggleAction (i18n ("&Keep width/height ratio"), 0, this, SLOT (keepSizeRatio ()), actionCollection (), "view_keep_ratio");
    viewShowConsoleOutput = new KToggleAction (i18n ("&Show Console Output"), 0, this, SLOT (showConsoleOutput ()), actionCollection (), "view_show_console");
    /*KAction *fullscreenact =*/ new KAction (i18n("&Full Screen"), 0, 0, this, SLOT(fullScreen ()), actionCollection (), "view_fullscreen");
    /*KAction *playact =*/ new KAction (i18n ("P&lay"), 0, 0, this, SLOT (play ()), actionCollection (), "view_play");
    /*KAction *pauseact =*/ new KAction (i18n ("&Pause"), 0, 0, m_player, SLOT (pause ()), actionCollection (), "view_pause");
    /*KAction *stopact =*/ new KAction (i18n ("&Stop"), 0, 0, m_player, SLOT (stop ()), actionCollection (), "view_stop");
    /*KAction *artsctrl =*/ new KAction (i18n ("&Arts Control"), 0, 0, this, SLOT (startArtsControl ()), actionCollection (), "view_arts_control");
    //viewToolBar = KStdAction::showToolbar(this, SLOT(slotViewToolBar()), actionCollection());
    viewStatusBar = KStdAction::showStatusbar(this, SLOT(slotViewStatusBar()), actionCollection());

    fileNewWindow->setStatusText(i18n("Opens a new application window"));
    fileNew->setStatusText(i18n("Creates a new document"));
    fileOpen->setStatusText(i18n("Opens an existing document"));
    fileOpenRecent->setStatusText(i18n("Opens a recently used file"));
    fileClose->setStatusText(i18n("Closes the actual document"));
    fileQuit->setStatusText(i18n("Quits the application"));
    //viewToolBar->setStatusText(i18n("Enables/disables the toolbar"));
    viewStatusBar->setStatusText(i18n("Enables/disables the statusbar"));

    // use the absolute path to your kmplayerui.rc file for testing purpose in createGUI();
    createGUI();
}

void KMPlayerApp::initStatusBar()
{
    ///////////////////////////////////////////////////////////////////
    // STATUSBAR
    // TODO: add your own items you need for displaying current application status.
    statusBar()->insertItem(i18n("Ready."), ID_STATUS_MSG);
}

void KMPlayerApp::initDocument()
{
    doc = new KMPlayerDoc(this);
    doc->newDocument();
}

void KMPlayerApp::initView ()
{ 
    ////////////////////////////////////////////////////////////////////
    // create the main widget here that is managed by KTMainWindow's view-region and
    // connect the widget to your document to display document contents.
    view = static_cast <KMPlayerView*> (m_player->view());
    doc->addView (view);
    setCentralWidget (view);
    m_sourcemenu = menuBar ()->findItem (menuBar ()->idAt (0));
    m_sourcemenu->setText (i18n ("S&ource"));
    m_dvdmenu = new QPopupMenu (this);
    m_dvdmenu->insertTearOffHandle ();
    m_dvdtitlemenu = new QPopupMenu (this);
    m_dvdsubtitlemenu = new QPopupMenu (this);
    m_dvdchaptermenu = new QPopupMenu (this);
    m_dvdlanguagemenu = new QPopupMenu (this);
    m_vcdmenu = new QPopupMenu (this);
    m_vcdmenu->insertTearOffHandle ();
    m_vcdtrackmenu = new QPopupMenu (this);
    m_dvdtitlemenu->setCheckable (true);
    m_dvdsubtitlemenu->setCheckable (true);
    m_dvdchaptermenu->setCheckable (true);
    m_dvdlanguagemenu->setCheckable (true);
    m_dvdmenuId = m_sourcemenu->popup ()->insertItem (i18n ("&DVD"), m_dvdmenu, -1, 3);
    m_havedvdmenu = true;
    m_dvdmenu->insertItem (i18n ("&Open DVD"), this, SLOT(openDVD ()));
    m_dvdmenu->insertItem (i18n ("&Titles"), m_dvdtitlemenu);
    m_dvdmenu->insertItem (i18n ("&Chapters"), m_dvdchaptermenu);
    m_dvdmenu->insertItem (i18n ("Audio &Language"), m_dvdlanguagemenu);
    m_dvdmenu->insertItem (i18n ("&SubTitles"), m_dvdsubtitlemenu);
    m_vcdmenuId = m_sourcemenu->popup ()->insertItem (i18n ("V&CD"), m_vcdmenu, -1, 4);
    m_havevcdmenu = true;
    m_vcdmenu->insertItem (i18n ("&Open VCD"), this, SLOT(openVCD ()));
    m_vcdmenu->insertItem (i18n ("&Tracks"), m_vcdtrackmenu);
    m_sourcemenu->popup ()->insertItem (i18n ("&Open Pipe ..."), this, SLOT(openPipe ()), 0, -1, 5);
    connect (view->playButton (), SIGNAL (clicked ()), this, SLOT (playDisc()));
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

    /*QPopupMenu * viewmenu = new QPopupMenu;
    viewmenu->insertItem (i18n ("Full Screen"), this, SLOT(fullScreen ()), 
                          QKeySequence ("CTRL + Key_F"));
    menuBar ()->insertItem (i18n ("&View"), viewmenu, -1, 2);*/
    toolBar("mainToolBar")->hide();
    setCaption (doc->URL (). fileName (), false);

}

void KMPlayerApp::loadingProgress (int percentage) {
    if (percentage >= 100)
        slotStatusMsg(i18n("Ready"));
    else
        slotStatusMsg (QString::number (percentage) + "%");
}

void KMPlayerApp::openDVD () {
    slotStatusMsg(i18n("Opening DVD..."));
    doc->newDocument();
    doc->setAspect (-1.0);
    m_player->stop ();
    m_player->setURL (KURL ());
    QString args ("-v dvd:// -identify -quiet -nocache");
    if (m_player->configDialog ()->dvddevice.length () > 0)
        args += QString(" -dvd-device ") + m_player->configDialog ()->dvddevice;
    bool loop = m_player->configDialog ()->loop;
    m_player->configDialog ()->loop = false;
    if (m_player->run (args.ascii())) {
        connect (m_player, SIGNAL (finished()), this, SLOT(finishedOpenDVD ()));
        m_openvcd = m_openpipe = false;
        m_opendvd = true;
    } else
        kdDebug () << "openDVD failed" << endl;
    m_player->configDialog ()->loop = loop;
}

void KMPlayerApp::openVCD () {
    slotStatusMsg(i18n("Opening VCD..."));
    doc->newDocument();
    doc->setAspect (-1.0);
    m_player->stop ();
    m_player->setURL (KURL ());
    QString args ("-v vcd:// -identify -quiet -nocache");
    if (m_player->configDialog ()->vcddevice.length () > 0)
        args += QString(" -cdrom-device ")+m_player->configDialog ()->vcddevice;
    bool loop = m_player->configDialog ()->loop;
    m_player->configDialog ()->loop = false;
    if (m_player->run (args.ascii())) {
        connect (m_player, SIGNAL (finished()), this, SLOT(finishedOpenVCD ()));
        m_opendvd = m_openpipe = false;
        m_openvcd = true;
    } else
        kdDebug () << "openVCD failed" << endl;
    m_player->configDialog ()->loop = loop;
}

void KMPlayerApp::openPipe () {
    slotStatusMsg(i18n("Opening Pipe..."));
    bool ok;
    QString cmd = KLineEditDlg::getText (i18n("Read from Pipe"),
                       i18n ("Enter command:"), m_pipe, &ok, m_player->view());
    if (!ok) {
        slotStatusMsg (i18n ("Ready."));
        return;
    }
    m_pipe = cmd;
    playPipe ();
}

void KMPlayerApp::playPipe () {
    doc->newDocument();
    doc->setAspect (-1.0);
    m_player->stop ();
    m_player->setURL (KURL ());
    QString args ("-quiet -");
    m_player->run (args.ascii(), m_pipe.ascii());
    setCaption (i18n ("Pipe - ") + m_pipe, false);
    m_openpipe = true;
    m_openvcd = m_opendvd = false;
    slotStatusMsg (i18n ("Ready."));
}

void KMPlayerApp::openDocumentFile (const KURL& url)
{
    slotStatusMsg(i18n("Opening file..."));
    doc->newDocument();		
    doc->setAspect (-1.0);
    m_openpipe = m_openvcd = m_opendvd = false;
    m_dvdtitlemenu->clear ();
    m_dvdsubtitlemenu->clear ();
    m_dvdchaptermenu->clear ();
    m_dvdlanguagemenu->clear ();
    m_player->stop ();
    m_player->setURL (url);
    bool loop = m_player->configDialog ()->loop;
    m_player->configDialog ()->loop = false;
    if (!url.isEmpty () && m_player->run ("-quiet -nocache -identify")) {
        connect (m_player, SIGNAL (finished ()), this, SLOT (finished ()));
        doc->setURL (url);
    } else
        slotStatusMsg (i18n ("Ready."));
    m_player->configDialog ()->loop = loop;
}

void KMPlayerApp::resizePlayer (int percentage) {
    int w = doc->width ();
    int h = doc->height ();
    if (w <= 0 || h <= 0) {
        m_player->sizes (w, h);
        doc->setWidth (w);
        doc->setHeight (h);
    }
    kdDebug () << "KMPlayerApp::resizePlayer (" << w << "," << h << ")" << endl;
    if (w > 0 && h > 0) {
        if (doc->aspect () > 0.01) {
            w = int (doc->aspect () * doc->height ());
            w += w % 2;
            doc->setWidth (w);
        } else
            doc->setAspect (1.0 * w/h);
        KMPlayerView * kview = static_cast <KMPlayerView*> (m_player->view());
        view->viewer()->setAspect (view->keepSizeRatio() ? doc->aspect() : 0.0);
        int h = doc->height () + 2 + kview->buttonBar()->frameSize ().height ();
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
        
void KMPlayerApp::finished () {
    disconnect (m_player, SIGNAL (finished ()), this, SLOT (finished ()));
    KMPlayerView * kview = static_cast <KMPlayerView*> (m_player->view());
    QMultiLineEdit * txt = kview->consoleOutput ();
    for (int i = 0; i < txt->numLines (); i++) {
        QString str = txt->textLine (i);
        if (str.startsWith ("ID_VIDEO_WIDTH")) {
            int pos = str.find ('=');
            if (pos > 0)
                doc->setWidth (str.mid (pos + 1).toInt());
        } else if (str.startsWith ("ID_VIDEO_HEIGHT")) {
            int pos = str.find ('=');
            if (pos > 0)
                doc->setHeight (str.mid (pos + 1).toInt());
        } else if (str.startsWith ("ID_VIDEO_ASPECT")) {
            int pos = str.find ('=');
            if (pos > 0)
                doc->setAspect (str.mid (pos + 1).toFloat());
        }
    }
    resizePlayer (100);
    const KURL & url = doc->URL ();
    if (m_player->openURL (url)) {
        fileOpenRecent->addURL (url);
        setCaption (url.fileName (), false);
    }
    //doc->openDocument( url);
    slotStatusMsg (i18n ("Ready."));
}

void KMPlayerApp::finishedOpenDVD () {
    disconnect (m_player, SIGNAL (finished()), this, SLOT (finishedOpenDVD ()));

    m_dvdtitlemenu->clear ();
    m_dvdsubtitlemenu->clear ();
    m_dvdchaptermenu->clear ();
    m_dvdlanguagemenu->clear ();

    KMPlayerView * kview = static_cast <KMPlayerView*> (m_player->view());
    QMultiLineEdit * txt = kview->consoleOutput ();
    KRegExp langRegExp (m_player->configDialog ()->langpattern.ascii());
    KRegExp subtitleRegExp (m_player->configDialog ()->subtitlespattern.ascii());
    KRegExp titleRegExp (m_player->configDialog ()->titlespattern.ascii());
    KRegExp chapterRegExp (m_player->configDialog ()->chapterspattern.ascii());
    //kdDebug () << "finishedOpenDVD " << txt->numLines () << endl;
    for (int i = 0; i < txt->numLines (); i++) {
        QString str = txt->textLine (i);
        const char * cstr = str.latin1 ();
        //kdDebug () << "scanning " << cstr << endl;
        if (str.startsWith ("ID_VIDEO_WIDTH")) {
            int pos = str.find ('=');
            if (pos > 0)
                doc->setWidth (str.mid (pos + 1).toInt());
        } else if (str.startsWith ("ID_VIDEO_HEIGHT")) {
            int pos = str.find ('=');
            if (pos > 0)
                doc->setHeight (str.mid (pos + 1).toInt());
        } else if (str.startsWith ("ID_VIDEO_ASPECT")) {
            int pos = str.find ('=');
            if (pos > 0)
                doc->setAspect (str.mid (pos + 1).toFloat());
        } else if (subtitleRegExp.match (cstr)) {
            m_dvdsubtitlemenu->insertItem (subtitleRegExp.group (2), this,
                                           SLOT (subtitleMenuClicked(int)), 0, 
                                           atoi (subtitleRegExp.group (1)));
            kdDebug () << "subtitle sid:" << subtitleRegExp.group (1) << 
                " lang:" << subtitleRegExp.group (2) << endl;
        } else if (langRegExp.match (cstr)) {
            m_dvdlanguagemenu->insertItem (langRegExp.group (1), this,
                                           SLOT (languageMenuClicked(int)), 0,
                                           atoi (langRegExp.group (2)));
            kdDebug () << "lang aid:" << langRegExp.group (2) << 
                " lang:" << langRegExp.group (1) << endl;
        } else if (titleRegExp.match (cstr)) {
            kdDebug () << "title " << titleRegExp.group (1) << endl;
            unsigned ts = QString (titleRegExp.group (1)).toInt ();
            if ( ts > 100) ts = 100;
            for (unsigned t = 0; t < ts; t++)
                m_dvdtitlemenu->insertItem (QString::number (t + 1), this, 
                                            SLOT (titleMenuClicked(int)), 0, t);
        } else if (chapterRegExp.match (cstr)) {
            kdDebug () << "chapter " << chapterRegExp.group (1) << endl;
            unsigned chs = QString (chapterRegExp.group (1)).toInt ();
            if ( chs > 100) chs = 100;
            for (unsigned c = 0; c < chs; c++)
                m_dvdchaptermenu->insertItem (QString::number (c + 1), this,
                                          SLOT (chapterMenuClicked(int)), 0, c);
        }
    }
    //if (m_dvdsubtitlemenu->count()) m_dvdsubtitlemenu->setItemChecked (m_dvdsubtitlemenu->idAt (0), true);
    if (m_dvdtitlemenu->count ()) m_dvdtitlemenu->setItemChecked (0, true);
    if (m_dvdchaptermenu->count ()) m_dvdchaptermenu->setItemChecked (0, true);
    if (m_dvdlanguagemenu->count()) m_dvdlanguagemenu->setItemChecked (m_dvdlanguagemenu->idAt (0), true);
    resizePlayer (100);
    if (m_player->configDialog ()->playdvd)
        playDVD ();
    //doc->openDocument( url);
    setCaption (i18n ("DVD"), false);
    slotStatusMsg (i18n ("Ready."));
}

void KMPlayerApp::finishedOpenVCD () {
    disconnect (m_player, SIGNAL (finished()), this, SLOT (finishedOpenVCD ()));

    m_vcdtrackmenu->clear ();

    KMPlayerView * kview = static_cast <KMPlayerView*> (m_player->view());
    QMultiLineEdit * txt = kview->consoleOutput ();
    KRegExp trackRegExp (m_player->configDialog ()->trackspattern.ascii());
    //kdDebug () << "finishedOpenDVD " << txt->numLines () << endl;
    for (int i = 0; i < txt->numLines (); i++) {
        QString str = txt->textLine (i);
        const char * cstr = str.latin1 ();
        //kdDebug () << "scanning " << cstr << endl;
        if (str.startsWith ("ID_VIDEO_WIDTH")) {
            int pos = str.find ('=');
            if (pos > 0)
                doc->setWidth (str.mid (pos + 1).toInt());
        } else if (str.startsWith ("ID_VIDEO_HEIGHT")) {
            int pos = str.find ('=');
            if (pos > 0)
                doc->setHeight (str.mid (pos + 1).toInt());
        } else if (str.startsWith ("ID_VIDEO_ASPECT")) {
            int pos = str.find ('=');
            if (pos > 0)
                doc->setAspect (str.mid (pos + 1).toFloat());
        } else if (trackRegExp.match (cstr)) {
            m_vcdtrackmenu->insertItem (trackRegExp.group (1), this,
                                        SLOT (trackMenuClicked(int)), 0, 
                                           m_vcdtrackmenu->count ());
            kdDebug () << "track " << trackRegExp.group (1) << endl;
        }
    }
    if (m_vcdtrackmenu->count()) m_vcdtrackmenu->setItemChecked (0, true);
    resizePlayer (100);
    if (m_player->configDialog ()->playvcd)
        playVCD ();
    //doc->openDocument( url);
    setCaption (i18n ("VCD"), false);
    slotStatusMsg (i18n ("Ready."));
}

void KMPlayerApp::playDVD () {
    if (!m_opendvd || m_player->playing ())
        return;
    QString args;
    unsigned i;
    args.sprintf ("-quiet -slave");
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

void KMPlayerApp::playVCD () {
    if (!m_openvcd || m_player->playing ())
        return;
    QString args;
    unsigned i;
    args.sprintf ("-quiet -slave");
    for (i = 0; i < m_vcdtrackmenu->count(); i++)
        if (m_vcdtrackmenu->isItemChecked (i)) {
            args += " -vcd " + m_vcdtrackmenu->findItem (i)->text ();
            break;
        }
    if (i == m_vcdtrackmenu->count())
        args += " vcd:// ";
    //if (m_player->configDialog ()->dvddevice.length () > 0)
      //  args += QString(" -dvd-device ") + m_player->configDialog ()->dvddevice;
    m_player->run (args.ascii());
}

void KMPlayerApp::play () {
    if ((!m_opendvd && !m_openvcd && !m_openpipe) || m_player->playing ())
        m_player->play ();
    else if (m_opendvd)
        playDVD ();
    else if (m_openvcd)
        playVCD ();
    else if (m_openpipe)
        playPipe ();
}

void KMPlayerApp::playDisc () {
    if (m_opendvd)
        playDVD ();
    else if (m_openvcd)
        playVCD ();
    else if (m_openpipe)
        playPipe ();
}

void KMPlayerApp::menuItemClicked (QPopupMenu * menu, int id) {
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
    if (m_opendvd && player ()->configDialog ()->playdvd) {
        player ()->stop ();
        playDVD ();
    } else if (m_openvcd && player ()->configDialog ()->playvcd) {
        player ()->stop ();
        playVCD ();
    }
}

void KMPlayerApp::titleMenuClicked (int id) {
    menuItemClicked (m_dvdtitlemenu, id);
}

void KMPlayerApp::subtitleMenuClicked (int id) {
    menuItemClicked (m_dvdsubtitlemenu, id);
}

void KMPlayerApp::languageMenuClicked (int id) {
    menuItemClicked (m_dvdlanguagemenu, id);
}

void KMPlayerApp::chapterMenuClicked (int id) {
    menuItemClicked (m_dvdchaptermenu, id);
}

void KMPlayerApp::trackMenuClicked (int id) {
    menuItemClicked (m_vcdtrackmenu, id);
}

KMPlayerDoc *KMPlayerApp::getDocument () const
{
    return doc;
}


void KMPlayerApp::saveOptions()
{	
    config->setGroup ("General Options");
    config->writeEntry ("Geometry", size());
    //config->writeEntry ("Show Toolbar", viewToolBar->isChecked());
    config->writeEntry ("Show Statusbar",viewStatusBar->isChecked());
    if (!m_pipe.isEmpty ()) {
        config->setGroup ("Pipe Command");
        config->writeEntry ("Command1", m_pipe);
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

    config->setGroup ("Pipe Command");
    m_pipe = config->readEntry ("Command1", "");

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

void KMPlayerApp::saveProperties(KConfig *_cfg) {
    if(doc->URL().fileName()!=i18n("Untitled") && !doc->isModified())
    {
        // saving to tempfile not necessary

    }
    else
    {
        KURL url=doc->URL();	
        _cfg->writeEntry("filename", url.url());
        _cfg->writeEntry("modified", doc->isModified());
        QString tempname = kapp->tempSaveName(url.url());
        QString tempurl= KURL::encode_string(tempname);
    }
}


void KMPlayerApp::readProperties(KConfig* _cfg)
{
    QString filename = _cfg->readEntry("filename", "");
    KURL url(filename);
    bool modified = _cfg->readBoolEntry("modified", false);
    if(modified)
    {
        bool canRecover;
        QString tempname = kapp->checkRecoverFile(filename, canRecover);
        KURL _url(tempname);

        if(canRecover)
        {
            doc->openDocument(_url);
            doc->setModified();
            setCaption(_url.fileName(),true);
            QFile::remove(tempname);
        }
    }
    else
    {
        if(!filename.isEmpty())
        {
            doc->openDocument(url);
            setCaption(url.fileName(),false);
        }
    }
}		

bool KMPlayerApp::queryClose ()
{
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

void KMPlayerApp::slotFileNew()
{
    slotStatusMsg(i18n("Creating new document..."));

    doc->newDocument();		
    setCaption(doc->URL().fileName(), false);

    slotStatusMsg(i18n("Ready."));
}

void KMPlayerApp::slotFileOpen()
{
    slotStatusMsg(i18n("Opening file..."));

    KURL url=KFileDialog::getOpenURL(QString::null,
            i18n("*|All files"), this, i18n("Open File..."));
    if(!url.isEmpty())
    {
        //doc->openDocument(url);
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


void KMPlayerApp::slotStatusMsg(const QString &text) {
    statusBar()->clear();
    statusBar()->changeItem(text, ID_STATUS_MSG);
}

void KMPlayerApp::fullScreen () {
    m_fullscreen = !m_fullscreen;
    if (m_fullscreen) {
        showFullScreen ();
        menuBar ()->hide ();
        statusBar()->hide();
        //toolBar("mainToolBar")->hide();
        m_sreensaverdisabled = false;
        QByteArray data, replydata;
        QCString replyType;
        if (kapp->dcopClient ()->call ("kdesktop", "KScreensaverIface",
                             "isEnabled()", data, replyType, replydata)) {
            bool enabled;
            QDataStream replystream (replydata, IO_ReadOnly);
            replystream >> enabled;
            if (enabled)
                m_sreensaverdisabled = kapp->dcopClient()->send
                    ("kdesktop", "KScreensaverIface", "enable(bool)", "false");
        }
    } else {
        showNormal ();
        menuBar ()->show ();
        //if (m_showToolbar) toolBar("mainToolBar")->show();
        if (m_showStatusbar) statusBar()->show();
        if (m_sreensaverdisabled)
            m_sreensaverdisabled = !kapp->dcopClient()->send
                ("kdesktop", "KScreensaverIface", "enable(bool)", "true");
    }
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
    view->viewer ()->setAspect (view->keepSizeRatio () ? doc->aspect () : 0.0);
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
#include "kmplayer.moc"
