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
#include <algorithm>
#include <functional>

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
    virtual void openBookmarkURL(const QString& _url);
    virtual QString currentTitle() const;
    virtual QString currentURL() const;
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
 : KMediaPlayer::Player (wparent, wname, parent, name),
   m_config (config),
   m_view (new KMPlayerView (wparent)),
   m_settings (new KMPlayerSettings (this, config)),
   m_process (0L),
   m_recorder (0L),
   m_mplayer (new MPlayer (this)),
   m_mencoder (new MEncoder (this)),
   m_mplayerdumpstream (new MPlayerDumpstream (this)),
   m_ffmpeg (new FFMpeg (this)),
   m_xine (new Xine (this)),
   m_bookmark_menu (0L),
   m_record_timer (0),
   m_ispart (false),
   m_noresize (false) {
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
    m_urlsource = new KMPlayerURLSource (this);
}

void KMPlayer::showConfigDialog () {
    m_settings->show ("GeneralPage");
}

void KMPlayer::addControlPanel (KMPlayerControlPanel * panel) {
    connect (panel->backButton (), SIGNAL (clicked ()), this, SLOT (back ()));
    connect (panel->playButton (), SIGNAL (clicked ()), this, SLOT (play ()));
    connect (panel->forwardButton (), SIGNAL (clicked ()), this, SLOT (forward ()));
    connect (panel->pauseButton (), SIGNAL (clicked ()), this, SLOT (pause ()));
    connect (panel->stopButton (), SIGNAL (clicked ()), this, SLOT (stop ()));
    connect (panel->recordButton(), SIGNAL (clicked()), this, SLOT (record()));
    connect (panel->positionSlider (), SIGNAL (valueChanged (int)), this, SLOT (positionValueChanged (int)));
    connect (panel->positionSlider (), SIGNAL (sliderPressed()), this, SLOT (posSliderPressed()));
    connect (panel->positionSlider (), SIGNAL (sliderReleased()), this, SLOT (posSliderReleased()));
    connect (panel->contrastSlider (), SIGNAL (valueChanged(int)), this, SLOT (contrastValueChanged(int)));
    connect (panel->brightnessSlider (), SIGNAL (valueChanged(int)), this, SLOT (brightnessValueChanged(int)));
    connect (panel->hueSlider (), SIGNAL (valueChanged(int)), this, SLOT (hueValueChanged(int)));
    connect (panel->saturationSlider (), SIGNAL (valueChanged(int)), this, SLOT (saturationValueChanged(int)));
    connect (panel, SIGNAL (destroyed(QObject *)), this, SLOT (controlPanelDestroyed(QObject *)));
    panel->popupMenu()->connectItem (KMPlayerControlPanel::menu_fullscreen, m_view, SLOT (fullScreen ()));
#ifdef HAVE_XINE
    QPopupMenu *menu = panel->playerMenu ();
    menu->connectItem (menu->idAt(0), this, SLOT (setMPlayer (int)));
    menu->connectItem (menu->idAt(1), this, SLOT (setXine (int)));
#endif
    if ((playing () && !panel->playButton ()->isOn ()) ||
            (!playing () && panel->playButton ()->isOn ()))
        panel->playButton ()->toggle ();
    m_panels.push_back (panel);
}

