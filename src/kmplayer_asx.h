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

#ifndef _KMPLAYER_ASX_H_
#define _KMPLAYER_ASX_H_

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

namespace ASX {

const short id_node_asx = 400;
const short id_node_entry = 401;
const short id_node_ref = 402;
const short id_node_entryref = 403;
const short id_node_title = 404;
const short id_node_base = 405;

/**
 * '<ASX>' tag
 */
class Asx : public Mrl {
public:
    KDE_NO_CDTOR_EXPORT Asx (NodePtr & d) : Mrl (d, id_node_asx) {}
    NodePtr childFromTag (const QString & tag);
    KDE_NO_EXPORT const char * nodeName () const { return "ASX"; }
    void closed ();
    bool expose () const { return !pretty_name.isEmpty (); }
    /**
     * True if no mrl children
     */
    bool isMrl ();
};

/**
 * Entry tag as found in ASX for playlist item
 */
class Entry : public Mrl {
public:
    KDE_NO_CDTOR_EXPORT Entry (NodePtr & d) : Mrl (d, id_node_entry) {}
    NodePtr childFromTag (const QString & tag);
    KDE_NO_EXPORT const char * nodeName () const { return "Entry"; }
    /**
     * True if has a Ref child
     */
    bool isMrl ();
    /**
     * Returns Ref child if isMrl() return true
     */
    virtual NodePtr realMrl ();
    /**
     * Override for activating Ref child
     */
    void activate ();
    void closed ();
    NodePtrW base;
};

/**
 * Ref tag as found in ASX for URL item in playlist item
 */
class Ref : public Mrl {
public:
    KDE_NO_CDTOR_EXPORT Ref (NodePtr & d) : Mrl (d, id_node_ref) {}
    //NodePtr childFromTag (const QString & tag);
    void opened ();
    KDE_NO_EXPORT const char * nodeName () const { return "Ref"; }
    bool expose () const;
};

/**
 * EntryRef tag as found in ASX for shortcut of Entry plus Ref playlist item
 */
class EntryRef : public Mrl {
public:
    KDE_NO_CDTOR_EXPORT EntryRef (NodePtr & d) : Mrl (d, id_node_entryref) {}
    //NodePtr childFromTag (const QString & tag);
    void opened ();
    KDE_NO_EXPORT const char * nodeName () const { return "EntryRef"; }
};

} //namespace ASX


} // namespace KMPlayer

#endif //_KMPLAYER_ASX_H_
