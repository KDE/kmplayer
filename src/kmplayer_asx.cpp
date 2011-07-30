/**
 * Copyright (C) 2005 by Koos Vriezen <koos.vriezen@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License version 2 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Steet, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 **/

#include "config-kmplayer.h"
#include <kdebug.h>

#include "kmplayer_asx.h"

#include <QUrl>

using namespace KMPlayer;

static QString getAsxAttribute (Element * e, const QString & attr) {
    for (Attribute *a = e->attributes ().first (); a; a = a->nextSibling ())
        if (attr == a->name ().toString ().toLower ())
            return a->value ();
    return QString ();
}

KDE_NO_EXPORT Node *ASX::Asx::childFromTag (const QString & tag) {
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
    return 0L;
}

void *ASX::Asx::role (RoleType msg, void *content)
{
    if (RolePlaylist == msg)
        return !title.isEmpty () ? (PlaylistRole *) this : NULL;
    return Mrl::role (msg, content);
}

KDE_NO_EXPORT void ASX::Asx::closed () {
    for (Node *e = firstChild (); e; e = e->nextSibling ()) {
        if (e->id == id_node_title)
            title = e->innerText ().simplified ();
        else if (e->id == id_node_base)
            src = getAsxAttribute (static_cast <Element *> (e), "href");
    }
}

//-----------------------------------------------------------------------------

KDE_NO_EXPORT Node *ASX::Entry::childFromTag (const QString & tag) {
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
    return 0L;
}

KDE_NO_EXPORT Node::PlayType ASX::Entry::playType () {
    return play_type_none;
}

KDE_NO_EXPORT void ASX::Entry::closed () {
    ref_child_count = 0;
    Node *ref = NULL;
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

KDE_NO_EXPORT void ASX::Entry::activate () {
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

KDE_NO_EXPORT void ASX::Entry::message (MessageType msg, void *content) {
    if (msg == MsgEventTimer) {
        duration_timer = NULL;
        deactivate ();
        return;
    }
    Mrl::message (msg, content);
}

KDE_NO_EXPORT void ASX::Entry::deactivate () {
    document ()->message (MsgInfoString, NULL);
    if (duration_timer) {
        document()->cancelPosting (duration_timer);
        duration_timer = NULL;
    }
    Mrl::deactivate ();
}

void *ASX::Entry::role (RoleType msg, void *content)
{
    if (RolePlaylist == msg)
        return ref_child_count > 1 && !title.isEmpty ()
            ? (PlaylistRole *) this : NULL;
    return Mrl::role (msg, content);
}

//-----------------------------------------------------------------------------

KDE_NO_EXPORT void ASX::Ref::opened () {
    src = getAsxAttribute (this, "href");
    Mrl::opened ();
}

//-----------------------------------------------------------------------------

KDE_NO_EXPORT void ASX::EntryRef::opened () {
    src = getAsxAttribute (this, "href");
    Mrl::opened ();
}

