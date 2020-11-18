/* This file is part of the KDE project
 *
 * Copyright (C) 2012 Koos Vriezen <koos.vriezen@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
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
 */

#ifndef _KMPLAYER_OPML_H_
#define _KMPLAYER_OPML_H_

#include <qstring.h>

#include "kmplayerplaylist.h"

namespace KMPlayer {

namespace OPML {

const short id_node_opml = 550;
const short id_node_head = 551;
const short id_node_title = 552;
const short id_node_body = 553;
const short id_node_outline = 554;
const short id_node_ignore = 555;

class KMPLAYER_NO_EXPORT Opml : public Element, public PlaylistRole {
public:
    Opml (NodePtr& d) : Element (d, id_node_opml) {}
    Node *childFromTag (const QString& tag);
    const char *nodeName () const { return "opml"; }
    void *role (RoleType msg, void *content=nullptr);
    void closed ();
};

class KMPLAYER_NO_EXPORT Head : public Element {
public:
    Head (NodePtr& d) : Element (d, id_node_head) {}
    Node *childFromTag (const QString& tag);
    const char *nodeName () const { return "head"; }
};

class KMPLAYER_NO_EXPORT Body : public Element {
public:
    Body (NodePtr& d) : Element (d, id_node_body) {}
    const char *nodeName () const { return "body"; }
    Node *childFromTag (const QString& tag);
};

class KMPLAYER_NO_EXPORT Outline : public Mrl {
public:
    Outline (NodePtr& d) : Mrl (d, id_node_outline) {}
    const char *nodeName () const { return "outline"; }
    void closed ();
};

} //namespace OPML


} // namespace KMPlayer

#endif //_KMPLAYER_OPML_H_
