/* This file is part of the KDE project
 *
 * Copyright (C) 2005-2006 Koos Vriezen <koos.vriezen@gmail.com>
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

#ifndef _KMPLAYER_RSS_H_
#define _KMPLAYER_RSS_H_

#include <qstring.h>

#include "kmplayerplaylist.h"

namespace KMPlayer {

namespace RSS {

const short id_node_rss = 200;
const short id_node_channel = 201;
const short id_node_item = 202;
const short id_node_title = 203;
const short id_node_description = 204;
const short id_node_enclosure = 205;
const short id_node_category = 206;
const short id_node_thumbnail = 207;
const short id_node_ignored = 208;

/**
 * '<RSS>' tag
 */
class Rss : public Element, public PlaylistRole
{
public:
    Rss (NodePtr & d) : Element (d, id_node_rss) {}
    Node *childFromTag (const QString & tag) override;
    const char * nodeName () const override { return "rss"; }
    void *role (RoleType msg, void *content=nullptr) override;
};

class Channel : public Element, public PlaylistRole
{
public:
    Channel (NodePtr & d) : Element (d, id_node_channel) {}
    Node *childFromTag (const QString & tag) override;
    const char * nodeName () const override { return "channel"; }
    void closed () override;
    void *role (RoleType msg, void *content=nullptr) override;
};

class Item : public Element, public PlaylistRole
{
public:
    Item (NodePtr &d)
        : Element (d, id_node_item), summary_added (false) {}
    Node *childFromTag (const QString & tag) override;
    const char * nodeName () const override { return "item"; }
    void closed () override;
    bool summary_added;
};

class Enclosure : public Mrl
{
public:
    Enclosure(NodePtr &d) : Mrl(d, id_node_enclosure) {}
    const char * nodeName () const override { return "enclosure"; }
    void closed () override;
    void activate () override;
    void deactivate () override;
    QString description;
};

} //namespace RSS


} // namespace KMPlayer

#endif //_KMPLAYER_RSS_H_
