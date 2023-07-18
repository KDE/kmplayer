/*
    This file belong to the KMPlayer project, a movie player plugin for Konqueror
    SPDX-FileCopyrightText: 2007 Koos Vriezen <koos.vriezen@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include <QTextStream>
#include <QApplication>
#include <QMovie>
#include <QBuffer>
#include <QPainter>
#include <QSvgRenderer>
#include <QImage>
#include <QFile>
#include <QUrl>
#include <QTextCodec>
#include <QTextStream>
#include <QMimeDatabase>
#include <QMimeType>

#include <KLocalizedString>
#include <KIO/Job>
#include <KUrlAuthorized>

#include "mediaobject.h"
#include "kmplayerprocess.h"
#include "kmplayerview.h"
#include "expression.h"
#include "viewarea.h"
#include "kmplayerpartbase.h"
#include "kmplayercommon_log.h"

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
        ~GlobalMediaData () override;
    };

    static GlobalMediaData *global_media;

    GlobalMediaData::~GlobalMediaData () {
        delete memory_cache;
        delete image_data_map;
        global_media = nullptr;
    }

    bool isBufferBinaryData(const QByteArray &data)
    {
        // Check the first 32 bytes (see shared-mime spec)
        const char *p = data.data();
        const int end = qMin(32, data.size());
        for (int i = 0; i < end; ++i) {
            if ((unsigned char)(p[i]) < 32 && p[i] != 9 && p[i] != 10 && p[i] != 13) { // ASCII control character
                return true;
            }
        }
        return false;
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
    for (ProcessList::iterator i = m_processes.begin ();
            i != m_processes.end ();
            i = m_processes.begin () /*~Process removes itself from this list*/)
    {
        qCDebug(LOG_KMPLAYER_COMMON) << "~MediaManager " << *i << endl;
        delete *i;
    }
    for (ProcessList::iterator i = m_recorders.begin ();
            i != m_recorders.end ();
            i = m_recorders.begin ())
    {
        qCDebug(LOG_KMPLAYER_COMMON) << "~MediaManager " << *i << endl;
        delete *i;
    }
    const ProcessInfoMap::iterator ie = m_process_infos.end ();
    for (ProcessInfoMap::iterator i = m_process_infos.begin (); i != ie; ++i)
        if (!m_record_infos.contains (i.key ()))
            delete i.value ();

    const ProcessInfoMap::iterator rie = m_record_infos.end ();
    for (ProcessInfoMap::iterator i = m_record_infos.begin (); i != rie; ++i)
        delete i.value ();

    if (m_media_objects.size ()) {
        qCCritical(LOG_KMPLAYER_COMMON) << "~MediaManager media list not empty " << m_media_objects.size () << endl;
        // bug elsewere, but don't crash
        const MediaList::iterator me = m_media_objects.end ();
        for (MediaList::iterator i = m_media_objects.begin (); i != me; ) {
            if (*i && (*i)->mrl () &&
                    (*i)->mrl ()->document ()->active ()) {
                (*i)->mrl ()->document ()->deactivate ();
                i = m_media_objects.begin ();
            } else {
                ++i;
            }
        }
        if (m_media_objects.size ())
            qCCritical(LOG_KMPLAYER_COMMON) << "~MediaManager media list still not empty" << m_media_objects.size () << endl;
    }
    global_media->unref ();
}

MediaObject *MediaManager::createAVMedia (Node *node, const QByteArray &) {
    RecordDocument *rec = id_node_record_document == node->id
        ? convertNode <RecordDocument> (node)
        : nullptr;
    if (!rec && !m_player->source()->authoriseUrl (
                node->mrl()->absolutePath ()))
        return nullptr;

    AudioVideoMedia *av = new AudioVideoMedia (this, node);
    if (rec) {
        av->process = m_record_infos[rec->recorder]->create (m_player, av);
        m_recorders.push_back (av->process);
        qCDebug(LOG_KMPLAYER_COMMON) << "Adding recorder " << endl;
    } else {
        av->process = m_process_infos[m_player->processName (
                av->mrl ())]->create (m_player, av);
        m_processes.push_back (av->process);
    }
    av->process->user = av;
    av->setViewer (!rec
        ? m_player->viewWidget ()->viewArea ()->createVideoWidget ()
        : nullptr);

    if (av->process->state () <= IProcess::Ready)
        av->process->ready ();
    return av;
}

static const QString statemap [] = {
    i18n ("Not Running"), i18n ("Ready"), i18n ("Buffering"), i18n ("Playing"),  i18n ("Paused")
};

