/*
    SPDX-FileCopyrightText: 2005-2006 Koos Vriezen <koos.vriezen@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef _KMPLAYER_ATOM_H_
#define _KMPLAYER_ATOM_H_

#include <qstringlist.h>

#include "kmplayerplaylist.h"

namespace KMPlayer {

namespace ATOM {

const short id_node_feed = 300;
const short id_node_entry = 301;
const short id_node_link = 302;
const short id_node_title = 303;
const short id_node_summary = 304;
const short id_node_content = 305;
const short id_node_media_group = 306;
const short id_node_media_content = 307;
const short id_node_media_title = 308;
const short id_node_media_description = 309;
const short id_node_media_player = 310;
const short id_node_media_thumbnail = 311;
const short id_node_gd_rating = 312;
const short id_node_ignored = 313;

/**
 * '<feed>' tag
 */
class Feed : public Element, public PlaylistRole
{
public:
    Feed (NodePtr & d) : Element (d, id_node_feed) {}
    Node *childFromTag (const QString & tag) override;
    const char * nodeName () const override { return "feed"; }
    void closed () override;
    void *role (RoleType msg, void *content=nullptr) override;
};

class Entry : public Element, public PlaylistRole
{
public:
    Entry (NodePtr & d) : Element (d, id_node_entry) {}
    Node *childFromTag (const QString & tag) override;
    const char * nodeName () const override { return "entry"; }
    void closed () override;
    void *role (RoleType msg, void *content=nullptr) override;
};

class Link : public Mrl
{
public:
    Link (NodePtr & d) : Mrl (d, id_node_link) {}
    const char * nodeName () const override { return "link"; }
    PlayType playType () override;
    void closed () override;
};

class Content : public Mrl
{
public:
    Content (NodePtr &d) : Mrl(d, id_node_content) {}
    const char * nodeName () const override { return "content"; }
    PlayType playType () override;
    void closed () override;
};

class MediaGroup : public Element
{
public:
    MediaGroup (NodePtr &d) : Element (d, id_node_media_group) {}
    Node *childFromTag (const QString &tag) override;
    void message (MessageType msg, void *content=nullptr) override;
    const char *nodeName () const override { return "media:group"; }
    void addSummary (Node *parent, Node *ratings, const QString& alt_title, const QString& alt_desc,
                     const QString& alt_img, int width, int height);
};

class MediaContent : public Mrl
{
public:
    MediaContent (NodePtr &d) : Mrl (d, id_node_media_content) {}
    const char *nodeName () const override { return "media:content"; }
    void closed () override;
};

} //namespace ATOM


} // namespace KMPlayer

#endif //_KMPLAYER_ATOM_H_
