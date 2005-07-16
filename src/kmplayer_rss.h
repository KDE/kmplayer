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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef _KMPLAYER_RSS_H_
#define _KMPLAYER_RSS_H_

#include <qobject.h>
#include <qstring.h>
#include <qstringlist.h>

#include "kmplayerplaylist.h"

class QTextStream;
class QPixmap;
class QPainter;

namespace KIO {
    class Job;
}

namespace KMPlayer {

namespace RSS {

class Channel : public Element {
public:
    KDE_NO_CDTOR_EXPORT Channel (NodePtr & d) : Element (d) {}
    NodePtr childFromTag (const QString & tag);
    KDE_NO_EXPORT const char * nodeName () const { return "channel"; }
};

class Item : public Mrl {
public:
    KDE_NO_CDTOR_EXPORT Item (NodePtr & d) : Mrl (d) {}
    NodePtr childFromTag (const QString & tag);
    KDE_NO_EXPORT const char * nodeName () const { return "item"; }
    bool isMrl () { return !src.isEmpty (); }
    void closed ();
};

class Enclosure : public Mrl {
public:
    KDE_NO_CDTOR_EXPORT Enclosure (NodePtr & d) : Mrl (d) {}
    KDE_NO_EXPORT const char * nodeName () const { return "enclosure"; }
    void closed ();
    bool expose () const { return false; }
};

} //namespace RSS


} // namespace KMPlayer

#endif //_KMPLAYER_RSS_H_
