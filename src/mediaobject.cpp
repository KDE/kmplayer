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
#include <klocale.h>
#include <kio/job.h>
#include <kio/jobclasses.h>
#include <kstaticdeleter.h>

#include "mediaobject.h"
#include "kmplayerpartbase.h"
#include "kmplayerprocess.h"
#include "kmplayersource.h"
#include "kmplayerview.h"
#include "viewarea.h"

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
    m_process_infos ["mplayer"] = new MPlayerProcessInfo (this);
    m_process_infos ["xine"] = new XineProcessInfo (this);
    //m_process_infos ["gstreamer"] = new GStreamer (this, m_settings);, i18n ("&GStreamer")
#ifdef HAVE_NSPR
    m_process_infos ["npp"] = new NppProcessInfo (this);
#endif
}

MediaManager::~MediaManager () {
    const ProcessList::iterator e = m_processes.end ();
    for (ProcessList::iterator i = m_processes.begin ();
            i != e;
            i = m_processes.begin () /*~Process removes itself from this list*/)
    {
        kdDebug() << "~MediaManager " << *i << endl;
        delete *i;
    }
    const ProcessInfoMap::iterator ie = m_process_infos.end ();
    for (ProcessInfoMap::iterator i = m_process_infos.begin (); i != ie; ++i)
        delete i.data ();
    if (m_media_objects.size ())
        kdError () << "~MediaManager media list not empty" << endl;
}

MediaObject *MediaManager::createMedia (MediaType type, Node *node) {
    switch (type) {
        case Audio:
        case AudioVideo: {
            if (!m_player->source()->authoriseUrl(node->mrl()->absolutePath ()))
                return NULL;
            AudioVideoMedia *av = new AudioVideoMedia (this, node);
            av->process = process (av);
            av->process->setMediaObject (av);
            av->viewer = static_cast <View *>(m_player->view ())->viewArea ()->createVideoWidget ();
            if (av->process->state () <= IProcess::Ready)
                av->process->ready ();
            return av;
        }
        case Image:
            return new ImageMedia (this, node);
        case Text:
            return new TextMedia (this, node);
    }
    return NULL;
}

static const QString statemap [] = {
    i18n ("Not Running"), i18n ("Ready"), i18n ("Buffering"), i18n ("Playing")
};

void MediaManager::stateChange(AudioVideoMedia *media,
        IProcess::State olds, IProcess::State news) {
    //p->viewer()->view()->controlPanel()->setPlaying(news > Process::Ready);
    kdDebug () << "processState " << statemap[olds] << " -> " << statemap[news] << endl;
    Mrl *mrl = media->mrl ();
    if (!mrl ||!m_player->view ())
        return;
    //m_player->updateStatus (i18n ("Player %1 %2").arg (p->name ()).arg (statemap[news]));
    if (IProcess::Playing == news) {
        if (Element::state_deferred == mrl->state)
            mrl->undefer ();
        m_player->viewWidget ()->playingStart ();
        if (m_player->view ()) {
            if (media->viewer)
                media->viewer->map ();
            if (Mrl::SingleMode == mrl->view_mode)
                m_player->viewWidget ()->viewArea ()->resizeEvent (NULL); // ugh
        }
    } else if (IProcess::NotRunning == news) {
        if (AudioVideoMedia::ask_delete == media->request) {
            delete media;
        } else if (mrl->unfinished ()) {
            mrl->endOfFile ();
        }
    } else if (IProcess::Ready == news) {
        if (olds > IProcess::Ready && mrl->unfinished ())
            mrl->endOfFile ();
        if (AudioVideoMedia::ask_play == media->request) {
            playAudioVideo (media);
        } else {
            if (Mrl::SingleMode == mrl->view_mode) {
                ProcessList::iterator e = m_processes.end ();
                for (ProcessList::iterator i = m_processes.begin(); i != e; ++i)
                    if (*i != media->process &&
                            (*i)->state () == IProcess::Ready)
                        (*i)->play (); // delayed playing
            }
            if (AudioVideoMedia::ask_delete == media->request)
                delete media;
        }
    } else if (IProcess::Buffering == news) {
        if (mrl->view_mode != Mrl::SingleMode)
            mrl->defer (); // paused the SMIL
    }
}

void MediaManager::playAudioVideo (AudioVideoMedia *media) {
    Mrl *mrl = media->mrl ();
    if (!mrl ||!m_player->view ())
        return;
    if (Mrl::SingleMode == mrl->view_mode) {
        ProcessList::iterator e = m_processes.end ();
        for (ProcessList::iterator i = m_processes.begin(); i != e; ++i)
        {
            kdDebug() << "found process " << (*i != media->process) << (*i)->state () << endl;
            if (*i != media->process && (*i)->state () > IProcess::Ready)
                return; // delay, avoiding two overlaping widgets
        }
    }
    media->process->play ();
}

IProcess *MediaManager::process (AudioVideoMedia *media) {
    QString p = m_player->processName (media->mrl ());
    if (!p.isEmpty ()) {
        IProcess *proc = m_process_infos[p]->create (
                m_player, m_process_infos[p], media);
        m_processes.push_back (proc);
        return proc;
    }
    return NULL;
}

void MediaManager::processDestroyed (IProcess *process) {
    kdDebug() << "processDestroyed " << process << endl;
    m_processes.remove (process);
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
   manager->medias ().push_back (this);
}

MediaObject::~MediaObject () {
    clearData ();
   m_manager->medias ().remove (this);
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

Mrl *MediaObject::mrl () {
    return m_node ? m_node->mrl () : NULL;
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

KDE_NO_EXPORT void MediaObject::destroy () {
    delete this;
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

IProcess::IProcess () :
    media_object (NULL),
    m_state (NotRunning) {}

AudioVideoMedia::AudioVideoMedia (MediaManager *manager, Node *node)
 : MediaObject (manager, node),
   process (NULL),
   viewer (NULL),
   request (ask_nothing) {
    kdDebug() << "AudioVideoMedia::AudioVideoMedia" << endl;
}

AudioVideoMedia::~AudioVideoMedia () {
    stop ();
    // delete m_process;
    if (viewer) { //nicer with QObject destroy signal, but preventing unmap on destruction
        View *view = m_manager->player ()->viewWidget ();
        if (view)
            view->viewArea ()->destroyVideoWidget (viewer);
    }
    delete process;
    kdDebug() << "AudioVideoMedia::~AudioVideoMedia" << endl;
}

bool AudioVideoMedia::play () {
    if (process) {
        kdDebug() << "AudioVideoMedia::play " << process->state () << endl;
        if (process->state () != IProcess::Ready) {
            request = ask_play;
            return true; // FIXME add Launching state
        }
        process->ready ();
        m_manager->playAudioVideo (this);
        return true;
    }
    return false;
}

void AudioVideoMedia::stop () {
    request = ask_stop;
    if (process)
        process->stop ();
}

void AudioVideoMedia::pause () {
    if (process)
        process->pause ();
}

void AudioVideoMedia::unpause () {
    process->pause ();
}

void AudioVideoMedia::destroy () {
    if (viewer)
        viewer->unmap ();
    if (!process || IProcess::Ready >= process->state ()) {
        delete this;
    } else {
        stop ();
        request = ask_delete;
    }
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

KDE_NO_EXPORT bool ImageMedia::play () {
    if (!img_movie)
        return false;
    img_movie->restart ();
    if (img_movie->paused ())
        img_movie->unpause ();
    return true;
}

KDE_NO_EXPORT void ImageMedia::stop () {
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

bool TextMedia::play () {
    return !text.isEmpty ();
}

#include "mediaobject.moc"
