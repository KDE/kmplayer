/*
    SPDX-FileCopyrightText: 2002-2003 Koos Vriezen <koos.vriezen@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#ifdef KDE_USE_FINAL
#undef Always
#endif

#include "config-kmplayer.h"

#include <math.h>

#include <qapplication.h>
#include <QByteArray>
#include <qcursor.h>
#include <qtimer.h>
#include <qpair.h>
#include <qpushbutton.h>
#include <QStandardPaths>
#include <qslider.h>
#include <qfile.h>
#include <qregexp.h>
#include <qprocess.h>
#include <QtDBus/QtDBus>
#include <QMimeDatabase>
#include <QMimeType>

#include <kmessagebox.h>
#include <kaboutdata.h>
#include <kbookmarkmenu.h>
#include <kbookmarkmanager.h>
#include <kbookmark.h>
#include <kconfig.h>
#include <kconfiggroup.h>
#include <kiconloader.h>
#include <klocalizedstring.h>
#include <kprotocolinfo.h>
#include <kio/job.h>
#include <kio/jobclasses.h>
#include <kurlauthorized.h>

#include "kmplayercommon_log.h"
#include "kmplayerpartbase.h"
#include "kmplayerview.h"
#include "playmodel.h"
#include "playlistview.h"
#include "kmplayerprocess.h"
#include "viewarea.h"
#include "kmplayercontrolpanel.h"
#include "kmplayerconfig.h"
#include "kmplayer_smil.h"
#include "mediaobject.h"
#include "partadaptor.h"

namespace KMPlayer {

class BookmarkOwner : public KBookmarkOwner {
public:
    BookmarkOwner (PartBase *);
    ~BookmarkOwner () override {}
    void openBookmark(const KBookmark &bm, Qt::MouseButtons mb, Qt::KeyboardModifiers km) override;
    QString currentTitle() const override;
    QString currentURL() const;
private:
    PartBase * m_player;
};

} // namespace

using namespace KMPlayer;

BookmarkOwner::BookmarkOwner (PartBase * player)
    : m_player (player) {}

void BookmarkOwner::openBookmark(const KBookmark &bm, Qt::MouseButtons, Qt::KeyboardModifiers) {
    if (!bm.isNull ())
        m_player->openUrl (bm.url ());
}

QString BookmarkOwner::currentTitle () const {
    return m_player->source ()->prettyName ();
}

QString BookmarkOwner::currentURL () const {
    return m_player->source ()->url ().url ();
}

//-----------------------------------------------------------------------------

PartBase::PartBase (QWidget * wparent, QObject * parent, KSharedConfigPtr config)
 : KMediaPlayer::Player (wparent, "kde_kmplayer_part", parent),
   m_config (config),
   m_view (new View (wparent)),
   m_settings (new Settings (this, config)),
   m_media_manager (new MediaManager (this)),
   m_play_model (new PlayModel (this, KIconLoader::global ())),
   m_source (nullptr),
   m_bookmark_menu (nullptr),
   m_update_tree_timer (0),
   m_rec_timer (0),
   m_noresize (false),
   m_auto_controls (true),
   m_bPosSliderPressed (false),
   m_in_update_tree (false),
   m_update_tree_full (false)
{
    m_sources ["urlsource"] = new URLSource (this);

    QString bmfile = QStandardPaths::locate(QStandardPaths::GenericDataLocation, "kmplayer/bookmarks.xml");
    QString localbmfile = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/kmplayer/bookmarks.xml";
    if (localbmfile != bmfile) {
        bool bmfileCopied = QFile(bmfile).copy(localbmfile);
        qCDebug(LOG_KMPLAYER_COMMON) << "bookmarks.xml copied successfully?" << bmfileCopied;
    }
    m_bookmark_manager = KBookmarkManager::managerForFile (localbmfile, "kmplayer");
    m_bookmark_owner = new BookmarkOwner (this);
}

void PartBase::showConfigDialog () {
    m_settings->show ("URLPage");
}

void PartBase::showPlayListWindow () {
    // note, this is also the slot of application's view_playlist action, but
    // anyhow, actions don't work for the fullscreen out-of-the-box, so ...
    if (m_view->viewArea ()->isFullScreen ())
        fullScreen ();
    else if (m_view->viewArea ()->isMinimalMode ())
        ; //done by app: m_view->viewArea ()->minimalMode ();
    else
        m_view->toggleShowPlaylist ();
}

void PartBase::addBookMark (const QString & t, const QString & url) {
    KBookmarkGroup b = m_bookmark_manager->root ();
    b.addBookmark (t, KUrl (url), KIO::iconNameForUrl(url));
    m_bookmark_manager->emitChanged (b);
}

void PartBase::init (KActionCollection * action_collection, const QString &objname, bool transparent) {
    KParts::Part::setWidget (m_view);
    m_view->init (action_collection, transparent);
    connect(m_settings, SIGNAL(configChanged()), this, SLOT(settingsChanged()));
    m_settings->readConfig ();
    m_settings->applyColorSetting (false);
    connect (m_view, SIGNAL(urlDropped(const QList<QUrl>&)), this, SLOT(openUrl(const QList<QUrl>&)));
    connectPlaylist (m_view->playList ());
    connectInfoPanel (m_view->infoPanel ());

    (void) new PartAdaptor (this);
    QDBusConnection::sessionBus().registerObject (objname, this);
}

void PartBase::connectPanel (ControlPanel * panel) {
    /*panel->contrastSlider ()->setValue (m_settings->contrast);
    panel->brightnessSlider ()->setValue (m_settings->brightness);
    panel->hueSlider ()->setValue (m_settings->hue);
    panel->saturationSlider ()->setValue (m_settings->saturation);
    panel->volumeBar ()->setValue (m_settings->volume);*/
    connect (panel->button (ControlPanel::button_playlist), SIGNAL (clicked ()), this, SLOT (showPlayListWindow ()));
    connect (panel->button (ControlPanel::button_back), SIGNAL (clicked ()), this, SLOT (back ()));
    connect (panel->button (ControlPanel::button_play), SIGNAL (clicked ()), this, SLOT (play ()));
    connect (panel->button (ControlPanel::button_forward), SIGNAL (clicked ()), this, SLOT (forward ()));
    connect (panel->button (ControlPanel::button_pause), SIGNAL (clicked ()), this, SLOT (pause ()));
    connect (panel->button (ControlPanel::button_stop), SIGNAL (clicked ()), this, SLOT (stop ()));
    connect (panel->button (ControlPanel::button_record), SIGNAL (clicked()), this, SLOT (record()));
    connect (panel->volumeBar (), SIGNAL (volumeChanged (int)), this, SLOT (volumeChanged (int)));
    connect (panel->positionSlider (), SIGNAL (valueChanged (int)), this, SLOT (positionValueChanged (int)));
    connect (panel->positionSlider (), SIGNAL (sliderPressed()), this, SLOT (posSliderPressed()));
    connect (panel->positionSlider (), SIGNAL (sliderReleased()), this, SLOT (posSliderReleased()));
    connect (this, SIGNAL (positioned (int, int)), panel, SLOT (setPlayingProgress (int, int)));
    connect (this, SIGNAL (loading(int)), panel, SLOT(setLoadingProgress(int)));
    /*connect (panel->contrastSlider (), SIGNAL (valueChanged(int)), this, SLOT (contrastValueChanged(int)));
    connect (panel->brightnessSlider (), SIGNAL (valueChanged(int)), this, SLOT (brightnessValueChanged(int)));
    connect (panel->hueSlider (), SIGNAL (valueChanged(int)), this, SLOT (hueValueChanged(int)));
    connect (panel->saturationSlider (), SIGNAL (valueChanged(int)), this, SLOT (saturationValueChanged(int)));*/
    connect (this, SIGNAL (languagesUpdated(const QStringList &, const QStringList &)), panel, SLOT (setLanguages (const QStringList &, const QStringList &)));
    connect (panel->audioMenu, SIGNAL (triggered (QAction*)), this, SLOT (audioSelected (QAction*)));
    connect (panel->subtitleMenu, SIGNAL (triggered (QAction*)), this, SLOT (subtitleSelected (QAction*)));
    connect (panel->playerMenu, SIGNAL (triggered (QAction*)), this, SLOT (slotPlayerMenu (QAction*)));
    connect (this, SIGNAL (panelActionToggled (QAction*)), panel, SLOT (actionToggled (QAction*)));
    connect (panel->fullscreenAction, SIGNAL (triggered (bool)),
            this, SLOT (fullScreen ()));
    connect (panel->configureAction, SIGNAL (triggered (bool)),
            this, SLOT (showConfigDialog ()));
    connect (panel->videoConsoleAction, SIGNAL (triggered (bool)),
            m_view, SLOT(toggleVideoConsoleWindow ()));
    connect (panel->playlistAction, SIGNAL (triggered (bool)),
            m_view, SLOT (toggleShowPlaylist ()));
    connect (this, SIGNAL (statusUpdated (const QString &)),
             panel->view (), SLOT (setStatusMessage (const QString &)));
    //connect (panel (), SIGNAL (clicked ()), m_settings, SLOT (show ()));
}

