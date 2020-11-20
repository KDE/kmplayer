/*
    SPDX-FileCopyrightText: 2003 Koos Vriezen <koos.vriezen@xs4all.nl>

    SPDX-License-Identifier: LGPL-2.0-or-later
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

#include <kio/global.h>

#include "kmplayerplaylist.h"
#include "kmplayerpartbase.h"
#include "mediaobject.h"
#include "kmplayercommon_export.h"

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
class KMPLAYERCOMMON_EXPORT Process : public QObject, public IProcess
{
    Q_OBJECT
public:
    Process (QObject *parent, ProcessInfo *, Settings *settings);
    ~Process () override;

    virtual void init ();
    virtual void initProcess ();
    void setAudioLang (int id) override;
    void setSubtitle (int id) override;
    bool running () const override;
    KMPLAYERCOMMON_NO_EXPORT QProcess * process () const { return m_process; }
    KMPLAYERCOMMON_NO_EXPORT Source * source () const { return m_source; }
    View *view () const;
    WId widget ();
    void setSource (Source * src) { m_source = src; }
    void setState (IProcess::State newstate);
    bool grabPicture (const QString &file, int frame) override KMPLAYERCOMMON_NO_EXPORT;
    Mrl *mrl () const;

    bool ready () override;
    bool play () override;
    void stop () override;
    void quit () override;
    void pause () override;
    void unpause () override;
    /* seek (pos, abs) seek position in deci-seconds */
    bool seek (int pos, bool absolute) override;
    /* volume from 0 to 100 */
    void volume (int pos, bool absolute) override;
    /* saturation/hue/contrast/brightness from -100 to 100 */
    bool saturation (int pos, bool absolute) override;
    bool hue (int pos, bool absolute) override;
    bool contrast (int pos, bool absolute) override;
    bool brightness (int pos, bool absolute) override;
signals:
    void grabReady (const QString & path);
protected slots:
    void rescheduledStateChanged () KMPLAYERCOMMON_NO_EXPORT;
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
    ~MPlayerBase () override;
    void initProcess () override;
    void stop () override;
    void quit () override;
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
class MPlayerProcessInfo : public ProcessInfo
{
public:
    MPlayerProcessInfo (MediaManager *);
    IProcess *create (PartBase*, ProcessUser*) override;
};

class KMPLAYERCOMMON_EXPORT MPlayer : public MPlayerBase
{
    Q_OBJECT
public:
    MPlayer (QObject *parent, ProcessInfo *pinfo, Settings *settings);
    ~MPlayer () override;

    void init () override KMPLAYERCOMMON_NO_EXPORT;
    bool grabPicture (const QString &file, int pos) override KMPLAYERCOMMON_NO_EXPORT;
    void setAudioLang (int id) override;
    void setSubtitle (int id) override;
    bool deMediafiedPlay () override KMPLAYERCOMMON_NO_EXPORT;
    void stop () override KMPLAYERCOMMON_NO_EXPORT;
    void pause () override KMPLAYERCOMMON_NO_EXPORT;
    void unpause () override KMPLAYERCOMMON_NO_EXPORT;
    bool seek (int pos, bool absolute) override KMPLAYERCOMMON_NO_EXPORT;
    void volume (int pos, bool absolute) override KMPLAYERCOMMON_NO_EXPORT;
    bool saturation (int pos, bool absolute) override KMPLAYERCOMMON_NO_EXPORT;
    bool hue (int pos, bool absolute) override KMPLAYERCOMMON_NO_EXPORT;
    bool contrast (int pos, bool absolute) override KMPLAYERCOMMON_NO_EXPORT;
    bool brightness (int pos, bool absolute) override KMPLAYERCOMMON_NO_EXPORT;
    bool ready () override KMPLAYERCOMMON_NO_EXPORT;
protected:
    void processStopped () override KMPLAYERCOMMON_NO_EXPORT;
private slots:
    void processOutput () KMPLAYERCOMMON_NO_EXPORT;
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
class MPlayerPreferencesPage : public PreferencesPage
{
public:
    enum Pattern {
        pat_size = 0, pat_cache, pat_pos, pat_index,
        pat_refurl, pat_ref, pat_start,
        pat_vcdtrack, pat_cdromtracks,
        pat_last
    };
    MPlayerPreferencesPage ();
    ~MPlayerPreferencesPage () override {}
    void write (KSharedConfigPtr) override;
    void read (KSharedConfigPtr) override;
    void sync (bool fromUI) override;
    void prefLocation (QString & item, QString & icon, QString & tab) override;
    QFrame * prefPage (QWidget * parent) override;
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
class RecordDocument : public SourceDocument
{
public:
    RecordDocument (const QString &url, const QString &rurl, const QString &rec,
                    Source *source);

