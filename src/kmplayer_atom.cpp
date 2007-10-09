/**
 * Copyright (C) 2005-2006 by Koos Vriezen <koos.vriezen@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License version 2 as published by the Free Software Foundation.
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
 **/

#include "config-kmplayer.h"
#include <kdebug.h>
#include "kmplayer_atom.h"

using namespace KMPlayer;

NodePtr ATOM::Feed::childFromTag (const QString & tag) {
    if (!strcmp (tag.latin1 (), "entry"))
        return new ATOM::Entry (m_doc);
    else if (!strcmp (tag.latin1 (), "link"))
        return new ATOM::Link (m_doc);
    else if (!strcmp (tag.latin1 (), "title"))
        return new DarkNode (m_doc, tag, id_node_title);
    return 0L;
}

void ATOM::Feed::closed () {
    for (NodePtr c = firstChild (); c; c = c->nextSibling ())
        if (c->id == id_node_title) {
            pretty_name = c->innerText ().simplifyWhiteSpace ();
            break;
        }
}

NodePtr ATOM::Entry::childFromTag (const QString & tag) {
    if (!strcmp (tag.latin1 (), "link"))
        return new ATOM::Link (m_doc);
    else if (!strcmp (tag.latin1 (), "content"))
        return new ATOM::Content (m_doc);
    else if (!strcmp (tag.latin1 (), "title"))
        return new DarkNode (m_doc, tag, id_node_title);
    else if (!strcmp (tag.latin1 (), "summary"))
        return new DarkNode (m_doc, tag, id_node_summary);
    return 0L;
}

void ATOM::Entry::closed () {
    for (NodePtr c = firstChild (); c; c = c->nextSibling ())
        if (c->id == id_node_title) {
            pretty_name = c->innerText ().simplifyWhiteSpace ();
            break;
        }
}

Node::PlayType ATOM::Link::playType () {
    return src.isEmpty () ? play_type_none : play_type_unknown;
}

void ATOM::Link::closed () {
    QString href;
    QString rel;
    for (AttributePtr a = attributes ()->first (); a; a = a->nextSibling ()) {
        if (a->name () == StringPool::attr_href)
            href = a->value ();
        else if (a->name () == StringPool::attr_title)
            pretty_name = a->value ();
        else if (a->name () == "rel")
            rel = a->value ();
    }
    if (!href.isEmpty () && rel == QString::fromLatin1 ("enclosure"))
        src = href;
    else if (pretty_name.isEmpty ())
        pretty_name = href;
}

void ATOM::Content::closed () {
    for (AttributePtr a = attributes ()->first (); a; a = a->nextSibling ()) {
        if (a->name () == StringPool::attr_src)
            src = a->value ();
        else if (a->name () == StringPool::attr_type) {
            QString v = a->value ().lower ();
            if (v == QString::fromLatin1 ("text"))
                mimetype = QString::fromLatin1 ("text/plain");
            else if (v == QString::fromLatin1 ("html"))
                mimetype = QString::fromLatin1 ("text/html");
            else if (v == QString::fromLatin1 ("xhtml"))
                mimetype = QString::fromLatin1 ("application/xhtml+xml");
            else
                mimetype = v;
        }
    }
}

Node::PlayType ATOM::Content::playType () {
    if (!hasChildNodes () && !src.isEmpty ())
        return play_type_unknown;
    return play_type_none;
}