void PartBase::createBookmarkMenu(QMenu *owner, KActionCollection *ac) {
    m_bookmark_menu = new KBookmarkMenu (m_bookmark_manager, m_bookmark_owner, owner, ac);
}

void PartBase::connectPlaylist (PlayListView * playlist) {
    playlist->setModel (m_play_model);
    connect (m_play_model, SIGNAL (updating (const QModelIndex &)),
             playlist, SLOT(modelUpdating (const QModelIndex &)));
    connect (m_play_model, SIGNAL (updated (const QModelIndex&, const QModelIndex&, bool, bool)),
             playlist, SLOT(modelUpdated (const QModelIndex&, const QModelIndex&, bool, bool)));
    connect (playlist->selectionModel(), SIGNAL(currentChanged(QModelIndex,QModelIndex)),
             playlist, SLOT(slotCurrentItemChanged(QModelIndex,QModelIndex)));
    connect (playlist, SIGNAL (addBookMark (const QString &, const QString &)),
             this, SLOT (addBookMark (const QString &, const QString &)));
    connect (playlist, SIGNAL (activated (const QModelIndex &)),
             this, SLOT (playListItemActivated (const QModelIndex &)));
    connect (playlist, SIGNAL (clicked (const QModelIndex&)),
             this, SLOT (playListItemClicked (const QModelIndex&)));
    connect (this, SIGNAL (treeChanged (int, NodePtr, NodePtr, bool, bool)),
             playlist->model (), SLOT (updateTree (int, NodePtr, NodePtr, bool, bool)));
}

void PartBase::connectInfoPanel (InfoWindow * infopanel) {
    connect (this, SIGNAL (infoUpdated (const QString &)),
             infopanel->view (), SLOT (setInfoMessage (const QString &)));
}

PartBase::~PartBase () {
    qCDebug(LOG_KMPLAYER_COMMON) << "PartBase::~PartBase";
    m_view = (View*) nullptr;
    stopRecording ();
    stop ();
    if (m_source)
        m_source->deactivate ();
    delete m_media_manager;
    if (m_record_doc)
        m_record_doc->document ()->dispose ();
    delete m_settings;
    delete m_bookmark_menu;
    delete m_sources ["urlsource"];
    //delete m_bookmark_manager;
    delete m_bookmark_owner;
}

void PartBase::showControls (bool show) {
    viewWidget ()->setControlPanelMode (
            show ? View::CP_Show : View::CP_Hide);
}

QString PartBase::getStatus () {
    QString rval = "Waiting";
    if (source() && source()->document()) {
        if (source()->document()->unfinished ())
            rval = "Playable";
        else if (source()->document()->state >= Node::state_deactivated)
            rval = "Complete";
    }
    return rval;
}

QString PartBase::doEvaluate (const QString &) {
    return "undefined";
}

void PartBase::settingsChanged () {
    if (!m_view)
        return;
    if (m_settings->showcnfbutton)
        m_view->controlPanel()->button (ControlPanel::button_config)->show();
    else
        m_view->controlPanel()->button (ControlPanel::button_config)->hide();
    m_view->controlPanel()->enableRecordButtons (m_settings->showrecordbutton);
    if (m_settings->showplaylistbutton)
        m_view->controlPanel()->button (ControlPanel::button_playlist)->show();
    else
        m_view->controlPanel()->button (ControlPanel::button_playlist)->hide();
    if (!m_settings->showbroadcastbutton)
        m_view->controlPanel ()->broadcastButton ()->hide ();
    keepMovieAspect (m_settings->sizeratio);
    m_settings->applyColorSetting (true);
}

KMediaPlayer::View* PartBase::view () {
    return m_view;
}

extern const char * strGeneralGroup;

QString PartBase::processName (Mrl *mrl) {
    if (id_node_grab_document == mrl->id)
        return QString ("mplayer"); //FIXME
    // determine backend, start with temp_backends
    QString p = temp_backends [m_source->name()];
    bool remember_backend = p.isEmpty ();
    MediaManager::ProcessInfoMap &pinfos = m_media_manager->processInfos ();
    if (p.isEmpty ()) {
        // next try to find mimetype match from kmplayerrc
        if (!mrl->mimetype.isEmpty ()) {
            KConfigGroup mime_cfg (m_config, mrl->mimetype);
            p = mime_cfg.readEntry ("player", QString ());
            remember_backend = !(!p.isEmpty () &&
                    pinfos.contains (p) &&
                    pinfos [p]->supports (m_source->name ()));
        }
    }
    if (p.isEmpty ())
        // try source match from kmplayerrc
        p = m_settings->backends [m_source->name()];
    if (p.isEmpty ()) {
        // try source match from kmplayerrc by re-reading
        p = KConfigGroup(m_config, strGeneralGroup).readEntry(m_source->name(),QString());
    }
    if (p.isEmpty () ||
            !pinfos.contains (p) ||
            !pinfos [p]->supports (m_source->name ())) {
        // finally find first supported player
        p.truncate (0);
        const MediaManager::ProcessInfoMap::const_iterator e = pinfos.constEnd();
        for (MediaManager::ProcessInfoMap::const_iterator i = pinfos.constBegin(); i != e; ++i)
            if (i.value ()->supports (m_source->name ())) {
                p = QString (i.value ()->name);
                break;
            }
    }
    if (!p.isEmpty ()) {
        updatePlayerMenu (m_view->controlPanel (), p);
        if (remember_backend)
            m_settings->backends [m_source->name()] = p;
        else
            temp_backends [m_source->name()] = QString ();
    }
    return p;
}

