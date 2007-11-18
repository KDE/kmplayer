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
#include <qcstring.h>
#include <qstringlist.h>
#include <qregexp.h>

#include <kurl.h>
#include <kio/global.h>

#include "kmplayerconfig.h"
#include "kmplayersource.h"
#include "mediaobject.h"

class QWidget;
class KProcess;

namespace KIO {
    class Job;
    class TransferJob;
}

namespace KMPlayer {

class Settings;
class View;
class MediaManager;
class AudioVideoMedia;
class Source;
class Callback;
class Backend_stub;
class MPlayerPreferencesPage;
class MPlayerPreferencesFrame;
class XMLPreferencesPage;
class XMLPreferencesFrame;


class KMPLAYER_EXPORT ProcessInfo {
public:
    ProcessInfo (const char *nm, const QString &lbl, const char **supported,
            MediaManager *, PreferencesPage *);
    virtual ~ProcessInfo ();

    bool supports (const char *source) const;
    virtual IProcess *create (PartBase*, ProcessInfo*, AudioVideoMedia*) = 0;

    const char *name;
    QString label;
    const char **supported_sources;
    MediaManager *manager;
    PreferencesPage *config_page;
};

/*
 * Base class for all backend processes
 */
class KMPLAYER_EXPORT Process : public QObject, public IProcess {
    Q_OBJECT
public:
    Process (QObject *parent, ProcessInfo *, Settings *settings, const char *n);
    virtual ~Process ();

    virtual void init ();
    virtual void initProcess ();
    virtual void setAudioLang (int, const QString &);
    virtual void setSubtitle (int, const QString &);
    virtual bool running () const;
    KDE_NO_EXPORT KProcess * process () const { return m_process; }
    KDE_NO_EXPORT Source * source () const { return m_source; }
    View *view () const;
    WId widget ();
    void setSource (Source * src) { m_source = src; }
    virtual bool grabPicture (const KURL & url, int pos);
    Mrl *mrl () const;
signals:
    void grabReady (const QString & path);
    void finished ();
public slots:
    virtual bool ready ();
    virtual bool play ();
    virtual void stop ();
    virtual void quit ();
    virtual void pause ();
    /* seek (pos, abs) seek position in deci-seconds */
    virtual bool seek (int pos, bool absolute);
    /* volume from 0 to 100 */
    virtual bool volume (int pos, bool absolute);
    /* saturation/hue/contrast/brightness from -100 to 100 */
    virtual bool saturation (int pos, bool absolute);
    virtual bool hue (int pos, bool absolute);
    virtual bool contrast (int pos, bool absolute);
    virtual bool brightness (int pos, bool absolute);
protected slots:
    void rescheduledStateChanged ();
    void result (KIO::Job *);
protected:
    void setState (IProcess::State newstate);
    virtual bool deMediafiedPlay ();
    virtual void terminateJobs ();

    ProcessInfo *process_info;
    Source * m_source;
    Settings * m_settings;
    State m_old_state;
    KProcess * m_process;
    KIO::Job * m_job;
    QString m_url;
    int m_request_seek;
};

/*
 * Base class for all MPlayer based processes
 */
class MPlayerBase : public Process {
    Q_OBJECT
public:
    MPlayerBase (QObject *parent, ProcessInfo *, Settings *, const char *);
    ~MPlayerBase ();
    void initProcess ();
public slots:
    virtual void stop ();
    virtual void quit ();
protected:
    bool sendCommand (const QString &);
    QStringList commands;
    bool m_use_slave : 1;
protected slots:
    virtual void processStopped (KProcess *);
private slots:
    void dataWritten (KProcess *);
};

/*
 * MPlayer process
 */
class KMPLAYER_NO_EXPORT MPlayerProcessInfo : public ProcessInfo {
public:
    MPlayerProcessInfo (MediaManager *);
    virtual IProcess *create (PartBase*, ProcessInfo*, AudioVideoMedia*);
};

class KDE_EXPORT MPlayer : public MPlayerBase {
    Q_OBJECT
public:
    MPlayer (QObject *parent, ProcessInfo *pinfo, Settings *settings);
    ~MPlayer ();

