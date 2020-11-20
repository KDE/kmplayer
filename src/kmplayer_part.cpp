/*
    SPDX-FileCopyrightText: 2002-2003 Koos Vriezen <koos.vriezen@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#ifdef KDE_USE_FINAL
#undef Always
#endif
#include "kmplayer_part.h"

#include <list>
#include <algorithm>

#include <QLabel>
#include <QMenu>
#include <QTimer>
#include <QPushButton>
#include <QSlider>
#include <QStatusBar>
#include <QFile>
#include <QMetaObject>
#include <QStandardPaths>

class KXMLGUIClient; // workaround for kde3.3 on sarge with gcc4, kactioncollection.h does not forward declare KXMLGUIClient
#include <KConfig>
#include <KUrlAuthorized>
#include <KIconLoader>
#include <KLocalizedString>
#if KCOREADDONS_VERSION >= QT_VERSION_CHECK(5, 77, 0)
#include <KPluginMetaData>
#endif

#include "kmplayerpart_log.h"
#include "kmplayerview.h"
#include "playlistview.h"
#include "kmplayercontrolpanel.h"
#include "kmplayerconfig.h"
#include "kmplayerprocess.h"
#include "viewarea.h"

#include <cstdlib>
#include <unistd.h> // unlink, getpid

using namespace KMPlayer;

typedef std::list <KMPlayerPart *> KMPlayerPartList;

class KMPlayerPartStatic : public GlobalShared<KMPlayerPartStatic>
{
public:
    KMPlayerPartStatic (KMPlayerPartStatic **);
    ~KMPlayerPartStatic () override;
    KMPlayerPartList partlist;
    int counter;
};

static KMPlayerPartStatic * kmplayerpart_static = nullptr;

KMPlayerPartStatic::KMPlayerPartStatic (KMPlayerPartStatic **glob) : GlobalShared<KMPlayerPartStatic> (glob), counter (0) {
    Ids::init ();
}

KMPlayerPartStatic::~KMPlayerPartStatic () {
    kmplayerpart_static = nullptr;
    Ids::reset ();
    // delete map content
}

struct GroupPredicate
{
    const KMPlayerPart * m_part;
    const QString & m_group;
    bool m_get_any;
    GroupPredicate(const KMPlayerPart *part, const QString &group, bool b=false)
        : m_part (part), m_group (group), m_get_any (b) {}
    bool operator () (const KMPlayerPart * part) const {
        return ((m_get_any && part != m_part &&
                    !part->master () && !part->url ().isEmpty ()) ||
                (m_part->allowRedir (part->docBase ()) &&
                 (part->m_group == m_group ||
                  part->m_group == QString::fromLatin1("_master") ||
                  m_group == QString::fromLatin1("_master")) &&
                 (part->m_features & KMPlayerPart::Feat_Viewer) !=
                 (m_part->m_features & KMPlayerPart::Feat_Viewer)));
    }
};

//-----------------------------------------------------------------------------

#if KCOREADDONS_VERSION < QT_VERSION_CHECK(5, 77, 0)
KAboutData* KMPlayerFactory::s_about = nullptr;
#endif

KMPlayerFactory::KMPlayerFactory () {
}

KMPlayerFactory::~KMPlayerFactory () {
}

#if KCOREADDONS_VERSION < QT_VERSION_CHECK(5, 77, 0)
KAboutData& KMPlayerFactory::aboutData() {
    if (!s_about) {
        s_about = new KAboutData("kmplayer", i18n("KMPlayer"), QStringLiteral(KMPLAYER_VERSION_STRING),
                i18n("Embedded MPlayer by KDE"),
                KAboutLicense::LGPL);
        s_about->addAuthor(QStringLiteral("Koos Vriezen"), QString(), "koos.vriezen@gmail.com");
    }
    return *s_about;
}
#endif

QObject* KMPlayerFactory::create(const char *iface, QWidget* parentWidget, QObject* parent,
        const QVariantList& args, const QString&)
{
#if KCOREADDONS_VERSION >= QT_VERSION_CHECK(5, 77, 0)
    return new KMPlayerPart(parentWidget, parent, metaData(), args);
#else
    return new KMPlayerPart(parentWidget, parent, args);
#endif
}

//-----------------------------------------------------------------------------

GrabDocument::GrabDocument (KMPlayerPart *part, const QString &url,
        const QString &file, PlayListNotify *)
 : SourceDocument (part->sources () ["urlsource"], url),
   m_grab_file (file),
   m_part (part) {
     id = id_node_grab_document;
     resolved = true;
}

void GrabDocument::activate () {
    media_info = new MediaInfo (this, MediaManager::AudioVideo);
    media_info->create ();
    qCDebug(LOG_KMPLAYER_PART) << src;
    Mrl::activate ();
}

void GrabDocument::undefer () {
    begin ();
}

void GrabDocument::begin () {
    setState (state_began);
    AudioVideoMedia *av = static_cast <AudioVideoMedia *> (media_info->media);
    qCDebug(LOG_KMPLAYER_PART) << m_grab_file;
    av->grabPicture (m_grab_file, 0);
}

void GrabDocument::message (MessageType msg, void *content) {
    if (MsgMediaFinished == msg) {
        state = state_finished;
        m_part->startUrl (QUrl (), m_grab_file);
        // deleted here by Source::reset
    } else {
        SourceDocument::message (msg, content);
    }
}

//-----------------------------------------------------------------------------

static bool getBoolValue (const QString & value) {
    return (value.toLower() != QString::fromLatin1("false") &&
            value.toLower() != QString::fromLatin1("off") &&
            value.toLower() != QString::fromLatin1("0"));
}

#define SET_FEAT_ON(f) { m_features |= f; turned_off_features &= ~f; }
#define SET_FEAT_OFF(f) { m_features &= ~f; turned_off_features |= f; }

#if KCOREADDONS_VERSION >= QT_VERSION_CHECK(5, 77, 0)
    KMPlayerPart::KMPlayerPart(QWidget* wparent, QObject* ppart, const KPluginMetaData& metaData, const QVariantList &args)
#else
KMPlayerPart::KMPlayerPart (QWidget *wparent,
                    QObject* ppart, const QVariantList& args)
#endif
 : PartBase (wparent, ppart, KSharedConfig::openConfig ("kmplayerrc")),
   m_master (nullptr),
   m_browserextension (new KMPlayerBrowserExtension (this)),
   m_liveconnectextension (new KMPlayerLiveConnectExtension (this)),
   m_playtime_info(nullptr),
   m_expected_view_width (0),
   m_expected_view_height (0),
   m_features (Feat_Unknown),
   m_started_emited (false),
   m_wait_npp_loaded (false)
{
#if KCOREADDONS_VERSION >= QT_VERSION_CHECK(5, 77, 0)
    setMetaData(metaData);
#else
    setComponentData(KMPlayerFactory::aboutData());
#endif
    qCDebug(LOG_KMPLAYER_PART) << "KMPlayerPart(" << this << ")::KMPlayerPart ()";
    bool show_fullscreen = false;
    if (!kmplayerpart_static)
        (void) new KMPlayerPartStatic (&kmplayerpart_static);
    else
        kmplayerpart_static->ref ();
    init (actionCollection (),
         QString ("/KMPlayerPart%1").arg(kmplayerpart_static->counter++), true);
    createBookmarkMenu (m_view->controlPanel ()->bookmarkMenu, actionCollection ());
    m_view->controlPanel ()->bookmarkAction->setVisible (true);
    ///*KAction *playact =*/ new KAction(i18n("P&lay"), QString ("player_play"), KShortcut (), this, SLOT(play ()), actionCollection (), "play");
    ///*KAction *pauseact =*/ new KAction(i18n("&Pause"), QString ("player_pause"), KShortcut (), this, SLOT(pause ()), actionCollection (), "pause");
    ///*KAction *stopact =*/ new KAction(i18n("&Stop"), QString ("player_stop"), KShortcut (), this, SLOT(stop ()), actionCollection (), "stop");
    //new KAction (i18n ("Increase Volume"), QString ("player_volume"), KShortcut (), this, SLOT (increaseVolume ()), actionCollection (), "edit_volume_up");
    //new KAction (i18n ("Decrease Volume"), QString ("player_volume"), KShortcut (), this, SLOT (decreaseVolume ()), actionCollection (), "edit_volume_down");
    Source *source = m_sources ["urlsource"];
    KMPlayer::ControlPanel * panel = m_view->controlPanel ();
    QVariantList::const_iterator it = args.begin ();
    QVariantList::const_iterator end = args.end ();
    int turned_off_features = 0;
    for ( ; it != end; ++it) {
        QString arg = (*it).toString();
        int equalPos = arg.indexOf("=");
        if (equalPos > 0) {
            QString name = arg.left(equalPos).toLower();
            QString value = arg.right(arg.size() - equalPos - 1);
            if (value.at(0)=='\"')
                value = value.right (value.size () - 1);
            if (value.at (value.size () - 1) == '\"')
                value.truncate (value.size () - 1);
            qCDebug(LOG_KMPLAYER_PART) << "name=" << name << " value=" << value;
            if (name == "href") {
                m_href_url = value;
            } else if (name == QString::fromLatin1("target")) {
                m_target = value;
            } else if (name == QString::fromLatin1("width")) {
                m_noresize = true;
                m_expected_view_width = value.toInt ();
            } else if (name == QString::fromLatin1("height")) {
                m_noresize = true;
                m_expected_view_height = value.toInt ();
            } else if (name == QString::fromLatin1("controls")) {
                //http://service.real.com/help/library/guides/production8/realpgd.htm?src=noref,rnhmpg_080301,rnhmtn,nosrc
                //http://service.real.com/help/library/guides/production8/htmfiles/control.htm
                QStringList sl = value.split(QChar (','));
                const QStringList::const_iterator e = sl.constEnd ();
                for (QStringList::const_iterator i = sl.constBegin (); i != e; ++i) {
                    QString val_lower ((*i).toLower ());
                    if (val_lower == QString::fromLatin1("imagewindow")) {
                        SET_FEAT_ON (Feat_ImageWindow | Feat_Viewer)
                    } else if (val_lower == QString::fromLatin1("all")) {
                        m_features = (Feat_Controls | Feat_StatusBar);
                    } else if (val_lower == QString::fromLatin1("tacctrl")) {
                        SET_FEAT_ON (Feat_Label)
                    } else if (val_lower == QString::fromLatin1("controlpanel")) {
                        SET_FEAT_ON (Feat_Controls)
                    } else if (val_lower == QString::fromLatin1("infovolumepanel")){
                        SET_FEAT_ON (Feat_Controls) // TODO
                    } else if (val_lower == QString::fromLatin1("positionfield") ||
                            val_lower == QString::fromLatin1("positionslider")) {
                        setAutoControls (false);
                        panel->positionSlider ()->show ();
                        SET_FEAT_ON (Feat_Controls)
                    } else if ( val_lower == QString::fromLatin1("homectrl")) {
                        setAutoControls (false);
                        panel->button (KMPlayer::ControlPanel::button_config)->show();
                    } else if (val_lower == QString::fromLatin1("mutectrl") ||
                            val_lower == QString::fromLatin1("mutevolume")) {
                        setAutoControls (false);
                        panel->volumeBar()->setMinimumSize (QSize (20, panel->volumeBar()->minimumSize ().height ()));
                        panel->volumeBar()->show ();
                        SET_FEAT_ON (Feat_Controls)
                    } else if (val_lower == QString::fromLatin1("rwctrl")) {
                        setAutoControls (false);
                        panel->button (KMPlayer::ControlPanel::button_back)->show (); // rewind ?
                        SET_FEAT_ON (Feat_Controls)
                    } else if ( val_lower == QString::fromLatin1("ffctrl")) {
                        setAutoControls (false);
                        panel->button(KMPlayer::ControlPanel::button_forward)->show();
                        m_features = Feat_Controls;
                    } else if ( val_lower ==QString::fromLatin1("stopbutton")) {
                        setAutoControls (false);
                        panel->button (KMPlayer::ControlPanel::button_stop)->show ();
                        SET_FEAT_ON (Feat_Controls)
                    } else if (val_lower == QString::fromLatin1("playbutton") ||
                            val_lower ==QString::fromLatin1("playonlybutton")) {
                        setAutoControls (false);
                        panel->button (KMPlayer::ControlPanel::button_play)->show ();
                        SET_FEAT_ON (Feat_Controls)
                    } else if (val_lower ==QString::fromLatin1("pausebutton")) {
                        setAutoControls (false);
                        panel->button (KMPlayer::ControlPanel::button_pause)->show ();
                        SET_FEAT_ON (Feat_Controls)
                    } else if (val_lower == QString::fromLatin1("statusbar") ||
                            val_lower == QString::fromLatin1("statusfield")) {
                        SET_FEAT_ON (Feat_StatusBar)
                    } else if (val_lower == QString::fromLatin1("infopanel")) {
                        SET_FEAT_ON (Feat_InfoPanel)
                    } else if (val_lower == QString::fromLatin1("playlist")) {
                        SET_FEAT_ON (Feat_PlayList)
                    } else if (val_lower==QString::fromLatin1("volumeslider")) {
                        SET_FEAT_ON (Feat_VolumeSlider)
                        setAutoControls (false);
                        panel->volumeBar()->show ();
                        panel->volumeBar()->setMinimumSize (QSize (20, panel->volumeBar()->minimumSize ().height ()));
                    }
                }
            } else if (name == QString::fromLatin1("uimode")) {
                QString val_lower (value.toLower ());
                if (val_lower == QString::fromLatin1("full"))
                    SET_FEAT_ON (Feat_All & ~(Feat_PlayList | Feat_ImageWindow))
                // TODO: invisible, none, mini
            } else if (name == QString::fromLatin1("nolabels")) {
                SET_FEAT_OFF (Feat_Label)
            } else if (name == QString::fromLatin1("nocontrols")) {
                SET_FEAT_OFF (Feat_Controls | Feat_VolumeSlider)
            } else if (name == QString::fromLatin1("showdisplay")) {
                // the author name, the clip name, and the copyright information
                if (getBoolValue (value))
                    SET_FEAT_ON (Feat_InfoPanel)
                else
                    SET_FEAT_OFF (Feat_InfoPanel)
            } else if (name == QString::fromLatin1("showcontrols") ||
                    name == QString::fromLatin1("controller")) {
                if (getBoolValue (value))
                    SET_FEAT_ON (Feat_Viewer | Feat_Controls)
                else
                    SET_FEAT_OFF (Feat_Controls | Feat_VolumeSlider)
            } else if (name == QString::fromLatin1("showstatusbar")) {
                if (getBoolValue (value))
                    SET_FEAT_ON (Feat_Viewer | Feat_StatusBar)
                else
                    SET_FEAT_OFF (Feat_StatusBar)
            // else showcaptioning/showgotobar/showpositioncontrols/showtracker
            } else if (name == QString::fromLatin1("console")) {
                m_group = value.isEmpty() ? QString::fromLatin1("_anonymous") : value;
            } else if (name == QString::fromLatin1("__khtml__pluginbaseurl")) {
                m_docbase = QUrl (value);
            } else if (name == QString::fromLatin1("src")) {
                m_src_url = value;
            } else if (name == QString::fromLatin1("filename")) {
                m_file_name = value;
            } else if (name == QString::fromLatin1 ("fullscreenmode")) {
                show_fullscreen = getBoolValue (value);
            } else if (name == QString::fromLatin1 ("autostart")) {
                source->setAutoPlay (getBoolValue (value));
	    }
            // volume/clicktoplay/transparentatstart/animationatstart
            // autorewind/displaysize/border
            if (!name.startsWith (QString::fromLatin1 ("__khtml__")))
                convertNode <KMPlayer::Element> (source->document ())->setAttribute (name, value);
        }
    }
    if (turned_off_features) {
        if (m_features == Feat_Unknown)
            m_features = (Feat_All & ~(Feat_PlayList | Feat_ImageWindow | Feat_StatusBar));
        m_features &= ~turned_off_features;
    }
    //KParts::Part::setWidget (m_view);
    setXMLFile("kmplayerpartui.rc");
    /*connect (panel->zoom50Action, SIGNAL (triggered (bool)),
            this, SLOT (setMenuZoom (bool)));
    panel->zoomMenu ()->connectItem (KMPlayer::ControlPanel::menu_zoom100,
                                      this, SLOT (setMenuZoom (int)));
    panel->zoomMenu ()->connectItem (KMPlayer::ControlPanel::menu_zoom150,
                                      this, SLOT (setMenuZoom (int)));*/

    m_view->setNoInfoMessages (m_features != Feat_InfoPanel);
    if (m_features == Feat_InfoPanel) {
        m_view->initDock (m_view->infoPanel ());
    } else if (m_features == Feat_PlayList) {
        m_view->initDock (m_view->playList ());
    } else if (m_features == Feat_StatusBar) {
        m_view->initDock (m_view->statusBar ());
    } else {
        m_view->initDock (m_view->viewArea ());
        if (m_features & Feat_StatusBar) {
            m_view->setStatusBarMode (KMPlayer::View::SB_Show);
            m_expected_view_height -= m_view->statusBar ()->height ();
        }
        if (m_features & (Feat_Controls | Feat_VolumeSlider)) {
            m_view->setControlPanelMode (m_features & Feat_Viewer ? KMPlayer::View::CP_Show : KMPlayer::View::CP_Only);
            m_expected_view_height -= m_view->controlPanel ()->height ();
        } else if (parent ())
            m_view->setControlPanelMode (KMPlayer::View::CP_Hide);
        else
            m_view->setControlPanelMode (KMPlayer::View::CP_AutoHide);
    }
    bool group_member = !m_group.isEmpty () && m_group != QString::fromLatin1("_unique") && m_features != Feat_Unknown;
    if (!group_member || m_features & Feat_Viewer) {
        // not part of a group or we're the viewer
        connectPanel (m_view->controlPanel ());
        if (m_features & Feat_StatusBar) {
            last_time_left = 0;
            connect (this, SIGNAL (positioned (int, int)),
                     this, SLOT (statusPosition (int, int)));
            m_playtime_info = new QLabel("--:--");
            m_view->statusBar()->addPermanentWidget(m_playtime_info);
        }
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
            cp->connectToPart (vp);
        }
    } else
        m_group.truncate (0);
    kmplayerpart_static->partlist.push_back (this);

    if (m_view->isFullScreen () != show_fullscreen)
        m_view->fullScreen ();
}

