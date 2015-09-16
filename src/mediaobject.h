/**
  This file belong to the KMPlayer project, a movie player plugin for Konqueror
  Copyright (C) 2007  Koos Vriezen <koos.vriezen@gmail.com>

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
**/

#ifndef _KMPLAYER__MEDIA_OBJECT_H_
#define _KMPLAYER__MEDIA_OBJECT_H_

#include <config-kmplayer.h>

#include <qobject.h>
#include <QtCore/QPair>
#include <qmap.h>
#include <qstring.h>
#include <QMovie>
#include <QList>

#include "kmplayer_def.h"
#include "kmplayerplaylist.h"

class QMovie;
class QImage;
class QSvgRenderer;
class QBuffer;
class QByteArray;
class KJob;
namespace KIO {
    class Job;
}

namespace KMPlayer {

class IViewer;
class PartBase;
class ProcessUser;
class ProcessInfo;
class MediaManager;
class AudioVideoMedia;
class PreferencesPage;
class MediaObject;
class CalculatedSizer;
class Surface;


class KMPLAYER_EXPORT IProcess {
public:
    enum State { NotRunning = 0, Ready, Buffering, Playing, Paused };

    virtual ~IProcess () {}

    virtual bool ready () = 0;
    virtual bool play () = 0;
    virtual void pause () = 0;
    virtual void unpause () = 0;
    virtual bool grabPicture (const QString &file, int frame) = 0;
    virtual void stop () = 0;
    virtual void quit () = 0;
    /* seek (pos, abs) seek position in deci-seconds */
    virtual bool seek (int pos, bool absolute) = 0;
    /* volume from 0 to 100 */
    virtual void volume (int pos, bool absolute) = 0;
    virtual bool saturation (int pos, bool absolute) = 0;
    virtual bool hue (int pos, bool absolute) = 0;
    virtual bool contrast (int pos, bool absolute) = 0;
    virtual bool brightness (int pos, bool absolute) = 0;
    virtual void setAudioLang (int id) = 0;
    virtual void setSubtitle (int id) = 0;
    virtual bool running () const = 0;

    State state () const { return m_state; }
    ProcessUser *user;
    ProcessInfo *process_info;

protected:
    IProcess (ProcessInfo *pinfo);

    State m_state;

private:
    IProcess (const IViewer &);
};

class KMPLAYER_EXPORT ProcessUser {
public:
    virtual ~ProcessUser () {}

    virtual void starting (IProcess *) = 0;
    virtual void stateChange (IProcess *, IProcess::State, IProcess::State) = 0;
    virtual void processDestroyed (IProcess*) = 0;
    virtual IViewer *viewer () = 0;
    virtual Mrl *getMrl () = 0;
};

class KMPLAYER_EXPORT ProcessInfo {
public:
    ProcessInfo (const char *nm, const QString &lbl, const char **supported,
            MediaManager *, PreferencesPage *);
    virtual ~ProcessInfo ();

    bool supports (const char *source) const;
    virtual IProcess *create (PartBase*, ProcessUser*) = 0;
    virtual void quitProcesses () {};

    const char *name;
    QString label;
    const char **supported_sources;
    MediaManager *manager;
    PreferencesPage *config_page;
};

/*
 * Class that creates MediaObject and keeps track objects
 */
class KMPLAYER_EXPORT MediaManager {
public:
    enum MediaType { Any, Audio, AudioVideo, Image, Text, Data };
    typedef QMap <QString, ProcessInfo *> ProcessInfoMap;
    typedef QList <IProcess *> ProcessList;
    typedef QList <MediaObject *> MediaList;

    MediaManager (PartBase *player);
    ~MediaManager ();

    MediaObject *createAVMedia (Node *node, const QByteArray &b);

    // backend process state changed
    void stateChange (AudioVideoMedia *m, IProcess::State o, IProcess::State n);
    void playAudioVideo (AudioVideoMedia *m);
    void grabPicture (AudioVideoMedia *m);

    void processDestroyed (IProcess *process);
    ProcessInfoMap &processInfos () { return m_process_infos; }
    ProcessList &processes () { return m_processes; }
    ProcessInfoMap &recorderInfos () { return m_record_infos; }
    ProcessList &recorders () { return m_recorders; }
    MediaList &medias () { return m_media_objects; }
    PartBase *player () const { return m_player; }

private:
    MediaList m_media_objects;
    ProcessInfoMap m_process_infos;
    ProcessList m_processes;
    ProcessInfoMap m_record_infos;
    ProcessList m_recorders;
    PartBase *m_player;
};


//------------------------%<----------------------------------------------------

/*
 * Abstract base of MediaObject types, handles downloading
 */

class KMPLAYER_NO_EXPORT DataCache : public QObject {
    Q_OBJECT
    typedef QMap <QString, QPair <QString, QByteArray> > DataMap;
    typedef QMap <QString, bool> PreserveMap;
    DataMap cache_map;
    PreserveMap preserve_map;
public:
    DataCache () {}
    ~DataCache () {}
    void add (const QString &, const QString &, const QByteArray &);
    bool get (const QString &, QString &, QByteArray &);
    bool preserve (const QString &);
    bool unpreserve (const QString &);
    bool isPreserved (const QString &);
signals:
    void preserveRemoved (const QString &); // ready or canceled
};

class KMPLAYER_EXPORT MediaObject : public QObject {
    Q_OBJECT
    friend class MediaManager;
public:
    virtual MediaManager::MediaType type () const = 0;

    virtual bool play () = 0;
    virtual void pause () {}
    virtual void unpause () {}
    virtual void stop () {}
    virtual void destroy ();

    Mrl *mrl ();

protected:
    MediaObject (MediaManager *manager, Node *node);
    virtual ~MediaObject ();

