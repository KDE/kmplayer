/**
 * Copyright (C) 2002-2003 by Koos Vriezen <koos ! vriezen ? xs4all ! nl>
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
 *  the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 *  Boston, MA 02111-1307, USA.
 **/

#ifdef KDE_USE_FINAL
#undef Always
#endif

#include <qapplication.h>
#include <qcstring.h>
#include <qcursor.h>
#include <qtimer.h>
#include <qpair.h>
#include <qpushbutton.h>
#include <qpopupmenu.h>
#include <qslider.h>
#include <qvaluelist.h>
#include <qfile.h>
#include <qxml.h>
#include <qregexp.h>
#include <qtextstream.h>

#include <kmessagebox.h>
#include <kaboutdata.h>
#include <kdebug.h>
#include <kbookmarkmenu.h>
#include <kbookmarkmanager.h>
#include <kconfig.h>
#include <ksimpleconfig.h>
#include <kaction.h>
#include <kprocess.h>
#include <kstandarddirs.h>
#include <kmimetype.h>
#include <kio/job.h>
#include <kio/jobclasses.h>

#include "kmplayerpartbase.h"
#include "kmplayerview.h"
#include "kmplayerconfig.h"
#include "kmplayerprocess.h"

class KMPlayerBookmarkOwner : public KBookmarkOwner {
public:
    KMPlayerBookmarkOwner (KMPlayer *);
    KDE_NO_CDTOR_EXPORT ~KMPlayerBookmarkOwner () {}
    void openBookmarkURL(const QString& _url);
    QString currentTitle() const;
    QString currentURL() const;
private:
    KMPlayer * m_player;
};

KDE_NO_CDTOR_EXPORT KMPlayerBookmarkOwner::KMPlayerBookmarkOwner (KMPlayer * player)
    : m_player (player) {}

KDE_NO_EXPORT void KMPlayerBookmarkOwner::openBookmarkURL (const QString & url) {
    m_player->openURL (KURL (url));
}

KDE_NO_EXPORT QString KMPlayerBookmarkOwner::currentTitle () const {
    return m_player->process ()->source ()->prettyName ();
}

KDE_NO_EXPORT QString KMPlayerBookmarkOwner::currentURL () const {
    return m_player->process ()->source ()->url ().url ();
}

//-----------------------------------------------------------------------------

class KMPlayerBookmarkManager : public KBookmarkManager {
public:
    KMPlayerBookmarkManager (const QString &);
};

inline KMPlayerBookmarkManager::KMPlayerBookmarkManager(const QString & bmfile)
  : KBookmarkManager (bmfile, false) {
}

//-----------------------------------------------------------------------------

KMPlayer::KMPlayer (QWidget * wparent, const char *wname,
                    QObject * parent, const char *name, KConfig * config)
 : KMediaPlayer::Player (wparent, wname ? wname : "kde_kmplayer_view", parent, name ? name : "kde_kmplayer_part"),
   m_config (config),
   m_view (new KMPlayerView (wparent, wname ? wname : "kde_kmplayer_view")),
   m_settings (new KMPlayerSettings (this, config)),
   m_process (0L),
   m_recorder (0L),
   m_bookmark_menu (0L),
   m_record_timer (0),
   m_ispart (false),
   m_noresize (false),
   m_in_update_tree (false)
{
    m_players ["mplayer"] = new MPlayer (this);
    m_players ["xine"] = new Xine (this);
    m_recorders ["mencoder"] = new MEncoder (this);
    m_recorders ["mplayerdumpstream"] = new MPlayerDumpstream (this);
    m_recorders ["ffmpeg"] = new FFMpeg (this);
    m_sources ["urlsource"] = new KMPlayerURLSource (this);

    QString bmfile = locate ("data", "kmplayer/bookmarks.xml");
    QString localbmfile = locateLocal ("data", "kmplayer/bookmarks.xml");
    if (localbmfile != bmfile) {
        kdDebug () << "cp " << bmfile << " " << localbmfile << endl;
        KProcess p;
        p << "/bin/cp" << QFile::encodeName (bmfile) << QFile::encodeName (localbmfile);
        p.start (KProcess::Block);
    }
    m_bookmark_manager = new KMPlayerBookmarkManager (localbmfile);
    m_bookmark_owner = new KMPlayerBookmarkOwner (this);
}

void KMPlayer::showConfigDialog () {
    m_settings->show ("GeneralPage");
}

KDE_NO_EXPORT void KMPlayer::showVideoWindow () {
    m_process->view ()->showWidget (KMPlayerView::WT_Video);
}

KDE_NO_EXPORT void KMPlayer::showPlayListWindow () {
    m_process->view ()->showPlaylist ();
}

KDE_NO_EXPORT void KMPlayer::showConsoleWindow () {
    m_process->view ()->showWidget (KMPlayerView::WT_Console);
}

KDE_NO_EXPORT void KMPlayer::addBookMark (const QString & t, const QString & url) {
    KBookmarkGroup b = m_bookmark_manager->root ();
    b.addBookmark (m_bookmark_manager, t, KURL (url));
    m_bookmark_manager->emitChanged (b);
}

