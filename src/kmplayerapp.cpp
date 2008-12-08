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
#include <qpushbutton.h>
#include <qkeysequence.h>
#include <qapplication.h>
#include <qslider.h>
#include <qlayout.h>
#include <qwhatsthis.h>
#include <qtimer.h>
#include <qfile.h>
#include <qmetaobject.h>
#include <QLabel>
#include <Q3TextDrag>
#include <QDockWidget>

// include files for KDE
#include <kapplication.h>
#include <kdeversion.h>
#include <kstandarddirs.h>
#include <kiconloader.h>
#include <kmessagebox.h>
#include <kfiledialog.h>
#include <kinputdialog.h>
#include <kmenubar.h>
#include <kstatusbar.h>
#include <ktoolbar.h>
#include <klocale.h>
#include <kconfig.h>
#include <kstandardaction.h>
#include <kactioncollection.h>
#include <kdebug.h>
#include <kmenu.h>
#include <kurlrequester.h>
#include <klineedit.h>
#include <kshortcutsdialog.h>
#include <ksystemtrayicon.h>
#include <kedittoolbar.h>
#include <krecentfilesaction.h>
#include <ktoggleaction.h>

// application specific includes
#include "kmplayer_def.h"
#include "kmplayerconfig.h"
#include "kmplayer.h"
#include "kmplayerview.h"
#include "playlistview.h"
#include "viewarea.h"
#include "kmplayercontrolpanel.h"
#include "kmplayerpartbase.h"
#include "kmplayerprocess.h"
#include "kmplayerappsource.h"
#include "kmplayertvsource.h"
//#include "kmplayerbroadcast.h"
//#include "kmplayervdr.h"
#include "kmplayerconfig.h"

static const int DVDNav_start = 1;
static const int DVDNav_previous = 2;
static const int DVDNav_next = 3;
static const int DVDNav_root = 4;
static const int DVDNav_up = 5;

extern const char * strMPlayerGroup;

static const short id_node_recent_document = 31;
static const short id_node_recent_node = 32;
static const short id_node_disk_document = 33;
static const short id_node_disk_node = 34;


class KMPLAYER_NO_EXPORT ListsSource : public KMPlayer::URLSource {
public:
    KDE_NO_CDTOR_EXPORT ListsSource (KMPlayer::PartBase * p)
        : KMPlayer::URLSource (p, KUrl ("lists://")) {}
    void play (KMPlayer::Mrl *);
    void activate ();
    QString prettyName () { return m_document->caption (); }
};

class KMPLAYER_NO_EXPORT Recents : public FileDocument {
public:
    Recents (KMPlayerApp *a);
    void defer ();
    void activate ();
    void *message (KMPlayer::MessageType msg, void *content=NULL);
    KMPlayer::NodePtr childFromTag (const QString & tag);
    KDE_NO_EXPORT const char * nodeName () const { return "playlist"; }
    KMPlayerApp * app;
};

class KMPLAYER_NO_EXPORT Recent : public KMPlayer::Mrl {
public:
    Recent (KMPlayer::NodePtr & doc, KMPlayerApp *a, const QString &url = QString());
    void activate ();
    void closed ();
    KDE_NO_EXPORT const char * nodeName () const { return "item"; }
    KMPlayerApp * app;
};

class KMPLAYER_NO_EXPORT Group : public KMPlayer::Title {
public:
    Group (KMPlayer::NodePtr &doc, KMPlayerApp *a, const QString &pn=QString());
    KMPlayer::NodePtr childFromTag (const QString & tag);
    void defer () {} // TODO lazy loading of largish sub trees
    void closed ();
    KDE_NO_EXPORT const char * nodeName () const { return "group"; }
    KMPlayerApp * app;
};

class KMPLAYER_NO_EXPORT Playlist : public FileDocument {
public:
    Playlist (KMPlayerApp *a, KMPlayer::Source *s, bool plmod = false);
    void *message (KMPlayer::MessageType msg, void *content=NULL);
    void defer ();
    void activate ();
    KMPlayer::NodePtr childFromTag (const QString & tag);
    KDE_NO_EXPORT const char * nodeName () const { return "playlist"; }
    KMPlayerApp * app;
    bool playmode;
};

class KMPLAYER_NO_EXPORT PlaylistItemBase : public KMPlayer::Mrl {
public:
    PlaylistItemBase (KMPlayer::NodePtr &d, short id, KMPlayerApp *a, bool pm);
    void activate ();
    void closed ();
    KMPlayerApp * app;
    bool playmode;
};

class KMPLAYER_NO_EXPORT PlaylistItem : public PlaylistItemBase {
public:
    PlaylistItem (KMPlayer::NodePtr & doc, KMPlayerApp *a, bool playmode, const QString &url = QString());
    void closed ();
    void begin ();
    void setNodeName (const QString &);
    const char * nodeName () const KDE_NO_EXPORT { return "item"; }
};

class KMPLAYER_NO_EXPORT PlaylistGroup : public KMPlayer::Title {
public:
    PlaylistGroup (KMPlayer::NodePtr &doc, KMPlayerApp *a, const QString &pn);
    PlaylistGroup (KMPlayer::NodePtr &doc, KMPlayerApp *a, bool plmode=false);
    KMPlayer::NodePtr childFromTag (const QString & tag);
    void closed ();
    void setNodeName (const QString &);
    KDE_NO_EXPORT const char * nodeName () const { return "group"; }
    KMPlayerApp * app;
    bool playmode;
};

class KMPLAYER_NO_EXPORT HtmlObject : public PlaylistItemBase {
public:
    HtmlObject (KMPlayer::NodePtr & doc, KMPlayerApp *a, bool playmode);
    void activate ();
    void closed ();
    KMPlayer::NodePtr childFromTag (const QString & tag);
    const char * nodeName () const KDE_NO_EXPORT { return "object"; }
};

KDE_NO_EXPORT void ListsSource::play (KMPlayer::Mrl *mrl) {
    if (m_player->source () == this)
        Source::play (mrl);
    else if (mrl)
        mrl->activate ();
}

KDE_NO_EXPORT void ListsSource::activate () {
    play (m_current ? m_current->mrl () : NULL);
}

KDE_NO_CDTOR_EXPORT FileDocument::FileDocument (short i, const QString &s, KMPlayer::Source *src)
 : KMPlayer::SourceDocument (src, s) {
    id = i;
}

KDE_NO_EXPORT KMPlayer::NodePtr FileDocument::childFromTag(const QString &tag) {
    if (tag == QString::fromLatin1 (nodeName ()))
        return this;
    return 0L;
}

void FileDocument::readFromFile (const QString & fn) {
    QFile file (fn);
    kDebug () << "readFromFile " << fn;
    if (file.exists ()) {
        file.open (IO_ReadOnly);
        QTextStream inxml (&file);
        KMPlayer::readXML (this, inxml, QString (), false);
        normalize ();
    }
}

void FileDocument::writeToFile (const QString & fn) {
    QFile file (fn);
    kDebug () << "writeToFile " << fn;
    file.open (IO_WriteOnly);
    QByteArray utf = outerXML ().toUtf8 ();
    file.writeBlock (utf, utf.length ());
}

KDE_NO_CDTOR_EXPORT Recents::Recents (KMPlayerApp *a)
    : FileDocument (id_node_recent_document, "recents://"),
      app(a) {
    title = i18n ("Most Recent");
}

KDE_NO_EXPORT void Recents::activate () {
    if (!resolved)
        defer ();
}

KDE_NO_EXPORT void Recents::defer () {
    if (!resolved) {
        resolved = true;
        readFromFile (KStandardDirs::locateLocal ("data", "kmplayer/recent.xml"));
    }
}

KDE_NO_EXPORT KMPlayer::NodePtr Recents::childFromTag (const QString & tag) {
    // kDebug () << nodeName () << " childFromTag " << tag;
    if (tag == QString::fromLatin1 ("item"))
        return new Recent (m_doc, app);
    else if (tag == QString::fromLatin1 ("group"))
        return new Group (m_doc, app);
    return FileDocument::childFromTag (tag);
}

KDE_NO_EXPORT void *Recents::message (KMPlayer::MessageType msg, void *data) {
    if (KMPlayer::MsgChildFinished) {
        finish ();
        return NULL;
    }
    return FileDocument::message (msg, data);
}

KDE_NO_CDTOR_EXPORT
Recent::Recent (KMPlayer::NodePtr & doc, KMPlayerApp * a, const QString &url)
  : KMPlayer::Mrl (doc, id_node_recent_node), app (a) {
    src = url;
    setAttribute (KMPlayer::StringPool::attr_url, url);
}

KDE_NO_EXPORT void Recent::closed () {
    if (src.isEmpty ())
        src = getAttribute (KMPlayer::StringPool::attr_url);
}

KDE_NO_EXPORT void Recent::activate () {
    app->openDocumentFile (KUrl (src));
}

KDE_NO_CDTOR_EXPORT
Group::Group (KMPlayer::NodePtr & doc, KMPlayerApp * a, const QString & pn)
  : KMPlayer::Title (doc, KMPlayer::id_node_group_node), app (a) {
    title = pn;
    if (!pn.isEmpty ())
        setAttribute (KMPlayer::StringPool::attr_title, pn);
}

KDE_NO_EXPORT KMPlayer::NodePtr Group::childFromTag (const QString & tag) {
    if (tag == QString::fromLatin1 ("item"))
        return new Recent (m_doc, app);
    else if (tag == QString::fromLatin1 ("group"))
        return new Group (m_doc, app);
    return 0L;
}

KDE_NO_EXPORT void Group::closed () {
    if (title.isEmpty ())
        title = getAttribute (KMPlayer::StringPool::attr_title);
}

KDE_NO_EXPORT void Playlist::defer () {
    if (playmode)
        KMPlayer::Document::defer ();
    else if (!resolved) {
        resolved = true;
        readFromFile (KStandardDirs::locateLocal ("data", "kmplayer/playlist.xml"));
    }
}

KDE_NO_EXPORT void Playlist::activate () {
    if (playmode)
        KMPlayer::Document::activate ();
    else if (!resolved)
        defer ();
}

KDE_NO_CDTOR_EXPORT Playlist::Playlist (KMPlayerApp *a, KMPlayer::Source *s, bool plmode)
    : FileDocument (KMPlayer::id_node_playlist_document, "Playlist://", s),
      app(a),
      playmode (plmode) {
    title = i18n ("Persistent Playlists");
}

KDE_NO_EXPORT KMPlayer::NodePtr Playlist::childFromTag (const QString & tag) {
    // kDebug () << nodeName () << " childFromTag " << tag;
    const char * name = tag.ascii ();
    if (!strcmp (name, "item"))
        return new PlaylistItem (m_doc, app, playmode);
    else if (!strcmp (name, "group"))
        return new PlaylistGroup (m_doc, app, playmode);
    else if (!strcmp (name, "object"))
        return new HtmlObject (m_doc, app, playmode);
    return FileDocument::childFromTag (tag);
}

KDE_NO_EXPORT void *Playlist::message (KMPlayer::MessageType msg, void *data) {
    if (KMPlayer::MsgChildFinished && !playmode) {
        finish ();
        return NULL;
    }
    return FileDocument::message (msg, data);
}

