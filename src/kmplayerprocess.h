/* This file is part of the KDE project
 *
 * Copyright (C) 2003 Koos Vriezen <koos.vriezen@xs4all.nl>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Steet, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef _KMPLAYERPROCESS_H_
#define _KMPLAYERPROCESS_H_

#include <qobject.h>
#include <qstring.h>
#include <qlist.h>
#include <qbytearray.h>
#include <qstringlist.h>
#include <qregexp.h>
#include <qprocess.h>

#include <kurl.h>
#include <kio/global.h>

#include "kmplayer_def.h"
#include "kmplayerplaylist.h"
#include "kmplayersource.h"
#include "mediaobject.h"

class QWidget;
class KJob;

namespace KIO {
    class Job;
    class TransferJob;
}

namespace KMPlayer {

class Settings;
class View;
class MediaManager;
class Source;
class Callback;
class Backend_stub;
class NpPlayer;
class MPlayerPreferencesPage;
class MPlayerPreferencesFrame;
class XMLPreferencesPage;
class XMLPreferencesFrame;


/*
 * Base class for all backend processes
 */
class KMPLAYER_EXPORT Process : public QObject, public IProcess {
    Q_OBJECT
public:
    Process (QObject *parent, ProcessInfo *, Settings *settings);
    virtual ~Process ();

    virtual void init ();
    virtual void initProcess ();
    virtual void setAudioLang (int id);
    virtual void setSubtitle (int id);
    virtual bool running () const;
    KDE_NO_EXPORT QProcess * process () const { return m_process; }
    KDE_NO_EXPORT Source * source () const { return m_source; }
    View *view () const;
    WId widget ();
    void setSource (Source * src) { m_source = src; }
    void setState (IProcess::State newstate);
    virtual bool grabPicture (const QString &file, int frame);
    Mrl *mrl () const;

    virtual bool ready ();
    virtual bool play ();
    virtual void stop ();
    virtual void quit ();
    virtual void pause ();
    virtual void unpause ();
    /* seek (pos, abs) seek position in deci-seconds */
    virtual bool seek (int pos, bool absolute);
    /* volume from 0 to 100 */
    virtual void volume (int pos, bool absolute);
    /* saturation/hue/contrast/brightness from -100 to 100 */
    virtual bool saturation (int pos, bool absolute);
    virtual bool hue (int pos, bool absolute);
    virtual bool contrast (int pos, bool absolute);
    virtual bool brightness (int pos, bool absolute);
signals:
    void grabReady (const QString & path);
protected slots:
    void rescheduledStateChanged ();
    void result (KJob *);
    void processStateChanged (QProcess::ProcessState);
protected:
    virtual bool deMediafiedPlay ();
    virtual void terminateJobs ();
    void startProcess (const QString &program, const QStringList &args);

    Source * m_source;
    Settings * m_settings;
    State m_old_state;
    QProcess * m_process;
    KIO::Job * m_job;
    QString m_url;
    int m_request_seek;
    QProcess::ProcessState m_process_state;
};


/*
 * Base class for all MPlayer based processes
 */
class MPlayerBase : public Process {
    Q_OBJECT
public:
    MPlayerBase (QObject *parent, ProcessInfo *, Settings *);
    ~MPlayerBase ();
    void initProcess ();
    virtual void stop ();
    virtual void quit ();
protected:
    bool sendCommand (const QString &);
    bool removeQueued (const char *cmd);
    QList<QByteArray> commands;
    bool m_needs_restarted;
protected slots:
    virtual void processStopped ();
private slots:
    void dataWritten (qint64);
    void processStopped (int, QProcess::ExitStatus);
};

/*
 * MPlayer process
 */
class KMPLAYER_NO_EXPORT MPlayerProcessInfo : public ProcessInfo {
public:
    MPlayerProcessInfo (MediaManager *);
    virtual IProcess *create (PartBase*, ProcessUser*);
};

class KDE_EXPORT MPlayer : public MPlayerBase {
    Q_OBJECT
public:
    MPlayer (QObject *parent, ProcessInfo *pinfo, Settings *settings);
    ~MPlayer ();