    virtual void init ();
    virtual bool grabPicture (const KURL & url, int pos);
    virtual void setAudioLang (int, const QString &);
    virtual void setSubtitle (int, const QString &);
    bool run (const char * args, const char * pipe = 0L);
public slots:
    virtual bool deMediafiedPlay ();
    virtual void stop ();
    virtual void pause ();
    virtual bool seek (int pos, bool absolute);
    virtual bool volume (int pos, bool absolute);
    virtual bool saturation (int pos, bool absolute);
    virtual bool hue (int pos, bool absolute);
    virtual bool contrast (int pos, bool absolute);
    virtual bool brightness (int pos, bool absolute);
    bool ready ();
protected slots:
    void processStopped (KProcess *);
private slots:
    void processOutput (KProcess *, char *, int);
private:
    QString m_process_output;
    QString m_grabfile;
    QWidget * m_widget;
    QString m_tmpURL;
    struct LangInfo {
        LangInfo (int i, const QString & n) : id (i), name (n) {}
        int id; QString name; SharedPtr <LangInfo> next;
    };
    SharedPtr <LangInfo> alanglist;
    WeakPtr <LangInfo> alanglist_end;
    SharedPtr <LangInfo> slanglist;
    WeakPtr <LangInfo> slanglist_end;
    int aid, sid;
    int old_volume;
    bool m_needs_restarted;
};

/*
 * MPlayer preferences page
 */
class KMPLAYER_NO_EXPORT MPlayerPreferencesPage : public PreferencesPage {
public:
    enum Pattern {
        pat_size = 0, pat_cache, pat_pos, pat_index,
        pat_refurl, pat_ref, pat_start,
        pat_dvdlang, pat_dvdsub, pat_dvdtitle, pat_dvdchapter,
        pat_vcdtrack, pat_cdromtracks,
        pat_last
    };
    MPlayerPreferencesPage ();
    KDE_NO_CDTOR_EXPORT ~MPlayerPreferencesPage () {}
    void write (KConfig *);
    void read (KConfig *);
    void sync (bool fromUI);
    void prefLocation (QString & item, QString & icon, QString & tab);
    QFrame * prefPage (QWidget * parent);
    QRegExp m_patterns[pat_last];
    int cachesize;
    QString mplayer_path;
    QString additionalarguments;
    bool alwaysbuildindex;
private:
    MPlayerPreferencesFrame *m_configframe;
};

/*
 * Base class for all recorders
 */
class KMPLAYER_EXPORT Recorder {
public:
    KDE_NO_EXPORT const KURL & recordURL () const { return m_recordurl; }
    KDE_NO_EXPORT void setURL (const KURL & url) { m_recordurl = url; }
protected:
    KURL m_recordurl;
};

/*
 * MEncoder recorder
 */
class MEncoder : public MPlayerBase, public Recorder {
    Q_OBJECT
public:
    MEncoder (QObject *parent, ProcessInfo *pinfo, Settings *settings);
    ~MEncoder ();
    virtual void init ();
    virtual bool deMediafiedPlay ();
public slots:
    virtual void stop ();
};

/*
 * MPlayer recorder, runs 'mplayer -dumpstream'
 */
class KMPLAYER_NO_EXPORT MPlayerDumpstream
  : public MPlayerBase, public Recorder {
    Q_OBJECT
public:
    MPlayerDumpstream (QObject * parent, Settings * settings);
    ~MPlayerDumpstream ();
    virtual void init ();
    virtual bool deMediafiedPlay ();
public slots:
    virtual void stop ();
};


/*
 * Base class for backend processes having the KMPlayer::Backend interface
 */
class KMPLAYER_NO_EXPORT CallbackProcessInfo
 : public QObject, public ProcessInfo {
    Q_OBJECT
public:
    CallbackProcessInfo (const char *nm, const QString &lbl,
            const char **supported,MediaManager *, PreferencesPage *);
    ~CallbackProcessInfo ();

    QString dcopName ();
    virtual bool startBackend () {};
    void stopBackend ();
    void backendStarted (QCString dcopname, QByteArray & data);

    QByteArray changed_data;
    NodePtr config_doc;
    Backend_stub *backend;
    Callback *callback;
    KProcess *m_process;

signals:
    void configReceived ();

protected slots:
    void processStopped (KProcess *);
    void processOutput (KProcess *, char *, int);
};

class KMPLAYER_EXPORT CallbackProcess : public Process {
    Q_OBJECT
    friend class CallbackProcessInfo;
    friend class Callback;
public:
    CallbackProcess (QObject *, ProcessInfo *, Settings *, const char * n);
    ~CallbackProcess ();
    virtual void setStatusMessage (const QString & msg);
    virtual void setErrorMessage (int code, const QString & msg);
    virtual void setFinished ();
    virtual void setPlaying ();
    virtual void setMovieParams (int length, int width, int height, float aspect, const QStringList & alang, const QStringList & slang);
    virtual void setMoviePosition (int position);
    virtual void setLoadingProgress (int percentage);
    virtual void setAudioLang (int, const QString &);
    virtual void setSubtitle (int, const QString &);
    KDE_NO_EXPORT bool haveConfig () { return m_have_config == config_yes; }
    bool getConfigData ();
    static void setChangedData (CallbackProcessInfo *, const QByteArray &);
    void initProcess ();
    virtual bool deMediafiedPlay ();
    virtual bool running () const;
public slots:
    void stop ();
    void quit ();
    void pause ();
    bool seek (int pos, bool absolute);
    bool volume (int pos, bool absolute);
    bool saturation (int pos, bool absolute);
    bool hue (int pos, bool absolute);
    bool contrast (int pos, bool absolute);
    bool brightness (int pos, bool absolute);
protected:
    XMLPreferencesPage * m_configpage;
    bool in_gui_update;
    enum { config_unknown, config_probe, config_yes, config_no } m_have_config;
    enum { send_no, send_try, send_new } m_send_config;
};

/*
 * Config document as used by kxineplayer backend
 */
struct KMPLAYER_NO_EXPORT ConfigDocument : public Document {
    ConfigDocument ();
    ~ConfigDocument ();
    NodePtr childFromTag (const QString & tag);
};

/*
 * Element for ConfigDocument
 */
struct KMPLAYER_NO_EXPORT ConfigNode : public DarkNode {
    ConfigNode (NodePtr & d, const QString & tag);
    KDE_NO_CDTOR_EXPORT ~ConfigNode () {}
    NodePtr childFromTag (const QString & tag);
    QWidget * w;
};

/*
 * Element for ConfigDocument, defining type of config item
 */
struct KMPLAYER_NO_EXPORT TypeNode : public ConfigNode {
    TypeNode (NodePtr & d, const QString & t);
    KDE_NO_CDTOR_EXPORT ~TypeNode () {}
    NodePtr childFromTag (const QString & tag);
    void changedXML (QTextStream & out);
    QWidget * createWidget (QWidget * parent);
    const char * nodeName () const { return tag.ascii (); }
    QString tag;
};

/*
 * Preference page for XML type of docuement
 */
class KMPLAYER_NO_EXPORT XMLPreferencesPage : public PreferencesPage {
public:
    XMLPreferencesPage (CallbackProcessInfo *);
    ~XMLPreferencesPage ();
    void write (KConfig *);
    void read (KConfig *);
    void sync (bool fromUI);
    void prefLocation (QString & item, QString & icon, QString & tab);
    QFrame * prefPage (QWidget * parent);
private:
    CallbackProcessInfo *m_process_info;
    XMLPreferencesFrame * m_configframe;
};

/*
 * Xine backend process
 */
class KMPLAYER_NO_EXPORT XineProcessInfo : public CallbackProcessInfo {
public:
    XineProcessInfo (MediaManager *);
    virtual IProcess *create (PartBase*, ProcessInfo*, AudioVideoMedia*);

