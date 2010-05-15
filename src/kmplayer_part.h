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

#ifndef KMPLAYER_PART_H
#define KMPLAYER_PART_H

#include <kparts/browserextension.h>
#include "kmplayerpartbase.h"
#include "kmplayersource.h"
#include "config-kmplayer.h"


class KMPlayerPart;
class KMPlayerLiveConnectExtension;
class KMPlayerScriptableExtension;
namespace KMPlayer {
    class PlayListNotify;
}

/*
 * Document to get first frame for streams starting with a picture
 */
class KMPLAYER_NO_EXPORT GrabDocument : public KMPlayer::SourceDocument {
public:
    GrabDocument (KMPlayerPart *part, const QString &url, const QString &file,
            KMPlayer::PlayListNotify *);

    virtual void activate ();
    virtual void undefer ();
    virtual void begin ();
    virtual void message (KMPlayer::MessageType msg, void *content);

    QString m_grab_file;
    KMPlayerPart *m_part;
};

/*
 * Part notifications to hosting application
 */
class KMPLAYER_NO_EXPORT KMPlayerBrowserExtension : public KParts::BrowserExtension {
    Q_OBJECT
public:
    KMPlayerBrowserExtension(KMPlayerPart *parent);
    KDE_NO_CDTOR_EXPORT ~KMPlayerBrowserExtension () {}
    void urlChanged (const QString & url);
    void setLoadingProgress (int percentage);

    void saveState (QDataStream & stream);
    void restoreState (QDataStream & stream);
    void requestOpenURL (const KUrl & url, const QString & target, const QString & service);
};


/*
 * Part that gets created when used a KPart
 */
class KMPLAYER_NO_EXPORT KMPlayerPart : public KMPlayer::PartBase {
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
    KMPlayerPart (QWidget *wparent, QObject *parent, const QStringList &args);
    ~KMPlayerPart ();

    KDE_NO_EXPORT KMPlayerBrowserExtension * browserextension() const
        { return m_browserextension; }
#if KDE_IS_VERSION(4, 4, 75)
    KMPlayerScriptableExtension * scriptableextension () const
        { return m_scriptableextension; }
#else
    KMPlayerLiveConnectExtension * liveconnectextension () const
        { return m_liveconnectextension; }
#endif
    KDE_NO_EXPORT bool hasFeature (int f) { return m_features & f; }
    bool allowRedir (const KUrl & url) const;
    void connectToPart (KMPlayerPart *);
    KMPlayerPart * master () const { return m_master; }
    void setMaster (KMPlayerPart * m) { m_master = m; }
    virtual void setLoaded (int percentage);
    bool openNewURL (const KUrl & url); // for JS interface
    bool startUrl (const KUrl &url, const QString &pic=QString ());//clickToPlay

    virtual QString doEvaluate (const QString &script);

public slots:
    virtual bool openUrl (const KUrl & url);
    virtual void openUrl (const KUrl &, const QString &t, const QString &srv);
    virtual bool closeUrl ();
    void setMenuZoom (int id);
protected:
    virtual void processCreated (KMPlayer::Process *);
    virtual void playingStarted ();
    virtual void playingStopped ();
protected slots:
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
#if KDE_IS_VERSION(4, 4, 75)
    KMPlayerScriptableExtension * m_scriptableextension;
#else
    KMPlayerLiveConnectExtension * m_liveconnectextension;
#endif
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