    virtual void init ();
    virtual bool grabPicture (const QString &file, int pos);
    virtual void setAudioLang (int id);
    virtual void setSubtitle (int id);
    virtual bool deMediafiedPlay ();
    virtual void stop ();
    virtual void pause ();
    virtual void unpause ();
    virtual bool seek (int pos, bool absolute);
    virtual void volume (int pos, bool absolute);
    virtual bool saturation (int pos, bool absolute);
    virtual bool hue (int pos, bool absolute);
    virtual bool contrast (int pos, bool absolute);
    virtual bool brightness (int pos, bool absolute);
    bool ready ();
protected:
    void processStopped ();
private slots:
    void processOutput ();
private:
    QString m_process_output;
    QString m_grab_file;
    QString m_grab_dir;
    QWidget * m_widget;
    QString m_tmpURL;
    Source::LangInfoPtr alanglist;
    WeakPtr <Source::LangInfo> alanglist_end;
    Source::LangInfoPtr slanglist;
    WeakPtr <Source::LangInfo> slanglist_end;
    State m_transition_state;
    int aid, sid;
    int old_volume;
};

#ifdef _KMPLAYERCONFIG_H_
/*
 * MPlayer preferences page
 */
class KMPLAYER_NO_EXPORT MPlayerPreferencesPage : public PreferencesPage {
public:
    enum Pattern {
        pat_size = 0, pat_cache, pat_pos, pat_index,
        pat_refurl, pat_ref, pat_start,
        pat_vcdtrack, pat_cdromtracks,
        pat_last
    };
    MPlayerPreferencesPage ();
    KDE_NO_CDTOR_EXPORT ~MPlayerPreferencesPage () {}
    void write (KSharedConfigPtr);
    void read (KSharedConfigPtr);
    void sync (bool fromUI);
    void prefLocation (QString & item, QString & icon, QString & tab);
    QFrame * prefPage (QWidget * parent);
    QRegExp m_patterns[pat_last];
    int cachesize;
    QString mplayer_path;
    QString additionalarguments;
    bool alwaysbuildindex;
private:
    MPlayer * m_process;
    MPlayerPreferencesFrame *m_configframe;
};

#endif
/*
 * Base class for all recorders
 */
class KMPLAYER_NO_EXPORT RecordDocument : public SourceDocument {
public:
    RecordDocument (const QString &url, const QString &rurl, const QString &rec,
                    Source *source);

    virtual void begin ();
    virtual void message (MessageType msg, void *);
    virtual void deactivate ();

    QString record_file;
    QString recorder;
};

/*
 * MEncoder recorder
 */
class KMPLAYER_NO_EXPORT MEncoderProcessInfo : public ProcessInfo {
public:
    MEncoderProcessInfo (MediaManager *);
    virtual IProcess *create (PartBase*, ProcessUser*);
};

class MEncoder : public MPlayerBase {
    Q_OBJECT
public:
    MEncoder (QObject *parent, ProcessInfo *pinfo, Settings *settings);
    ~MEncoder ();
    virtual void init ();
    virtual bool deMediafiedPlay ();
    virtual void stop ();
};

/*
 * MPlayer recorder, runs 'mplayer -dumpstream'
 */
class KMPLAYER_NO_EXPORT MPlayerDumpProcessInfo : public ProcessInfo {
public:
    MPlayerDumpProcessInfo (MediaManager *);
    virtual IProcess *create (PartBase*, ProcessUser*);
};

class KMPLAYER_NO_EXPORT MPlayerDumpstream : public MPlayerBase {
    Q_OBJECT
public:
    MPlayerDumpstream (QObject *parent, ProcessInfo *pinfo, Settings *settings);
    ~MPlayerDumpstream ();
    virtual void init ();
    virtual bool deMediafiedPlay ();
    virtual void stop ();
};

class KMPLAYER_NO_EXPORT MasterProcessInfo : public QObject, public ProcessInfo {
    Q_OBJECT
public:
    MasterProcessInfo (const char *nm, const QString &lbl,
            const char **supported,MediaManager *, PreferencesPage *);
    ~MasterProcessInfo ();

    virtual void quitProcesses ();

    void running (const QString &srv);

    QString m_service;
    QString m_path;
    QString m_slave_service;
    QProcess *m_slave;

private slots:
    void slaveStopped (int, QProcess::ExitStatus);
    void slaveOutput ();

protected:
    virtual void initSlave ();
    virtual bool startSlave () = 0;
    virtual void stopSlave ();
};

class KMPLAYER_NO_EXPORT MasterProcess : public Process {
    Q_OBJECT
public:
    MasterProcess (QObject *p, ProcessInfo *pi, Settings *s);
    ~MasterProcess ();