KDE_NO_CDTOR_EXPORT
PlaylistItemBase::PlaylistItemBase (KMPlayer::NodePtr &d, short i, KMPlayerApp *a, bool pm)
    : KMPlayer::Mrl (d, i), app (a), playmode (pm) {
}

KDE_NO_EXPORT void PlaylistItemBase::activate () {
    if (playmode) {
        Mrl::activate ();
    } else {
        ListsSource * source = static_cast <ListsSource *> (app->player ()->sources () ["listssource"]);
        KMPlayer::NodePtr pl = new Playlist (app, source, true);
        QString data;
        QString pn;
        if (parentNode ()->id == KMPlayer::id_node_group_node) {
            data = QString ("<playlist>") +
                parentNode ()->innerXML () +
                QString ("</playlist>");
            pn = parentNode ()->caption ();
        } else {
            data = outerXML ();
            pn = title.isEmpty () ? src : title;
        }
        pl->setCaption (pn);
        //kDebug () << "cloning to " << data;
        QTextStream inxml (&data, QIODevice::ReadOnly);
        KMPlayer::readXML (pl, inxml, QString (), false);
        pl->normalize ();
        KMPlayer::NodePtr cur = pl->firstChild ();
        pl->mrl ()->resolved = !!cur;
        if (parentNode ()->id == KMPlayer::id_node_group_node && cur) {
            KMPlayer::NodePtr sister = parentNode ()->firstChild ();
            while (sister && cur && sister.ptr () != this) {
                sister = sister->nextSibling ();
                cur = cur->nextSibling ();
            }
        }
        bool reset_only = source == app->player ()->source ();
        if (reset_only)
            app->player ()->stop ();
        source->setDocument (pl, cur);
        if (reset_only) {
            source->activate ();
            app->setCaption (pn);
        } else
            app->player ()->setSource (source);
    }
}

void PlaylistItemBase::closed () {
    if (title.isEmpty ())
        title = getAttribute (KMPlayer::StringPool::attr_title);
}

KDE_NO_CDTOR_EXPORT
PlaylistItem::PlaylistItem (KMPlayer::NodePtr & doc, KMPlayerApp *a, bool pm, const QString &url)
 : PlaylistItemBase (doc, KMPlayer::id_node_playlist_item, a, pm) {
    src = url;
    setAttribute (KMPlayer::StringPool::attr_url, url);
}

KDE_NO_EXPORT void PlaylistItem::closed () {
    if (src.isEmpty ())
        src = getAttribute (KMPlayer::StringPool::attr_url);
    PlaylistItemBase::closed ();
}

KDE_NO_EXPORT void PlaylistItem::begin () {
    if (playmode && firstChild ())
        firstChild ()->activate ();
    else
        Mrl::begin ();
}

KDE_NO_EXPORT void PlaylistItem::setNodeName (const QString & s) {
    src = s;
    setAttribute (KMPlayer::StringPool::attr_url, s);
}

KDE_NO_CDTOR_EXPORT
PlaylistGroup::PlaylistGroup (KMPlayer::NodePtr &doc, KMPlayerApp *a, const QString &pn)
  : KMPlayer::Title (doc, KMPlayer::id_node_group_node), app (a), playmode (false) {
    title = pn;
    if (!pn.isEmpty ())
        setAttribute (KMPlayer::StringPool::attr_title, pn);
}

KDE_NO_CDTOR_EXPORT
PlaylistGroup::PlaylistGroup (KMPlayer::NodePtr &doc, KMPlayerApp *a, bool lm)
  : KMPlayer::Title (doc, KMPlayer::id_node_group_node), app (a), playmode (lm) {
}

KDE_NO_EXPORT KMPlayer::NodePtr PlaylistGroup::childFromTag (const QString &tag) {
    const char * name = tag.ascii ();
    if (!strcmp (name, "item"))
        return new PlaylistItem (m_doc, app, playmode);
    else if (!strcmp (name, "group"))
        return new PlaylistGroup (m_doc, app, playmode);
    else if (!strcmp (name, "object"))
        return new HtmlObject (m_doc, app, playmode);
    return 0L;
}

KDE_NO_EXPORT void PlaylistGroup::closed () {
    if (title.isEmpty ())
        title = getAttribute (KMPlayer::StringPool::attr_title);
}

KDE_NO_EXPORT void PlaylistGroup::setNodeName (const QString &t) {
    title = t;
    setAttribute (KMPlayer::StringPool::attr_title, t);
}

KDE_NO_CDTOR_EXPORT
HtmlObject::HtmlObject (KMPlayer::NodePtr &doc, KMPlayerApp *a, bool pm)
  : PlaylistItemBase (doc, KMPlayer::id_node_html_object, a, pm) {}

KDE_NO_EXPORT void HtmlObject::activate () {
    if (playmode)
        KMPlayer::Mrl::activate ();
    else
        PlaylistItemBase::activate ();
}

KDE_NO_EXPORT void HtmlObject::closed () {
    for (Node *n = firstChild ().ptr (); n; n = n->nextSibling ().ptr ()) {
        if (n->id == KMPlayer::id_node_param) {
            KMPlayer::Element *e = static_cast <KMPlayer::Element *> (n);
            QString name = e->getAttribute (KMPlayer::StringPool::attr_name);
            if (name == "type")
                mimetype = e->getAttribute (KMPlayer::StringPool::attr_value);
            else if (name == "movie")
                src = e->getAttribute (KMPlayer::StringPool::attr_value);
        } else if (n->id == KMPlayer::id_node_html_embed) {
            KMPlayer::Element *e = static_cast <KMPlayer::Element *> (n);
            QString type = e->getAttribute (KMPlayer::StringPool::attr_type);
            if (!type.isEmpty ())
                mimetype = type;
            QString asrc = e->getAttribute (KMPlayer::StringPool::attr_src);
            if (!asrc.isEmpty ())
                src = asrc;
        }
    }
    PlaylistItemBase::closed ();
}

KDE_NO_EXPORT KMPlayer::NodePtr HtmlObject::childFromTag (const QString & tag) {
    const char *name = tag.ascii ();
    if (!strcasecmp (name, "param"))
        return new KMPlayer::DarkNode (m_doc, name, KMPlayer::id_node_param);
    else if (!strcasecmp (name, "embed"))
        return new KMPlayer::DarkNode(m_doc, name,KMPlayer::id_node_html_embed);
    return NULL;
}

KDE_NO_CDTOR_EXPORT KMPlayerApp::KMPlayerApp (QWidget *)
    : KXmlGuiWindow (NULL),
      m_systray (0L),
      m_player (new KMPlayer::PartBase (this, 0L, KGlobal::config ())),
      m_view (static_cast <KMPlayer::View*> (m_player->view())),
      m_dvdmenu (new QMenu (this)),
      m_dvdnavmenu (new QMenu (this)),
      m_vcdmenu (new QMenu (this)),
      m_audiocdmenu (new QMenu (this)),
      m_tvmenu (new QMenu (this)),
      //m_ffserverconfig (new KMPlayerFFServerConfig),
      //m_broadcastconfig (new KMPlayerBroadcastConfig (m_player, m_ffserverconfig)),
      edit_tree_id (-1),
      last_time_left (0),
      m_played_intro (false),
      m_played_exit (false),
      m_minimal_mode (false)
{
    setCentralWidget (m_view);
    //connect (m_broadcastconfig, SIGNAL (broadcastStarted()), this, SLOT (broadcastStarted()));
    //connect (m_broadcastconfig, SIGNAL (broadcastStopped()), this, SLOT (broadcastStopped()));
    initStatusBar();
#ifdef KMPLAYER_WITH_DBUS
    m_player->setServiceName (QString ("org.kde.kmplayer-%1").arg (getpid ()));
#endif
    m_player->init (actionCollection ());
    m_view->initDock (m_view->viewArea ());
    //m_player->mediaManager ()->processInfos () ["xvideo"] =
    //    new XvProcessInfo (m_player->mediaManager ());
    ListsSource * lstsrc = new ListsSource (m_player);
    m_player->sources () ["listssource"] = lstsrc;
    m_player->sources () ["dvdsource"] = new ::KMPlayerDVDSource(this, m_dvdmenu);
    m_player->sources () ["dvdnavsource"] = new KMPlayerDVDNavSource (this, m_dvdnavmenu);
    m_player->sources () ["vcdsource"] = new KMPlayerVCDSource(this, m_vcdmenu);
    m_player->sources () ["audiocdsource"] = new KMPlayerAudioCDSource (this, m_audiocdmenu);
    m_player->sources () ["pipesource"] = new KMPlayerPipeSource (this);
    m_player->sources () ["tvsource"] = new KMPlayerTVSource (this, m_tvmenu);
    //m_player->sources () ["vdrsource"] = new KMPlayerVDRSource (this);
    m_player->setSource (m_player->sources () ["urlsource"]);
    initActions();
    initView();

    //setAutoSaveSettings();
    playlist = new Playlist (this, lstsrc);
    playlist_id = m_view->playList ()->addTree (playlist, "listssource", "view-media-playlist", KMPlayer::PlayListView::AllowDrag | KMPlayer::PlayListView::AllowDrops | KMPlayer::PlayListView::TreeEdit | KMPlayer::PlayListView::Moveable | KMPlayer::PlayListView::Deleteable);
    readOptions();
}

KDE_NO_CDTOR_EXPORT KMPlayerApp::~KMPlayerApp () {
    //delete m_broadcastconfig;
    if (recents)
        recents->document ()->dispose ();
    if (playlist)
        playlist->document ()->dispose ();
}


