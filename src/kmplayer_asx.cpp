/**
 * Copyright (C) 2005 by Koos Vriezen <koos ! vriezen ? xs4all ! nl>
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

#include <config.h>
#include <kdebug.h>

#include "kmplayer_asx.h"

using namespace KMPlayer;


KDE_NO_EXPORT NodePtr ASX::Asx::childFromTag (const QString & tag) {
    const char * name = tag.latin1 ();
    if (!strcasecmp (name, "entry"))
        return new ASX::Entry (m_doc);
    else if (!strcasecmp (name, "entryref"))
        return new ASX::EntryRef (m_doc);
    else if (!strcasecmp (name, "title"))
        return new DarkNode (m_doc, name, id_node_title);
    else if (!strcasecmp (name, "base"))
        return new DarkNode (m_doc, name, id_node_base);
    return 0L;
}

KDE_NO_EXPORT bool ASX::Asx::isMrl () {
    if (cached_ismrl_version != document ()->m_tree_version)
        for (NodePtr e = firstChild (); e; e = e->nextSibling ()) {
            if (e->id == id_node_title)
                pretty_name = e->innerText ().simplifyWhiteSpace ();
            else if (e->id == id_node_base)
                src = convertNode <Element> (e)->getAttribute ("href");
        }
    return Mrl::isMrl ();
}

KDE_NO_EXPORT void ASX::Asx::closed () {
}

//-----------------------------------------------------------------------------

KDE_NO_EXPORT NodePtr ASX::Entry::childFromTag (const QString & tag) {
    const char * name = tag.latin1 ();
    if (!strcasecmp (name, "ref"))
        return new ASX::Ref (m_doc);
    else if (!strcasecmp (name, "title"))
        return new DarkNode (m_doc, name, id_node_title);
    else if (!strcasecmp (name, "base")) {
        NodePtr bn = new DarkNode (m_doc, name, id_node_base);
        base = bn;
        return bn;
    }
    return 0L;
}

KDE_NO_EXPORT bool ASX::Entry::isMrl () {
    if (cached_ismrl_version != document ()->m_tree_version) {
        QString pn;
        bool foundone = false;
        if (base)
            src = convertNode <Element> (base)->getAttribute ("href");
        for (NodePtr e = firstChild (); e; e = e->nextSibling ()) {
            if (e->isMrl () && !e->hasChildNodes ()) {
                if (foundone) {
                    src.truncate (0);
                    pn.truncate (0);
                } else {
                    src = e->mrl ()->src;
                    pn = e->mrl ()->pretty_name;
                }
                foundone = true;
            } else if (e->id == id_node_title)
                pretty_name = e->innerText (); // already normalized (hopefully)
        }
        if (pretty_name.isEmpty ())
            pretty_name = pn;
        cached_ismrl_version = document()->m_tree_version;
    }
    return !src.isEmpty ();
}

KDE_NO_EXPORT NodePtr ASX::Entry::realMrl () {
    for (NodePtr e = firstChild (); e; e = e->nextSibling ())
        if (e->isMrl ())
            return e;
    return this;
}

KDE_NO_EXPORT void ASX::Entry::activate () {
    if (!isMrl ()) { // eg. multible child Ref's
        Element::activate ();
    } else {
        NodePtr mrl = realMrl ();
        if (mrl != m_self)
            mrl->setState (state_activated);
        Mrl::activate ();
    }
}

KDE_NO_EXPORT void ASX::Entry::closed () {
    if (base)
        src = convertNode <Element> (base)->getAttribute ("href");
}

//-----------------------------------------------------------------------------

KDE_NO_EXPORT void ASX::Ref::opened () {
    for (AttributePtr a = m_attributes->first (); a; a = a->nextSibling ()) {
        if (!strcasecmp (a->nodeName (), "href"))
            src = a->nodeValue ();
        else
            kdError () << "Warning: unhandled Ref attr: " << a->nodeName () << "=" << a->nodeValue () << endl;

    }
    //kdDebug () << "Ref attr found src: " << src << endl;
}

KDE_NO_EXPORT bool ASX::Ref::expose () const {
    return parentNode () && !parentNode ()->isMrl (); // eg. having Ref sisters
}

//-----------------------------------------------------------------------------

KDE_NO_EXPORT void ASX::EntryRef::opened () {
    for (AttributePtr a = m_attributes->first (); a; a = a->nextSibling ()) {
        if (!strcasecmp (a->nodeName (), "href"))
            src = a->nodeValue ();
        else
            kdError () << "unhandled EntryRef attr: " << a->nodeName () << "=" << a->nodeValue () << endl;
    }
    //kdDebug () << "EntryRef attr found src: " << src << endl;
}

