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

#ifndef KMPLAYER_PART_H
#define KMPLAYER_PART_H

#include <kmediaplayer/player.h>
#include <kparts/browserextension.h>
#include <kparts/factory.h>
#include <kurl.h>
#include <qobject.h>
#include <qvaluelist.h>
#include <qstringlist.h>
#include <qguardedptr.h>
#include <qregexp.h>
#include "kmplayerview.h"
#include "kmplayersource.h"


class KProcess;
class KAboutData;
class KMPlayer;
class KMPlayerSettings;
class KInstance;
class KConfig;

/**
  *@author Koos Vriezen
  */
class KMPlayerBrowserExtension : public KParts::BrowserExtension {
    Q_OBJECT
public:
    KMPlayerBrowserExtension(KMPlayer *parent);
    void urlChanged (const QString & url);
    void setLoadingProgress (int percentage);

    void setURLArgs (const KParts::URLArgs & args);
    void saveState (QDataStream & stream);
    void restoreState (QDataStream & stream);
};

class KMPlayerLiveConnectExtension : public KParts::LiveConnectExtension {
    Q_OBJECT
public:
    KMPlayerLiveConnectExtension (KMPlayer * parent);
    ~KMPlayerLiveConnectExtension ();

    // LiveConnect interface
    bool get (const unsigned long, const QString &,
            KParts::LiveConnectExtension::Type &, unsigned long &, QString &);
    bool put (const unsigned long, const QString &, const QString &);
    bool call (const unsigned long, const QString &,
            const QStringList &, KParts::LiveConnectExtension::Type &, 
            unsigned long &, QString &);
    void unregister (const unsigned long);
    void sendEvent(const unsigned long objid, const QString & event, const KParts::LiveConnectExtension::ArgList & args ) {
        emit partEvent(objid, event, args);
    }

    void enableFinishEvent (bool b = true) { m_enablefinish = b; }
signals:
    void partEvent (const unsigned long, const QString &,
                    const KParts::LiveConnectExtension::ArgList &);
public slots:
    void setSize (int w, int h);
private slots:
    void started ();
    void finished ();
private:
    KMPlayer * player;
    bool m_started : 1;
    bool m_enablefinish : 1;
};


class KMPlayerURLSource : public KMPlayerSource {
    Q_OBJECT
public:
    KMPlayerURLSource (KMPlayer * player, const KURL & url = KURL ());
    virtual ~KMPlayerURLSource ();

    virtual bool processOutput (const QString & line);
    virtual bool hasLength ();

    void setURL (const KURL & url);
    const KURL & url () const { return m_url; }
public slots:
    virtual void init ();
    virtual void activate ();
    virtual void deactivate ();
    virtual void play ();
    virtual void finished ();
private:
    QValueList <KURL> m_urls;
    KURL m_url;
    KURL m_urlother;
    bool isreference;
    bool foundnonreference;
};


class KMPlayerHRefSource : public KMPlayerSource {
    Q_OBJECT
public:
    KMPlayerHRefSource (KMPlayer * player);
    virtual ~KMPlayerHRefSource ();
    virtual bool processOutput (const QString & line);
    virtual bool hasLength ();

    //void setURL (const KURL & url) { m_url = url; }
    void setURL (const KURL &);
    KURL url () const { return m_url; }
public slots:
    virtual void init ();
    virtual void activate ();
    virtual void deactivate ();
    virtual void play ();
    virtual void finished ();
private:
    KURL m_url;
    bool m_finished;
};


class KMPlayer : public KMediaPlayer::Player  {
    Q_OBJECT
public:
    KMPlayer (QWidget * parent, KConfig *);
    KMPlayer (QWidget * wparent, const char * wname,
              QObject * parent, const char * name, const QStringList &args);
    ~KMPlayer ();
    virtual KMediaPlayer::View* view ();
    static KAboutData* createAboutData ();

    KMPlayerSettings * settings () const { return m_settings; }
    KProcess * process () const { return m_process; }
    void initProcess ();
    int seekTime () const { return m_seektime; }
    void setSeekTime (int t) { m_seektime = t; }
    void keepMovieAspect (bool);
    KURL url () const { return m_urlsource->url (); }
    void setURL (const KURL & url) { m_urlsource->setURL (url); }
    KMPlayerBrowserExtension * browserextension() const
        { return m_browserextension; }
    KMPlayerLiveConnectExtension * liveconnectextension () const
        { return m_liveconnectextension; }
    void sizes (int & w, int & h) const;
    void setMovieLength (int len);
    void setSource (KMPlayerSource * source, bool keepsizes = false);
    KMPlayerSource * source () const { return m_source; }
    KMPlayerURLSource * urlSource () const { return m_urlsource; }
    KMPlayerHRefSource * hrefSource () const { return m_hrefsource; }
    bool autoPlay () const { return m_autoplay; }
public slots:
    virtual bool openURL (const KURL & url);
    virtual bool closeURL ();
    virtual void pause (void);
    virtual void play (void);
    virtual void stop (void);
    void record ();
    virtual void seek (unsigned long msec);
    virtual void seekPercent (float per);

    void adjustVolume (int incdec);
    bool run (const char * args, const char * pipe = 0L);
    bool playing () const;
    void showConfigDialog ();
    void setMenuZoom (int id);
public:
    virtual bool isSeekable (void) const { return m_source->isSeekable (); }
    virtual unsigned long position (void) const { return m_movie_position; }
    virtual bool hasLength (void) const { return m_source->hasLength (); }
    virtual unsigned long length (void) const { return m_source->length (); }
signals:
    void finished ();
protected:
    bool openFile();
    void timerEvent (QTimerEvent *);
private slots:
    void processOutput (KProcess *, char *, int);
    void processStopped (KProcess *);
    void processDataWritten (KProcess *);
    void back ();
    void forward ();
    void posSliderPressed ();
    void posSliderReleased ();
    void positonValueChanged (int val);
    void contrastValueChanged (int val);
    void brightnessValueChanged (int val);
    void hueValueChanged (int val);
    void saturationValueChanged (int val);
private:
    void init ();
    void sendCommand (const QString &);
    KConfig * m_config;
    QGuardedPtr <KMPlayerView> m_view;
    KMPlayerSettings * m_settings;
    KMPlayerSource * m_source;
    KMPlayerURLSource * m_urlsource;
    KMPlayerHRefSource * m_hrefsource;
    KProcess * m_process;
    KMPlayerBrowserExtension * m_browserextension;
    KMPlayerLiveConnectExtension * m_liveconnectextension;
    QRegExp m_posRegExp;
    QRegExp m_cacheRegExp;
    QRegExp m_indexRegExp;
    QStringList commands;
    KURL m_recordurl;
    QString m_process_output;
    int m_seektime;
    int movie_width;
    int movie_height;
    int m_movie_position;
    bool m_started_emited : 1;
    bool m_autoplay : 1;
    bool m_ispart : 1;
    bool m_use_slave : 1;
    bool m_recording : 1;
    bool m_bPosSliderPressed : 1;
    bool m_havehref : 1;
};


class KMPlayerFactory : public KParts::Factory {
    Q_OBJECT
public:
    KMPlayerFactory ();
    virtual ~KMPlayerFactory ();
    virtual KParts::Part *createPartObject 
        (QWidget *wparent, const char *wname,
         QObject *parent, const char *name,
         const char *className, const QStringList &args);
    static KInstance * instance () { return s_instance; }
private:
    static KInstance * s_instance;
};

#endif
