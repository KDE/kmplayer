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

#include <qtextstream.h>
#include <qapplication.h>
#include <qmovie.h>
#include <qimage.h>
#include <qfile.h>
#include <qtextcodec.h>

#include <kdebug.h>
#include <kmimetype.h>
#include <kio/job.h>
#include <kio/jobclasses.h>
#include <kstaticdeleter.h>

#include "mediaobject.h"

namespace KMPlayer {

const unsigned int event_data_arrived = 100;
const unsigned int event_img_updated = 101;
const unsigned int event_img_anim_finished = 102;

}

using namespace KMPlayer;

namespace {

    typedef QMap <QString, ImageDataPtrW> ImageDataMap;

    static DataCache *memory_cache;
    static ImageDataMap *image_data_map;

    struct GlobalMediaData {
        GlobalMediaData () {
            memory_cache = new DataCache;
            image_data_map = new ImageDataMap;
        }
        ~GlobalMediaData ();
    };

    static GlobalMediaData *global_media;
    static KStaticDeleter <GlobalMediaData> globalMediaDataDeleter;

    GlobalMediaData::~GlobalMediaData () {
        delete memory_cache;
        delete image_data_map;
        global_media = NULL;
    }
}

//------------------------%<----------------------------------------------------

MediaManager::MediaManager (PartBase *player) : m_player (player) {
    if (!global_media)
        globalMediaDataDeleter.setObject (global_media, new GlobalMediaData);
}

MediaManager::~MediaManager () {
}

MediaObject *MediaManager::createMedia (MediaType type, Node *node) {
    switch (type) {
        case AudioVideo:
            return new AudioVideoMedia (this, node);
        case Image:
            return new ImageMedia (this, node);
        case Text:
            return new TextMedia (this, node);
    }
    return NULL;
}

//------------------------%<----------------------------------------------------

void DataCache::add (const QString & url, const QByteArray & data) {
    QByteArray bytes;
    bytes.duplicate (data);
    cache_map.insert (url, bytes);
    preserve_map.erase (url);
    emit preserveRemoved (url);
}

bool DataCache::get (const QString & url, QByteArray & data) {
    DataMap::const_iterator it = cache_map.find (url);
    if (it != cache_map.end ()) {
        data.duplicate (it.data ());
        return true;
    }
    return false;
}

bool DataCache::preserve (const QString & url) {
    PreserveMap::const_iterator it = preserve_map.find (url);
    if (it == preserve_map.end ()) {
        preserve_map.insert (url, true);
        return true;
    }
    return false;
}

bool DataCache::isPreserved (const QString & url) {
    return preserve_map.find (url) != preserve_map.end ();
}

bool DataCache::unpreserve (const QString & url) {
    const PreserveMap::iterator it = preserve_map.find (url);
    if (it == preserve_map.end ())
        return false;
    preserve_map.erase (it);
    emit preserveRemoved (url);
    return true;
}

MediaObject::MediaObject (MediaManager *manager, Node *node)
 : m_manager (manager), m_node (node), job (NULL), preserve_wait (false) {
}

MediaObject::~MediaObject () {
    clearData ();
}

KDE_NO_EXPORT void MediaObject::killWGet () {
    if (job) {
        job->kill (); // quiet, no result signal
        job = 0L;
        memory_cache->unpreserve (url);
    } else if (preserve_wait) {
        disconnect (memory_cache, SIGNAL (preserveRemoved (const QString &)),
                    this, SLOT (cachePreserveRemoved (const QString &)));
        preserve_wait = false;
    }
}

/**
 * Gets contents from url and puts it in m_data
 */
KDE_NO_EXPORT bool MediaObject::wget (const QString &str) {
    clearData ();
    url = str;
    KURL kurl (str);
    if (kurl.isLocalFile ()) {
        QFile file (kurl.path ());
        if (file.exists () && file.open (IO_ReadOnly)) {
            data = file.readAll ();
            file.close ();
        }
        remoteReady (str);
        return true;
    }
    if (memory_cache->get (str, data)) {
        //kdDebug () << "download found in cache " << str << endl;
        remoteReady (str);
        return true;
    }
    if (memory_cache->preserve (str)) {
        //kdDebug () << "downloading " << str << endl;
        job = KIO::get (kurl, false, false);
        connect (job, SIGNAL (data (KIO::Job *, const QByteArray &)),
                this, SLOT (slotData (KIO::Job *, const QByteArray &)));
        connect (job, SIGNAL (result (KIO::Job *)),
                this, SLOT (slotResult (KIO::Job *)));
        connect (job, SIGNAL (mimetype (KIO::Job *, const QString &)),
                this, SLOT (slotMimetype (KIO::Job *, const QString &)));
    } else {
        //kdDebug () << "download preserved " << str << endl;
        connect (memory_cache, SIGNAL (preserveRemoved (const QString &)),
                 this, SLOT (cachePreserveRemoved (const QString &)));
        preserve_wait = true;
    }
    return false;
}

KDE_NO_EXPORT QString MediaObject::mimetype () {
    if (data.size () > 0 && mime.isEmpty ()) {
        int accuraty;
        KMimeType::Ptr mimep = KMimeType::findByContent (data, &accuraty);
        if (mimep)
            mime = mimep->name ();
    }
    return mime;
}

KDE_NO_EXPORT void MediaObject::clearData () {
    killWGet ();
    url.truncate (0);
    mime.truncate (0);
    data.resize (0);
}

KDE_NO_EXPORT bool MediaObject::downloading () const {
    return !!job;
}

KDE_NO_EXPORT void MediaObject::slotResult (KIO::Job * kjob) {
    if (!kjob->error ())
        memory_cache->add (url, data);
    else
        data.resize (0);
    job = 0L; // signal KIO::Job::result deletes itself
    remoteReady (url);
}