void KMPlayer::init (KActionCollection * action_collection) {
    m_view->init ();
    m_settings->readConfig ();
    KMPlayerControlPanel * panel = m_view->buttonBar ();
    m_bookmark_menu = new KBookmarkMenu (m_bookmark_manager, m_bookmark_owner,
                        panel->bookmarkMenu (), action_collection, true, true);
    m_bPosSliderPressed = false;
    panel->contrastSlider ()->setValue (m_settings->contrast);
    panel->brightnessSlider ()->setValue (m_settings->brightness);
    panel->hueSlider ()->setValue (m_settings->hue);
    panel->saturationSlider ()->setValue (m_settings->saturation);
    connect (panel->button (KMPlayerControlPanel::button_back), SIGNAL (clicked ()), this, SLOT (back ()));
    connect (panel->button (KMPlayerControlPanel::button_play), SIGNAL (clicked ()), this, SLOT (play ()));
    connect (panel->button (KMPlayerControlPanel::button_forward), SIGNAL (clicked ()), this, SLOT (forward ()));
    connect (panel->button (KMPlayerControlPanel::button_pause), SIGNAL (clicked ()), this, SLOT (pause ()));
    connect (panel->button (KMPlayerControlPanel::button_stop), SIGNAL (clicked ()), this, SLOT (stop ()));
    connect (panel->button (KMPlayerControlPanel::button_record), SIGNAL (clicked()), this, SLOT (record()));
    connect (panel->positionSlider (), SIGNAL (valueChanged (int)), this, SLOT (positionValueChanged (int)));
    connect (panel->positionSlider (), SIGNAL (sliderPressed()), this, SLOT (posSliderPressed()));
    connect (panel->positionSlider (), SIGNAL (sliderReleased()), this, SLOT (posSliderReleased()));
    connect (panel->contrastSlider (), SIGNAL (valueChanged(int)), this, SLOT (contrastValueChanged(int)));
    connect (panel->brightnessSlider (), SIGNAL (valueChanged(int)), this, SLOT (brightnessValueChanged(int)));
    connect (panel->hueSlider (), SIGNAL (valueChanged(int)), this, SLOT (hueValueChanged(int)));
    connect (panel->saturationSlider (), SIGNAL (valueChanged(int)), this, SLOT (saturationValueChanged(int)));
    connect (m_view->playList (), SIGNAL (addBookMark (const QString &, const QString &)), this, SLOT (addBookMark (const QString &, const QString &)));
    connect (m_view->playList (), SIGNAL (executed (QListViewItem *)), this, SLOT (playListItemSelected (QListViewItem *)));
    panel->popupMenu()->connectItem (KMPlayerControlPanel::menu_fullscreen, this, SLOT (fullScreen ()));
    connect (m_view, SIGNAL (urlDropped (const KURL &)), this, SLOT (openURL (const KURL &)));
    panel->popupMenu ()->connectItem (KMPlayerControlPanel::menu_config,
                                       this, SLOT (showConfigDialog ()));
    panel->viewMenu ()->connectItem (KMPlayerControlPanel::menu_video,
                                       this, SLOT (showVideoWindow ()));
    panel->viewMenu ()->connectItem (KMPlayerControlPanel::menu_playlist,
                                       this, SLOT (showPlayListWindow ()));
    panel->viewMenu ()->connectItem (KMPlayerControlPanel::menu_console,
                                       this, SLOT (showConsoleWindow ()));
    //connect (panel (), SIGNAL (clicked ()), m_settings, SLOT (show ()));
}

KMPlayer::~KMPlayer () {
    kdDebug() << "KMPlayer::~KMPlayer" << endl;
    if (!m_ispart)
        delete (KMPlayerView*) m_view;
    m_view = (KMPlayerView*) 0;
    stop ();
    if (m_process && m_process->source ())
        m_process->source ()->deactivate ();
    delete m_settings;
    delete m_bookmark_menu;
    delete m_bookmark_manager;
    delete m_bookmark_owner;
}

KMediaPlayer::View* KMPlayer::view () {
    return m_view;
}

void KMPlayer::setProcess (const char * name) {
    KMPlayerProcess * process = name ? m_players [name] : 0L;
    if (m_process == process)
        return;
    KMPlayerSource * source = process ? process->source () : 0L;
    if (!source)
        source = m_sources ["urlsource"];
    if (m_process) {
        disconnect (m_process, SIGNAL (finished ()),
                    this, SLOT (processFinished ()));
        disconnect (m_process, SIGNAL (started ()),
                    this, SLOT (processStarted ()));
        disconnect (m_process, SIGNAL (startedPlaying ()),
                    this, SLOT (processStartedPlaying ()));
        disconnect (m_process, SIGNAL (positioned (int)),
                    this, SLOT (positioned (int)));
        disconnect (m_process, SIGNAL (loaded (int)),
                    this, SLOT (loaded (int)));
        disconnect (m_process, SIGNAL (lengthFound (int)),
                    this, SLOT (lengthFound (int)));
        m_process->quit ();
        source = m_process->source ();
    }
    m_process = process;
    if (!process)
        return;
    m_process->setSource (source); // will stop the process if new source
    connect (m_process, SIGNAL (started ()), this, SLOT (processStarted ()));
    connect (m_process, SIGNAL (startedPlaying ()), this, SLOT (processStartedPlaying ()));
    connect (m_process, SIGNAL (finished ()), this, SLOT (processFinished ()));
    connect (m_process, SIGNAL (positioned(int)), this, SLOT (positioned(int)));
    connect (m_process, SIGNAL (loaded (int)), this, SLOT (loaded (int)));
    connect (m_process, SIGNAL(lengthFound(int)), this, SLOT(lengthFound(int)));
    if (m_process->playing ()) {
        m_view->buttonBar ()->setPlaying (true);
        m_view->buttonBar ()->showPositionSlider (!!m_process->source ()->length());
        m_view->buttonBar ()->enableSeekButtons (m_process->source()->isSeekable());
    }
    emit processChanged (name);
}

