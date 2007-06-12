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
#include <qguardedptr.h>
#include <qstring.h>
#include <qcstring.h>
#include <qstringlist.h>
#include <qregexp.h>

#include <kurl.h>

#include "kmplayerconfig.h"
#include "kmplayersource.h"

class QWidget;
class KProcess;

namespace KIO {
    class Job;
    class TransferJob;
}

namespace KMPlayer {
    
class Settings;
class Viewer;
class Source;
class Callback;
class Backend_stub;

/*
 * Base class for all backend processes
 */
class KMPLAYER_EXPORT Process : public QObject {
    Q_OBJECT
public:
    enum State {
        NotRunning = 0, Ready, Buffering, Playing
    };
    Process (QObject * parent, Settings * settings, const char * n);
    virtual ~Process ();
    virtual void init ();
    virtual void initProcess (Viewer *);
    virtual QString menuName () const;
    virtual void setAudioLang (int, const QString &);
    virtual void setSubtitle (int, const QString &);
    bool playing () const;
    KDE_NO_EXPORT KProcess * process () const { return m_process; }
    KDE_NO_EXPORT Source * source () const { return m_source; }
    virtual WId widget ();
    Viewer * viewer () const;
    void setSource (Source * src) { m_source = src; }
    virtual bool grabPicture (const KURL & url, int pos);
    bool supports (const char * source) const;
    State state () const { return m_state; }
    NodePtr mrl () const { return m_mrl; }
signals:
    void grabReady (const QString & path);
public slots:
    virtual bool ready (Viewer *);
    bool play (Source *, NodePtr mrl);
    virtual bool stop ();
    virtual bool quit ();
    virtual bool pause ();
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
    void setState (State newstate);
    virtual bool deMediafiedPlay ();
    virtual void terminateJobs ();
    Source * m_source;
    Settings * m_settings;
    NodePtrW m_mrl;
    State m_state;
    State m_old_state;
    KProcess * m_process;
    KIO::Job * m_job;
    QString m_url;
    int m_request_seek;
    const char ** m_supported_sources;
private:
    QGuardedPtr <Viewer> m_viewer;
};

/*
 * Base class for all MPlayer based processes
 */
class MPlayerBase : public Process {
    Q_OBJECT
public:
    MPlayerBase (QObject * parent, Settings * settings, const char * n);
    ~MPlayerBase ();
    void initProcess (Viewer *);
public slots:
    virtual bool stop ();
    virtual bool quit ();
protected:
    bool sendCommand (const QString &);
    QStringList commands;
    bool m_use_slave : 1;
protected slots:
    virtual void processStopped (KProcess *);
private slots:
    void dataWritten (KProcess *);
};

class MPlayerPreferencesPage;
class MPlayerPreferencesFrame;

/*
 * MPlayer process
 */
class KDE_EXPORT MPlayer : public MPlayerBase {
    Q_OBJECT
public:
    MPlayer (QObject * parent, Settings * settings);
    ~MPlayer ();
    virtual void init ();
    virtual QString menuName () const;
    virtual WId widget ();
    virtual bool grabPicture (const KURL & url, int pos);
    virtual void setAudioLang (int, const QString &);
    virtual void setSubtitle (int, const QString &);
    bool run (const char * args, const char * pipe = 0L);
public slots:
    virtual bool deMediafiedPlay ();
    virtual bool stop ();
    virtual bool pause ();
    virtual bool seek (int pos, bool absolute);
    virtual bool volume (int pos, bool absolute);
    virtual bool saturation (int pos, bool absolute);
    virtual bool hue (int pos, bool absolute);
    virtual bool contrast (int pos, bool absolute);
    virtual bool brightness (int pos, bool absolute);
    MPlayerPreferencesPage * configPage () const { return m_configpage; }
    bool ready (Viewer *);
protected slots:
    void processStopped (KProcess *);
private slots:
    void processOutput (KProcess *, char *, int);
private:
    QString m_process_output;
    QString m_grabfile;
    QWidget * m_widget;
    MPlayerPreferencesPage * m_configpage;
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
    MPlayerPreferencesPage (MPlayer *);
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
    MPlayer * m_process;
    MPlayerPreferencesFrame * m_configframe;
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
    MEncoder (QObject * parent, Settings * settings);
    ~MEncoder ();
    virtual void init ();
    virtual bool deMediafiedPlay ();
public slots:
    virtual bool stop ();
};

/*
 * MPlayer recorder, runs 'mplayer -dumpstream'
 */
class MPlayerDumpstream : public MPlayerBase, public Recorder {
    Q_OBJECT
public:
    MPlayerDumpstream (QObject * parent, Settings * settings);
    ~MPlayerDumpstream ();
    virtual void init ();
    virtual bool deMediafiedPlay ();
public slots:
    virtual bool stop ();
};

class XMLPreferencesPage;
class XMLPreferencesFrame;

/*
 * Base class for all backend processes having the KMPlayer::Backend interface
 */
class KMPLAYER_EXPORT CallbackProcess : public Process {
    Q_OBJECT
    friend class Callback;
public:
    CallbackProcess (QObject * parent, Settings * settings, const char * n, const QString & menu);
    ~CallbackProcess ();
    virtual void setStatusMessage (const QString & msg);
    virtual void setErrorMessage (int code, const QString & msg);
    virtual void setFinished ();
    virtual void setPlaying ();
    virtual void setStarted (QCString dcopname, QByteArray & data);
    virtual void setMovieParams (int length, int width, int height, float aspect, const QStringList & alang, const QStringList & slang);
    virtual void setMoviePosition (int position);
    virtual void setLoadingProgress (int percentage);
    virtual void setAudioLang (int, const QString &);
    virtual void setSubtitle (int, const QString &);
    virtual QString menuName () const;
    virtual WId widget ();
    KDE_NO_EXPORT QByteArray & configData () { return m_configdata; }
    KDE_NO_EXPORT bool haveConfig () { return m_have_config == config_yes; }
    bool getConfigData ();
    void setChangedData (const QByteArray &);
    QString dcopName ();
    NodePtr configDocument () { return configdoc; }
    void initProcess (Viewer *);
    virtual bool deMediafiedPlay ();
public slots:
    bool stop ();
    bool quit ();
    bool pause ();
    bool seek (int pos, bool absolute);
    bool volume (int pos, bool absolute);
    bool saturation (int pos, bool absolute);
    bool hue (int pos, bool absolute);
    bool contrast (int pos, bool absolute);
    bool brightness (int pos, bool absolute);
signals:
    void configReceived ();
protected slots:
    void processStopped (KProcess *);
    void processOutput (KProcess *, char *, int);
protected:
    Callback * m_callback;
    Backend_stub * m_backend;
    QString m_menuname;
    QByteArray m_configdata;
    QByteArray m_changeddata;
    XMLPreferencesPage * m_configpage;
    NodePtr configdoc;
    bool in_gui_update;
    enum { config_unknown, config_probe, config_yes, config_no } m_have_config;
    enum { send_no, send_try, send_new } m_send_config;
};

/*
 * Config document as used by kxineplayer backend
 */
struct KMPLAYER_EXPORT ConfigDocument : public Document {
    ConfigDocument ();
    ~ConfigDocument ();
    NodePtr childFromTag (const QString & tag);
};

/*
 * Element for ConfigDocument
 */
struct KMPLAYER_EXPORT ConfigNode : public DarkNode {
    ConfigNode (NodePtr & d, const QString & tag);
    KDE_NO_CDTOR_EXPORT ~ConfigNode () {}
    NodePtr childFromTag (const QString & tag);
    QWidget * w;
};

/*
 * Element for ConfigDocument, defining type of config item
 */
struct KMPLAYER_EXPORT TypeNode : public ConfigNode {
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
class KMPLAYER_EXPORT XMLPreferencesPage : public PreferencesPage {
public:
    XMLPreferencesPage (CallbackProcess *);
    ~XMLPreferencesPage ();
    void write (KConfig *);
    void read (KConfig *);
    void sync (bool fromUI);
    void prefLocation (QString & item, QString & icon, QString & tab);
    QFrame * prefPage (QWidget * parent);
private:
    CallbackProcess * m_process;
    XMLPreferencesFrame * m_configframe;
};

/*
 * Xine backend process
 */
class Xine : public CallbackProcess, public Recorder {
    Q_OBJECT
public:
    Xine (QObject * parent, Settings * settings);
    ~Xine ();
public slots:
    bool ready (Viewer *);
};

/*
 * GStreamer backend process
 */
class GStreamer : public CallbackProcess {
    Q_OBJECT
public:
    GStreamer (QObject * parent, Settings * settings);
    ~GStreamer ();
public slots:
    virtual bool ready (Viewer *);
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
    virtual bool stop ();
    virtual bool quit ();
private slots:
    void processStopped (KProcess *);
};

#ifdef HAVE_NSPR
/*
 * npplayer backend
 */
class KMPLAYER_NO_EXPORT NpPlayer : public Process {
    Q_OBJECT
public:
    enum Reason {
        NoReason = -1,
        BecauseDone = 0, BecauseError = 1, BecauseStopped = 2
    };