#undef SET_FEAT_ON
#undef SET_FEAT_OFF

KMPlayerPart::~KMPlayerPart () {
    qCDebug(LOG_KMPLAYER_PART) << "KMPlayerPart::~KMPlayerPart";
    //if (!m_group.isEmpty ()) {
        KMPlayerPartList::iterator i = std::find (kmplayerpart_static->partlist.begin (), kmplayerpart_static->partlist.end (), this);
        if (i != kmplayerpart_static->partlist.end ())
            kmplayerpart_static->partlist.erase (i);
        else
            qCCritical(LOG_KMPLAYER_PART) << "KMPlayerPart::~KMPlayerPart group lost" << endl;
    //}
    if (!m_grab_file.isEmpty ())
        ::unlink (m_grab_file.toLocal8Bit ().data ());
    if (m_source)
        m_source->deactivate ();
    m_config = nullptr;
    kmplayerpart_static->unref ();
}

void KMPlayerPart::processCreated (KMPlayer::Process *p) {
#ifdef KMPLAYER_WITH_NPP
    if (p->objectName() == "npp") {
        if (m_wait_npp_loaded)
            connect (p, SIGNAL (loaded ()), this, SLOT (nppLoaded ()));
        connect (p, SIGNAL (evaluate (const QString &, bool, QString &)),
                m_liveconnectextension,
                SLOT (evaluate (const QString &, bool, QString &)));
        connect (m_liveconnectextension,
                SIGNAL (requestGet (const uint32_t, const QString &, QString *)),
                p, SLOT (requestGet (const uint32_t, const QString &, QString *)));
        connect (m_liveconnectextension,
                SIGNAL (requestCall (const uint32_t, const QString &, const QStringList, QString *)),
                p, SLOT (requestCall (const uint32_t, const QString &, const QStringList, QString *)));
    }
#endif
}

