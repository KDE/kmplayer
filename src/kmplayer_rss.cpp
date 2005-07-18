/**
 * Copyright (C) 2005 by Koos Vriezen <koos ! vriezen ? xs4all ! nl>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License version 2 as published by the Free Software Foundation.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 *  Boston, MA 02111-1307, USA.
 **/

#include <config.h>
#include <kdebug.h>
#include "kmplayer_rss.h"

using namespace KMPlayer;

NodePtr RSS::Rss::childFromTag (const QString & tag) {
    if (!strcmp (tag.latin1 (), "channel"))
        return (new RSS::Channel (m_doc))->self ();
    return NodePtr ();
}

NodePtr RSS::Channel::childFromTag (const QString & tag) {
    if (!strcmp (tag.latin1 (), "item"))
        return (new RSS::Item (m_doc))->self ();
    else if (!strcmp (tag.latin1 (), "title"))
        return (new DarkNode (m_doc, tag, id_node_title))->self ();
    return NodePtr ();
}

void RSS::Channel::closed () {
    for (NodePtr c = firstChild (); c; c = c->nextSibling ()) {
        if (c->id == id_node_title) {
            QString str = c->innerText ();
            pretty_name = str.left (str.find (QChar ('\n')));
        }
    }
}

NodePtr RSS::Item::childFromTag (const QString & tag) {
    if (!strcmp (tag.latin1 (), "enclosure"))
        return (new RSS::Enclosure (m_doc))->self ();
    else if (!strcmp (tag.latin1 (), "title"))
        return (new DarkNode (m_doc, tag, id_node_title))->self ();
    else if (!strcmp (tag.latin1 (), "description"))
        return (new DarkNode (m_doc, tag, id_node_description))->self ();
    return NodePtr ();
}

void RSS::Item::closed () {
    for (NodePtr c = firstChild (); c; c = c->nextSibling ()) {
        if (c->id == id_node_title) {
            QString str = c->innerText ();
            pretty_name = str.left (str.find (QChar ('\n')));
        }
        if (c->isMrl ())
            src = c->mrl ()->src;
    }
}

void RSS::Item::activate () {
    PlayListNotify * n = document()->notify_listener;
    if (n) {
        for (NodePtr c = firstChild (); c; c = c->nextSibling ())
            if (c->id == id_node_description)
                n->setInfoMessage (c->innerText ());
    }
    Mrl::activate ();
}

void RSS::Item::deactivate () {
    PlayListNotify * n = document()->notify_listener;
    if (n)
        n->setInfoMessage (QString::null);
    Mrl::deactivate ();
}

void RSS::Enclosure::closed () {
    src = getAttribute (QString::fromLatin1 ("url"));
}
