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

#include <config.h>

#include <qobject.h>
#include <qvaluelist.h>
#include <qmap.h>
#include <qstring.h>

#include "kmplayer_def.h"
#include "kmplayerplaylist.h"

class QMovie;
class QTextCodec;
class QImage;
namespace KIO {
    class Job;
}

namespace KMPlayer {

extern const unsigned int event_data_arrived;
extern const unsigned int event_img_updated;
extern const unsigned int event_img_anim_finished;

class IProcess;
class IViewer;
class PartBase;


/*
 * Class that creates MediaObject and keeps track objects
 */
class KMPLAYER_EXPORT MediaManager {
public:
    enum MediaType { AudioVideo, Image, Text };

    MediaManager (PartBase *player);
    ~MediaManager ();

    MediaObject *createMedia (MediaType type, Node *node);
private:
    QValueList <MediaObject *> media_objects;
    PartBase *m_player;
};


//------------------------%<----------------------------------------------------

/*
 * Abstract base of MediaObject types, handles downloading
 */

class KMPLAYER_NO_EXPORT DataCache : public QObject {
    Q_OBJECT
    typedef QMap <QString, QByteArray> DataMap;
    typedef QMap <QString, bool> PreserveMap;
    DataMap cache_map;
    PreserveMap preserve_map;
public:
    DataCache () {}
    ~DataCache () {}
    void add (const QString &, const QByteArray &);
    bool get (const QString &, QByteArray &);
    bool preserve (const QString &);
    bool unpreserve (const QString &);
    bool isPreserved (const QString &);
signals:
    void preserveRemoved (const QString &); // ready or canceled
};

class KMPLAYER_EXPORT MediaObject : public QObject {
    Q_OBJECT
public:
    MediaObject (MediaManager *manager, Node *node);
    virtual ~MediaObject ();

    virtual MediaManager::MediaType type () const = 0;

    virtual void clipStart () {}
    virtual void clipStop () {}
    virtual void pause () {}
    virtual void unpause () {}

    bool wget (const QString & url);
    void killWGet ();
    void clearData ();
    QString mimetype ();
    bool downloading () const;

    Node *node ();
    QByteArray &rawData () { return data; }

private slots:
    void slotResult (KIO::Job *);
    void slotData (KIO::Job *, const QByteArray &qb);
    void slotMimetype (KIO::Job *job, const QString &mimestr);
    void cachePreserveRemoved (const QString &);

protected:
    virtual void remoteReady (const QString &url);

    MediaManager *m_manager;
    NodePtrW m_node;
    QString url;
    QByteArray data;
    QString mime;
    KIO::Job *job;
    bool preserve_wait;
};


//------------------------%<----------------------------------------------------

/*
 * MediaObject for audio/video, groups Mrl, Process and Viewer
 */

typedef unsigned long WindowId;

class AudioVideoMedia;

class KMPLAYER_NO_EXPORT IViewer {
public:
    virtual WindowId windowHandle () = 0;
    virtual void setGeometry (const IRect &rect) = 0;
protected:
    virtual ~IViewer () {}
private:
    IViewer (const IViewer &);
};

class KMPLAYER_EXPORT IProcess {
public:
    virtual bool play (Mrl *) = 0;
    virtual void pause () = 0;
    virtual void stop () = 0;
    /* seek (pos, abs) seek position in deci-seconds */
    virtual bool seek (int pos, bool absolute);
    /* volume from 0 to 100 */
    virtual bool volume (int pos, bool absolute);
    virtual bool running () = 0;
protected:
    virtual ~IProcess () {}
private:
    IProcess (const IViewer &);
};

class KMPLAYER_NO_EXPORT AudioVideoMedia : public MediaObject {
public:
    AudioVideoMedia (MediaManager *manager, Node *node);
    ~AudioVideoMedia ();

    MediaManager::MediaType type () const { return MediaManager::AudioVideo; }

    void clipStart ();
    void clipStop ();
    void pause ();
    void unpause ();

    IProcess *process ();
    IViewer *viewer();
private:
    IProcess *m_process;
    IViewer *m_viewer;
};


//------------------------%<----------------------------------------------------

/*
 * MediaObject for (animated)images
 */

struct KMPLAYER_NO_EXPORT ImageData {
    ImageData( const QString & img);
    ~ImageData();
    QImage *image;
private:
    QString url;
};

typedef SharedPtr <ImageData> ImageDataPtr;
typedef WeakPtr <ImageData> ImageDataPtrW;

struct KMPLAYER_NO_EXPORT CachedImage {
    void setUrl (const QString & url);
    bool isEmpty ();
    ImageDataPtr data;
};

class KMPLAYER_NO_EXPORT ImageMedia : public MediaObject {
    Q_OBJECT
public:
    ImageMedia (MediaManager *manager, Node *node);
    ~ImageMedia ();

    MediaManager::MediaType type () const { return MediaManager::Image; }

    void clipStart ();
    void clipStop ();
    void pause ();
    void unpause ();

    CachedImage cached_img;

private slots:
    void movieUpdated (const QRect &);
    void movieStatus (int);
    void movieResize (const QSize &);

protected:
    void remoteReady (const QString &url);

private:
    QMovie *img_movie;
    int frame_nr;
};

//------------------------%<----------------------------------------------------

/*
 * MediaObject for text
 */
class KMPLAYER_NO_EXPORT TextMedia : public MediaObject {
public:
    TextMedia (MediaManager *manager, Node *node);
    ~TextMedia ();

    MediaManager::MediaType type () const { return MediaManager::Text; }

    QString text;
    QTextCodec *codec;
    int default_font_size;

protected:
    void remoteReady (const QString &url);
};

} // namespace

#endif
