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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef _KMPLAYERPROCESS_H_
#define _KMPLAYERPROCESS_H_

#include <qobject.h>
#include <qstring.h>
#include <qcstring.h>
#include <qstringlist.h>
#include <qregexp.h>

#include <kurl.h>

#include "kmplayerconfig.h"
#include "kmplayersource.h"

class QWidget;
class KProcess;

namespace KMPlayer {
    
class PartBase;
class View;
class Source;
class Callback;
class Backend_stub;

class KMPLAYER_EXPORT Process : public QObject {
    Q_OBJECT
public:
    Process (PartBase * player, const char * n);
    virtual ~Process ();
    virtual void init ();
    virtual void initProcess ();
    virtual QString menuName () const;
    bool playing () const;
    KDE_NO_EXPORT Source * source () const { return m_source; }
    KDE_NO_EXPORT KProcess * process () const { return m_process; }
    KDE_NO_EXPORT PartBase * player () const { return m_player; }
    virtual WId widget ();
    View * view ();
    void setSource (Source * source);
    virtual bool grabPicture (const KURL & url, int pos);
    bool supports (const char * source) const;
signals:
    // backend process is running
    void started ();
    // backend process is playing
    void startedPlaying ();
    // backend process has ended
    void finished ();
    void grabReady (const QString & path);
    void loaded (int percentage);
    void positioned (int pos);
    void lengthFound (int len);
public slots:
    virtual bool play ();
    virtual bool stop ();
    virtual bool quit ();
    virtual bool pause ();
    /* seek (pos, abs) seek positon in deci-seconds */
    virtual bool seek (int pos, bool absolute);
    virtual bool volume (int pos, bool absolute);
    virtual bool saturation (int pos, bool absolute);
    virtual bool hue (int pos, bool absolute);
    virtual bool contrast (int pos, bool absolute);
    virtual bool brightness (int pos, bool absolute);
protected:
    PartBase * m_player;
    Source * m_source;
    KProcess * m_process;
    QString m_url;
    int m_request_seek;
    const char ** m_supported_sources;
protected slots:
    virtual void urlForPlaying (const QString &);
    // QTimer::singleShot slots for the signals
    void emitStarted () { emit started (); }
    void emitFinished () { emit finished (); }
};

class MPlayerBase : public Process {
    Q_OBJECT
public:
    MPlayerBase (PartBase * player, const char * n);
    ~MPlayerBase ();
    void initProcess ();
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

class MPlayer : public MPlayerBase {
    Q_OBJECT
public:
    MPlayer (PartBase * player);
    ~MPlayer ();
    virtual void init ();
    virtual QString menuName () const;
    virtual WId widget ();
    virtual bool grabPicture (const KURL & url, int pos);
    bool run (const char * args, const char * pipe = 0L);
public slots:
    virtual bool play ();
    virtual bool stop ();
    virtual bool pause ();
    virtual bool seek (int pos, bool absolute);
    virtual bool volume (int pos, bool absolute);
    virtual bool saturation (int pos, bool absolute);
    virtual bool hue (int pos, bool absolute);
    virtual bool contrast (int pos, bool absolute);
    virtual bool brightness (int pos, bool absolute);
    MPlayerPreferencesPage * configPage () const { return m_configpage; }
protected slots:
    void processStopped (KProcess *);
    void urlForPlaying (const QString &);
private slots:
    void processOutput (KProcess *, char *, int);
private:
    QString m_process_output;
    QString m_grabfile;
    QWidget * m_widget;
    MPlayerPreferencesPage * m_configpage;
    QString m_tmpURL;
};

class MPlayerPreferencesPage : public PreferencesPage {
public:
    enum Pattern {
        pat_size = 0, pat_cache, pat_pos, pat_index,
        pat_refurl, pat_ref, pat_start,
        pat_dvdlang, pat_dvdsub, pat_dvdtitle, pat_dvdchapter, pat_vcdtrack,
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
    QString additionalarguments;
    bool alwaysbuildindex;
private:
    MPlayer * m_process;
    MPlayerPreferencesFrame * m_configframe;
};

class KMPLAYER_EXPORT Recorder {
public:
    KDE_NO_EXPORT const KURL & recordURL () const { return m_recordurl; }
    KDE_NO_EXPORT void setURL (const KURL & url) { m_recordurl = url; }
protected:
    KURL m_recordurl;
};

class MEncoder : public MPlayerBase, public Recorder {
    Q_OBJECT
public:
    MEncoder (PartBase * player);
    ~MEncoder ();
    virtual void init ();
    KDE_NO_EXPORT const KURL & recordURL () const { return m_recordurl; }
public slots:
    virtual void urlForPlaying (const QString &);
    virtual bool stop ();
};

class MPlayerDumpstream : public MPlayerBase, public Recorder {
    Q_OBJECT
public:
    MPlayerDumpstream (PartBase * player);
    ~MPlayerDumpstream ();
    virtual void init ();
    KDE_NO_EXPORT const KURL & recordURL () const { return m_recordurl; }
public slots:
    virtual void urlForPlaying (const QString &);
    virtual bool stop ();
};

class XMLPreferencesPage;
class XMLPreferencesFrame;

class KMPLAYER_EXPORT CallbackProcess : public Process {
    Q_OBJECT
public:
    CallbackProcess (PartBase * player, const char * n);
    ~CallbackProcess ();
    virtual void setStatusMessage (const QString & msg);
    virtual void setErrorMessage (int code, const QString & msg);
    virtual void setFinished ();
    virtual void setPlaying ();
    virtual void setStarted (QCString dcopname, QByteArray & data);
    virtual void setMovieParams (int length, int width, int height, float aspect);
    virtual void setMoviePosition (int position);
    virtual void setLoadingProgress (int percentage);
    KDE_NO_EXPORT QByteArray & configData () { return m_configdata; }
    KDE_NO_EXPORT bool haveConfig () { return m_have_config == config_yes; }
    bool getConfigData ();
    void setChangedData (const QByteArray &);
    QString dcopName ();
public slots:
    bool stop ();
    bool pause ();
    bool saturation (int pos, bool absolute);
    bool hue (int pos, bool absolute);
    bool contrast (int pos, bool absolute);
    bool brightness (int pos, bool absolute);
signals:
    void configReceived ();
protected:
    virtual void runForConfig ();
    Callback * m_callback;
    Backend_stub * m_backend;
    QByteArray m_configdata;
    QByteArray m_changeddata;
    XMLPreferencesPage * m_configpage;
    bool in_gui_update;
    enum { config_unknown, config_probe, config_yes, config_no } m_have_config;
    enum { send_no, send_try, send_new } m_send_config;
    enum { status_stop, status_play, status_start } m_status;
};

class XMLPreferencesPage : public PreferencesPage {
public:
    XMLPreferencesPage (CallbackProcess *);
    KDE_NO_CDTOR_EXPORT ~XMLPreferencesPage () {}
    void write (KConfig *);
    void read (KConfig *);
    void sync (bool fromUI);
    void prefLocation (QString & item, QString & icon, QString & tab);
    QFrame * prefPage (QWidget * parent);
private:
    CallbackProcess * m_process;
    XMLPreferencesFrame * m_configframe;
};

class Xine : public CallbackProcess {
    Q_OBJECT
public:
    Xine (PartBase * player);
    ~Xine ();
    QString menuName () const;
    WId widget ();
    void setFinished ();
    void initProcess ();
public slots:
    virtual bool play ();
    virtual void urlForPlaying (const QString &);
    bool quit ();
    bool seek (int pos, bool absolute);
protected:
    virtual void runForConfig ();
private slots:
    void processStopped (KProcess *);
    void processOutput (KProcess *, char *, int);
};

class GStreamer : public CallbackProcess {
    Q_OBJECT
public:
    GStreamer (PartBase * player);
    ~GStreamer ();
    QString menuName () const;
    WId widget ();
    void setFinished ();
    void initProcess ();
public slots:
    virtual bool play ();
    virtual void urlForPlaying (const QString &);
    bool quit ();
    bool seek (int pos, bool absolute);
private slots:
    void processStopped (KProcess *);
    void processOutput (KProcess *, char *, int);
};

class KMPLAYER_EXPORT FFMpeg : public Process, public Recorder {
    Q_OBJECT
public:
    FFMpeg (PartBase * player);
    ~FFMpeg ();
    virtual void init ();
    void setArguments (const QString & args) { arguments = args; }
public slots:
    virtual void urlForPlaying (const QString &);
    virtual bool stop ();
private slots:
    void processStopped (KProcess *);
private:
    QString arguments;
};

} // namespace

#endif //_KMPLAYERPROCESS_H_