    virtual void init ();
    virtual bool deMediafiedPlay ();
    virtual bool running () const;

    void streamInfo (uint64_t length, double aspect);
    void streamMetaInfo (QString info);
    void loading (int p);
    void playing ();
    void progress (uint64_t pos);
    void pause ();
    void unpause ();
    bool seek (int pos, bool absolute);
    void volume (int pos, bool absolute);
    void eof ();
    void stop ();

private:
    QString m_slave_path;
};

class KMPLAYER_NO_EXPORT PhononProcessInfo : public MasterProcessInfo {
public:
    PhononProcessInfo (MediaManager *);

    virtual IProcess *create (PartBase*, ProcessUser*);

    virtual bool startSlave ();
};

class KMPLAYER_NO_EXPORT Phonon : public MasterProcess {
    Q_OBJECT
public:
    Phonon (QObject *parent, ProcessInfo*, Settings *settings);

    virtual bool ready ();
};

/*
 * Config document as used by kxineplayer backend
 */
struct KMPLAYER_NO_EXPORT ConfigDocument : public Document {
    ConfigDocument ();
    ~ConfigDocument ();
    Node *childFromTag (const QString & tag);
};

/*
 * Element for ConfigDocument
 */
struct KMPLAYER_NO_EXPORT ConfigNode : public DarkNode {
    ConfigNode (NodePtr & d, const QString & tag);
    KDE_NO_CDTOR_EXPORT ~ConfigNode () {}
    Node *childFromTag (const QString & tag);
    QWidget * w;
};

/*
 * Element for ConfigDocument, defining type of config item
 */
struct KMPLAYER_NO_EXPORT TypeNode : public ConfigNode {
    TypeNode (NodePtr & d, const QString & t);
    KDE_NO_CDTOR_EXPORT ~TypeNode () {}
    Node *childFromTag (const QString & tag);
    void changedXML (QTextStream & out);
    QWidget * createWidget (QWidget * parent);
    const char * nodeName () const { return tag.toAscii (); }
    QString tag;
};

/*
 * ffmpeg backend recorder
 */
class KMPLAYER_NO_EXPORT FFMpegProcessInfo : public ProcessInfo {
public:
    FFMpegProcessInfo (MediaManager *);
    virtual IProcess *create (PartBase*, ProcessUser*);
};

class KMPLAYER_EXPORT FFMpeg : public Process {
    Q_OBJECT
public:
    FFMpeg (QObject *parent, ProcessInfo *pinfo, Settings *settings);
    ~FFMpeg ();
    virtual void init ();
    virtual bool deMediafiedPlay ();
    virtual void stop ();
    virtual void quit ();
private slots:
    void processStopped (int, QProcess::ExitStatus);
};

/*
 * npplayer backend
 */

class KMPLAYER_NO_EXPORT NpStream : public QObject {
    Q_OBJECT
public:
    enum Reason {
        NoReason = -1,
        BecauseDone = 0, BecauseError = 1, BecauseStopped = 2
    };

    NpStream (NpPlayer *parent, uint32_t stream_id, const QString &url, const QByteArray &post);
    ~NpStream ();

    void open () KMPLAYER_NO_MBR_EXPORT;
    void close () KMPLAYER_NO_MBR_EXPORT;

    void destroy () KMPLAYER_NO_MBR_EXPORT;

    QString url;
    QByteArray post;
    QByteArray pending_buf;
    KIO::TransferJob *job;
    timeval data_arrival;
    uint32_t bytes;
    uint32_t stream_id;
    uint32_t content_length;
    Reason finish_reason;
    QString mimetype;
    QString http_headers;
    bool received_data;
signals:
    void stateChanged ();
    void redirected (uint32_t, const KUrl &);
private slots:
    void slotResult (KJob*);
    void slotData (KIO::Job*, const QByteArray& qb);
    void redirection (KIO::Job *, const KUrl &url);
    void slotMimetype (KIO::Job *, const QString &mime);
    void slotTotalSize (KJob *, qulonglong sz);
};

class KMPLAYER_NO_EXPORT NppProcessInfo : public ProcessInfo {
public:
    NppProcessInfo (MediaManager *);
    virtual IProcess *create (PartBase*, ProcessUser*);
};

} // namespace

#endif //_KMPLAYERPROCESS_H_