void PartBase::processCreated (Process*) {}

void PartBase::slotPlayerMenu (QAction* act) {
    Mrl *mrl = m_source->current ();
    bool playing = mrl && mrl->active ();
    const char * srcname = m_source->name ();
    QMenu *player_menu = m_view->controlPanel ()->playerMenu;
    MediaManager::ProcessInfoMap &pinfos = m_media_manager->processInfos ();
    const MediaManager::ProcessInfoMap::const_iterator e = pinfos.constEnd();
    int id = 0;
    for (MediaManager::ProcessInfoMap::const_iterator i = pinfos.constBegin();
            id < player_menu->actions().count() && i != e;
            ++i) {
        ProcessInfo *pinfo = i.value ();
        if (!pinfo->supports (srcname))
            continue;
        QAction* menu = player_menu->actions().at (id);
        menu->setChecked (menu == act);
        if (menu == act) {
            if (strcmp (pinfo->name, "npp"))
                m_settings->backends [srcname] = pinfo->name;
            temp_backends [srcname] = pinfo->name;
        }
        id++;
    }
    if (playing)
        m_source->play (mrl);
}

void PartBase::updatePlayerMenu (ControlPanel *panel, const QString &backend) {
    if (!m_view)
        return;
    QMenu *menu = panel->playerMenu;
    menu->clear ();
    MediaManager::ProcessInfoMap &pinfos = m_media_manager->processInfos ();
    const MediaManager::ProcessInfoMap::const_iterator e = pinfos.constEnd();
    for (MediaManager::ProcessInfoMap::const_iterator i = pinfos.constBegin(); i != e; ++i) {
        ProcessInfo *p = i.value ();
        if (p->supports (m_source ? m_source->name () : "urlsource")) {
            QAction* act = menu->addAction (p->label);
            act->setCheckable(true);
            if (backend == p->name)
                act->setChecked (true);
        }
    }
}

void PartBase::connectSource (Source * old_source, Source * source) {
    if (old_source) {
        disconnect (old_source, SIGNAL(endOfPlayItems ()), this, SLOT(stop ()));
        disconnect (old_source, SIGNAL (dimensionsChanged ()),
                    this, SLOT (sourceHasChangedAspects ()));
        disconnect (old_source, SIGNAL (startPlaying ()),
                    this, SLOT (slotPlayingStarted ()));
        disconnect (old_source, SIGNAL (stopPlaying ()),
                    this, SLOT (slotPlayingStopped ()));
    }
    if (source) {
        connect (source, SIGNAL (endOfPlayItems ()), this, SLOT (stop ()));
        connect (source, SIGNAL (dimensionsChanged ()),
                this, SLOT (sourceHasChangedAspects ()));
        connect (source, SIGNAL (startPlaying ()),
                this, SLOT (slotPlayingStarted ()));
        connect (source, SIGNAL (stopPlaying ()),
                this, SLOT (slotPlayingStopped ()));
    }
}

void PartBase::setSource (Source * _source) {
    Source * old_source = m_source;
    if (m_source) {
        m_source->deactivate ();
        stop ();
        if (m_view) {
            m_view->reset ();
            emit infoUpdated (QString ());
        }
        disconnect (this, SIGNAL (audioIsSelected (int)),
                    m_source, SLOT (setAudioLang (int)));
        disconnect (this, SIGNAL (subtitleIsSelected (int)),
                    m_source, SLOT (setSubtitle (int)));
    }
    if (m_view) {
        if (m_auto_controls)
            m_view->controlPanel ()->setAutoControls (m_auto_controls);
        m_view->controlPanel ()->enableRecordButtons (m_settings->showrecordbutton);
        if (!m_settings->showcnfbutton)
            m_view->controlPanel()->button(ControlPanel::button_config)->hide();
        if (!m_settings->showplaylistbutton)
          m_view->controlPanel()->button(ControlPanel::button_playlist)->hide();
    }
    m_source = _source;
    connectSource (old_source, m_source);
    connect (this, SIGNAL (audioIsSelected (int)),
             m_source, SLOT (setAudioLang (int)));
    connect (this, SIGNAL (subtitleIsSelected (int)),
             m_source, SLOT (setSubtitle (int)));
    m_source->init ();
    m_source->setIdentified (false);
    if (m_view)
        updatePlayerMenu (m_view->controlPanel ());
    if (m_source && !m_source->avoidRedirects ())
        QTimer::singleShot (0, m_source, SLOT (slotActivate ()));
    updateTree (true, true);
    emit sourceChanged (old_source, m_source);
}

void PartBase::changeURL (const QString & url) {
    emit urlChanged (url);
}

bool PartBase::isSeekable (void) const {
    return m_source ? m_source->isSeekable () : false;
}

bool PartBase::hasLength () const {
    return m_source ? m_source->hasLength () : false;
}

qlonglong PartBase::length () const {
    return m_source ? m_source->length () : 0;
}

bool PartBase::openUrl (const QUrl &url) {
    qCDebug(LOG_KMPLAYER_COMMON) << "PartBase::openUrl " << url.url() << url.isValid ();
    if (!m_view) return false;
    stop ();
    Source * src = (url.isEmpty () ? m_sources ["urlsource"] : (!url.scheme().compare ("kmplayer") && m_sources.contains (url.host ()) ? m_sources [url.host ()] : m_sources ["urlsource"]));
    setSource (src);
    src->setSubURL (QUrl ());
    src->setUrl (url.isLocalFile () ? url.toLocalFile() : url.url ());
    if (src->avoidRedirects ())
        src->activate ();
    return true;
}

bool PartBase::openUrl(const QList<QUrl>& urls) {
    if (urls.size () == 1) {
        openUrl(urls[0]);
    } else {
        openUrl (QUrl ());
        NodePtr d = m_source->document ();
        if (d)
            for (int i = 0; i < urls.size (); i++) {
                const QUrl &url = urls [i];
                d->appendChild (new GenericURL (d,
                            url.isLocalFile() ? url.toLocalFile() : url.toString()));
            }
    }
    return true;
}

void PartBase::openUrl (const KUrl &u, const QString &t, const QString &srv) {
    qCDebug(LOG_KMPLAYER_COMMON) << u << " " << t << " " << srv;
    QDBusMessage msg = QDBusMessage::createMethodCall (
            "org.kde.klauncher", "/KLauncher",
            "org.kde.KLauncher", "start_service_by_desktop_name");
    QStringList urls;
    urls << u.url ();
    msg << "kfmclient" << urls << QStringList () << QString () << true;
    msg.setDelayedReply (false);
    QDBusConnection::sessionBus().send (msg);
}

