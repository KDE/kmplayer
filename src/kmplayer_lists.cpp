/**
  This file belong to the KMPlayer project, a movie player plugin for Konqueror
  Copyright (C) 2009  Koos Vriezen <koos.vriezen@gmail.com>

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

#include <qfile.h>

#include <kstandarddirs.h>
#include <klocale.h>
#include <kdebug.h>

#include "kmplayer_lists.h"
#include "kmplayer.h"
#include "mediaobject.h"


KDE_NO_EXPORT void ListsSource::play (KMPlayer::Mrl *mrl) {
    if (m_player->source () == this)
        Source::play (mrl);
    else if (mrl)
        mrl->activate ();
}

KDE_NO_EXPORT void ListsSource::activate () {
    play (m_current ? m_current->mrl () : NULL);
}

KDE_NO_CDTOR_EXPORT FileDocument::FileDocument (short i, const QString &s, KMPlayer::Source *src)
 : KMPlayer::SourceDocument (src, s) {
    id = i;
}

KDE_NO_EXPORT KMPlayer::Node *FileDocument::childFromTag(const QString &tag) {
    if (tag == QString::fromLatin1 (nodeName ()))
        return this;
    return 0L;
}

void FileDocument::readFromFile (const QString & fn) {
    QFile file (fn);
    kDebug () << "readFromFile " << fn;
    if (file.exists ()) {
        file.open (IO_ReadOnly);
        QTextStream inxml (&file);
        KMPlayer::readXML (this, inxml, QString (), false);
        normalize ();
    }
}

void FileDocument::writeToFile (const QString & fn) {
    QFile file (fn);
    kDebug () << "writeToFile " << fn;
    file.open (IO_WriteOnly);
    QByteArray utf = outerXML ().toUtf8 ();
    file.writeBlock (utf, utf.length ());
}

KDE_NO_CDTOR_EXPORT Recents::Recents (KMPlayerApp *a)
    : FileDocument (id_node_recent_document, "recents://"),
      app(a) {
    title = i18n ("Most Recent");
}

KDE_NO_EXPORT void Recents::activate () {
    if (!resolved)
        defer ();
}

KDE_NO_EXPORT void Recents::defer () {
    if (!resolved) {
        resolved = true;
        readFromFile (KStandardDirs::locateLocal ("data", "kmplayer/recent.xml"));
    }
}

KDE_NO_EXPORT KMPlayer::Node *Recents::childFromTag (const QString & tag) {
    // kDebug () << nodeName () << " childFromTag " << tag;
    if (tag == QString::fromLatin1 ("item"))
        return new Recent (m_doc, app);
    else if (tag == QString::fromLatin1 ("group"))
        return new Group (m_doc, app);
    return FileDocument::childFromTag (tag);
}

KDE_NO_EXPORT void Recents::message (KMPlayer::MessageType msg, void *data) {
    if (KMPlayer::MsgChildFinished)
        finish ();
    else
        FileDocument::message (msg, data);
}

KDE_NO_CDTOR_EXPORT
Recent::Recent (KMPlayer::NodePtr & doc, KMPlayerApp * a, const QString &url)
  : KMPlayer::Mrl (doc, id_node_recent_node), app (a) {
    src = url;
    setAttribute (KMPlayer::StringPool::attr_url, url);
}

KDE_NO_EXPORT void Recent::closed () {
    if (src.isEmpty ())
        src = getAttribute (KMPlayer::StringPool::attr_url);
}

KDE_NO_EXPORT void Recent::activate () {
    app->openDocumentFile (KUrl (src));
}

KDE_NO_CDTOR_EXPORT
Group::Group (KMPlayer::NodePtr & doc, KMPlayerApp * a, const QString & pn)
  : KMPlayer::Title (doc, KMPlayer::id_node_group_node), app (a) {
    title = pn;
    if (!pn.isEmpty ())
        setAttribute (KMPlayer::StringPool::attr_title, pn);
}

KDE_NO_EXPORT KMPlayer::Node *Group::childFromTag (const QString & tag) {
    if (tag == QString::fromLatin1 ("item"))
        return new Recent (m_doc, app);
    else if (tag == QString::fromLatin1 ("group"))
        return new Group (m_doc, app);
    return 0L;
}

KDE_NO_EXPORT void Group::closed () {
    if (title.isEmpty ())
        title = getAttribute (KMPlayer::StringPool::attr_title);
}

KDE_NO_EXPORT void Playlist::defer () {
    if (playmode)
        KMPlayer::Document::defer ();
    else if (!resolved) {
        resolved = true;
        readFromFile (KStandardDirs::locateLocal ("data", "kmplayer/playlist.xml"));
    }
}

KDE_NO_EXPORT void Playlist::activate () {
    if (playmode)
        KMPlayer::Document::activate ();
    else if (!resolved)
        defer ();
}

KDE_NO_CDTOR_EXPORT Playlist::Playlist (KMPlayerApp *a, KMPlayer::Source *s, bool plmode)
    : FileDocument (KMPlayer::id_node_playlist_document, "Playlist://", s),
      app(a),
      playmode (plmode) {
    title = i18n ("Persistent Playlists");
}

KDE_NO_EXPORT KMPlayer::Node *Playlist::childFromTag (const QString & tag) {
    // kDebug () << nodeName () << " childFromTag " << tag;
    const char * name = tag.ascii ();
    if (!strcmp (name, "item"))
        return new PlaylistItem (m_doc, app, playmode);
    else if (!strcmp (name, "group"))
        return new PlaylistGroup (m_doc, app, playmode);
    else if (!strcmp (name, "object"))
        return new HtmlObject (m_doc, app, playmode);
    return FileDocument::childFromTag (tag);
}

KDE_NO_EXPORT void Playlist::message (KMPlayer::MessageType msg, void *data) {
    if (KMPlayer::MsgChildFinished && !playmode)
        finish ();
    else
        FileDocument::message (msg, data);
}

KDE_NO_CDTOR_EXPORT
PlaylistItemBase::PlaylistItemBase (KMPlayer::NodePtr &d, short i, KMPlayerApp *a, bool pm)
    : KMPlayer::Mrl (d, i), app (a), playmode (pm) {
}

KDE_NO_EXPORT void PlaylistItemBase::activate () {
    if (playmode) {
        Mrl::activate ();
    } else {
        ListsSource * source = static_cast <ListsSource *> (app->player ()->sources () ["listssource"]);
        KMPlayer::NodePtr pl = new Playlist (app, source, true);
        QString data;
        QString pn;
        if (parentNode ()->id == KMPlayer::id_node_group_node) {
            data = QString ("<playlist>") +
                parentNode ()->innerXML () +
                QString ("</playlist>");
            pn = parentNode ()->caption ();
        } else {
            data = outerXML ();
            pn = title.isEmpty () ? src : title;
        }
        pl->setCaption (pn);
        //kDebug () << "cloning to " << data;
        QTextStream inxml (&data, QIODevice::ReadOnly);
        KMPlayer::readXML (pl, inxml, QString (), false);
        pl->normalize ();
        KMPlayer::Node *cur = pl->firstChild ();
        pl->mrl ()->resolved = !!cur;
        if (parentNode ()->id == KMPlayer::id_node_group_node && cur) {
            KMPlayer::Node *sister = parentNode ()->firstChild ();
            while (sister && cur && sister != this) {
                sister = sister->nextSibling ();
                cur = cur->nextSibling ();
            }
        }
        bool reset_only = source == app->player ()->source ();
        if (reset_only)
            app->player ()->stop ();
        source->setDocument (pl, cur);
        if (reset_only) {
            source->activate ();
            app->setCaption (pn);
        } else
            app->player ()->setSource (source);
    }
}

void PlaylistItemBase::closed () {
    if (title.isEmpty ())
        title = getAttribute (KMPlayer::StringPool::attr_title);
}

KDE_NO_CDTOR_EXPORT
PlaylistItem::PlaylistItem (KMPlayer::NodePtr & doc, KMPlayerApp *a, bool pm, const QString &url)
 : PlaylistItemBase (doc, KMPlayer::id_node_playlist_item, a, pm) {
    src = url;
    setAttribute (KMPlayer::StringPool::attr_url, url);
}

KDE_NO_EXPORT void PlaylistItem::closed () {
    if (src.isEmpty ())
        src = getAttribute (KMPlayer::StringPool::attr_url);
    PlaylistItemBase::closed ();
}

KDE_NO_EXPORT void PlaylistItem::begin () {
    if (playmode && firstChild ())
        firstChild ()->activate ();
    else
        Mrl::begin ();
}

KDE_NO_EXPORT void PlaylistItem::setNodeName (const QString & s) {
    src = s;
    setAttribute (KMPlayer::StringPool::attr_url, s);
}

KDE_NO_CDTOR_EXPORT
PlaylistGroup::PlaylistGroup (KMPlayer::NodePtr &doc, KMPlayerApp *a, const QString &pn)
  : KMPlayer::Title (doc, KMPlayer::id_node_group_node), app (a), playmode (false) {
    title = pn;
    if (!pn.isEmpty ())
        setAttribute (KMPlayer::StringPool::attr_title, pn);
}

KDE_NO_CDTOR_EXPORT
PlaylistGroup::PlaylistGroup (KMPlayer::NodePtr &doc, KMPlayerApp *a, bool lm)
  : KMPlayer::Title (doc, KMPlayer::id_node_group_node), app (a), playmode (lm) {
}

KDE_NO_EXPORT KMPlayer::Node *PlaylistGroup::childFromTag (const QString &tag) {
    const char * name = tag.ascii ();
    if (!strcmp (name, "item"))
        return new PlaylistItem (m_doc, app, playmode);
    else if (!strcmp (name, "group"))
        return new PlaylistGroup (m_doc, app, playmode);
    else if (!strcmp (name, "object"))
        return new HtmlObject (m_doc, app, playmode);
    return 0L;
}

KDE_NO_EXPORT void PlaylistGroup::closed () {
    if (title.isEmpty ())
        title = getAttribute (KMPlayer::StringPool::attr_title);
}

KDE_NO_EXPORT void PlaylistGroup::setNodeName (const QString &t) {
    title = t;
    setAttribute (KMPlayer::StringPool::attr_title, t);
}

KDE_NO_CDTOR_EXPORT
HtmlObject::HtmlObject (KMPlayer::NodePtr &doc, KMPlayerApp *a, bool pm)
  : PlaylistItemBase (doc, KMPlayer::id_node_html_object, a, pm) {}

KDE_NO_EXPORT void HtmlObject::activate () {
    if (playmode)
        KMPlayer::Mrl::activate ();
    else
        PlaylistItemBase::activate ();
}

KDE_NO_EXPORT void HtmlObject::closed () {
    for (Node *n = firstChild (); n; n = n->nextSibling ()) {
        if (n->id == KMPlayer::id_node_param) {
            KMPlayer::Element *e = static_cast <KMPlayer::Element *> (n);
            QString name = e->getAttribute (KMPlayer::StringPool::attr_name);
            if (name == "type")
                mimetype = e->getAttribute (KMPlayer::StringPool::attr_value);
            else if (name == "movie")
                src = e->getAttribute (KMPlayer::StringPool::attr_value);
        } else if (n->id == KMPlayer::id_node_html_embed) {
            KMPlayer::Element *e = static_cast <KMPlayer::Element *> (n);
            QString type = e->getAttribute (KMPlayer::StringPool::attr_type);
            if (!type.isEmpty ())
                mimetype = type;
            QString asrc = e->getAttribute (KMPlayer::StringPool::attr_src);
            if (!asrc.isEmpty ())
                src = asrc;
        }
    }
    PlaylistItemBase::closed ();
}

KDE_NO_EXPORT KMPlayer::Node *HtmlObject::childFromTag (const QString & tag) {
    const char *name = tag.ascii ();
    if (!strcasecmp (name, "param"))
        return new KMPlayer::DarkNode (m_doc, name, KMPlayer::id_node_param);
    else if (!strcasecmp (name, "embed"))
        return new KMPlayer::DarkNode(m_doc, name,KMPlayer::id_node_html_embed);
    return NULL;
}
