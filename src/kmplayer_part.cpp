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
 *  the Free Software Foundation, Inc., 51 Franklin Steet, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 **/

#ifdef KDE_USE_FINAL
#undef Always
#endif
#include <list>
#include <algorithm>

#include <config.h>
#include <qcstring.h>
#include <qpopupmenu.h>
#include <qtimer.h>
#include <qpushbutton.h>
#include <qslider.h>

class KXMLGUIClient; // workaround for kde3.3 on sarge with gcc4, kactioncollection.h does not forward declare KXMLGUIClient
#include <klibloader.h>
#include <kdebug.h>
#include <kconfig.h>
#include <ksimpleconfig.h>
#include <kaction.h>
#include <kapplication.h>
#include <klocale.h>
#include <kinstance.h>
#include <kparts/factory.h>
#include <kstaticdeleter.h>
#include <kstatusbar.h>

#include "kmplayer_part.h"
#include "kmplayerview.h"
#include "kmplayercontrolpanel.h"
#include "kmplayerconfig.h"
#include "kmplayerprocess.h"

using namespace KMPlayer;

typedef std::list <KMPlayerPart *> KMPlayerPartList;

struct KMPlayerPartStatic {
    KDE_NO_CDTOR_EXPORT KMPlayerPartStatic () {}
    ~KMPlayerPartStatic ();
    KMPlayerPartList partlist;
};

static KMPlayerPartStatic * kmplayerpart_static = 0L;

KDE_NO_CDTOR_EXPORT KMPlayerPartStatic::~KMPlayerPartStatic () {
    kmplayerpart_static = 0L;
    // delete map content
}

struct GroupPredicate {
    const KMPlayerPart * m_part;
    const QString & m_group;
    bool m_get_any;
    GroupPredicate(const KMPlayerPart *part, const QString &group, bool b=false)
        : m_part (part), m_group (group), m_get_any (b) {}
    bool operator () (const KMPlayerPart * part) const {
        return ((m_get_any && part != m_part &&
                    !part->master () && !part->url ().isEmpty ()) ||
                (m_part->allowRedir (part->m_docbase) &&
                 (part->m_group == m_group ||
                  part->m_group == QString::fromLatin1("_master") ||
                  m_group == QString::fromLatin1("_master")) &&
                 (part->m_features & KMPlayerPart::Feat_Viewer) !=
                 (m_part->m_features & KMPlayerPart::Feat_Viewer)));
    }
};

static KStaticDeleter <KMPlayerPartStatic> kmplayerpart_staticdeleter;

//-----------------------------------------------------------------------------

class KMPlayerFactory : public KParts::Factory {
public:
    KMPlayerFactory ();
    virtual ~KMPlayerFactory ();
    virtual KParts::Part *createPartObject
        (QWidget *wparent, const char *wname,
         QObject *parent, const char *name,
         const char *className, const QStringList &args);
    static KInstance * instance () { return s_instance; }
private:
    static KInstance * s_instance;
};

K_EXPORT_COMPONENT_FACTORY (libkmplayerpart, KMPlayerFactory)

KInstance *KMPlayerFactory::s_instance = 0;

KDE_NO_CDTOR_EXPORT KMPlayerFactory::KMPlayerFactory () {
    s_instance = new KInstance ("kmplayer");
}

KDE_NO_CDTOR_EXPORT KMPlayerFactory::~KMPlayerFactory () {
    delete s_instance;
}

KDE_NO_EXPORT KParts::Part *KMPlayerFactory::createPartObject
  (QWidget *wparent, const char *wname,
   QObject *parent, const char * name,
   const char * cls, const QStringList & args) {
      kdDebug() << "KMPlayerFactory::createPartObject " << cls << endl;
      return new KMPlayerPart (wparent, wname, parent, name, args);
}

//-----------------------------------------------------------------------------

static bool getBoolValue (const QString & value) {
    return (value.lower() != QString::fromLatin1("false") &&
            value.lower() != QString::fromLatin1("off") &&
            value.lower() != QString::fromLatin1("0"));
}

