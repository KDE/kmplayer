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
#include <qcstring.h>
#include <qpopupmenu.h>
#include <qtimer.h>
#include <qpushbutton.h>
#include <qslider.h>
#include <qmap.h>
#include <qvaluelist.h>

#include <klibloader.h>
#include <kdebug.h>
#include <kconfig.h>
#include <kaction.h>
#include <kapplication.h>
#include <klocale.h>
#include <kinstance.h>
#include <kparts/factory.h>
#include <kstaticdeleter.h>

#include "kmplayer_part.h"
#include "kmplayerview.h"
#include "kmplayerconfig.h"
#include "kmplayerprocess.h"

typedef QValueList <QGuardedPtr <KMPlayerPart> > KMPlayerPartList;

struct KMPlayerPartStatic {
    KMPlayerPartStatic () {}
    ~KMPlayerPartStatic ();
    KMPlayerPartList kmplayer_parts;
};

static KMPlayerPartStatic * kmplayerpart_static = 0L;

KMPlayerPartStatic::~KMPlayerPartStatic () {
    kmplayerpart_static = 0L;
    // delete map content
}

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

KMPlayerFactory::KMPlayerFactory () {
    s_instance = new KInstance ("kmplayer");
}

KMPlayerFactory::~KMPlayerFactory () {
    delete s_instance;
}

KParts::Part *KMPlayerFactory::createPartObject
  (QWidget *wparent, const char *wname,
   QObject *parent, const char * name,
   const char * cls, const QStringList & args) {
      kdDebug() << "KMPlayerFactory::createPartObject " << cls << endl;
      return new KMPlayerPart (wparent, wname, parent, name, args);
}

//-----------------------------------------------------------------------------

KMPlayerPart::KMPlayerPart (QWidget * wparent, const char *wname,
                    QObject * parent, const char *name, const QStringList &args)
 : KMPlayer (wparent, wname, parent, name, new KConfig ("kmplayerrc")),
   m_browserextension (new KMPlayerBrowserExtension (this)),
   m_liveconnectextension (new KMPlayerLiveConnectExtension (this)),
   m_hrefsource (new KMPlayerHRefSource (this)),
   m_features (Feat_Unknown),
   m_started_emited (false),
   m_havehref (false) {
    m_ispart = true;
    kdDebug () << "MPlayer::KMPlayer ()" << endl;
    if (!kmplayerpart_static)
        kmplayerpart_static = kmplayerpart_staticdeleter.setObject (new KMPlayerPartStatic ());
    setInstance (KMPlayerFactory::instance (), true);
    /*KAction *playact =*/ new KAction(i18n("P&lay"), 0, 0, this, SLOT(play ()), actionCollection (), "view_play");
    /*KAction *pauseact =*/ new KAction(i18n("&Pause"), 0, 0, this, SLOT(pause ()), actionCollection (), "view_pause");
    /*KAction *stopact =*/ new KAction(i18n("&Stop"), 0, 0, this, SLOT(stop ()), actionCollection (), "view_stop");
    QStringList::const_iterator it = args.begin ();
    for ( ; it != args.end (); ++it) {
        int equalPos = (*it).find("=");
        if (equalPos > 0) {
            QString name = (*it).left (equalPos).upper ();
            QString value = (*it).right ((*it).length () - equalPos - 1);
            if (value.at(0)=='\"')
                value = value.right (value.length () - 1);
            if (value.at (value.length () - 1) == '\"')
                value.truncate (value.length () - 1);
            kdDebug () << "name=" << name << " value=" << value << endl;
            if (name.lower () == "href") {
                m_urlsource->setURL (KURL (value));
                m_urlsource->setIdentified (false);
                m_havehref = true;
            } else if (name.lower()==QString::fromLatin1("width")) {
                m_noresize = true;
            } else if (name.lower()==QString::fromLatin1("height")) {
                m_noresize = true;
            } else if (name.lower()==QString::fromLatin1("controls")) {
                if (value.lower () == QString::fromLatin1("imagewindow")) {
                    m_features = Feat_Viewer;
                } else if (value.lower () == QString::fromLatin1("all")) {
                    m_features = Feat_All;
                } else if (value.lower () == QString::fromLatin1("tacctrl")) {
                    m_features = Feat_Label;
                } else if (value.lower () == QString::fromLatin1("controlpanel")) {
                    m_features = Feat_Controls;
                }
            } else if (name.lower()==QString::fromLatin1("nolabels")) {
                m_features &= ~Feat_Label;
            } else if (name.lower()==QString::fromLatin1("nocontrols")) {
                m_features &= ~Feat_Controls;
            } else if (name.lower()==QString::fromLatin1("console")) {
                m_group = value.isEmpty() ? QString::fromLatin1("_anonymous") : value;
            } else if (name.lower()==QString::fromLatin1("__khtml__pluginbaseurl")) {
                m_docbase = KURL (value);
            } else if (name.lower()==QString::fromLatin1("src")) {
                m_src_url = value;
            } else if (name.lower()==QString::fromLatin1("autostart"))
                m_autoplay = !(value.lower() == QString::fromLatin1("false") ||
                               value.lower() == QString::fromLatin1("0"));
        }
    }
    init ();
    m_view->zoomMenu ()->connectItem (KMPlayerView::menu_zoom50,
                                      this, SLOT (setMenuZoom (int)));
    m_view->zoomMenu ()->connectItem (KMPlayerView::menu_zoom100,
                                      this, SLOT (setMenuZoom (int)));
    m_view->zoomMenu ()->connectItem (KMPlayerView::menu_zoom150,
                                      this, SLOT (setMenuZoom (int)));
    KParts::Part::setWidget (m_view);
    setXMLFile("kmplayerpartui.rc");
   if (!m_group.isEmpty ())
        kmplayerpart_static->kmplayer_parts.push_back (this);
}

