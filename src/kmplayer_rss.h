/*
    SPDX-FileCopyrightText: 2005-2006 Koos Vriezen <koos.vriezen@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef _KMPLAYER_RSS_H_
#define _KMPLAYER_RSS_H_

#include <QString>

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