void KMPlayer::setRecorder (const char * name) {
    KMPlayerProcess * recorder = name ? m_recorders [name] : 0L;
    if (m_recorder == recorder)
        return;
    if (m_recorder) {
        disconnect (m_recorder, SIGNAL (finished ()),
                    this, SLOT (processFinished ()));
        disconnect (recorder, SIGNAL (started()),
                    this, SLOT (recordingStarted()));
        m_recorder->quit ();
    }
    m_recorder = recorder;
    if (!recorder)
        return;
    connect (m_recorder, SIGNAL (started()), this, SLOT (recordingStarted()));
    connect (m_recorder, SIGNAL (finished()), this, SLOT (recordingFinished()));
    m_recorder->setSource (m_process->source ());
}

extern const char * strGeneralGroup;

KDE_NO_EXPORT void KMPlayer::slotPlayerMenu (int id) {
    bool playing = m_process->playing ();
    QPopupMenu * menu = m_view->buttonBar ()->playerMenu ();
    ProcessMap::const_iterator pi = m_players.begin(), e = m_players.end();
    for (unsigned i = 0; i < menu->count(); i++, ++pi) {
        int menuid = menu->idAt (i);
        menu->setItemChecked (menuid, menuid == id);
        if (menuid == id) {
            m_settings->backends [m_process->source()->name()] = pi.data ()->name ();
            if (playing && strcmp (m_process->name (), pi.data ()->name ()))
                m_process->stop ();
            setProcess (pi.data ()->name ());
        }
    }
    if (playing)
        setSource (m_process->source ()); // re-activate
}

void KMPlayer::updatePlayerMenu () {
    if (!m_view || !m_process || !m_process->source ())
        return;
    QPopupMenu * menu = m_view->buttonBar ()->playerMenu ();
    menu->clear ();
    const ProcessMap::const_iterator e = m_players.end();
    for (ProcessMap::const_iterator i = m_players.begin(); i != e; ++i) {
        KMPlayerProcess * p = i.data ();
        if (p->supports (m_process->source ()->name ())) {
            int id = menu->insertItem (p->menuName (), this, SLOT (slotPlayerMenu (int)));
            if (i.data() == m_process)
                menu->setItemChecked (id, true);
        }
    }
}

void KMPlayer::setSource (KMPlayerSource * source) {
    KMPlayerSource * oldsource = m_process->source ();
    if (oldsource) {
        oldsource->deactivate ();
        stop ();
        if (m_view) {
            if (m_view->viewer ())
                m_view->viewer ()->setAspect (0.0);
            m_view->reset ();
        }
    }
    m_view->buttonBar ()->setAutoControls (true);
    QString p = m_settings->backends [source->name()];
    if (p.isEmpty ()) {
        m_config->setGroup (strGeneralGroup);
        p = m_config->readEntry (source->name(), "");
    }
    if (p.isEmpty () || !m_players.contains (p) || !m_players [p]->supports (source->name ())) {
        p.truncate (0);
        if (!m_process->supports (source->name ())) {
            ProcessMap::const_iterator i, e = m_players.end();
            for (i = m_players.begin(); i != e; ++i)
                if (i.data ()->supports (source->name ())) {
                    p = QString (i.data ()->name ());
                    break;
                }
            if (i == e) {
                kdError() << "Source has no player" << endl;
                return;
            }
        } else
            p = QString (m_process->name ());
    }
    if (p != m_process->name ()) {
        m_players [p]->setSource (source);
        setProcess (p.ascii ());
    }
    m_settings->backends [source->name()] = m_process->name ();
    m_process->setSource (source);
    if (!m_recorder->playing ())
        m_recorder->setSource (source);
    updatePlayerMenu ();
    source->init ();
    if (source) QTimer::singleShot (0, source, SLOT (activate ()));
    emit sourceChanged (source);
}

KDE_NO_EXPORT void KMPlayer::changeURL (const QString & url) {
    emit urlChanged (url);
}

void KMPlayer::changeTitle (const QString & title) {
    emit titleChanged (title);
}

bool KMPlayer::isSeekable (void) const {
    return m_process->source ()->isSeekable ();
}

bool KMPlayer::hasLength () const {
    return m_process->source ()->hasLength (); 
}

unsigned long KMPlayer::length () const {
    return m_process->source ()->length ();
}
            
bool KMPlayer::openURL (const KURL & url) {
    kdDebug () << "KMPlayer::openURL " << url.url() << url.isValid () << endl;
    if (!m_view || url.isEmpty ()) return false;
    stop ();
    KMPlayerSource * source = m_sources ["urlsource"];
    source->setSubURL (KURL ());
    source->setURL (url);
    source->setIdentified (false);
    setSource (source);
    return true;
}

bool KMPlayer::closeURL () {
    stop ();
    if (!m_view) return false;
    if (m_view->viewer ())
        m_view->viewer ()->setAspect (0.0);
    m_view->reset ();
    return true;
}

bool KMPlayer::openFile () {
    return false;
}

void KMPlayer::keepMovieAspect (bool b) {
    if (!m_view) return;
    m_view->setKeepSizeRatio (b);
    if (m_view->viewer ())
        m_view->viewer ()->setAspect (b ? m_process->source ()->aspect () : 0.0);
}

KDE_NO_EXPORT void KMPlayer::recordingStarted () {
    if (!m_view) return;
    m_view->buttonBar ()->setRecording (true);
    if (m_settings->replayoption == KMPlayerSettings::ReplayAfter)
        m_record_timer = startTimer (1000 * m_settings->replaytime);
    emit startRecording ();
}

KDE_NO_EXPORT void KMPlayer::recordingFinished () {
    if (!m_view) return;
    m_view->buttonBar ()->setRecording (false);
    emit stopRecording ();
    killTimer (m_record_timer);
    m_record_timer = 0;
    if (m_settings->replayoption == KMPlayerSettings::ReplayFinished ||
        (m_settings->replayoption == KMPlayerSettings::ReplayAfter && !playing ())) {
        Recorder * rec = dynamic_cast <Recorder*> (m_recorder);
        if (rec)
            openURL (rec->recordURL ());
    }
}

