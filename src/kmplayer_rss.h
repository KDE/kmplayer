/* This file is part of the KDE project
 *
 * Copyright (C) 2005 Koos Vriezen <koos.vriezen@xs4all.nl>
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

#include <qobject.h>
#include <qstring.h>
#include <qstringlist.h>

#include "kmplayerplaylist.h"

class QPixmap;
class QPainter;

namespace KIO {
    class Job;
}

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
class Rss : public Mrl {
public:
    KDE_NO_CDTOR_EXPORT Rss (NodePtr & d) : Mrl (d, id_node_rss) {}
    NodePtr childFromTag (const QString & tag);
    KDE_NO_EXPORT const char * nodeName () const { return "rss"; }
};
    
class Channel : public Mrl {
public:
    KDE_NO_CDTOR_EXPORT Channel (NodePtr & d) : Mrl (d, id_node_channel) {}
    NodePtr childFromTag (const QString & tag);
    KDE_NO_EXPORT const char * nodeName () const { return "channel"; }
    bool isMrl () { return false; }
    void closed ();
};

class Item : public Mrl {
public:
    KDE_NO_CDTOR_EXPORT Item (NodePtr & d) : Mrl (d, id_node_item) {}
    NodePtr childFromTag (const QString & tag);
    KDE_NO_EXPORT const char * nodeName () const { return "item"; }
    bool isMrl () { return !src.isEmpty (); }
    void closed ();
    void activate ();
    void deactivate ();
};

class Enclosure : public Mrl {
public:
    KDE_NO_CDTOR_EXPORT Enclosure(NodePtr &d) : Mrl(d, id_node_enclosure) {}
    KDE_NO_EXPORT const char * nodeName () const { return "enclosure"; }
    void closed ();
    bool expose () const { return false; }
};

} //namespace RSS


} // namespace KMPlayer

#endif //_KMPLAYER_RSS_H_