KDE_NO_EXPORT void KMPlayerApp::initActions () {
    KActionCollection * ac = actionCollection ();
    fileNewWindow = ac->addAction ("new_window");
    //fileNewWindow->setIcon (KIcon ("window-new"));
    connect (fileNewWindow, SIGNAL (triggered (bool)), this, SLOT (slotFileNewWindow ()));
    fileOpen = KStandardAction::open (this, SLOT (slotFileOpen()), ac);
    fileOpenRecent = KStandardAction::openRecent(this, SLOT(slotFileOpenRecent(const KUrl&)), ac);
    KStandardAction::saveAs (this, SLOT (slotSaveAs ()), ac);
    fileClose = KStandardAction::close (this, SLOT (slotFileClose ()), ac);
    fileQuit = KStandardAction::quit (this, SLOT (slotFileQuit ()), ac);
    viewEditMode = ac->addAction ("edit_mode");
    viewEditMode->setCheckable (true);
    viewEditMode->setText (i18n ("&Edit mode"));
    connect (viewEditMode, SIGNAL (triggered (bool)), this, SLOT (editMode ()));
    QAction *viewplaylist = ac->addAction ( "view_playlist");
    viewplaylist->setText (i18n ("Pla&y List"));
    //viewplaylist->setIcon (KIcon ("media-playlist"));
    connect (viewplaylist, SIGNAL(triggered(bool)), m_player, SLOT(showPlayListWindow()));
    KStandardAction::preferences (m_player, SLOT (showConfigDialog ()), ac);
    QAction *playmedia = ac->addAction ("play");
    playmedia->setText (i18n ("P&lay"));
    connect (playmedia, SIGNAL (triggered (bool)), m_player, SLOT (play ()));
    QAction *pausemedia = ac->addAction ("pause");
    pausemedia->setText (i18n ("&Pause"));
    connect (pausemedia, SIGNAL (triggered (bool)), m_player, SLOT (pause ()));
    QAction *stopmedia = ac->addAction ("stop");
    stopmedia->setText (i18n ("&Stop"));
    connect (stopmedia, SIGNAL (triggered (bool)), m_player, SLOT (stop ()));
    KStandardAction::keyBindings (this, SLOT (slotConfigureKeys()), ac);
    KStandardAction::configureToolbars (this, SLOT (slotConfigureToolbars ()), ac);
    viewFullscreen = ac->addAction ("view_fullscreen");
    viewFullscreen->setCheckable (true);
    viewFullscreen->setText (i18n("Fullscreen"));
    connect (viewFullscreen, SIGNAL (triggered (bool)), this, SLOT (fullScreen ()));
    toggleView = ac->addAction ("view_video");
    toggleView->setText (i18n ("C&onsole"));
    toggleView->setIcon (KIcon ("utilities-terminal"));
    connect (toggleView, SIGNAL (triggered (bool)),
            m_player->view (), SLOT (toggleVideoConsoleWindow ()));
    viewSyncEditMode = ac->addAction ("sync_edit_mode");
    viewSyncEditMode->setText (i18n ("Reload"));
    viewSyncEditMode->setIcon (KIcon ("view-refresh"));
    connect (viewSyncEditMode, SIGNAL (triggered (bool)), this, SLOT (syncEditMode ()));
    viewSyncEditMode->setEnabled (false);
    viewToolBar = KStandardAction::create (KStandardAction::ShowToolbar,
            this, SLOT (slotViewToolBar ()), ac);
    viewStatusBar = KStandardAction::create (KStandardAction::ShowStatusbar,
            this,SLOT (slotViewStatusBar ()),ac);
    viewMenuBar = KStandardAction::create (KStandardAction::ShowMenubar,
            this, SLOT (slotViewMenuBar ()), ac);

    /*fileNewWindow = new KAction(i18n("New &Window"), 0, 0, this, SLOT(slotFileNewWindow()), ac, "new_window");
    new KAction (i18n ("Clear &History"), 0, 0, this, SLOT (slotClearHistory ()), ac, "clear_history");
    new KAction (i18n ("&Open DVD"), QString ("dvd_mount"), KShortcut (), this, SLOT(openDVD ()), ac, "opendvd");
    new KAction (i18n ("&Open VCD"), QString ("cdrom_mount"), KShortcut (), this, SLOT(openVCD ()), ac, "openvcd");
    new KAction (i18n ("&Open Audio CD"), QString ("cdrom_mount"), KShortcut (), this, SLOT(openAudioCD ()), ac, "openaudiocd");
    new KAction (i18n ("&Open Pipe..."), QString ("pipe"), KShortcut (), this, SLOT(openPipe ()), ac, "source_pipe");
    //KIconLoader::global ()->loadIconSet (QString ("video-television"), K3Icon::Small, 0,true)
    new KAction (i18n ("&Connect"), QString ("connect_established"), KShortcut (), this, SLOT (openVDR ()), ac, "vdr_connect");
    editVolumeInc = new KAction (i18n ("Increase Volume"), QString ("player_volume"), KShortcut (), m_player, SLOT (increaseVolume ()), ac, "edit_volume_up");
    editVolumeDec = new KAction (i18n ("Decrease Volume"), QString ("player_volume"), KShortcut (), m_player, SLOT(decreaseVolume ()), ac, "edit_volume_down");
    //new KAction (i18n ("V&ideo"), QString ("video"), KShortcut (), m_view, SLOT (toggleVideoConsoleWindow ()), ac, "view_video");
    new KAction (i18n ("Pla&y List"), QString ("player_playlist"), KShortcut (), m_player, SLOT (showPlayListWindow ()), ac, "view_playlist");
    new KAction (i18n ("Minimal mode"), QString ("empty"), KShortcut (), this, SLOT (slotMinimalMode ()), ac, "view_minimal");
    new KAction (i18n ("50%"), 0, 0, this, SLOT (zoom50 ()), ac, "view_zoom_50");
    new KAction (i18n ("100%"), QString ("viewmagfit"), KShortcut (), this, SLOT (zoom100 ()), ac, "view_zoom_100");
    new KAction (i18n ("150%"), 0, 0, this, SLOT (zoom150 ()), ac, "view_zoom_150");
    new KAction (i18n ("Show Popup Menu"), KShortcut (), m_view->controlPanel (), SLOT (showPopupMenu ()), ac, "view_show_popup_menu");
    new KAction (i18n ("Show Language Menu"), KShortcut (Qt::Key_L), m_view->controlPanel (), SLOT (showLanguageMenu ()), ac, "view_show_lang_menu");
    viewKeepRatio = new KToggleAction (i18n ("&Keep Width/Height Ratio"), 0, this, SLOT (keepSizeRatio ()), ac, "view_keep_ratio");
#if KDE_IS_VERSION(3,1,90)
#else
    viewFullscreen = new KAction (i18n("&Full Screen"), 0, 0, this, SLOT(fullScreen ()), ac, "view_fullscreen");
#endif
    fileNewWindow->setStatusText(i18n("Opens a new application window"));
    fileOpen->setStatusText(i18n("Opens an existing file"));
    fileOpenRecent->setStatusText(i18n("Opens a recently used file"));
    fileClose->setStatusText(i18n("Closes the actual source"));
    fileQuit->setStatusText(i18n("Quits the application"));*/
    viewStatusBar->setStatusTip (i18n ("Enables/disables the statusbar"));
    viewMenuBar->setStatusTip (i18n ("Enables/disables the menubar"));
    viewToolBar->setStatusTip (i18n ("Enables/disables the toolbar"));
}

KDE_NO_EXPORT void KMPlayerApp::initStatusBar () {
    KStatusBar *sb = statusBar ();
    sb->insertItem (i18n ("Ready."), id_status_msg);
    sb->setItemAlignment (id_status_msg, Qt::AlignLeft);
    sb->insertItem (QString ("--:--"), id_status_timer, 0);
    sb->setItemAlignment (id_status_timer, Qt::AlignRight);
}

KDE_NO_EXPORT void KMPlayerApp::initMenu () {
    createGUI ("kmplayerui.rc"); // first build the one from the kmplayerui.rc

    //QAction *bookmark_action = actionCollection ()->addAction ("bookmarks");
    QList<QAction *> acts = menuBar()->actions();
    if (acts.size () > 2) {
        KMenu *bookmark_menu = new KMenu (this);
        QAction *bookmark_action = menuBar()->insertMenu (acts.at(2), bookmark_menu);
        bookmark_action->setText (i18n( "&Bookmarks"));
        m_player->createBookmarkMenu (bookmark_menu, actionCollection ());
    }

    /*QMenu *bookmarkmenu = m_view->controlPanel()->bookmarkMenu;
    m_view->controlPanel()->bookmarkAction->setVisible (false);
    menuBar ()->insertItem (i18n ("&Bookmarks"), bookmarkmenu, -1, 2);
    m_sourcemenu = menuBar ()->findItem (menuBar ()->idAt (0));
    m_sourcemenu->setText (i18n ("S&ource"));
    m_sourcemenu->popup ()->insertItem (KIconLoader::global ()->loadIconSet (QString ("dvd_mount"), K3Icon::Small, 0, true), i18n ("&DVD"), m_dvdmenu, -1, 5);
    m_dvdmenu->clear ();
#ifdef KMPLAYER_WITH_XINE
    m_dvdnavmenu->clear ();
    m_dvdnavmenu->insertItem (i18n ("&Start"), this, SLOT (dvdNav ()));
    m_dvdmenu->insertItem (i18n ("&DVD Navigator"), m_dvdnavmenu, -1, 1);
    m_dvdmenu->insertItem (i18n ("&Open DVD"), this, SLOT(openDVD ()), 0,-1, 2);
#else
    m_dvdmenu->insertItem (i18n ("&Open DVD"), this, SLOT(openDVD ()), 0,-1, 1);
#endif
    m_sourcemenu->popup ()->insertItem (KIconLoader::global ()->loadIconSet (QString ("cdrom_mount"), K3Icon::Small, 0, true), i18n ("V&CD"), m_vcdmenu, -1, 6);
    m_vcdmenu->clear ();
    m_sourcemenu->popup ()->insertItem (KIconLoader::global ()->loadIconSet (QString ("video-television"), K3Icon::Small, 0, true), i18n ("&TV"), m_tvmenu, -1, 8);
    m_vcdmenu->insertItem (i18n ("&Open VCD"), this, SLOT(openVCD ()), 0,-1, 1);
    m_sourcemenu->popup ()->insertItem (KIconLoader::global ()->loadIconSet (QString ("cdrom_mount"), K3Icon::Small, 0, true), i18n ("&Audio CD"), m_audiocdmenu, -1, 7);
    m_audiocdmenu->insertItem (i18n ("&Open Audio CD"), this, SLOT(openAudioCD ()), 0,-1, 1);*/
}

KDE_NO_EXPORT void KMPlayerApp::initView () {
    KSharedConfigPtr config = KGlobal::config ();
    //m_view->docArea ()->readDockConfig (config.data (), QString ("Window Layout"));
    m_player->connectPanel (m_view->controlPanel ());
    initMenu ();
    //new KAction (i18n ("Increase Volume"), editVolumeInc->shortcut (), m_player, SLOT (increaseVolume ()), m_view->viewArea ()->actionCollection (), "edit_volume_up");
    //new KAction (i18n ("Decrease Volume"), editVolumeDec->shortcut (), m_player, SLOT(decreaseVolume ()), m_view->viewArea ()->actionCollection (), "edit_volume_down");
    connect (m_player->settings (), SIGNAL (configChanged ()),
             this, SLOT (configChanged ()));
    connect (m_player, SIGNAL (loading (int)),
             this, SLOT (loadingProgress (int)));
    connect (m_player, SIGNAL (positioned (int, int)),
             this, SLOT (positioned (int, int)));
    connect (m_player, SIGNAL (statusUpdated (const QString &)),
             this, SLOT (slotStatusMsg (const QString &)));
    connect (m_view, SIGNAL (windowVideoConsoleToggled (bool)),
             this, SLOT (windowVideoConsoleToggled (bool)));
    connect (m_player, SIGNAL (sourceChanged (KMPlayer::Source *, KMPlayer::Source *)), this, SLOT (slotSourceChanged(KMPlayer::Source *, KMPlayer::Source *)));
    /*m_view->controlPanel ()->zoomMenu ()->connectItem (KMPlayer::ControlPanel::menu_zoom50,
            this, SLOT (zoom50 ()));
    m_view->controlPanel ()->zoomMenu ()->connectItem (KMPlayer::ControlPanel::menu_zoom100,
            this, SLOT (zoom100 ()));
    m_view->controlPanel ()->zoomMenu ()->connectItem (KMPlayer::ControlPanel::menu_zoom150,
            this, SLOT (zoom150 ()));
    connect (m_view->controlPanel()->broadcastButton (), SIGNAL (clicked ()),
            this, SLOT (broadcastClicked ()));*/
    m_auto_resize = m_player->settings ()->autoresize;
    if (m_auto_resize)
        connect (m_player, SIGNAL (sourceDimensionChanged ()),
                 this, SLOT (zoom100 ()));
    connect (m_view, SIGNAL (fullScreenChanged ()),
            this, SLOT (fullScreen ()));
    connect (m_view->playList (), SIGNAL (selectionChanged (Q3ListViewItem *)),
            this, SLOT (playListItemSelected (Q3ListViewItem *)));
    connect (m_view->playList(), SIGNAL (dropped (QDropEvent*, Q3ListViewItem*)),
            this, SLOT (playListItemDropped (QDropEvent *, Q3ListViewItem *)));
    connect (m_view->playList(), SIGNAL (moved ()),
            this, SLOT (playListItemMoved ()));
    connect (m_view->playList(), SIGNAL (prepareMenu (KMPlayer::PlayListItem *, QMenu *)), this, SLOT (preparePlaylistMenu (KMPlayer::PlayListItem *, QMenu *)));
    m_dropmenu = new QMenu (m_view->playList ());
    m_dropmenu->insertItem (KIcon ("view-media-playlist"),
                i18n ("&Add to list"), this, SLOT (menuDropInList ()), 0, 0);
    m_dropmenu->insertItem (KIcon ("folder-grey"),
        i18n ("Add in new &Group"), this, SLOT (menuDropInGroup ()), 0, 1);
    m_dropmenu->insertItem (KIcon ("edit-copy"),
            i18n ("&Copy here"), this, SLOT (menuCopyDrop ()), 0, 2);
    m_dropmenu->insertItem (KIcon ("edit-delete"),
            i18n ("&Delete"), this, SLOT (menuDeleteNode ()), 0, 3);
    /*QMenu * viewmenu = new QMenu;
    viewmenu->insertItem (i18n ("Full Screen"), this, SLOT(fullScreen ()),
                          QKeySequence ("CTRL + Key_F"));
    menuBar ()->insertItem (i18n ("&View"), viewmenu, -1, 2);*/
    //toolBar("mainToolBar")->hide();
    setAcceptDrops (true);
}