void KMPlayer::timerEvent (QTimerEvent * e) {
    kdDebug () << "record timer event" << (m_recorder->playing () && !playing ()) << endl;
    killTimer (e->timerId ());
    m_record_timer = 0;
    if (m_recorder->playing () && !playing ()) {
        Recorder * rec = dynamic_cast <Recorder*> (m_recorder);
        if (rec)
            openURL (rec->recordURL ());
    }
}

void KMPlayer::processFinished () {
    kdDebug () << "process finished" << endl;
    if (m_process->source ()->hasLength () &&
            m_process->source ()->position () > m_process->source ()->length ())
        m_process->source ()->setLength (m_process->source ()->position ());
    m_process->source ()->setPosition (0);
    if (!m_view) return;
    m_view->buttonBar ()->setPlaying (false);
    m_view->reset ();
    emit stopPlaying ();
}

void KMPlayer::processStarted () {
    if (!m_view) return;
    m_view->buttonBar ()->setPlaying (true);
}

KDE_NO_EXPORT void KMPlayer::positioned (int pos) {
    if (m_view && !m_bPosSliderPressed)
        m_view->buttonBar ()->setPlayingProgress (pos);
}

void KMPlayer::loaded (int percentage) {
    if (m_view && !m_bPosSliderPressed)
        m_view->buttonBar ()->setLoadingProgress (percentage);
    emit loading (percentage);
}

void KMPlayer::lengthFound (int len) {
    if (!m_view) return;
    m_view->buttonBar ()->setPlayingLength (len);
}

void KMPlayer::processStartedPlaying () {
    if (!m_view) return;
    m_process->view ()->videoStart ();
    kdDebug () << "KMPlayer::processStartedPlaying " << endl;
    if (m_settings->sizeratio && m_view->viewer ())
        m_view->viewer ()->setAspect (m_process->source ()->aspect ());
    m_view->buttonBar ()->showPositionSlider (!!m_process->source ()->length());
    m_view->buttonBar ()->enableSeekButtons (m_process->source()->isSeekable());
    emit loading (100);
    emit startPlaying ();
}

unsigned long KMPlayer::position () const {
    return 100 * m_process->source ()->position ();
}

void KMPlayer::pause () {
    m_process->pause ();
}

void KMPlayer::back () {
    m_process->source ()->backward ();
}

void KMPlayer::forward () {
    m_process->source ()->forward ();
}

void KMPlayer::playListItemSelected (QListViewItem * item) {
    if (m_in_update_tree) return;
    KMPlayerListViewItem * vi = static_cast <KMPlayerListViewItem *> (item);
    m_process->source ()->jump (vi->m_elm);
}

void KMPlayer::updateTree (const ElementPtr & d, const ElementPtr & c) {
    m_in_update_tree = true;
    if (m_process && m_process->view ())
        m_process->view ()->playList ()->updateTree (d, c);
    m_in_update_tree = false;
}

void KMPlayer::record () {
    if (m_view) m_view->setCursor (QCursor (Qt::WaitCursor));
    if (m_recorder->playing ()) {
        m_recorder->stop ();
    } else {
        m_process->stop ();
        m_settings->show  ("RecordPage");
        m_view->buttonBar ()->setRecording (false);
    }
    if (m_view) m_view->setCursor (QCursor (Qt::ArrowCursor));
}

void KMPlayer::play () {
    m_process->play ();
    m_view->buttonBar ()->setPlaying (m_process->playing ());
}

bool KMPlayer::playing () const {
    return m_process && m_process->playing ();
}

void KMPlayer::stop () {
    if (m_view) {
        if (!m_view->buttonBar ()->button (KMPlayerControlPanel::button_stop)->isOn ())
        m_view->buttonBar ()->button (KMPlayerControlPanel::button_stop)->toggle ();
        m_view->setCursor (QCursor (Qt::WaitCursor));
    }
    if (m_process) {
        m_process->source ()->first ();
        m_process->quit ();
    }
    if (m_view) {
        m_view->setCursor (QCursor (Qt::ArrowCursor));
        if (m_view->buttonBar ()->button (KMPlayerControlPanel::button_stop)->isOn ())
            m_view->buttonBar ()->button (KMPlayerControlPanel::button_stop)->toggle ();
    }
}

void KMPlayer::seek (unsigned long msec) {
    m_process->seek (msec/100, true);
}

void KMPlayer::adjustVolume (int incdec) {
    m_process->volume (incdec, false);
}

void KMPlayer::sizes (int & w, int & h) const {
    if (m_noresize && m_view->viewer ()) {
        w = m_view->viewer ()->width ();
        h = m_view->viewer ()->height ();
    } else {
        w = m_process->source ()->width ();
        h = m_process->source ()->height ();
    }
}

KDE_NO_EXPORT void KMPlayer::posSliderPressed () {
    m_bPosSliderPressed=true;
}

KDE_NO_EXPORT void KMPlayer::posSliderReleased () {
    m_bPosSliderPressed=false;
#if (QT_VERSION < 0x030200)
    const QSlider * posSlider = dynamic_cast <const QSlider *> (sender ());
#else
    const QSlider * posSlider = ::qt_cast<const QSlider *> (sender ());
#endif
    if (posSlider)
        m_process->seek (posSlider->value(), true);
}

KDE_NO_EXPORT void KMPlayer::contrastValueChanged (int val) {
    m_settings->contrast = val;
    m_process->contrast (val, true);
}

KDE_NO_EXPORT void KMPlayer::brightnessValueChanged (int val) {
    m_settings->brightness = val;
    m_process->brightness (val, true);
}

