/**
 * Copyright (C) 2002-2003 by Koos Vriezen <koos.vriezen@gmail.com>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License version 2 as published by the Free Software Foundation.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Steet, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 **/

#ifdef KDE_USE_FINAL
#undef Always
#endif

#include <config.h>

#include <math.h>

#include <qapplication.h>
#include <QByteArray>
#include <qcursor.h>
#include <qtimer.h>
#include <qpair.h>
#include <qpushbutton.h>
#include <QMenu>
#include <qslider.h>
#include <qfile.h>
#include <qregexp.h>
#include <qtextstream.h>

#include <kmessagebox.h>
#include <kaboutdata.h>
#include <kdebug.h>
#include <kbookmarkmenu.h>
#include <kbookmarkmanager.h>
#include <kbookmark.h>
#include <kconfig.h>
#include <ksimpleconfig.h>
#include <kaction.h>
#include <k3process.h>
#include <kstandarddirs.h>
#include <kmimetype.h>
#include <kprotocolinfo.h>
#include <kauthorized.h>
#include <kio/job.h>
#include <kio/jobclasses.h>

#include "kmplayerpartbase.h"
#include "kmplayerview.h"
#include "playlistview.h"
#include "viewarea.h"
#include "kmplayercontrolpanel.h"
#include "kmplayerconfig.h"
#include "kmplayerprocess.h"
#include "kmplayer_smil.h"

namespace KMPlayer {

class KMPLAYER_NO_EXPORT BookmarkOwner : public KBookmarkOwner {
public:
    BookmarkOwner (PartBase *);
    KDE_NO_CDTOR_EXPORT virtual ~BookmarkOwner () {}
    void openBookmark(const KBookmark &bm, Qt::MouseButtons mb, Qt::KeyboardModifiers km);
    QString currentTitle() const;
    QString currentURL() const;
private:
    PartBase * m_player;
};

} // namespace

using namespace KMPlayer;

static KMPlayer::DataCache *memory_cache;

KDE_NO_CDTOR_EXPORT BookmarkOwner::BookmarkOwner (PartBase * player)
    : m_player (player) {}

KDE_NO_EXPORT void BookmarkOwner::openBookmark(const KBookmark &bm, Qt::MouseButtons, Qt::KeyboardModifiers) {
    if (!bm.isNull ())
        m_player->openURL (bm.url ());
}

KDE_NO_EXPORT QString BookmarkOwner::currentTitle () const {
    return m_player->source ()->prettyName ();
}

KDE_NO_EXPORT QString BookmarkOwner::currentURL () const {
    return m_player->source ()->url ().url ();
}

//-----------------------------------------------------------------------------

PartBase::PartBase (QWidget * wparent, QObject * parent, KSharedConfigPtr config)
 : KMediaPlayer::Player (wparent, "kde_kmplayer_part", parent),
   m_config (config),
   m_view (new View (wparent)),
   m_settings (new Settings (this, config)),
   m_recorder (0L),
   m_source (0L),
   m_bookmark_menu (0L),
   m_record_timer (0),
   m_update_tree_timer (0),
   m_noresize (false),
   m_auto_controls (true),
   m_bPosSliderPressed (false),
   m_in_update_tree (false)
{
    if (!memory_cache)
        (void) new DataCache (&memory_cache);

    MPlayer *mplayer = new MPlayer (this, m_settings);
    m_players ["mplayer"] = mplayer;
    m_process = mplayer;
    //Xine * xine = new Xine (this, m_settings);
    //m_players ["xine"] = xine;
    //m_players ["gstreamer"] = new GStreamer (this, m_settings);
    m_recorders ["mencoder"] = new MEncoder (this, m_settings);
    m_recorders ["mplayerdumpstream"] = new MPlayerDumpstream(this, m_settings);
    m_recorders ["ffmpeg"] = new FFMpeg (this, m_settings);
    //m_recorders ["xine"] = xine;
    m_sources ["urlsource"] = new URLSource (this);

    QString bmfile = KStandardDirs::locate ("data", "kmplayer/bookmarks.xml");
    QString localbmfile = KStandardDirs::locateLocal ("data", "kmplayer/bookmarks.xml");
    if (localbmfile != bmfile) {
        kDebug () << "cp " << bmfile << " " << localbmfile << endl;
        K3Process p;
        p << "/bin/cp" << QFile::encodeName (bmfile) << QFile::encodeName (localbmfile);
        p.start (K3Process::Block);
    }
    m_bookmark_manager = KBookmarkManager::managerForFile (localbmfile, "kmplayer");
    m_bookmark_owner = new BookmarkOwner (this);
}

void PartBase::showConfigDialog () {
    m_settings->show ("URLPage");
}

KDE_NO_EXPORT void PartBase::showPlayListWindow () {
    // note, this is also the slot of application's view_playlist action, but
    // anyhow, actions don't work for the fullscreen out-of-the-box, so ...
    if (m_view->viewArea ()->isFullScreen ())
        fullScreen ();
    else if (m_view->viewArea ()->isMinimalMode ())
        ; //done by app: m_view->viewArea ()->minimalMode ();
    else
        m_view->toggleShowPlaylist ();
}

KDE_NO_EXPORT void PartBase::addBookMark (const QString & t, const QString & url) {
    KBookmarkGroup b = m_bookmark_manager->root ();
    b.addBookmark (t, KUrl (url));
    m_bookmark_manager->emitChanged (b);
}