    virtual bool startBackend ();
};

class KMPLAYER_NO_EXPORT Xine : public CallbackProcess, public Recorder {
    Q_OBJECT
public:
    Xine (QObject *parent, ProcessInfo*, Settings *settings);
    ~Xine ();

public slots:
    bool ready ();
};

/*
 * GStreamer backend process
 */
class KMPLAYER_NO_EXPORT GStreamer : public CallbackProcess {
    Q_OBJECT
public:
    GStreamer (QObject * parent, Settings * settings);
    ~GStreamer ();
public slots:
    virtual bool ready ();
};

/*
 * ffmpeg backend recorder
 */
class KMPLAYER_EXPORT FFMpeg : public Process, public Recorder {
    Q_OBJECT
public:
    FFMpeg (QObject * parent, Settings * settings);
    ~FFMpeg ();
    virtual void init ();
    virtual bool deMediafiedPlay ();
public slots:
    virtual void stop ();
    virtual void quit ();
private slots:
    void processStopped (KProcess *);
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

    NpStream (QObject *parent, Q_UINT32 stream_id, const KURL & url);
    ~NpStream ();

    void open ();
    void close ();

    KURL url;
    QByteArray pending_buf;
    KIO::TransferJob *job;
    timeval data_arrival;
    Q_UINT32 bytes;
    Q_UINT32 stream_id;
    Q_UINT32 content_length;
    Reason finish_reason;
    QString mimetype;
signals:
    void stateChanged ();
    void redirected (Q_UINT32, const KURL &);
private slots:
    void slotResult (KIO::Job*);
    void slotData (KIO::Job*, const QByteArray& qb);
    void redirection (KIO::Job *, const KURL &url);
    void slotMimetype (KIO::Job *, const QString &mime);
    void slotTotalSize (KIO::Job *, KIO::filesize_t sz);
};

class KMPLAYER_NO_EXPORT NppProcessInfo : public ProcessInfo {
public:
    NppProcessInfo (MediaManager *);
    virtual IProcess *create (PartBase*, ProcessInfo*, AudioVideoMedia*);
};

class KMPLAYER_NO_EXPORT NpPlayer : public Process {
    Q_OBJECT
public:
    NpPlayer (QObject *, KMPlayer::ProcessInfo*, Settings *, const QString &sv);
    ~NpPlayer ();