KMPlayerPart::~KMPlayerPart () {
    kdDebug() << "KMPlayerPart::~KMPlayerPart" << endl;
    if (!m_group.isEmpty ())
        kmplayerpart_static->kmplayer_parts.remove (this);
    delete m_config;
    m_config = 0L;
}

bool KMPlayerPart::allowRedir (const KURL & url) {
    return kapp->authorizeURLAction ("redirect", url, m_docbase);
}

bool KMPlayerPart::openURL (const KURL & url) {
    kdDebug () << "KMPlayerPart::openURL " << url.url() << endl;
    KMPlayerPart * current_player = 0L;
    KMPlayerPartList::iterator i = kmplayerpart_static->kmplayer_parts.begin ();
    for (; i != kmplayerpart_static->kmplayer_parts.end (); ++i) {
        kdDebug() << "[00;31m" << m_src_url << " other:" << (*i)->m_src_url << "[00m " << (current_player!=(KMPlayerPart*)*i) << " " << (m_group == (*i)->m_group) << ((*i)->m_src_url == m_src_url) <<endl;
        if (!current_player && *i && (KMPlayerPart*)*i != this &&
                (*i)->m_src_url == m_src_url &&
                m_group != QString::fromLatin1("_unique") &&
                (*i)->m_group != QString::fromLatin1("_unique") &&
                ((*i)->m_group == m_group ||
                 (*i)->m_group == QString::fromLatin1("_master") ||
                 m_group == QString::fromLatin1("_master"))) {
            current_player = *i;
            break;
        }
    }
    KMPlayerControlPanel * panel = m_view->buttonBar ();
    if (m_features & Feat_Controls) {
        panel->show ();
        m_view->setAutoHideButtons (false);
    }
    if (current_player) {
        if (m_features & Feat_Controls) {
            removeControlPanel (panel);
            current_player->addControlPanel (panel);
            if (current_player->m_view) {
                current_player->m_view->setAutoHideButtons (false);
                current_player->m_view->buttonBar ()->hide ();
            }
        }
        if (m_features & Feat_Viewer) {
            m_view->setForeignViewer (current_player->m_view);
            m_view->setAutoHideButtons (false);
            panel->hide ();
        }
        return true;
    }
    if (!m_view || !url.isValid ()) return false;
    if (m_havehref && !kapp->authorizeURLAction ("redirect", url, m_urlsource->url ()))
        m_havehref = false;
    if (m_havehref && m_settings->allowhref) {
        m_hrefsource->setURL (url);
        setSource (m_hrefsource);
    } else {
        m_hrefsource->clear ();
        KMPlayer::openURL (m_havehref ? m_urlsource->url () : url);
    }
    m_havehref = false;
    return true;
}

bool KMPlayerPart::closeURL () {
    if (!m_group.isEmpty ()) {
        kmplayerpart_static->kmplayer_parts.remove (this);
        m_group.truncate (0);
    }
    return KMPlayer::closeURL ();
}