bool KMPlayerPart::allowRedir (const QUrl & url) const
{
    return KUrlAuthorized::authorizeUrlAction ("redirect", m_docbase, url);
}

void KMPlayerPart::setAutoControls (bool b) {
    m_auto_controls = b;
    m_view->controlPanel ()->setAutoControls (b);
}

void KMPlayerPart::viewerPartDestroyed (QObject * o) {
    if (o == m_master)
        m_master = nullptr;
    qCDebug(LOG_KMPLAYER_PART) << "KMPlayerPart(" << this << ")::viewerPartDestroyed";
    const KMPlayerPartList::iterator e =kmplayerpart_static->partlist.end();
    KMPlayerPartList::iterator i = std::find_if (kmplayerpart_static->partlist.begin (), e, GroupPredicate (this, m_group));
    if (i != e && *i != this)
        (*i)->updatePlayerMenu (m_view->controlPanel ());
}

void KMPlayerPart::viewerPartProcessChanged (const char *) {
    const KMPlayerPartList::iterator e =kmplayerpart_static->partlist.end();
    KMPlayerPartList::iterator i = std::find_if (kmplayerpart_static->partlist.begin (), e, GroupPredicate (this, m_group));
    if (i != e && *i != this)
        (*i)->updatePlayerMenu (m_view->controlPanel ());
}

void KMPlayerPart::viewerPartSourceChanged(Source *o, Source *s) {
    qCDebug(LOG_KMPLAYER_PART) << "KMPlayerPart::source changed " << m_master;
    if (m_master && m_view) {
        connectSource (o, s);
        m_master->updatePlayerMenu (m_view->controlPanel ());
    }
}

