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

#ifndef KMPLAYERPARTBASE_H
#define KMPLAYERPARTBASE_H

#include "config-kmplayer.h"

#include "kmplayer_def.h"

#include <qobject.h>
#include <QPointer>
#include <qstringlist.h>
#include <qmap.h>

#include <kmediaplayer/player.h>
#include <kurl.h>
#include <kurl.h>

#include "kmplayerview.h"
#include "kmplayersource.h"


class KAboutData;
class KInstance;
class KActionCollection;
class KBookmarkMenu;
class KBookmarkManager;
class QIODevice;
class QTextStream;
class Q3ListViewItem;
class KJob;
class KSharedConfig;
template<class T> class KSharedPtr;
typedef KSharedPtr<KSharedConfig> KSharedConfigPtr;

namespace KIO {
    class Job;
}

namespace KMPlayer {

class PartBase;
class Process;
class MPlayer;
class BookmarkOwner;
class MEncoder;
class MPlayerDumpstream;
class FFMpeg;
class Xine;
class Settings;

/*
 * Source from URLs
 */
class KMPLAYER_EXPORT URLSource : public Source {
    Q_OBJECT
public:
    URLSource (PartBase * player, const KURL & url = KURL ());
    virtual ~URLSource ();

    virtual void dimensions (int & w, int & h);
    virtual bool hasLength ();
    virtual QString prettyName ();
    virtual void reset ();
    virtual void setURL (const KURL & url);
public slots:
    virtual void init ();
    virtual void activate ();
    virtual void deactivate ();
    virtual void playCurrent ();
    virtual void forward ();
    virtual void backward ();
    virtual void jump (NodePtr e);
    void play ();
private slots:
    void kioData (KIO::Job *, const QByteArray &);
    void kioMimetype (KIO::Job *, const QString &);
    void kioResult (KJob *);
protected:
    virtual bool requestPlayURL (NodePtr mrl);
    virtual bool resolveURL (NodePtr mrl);
private:
    void read (NodePtr mrl, QTextStream &);
    void stopResolving ();
    struct ResolveInfo {
        ResolveInfo (NodePtr mrl, KIO::Job * j, SharedPtr <ResolveInfo> & n)
            : resolving_mrl (mrl), job (j), progress (0), next (n) {}
        NodePtrW resolving_mrl;
        KIO::Job * job;
        QByteArray data;
        int progress;
        SharedPtr <ResolveInfo> next;
    };
    SharedPtr <ResolveInfo> m_resolve_info;
    bool activated; // 'solve' an singleShot race w/ cmdline url's
};

/*
 * KDE's KMediaPlayer::Player implementation and base for KMPlayerPart
 */
class KMPLAYER_EXPORT PartBase : public KMediaPlayer::Player {
    Q_OBJECT
public:
    typedef QMap <QString, Process *> ProcessMap;
    PartBase (QWidget *parent, QObject *objParent, KSharedConfigPtr);
    ~PartBase ();
    void init (KActionCollection * = 0L);
    virtual KMediaPlayer::View* view ();
    static KAboutData* createAboutData ();

    Settings * settings () const { return m_settings; }
    void keepMovieAspect (bool);
    KURL url () const { return m_sources ["urlsource"]->url (); }
    void setURL (const KURL & url) { m_sources ["urlsource"]->setURL (url); }

    /* Changes the backend process */
    void setProcess (const char *);
    bool setProcess (Mrl *mrl);
    void setRecorder (const char *);

    /* Changes the source,
     * calls init() and reschedules an activate() on the source
     * */
    void setSource (Source * source);
    void connectPanel (ControlPanel * panel);
    void connectPlaylist (PlayListView * playlist);
    void connectInfoPanel (InfoWindow * infopanel);
    void connectSource (Source * old_source, Source * source);
    Process * process () const { return m_process; }
    Process * recorder () const { return m_recorder; }
    Source * source () const { return m_source; }
    QMap <QString, Process *> & players () { return m_players; }
    QMap <QString, Process *> & recorders () { return m_recorders; }
    QMap <QString, Source *> & sources () { return m_sources; }
    KSharedConfigPtr config () const { return m_config; }
    bool mayResize () const { return !m_noresize; }
    void updatePlayerMenu (ControlPanel *);
    void updateInfo (const QString & msg);
    void updateStatus (const QString & msg);
#ifdef HAVE_DBUS
    void setServiceName (const QString & srv) { m_service = srv; }
    QString serviceName () const { return m_service; }
#endif

