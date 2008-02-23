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
#include <QBuffer>
#include <qimage.h>
#include <qfile.h>
#include <qtextcodec.h>

#include <kdebug.h>
#include <kmimetype.h>
#include <klocale.h>
#include <kio/job.h>
#include <kio/jobclasses.h>

#include "mediaobject.h"
#include "kmplayerpartbase.h"
#include "kmplayerprocess.h"
#include "kmplayersource.h"
#include "kmplayerview.h"
#include "viewarea.h"

namespace KMPlayer {

const unsigned int event_media_ready = 100;
const unsigned int event_img_updated = 101;
const unsigned int event_img_anim_finished = 102;

}

using namespace KMPlayer;

namespace {

    typedef QMap <QString, ImageDataPtrW> ImageDataMap;

    static DataCache *memory_cache;
    static ImageDataMap *image_data_map;

    struct GlobalMediaData : public GlobalShared<GlobalMediaData> {
        GlobalMediaData (GlobalMediaData **gb)
         : GlobalShared<GlobalMediaData> (gb) {
            memory_cache = new DataCache;
            image_data_map = new ImageDataMap;
        }
        ~GlobalMediaData ();
    };

    static GlobalMediaData *global_media;

    GlobalMediaData::~GlobalMediaData () {
        delete memory_cache;
        delete image_data_map;
        global_media = NULL;
    }
}

//------------------------%<----------------------------------------------------

MediaManager::MediaManager (PartBase *player) : m_player (player) {
    if (!global_media)
        (void) new GlobalMediaData (&global_media);
    else
        global_media->ref ();

    m_process_infos ["mplayer"] = new MPlayerProcessInfo (this);
    m_process_infos ["phonon"] = new PhononProcessInfo (this);
    //XineProcessInfo *xpi = new XineProcessInfo (this);
    //m_process_infos ["xine"] = xpi;
    //m_process_infos ["gstreamer"] = new GStreamer (this, m_settings);, i18n ("&GStreamer")
#ifdef KMPLAYER_WITH_NPP
    m_process_infos ["npp"] = new NppProcessInfo (this);
#endif
    m_record_infos ["mencoder"] = new MEncoderProcessInfo (this);
    m_record_infos ["mplayerdumpstream"] = new MPlayerDumpProcessInfo (this);
    m_record_infos ["ffmpeg"] = new FFMpegProcessInfo (this);
    //m_record_infos ["xine"] = xpi;
}

MediaManager::~MediaManager () {
    const ProcessList::iterator e = m_processes.end ();
    for (ProcessList::iterator i = m_processes.begin ();
            i != e;
            i = m_processes.begin () /*~Process removes itself from this list*/)
    {
        kDebug() << "~MediaManager " << *i << endl;
        delete *i;
    }
    const ProcessList::iterator re = m_recorders.end ();
    for (ProcessList::iterator i = m_recorders.begin ();
            i != re;
            i = m_recorders.begin ())
    {
        kDebug() << "~MediaManager " << *i << endl;
        delete *i;
    }
    const ProcessInfoMap::iterator ie = m_process_infos.end ();
    for (ProcessInfoMap::iterator i = m_process_infos.begin (); i != ie; ++i)
        if (!m_record_infos.contains (i.key ()))
            delete i.data ();

    const ProcessInfoMap::iterator rie = m_record_infos.end ();
    for (ProcessInfoMap::iterator i = m_record_infos.begin (); i != rie; ++i)
        delete i.data ();

    if (m_media_objects.size ()) {
        kError () << "~MediaManager media list not empty " << m_media_objects.size () << endl;
        // bug elsewere, but don't crash
        const MediaList::iterator me = m_media_objects.end ();
        for (MediaList::iterator i = m_media_objects.begin (); i != me; ) {
            if ((*i)->mrl () &&
                    (*i)->mrl ()->document ()->active ()) {
                (*i)->mrl ()->document ()->deactivate ();
                i = m_media_objects.begin ();
            } else {
                ++i;
            }
        }
        if (m_media_objects.size ())
            kError () << "~MediaManager media list still not empty" << m_media_objects.size () << endl;
    }
    global_media->unref ();
}