KDE_NO_EXPORT void KMPlayer::hueValueChanged (int val) {
    m_settings->hue = val;
    m_process->hue (val, true);
}

KDE_NO_EXPORT void KMPlayer::saturationValueChanged (int val) {
    m_settings->saturation = val;
    m_process->saturation (val, true);
}

KDE_NO_EXPORT void KMPlayer::positionValueChanged (int pos) {
    QSlider * slider = ::qt_cast <QSlider *> (sender ());
    if (slider && slider->isEnabled ())
        m_process->seek (pos, true);
}

KDE_NO_EXPORT void KMPlayer::fullScreen () {
    m_process->view ()->fullScreen ();
}

KAboutData* KMPlayer::createAboutData () {
    KMessageBox::error(0L, "createAboutData", "KMPlayer");
    return 0;
}

//-----------------------------------------------------------------------------

KMPlayerSource::KMPlayerSource (const QString & name, KMPlayer * player, const char * n)
 : QObject (player, n),
   m_name (name), m_player (player),
   m_auto_play (true) {
    kdDebug () << "KMPlayerSource::KMPlayerSource" << endl;
    init ();
}

KMPlayerSource::~KMPlayerSource () {
    kdDebug () << "KMPlayerSource::~KMPlayerSource" << endl;
    if (m_document)
        m_document->document ()->dispose ();
}

void KMPlayerSource::init () {
    m_width = 0;
    m_height = 0;
    m_aspect = 0.0;
    setLength (0);
    m_position = 0;
    m_identified = false;
    m_recordcmd.truncate (0);
}

void KMPlayerSource::setLength (int len) {
    m_length = len;
}

static void printTree (ElementPtr root, QString off=QString()) {
    if (!root) {
        kdDebug() << off << "[null]" << endl;
        return;
    }
    kdDebug() << off << root->nodeName() << " " << (Element*)root << (root->isMrl() ? root->mrl ()->src : QString ("-")) << endl;
    off += QString ("  ");
    for (ElementPtr e = root->firstChild(); e; e = e->nextSibling())
        printTree(e, off);
}

void KMPlayerSource::setURL (const KURL & url) {
    m_url = url;
    m_back_request = 0L;
    if (m_document && !m_document->hasChildNodes () &&
                (m_document->mrl()->src.isEmpty () || m_document->mrl()->src == url.url ()))
        // special case, mime is set first by plugin FIXME v
        m_document->mrl()->src = url.url ();
    else {
        if (m_document)
            m_document->document ()->dispose ();
        m_document = (new Document (url.url ()))->self ();
        if (m_player->process () && m_player->process ()->source () == this)
            m_player->updateTree (m_document, m_current);
    }
    m_current = m_document;
}

QString KMPlayerSource::first () {
    if (m_document) {
        kdDebug() << "KMPlayerSource::first" << endl;
        m_current = m_document;
        if (!m_document->isMrl ())
            return next ();
        m_player->updateTree (m_document, m_current);
    }
    return current ();
}

QString KMPlayerSource::current () {
    return m_current ? m_current->mrl()->src : QString ();
}

void KMPlayerSource::getCurrent () {
    QString url = current ();
    m_player->changeURL (url);
    if (m_player->process () && m_player->process ()->view ())
        m_player->process ()->view ()->videoStop (); // show buttonbar
    emit currentURL (url);
}

static ElementPtr findDepthFirst (ElementPtr elm) {
    if (!elm)
        return ElementPtr ();
    ElementPtr tmp = elm;
    for ( ; tmp; tmp = tmp->nextSibling ()) {
        if (tmp->isMrl ())
            return tmp;
        ElementPtr tmp2 = findDepthFirst (tmp->firstChild ());
        if (tmp2)
            return tmp2;
    }
    return ElementPtr ();
}

QString KMPlayerSource::next () {
    if (m_document) {
        kdDebug() << "KMPlayerSource::next" << endl;
        if (m_back_request && m_back_request->isMrl ()) {
            m_current = m_back_request;
            m_back_request = 0L;
        } else if (m_current) {
            ElementPtr e = findDepthFirst (m_current->isMrl () ? m_current->nextSibling (): m_current);
            if (e) {
                m_current = e;
            } else while (m_current) {
                m_current = m_current->parentNode ();
                if (m_current && m_current->nextSibling ()) {
                    m_current = m_current->nextSibling ();
                    e = findDepthFirst (m_current);
                    if (e) {
                        m_current = e;
                        break;
                    }
                }
            }
        }
        m_player->updateTree (m_document, m_current);
    }
    return current ();
}

void KMPlayerSource::insertURL (const QString & mrl) {
    kdDebug() << "KMPlayerSource::insertURL " << (Element*)m_current << mrl << endl;
    KURL url (current (), mrl);
    if (!url.isValid ())
        kdError () << "try to append non-valid url" << endl;
    else if (KURL (current ()) == url)
        kdError () << "try to append url to itself" << endl;
    else if (m_current)
        m_current->appendChild ((new GenericURL (m_document, KURL::decode_string (url.url ()), KURL::decode_string (mrl)))->self ());
}

void KMPlayerSource::play () {
    m_player->updateTree (m_document, m_current);
    QTimer::singleShot (0, m_player, SLOT (play ()));
    printTree (m_document);
}