void MediaManager::stateChange (AudioVideoMedia *media,
        IProcess::State olds, IProcess::State news) {
    //p->viewer()->view()->controlPanel()->setPlaying(news > Process::Ready);
    Mrl *mrl = media->mrl ();
    qCDebug(LOG_KMPLAYER_COMMON) << "processState " << media->process->process_info->name << " "
        << statemap[olds] << " -> " << statemap[news];

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
        if (Element::state_deferred == mrl->state)
            mrl->undefer ();
        bool has_video = !is_rec;
        if (is_rec && m_recorders.contains(media->process))
            m_player->recorderPlaying ();
        if (has_video) {
            if (m_player->view ()) {
                if (media->viewer ()) {
                    media->viewer ()->setAspect (mrl->aspect);
                    media->viewer ()->map ();
                }
                if (Mrl::SingleMode == mrl->view_mode)
                    m_player->viewWidget ()->viewArea ()->resizeEvent (nullptr);
            }
        }
    } else if (IProcess::NotRunning == news) {
        if (AudioVideoMedia::ask_delete == media->request) {
            delete media;
        } else if (mrl->unfinished ()) {
            mrl->document ()->post (mrl, new Posting (mrl, MsgMediaFinished));
        }
    } else if (IProcess::Ready == news) {
        if (AudioVideoMedia::ask_play == media->request) {
            playAudioVideo (media);
        } else if (AudioVideoMedia::ask_grab == media->request) {
            grabPicture (media);
        } else {
            if (!is_rec && Mrl::SingleMode == mrl->view_mode) {
                ProcessList::ConstIterator i, e = m_processes.constEnd ();
                for (i = m_processes.constBegin(); i != e; ++i)
                    if (*i != media->process &&
                            (*i)->state () == IProcess::Ready)
                        (*i)->play (); // delayed playing
                e = m_recorders.constEnd ();
                for (i = m_recorders.constBegin (); i != e; ++i)
                    if (*i != media->process &&
                            (*i)->state () == IProcess::Ready)
                        (*i)->play (); // delayed recording
            }
            if (AudioVideoMedia::ask_delete == media->request) {
                delete media;
            } else if (olds > IProcess::Ready) {
                if (is_rec)
                    mrl->message (MsgMediaFinished, nullptr); // FIXME
                else
                    mrl->document()->post(mrl, new Posting (mrl, MsgMediaFinished));
            }
        }
    } else if (IProcess::Buffering == news) {
        if (AudioVideoMedia::ask_pause == media->request) {
            media->pause ();
        } else if (mrl->view_mode != Mrl::SingleMode) {
            mrl->defer (); // paused the SMIL
        }
    }
}