void KMPlayerPart::processFinished () {
    KMPlayer::processFinished ();
    if (m_started_emited) {
        m_started_emited = false;
        m_browserextension->setLoadingProgress (100);
        emit completed ();
    } else {
        emit canceled (i18n ("Could not start MPlayer"));
    }
    if (m_view)
        m_browserextension->infoMessage (i18n ("KMPlayer: Stop Playing"));
}

void KMPlayerPart::processLoading (int percentage) {
    KMPlayer::processLoading (percentage);
    if (percentage < 100) {
        m_browserextension->setLoadingProgress (percentage);
        m_browserextension->infoMessage 
            (QString::number (percentage) + i18n ("% Cache fill"));
    }
}

void KMPlayerPart::processStarted () {
    KMPlayer::processStarted ();
    emit started (0L);
    m_started_emited = true;
}

void KMPlayerPart::processPlaying () {
    if (!m_view) return;
    kdDebug () << "KMPlayerPart::processPlaying " << endl;
    KMPlayer::processPlaying ();
    if (m_settings->sizeratio && !m_noresize)
        m_liveconnectextension->setSize (m_process->source ()->width (),
                                         m_process->source ()->height ());
    m_browserextension->setLoadingProgress (100);
    emit completed ();
    m_started_emited = false;
    m_browserextension->infoMessage (i18n("KMPlayer: Playing"));
}

void KMPlayerPart::setMenuZoom (int id) {
    int w, h;
    sizes (w, h);
    if (id == KMPlayerView::menu_zoom100) {
        m_liveconnectextension->setSize (w, h);
        return;
    }
    float scale = 1.5;
    if (id == KMPlayerView::menu_zoom50)
        scale = 0.5;
    if (m_view->viewer ())
        m_liveconnectextension->setSize (int (scale * m_view->viewer ()->width ()),
                                         int (scale * m_view->viewer ()->height()));
}

//---------------------------------------------------------------------

KMPlayerBrowserExtension::KMPlayerBrowserExtension (KMPlayerPart * parent)
  : KParts::BrowserExtension (parent, "KMPlayer Browser Extension") {
}

void KMPlayerBrowserExtension::urlChanged (const QString & url) {
    emit setLocationBarURL (url);
}

void KMPlayerBrowserExtension::setLoadingProgress (int percentage) {
    emit loadingProgress (percentage);
}

void KMPlayerBrowserExtension::setURLArgs (const KParts::URLArgs & /*args*/) {
}

void KMPlayerBrowserExtension::saveState (QDataStream & stream) {
    stream << static_cast <KMPlayer *> (parent ())->url ().url ();
}

void KMPlayerBrowserExtension::restoreState (QDataStream & stream) {
    QString url;
    stream >> url;
    static_cast <KMPlayer *> (parent ())->openURL (KURL(url));
}
//---------------------------------------------------------------------

enum JSCommand {
    notsupported,
    canpause, canplay, canstop, canseek, 
    isfullscreen, isloop, isaspect,
    length, width, height, playstate, position, source, setsource, protocol,
    gotourl, nextentry, jsc_pause, play, preventry, stop, volume
};

struct JSCommandEntry {
    const char * name;
    JSCommand command;
    const char * defaultvalue;
    const KParts::LiveConnectExtension::Type rettype;
};

const int jscommandentries = 112;

// keep this list in alphabetic order
// http://service.real.com/help/library/guides/realonescripting/browse/htmfiles/embedmet.htm
static const JSCommandEntry JSCommandList [jscommandentries] = {
    { "CanPause", canpause, 0L, KParts::LiveConnectExtension::TypeBool },
    { "CanPlay", canplay, 0L, KParts::LiveConnectExtension::TypeBool },
    { "CanStop", canstop, 0L, KParts::LiveConnectExtension::TypeBool },
    { "DoGotoURL", notsupported, 0L, KParts::LiveConnectExtension::TypeVoid },
    { "DoNextEntry", notsupported, "false", KParts::LiveConnectExtension::TypeBool },
    { "DoPause", jsc_pause, "true", KParts::LiveConnectExtension::TypeBool },
    { "DoPlay", play, 0L, KParts::LiveConnectExtension::TypeBool },
    { "DoPrevEntry", notsupported, "false", KParts::LiveConnectExtension::TypeBool },
    { "DoStop", stop, 0L, KParts::LiveConnectExtension::TypeBool },
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
    { "GetVolume", notsupported, "100", KParts::LiveConnectExtension::TypeNumber },
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
    { "SetVolume", notsupported, "true", KParts::LiveConnectExtension::TypeBool },
    { "SetWantErrors", notsupported, "true", KParts::LiveConnectExtension::TypeBool },
    { "SetWantKeyboardEvents", notsupported, "true", KParts::LiveConnectExtension::TypeBool },
    { "SetWantMouseEvents", notsupported, "true", KParts::LiveConnectExtension::TypeBool },
    { "Stop", stop, 0L, KParts::LiveConnectExtension::TypeBool },
    { "Volume", volume, 0L, KParts::LiveConnectExtension::TypeBool },
    { "pause", jsc_pause, 0L, KParts::LiveConnectExtension::TypeBool },
    { "play", play, 0L, KParts::LiveConnectExtension::TypeBool },
    { "stop", stop, 0L, KParts::LiveConnectExtension::TypeBool },
    { "volume", volume, 0L, KParts::LiveConnectExtension::TypeBool },
};