KDE_NO_EXPORT void KMPlayerApp::loadingProgress (int perc) {
    if (perc < 100)
        statusBar ()->changeItem (QString ("%1%").arg (perc), id_status_timer);
    else
        statusBar ()->changeItem (QString ("--:--"), id_status_timer);
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

KDE_NO_EXPORT void KMPlayerApp::windowVideoConsoleToggled (bool show) {
    if (show) {
        toggleView->setText (i18n ("V&ideo"));
        toggleView->setIcon (KIcon ("video-display"));
    } else {
        toggleView->setText (i18n ("C&onsole"));
        toggleView->setIcon (KIcon ("utilities-terminal"));
    }
}

KDE_NO_EXPORT void KMPlayerApp::playerStarted () {
    KMPlayer::Source * source = m_player->source ();
    if (!strcmp (source->name (), "urlsource")) {
        KUrl url = source->url ();
        if (url.isEmpty () || url.url ().startsWith ("lists"))
            return;
        //if (url.isEmpty () && m_player->process ()->mrl ())
        //    url = KUrl (m_player->process ()->mrl ()->mrl ()->src);
        recentFiles ()->addUrl (url);
        recents->defer (); // make sure it's loaded
        recents->insertBefore (new Recent (recents, this, url.url ()), recents->firstChild ());
        KMPlayer::NodePtr c = recents->firstChild ()->nextSibling ();
        int count = 1;
        KMPlayer::NodePtr more;
        while (c) {
            if (c->id == id_node_recent_node &&
                    c->mrl ()->src == url.url ()) {
                KMPlayer::NodePtr tmp = c->nextSibling ();
                recents->removeChild (c);
                c = tmp;
            } else {
                if (c->id == KMPlayer::id_node_group_node)
                    more = c;
                c = c->nextSibling ();
                count++;
            }
        }
        if (!more && count > 10) {
            more = new Group (recents, this, i18n ("More..."));
            recents->appendChild (more);
        }
        if (more) {
            KMPlayer::NodePtr item;
            if (count > 10) {
                KMPlayer::NodePtr item = more->previousSibling ();
                recents->removeChild (item);
                more->insertBefore (item, more->firstChild ());
            }
            if (more->firstChild ())
                c = more->firstChild ()->nextSibling ();
            count = 0;
            while (c) {
                if (c->id == id_node_recent_node &&
                         c->mrl ()->src == url.url ()) {
                    KMPlayer::NodePtr tmp = c->nextSibling ();
                    more->removeChild (c);
                    c = tmp;
                } else {
                    c = c->nextSibling ();
                    count++;
                }
            }
            if (count > 50)
                more->removeChild (more->lastChild ());
        }
        m_view->playList ()->updateTree (recents_id, recents, 0, false, false);
    }
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
        viewSyncEditMode->setEnabled (m_view->editMode () ||
                !strcmp (m_player->source ()->name (), "urlsource"));
    }
}

