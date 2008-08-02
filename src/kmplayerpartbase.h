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
class KActionCollection;
class KBookmarkMenu;
class KBookmarkManager;
class QTextStream;
class Q3ListViewItem;
class KMenu;
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
class Settings;
class MediaManager;

/*
 * Source from URLs
 */
class KMPLAYER_EXPORT URLSource : public Source {
    Q_OBJECT
public:
    URLSource (PartBase * player, const KUrl & url = KUrl ());
    virtual ~URLSource ();

    virtual void dimensions (int & w, int & h);
    virtual bool hasLength ();
    virtual QString prettyName ();
    virtual void reset ();
    virtual void setUrl (const QString &url);
    virtual bool authoriseUrl (const QString &url);
public slots:
    virtual void init ();
    virtual void activate ();
    virtual void deactivate ();
    virtual void forward ();
    virtual void backward ();
    virtual void play (Mrl *);
private:
    bool activated; // 'solve' an singleShot race w/ cmdline url's
};

/*
 * KDE's KMediaPlayer::Player implementation and base for KMPlayerPart
 */
class KMPLAYER_EXPORT PartBase : public KMediaPlayer::Player {
    Q_OBJECT
public:
    PartBase (QWidget *parent, QObject *objParent, KSharedConfigPtr);
    ~PartBase ();
    void init (KActionCollection * = 0L);
    virtual KMediaPlayer::View* view ();
    View* viewWidget () const { return m_view; }
    static KAboutData* createAboutData ();

    Settings * settings () const { return m_settings; }
    void keepMovieAspect (bool);
    KUrl url () const { return m_sources ["urlsource"]->url (); }
    void setUrl (const QString &url) { m_sources ["urlsource"]->setUrl (url); }
    KUrl docBase () const { return m_docbase; }

    /* Changes the backend process */
    QString processName (Mrl *mrl);

    /* Changes the source,
     * calls init() and reschedules an activate() on the source
     * */
    void setSource (Source * source);
    void createBookmarkMenu (KMenu *owner, KActionCollection *ac);
    void connectPanel (ControlPanel * panel);
    void connectPlaylist (PlayListView * playlist);
    void connectInfoPanel (InfoWindow * infopanel);
    void connectSource (Source * old_source, Source * source);
    MediaManager *mediaManager () const { return m_media_manager; }
    Source * source () const { return m_source; }
    QMap <QString, Source *> & sources () { return m_sources; }
    KSharedConfigPtr config () const { return m_config; }
    bool mayResize () const { return !m_noresize; }
    void updatePlayerMenu (ControlPanel *, const QString &backend=QString ());
    void updateInfo (const QString & msg);
    void updateStatus (const QString & msg);
#ifdef KMPLAYER_WITH_DBUS
    void setServiceName (const QString & srv) { m_service = srv; }
    QString serviceName () const { return m_service; }
#endif

    // these are called from Process
    void changeURL (const QString & url);
    void updateTree (bool full=true, bool force=false);
    void setLanguages (const QStringList & alang, const QStringList & slang);
    void startRecording ();
    void stopRecording ();
public slots:
    virtual bool openUrl (const KUrl & url);
    virtual bool openUrl (const KUrl::List & urls);
    virtual void openUrl (const KUrl &, const QString &t, const QString &srv);
    virtual bool closeUrl ();
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
    virtual void processCreated (Process *);
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
    void recording (bool);
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
    void settingsChanged ();
    void audioSelected (int);
    void subtitleSelected (int);
protected:
    KUrl m_docbase;
    KSharedConfigPtr m_config;
    QPointer <View> m_view;
    QMap <QString, QString> temp_backends;
    Settings *m_settings;
    MediaManager *m_media_manager;
    Source * m_source;
    QMap <QString, Source *> m_sources;
    KBookmarkManager * m_bookmark_manager;
    BookmarkOwner * m_bookmark_owner;
    KBookmarkMenu * m_bookmark_menu;
#ifdef KMPLAYER_WITH_DBUS
    QString m_service;
#endif
    int m_update_tree_timer;
    bool m_noresize : 1;
    bool m_auto_controls : 1;
    bool m_use_slave : 1;
    bool m_bPosSliderPressed : 1;
    bool m_in_update_tree : 1;
    bool m_update_tree_full : 1;
};

} // namespace

#endif