MediaObject *MediaManager::createMedia (MediaType type, Node *node) {
    switch (type) {
        case Audio:
        case AudioVideo: {
            RecordDocument *rec = id_node_record_document == node->id
                ? convertNode <RecordDocument> (node)
                : NULL;
            if (!rec && !m_player->source()->authoriseUrl (
                        node->mrl()->absolutePath ()))
                return NULL;

            AudioVideoMedia *av = new AudioVideoMedia (this, node);
            if (rec) {
                av->process = m_record_infos[rec->recorder]->create (m_player, av);
                m_recorders.push_back (av->process);
                kDebug() << "Adding recorder " << endl;
            } else {
                av->process = m_process_infos[m_player->processName (
                        av->mrl ())]->create (m_player, av);
                m_processes.push_back (av->process);
            }
            av->process->media_object = av;
            av->viewer = !rec || rec->has_video
                ? m_player->viewWidget ()->viewArea ()->createVideoWidget ()
                : NULL;

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

void MediaManager::stateChange (AudioVideoMedia *media,
        IProcess::State olds, IProcess::State news) {
    //p->viewer()->view()->controlPanel()->setPlaying(news > Process::Ready);
    Mrl *mrl = media->mrl ();
    kDebug () << "processState " << media->process->process_info->name << " "
        << statemap[olds] << " -> " << statemap[news] << endl;

    if (!mrl) { // document dispose
        if (IProcess::Ready < news)
            media->process->quit ();
        else
            delete media;
        return;
    }

    if (!m_player->view ()) // part destruction
        return;

    bool is_rec = id_node_record_document == mrl->id;
    m_player->updateStatus (i18n ("Player %1 %2",
                media->process->process_info->name, statemap[news]));
    if (IProcess::Playing == news) {
        if (Element::state_deferred == mrl->state) {
            media->ignore_pause = true;
            mrl->undefer ();
            media->ignore_pause = false;
        }
        bool has_video = !is_rec;
        if (is_rec) {
            const ProcessList::iterator i = m_recorders.find (media->process);
            if (i != m_recorders.end ())
                m_player->startRecording ();
            has_video = static_cast <RecordDocument *> (mrl)->has_video;
        }
        if (has_video) {
            if (m_player->view ()) {
                if (media->viewer)
                    media->viewer->map ();
                if (Mrl::SingleMode == mrl->view_mode)
                    m_player->viewWidget ()->viewArea ()->resizeEvent (NULL);
            }
        }
    } else if (IProcess::NotRunning == news) {
        if (AudioVideoMedia::ask_delete == media->request) {
            delete media;
        } else if (mrl->unfinished ()) {
            mrl->endOfFile ();
        }
    } else if (IProcess::Ready == news) {
        if (AudioVideoMedia::ask_play == media->request) {
            playAudioVideo (media);
        } else if (AudioVideoMedia::ask_grab == media->request) {
            grabPicture (media);
        } else {
            if (!is_rec && Mrl::SingleMode == mrl->view_mode) {
                ProcessList::iterator e = m_processes.end ();
                for (ProcessList::iterator i = m_processes.begin(); i != e; ++i)
                    if (*i != media->process &&
                            (*i)->state () == IProcess::Ready)
                        (*i)->play (); // delayed playing
            }
            if (AudioVideoMedia::ask_delete == media->request)
                delete media;
            else if (olds > IProcess::Ready && mrl->unfinished ())
                mrl->endOfFile ();
        }
    } else if (IProcess::Buffering == news) {
        if (mrl->view_mode != Mrl::SingleMode) {
            media->ignore_pause = true;
            mrl->defer (); // paused the SMIL
            media->ignore_pause = false;
        }
    }
}

void MediaManager::playAudioVideo (AudioVideoMedia *media) {
    Mrl *mrl = media->mrl ();
    media->request = AudioVideoMedia::ask_nothing;
    if (!mrl ||!m_player->view ())
        return;
    if (id_node_record_document != mrl->id &&
            Mrl::SingleMode == mrl->view_mode) {
        ProcessList::iterator e = m_processes.end ();
        for (ProcessList::iterator i = m_processes.begin(); i != e; ++i)
        {
            kDebug() << "found process " << (*i != media->process) << (*i)->state () << endl;
            if (*i != media->process && (*i)->state () > IProcess::Ready)
                return; // delay, avoiding two overlaping widgets
        }
    }
    media->process->play ();
}

void MediaManager::grabPicture (AudioVideoMedia *media) {
    Mrl *mrl = media->mrl ();
    media->request = AudioVideoMedia::ask_nothing;
    if (!mrl)
        return;
    media->process->grabPicture (media->m_grab_file, media->m_frame);
}

void MediaManager::processDestroyed (IProcess *process) {
    kDebug() << "processDestroyed " << process << endl;
    m_processes.remove (process);
    m_recorders.remove (process);
    if (process->media_object &&
            AudioVideoMedia::ask_delete == process->media_object->request)
        delete process->media_object;
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
        data = it.data ();
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
    KUrl kurl (str);
    if (kurl.isLocalFile ()) {
        QFile file (kurl.path ());
        if (file.exists () && file.open (IO_ReadOnly)) {
            data = file.readAll ();
            file.close ();
        }
        ready (str);
        return true;
    }
    if (memory_cache->get (str, data)) {
        //kDebug () << "download found in cache " << str << endl;
        ready (str);
        return true;
    }
    if (memory_cache->preserve (str)) {
        //kDebug () << "downloading " << str << endl;
        job = KIO::get (kurl, KIO::NoReload, KIO::HideProgressInfo);
        connect (job, SIGNAL (data (KIO::Job *, const QByteArray &)),
                this, SLOT (slotData (KIO::Job *, const QByteArray &)));
        connect (job, SIGNAL (result (KJob *)),
                this, SLOT (slotResult (KJob *)));
        connect (job, SIGNAL (mimetype (KIO::Job *, const QString &)),
                this, SLOT (slotMimetype (KIO::Job *, const QString &)));
    } else {
        //kDebug () << "download preserved " << str << endl;
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

KDE_NO_EXPORT void MediaObject::slotResult (KJob *kjob) {
    if (!kjob->error ())
        memory_cache->add (url, data);
    else
        data.resize (0);
    job = 0L; // signal KIO::Job::result deletes itself
    ready (url);
}

KDE_NO_EXPORT void MediaObject::ready (const QString &) {
    if (m_node)
        m_node->document()->postEvent (m_node.ptr(), new Event (NULL, event_media_ready));
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
        memcpy (data.data () + old_size, qb.constData (), qb.size ());
    }
}

KDE_NO_EXPORT void MediaObject::slotMimetype (KIO::Job *, const QString & m) {
    mime = m;
}

//------------------------%<----------------------------------------------------

IProcess::IProcess (ProcessInfo *pinfo) :
    media_object (NULL),
    process_info (pinfo),
    m_state (NotRunning) {}

AudioVideoMedia::AudioVideoMedia (MediaManager *manager, Node *node)
 : MediaObject (manager, node),
   process (NULL),
   viewer (NULL),
   request (ask_nothing),
   ignore_pause (false) {
    kDebug() << "AudioVideoMedia::AudioVideoMedia" << endl;
}

AudioVideoMedia::~AudioVideoMedia () {
    stop ();
    // delete m_process;
    if (viewer) { //nicer with QObject destroy signal, but preventing unmap on destruction
        View *view = m_manager->player ()->viewWidget ();
        if (view)
            view->viewArea ()->destroyVideoWidget (viewer);
    }
    if (process) {
        process->media_object = NULL;
        delete process;
    }
    kDebug() << "AudioVideoMedia::~AudioVideoMedia";
}

bool AudioVideoMedia::play () {
    if (process) {
        kDebug() << "AudioVideoMedia::play " << process->state () << endl;
        if (process->state () > IProcess::Ready) {
            kError() << "already playing" << endl;
            return true;
        }
        if (process->state () != IProcess::Ready) {
            request = ask_play;
            return true; // FIXME add Launching state
        }
        m_manager->playAudioVideo (this);
        return true;
    }
    return false;
}

bool AudioVideoMedia::grabPicture (const QString &file, int frame) {
    if (process) {
        kDebug() << "AudioVideoMedia::grab " << file << endl;
        m_grab_file = file;
        m_frame = frame;
        if (process->state () < IProcess::Ready) {
            request = ask_grab;
            return true; // FIXME add Launching state
        }
        m_manager->grabPicture (this);
        return true;
    }
    return false;
}

void AudioVideoMedia::stop () {
    if (ask_delete != request)
        request = ask_stop;
    if (process)
        process->stop ();
}

void AudioVideoMedia::pause () {
    if (!ignore_pause && process)
        process->pause ();
}

void AudioVideoMedia::unpause () {
    if (!ignore_pause && process)
        process->pause ();
}

void AudioVideoMedia::destroy () {
    if (m_manager->player ()->view () && viewer)
        viewer->unmap ();
    if (!process || IProcess::Ready >= process->state ()) {
        delete this;
    } else {
        stop ();
        request = ask_delete;
    }
}

//------------------------%<----------------------------------------------------

#ifdef KMPLAYER_WITH_CAIRO
# include <cairo.h>
#endif

ImageData::ImageData( const QString & img)
 : width (0),
   height (0),
   flags (0),
   has_alpha (false),
   image (0L),
#ifdef KMPLAYER_WITH_CAIRO
   surface (NULL),
#endif
   url (img) {
    //if (img.isEmpty ())
    //    //kDebug() << "New ImageData for " << this << endl;
    //else
    //    //kDebug() << "New ImageData for " << img << endl;
}

ImageData::~ImageData() {
    if (!url.isEmpty ())
        image_data_map->erase (url);
#ifdef KMPLAYER_WITH_CAIRO
    if (surface)
        cairo_surface_destroy (surface);
#endif
    delete image;
}

void ImageData::setImage (QImage *img) {
    if (image != img) {
        delete image;
#ifdef KMPLAYER_WITH_CAIRO
        if (surface)
            cairo_surface_destroy (surface);
#endif
        image = img;
        width = img->width ();
        height = img->height ();
        has_alpha = img->hasAlphaBuffer ();
    }
}

ImageMedia::ImageMedia (MediaManager *manager, Node *node)
 : MediaObject (manager, node), buffer (NULL), img_movie (NULL) {}

ImageMedia::~ImageMedia () {
    delete img_movie;
    delete buffer;
}

KDE_NO_EXPORT bool ImageMedia::play () {
    if (!img_movie)
        return false;
    if (img_movie->state () == QMovie::Paused)
        img_movie->setPaused (false);
    else if (img_movie->state () != QMovie::Running)
        img_movie->start ();
    return true;
}

KDE_NO_EXPORT void ImageMedia::stop () {
    pause ();
}

void ImageMedia::pause () {
    if (img_movie && !img_movie->state () == QMovie::Paused)
        img_movie->setPaused (true);
}

void ImageMedia::unpause () {
    if (img_movie && img_movie->paused ())
        img_movie->setPaused (false);
}

KDE_NO_EXPORT void ImageMedia::setupImage () {
    if (isEmpty () && data.size ()) {
        QImage *pix = new QImage (data);
        if (!pix->isNull ()) {
            cached_img = ImageDataPtr (new ImageData (url));
            cached_img->setImage (pix);
        } else {
            delete pix;
        }
    }
    if (!isEmpty ()) {
        buffer = new QBuffer (&data);
        img_movie = new QMovie (buffer);
        kDebug() << img_movie->frameCount ();
        if (img_movie->frameCount () > 1) {
            cached_img->flags |= (short)ImageData::ImagePixmap | ImageData::ImageAnimated;
            connect (img_movie, SIGNAL (updated (const QRect &)),
                    this, SLOT (movieUpdated (const QRect &)));
            connect (img_movie, SIGNAL (stateChanged (QMovie::MovieState)),
                    this, SLOT (movieStatus (QMovie::MovieState)));
            connect (img_movie, SIGNAL (resized (const QSize &)),
                    this, SLOT (movieResize (const QSize &)));
        } else {
            delete img_movie;
            img_movie = 0L;
            delete buffer;
            buffer = 0L;
            frame_nr = 0;
            cached_img->flags |= (short)ImageData::ImagePixmap;
            image_data_map->insert (url, ImageDataPtrW (cached_img));
        }
    }
    data = QByteArray ();
}

KDE_NO_EXPORT void ImageMedia::ready (const QString &url) {
    if (data.size ()) {
        QString mime = mimetype ();
        if (!mime.startsWith (QString::fromLatin1 ("text/"))) { // FIXME svg
            ImageDataMap::iterator i = image_data_map->find (url);
            if (i == image_data_map->end ())
                setupImage ();
            else
                cached_img = i.data ();
        }
    }
    MediaObject::ready (url);
}

bool ImageMedia::isEmpty () const {
    return !cached_img || cached_img->isEmpty ();
}

KDE_NO_EXPORT bool ImageMedia::wget (const QString &str) {
    ImageDataMap::iterator i = image_data_map->find (str);
    if (i != image_data_map->end ()) {
        cached_img = i.data ();
        MediaObject::ready (str);
        return true;
    } else {
        return MediaObject::wget (str);
    }
}

KDE_NO_EXPORT void ImageMedia::movieResize (const QSize &) {
    //kDebug () << "movieResize" << endl;
    if (m_node)
        m_node->document ()->postEvent (m_node, new Event (NULL, event_img_updated));
}

KDE_NO_EXPORT void ImageMedia::movieUpdated (const QRect &) {
    if (frame_nr++) {
        ASSERT (cached_img && isEmpty ());
        QImage *img = new QImage;
        *img = img_movie->framePixmap ();
        cached_img->setImage (img);
        cached_img->flags = (int)(ImageData::ImagePixmap | ImageData::ImageAnimated); //TODO
        if (m_node)
            m_node->document ()->postEvent (m_node, new Event (NULL, event_img_updated));
    }
}

KDE_NO_EXPORT void ImageMedia::movieStatus (QMovie::MovieState status) {
    if (QMovie::NotRunning == status && m_node)
        m_node->document ()->postEvent(m_node, new Event (NULL, event_img_anim_finished));
}

//------------------------%<----------------------------------------------------

TextMedia::TextMedia (MediaManager *manager, Node *node)
 : MediaObject (manager, node), codec (NULL) {
    default_font_size = QApplication::font ().pointSize ();
}

TextMedia::~TextMedia () {
}

KDE_NO_EXPORT void TextMedia::ready (const QString &url) {
    if (data.size ()) {
        if (!data [data.size () - 1])
            data.resize (data.size () - 1); // strip zero terminate char
        QTextStream ts (data, IO_ReadOnly);
        if (codec)
            ts.setCodec (codec);
        text  = ts.read ();
    }
    MediaObject::ready (url);
}

bool TextMedia::play () {
    return !text.isEmpty ();
}

#include "mediaobject.moc"
