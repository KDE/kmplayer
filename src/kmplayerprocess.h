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
#include <qstringlist.h>
#include <kurl.h>

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
    bool playing () const;
    KMPlayerSource * source () const { return m_source; }
    KProcess * process () const { return m_process; }
    virtual QWidget * widget ();
    void setSource (KMPlayerSource * source) { m_source = source; }
signals:
    void started ();
    void finished ();
    void positionChanged (int pos);
    void loading (int percentage);
    void startPlaying ();
    void output (const QString & msg);
public slots:
    virtual bool play () = 0;
    virtual bool stop ();
    virtual bool pause ();
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
protected:
    bool sendCommand (const QString &);
    QStringList commands;
    bool m_use_slave : 1;
protected slots:
    virtual void processStopped (KProcess *);
private slots:
    void dataWritten (KProcess *);
};

class MPlayer : public MPlayerBase {
    Q_OBJECT
public:
    MPlayer (KMPlayer * player);
    ~MPlayer ();
    virtual void init ();
    virtual QWidget * widget ();
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
protected slots:
    void processStopped (KProcess *);
private slots:
    void processOutput (KProcess *, char *, int);
private:
    QString m_process_output;
    QWidget * m_widget;
    QRegExp m_posRegExp;
    QRegExp m_cacheRegExp;
    QRegExp m_indexRegExp;
};

class MEncoder : public MPlayerBase {
    Q_OBJECT
public:
    MEncoder (KMPlayer * player);
    ~MEncoder ();
    virtual void init ();
    const KURL & recordURL () const { return m_recordurl; }
public slots:
    virtual bool play ();
    virtual bool stop ();
private:
    KURL m_recordurl;
};

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
    virtual void setStarted ();
    virtual void setMovieParams (int length, int width, int height, float aspect);
    virtual void setMoviePosition (int position);
signals:
    void running ();
protected:
    KMPlayerCallback * m_callback;
    QStringList m_urls;
protected slots:
    void emitRunning () { emit running (); }
};

class Xine : public KMPlayerCallbackProcess {
    Q_OBJECT
public:
    Xine (KMPlayer * player);
    ~Xine ();
    QWidget * widget ();
public slots:
    bool play ();
    bool stop ();
    void setFinished ();
    bool pause ();
    bool saturation (int pos, bool absolute);
    bool hue (int pos, bool absolute);
    bool contrast (int pos, bool absolute);
    bool brightness (int pos, bool absolute);
private slots:
    void processRunning ();
    void processStopped (KProcess *);
private:
    KMPlayerBackend_stub * m_backend;
};

class FFMpeg : public KMPlayerProcess {
    Q_OBJECT
public:
    FFMpeg (KMPlayer * player);
    ~FFMpeg ();
    virtual void init ();
public slots:
    virtual bool play ();
    virtual bool stop ();
private slots:
    void processStopped (KProcess *);
};

#endif //_KMPLAYERPROCESS_H_