KDE_NO_CDTOR_EXPORT KMPlayerPart::KMPlayerPart (QWidget * wparent, const char *wname,
                    QObject * parent, const char *name, const QStringList &args)
 : PartBase (wparent, wname, parent, name, new KSimpleConfig ("kmplayerrc")),
   m_master (0L),
   m_browserextension (new KMPlayerBrowserExtension (this)),
   m_liveconnectextension (new KMPlayerLiveConnectExtension (this)),
   m_features (Feat_Unknown),
   m_started_emited (false),
   m_havehref (false) {
    kdDebug () << "KMPlayerPart(" << this << ")::KMPlayerPart ()" << endl;
    bool show_fullscreen = false;
    if (!kmplayerpart_static)
        kmplayerpart_static = kmplayerpart_staticdeleter.setObject (new KMPlayerPartStatic ());
    setInstance (KMPlayerFactory::instance (), true);
    init ();
    m_sources ["hrefsource"] = (new KMPlayerHRefSource (this));
    /*KAction *playact =*/ new KAction(i18n("P&lay"), QString ("player_play"), KShortcut (), this, SLOT(play ()), actionCollection (), "play");
    /*KAction *pauseact =*/ new KAction(i18n("&Pause"), QString ("player_pause"), KShortcut (), this, SLOT(pause ()), actionCollection (), "pause");
    /*KAction *stopact =*/ new KAction(i18n("&Stop"), QString ("player_stop"), KShortcut (), this, SLOT(stop ()), actionCollection (), "stop");
    new KAction (i18n ("Increase Volume"), QString ("player_volume"), KShortcut (), this, SLOT (increaseVolume ()), actionCollection (), "edit_volume_up");
    new KAction (i18n ("Decrease Volume"), QString ("player_volume"), KShortcut (), this, SLOT (decreaseVolume ()), actionCollection (), "edit_volume_down");
    Source * urlsource = m_sources ["urlsource"];
    KMPlayer::ControlPanel * panel = m_view->controlPanel ();
    QStringList::const_iterator it = args.begin ();
    QStringList::const_iterator end = args.end ();
    for ( ; it != end; ++it) {
        int equalPos = (*it).find("=");
        if (equalPos > 0) {
            QString name = (*it).left (equalPos).lower ();
            QString value = (*it).right ((*it).length () - equalPos - 1);
            if (value.at(0)=='\"')
                value = value.right (value.length () - 1);
            if (value.at (value.length () - 1) == '\"')
                value.truncate (value.length () - 1);
            kdDebug () << "name=" << name << " value=" << value << endl;
            if (name == "href") {
                urlsource->setURL (KURL (value));
                urlsource->setIdentified (false);
                m_havehref = true;
            } else if (name == QString::fromLatin1("width")) {
                m_noresize = true;
            } else if (name == QString::fromLatin1("height")) {
                m_noresize = true;
            } else if (name == QString::fromLatin1("type")) {
                urlsource->setMime (value);
            } else if (name == QString::fromLatin1("controls")) {
                //http://service.real.com/help/library/guides/production8/realpgd.htm?src=noref,rnhmpg_080301,rnhmtn,nosrc
                QString val_lower (value.lower ());
                if (val_lower == QString::fromLatin1("imagewindow")) {
                    m_features |= Feat_Viewer;
                } else if (val_lower == QString::fromLatin1("all")) {
                    m_features = Feat_All;
                } else if (val_lower == QString::fromLatin1("tacctrl")) {
                    m_features |= Feat_Label;
                } else if (val_lower == QString::fromLatin1("controlpanel")) {
                    m_features |= Feat_Controls;
                } else if (val_lower == QString::fromLatin1("infovolumepanel")){
                    m_features |= Feat_Controls; // TODO
                } else if (val_lower == QString::fromLatin1("positionfield") ||
                        val_lower == QString::fromLatin1("positionslider")) {
                    setAutoControls (false);
                    panel->positionSlider ()->show ();
                    m_features |= Feat_Controls;
                } else if ( val_lower == QString::fromLatin1("homectrl")) {
                    setAutoControls (false);
                    panel->button (KMPlayer::ControlPanel::button_config)->show();
                } else if (val_lower == QString::fromLatin1("mutectrl") ||
                        val_lower == QString::fromLatin1("mutevolume")) {
                    setAutoControls (false);
                    panel->volumeBar()->setMinimumSize (QSize (20, panel->volumeBar()->minimumSize ().height ()));
                    panel->volumeBar()->show ();
                    m_features |= Feat_Controls;
                } else if (val_lower == QString::fromLatin1("rwctrl")) {
                    setAutoControls (false);
                    panel->button (KMPlayer::ControlPanel::button_back)->show (); // rewind ?
                    m_features |= Feat_Controls;
                } else if ( val_lower == QString::fromLatin1("ffctrl")) {
                    setAutoControls (false);
                    panel->button(KMPlayer::ControlPanel::button_forward)->show();
                    m_features = Feat_Controls;
                } else if ( val_lower == QString::fromLatin1("stopbutton")) {
                    setAutoControls (false);
                    panel->button (KMPlayer::ControlPanel::button_stop)->show ();
                    m_features |= Feat_Controls;
                } else if (val_lower == QString::fromLatin1("playbutton") ||
                        val_lower == QString::fromLatin1("playonlybutton")) {
                    setAutoControls (false);
                    panel->button (KMPlayer::ControlPanel::button_play)->show ();
                    m_features |= Feat_Controls;
                } else if (val_lower == QString::fromLatin1("pausebutton")) {
                    setAutoControls (false);
                    panel->button (KMPlayer::ControlPanel::button_pause)->show ();
                    m_features |= Feat_Controls;
                } else if (val_lower == QString::fromLatin1("statusbar") ||
                        val_lower == QString::fromLatin1("statusfield")) {
                    m_features |= Feat_StatusBar;
                } else if (val_lower == QString::fromLatin1("infopanel")) {
                    m_features |= Feat_InfoPanel;
                } else if (val_lower == QString::fromLatin1("playlist")) {
                    m_features |= Feat_PlayList;
                } else if (val_lower == QString::fromLatin1("volumeslider")) {
                    m_features |= Feat_VolumeSlider;
                    setAutoControls (false);
                    panel->volumeBar()->show ();
                    panel->volumeBar()->setMinimumSize (QSize (20, panel->volumeBar()->minimumSize ().height ()));
                }
            } else if (name == QString::fromLatin1("nolabels")) {
                m_features &= ~Feat_Label;
            } else if (name == QString::fromLatin1("nocontrols")) {
                m_features &= ~Feat_Controls;
            } else if (name == QString::fromLatin1("console")) {
                m_group = value.isEmpty() ? QString::fromLatin1("_anonymous") : value;
            } else if (name == QString::fromLatin1("__khtml__pluginbaseurl")) {
                m_docbase = KURL (value);
            } else if (name == QString::fromLatin1("src")) {
                m_src_url = value;
            } else if (name == QString::fromLatin1("filename")) {
                m_file_name = value;
            } else if (name == QString::fromLatin1 ("fullscreenmode")) {
                show_fullscreen = getBoolValue (value);
            } else if (name == QString::fromLatin1 ("autostart")) {
                urlsource->setAutoPlay (getBoolValue (value));
	    }
            if (!name.startsWith (QString::fromLatin1 ("__khtml__")))
                convertNode <KMPlayer::Element> (urlsource->document ())->setAttribute (name, value);
        }
    }
    //KParts::Part::setWidget (m_view);
    setXMLFile("kmplayerpartui.rc");
    panel->zoomMenu ()->connectItem (KMPlayer::ControlPanel::menu_zoom50,
                                      this, SLOT (setMenuZoom (int)));
    panel->zoomMenu ()->connectItem (KMPlayer::ControlPanel::menu_zoom100,
                                      this, SLOT (setMenuZoom (int)));
    panel->zoomMenu ()->connectItem (KMPlayer::ControlPanel::menu_zoom150,
                                      this, SLOT (setMenuZoom (int)));

    m_view->setNoInfoMessages (m_features != Feat_InfoPanel);
    if (m_features == Feat_InfoPanel)
        m_view->setInfoPanelOnly ();
    else if (m_features == Feat_PlayList)
        m_view->setPlaylistOnly ();
    else {
        if (m_features & Feat_StatusBar)
            m_view->setStatusBarMode (KMPlayer::View::SB_Show);
        if (m_features & (Feat_Controls | Feat_VolumeSlider))
            m_view->setControlPanelMode (m_features & Feat_Viewer ? KMPlayer::View::CP_Show : KMPlayer::View::CP_Only);
        else if (m_features != Feat_Unknown)
            m_view->setControlPanelMode (KMPlayer::View::CP_Hide);
        else
            m_view->setControlPanelMode (KMPlayer::View::CP_AutoHide);
    }
    bool group_member = !m_group.isEmpty () && m_group != QString::fromLatin1("_unique") && m_features != Feat_Unknown;
    if (!group_member || m_features & Feat_Viewer) {
        // not part of a group or we're the viewer
        setProcess ("mplayer");
        setRecorder ("mencoder");
        connectPanel (m_view->controlPanel ());
    }
    if (group_member) {
        KMPlayerPartList::iterator i =kmplayerpart_static->partlist.begin ();
        KMPlayerPartList::iterator e =kmplayerpart_static->partlist.end ();
        GroupPredicate pred (this, m_group);
        for (i = std::find_if (i, e, pred);
                i != e;
                i = std::find_if (++i, e, pred)) {
            // found viewer and control part, exchange players now
            KMPlayerPart * vp = (m_features & Feat_Viewer) ? this : *i;
            KMPlayerPart * cp = (m_features & Feat_Viewer) ? *i : this;
            setProcess ("mplayer");
            cp->connectToPart (vp);
        }
    } else
        m_group.truncate (0);
    kmplayerpart_static->partlist.push_back (this);
    if (m_view->isFullScreen () != show_fullscreen)
        m_view->fullScreen ();
}