bool KMPlayerPart::openUrl(const QUrl& _url) {
    QUrl url;
    KParts::OpenUrlArguments args = arguments ();
    Source * urlsource = m_sources ["urlsource"];
    KMPlayerPartList::iterator i =kmplayerpart_static->partlist.begin ();
    KMPlayerPartList::iterator e =kmplayerpart_static->partlist.end ();
    GroupPredicate pred (this, m_group);
    bool emit_started = !m_settings->clicktoplay;

    qCDebug(LOG_KMPLAYER_PART) << "KMPlayerPart::openUrl " << _url.url() << " " << args.mimeType();;
    if (args.mimeType () == "application/x-shockwave-flash" ||
            args.mimeType () == "application/futuresplash") {
        m_wait_npp_loaded = true;
        urlsource->setAvoidRedirects (true);
    }

    if (m_wait_npp_loaded && (_url.isEmpty () || _url == m_docbase)) {
        url = QUrl ("about:empty");
        m_wait_npp_loaded = emit_started = false;
    } else if (!m_file_name.isEmpty () && (_url.isEmpty () || _url == m_docbase)) {
        url = m_docbase.resolved(QUrl(m_file_name)); // fix misdetected SRC attr
    } else if (_url != m_docbase) {
        url = _url;
        if (!m_file_name.isEmpty() && _url.url().indexOf(m_file_name) < 0) {
            QUrl u = QUrl::fromUserInput(m_file_name);
            if ((u.scheme () == QLatin1String("mms")) ||
                    _url.scheme().isEmpty()) {
                // see if we somehow have to merge these
                int p = _url.port ();
                if (p > 0)
                    u.setPort (p);
                if (u.path ().isEmpty ())
                    u.setPath (QChar ('/') + _url.host ());
                if (allowRedir (u)) {
                    url = u;
                    qCDebug(LOG_KMPLAYER_PART) << "KMPlayerPart::openUrl compose " << m_file_name << " " << _url.url() << " ->" << u.url();
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
    if (!m_href_url.isEmpty () &&
            !KUrlAuthorized::authorizeUrlAction (
                "redirect", url, m_docbase.resolved(QUrl(m_href_url))))
           m_href_url.truncate (0);
    if (m_href_url.isEmpty ())
        setUrl (url.url ());

    if (url.isEmpty ()) {
        if (!m_master && !(m_features & Feat_Viewer))
            // no master set, wait for a viewer to attach or timeout
            QTimer::singleShot (50, this, SLOT (waitForImageWindowTimeOut ()));
        return true;
    }
    m_src_url = url.url ();

    if (!m_group.isEmpty () && !(m_features & Feat_Viewer)) {
        // group member, not the image window
        for (i = std::find_if (i, e, pred);
                i != e;
                i = std::find_if (++i, e, pred))
            if ((*i)->url ().isEmpty ()) // image window created w/o url
                return (*i)->startUrl (_url);
        QTimer::singleShot (50, this, SLOT (waitForImageWindowTimeOut ()));
        //qCCritical(LOG_KMPLAYER_PART) << "Not the ImageWindow and no ImageWindow found" << endl;
        return true;
    }
    if (!m_view || !url.isValid ())
        return false;

    if (!args.mimeType ().isEmpty ())
        urlsource->document ()->mrl ()->mimetype = args.mimeType ();

    startUrl (url);

    if (emit_started && urlsource->autoPlay ()) {
        Q_EMIT started (nullptr);
        m_started_emited = true;
    }
    return true;
}

void
KMPlayerPart::openUrl(const QUrl& u, const QString& t, const QString& srv) {
    m_browserextension->requestOpenURL (u, t, srv);
}

bool KMPlayerPart::openNewURL (const QUrl & url) {
    m_file_name.truncate (0);
    m_href_url.truncate (0);
    m_sources ["urlsource"]->setAutoPlay (true);
    return openUrl (url);
}

bool KMPlayerPart::startUrl(const QUrl &uri, const QString &img)
{
    Source * src = sources () ["urlsource"];
    QUrl url (uri);
    qCDebug(LOG_KMPLAYER_PART) << "uri '" << uri << "' img '" << img;
    if (url.isEmpty ()) {
        url = QUrl::fromUserInput(m_src_url);
    } else if (m_settings->grabhref && !m_href_url.isEmpty ()) {
        static int counter;
        m_href_url = m_docbase.resolved(QUrl(m_href_url)).url ();
        m_grab_file = QString ("%1grab-%2-%3.jpg")
            .arg (QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)+ "kmplayer/")
            .arg (getpid ())
            .arg (counter++);
        Node *n = new GrabDocument (this, url.url (), m_grab_file, src);
        src->setUrl (url.url ());
        m_src_url = m_href_url;
        src->setDocument (n, n);
        setSource (src);
        if (m_source->avoidRedirects ())
            m_source->activate();
        return true;
    }
    if ((m_settings->clicktoplay || !m_href_url.isEmpty ()) &&
            (m_features & Feat_Viewer ||
             Feat_Unknown == m_features) &&
            m_expected_view_width > 10 &&
            m_expected_view_height > 10 &&
            parent () &&
            !strcmp ("KHTMLPart", parent ()->metaObject ()->className())) {
        QString pic (img);
        if (!pic.isEmpty ()) {
            QFile file (pic);
            if (!file.exists ()) {
                m_grab_file.truncate (0);
                pic.truncate (0);
            } else if (!file.size ()) {
                pic.truncate (0);
            }
        }
        QString img = pic.isEmpty ()
            ? QUrl::fromLocalFile (KIconLoader::global()->iconPath (
                        QString::fromLatin1 ("video-x-generic"),
#ifdef KMPLAYER_WITH_CAIRO
                        -128
#else
                        -32
#endif
                        )).url ()
            : pic;
#ifdef KMPLAYER_WITH_CAIRO
        QString smil = QString::fromLatin1 (
          "<smil><head><layout>"
          "<region id='reg1' left='12.5%' top='5%' right='12.5%' bottom='5%' "
          "background-color='#202030' showBackground='whenActive'/>"
          "<region id='reg2'/>"
          "<region id='reg3'/>"
          "</layout>"
          "<transition id='clockwipe1' dur='1' type='clockWipe'/>"
          "</head>"
          "<body>"
          "<a href='%1'%2>"
          "<img id='image1' src='%3' region='reg%4' fit='meet' "
          "begin='.5' dur='indefinite' transIn='clockwipe1'/>"
          "</a>"
          "<video id='video1' region='reg2' fit='meet'/>"
          "</body></smil>"
          ).arg (m_target.isEmpty ()
              ? QString ("#video1")
              : m_href_url.isEmpty () ? m_src_url : m_href_url)
           .arg (m_target.isEmpty ()
                   ? QString ()
                   : QString (" target='%1'").arg (m_target))
           .arg (img)
           .arg (pic.isEmpty () ? "1" : "3");
        QByteArray ba = smil.toUtf8 ();
        QTextStream ts (&ba, QIODevice::ReadOnly);
        if (m_source)
            m_source->deactivate ();
        NodePtr doc = src->document ();
        KMPlayer::readXML (doc, ts, QString (), false);
        NodePtr n = doc->document ()->getElementById ("video1");
        if (n) {
            Mrl *mrl = new GenericURL (doc, url.url ());
            n->appendChild (mrl);
            mrl->mimetype = doc->document ()->mimetype;
            mrl->opener = n;
            mrl->setAttributes (convertNode <Element> (doc)->attributes ());
            n->closed ();
            NodePtr im = doc->document ()->getElementById ("image1");
            if (im)
                im->mrl ()->access_granted = true;
        }
        doc->document ()->resolved = true;
        if (m_source) {
            m_source->activate();
        } else {
            setSource (src);
            if (m_source->avoidRedirects ())
                m_source->activate();
        }
#else
        if (m_view->setPicture (img)) {
            connect (m_view, SIGNAL (pictureClicked ()),
                     this, SLOT (pictureClicked ()));
            Q_EMIT completed ();
            m_started_emited = false;
        } else {
            return PartBase::openUrl(m_href_url.isEmpty() ? url : QUrl::fromUserInput(m_href_url));
        }
#endif
        return true;
    } else
        return PartBase::openUrl(m_href_url.isEmpty() ? url : QUrl::fromUserInput(m_href_url));
}

#ifndef KMPLAYER_WITH_CAIRO
void KMPlayerPart::pictureClicked () {
    m_view->setPicture (QString ());
    PartBase::openUrl (QUrl::fromUserInput(m_src_url));
}
#endif

void KMPlayerPart::waitForImageWindowTimeOut () {
    if (!m_master) {
        // still no ImageWindow attached, eg. audio only
        const KMPlayerPartList::iterator e =kmplayerpart_static->partlist.end();
        GroupPredicate pred (this, m_group);
        KMPlayerPartList::iterator i = std::find_if (kmplayerpart_static->partlist.begin (), e, pred);
        bool noattach = (i == e || *i == this);
        if (noattach) {
            if (!url ().isEmpty ()) {
                m_features |= KMPlayerPart::Feat_Viewer; //hack, become the view
                for (i = std::find_if (kmplayerpart_static->partlist.begin (), e, pred); i != e; i = std::find_if (++i, e, pred))
                    (*i)->connectToPart (this);
                PartBase::openUrl (url ());
            } else { // see if we can attach to something out there ..
                i = std::find_if (kmplayerpart_static->partlist.begin (), e, GroupPredicate (this, m_group, true));
                noattach = (i == e);
            }
        }
        if (!noattach)
            connectToPart (*i);
    }
}

bool KMPlayerPart::closeUrl () {
    if (!m_group.isEmpty ()) {
        kmplayerpart_static->partlist.remove (this);
        m_group.truncate (0);
    }
    return PartBase::closeUrl ();
}

void KMPlayerPart::connectToPart (KMPlayerPart * m) {
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
        m_playtime_info = new QLabel("--:--");
        m_view->statusBar()->addPermanentWidget(m_playtime_info);
    }
}

void KMPlayerPart::setLoaded (int percentage) {
    PartBase::setLoaded (percentage);
    if (percentage < 100) {
        m_browserextension->setLoadingProgress (percentage);
        m_browserextension->infoMessage
		// xgettext: no-c-format
            (QString::number (percentage) + i18n ("% Cache fill"));
    }
}

void KMPlayerPart::playingStarted () {
    const KMPlayerPartList::iterator e =kmplayerpart_static->partlist.end();
    KMPlayerPartList::iterator i = std::find_if (kmplayerpart_static->partlist.begin (), e, GroupPredicate (this, m_group));
    if (i != e && *i != this && m_view && (*i)->source()) {
        m_view->controlPanel ()->setPlaying (true);
        m_view->controlPanel ()->showPositionSlider(!!(*i)->source()->length ());
        m_view->controlPanel()->enableSeekButtons((*i)->source()->isSeekable());
        Q_EMIT loading (100);
    } else if (m_source)
        KMPlayer::PartBase::playingStarted ();
    else
        return; // ugh
    qCDebug(LOG_KMPLAYER_PART) << "KMPlayerPart::processStartedPlaying ";
    if (m_settings->sizeratio && !m_noresize && m_source->width() > 0 && m_source->height() > 0)
        m_liveconnectextension->setSize (m_source->width(), m_source->height());
    m_browserextension->setLoadingProgress (100);
    if (m_started_emited && !m_wait_npp_loaded) {
        Q_EMIT completed ();
        m_started_emited = false;
    }
    m_liveconnectextension->started ();
    m_browserextension->infoMessage (i18n("KMPlayer: Playing"));
}

void KMPlayerPart::playingStopped () {
    KMPlayer::PartBase::playingStopped ();
    if (m_started_emited && !m_wait_npp_loaded) {
        m_started_emited = false;
        m_browserextension->setLoadingProgress (100);
        Q_EMIT completed ();
    }
    m_liveconnectextension->finished ();
    m_browserextension->infoMessage (i18n ("KMPlayer: Stop Playing"));
    if (m_view)
        m_view->controlPanel ()->setPlaying (false);
}

void KMPlayerPart::nppLoaded () {
    if (m_started_emited && m_wait_npp_loaded) {
        m_wait_npp_loaded = false;
        m_started_emited = false;
        m_browserextension->setLoadingProgress (100);
        Q_EMIT completed ();
    }
}

void KMPlayerPart::setMenuZoom (int /*id*/) {
    /*int w = 0, h = 0;
    if (m_source)
        m_source->dimensions (w, h);
    if (id == KMPlayer::ControlPanel::menu_zoom100) {
        m_liveconnectextension->setSize (w, h);
        return;
    }
    float scale = 1.5;
    if (id == KMPlayer::ControlPanel::menu_zoom50)
        scale = 0.5;
    if (m_view)
        m_liveconnectextension->setSize (int (scale * m_view->viewArea ()->width ()),
                                         int (scale * m_view->viewArea ()->height()));*/
}

void KMPlayerPart::statusPosition (int pos, int length) {
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
        m_playtime_info->setText(text);
    }
}

QString KMPlayerPart::doEvaluate (const QString &script) {
    return m_liveconnectextension->evaluate (
            QString ("this.__kmplayer__res=" ) + script);
}

//---------------------------------------------------------------------

KMPlayerBrowserExtension::KMPlayerBrowserExtension (KMPlayerPart * parent)
  : KParts::BrowserExtension (parent) {
}

void KMPlayerBrowserExtension::urlChanged (const QString & url) {
    Q_EMIT setLocationBarUrl (url);
}

void KMPlayerBrowserExtension::setLoadingProgress (int percentage) {
    Q_EMIT loadingProgress (percentage);
}

void KMPlayerBrowserExtension::saveState (QDataStream & stream) {
    stream << static_cast <PartBase *> (parent ())->url ().url ();
}

void KMPlayerBrowserExtension::restoreState (QDataStream & stream) {
    QString url;
    stream >> url;
    static_cast <PartBase *> (parent ())->openUrl (QUrl(url));
}

void KMPlayerBrowserExtension::requestOpenURL (const QUrl & url, const QString & target, const QString & service)
{
    KParts::OpenUrlArguments args;
    KParts::BrowserArguments bargs;
    bargs.frameName = target;
    args.setMimeType (service);
    Q_EMIT openUrlRequest (url, args, bargs);
}

//---------------------------------------------------------------------
/*
 * add
 * .error.errorCount
 * .error.item(count)
 *   .errorDescription
 *   .errorCode
 * .controls.stop()
 * .controls.play()
 */

enum JSCommand {
    notsupported,
    canpause, canplay, canstop, canseek,
    isfullscreen, isloop, isaspect, showcontrolpanel,
    length, width, height, playstate, position, source, setsource, protocol,
    gotourl, nextentry, jsc_pause, play, preventry, start, stop,
    volume, setvolume,
    prop_error, prop_source, prop_volume,
    prop_qt_status, prop_qt_rate
};

struct JSCommandEntry
{
    const char * name;
    JSCommand command;
    const char * defaultvalue;
    const KParts::LiveConnectExtension::Type rettype;
};

// keep this list in alphabetic order
// http://service.real.com/help/library/guides/realonescripting/browse/htmfiles/embedmet.htm
static const JSCommandEntry JSCommandList [] = {
    { "CanPause", canpause, nullptr, KParts::LiveConnectExtension::TypeBool },
    { "CanPlay", canplay, nullptr, KParts::LiveConnectExtension::TypeBool },
    { "CanStop", canstop, nullptr, KParts::LiveConnectExtension::TypeBool },
    { "DoGotoURL", notsupported, nullptr, KParts::LiveConnectExtension::TypeVoid },
    { "DoNextEntry", notsupported, "false", KParts::LiveConnectExtension::TypeBool },
    { "DoPause", jsc_pause, "true", KParts::LiveConnectExtension::TypeBool },
    { "DoPlay", play, nullptr, KParts::LiveConnectExtension::TypeBool },
    { "DoPlayPause", play, nullptr, KParts::LiveConnectExtension::TypeBool },
    { "DoPrevEntry", notsupported, "false", KParts::LiveConnectExtension::TypeBool },
    { "DoStop", stop, nullptr, KParts::LiveConnectExtension::TypeBool },
    { "FileName", prop_source, nullptr, KParts::LiveConnectExtension::TypeString },
    { "GetAuthor", notsupported, "noname", KParts::LiveConnectExtension::TypeString },
    { "GetAutoGoToURL", notsupported, "true", KParts::LiveConnectExtension::TypeBool },
    { "GetBackgroundColor", notsupported, "#ffffff", KParts::LiveConnectExtension::TypeString },
    { "GetBandwidthAverage", notsupported, "64", KParts::LiveConnectExtension::TypeNumber },
    { "GetBandwidthCurrent", notsupported, "64", KParts::LiveConnectExtension::TypeNumber },
    { "GetBufferingTimeElapsed", notsupported, "0", KParts::LiveConnectExtension::TypeNumber },
    { "GetBufferingTimeRemaining", notsupported, "0", KParts::LiveConnectExtension::TypeNumber },
    { "GetCanSeek", canseek, nullptr, KParts::LiveConnectExtension::TypeBool },
    { "GetCenter", notsupported, "true", KParts::LiveConnectExtension::TypeBool },
    { "GetClipHeight", height, nullptr, KParts::LiveConnectExtension::TypeNumber },
    { "GetClipWidth", width, nullptr, KParts::LiveConnectExtension::TypeNumber },
    { "GetConnectionBandwidth", notsupported, "64", KParts::LiveConnectExtension::TypeNumber },
    { "GetConsole", notsupported, "unknown", KParts::LiveConnectExtension::TypeString },
    { "GetConsoleEvents", notsupported, "false", KParts::LiveConnectExtension::TypeBool },
    { "GetControls", notsupported, "buttons", KParts::LiveConnectExtension::TypeString },
    { "GetCopyright", notsupported, "(c) whoever", KParts::LiveConnectExtension::TypeString },
    { "GetCurrentEntry", notsupported, "1", KParts::LiveConnectExtension::TypeNumber },
    { "GetDuration", notsupported, "0", KParts::LiveConnectExtension::TypeNumber },
    { "GetDRMInfo", notsupported, "RNBA", KParts::LiveConnectExtension::TypeString },
    { "GetDoubleSize", notsupported, "false", KParts::LiveConnectExtension::TypeBool },
    { "GetEntryAbstract", notsupported, "abstract", KParts::LiveConnectExtension::TypeString },
    { "GetEntryAuthor", notsupported, "noname", KParts::LiveConnectExtension::TypeString },
    { "GetEntryCopyright", notsupported, "(c)", KParts::LiveConnectExtension::TypeString },
    { "GetEntryTitle", notsupported, "title", KParts::LiveConnectExtension::TypeString },
    { "GetFullScreen", isfullscreen, nullptr, KParts::LiveConnectExtension::TypeBool },
    { "GetImageStatus", notsupported, "false", KParts::LiveConnectExtension::TypeBool },
    { "GetLastErrorMoreInfoURL", notsupported, "no error", KParts::LiveConnectExtension::TypeString },
    { "GetLastErrorRMACode", notsupported, "0", KParts::LiveConnectExtension::TypeNumber },
    { "GetLastErrorSeverity", notsupported, "6", KParts::LiveConnectExtension::TypeNumber },
    { "GetLastErrorUserCode", notsupported, "0", KParts::LiveConnectExtension::TypeNumber },
    { "GetLastErrorUserString", notsupported, "no error", KParts::LiveConnectExtension::TypeString },
    { "GetLastMessage", notsupported, "no error", KParts::LiveConnectExtension::TypeString },
    { "GetLastStatus", notsupported, "no error", KParts::LiveConnectExtension::TypeString },
    { "GetLength", length, nullptr, KParts::LiveConnectExtension::TypeNumber },
    { "GetLiveState", notsupported, "false", KParts::LiveConnectExtension::TypeBool },
    { "GetLoop", isloop, nullptr, KParts::LiveConnectExtension::TypeBool },
    { "GetMaintainAspect", isaspect, nullptr, KParts::LiveConnectExtension::TypeBool },
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
    { "GetPlayState", playstate, nullptr, KParts::LiveConnectExtension::TypeNumber },
    { "GetPluginStatus", prop_qt_status, nullptr, KParts::LiveConnectExtension::TypeString },
    { "GetPosition", position, nullptr, KParts::LiveConnectExtension::TypeNumber },
    { "GetPreFetch", notsupported, "false", KParts::LiveConnectExtension::TypeBool },
    { "GetRate", prop_qt_rate, nullptr, KParts::LiveConnectExtension::TypeNumber },
    { "GetShowAbout", notsupported, "false", KParts::LiveConnectExtension::TypeBool },
    { "GetShowPreferences", notsupported, "false", KParts::LiveConnectExtension::TypeBool },
    { "GetShowStatistics", notsupported, "false", KParts::LiveConnectExtension::TypeBool },
    { "GetShuffle", notsupported, "false", KParts::LiveConnectExtension::TypeBool },
    { "GetSource", source, nullptr, KParts::LiveConnectExtension::TypeString },
    { "GetSourceTransport", protocol, nullptr, KParts::LiveConnectExtension::TypeString },
    { "GetStereoState", notsupported, "true", KParts::LiveConnectExtension::TypeBool },
    { "GetTitle", notsupported, "title", KParts::LiveConnectExtension::TypeString },
    { "GetVersionInfo", notsupported, "version", KParts::LiveConnectExtension::TypeString },
    { "GetVolume", volume, "100", KParts::LiveConnectExtension::TypeNumber },
    { "GetWantErrors", notsupported, "false", KParts::LiveConnectExtension::TypeBool },
    { "GetWantKeyboardEvents", notsupported, "false", KParts::LiveConnectExtension::TypeBool },
    { "GetWantMouseEvents", notsupported, "false", KParts::LiveConnectExtension::TypeBool },
    { "HasNextEntry", notsupported, "false", KParts::LiveConnectExtension::TypeBool },
    { "Pause", jsc_pause, nullptr, KParts::LiveConnectExtension::TypeBool },
    { "Play", play, nullptr, KParts::LiveConnectExtension::TypeBool },
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
    { "SetFileName", setsource, nullptr, KParts::LiveConnectExtension::TypeBool },
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
    { "SetSource", setsource, nullptr, KParts::LiveConnectExtension::TypeBool },
    { "SetTitle", notsupported, "true", KParts::LiveConnectExtension::TypeBool },
    { "SetVolume", setvolume, "true", KParts::LiveConnectExtension::TypeBool },
    { "SetWantErrors", notsupported, "true", KParts::LiveConnectExtension::TypeBool },
    { "SetWantKeyboardEvents", notsupported, "true", KParts::LiveConnectExtension::TypeBool },
    { "SetWantMouseEvents", notsupported, "true", KParts::LiveConnectExtension::TypeBool },
    { "ShowControls", showcontrolpanel, "true", KParts::LiveConnectExtension::TypeBool },
    { "Start", start, nullptr, KParts::LiveConnectExtension::TypeBool },
    { "Stop", stop, nullptr, KParts::LiveConnectExtension::TypeBool },
    { "Volume", prop_volume, "100", KParts::LiveConnectExtension::TypeNumber },
    { "errorCode", prop_error, "0",KParts::LiveConnectExtension::TypeNumber },
    { "pause", jsc_pause, nullptr, KParts::LiveConnectExtension::TypeBool },
    { "play", play, nullptr, KParts::LiveConnectExtension::TypeBool },
    { "put", prop_source, nullptr, KParts::LiveConnectExtension::TypeString },
    { "stop", stop, nullptr, KParts::LiveConnectExtension::TypeBool },
    { "volume", volume, nullptr, KParts::LiveConnectExtension::TypeBool },
};

static const JSCommandEntry * getJSCommandEntry (const char * name, int start = 0, int end = sizeof (JSCommandList)/sizeof (JSCommandEntry)) {
    if (end - start < 2) {
        if (start != end && !strcasecmp (JSCommandList[start].name, name))
            return &JSCommandList[start];
        return nullptr;
    }
    int mid = (start + end) / 2;
    int cmp = strcasecmp (JSCommandList[mid].name, name);
    if (cmp < 0)
        return getJSCommandEntry (name, mid + 1, end);
    if (cmp > 0)
        return getJSCommandEntry (name, start, mid);
    return &JSCommandList[mid];
}

KMPlayerLiveConnectExtension::KMPlayerLiveConnectExtension (KMPlayerPart * parent)
  : KParts::LiveConnectExtension (parent), player (parent),
    lastJSCommandEntry (nullptr),
    object_counter (0),
    m_started (false),
    m_enablefinish (false),
    m_evaluating (false),
    m_skip_put (false) {
      connect (parent, SIGNAL (started (KIO::Job *)), this, SLOT (started ()));
}

KMPlayerLiveConnectExtension::~KMPlayerLiveConnectExtension() {
    qCDebug(LOG_KMPLAYER_PART) << "KMPlayerLiveConnectExtension::~KMPlayerLiveConnectExtension()";
}

void KMPlayerLiveConnectExtension::started () {
    m_started = true;
}

void KMPlayerLiveConnectExtension::finished () {
    if (m_started && m_enablefinish) {
        KParts::LiveConnectExtension::ArgList args;
        args.push_back (qMakePair (KParts::LiveConnectExtension::TypeString, QString("if (window.onFinished) onFinished();")));
        Q_EMIT partEvent (0, "eval", args);
        m_started = true;
        m_enablefinish = false;
    }
}

QString KMPlayerLiveConnectExtension::evaluate (const QString &script) {
    KParts::LiveConnectExtension::ArgList args;
    args.push_back(qMakePair(KParts::LiveConnectExtension::TypeString, script));
    script_result.clear ();
    Q_EMIT partEvent (0, "eval", args);
    //qCDebug(LOG_KMPLAYER_PART) << script << script_result;
    return script_result;
}

void KMPlayerLiveConnectExtension::evaluate (
        const QString & scr, bool store, QString & result) {
    m_evaluating = true;

    QString script (scr);
    script = script.replace ('\\', "\\\\");
    script = script.replace ('\n', "\\n");
    script = script.replace ('\r', "");
    script = script.replace ('"', "\\\"");
    QString obj_var = QString ("this.__kmplayer__obj_%1").arg (object_counter);
    script = obj_var + QString ("=eval(\"%1\")").arg (script);
    QString eval_result = evaluate (script);

    bool clear_result = true;
    if (!store) {
        result = eval_result;
        if (scr.startsWith ("this.__kmplayer__obj_")) {
            // TODO add dbus method for this
            int p = scr.indexOf ("=null", 21);
            if (p > -1) {
                int i = scr.mid (21, p - 21).toInt ();
                if (i == (int)object_counter-1)
                    object_counter--; // works most of the time :-)
            }
        }
    } else {
        script = QString ("this.__kmplayer__res=typeof(%1)").arg (obj_var);
        QString result_type = evaluate (script);

        if (result_type == "string") {
            result = QString ("s:") + eval_result;
        } else if (result_type == "object" ||
                result_type == "function" ||
                result_type.startsWith ("[")) {
            result = QString ("o:") + obj_var;
            clear_result = false;
            object_counter++;
        } else if (result_type == "number") {
            result = QString ("n:") + eval_result;
        } else if (result_type == "boolean") {
            result = QString ("b:") + eval_result;
        } else if (result_type == "undefined" || result_type == "null") {
            result = QString ("u:") + eval_result;
        } else {
            result = "error";
        }
    }
    if (clear_result)
        evaluate (obj_var + "=null");

    script_result.clear ();

    m_evaluating = false;
}

static
bool str2LC (const QString s, KParts::LiveConnectExtension::Type &type, QString &rval) {
    if (s == "error")
        return false;
    if (s == "o:function") {
        type = KParts::LiveConnectExtension::TypeFunction;
    } else if (s.startsWith (QChar ('\'')) && s.endsWith (QChar ('\''))) {
        type = KParts::LiveConnectExtension::TypeString;
        rval = s.mid (1, s.size () - 2);
    } else if (s == "true" || s == "false") {
        type = KParts::LiveConnectExtension::TypeBool;
        rval = s;
    } else {
        bool ok;
        s.toInt (&ok);
        if (!ok)
            s.toDouble (&ok);
        if (ok) {
            type = KParts::LiveConnectExtension::TypeNumber;
            rval = s;
        } else {
            type = KParts::LiveConnectExtension::TypeVoid;
            rval = s;
        }
    }
    return true;
}

bool KMPlayerLiveConnectExtension::get
  (const unsigned long id, const QString & name,
   KParts::LiveConnectExtension::Type & type,
   unsigned long & rid, QString & rval)
{
    if (name.startsWith ("__kmplayer__obj_")) {
        if (m_evaluating)
            return false;
        rid = 0;
        type = KParts::LiveConnectExtension::TypeString;
        rval = "Access denied";
        return true;
    }
    if (name.startsWith ("__kmplayer_func")) {
        rid = id;
        type = KParts::LiveConnectExtension::TypeFunction;
        return true;
    }
    if (name.startsWith ("__kmplayer_util_") ||
            redir_funcs.indexOf(name) >= 0)
        return false;
    if (name == "__kmplayer_unique_name") {
        rval = QString ("__kmplayer__obj_%1").arg (object_counter);
        type = KParts::LiveConnectExtension::TypeString;
        rid = id;
        object_counter++;
        m_allow = rval;
        return true;
    }
    rid = id;
    QString req_result;
    Q_EMIT requestGet (id, name, &req_result);
    if (!req_result.isEmpty ()) {
        if (str2LC (req_result, type, rval)) {
            if (KParts::LiveConnectExtension::TypeFunction == type) {
                m_skip_put = true;
                if (!redir_funcs.size ())
                    evaluate (
                            "this.__kmplayer_util_make_arg = function(arg) {"
                            "  var t = typeof arg;"
                            "  if (t == 'number')"
                            "    return 'n:' + arg;"
                            "  if (t == 'object') {"
                            "    var s = this.__kmplayer_unique_name;"
                            "    this[s] = arg;"
                            "    return 'o:this.' + s;"
                            "  }"
                            "  if (t == 'function') {"
                            "    var s = this.__kmplayer_unique_name;"
                            "    this[s] = arg;"
                            "    return 'o:this.' + s;"
                            "  }"
                            "  if (t == 'boolean')"
                            "    return 'b:' + arg;"
                            "  if (t == 'undefined' || t == null)"
                            "    return 'u:' + arg;"
                            "  var s = '' + arg;"
                            "  s = s.replace('\\\\', '\\\\\\\\');"
                            "  s = s.replace('\\n', '\\\\n');"
                            "  return 's:' + s;"
                            "}");
                evaluate (QString (
                            "this.%1=function(){"
                            "  var args=[];"
                            "  for (var i=0;i<arguments.length;++i)"
                            "      args.push (this.__kmplayer_util_make_arg("
                            "                                   arguments[i]));"
                            "  return this.__kmplayer_func('%2',args.join('\\n'));"
                            "}")
                        .arg (name)
                        .arg (name));
                redir_funcs.push_back (name);
                m_skip_put = false;
                return false;
            }
            return true;
        }
    }
    qCDebug(LOG_KMPLAYER_PART) << "[01;35mget[00m " << name;
    const JSCommandEntry * entry = getJSCommandEntry (name.toLatin1 ().constData ());
    if (!entry)
        return false;
    type = entry->rettype;
    switch (entry->command) {
        case prop_source:
            type = KParts::LiveConnectExtension::TypeString;
            rval = player->url ().url ();
            break;
        case prop_volume:
            if (player->view ())
                rval = QString::number (player->viewWidget ()->controlPanel()->volumeBar()->value());
            break;
        case prop_error:
            type = KParts::LiveConnectExtension::TypeNumber;
            rval = QString::number (0);
            break;
        case prop_qt_status:
            rval = player->getStatus ();
            break;
        case prop_qt_rate:
            rval = QString::number (0.0);
            if (player->source() && player->source()->document() &&
                    player->source()->document()->state !=
                        KMPlayer::Node::state_deferred &&
                    player->source()->document()->unfinished ())
                rval = QString::number (1.0);
            break;
        default:
            lastJSCommandEntry = entry;
            type = KParts::LiveConnectExtension::TypeFunction;
    }
    return true;
}

bool KMPlayerLiveConnectExtension::put
  (const unsigned long, const QString & name, const QString & val) {
    if (m_skip_put)
        return false;
    if (name == "__kmplayer__res") {
        script_result = val;
        return true;
    }
    if (name.startsWith ("__kmplayer__obj_")) {
        script_result = val;
        if (name == m_allow) {
            m_allow.clear ();
            return false;
        }
        return !m_evaluating;
    }

    qCDebug(LOG_KMPLAYER_PART) << "[01;35mput[00m " << name << "=" << val;

    const JSCommandEntry * entry = getJSCommandEntry (name.toLatin1 ().constData ());
    if (!entry)
        return false;
    switch (entry->command) {
        case prop_source: {
            const QUrl url = QUrl::fromUserInput(val);
            if (player->allowRedir (url))
                player->openNewURL (url);
            break;
        }
        case prop_volume:
            if (player->view ())
                player->viewWidget ()->controlPanel()->volumeBar()->setValue(val.toInt ());
            break;
        default:
            return false;
    }
    return true;
}

static QString unescapeArg (const QString &arg) {
    QString s;
    bool last_escape = false;
    for (int i = 0; i < arg.length (); ++i)
        switch (arg[i].unicode ()) {
        case '\\':
            if (last_escape) {
                s += QChar ('\\');
                last_escape = false;
            } else {
                last_escape = true;
            }
            break;
        case 'n':
            if (last_escape) {
                s += QChar ('\n');
                last_escape = false;
                break;
            } // else fall through
        default:
            if (last_escape) {
                qCCritical(LOG_KMPLAYER_PART) << "unescape error " << arg;
                last_escape = false;
            }
            s += arg[i];
        }
    return s;
}

bool KMPlayerLiveConnectExtension::call
  (const unsigned long id, const QString & name,
   const QStringList & args, KParts::LiveConnectExtension::Type & type,
   unsigned long & rid, QString & rval) {
    QString func = name;
    QString req_result;
    QStringList arglst;
    int oid = id;
    QList<QString>::const_iterator it = args.begin ();
    if (func == "__kmplayer_func") {
        if ( it != args.end ()) {
            func = *it;
            oid = 0;
            if ( ++it != args.end ()) {
                QStringList a = (*it).split ("\n");
                for (QStringList::iterator i = a.begin(); i != a.end (); ++i)
                    arglst << ((*i).startsWith ("s:") ? unescapeArg (*i) : *i);
            }
        } else {
            return false;
        }
    } else {
        for (; it != args.end (); ++it) {
            bool ok;
            int iv = (*it).toInt (&ok);
            if (ok) {
                arglst << QString ("n:%1").arg (iv);
            } else {
                double dv = (*it).toDouble (&ok);
                if (ok) {
                    arglst << QString ("n:%1").arg (dv);
                } else {
                    arglst << QString ("s:%1").arg (*it);
                }
            }
        }
    }
    rid = oid;
    Q_EMIT requestCall (oid, func, arglst, &req_result);
    if (!req_result.isEmpty ()) {
        if (str2LC (req_result, type, rval))
            return true;
    }
    qCDebug(LOG_KMPLAYER_PART) << "[01;35mentry[00m " << func;
    const JSCommandEntry * entry = lastJSCommandEntry;
    const QByteArray latin1 = func.toLatin1 ();
    if (!entry || strcmp (entry->name, latin1.constData ()))
        entry = getJSCommandEntry (latin1.constData ());
    if (!entry)
        return false;
    for (QStringList::size_type i = 0; i < args.size (); ++i)
        qCDebug(LOG_KMPLAYER_PART) << "      " << args[i];
    if (!player->view ())
        return false;
    type = entry->rettype;
    switch (entry->command) {
        case notsupported:
            if (entry->rettype != KParts::LiveConnectExtension::TypeVoid)
                rval = entry->defaultvalue;
            break;
        case canpause:
            rval = (player->playing () && !player->viewWidget ()->controlPanel()->button (KMPlayer::ControlPanel::button_pause)->isChecked ()) ? "true" : "false";
            break;
        case canplay:
            rval = (!player->playing () || player->viewWidget ()->controlPanel()->button (KMPlayer::ControlPanel::button_pause)->isChecked ()) ? "true" : "false";
            break;
        case canstop:
            rval = player->playing () ? "true" : "false";
            break;
        case canseek:
            rval = player->source ()->isSeekable () ? "true" : "false";
            break;
        case play:
            if (args.size ()) {
                const QUrl url = QUrl::fromUserInput(args.first ());
                if (player->allowRedir (url))
                    player->openNewURL (url);
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
                player->viewWidget ()->setControlPanelMode (KMPlayer::View::CP_Hide);
            else
                player->viewWidget ()->setControlPanelMode (KMPlayer::View::CP_Show);
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
            rval = player->viewWidget ()->isFullScreen () ? "true" : "false";
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
            rval = player->playing () ? "3" : "0";
            break;
        case position:
            rval.setNum (player->position ());
            break;
        case protocol:
            rval = player->url ().scheme();
            break;
        case setsource:
            rval ="false";
            if (args.size ()) {
                const QUrl url = QUrl::fromUserInput (args.first ());
                if (player->allowRedir (url) && player->openNewURL (url))
                    rval = "true";
            }
            break;
        case setvolume:
            if (!args.size ())
                return false;
            player->viewWidget ()->controlPanel()->volumeBar()->setValue(args.first ().toInt ());
            rval = "true";
            break;
        case source:
            rval = player->url ().url ();
            break;
        case volume:
            if (player->view ())
                rval = QString::number (player->viewWidget ()->controlPanel()->volumeBar()->value());
            break;
        default:
            return false;
    }
    return true;
}

void KMPlayerLiveConnectExtension::unregister (const unsigned long) {
}

void KMPlayerLiveConnectExtension::setSize (int w, int h) {
    KMPlayer::View * view = static_cast <KMPlayer::View*> (player->view ());
    if (view->controlPanelMode () == KMPlayer::View::CP_Show)
        h += view->controlPanel()->height();
    QString jscode;
    jscode.sprintf("try { eval(\"this.setAttribute('WIDTH',%d);this.setAttribute('HEIGHT',%d)\"); } catch(e){}", w, h);
    KParts::LiveConnectExtension::ArgList args;
    args.push_back (qMakePair (KParts::LiveConnectExtension::TypeString, jscode));
    Q_EMIT partEvent (0, "eval", args);
}