static const JSCommandEntry * getJSCommandEntry (const char * name, int start = 0, int end = jscommandentries) {
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

KMPlayerLiveConnectExtension::KMPlayerLiveConnectExtension (KMPlayerPart * parent)
  : KParts::LiveConnectExtension (parent), player (parent),
    lastJSCommandEntry (0L),
    m_started (false),
    m_enablefinish (false) {
      connect (parent, SIGNAL (started (KIO::Job *)), this, SLOT (started ()));
      connect (parent, SIGNAL (stopPlaying ()), this, SLOT (finished ()));
}

KMPlayerLiveConnectExtension::~KMPlayerLiveConnectExtension() {
    kdDebug () << "KMPlayerLiveConnectExtension::~KMPlayerLiveConnectExtension()" << endl;
}

void KMPlayerLiveConnectExtension::started () {
    m_started = true;
}

void KMPlayerLiveConnectExtension::finished () {
    if (m_started && m_enablefinish) {
        KParts::LiveConnectExtension::ArgList args;
        args.push_back (qMakePair (KParts::LiveConnectExtension::TypeString, QString("if (window.onFinished) onFinished();")));
        emit partEvent (0, "eval", args);
        m_started = true;
        m_enablefinish = false;
    }
}

bool KMPlayerLiveConnectExtension::get
  (const unsigned long id, const QString & name,
   KParts::LiveConnectExtension::Type & type, unsigned long & rid, QString &) {
    const char * str = name.ascii ();
    kdDebug () << "get: " << str << endl;
    lastJSCommandEntry = getJSCommandEntry (str);
    if (lastJSCommandEntry) {
        type = KParts::LiveConnectExtension::TypeFunction;
        rid = id;
        return true;
    }
    return false;
}

bool KMPlayerLiveConnectExtension::put
  (const unsigned long, const QString &, const QString &) {
    return false;
}

bool KMPlayerLiveConnectExtension::call
  (const unsigned long id, const QString & name,
   const QStringList & args, KParts::LiveConnectExtension::Type & type,
   unsigned long & rid, QString & rval) {
    const JSCommandEntry * entry = lastJSCommandEntry;
    const char * str = name.ascii ();
    if (!entry || strcmp (entry->name, str))
        entry = getJSCommandEntry (str);
    if (!entry)
        return false;
    kdDebug () << "entry " << entry->name << endl;
    KMPlayerView * view = static_cast <KMPlayerView*> (player->view ());
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
            rval = (player->process ()->playing () && !view->buttonBar()->pauseButton ()->isOn ()) ? "true" : "false";
            break;
        case canplay:
            rval = (!player->process ()->playing () || view->buttonBar()->pauseButton ()->isOn ()) ? "true" : "false";
            break;
        case canstop:
            rval = player->process ()->playing () ? "true" : "false";
            break;
        case canseek:
            rval = player->process ()->source ()->isSeekable () ? "true" : "false";
            break;
        case play:
            player->play ();
            rval = "true";
            break;
        case stop:
            player->stop ();
            rval = "true";
            break;
        case jsc_pause:
            player->pause ();
            rval = "true";
            break;
        case volume:
            if (!args.size ())
                return false;
            player->adjustVolume (args.first ().toInt ());
            rval = "true";
            break;
        case isloop:
            rval = player->settings ()->loop ? "true" : "false";
            break;
        case isaspect:
            rval = player->settings ()->sizeratio ? "true" : "false";
            break;
        case isfullscreen:
            rval = static_cast <KMPlayerView*> (player->view ())->isFullScreen () ? "true" : "false";
            break;
        case length:
            rval.setNum (player->process ()->source ()->length ());
            break;
        case width:
            rval.setNum (player->process ()->source ()->width ());
            break;
        case height:
            rval.setNum (player->process ()->source ()->height ());
            break;
        case playstate: // FIXME 0-6
            rval = player->process ()->playing () ? "3" : "0";
            break;
        case position:
            rval.setNum (player->position ());
            break;
        case source:
            rval = player->url ().url ();
            break;
        case setsource:
            rval ="false";
            if (args.size ()) {
                KURL url (args.first ());
                if (player->allowRedir (url) && player->openURL (url))
                    rval = "true";
            }
            break;
        case protocol:
            rval = player->url ().protocol ();
            break;
        default:
            return false;
    }
    return true;
}