KDE_NO_CDTOR_EXPORT KMPlayerPart::~KMPlayerPart () {
    kdDebug() << "KMPlayerPart::~KMPlayerPart" << endl;
    //if (!m_group.isEmpty ()) {
        KMPlayerPartList::iterator i = std::find (kmplayerpart_static->partlist.begin (), kmplayerpart_static->partlist.end (), this);
        if (i != kmplayerpart_static->partlist.end ())
            kmplayerpart_static->partlist.erase (i);
        else
            kdError () << "KMPlayerPart::~KMPlayerPart group lost" << endl;
    //}
    delete m_config;
    m_config = 0L;
}

KDE_NO_EXPORT bool KMPlayerPart::allowRedir (const KURL & url) const {
    return kapp->authorizeURLAction ("redirect", m_docbase, url);
}

KDE_NO_EXPORT void KMPlayerPart::setAutoControls (bool b) {
    m_auto_controls = b;
    m_view->controlPanel ()->setAutoControls (b);
}

KDE_NO_EXPORT void KMPlayerPart::viewerPartDestroyed (QObject * o) {
    if (o == m_master)
        m_master = 0L;
    kdDebug () << "KMPlayerPart(" << this << ")::viewerPartDestroyed" << endl;
    const KMPlayerPartList::iterator e =kmplayerpart_static->partlist.end();
    KMPlayerPartList::iterator i = std::find_if (kmplayerpart_static->partlist.begin (), e, GroupPredicate (this, m_group));
    if (i != e && *i != this)
        (*i)->updatePlayerMenu (m_view->controlPanel ());
}

KDE_NO_EXPORT void KMPlayerPart::viewerPartProcessChanged (const char *) {
    const KMPlayerPartList::iterator e =kmplayerpart_static->partlist.end();
    KMPlayerPartList::iterator i = std::find_if (kmplayerpart_static->partlist.begin (), e, GroupPredicate (this, m_group));
    if (i != e && *i != this)
        (*i)->updatePlayerMenu (m_view->controlPanel ());
}

KDE_NO_EXPORT void KMPlayerPart::viewerPartSourceChanged(Source *o, Source *s) {
    kdDebug () << "KMPlayerPart::source changed " << m_master << endl;
    if (m_master && m_view) {
        connectSource (o, s);
        m_master->updatePlayerMenu (m_view->controlPanel ());
    }
}

KDE_NO_EXPORT bool KMPlayerPart::openURL (const KURL & _url) {
    kdDebug () << "KMPlayerPart::openURL " << _url.url() << endl;
    Source * urlsource = m_sources ["urlsource"];
    KMPlayerHRefSource * hrefsource = static_cast <KMPlayerHRefSource *>(m_sources ["hrefsource"]);
    KMPlayerPartList::iterator i =kmplayerpart_static->partlist.begin ();
    KMPlayerPartList::iterator e =kmplayerpart_static->partlist.end ();
    GroupPredicate pred (this, m_group);
    KURL url;
    if (_url != m_docbase) {
        url = _url;
        if (!m_file_name.isEmpty () && _url.url ().find (m_file_name) < 0) {
            KURL u (m_file_name);
            if ((u.protocol () == QString ("mms")) ||
                    _url.protocol ().isEmpty ()) {
                // see if we somehow have to merge these
                int p = _url.port ();
                if (p > 0)
                    u.setPort (p);
                u.setPath (u.path (1) + _url.host () + _url.path ());
                if (allowRedir (u)) {
                    url = u;
                    kdDebug () << "KMPlayerPart::openURL compose " << m_file_name << " " << _url.url() << " ->" << u.url() << endl;
                }
            }
        }
    } else { // if url is the container document, then it's an empty URL
        if (m_features & Feat_Viewer) // damn, look in the group
            for (i = std::find_if (i, e, pred);
                    i != e;
                    i = std::find_if (++i, e, pred))
                if (!(*i)->url ().isEmpty ()) {
                    url = (*i)->url ();
                    break;
                }
    }
    if (m_havehref && (!kapp->authorizeURLAction ("redirect", url, urlsource->url ()) || !m_settings->allowhref)) {
        m_havehref = false;
        url = urlsource->url ();
    }
    if (!m_havehref)
        setURL (url);
    if (url.isEmpty ()) {
        if (!m_master && !(m_features & Feat_Viewer))
            // no master set, wait for a viewer to attach or timeout
            QTimer::singleShot (50, this, SLOT (waitForImageWindowTimeOut ()));
        return true;
    }
    if (!m_group.isEmpty () && !(m_features & Feat_Viewer)) {
        // group member, not the image window
        for (i = std::find_if (i, e, pred);
                i != e;
                i = std::find_if (++i, e, pred))
            if ((*i)->url ().isEmpty ()) // image window created w/o url
                return (*i)->openURL (_url);
        kdError () << "Not the ImageWindow and no ImageWindow found" << endl;
        return true;
    }
    if (!m_view || !url.isValid ()) return false;
    KParts::URLArgs args = m_browserextension->urlArgs();
    if (!args.serviceType.isEmpty ())
        urlsource->setMime (args.serviceType);
    if (m_havehref && m_settings->allowhref) {
        hrefsource->setURL (url);
        setSource (hrefsource);
    } else {
        hrefsource->clear ();
        PartBase::openURL (m_havehref ? m_sources ["urlsource"]->url () : url);
        emit started (0L);
        m_started_emited = true;
        m_havehref = false;
    }
    return true;
}