void MediaManager::playAudioVideo (AudioVideoMedia *media) {
    Mrl *mrl = media->mrl ();
    media->request = AudioVideoMedia::ask_nothing;
    if (!mrl ||!m_player->view ())
        return;
    if (Mrl::SingleMode == mrl->view_mode) {
        ProcessList::ConstIterator i, e = m_processes.constEnd ();
        for (i = m_processes.constBegin(); i != e; ++i)
            if (*i != media->process && (*i)->state () > IProcess::Ready)
                return; // delay, avoiding two overlaping widgets
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
    qCDebug(LOG_KMPLAYER_COMMON) << "processDestroyed " << process << endl;
    m_processes.removeAll (process);
    m_recorders.removeAll (process);
}

//------------------------%<----------------------------------------------------

MediaObject::MediaObject (MediaManager *manager, Node *node)
 : m_manager (manager), m_node (node) {
    manager->medias ().push_back (this);
}

MediaObject::~MediaObject () {
    m_manager->medias ().removeAll (this);
}

void MediaObject::destroy () {
    delete this;
}

Mrl *MediaObject::mrl () {
    return m_node ? m_node->mrl () : nullptr;
}

//------------------------%<----------------------------------------------------

void DataCache::add (const QString & url, const QString &mime, const QByteArray & data) {
    QByteArray bytes;
    bytes = data;
    cache_map.insert (url, qMakePair (mime, bytes));
    preserve_map.remove (url);
    Q_EMIT preserveRemoved (url);
}

bool DataCache::get (const QString & url, QString &mime, QByteArray & data) {
    DataMap::const_iterator it = cache_map.constFind (url);
    if (it != cache_map.constEnd ()) {
        mime = it.value ().first;
        data = it.value ().second;
        return true;
    }
    return false;
}

bool DataCache::preserve (const QString & url) {
    PreserveMap::const_iterator it = preserve_map.constFind (url);
    if (it == preserve_map.constEnd ()) {
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
    Q_EMIT preserveRemoved (url);
    return true;
}

//------------------------%<----------------------------------------------------

static bool isPlayListMime (const QString & mime) {
    QString m (mime);
    int plugin_pos = m.indexOf ("-plugin");
    if (plugin_pos > 0)
        m.truncate (plugin_pos);
    QByteArray ba = m.toLatin1 ();
    const char * mimestr = ba.data ();
    qCDebug(LOG_KMPLAYER_COMMON) << "isPlayListMime " << mimestr;
    return mimestr && (!strcmp (mimestr, "audio/mpegurl") ||
            !strcmp (mimestr, "audio/x-mpegurl") ||
            !strncmp (mimestr, "video/x-ms", 10) ||
            !strncmp (mimestr, "audio/x-ms", 10) ||
            //!strcmp (mimestr, "video/x-ms-wmp") ||
            //!strcmp (mimestr, "video/x-ms-asf") ||
            //!strcmp (mimestr, "video/x-ms-wmv") ||
            //!strcmp (mimestr, "video/x-ms-wvx") ||
            //!strcmp (mimestr, "video/x-msvideo") ||
            !strcmp (mimestr, "audio/x-scpls") ||
            !strcmp (mimestr, "audio/x-shoutcast-stream") ||
            !strcmp (mimestr, "audio/x-pn-realaudio") ||
            !strcmp (mimestr, "audio/vnd.rn-realaudio") ||
            !strcmp (mimestr, "audio/m3u") ||
            !strcmp (mimestr, "audio/x-m3u") ||
            !strncmp (mimestr, "text/", 5) ||
            (!strncmp (mimestr, "application/", 12) &&
             strstr (mimestr + 12,"+xml")) ||
            !strncasecmp (mimestr, "application/smil", 16) ||
            !strncasecmp (mimestr, "application/xml", 15) ||
            //!strcmp (mimestr, "application/rss+xml") ||
            //!strcmp (mimestr, "application/atom+xml") ||
            !strcmp (mimestr, "image/svg+xml") ||
            !strcmp (mimestr, "image/vnd.rn-realpix") ||
            !strcmp (mimestr, "application/x-mplayer2"));
}

static QString mimeByContent (const QByteArray &data)
{
    const QMimeType mimeType = QMimeDatabase().mimeTypeForData(data);
    if (mimeType.isValid())
        return mimeType.name ();
    return QString ();
}

MediaInfo::MediaInfo (Node *n, MediaManager::MediaType t)
 : media (nullptr), type (t), node (n), job (nullptr),
    preserve_wait (false), check_access (false) {
}

MediaInfo::~MediaInfo () {
    clearData ();
}

void MediaInfo::killWGet () {
    if (job) {
        job->kill (); // quiet, no result signal
        job = nullptr;
        memory_cache->unpreserve (url);
    } else if (preserve_wait) {
        disconnect (memory_cache, &DataCache::preserveRemoved,
                    this, &MediaInfo::cachePreserveRemoved);
        preserve_wait = false;
    }
}

/**
 * Gets contents from url and puts it in m_data
 */
bool MediaInfo::wget(const QString& str, const QString& domain) {
    clearData ();
    url = str;

    if (MediaManager::Any == type || MediaManager::Image == type) {
        ImageDataMap::iterator i = image_data_map->find (str);
        if (i != image_data_map->end ()) {
            media = new ImageMedia (node, i.value ());
            type = MediaManager::Image;
            ready ();
            return true;
        }
    }

    Mrl *mrl = node->mrl ();
    if (mrl && (MediaManager::Any == type || MediaManager::AudioVideo == type))
    {
        if (!mrl->mimetype.isEmpty ())
            setMimetype (mrl->mimetype);
        if (mrl && (MediaManager::Any == type || MediaManager::AudioVideo == type))
            if (mime == "application/x-shockwave-flash" ||
                    mime == "application/futuresplash" ||
                    str.startsWith ("tv:")) {
                ready ();
                return true; // FIXME
            }
    }

    QUrl kurl = QUrl::fromUserInput(str);
    if (!mrl || !mrl->access_granted)
        for (Node *p = node->parentNode (); p; p = p->parentNode ()) {
            Mrl *m = p->mrl ();
            if (m && !m->src.isEmpty () &&
                  m->src != "Playlist://" &&
                  !KUrlAuthorized::authorizeUrlAction ("redirect", QUrl::fromUserInput(m->src), kurl)) {
                qCWarning(LOG_KMPLAYER_COMMON) << "redirect access denied";
                ready ();
                return true;
            }
        }

    bool only_playlist = false;
    bool maybe_playlist = false;
    if (MediaManager::Audio == type
            || MediaManager::AudioVideo == type
            || MediaManager::Any == type) {
        only_playlist = true;
        maybe_playlist = isPlayListMime (mime);
    }

    if (kurl.isLocalFile ()) {
        QFile file (kurl.toLocalFile ());
        if (file.exists ()) {
            if (MediaManager::Data != type && mime.isEmpty ()) {
                const QMimeType mimeTyoe = QMimeDatabase().mimeTypeForUrl (kurl);
                if (mrl && mimeTyoe.isValid()) {
                    mrl->mimetype = mimeTyoe.name ();
                    setMimetype (mrl->mimetype);
                }
                qCDebug(LOG_KMPLAYER_COMMON) << "wget2 " << str << " " << mime;
            } else {
                setMimetype (mime);
            }
            only_playlist = MediaManager::Audio == type ||
                MediaManager::AudioVideo == type;
            maybe_playlist = isPlayListMime (mime); // get new mime
            if (file.open (QIODevice::ReadOnly)) {
                if (only_playlist) {
                    maybe_playlist &= file.size () < 2000000;
                    if (maybe_playlist) {
                        char databuf [512];
                        int nr_bytes = file.read (databuf, 512);
                        if (nr_bytes > 3 &&
                                (::isBufferBinaryData (QByteArray (databuf, nr_bytes)) ||
                                 !strncmp (databuf, "RIFF", 4)))
                            maybe_playlist = false;
                    }
                    if (!maybe_playlist) {
                        ready ();
                        return true;
                    }
                    file.reset ();
                }
                data = file.readAll ();
                file.close ();
            }
        }
        ready ();
        return true;
    }
    QString protocol = kurl.scheme ();
    if (!domain.isEmpty ()) {
        QString get_from = protocol + "://" + kurl.host ();
        if (get_from != domain) {
            check_access = true;
            access_from = domain;
            cross_domain = get_from + "/crossdomain.xml";
            kurl = QUrl (cross_domain);
        }
    }
    if (!check_access) {
        if (MediaManager::Data != type &&
                (memory_cache->get (str, mime, data) ||
                 protocol == "mms" || protocol == "rtsp" ||
                 protocol == "rtp" || protocol == "rtmp" ||
                 (only_playlist && !maybe_playlist && !mime.isEmpty ()))) {
            setMimetype (mime);
            if (MediaManager::Any == type)
                type = MediaManager::AudioVideo;
            ready ();
            return true;
        }
    }
    if (check_access || memory_cache->preserve (str)) {
        //qCDebug(LOG_KMPLAYER_COMMON) << "downloading " << str;
        job = KIO::get (kurl, KIO::NoReload, KIO::HideProgressInfo);
        job->addMetaData ("PropagateHttpHeader", "true");
        job->addMetaData ("errorPage", "false");
        connect (job, &KIO::TransferJob::data,
                this, &MediaInfo::slotData);
        connect (job, &KJob::result,
                this, &MediaInfo::slotResult);
        if (!check_access)
            connect (job, QOverload<KIO::Job*, const QString&>::of(&KIO::TransferJob::mimetype),
                    this, &MediaInfo::slotMimetype);
    } else {
        //qCDebug(LOG_KMPLAYER_COMMON) << "download preserved " << str;
        connect (memory_cache, &DataCache::preserveRemoved,
                 this, &MediaInfo::cachePreserveRemoved);
        preserve_wait = true;
    }
    return false;
}

bool MediaInfo::readChildDoc () {
    QTextStream textstream (data, QIODevice::ReadOnly);
    QString line;
    NodePtr cur_elm = node;
    do {
        line = textstream.readLine ();
    } while (!line.isNull () && line.trimmed ().isEmpty ());
    if (!line.isNull ()) {
        bool pls_groupfound =
            line.startsWith ("[") && line.endsWith ("]") &&
            line.mid (1, line.size () - 2).trimmed () == "playlist";
        if ((pls_groupfound &&
                    cur_elm->mrl ()->mimetype.startsWith ("audio/")) ||
                cur_elm->mrl ()->mimetype == QString ("audio/x-scpls")) {
            int nr = -1;
            struct Entry {
                QString url, title;
            } * entries = nullptr;
            do {
                line = line.trimmed ();
                if (!line.isEmpty ()) {
                    if (line.startsWith ("[") && line.endsWith ("]")) {
                        if (line.mid (1, line.size () - 2).trimmed () == "playlist")
                            pls_groupfound = true;
                        else
                            break;
                        qCDebug(LOG_KMPLAYER_COMMON) << "Group found: " << line;
                    } else if (pls_groupfound) {
                        int eq_pos = line.indexOf (QChar ('='));
                        if (eq_pos > 0) {
                            if (line.toLower ().startsWith (QString ("numberofentries"))) {
                                nr = line.mid (eq_pos + 1).trimmed ().toInt ();
                                qCDebug(LOG_KMPLAYER_COMMON) << "numberofentries : " << nr;
                                if (nr > 0 && nr < 1024)
                                    entries = new Entry[nr];
                                else
                                    nr = 0;
                            } else if (nr > 0) {
                                QString ll = line.toLower ();
                                if (ll.startsWith (QString ("file"))) {
                                    int i = line.mid (4, eq_pos-4).toInt ();
                                    if (i > 0 && i <= nr)
                                        entries[i-1].url = line.mid (eq_pos + 1).trimmed ();
                                } else if (ll.startsWith (QString ("title"))) {
                                    int i = line.mid (5, eq_pos-5).toInt ();
                                    if (i > 0 && i <= nr)
                                        entries[i-1].title = line.mid (eq_pos + 1).trimmed ();
                                }
                            }
                        }
                    }
                }
                line = textstream.readLine ();
            } while (!line.isNull ());
            NodePtr doc = node->document ();
            for (int i = 0; i < nr; i++)
                if (!entries[i].url.isEmpty ())
                    cur_elm->appendChild (new GenericURL (doc,
                           QUrl::fromPercentEncoding (entries[i].url.toUtf8 ()),
                           entries[i].title));
            delete [] entries;
        } else if (line.trimmed ().startsWith (QChar ('<'))) {
            readXML (cur_elm, textstream, line);
            //cur_elm->normalize ();
        } else if (line.toLower () != QString ("[reference]")) {
            bool extm3u = line.startsWith ("#EXTM3U");
            QString title;
            NodePtr doc = node->document ();
            if (extm3u)
                line = textstream.readLine ();
            while (!line.isNull ()) {
             /* TODO && m_document.size () < 1024 / * support 1k entries * /);*/
                QString mrl = line.trimmed ();
                if (line == QString ("--stop--"))
                    break;
                if (mrl.toLower ().startsWith (QString ("asf ")))
                    mrl = mrl.mid (4).trimmed ();
                if (!mrl.isEmpty ()) {
                    if (extm3u && mrl.startsWith (QChar ('#'))) {
                        if (line.startsWith ("#EXTINF:"))
                            title = line.mid (9);
                        else
                            title = mrl.mid (1);
                    } else if (!line.startsWith (QChar ('#'))) {
                        cur_elm->appendChild (new GenericURL (doc, mrl, title));
                        title.truncate (0);
                    }
                }
                line = textstream.readLine ();
            }
        }
    }
    return !cur_elm->isPlayable ();
}

void MediaInfo::setMimetype (const QString &mt)
{
    mime = mt;

    Mrl *mrl = node ? node->mrl () : nullptr;
    if (mrl && mrl->mimetype.isEmpty ())
        mrl->mimetype = mt;

    if (MediaManager::Any == type) {
        if (mimetype ().startsWith ("image/"))
            type = MediaManager::Image;
        else if (mime.startsWith ("audio/"))
            type = MediaManager::Audio;
        else
            type = MediaManager::AudioVideo;
    }
}

QString MediaInfo::mimetype () {
    if (data.size () > 0 && mime.isEmpty ())
        setMimetype (mimeByContent (data));
    return mime;
}

void MediaInfo::clearData () {
    killWGet ();
    if (media) {
        media->destroy ();
        media = nullptr;
    }
    url.truncate (0);
    mime.truncate (0);
    access_from.truncate (0);
    data.resize (0);
}

bool MediaInfo::downloading () const {
    return !!job;
}

void MediaInfo::create () {
    MediaManager *mgr = (MediaManager*)node->document()->role(RoleMediaManager);
    if (!media && mgr) {
        switch (type) {
        case MediaManager::Audio:
        case MediaManager::AudioVideo:
            qCDebug(LOG_KMPLAYER_COMMON) << data.size ();
            if (!data.size () || !readChildDoc ())
                media = mgr->createAVMedia (node, data);
            break;
        case MediaManager::Image:
            if (data.size () && mime == "image/svg+xml") {
                readChildDoc ();
                if (node->firstChild () &&
                        id_node_svg == node->lastChild ()->id) {
                    media = new ImageMedia (node);
                    break;
                }
            }
            if (data.size () &&
                    (!(mimetype ().startsWith ("text/") ||
                       mime == "image/vnd.rn-realpix") ||
                     !readChildDoc ()))
                media = new ImageMedia (mgr, node, url, data);
            break;
        case MediaManager::Text:
            if (data.size ())
                media = new TextMedia (mgr, node, data);
            break;
        default: // Any
            break;
        }
    }
}

void MediaInfo::ready () {
    if (MediaManager::Data != type) {
        create ();
        if (id_node_record_document == node->id)
            node->message (MsgMediaReady);
        else
            node->document()->post (node, new Posting (node, MsgMediaReady));
    } else {
        node->message (MsgMediaReady);
    }
}

static bool validDataFormat (MediaManager::MediaType type, const QByteArray &ba)
{
    switch (type) {
        case MediaManager::Audio:
        case MediaManager::AudioVideo:
            return !(ba.size () > 2000000 || ba.size () < 4 ||
                    ::isBufferBinaryData (ba) ||
                     !strncmp (ba.data (), "RIFF", 4));
        default:
            return true;
    }
}

void MediaInfo::slotResult (KJob *kjob) {
    job = nullptr; // signal KIO::Job::result deletes itself
    if (check_access) {
        check_access = false;

        bool success = false;
        if (!kjob->error () && data.size () > 0) {
            QTextStream ts (data, QIODevice::ReadOnly);
            NodePtr doc = new Document (QString ());
            readXML (doc, ts, QString ());

            Expression *expr = evaluateExpr (
                    "//cross-domain-policy/allow-access-from/@domain");
            if (expr) {
                expr->setRoot (doc);
                Expression::iterator it, e = expr->end();
                for (it = expr->begin(); it != e; ++it) {
                    QRegExp match (it->value(), Qt::CaseInsensitive, QRegExp::Wildcard);
                    if (match.exactMatch (access_from)) {
                        success = true;
                        break;
                    }
                }
                delete expr;
            }
            doc->document ()->dispose ();
        }
        if (success) {
            wget (QString (url));
        } else {
            data.resize (0);
            ready ();
        }
    } else {
        if (MediaManager::Data != type && !kjob->error ()) {
            if (data.size () && data.size () < 512) {
                setMimetype (mimeByContent (data));
                if (!validDataFormat (type, data))
                    data.resize (0);
            }
            memory_cache->add (url, mime, data);
        } else {
            memory_cache->unpreserve (url);
            if (MediaManager::Data != type)
                data.resize (0);
        }
        ready ();
    }
}

void MediaInfo::cachePreserveRemoved (const QString & str) {
    if (str == url && !memory_cache->isPreserved (str)) {
        preserve_wait = false;
        disconnect (memory_cache, &DataCache::preserveRemoved,
                    this, &MediaInfo::cachePreserveRemoved);
        wget (str);
    }
}

void MediaInfo::slotData (KIO::Job *, const QByteArray &qb) {
    if (qb.size ()) {
        int old_size = data.size ();
        int newsize = old_size + qb.size ();
        data.resize (newsize);
        memcpy (data.data () + old_size, qb.constData (), qb.size ());
        if (!check_access && old_size < 512 && newsize >= 512) {
            setMimetype (mimeByContent (data));
            if (!validDataFormat (type, data)) {
                data.resize (0);
                job->kill (KJob::EmitResult);
                return;
            }
        }
    }
}

void MediaInfo::slotMimetype (KIO::Job *, const QString & m) {
    Mrl *mrl = node->mrl ();
    mime = m;
    if (mrl)
        mrl->mimetype = m;
    switch (type) {
    case MediaManager::Any:
        //fall through
        break;
    case MediaManager::Audio:
    case MediaManager::AudioVideo:
        if (!isPlayListMime (m))
            job->kill (KJob::EmitResult);
        break;
    default:
        //TODO
        break;
    }
}

//------------------------%<----------------------------------------------------

IProcess::IProcess (ProcessInfo *pinfo) :
    user (nullptr),
    process_info (pinfo),
    m_state (NotRunning) {}

AudioVideoMedia::AudioVideoMedia (MediaManager *manager, Node *node)
 : MediaObject (manager, node),
   process (nullptr),
   m_viewer (nullptr),
   request (ask_nothing) {
    qCDebug(LOG_KMPLAYER_COMMON) << "AudioVideoMedia::AudioVideoMedia" << endl;
}

AudioVideoMedia::~AudioVideoMedia () {
    stop ();
    // delete m_process;
    if (m_viewer) { //nicer with QObject destroy signal, but preventing unmap on destruction
        View *view = m_manager->player ()->viewWidget ();
        if (view)
            view->viewArea ()->destroyVideoWidget (m_viewer);
    }
    if (process) {
        request = ask_nothing;
        delete process;
    }
    qCDebug(LOG_KMPLAYER_COMMON) << "AudioVideoMedia::~AudioVideoMedia";
}

bool AudioVideoMedia::play () {
    qCDebug(LOG_KMPLAYER_COMMON) << process;
    if (process) {
        qCDebug(LOG_KMPLAYER_COMMON) << process->state ();
        if (process->state () > IProcess::Ready) {
            qCCritical(LOG_KMPLAYER_COMMON) << "already playing" << endl;
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
        qCDebug(LOG_KMPLAYER_COMMON) << "AudioVideoMedia::grab " << file << endl;
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
    if (m_manager->player ()->view () && m_viewer)
        m_viewer->unmap ();
}

void AudioVideoMedia::pause () {
    if (process) {
        if (process->state () > IProcess::Ready) {
            request = ask_nothing;
            process->pause ();
        } else {
            request = ask_pause;
        }
    }
}

void AudioVideoMedia::unpause () {
    if (process) {
        if (request == ask_pause) {
            request = ask_nothing;
        } else {
            process->unpause ();
        }
    }
}

void AudioVideoMedia::destroy () {
    if (m_manager->player ()->view () && m_viewer)
        m_viewer->unmap ();
    if (!process || IProcess::Ready >= process->state ()) {
        delete this;
    } else {
        stop ();
        request = ask_delete;
    }
}

void AudioVideoMedia::starting (IProcess*) {
    request = AudioVideoMedia::ask_nothing;
}

void AudioVideoMedia::stateChange (IProcess *,
                                   IProcess::State os, IProcess::State ns) {
    m_manager->stateChange (this, os, ns);
}

void AudioVideoMedia::processDestroyed (IProcess *p) {
    m_manager->processDestroyed (p);
    process = nullptr;
    if (ask_delete == request)
        delete this;
}

IViewer *AudioVideoMedia::viewer () {
    return m_viewer;
}

Mrl *AudioVideoMedia::getMrl () {
    return mrl ();
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
   image (nullptr),
#ifdef KMPLAYER_WITH_CAIRO
   surface (nullptr),
#endif
   url (img) {
    //if (img.isEmpty ())
    //    //qCDebug(LOG_KMPLAYER_COMMON) << "New ImageData for " << this << endl;
    //else
    //    //qCDebug(LOG_KMPLAYER_COMMON) << "New ImageData for " << img << endl;
}

ImageData::~ImageData() {
    if (!url.isEmpty ())
        image_data_map->remove (url);
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
        if (surface) {
            cairo_surface_destroy (surface);
            surface = nullptr;
        }
#endif
        image = img;
        if (img) {
            width = img->width ();
            height = img->height ();
            has_alpha = img->hasAlphaChannel ();
        } else {
            width = height = 0;
        }
    }
}

ImageMedia::ImageMedia (MediaManager *manager, Node *node,
        const QString &url, const QByteArray &ba)
 : MediaObject (manager, node), data (ba), buffer (nullptr),
   img_movie (nullptr),
   svg_renderer (nullptr),
   update_render (false),
   paused (false) {
    setupImage (url);
}

ImageMedia::ImageMedia (Node *node, ImageDataPtr id)
 : MediaObject ((MediaManager *)node->document()->role (RoleMediaManager),
         node),
   buffer (nullptr),
   img_movie (nullptr),
   svg_renderer (nullptr),
   update_render (false) {
    if (!id) {
        Node *c = findChildWithId (node, id_node_svg);
        if (c) {
            svg_renderer = new QSvgRenderer (c->outerXML().toUtf8 ());
            if (svg_renderer->isValid ()) {
                cached_img = new ImageData (QString ());
                cached_img->flags = ImageData::ImageScalable;
                if (svg_renderer->animated())
                    connect(svg_renderer, &QSvgRenderer::repaintNeeded,
                            this, &ImageMedia::svgUpdated);
            } else {
                delete svg_renderer;
                svg_renderer = nullptr;
            }
        }
    } else {
        cached_img = id;
    }
}

ImageMedia::~ImageMedia () {
    delete img_movie;
    delete svg_renderer;
    delete buffer;
}

bool ImageMedia::play () {
    if (!img_movie)
        return false;
    if (img_movie->state () == QMovie::Paused)
        img_movie->setPaused (false);
    else if (img_movie->state () != QMovie::Running)
        img_movie->start ();
    return true;
}

void ImageMedia::stop () {
    pause ();
}

void ImageMedia::pause () {
    if (!paused && svg_renderer && svg_renderer->animated())
        disconnect(svg_renderer, &QSvgRenderer::repaintNeeded,
                this, &ImageMedia::svgUpdated);
    if (img_movie && img_movie->state () != QMovie::Paused)
        img_movie->setPaused (true);
    paused = true;
}

void ImageMedia::unpause () {
    if (paused && svg_renderer && svg_renderer->animated())
        connect(svg_renderer, &QSvgRenderer::repaintNeeded,
                this, &ImageMedia::svgUpdated);
    if (img_movie && QMovie::Paused == img_movie->state ())
        img_movie->setPaused (false);
    paused = false;
}

void ImageMedia::setupImage (const QString &url) {
    if (isEmpty () && data.size ()) {
        QImage *pix = new QImage;
        if (pix->loadFromData((data))) {
            cached_img = ImageDataPtr (new ImageData (url));
            cached_img->setImage (pix);
        } else {
            delete pix;
        }
    }
    if (!isEmpty ()) {
        buffer = new QBuffer (&data);
        img_movie = new QMovie (buffer);
        //qCDebug(LOG_KMPLAYER_COMMON) << img_movie->frameCount ();
        if (img_movie->frameCount () > 1) {
            cached_img->flags |= (short)ImageData::ImagePixmap | ImageData::ImageAnimated;
            connect (img_movie, &QMovie::updated,
                    this, &ImageMedia::movieUpdated);
            connect (img_movie, &QMovie::stateChanged,
                    this, &ImageMedia::movieStatus);
            connect (img_movie, &QMovie::resized,
                    this, &ImageMedia::movieResize);
        } else {
            delete img_movie;
            img_movie = nullptr;
            delete buffer;
            buffer = nullptr;
            frame_nr = 0;
            cached_img->flags |= (short)ImageData::ImagePixmap;
            image_data_map->insert (url, ImageDataPtrW (cached_img));
        }
    }
}

void ImageMedia::render (const ISize &sz) {
    if (svg_renderer && update_render) {
        delete svg_renderer;
        svg_renderer = nullptr;
        Node *c = findChildWithId (m_node, id_node_svg);
        if (c) {
            QSvgRenderer *r = new QSvgRenderer (c->outerXML().toUtf8 ());
            if (r->isValid ()) {
                cached_img->setImage (nullptr);
                svg_renderer = r;
            } else {
                delete r;
            }
        }
        update_render = false;
    }
    if (svg_renderer &&
           (cached_img->width != sz.width || cached_img->height != sz.height)) {
        QImage *img = new QImage (sz.width, sz.height,
                QImage::Format_ARGB32_Premultiplied);
        img->fill (0x0);
        QPainter paint (img);
        paint.setViewport (QRect (0, 0, sz.width, sz.height));
        svg_renderer->render (&paint);
        cached_img->setImage (img);
    }
}

void ImageMedia::updateRender () {
    update_render = true;
    if (m_node)
        m_node->document()->post(m_node, new Posting (m_node, MsgMediaUpdated));
}

void ImageMedia::sizes (SSize &size) {
    if (svg_renderer) {
        QSize s = svg_renderer->defaultSize ();
        size.width = s.width ();
        size.height = s.height ();
    } else if (cached_img) {
        size.width = cached_img->width;
        size.height = cached_img->height;
    } else {
        size.width = 0;
        size.height = 0;
    }
}

bool ImageMedia::isEmpty () const {
    return !cached_img || (!svg_renderer && cached_img->isEmpty ());
}

void ImageMedia::svgUpdated() {
    cached_img->setImage (nullptr);
    if (m_node)
        m_node->document ()->post (m_node, new Posting (m_node, MsgMediaUpdated));
}

void ImageMedia::movieResize (const QSize &) {
    //qCDebug(LOG_KMPLAYER_COMMON) << "movieResize" << endl;
    if (m_node)
        m_node->document ()->post (m_node, new Posting (m_node, MsgMediaUpdated));
}

void ImageMedia::movieUpdated (const QRect &) {
    if (frame_nr++) {
        Q_ASSERT (cached_img && isEmpty ());
        QImage *img = new QImage;
        *img = img_movie->currentImage ();
        cached_img->setImage (img);
        cached_img->flags = (int)(ImageData::ImagePixmap | ImageData::ImageAnimated); //TODO
        if (m_node)
            m_node->document ()->post (m_node, new Posting (m_node, MsgMediaUpdated));
    }
}

void ImageMedia::movieStatus (QMovie::MovieState status) {
    if (QMovie::NotRunning == status && m_node)
        m_node->document ()->post (m_node, new Posting (m_node, MsgMediaFinished));
}

//------------------------%<----------------------------------------------------

static int default_font_size = -1;

TextMedia::TextMedia (MediaManager *manager, Node *node, const QByteArray &ba)
 : MediaObject (manager, node) {
    QByteArray data (ba);
    if (!data [data.size () - 1])
        data.resize (data.size () - 1); // strip zero terminate char
    QTextStream ts (data, QIODevice::ReadOnly);
    QString val = convertNode <Element> (node)->getAttribute ("charset");
    if (!val.isEmpty ()) {
        QTextCodec *codec = QTextCodec::codecForName (val.toLatin1 ());
        if (codec)
            ts.setCodec (codec);
    }
    if (node->mrl() && node->mrl()->mimetype == "text/html") {
        Document *doc = new Document (QString ());
        NodePtr store = doc;
        readXML (doc, ts, QString ());
        text = doc->innerText ();
        doc->dispose ();
    } else {
        text = ts.readAll ();
    }
}

TextMedia::~TextMedia () {
}

bool TextMedia::play () {
    return !text.isEmpty ();
}

int TextMedia::defaultFontSize () {
    if (default_font_size < 0)
        default_font_size = QApplication::font ().pointSize ();
    return default_font_size;
}

#include "moc_mediaobject.cpp"
