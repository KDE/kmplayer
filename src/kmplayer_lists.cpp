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
#include <qurl.h>
#include <qtextstream.h>
#include <qbytearray.h>
#include <QStandardPaths>

#include <kinputdialog.h>
#include <kfiledialog.h>
#include <kglobal.h>
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
    activated = true;
    play (m_current ? m_current->mrl () : NULL);
}

QString ListsSource::prettyName ()
{
    return ((KMPlayer::PlaylistRole *)m_document->role (KMPlayer::RolePlaylist))->caption ();
}

KDE_NO_CDTOR_EXPORT FileDocument::FileDocument (short i, const QString &s, KMPlayer::Source *src)
 : KMPlayer::SourceDocument (src, s), load_tree_version ((unsigned int)-1) {
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
    if (QFileInfo (file).exists ()) {
        file.open (QIODevice::ReadOnly);
        QTextStream inxml (&file);
        inxml.setCodec("UTF-8");
        KMPlayer::readXML (this, inxml, QString (), false);
        normalize ();
    }
    load_tree_version = m_tree_version;
}

void FileDocument::writeToFile (const QString & fn) {
    QFile file (fn);
    kDebug () << "writeToFile " << fn;
    file.open (QIODevice::WriteOnly | QIODevice::Truncate);
    file.write (outerXML ().toUtf8 ());
    load_tree_version = m_tree_version;
}