KDE_NO_EXPORT void KMPlayerPart::waitForImageWindowTimeOut () {
    if (!m_master) {
        // still no ImageWindow attached, eg. audio only
        const KMPlayerPartList::iterator e =kmplayerpart_static->partlist.end();
        KMPlayerPartList::iterator i = std::find_if (kmplayerpart_static->partlist.begin (), e, GroupPredicate (this, m_group));
        bool noattach = (i == e || *i == this);
        if (noattach) {
            if (!url ().isEmpty ())
                PartBase::openURL (url ());
            else { // see if we can attach to something out there ..
                i = std::find_if (kmplayerpart_static->partlist.begin (), e, GroupPredicate (this, m_group, true));
                noattach = (i == e);
            }
        }
        if (!noattach)
            connectToPart (*i);
    }
}

KDE_NO_EXPORT bool KMPlayerPart::closeURL () {
    if (!m_group.isEmpty ()) {
        kmplayerpart_static->partlist.remove (this);
        m_group.truncate (0);
    }
    return PartBase::closeURL ();
}

KDE_NO_EXPORT void KMPlayerPart::connectToPart (KMPlayerPart * m) {
    m_master = m;
    m->connectPanel (m_view->controlPanel ());
    m->updatePlayerMenu (m_view->controlPanel ());
    if (m_features & Feat_PlayList)
        m->connectPlaylist (m_view->playList ());
    if (m_features & Feat_InfoPanel)
        m->connectInfoPanel (m_view->infoPanel ());
    connectSource (m_source, m->source ());
    connect (m, SIGNAL (destroyed (QObject *)),
            this, SLOT (viewerPartDestroyed (QObject *)));
    connect (m, SIGNAL (processChanged (const char *)),
            this, SLOT (viewerPartProcessChanged (const char *)));
    connect (m, SIGNAL (sourceChanged (KMPlayer::Source *, KMPlayer::Source *)),
            this, SLOT (viewerPartSourceChanged (KMPlayer::Source *, KMPlayer::Source *)));
    if (m_features & Feat_StatusBar) {
        last_time_left = 0;
        connect (m, SIGNAL (positioned (int, int)),
                 this, SLOT (statusPosition (int, int)));
        m_view->statusBar ()->insertItem (QString ("--:--"), 1, 0, true);
    }
}

KDE_NO_EXPORT void KMPlayerPart::setLoaded (int percentage) {
    PartBase::setLoaded (percentage);
    if (percentage < 100) {
        m_browserextension->setLoadingProgress (percentage);
        m_browserextension->infoMessage
            (QString::number (percentage) + i18n ("% Cache fill"));
    }
}

KDE_NO_EXPORT void KMPlayerPart::playingStarted () {
    const KMPlayerPartList::iterator e =kmplayerpart_static->partlist.end();
    KMPlayerPartList::iterator i = std::find_if (kmplayerpart_static->partlist.begin (), e, GroupPredicate (this, m_group));
    if (i != e && *i != this && m_view && (*i)->source()) {
        m_view->controlPanel ()->setPlaying (true);
        m_view->controlPanel ()->showPositionSlider(!!(*i)->source()->length());
        m_view->controlPanel()->enableSeekButtons((*i)->source()->isSeekable());
        emit loading (100);
    } else if (m_source)
        KMPlayer::PartBase::playingStarted ();
    else
        return; // ugh
    kdDebug () << "KMPlayerPart::processStartedPlaying " << endl;
    if (m_settings->sizeratio && !m_noresize && m_source->width() > 0 && m_source->height() > 0)
        m_liveconnectextension->setSize (m_source->width(), m_source->height());
    m_browserextension->setLoadingProgress (100);
    emit completed ();
    m_started_emited = false;
    m_liveconnectextension->started ();
    m_browserextension->infoMessage (i18n("KMPlayer: Playing"));
}

KDE_NO_EXPORT void KMPlayerPart::playingStopped () {
    KMPlayer::PartBase::playingStopped ();
    if (m_started_emited) {
        m_started_emited = false;
        m_browserextension->setLoadingProgress (100);
        emit completed ();
    }
    m_liveconnectextension->finished ();
    if (m_havehref)
        static_cast <KMPlayerHRefSource *>(m_sources["hrefsource"])->finished();
    m_browserextension->infoMessage (i18n ("KMPlayer: Stop Playing"));
    if (m_view)
        m_view->controlPanel ()->setPlaying (false);
}

KDE_NO_EXPORT void KMPlayerPart::setMenuZoom (int id) {
    int w = 0, h = 0;
    if (m_source)
        m_source->dimensions (w, h);
    if (id == KMPlayer::ControlPanel::menu_zoom100) {
        m_liveconnectextension->setSize (w, h);
        return;
    }
    float scale = 1.5;
    if (id == KMPlayer::ControlPanel::menu_zoom50)
        scale = 0.5;
    if (m_view->viewer ())
        m_liveconnectextension->setSize (int (scale * m_view->viewer ()->width ()),
                                         int (scale * m_view->viewer ()->height()));
}

KDE_NO_EXPORT void KMPlayerPart::statusPosition (int pos, int length) {
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
        m_view->statusBar ()->changeItem (text, 1);
    }
}

//---------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT KMPlayerBrowserExtension::KMPlayerBrowserExtension (KMPlayerPart * parent)
  : KParts::BrowserExtension (parent, "KMPlayer Browser Extension") {
}