    void begin () override;
    void message (MessageType msg, void *) override;
    void deactivate () override;

    QString record_file;
    QString recorder;
};

/*
 * MEncoder recorder
 */
class MEncoderProcessInfo : public ProcessInfo
{
public:
    MEncoderProcessInfo (MediaManager *);
    IProcess *create (PartBase*, ProcessUser*) override;
};

class MEncoder : public MPlayerBase {
    Q_OBJECT
public:
    MEncoder (QObject *parent, ProcessInfo *pinfo, Settings *settings);
    ~MEncoder () override;
    void init () override;
    bool deMediafiedPlay () override;
    void stop () override;
};

/*
 * MPlayer recorder, runs 'mplayer -dumpstream'
 */
class MPlayerDumpProcessInfo : public ProcessInfo
{
public:
    MPlayerDumpProcessInfo (MediaManager *);
    IProcess *create (PartBase*, ProcessUser*) override;
};

class MPlayerDumpstream : public MPlayerBase
{
    Q_OBJECT
public:
    MPlayerDumpstream (QObject *parent, ProcessInfo *pinfo, Settings *settings);
    ~MPlayerDumpstream () override;
    void init () override;
    bool deMediafiedPlay () override;
    void stop () override;
};

class MasterProcessInfo : public QObject, public ProcessInfo
{
    Q_OBJECT
public:
    MasterProcessInfo (const char *nm, const QString &lbl,
            const char **supported,MediaManager *, PreferencesPage *);
    ~MasterProcessInfo () override;

    void quitProcesses () override;

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

class MasterProcess : public Process
{
    Q_OBJECT
public:
    MasterProcess (QObject *p, ProcessInfo *pi, Settings *s);
    ~MasterProcess () override;

    void init () override;
    bool deMediafiedPlay () override;
    bool running () const override;

    void streamInfo (uint64_t length, double aspect);
    void streamMetaInfo (QString info);
    void loading (int p);
    void playing ();
    void progress (uint64_t pos);
    void pause () override;
    void unpause () override;
    bool seek (int pos, bool absolute) override;
    void volume (int pos, bool absolute) override;
    void eof ();
    void stop () override;

private:
    QString m_slave_path;
};

class PhononProcessInfo : public MasterProcessInfo
{
public:
    PhononProcessInfo (MediaManager *);

    IProcess *create (PartBase*, ProcessUser*) override;

    bool startSlave () override;
};

class Phonon : public MasterProcess
{
    Q_OBJECT
public:
    Phonon (QObject *parent, ProcessInfo*, Settings *settings);