    MediaManager *m_manager;
    NodePtrW m_node;
};

//------------------------%<----------------------------------------------------

class KMPLAYER_EXPORT MediaInfo : public QObject {
    Q_OBJECT
public:
    MediaInfo (Node *node, MediaManager::MediaType type);
    ~MediaInfo ();

    bool wget (const QString &url, const QString &from_domain=QString ());
    void killWGet ();
    void clearData ();
    QString mimetype ();
    bool downloading () const;
    void create ();

    QByteArray &rawData () { return data; }
    MediaObject *media;
    QString url;
    QByteArray data;
    QString mime;
    MediaManager::MediaType type;

private slots:
    void slotResult (KJob *);
    void slotData (KIO::Job *, const QByteArray &qb);
    void slotMimetype (KIO::Job *job, const QString &mimestr);
    void cachePreserveRemoved (const QString &);

private:
    void ready ();
    bool readChildDoc ();
    void setMimetype (const QString &);

    Node *node;
    KIO::Job *job;
    QString cross_domain;
    QString access_from;
    bool preserve_wait;
    bool check_access;
};

//------------------------%<----------------------------------------------------

/*
 * MediaObject for audio/video, groups Mrl, Process and Viewer
 */

typedef unsigned long WindowId;

class AudioVideoMedia;

class KMPLAYER_NO_EXPORT IViewer {
public:
    enum Monitor {
        MonitorNothing = 0, MonitorMouse = 1, MonitorKey = 2 , MonitorAll = 3
    };
    IViewer () {}
    virtual ~IViewer () {}

    virtual WindowId windowHandle () = 0;
    virtual WindowId clientHandle () = 0;
    virtual WindowId ownHandle() = 0;
    virtual void setGeometry (const IRect &rect) = 0;
    virtual void setAspect (float a) = 0;
    virtual float aspect () = 0;
    virtual void useIndirectWidget (bool) = 0;
    virtual void setMonitoring (Monitor) = 0;
    virtual void map () = 0;
    virtual void unmap () = 0;
    virtual void embedded() = 0;
private:
    IViewer (const IViewer &);
};

class KMPLAYER_NO_EXPORT AudioVideoMedia : public MediaObject, ProcessUser {
    friend class MediaManager;
public:
    enum Request {
        ask_nothing, ask_play, ask_pause, ask_grab, ask_stop, ask_delete
    };

    AudioVideoMedia (MediaManager *manager, Node *node);

    MediaManager::MediaType type () const { return MediaManager::AudioVideo; }

    virtual bool play ();
    virtual bool grabPicture (const QString &file, int frame);
    virtual void stop ();
    virtual void pause ();
    virtual void unpause ();
    virtual void destroy ();

    virtual void starting (IProcess *);
    virtual void stateChange (IProcess *, IProcess::State, IProcess::State);
    virtual void processDestroyed (IProcess *p);
    virtual IViewer *viewer ();
    virtual Mrl *getMrl ();

    void setViewer (IViewer *v) { m_viewer = v; }

    IProcess *process;
    IViewer *m_viewer;
    QString m_grab_file;
    int m_frame;
    Request request;

protected:
    ~AudioVideoMedia ();
};


//------------------------%<----------------------------------------------------

/*
 * MediaObject for (animated)images
 */

struct KMPLAYER_NO_EXPORT ImageData {
    enum ImageFlags {
        ImageAny=0, ImagePixmap=0x1, ImageAnimated=0x2, ImageScalable=0x4
    };
    ImageData( const QString & img);
    ~ImageData();
    void setImage (QImage *img);
#ifdef KMPLAYER_WITH_CAIRO
    void copyImage (Surface *s, const SSize &sz, cairo_surface_t *similar, CalculatedSizer *zoom=NULL);
#endif
    bool isEmpty () const {
        return !image
#ifdef KMPLAYER_WITH_CAIRO
            && !surface
#endif
            ;
    }

    unsigned short width;
    unsigned short height;
    short flags;
    bool has_alpha;
private:
    QImage *image;
#ifdef KMPLAYER_WITH_CAIRO
    cairo_surface_t *surface;
#endif
    QString url;
};

typedef SharedPtr <ImageData> ImageDataPtr;
typedef WeakPtr <ImageData> ImageDataPtrW;

class KMPLAYER_NO_EXPORT ImageMedia : public MediaObject {
    Q_OBJECT
public:
    ImageMedia (MediaManager *manager, Node *node,
            const QString &url, const QByteArray &data);
    ImageMedia (Node *node, ImageDataPtr id = NULL);

    MediaManager::MediaType type () const { return MediaManager::Image; }

    bool play ();
    void stop ();
    void pause ();
    void unpause ();

    bool wget (const QString &url);
    bool isEmpty () const;
    void render (const ISize &size);
    void sizes (SSize &size);
    void updateRender ();

    ImageDataPtr cached_img;

private slots:
    void movieUpdated (const QRect &);
    void movieStatus (QMovie::MovieState);
    void movieResize (const QSize &);

protected:
    ~ImageMedia ();

private:
    void setupImage (const QString &url);

    QByteArray data;
    QBuffer *buffer;
    QMovie *img_movie;
    QSvgRenderer *svg_renderer;
    int frame_nr;
    bool update_render;
};

//------------------------%<----------------------------------------------------

/*
 * MediaObject for text
 */
class KMPLAYER_NO_EXPORT TextMedia : public MediaObject {
public:
    TextMedia (MediaManager *manager, Node *node, const QByteArray &ba);

    MediaManager::MediaType type () const { return MediaManager::Text; }

    bool play ();

    QString text;
    static int defaultFontSize ();

protected:
    ~TextMedia ();
};

} // namespace

#endif