void KMPlayerSource::backward () {
    if (m_document->hasChildNodes ()) {
        m_back_request = m_current;
        if (!m_back_request || m_back_request == m_document) {
            m_back_request = m_document->lastChild ();
            while (m_back_request->lastChild () && !m_back_request->isMrl ())
                m_back_request = m_back_request->lastChild ();
            if (m_back_request->isMrl ())
                return;
        }
        while (m_back_request && m_back_request != m_document) {
            if (m_back_request->previousSibling ()) {
                m_back_request = m_back_request->previousSibling ();
                ElementPtr e = findDepthFirst (m_back_request); // lastDepth..
                if (e) {
                    m_back_request = e;
                    m_player->process ()->stop ();
                    return;
                }
            } else
                m_back_request = m_back_request->parentNode ();
        }
        m_back_request = 0L;
    } else
        m_player->process ()->seek (-1 * m_player->settings ()->seektime * 10, false);
}

void KMPlayerSource::forward () {
    if (m_document->hasChildNodes ()) {
        m_player->process ()->stop ();
    } else
        m_player->process ()->seek (m_player->settings()->seektime * 10, false);
}

void KMPlayerSource::jump (ElementPtr e) {
    if (e->isMrl ()) {
        if (m_player->playing ()) {
            m_back_request = e;
            m_player->process ()->stop ();
        } else
            m_current = e;
    } else
        m_player->updateTree (m_document, m_current);
}

QString KMPlayerSource::mime () const {
    return m_current ? m_current->mrl ()->mimetype : (m_document ? m_document->mrl ()->mimetype : QString ());
}

void KMPlayerSource::setMime (const QString & m) {
    kdDebug () << "setMime " << m << endl;
    if (m_current)
        m_current->mrl ()->mimetype = m;
    else {
        if (m_document)
            m_document->document ()->dispose ();
        m_document = (new Document (QString ()))->self ();
    }
}

bool KMPlayerSource::processOutput (const QString & str) {
    if (m_identified)
        return false;
    if (str.startsWith ("ID_VIDEO_WIDTH")) {
        int pos = str.find ('=');
        if (pos > 0)
            setWidth (str.mid (pos + 1).toInt());
        kdDebug () << "KMPlayerSource::processOutput " << width() << endl;
    } else if (str.startsWith ("ID_VIDEO_HEIGHT")) {
        int pos = str.find ('=');
        if (pos > 0)
            setHeight (str.mid (pos + 1).toInt());
    } else if (str.startsWith ("ID_VIDEO_ASPECT")) {
        int pos = str.find ('=');
        if (pos > 0)
            setAspect (str.mid (pos + 1).replace (',', '.').toFloat ());
    } else
        return false;
    return true;
}

QString KMPlayerSource::filterOptions () {
    KMPlayerSettings* m_settings = m_player->settings ();
    QString PPargs ("");
    if (m_settings->postprocessing)
    {
        if (m_settings->pp_default)
            PPargs = "-vop pp=de";
        else if (m_settings->pp_fast)
            PPargs = "-vop pp=fa";
        else if (m_settings->pp_custom) {
            PPargs = "-vop pp=";
            if (m_settings->pp_custom_hz) {
                PPargs += "hb";
                if (m_settings->pp_custom_hz_aq && \
                        m_settings->pp_custom_hz_ch)
                    PPargs += ":ac";
                else if (m_settings->pp_custom_hz_aq)
                    PPargs += ":a";
                else if (m_settings->pp_custom_hz_ch)
                    PPargs += ":c";
                PPargs += "/";
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
                PPargs += "/";
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
                PPargs += "/";
            }
            if (m_settings->pp_custom_al) {
                PPargs += "al";
                if (m_settings->pp_custom_al_f)
                    PPargs += ":f";
                PPargs += "/";
            }
            if (m_settings->pp_custom_tn) {
                PPargs += "tn";
                /*if (1 <= m_settings->pp_custom_tn_s <= 3){
                    PPargs += ":";
                    PPargs += m_settings->pp_custom_tn_s;
                    }*/ //disabled 'cos this is wrong
                PPargs += "/";
            }
            if (m_settings->pp_lin_blend_int) {
                PPargs += "lb";
                PPargs += "/";
            }
            if (m_settings->pp_lin_int) {
                PPargs += "li";
                PPargs += "/";
            }
            if (m_settings->pp_cub_int) {
                PPargs += "ci";
                PPargs += "/";
            }
            if (m_settings->pp_med_int) {
                PPargs += "md";
                PPargs += "/";
            }
            if (m_settings->pp_ffmpeg_int) {
                PPargs += "fd";
                PPargs += "/";
            }
        }
        if (PPargs.endsWith("/"))
            PPargs.truncate(PPargs.length()-1);
    }
    return PPargs;
}

bool KMPlayerSource::hasLength () {
    return true;
}

bool KMPlayerSource::isSeekable () {
    return true;
}

void KMPlayerSource::setIdentified (bool b) {
    kdDebug () << "KMPlayerSource::setIdentified " << m_identified << b <<endl;
    m_identified = b;
}

QString KMPlayerSource::prettyName () {
    return QString (i18n ("Unknown"));
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT KMPlayerURLSource::KMPlayerURLSource (KMPlayer * player, const KURL & url)
    : KMPlayerSource (i18n ("URL"), player, "urlsource"), m_job (0L) {
    setURL (url);
    kdDebug () << "KMPlayerURLSource::KMPlayerURLSource" << endl;
}

KDE_NO_CDTOR_EXPORT KMPlayerURLSource::~KMPlayerURLSource () {
    kdDebug () << "KMPlayerURLSource::~KMPlayerURLSource" << endl;
}

KDE_NO_EXPORT void KMPlayerURLSource::init () {
    KMPlayerSource::init ();
}

KDE_NO_EXPORT bool KMPlayerURLSource::hasLength () {
    return !!length ();
}

KDE_NO_EXPORT void KMPlayerURLSource::activate () {
    if (url ().isEmpty ())
        return;
    if (m_auto_play)
        play ();
}

KDE_NO_EXPORT void KMPlayerURLSource::deactivate () {
    if (m_job)
        m_job->kill (); // silent, no kioResult signal
    m_job = 0L;
}

KDE_NO_EXPORT QString KMPlayerURLSource::prettyName () {
    if (m_url.isEmpty ())
        return QString (i18n ("URL"));
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
        while (path.url ().length () + len > 50 && path != path.upURL ()) {
            path = path.upURL ();
            modified = true;
        }
        QString dir = path.directory ();
        if (!dir.endsWith (QString ("/")))
            dir += '/';
        if (modified)
            dir += QString (".../");
        newurl += dir + file;
        return QString (i18n ("URL - %1").arg (newurl));
    }
    return QString (i18n ("URL - %1").arg (m_url.prettyURL ()));
}