KDE_NO_EXPORT void KMPlayerBrowserExtension::urlChanged (const QString & url) {
    emit setLocationBarURL (url);
}

KDE_NO_EXPORT void KMPlayerBrowserExtension::setLoadingProgress (int percentage) {
    emit loadingProgress (percentage);
}

KDE_NO_EXPORT void KMPlayerBrowserExtension::saveState (QDataStream & stream) {
    stream << static_cast <PartBase *> (parent ())->url ().url ();
}

KDE_NO_EXPORT void KMPlayerBrowserExtension::restoreState (QDataStream & stream) {
    QString url;
    stream >> url;
    static_cast <PartBase *> (parent ())->openURL (KURL(url));
}
//---------------------------------------------------------------------

enum JSCommand {
    notsupported,
    canpause, canplay, canstop, canseek,
    isfullscreen, isloop, isaspect, showcontrolpanel,
    length, width, height, playstate, position, source, setsource, protocol,
    gotourl, nextentry, jsc_pause, play, preventry, start, stop,
    volume, setvolume,
    prop_source, prop_volume
};

struct JSCommandEntry {
    const char * name;
    JSCommand command;
    const char * defaultvalue;
    const KParts::LiveConnectExtension::Type rettype;
};

// keep this list in alphabetic order
// http://service.real.com/help/library/guides/realonescripting/browse/htmfiles/embedmet.htm
static const JSCommandEntry JSCommandList [] = {
    { "CanPause", canpause, 0L, KParts::LiveConnectExtension::TypeBool },
    { "CanPlay", canplay, 0L, KParts::LiveConnectExtension::TypeBool },
    { "CanStop", canstop, 0L, KParts::LiveConnectExtension::TypeBool },
    { "DoGotoURL", notsupported, 0L, KParts::LiveConnectExtension::TypeVoid },
    { "DoNextEntry", notsupported, "false", KParts::LiveConnectExtension::TypeBool },
    { "DoPause", jsc_pause, "true", KParts::LiveConnectExtension::TypeBool },
    { "DoPlay", play, 0L, KParts::LiveConnectExtension::TypeBool },
    { "DoPlayPause", play, 0L, KParts::LiveConnectExtension::TypeBool },
    { "DoPrevEntry", notsupported, "false", KParts::LiveConnectExtension::TypeBool },
    { "DoStop", stop, 0L, KParts::LiveConnectExtension::TypeBool },
    { "FileName", prop_source, 0L, KParts::LiveConnectExtension::TypeString },
    { "GetAuthor", notsupported, "noname", KParts::LiveConnectExtension::TypeString },
    { "GetAutoGoToURL", notsupported, "true", KParts::LiveConnectExtension::TypeBool },
    { "GetBackgroundColor", notsupported, "#ffffff", KParts::LiveConnectExtension::TypeString },
    { "GetBandwidthAverage", notsupported, "64", KParts::LiveConnectExtension::TypeNumber },
    { "GetBandwidthCurrent", notsupported, "64", KParts::LiveConnectExtension::TypeNumber },
    { "GetBufferingTimeElapsed", notsupported, "0", KParts::LiveConnectExtension::TypeNumber },
    { "GetBufferingTimeRemaining", notsupported, "0", KParts::LiveConnectExtension::TypeNumber },
    { "GetCanSeek", canseek, 0L, KParts::LiveConnectExtension::TypeBool },
    { "GetCenter", notsupported, "true", KParts::LiveConnectExtension::TypeBool },
    { "GetClipHeight", height, 0L, KParts::LiveConnectExtension::TypeNumber },
    { "GetClipWidth", width, 0L, KParts::LiveConnectExtension::TypeNumber },
    { "GetConnectionBandwidth", notsupported, "64", KParts::LiveConnectExtension::TypeNumber },
    { "GetConsole", notsupported, "unknown", KParts::LiveConnectExtension::TypeString },
    { "GetConsoleEvents", notsupported, "false", KParts::LiveConnectExtension::TypeBool },
    { "GetControls", notsupported, "buttons", KParts::LiveConnectExtension::TypeString },
    { "GetCopyright", notsupported, "(c) whoever", KParts::LiveConnectExtension::TypeString },
    { "GetCurrentEntry", notsupported, "1", KParts::LiveConnectExtension::TypeNumber },
    { "GetDRMInfo", notsupported, "RNBA", KParts::LiveConnectExtension::TypeString },
    { "GetDoubleSize", notsupported, "false", KParts::LiveConnectExtension::TypeBool },
    { "GetEntryAbstract", notsupported, "abstract", KParts::LiveConnectExtension::TypeString },
    { "GetEntryAuthor", notsupported, "noname", KParts::LiveConnectExtension::TypeString },
    { "GetEntryCopyright", notsupported, "(c)", KParts::LiveConnectExtension::TypeString },
    { "GetEntryTitle", notsupported, "title", KParts::LiveConnectExtension::TypeString },
    { "GetFullScreen", isfullscreen, 0L, KParts::LiveConnectExtension::TypeBool },
    { "GetImageStatus", notsupported, "false", KParts::LiveConnectExtension::TypeBool },
    { "GetLastErrorMoreInfoURL", notsupported, "no error", KParts::LiveConnectExtension::TypeString },
    { "GetLastErrorRMACode", notsupported, "0", KParts::LiveConnectExtension::TypeNumber },
    { "GetLastErrorSeverity", notsupported, "6", KParts::LiveConnectExtension::TypeNumber },
    { "GetLastErrorUserCode", notsupported, "0", KParts::LiveConnectExtension::TypeNumber },
    { "GetLastErrorUserString", notsupported, "no error", KParts::LiveConnectExtension::TypeString },
    { "GetLastMessage", notsupported, "no error", KParts::LiveConnectExtension::TypeString },
    { "GetLastStatus", notsupported, "no error", KParts::LiveConnectExtension::TypeString },
    { "GetLength", length, 0L, KParts::LiveConnectExtension::TypeNumber },
    { "GetLiveState", notsupported, "false", KParts::LiveConnectExtension::TypeBool },
    { "GetLoop", isloop, 0L, KParts::LiveConnectExtension::TypeBool },
    { "GetMaintainAspect", isaspect, 0L, KParts::LiveConnectExtension::TypeBool },
    { "GetMute", notsupported, "false", KParts::LiveConnectExtension::TypeBool },
    { "GetNumEntries", notsupported, "1", KParts::LiveConnectExtension::TypeNumber },
    { "GetNumLoop", notsupported, "0", KParts::LiveConnectExtension::TypeNumber },
    { "GetNumSources", notsupported, "1", KParts::LiveConnectExtension::TypeNumber },
    { "GetOriginalSize", notsupported, "true", KParts::LiveConnectExtension::TypeBool },
    { "GetPacketsEarly", notsupported, "0", KParts::LiveConnectExtension::TypeNumber },
    { "GetPacketsLate", notsupported, "0", KParts::LiveConnectExtension::TypeNumber },
    { "GetPacketsMissing", notsupported, "0", KParts::LiveConnectExtension::TypeNumber },
    { "GetPacketsOutOfOrder", notsupported, "0", KParts::LiveConnectExtension::TypeNumber },
    { "GetPacketsReceived", notsupported, "0", KParts::LiveConnectExtension::TypeNumber },
    { "GetPacketsTotal", notsupported, "0", KParts::LiveConnectExtension::TypeNumber },
    { "GetPlayState", playstate, 0L, KParts::LiveConnectExtension::TypeNumber },
    { "GetPosition", position, 0L, KParts::LiveConnectExtension::TypeNumber },
    { "GetPreFetch", notsupported, "false", KParts::LiveConnectExtension::TypeBool },
    { "GetShowAbout", notsupported, "false", KParts::LiveConnectExtension::TypeBool },
    { "GetShowPreferences", notsupported, "false", KParts::LiveConnectExtension::TypeBool },
    { "GetShowStatistics", notsupported, "false", KParts::LiveConnectExtension::TypeBool },
    { "GetShuffle", notsupported, "false", KParts::LiveConnectExtension::TypeBool },
    { "GetSource", source, 0L, KParts::LiveConnectExtension::TypeString },
    { "GetSourceTransport", protocol, 0L, KParts::LiveConnectExtension::TypeString },
    { "GetStereoState", notsupported, "true", KParts::LiveConnectExtension::TypeBool },
    { "GetTitle", notsupported, "title", KParts::LiveConnectExtension::TypeString },
    { "GetVersionInfo", notsupported, "version", KParts::LiveConnectExtension::TypeString },
    { "GetVolume", volume, "100", KParts::LiveConnectExtension::TypeNumber },
    { "GetWantErrors", notsupported, "false", KParts::LiveConnectExtension::TypeBool },
    { "GetWantKeyboardEvents", notsupported, "false", KParts::LiveConnectExtension::TypeBool },
    { "GetWantMouseEvents", notsupported, "false", KParts::LiveConnectExtension::TypeBool },
    { "HasNextEntry", notsupported, "false", KParts::LiveConnectExtension::TypeBool },
    { "Pause", jsc_pause, 0L, KParts::LiveConnectExtension::TypeBool },
    { "Play", play, 0L, KParts::LiveConnectExtension::TypeBool },
    { "SetAuthor", notsupported, "true", KParts::LiveConnectExtension::TypeBool },
    { "SetAutoGoToURL", notsupported, "true", KParts::LiveConnectExtension::TypeBool },
    { "SetAutoStart", notsupported, "true", KParts::LiveConnectExtension::TypeBool },
    { "SetBackgroundColor", notsupported, "true", KParts::LiveConnectExtension::TypeBool },
    { "SetCanSeek", notsupported, "true", KParts::LiveConnectExtension::TypeBool },
    { "SetCenter", notsupported, "true", KParts::LiveConnectExtension::TypeBool },
    { "SetConsole", notsupported, "true", KParts::LiveConnectExtension::TypeBool },
    { "SetConsoleEvents", notsupported, "true", KParts::LiveConnectExtension::TypeBool },
    { "SetControls", notsupported, "true", KParts::LiveConnectExtension::TypeBool },
    { "SetCopyright", notsupported, "true", KParts::LiveConnectExtension::TypeBool },
    { "SetCurrentPosition", notsupported, "true", KParts::LiveConnectExtension::TypeBool },
    { "SetDoubleSize", notsupported, "true", KParts::LiveConnectExtension::TypeBool },
    { "SetFileName", setsource, 0L, KParts::LiveConnectExtension::TypeBool },
    { "SetFullScreen", notsupported, "true", KParts::LiveConnectExtension::TypeBool },
    { "SetImageStatus", notsupported, "true", KParts::LiveConnectExtension::TypeBool },
    { "SetLoop", notsupported, "true", KParts::LiveConnectExtension::TypeBool },
    { "SetMaintainAspect", notsupported, "true", KParts::LiveConnectExtension::TypeBool },
    { "SetMute", notsupported, "true", KParts::LiveConnectExtension::TypeBool },
    { "SetNumLoop", notsupported, "true", KParts::LiveConnectExtension::TypeBool },
    { "SetOriginalSize", notsupported, "true", KParts::LiveConnectExtension::TypeBool },
    { "SetPosition", notsupported, "true", KParts::LiveConnectExtension::TypeBool },
    { "SetPreFetch", notsupported, "true", KParts::LiveConnectExtension::TypeBool },
    { "SetShowAbout", notsupported, "true", KParts::LiveConnectExtension::TypeBool },
    { "SetShowPreferences", notsupported, "true", KParts::LiveConnectExtension::TypeBool },
    { "SetShowStatistics", notsupported, "true", KParts::LiveConnectExtension::TypeBool },
    { "SetShuffle", notsupported, "true", KParts::LiveConnectExtension::TypeBool },
    { "SetSource", setsource, 0L, KParts::LiveConnectExtension::TypeBool },
    { "SetTitle", notsupported, "true", KParts::LiveConnectExtension::TypeBool },
    { "SetVolume", setvolume, "true", KParts::LiveConnectExtension::TypeBool },
    { "SetWantErrors", notsupported, "true", KParts::LiveConnectExtension::TypeBool },
    { "SetWantKeyboardEvents", notsupported, "true", KParts::LiveConnectExtension::TypeBool },
    { "SetWantMouseEvents", notsupported, "true", KParts::LiveConnectExtension::TypeBool },
    { "ShowControls", showcontrolpanel, "true", KParts::LiveConnectExtension::TypeBool },
    { "Start", start, 0L, KParts::LiveConnectExtension::TypeBool },
    { "Stop", stop, 0L, KParts::LiveConnectExtension::TypeBool },
    { "Volume", prop_volume, "100", KParts::LiveConnectExtension::TypeNumber },
    { "pause", jsc_pause, 0L, KParts::LiveConnectExtension::TypeBool },
    { "play", play, 0L, KParts::LiveConnectExtension::TypeBool },
    { "stop", stop, 0L, KParts::LiveConnectExtension::TypeBool },
    { "volume", volume, 0L, KParts::LiveConnectExtension::TypeBool },
};

