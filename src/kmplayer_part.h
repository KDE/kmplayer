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


class KMPlayerPart;
class JSCommandEntry;

/*
 * Wrapper source for URLSource that has a HREF attribute
 */
class KMPLAYER_NO_EXPORT KMPlayerHRefSource : public KMPlayer::Source {
    Q_OBJECT
public:
    KMPlayerHRefSource (KMPlayer::PartBase * player);
    virtual ~KMPlayerHRefSource ();
    virtual bool processOutput (const QString & line);
    virtual bool hasLength ();

    void setUrl (const KUrl &);
    void clear ();
    virtual QString prettyName ();
public slots:
    virtual void init ();
    virtual void activate ();
    virtual void deactivate ();
    void finished ();
private slots:
    void grabReady (const QString & path);
    void play ();
private:
    QString m_grabfile;
    bool m_finished;
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
public slots:
    void slotRequestOpenURL (const KUrl & url, const QString & target);
};

/*
 * Part javascript support
 */
class KMPLAYER_NO_EXPORT KMPlayerLiveConnectExtension : public KParts::LiveConnectExtension {
    Q_OBJECT
public:
    KMPlayerLiveConnectExtension (KMPlayerPart * parent);
    ~KMPlayerLiveConnectExtension ();

    // LiveConnect interface
    bool get (const unsigned long, const QString &,
            KParts::LiveConnectExtension::Type &, unsigned long &, QString &);
    bool put (const unsigned long, const QString &, const QString &);
    bool call (const unsigned long, const QString &,
            const QStringList &, KParts::LiveConnectExtension::Type &, 
            unsigned long &, QString &);
    void unregister (const unsigned long);
    void sendEvent(const unsigned long objid, const QString & event, const KParts::LiveConnectExtension::ArgList & args ) {
        emit partEvent(objid, event, args);
    }

    void enableFinishEvent (bool b = true) { m_enablefinish = b; }
signals:
    void partEvent (const unsigned long, const QString &,
                    const KParts::LiveConnectExtension::ArgList &);
public slots:
    void setSize (int w, int h);
    void started ();
    void finished ();
    void evaluate (const QString & script, QString & result);
private:
    KMPlayerPart * player;
    QString script_result;
    const JSCommandEntry * lastJSCommandEntry;
    bool m_started;
    bool m_enablefinish;
    bool m_evaluating;
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
    KMPlayerLiveConnectExtension * liveconnectextension () const
        { return m_liveconnectextension; }
    KDE_NO_EXPORT bool hasFeature (int f) { return m_features & f; }
    bool allowRedir (const KUrl & url) const;
    void connectToPart (KMPlayerPart *);
    KMPlayerPart * master () const { return m_master; }
    void setMaster (KMPlayerPart * m) { m_master = m; }
    virtual void setLoaded (int percentage);
    bool openNewURL (const KUrl & url); // for JS interface
public slots:
    virtual bool openUrl (const KUrl & url);
    virtual bool closeUrl ();
    void setMenuZoom (int id);
protected slots:
    virtual void playingStarted ();
    virtual void playingStopped ();
    void viewerPartDestroyed (QObject *);
    void viewerPartProcessChanged (const char *);
    void viewerPartSourceChanged (KMPlayer::Source *, KMPlayer::Source *);
    void waitForImageWindowTimeOut ();
    void statusPosition (int pos, int length);
private:
    void setAutoControls (bool);
    KMPlayerPart * m_master;
    KMPlayerBrowserExtension * m_browserextension;
    KMPlayerLiveConnectExtension * m_liveconnectextension;
    QString m_group;
    KUrl m_docbase;
    QString m_src_url;
    QString m_file_name;
    int m_features;
    int last_time_left;
    bool m_started_emited : 1;
    //bool m_noresize : 1;
    bool m_havehref : 1;
};


#endif