    NpPlayer (QObject * parent, Settings * settings, const QString & srv);
    ~NpPlayer ();
    virtual void init ();
    virtual bool deMediafiedPlay ();
    virtual void initProcess (Viewer * viewer);
    virtual QString menuName () const;

    void setStarted (const QString & srv);
    void requestStream (const QString & url);
    void finishStream (Reason because);

    KDE_NO_EXPORT const QString & destination () const { return service; }
    KDE_NO_EXPORT const QString & interface () const { return iface; }
    KDE_NO_EXPORT QString objectPath () const { return path; }
public slots:
    virtual bool stop ();
    virtual bool quit ();
public slots:
    bool ready (Viewer *);
private slots:
    void processOutput (KProcess *, char *, int);
    void processStopped (KProcess *);
    void wroteStdin (KProcess *);
    void slotResult (KIO::Job*);
    void slotData (KIO::Job*, const QByteArray& qb);
protected:
    virtual void terminateJobs ();
private:
    void sendFinish (Reason because);
    QString service;
    QString iface;
    QString path;
    QString filter;
    QString remote_service;
    KIO::TransferJob * job;
    unsigned int bytes;
    bool write_in_progress;
    Reason finish_reason;
};
#endif

} // namespace

#endif //_KMPLAYERPROCESS_H_