    static const char *name;
    static const char *supports [];
    static IProcess *create (PartBase *, ProcessInfo *, AudioVideoMedia *);

    virtual void init ();
    virtual bool deMediafiedPlay ();
    virtual void initProcess ();

    void setStarted (const QString & srv);
    void requestStream (const QString & path, const QString & url, const QString & target);
    void destroyStream (const QString & path);

    KDE_NO_EXPORT const QString & destination () const { return service; }
    KDE_NO_EXPORT const QString & interface () const { return iface; }
    KDE_NO_EXPORT QString objectPath () const { return path; }
    QString evaluateScript (const QString & scr);
signals:
    void evaluate (const QString & scr, QString & result);
    void openUrl (const KURL & url, const QString & target);
public slots:
    virtual void stop ();
    virtual void quit ();
public slots:
    bool ready ();
private slots:
    void processOutput (KProcess *, char *, int);
    void processStopped (KProcess *);
    void wroteStdin (KProcess *);
    void streamStateChanged ();
    void streamRedirected (Q_UINT32, const KURL &);
protected:
    virtual void terminateJobs ();
private:
    void sendFinish (Q_UINT32 sid, Q_UINT32 total, NpStream::Reason because);
    void processStreams ();
    QString service;
    QString iface;
    QString path;
    QString filter;
    typedef QMap <Q_UINT32, NpStream *> StreamMap;
    StreamMap streams;
    QString remote_service;
    QByteArray send_buf;
    bool write_in_progress;
};

} // namespace

#endif //_KMPLAYERPROCESS_H_