void KMPlayer::removeControlPanel (KMPlayerControlPanel * panel) {
    if (std::find (m_panels.begin(), m_panels.end(), panel) == m_panels.end()) {
        kdError () << "Control panel not found" << endl;
        return;
    }
    disconnect (panel->backButton (), SIGNAL (clicked ()), this, SLOT (back ()));
    disconnect (panel->playButton (), SIGNAL (clicked ()), this, SLOT (play ()));
    disconnect (panel->forwardButton (), SIGNAL (clicked ()), this, SLOT (forward ()));
    disconnect (panel->pauseButton (), SIGNAL (clicked ()), this, SLOT (pause ()));
    disconnect (panel->stopButton (), SIGNAL (clicked ()), this, SLOT (stop ()));
    disconnect (panel->recordButton(), SIGNAL (clicked()), this, SLOT (record()));
    disconnect (panel->positionSlider (), SIGNAL (valueChanged (int)), this, SLOT (positionValueChanged (int)));
    disconnect (panel->positionSlider (), SIGNAL (sliderPressed()), this, SLOT (posSliderPressed()));
    disconnect (panel->positionSlider (), SIGNAL (sliderReleased()), this, SLOT (posSliderReleased()));
    disconnect (panel->contrastSlider (), SIGNAL (valueChanged(int)), this, SLOT (contrastValueChanged(int)));
    disconnect (panel->brightnessSlider (), SIGNAL (valueChanged(int)), this, SLOT (brightnessValueChanged(int)));
    disconnect (panel->hueSlider (), SIGNAL (valueChanged(int)), this, SLOT (hueValueChanged(int)));
    disconnect (panel->saturationSlider (), SIGNAL (valueChanged(int)), this, SLOT (saturationValueChanged(int)));
    disconnect (panel, SIGNAL (destroyed(QObject *)), this, SLOT (controlPanelDestroyed(QObject *)));
    panel->popupMenu()->disconnectItem (KMPlayerControlPanel::menu_fullscreen, m_view, SLOT (fullScreen ()));
#ifdef HAVE_XINE
    QPopupMenu *menu = panel->playerMenu ();
    menu->disconnectItem (menu->idAt(0), this, SLOT (setMPlayer (int)));
    menu->disconnectItem (menu->idAt(1), this, SLOT (setXine (int)));
#endif
    m_panels.remove (panel);
}

KDE_NO_EXPORT void KMPlayer::controlPanelDestroyed (QObject * panel) {
    m_panels.remove (static_cast <KMPlayerControlPanel *> (panel));
}

void KMPlayer::init (KActionCollection * action_collection) {
    m_view->init ();
    m_settings->readConfig ();
    m_bookmark_menu = new KBookmarkMenu (m_bookmark_manager, m_bookmark_owner,
                        m_view->buttonBar ()->bookmarkMenu (), action_collection, true, true);
    setProcess (m_mplayer);
    addControlPanel (m_view->buttonBar ());
    m_bPosSliderPressed = false;
    m_view->buttonBar ()->contrastSlider ()->setValue (m_settings->contrast);
    m_view->buttonBar ()->brightnessSlider ()->setValue (m_settings->brightness);
    m_view->buttonBar ()->hueSlider ()->setValue (m_settings->hue);
    m_view->buttonBar ()->saturationSlider ()->setValue (m_settings->saturation);
    connect (m_view, SIGNAL (urlDropped (const KURL &)), this, SLOT (openURL (const KURL &)));
    m_view->buttonBar ()->popupMenu ()->connectItem (KMPlayerControlPanel::menu_config,
                                       this, SLOT (showConfigDialog ()));
    setRecorder (m_mencoder);
    //connect (m_view->configButton (), SIGNAL (clicked ()), m_settings, SLOT (show ()));
}

KMPlayer::~KMPlayer () {
    kdDebug() << "KMPlayer::~KMPlayer" << endl;
    if (!m_ispart)
        delete (KMPlayerView*) m_view;
    m_view = (KMPlayerView*) 0;
    stop ();
    delete m_settings;
    delete m_bookmark_menu;
    delete m_bookmark_manager;
    delete m_bookmark_owner;
}

KMediaPlayer::View* KMPlayer::view () {
    return m_view;
}

void KMPlayer::setProcess (KMPlayerProcess * process) {
    if (m_process == process)
        return;
    KMPlayerSource * source = m_urlsource;
    if (m_process) {
        disconnect (m_process, SIGNAL (finished ()),
                    this, SLOT (processFinished ()));
        disconnect (m_process, SIGNAL (started ()),
                    this, SLOT (processStarted ()));
        m_process->quit ();
        source = m_process->source ();
    }
    m_process = process;
    m_process->setSource (source); // will stop the process
    connect (m_process, SIGNAL (started ()), this, SLOT (processStarted ()));
    connect (m_process, SIGNAL (finished ()), this, SLOT (processFinished ()));
}