bool PartBase::closeUrl () {
    stop ();
    if (m_view)
        m_view->reset ();
    return true;
}

bool PartBase::openFile () {
    return false;
}

void PartBase::keepMovieAspect (bool b) {
    if (m_view)
        m_view->setKeepSizeRatio (b);
}

void PartBase::timerEvent (QTimerEvent * e) {
    if (e->timerId () == m_update_tree_timer) {
        m_update_tree_timer = 0;
        updateTree (m_update_tree_full, true);
    } else if (e->timerId () == m_rec_timer) {
        m_rec_timer = 0;
        if (m_record_doc)
            openUrl(KUrl(convertNode <RecordDocument> (m_record_doc)->record_file));
    }
    killTimer (e->timerId ());
}

void PartBase::playingStarted () {
    qCDebug(LOG_KMPLAYER_COMMON) << "playingStarted " << this;
    if (m_view) {
        m_view->controlPanel ()->setPlaying (true);
        m_view->controlPanel ()->showPositionSlider (!!m_source->length ());
        m_view->controlPanel ()->enableSeekButtons (m_source->isSeekable ());
        m_view->playingStart ();
        //if (m_settings->autoadjustvolume && m_process)
        //   m_process->volume(m_view->controlPanel()->volumeBar()->value(),true);
    }
    emit loading (100);
}

void PartBase::slotPlayingStarted () {
    playingStarted ();
}

void PartBase::playingStopped () {
    qCDebug(LOG_KMPLAYER_COMMON) << "playingStopped " << this;
    if (m_view) {
        m_view->controlPanel ()->setPlaying (false);
        m_view->playingStop ();
        m_view->reset ();
    }
    m_bPosSliderPressed = false;
}

void PartBase::slotPlayingStopped () {
    playingStarted ();
}

void PartBase::setPosition (int position, int length) {
    if (m_view && !m_bPosSliderPressed) {
        if (m_media_manager->processes ().size () > 1)
            emit positioned (0, 0);
        else
            emit positioned (position, length);
    }
}

void PartBase::setLoaded (int percentage) {
    emit loading (percentage);
}

qlonglong PartBase::position () const {
    return m_source ? 100 * m_source->position () : 0;
}

void PartBase::pause () {
    NodePtr doc = m_source ? m_source->document () : nullptr;
    if (doc) {
        Mrl *mrl = nullptr;
        NodePtrW cur = m_source->current ();
        if (cur) {
            mrl = cur->mrl ();
            if (mrl && Mrl::WindowMode == mrl->view_mode)
                mrl = nullptr;
        }
        if (doc->state == Node::state_deferred) {
            doc->undefer ();
            if (cur && mrl && mrl->state == Node::state_deferred)
                mrl->undefer ();
        } else {
            doc->defer ();
            if (cur && mrl && mrl->unfinished ())
                mrl->defer ();
        }
    }
}

void PartBase::back () {
    m_source->backward ();
}

void PartBase::forward () {
    m_source->forward ();
}

void PartBase::playListItemClicked (const QModelIndex& index) {
    if (!index.isValid ())
        return;
    PlayListView *pv = qobject_cast <PlayListView *> (sender ());
    if (pv->model ()->rowCount ()) {
        if (pv->isExpanded (index))
            pv->setExpanded (index, false);
        else
            pv->setExpanded (index, true);
    }
}

void PartBase::playListItemActivated(const QModelIndex &index) {
    if (m_in_update_tree) return;
    if (m_view->editMode ()) return;
    PlayListView *pv = qobject_cast <PlayListView *> (sender ());
    if (!pv->model ()->parent (index).isValid () && index.row ())
        return; // handled by playListItemClicked
    PlayItem *vi = static_cast <PlayItem *> (index.internalPointer ());
    TopPlayItem *ri = vi->rootItem ();
    if (vi->node) {
        QString src = ri->source;
        NodePtrW node = vi->node;
        //qCDebug(LOG_KMPLAYER_COMMON) << src << " " << vi->node->nodeName();
        Source * source = src.isEmpty() ? m_source : m_sources[src.toAscii().constData()];
        if (node->isPlayable () || id_node_playlist_item == node->id) {
            source->play (node->mrl ()); //may become !isPlayable by lazy loading
            if (node && !node->isPlayable ())
                emit treeChanged (ri->id, node, nullptr, false, true);
        } // else if (vi->childCount ()) {handled by playListItemClicked
    } else if (vi->attribute) {
        if (vi->attribute->name () == Ids::attr_src ||
                vi->attribute->name () == Ids::attr_href ||
                vi->attribute->name () == Ids::attr_url ||
                vi->attribute->name () == Ids::attr_value ||
                vi->attribute->name () == "data") {
            QString src (vi->attribute->value ());
            if (!src.isEmpty ()) {
                PlayItem *pi = vi->parent ();
                if (pi) {
                    for (Node *e = pi->node.ptr (); e; e = e->parentNode ()) {
                        Mrl * mrl = e->mrl ();
                        if (mrl)
                            src = KUrl (mrl->absolutePath (), src).url ();
                    }
                    KUrl url (src);
                    if (url.isValid ())
                        openUrl (url);
                }
            }
        }
    } else
        emit treeChanged (ri->id, ri->node, nullptr, false, false);
    if (m_view)
        m_view->viewArea ()->setFocus ();
}

void PartBase::updateTree (bool full, bool force) {
    if (force) {
        m_in_update_tree = true;
        if (m_update_tree_full) {
            if (m_source)
                emit treeChanged (0, m_source->root (), m_source->current (), true, false);
        }
        m_in_update_tree = false;
        if (m_update_tree_timer) {
            killTimer (m_update_tree_timer);
            m_update_tree_timer = 0;
        }
    } else if (!m_update_tree_timer) {
        m_update_tree_timer = startTimer (100);
        m_update_tree_full = full;
    } else
        m_update_tree_full |= full;
}

void PartBase::updateInfo (const QString & msg) {
    emit infoUpdated (msg);
}

void PartBase::updateStatus (const QString & msg) {
    emit statusUpdated (msg);
}

void PartBase::setLanguages (const QStringList & al, const QStringList & sl) {
    emit languagesUpdated (al, sl);
}

void PartBase::audioSelected (QAction* act) {
    emit panelActionToggled(act);
    int i = act->parentWidget()->actions().indexOf(act);
    if (i >= 0)
        emit audioIsSelected (i);
}

void PartBase::subtitleSelected (QAction* act) {
    emit panelActionToggled(act);
    int i = act->parentWidget()->actions().indexOf(act);
    if (i >= 0)
        emit subtitleIsSelected (i);
}

void PartBase::recorderPlaying () {
    stop ();
    m_view->controlPanel ()->setRecording (true);
    emit recording (true);
}

