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
#include "kmplayer_rss.h"

using namespace KMPlayer;

KDE_NO_EXPORT NodePtr RSS::Rss::childFromTag (const QString & tag) {
    if (!strcmp (tag.latin1 (), "channel"))
        return new RSS::Channel (m_doc);
    return 0L;
}

KDE_NO_EXPORT NodePtr RSS::Channel::childFromTag (const QString & tag) {
    const char *ctag = tag.ascii ();
    if (!strcmp (ctag, "item"))
        return new RSS::Item (m_doc);
    else if (!strcmp (ctag, "title"))
        return new DarkNode (m_doc, ctag, id_node_title);
    return 0L;
}

KDE_NO_EXPORT void RSS::Channel::closed () {
    for (NodePtr c = firstChild (); c; c = c->nextSibling ())
        if (c->id == id_node_title) {
            pretty_name = c->innerText ().simplifyWhiteSpace ();
            break;
        }
}

KDE_NO_EXPORT bool RSS::Channel::expose () const {
    return !pretty_name.isEmpty () || //return false if no title and only one
        previousSibling () || nextSibling ();
}

KDE_NO_EXPORT NodePtr RSS::Item::childFromTag (const QString & tag) {
    const char *ctag = tag.ascii ();
    if (!strcmp (ctag, "enclosure"))
        return new RSS::Enclosure (m_doc);
    else if (!strcmp (ctag, "title"))
        return new DarkNode (m_doc, ctag, id_node_title);
    else if (!strcmp (ctag, "description"))
        return new DarkNode (m_doc, ctag, id_node_description);
    return 0L;
}

KDE_NO_EXPORT void RSS::Item::closed () {
    cached_play_type = play_type_none;
    for (NodePtr c = firstChild (); c; c = c->nextSibling ()) {
        switch (c->id) {
            case id_node_title:
                pretty_name = c->innerText ().simplifyWhiteSpace ();
                break;
            case id_node_enclosure:
                enclosure = c;
                src = c->mrl ()->src;
                break;
            case id_node_description:
                cached_play_type = play_type_info;
                break;
        }
    }
    if (enclosure && !enclosure->mrl ()->src.isEmpty ())
        cached_play_type = play_type_audio;
}

KDE_NO_EXPORT Mrl * RSS::Item::linkNode () {
    if (enclosure)
        return enclosure->mrl ();
    return Mrl::linkNode ();
}

KDE_NO_EXPORT void RSS::Item::activate () {
    PlayListNotify * n = document()->notify_listener;
    if (n) {
        for (NodePtr c = firstChild (); c; c = c->nextSibling ())
            if (c->id == id_node_description) {
                QString s = c->innerText ();
                n->setInfoMessage (s);
                if (!enclosure && !s.isEmpty ()) {
                    setState (state_activated);
                    begin ();
                    timer = document ()->setTimeout (this, 5000+s.length()*200);
                    return;
                }
                break;
            }
    }
    Mrl::activate ();
}

KDE_NO_EXPORT void RSS::Item::deactivate () {
    if (timer) {
        document ()->cancelTimer (timer);
        timer = 0L;
    }
    PlayListNotify * n = document()->notify_listener;
    if (n)
        n->setInfoMessage (QString ());
    Mrl::deactivate ();
}

KDE_NO_EXPORT bool RSS::Item::handleEvent (EventPtr event) {
    if (event->id () == event_timer) {
        timer = 0L;
        finish ();
    }
    return true;
}

KDE_NO_EXPORT void RSS::Enclosure::closed () {
    src = getAttribute (StringPool::attr_url);
}
