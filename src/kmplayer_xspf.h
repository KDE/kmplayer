/*
    SPDX-FileCopyrightText: 2006 Koos Vriezen <koos.vriezen@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef _KMPLAYER_XSPF_H_
#define _KMPLAYER_XSPF_H_

#include <QString>

#include "kmplayerplaylist.h"

namespace KMPlayer {

namespace XSPF {

const short id_node_playlist = 500;
const short id_node_title = 501;
const short id_node_creator = 502;
const short id_node_annotation = 503;
const short id_node_info = 504;
const short id_node_location = 505;
const short id_node_identifier = 506;
const short id_node_image = 507;
const short id_node_date = 508;
const short id_node_license = 509;
const short id_node_attribution = 510;
const short id_node_meta = 511;
const short id_node_extension = 512;
const short id_node_tracklist = 513;
const short id_node_track = 514;
const short id_node_album = 515;
const short id_node_tracknum = 516;
const short id_node_duration = 517;
const short id_node_link = 518;

class Playlist : public Mrl
{
public:
    Playlist (NodePtr & d) : Mrl (d, id_node_playlist) {}
    Node *childFromTag (const QString & tag) override;
    const char * nodeName () const override { return "playlist"; }
    void *role (RoleType msg, void *content=nullptr) override;
    void closed () override;
};

class Tracklist : public Element
{
public:
    Tracklist (NodePtr & d) : Element (d, id_node_tracklist) {}
    Node *childFromTag (const QString & tag) override;
    const char * nodeName () const override { return "tracklist"; }
};

class Track : public Element
{
public:
    Track (NodePtr & d) : Element (d, id_node_track) {}
    void closed () override;
    void activate () override;
    const char * nodeName () const override { return "track"; }
    Node *childFromTag (const QString & tag) override;
};

class Location : public Mrl
{
public:
    Location (NodePtr &d) : Mrl (d, id_node_location) {}
    const char * nodeName () const override { return "location"; }
    void closed () override;
};

} //namespace XSPF


} // namespace KMPlayer

#endif //_KMPLAYER_XSPF_H_
