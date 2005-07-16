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
#include <qtextstream.h>
#include "kmplayer_rss.h"

using namespace KMPlayer;

static short node_rss_title = 200;

NodePtr RSS::Rss::childFromTag (const QString & tag) {
    if (!strcmp (tag.latin1 (), "channel"))
        return (new RSS::Channel (m_doc))->self ();
    return NodePtr ();
}

bool RSS::Rss::isMrl() {
    return hasChildNodes ();
}

KDE_NO_CDTOR_EXPORT RSS::Title::Title (NodePtr & d) : Element (d) {
    id = node_rss_title;
}

NodePtr RSS::Channel::childFromTag (const QString & tag) {
    if (!strcmp (tag.latin1 (), "item"))
        return (new RSS::Item (m_doc))->self ();
    else if (!strcmp (tag.latin1 (), "title"))
        return (new RSS::Title (m_doc))->self ();
    return NodePtr ();
}

void RSS::Channel::closed () {
    for (NodePtr c = firstChild (); c; c = c->nextSibling ()) {
        if (c->id == node_rss_title) {
            QString str = c->innerText ();
            pretty_name = str.left (str.find (QChar ('\n')));
        }
    }
}

NodePtr RSS::Item::childFromTag (const QString & tag) {
    if (!strcmp (tag.latin1 (), "enclosure"))
        return (new RSS::Enclosure (m_doc))->self ();
    else if (!strcmp (tag.latin1 (), "title"))
        return (new RSS::Title (m_doc))->self ();
    return NodePtr ();
}

void RSS::Item::closed () {
    for (NodePtr c = firstChild (); c; c = c->nextSibling ()) {
        if (c->id == node_rss_title) {
            QString str = c->innerText ();
            pretty_name = str.left (str.find (QChar ('\n')));
        }
        if (c->isMrl ())
            src = c->mrl ()->src;
    }
}

void RSS::Enclosure::closed () {
    src = getAttribute (QString::fromLatin1 ("url"));
}
