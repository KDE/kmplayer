/*
    SPDX-FileCopyrightText: 2005 Koos Vriezen <koos.vriezen@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include "config-kmplayer.h"

#include "kmplayercommon_log.h"
#include "kmplayer_asx.h"

#include <QUrl>

using namespace KMPlayer;

static QString getAsxAttribute (Element * e, const QString & attr) {
    for (Attribute *a = e->attributes ().first (); a; a = a->nextSibling ())
        if (attr == a->name ().toString ().toLower ())
            return a->value ();
    return QString ();
}

Node *ASX::Asx::childFromTag (const QString & tag) {
    QByteArray ba = tag.toLatin1 ();
    const char *name = ba.constData ();
    if (!strcasecmp (name, "entry"))
        return new ASX::Entry (m_doc);
    else if (!strcasecmp (name, "entryref"))
        return new ASX::EntryRef (m_doc);
    else if (!strcasecmp (name, "title"))
        return new DarkNode (m_doc, name, id_node_title);
    else if (!strcasecmp (name, "base"))
        return new DarkNode (m_doc, name, id_node_base);
    else if (!strcasecmp (name, "param"))
        return new DarkNode (m_doc, name, id_node_param);
    return nullptr;
}

void *ASX::Asx::role (RoleType msg, void *content)
{
    if (RolePlaylist == msg)
        return !title.isEmpty () ? (PlaylistRole *) this : nullptr;
    return Mrl::role (msg, content);
}

void ASX::Asx::closed () {
    for (Node *e = firstChild (); e; e = e->nextSibling ()) {
        if (e->id == id_node_title)
            title = e->innerText ().simplified ();
        else if (e->id == id_node_base)
            src = getAsxAttribute (static_cast <Element *> (e), "href");
    }
}

//-----------------------------------------------------------------------------

Node *ASX::Entry::childFromTag (const QString & tag) {
    QByteArray ba = tag.toLatin1 ();
    const char *name = ba.constData ();
    if (!strcasecmp (name, "ref"))
        return new ASX::Ref (m_doc);
    else if (!strcasecmp (name, "title"))
        return new DarkNode (m_doc, name, id_node_title);
    else if (!strcasecmp (name, "base"))
        return new DarkNode (m_doc, name, id_node_base);
    else if (!strcasecmp (name, "param"))
        return new DarkNode (m_doc, name, id_node_param);
    else if (!strcasecmp (name, "starttime"))
        return new DarkNode (m_doc, name, id_node_starttime);
    else if (!strcasecmp (name, "duration"))
        return new DarkNode (m_doc, name, id_node_duration);
    return nullptr;
}

Node::PlayType ASX::Entry::playType () {
    return play_type_none;
}

void ASX::Entry::closed () {
    ref_child_count = 0;
    Node *ref = nullptr;
    for (Node *e = firstChild (); e; e = e->nextSibling ()) {
        switch (e->id) {
        case id_node_title:
            title = e->innerText (); // already normalized (hopefully)
            break;
        case id_node_base:
            src = getAsxAttribute (static_cast <Element *> (e), "href");
            break;
        case id_node_ref:
            ref = e;
            ref_child_count++;
        }
    }
    if (ref_child_count == 1 && !title.isEmpty ())
        static_cast <ASX::Ref *> (ref)->title = title;
}

void ASX::Entry::activate () {
    resolved = true;
    for (Node *e = firstChild (); e; e = e->nextSibling ())
        if (e->id == id_node_param) {
            Element * elm = static_cast <Element *> (e);
            if (getAsxAttribute(elm,"name").toLower() == QString("clipsummary")) {
                QString inf = QUrl::fromPercentEncoding (
                                getAsxAttribute (elm, "value").toUtf8 ());
                document ()->message (MsgInfoString, &inf);
            }
        } else if (e->id == id_node_duration) {
            QString s = static_cast <Element *> (e)->getAttribute (
                    Ids::attr_value);
            int pos = parseTimeString( s );
            if (pos > 0)
                duration_timer = document()->post (
                        this, new TimerPosting (pos * 10));
        }
    Mrl::activate ();
}

void ASX::Entry::message (MessageType msg, void *content) {
    if (msg == MsgEventTimer) {
        duration_timer = nullptr;
        deactivate ();
        return;
    }
    Mrl::message (msg, content);
}

void ASX::Entry::deactivate () {
    document ()->message (MsgInfoString, nullptr);
    if (duration_timer) {
        document()->cancelPosting (duration_timer);
        duration_timer = nullptr;
    }
    Mrl::deactivate ();
}

void *ASX::Entry::role (RoleType msg, void *content)
{
    if (RolePlaylist == msg)
        return ref_child_count > 1 && !title.isEmpty ()
            ? (PlaylistRole *) this : nullptr;
    return Mrl::role (msg, content);
}

//-----------------------------------------------------------------------------

void ASX::Ref::opened () {
    src = getAsxAttribute (this, "href");
    Mrl::opened ();
}

//-----------------------------------------------------------------------------

void ASX::EntryRef::opened () {
    src = getAsxAttribute (this, "href");
    Mrl::opened ();
}

