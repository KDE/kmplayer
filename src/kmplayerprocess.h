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

class QWidget;
class KProcess;
class KMPlayer;
class KMPlayerSource;
class KMPlayerCallback;
class KMPlayerBackend_stub;

class KMPlayerProcess : public QObject {
    Q_OBJECT
public:
    KMPlayerProcess (KMPlayer * player);
    virtual ~KMPlayerProcess ();
    virtual void init ();
    virtual void initProcess ();
    bool playing () const;
    KMPlayerSource * source () const { return m_source; }
    KProcess * process () const { return m_process; }
    virtual WId widget ();
    void setSource (KMPlayerSource * source);
    virtual bool grabPicture (const KURL & url, int pos);
signals:
    // backend process is running
    void started ();
    // backend process has ended
    void finished ();
    void positionChanged (int pos);
    void loading (int percentage);
    // backend process start to play (after filling its cache)
    void startPlaying ();
    void grabReady (const QString & path);
public slots:
    virtual bool play () = 0;
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
    KMPlayer * m_player;
    KMPlayerSource * m_source;
    KProcess * m_process;
    QString m_url;
    int m_request_seek;
protected slots:
    // QTimer::singleShot slots for the signals
    void emitStarted () { emit started (); }
    void emitFinished () { emit finished (); }
};

class MPlayerBase : public KMPlayerProcess {
    Q_OBJECT
public:
    MPlayerBase (KMPlayer * player);
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
    MPlayer (KMPlayer * player);
    ~MPlayer ();
    virtual void init ();
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
private slots:
    void processOutput (KProcess *, char *, int);
private:
    QString m_process_output;
    QString m_grabfile;
    QWidget * m_widget;
    MPlayerPreferencesPage * m_configpage;
    QString m_tmpURL;
};

class MPlayerPreferencesPage : public KMPlayerPreferencesPage {
public:
    enum Pattern {
        pat_size = 0, pat_cache, pat_pos, pat_index,
        pat_refurl, pat_ref, pat_start,
        pat_dvdlang, pat_dvdsub, pat_dvdtitle, pat_dvdchapter, pat_vcdtrack,
        pat_last
    };
    MPlayerPreferencesPage (MPlayer *);
    ~MPlayerPreferencesPage () {}
    void write (KConfig *);
    void read (KConfig *);
    void sync (bool fromUI);
    void prefLocation (QString & item, QString & icon, QString & tab);
    QFrame * prefPage (QWidget * parent);
    QRegExp m_patterns[pat_last];
    int cachesize;
    QString additionalarguments;
private:
    MPlayer * m_process;
    MPlayerPreferencesFrame * m_configframe;
};

class Recorder {
public:
    const KURL & recordURL () const { return m_recordurl; }
    void setURL (const KURL & url) { m_recordurl = url; }
protected:
    KURL m_recordurl;
};

class MEncoder : public MPlayerBase, public Recorder {
    Q_OBJECT
public:
    MEncoder (KMPlayer * player);
    ~MEncoder ();
    virtual void init ();
    const KURL & recordURL () const { return m_recordurl; }
public slots:
    virtual bool play ();
    virtual bool stop ();
};

class KMPlayerXMLPreferencesPage;
class KMPlayerXMLPreferencesFrame;

class KMPlayerCallbackProcess : public KMPlayerProcess {
    Q_OBJECT
public:
    KMPlayerCallbackProcess (KMPlayer * player);
    ~KMPlayerCallbackProcess ();
    virtual void setURL (const QString & url);
    virtual void setStatusMessage (const QString & msg);
    virtual void setErrorMessage (int code, const QString & msg);
    virtual void setFinished ();
    virtual void setPlaying ();
    virtual void setStarted (QByteArray & data);
    virtual void setMovieParams (int length, int width, int height, float aspect);
    virtual void setMoviePosition (int position);
    virtual void setLoadingProgress (int percentage);
    QByteArray & configData () { return m_configdata; }
    bool haveConfig () { return m_have_config == config_yes; }
    bool getConfigData ();
    void setChangedData (const QByteArray &);
protected:
    KMPlayerCallback * m_callback;
    KMPlayerBackend_stub * m_backend;
    QByteArray m_configdata;
    QByteArray m_changeddata;
    KMPlayerXMLPreferencesPage * m_configpage;
    enum { config_unknown, config_probe, config_yes, config_no } m_have_config;
    enum { send_no, send_try, send_new } m_send_config;
};

class KMPlayerXMLPreferencesPage : public KMPlayerPreferencesPage {
public:
    KMPlayerXMLPreferencesPage (KMPlayerCallbackProcess *);
    ~KMPlayerXMLPreferencesPage () {}
    void write (KConfig *);
    void read (KConfig *);
    void sync (bool fromUI);
    void prefLocation (QString & item, QString & icon, QString & tab);
    QFrame * prefPage (QWidget * parent);
private:
    KMPlayerCallbackProcess * m_process;
    KMPlayerXMLPreferencesFrame * m_configframe;
};

class Xine : public KMPlayerCallbackProcess {
    Q_OBJECT
public:
    Xine (KMPlayer * player);
    ~Xine ();
    WId widget ();
    void setFinished ();
    void initProcess ();
public slots:
    bool play ();
    bool stop ();
    bool quit ();
    bool pause ();
    bool saturation (int pos, bool absolute);
    bool hue (int pos, bool absolute);
    bool contrast (int pos, bool absolute);
    bool brightness (int pos, bool absolute);
    bool seek (int pos, bool absolute);
    void setStarted (QByteArray & data);
private slots:
    void processStopped (KProcess *);
    void processOutput (KProcess *, char *, int);
};

class FFMpeg : public KMPlayerProcess, public Recorder {
    Q_OBJECT
public:
    FFMpeg (KMPlayer * player);
    ~FFMpeg ();
    virtual void init ();
    void setArguments (const QString & args) { arguments = args; }
public slots:
    virtual bool play ();
    virtual bool stop ();
private slots:
    void processStopped (KProcess *);
private:
    QString arguments;
};

#endif //_KMPLAYERPROCESS_H_