void PartBase::recorderStopped () {
    stopRecording ();
    if (m_view && m_rec_timer < 0 && m_record_doc)
        openUrl(KUrl(convertNode <RecordDocument> (m_record_doc)->record_file));
}

void PartBase::stopRecording () {
    if (m_view) {
        m_view->controlPanel ()->setRecording (false);
        emit recording (false);
        if (m_record_doc && m_record_doc->active ()) {
            m_record_doc->deactivate ();
            if (m_rec_timer > 0)
                killTimer (m_rec_timer);
            m_rec_timer = 0;
        }
    }
}

bool PartBase::isRecording ()
{
    return m_record_doc && m_record_doc->active ();
}

void PartBase::record () {
    if (m_view) m_view->setCursor (QCursor (Qt::WaitCursor));
    if (!m_view->controlPanel()->button(ControlPanel::button_record)->isChecked ()) {
        stopRecording ();
    } else {
        m_settings->show  ("RecordPage");
        m_view->controlPanel ()->setRecording (false);
    }
    if (m_view) m_view->setCursor (QCursor (Qt::ArrowCursor));
}

void PartBase::record (const QString &src, const QString &f, const QString &rec, int auto_start)
{
    if (m_record_doc) {
        if (m_record_doc->active ())
            m_record_doc->reset ();
        m_record_doc->document ()->dispose ();
    }
    m_record_doc = new RecordDocument (src, f, rec, source ());
    m_record_doc->activate ();
    if (auto_start > 0)
        m_rec_timer = startTimer (auto_start);
    else
        m_rec_timer = auto_start;
}

void PartBase::play () {
    if (!m_view)
        return;
    QPushButton *pb = ::qobject_cast <QPushButton *> (sender ());
    if (pb && !pb->isChecked ()) {
        stop ();
        return;
    }
    if (m_update_tree_timer) {
        killTimer (m_update_tree_timer);
        m_update_tree_timer = 0;
    }
    if (!playing ()) {
        PlayItem *lvi = m_view->playList ()->selectedItem ();
        if (lvi) {
            TopPlayItem *ri = lvi->rootItem ();
            if (ri->id != 0) // make sure it's in the first tree
                lvi = nullptr;
        }
        if (!lvi) {
            QModelIndex index = m_view->playList ()->model ()->index (0, 0);
            lvi = static_cast<PlayItem*>(index.internalPointer ());
            if (!lvi->node)
                lvi = nullptr;
        }
        if (lvi) {
            Mrl *mrl = nullptr;
            for (Node * n = lvi->node.ptr (); n; n = n->parentNode ()) {
                if (n->isPlayable ()) {
                    mrl = n->mrl ();
                    break;
                }
                if (!mrl && n->mrl () && !n->mrl ()->src.isEmpty ())
                    mrl = n->mrl ();
            }
            if (mrl)
                m_source->play (mrl);
        }
    } else {
        m_source->play (nullptr);
    }
}

bool PartBase::playing () const {
    return m_source && m_source->document ()->active ();
}

void PartBase::stop () {
    QPushButton * b = m_view ? m_view->controlPanel ()->button (ControlPanel::button_stop) : nullptr;
    if (b) {
        if (!b->isChecked ())
            b->toggle ();
        m_view->setCursor (QCursor (Qt::WaitCursor));
    }
    if (m_source)
        m_source->reset ();
    MediaManager::ProcessInfoMap &pi = m_media_manager->processInfos ();
    const MediaManager::ProcessInfoMap::const_iterator ie = pi.constEnd();
    for (MediaManager::ProcessInfoMap::const_iterator i = pi.constBegin(); i != ie; ++i)
        i.value ()->quitProcesses ();
    MediaManager::ProcessList &processes = m_media_manager->processes ();
    const MediaManager::ProcessList::const_iterator e = processes.constEnd();
    for (MediaManager::ProcessList::const_iterator i = processes.constBegin(); i != e; ++i)
        (*i)->quit ();
    if (m_view) {
        m_view->setCursor (QCursor (Qt::ArrowCursor));
        if (b->isChecked ())
            b->toggle ();
        m_view->controlPanel ()->setPlaying (false);
        setLoaded (100);
        updateStatus (i18n ("Ready"));
    }
    playingStopped ();
}

void PartBase::seek (qlonglong msec) {
    if (m_media_manager->processes ().size () == 1)
        m_media_manager->processes ().first ()->seek (msec/100, true);
}

void PartBase::adjustVolume (int incdec) {
    if (m_media_manager->processes ().size () > 0)
        m_media_manager->processes ().first ()->volume (incdec, false);
}

void PartBase::increaseVolume () {
    /*if (m_view)
        m_view->controlPanel ()->volumeBar ()->setValue (m_view->controlPanel ()->volumeBar ()->value () + 2);*/
}

void PartBase::decreaseVolume () {
    //if (m_view)
    //    m_view->controlPanel ()->volumeBar ()->setValue (m_view->controlPanel ()->volumeBar ()->value () - 2);
}

void PartBase::posSliderPressed () {
    m_bPosSliderPressed=true;
}

void PartBase::posSliderReleased () {
    m_bPosSliderPressed=false;
    const QSlider * posSlider = ::qobject_cast<const QSlider *> (sender ());
    if (m_media_manager->processes ().size () == 1)
        m_media_manager->processes ().first ()->seek (posSlider->value(), true);
}

void PartBase::volumeChanged (int val) {
    if (m_media_manager->processes ().size () > 0) {
        m_settings->volume = val;
        m_media_manager->processes ().first ()->volume (val, true);
    }
}

void PartBase::contrastValueChanged (int val) {
    if (m_media_manager->processes ().size () > 0)
        m_media_manager->processes ().first ()->contrast (val, true);
}

void PartBase::brightnessValueChanged (int val) {
    if (m_media_manager->processes ().size () > 0)
        m_media_manager->processes ().first ()->brightness (val, true);
}

void PartBase::hueValueChanged (int val) {
    if (m_media_manager->processes ().size () > 0)
        m_media_manager->processes ().first ()->hue (val, true);
}

void PartBase::saturationValueChanged (int val) {
    m_settings->saturation = val;
    if (m_media_manager->processes ().size () > 0)
        m_media_manager->processes ().first ()->saturation (val, true);
}

void PartBase::sourceHasChangedAspects () {
    emit sourceDimensionChanged ();
}

void PartBase::positionValueChanged (int pos) {
    QSlider * slider = ::qobject_cast <QSlider *> (sender ());
    if (m_media_manager->processes ().size () == 1 &&
            slider && slider->isEnabled ())
        m_media_manager->processes ().first ()->seek (pos, true);
}

void PartBase::fullScreen () {
    if (m_view)
        m_view->fullScreen ();
}

void PartBase::toggleFullScreen () {
    m_view->fullScreen ();
}

bool PartBase::isPlaying () {
    return playing ();
}

KAboutData* PartBase::createAboutData () {
    KMessageBox::error(nullptr, "createAboutData", "KMPlayer");
    return nullptr;
}

