/*
    SPDX-FileCopyrightText: 2012 Koos Vriezen <koos.vriezen@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
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

class Opml : public Element, public PlaylistRole
{
public:
    Opml (NodePtr& d) : Element (d, id_node_opml) {}
    Node *childFromTag (const QString& tag) override;
    const char *nodeName () const override { return "opml"; }
    void *role (RoleType msg, void *content=nullptr) override;
    void closed () override;
};

class Head : public Element
{
public:
    Head (NodePtr& d) : Element (d, id_node_head) {}
    Node *childFromTag (const QString& tag) override;
    const char *nodeName () const override { return "head"; }
};

class Body : public Element
{
public:
    Body (NodePtr& d) : Element (d, id_node_body) {}
    const char *nodeName () const override { return "body"; }
    Node *childFromTag (const QString& tag) override;
};

class Outline : public Mrl
{
public:
    Outline (NodePtr& d) : Mrl (d, id_node_outline) {}
    const char *nodeName () const override { return "outline"; }
    void closed () override;
};

} //namespace OPML


} // namespace KMPlayer

#endif //_KMPLAYER_OPML_H_
