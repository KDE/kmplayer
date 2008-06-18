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
#include <kurl.h>

#include "kmplayer_asx.h"

using namespace KMPlayer;

static QString getAsxAttribute (Element * e, const QString & attr) {
    for (AttributePtr a = e->attributes ()->first (); a; a = a->nextSibling ())
        if (attr == a->name ().toString ().lower ())
            return a->value ();
    return QString ();
}

KDE_NO_EXPORT NodePtr ASX::Asx::childFromTag (const QString & tag) {
    const char * name = tag.latin1 ();
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

KDE_NO_EXPORT Node::PlayType ASX::Asx::playType () {
    if (cached_ismrl_version != document ()->m_tree_version)
        for (NodePtr e = firstChild (); e; e = e->nextSibling ()) {
            if (e->id == id_node_title)
                pretty_name = e->innerText ().simplifyWhiteSpace ();
            else if (e->id == id_node_base)
                src = getAsxAttribute (convertNode <Element> (e), "href");
        }
    return Mrl::playType ();
}

//-----------------------------------------------------------------------------

KDE_NO_EXPORT NodePtr ASX::Entry::childFromTag (const QString & tag) {
    const char * name = tag.latin1 ();
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
    if (cached_ismrl_version != document ()->m_tree_version) {
        ref_child_count = 0;
        NodePtr ref;
        for (NodePtr e = firstChild (); e; e = e->nextSibling ()) {
            switch (e->id) {
            case id_node_title:
                pretty_name = e->innerText (); // already normalized (hopefully)
                break;
            case id_node_base:
                src = getAsxAttribute (convertNode <Element> (e), "href");
                break;
            case id_node_ref:
                ref = e;
                ref_child_count++;
            }
        }
        if (ref_child_count == 1 && !pretty_name.isEmpty ())
            convertNode <ASX::Ref> (ref)->pretty_name = pretty_name;
        cached_ismrl_version = document()->m_tree_version;
    }
    return play_type_none;
}

KDE_NO_EXPORT void ASX::Entry::activate () {
    resolved = true;
    for (NodePtr e = firstChild (); e; e = e->nextSibling ())
        if (e->id == id_node_param) {
            Element * elm = convertNode <Element> (e);
            if (getAsxAttribute(elm,"name").lower() == QString("clipsummary")) {
                PlayListNotify * n = document ()->notify_listener;
                if (n)
                    n->setInfoMessage (KURL::decode_string (
                                getAsxAttribute (elm, "value")));
            }
        } else if (e->id == id_node_duration) {
            QString s = convertNode <Element> (e)->getAttribute (
                    StringPool::attr_value);
            int multiply[] = { 1, 60, 60 * 60, 24 * 60 * 60, 0 };
            int mpos = 0;
            double d = 0;
            while (!s.isEmpty () && multiply[mpos]) {
                int p = s.lastIndexOf (QChar (':'));
                QString t = p >= 0 ? s.mid (p + 1) : s;
                d += multiply[mpos++] * t.toDouble();
                s = p >= 0 ? s.left (p) : QString ();
            }
            if (d > 0.00001)
                duration_timer = document()->post (
                        this, new TimerPosting (int (d * 1000)));
        }
    Mrl::activate ();
}

KDE_NO_EXPORT void *ASX::Entry::message (MessageType msg, void *content) {
    if (msg == MsgEventTimer) {
        duration_timer = NULL;
        deactivate ();
        return NULL;
    }
    return Mrl::message (msg, content);
}

KDE_NO_EXPORT void ASX::Entry::deactivate () {
    PlayListNotify * n = document ()->notify_listener;
    if (n)
        n->setInfoMessage (QString ());
    if (duration_timer) {
        document()->cancelPosting (duration_timer);
        duration_timer = NULL;
    }
    Mrl::deactivate ();
}

KDE_NO_EXPORT bool ASX::Entry::expose () const {
    return ref_child_count > 1 && !pretty_name.isEmpty ();
}

//-----------------------------------------------------------------------------

KDE_NO_EXPORT void ASX::Ref::opened () {
    src = getAsxAttribute (this, "href");
    //kdDebug () << "Ref attr found src: " << src << endl;
}

KDE_NO_EXPORT bool ASX::Ref::expose () const {
    return !src.isEmpty ();
}

//-----------------------------------------------------------------------------

KDE_NO_EXPORT void ASX::EntryRef::opened () {
    src = getAsxAttribute (this, "href");
    //kdDebug () << "EntryRef attr found src: " << src << endl;
}

