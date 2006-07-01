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

/**
 * '<feed>' tag
 */
class Feed : public Mrl {
public:
    KDE_NO_CDTOR_EXPORT Feed (NodePtr & d) : Mrl (d, id_node_feed) {}
    NodePtr childFromTag (const QString & tag);
    KDE_NO_EXPORT const char * nodeName () const { return "feed"; }
    void closed ();
    bool expose () const { return !pretty_name.isEmpty (); }
};

class KMPLAYER_NO_EXPORT Entry : public Mrl {
public:
    KDE_NO_CDTOR_EXPORT Entry (NodePtr & d) : Mrl (d, id_node_entry) {}
    NodePtr childFromTag (const QString & tag);
    KDE_NO_EXPORT const char * nodeName () const { return "entry"; }
    bool isPlayable () { return false; }
    void closed ();
};

class KMPLAYER_NO_EXPORT Link : public Mrl {
public:
    KDE_NO_CDTOR_EXPORT Link (NodePtr & d) : Mrl (d, id_node_link) {}
    KDE_NO_EXPORT const char * nodeName () const { return "link"; }
    bool isPlayable () { return !src.isEmpty (); }
    void closed ();
};

class KMPLAYER_NO_EXPORT Content : public Mrl {
public:
    KDE_NO_CDTOR_EXPORT Content (NodePtr &d) : Mrl(d, id_node_content) {}
    KDE_NO_EXPORT const char * nodeName () const { return "content"; }
    bool isPlayable ();
    void closed ();
    //bool expose () const { return isPlayable (); }
};

} //namespace ATOM


} // namespace KMPlayer

#endif //_KMPLAYER_ATOM_H_
