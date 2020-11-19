/*
    SPDX-FileCopyrightText: 2005 Koos Vriezen <koos.vriezen@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include "config-kmplayer.h"

#include "kmplayercommon_log.h"
#include "kmplayer_rss.h"
#include "kmplayer_atom.h"

using namespace KMPlayer;

Node *RSS::Rss::childFromTag (const QString & tag) {
    if (!strcmp (tag.toLatin1 ().constData (), "channel"))
        return new RSS::Channel (m_doc);
    return nullptr;
}

void *RSS::Rss::role (RoleType msg, void *content)
{
    if (RolePlaylist == msg)
        return nullptr;
    return Element::role (msg, content);
}

Node *RSS::Channel::childFromTag (const QString & tag) {
    QByteArray ba = tag.toLatin1 ();
    const char *ctag = ba.constData ();
    if (!strcmp (ctag, "item"))
        return new RSS::Item (m_doc);
    else if (!strcmp (ctag, "title"))
        return new DarkNode (m_doc, ctag, id_node_title);
    else if (!strncmp (ctag, "itunes", 6) ||
            !strncmp (ctag, "media", 5))
        return new DarkNode (m_doc, ctag, id_node_ignored);
    return nullptr;
}

void RSS::Channel::closed () {
    for (Node *c = firstChild (); c; c = c->nextSibling ())
        if (c->id == id_node_title) {
            title = c->innerText ().simplified ();
            break;
        }
    Element::closed ();
}

void *RSS::Channel::role (RoleType msg, void *content)
{
    if (RolePlaylist == msg)
        return !title.isEmpty () || //return false if no title and only one
            previousSibling () || nextSibling () ? (PlaylistRole *) this : nullptr;
    return Element::role (msg, content);
}

Node *RSS::Item::childFromTag (const QString & tag) {
    QByteArray ba = tag.toLatin1 ();
    const char *ctag = ba.constData ();
    if (!strcmp (ctag, "enclosure"))
        return new RSS::Enclosure (m_doc);
    else if (!strcmp (ctag, "title"))
        return new DarkNode (m_doc, ctag, id_node_title);
    else if (!strcmp (ctag, "description"))
        return new DarkNode (m_doc, ctag, id_node_description);
    else if (!strcmp (ctag, "category"))
        return new DarkNode (m_doc, ctag, id_node_category);
    else if (!strcmp (ctag, "media:group"))
        return new ATOM::MediaGroup (m_doc);
    else if (!strcmp (ctag, "media:thumbnail"))
        return new DarkNode (m_doc, ctag, id_node_thumbnail);
    else if (!strncmp (ctag, "itunes", 6) ||
            !strncmp (ctag, "feedburner", 10) ||
            !strcmp (ctag, "link") ||
            !strcmp (ctag, "pubDate") ||
            !strcmp (ctag, "guid") ||
            !strncmp (ctag, "media", 5))
        return new DarkNode (m_doc, ctag, id_node_ignored);
    return nullptr;
}

void RSS::Item::closed () {
    if (!summary_added) {
        ATOM::MediaGroup *group = nullptr;
        Enclosure *enclosure = nullptr;
        QString description;
        QString thumbnail;
        int width = 0, height = 0;
        for (Node *c = firstChild (); c; c = c->nextSibling ()) {
            switch (c->id) {
                case id_node_title:
                    title = c->innerText ().simplified ();
                    break;
                case id_node_enclosure:
                    enclosure = static_cast <Enclosure *> (c);
                    break;
                case id_node_description:
                    description = c->innerText ();
                    break;
                case ATOM::id_node_media_group:
                    group = static_cast <ATOM::MediaGroup *> (c);
                    break;
                case id_node_thumbnail:
                    thumbnail = static_cast<Element*>(c)->getAttribute(Ids::attr_url);
                    width = static_cast<Element*>(c)->getAttribute(Ids::attr_width).toInt();
                    height = static_cast<Element*>(c)->getAttribute(Ids::attr_height).toInt();
                    break;
            }
        }
        if (group)
            group->addSummary (this, nullptr, title, description, thumbnail, width, height);
        if (enclosure) {
            enclosure->setCaption (title);
            enclosure->description = description;
        }
        summary_added = true;
    }
    Element::closed ();
}

void RSS::Enclosure::activate () {
    document ()->message (MsgInfoString, &description);
    Mrl::activate ();
}

void RSS::Enclosure::deactivate () {
    document ()->message (MsgInfoString, nullptr);
    Mrl::deactivate ();
}

void RSS::Enclosure::closed () {
    src = getAttribute (Ids::attr_url);
    Mrl::closed ();
}