void PartBase::init (KActionCollection * action_collection) {
    KParts::Part::setWidget (m_view);
    m_view->init (action_collection);
#ifdef HAVE_NSPR
    m_players ["npp"] = new NpPlayer (this, m_settings, m_service);
#endif
    connect(m_settings, SIGNAL(configChanged()), this, SLOT(settingsChanged()));
    m_settings->readConfig ();
    m_settings->applyColorSetting (false);
    m_bookmark_menu = new KBookmarkMenu (m_bookmark_manager, m_bookmark_owner, m_view->controlPanel ()->bookmarkMenu, action_collection);
    connect (m_view, SIGNAL (urlDropped (const KURL::List &)), this, SLOT (openURL (const KURL::List &)));
    connectPlaylist (m_view->playList ());
    connectInfoPanel (m_view->infoPanel ());
    //new KAction (i18n ("Edit playlist &item"), 0, 0, m_view->playList (), SLOT (editCurrent ()), action_collection, "edit_playlist_item");
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
    //connect (panel->volumeBar (), SIGNAL (volumeChanged (int)), this, SLOT (volumeChanged (int)));
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
    connect (panel->audioMenu, SIGNAL (activated (int)), this, SLOT (audioSelected (int)));
    connect (panel->subtitleMenu, SIGNAL (activated (int)), this, SLOT (subtitleSelected (int)));
    connect (this, SIGNAL (audioIsSelected (int)), panel, SLOT (selectAudioLanguage (int)));
    connect (this, SIGNAL (subtitleIsSelected (int)), panel, SLOT (selectSubtitle (int)));
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

void PartBase::connectPlaylist (PlayListView * playlist) {
    connect (playlist, SIGNAL (addBookMark (const QString &, const QString &)),
             this, SLOT (addBookMark (const QString &, const QString &)));
    connect (playlist, SIGNAL (executed (Q3ListViewItem *)),
             this, SLOT (playListItemExecuted (Q3ListViewItem *)));
    connect (playlist, SIGNAL (clicked (Q3ListViewItem *)),
             this, SLOT (playListItemClicked (Q3ListViewItem *)));
    connect (this, SIGNAL (treeChanged (int, NodePtr, NodePtr, bool, bool)),
             playlist, SLOT (updateTree (int, NodePtr, NodePtr, bool, bool)));
    connect (this, SIGNAL (treeUpdated ()),
             playlist, SLOT (triggerUpdate ()));
}

void PartBase::connectInfoPanel (InfoWindow * infopanel) {
    connect (this, SIGNAL (infoUpdated (const QString &)),
             infopanel->view (), SLOT (setInfoMessage (const QString &)));
}

PartBase::~PartBase () {
    kDebug() << "PartBase::~PartBase" << endl;
    m_view = (View*) 0;
    stop ();
    if (m_source)
        m_source->deactivate ();
    delete m_settings;
    delete m_bookmark_menu;
    delete m_bookmark_manager;
    delete m_bookmark_owner;
    memory_cache->unref ();
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

bool PartBase::setProcess (Mrl *mrl) {
    // determine backend, start with temp_backends
    QString p = temp_backends [m_source->name()];
    bool remember_backend = p.isEmpty ();
    bool changed = false;
    if (p.isEmpty ()) {
        // next try to find mimetype match from kmplayerrc
        if (!mrl->mimetype.isEmpty ()) {
            m_config->setGroup (mrl->mimetype);
            p = m_config->readEntry ("player", "" );
            remember_backend = !(!p.isEmpty () &&
                    m_players.contains (p) &&
                    m_players [p]->supports (m_source->name ()));
        }
    }
    if (p.isEmpty ())
        // try source match from kmplayerrc
        p = m_settings->backends [m_source->name()];
    if (p.isEmpty ()) {
        // try source match from kmplayerrc by re-reading
        m_config->setGroup (strGeneralGroup);
        p = m_config->readEntry (m_source->name (), "");
    }
    if (p.isEmpty () ||
            !m_players.contains (p) ||
            !m_players [p]->supports (m_source->name ())) {
        // finally find first supported player
        p.truncate (0);
        if (!m_process || !m_process->supports (m_source->name ())) {
            ProcessMap::const_iterator i, e = m_players.end();
            for (i = m_players.begin(); i != e; ++i)
                if (i.data ()->supports (m_source->name ())) {
                    p = QString (i.data ()->name ());
                    break;
                }
        } else
            p = QString (m_process->name ());
    }
    if (!p.isEmpty ()) {
        if (!m_process || p != m_process->name ()) {
            setProcess (p.ascii ());
            updatePlayerMenu (m_view->controlPanel ());
            changed = true;
        }
        if (remember_backend)
            m_settings->backends [m_source->name()] = m_process->name ();
        else
            temp_backends.remove (m_source->name());
    }
    return changed;
}

void PartBase::setProcess (const char * name) {
    Process * process = name ? m_players [name] : 0L;
    if (m_process == process)
        return;
    if (!m_source)
        m_source = m_sources ["urlsource"];
    Process * old_process = m_process;
    m_process = process;
    if (old_process && old_process->state () > Process::NotRunning)
        old_process->quit ();
    if (!m_process)
        return;
    m_process->setSource (m_source);
    if (m_process->playing ()) {
        m_view->controlPanel ()->setPlaying (true);
        m_view->controlPanel ()->showPositionSlider (!!m_source->length ());
        m_view->controlPanel ()->enableSeekButtons (m_source->isSeekable ());
    }
    emit processChanged (name);
}

void PartBase::setRecorder (const char * name) {
    Process * recorder = name ? m_recorders [name] : 0L;
    if (m_recorder == recorder)
        return;
    if (m_recorder)
        m_recorder->quit ();
    m_recorder = recorder;
}

KDE_NO_EXPORT void PartBase::slotPlayerMenu (int id) {
    bool playing = m_process->playing ();
    const char * srcname = m_source->name ();
    QMenu *menu = m_view->controlPanel ()->playerMenu;
    ProcessMap::const_iterator pi = m_players.begin(), e = m_players.end();
    unsigned i = 0;
    for (; pi != e && i < menu->count(); ++pi) {
        Process * proc = pi.data ();
        if (!proc->supports (srcname))
            continue;
        int menuid = menu->idAt (i);
        menu->setItemChecked (menuid, menuid == id);
        if (menuid == id) {
            if (proc->name () != QString ("npp"))
                m_settings->backends [srcname] = proc->name ();
            temp_backends [srcname] = proc->name ();
            if (playing && strcmp (m_process->name (), proc->name ()))
                m_process->quit ();
            setProcess (proc->name ());
        }
        ++i;
    }
    if (playing)
        setSource (m_source); // re-activate
}

void PartBase::updatePlayerMenu (ControlPanel * panel) {
    if (!m_view || !m_process)
        return;
    QMenu *menu = panel->playerMenu;
    menu->clear ();
    if (!m_source)
        return;
    const ProcessMap::const_iterator e = m_players.end();
    int id = 0; // if multiple parts, id's should be the same for all menu's
    for (ProcessMap::const_iterator i = m_players.begin(); i != e; ++i) {
        Process * p = i.data ();
        if (p->supports (m_source->name ())) {
            menu->insertItem (p->menuName (), this, SLOT (slotPlayerMenu (int)), 0, id++);
            if (i.data() == m_process)
                menu->setItemChecked (id-1, true);
        }
    }
}

void PartBase::connectSource (Source * old_source, Source * source) {
    if (old_source) {
        disconnect (old_source, SIGNAL(endOfPlayItems ()), this, SLOT(stop ()));
        disconnect (old_source, SIGNAL (dimensionsChanged ()),
                    this, SLOT (sourceHasChangedAspects ()));
        disconnect (old_source, SIGNAL (startPlaying ()),
                    this, SLOT (playingStarted ()));
        disconnect (old_source, SIGNAL (stopPlaying ()),
                    this, SLOT (playingStopped ()));
    }
    if (source) {
        connect (source, SIGNAL (endOfPlayItems ()), this, SLOT (stop ()));
        connect (source, SIGNAL (dimensionsChanged ()),
                this, SLOT (sourceHasChangedAspects ()));
        connect (source, SIGNAL (startPlaying()), this, SLOT(playingStarted()));
        connect (source, SIGNAL (stopPlaying ()), this, SLOT(playingStopped()));
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
        disconnect (m_source, SIGNAL (startRecording ()),
                    this, SLOT (recordingStarted ()));
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
    m_process->setSource (m_source);
    connect (m_source, SIGNAL(startRecording()), this,SLOT(recordingStarted()));
    connect (this, SIGNAL (audioIsSelected (int)),
             m_source, SLOT (setAudioLang (int)));
    connect (this, SIGNAL (subtitleIsSelected (int)),
             m_source, SLOT (setSubtitle (int)));
    m_source->init ();
    m_source->setIdentified (false);
    if (m_view && m_view->viewer ()) {
        updatePlayerMenu (m_view->controlPanel ());
        m_view->viewer ()->setAspect (0.0);
    }
    if (m_source) QTimer::singleShot (0, m_source, SLOT (activate ()));
    updateTree (true, true);
    emit sourceChanged (old_source, m_source);
}

KDE_NO_EXPORT void PartBase::changeURL (const QString & url) {
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

bool PartBase::openURL (const KURL & url) {
    kDebug () << "PartBase::openURL " << url.url() << url.isValid () << endl;
    if (!m_view) return false;
    stop ();
    Source * src = (url.isEmpty () ? m_sources ["urlsource"] : (!url.protocol ().compare ("kmplayer") && m_sources.contains (url.host ()) ? m_sources [url.host ()] : m_sources ["urlsource"]));
    src->setSubURL (KURL ());
    src->setURL (url);
    setSource (src);
    return true;
}

bool PartBase::openURL (const KURL::List & urls) {
    if (urls.size () == 1) {
        openURL (urls[0]);
    } else {
        openURL (KURL ());
        NodePtr d = m_source->document ();
        if (d)
            for (unsigned int i = 0; i < urls.size (); i++)
                d->appendChild (new GenericURL (d, KURL::decode_string (urls [i].url ())));
    }
    return true;
}

bool PartBase::closeURL () {
    stop ();
    if (m_view) {
        m_view->viewer ()->setAspect (0.0);
        m_view->reset ();
    }
    return true;
}

bool PartBase::openFile () {
    return false;
}

void PartBase::keepMovieAspect (bool b) {
    if (m_view) {
        m_view->setKeepSizeRatio (b);
        if (m_source)
            m_view->viewer ()->setAspect (b ? m_source->aspect () : 0.0);
    }
}

void PartBase::recordingStarted () {
    if (m_settings->replayoption == Settings::ReplayAfter)
        m_record_timer = startTimer (1000 * m_settings->replaytime);
}

void PartBase::recordingStopped () {
    killTimer (m_record_timer);
    m_record_timer = 0;
    Recorder * rec = dynamic_cast <Recorder*> (m_recorder);
    if (rec) {
        if (m_settings->replayoption == Settings::ReplayFinished ||
             (m_settings->replayoption == Settings::ReplayAfter && !playing ()))
            openURL (rec->recordURL ());
        rec->setURL (KURL ());
    }
    setRecorder ("mencoder"); //FIXME see PartBase::record() checking playing()
}

void PartBase::timerEvent (QTimerEvent * e) {
    if (e->timerId () == m_record_timer) {
        kDebug () << "record timer event" << (m_recorder->playing () && !playing ()) << endl;
        m_record_timer = 0;
        if (m_recorder->playing () && !playing ()) {
            Recorder * rec = dynamic_cast <Recorder*> (m_recorder);
            if (rec) {
                openURL (rec->recordURL ());
                rec->setURL (KURL ());
            }
        }
    } else if (e->timerId () == m_update_tree_timer) {
        m_update_tree_timer = 0;
        updateTree (m_update_tree_full, true);
    }
    killTimer (e->timerId ());
}

void PartBase::playingStarted () {
    //m_view->viewer ()->setAspect (m_source->aspect ());
    if (m_view) {
        m_view->controlPanel ()->setPlaying (true);
        m_view->controlPanel ()->showPositionSlider (!!m_source->length ());
        m_view->controlPanel ()->enableSeekButtons (m_source->isSeekable ());
        //if (m_settings->autoadjustvolume && m_process)
        //   m_process->volume(m_view->controlPanel()->volumeBar()->value(),true);
    }
    emit loading (100);
}

void PartBase::playingStopped () {
    kDebug () << "playingStopped " << this << endl;
    if (m_view) {
        m_view->controlPanel ()->setPlaying (false);
        m_view->reset ();
    }
    m_bPosSliderPressed = false;
}

KDE_NO_EXPORT void PartBase::setPosition (int position, int length) {
    if (m_view && !m_bPosSliderPressed)
        emit positioned (position, length);
}

void PartBase::setLoaded (int percentage) {
    emit loading (percentage);
}

qlonglong PartBase::position () const {
    return m_source ? 100 * m_source->position () : 0;
}

void PartBase::pause () {
    NodePtr doc = m_source ? m_source->document () : 0L;
    if (doc) {
        if (doc->state == Node::state_deferred)
            doc->undefer ();
        else
            doc->defer ();
    }
}

void PartBase::back () {
    m_source->backward ();
}

void PartBase::forward () {
    m_source->forward ();
}

KDE_NO_EXPORT void PartBase::playListItemClicked (Q3ListViewItem * item) {
    if (!item)
        return;
    PlayListItem * vi = static_cast <PlayListItem *> (item);
    RootPlayListItem * ri = vi->playListView ()->rootItem (item);
    if (ri == item && vi->node) {
        QString src = ri->source;
        //kDebug() << "playListItemClicked " << src << " " << vi->node->nodeName() << endl;
        Source * source = src.isEmpty() ? m_source : m_sources[src.ascii()];
        if (vi->node->isPlayable ()) {
            source->jump (vi->node); //may become !isPlayable by lazy loading
            if (!vi->node->isPlayable ())
                emit treeChanged (ri->id, vi->node, 0, false, true);
        } else if (vi->firstChild ())
            vi->listView ()->setOpen (vi, !vi->isOpen ());
    } else if (!vi->node && !vi->m_attr)
        updateTree (); // items already deleted
}

KDE_NO_EXPORT void PartBase::playListItemExecuted (Q3ListViewItem * item) {
    if (m_in_update_tree) return;
    if (m_view->editMode ()) return;
    PlayListItem * vi = static_cast <PlayListItem *> (item);
    RootPlayListItem * ri = vi->playListView ()->rootItem (item);
    if (ri == item)
        return; // both null or handled by playListItemClicked
    if (vi->node) {
        QString src = ri->source;
        //kDebug() << "playListItemExecuted " << src << " " << vi->node->nodeName() << endl;
        Source * source = src.isEmpty() ? m_source : m_sources[src.ascii()];
        if (vi->node->isPlayable ()) {
            source->jump (vi->node); //may become !isPlayable by lazy loading
            if (!vi->node->isPlayable ())
                emit treeChanged (ri->id, vi->node, 0, false, true);
        } else if (vi->firstChild ())
            vi->listView ()->setOpen (vi, !vi->isOpen ());
    } else if (vi->m_attr) {
        if (vi->m_attr->name () == StringPool::attr_src ||
                vi->m_attr->name () == StringPool::attr_href ||
                vi->m_attr->name () == StringPool::attr_url ||
                vi->m_attr->name () == StringPool::attr_value ||
                vi->m_attr->name () == "data") {
            QString src (vi->m_attr->value ());
            if (!src.isEmpty ()) {
                PlayListItem * pi = static_cast <PlayListItem*>(item->parent());
                if (pi) {
                    for (NodePtr e = pi->node; e; e = e->parentNode ()) {
                        Mrl * mrl = e->mrl ();
                        if (mrl)
                            src = KURL (mrl->absolutePath (), src).url ();
                    }
                    KURL url (src);
                    if (url.isValid ())
                        openURL (url);
                }
            }
        }
    } else
        emit treeChanged (ri->id, ri->node, 0L, false, false);
    if (m_view)
        m_view->viewArea ()->setFocus ();
}

void PartBase::updateTree (bool full, bool force) {
    if (force) {
        m_in_update_tree = true;
        if (m_update_tree_full) {
            if (m_source)
                emit treeChanged (0, m_source->root (), m_source->current (), true, false);
        } else
            emit treeUpdated ();
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

KDE_NO_EXPORT void PartBase::audioSelected (int id) {
    emit audioIsSelected (id);
}

KDE_NO_EXPORT void PartBase::subtitleSelected (int id) {
    emit subtitleIsSelected (id);
}

void PartBase::record () {
    if (m_view) m_view->setCursor (QCursor (Qt::WaitCursor));
    if (m_recorder->playing ()) {
        m_recorder->stop ();
    } else {
        m_settings->show  ("RecordPage");
        m_view->controlPanel ()->setRecording (false);
    }
    if (m_view) m_view->setCursor (QCursor (Qt::ArrowCursor));
}

void PartBase::play () {
    if (!m_process || !m_view) return;
    QPushButton * pb = ::qobject_cast <QPushButton *> (sender ());
    if (pb && !pb->isOn ()) {
        stop ();
        return;
    }
    if (m_update_tree_timer) {
        killTimer (m_update_tree_timer);
        m_update_tree_timer = 0;
    }
    if (m_process->state () == Process::NotRunning) {
        PlayListItem * lvi = m_view->playList ()->currentPlayListItem ();
        if (lvi) { // make sure it's in the first tree
            Q3ListViewItem * pitem = lvi;
            while (pitem->parent())
                pitem = pitem->parent();
            if (pitem != m_view->playList ()->firstChild ())
                lvi = 0L;
        }
        if (!lvi)
            lvi = static_cast<PlayListItem*>(m_view->playList()->firstChild());
        if (lvi)
            for (NodePtr n = lvi->node; n; n = n->parentNode ()) {
                if (n->isPlayable ()) {
                    m_source->setCurrent (n);
                    break;
                }
            }
        m_process->ready (m_view->viewer ());
    } else if (m_process->state () == Process::Ready) {
        m_source->playCurrent ();
    } else
        m_process->play (m_source, m_source->current ());
}

bool PartBase::playing () const {
    return m_process && m_process->state () > Process::Ready;
}

void PartBase::stop () {
    QPushButton * b = m_view ? m_view->controlPanel ()->button (ControlPanel::button_stop) : 0L;
    if (b) {
        if (!b->isOn ())
            b->toggle ();
        m_view->setCursor (QCursor (Qt::WaitCursor));
    }
    if (m_process)
        m_process->quit ();
    if (m_source)
        m_source->reset ();
    if (m_view) {
        m_view->setCursor (QCursor (Qt::ArrowCursor));
        if (b->isOn ())
            b->toggle ();
        m_view->controlPanel ()->setPlaying (false);
        setLoaded (100);
    }
}

void PartBase::seek (qlonglong msec) {
    if (m_process)
        m_process->seek (msec/100, true);
}

void PartBase::adjustVolume (int incdec) {
    m_process->volume (incdec, false);
}

void PartBase::increaseVolume () {
    /*if (m_view)
        m_view->controlPanel ()->volumeBar ()->setValue (m_view->controlPanel ()->volumeBar ()->value () + 2);*/
}

void PartBase::decreaseVolume () {
    //if (m_view)
    //    m_view->controlPanel ()->volumeBar ()->setValue (m_view->controlPanel ()->volumeBar ()->value () - 2);
}

KDE_NO_EXPORT void PartBase::posSliderPressed () {
    m_bPosSliderPressed=true;
}

KDE_NO_EXPORT void PartBase::posSliderReleased () {
    m_bPosSliderPressed=false;
#if (QT_VERSION < 0x030200)
    const QSlider * posSlider = dynamic_cast <const QSlider *> (sender ());
#else
    const QSlider * posSlider = ::qobject_cast<const QSlider *> (sender ());
#endif
    if (posSlider)
        m_process->seek (posSlider->value(), true);
}

KDE_NO_EXPORT void PartBase::volumeChanged (int val) {
    if (m_process) {
        m_settings->volume = val;
        m_process->volume (val, true);
    }
}

KDE_NO_EXPORT void PartBase::contrastValueChanged (int val) {
    m_settings->contrast = val;
    m_process->contrast (val, true);
}

KDE_NO_EXPORT void PartBase::brightnessValueChanged (int val) {
    m_settings->brightness = val;
    m_process->brightness (val, true);
}

KDE_NO_EXPORT void PartBase::hueValueChanged (int val) {
    m_settings->hue = val;
    m_process->hue (val, true);
}

KDE_NO_EXPORT void PartBase::saturationValueChanged (int val) {
    m_settings->saturation = val;
    m_process->saturation (val, true);
}

KDE_NO_EXPORT void PartBase::sourceHasChangedAspects () {
    if (m_view && m_source) {
      //kDebug () << "sourceHasChangedAspects " << m_source->aspect () << endl;
        m_view->viewer ()->setAspect (m_source->aspect ());
        m_view->updateLayout ();
    }
    emit sourceDimensionChanged ();
}

KDE_NO_EXPORT void PartBase::positionValueChanged (int pos) {
    QSlider * slider = ::qobject_cast <QSlider *> (sender ());
    if (slider && slider->isEnabled ())
        m_process->seek (pos, true);
}

KDE_NO_EXPORT void PartBase::fullScreen () {
    if (m_view)
        m_view->fullScreen ();
}

KDE_NO_EXPORT void PartBase::toggleFullScreen () {
    m_view->fullScreen ();
}

KDE_NO_EXPORT bool PartBase::isPlaying () {
    return playing ();
}

KAboutData* PartBase::createAboutData () {
    KMessageBox::error(0L, "createAboutData", "KMPlayer");
    return 0;
}

//-----------------------------------------------------------------------------

Source::Source (const QString & name, PartBase * player, const char * n)
 : QObject (player, n),
   m_name (name), m_player (player), m_identified (false), m_auto_play (true),
   m_frequency (0), m_xvport (0), m_xvencoding (-1), m_doc_timer (0) {
    init ();
}

Source::~Source () {
    if (m_document)
        m_document->document ()->dispose ();
    m_document = 0L;
    Q_ASSERT (m_current.ptr () == 0L);
}

void Source::init () {
    //setDimensions (320, 240);
    m_width = 0;
    m_height = 0;
    m_aspect = 0.0;
    m_length = 0;
    m_position = 0;
    setLength (m_document, 0);
    m_recordcmd.truncate (0);
}

KDE_NO_EXPORT void Source::setLanguages (const QStringList & alang, const QStringList & slang) {
    m_player->setLanguages (alang, slang);
}

void Source::setDimensions (NodePtr node, int w, int h) {
    Mrl * mrl = node ? node->mrl () : 0L;
    if (mrl && mrl->view_mode == Mrl::WindowMode) {
        mrl->width = w;
        mrl->height = h;
        float a = h > 0 ? 1.0 * w / h : 0.0;
        mrl->aspect = a;
        if (m_player->view ()) {
            static_cast <View *> (m_player->view())->viewer()->setAspect(a);
            static_cast <View *> (m_player->view ())->updateLayout ();
        }
    } else if (m_aspect < 0.001 || m_width != w || m_height != h) {
        bool ev = (w > 0 && h > 0) ||
            (h == 0 && m_height > 0) ||
            (w == 0 && m_width > 0);
        m_width = w;
        m_height = h;
        if (m_aspect < 0.001)
            setAspect (node, h > 0 ? 1.0 * w / h : 0.0);
        //kDebug () << "setDimensions " << w << "x" << h << " a:" << m_aspect << endl;
        if (ev)
            emit dimensionsChanged ();
    }
}

void Source::setAspect (NodePtr node, float a) {
    //kDebug () << "setAspect " << a << endl;
    Mrl * mrl = node ? node->mrl () : 0L;
    bool changed = false;
    if (mrl) {
        if (mrl->view_mode == Mrl::WindowMode)
            changed |= (fabs (mrl->aspect - a) > 0.001);
        mrl->aspect = a;
    }
    if (!mrl || mrl->view_mode == Mrl::SingleMode) {
        changed |= (fabs (m_aspect - a) > 0.001);
        m_aspect = a;
    }
    if (changed)
        emit dimensionsChanged ();
}

void Source::setLength (NodePtr, int len) {
    m_length = len;
    m_player->setPosition (m_position, m_length);
}

KDE_NO_EXPORT void Source::setPosition (int pos) {
    m_position = pos;
    m_player->setPosition (pos, m_length);
}

KDE_NO_EXPORT void Source::setLoading (int percentage) {
    m_player->setLoaded (percentage);
}

/*
static void printTree (NodePtr root, QString off=QString()) {
    if (!root) {
        kDebug() << off << "[null]" << endl;
        return;
    }
    kDebug() << off << root->nodeName() << " " << (Element*)root << (root->isPlayable() ? root->mrl ()->src : QString ("-")) << endl;
    off += QString ("  ");
    for (NodePtr e = root->firstChild(); e; e = e->nextSibling())
        printTree(e, off);
}*/

void Source::setURL (const KURL & url) {
    m_url = url;
    m_back_request = 0L;
    if (m_document && !m_document->hasChildNodes () &&
            (m_document->mrl()->src.isEmpty () ||
             m_document->mrl()->src == url.url ()))
        // special case, mime is set first by plugin FIXME v
        m_document->mrl()->src = url.url ();
    else {
        if (m_document)
            m_document->document ()->dispose ();
        m_document = new Document (url.url (), this);
    }
    if (m_player->process () && m_player->source () == this)
        m_player->updateTree ();
    //kDebug() << name() << " setURL " << url << endl;
    m_current = m_document;
}

void Source::setTitle (const QString & title) {
    emit titleChanged (title);
}

KDE_NO_EXPORT void Source::setAudioLang (int id) {
    //View * v = static_cast <View *> (m_player->view());
    //if (v && m_player->process ())
    //    m_player->process ()->setAudioLang (id, v->controlPanel ()->audioMenu->text (id));
}

KDE_NO_EXPORT void Source::setSubtitle (int id) {
    //View * v = static_cast <View *> (m_player->view());
    //if (v && m_player->process ())
    //    m_player->process ()->setSubtitle (id, v->controlPanel ()->subtitleMenu->text (id));
}

void Source::reset () {
    if (m_document) {
        //kDebug() << "Source::first" << endl;
        m_current = NodePtr ();
        m_document->reset ();
        m_player->updateTree ();
    }
    init ();
}

QString Source::currentMrl () {
    Mrl * mrl = m_current ? m_current->mrl () : 0L;
    kDebug() << "Source::currentMrl " << (m_current ? m_current->nodeName():"") << " src:" << (mrl ? mrl->absolutePath ()  : QString ()) << endl;
    return mrl ? mrl->absolutePath () : QString ();
}

void Source::playCurrent () {
    QString url = currentMrl ();
    m_player->changeURL (url);
    m_width = m_height = 0;
    m_aspect = 0.0;
    if (m_player->view ())
        static_cast <View *> (m_player->view ())->playingStop ();//show controls
    if (m_document && !m_document->active ()) {
        if (!m_current)
            m_document->activate ();
        else { // ugly code duplicate w/ back_request
            for (NodePtr p = m_current->parentNode(); p; p = p->parentNode())
                p->state = Element::state_activated;
            m_current->activate ();
        }
    } else if (!m_current) {
        emit endOfPlayItems ();
    } else if (m_current->state == Element::state_deferred) {
     //   m_current->undefer ();
    } else if (m_player->process ()->state () == Process::NotRunning) {
        m_player->process ()->ready (static_cast <View *> (m_player->view ())->viewer ());
    } else if (m_player->process ()) {
        Mrl * mrl = m_back_request ? m_back_request->mrl () : m_current->mrl ();
        if (mrl->view_mode == Mrl::SingleMode) {
            // don't reset the dimensions if we have any
            m_width = mrl->width;
            m_height = mrl->height;
            m_aspect = mrl->aspect;
        }
        m_back_request = 0L;
        m_player->process ()->play (this, mrl->linkNode ());
    }
    //kDebug () << "Source::playCurrent " << (m_current ? m_current->nodeName():" doc act:") <<  (m_document && !m_document->active ()) << " cur:" << (!m_current)  << " cur act:" << (m_current && !m_current->active ()) <<  endl;
    m_player->updateTree ();
    emit dimensionsChanged ();
}

static NodePtr findDepthFirst (NodePtr elm) {
    if (!elm)
        return NodePtr ();
    NodePtr tmp = elm;
    for ( ; tmp; tmp = tmp->nextSibling ()) {
        if (tmp->isPlayable ())
            return tmp;
        NodePtr tmp2 = findDepthFirst (tmp->firstChild ());
        if (tmp2)
            return tmp2;
    }
    return NodePtr ();
}

bool Source::requestPlayURL (NodePtr mrl) {
    //kDebug() << "Source::requestPlayURL " << mrl->mrl ()->src << endl;
    if (m_player->process ()->state () > Process::Ready) {
        if (m_player->process ()->mrl () == mrl->mrl ()->linkNode ())
            return true;
        m_back_request = mrl; // still playing, schedule it
        m_player->process ()->stop ();
    } else {
        if (mrl->mrl ()->view_mode == Mrl::SingleMode)
            m_current = mrl;
        else
            m_back_request = mrl;
        m_player->updateTree ();
        QTimer::singleShot (0, this, SLOT (playCurrent ()));
    }
    m_player->setProcess (mrl->mrl ());
    return true;
}

bool Source::resolveURL (NodePtr) {
    return true;
}

void Source::setTimeout (int ms) {
    //kDebug () << "Source::setTimeout " << ms << endl;
    if (m_doc_timer)
        killTimer (m_doc_timer);
    m_doc_timer = ms > -1 ? startTimer (ms) : 0;
}

void Source::timerEvent (QTimerEvent * e) {
    if (e->timerId () == m_doc_timer && m_document)
        m_document->document ()->timer (); // will call setTimeout()
    else
        killTimer (e->timerId ());
}

bool Source::setCurrent (NodePtr mrl) {
    m_current = mrl;
    return true;
}

void Source::stateElementChanged (Node * elm, Node::State os, Node::State ns) {
    //kDebug() << "[01;31mSource::stateElementChanged[00m " << elm->nodeName () << " state:" << (int) elm->state << " cur isPlayable:" << (m_current && m_current->isPlayable ()) << " elm==linkNode:" << (m_current && elm == m_current->mrl ()->linkNode ()) << " p state:" << m_player->process ()->state () << endl;
    if (ns == Node::state_deactivated && elm == m_document && !m_back_request) {
        emit endOfPlayItems (); // played all items
    } else if ((ns == Node::state_deactivated || ns == Node::state_finished) &&
             m_player->process ()->mrl() &&
             elm == m_player->process ()->mrl ()->mrl ()->linkNode ()) {
        if (m_player->process ()->state () > Process::Ready)
            //a SMIL movies stopped by SMIL events rather than movie just ending
            m_player->process ()->stop ();
        if (m_player->view ()) // move away the video widget
            QTimer::singleShot (0, m_player->view (), SLOT (updateLayout ()));
    } else if ((ns == Node::state_deferred ||
                (os == Node::state_deferred && ns > Node::state_deferred)) &&
            elm == m_document) {
        m_player->process ()->pause ();
    } else if (ns == Node::state_activated &&
            elm->isPlayable () &&
            elm->mrl ()->view_mode == Mrl::SingleMode) {
        Node *p = elm->parentNode();
        if (!p || !p->mrl () || p->mrl ()->view_mode == Mrl::SingleMode)
            // make sure we don't set current to nested document
            m_current = elm;
    }
    if (elm->expose ()) {
        if (ns == Node::state_activated || ns == Node::state_deactivated)
            m_player->updateTree ();
        else if (ns == Node::state_began || os == Node::state_began)
            m_player->updateTree (false);
    }
}

SurfacePtr Source::getSurface (NodePtr n) {
    if (m_player->view ())
        return static_cast <View*>(m_player->view())->viewArea()->getSurface(n);
    return 0L;
}

void Source::setInfoMessage (const QString & msg) {
    m_player->updateInfo (msg);
}

void Source::bitRates (int & preferred, int & maximal) {
    preferred = 1024 * m_player->settings ()->prefbitrate;
    maximal= 1024 * m_player->settings ()->maxbitrate;
}

void Source::insertURL (NodePtr node, const QString & mrl, const QString & title) {
    if (!node || !node->mrl ()) // this should always be false
        return;
    QString cur_url = node->mrl ()->absolutePath ();
    KURL url (cur_url, mrl);
    kDebug() << "Source::insertURL " << KURL (cur_url) << " " << url << endl;
    if (!url.isValid ())
        kError () << "try to append non-valid url" << endl;
    else if (KURL (cur_url) == url)
        kError () << "try to append url to itself" << endl;
    else {
        int depth = 0; // cache this?
        for (NodePtr e = node; e->parentNode (); e = e->parentNode ())
            ++depth;
        if (depth < 40) {
            node->appendChild (new GenericURL (m_document, KURL::decode_string (url.url ()), title.isEmpty() ? KURL::decode_string (mrl) : title));
            m_player->updateTree ();
        } else
            kError () << "insertURL exceeds depth limit" << endl;
    }
}

void Source::play () {
    m_player->updateTree ();
    QTimer::singleShot (0, m_player, SLOT (play ()));
    //printTree (m_document);
}

void Source::backward () {
    if (m_document->hasChildNodes ()) {
        m_back_request = m_current;
        if (!m_back_request || m_back_request == m_document) {
            m_back_request = m_document->lastChild ();
            while (m_back_request->lastChild () && !m_back_request->isPlayable ())
                m_back_request = m_back_request->lastChild ();
            if (m_back_request->isPlayable ())
                return;
        }
        while (m_back_request && m_back_request != m_document) {
            if (m_back_request->previousSibling ()) {
                m_back_request = m_back_request->previousSibling ();
                NodePtr e = findDepthFirst (m_back_request); // lastDepth..
                if (e) {
                    m_back_request = e;
                    if (m_player->playing ())
                        m_player->process ()->stop ();
                    else if (m_current) {
                        m_document->reset ();
                        m_current = e;
                        QTimer::singleShot (0, this, SLOT (playCurrent ()));
                    }
                    return;
                }
            } else
                m_back_request = m_back_request->parentNode ();
        }
        m_back_request = 0L;
    } else
        m_player->process ()->seek (-1 * m_player->settings ()->seektime * 10, false);
}

void Source::forward () {
    if (m_document->hasChildNodes ()) {
        if (m_player->playing ())
            m_player->process ()->stop ();
        else if (m_current)
            m_current->finish ();
    } else
        m_player->process ()->seek (m_player->settings()->seektime * 10, false);
}

void Source::jump (NodePtr e) {
    if (e->isPlayable ()) {
        if (m_player->playing ()) {
            m_back_request = e;
            m_player->process ()->stop ();
        } else {
            if (m_current)
                m_document->reset ();
            m_current = e;
            QTimer::singleShot (0, this, SLOT (playCurrent ()));
        }
    } else
        m_player->updateTree ();
}

NodePtr Source::document () {
    if (!m_document)
        m_document = new Document (QString (), this);
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
            PPargs.truncate(PPargs.length()-1);
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
    //kDebug () << "Source::setIdentified " << m_identified << b <<endl;
    m_identified = b;
}

static const QString statemap [] = {
    i18n ("Not Running"), i18n ("Ready"), i18n ("Buffering"), i18n ("Playing")
};

void Source::stateChange(Process *p, Process::State olds, Process::State news) {
    if (!p || !p->viewer ()) return;
    Recorder *rec = dynamic_cast <Recorder *> (p);
    if (rec && !rec->recordURL ().isEmpty ()) {
        kDebug () << "recordState " << statemap[olds] << " -> " << statemap[news] << endl;
        m_player->updateStatus (i18n ("Recorder %1 %2").arg (p->name ()).arg (statemap[news]));
        p->viewer ()->view ()->controlPanel ()->setRecording (news > Process::Ready);
        if (news == Process::Ready) {
            if (olds > Process::Ready) {
                p->quit ();
            } else {
                NodePtr n = current ();
                if (!n)
                    n = document ();
                p->play (this, n);
            }
        } else if (news > Process::Ready) {
            emit startRecording ();
        } else if (news == Process::NotRunning)
            emit stopRecording ();
    } else {
        p->viewer()->view()->controlPanel()->setPlaying(news > Process::Ready);
        kDebug () << "processState " << statemap[olds] << " -> " << statemap[news] << endl;
        m_player->updateStatus (i18n ("Player %1 %2").arg (p->name ()).arg (statemap[news]));
        if (!p->mrl () && news > Process::Ready) {
            p->stop (); // reschedule for Ready state
        } else if (news == Process::Playing) {
            if (p->mrl ()->state == Element::state_deferred)
                p->mrl ()->undefer ();
            p->viewer ()->view ()->playingStart ();
            emit startPlaying ();
        } else if (news == Process::NotRunning) {
            if (hasLength () && position () > length ())
                setLength (m_document, position ());
            setPosition (0);
            if (p == m_player->process ())
                emit stopPlaying ();
            // else changed process
        } else if (news == Process::Ready) {
            if (olds > Process::Ready) {
                NodePtr node = p->mrl (); // p->mrl is weak, needs check
                Mrl * mrl = node ? node->mrl () : 0L;
                if (m_back_request && m_back_request->isPlayable ()) {
                    if (m_back_request->mrl ()->view_mode == Mrl::SingleMode)
                        // jump in pl
                        m_current = m_back_request;
                    else if (mrl)
                        // overlapping SMIL audio/video
                        mrl->endOfFile ();
                    if (m_current->id >= SMIL::id_node_first &&
                            m_current->id < SMIL::id_node_last) {
                        playCurrent ();  // just play back_request
                    } else {
                        // sanitize pl having all parents of current activated
                        m_document->reset (); // deactivate everything
                        for (NodePtr p = m_current->parentNode(); p; p = p->parentNode())
                            p->state = Element::state_activated;
                        m_current->activate (); // calls requestPlayUrl
                    }
                    m_back_request = 0L;
                } else if(mrl)
		{
                    mrl->endOfFile (); // set node to finished
		}
                if (m_player->view() &&
                        (!mrl || mrl->view_mode != Mrl::WindowMode))
                    static_cast<View*>(m_player->view())->viewArea()->repaint();
            } else
                QTimer::singleShot (0, this, SLOT (playCurrent ()));
        } else if (news == Process::Buffering) {
            if (p->mrl ()->mrl ()->view_mode != Mrl::SingleMode)
                p->mrl ()->defer (); // paused the SMIL
        }
    }
}

QString Source::plugin (const QString &mime) const {
    m_player->config ()->setGroup (mime);
    return m_player->config ()->readEntry ("plugin", "" );
}

QString Source::prettyName () {
    return i18n ("Unknown");
}

//-----------------------------------------------------------------------------

URLSource::URLSource (PartBase * player, const KURL & url)
    : Source (i18n ("URL"), player, "urlsource"), activated (false) {
    setURL (url);
    //kDebug () << "URLSource::URLSource" << endl;
}

URLSource::~URLSource () {
    //kDebug () << "URLSource::~URLSource" << endl;
}

void URLSource::init () {
    Source::init ();
}

void URLSource::dimensions (int & w, int & h) {
    if (!m_player->mayResize () && m_player->view ()) {
        w = static_cast <View *> (m_player->view ())->viewer ()->width ();
        h = static_cast <View *> (m_player->view ())->viewer ()->height ();
    } else
        Source::dimensions (w, h);
    
}

bool URLSource::hasLength () {
    return !!length ();
}

KDE_NO_EXPORT void URLSource::activate () {
    if (activated)
        return;
    activated = true;
    if (url ().isEmpty () && (!m_document || !m_document->hasChildNodes ())) {
        m_player->updateTree ();
        return;
    }
    if (m_auto_play)
        play ();
}

KDE_NO_EXPORT void URLSource::stopResolving () {
    if (m_resolve_info) {
        for (SharedPtr <ResolveInfo> ri = m_resolve_info; ri; ri = ri->next)
            ri->job->kill ();
        m_resolve_info = 0L;
        m_player->updateStatus (i18n ("Disconnected"));
        m_player->setLoaded (100);
    }
}

void URLSource::reset () {
    stopResolving ();
    Source::reset ();
}

void URLSource::forward () {
    stopResolving ();
    Source::forward ();
}

void URLSource::backward () {
    stopResolving ();
    Source::backward ();
}

void URLSource::jump (NodePtr e) {
    stopResolving ();
    Source::jump (e);
}

void URLSource::deactivate () {
    activated = false;
    reset ();
    getSurface (0L);
}

QString URLSource::prettyName () {
    if (m_url.isEmpty ())
        return i18n ("URL");
    if (m_url.url ().length () > 50) {
        QString newurl = m_url.protocol () + QString ("://");
        if (m_url.hasHost ())
            newurl += m_url.host ();
        if (m_url.port ())
            newurl += QString (":%1").arg (m_url.port ());
        QString file = m_url.fileName ();
        int len = newurl.length () + file.length ();
        KURL path = KURL (m_url.directory ());
        bool modified = false;
        while (path.url ().length () + len > 50 && path != path.upUrl ()) {
            path = path.upUrl ();
            modified = true;
        }
        QString dir = path.directory ();
        if (!dir.endsWith (QString ("/")))
            dir += '/';
        if (modified)
            dir += QString (".../");
        newurl += dir + file;
        return i18n ("URL - %1").arg (newurl);
    }
    return i18n ("URL - %1").arg (m_url.prettyUrl ());
}

static bool isPlayListMime (const QString & mime) {
    QString m (mime);
    int plugin_pos = m.find ("-plugin");
    if (plugin_pos > 0)
        m.truncate (plugin_pos);
    const char * mimestr = m.ascii ();
    return mimestr && (!strcmp (mimestr, "audio/mpegurl") ||
            !strcmp (mimestr, "audio/x-mpegurl") ||
            !strncmp (mimestr, "video/x-ms", 10) ||
            !strncmp (mimestr, "audio/x-ms", 10) ||
            //!strcmp (mimestr, "video/x-ms-wmp") ||
            //!strcmp (mimestr, "video/x-ms-asf") ||
            //!strcmp (mimestr, "video/x-ms-wmv") ||
            //!strcmp (mimestr, "video/x-ms-wvx") ||
            //!strcmp (mimestr, "video/x-msvideo") ||
            !strcmp (mimestr, "audio/x-scpls") ||
            !strcmp (mimestr, "audio/x-pn-realaudio") ||
            !strcmp (mimestr, "audio/vnd.rn-realaudio") ||
            !strcmp (mimestr, "audio/m3u") ||
            !strcmp (mimestr, "audio/x-m3u") ||
            !strncmp (mimestr, "text/", 5) ||
            (!strncmp (mimestr, "application/", 12) &&
             strstr (mimestr + 12,"+xml")) ||
            !strncasecmp (mimestr, "application/smil", 16) ||
            !strncasecmp (mimestr, "application/xml", 15) ||
            //!strcmp (mimestr, "application/rss+xml") ||
            //!strcmp (mimestr, "application/atom+xml") ||
            !strcmp (mimestr, "application/x-mplayer2"));
}

KDE_NO_EXPORT void URLSource::read (NodePtr root, QTextStream & textstream) {
    QString line;
    do {
        line = textstream.readLine ();
    } while (!line.isNull () && line.stripWhiteSpace ().isEmpty ());
    if (!line.isNull ()) {
        NodePtr cur_elm = root;
        if (cur_elm->isPlayable ())
            cur_elm = cur_elm->mrl ()->linkNode ();
        if (cur_elm->mrl ()->mimetype == QString ("audio/x-scpls")) {
            bool groupfound = false;
            int nr = -1;
            struct Entry {
                QString url, title;
            } * entries = 0L;
            do {
                line = line.stripWhiteSpace ();
                if (!line.isEmpty ()) {
                    if (line.startsWith (QString ("[")) && line.endsWith (QString ("]"))) {
                        if (!groupfound && line.mid (1, line.length () - 2).stripWhiteSpace () == QString ("playlist"))
                            groupfound = true;
                        else
                            break;
                        kDebug () << "Group found: " << line << endl;
                    } else if (groupfound) {
                        int eq_pos = line.find (QChar ('='));
                        if (eq_pos > 0) {
                            if (line.lower ().startsWith (QString ("numberofentries"))) {
                                nr = line.mid (eq_pos + 1).stripWhiteSpace ().toInt ();
                                kDebug () << "numberofentries : " << nr << endl;
                                if (nr > 0 && nr < 1024)
                                    entries = new Entry[nr];
                                else
                                    nr = 0;
                            } else if (nr > 0) {
                                QString ll = line.lower ();
                                if (ll.startsWith (QString ("file"))) {
                                    int i = line.mid (4, eq_pos-4).toInt ();
                                    if (i > 0 && i <= nr)
                                        entries[i-1].url = line.mid (eq_pos + 1).stripWhiteSpace ();
                                } else if (ll.startsWith (QString ("title"))) {
                                    int i = line.mid (5, eq_pos-5).toInt ();
                                    if (i > 0 && i <= nr)
                                        entries[i-1].title = line.mid (eq_pos + 1).stripWhiteSpace ();
                                }
                            }
                        }
                    }
                }
                line = textstream.readLine ();
            } while (!line.isNull ());
            for (int i = 0; i < nr; i++)
                if (!entries[i].url.isEmpty ())
                    cur_elm->appendChild (new GenericURL (m_document, KURL::decode_string (entries[i].url), entries[i].title));
            delete [] entries;
        } else if (line.stripWhiteSpace ().startsWith (QChar ('<'))) {
            readXML (cur_elm, textstream, line);
            //cur_elm->normalize ();
            if (m_document && m_document->firstChild ()) {
                // SMIL documents have set its size of root-layout
                Mrl * mrl = m_document->firstChild ()->mrl ();
                if (mrl)
                    Source::setDimensions (m_document->firstChild (), mrl->width, mrl->height);
            }
        } else if (line.lower () != QString ("[reference]")) do {
            QString mrl = line.stripWhiteSpace ();
            if (line == QString ("--stop--"))
                break;
            if (mrl.lower ().startsWith (QString ("asf ")))
                mrl = mrl.mid (4).stripWhiteSpace ();
            if (!mrl.isEmpty () && !mrl.startsWith (QChar ('#')))
                cur_elm->appendChild (new GenericURL (m_document, mrl));
            line = textstream.readLine ();
        } while (!line.isNull ()); /* TODO && m_document.size () < 1024 / * support 1k entries * /);*/
    }
}

KDE_NO_EXPORT void URLSource::kioData (KIO::Job * job, const QByteArray & d) {
    SharedPtr <ResolveInfo> rinfo = m_resolve_info;
    while (rinfo && rinfo->job != job)
        rinfo = rinfo->next;
    if (!rinfo) {
        kWarning () << "Spurious kioData" << endl;
        return;
    }
    int size = rinfo->data.size ();
    int newsize = size + d.size ();
    if (!size) { // first data
        int accuraty = 0;
        KMimeType::Ptr mime = KMimeType::findByContent (d, &accuraty);
        if (!mime ||
                !mime->name ().startsWith (QString ("text/")) ||
                (newsize > 4 && !strncmp (d.data (), "RIFF", 4))) {
            newsize = 0;
            kDebug () << "URLSource::kioData: " << mime->name () << accuraty << endl;
        }
    }
    //kDebug () << "URLSource::kioData: " << newsize << endl;
    if (newsize <= 0 || newsize > 200000) {
        rinfo->data.resize (0);
        rinfo->job->kill (KJob::EmitResult);
        m_player->setLoaded (100);
    } else  {
        rinfo->data.resize (newsize);
        memcpy (rinfo->data.data () + size, d.data (), newsize - size);
        m_player->setLoaded (++rinfo->progress);
    }
}

KDE_NO_EXPORT void URLSource::kioMimetype (KIO::Job * job, const QString & mimestr) {
    SharedPtr <ResolveInfo> rinfo = m_resolve_info;
    while (rinfo && rinfo->job != job)
        rinfo = rinfo->next;
    if (!rinfo) {
        kWarning () << "Spurious kioData" << endl;
        return;
    }
    if (!rinfo->resolving_mrl || !isPlayListMime (mimestr))
        job->kill (KJob::EmitResult);
    if (rinfo->resolving_mrl)
        rinfo->resolving_mrl->mrl ()->mimetype = mimestr;
}

KDE_NO_EXPORT void URLSource::kioResult (KIO::Job * job) {
    SharedPtr <ResolveInfo> previnfo, rinfo = m_resolve_info;
    while (rinfo && rinfo->job != job) {
        previnfo = rinfo;
        rinfo = rinfo->next;
    }
    if (!rinfo) {
        kWarning () << "Spurious kioData" << endl;
        return;
    }
    m_player->updateStatus ("");
    m_player->setLoaded (100);
    if (previnfo)
        previnfo->next = rinfo->next;
    else
        m_resolve_info = rinfo->next;
    QTextStream textstream (rinfo->data, IO_ReadOnly);
    if (rinfo->resolving_mrl) {
        if (isPlayListMime (rinfo->resolving_mrl->mrl ()->mimetype))
            read (rinfo->resolving_mrl, textstream);
        rinfo->resolving_mrl->mrl ()->resolved = true;
        rinfo->resolving_mrl->undefer ();
    }
    static_cast <View *> (m_player->view())->controlPanel()->setPlaying (false);
}

void URLSource::playCurrent () {
    Mrl *mrl = m_back_request
        ? m_back_request->mrl ()
        : m_current ? m_current->mrl () : NULL;
    if (mrl && mrl->active () && (!mrl->isPlayable () || !mrl->resolved))
        // an async playCurrent() call (eg. backend is up & running), ignore
        return;
    Source::playCurrent ();
}

void URLSource::play () {
    Source::play ();
}

bool URLSource::requestPlayURL (NodePtr mrl) {
    if (m_document.ptr () != mrl->mrl ()->linkNode ()) {
        KURL base = m_document->mrl ()->src;
        KURL dest = mrl->mrl ()->linkNode ()->absolutePath ();
        // check if some remote playlist tries to open something local, but
        // do ignore unknown protocols because there are so many and we only
        // want to cache local ones.
        if (
#if 0
            !KProtocolInfo::protocolClass (dest.protocol ()).isEmpty () &&
#else
            dest.isLocalFile () &&
#endif
                !KAuthorized::authorizeUrlAction ("redirect", base, dest)) {
            kWarning () << "requestPlayURL from document " << base << " to play " << dest << " is not allowed" << endl;
            return false;
        }
    }
    return Source::requestPlayURL (mrl);
}

void URLSource::setURL (const KURL & url) {
    Source::setURL (url);
    Mrl *mrl = document ()->mrl ();
    if (!url.isEmpty () && url.isLocalFile () && mrl->mimetype.isEmpty ()) {
        KMimeType::Ptr mimeptr = KMimeType::findByUrl (url);
        if (mimeptr)
            mrl->mimetype = mimeptr->name ();
    }
}

bool URLSource::resolveURL (NodePtr m) {
    Mrl * mrl = m->mrl ();
    if (!mrl || mrl->src.isEmpty ())
        return true;
    int depth = 0;
    for (NodePtr e = m->parentNode (); e; e = e->parentNode ())
        ++depth;
    if (depth > 40)
        return true;
    KURL url (mrl->absolutePath ());
    QString mimestr = mrl->mimetype;
    if (mimestr == "application/x-shockwave-flash" ||
            mimestr == "application/futuresplash")
        return true; // FIXME
    bool maybe_playlist = isPlayListMime (mimestr);
    kDebug () << "resolveURL " << mrl->absolutePath () << " " << mimestr << endl;
    if (url.isLocalFile ()) {
        QFile file (url.path ());
        if (!file.exists ()) {
            kWarning () << "resolveURL " << url.path() << " not found" << endl;
            return true;
        }
        if (mimestr.isEmpty ()) {
            KMimeType::Ptr mimeptr = KMimeType::findByUrl (url);
            if (mimeptr) {
                mrl->mimetype = mimeptr->name ();
                maybe_playlist = isPlayListMime (mrl->mimetype); // get new mime
            }
        }
        if (maybe_playlist && file.size () < 2000000 && file.open (IO_ReadOnly)) {
            char databuf [512];
            int nr_bytes = file.readBlock (databuf, 512);
            if (nr_bytes > 3) {
                int accuraty = 0;
                KMimeType::Ptr mime = KMimeType::findByContent (QByteArray (databuf, nr_bytes), &accuraty);
                if ((mime && !mime->name().startsWith (QString("text/"))) ||
                        !strncmp (databuf, "RIFF", 4)) {
                    return true;
                }
                kDebug () << "mime: " << (mime ? mime->name (): QString("null")) << endl;
            }
            file.reset ();
            QTextStream textstream (&file);
            read (m, textstream);
        }
    } else if ((maybe_playlist &&
                url.protocol ().compare (QString ("mms")) &&
                url.protocol ().compare (QString ("rtsp")) &&
                url.protocol ().compare (QString ("rtp"))) ||
            (mimestr.isEmpty () &&
             (url.protocol ().startsWith (QString ("http")) ||
              url.protocol () == QString::fromLatin1 ("media") ||
              url.protocol () == QString::fromLatin1 ("remote")))) {
        KIO::Job * job = KIO::get (url, false, false);
        job->addMetaData ("PropagateHttpHeader", "true");
        job->addMetaData ("errorPage", "false");
        m_resolve_info = new ResolveInfo (m, job, m_resolve_info);
        connect (m_resolve_info->job, SIGNAL(data(KIO::Job*,const QByteArray&)),
                this, SLOT (kioData (KIO::Job *, const QByteArray &)));
        //connect( m_job, SIGNAL(connected(KIO::Job*)),
        //         this, SLOT(slotConnected(KIO::Job*)));
        connect(m_resolve_info->job, SIGNAL(mimetype(KIO::Job*,const QString&)),
                this, SLOT (kioMimetype (KIO::Job *, const QString &)));
        connect (m_resolve_info->job, SIGNAL (result (KIO::Job *)),
                this, SLOT (kioResult (KIO::Job *)));
        static_cast <View *> (m_player->view ())->controlPanel ()->setPlaying (true);
        m_player->updateStatus (i18n ("Connecting"));
        m_player->setLoaded (0);
        return false; // wait for result ..
    }
    return true;
}

//-----------------------------------------------------------------------------

void DataCache::add (const QString & url, const QByteArray & data) {
    QByteArray bytes;
    bytes.duplicate (data);
    cache_map.insert (url, bytes);
    preserve_map.erase (url);
    emit preserveRemoved (url);
}

bool DataCache::get (const QString & url, QByteArray & data) {
    DataMap::const_iterator it = cache_map.find (url);
    if (it != cache_map.end ()) {
        data.duplicate (it.data ());
        return true;
    }
    return false;
}

bool DataCache::preserve (const QString & url) {
    PreserveMap::const_iterator it = preserve_map.find (url);
    if (it == preserve_map.end ()) {
        preserve_map.insert (url, true);
        return true;
    }
    return false;
}

bool DataCache::isPreserved (const QString & url) {
    return preserve_map.find (url) != preserve_map.end ();
}

bool DataCache::unpreserve (const QString & url) {
    const PreserveMap::iterator it = preserve_map.find (url);
    if (it == preserve_map.end ())
        return false;
    preserve_map.erase (it);
    emit preserveRemoved (url);
    return true;
}

RemoteObjectPrivate::RemoteObjectPrivate (RemoteObject * r)
 : job (0L), remote_object (r), preserve_wait (false) {
}

RemoteObjectPrivate::~RemoteObjectPrivate () {
    clear ();
}

KDE_NO_EXPORT bool RemoteObjectPrivate::download (const QString & str) {
    url = str;
    KURL kurl (str);
    if (kurl.isLocalFile ()) {
        QFile file (kurl.path ());
        if (file.exists () && file.open (IO_ReadOnly)) {
            data = file.readAll ();
            file.close ();
        }
        remote_object->remoteReady (data);
        return true;
    }
    if (memory_cache->get (str, data)) {
        //kDebug () << "download found in cache " << str << endl;
        remote_object->remoteReady (data);
        return true;
    }
    if (memory_cache->preserve (str)) {
        //kDebug () << "downloading " << str << endl;
        job = KIO::get (kurl, false, false);
        connect (job, SIGNAL (data (KIO::Job *, const QByteArray &)),
                this, SLOT (slotData (KIO::Job *, const QByteArray &)));
        connect (job, SIGNAL (result (KIO::Job *)),
                this, SLOT (slotResult (KIO::Job *)));
        connect (job, SIGNAL (mimetype (KIO::Job *, const QString &)),
                this, SLOT (slotMimetype (KIO::Job *, const QString &)));
    } else {
        //kDebug () << "download preserved " << str << endl;
        connect (memory_cache, SIGNAL (preserveRemoved (const QString &)),
                 this, SLOT (cachePreserveRemoved (const QString &)));
        preserve_wait = true;
    }
    return false;
}

KDE_NO_EXPORT void RemoteObjectPrivate::clear () {
    if (job) {
        job->kill (); // quiet, no result signal
        job = 0L;
        memory_cache->unpreserve (url);
    } else if (preserve_wait) {
        disconnect (memory_cache, SIGNAL (preserveRemoved (const QString &)),
                    this, SLOT (cachePreserveRemoved (const QString &)));
        preserve_wait = false;
    }
}

KDE_NO_EXPORT void RemoteObjectPrivate::slotResult (KIO::Job * kjob) {
    if (!kjob->error ())
        memory_cache->add (url, data);
    else
        data.resize (0);
    job = 0L; // signal KIO::Job::result deletes itself
    remote_object->remoteReady (data);
}

KDE_NO_EXPORT
void RemoteObjectPrivate::cachePreserveRemoved (const QString & str) {
    if (str == url && !memory_cache->isPreserved (str)) {
        preserve_wait = false;
        disconnect (memory_cache, SIGNAL (preserveRemoved (const QString &)),
                    this, SLOT (cachePreserveRemoved (const QString &)));
        download (str);
    }
}

KDE_NO_EXPORT
void RemoteObjectPrivate::slotData (KIO::Job*, const QByteArray& qb) {
    if (qb.size ()) {
        int old_size = data.size ();
        data.resize (old_size + qb.size ());
        memcpy (data.data () + old_size, qb.data (), qb.size ());
    }
}

KDE_NO_EXPORT
void RemoteObjectPrivate::slotMimetype (KIO::Job *, const QString & m) {
    mime = m;
}

KDE_NO_CDTOR_EXPORT RemoteObject::RemoteObject ()
 : d (new RemoteObjectPrivate (this)) {}

KDE_NO_CDTOR_EXPORT RemoteObject::~RemoteObject () {
    delete d;
}

/**
 * abort previous wget job
 */
KDE_NO_EXPORT void RemoteObject::killWGet () {
    d->clear (); // assume data is invalid
}

/**
 * Gets contents from url and puts it in m_data
 */
KDE_NO_EXPORT bool RemoteObject::wget (const QString & url) {
    clear ();
    return d->download (url);
}

KDE_NO_EXPORT QString RemoteObject::mimetype () {
    if (d->data.size () > 0 && d->mime.isEmpty ()) {
        int accuraty;
        KMimeType::Ptr mime = KMimeType::findByContent (d->data, &accuraty);
        if (mime)
            d->mime = mime->name ();
    }
    return d->mime;
}

KDE_NO_EXPORT void RemoteObject::clear () {
    killWGet ();
    d->url.truncate (0);
    d->mime.truncate (0);
    d->data.resize (0);
}

KDE_NO_EXPORT bool RemoteObject::downloading () const {
    return !!d->job;
}

//-----------------------------------------------------------------------------

#include "kmplayerpartbase.moc"
#include "kmplayersource.moc"