class MMXmlContentHandler : public QXmlDefaultHandler {
    KMPlayerURLSource * m_urlsource;
public:
    KDE_NO_CDTOR_EXPORT MMXmlContentHandler (ElementPtr d) : m_start (d), m_elm (d), m_ignore_depth (0) {}
    KDE_NO_CDTOR_EXPORT ~MMXmlContentHandler () {}
    KDE_NO_EXPORT bool startDocument () {
        return m_start->self () == m_elm;
    }
    KDE_NO_EXPORT bool endDocument () {
        return m_start->self () == m_elm;
    }
    KDE_NO_EXPORT bool startElement (const QString &, const QString &, const QString & tag,
            const QXmlAttributes & atts) {
        if (m_ignore_depth) {
            kdDebug () << "Warning: ignored tag " << tag << endl;
            m_ignore_depth++;
        } else {
            ElementPtr e = m_elm->childFromTag (tag);
            if (e) {
                kdDebug () << "Found tag " << tag << endl;
                e->setAttributes (atts);
                m_elm->appendChild (e);
                m_elm = e;
            } else {
                kdError () << "Warning: unknown tag " << tag << endl;
                m_ignore_depth = 1;
            }
        }
        return true;
    }
    KDE_NO_EXPORT bool endElement (const QString & /*nsURI*/,
                   const QString & /*tag*/, const QString & /*fqtag*/) {
        if (m_ignore_depth) {
            // kdError () << "Warning: ignored end tag " << endl;
            m_ignore_depth--;
            return true;
        }
        if (m_elm == m_start->self ()) {
            kdError () << "m_elm == m_start, stack underflow " << endl;
            return false;
        }
        // kdError () << "end tag " << endl;
        m_elm = m_elm->parentNode ();
        return true;
    }
    KDE_NO_EXPORT bool characters (const QString & ch) {
        if (m_ignore_depth)
            return true;
        if (!m_elm)
            return false;
        m_elm->characterData (ch);
        return true;
    }
    KDE_NO_EXPORT bool fatalError (const QXmlParseException & ex) {
        kdError () << "fatal error " << ex.message () << endl;
        return true;
    }
    ElementPtr m_start;
    ElementPtr m_elm;
    int m_ignore_depth;
};

class FilteredInputSource : public QXmlInputSource {
    QTextStream  & textstream;
    QString & buffer;
    int pos;
public:
    KDE_NO_CDTOR_EXPORT FilteredInputSource (QTextStream & ts, QString & b) : textstream (ts), buffer (b), pos (0) {}
    KDE_NO_CDTOR_EXPORT ~FilteredInputSource () {}
    KDE_NO_EXPORT QString data () { return textstream.read (); }
    void fetchData ();
    QChar next ();
};

KDE_NO_EXPORT QChar FilteredInputSource::next () {
    if (pos + 8 >= (int) buffer.length ())
        fetchData ();
    if (pos >= (int) buffer.length ())
        return QXmlInputSource::EndOfData;
    QChar ch = buffer.at (pos++);
    if (ch == QChar ('&')) {
        QRegExp exp (QString ("\\w+;"));
        if (buffer.find (exp, pos) != pos) {
            buffer = QString ("&amp;") + buffer.mid (pos);
            pos = 1;
        }
    }
    return ch;
}

KDE_NO_EXPORT void FilteredInputSource::fetchData () {
    if (pos > 0)
        buffer = buffer.mid (pos);
    pos = 0;
    if (textstream.atEnd ())
        return;
    buffer += textstream.readLine ();
}

static bool isPlayListMime (const QString & mime) {
    const char * mimestr = mime.ascii ();
    return mimestr && (!strcmp (mimestr, "audio/mpegurl") ||
            !strcmp (mimestr ,"audio/x-mpegurl") ||
            !strcmp (mimestr ,"video/x-ms-wmp") ||
            !strcmp (mimestr ,"video/x-ms-asf") ||
            !strcmp (mimestr ,"audio/x-scpls") ||
            !strcmp (mimestr ,"audio/x-pn-realaudio") ||
            !strcmp (mimestr ,"audio/vnd.rn-realaudio") ||
            !strcmp (mimestr ,"application/smil") ||
            !strcmp (mimestr ,"text/xml") ||
            !strcmp (mimestr ,"application/x-mplayer2"));
}