static const JSCommandEntry * getJSCommandEntry (const char * name, int start = 0, int end = sizeof (JSCommandList)/sizeof (JSCommandEntry)) {
    if (end - start < 2) {
        if (start != end && !strcmp (JSCommandList[start].name, name))
            return &JSCommandList[start];
        return 0L;
    }
    int mid = (start + end) / 2;
    int cmp = strcmp (JSCommandList[mid].name, name);
    if (cmp < 0)
        return getJSCommandEntry (name, mid + 1, end);
    if (cmp > 0)
        return getJSCommandEntry (name, start, mid);
    return &JSCommandList[mid];
}

KDE_NO_CDTOR_EXPORT KMPlayerLiveConnectExtension::KMPlayerLiveConnectExtension (KMPlayerPart * parent)
  : KParts::LiveConnectExtension (parent), player (parent),
    lastJSCommandEntry (0L),
    m_started (false),
    m_enablefinish (false) {
      connect (parent, SIGNAL (started (KIO::Job *)), this, SLOT (started ()));
}

KDE_NO_CDTOR_EXPORT KMPlayerLiveConnectExtension::~KMPlayerLiveConnectExtension() {
    kdDebug () << "KMPlayerLiveConnectExtension::~KMPlayerLiveConnectExtension()" << endl;
}