    bool ready () override;
};

/*
 * Config document as used by kxineplayer backend
 */
struct ConfigDocument : public Document
{
    ConfigDocument ();
    ~ConfigDocument () override;
    Node *childFromTag (const QString & tag) override;
};

/*
 * Element for ConfigDocument
 */
struct ConfigNode : public DarkNode
{
    ConfigNode (NodePtr & d, const QString & tag);
    ~ConfigNode () override {}
    Node *childFromTag (const QString & tag) override;
    QWidget * w;
};

/*
 * Element for ConfigDocument, defining type of config item
 */
struct TypeNode : public ConfigNode
{
    TypeNode (NodePtr & d, const QString & t);
    ~TypeNode () override {}
    Node *childFromTag (const QString & tag) override;
    void changedXML (QTextStream & out);
    QWidget * createWidget (QWidget * parent);
    const char * nodeName () const override { return tag.toLatin1().constData(); }
    QString tag;
};

/*
 * ffmpeg backend recorder
 */
class FFMpegProcessInfo : public ProcessInfo
{
public:
    FFMpegProcessInfo (MediaManager *);
    IProcess *create (PartBase*, ProcessUser*) override;
};

class KMPLAYERCOMMON_EXPORT FFMpeg : public Process
{
    Q_OBJECT
public:
    FFMpeg (QObject *parent, ProcessInfo *pinfo, Settings *settings);
    ~FFMpeg () override;
    void init () override KMPLAYERCOMMON_NO_EXPORT;
    bool deMediafiedPlay () override;
    void stop () override KMPLAYERCOMMON_NO_EXPORT;
    void quit () override KMPLAYERCOMMON_NO_EXPORT;
private slots:
    void processStopped (int, QProcess::ExitStatus) KMPLAYERCOMMON_NO_EXPORT;
};

/*
 * npplayer backend
 */

class NpStream : public QObject
{
    Q_OBJECT
public:
    enum Reason {
        NoReason = -1,
        BecauseDone = 0, BecauseError = 1, BecauseStopped = 2
    };

    NpStream (NpPlayer *parent, uint32_t stream_id, const QString &url, const QByteArray &post);
    ~NpStream () override;

    void open ();
    void close ();

    void destroy ();

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
    void redirected(uint32_t, const QUrl&);
private slots:
    void slotResult (KJob*);
    void slotData (KIO::Job*, const QByteArray& qb);
    void redirection(KIO::Job*, const QUrl& url);
    void slotMimetype (KIO::Job *, const QString &mime);
    void slotTotalSize (KJob *, qulonglong sz);
};

class NppProcessInfo : public ProcessInfo
{
public:
    NppProcessInfo (MediaManager *);
    IProcess *create (PartBase*, ProcessUser*) override;
};

class NpPlayer : public Process
{
    Q_OBJECT
public:
    NpPlayer (QObject *, KMPlayer::ProcessInfo*, Settings *);
    ~NpPlayer () override;

    static const char *name;
    static const char *supports [];
    static IProcess *create (PartBase *, ProcessUser *);

    void init () override;
    bool deMediafiedPlay () override;
    void initProcess () override;

    using Process::running;
    void running (const QString &srv);
    void plugged ();
    void request_stream (const QString &path, const QString &url, const QString &target, const QByteArray &post);
    QString evaluate (const QString &script, bool store);
    QString cookie (const QString &url);
    void dimension (int w, int h);

    void destroyStream (uint32_t sid);

    const QString & destination () const { return service; }
    const QString & interface () const { return iface; }
    QString objectPath () const { return path; }
    void stop () override;
    void quit () override;
    bool ready () override;
signals:
    void evaluate (const QString & scr, bool store, QString & result);
    void loaded ();
public slots:
    void requestGet (const uint32_t, const QString &, QString *);
    void requestCall (const uint32_t, const QString &, const QStringList &, QString *);
private slots:
    void processOutput ();
    void processStopped (int, QProcess::ExitStatus);
    void wroteStdin (qint64);
    void streamStateChanged ();
    void streamRedirected(uint32_t, const QUrl&);
protected:
    void terminateJobs () override;
private:
    void sendFinish (uint32_t sid, uint32_t total, NpStream::Reason because);
    void processStreams ();
    QString service;
    QString iface;
    QString path;
    QString filter;
    typedef QMap <uint32_t, NpStream *> StreamMap;
    StreamMap streams;
    QString remote_service;
    QString m_base_url;
    QByteArray send_buf;
    bool write_in_progress;
    bool in_process_stream;
};

} // namespace

#endif //_KMPLAYERPROCESS_H_