//-----------------------------------------------------------------------------

SourceDocument::SourceDocument (Source *s, const QString &url)
    : Document (url, s), m_source (s) {}

void SourceDocument::message (MessageType msg, void *data) {
    switch (msg) {

    case MsgInfoString: {
        QString info (data ? *((QString *) data) : QString ());
        m_source->player ()->updateInfo (info);
        return;
    }

    case MsgAccessKey:
        for (Connection *c = m_KeyListeners.first(); c; c = m_KeyListeners.next ())
            if (c->payload && c->connecter) {
                KeyLoad *load = (KeyLoad *) c->payload;
                if (load->key == (int) (long) data)
                    post (c->connecter, new Posting (this, MsgAccessKey));
            }
        return;

    default:
        break;
    }
    Document::message (msg, data);
}

void *SourceDocument::role (RoleType msg, void *data) {
    switch (msg) {

    case RoleMediaManager:
        return m_source->player ()->mediaManager ();

    case RoleChildDisplay: {
        PartBase *p = m_source->player ();
        if (p->view ())
            return p->viewWidget ()->viewArea ()->getSurface ((Mrl *) data);
        return nullptr;
    }

    case RoleReceivers:

        switch ((MessageType) (long) data) {

        case MsgAccessKey:
            return &m_KeyListeners;

        case MsgSurfaceUpdate: {
            PartBase *p = m_source->player ();
            if (p->view ())
                return p->viewWidget ()->viewArea ()->updaters ();
        }
        // fall through

        default:
            break;
        }
        // fall through

    default:
        break;
    }
    return Document::role (msg, data);
}


Source::Source (const QString&, PartBase * player, const char * n)
 : QObject (player),
   m_name (n), m_player (player),
   m_identified (false), m_auto_play (true), m_avoid_redirects (false),
   m_frequency (0), m_xvport (0), m_xvencoding (-1), m_doc_timer (0) {
    init ();
}

Source::~Source () {
    if (m_document)
        m_document->document ()->dispose ();
    m_document = nullptr;
}

void Source::init () {
    //setDimensions (320, 240);
    m_width = 0;
    m_height = 0;
    m_aspect = 0.0;
    m_length = 0;
    m_audio_id = -1;
    m_subtitle_id = -1;
    m_position = 0;
    setLength (m_document, 0);
    m_recordcmd.truncate (0);
}

void Source::setLanguages (LangInfoPtr audio, LangInfoPtr sub)
{
    m_audio_infos = audio;
    m_subtitle_infos = sub;

    QStringList alst;
    QStringList slst;
    for (LangInfoPtr li = audio; li; li = li->next)
        alst.push_back (li->name);
    for (LangInfoPtr li = sub; li; li = li->next)
        slst.push_back (li->name);

    m_player->setLanguages (alst, slst);
}

void Source::setDimensions (NodePtr node, int w, int h) {
    Mrl *mrl = node ? node->mrl () : nullptr;
    if (mrl) {
        float a = h > 0 ? 1.0 * w / h : 0.0;
        mrl->size = SSize (w, h);
        mrl->aspect = a;
        bool ev = (w > 0 && h > 0) ||
            (h == 0 && m_height > 0) ||
            (w == 0 && m_width > 0);
        if (Mrl::SingleMode == mrl->view_mode) {
            m_width = w;
            m_height = h;
        }
        if (Mrl::WindowMode == mrl->view_mode || m_aspect < 0.001)
            setAspect (node, h > 0 ? 1.0 * w / h : 0.0);
            //qCDebug(LOG_KMPLAYER_COMMON) << "setDimensions " << w << "x" << h << " a:" << m_aspect;
        else if (ev)
            emit dimensionsChanged ();
    }
}

void Source::setAspect (NodePtr node, float a) {
    //qCDebug(LOG_KMPLAYER_COMMON) << "setAspect " << a;
    Mrl *mrl = node ? node->mrl () : nullptr;
    bool changed = false;
    if (mrl &&
            mrl->media_info &&
            mrl->media_info->media &&
            MediaManager::AudioVideo == mrl->media_info->type) {
        static_cast <AudioVideoMedia*>(mrl->media_info->media)->viewer ()->setAspect(a);
        if (mrl->view_mode == Mrl::WindowMode)
            changed |= (fabs (mrl->aspect - a) > 0.001);
        mrl->aspect = a;
    }
    if (!mrl || mrl->view_mode == Mrl::SingleMode) {
        changed |= (fabs (m_aspect - a) > 0.001);
        m_aspect = a;
        if (changed && m_player->view ())
            m_player->viewWidget ()->viewArea ()->resizeEvent (nullptr);

    } else {
       mrl->message (MsgSurfaceBoundsUpdate);
    }
    if (changed)
        emit dimensionsChanged ();
}

void Source::setLength (NodePtr, int len) {
    m_length = len;
    m_player->setPosition (m_position, m_length);
}

void Source::setPosition (int pos) {
    m_position = pos;
    m_player->setPosition (pos, m_length);
}

void Source::setLoading (int percentage) {
    m_player->setLoaded (percentage);
}

/*
static void printTree (NodePtr root, QString off=QString()) {
    if (!root) {
        qCDebug(LOG_KMPLAYER_COMMON) << off << "[null]";
        return;
    }
    qCDebug(LOG_KMPLAYER_COMMON) << off << root->nodeName() << " " << (Element*)root << (root->isPlayable() ? root->mrl ()->src : QString ("-"));
    off += QString ("  ");
    for (NodePtr e = root->firstChild(); e; e = e->nextSibling())
        printTree(e, off);
}*/

void Source::setUrl (const QString &url) {
    qCDebug(LOG_KMPLAYER_COMMON) << url;
    m_url = KUrl (url);
    if (m_document && !m_document->hasChildNodes () &&
            (m_document->mrl()->src.isEmpty () ||
             m_document->mrl()->src == url))
        // special case, mime is set first by plugin FIXME v
        m_document->mrl()->src = url;
    else {
        if (m_document)
            m_document->document ()->dispose ();
        m_document = new SourceDocument (this, url);
    }
    if (m_player->source () == this)
        m_player->updateTree ();

    QTimer::singleShot (0, this, SLOT(changedUrl ()));
}

void Source::changedUrl()
{
    emit titleChanged (this->prettyName ());
}

void Source::setTitle (const QString & title) {
    emit titleChanged (title);
}

void Source::setAudioLang (int id) {
    LangInfoPtr li = m_audio_infos;
    for (; id > 0 && li; li = li->next)
        id--;
    m_audio_id = li ? li->id : -1;
    if (m_player->view () && m_player->mediaManager ()->processes ().size ())
        m_player->mediaManager ()->processes ().first ()->setAudioLang (m_audio_id);
}

void Source::setSubtitle (int id) {
    LangInfoPtr li = m_subtitle_infos;
    for (; id > 0 && li; li = li->next)
        id--;
    m_subtitle_id = li ? li->id : -1;
    if (m_player->view () && m_player->mediaManager ()->processes ().size ())
        m_player->mediaManager ()->processes ().first ()->setSubtitle (m_subtitle_id);
}

