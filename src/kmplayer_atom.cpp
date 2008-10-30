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
#include "kmplayer_smil.h"

using namespace KMPlayer;

NodePtr ATOM::Feed::childFromTag (const QString & tag) {
    if (!strcmp (tag.latin1 (), "entry"))
        return new ATOM::Entry (m_doc);
    else if (!strcmp (tag.latin1 (), "link"))
        return new ATOM::Link (m_doc);
    else if (!strcmp (tag.latin1 (), "title"))
        return new DarkNode (m_doc, tag.toUtf8 (), id_node_title);
    return NULL;
}

void ATOM::Feed::closed () {
    for (NodePtr c = firstChild (); c; c = c->nextSibling ())
        if (c->id == id_node_title) {
            pretty_name = c->innerText ().simplifyWhiteSpace ();
            break;
        }
    Mrl::closed ();
}

NodePtr ATOM::Entry::childFromTag (const QString &tag) {
    const char *cstr = tag.latin1 ();
    if (!strcmp (cstr, "link"))
        return new ATOM::Link (m_doc);
    else if (!strcmp (cstr, "content"))
        return new ATOM::Content (m_doc);
    else if (!strcmp (cstr, "title"))
        return new DarkNode (m_doc, tag.toUtf8 (), id_node_title);
    else if (!strcmp (cstr, "summary"))
        return new DarkNode (m_doc, tag.toUtf8 (), id_node_summary);
    else if (!strcmp (cstr, "media:group"))
        return new MediaGroup (m_doc);
    else if (!strcmp (cstr, "category") ||
            !strcmp (cstr, "author:") ||
            !strcmp (cstr, "id") ||
            !strcmp (cstr, "updated") ||
            !strncmp (cstr, "yt:", 3) ||
            !strncmp (cstr, "gd:", 3))
        return new DarkNode (m_doc, tag.toUtf8 (), id_node_ignored);
    return NULL;
}

void ATOM::Entry::closed () {
    for (NodePtr c = firstChild (); c; c = c->nextSibling ())
        if (c->id == id_node_title) {
            pretty_name = c->innerText ().simplifyWhiteSpace ();
            break;
        }
    Mrl::closed ();
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
    Mrl::closed ();
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
    Mrl::closed ();
}

Node::PlayType ATOM::Content::playType () {
    if (!hasChildNodes () && !src.isEmpty ())
        return play_type_unknown;
    return play_type_none;
}

NodePtr ATOM::MediaGroup::childFromTag (const QString &tag) {
    const char *cstr = tag.latin1 ();
    if (!strcmp (cstr, "media:content"))
        return new ATOM::MediaContent (m_doc);
    else if (!strcmp (cstr, "media:title"))
        return new DarkNode (m_doc, tag.toUtf8 (), id_node_media_title);
    else if (!strcmp (cstr, "media:description"))
        return new DarkNode (m_doc, tag.toUtf8 (), id_node_media_description);
    else if (!strcmp (cstr, "media:thumbnail"))
        return new DarkNode (m_doc, tag.toUtf8 (), id_node_media_thumbnail);
    else if (!strcmp (cstr, "media:category") ||
            !strcmp (cstr, "media:keywords"))
        return new DarkNode (m_doc, tag.toUtf8 (), id_node_ignored);
    else if (!strcmp (cstr, "smil"))
        return new SMIL::Smil (m_doc);
    return NULL;
}

void *ATOM::MediaGroup::message (MessageType msg, void *content) {
    if (MsgChildFinished == msg &&
            ((Posting *) content)->source->isPlayable ())
        finish (); // only play one
    return Element::message (msg, content);
}

//http://code.google.com/apis/youtube/2.0/developers_guide_protocol.html
void ATOM::MediaGroup::closed () {
    QString images;
    QString desc;
    QString title;
    int img_count = 0;
    for (Node *c = firstChild ().ptr (); c; c = c->nextSibling ().ptr ()) {
        switch (c->id) {
        case id_node_media_title:
            title = c->innerText ();
            break;
        case id_node_media_description:
            desc = c->innerText ();
            break;
        case id_node_media_thumbnail:
        {
            Element *e = static_cast <Element *> (c);
            QString url = e->getAttribute (StringPool::attr_url);
            if (!url.isEmpty ()) {
                images += QString ("<img region=\"image\" src=\"") + url + QChar ('"');
                QString w = e->getAttribute (StringPool::attr_width);
                if (!w.isEmpty ())
                    images += QString (" width=\"") + w + QChar ('"');
                QString h = e->getAttribute (StringPool::attr_height);
                if (!h.isEmpty ())
                    images += QString (" height=\"") + h + QChar ('"');
                QString t = e->getAttribute (TrieString ("time"));
                if (!t.isEmpty ())
                    images += QString (" dur=\"") +
                        QString::number (Mrl::parseTimeString (t) / 10) +
                        QChar ('"');
                images += QString (" transIn=\"fade\" transOut=\"ellipsewipe\" fit=\"meet\"/>");
                img_count++;
            }
            break;
        }
        }
    }
    if (img_count) {
        QString buf;
        QTextOStream out (&buf);
        out << "<smil><head>";
        if (!title.isEmpty ())
            out << "<title>" << title << "</title>";
        out << "<layout><root-layout width=\"400\" height=\"300\" background-color=\"#FFFFF0\"/>"
            "<region id=\"image\" left=\"5\" top=\"20\" width=\"130\"/>"
            "<region id=\"text\" left=\"140\" top=\"10\" fit=\"scroll\"/>"
            "</layout>"
            "<transition id=\"fade\" dur=\"0.3\" type=\"fade\"/>"
            "<transition id=\"ellipsewipe\" dur=\"0.5\" type=\"ellipseWipe\"/>"
            "</head><body>"
            "<par><seq repeatCount=\"indefinite\">";
        out << images;
        out << QString ("</seq><smilText region=\"text\">");
        out << XMLStringlet (desc);
        out << QString ("</smilText></par></body></smil>");
        QTextStream inxml (&buf, QIODevice::ReadOnly);
        KMPlayer::readXML (this, inxml, QString (), false);
        NodePtr n = lastChild();
        n->normalize ();
        n->auxiliary_node = true;
        removeChild (n);
        insertBefore (n, firstChild ());
    }
    Element::closed ();
}

void ATOM::MediaContent::closed () {
    for (AttributePtr a = attributes ()->first (); a; a = a->nextSibling ()) {
        if (a->name () == StringPool::attr_url)
            src = a->value();
        else if (a->name () == StringPool::attr_type)
            mimetype = a->value ();
    }
    Mrl::closed ();
}

Node::PlayType ATOM::MediaContent::playType () {
    return src.isEmpty () ? play_type_none : play_type_unknown;
}