KDE_NO_EXPORT void KMPlayerLiveConnectExtension::started () {
    m_started = true;
}

KDE_NO_EXPORT void KMPlayerLiveConnectExtension::finished () {
    if (m_started && m_enablefinish) {
        KParts::LiveConnectExtension::ArgList args;
        args.push_back (qMakePair (KParts::LiveConnectExtension::TypeString, QString("if (window.onFinished) onFinished();")));
        emit partEvent (0, "eval", args);
        m_started = true;
        m_enablefinish = false;
    }
}

KDE_NO_EXPORT bool KMPlayerLiveConnectExtension::get
  (const unsigned long id, const QString & name,
   KParts::LiveConnectExtension::Type & type,
   unsigned long & rid, QString & rval) {
    const char * str = name.ascii ();
    kdDebug () << "[01;35mget[00m " << str << endl;
    const JSCommandEntry * entry = getJSCommandEntry (str);
    if (!entry)
        return false;
    rid = id;
    type = entry->rettype;
    switch (entry->command) {
        case prop_source:
            type = KParts::LiveConnectExtension::TypeString;
            rval = player->url ().url ();
            break;
        case prop_volume:
            if (player->view ())
                rval = QString::number (player->process()->viewer ()->view()->controlPanel()->volumeBar()->value());
            break;
        default:
            lastJSCommandEntry = entry;
            type = KParts::LiveConnectExtension::TypeFunction;
    }
    return true;
}

KDE_NO_EXPORT bool KMPlayerLiveConnectExtension::put
  (const unsigned long, const QString & name, const QString & val) {
    kdDebug () << "[01;35mput[00m " << name << "=" << val << endl;
    const JSCommandEntry * entry = getJSCommandEntry (name.ascii ());
    switch (entry->command) {
        case prop_source: {
            KURL url (val);
            if (player->allowRedir (url))
                player->openURL (url);
            break;
        }
        case prop_volume:
            if (player->view ())
                player->process()->viewer ()->view()->controlPanel()->volumeBar()->setValue(val.toInt ());
            break;
        default:
            return false;
    }
    return true;
}

KDE_NO_EXPORT bool KMPlayerLiveConnectExtension::call
  (const unsigned long id, const QString & name,
   const QStringList & args, KParts::LiveConnectExtension::Type & type,
   unsigned long & rid, QString & rval) {
    const JSCommandEntry * entry = lastJSCommandEntry;
    const char * str = name.ascii ();
    if (!entry || strcmp (entry->name, str))
        entry = getJSCommandEntry (str);
    if (!entry)
        return false;
    kdDebug () << "[01;35mentry[00m " << entry->name << endl;
    for (unsigned int i = 0; i < args.size (); ++i)
        kdDebug () << "      " << args[i] << endl;
    KMPlayer::View * view = static_cast <KMPlayer::View*> (player->view ());
    if (!view)
        return false;
    rid = id;
    type = entry->rettype;
    switch (entry->command) {
        case notsupported:
            if (entry->rettype != KParts::LiveConnectExtension::TypeVoid)
                rval = entry->defaultvalue;
            break;
        case canpause:
            rval = (player->process ()->playing () && !view->controlPanel()->button (KMPlayer::ControlPanel::button_pause)->isOn ()) ? "true" : "false";
            break;
        case canplay:
            rval = (!player->process ()->playing () || view->controlPanel()->button (KMPlayer::ControlPanel::button_pause)->isOn ()) ? "true" : "false";
            break;
        case canstop:
            rval = player->process ()->playing () ? "true" : "false";
            break;
        case canseek:
            rval = player->source ()->isSeekable () ? "true" : "false";
            break;
        case play:
            if (args.size ()) {
                KURL url (args.first ());
                if (player->allowRedir (url))
                    player->openURL (url);
            } else
                player->play ();
            rval = "true";
            break;
        case start:
            player->play ();
            rval = "true";
            break;
        case stop:
            player->stop ();
            rval = "true";
            break;
        case showcontrolpanel:
            if (args.size () &&
                    (args.first () == QString::fromLatin1 ("0") ||
                     args.first () == QString::fromLatin1 ("false")))
                static_cast <KMPlayer::View*> (player->view ())->setControlPanelMode (KMPlayer::View::CP_Hide);
            else
                static_cast <KMPlayer::View*> (player->view ())->setControlPanelMode (KMPlayer::View::CP_Show);
            break;
        case jsc_pause:
            player->pause ();
            rval = "true";
            break;
        case isloop:
            rval = player->settings ()->loop ? "true" : "false";
            break;
        case isaspect:
            rval = player->settings ()->sizeratio ? "true" : "false";
            break;
        case isfullscreen:
            rval = static_cast <KMPlayer::View*> (player->view ())->isFullScreen () ? "true" : "false";
            break;
        case length:
            rval.setNum (player->source ()->length ());
            break;
        case width:
            rval.setNum (player->source ()->width ());
            break;
        case height:
            rval.setNum (player->source ()->height ());
            break;
        case playstate: // FIXME 0-6
            rval = player->process ()->playing () ? "3" : "0";
            break;
        case position:
            rval.setNum (player->position ());
            break;
        case protocol:
            rval = player->url ().protocol ();
            break;
        case setsource:
            rval ="false";
            if (args.size ()) {
                KURL url (args.first ());
                if (player->allowRedir (url) && player->openURL (url))
                    rval = "true";
            }
            break;
        case setvolume:
            if (!args.size ())
                return false;
            player->process()->viewer ()->view()->controlPanel()->volumeBar()->setValue(args.first ().toInt ());
            rval = "true";
            break;
        case source:
            rval = player->url ().url ();
            break;
        case volume:
            if (player->view ())
                rval = QString::number (player->process()->viewer ()->view()->controlPanel()->volumeBar()->value());
            break;
        default:
            return false;
    }
    return true;
}