void Source::reset () {
    if (m_document) {
        qCDebug(LOG_KMPLAYER_COMMON) << "Source::reset " << name () << endl;
        NodePtr doc = m_document; // avoid recursive calls
        m_document = nullptr;
        doc->reset ();
        m_document = doc;
        m_player->updateTree ();
    }
    init ();
}

void Source::play (Mrl *mrl) {
    if (!mrl)
        mrl = document ()->mrl ();
    NodePtrW guarded = mrl;
    blockSignals (true); //endOfPlayItems, but what is hyperspace?
    document ()->reset ();
    blockSignals (false);
    mrl = guarded ? guarded->mrl () : m_document->mrl ();
    if (!mrl)
        return;
    m_width = m_height = 0;
    m_player->changeURL (mrl->src);
    for (Node *p = mrl->parentNode(); p; p = p->parentNode())
        p->state = Element::state_activated;
    mrl->activate ();
    m_width = mrl->size.width;
    m_height = mrl->size.height;
    m_aspect = mrl->aspect;
    //qCDebug(LOG_KMPLAYER_COMMON) << "Source::playCurrent " << (m_current ? m_current->nodeName():" doc act:") <<  (m_document && !m_document->active ()) << " cur:" << (!m_current)  << " cur act:" << (m_current && !m_current->active ());
    m_player->updateTree ();
    emit dimensionsChanged ();
}

bool Source::authoriseUrl (const QString &) {
    return true;
}

void Source::setTimeout (int ms) {
    //qCDebug(LOG_KMPLAYER_COMMON) << "Source::setTimeout " << ms;
    if (m_doc_timer)
        killTimer (m_doc_timer);
    m_doc_timer = ms > -1 ? startTimer (ms) : 0;
}

void Source::timerEvent (QTimerEvent * e) {
    if (e->timerId () == m_doc_timer && m_document && m_document->active ())
        m_document->document ()->timer (); // will call setTimeout()
    else
        killTimer (e->timerId ());
}

void Source::setCurrent (Mrl *mrl) {
    m_current = mrl;
    m_width = mrl->size.width;
    m_height = mrl->size.height;
    m_aspect = mrl->aspect;
}

void Source::stateElementChanged (Node *elm, Node::State os, Node::State ns) {
    //qCDebug(LOG_KMPLAYER_COMMON) << "[01;31mSource::stateElementChanged[00m " << elm->nodeName () << " state:" << (int) elm->state << " cur isPlayable:" << (m_current && m_current->isPlayable ()) << " elm==linkNode:" << (m_current && elm == m_current->mrl ()->linkNode ()) << endl;
    if (ns == Node::state_activated &&
            elm->mrl ()) {
        if (Mrl::WindowMode != elm->mrl ()->view_mode &&
                (!elm->parentNode () ||
                 !elm->parentNode ()->mrl () ||
                 Mrl::WindowMode != elm->parentNode ()->mrl ()->view_mode))
            setCurrent (elm->mrl ());
        if (m_current.ptr () == elm)
            emit startPlaying ();
    } else if (ns == Node::state_deactivated) {
        if (elm == m_document) {
            NodePtrW guard = elm;
            emit endOfPlayItems (); // played all items FIXME on jumps
            if (!guard)
                return;
        } else if (m_current.ptr () == elm) {
            emit stopPlaying ();
        }
    }
    if (elm->role (RolePlaylist)) {
        if (ns == Node::state_activated || ns == Node::state_deactivated)
            m_player->updateTree ();
        else if (ns == Node::state_began || os == Node::state_began)
            m_player->updateTree (false);
    }
}

void Source::bitRates (int & preferred, int & maximal) {
    preferred = 1024 * m_player->settings ()->prefbitrate;
    maximal= 1024 * m_player->settings ()->maxbitrate;
}

void Source::openUrl (const QUrl &url, const QString &t, const QString &srv) {
    m_player->openUrl (url, t, srv);
}

void Source::enableRepaintUpdaters (bool enable, unsigned int off_time) {
    if (m_player->view ())
        m_player->viewWidget ()->viewArea()->enableUpdaters (enable, off_time);
}

void Source::insertURL (NodePtr node, const QString & mrl, const QString & title) {
    if (!node || !node->mrl ()) // this should always be false
        return;
    QString cur_url = node->mrl ()->absolutePath ();
    KUrl url (cur_url, mrl);
    QString urlstr = QUrl::fromPercentEncoding (url.url ().toUtf8 ());
    qCDebug(LOG_KMPLAYER_COMMON) << cur_url << " " << urlstr;
    if (!url.isValid ())
        qCCritical(LOG_KMPLAYER_COMMON) << "try to append non-valid url" << endl;
    else if (QUrl::fromPercentEncoding (cur_url.toUtf8 ()) == urlstr)
        qCCritical(LOG_KMPLAYER_COMMON) << "try to append url to itself" << endl;
    else {
        int depth = 0; // cache this?
        for (Node *e = node; e->parentNode (); e = e->parentNode ())
            ++depth;
        if (depth < 40) {
            node->appendChild (new GenericURL (m_document, urlstr, title.isEmpty() ? QUrl::fromPercentEncoding (mrl.toUtf8 ()) : title));
            m_player->updateTree ();
        } else
            qCCritical(LOG_KMPLAYER_COMMON) << "insertURL exceeds depth limit" << endl;
    }
}

void Source::backward () {
    Node *back = m_current ? m_current.ptr () : m_document.ptr ();
    while (back && back != m_document.ptr ()) {
        if (back->previousSibling ()) {
            back = back->previousSibling ();
            while (!back->isPlayable () && back->lastChild ())
                back = back->lastChild ();
            if (back->isPlayable () && !back->active ()) {
                play (back->mrl ());
                break;
            }
        } else {
            back = back->parentNode();
        }
    }
}

void Source::forward () {
    if (m_current)
        m_current->finish ();
    if (m_document && !m_document->active ())
        play (m_document->mrl ());
}

void Source::setDocument (KMPlayer::NodePtr doc, KMPlayer::NodePtr cur) {
    if (m_document)
        m_document->document()->dispose ();
    m_document = doc;
    setCurrent (cur->mrl ());
    //qCDebug(LOG_KMPLAYER_COMMON) << "setDocument: " << m_document->outerXML ();
}

NodePtr Source::document () {
    if (!m_document)
        m_document = new SourceDocument (this, QString ());
    return m_document;
}

NodePtr Source::root () {
    return document ();
}

bool Source::processOutput (const QString &) {
    return false;
}