KDE_NO_EXPORT void KMPlayerURLSource::read (QTextStream & textstream) {
    QString line;
    do {
        line = textstream.readLine ();
    } while (!line.isNull () && line.stripWhiteSpace ().isEmpty ());
    if (!line.isNull ()) {
        if (mime () == QString ("audio/x-scpls")) {
            bool groupfound = false;
            int nr = -1;
            do {
                line = line.stripWhiteSpace ();
                if (!line.isEmpty ()) {
                    if (line.startsWith (QString ("[")) && line.endsWith (QString ("]"))) {
                        if (!groupfound && line.mid (1, line.length () - 2).stripWhiteSpace () == QString ("playlist"))
                            groupfound = true;
                        else
                            break;
                        kdDebug () << "Group found: " << line << endl;
                    } else if (groupfound) {
                        int eq_pos = line.find (QChar ('='));
                        if (eq_pos > 0) {
                            if (line.lower ().startsWith (QString ("numberofentries"))) {
                                nr = line.mid (eq_pos + 1).stripWhiteSpace ().toInt ();
                                kdDebug () << "numberofentries : " << nr << endl;
                            } else if (nr > 0 && line.lower ().startsWith (QString ("file"))) {
                                QString mrl = line.mid (eq_pos + 1).stripWhiteSpace ();
                                if (!mrl.isEmpty ())
                                    insertURL (mrl);
                            }
                        }
                    }
                }
                line = textstream.readLine ();
            } while (!line.isNull ());
        } else if (line.stripWhiteSpace ().startsWith (QChar ('<'))) {
            QXmlSimpleReader reader;
            MMXmlContentHandler content_handler (m_current);
            FilteredInputSource input_source (textstream, line);
            reader.setContentHandler (&content_handler);
            reader.setErrorHandler (&content_handler);
            reader.parse (&input_source);
        } else if (line.lower () != QString ("[reference]")) do {
            QString mrl = line.stripWhiteSpace ();
            if (mrl.lower ().startsWith (QString ("asf ")))
                mrl = mrl.mid (4).stripWhiteSpace ();
            if (!mrl.isEmpty () && !mrl.startsWith (QChar ('#')))
                insertURL (mrl);
            line = textstream.readLine ();
        } while (!line.isNull ()); /* TODO && m_document.size () < 1024 / * support 1k entries * /);*/
    }
;
}

KDE_NO_EXPORT void KMPlayerURLSource::kioData (KIO::Job *, const QByteArray & d) {
    int size = m_data.size ();
    int newsize = size + d.size ();
    kdDebug () << "KMPlayerURLSource::kioData: " << newsize << endl;
    if (newsize > 50000) {
        m_job->kill (false);
    } else  {
        m_data.resize (newsize);
        memcpy (m_data.data () + size, d.data (), newsize - size);
    }
}

KDE_NO_EXPORT void KMPlayerURLSource::kioMimetype (KIO::Job *, const QString & mimestr) {
    kdDebug () << "KMPlayerURLSource::kioMimetype " << mimestr << endl;
    setMime (mimestr);
    if (!isPlayListMime (mimestr))
        m_job->kill (false);
}

KDE_NO_EXPORT void KMPlayerURLSource::kioResult (KIO::Job *) {
    m_job = 0L; // KIO::Job::kill deletes itself
    QTextStream textstream (m_data, IO_ReadOnly);
    if (isPlayListMime (mime ()))
        read (textstream);
    if (m_current) {
        m_current->mrl ()->parsed = true;
        if (!m_current->isMrl ())
            next ();
        getCurrent ();
    }
    m_player->process ()->view ()->buttonBar ()->setPlaying (false);
}

void KMPlayerURLSource::getCurrent () {
    if (m_current && !m_current->isMrl ())
        next ();
    KURL url (current ());
    int depth = 0;
    if (m_current)
        for (ElementPtr e = m_current; e->parentNode (); e = e->parentNode ())
            ++depth;
    if (depth > 40 || url.isEmpty () || m_current->mrl ()->parsed) {
        KMPlayerSource::getCurrent ();
    } else {
        QString mimestr = mime ();
        int plugin_pos = mimestr.find ("-plugin");
        if (plugin_pos > 0)
            mimestr.truncate (plugin_pos);
        bool maybe_playlist = isPlayListMime (mimestr);
        kdDebug () << "KMPlayerURLSource::getCurrent " << mimestr << maybe_playlist << endl;
        if (url.isLocalFile ()) {
            QFile file (url.path ());
            if (!file.exists ())
                return;
            if (mimestr.isEmpty ()) {
                KMimeType::Ptr mime = KMimeType::findByURL (url);
                if (mime) {
                    mimestr = mime->name ();
                    setMime (mimestr);
                    maybe_playlist = isPlayListMime (mimestr);
                }
            }
            if (maybe_playlist && file.size () < 50000 && file.open (IO_ReadOnly)) {
                QTextStream textstream (&file);
                read (textstream);
            }
            m_current->mrl ()->parsed = true;
            getCurrent ();
        } else if ((maybe_playlist &&
                    url.protocol ().compare (QString ("mms")) &&
                    url.protocol ().compare (QString ("rtsp")) &&
                    url.protocol ().compare (QString ("rtp"))) ||
                (mimestr.isEmpty () &&
                 url.protocol ().startsWith (QString ("http")))) {
            m_data.truncate (0);
            m_job = KIO::get (url, false, false);
            m_job->addMetaData ("PropagateHttpHeader", "true");
            connect (m_job, SIGNAL (data (KIO::Job *, const QByteArray &)),
                    this, SLOT (kioData (KIO::Job *, const QByteArray &)));
            //connect( m_job, SIGNAL(connected(KIO::Job*)),
            //         this, SLOT(slotConnected(KIO::Job*)));
            connect (m_job, SIGNAL (mimetype (KIO::Job *, const QString &)),
                    this, SLOT (kioMimetype (KIO::Job *, const QString &)));
            connect (m_job, SIGNAL (result (KIO::Job *)),
                    this, SLOT (kioResult (KIO::Job *)));
            m_player->process ()->view ()->buttonBar ()->setPlaying (true);
        } else {
            m_current->mrl ()->parsed = true;
            getCurrent ();
        }
    }
}

KDE_NO_EXPORT void KMPlayerURLSource::play () {
    KMPlayerSource::play ();
}

#include "kmplayerpartbase.moc"
#include "kmplayersource.moc"