KDE_NO_EXPORT void MediaObject::remoteReady (const QString &) {
    if (m_node)
        m_node->handleEvent (new Event (event_data_arrived));
}

KDE_NO_EXPORT void MediaObject::cachePreserveRemoved (const QString & str) {
    if (str == url && !memory_cache->isPreserved (str)) {
        preserve_wait = false;
        disconnect (memory_cache, SIGNAL (preserveRemoved (const QString &)),
                    this, SLOT (cachePreserveRemoved (const QString &)));
        wget (str);
    }
}

KDE_NO_EXPORT void MediaObject::slotData (KIO::Job*, const QByteArray& qb) {
    if (qb.size ()) {
        int old_size = data.size ();
        data.resize (old_size + qb.size ());
        memcpy (data.data () + old_size, qb.data (), qb.size ());
    }
}

KDE_NO_EXPORT void MediaObject::slotMimetype (KIO::Job *, const QString & m) {
    mime = m;
}

//------------------------%<----------------------------------------------------

AudioVideoMedia::AudioVideoMedia (MediaManager *manager, Node *node)
 : MediaObject (manager, node) {}

AudioVideoMedia::~AudioVideoMedia () {
    m_process->stop ();
    // delete m_process;
}

void AudioVideoMedia::clipStart () {
    m_process->play (m_node->mrl ());
}

void AudioVideoMedia::clipStop () {
    m_process->stop ();
}

void AudioVideoMedia::pause () {
    m_process->pause ();
}

void AudioVideoMedia::unpause () {
    m_process->pause ();
}

IProcess *AudioVideoMedia::process () {
    return m_process;
}

IViewer *AudioVideoMedia::viewer () {
    return m_viewer;
}

//------------------------%<----------------------------------------------------

ImageData::ImageData( const QString & img) : image (0L), url (img) {
    //if (img.isEmpty ())
    //    //kdDebug() << "New ImageData for " << this << endl;
    //else
    //    //kdDebug() << "New ImageData for " << img << endl;
}

ImageData::~ImageData() {
    if (!url.isEmpty ())
        image_data_map->erase (url);
    delete image;
}

ImageMedia::ImageMedia (MediaManager *manager, Node *node)
 : MediaObject (manager, node), img_movie (NULL) {}

ImageMedia::~ImageMedia () {}

KDE_NO_EXPORT void ImageMedia::clipStart () {
    if (img_movie) {
        img_movie->restart ();
        if (img_movie->paused ())
            img_movie->unpause ();
    }
}

KDE_NO_EXPORT void ImageMedia::clipStop () {
    pause ();
}

void ImageMedia::pause () {
    if (img_movie && !img_movie->paused ())
        img_movie->pause ();
}

void ImageMedia::unpause () {
    if (img_movie && img_movie->paused ())
        img_movie->unpause ();
}

KDE_NO_EXPORT void ImageMedia::remoteReady (const QString &url) {
    if (data.size ()) {
        QString mime = mimetype ();
        if (!mime.startsWith (QString::fromLatin1 ("text/"))) { // FIXME svg
            setUrl (url);
            delete img_movie;
            img_movie = 0L;
            QImage *pix = isEmpty () ? new QImage (data) : cached_img->image;
            if (!pix->isNull ()) {
                cached_img->image = pix;
                img_movie = new QMovie (data, data.size ());
                img_movie->connectUpdate(this,SLOT(movieUpdated(const QRect&)));
                img_movie->connectStatus (this, SLOT (movieStatus (int)));
                img_movie->connectResize(this,SLOT (movieResize(const QSize&)));
                frame_nr = 0;
            } else {
                delete pix;
            }
        }
    }
    MediaObject::remoteReady (url);
}

bool ImageMedia::isEmpty () {
    return !cached_img || !cached_img->image;
}

void ImageMedia::setUrl (const QString & url) {
    if (url.isEmpty ()) {
        cached_img = ImageDataPtr (new ImageData (url));
    } else {
        ImageDataMap::iterator i = image_data_map->find (url);
        if (i == image_data_map->end ()) {
            cached_img = ImageDataPtr (new ImageData (url));
            image_data_map->insert (url, ImageDataPtrW (cached_img));
        } else {
            cached_img = i.data ();
        }
    }
}

KDE_NO_EXPORT void ImageMedia::movieResize (const QSize &) {
    //kdDebug () << "movieResize" << endl;
    if (m_node)
        m_node->handleEvent (new Event (event_img_updated));
}

KDE_NO_EXPORT void ImageMedia::movieUpdated (const QRect &) {
    if (frame_nr++) {
        setUrl (QString ());
        ASSERT (cached_img && isEmpty ());
        cached_img->image = new QImage;
        *cached_img->image = img_movie->framePixmap ();
        if (m_node)
            m_node->handleEvent (new Event (event_img_updated));
    }
}

KDE_NO_EXPORT void ImageMedia::movieStatus (int status) {
    if (QMovie::EndOfMovie == status && m_node)
        m_node->handleEvent (new Event (event_img_anim_finished));
}

//------------------------%<----------------------------------------------------

TextMedia::TextMedia (MediaManager *manager, Node *node)
 : MediaObject (manager, node), codec (NULL) {
    default_font_size = QApplication::font ().pointSize ();
}

TextMedia::~TextMedia () {
    delete codec;
}

KDE_NO_EXPORT void TextMedia::remoteReady (const QString &url) {
    if (data.size ()) {
        if (!data [data.size () - 1])
            data.resize (data.size () - 1); // strip zero terminate char
        QTextStream ts (data, IO_ReadOnly);
        if (codec)
            ts.setCodec (codec);
        text  = ts.read ();
    }
    MediaObject::remoteReady (url);
}

#include "mediaobject.moc"
