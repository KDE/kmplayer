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

#ifndef KMPLAYERPARTBASE_H
#define KMPLAYERPARTBASE_H

#include <qobject.h>
#include <qguardedptr.h>
#include <qvaluelist.h>
#include <qcstring.h>
#include <qmap.h>

#include <kmediaplayer/player.h>
#include <kurl.h>

#include "kmplayerview.h"
#include "kmplayersource.h"
#include "kmplayerprocess.h"


class KAboutData;
class KInstance;
class KActionCollection;
class KBookmarkMenu;
class KConfig;
class QIODevice;
class QTextStream;

namespace KIO {
    class Job;
}

namespace KMPlayer {
    
class PartBase;
class Process;
class MPlayer;
class BookmarkOwner;
class BookmarkManager;
class MEncoder;
class MPlayerDumpstream;
class FFMpeg;
class Xine;
class Settings;

class KMPLAYER_EXPORT URLSource : public Source {
    Q_OBJECT
public:
    URLSource (PartBase * player, const KURL & url = KURL ());
    virtual ~URLSource ();

    virtual bool hasLength ();
    virtual QString prettyName ();
public slots:
    virtual void init ();
    virtual void activate ();
    virtual void deactivate ();
    virtual void playCurrent ();
    void play ();
private slots:
    void kioData (KIO::Job *, const QByteArray &);
    void kioMimetype (KIO::Job *, const QString &);
    void kioResult (KIO::Job *);
private:
    void read (QTextStream &);
    void terminateJob ();
    KIO::Job * m_job;
    QByteArray m_data;
};


class KMPLAYER_EXPORT PartBase : public KMediaPlayer::Player {
    Q_OBJECT
public:
    typedef QMap <QString, Process *> ProcessMap;
    PartBase (QWidget * parent,  const char * wname,QObject * parent, const char * name, KConfig *);
    ~PartBase ();
    void init (KActionCollection * = 0L);
    virtual KMediaPlayer::View* view ();
    static KAboutData* createAboutData ();

    KDE_NO_EXPORT Settings * settings () const { return m_settings; }
    void keepMovieAspect (bool);
    KDE_NO_EXPORT KURL url () const { return m_sources ["urlsource"]->url (); }
    KDE_NO_EXPORT void setURL (const KURL & url) { m_sources ["urlsource"]->setURL (url); }
    void sizes (int & w, int & h) const;

    /* Changes the process,
     * calls setSource if process was playing
     * */
    void setProcess (const char *);
    void setRecorder (const char *);

    /* Changes the source,
     * calls init() and reschedules an activate() on the source
     * */
    void setSource (Source * source);
    KDE_NO_EXPORT Process * process () const { return m_process; }
    KDE_NO_EXPORT Process * recorder () const { return m_recorder; }
    KDE_NO_EXPORT Source * source () const { return m_source; }
    QMap <QString, Process *> & players () { return m_players; }
    QMap <QString, Process *> & recorders () { return m_recorders; }
    QMap <QString, Source *> & sources () { return m_sources; }
    KDE_NO_EXPORT KConfig * config () const { return m_config; }
    void updatePlayerMenu ();

    // these are called from Process
    void changeURL (const QString & url);
    void changeTitle (const QString & title);
    void updateTree ();
public slots:
    virtual bool openURL (const KURL & url);
    virtual bool closeURL ();
    virtual void pause (void);
    virtual void play (void);
    virtual void stop (void);
    void record ();
    virtual void seek (unsigned long msec);
    void adjustVolume (int incdec);
    bool playing () const;
    void showConfigDialog ();
    void showVideoWindow ();
    void showPlayListWindow ();
    void showConsoleWindow ();
    void slotPlayerMenu (int);
    void back ();
    void forward ();
    void addBookMark (const QString & title, const QString & url);
    void volumeChanged (int);
    void increaseVolume ();
    void decreaseVolume ();
public:
    virtual bool isSeekable (void) const;
    virtual unsigned long position (void) const;
    virtual bool hasLength (void) const;
    virtual unsigned long length (void) const;
signals:
    void startPlaying ();
    void stopPlaying ();
    void startRecording ();
    void stopRecording ();
    void sourceChanged (KMPlayer::Source *);
    void loading (int percentage);
    void urlAdded (const QString & url);
    void urlChanged (const QString & url);
    void titleChanged (const QString & title);
    void processChanged (const char *);
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
    virtual void processStateChange (KMPlayer::Process::State, KMPlayer::Process::State);
    void recordingStateChange (KMPlayer::Process::State, KMPlayer::Process::State);
    void positioned (int pos);
    void lengthFound (int len);
    virtual void loaded (int percentage);
    void fullScreen ();
    void playListItemSelected (QListViewItem *);
protected:
    KConfig * m_config;
    QGuardedPtr <View> m_view;
    Settings * m_settings;
    Process * m_process;
    Process * m_recorder;
    Source * m_source;
    ProcessMap m_players;
    ProcessMap m_recorders;
    QMap <QString, Source *> m_sources;
    BookmarkManager * m_bookmark_manager;
    BookmarkOwner * m_bookmark_owner;
    KBookmarkMenu * m_bookmark_menu;
    int m_record_timer;
    bool m_noresize : 1;
    bool m_use_slave : 1;
    bool m_bPosSliderPressed : 1;
    bool m_in_update_tree : 1;
};

} // namespace

#endif