void KMPlayer::setRecorder (KMPlayerProcess * recorder) {
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
    connect (m_recorder, SIGNAL (started()), this, SLOT (recordingStarted()));
    connect (m_recorder, SIGNAL (finished()), this, SLOT (recordingFinished()));
    m_recorder->setSource (m_process->source ());
}

extern const char * strUrlBackend;
extern const char * strMPlayerGroup;

KDE_NO_EXPORT void KMPlayer::setXine (int id) {
    bool playing = m_process->playing ();
    m_settings->urlbackend = QString ("Xine");
    m_config->setGroup (strMPlayerGroup);
    m_config->writeEntry (strUrlBackend, m_settings->urlbackend);
    m_config->sync ();
    setProcess (m_xine);
    QPopupMenu * menu = m_view->buttonBar ()->playerMenu ();
    for (unsigned i = 0; i < menu->count(); i++) {
        int menuid = menu->idAt (i);
        menu->setItemChecked (menuid, menuid == id);
    }
    if (playing)
        setSource (m_process->source ()); // re-activate
}

KDE_NO_EXPORT void KMPlayer::setMPlayer (int id) {
    bool playing = m_process->playing ();
    m_settings->urlbackend = QString ("MPlayer");
    m_config->setGroup (strMPlayerGroup);
    m_config->writeEntry (strUrlBackend, m_settings->urlbackend);
    m_config->sync ();
    setProcess (m_mplayer);
    QPopupMenu * menu = m_view->buttonBar ()->playerMenu ();
    for (unsigned i = 0; i < menu->count(); i++) {
        int menuid = menu->idAt (i);
        menu->setItemChecked (menuid, menuid == id);
    }
    if (playing)
        setSource (m_process->source ()); // re-activate
}

void KMPlayer::enablePlayerMenu (bool enable) {
#ifdef HAVE_XINE
    if (!m_view)
        return;
    ControlPanelList::iterator e = m_panels.end();
    for (ControlPanelList::iterator i = m_panels.begin (); i != e; ++i) {
        if (enable) {
            QPopupMenu * menu = (*i)->playerMenu ();
            menu->clear ();
            menu->insertItem (i18n ("&MPlayer"), this, SLOT (setMPlayer (int)));
            menu->insertItem (i18n ("&Xine"), this, SLOT (setXine (int)));
            menu->setEnabled (true);
            if (m_settings->urlbackend == QString ("Xine")) {
                menu->setItemChecked (menu->idAt (1), true);
                setProcess (m_xine);
            } else {
                setProcess (m_mplayer);
                menu->setItemChecked (menu->idAt (0), true);
            }
            (*i)->popupMenu ()->setItemVisible (KMPlayerControlPanel::menu_player, true);
        } else {
            (*i)->popupMenu ()->setItemVisible (KMPlayerControlPanel::menu_player, false);
        }
    }
#endif
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
    m_process->setSource (source);
    if (!m_recorder->playing ())
        m_recorder->setSource (source);
    source->init ();
    if (source) QTimer::singleShot (0, source, SLOT (activate ()));
    emit sourceChanged (source);
}

KDE_NO_EXPORT void KMPlayer::addURL (const QString & _url) {
    QString url = KURL::fromPathOrURL (_url).url ();
    kdDebug () << "mrl added: " << url << endl;
    if (m_process->source ()->url ().url () != url && m_url != url)
        m_process->source ()->insertURL (url);
    emit urlAdded (url);
}

KDE_NO_EXPORT void KMPlayer::changeURL (const QString & url) {
    emit urlChanged (url);
}

KDE_NO_EXPORT void KMPlayer::changeTitle (const QString & title) {
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
    m_urlsource->setSubURL (KURL ());
    m_urlsource->setURL (url);
    m_urlsource->setIdentified (false);
    setSource (m_urlsource);
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
    std::for_each (m_panels.begin (), m_panels.end (),
            std::bind2nd (std::mem_fun (&KMPlayerControlPanel::setRecording), true));
    if (m_settings->replayoption == KMPlayerSettings::ReplayAfter)
        m_record_timer = startTimer (1000 * m_settings->replaytime);
    emit startRecording ();
}