KDE_NO_EXPORT void KMPlayerApp::dvdNav () {
    slotStatusMsg(i18n("DVD Navigation..."));
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

KDE_NO_EXPORT void KMPlayerApp::openAudioCD () {
    slotStatusMsg(i18n("Opening Audio CD..."));
    m_player->setSource (m_player->sources () ["audiocdsource"]);
}

KDE_NO_EXPORT void KMPlayerApp::openPipe () {
    slotStatusMsg(i18n("Opening pipe..."));
    bool ok;
    QString cmd = KInputDialog::getText (i18n("Read From Pipe"),
      i18n ("Enter a command that will output an audio/video stream\nto the stdout. This will be piped to a player's stdin.\n\nCommand:"), m_player->sources () ["pipesource"]->pipeCmd (), &ok, m_player->view());
    if (!ok) {
        slotStatusMsg (i18n ("Ready."));
        return;
    }
    static_cast <KMPlayerPipeSource *> (m_player->sources () ["pipesource"])->setCommand (cmd);
    m_player->setSource (m_player->sources () ["pipesource"]);
}

KDE_NO_EXPORT void KMPlayerApp::openVDR () {
    /*slotStatusMsg(i18n("Opening VDR..."));
    if (!strcmp (m_player->source ()->name (), "vdrsource") && m_player->playing ())
        static_cast<KMPlayerVDRSource *>(m_player->source())->toggleConnected();
    else
        m_player->setSource (m_player->sources () ["vdrsource"]);*/
}

#ifdef KMPLAYER_WITH_CAIRO
static const char *svg_bat =
    "<svg width='64' height='64'>"
    "<path style='fill:#000000;'"
    " d='M 32.120,14.655"
    " C 31.374,14.777 30.356,14.660 30.073,14.204"
    " C 29.358,12.759 29.294,12.087 28.475,10.922"
    " C 27.216,9.132 29.242,23.435 25.250,22.485"
    " C 22.700,22.632 22.131,22.902 20.038,22.518"
    " C 14.017,21.412 11.310,19.129 17.209,12.808"
    " C 0.858,20.547 -1.279,37.053 14.151,48.311"
    " C 8.665,41.481 16.731,35.528 20.131,44.676"
    " C 26.243,38.164 29.892,38.528 32.120,50.180"
    " C 34.405,38.488 38.054,38.124 44.167,44.635"
    " C 47.567,35.487 55.633,41.441 50.146,48.271"
    " C 65.577,37.013 63.439,20.507 47.089,12.768"
    " C 52.987,19.089 50.281,21.372 44.260,22.477"
    " C 42.166,22.862 41.597,22.592 39.047,22.445"
    " C 35.056,23.395 37.070,9.162 35.806,10.949"
    " C 34.970,12.130 35.321,12.669 34.242,14.147"
    " C 33.975,14.570 32.841,14.787 31.374,14.777 z'/>"
    "</svg>";

static const char *svg_rat =
    "<svg width='64' height='64'>"
    "<path style='fill:#000000'"
    " d='M 37.966,10.702"
    " C 32.946,10.674 26.432,11.605 20.582,16.078"
    " C 19.781,16.691 20.028,14.288 19.307,14.084"
    " C 16.546,12.762 16.018,13.323 15.487,15.518"
    " C 15.440,15.712 14.021,15.893 13.561,15.455"
    " C 13.561,15.455 13.079,12.719 12.164,14.205"
    " C 11.787,14.818 11.688,13.574 10.500,15.472"
    " C 10.195,15.959 11.279,16.212 11.279,16.212"
    " C 11.279,16.212 7.825,19.421 7.869,20.140"
    " C 7.817,20.879 3.397,23.614 3.003,24.316"
    " C 2.285,25.598 5.593,27.321 9.036,26.688"
    " C 15.133,24.334 13.390,27.117 20.559,32.120"
    " C 20.559,32.120 20.490,33.826 20.491,34.474"
    " C 20.491,35.221 17.934,35.078 17.999,35.470"
    " C 18.077,35.949 17.590,35.794 17.702,36.316"
    " C 17.702,36.316 17.698,36.693 17.769,37.147"
    " C 17.685,37.334 19.341,36.803 19.472,37.475"
    " C 19.721,37.833 20.264,36.338 20.264,36.338"
    " C 20.544,35.759 21.054,35.192 21.890,36.278"
    " C 22.329,36.847 21.930,32.105 21.930,32.105"
    " C 21.930,32.105 27.859,33.031 27.478,34.460"
    " C 27.017,36.193 25.302,37.245 25.302,37.245"
    " C 23.675,37.429 23.683,37.998 23.440,38.876"
    " C 23.257,39.298 24.390,39.555 25.819,39.147"
    " C 26.419,38.871 26.508,39.644 26.726,39.392"
    " C 26.832,38.455 26.835,38.181 27.518,38.994"
    " C 27.263,36.857 29.360,35.554 29.435,35.442"
    " C 30.664,35.520 31.209,35.547 31.638,35.304"
    " C 32.010,34.551 33.203,34.079 35.435,33.716"
    " C 38.129,34.629 41.428,35.382 41.335,35.737"
    " C 41.206,36.225 36.891,35.726 37.019,37.589"
    " C 37.037,37.999 38.802,37.810 39.455,38.240"
    " C 39.756,38.296 41.215,37.707 42.661,36.513"
    " C 47.291,36.531 45.592,36.114 46.395,34.658"
    " C 46.623,34.246 54.256,35.533 55.788,33.790"
    " C 71.658,42.231 45.020,46.142 35.779,49.254"
    " C 31.072,50.987 13.462,51.472 12.112,52.822"
    " C 11.971,52.964 29.064,52.059 36.085,50.330"
    " C 79.669,41.497 56.851,34.139 56.427,30.933"
    " C 52.013,19.888 51.186,14.211 37.966,10.702 z'/>"
    "</svg>";

struct IntroSource : public KMPlayer::Source {
    KMPlayerApp * m_app;
    IntroSource (KMPlayer::PartBase *p, KMPlayerApp * a)
        : KMPlayer::Source (i18n ("Intro"), p, "introsource"), m_app (a) {}
    KDE_NO_EXPORT bool hasLength () { return false; }
    KDE_NO_EXPORT bool isSeekable () { return false; }
    KDE_NO_EXPORT QString prettyName () { return i18n ("Intro"); }
    void activate ();
    void deactivate ();
    void stateElementChanged (KMPlayer::Node * node, KMPlayer::Node::State os, KMPlayer::Node::State ns);
    bool deactivated;
    bool finished;
};

QString makeNumber (int i) {
    return QString (
            "<svg width='64' height='64'>"
            "<circle id='circle0' cx='32' cy='32' r='30' stroke='#B0B0B0'"
            "stroke-width='4' fill='#A0A0A0'/>"
            "<text x='15' y='50'"
            "font-family='Sans' font-size='55' fill='black'>%1</text>"
            "</svg>").arg (i);
}

KDE_NO_EXPORT void IntroSource::activate () {
    if (m_player->settings ()->autoresize)
        m_app->disconnect(m_player, SIGNAL(sourceDimensionChanged()),m_app,SLOT(zoom100()));
    m_document = new KMPlayer::SourceDocument (this, QString ());
    QString introfile = KStandardDirs::locate ("data", "kmplayer/intro.xml");
    QFile file (introfile);
    if (file.exists () && file.open (IO_ReadOnly)) {
        QTextStream ts (&file);
        KMPlayer::readXML (m_document, ts, QString (), false);
    } else {
        QString buf;
        QTextOStream out (&buf);
        out << "<smil><head><layout>"
            "<root-layout width='320' height='240' background-color='black'/>"
            "<region id='stage1' left='16' top='12' width='288' height='216'/>"
            "<region id='stage2' top='40' height='160'/>"
            "<region id='switch' top='30' width='20' height='20' right='20'/>"
            "<region id='reg' left='128' width='64' top='88' height='64' z-index='1'>"
            "<region id='icon' z-index='1'/>"
            "<region id='two' z-index='3'/>"
            "<region id='one' z-index='2'/>"
            "</region>"
            "<region id='spot' width='80' top='80' height='80'/>"
            "<region id='bat' left='208' width='64' top='48' height='64'/>"
            "<region id='rat' left='18' width='64' top='128' height='64'/>"
            "</layout>"
            "<transition id='clock1' dur='.3' type='clockWipe' direction='reverse'/>"
            "<transition id='fade1' dur='.1' type='fade'/>"
            "<transition id='fade2' dur='.2' type='fade'/>"
            "</head><body><excl><par>"
            "<brush region='stage1' dur='1' color='#303030'/>"
            "<img region='two' dur='.4' transIn='fade1' transOut='clock1'>" <<
            makeNumber (2) <<
            "</img><img region='switch' begin='0.08' dur='.1'>"
            "<svg width='20' height='20'>"
            "<path fill='white' d='M 2 2 L 18 2 L 9 12.7 z'/>"
            "</svg></img>"
            "<img region='one' begin='.1' dur='.8' transOut='clock1'>" <<
            makeNumber (1) <<
            "</img><img region='switch' begin='.7' dur='.1'>"
            "<svg width='20' height='20'>"
            "<circle fill='white' cx='9' cy='9' r='9'/>"
            "</svg></img>"
            "<brush region='stage2' begin='1.5' dur='.4' color='#101020'/>"
            "<img region='spot' begin='1' dur='.3' transIn='fade1' repeat='3'>"
            "<svg width='80' height='80'>"
            "<circle id='light' fill='red' cx='40' cy='40' r='40'/>"
            "</svg>"
            "</img>"
            "<img region='bat' begin='1' dur='.9'>" <<
            svg_bat <<
            "</img>"
            "<img region='rat' begin='1.2' dur='.7'>" <<
            svg_rat <<
            "</img>"
            "<animateMotion target='spot' begin='1' dur='.6' "
            "calcMode='discrete' values='200,40;10,120;120,80'/>"
            "<animate target='light' begin='1' dur='.9' calcMode='discrete'"
            "attribute='fill' values='#A04040;#40A040;#4040A0'/>"
            "<img region='icon' begin='1.5' dur='0.4' transIn='fade2' "
            "transOut='fade1' fit='meet' src='" <<
            KIconLoader::global()->iconPath (QString::fromLatin1 ("kmplayer"), -128) <<
            "'/></par><seq begin='stage1.activateEvent'/>"
            "</excl></body></smil>";

        QTextStream ts (&buf, QIODevice::ReadOnly);
        KMPlayer::readXML (m_document, ts, QString (), false);
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

KDE_NO_EXPORT void IntroSource::stateElementChanged (KMPlayer::Node * node, KMPlayer::Node::State, KMPlayer::Node::State new_state) {
    if (new_state == KMPlayer::Node::state_deactivated &&
            m_document == node) {
        m_document->reset ();
        finished = true;
        if (m_player->view ())
            m_app->restoreFromConfig ();
        emit stopPlaying ();
        if (!deactivated) // replace introsource with urlsource
            m_player->openUrl (KUrl ());
    }
}

KDE_NO_EXPORT void IntroSource::deactivate () {
    deactivated = true;
    if (m_player->settings ()->autoresize)
        m_app->connect(m_player, SIGNAL(sourceDimensionChanged()),m_app,SLOT(zoom100()));
    if (!finished && m_document) // user opens a source while introducing
        m_document->reset ();
}
#endif

KDE_NO_EXPORT void KMPlayerApp::restoreFromConfig () {
    if (m_player->view ()) {
        m_view->dockArea ()->hide ();
        KConfigGroup dock_cfg (KGlobal::config(), "Window Layout");
        m_view->dockArea ()->restoreState (dock_cfg.readEntry ("Layout", QByteArray ()));
        m_view->dockPlaylist ()->setVisible (dock_cfg.readEntry ("Show playlist", false));
        m_view->dockArea ()->show ();
        m_view->layout ()->activate ();
    }
}

KDE_NO_EXPORT void KMPlayerApp::openDocumentFile (const KUrl& url)
{
    if (!m_played_intro) {
        m_played_intro = true;
        KMPlayer::Source * src = m_player->sources () ["urlsource"];
        if (url.isEmpty () && src->document () &&
                src->document ()->hasChildNodes ()) {
            restoreFromConfig ();
            m_player->setSource (src);
            return;
#ifdef KMPLAYER_WITH_CAIRO
        } else if (!m_player->settings ()->no_intro && url.isEmpty ()) {
            m_player->setSource (new IntroSource (m_player, this));
            return;
#endif
        } else {
            m_played_exit = true; // no intro, so no exit as well
            restoreFromConfig ();
        }
    }
    slotStatusMsg(i18n("Opening file..."));
    m_player->openUrl (url);
    /*if (m_broadcastconfig->broadcasting () && url.url() == m_broadcastconfig->serverURL ()) {
        // speed up replay
        FFServerSetting & ffs = m_broadcastconfig->ffserversettings;
        KMPlayer::Source * source = m_player->source ();
        if (!ffs.width.isEmpty () && !ffs.height.isEmpty ()) {
            source->setWidth (ffs.width.toInt ());
            source->setHeight (ffs.height.toInt ());
        }
        source->setIdentified ();
    }*/
    slotStatusMsg (i18n ("Ready."));
}

KDE_NO_EXPORT void KMPlayerApp::addUrl (const KUrl& url) {
    KMPlayer::Source * src = m_player->sources () ["urlsource"];
    KMPlayer::NodePtr d = src->document ();
    if (d)
        d->appendChild (new KMPlayer::GenericURL (d, url.url ()));
}

KDE_NO_EXPORT void KMPlayerApp::saveProperties () {
    KConfigGroup def_cfg (KGlobal::config(), "<default>");
    def_cfg.writeEntry ("URL", m_player->source ()->url ().url ());
    def_cfg.writeEntry ("Visible", isVisible ());
}

KDE_NO_EXPORT void KMPlayerApp::readProperties () {
    KConfigGroup def_cfg (KGlobal::config(), "<default>");
    KUrl url (def_cfg.readEntry ("URL", QString ()));
    openDocumentFile (url);
    if (!def_cfg.readEntry ("Visible", true) && m_systray)
        hide ();
}

KDE_NO_EXPORT void KMPlayerApp::resizePlayer (int percentage) {
    /*KMPlayer::Source * source = m_player->source ();
    if (!source)
        return;
    int w, h;
    source->dimensions (w, h);
    if (w == 0 && h == 0) {
        w = 320;
        h = 240;
    } else
        h = m_view->viewer ()->heightForWidth (w);
    //kDebug () << "KMPlayerApp::resizePlayer (" << w << "," << h << ")";
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
    }*/
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

KDE_NO_EXPORT void KMPlayerApp::editMode () {
    //m_view->dockArea ()->hide ();
    bool editmode = !m_view->editMode ();
    KMPlayer::PlayListItem * pi = m_view->playList ()->currentPlayListItem ();
    if (!pi || !pi->node)
        editmode = false;
    //m_view->dockArea ()->show ();
    viewEditMode->setChecked (editmode);
    KMPlayer::RootPlayListItem * ri = (edit_tree_id > 0 && !editmode)
        ? m_view->playList ()->rootItem (edit_tree_id)
        : m_view->playList ()->rootItem (pi);
    if (editmode) {
        edit_tree_id = ri->id;
        m_view->setEditMode (ri, true);
        m_view->setInfoMessage (pi->node->innerXML ());
        viewSyncEditMode->setEnabled (true);
    } else {
        m_view->setEditMode (ri, false);
        edit_tree_id = -1;
        viewSyncEditMode->setEnabled (!strcmp (m_player->source()->name (), "urlsource"));
    }
}

KDE_NO_EXPORT void KMPlayerApp::syncEditMode () {
    if (edit_tree_id > -1) {
        KMPlayer::PlayListItem *si = m_view->playList()->selectedPlayListItem();
        if (si && si->node) {
            si->node->clearChildren ();
            QString txt = m_view->infoPanel ()->text ();
            QTextStream ts (&txt, QIODevice::ReadOnly);
            KMPlayer::readXML (si->node, ts, QString (), false);
            m_view->playList ()->updateTree (edit_tree_id, si->node->document(), si->node, true, false);
        }
    } else
        m_player->openUrl (m_player->source ()->url ());
}

KDE_NO_EXPORT void KMPlayerApp::showBroadcastConfig () {
    /*m_player->settings ()->addPage (m_broadcastconfig);
    m_player->settings ()->addPage (m_ffserverconfig);*/
}

KDE_NO_EXPORT void KMPlayerApp::hideBroadcastConfig () {
    /*m_player->settings ()->removePage (m_broadcastconfig);
    m_player->settings ()->removePage (m_ffserverconfig);*/
}

KDE_NO_EXPORT void KMPlayerApp::broadcastClicked () {
    /*if (m_broadcastconfig->broadcasting ())
        m_broadcastconfig->stopServer ();
    else {
        m_player->settings ()->show ("BroadcastPage");
        m_view->controlPanel()->broadcastButton ()->toggle ();
    }*/
}

KDE_NO_EXPORT void KMPlayerApp::broadcastStarted () {
    /*if (!m_view->controlPanel()->broadcastButton ()->isOn ())
        m_view->controlPanel()->broadcastButton ()->toggle ();*/
}

KDE_NO_EXPORT void KMPlayerApp::broadcastStopped () {
    /*if (m_view->controlPanel()->broadcastButton ()->isOn ())
        m_view->controlPanel()->broadcastButton ()->toggle ();
    if (m_player->source () != m_player->sources () ["tvsource"])
        m_view->controlPanel()->broadcastButton ()->hide ();
    setCursor (QCursor (Qt::ArrowCursor));*/
}

KDE_NO_EXPORT bool KMPlayerApp::broadcasting () const {
    //return m_broadcastconfig->broadcasting ();
    return false;
}

KDE_NO_EXPORT void KMPlayerApp::saveOptions()
{
    KSharedConfigPtr config = KGlobal::config ();
    KConfigGroup general (config, "General Options");
    if (m_player->settings ()->remembersize)
        general.writeEntry ("Geometry", size());
    general.writeEntry ("Show Toolbar", viewToolBar->isChecked());
    //general.writeEntry ("ToolBarPos", (int) toolBar("mainToolBar")->barPos());
    general.writeEntry ("Show Statusbar", viewStatusBar->isChecked ());
    general.writeEntry ("Show Menubar", viewMenuBar->isChecked ());
    if (!m_player->sources () ["pipesource"]->pipeCmd ().isEmpty ()) {
        KConfigGroup (config, "Pipe Command").writeEntry (
                "Command1", m_player->sources () ["pipesource"]->pipeCmd ());
    }
    m_view->setInfoMessage (QString ());
    KConfigGroup dock_cfg (KGlobal::config(), "Window Layout");
    dock_cfg.writeEntry ("Layout", m_view->dockArea ()->saveState ());
    dock_cfg.writeEntry ("Show playlist", m_view->dockPlaylist ()->isVisible ());
    Recents * rc = static_cast <Recents *> (recents.ptr ());
    if (rc && rc->resolved) {
        fileOpenRecent->saveEntries (KConfigGroup (config, "Recent Files"));
        rc->writeToFile (KStandardDirs::locateLocal ("data", "kmplayer/recent.xml"));
    }
    Playlist * pl = static_cast <Playlist *> (playlist.ptr ());
    if (pl && pl->resolved)
        pl->writeToFile (KStandardDirs::locateLocal ("data", "kmplayer/playlist.xml"));
}


KDE_NO_EXPORT void KMPlayerApp::readOptions() {
    KSharedConfigPtr config = KGlobal::config ();

    KConfigGroup gen_cfg (config, "General Options");

    // bar position settings
    //KToolBar::BarPosition toolBarPos;
    //toolBarPos=(KToolBar::BarPosition) gen_cfg.readNumEntry("ToolBarPos", KToolBar::Top);
    //toolBar("mainToolBar")->setBarPos(toolBarPos);

    // bar status settings
    viewToolBar->setChecked (gen_cfg.readEntry("Show Toolbar", true));
    slotViewToolBar();

    bool bViewStatusbar = gen_cfg.readEntry("Show Statusbar", true);
    viewStatusBar->setChecked(bViewStatusbar);
    slotViewStatusBar();

    viewMenuBar->setChecked (gen_cfg.readEntry("Show Menubar", true));
    slotViewMenuBar();

    QSize size = gen_cfg.readEntry ("Geometry", QSize ());
    if (!size.isEmpty ())
        resize (size);
    else if (m_player->settings ()->remembersize)
        resize (QSize (640, 480));

    KConfigGroup pipe_cfg (config, "Pipe Command");
    static_cast <KMPlayerPipeSource *> (m_player->sources () ["pipesource"])->setCommand (
            pipe_cfg.readEntry ("Command1", QString ()));
    // initialize the recent file list
    if (!recents) {
        fileOpenRecent->loadEntries (KConfigGroup (config, "Recent Files"));
        recents = new Recents (this);
        recents_id = m_view->playList ()->addTree (recents, "listssource", "view-history", KMPlayer::PlayListView::AllowDrag);
    }
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
    NETWinInfo winfo (QX11Info::display (), winId (), QX11Info::appRootWindow(), props);
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

#ifdef KMPLAYER_WITH_CAIRO
struct ExitSource : public KMPlayer::Source {
    KDE_NO_CDTOR_EXPORT ExitSource (KMPlayer::PartBase *p)
        : KMPlayer::Source (i18n ("Exit"), p, "exitsource") {}
    KDE_NO_EXPORT QString prettyName () { return i18n ("Exit"); }
    KDE_NO_EXPORT bool hasLength () { return false; }
    KDE_NO_EXPORT bool isSeekable () { return false; }
    void activate ();
    KDE_NO_EXPORT void deactivate () {}
    void stateElementChanged (KMPlayer::Node * node, KMPlayer::Node::State os, KMPlayer::Node::State ns);
};

KDE_NO_EXPORT void ExitSource::activate () {
    m_document = new KMPlayer::SourceDocument (this, QString ());
    QString exitfile = KStandardDirs::locate ("data", "kmplayer/exit.xml");
    QFile file (exitfile);
    if (file.exists () && file.open (IO_ReadOnly)) {
        QTextStream ts (&file);
        KMPlayer::readXML (m_document, ts, QString (), false);
    } else {
        QString smil = QString::fromLatin1 ("<smil><head><layout>"
          "<root-layout width='320' height='240' background-color='black'/>"
          "<region top='40' height='160' background-color='#101020'>"
          "<region id='image' left='128' top='28' width='64' bottom='28'/>"
          "</region></layout>"
          "<transition id='pw' dur='0.3' type='pushWipe' subtype='fromBottom'/>"
          "</head><body>"
          "<par>"
          //"<animate target='reg1' attribute='background-color' calcMode='discrete' values='#FFFFFF;#E4E4E4;#CCCCCC;#B4B4B4;#9E9E9E;#8A8A8A;#777777;#656565;#555555;#464646;#393939;#2D2D2D;#222222;#191919;#111111;#0B0B0B;#060606;#020202;#000000;#000000' dur='0.6'/>"
          "<img src='%2' id='img1' region='image' dur='0.4' fit='hidden' transOut='pw'/>"
          "</par>"
          "</body></smil>").arg (KIconLoader::global()->iconPath (QString::fromLatin1 ("kmplayer"), -64));
        QByteArray ba = smil.toUtf8 ();
        QTextStream ts (&ba, QIODevice::ReadOnly);
        KMPlayer::readXML (m_document, ts, QString (), false);
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

KDE_NO_EXPORT void ExitSource::stateElementChanged (KMPlayer::Node * node, KMPlayer::Node::State, KMPlayer::Node::State new_state) {
    if (new_state == KMPlayer::Node::state_deactivated &&
            m_document == node &&
            m_player->view ())
       m_player->view ()->topLevelWidget ()->close ();
}
#endif

KDE_NO_EXPORT bool KMPlayerApp::queryClose () {
    // KMPlayerVDRSource has to wait for pending commands like mute and quit
    m_player->stop ();
    //static_cast <KMPlayerVDRSource *> (m_player->sources () ["vdrsource"])->waitForConnectionClose ();
    if (m_played_exit || m_player->settings ()->no_intro || kapp->sessionSaving() )
        return true;
    if (m_auto_resize)
        disconnect(m_player, SIGNAL(sourceDimensionChanged()),this,SLOT(zoom100()));
    m_played_exit = true;
    if (!m_minimal_mode)
        minimalMode (false);
#ifdef KMPLAYER_WITH_CAIRO
    m_player->setSource (new ExitSource (m_player));
    return false;
#else
    return true;
#endif
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
    KUrl::List urls = KFileDialog::getOpenUrls (QString (), i18n ("*|All Files"), this, i18n ("Open File"));
    if (urls.size () == 1) {
        openDocumentFile (urls [0]);
    } else if (urls.size () > 1) {
        m_player->openUrl (KUrl ());
        for (unsigned int i = 0; i < urls.size (); i++)
            addUrl (urls [i]);
    }
}

KDE_NO_EXPORT void KMPlayerApp::slotFileOpenRecent(const KUrl& url)
{
    slotStatusMsg(i18n("Opening file..."));

    openDocumentFile (url);

}

KDE_NO_EXPORT void KMPlayerApp::slotSaveAs () {
    QString url = KFileDialog::getSaveFileName (QString (), QString (), this, i18n ("Save File"));
    if (!url.isEmpty ()) {
        QFile file (url);
        if (!file.open (IO_WriteOnly)) {
            KMessageBox::error (this, i18n ("Error opening file %1.\n%2.",url,file.errorString ()), i18n("Error"));
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
    fileOpenRecent->clear ();
    int mi = fileOpenRecent->maxItems ();
    fileOpenRecent->setMaxItems (0);
    fileOpenRecent->setMaxItems (mi);
    m_player->settings ()->urllist.clear ();
    m_player->settings ()->sub_urllist.clear ();
    if (recents) { // small window this check fails and thus ClearHistory fails
        recents->defer (); // make sure it's loaded
        recents->clear ();
        m_view->playList ()->updateTree (recents_id, recents, 0, false, false);
    }
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
    if (memberList ().count () > 1)
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

KDE_NO_EXPORT void KMPlayerApp::slotConfigureKeys () {
  KShortcutsDialog::configure (actionCollection ());
}

KDE_NO_EXPORT void KMPlayerApp::slotConfigureToolbars () {
    //KEditToolbar dlg (actionCollection ());
    //if (dlg.exec ())
    //    initMenu (); // also add custom popups //createGUI ();
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
    statusBar()->setVisible (m_showStatusbar);
}

KDE_NO_EXPORT void KMPlayerApp::slotViewMenuBar() {
    m_showMenubar = viewMenuBar->isChecked();
    if (m_showMenubar) {
        menuBar()->show();
        slotStatusMsg(i18n("Ready"));
    } else {
        menuBar()->hide();
        slotStatusMsg (i18n ("Show Menubar with %1",
                    viewMenuBar->shortcut ().toString ()));
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
    if (qobject_cast <KAction *> (sender ()))
        m_view->fullScreen();
#if KDE_IS_VERSION(3,1,90)
    viewFullscreen->setChecked (m_view->isFullScreen ());
#endif
    if (m_view->isFullScreen())
        hide ();
    else {
        show ();
        setGeometry (m_view->viewArea ()->topWindowRect ());
    }
}

KDE_NO_EXPORT void KMPlayerApp::playListItemSelected (Q3ListViewItem * item) {
    KMPlayer::PlayListItem * vi = static_cast <KMPlayer::PlayListItem *> (item);
    if (edit_tree_id > -1) {
        if (vi->playListView ()->rootItem (item)->id != edit_tree_id)
            editMode ();
        m_view->setInfoMessage (edit_tree_id > -1 ? vi->node->innerXML () : QString ());
    }
    viewEditMode->setEnabled (vi->playListView ()->rootItem (item)->flags & KMPlayer::PlayListView::TreeEdit);
}

KDE_NO_EXPORT
void KMPlayerApp::playListItemDropped (QDropEvent * de, Q3ListViewItem * after) {
    if (!after) { // could still be a descendent
        after = m_view->playList()->itemAt (m_view->playList()->contentsToViewport (de->pos ()));
        if (after) {
            Q3ListViewItem * p = after->itemAbove ();
            if (p && p->nextSibling () != after)
                after = after->parent ();
        }
    }
    if (!after)
        return;
    KMPlayer::RootPlayListItem *ritem = m_view->playList()->rootItem(after);
    if (ritem->id == 0)
        return;
    manip_node = 0L;
    m_drop_list.clear ();
    m_drop_after = after;
    KMPlayer::NodePtr after_node = static_cast<KMPlayer::PlayListItem*> (after)->node;
    if (after_node->id == KMPlayer::id_node_playlist_document ||
            after_node->id == KMPlayer::id_node_group_node)
        after_node->defer (); // make sure it has loaded
    if (de->source () == m_view->playList() &&
            m_view->playList()->lastDragTreeId () == playlist_id)
        manip_node = m_view->playList()->lastDragNode ();
    if (!manip_node && ritem->id == playlist_id) {
        m_drop_list = KUrl::List::fromMimeData (de->mimeData ());
        if (m_drop_list.isEmpty () && Q3TextDrag::canDecode (de)) {
            QString text;
            if (Q3TextDrag::decode (de, text) && KUrl (text).isValid ())
                m_drop_list.push_back (KUrl (text));
        }
    }
    m_dropmenu->changeItem (m_dropmenu->idAt (0),
            !!manip_node ? i18n ("Move here") : i18n ("&Add to list"));
    m_dropmenu->setItemVisible (m_dropmenu->idAt (3), !!manip_node);
    m_dropmenu->setItemVisible (m_dropmenu->idAt (2), (manip_node && manip_node->isPlayable ()));
    if (manip_node || m_drop_list.size () > 0)
        m_dropmenu->exec (m_view->playList ()->mapToGlobal (m_view->playList ()->contentsToViewport (de->pos ())));
}

KDE_NO_EXPORT void KMPlayerApp::menuDropInList () {
    KMPlayer::NodePtr n = static_cast<KMPlayer::PlayListItem*>(m_drop_after)->node;
    KMPlayer::NodePtr pi;
    for (int i = m_drop_list.size (); n && (i > 0 || manip_node); i--) {
        if (manip_node && manip_node->parentNode ()) {
            pi = manip_node;
            manip_node = 0L;
            pi->parentNode ()->removeChild (pi);
        } else
            pi = new PlaylistItem(playlist, this,false, m_drop_list[i-1].url());
        if (n == playlist || m_drop_after->isOpen ())
            n->insertBefore (pi, n->firstChild ());
        else
            n->parentNode ()->insertBefore (pi, n->nextSibling ());
    }
    m_view->playList()->updateTree (playlist_id, playlist, pi, true, false);
}

KDE_NO_EXPORT void KMPlayerApp::menuDropInGroup () {
    KMPlayer::NodePtr n = static_cast<KMPlayer::PlayListItem*>(m_drop_after)->node;
    if (!n)
        return;
    KMPlayer::NodePtr g = new PlaylistGroup (playlist, this, i18n("New group"));
    if (n == playlist || m_drop_after->isOpen ())
        n->insertBefore (g, n->firstChild ());
    else
        n->parentNode ()->insertBefore (g, n->nextSibling ());
    KMPlayer::NodePtr pi;
    for (int i = 0; i < m_drop_list.size () || manip_node; ++i) {
        if (manip_node && manip_node->parentNode ()) {
            pi = manip_node;
            manip_node = 0L;
            pi->parentNode ()->removeChild (pi);
        } else
            pi = new PlaylistItem (playlist,this, false, m_drop_list[i].url ());
        g->appendChild (pi);
    }
    m_view->playList()->updateTree (playlist_id, playlist, pi, true, false);
}

KDE_NO_EXPORT void KMPlayerApp::menuCopyDrop () {
    KMPlayer::NodePtr n = static_cast<KMPlayer::PlayListItem*>(m_drop_after)->node;
    if (n && manip_node) {
        KMPlayer::NodePtr pi = new PlaylistItem (playlist, this, false, manip_node->mrl ()->src);
        if (n == playlist || m_drop_after->isOpen ())
            n->insertBefore (pi, n->firstChild ());
        else
            n->parentNode ()->insertBefore (pi, n->nextSibling ());
        m_view->playList()->updateTree (playlist_id, playlist, pi, true, false);
    }
}

KDE_NO_EXPORT void KMPlayerApp::menuDeleteNode () {
    KMPlayer::NodePtr n;
    if (manip_node && manip_node->parentNode ()) {
        n = manip_node->previousSibling() ? manip_node->previousSibling() : manip_node->parentNode ();
        manip_node->parentNode ()->removeChild (manip_node);
    }
    m_view->playList()->updateTree (manip_tree_id, 0L, n, true, false);
}

KDE_NO_EXPORT void KMPlayerApp::menuMoveUpNode () {
    KMPlayer::NodePtr n = manip_node;
    if (n && n->parentNode () && n->previousSibling ()) {
        KMPlayer::NodePtr prev = n->previousSibling ();
        n->parentNode ()->removeChild (n);
        prev->parentNode ()->insertBefore (n, prev);
    }
    m_view->playList()->updateTree (manip_tree_id, 0L, n, true, false);
}

KDE_NO_EXPORT void KMPlayerApp::menuMoveDownNode () {
    KMPlayer::NodePtr n = manip_node;
    if (n && n->parentNode () && n->nextSibling ()) {
        KMPlayer::NodePtr next = n->nextSibling ();
        n->parentNode ()->removeChild (n);
        next->parentNode ()->insertBefore (n, next->nextSibling ());
    }
    m_view->playList()->updateTree (manip_tree_id, 0L, n, true, false);
}

KDE_NO_EXPORT void KMPlayerApp::playListItemMoved () {
    KMPlayer::PlayListItem * si = m_view->playList ()->selectedPlayListItem ();
    KMPlayer::RootPlayListItem * ri = m_view->playList ()->rootItem (si);
    kDebug() << "playListItemMoved " << (ri->id == playlist_id) << !! si->node;
    if (ri->id == playlist_id && si->node) {
        KMPlayer::NodePtr p = si->node->parentNode ();
        if (p) {
            p->removeChild (si->node);
            m_view->playList()->updateTree(playlist_id,playlist,0L,false,false);
        }
    }
}

KDE_NO_EXPORT void KMPlayerApp::preparePlaylistMenu (KMPlayer::PlayListItem * item, QMenu * pm) {
    KMPlayer::RootPlayListItem * ri = m_view->playList ()->rootItem (item);
    if (item->node &&
        ri->flags & (KMPlayer::PlayListView::Moveable | KMPlayer::PlayListView::Deleteable)) {
        manip_tree_id = ri->id;
        pm->insertSeparator ();
        manip_node = item->node;
        if (ri->flags & KMPlayer::PlayListView::Deleteable)
            pm->insertItem (KIconLoader::global ()->loadIconSet (QString ("edit-delete"), KIconLoader::Small, 0, true), i18n ("&Delete item"), this, SLOT (menuDeleteNode ()));
        if (ri->flags & KMPlayer::PlayListView::Moveable) {
            if (manip_node->previousSibling ())
                pm->insertItem (KIconLoader::global ()->loadIconSet (QString ("go-up"), KIconLoader::Small, 0, true), i18n ("&Move up"), this, SLOT (menuMoveUpNode ()));
            if (manip_node->nextSibling ())
                pm->insertItem (KIconLoader::global()->loadIconSet (QString ("go-down"), KIconLoader::Small, 0, true), i18n ("Move &down"), this, SLOT (menuMoveDownNode ()));
        }
    }
}

KDE_NO_EXPORT void KMPlayerApp::configChanged () {
    //viewKeepRatio->setChecked (m_player->settings ()->sizeratio);
    if (m_player->settings ()->docksystray && !m_systray) {
        m_systray = new KSystemTrayIcon (KIcon ("kmplayer"), this);
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
    //viewKeepRatio->setChecked (m_view->keepSizeRatio ());
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT KMPlayerMenuSource::KMPlayerMenuSource (const QString & n, KMPlayerApp * a, QMenu * m, const char * src)
    : KMPlayer::Source (n, a->player (), src), m_menu (m), m_app (a) {
}

KDE_NO_CDTOR_EXPORT KMPlayerMenuSource::~KMPlayerMenuSource () {
}

KDE_NO_EXPORT void KMPlayerMenuSource::menuItemClicked (QMenu * menu, int id) {
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
    dvddevice = new KUrlRequester (KUrl ("/dev/dvd"), this);
    QWhatsThis::add(dvddevice, i18n ("Path to your DVD device, you must have read rights to this device"));
    layout->addWidget (autoPlayDVD);
    layout->addItem (new QSpacerItem (0, 10, QSizePolicy::Minimum, QSizePolicy::Minimum));
    layout->addWidget (dvdDevicePathLabel);
    layout->addWidget (dvddevice);
    layout->addItem (new QSpacerItem (0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding));
}

//-----------------------------------------------------------------------------

class KMPLAYER_NO_EXPORT Disks : public KMPlayer::Document {
public:
    Disks (KMPlayerApp * a);
    void *message (KMPlayer::MessageType msg, void *data);
    KMPlayerApp * app;
};

class KMPLAYER_NO_EXPORT Disk : public KMPlayer::Mrl {
public:
    Disk (KMPlayer::NodePtr & doc, KMPlayerApp *a, const QString &url, const QString &pn);
    void activate ();
    KMPlayerApp * app;
};

KDE_NO_CDTOR_EXPORT Disks::Disks (KMPlayerApp * a)
                : KMPlayer::Document ("disks://", 0L), app (a) {
    id = id_node_disk_document;
    title = i18n ("Optical Disks");
}

KDE_NO_EXPORT void *Disks::message (KMPlayer::MessageType msg, void *data) {
    if (KMPlayer::MsgChildFinished) {
        finish ();
        return NULL;
    }
    return KMPlayer::Document::message (msg, data);
}

KDE_NO_CDTOR_EXPORT Disk::Disk (KMPlayer::NodePtr & doc, KMPlayerApp * a, const QString &url, const QString &pn) 
  : KMPlayer::Mrl (doc, id_node_disk_node), app (a) {
    src = url;
    title = pn;
}

KDE_NO_EXPORT void Disk::activate () {
    const char * sn;
    if (src.startsWith ("cdda"))
        sn = "audiocdsource";
    else if (src.startsWith ("vcd"))
        sn = "vcdsource";
    else
        sn = "dvdsource";
    app->player ()->setSource (app->player ()->sources () [sn]);
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT KMPlayerDVDSource::KMPlayerDVDSource (KMPlayerApp * a, QMenu * m)
    : KMPlayerMenuSource (i18n ("DVD"), a, m, "dvdsource"), m_configpage (0L) {
    m_menu->insertTearOffHandle ();
    m_dvdtitlemenu = new QMenu (m_app);
    m_dvdsubtitlemenu = new QMenu (m_app);
    m_dvdchaptermenu = new QMenu (m_app);
    m_dvdlanguagemenu = new QMenu (m_app);
    m_dvdtitlemenu->setCheckable (true);
    m_dvdsubtitlemenu->setCheckable (true);
    m_dvdchaptermenu->setCheckable (true);
    m_dvdlanguagemenu->setCheckable (true);
    setUrl ("dvd://");
    m_player->settings ()->addPage (this);
    disks = new Disks (a);
    disks->appendChild (new Disk (disks, a, "cdda://", i18n ("CDROM - Audio Compact Disk")));
    disks->appendChild (new Disk (disks, a, "vcd://", i18n ("VCD - Video Compact Disk")));
    disks->appendChild (new Disk (disks, a, "dvd://", i18n ("DVD - Digital Video Disk")));
    m_app->view()->playList()->addTree (disks, "listssource", "media-optical", 0);
}

KDE_NO_CDTOR_EXPORT KMPlayerDVDSource::~KMPlayerDVDSource () {
    disks->document ()->dispose ();
}

KDE_NO_EXPORT bool KMPlayerDVDSource::processOutput (const QString & str) {
    if (KMPlayer::Source::processOutput (str))
        return true;
    if (m_identified)
        return false;
    //kDebug () << "scanning " << cstr;
    QRegExp * patterns = static_cast <KMPlayer::MPlayerPreferencesPage *> (m_player->mediaManager ()->processInfos () ["mplayer"]->config_page)->m_patterns;
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
        kDebug () << "subtitle sid:" << sub_id << " lang:" << sub_title;
    } else if (!post090 && langRegExp.search (str) > -1) {
        bool ok;
        int lang_id = langRegExp.cap (1).toInt (&ok);
        QString lang_title = ok ? langRegExp.cap (2) : langRegExp.cap (1);
        if (!ok)
            lang_id = langRegExp.cap (2).toInt (&ok);
        m_dvdlanguagemenu->insertItem (lang_title, lang_id);
        kDebug () << "lang aid:" << lang_id << " lang:" << lang_title;
    } else if (titleRegExp.search (str) > -1) {
        kDebug () << "title " << titleRegExp.cap (1);
        unsigned ts = titleRegExp.cap (1).toInt ();
        if ( ts > 100) ts = 100;
        for (unsigned t = 1; t <= ts; t++)
            m_dvdtitlemenu->insertItem (QString::number (t), t);
    } else if (chapterRegExp.search (str) > -1) {
        kDebug () << "chapter " << chapterRegExp.cap (1);
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
    setUrl ("dvd://");
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

KDE_NO_EXPORT void KMPlayerDVDSource::setCurrent (KMPlayer::Mrl *cur) {
    KMPlayer::Source::setCurrent (cur);
    QString url ("dvd://");
    if (m_document) {
        if (m_current_title > 0)
            url += QString::number (m_current_title);
        m_document->mrl ()->src = url;
    } else
        setUrl (url);
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
    m_recordcmd = m_options + QString (" -vf scale -zoom");
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
        m_dvdtitlemenu->clear ();
        m_dvdsubtitlemenu->clear ();
        m_dvdchaptermenu->clear ();
        m_dvdlanguagemenu->clear ();
        if (m_start_play)
            QTimer::singleShot (0, m_player, SLOT (play ()));
    }
}

KDE_NO_EXPORT void KMPlayerDVDSource::play () {
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
    return i18n ("DVD");
}

static const char * strPlayDVD = "Immediately Play DVD";

KDE_NO_EXPORT void KMPlayerDVDSource::write (KSharedConfigPtr config) {
    KConfigGroup (config, strMPlayerGroup).writeEntry (strPlayDVD, m_auto_play);
}

KDE_NO_EXPORT void KMPlayerDVDSource::read (KSharedConfigPtr config) {
    m_auto_play = KConfigGroup (config, strMPlayerGroup).readEntry (strPlayDVD, true);
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

KDE_NO_CDTOR_EXPORT KMPlayerDVDNavSource::KMPlayerDVDNavSource (KMPlayerApp * app, QMenu * m)
    : KMPlayerMenuSource (i18n ("DVDNav"), app, m, "dvdnavsource") {
    m_menu->insertTearOffHandle (-1, 0);
    setUrl ("dvd://");
}

KDE_NO_CDTOR_EXPORT KMPlayerDVDNavSource::~KMPlayerDVDNavSource () {}

KDE_NO_EXPORT void KMPlayerDVDNavSource::activate () {
    setUrl ("dvd://");
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
    /*switch (id) {
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
    }*/
}

KDE_NO_EXPORT QString KMPlayerDVDNavSource::prettyName () {
    return i18n ("DVD");
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT KMPlayerPrefSourcePageVCD::KMPlayerPrefSourcePageVCD (QWidget * parent)
 : QFrame (parent) {
     QVBoxLayout *layout = new QVBoxLayout (this, 5, 2);
     autoPlayVCD = new QCheckBox (i18n ("Auto play after opening a VCD"), this, 0);
     QWhatsThis::add(autoPlayVCD, i18n ("Start playing VCD right after opening VCD"));
     QLabel *vcdDevicePathLabel = new QLabel (i18n ("VCD (CDROM) device:"), this, 0);
     vcddevice= new KUrlRequester (KUrl ("/dev/cdrom"), this);
     QWhatsThis::add(vcddevice, i18n ("Path to your CDROM/DVD device, you must have read rights to this device"));
     layout->addWidget (autoPlayVCD);
     layout->addItem (new QSpacerItem (0, 10, QSizePolicy::Minimum, QSizePolicy::Minimum));
     layout->addWidget (vcdDevicePathLabel);
     layout->addWidget (vcddevice);
     layout->addItem (new QSpacerItem (0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding));
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT KMPlayerVCDSource::KMPlayerVCDSource (KMPlayerApp * a, QMenu * m)
    : KMPlayerMenuSource (i18n ("VCD"), a, m, "vcdsource"), m_configpage (0L) {
    m_player->settings ()->addPage (this);
    setUrl ("vcd://");
}

KDE_NO_CDTOR_EXPORT KMPlayerVCDSource::~KMPlayerVCDSource () {
}

KDE_NO_EXPORT bool KMPlayerVCDSource::processOutput (const QString & str) {
    if (KMPlayer::Source::processOutput (str))
        return true;
    if (m_identified)
        return false;
    //kDebug () << "scanning " << cstr;
    QRegExp * patterns = static_cast <KMPlayer::MPlayerPreferencesPage *> (m_player->mediaManager ()->processInfos () ["mplayer"]->config_page)->m_patterns;
    QRegExp & trackRegExp = patterns [KMPlayer::MPlayerPreferencesPage::pat_vcdtrack];
    if (trackRegExp.search (str) > -1) {
        m_document->state = KMPlayer::Element::state_deferred;
        m_document->appendChild (new KMPlayer::GenericMrl (m_document, QString ("vcd://") + trackRegExp.cap (1), i18n ("Track ") + trackRegExp.cap (1)));
        kDebug () << "track " << trackRegExp.cap (1);
        return true;
    }
    return false;
}

KDE_NO_EXPORT void KMPlayerVCDSource::activate () {
    m_player->stop ();
    init ();
    m_start_play = m_auto_play;
    setUrl ("vcd://");
    if (m_start_play)
        QTimer::singleShot (0, m_player, SLOT (play ()));
}

KDE_NO_EXPORT void KMPlayerVCDSource::deactivate () {
}

KDE_NO_EXPORT void KMPlayerVCDSource::setIdentified (bool b) {
    KMPlayer::Source::setIdentified (b);
    setCurrent (!m_current || !m_document->hasChildNodes ()
            ? m_document->mrl () : m_current->mrl ());
    m_player->updateTree ();
    if (m_current->state == KMPlayer::Element::state_deferred)
        m_current->undefer ();
    m_app->slotStatusMsg (i18n ("Ready."));
}

KDE_NO_EXPORT void KMPlayerVCDSource::setCurrent (KMPlayer::Mrl *cur) {
    KMPlayer::Source::setCurrent (cur);
    QString url ("vcd://");
    if (m_current && m_current != m_document)
        url += m_current->mrl ()->src;
    m_options.truncate (0);
    if (m_player->settings ()->vcddevice.length () > 0)
        m_options+=QString(" -cdrom-device ") + m_player->settings()->vcddevice;
    m_recordcmd = m_options;
}

KDE_NO_EXPORT QString KMPlayerVCDSource::prettyName () {
    return i18n ("VCD");
}

static const char * strPlayVCD = "Immediately Play VCD";

KDE_NO_EXPORT void KMPlayerVCDSource::write (KSharedConfigPtr config) {
    KConfigGroup (config, strMPlayerGroup).writeEntry (strPlayVCD, m_auto_play);
}

KDE_NO_EXPORT void KMPlayerVCDSource::read (KSharedConfigPtr config) {
    m_auto_play = KConfigGroup (config, strMPlayerGroup).readEntry (strPlayVCD, true);
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

KDE_NO_CDTOR_EXPORT KMPlayerAudioCDSource::KMPlayerAudioCDSource (KMPlayerApp * a, QMenu * m)
    : KMPlayerMenuSource (i18n ("Audio CD"), a, m, "audiocdsource") {
    setUrl ("cdda://");
}

KDE_NO_CDTOR_EXPORT KMPlayerAudioCDSource::~KMPlayerAudioCDSource () {
}

KDE_NO_EXPORT bool KMPlayerAudioCDSource::processOutput (const QString & str) {
    if (KMPlayer::Source::processOutput (str))
        return true;
    if (m_identified)
        return false;
    //kDebug () << "scanning " << str;
    QRegExp * patterns = static_cast <KMPlayer::MPlayerPreferencesPage *> (m_player->mediaManager ()->processInfos () ["mplayer"]->config_page)->m_patterns;
    QRegExp & trackRegExp = patterns [KMPlayer::MPlayerPreferencesPage::pat_cdromtracks];
    if (trackRegExp.search (str) > -1) {
        //if (m_document->state != KMPlayer::Element::state_deferred)
        //    m_document->defer ();
        int nt = trackRegExp.cap (1).toInt ();
        kDebug () << "tracks " << trackRegExp.cap (1);
        for (int i = 0; i < nt; i++)
            m_document->appendChild (new KMPlayer::GenericMrl (m_document, QString ("cdda://%1").arg (i+1), i18n ("Track %1",QString::number(i+1))));
        return true;
    }
    return false;
}

KDE_NO_EXPORT void KMPlayerAudioCDSource::activate () {
    m_player->stop ();
    init ();
    //m_start_play = m_auto_play;
    setUrl ("cdda://");
    //if (m_start_play)
        QTimer::singleShot (0, m_player, SLOT (play ()));
}

KDE_NO_EXPORT void KMPlayerAudioCDSource::deactivate () {
}

KDE_NO_EXPORT void KMPlayerAudioCDSource::setIdentified (bool b) {
    KMPlayer::Source::setIdentified (b);
    setCurrent (!m_current || !m_document->hasChildNodes ()
            ? m_document->mrl () : m_current->mrl ());
    if (m_current == m_document && m_document->hasChildNodes ()) {
        //m_back_request = m_document->firstChild ();
        //m_player->process ()->stop ();
    }
    m_player->updateTree ();
    //if (m_current->state == KMPlayer::Element::state_deferred)
    //    m_current->undefer ();
    m_app->slotStatusMsg (i18n ("Ready."));
}

KDE_NO_EXPORT void KMPlayerAudioCDSource::setCurrent (KMPlayer::Mrl *cur) {
    KMPlayer::Source::setCurrent (cur);
    QString url ("cdda://");
    if (m_current && m_current != m_document)
        url += m_current->mrl ()->src;
    m_options = "-cdda speed=3";
    if (m_player->settings ()->vcddevice.length () > 0)
        m_options+=QString(" -cdrom-device ") + m_player->settings()->vcddevice;
    m_recordcmd = m_options;
}

KDE_NO_EXPORT QString KMPlayerAudioCDSource::prettyName () {
    return i18n ("Audio CD");
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
    //    m_pipecmd = KUrl::decode_string (m_url.path ()).mid (1);
    setUrl ("stdin://");
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
    return i18n ("Pipe - %1",m_pipecmd);
}

KDE_NO_EXPORT void KMPlayerPipeSource::setCommand (const QString & cmd) {
    m_pipecmd = cmd;
    if (m_document)
        m_document->mrl ()->title = cmd;
}

#include "kmplayer.moc"
#include "kmplayerappsource.moc"
