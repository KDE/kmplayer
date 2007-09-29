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

/**
 * '<RSS>' tag
 */
class KMPLAYER_NO_EXPORT Rss : public Mrl {
public:
    KDE_NO_CDTOR_EXPORT Rss (NodePtr & d) : Mrl (d, id_node_rss) {}
    NodePtr childFromTag (const QString & tag);
    KDE_NO_EXPORT const char * nodeName () const { return "rss"; }
    bool expose () const { return false; }
};
    
class KMPLAYER_NO_EXPORT Channel : public Mrl {
public:
    KDE_NO_CDTOR_EXPORT Channel (NodePtr & d) : Mrl (d, id_node_channel) {}
    NodePtr childFromTag (const QString & tag);
    KDE_NO_EXPORT const char * nodeName () const { return "channel"; }
    PlayType playType () { return play_type_none; }
    void closed ();
    bool expose () const;
};

class KMPLAYER_NO_EXPORT Item : public Mrl {
public:
    KDE_NO_CDTOR_EXPORT Item (NodePtr & d) : Mrl (d, id_node_item) {}
    NodePtr childFromTag (const QString & tag);
    KDE_NO_EXPORT const char * nodeName () const { return "item"; }
    PlayType playType () { return cached_play_type; }
    Mrl * linkNode ();
    void closed ();
    void activate ();
    void deactivate ();
    bool handleEvent (EventPtr event);
    NodePtrW enclosure;
    TimerInfoPtrW timer;
};

class KMPLAYER_NO_EXPORT Enclosure : public Mrl {
public:
    KDE_NO_CDTOR_EXPORT Enclosure(NodePtr &d) : Mrl(d, id_node_enclosure) {}
    KDE_NO_EXPORT const char * nodeName () const { return "enclosure"; }
    void closed ();
    bool expose () const { return false; }
};

} //namespace RSS


} // namespace KMPlayer

#endif //_KMPLAYER_RSS_H_