KDE_NO_EXPORT void KMPlayer::recordingFinished () {
    if (!m_view) return;
    std::for_each (m_panels.begin (), m_panels.end (),
            std::bind2nd (std::mem_fun (&KMPlayerControlPanel::setRecording), false));
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
    std::for_each (m_panels.begin (), m_panels.end (),
            std::bind2nd (std::mem_fun (&KMPlayerControlPanel::setPlaying), false));
    m_view->reset ();
    emit stopPlaying ();
}

void KMPlayer::processStarted () {
    if (!m_view) return;
    std::for_each (m_panels.begin (), m_panels.end (),
            std::bind2nd (std::mem_fun (&KMPlayerControlPanel::setPlaying), true));
}

KDE_NO_EXPORT void KMPlayer::processPositioned (int pos) {
    if (m_view && !m_bPosSliderPressed)
        std::for_each (m_panels.begin (), m_panels.end (),
            std::bind2nd (std::mem_fun (&KMPlayerControlPanel::setPlayingProgress), pos));
}

void KMPlayer::processLoaded (int percentage) {
    if (m_view && !m_bPosSliderPressed)
        std::for_each (m_panels.begin (), m_panels.end (),
                std::bind2nd (std::mem_fun (&KMPlayerControlPanel::setLoadingProgress), percentage));
    emit loading (percentage);
}