    // these are called from Process
    void changeURL (const QString & url);
    void updateTree (bool full=true, bool force=false);
    void setLanguages (const QStringList & alang, const QStringList & slang);
public slots:
    virtual bool openURL (const KURL & url);
    virtual bool openURL (const KURL::List & urls);
    virtual bool closeURL ();
    virtual void pause (void);
    virtual void play (void);
    virtual void stop (void);
    void record ();
    void adjustVolume (int incdec);
    bool playing () const;
    void showConfigDialog ();
    void showPlayListWindow ();
    void slotPlayerMenu (int);
    void back ();
    void forward ();
    void addBookMark (const QString & title, const QString & url);
    void volumeChanged (int);
    void increaseVolume ();
    void decreaseVolume ();
    void setPosition (int position, int length);
    virtual void setLoaded (int percentage);
public:
    virtual bool isSeekable (void) const;
    virtual qlonglong position (void) const;
    virtual bool hasLength (void) const;
    virtual qlonglong length (void) const;
    virtual void seek (qlonglong);
    void toggleFullScreen ();
    bool isPlaying ();
signals:
    void sourceChanged (KMPlayer::Source * old, KMPlayer::Source * nw);
    void sourceDimensionChanged ();
    void loading (int percentage);
    void urlAdded (const QString & url);
    void urlChanged (const QString & url);
    void processChanged (const char *);
    void treeChanged (int id, NodePtr root, NodePtr, bool select, bool open);
    void treeUpdated ();
    void infoUpdated (const QString & msg);
    void statusUpdated (const QString & msg);
    void languagesUpdated(const QStringList & alang, const QStringList & slang);
    void audioIsSelected (int id);
    void subtitleIsSelected (int id);
    void positioned (int pos, int length);
protected:
    bool openFile();
    virtual void timerEvent (QTimerEvent *);
protected slots:
    void posSliderPressed ();
    void posSliderReleased ();
    void positionValueChanged (int val);
    void contrastValueChanged (int val);
    void brightnessValueChanged (int val);
    void hueValueChanged (int val);
    void saturationValueChanged (int val);
    void sourceHasChangedAspects ();
    void fullScreen ();
    void playListItemClicked (Q3ListViewItem *);
    void playListItemExecuted (Q3ListViewItem *);
    virtual void playingStarted ();
    virtual void playingStopped ();
    void recordingStarted ();
    void recordingStopped ();
    void settingsChanged ();
    void audioSelected (int);
    void subtitleSelected (int);
protected:
    KSharedConfigPtr m_config;
    QPointer <View> m_view;
    QMap <QString, QString> temp_backends;
    Settings * m_settings;
    Process * m_process;
    Process * m_recorder;
    Source * m_source;
    ProcessMap m_players;
    ProcessMap m_recorders;
    QMap <QString, Source *> m_sources;
    KBookmarkManager * m_bookmark_manager;
    BookmarkOwner * m_bookmark_owner;
    KBookmarkMenu * m_bookmark_menu;
#ifdef HAVE_DBUS
    QString m_service;
#endif
    int m_record_timer;
    int m_update_tree_timer;
    bool m_noresize : 1;
    bool m_auto_controls : 1;
    bool m_use_slave : 1;
    bool m_bPosSliderPressed : 1;
    bool m_in_update_tree : 1;
    bool m_update_tree_full : 1;
};

class KMPLAYER_NO_EXPORT DataCache : public QObject, public GlobalShared<DataCache> {
    Q_OBJECT
    typedef QMap <QString, QByteArray> DataMap;
    typedef QMap <QString, bool> PreserveMap;
    DataMap cache_map;
    PreserveMap preserve_map;
public:
    DataCache (DataCache **glob) : QObject (NULL), GlobalShared<DataCache> (glob) {}
    ~DataCache () {}

    void add (const QString &, const QByteArray &);
    bool get (const QString &, QByteArray &);
    bool preserve (const QString &);
    bool unpreserve (const QString &);
    bool isPreserved (const QString &);
signals:
    void preserveRemoved (const QString &); // ready or canceled
private:
    DataCache (const DataCache &);
};

class KMPLAYER_NO_EXPORT RemoteObjectPrivate : public QObject {
    Q_OBJECT
public:
    RemoteObjectPrivate (RemoteObject * r);
    ~RemoteObjectPrivate ();
    bool download (const QString &);
    void clear ();
    KIO::Job * job;
    QString url;
    QByteArray data;
    QString mime;
private slots:
    void slotResult (KJob*);
    void slotData (KIO::Job*, const QByteArray& qb);
    void slotMimetype (KIO::Job * job, const QString & mimestr);
    void cachePreserveRemoved (const QString &);
private:
    RemoteObject * remote_object;
    bool preserve_wait;
};

} // namespace

#endif