QString Source::filterOptions () {
    Settings* m_settings = m_player->settings ();
    QString PPargs ("");
    if (m_settings->postprocessing)
    {
        if (m_settings->pp_default)
            PPargs = "-vf pp=de";
        else if (m_settings->pp_fast)
            PPargs = "-vf pp=fa";
        else if (m_settings->pp_custom) {
            PPargs = "-vf pp=";
            if (m_settings->pp_custom_hz) {
                PPargs += "hb";
                if (m_settings->pp_custom_hz_aq && \
                        m_settings->pp_custom_hz_ch)
                    PPargs += ":ac";
                else if (m_settings->pp_custom_hz_aq)
                    PPargs += ":a";
                else if (m_settings->pp_custom_hz_ch)
                    PPargs += ":c";
                PPargs += '/';
            }
            if (m_settings->pp_custom_vt) {
                PPargs += "vb";
                if (m_settings->pp_custom_vt_aq && \
                        m_settings->pp_custom_vt_ch)
                    PPargs += ":ac";
                else if (m_settings->pp_custom_vt_aq)
                    PPargs += ":a";
                else if (m_settings->pp_custom_vt_ch)
                    PPargs += ":c";
                PPargs += '/';
            }
            if (m_settings->pp_custom_dr) {
                PPargs += "dr";
                if (m_settings->pp_custom_dr_aq && \
                        m_settings->pp_custom_dr_ch)
                    PPargs += ":ac";
                else if (m_settings->pp_custom_dr_aq)
                    PPargs += ":a";
                else if (m_settings->pp_custom_dr_ch)
                    PPargs += ":c";
                PPargs += '/';
            }
            if (m_settings->pp_custom_al) {
                PPargs += "al";
                if (m_settings->pp_custom_al_f)
                    PPargs += ":f";
                PPargs += '/';
            }
            if (m_settings->pp_custom_tn) {
                PPargs += "tn";
                /*if (1 <= m_settings->pp_custom_tn_s <= 3){
                    PPargs += ":";
                    PPargs += m_settings->pp_custom_tn_s;
                    }*/ //disabled 'cos this is wrong
                PPargs += '/';
            }
            if (m_settings->pp_lin_blend_int) {
                PPargs += "lb";
                PPargs += '/';
            }
            if (m_settings->pp_lin_int) {
                PPargs += "li";
                PPargs += '/';
            }
            if (m_settings->pp_cub_int) {
                PPargs += "ci";
                PPargs += '/';
            }
            if (m_settings->pp_med_int) {
                PPargs += "md";
                PPargs += '/';
            }
            if (m_settings->pp_ffmpeg_int) {
                PPargs += "fd";
                PPargs += '/';
            }
        }
        if (PPargs.endsWith("/"))
            PPargs.truncate(PPargs.size ()-1);
    }
    return PPargs;
}

bool Source::hasLength () {
    return true;
}

bool Source::isSeekable () {
    return true;
}

void Source::setIdentified (bool b) {
    //qCDebug(LOG_KMPLAYER_COMMON) << "Source::setIdentified " << m_identified << b;
    m_identified = b;
    if (!b) {
        m_audio_infos = nullptr;
        m_subtitle_infos = nullptr;
    }
}

QString Source::plugin (const QString &mime) const {
    return KConfigGroup (m_player->config (), mime).readEntry ("plugin", QString ());
}

QString Source::prettyName () {
    return i18n ("Unknown");
}

void Source::slotActivate ()
{
    activate ();
}

//-----------------------------------------------------------------------------

URLSource::URLSource (PartBase * player, const KUrl & url)
    : Source (i18n ("URL"), player, "urlsource"), activated (false) {
    setUrl (url.url ());
    //qCDebug(LOG_KMPLAYER_COMMON) << "URLSource::URLSource";
}

URLSource::~URLSource () {
    //qCDebug(LOG_KMPLAYER_COMMON) << "URLSource::~URLSource";
}

void URLSource::init () {
    Source::init ();
}

void URLSource::dimensions (int & w, int & h) {
    if (!m_player->mayResize () && m_player->view ()) {
        w = static_cast <View *> (m_player->view ())->viewArea ()->width ();
        h = static_cast <View *> (m_player->view ())->viewArea ()->height ();
    } else {
        Source::dimensions (w, h);
    }
}

bool URLSource::hasLength () {
    return !!length ();
}

void URLSource::activate () {
    if (activated)
        return;
    activated = true;
    if (url ().isEmpty () && (!m_document || !m_document->hasChildNodes ())) {
        m_player->updateTree ();
        return;
    }
    if (m_auto_play)
        play (nullptr);
}

void URLSource::reset () {
    Source::reset ();
}

void URLSource::forward () {
    Source::forward ();
}

void URLSource::backward () {
    Source::backward ();
}

void URLSource::play (Mrl *mrl) {
    Source::play (mrl);
}

void URLSource::deactivate () {
    if (!activated)
        return;
    activated = false;
    reset ();
    if (m_document) {
        m_document->document ()->dispose ();
        m_document = nullptr;
    }
    if (m_player->view ())
        m_player->viewWidget ()->viewArea ()->getSurface (nullptr);
}

QString URLSource::prettyName () {
    if (m_url.isEmpty ())
        return i18n ("URL");
    if (m_url.url ().size () > 50) {
        QString newurl;
        if (!m_url.isLocalFile ()) {
            newurl = m_url.scheme() + QString ("://");
            if (m_url.hasHost ())
                newurl += m_url.host ();
            if (m_url.port () != -1)
                newurl += QString (":%1").arg (m_url.port ());
        }
        QString file = m_url.fileName ();
        int len = newurl.size () + file.size ();
        KUrl path = KUrl (m_url.directory ());
        bool modified = false;
        while (path.url ().size () + len > 50 && path != path.upUrl ()) {
            path = path.upUrl ();
            modified = true;
        }
        QString dir = path.directory ();
        if (!dir.endsWith (QString ("/")))
            dir += '/';
        if (modified)
            dir += QString (".../");
        newurl += dir + file;
        return i18n ("URL - ") + newurl;
    }
    if (m_url.isLocalFile())
        return i18n ("URL - ") + m_url.toLocalFile();
    return i18n ("URL - ") + m_url.toDisplayString();
}

bool URLSource::authoriseUrl (const QString &url) {
    KUrl base = document ()->mrl ()->src;
    if (base != url) {
        KUrl dest (url);
        // check if some remote playlist tries to open something local, but
        // do ignore unknown protocols because there are so many and we only
        // want to cache local ones.
        if (
#if 0
            !KProtocolInfo::protocolClass (dest.scheme()).isEmpty () &&
#else
            dest.isLocalFile () &&
#endif
                !KUrlAuthorized::authorizeUrlAction ("redirect", base, dest)) {
            qCWarning(LOG_KMPLAYER_COMMON) << "requestPlayURL from document " << base << " to play " << dest << " is not allowed";
            return false;
        }
    }
    return Source::authoriseUrl (url);
}

void URLSource::setUrl (const QString &url) {
    Source::setUrl (url);
    Mrl *mrl = document ()->mrl ();
    if (!url.isEmpty () && m_url.isLocalFile () && mrl->mimetype.isEmpty ()) {
        const QMimeType mimeType = QMimeDatabase().mimeTypeForUrl(m_url);
        if (mimeType.isValid())
            mrl->mimetype = mimeType.name();
    }
}

//-----------------------------------------------------------------------------