void KMPlayer::processStartedPlaying () {
    if (!m_view) return;
    m_view->videoStart ();
    kdDebug () << "KMPlayer::processStartedPlaying " << endl;
    if (m_settings->sizeratio && m_view->viewer ())
        m_view->viewer ()->setAspect (m_process->source ()->aspect ());
    ControlPanelList::iterator e = m_panels.end();
    int len = m_process->source ()->length ();
    bool seek = m_process->source ()->isSeekable ();
    for (ControlPanelList::iterator i = m_panels.begin (); i != e; ++i) {
        (*i)->showPositionSlider (!!len);
        (*i)->setPlayingLength (len);
        (*i)->enableSeekButtons (seek);
    }
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

void KMPlayer::record () {
    if (m_view) m_view->setCursor (QCursor (Qt::WaitCursor));
    if (m_recorder->playing ()) {
        m_recorder->stop ();
    } else {
        m_process->stop ();
        m_settings->show  ("RecordPage");
        std::for_each (m_panels.begin (), m_panels.end (),
                std::bind2nd (std::mem_fun (&KMPlayerControlPanel::setRecording), false));
    }
    if (m_view) m_view->setCursor (QCursor (Qt::ArrowCursor));
}

void KMPlayer::play () {
    m_process->play ();
    std::for_each (m_panels.begin (), m_panels.end (),
            std::bind2nd (std::mem_fun (&KMPlayerControlPanel::setPlaying),
                m_process->playing ()));
}

bool KMPlayer::playing () const {
    return m_process && m_process->playing ();
}

void KMPlayer::stop () {
    ControlPanelList::iterator e = m_panels.end();
    for (ControlPanelList::iterator i = m_panels.begin (); i != e; ++i) {
        if (!(*i)->stopButton ()->isOn ())
            (*i)->stopButton ()->toggle ();
    }
    if (m_view) m_view->setCursor (QCursor (Qt::WaitCursor));
    m_process->source ()->first ();
    m_process->quit ();
    if (m_view) m_view->setCursor (QCursor (Qt::ArrowCursor));
    for (ControlPanelList::iterator i = m_panels.begin (); i != e; ++i) {
        if ((*i)->stopButton ()->isOn ())
            (*i)->stopButton ()->toggle ();
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

KAboutData* KMPlayer::createAboutData () {
    KMessageBox::error(0L, "createAboutData", "KMPlayer");
    return 0;
}

//-----------------------------------------------------------------------------

KMPlayerSource::KMPlayerSource (const QString & name, KMPlayer * player)
 : QObject (player), m_name (name), m_player (player),
   m_auto_play (true),
   m_currenturl (m_refurls.end ()) {
    kdDebug () << "KMPlayerSource::KMPlayerSource" << endl;
    init ();
}

KMPlayerSource::~KMPlayerSource () {
    kdDebug () << "KMPlayerSource::~KMPlayerSource" << endl;
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

void KMPlayerSource::setURL (const KURL & url) {
    m_url = url;
    if (m_refurls.size () == 1 && m_refurls.begin ()->url.isEmpty ())
        m_refurls.begin ()->url = url.url ();
    else {
        m_refurls.clear ();
        m_refurls.push_back (url.url ());
    }
    m_currenturl = m_refurls.begin ();
    m_nexturl = m_refurls.end ();
}

KDE_NO_EXPORT void KMPlayerSource::checkList () {
    URLInfoList::iterator tmp = m_currenturl;
    if (tmp != m_refurls.end () && m_nexturl != ++tmp) {
        kdDebug () << "Erasing " << m_currenturl->url << endl;
        tmp = m_currenturl = m_refurls.erase (m_currenturl);
        m_nexturl = tmp == m_refurls.end () ? tmp : ++tmp;
    }
}

QString KMPlayerSource::first () {
    checkList (); // update insertions
    m_nexturl = m_currenturl = m_refurls.begin ();
    if (m_currenturl == m_refurls.end ())
        return QString ();
    ++m_nexturl;
    return m_currenturl->url;
}

QString KMPlayerSource::current () {
    checkList (); // update insertions
    return m_currenturl == m_refurls.end () ? QString () : m_currenturl->url;
}

QString KMPlayerSource::next () {
    URLInfoList::iterator tmp = m_currenturl;
    if (tmp == m_refurls.end ())
        return QString ();
    checkList (); // update insertions
    if (m_currenturl == tmp)
        m_nexturl = m_currenturl = ++tmp;
    else
        tmp = m_nexturl = m_currenturl;
    if (m_currenturl == m_refurls.end ())
        return QString ();
    m_nexturl = ++tmp;
    return m_currenturl->url;
}

void KMPlayerSource::insertURL (const QString & url) {
    URLInfoList::iterator tmp = m_refurls.begin ();
    for (; tmp != m_refurls.end (); ++tmp)
        if (tmp->url == url)
            return;
    m_refurls.insert (m_nexturl, URLInfo (url));
}

void KMPlayerSource::play () {
    QTimer::singleShot (0, m_player, SLOT (play ()));
}

void KMPlayerSource::backward () {
    if (m_refurls.size () > 1) {
        if (m_currenturl != m_refurls.begin ())
            m_nexturl = m_currenturl--;
        m_player->process ()->stop ();
    } else
        m_player->process ()->seek (-1 * m_player->settings ()->seektime * 10, false);
}

void KMPlayerSource::forward () {
    if (m_refurls.size () > 1) {
        m_player->process ()->stop ();
    } else
        m_player->process ()->seek (m_player->settings()->seektime * 10, false);
}

QString KMPlayerSource::mime () const {
    return m_currenturl == m_refurls.end () ? QString () : m_currenturl->mime;
}

void KMPlayerSource::setMime (const QString & m) {
    if (m_currenturl != m_refurls.end ())
        m_currenturl->mime = m;
    else
        m_refurls.push_back (URLInfo (QString (), m));
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
    } else if (str.startsWith ("ID_LENGTH")) {
        int pos = str.find ('=');
        if (pos > 0)
            setLength (10 * str.mid (pos + 1).toInt());
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
    : KMPlayerSource (i18n ("URL"), player), m_job (0L) {
    setURL (url);
    kdDebug () << "KMPlayerURLSource::KMPlayerURLSource" << endl;
}

KDE_NO_CDTOR_EXPORT KMPlayerURLSource::~KMPlayerURLSource () {
    kdDebug () << "KMPlayerURLSource::~KMPlayerURLSource" << endl;
}

KDE_NO_EXPORT void KMPlayerURLSource::init () {
    KMPlayerSource::init ();
    m_player->enablePlayerMenu (true);
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
    m_player->enablePlayerMenu (false);
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
    KDE_NO_CDTOR_EXPORT MMXmlContentHandler (KMPlayerURLSource * source) : m_urlsource (source) {}
    KDE_NO_CDTOR_EXPORT ~MMXmlContentHandler () {}
    KDE_NO_EXPORT bool startElement (const QString &, const QString &,
                       const QString & elm, const QXmlAttributes & atts) {
        kdDebug() << "startElement=" << elm << endl;
        if (elm.lower () == QString ("entryref") ||
                elm.lower () == QString ("ref") ||
                elm.lower () == QString ("video"))
            for (int i = 0; i < atts.length (); i++)
                if (atts.qName (i).lower () == QString ("href") ||
                    atts.qName (i).lower () == QString ("src")) {
                    kdDebug() << atts.qName(i) << "=" << atts.value(i) << endl;
                    m_urlsource->insertURL (atts.value (i));
                }
        return true;
    }
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

KDE_NO_EXPORT void KMPlayerURLSource::read (QTextStream & textstream) {
    QString line;
    do {
        line = textstream.readLine ();
    } while (!line.isNull () && line.stripWhiteSpace ().isEmpty ());
    if (!line.isNull ()) {
        if (line.stripWhiteSpace ().startsWith (QChar ('<'))) {
            QXmlSimpleReader reader;
            MMXmlContentHandler content_handler (this);
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
        } while (!line.isNull () && m_refurls.size () < 1024 /* support 1k entries */);
    }
;
}

KDE_NO_EXPORT void KMPlayerURLSource::kioData (KIO::Job *, const QByteArray & d) {
    int size = m_data.size ();
    int newsize = size + d.size ();
    if (newsize > 50000) {
        kdDebug () << "KMPlayerURLSource::kioData: " << newsize << endl;
        m_job->kill();
        m_job = 0L; // KIO::Job::kill deletes itself
    } else  {
        m_data.resize (newsize);
        memcpy (m_data.data () + size, d.data (), newsize - size);
    }
}

KDE_NO_EXPORT void KMPlayerURLSource::kioMimetype (KIO::Job *, const QString & mimestr) {
    kdDebug () << "KMPlayerURLSource::kioMimetype " << mimestr << endl;
    setMime (mimestr);
}

KDE_NO_EXPORT void KMPlayerURLSource::kioResult (KIO::Job *) {
    m_job = 0L; // KIO::Job::kill deletes itself
    QTextStream textstream (m_data, IO_ReadOnly);
    read (textstream);
    if (m_currenturl != m_refurls.end ()) {
        m_currenturl->dereferenced = true;
        checkList ();
        QTimer::singleShot (0, this, SLOT (play ()));
    }
}

KDE_NO_EXPORT void KMPlayerURLSource::play () {
    KURL url (current ());
    if (url.isEmpty ())
        return;
    QString mimestr = mime ();
    bool maybe_playlist = (url.url ().lower ().endsWith (QString ("m3u")) ||
            url.url ().lower ().endsWith (QString ("asx")) ||
            mimestr == QString ("audio/mpegurl") ||
            mimestr == QString ("audio/x-mpegurl") ||
            mimestr == QString ("video/x-ms-wmp") ||
            mimestr == QString ("video/x-ms-asf") ||
            mimestr == QString ("application/smil") ||
            mimestr == QString ("application/x-mplayer2"));
    if (!m_currenturl->dereferenced && url.isLocalFile ()) {
        QFile file (url.path ());
        if (!file.exists ())
            return;
        if (url.url ().lower ().endsWith (QString ("pls")) ||
                mimestr == QString ("audio/x-scpls")) {
            KSimpleConfig kc (url.path (), true);
            kc.setGroup ("playlist");
            int nr = kc.readNumEntry ("numberofentries", 0);
            if (nr == 0)
                nr = kc.readNumEntry ("NumberOfEntries", 0);
            for (int i = 0; i < nr; i++) {
                QString mrl = kc.readEntry (QString ("File%1").arg(i+1), "");
                if (!mrl.isEmpty ())
                    insertURL (mrl);
            }
        } else if (maybe_playlist && file.size () < 50000 && file.open (IO_ReadOnly)) {
            QTextStream textstream (&file);
            read (textstream);
        }
        m_currenturl->dereferenced = true;
    } else if (!m_currenturl->dereferenced && maybe_playlist) {
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
        return;
    }
    KMPlayerSource::play ();
}

#include "kmplayerpartbase.moc"
#include "kmplayersource.moc"