void FileDocument::sync (const QString &fn)
{
    if (resolved && load_tree_version != m_tree_version)
        writeToFile (fn);
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
        readFromFile(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/kmplayer/recent.xml");
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
    if (KMPlayer::MsgChildFinished == msg)
        finish ();
    else
        FileDocument::message (msg, data);
}

KDE_NO_CDTOR_EXPORT
Recent::Recent (KMPlayer::NodePtr & doc, KMPlayerApp * a, const QString &url)
  : KMPlayer::Mrl (doc, id_node_recent_node), app (a) {
    src = url;
    setAttribute (KMPlayer::Ids::attr_url, url);
}

KDE_NO_EXPORT void Recent::closed () {
    src = getAttribute (KMPlayer::Ids::attr_url);
    Mrl::closed ();
}

KDE_NO_EXPORT void Recent::activate () {
    app->openDocumentFile (KUrl (src));
}

KDE_NO_CDTOR_EXPORT
Group::Group (KMPlayer::NodePtr & doc, KMPlayerApp * a, const QString & pn)
  : KMPlayer::Element (doc, KMPlayer::id_node_group_node), app (a) {
    title = pn;
    if (!pn.isEmpty ())
        setAttribute (KMPlayer::Ids::attr_title, pn);
}

KDE_NO_EXPORT KMPlayer::Node *Group::childFromTag (const QString & tag) {
    if (tag == QString::fromLatin1 ("item"))
        return new Recent (m_doc, app);
    else if (tag == QString::fromLatin1 ("group"))
        return new Group (m_doc, app);
    return 0L;
}

KDE_NO_EXPORT void Group::closed () {
    title = getAttribute (KMPlayer::Ids::attr_title);
    Element::closed ();
}

void *Group::role (KMPlayer::RoleType msg, void *content)
{
    if (KMPlayer::RolePlaylist == msg)
        return (KMPlayer::PlaylistRole *) this ;
    return Element::role (msg, content);
}

KDE_NO_EXPORT void Playlist::defer () {
    if (playmode) {
        KMPlayer::Document::defer ();
        // Hack: Node::undefer will restart first item when state=init
        if (firstChild() && KMPlayer::Node::state_init == firstChild()->state)
            firstChild()->state = KMPlayer::Node::state_activated;
    } else if (!resolved) {
        resolved = true;
        readFromFile(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/kmplayer/playlist.xml");
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
    QByteArray ba = tag.toUtf8 ();
    const char *name = ba.constData ();
    if (!strcmp (name, "item"))
        return new PlaylistItem (m_doc, app, playmode);
    else if (!strcmp (name, "group"))
        return new PlaylistGroup (m_doc, app, playmode);
    else if (!strcmp (name, "object"))
        return new HtmlObject (m_doc, app, playmode);
    return FileDocument::childFromTag (tag);
}

KDE_NO_EXPORT void Playlist::message (KMPlayer::MessageType msg, void *data) {
    if (KMPlayer::MsgChildFinished == msg && !playmode)
        finish ();
    else
        FileDocument::message (msg, data);
}

KDE_NO_CDTOR_EXPORT
PlaylistItemBase::PlaylistItemBase (KMPlayer::NodePtr &d, short i, KMPlayerApp *a, bool pm)
    : KMPlayer::Mrl (d, i), app (a), playmode (pm) {
    editable = !pm;
}

KDE_NO_EXPORT void PlaylistItemBase::activate () {
    if (playmode) {
        Mrl::activate ();
    } else {
        ListsSource * source = static_cast <ListsSource *> (app->player ()->sources () ["listssource"]);
        Playlist *pl = new Playlist (app, source, true);
        KMPlayer::NodePtr n = pl;
        pl->src.clear ();
        QString data;
        QString pn;
        if (parentNode ()->id == KMPlayer::id_node_group_node) {
            data = QString ("<playlist>") +
                parentNode ()->innerXML () +
                QString ("</playlist>");
            pn = ((KMPlayer::PlaylistRole *)parentNode ()->role (KMPlayer::RolePlaylist))->caption ();
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
    title = getAttribute (KMPlayer::Ids::attr_title);
    Mrl::closed ();
}

KDE_NO_CDTOR_EXPORT
PlaylistItem::PlaylistItem (KMPlayer::NodePtr & doc, KMPlayerApp *a, bool pm, const QString &url)
 : PlaylistItemBase (doc, KMPlayer::id_node_playlist_item, a, pm) {
    src = url;
    setAttribute (KMPlayer::Ids::attr_url, url);
}

KDE_NO_EXPORT void PlaylistItem::closed () {
    src = getAttribute (KMPlayer::Ids::attr_url);
    PlaylistItemBase::closed ();
}

KDE_NO_EXPORT void PlaylistItem::begin () {
    if (playmode && firstChild ())
        firstChild ()->activate ();
    else
        Mrl::begin ();
}

KDE_NO_EXPORT void PlaylistItem::setNodeName (const QString & s) {
    bool uri = s.startsWith (QChar ('/'));
    if (!uri) {
        int p = s.indexOf ("://");
        uri = p > 0 && p < 10;
    }
    if (uri) {
        if (title.isEmpty () || title == src)
            title = s;
        src = s;
        setAttribute (KMPlayer::Ids::attr_url, s);
    } else {
        title = s;
        setAttribute (KMPlayer::Ids::attr_title, s);
    }
}

KDE_NO_CDTOR_EXPORT
PlaylistGroup::PlaylistGroup (KMPlayer::NodePtr &doc, KMPlayerApp *a, const QString &pn)
  : KMPlayer::Element (doc, KMPlayer::id_node_group_node), app (a), playmode (false) {
    title = pn;
    if (!pn.isEmpty ())
        setAttribute (KMPlayer::Ids::attr_title, pn);
}

KDE_NO_CDTOR_EXPORT
PlaylistGroup::PlaylistGroup (KMPlayer::NodePtr &doc, KMPlayerApp *a, bool lm)
  : KMPlayer::Element (doc, KMPlayer::id_node_group_node), app (a), playmode (lm) {
    editable = !lm;
}

KDE_NO_EXPORT KMPlayer::Node *PlaylistGroup::childFromTag (const QString &tag) {
    QByteArray ba = tag.toUtf8 ();
    const char *name = ba.constData ();
    if (!strcmp (name, "item"))
        return new PlaylistItem (m_doc, app, playmode);
    else if (!strcmp (name, "group"))
        return new PlaylistGroup (m_doc, app, playmode);
    else if (!strcmp (name, "object"))
        return new HtmlObject (m_doc, app, playmode);
    return 0L;
}

KDE_NO_EXPORT void PlaylistGroup::closed () {
    title = getAttribute (KMPlayer::Ids::attr_title);
    Element::closed ();
}

KDE_NO_EXPORT void PlaylistGroup::setNodeName (const QString &t) {
    title = t;
    setAttribute (KMPlayer::Ids::attr_title, t);
}

void *PlaylistGroup::role (KMPlayer::RoleType msg, void *content)
{
    if (KMPlayer::RolePlaylist == msg)
        return (KMPlayer::PlaylistRole *) this ;
    return Element::role (msg, content);
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
            QString name = e->getAttribute (KMPlayer::Ids::attr_name);
            if (name == "type")
                mimetype = e->getAttribute (KMPlayer::Ids::attr_value);
            else if (name == "movie")
                src = e->getAttribute (KMPlayer::Ids::attr_value);
        } else if (n->id == KMPlayer::id_node_html_embed) {
            KMPlayer::Element *e = static_cast <KMPlayer::Element *> (n);
            QString type = e->getAttribute (KMPlayer::Ids::attr_type);
            if (!type.isEmpty ())
                mimetype = type;
            QString asrc = e->getAttribute (KMPlayer::Ids::attr_src);
            if (!asrc.isEmpty ())
                src = asrc;
        }
    }
    PlaylistItemBase::closed ();
}

KDE_NO_EXPORT KMPlayer::Node *HtmlObject::childFromTag (const QString & tag) {
    QByteArray ba = tag.toUtf8 ();
    const char *name = ba.constData ();
    if (!strcasecmp (name, "param"))
        return new KMPlayer::DarkNode (m_doc, name, KMPlayer::id_node_param);
    else if (!strcasecmp (name, "embed"))
        return new KMPlayer::DarkNode(m_doc, name,KMPlayer::id_node_html_embed);
    return NULL;
}

Generator::Generator (KMPlayerApp *a)
 : FileDocument (id_node_gen_document, QString (),
            a->player ()->sources () ["listssource"]),
   app (a), qprocess (NULL), data (NULL)
{}

KMPlayer::Node *Generator::childFromTag (const QString &tag) {
    QByteArray ba = tag.toUtf8();
    const char *ctag = ba.constData ();
    if (!strcmp (ctag, "generator"))
        return new GeneratorElement (m_doc, tag, id_node_gen_generator);
    return NULL;
}

QString Generator::genReadAsk (KMPlayer::Node *n) {
    QString title;
    QString desc;
    QString type = static_cast <Element *> (n)->getAttribute (
            KMPlayer::Ids::attr_type);
    QString key = static_cast<Element*>(n)->getAttribute ("key");
    QString def = static_cast<Element*>(n)->getAttribute ("default");
    QString input;
    KConfigGroup cfg (KGlobal::config(), "Generator Defaults");
    if (!key.isEmpty ())
        def = cfg.readEntry (key, def);
    if (type == "file") {
        input = KFileDialog::getOpenFileName (KUrl (def), QString(), app);
    } else if (type == "dir") {
        input = KFileDialog::getExistingDirectoryUrl (KUrl (def), app).toLocalFile ();
        if (!input.isEmpty ())
            input += QChar ('/');
    } else {
        for (KMPlayer::Node *c = n->firstChild (); c; c = c->nextSibling ())
            switch (c->id) {
                case id_node_gen_title:
                    title = c->innerText ().simplified ();
                    break;
                case id_node_gen_description:
                    desc = c->innerText ().simplified ();
                    break;
            }
        input = KInputDialog::getText (title, desc, def);
    }
    if (input.isNull ())
        canceled = true;
    else if (!key.isEmpty ())
        cfg.writeEntry (key, input);
    return input;
}

QString Generator::genReadUriGet (KMPlayer::Node *n) {
    QString str;
    bool first = true;
    for (KMPlayer::Node *c = n->firstChild (); c && !canceled; c = c->nextSibling ()) {
        QString key;
        QString val;
        switch (c->id) {
        case id_node_gen_http_key_value: {
            KMPlayer::Node *q = c->firstChild ();
            if (q) {
                key = genReadString (q);
                q = q->nextSibling ();
                if (q && !canceled)
                    val = genReadString (q);
            }
            break;
        }
        default:
            key = genReadString (c);
            break;
        }
        if (!key.isEmpty ()) {
            if (first) {
                str += QChar ('?');
                first = false;
            } else {
                str += QChar ('&');
            }
            str += QUrl::toPercentEncoding (key);
            if (!val.isEmpty ())
                str += QChar ('=') + QString (QUrl::toPercentEncoding (val));
        }
    }
    return str;
}

QString Generator::genReadString (KMPlayer::Node *n) {
    QString str;
    bool need_quote = quote;
    bool find_resource = false;
    quote = false;
    for (KMPlayer::Node *c = n->firstChild (); c && !canceled; c = c->nextSibling ())
        switch (c->id) {
        case id_node_gen_uri:
        case id_node_gen_sequence:
            str += genReadString (c);
            break;
        case id_node_gen_literal:
            str += c->innerText ().simplified ();
            break;
        case id_node_gen_predefined: {
            QString val = static_cast <Element *>(c)->getAttribute ("key");
            if (val == "data" || val == "sysdata") {
                str += "kmplayer";
                find_resource = true;
            }
            break;
        }
        case id_node_gen_http_get:
            str += genReadUriGet (c);
            break;
        case id_node_gen_ask:
            str += genReadAsk (c);
            break;
        case KMPlayer::id_node_text:
             str += c->nodeValue ().simplified ();
        }
    if (find_resource)
        str = QStandardPaths::locate(QStandardPaths::GenericDataLocation, str);
    if (!static_cast <Element *>(n)->getAttribute ("encoding").isEmpty ())
        str = QUrl::toPercentEncoding (str);
    if (need_quote) {
        //from QProcess' parseCombinedArgString
        str.replace (QChar ('"'), QString ("\"\"\""));
        str = QChar ('"') + str + QChar ('"');
        quote = true;
    }
    return str;
}

QString Generator::genReadInput (KMPlayer::Node *n) {
    quote = false;
    return genReadString (n);
}

QString Generator::genReadProcess (KMPlayer::Node *n) {
    QString process;
    QString str;
    quote = true;
    for (KMPlayer::Node *c = n->firstChild (); c && !canceled; c = c->nextSibling ())
        switch (c->id) {
        case id_node_gen_program:
            process = QString (genReadString (c));
            break;
        case id_node_gen_argument:
            process += QChar (' ') + genReadString (c);
            break;
        }
    return process;
}

void Generator::activate () {
    QString input;
    canceled = false;
    KMPlayer::Node *n = firstChild ();
    if (n && n->id == id_node_gen_generator) {
        title = static_cast<Element *>(n)->getAttribute (
                KMPlayer::Ids::attr_name);
        for (KMPlayer::Node *c = n->firstChild (); c && !canceled; c = c->nextSibling ())
            switch (c->id) {
            case id_node_gen_input:
                input = genReadInput (c);
                break;
            case id_node_gen_process:
                process = genReadProcess (c);
            }
    }
    if (canceled)
        return;
    if (!input.isEmpty () && process.isEmpty ()) {
        message (KMPlayer::MsgInfoString, &input);
        //openFile (m_control->m_app, input);
    } else if (!process.isEmpty ()) {
        data = new QTextStream (&buffer);
        if (input.isEmpty ()) {
            message (KMPlayer::MsgInfoString, &process);
            begin ();
        } else {
            QString cmdline (input + " | " + process);
            message (KMPlayer::MsgInfoString, &cmdline);
            if (!media_info)
                media_info = new KMPlayer::MediaInfo (
                        this, KMPlayer::MediaManager::Data);
            state = state_activated;
            media_info->wget (input);
        }
    }
}

void Generator::begin () {
    if (!qprocess) {
        qprocess = new QProcess (app);
        connect (qprocess, SIGNAL (started ()),
                 this, SLOT (started ()));
        connect (qprocess, SIGNAL (error (QProcess::ProcessError)),
                 this, SLOT (error (QProcess::ProcessError)));
        connect (qprocess, SIGNAL (finished (int, QProcess::ExitStatus)),
                 this, SLOT (finished ()));
        connect (qprocess, SIGNAL (readyReadStandardOutput ()),
                 this, SLOT (readyRead ()));
    }
    QString info;
    if (media_info)
        info = QString ("Input data ") +
            QString::number (media_info->rawData ().size () / 1024.0) + "kb | ";
    info += process;
    message (KMPlayer::MsgInfoString, &info);
    kDebug() << process;
    qprocess->start (process);
    state = state_began;
}

void Generator::deactivate () {
    if (qprocess) {
        disconnect (qprocess, SIGNAL (started ()),
                    this, SLOT (started ()));
        disconnect (qprocess, SIGNAL (error (QProcess::ProcessError)),
                    this, SLOT (error (QProcess::ProcessError)));
        disconnect (qprocess, SIGNAL (finished (int, QProcess::ExitStatus)),
                    this, SLOT (finished ()));
        disconnect (qprocess, SIGNAL (readyReadStandardOutput ()),
                    this, SLOT (readyRead ()));
        qprocess->kill ();
        qprocess->deleteLater ();
    }
    qprocess = NULL;
    delete data;
    data = NULL;
    buffer.clear ();
    FileDocument::deactivate ();
}

void Generator::message (KMPlayer::MessageType msg, void *content) {
    if (KMPlayer::MsgMediaReady == msg) {
        if (!media_info->rawData ().size ()) {
            QString err ("No input data received");
            message (KMPlayer::MsgInfoString, &err);
            deactivate ();
        } else {
            begin ();
        }
    } else {
        FileDocument::message (msg, content);
    }
}

void Generator::readyRead () {
    if (qprocess->bytesAvailable ())
        *data << qprocess->readAll();
    if (qprocess->state () == QProcess::NotRunning) {
        if (!buffer.isEmpty ()) {
            Playlist *pl = new Playlist (app, m_source, true);
            KMPlayer::NodePtr n = pl;
            pl->src.clear ();
            QTextStream stream (&buffer, QIODevice::ReadOnly);
            KMPlayer::readXML (pl, stream, QString (), false);
            pl->title = title;
            pl->normalize ();
            message (KMPlayer::MsgInfoString, NULL);
            bool reset_only = m_source == app->player ()->source ();
            if (reset_only)
                app->player ()->stop ();
            m_source->setDocument (pl, pl);
            if (reset_only) {
                m_source->activate ();
                app->setCaption (getAttribute(KMPlayer::Ids::attr_name));
            } else {
                app->player ()->setSource (m_source);
            }
        } else {
            QString err ("No data received");
            message (KMPlayer::MsgInfoString, &err);
        }
        deactivate ();
    }
}

void Generator::started () {
    if (media_info) {
        QByteArray &ba = media_info->rawData ();
        // TODO validate utf8
        if (ba.size ())
            qprocess->write (ba);
        qprocess->closeWriteChannel ();
        return;
    }
    message (KMPlayer::MsgInfoString, &process);
}

void Generator::error (QProcess::ProcessError err) {
    kDebug () << (int)err;
    QString msg ("Couldn't start process");
    message (KMPlayer::MsgInfoString, &msg);
    deactivate ();
}

void Generator::finished () {
    if (active () && state_deferred != state)
        readyRead ();
}

struct GeneratorTag {
    const char *tag;
    short id;
} gen_tags[] = {
    { "input", id_node_gen_input },
    { "process", id_node_gen_process },
    { "uri", id_node_gen_uri },
    { "literal", id_node_gen_literal },
    { "ask", id_node_gen_ask },
    { "title", id_node_gen_title },
    { "description", id_node_gen_description },
    { "process", id_node_gen_process },
    { "program", id_node_gen_program },
    { "argument", id_node_gen_argument },
    { "predefined", id_node_gen_predefined },
    { "http-get", id_node_gen_http_get },
    { "key-value", id_node_gen_http_key_value },
    { "key", id_node_gen_sequence },
    { "value", id_node_gen_sequence },
    { "sequence", id_node_gen_sequence },
    { NULL, -1 }
};

KMPlayer::Node *GeneratorElement::childFromTag (const QString &tag) {
    QByteArray ba = tag.toUtf8();
    const char *ctag = ba.constData ();
    for (GeneratorTag *t = gen_tags; t->tag; ++t)
        if (!strcmp (ctag, t->tag))
            return new GeneratorElement (m_doc, tag, t->id);
    return NULL;
}