void KMPlayerLiveConnectExtension::unregister (const unsigned long) {
}

void KMPlayerLiveConnectExtension::setSize (int w, int h) {
    KMPlayerView * view = static_cast <KMPlayerView*> (player->view ());
    if (view->buttonBar ()->isVisible () &&
            !player->settings ()->autohidebuttons)
        h += view->buttonBar()->height();
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

KMPlayerHRefSource::KMPlayerHRefSource (KMPlayer * player)
    : KMPlayerSource (i18n ("HREF"), player) {
    kdDebug () << "KMPlayerHRefSource::KMPlayerHRefSource" << endl;
}

KMPlayerHRefSource::~KMPlayerHRefSource () {
    kdDebug () << "KMPlayerHRefSource::~KMPlayerHRefSource" << endl;
}

void KMPlayerHRefSource::init () {
    KMPlayerSource::init ();
    setIdentified ();
}

bool KMPlayerHRefSource::hasLength () {
    return false;
}

bool KMPlayerHRefSource::processOutput (const QString & /*str*/) {
    //return KMPlayerSource::processOutput (str);
    return true;
}

void KMPlayerHRefSource::setURL (const KURL & url) { 
    m_url = url;
    m_identified = false;
    m_finished = false;
    kdDebug () << "KMPlayerHRefSource::setURL " << m_url.url() << endl;
}

void KMPlayerHRefSource::play () {
    kdDebug () << "KMPlayerHRefSource::play " << m_url.url() << endl;
    m_player->setSource (m_player->urlSource ());
}

void KMPlayerHRefSource::activate () {
    m_player->stop ();
    if (m_finished) {
        QTimer::singleShot (0, this, SLOT (finished ()));
        return;
    }
    init ();
    m_player->setProcess (m_player->mplayer ());
    if (m_player->process ()->grabPicture (m_url, 0))
        connect (m_player->process (), SIGNAL (grabReady (const QString &)),
                 this, SLOT (grabReady (const QString &)));
    else {
        setURL (KURL ());
        QTimer::singleShot (0, this, SLOT (play ()));
    }
}

void KMPlayerHRefSource::clear () {
    setURL (KURL ());
    disconnect (m_player, SIGNAL (stopPlaying ()), this, SLOT (finished ()));
}

void KMPlayerHRefSource::grabReady (const QString & path) {
    kdDebug () << "KMPlayerHRefSource::grabReady(" << path << ")" << endl;
    m_finished = true;
    m_grabfile = path;
    connect (m_player, SIGNAL (stopPlaying ()), this, SLOT (finished ()));
    finished ();
}

void KMPlayerHRefSource::finished () {
    kdDebug () << "KMPlayerHRefSource::finished()" << endl;
    KMPlayerView * view = static_cast <KMPlayerView*> (m_player->view ());
    if (!view->setPicture (m_grabfile)) {
        clear ();
        QTimer::singleShot (0, this, SLOT (play ()));
        return;
    }
    if (view->viewer ())
        connect (view->viewer (), SIGNAL (clicked ()), this, SLOT (play ()));
}

void KMPlayerHRefSource::deactivate () {
    kdDebug () << "KMPlayerHRefSource::deactivate()" << endl;
    KMPlayerView * view = static_cast <KMPlayerView*> (m_player->view ());
    view->setPicture (QString::null);
    if (view->viewer ())
        disconnect (view->viewer (), SIGNAL (clicked ()), this, SLOT (play ()));
}

QString KMPlayerHRefSource::prettyName () {
    return QString (i18n ("WEB"));
}

#include "kmplayer_part.moc"