KDE_NO_EXPORT void KMPlayerLiveConnectExtension::unregister (const unsigned long) {
}

KDE_NO_EXPORT void KMPlayerLiveConnectExtension::setSize (int w, int h) {
    KMPlayer::View * view = static_cast <KMPlayer::View*> (player->view ());
    if (view->controlPanelMode () == KMPlayer::View::CP_Show)
        h += view->controlPanel()->height();
    QCString jscode;
    //jscode.sprintf("this.width=%d;this.height=%d;kmplayer", w, h);
    KParts::LiveConnectExtension::ArgList args;
    args.push_back (qMakePair (KParts::LiveConnectExtension::TypeString, QString("width")));
    args.push_back (qMakePair (KParts::LiveConnectExtension::TypeNumber, QString::number (w)));
    emit partEvent (0, "this.setAttribute", args);
    args.clear();
    args.push_back (qMakePair (KParts::LiveConnectExtension::TypeString, QString("height")));
    args.push_back (qMakePair (KParts::LiveConnectExtension::TypeNumber, QString::number (h)));
    emit partEvent (0, "this.setAttribute", args);
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT KMPlayerHRefSource::KMPlayerHRefSource (PartBase * player)
    : Source (i18n ("HREF"), player, "hrefsource") {
    //kdDebug () << "KMPlayerHRefSource::KMPlayerHRefSource" << endl;
}

KDE_NO_CDTOR_EXPORT KMPlayerHRefSource::~KMPlayerHRefSource () {
    //kdDebug () << "KMPlayerHRefSource::~KMPlayerHRefSource" << endl;
}

KDE_NO_EXPORT void KMPlayerHRefSource::init () {
    Source::init ();
    setIdentified ();
}

KDE_NO_EXPORT bool KMPlayerHRefSource::hasLength () {
    return false;
}

KDE_NO_EXPORT bool KMPlayerHRefSource::processOutput (const QString & /*str*/) {
    //return Source::processOutput (str);
    return true;
}

KDE_NO_EXPORT void KMPlayerHRefSource::setURL (const KURL & url) {
    m_url = url;
    m_identified = false;
    m_finished = false;
    Source::setURL (url);
    kdDebug () << "KMPlayerHRefSource::setURL " << m_url.url() << endl;
}

KDE_NO_EXPORT void KMPlayerHRefSource::play () {
    kdDebug () << "KMPlayerHRefSource::play " << m_url.url() << endl;
    m_player->setSource (m_player->sources () ["urlsource"]);
}

KDE_NO_EXPORT void KMPlayerHRefSource::activate () {
    m_player->stop ();
    if (m_finished) {
        QTimer::singleShot (0, this, SLOT (finished ()));
        return;
    }
    init ();
    m_player->setProcess ("mplayer");
    if (m_player->process ()->grabPicture (m_url, 0))
        connect (m_player->process (), SIGNAL (grabReady (const QString &)),
                 this, SLOT (grabReady (const QString &)));
    else {
        setURL (KURL ());
        QTimer::singleShot (0, this, SLOT (play ()));
    }
}

KDE_NO_EXPORT void KMPlayerHRefSource::clear () {
    setURL (KURL ());
}

KDE_NO_EXPORT void KMPlayerHRefSource::grabReady (const QString & path) {
    kdDebug () << "KMPlayerHRefSource::grabReady(" << path << ")" << endl;
    m_finished = true;
    m_grabfile = path;
    finished ();
}

KDE_NO_EXPORT void KMPlayerHRefSource::finished () {
    kdDebug () << "KMPlayerHRefSource::finished()" << endl;
    KMPlayer::View * view = static_cast <KMPlayer::View*> (m_player->view ());
    if (!view) return;
    if (!view->setPicture (m_grabfile)) {
        clear ();
        QTimer::singleShot (0, this, SLOT (play ()));
        return;
    }
    if (view->viewer ())
        connect (view, SIGNAL (pictureClicked ()), this, SLOT (play ()));
}

KDE_NO_EXPORT void KMPlayerHRefSource::deactivate () {
    kdDebug () << "KMPlayerHRefSource::deactivate()" << endl;
    KMPlayer::View * view = static_cast <KMPlayer::View*> (m_player->view ());
    if (!view) return;
    view->setPicture (QString::null);
    if (view->viewer ())
        disconnect (view, SIGNAL (pictureClicked ()), this, SLOT (play ()));
}

KDE_NO_EXPORT QString KMPlayerHRefSource::prettyName () {
    return QString (i18n ("WEB"));
}

#include "kmplayer_part.moc"
