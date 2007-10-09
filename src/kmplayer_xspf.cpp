/**
 * Copyright (C) 2006 by Koos Vriezen <koos.vriezen@gmail.com>
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

#include "kmplayer_xspf.h"

using namespace KMPlayer;


KDE_NO_EXPORT NodePtr XSPF::Playlist::childFromTag (const QString & tag) {
    const char * name = tag.latin1 ();
    if (!strcasecmp (name, "tracklist"))
        return new Tracklist (m_doc);
    else if (!strcasecmp (name, "creator"))
        return new DarkNode (m_doc, name, id_node_creator);
    else if (!strcasecmp (name, "title"))
        return new DarkNode (m_doc, name, id_node_title);
    else if (!strcasecmp (name, "annotation"))
        return new DarkNode (m_doc, name, id_node_annotation);
    else if (!strcasecmp (name, "info"))
        return new DarkNode (m_doc, name, id_node_info);
    else if (!strcasecmp (name, "location"))
        return new DarkNode (m_doc, name, id_node_location);
    else if (!strcasecmp (name, "identifier"))
        return new DarkNode (m_doc, name, id_node_identifier);
    else if (!strcasecmp (name, "image"))
        return new DarkNode (m_doc, name, id_node_image);
    else if (!strcasecmp (name, "date"))
        return new DarkNode (m_doc, name, id_node_date);
    else if (!strcasecmp (name, "license"))
        return new DarkNode (m_doc, name, id_node_license);
    else if (!strcasecmp (name, "attribution"))
        return new DarkNode (m_doc, name, id_node_attribution);
    else if (!strcasecmp (name, "link"))
        return new DarkNode (m_doc, name, id_node_link);
    else if (!strcasecmp (name, "meta"))
        return new DarkNode (m_doc, name, id_node_meta);
    else if (!strcasecmp (name, "extension"))
        return new DarkNode (m_doc, name, id_node_extension);
    return 0L;
}

KDE_NO_EXPORT void XSPF::Playlist::closed () {
    for (NodePtr e = firstChild (); e; e = e->nextSibling ()) {
        if (e->id == id_node_title)
            pretty_name = e->innerText ().simplifyWhiteSpace ();
        else if (e->id == id_node_location)
            src = e->innerText ().stripWhiteSpace ();
    }
}

//-----------------------------------------------------------------------------

KDE_NO_EXPORT NodePtr XSPF::Tracklist::childFromTag (const QString & tag) {
    const char * name = tag.latin1 ();
    if (!strcasecmp (name, "track"))
        return new XSPF::Track (m_doc);
    return 0L;
}

//-----------------------------------------------------------------------------

KDE_NO_EXPORT NodePtr XSPF::Track::childFromTag (const QString & tag) {
    const char * name = tag.latin1 ();
    if (!strcasecmp (name, "location"))
        return new Location (m_doc);
    else if (!strcasecmp (name, "creator"))
        return new DarkNode (m_doc, name, id_node_creator);
    else if (!strcasecmp (name, "title"))
        return new DarkNode (m_doc, name, id_node_title);
    else if (!strcasecmp (name, "annotation"))
        return new DarkNode (m_doc, name, id_node_annotation);
    else if (!strcasecmp (name, "info"))
        return new DarkNode (m_doc, name, id_node_info);
    else if (!strcasecmp (name, "identifier"))
        return new DarkNode (m_doc, name, id_node_identifier);
    else if (!strcasecmp (name, "album"))
        return new DarkNode (m_doc, name, id_node_album);
    else if (!strcasecmp (name, "image"))
        return new DarkNode (m_doc, name, id_node_image);
    else if (!strcasecmp (name, "trackNum"))
        return new DarkNode (m_doc, name, id_node_tracknum);
    else if (!strcasecmp (name, "duration"))
        return new DarkNode (m_doc, name, id_node_duration);
    else if (!strcasecmp (name, "link"))
        return new DarkNode (m_doc, name, id_node_link);
    else if (!strcasecmp (name, "meta"))
        return new DarkNode (m_doc, name, id_node_meta);
    else if (!strcasecmp (name, "extension"))
        return new DarkNode (m_doc, name, id_node_extension);
    return 0L;
}

KDE_NO_EXPORT void XSPF::Track::closed () {
    for (NodePtr e = firstChild (); e; e = e->nextSibling ()) {
        switch (e->id) {
            case id_node_title:
                pretty_name = e->innerText ();
                break;
            case id_node_location:
                location = e;
                src = e->mrl ()->src;
                break;
        }
    }
}

KDE_NO_EXPORT void XSPF::Track::activate () {
    for (NodePtr e = firstChild (); e; e = e->nextSibling ())
        if (e->id == id_node_annotation) {
            PlayListNotify * n = document ()->notify_listener;
            if (n)
                n->setInfoMessage (e->innerText ().stripWhiteSpace ());
            break;
        }
    Mrl::activate ();
}

KDE_NO_EXPORT Node::PlayType XSPF::Track::playType () {
    if (location)
        return location->playType ();
    return Mrl::playType ();
}

KDE_NO_EXPORT Mrl * XSPF::Track::linkNode () {
    if (location)
        return location->mrl ();
    return Mrl::linkNode ();
}

void XSPF::Location::closed () {
    src = innerText ().stripWhiteSpace ();
}
