/*
    This file belong to the KMPlayer project, a movie player plugin for Konqueror
    SPDX-FileCopyrightText: 2009 Koos Vriezen <koos.vriezen@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include <QFile>
#include <QUrl>
#include <QTextStream>
#include <QByteArray>
#include <QInputDialog>
#include <QStandardPaths>
#include <QFileDialog>

#include <KSharedConfig>
#include <KLocalizedString>

#include "kmplayerapp_log.h"
#include "kmplayer_lists.h"
#include "kmplayer.h"
#include "mediaobject.h"


void ListsSource::play (KMPlayer::Mrl *mrl) {
    if (m_player->source () == this)
        Source::play (mrl);
    else if (mrl)
        mrl->activate ();
}

void ListsSource::activate () {
    activated = true;
    play (m_current ? m_current->mrl () : nullptr);
}

QString ListsSource::prettyName ()
{
    return ((KMPlayer::PlaylistRole *)m_document->role (KMPlayer::RolePlaylist))->caption ();
}

FileDocument::FileDocument (short i, const QString &s, KMPlayer::Source *src)
 : KMPlayer::SourceDocument (src, s), load_tree_version ((unsigned int)-1) {
    id = i;
}

KMPlayer::Node *FileDocument::childFromTag(const QString &tag) {
    if (tag == QString::fromLatin1 (nodeName ()))
        return this;
    return nullptr;
}

void FileDocument::readFromFile (const QString & fn) {
    QFile file (fn);
    qCDebug(LOG_KMPLAYER_APP) << "readFromFile " << fn;
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
    qCDebug(LOG_KMPLAYER_APP) << "writeToFile " << fn;
    file.open (QIODevice::WriteOnly | QIODevice::Truncate);
    file.write (outerXML ().toUtf8 ());
    load_tree_version = m_tree_version;
}

void FileDocument::sync (const QString &fn)
{
    if (resolved && load_tree_version != m_tree_version)
        writeToFile (fn);
}

Recents::Recents (KMPlayerApp *a)
    : FileDocument (id_node_recent_document, "recents://"),
      app(a) {
    title = i18n ("Most Recent");
    bookmarkable = false;
}

void Recents::activate () {
    if (!resolved)
        defer ();
}

void Recents::defer () {
    if (!resolved) {
        resolved = true;
        readFromFile(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/kmplayer/recent.xml");
    }
}

KMPlayer::Node *Recents::childFromTag (const QString & tag) {
    // qCDebug(LOG_KMPLAYER_APP) << nodeName () << " childFromTag " << tag;
    if (tag == QString::fromLatin1 ("item"))
        return new Recent (m_doc, app);
    else if (tag == QString::fromLatin1 ("group"))
        return new Group (m_doc, app);
    return FileDocument::childFromTag (tag);
}

void Recents::message (KMPlayer::MessageType msg, void *data) {
    if (KMPlayer::MsgChildFinished == msg)
        finish ();
    else
        FileDocument::message (msg, data);
}

Recent::Recent (KMPlayer::NodePtr & doc, KMPlayerApp * a, const QString &url)
  : KMPlayer::Mrl (doc, id_node_recent_node), app (a) {
    src = url;
    setAttribute (KMPlayer::Ids::attr_url, url);
}

void Recent::closed () {
    src = getAttribute (KMPlayer::Ids::attr_url);
    Mrl::closed ();
}

void Recent::activate () {
    app->openDocumentFile (QUrl (src));
}

Group::Group (KMPlayer::NodePtr & doc, KMPlayerApp * a, const QString & pn)
  : KMPlayer::Element (doc, KMPlayer::id_node_group_node), app (a) {
    title = pn;
    if (!pn.isEmpty ())
        setAttribute (KMPlayer::Ids::attr_title, pn);
}

KMPlayer::Node *Group::childFromTag (const QString & tag) {
    if (tag == QString::fromLatin1 ("item"))
        return new Recent (m_doc, app);
    else if (tag == QString::fromLatin1 ("group"))
        return new Group (m_doc, app);
    return nullptr;
}

void Group::closed () {
    title = getAttribute (KMPlayer::Ids::attr_title);
    Element::closed ();
}

void *Group::role (KMPlayer::RoleType msg, void *content)
{
    if (KMPlayer::RolePlaylist == msg)
        return (KMPlayer::PlaylistRole *) this ;
    return Element::role (msg, content);
}

void Playlist::defer () {
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

void Playlist::activate () {
    if (playmode)
        KMPlayer::Document::activate ();
    else if (!resolved)
        defer ();
}

Playlist::Playlist (KMPlayerApp *a, KMPlayer::Source *s, bool plmode)
    : FileDocument (KMPlayer::id_node_playlist_document, "Playlist://", s),
      app(a),
      playmode (plmode) {
    title = i18n ("Persistent Playlists");
    bookmarkable = false;
}

KMPlayer::Node *Playlist::childFromTag (const QString & tag) {
    // qCDebug(LOG_KMPLAYER_APP) << nodeName () << " childFromTag " << tag;
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

void Playlist::message (KMPlayer::MessageType msg, void *data) {
    if (KMPlayer::MsgChildFinished == msg && !playmode)
        finish ();
    else
        FileDocument::message (msg, data);
}

PlaylistItemBase::PlaylistItemBase (KMPlayer::NodePtr &d, short i, KMPlayerApp *a, bool pm)
    : KMPlayer::Mrl (d, i), app (a), playmode (pm) {
    editable = !pm;
}

void PlaylistItemBase::activate () {
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
        //qCDebug(LOG_KMPLAYER_APP) << "cloning to " << data;
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

PlaylistItem::PlaylistItem (KMPlayer::NodePtr & doc, KMPlayerApp *a, bool pm, const QString &url)
 : PlaylistItemBase (doc, KMPlayer::id_node_playlist_item, a, pm) {
    src = url;
    setAttribute (KMPlayer::Ids::attr_url, url);
}

void PlaylistItem::closed () {
    src = getAttribute (KMPlayer::Ids::attr_url);
    PlaylistItemBase::closed ();
}

void PlaylistItem::begin () {
    if (playmode && firstChild ())
        firstChild ()->activate ();
    else
        Mrl::begin ();
}

void PlaylistItem::setNodeName (const QString & s) {
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

PlaylistGroup::PlaylistGroup (KMPlayer::NodePtr &doc, KMPlayerApp *a, const QString &pn)
  : KMPlayer::Element (doc, KMPlayer::id_node_group_node), app (a), playmode (false) {
    title = pn;
    editable = true;
    if (!pn.isEmpty ())
        setAttribute (KMPlayer::Ids::attr_title, pn);
}

PlaylistGroup::PlaylistGroup (KMPlayer::NodePtr &doc, KMPlayerApp *a, bool lm)
  : KMPlayer::Element (doc, KMPlayer::id_node_group_node), app (a), playmode (lm) {
    editable = !lm;
}

KMPlayer::Node *PlaylistGroup::childFromTag (const QString &tag) {
    QByteArray ba = tag.toUtf8 ();
    const char *name = ba.constData ();
    if (!strcmp (name, "item"))
        return new PlaylistItem (m_doc, app, playmode);
    else if (!strcmp (name, "group"))
        return new PlaylistGroup (m_doc, app, playmode);
    else if (!strcmp (name, "object"))
        return new HtmlObject (m_doc, app, playmode);
    return nullptr;
}

void PlaylistGroup::closed () {
    title = getAttribute (KMPlayer::Ids::attr_title);
    Element::closed ();
}

void PlaylistGroup::setNodeName (const QString &t) {
    title = t;
    setAttribute (KMPlayer::Ids::attr_title, t);
}

void *PlaylistGroup::role (KMPlayer::RoleType msg, void *content)
{
    if (KMPlayer::RolePlaylist == msg)
        return (KMPlayer::PlaylistRole *) this ;
    return Element::role (msg, content);
}

HtmlObject::HtmlObject (KMPlayer::NodePtr &doc, KMPlayerApp *a, bool pm)
  : PlaylistItemBase (doc, KMPlayer::id_node_html_object, a, pm) {}

void HtmlObject::activate () {
    if (playmode)
        KMPlayer::Mrl::activate ();
    else
        PlaylistItemBase::activate ();
}

void HtmlObject::closed () {
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

KMPlayer::Node *HtmlObject::childFromTag (const QString & tag) {
    QByteArray ba = tag.toUtf8 ();
    const char *name = ba.constData ();
    if (!strcasecmp (name, "param"))
        return new KMPlayer::DarkNode (m_doc, name, KMPlayer::id_node_param);
    else if (!strcasecmp (name, "embed"))
        return new KMPlayer::DarkNode(m_doc, name,KMPlayer::id_node_html_embed);
    return nullptr;
}

Generator::Generator (KMPlayerApp *a)
 : FileDocument (id_node_gen_document, QString (),
            a->player ()->sources () ["listssource"]),
   app (a), qprocess (nullptr), data (nullptr)
{}

KMPlayer::Node *Generator::childFromTag (const QString &tag) {
    QByteArray ba = tag.toUtf8();
    const char *ctag = ba.constData ();
    if (!strcmp (ctag, "generator"))
        return new GeneratorElement (m_doc, tag, id_node_gen_generator);
    return nullptr;
}

QString Generator::genReadAsk (KMPlayer::Node *n) {
    QString title;
    QString desc;
    QString type = static_cast <Element *> (n)->getAttribute (
            KMPlayer::Ids::attr_type);
    QString key = static_cast<Element*>(n)->getAttribute ("key");
    QString def = static_cast<Element*>(n)->getAttribute ("default");
    QString input;
    KConfigGroup cfg(KSharedConfig::openConfig(), "Generator Defaults");
    if (!key.isEmpty ())
        def = cfg.readEntry (key, def);
    if (type == "file") {
        input = QFileDialog::getOpenFileUrl (app, QString(), QUrl::fromUserInput(def)).url();
    } else if (type == "dir") {
        input = QFileDialog::getExistingDirectoryUrl (app, QString(), QUrl::fromUserInput(def)).url();
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
        input = QInputDialog::getText(nullptr, title, desc, QLineEdit::Normal, def);
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

QString Generator::ProgramCmd::toString() const
{
    return program + QLatin1Char(' ') + args.join(QLatin1Char(' '));
}
Generator::ProgramCmd Generator::genReadProgramCmd (KMPlayer::Node *n)
{
    ProgramCmd programCmd;
    quote = true;
    for (KMPlayer::Node *c = n->firstChild (); c && !canceled; c = c->nextSibling ())
        switch (c->id) {
        case id_node_gen_program:
            programCmd.program = QString (genReadString (c));
            break;
        case id_node_gen_argument:
            programCmd.args.append(genReadString(c));
            break;
        }
    return programCmd;
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
                process = genReadProgramCmd (c);
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
            QString cmd = process.toString();
            message (KMPlayer::MsgInfoString, &cmd);
            begin ();
        } else {
            QString cmdline (input + " | " + process.toString());
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
        connect (qprocess, &QProcess::started,
                 this, &Generator::started);
        connect (qprocess, &QProcess::errorOccurred,
                 this, &Generator::error);
        connect (qprocess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                 this, &Generator::finished);
        connect (qprocess, &QProcess::readyReadStandardOutput,
                 this, &Generator::readyRead);
    }
    QString info;
    if (media_info)
        info = QString ("Input data ") +
            QString::number (media_info->rawData ().size () / 1024.0) + "kb | ";
    info += process.toString();
    message (KMPlayer::MsgInfoString, &info);
    qCDebug(LOG_KMPLAYER_APP) << process.toString();
    qprocess->start (process.program, process.args);
    state = state_began;
}

void Generator::deactivate () {
    if (qprocess) {
        disconnect (qprocess, &QProcess::started,
                    this, &Generator::started);
        disconnect (qprocess, &QProcess::errorOccurred,
                    this, &Generator::error);
        disconnect (qprocess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                    this, &Generator::finished);
        disconnect (qprocess, &QProcess::readyReadStandardOutput,
                    this, &Generator::readyRead);
        qprocess->kill ();
        qprocess->deleteLater ();
    }
    qprocess = nullptr;
    delete data;
    data = nullptr;
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
            message (KMPlayer::MsgInfoString, nullptr);
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
    QString cmd = process.toString();
    message (KMPlayer::MsgInfoString, &cmd);
}

void Generator::error (QProcess::ProcessError err) {
    qCDebug(LOG_KMPLAYER_APP) << (int)err;
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
    { nullptr, -1 }
};

KMPlayer::Node *GeneratorElement::childFromTag (const QString &tag) {
    QByteArray ba = tag.toUtf8();
    const char *ctag = ba.constData ();
    for (GeneratorTag *t = gen_tags; t->tag; ++t)
        if (!strcmp (ctag, t->tag))
            return new GeneratorElement (m_doc, tag, t->id);
    return nullptr;
}

