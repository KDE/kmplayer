/*
    SPDX-FileCopyrightText: 2002-2003 Koos Vriezen <koos.vriezen@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#ifndef KMPLAYER_PART_H
#define KMPLAYER_PART_H

#include <kpluginfactory.h>
#include <kparts/browserextension.h>
#include <kparts/liveconnectextension.h>
#include <kcoreaddons_version.h>
#if KCOREADDONS_VERSION < QT_VERSION_CHECK(5, 77, 0)
#include <KAboutData>
#endif
#include "kmplayerpartbase.h"
#include "config-kmplayer.h"


class QLabel;
class KMPlayerPart;
class JSCommandEntry;
namespace KMPlayer {
    class PlayListNotify;
}

class KMPlayerFactory : public KPluginFactory {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.kde.KPluginFactory" FILE "kmplayer_part.json")
    Q_INTERFACES(KPluginFactory)
public:
    KMPlayerFactory();
    ~KMPlayerFactory() override;

    QObject* create(const char *iface, QWidget* parentWidget, QObject* parent,
            const QVariantList& args, const QString& keyword) Q_DECL_OVERRIDE;
#if KCOREADDONS_VERSION < QT_VERSION_CHECK(5, 77, 0)
    static KAboutData& aboutData();
private:
    static KAboutData* s_about;
#endif
};

/*
 * Document to get first frame for streams starting with a picture
 */
class GrabDocument : public KMPlayer::SourceDocument
{
public:
    GrabDocument (KMPlayerPart *part, const QString &url, const QString &file,
            KMPlayer::PlayListNotify *);

    void activate () override;
    void undefer () override;
    void begin () override;
    void message (KMPlayer::MessageType msg, void *content) override;

    QString m_grab_file;
    KMPlayerPart *m_part;
};

/*
 * Part notifications to hosting application
 */
class KMPlayerBrowserExtension : public KParts::BrowserExtension
{
    Q_OBJECT
public:
    KMPlayerBrowserExtension(KMPlayerPart *parent);
    ~KMPlayerBrowserExtension () override {}
    void urlChanged (const QString & url);
    void setLoadingProgress (int percentage);

    void saveState (QDataStream & stream) override;
    void restoreState (QDataStream & stream) override;
    void requestOpenURL (const QUrl & url, const QString & target, const QString & service);
};

/*
 * Part javascript support
 */
class KMPlayerLiveConnectExtension : public KParts::LiveConnectExtension
{
    Q_OBJECT
public:
    KMPlayerLiveConnectExtension (KMPlayerPart * parent);
    ~KMPlayerLiveConnectExtension () override;

    // LiveConnect interface
    bool get (const unsigned long, const QString &,
            KParts::LiveConnectExtension::Type &, unsigned long &, QString &) override;
    bool put (const unsigned long, const QString &, const QString &) override;
    bool call (const unsigned long, const QString &,
            const QStringList &, KParts::LiveConnectExtension::Type &, 
            unsigned long &, QString &) override;
    void unregister (const unsigned long) override;
    void sendEvent(const unsigned long objid, const QString & event, const KParts::LiveConnectExtension::ArgList & args ) {
        Q_EMIT partEvent(objid, event, args);
    }

    void enableFinishEvent (bool b = true) { m_enablefinish = b; }
    QString evaluate (const QString & script);
Q_SIGNALS:
    void partEvent (const unsigned long, const QString &,
                    const KParts::LiveConnectExtension::ArgList &);
    void requestGet (const uint32_t, const QString &, QString *);
    void requestCall (const uint32_t, const QString &, const QStringList &, QString *);
public Q_SLOTS:
    void setSize (int w, int h);
    void started ();
    void finished ();
    void evaluate (const QString & script, bool store, QString & result);
private:
    KMPlayerPart * player;
    QString script_result;
    QString m_allow;
    QStringList redir_funcs;
    const JSCommandEntry * lastJSCommandEntry;
    unsigned int object_counter;
    bool m_started;
    bool m_enablefinish;
    bool m_evaluating;
    bool m_skip_put;
};


/*
 * Part that gets created when used a KPart
 */
class KMPlayerPart : public KMPlayer::PartBase
{
    Q_OBJECT
    friend struct GroupPredicate;
public:
    enum Features {
        Feat_Unknown = 0,
        Feat_Viewer = 0x01, Feat_Controls = 0x02,
        Feat_Label = 0x04, Feat_StatusBar = 0x08,
        Feat_InfoPanel = 0x10, Feat_VolumeSlider = 0x20, Feat_PlayList = 0x40,
        Feat_ImageWindow = 0x80, Feat_All = 0xff
    };
#if KCOREADDONS_VERSION >= QT_VERSION_CHECK(5, 77, 0)
    KMPlayerPart(QWidget* parentWidget, QObject* parent, const KPluginMetaData& metaData, const QVariantList &args);
#else
    KMPlayerPart (QWidget *wparent, QObject *parent, const QVariantList &args);
#endif
    ~KMPlayerPart () override;

    KMPlayerBrowserExtension * browserextension() const
        { return m_browserextension; }
    KMPlayerLiveConnectExtension * liveconnectextension () const
        { return m_liveconnectextension; }
    bool hasFeature (int f) { return m_features & f; }
    bool allowRedir (const QUrl & url) const;
    void connectToPart (KMPlayerPart *);
    KMPlayerPart * master () const { return m_master; }
    void setMaster (KMPlayerPart * m) { m_master = m; }
    void setLoaded (int percentage) override;
    bool openNewURL (const QUrl & url); // for JS interface
    /// @param pic local file path to a placeholder image
    bool startUrl (const QUrl &url, const QString &pic=QString ());//clickToPlay

    QString doEvaluate (const QString &script) override;

    using KMPlayer::PartBase::openUrl;
public Q_SLOTS:
    bool openUrl(const QUrl& url) override;
    virtual void openUrl(const QUrl&, const QString& t, const QString& srv);
    bool closeUrl () override;
    void setMenuZoom (int id);
protected:
    void processCreated (KMPlayer::Process *) override;
    void playingStarted () override;
    void playingStopped () override;
protected Q_SLOTS:
    void viewerPartDestroyed (QObject *);
    void viewerPartProcessChanged (const char *);
    void viewerPartSourceChanged (KMPlayer::Source *, KMPlayer::Source *);
    void waitForImageWindowTimeOut ();
    void statusPosition (int pos, int length);
    void nppLoaded ();
#ifndef KMPLAYER_WITH_CAIRO
    void pictureClicked ();
#endif
private:
    void setAutoControls (bool);
    KMPlayerPart * m_master;
    KMPlayerBrowserExtension * m_browserextension;
    KMPlayerLiveConnectExtension * m_liveconnectextension;
    QLabel* m_playtime_info;
    QString m_group;
    QString m_src_url;
    QString m_href_url;
    QString m_file_name;
    QString m_grab_file;
    QString m_target;
    int m_expected_view_width;
    int m_expected_view_height;
    int m_features;
    int last_time_left;
    bool m_started_emited : 1;
    bool m_wait_npp_loaded : 1;
    //bool m_noresize : 1;
};


#endif
