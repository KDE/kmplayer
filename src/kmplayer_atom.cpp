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

Node *ATOM::Feed::childFromTag (const QString & tag) {
    if (!strcmp (tag.latin1 (), "entry"))
        return new ATOM::Entry (m_doc);
    else if (!strcmp (tag.latin1 (), "link"))
        return new ATOM::Link (m_doc);
    else if (!strcmp (tag.latin1 (), "title"))
        return new DarkNode (m_doc, tag.toUtf8 (), id_node_title);
    return NULL;
}

void ATOM::Feed::closed () {
    for (Node *c = firstChild (); c; c = c->nextSibling ())
        if (c->id == id_node_title) {
            title = c->innerText ().simplifyWhiteSpace ();
            break;
        }
    Mrl::closed ();
}

Node *ATOM::Entry::childFromTag (const QString &tag) {
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
    else if (!strcmp (cstr, "gd:rating"))
        return new DarkNode (m_doc, tag.toUtf8 (), id_node_gd_rating);
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
    MediaGroup *group = NULL;
    Node *rating = NULL;
    for (Node *c = firstChild (); c; c = c->nextSibling ())
        if (c->id == id_node_title) {
            title = c->innerText ().simplifyWhiteSpace ();
        } else if (c->id == id_node_gd_rating) {
            rating = c;
        } else if (c->id == id_node_media_group) {
            group = static_cast <MediaGroup *> (c);
        }
    if (group)
        group->addSummary (this, rating);
    Mrl::closed ();
}

Node::PlayType ATOM::Link::playType () {
    return src.isEmpty () ? play_type_none : play_type_unknown;
}

void ATOM::Link::closed () {
    QString href;
    QString rel;
    for (Attribute *a = attributes ()->first (); a; a = a->nextSibling ()) {
        if (a->name () == StringPool::attr_href)
            href = a->value ();
        else if (a->name () == StringPool::attr_title)
            title = a->value ();
        else if (a->name () == "rel")
            rel = a->value ();
    }
    if (!href.isEmpty () && rel == QString::fromLatin1 ("enclosure"))
        src = href;
    else if (title.isEmpty ())
        title = href;
    Mrl::closed ();
}

void ATOM::Content::closed () {
    for (Attribute *a = attributes ()->first (); a; a = a->nextSibling ()) {
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

Node *ATOM::MediaGroup::childFromTag (const QString &tag) {
    const char *cstr = tag.latin1 ();
    if (!strcmp (cstr, "media:content"))
        return new ATOM::MediaContent (m_doc);
    else if (!strcmp (cstr, "media:title"))
        return new DarkNode (m_doc, tag.toUtf8 (), id_node_media_title);
    else if (!strcmp (cstr, "media:description"))
        return new DarkNode (m_doc, tag.toUtf8 (), id_node_media_description);
    else if (!strcmp (cstr, "media:thumbnail"))
        return new DarkNode (m_doc, tag.toUtf8 (), id_node_media_thumbnail);
    else if (!strcmp (cstr, "media:player"))
        return new DarkNode (m_doc, tag.toUtf8 (), id_node_media_player);
    else if (!strcmp (cstr, "media:category") ||
            !strcmp (cstr, "media:keywords") ||
            !strcmp (cstr, "media:credit"))
        return new DarkNode (m_doc, tag.toUtf8 (), id_node_ignored);
    else if (!strcmp (cstr, "smil"))
        return new SMIL::Smil (m_doc);
    return NULL;
}

void ATOM::MediaGroup::message (MessageType msg, void *content) {
    if (MsgChildFinished == msg &&
            id_node_media_content == ((Posting *) content)->source->id)
        finish (); // only play one
    Element::message (msg, content);
}

static QString makeStar (int x, bool fill) {
    QString path = "<path style=\"stroke:#A0A0A0;stroke-width:2px;stroke-opacity:1;";
    if (fill)
        path += "fill:#ff0000";
    else
        path += "fill:#C0C0C0";
    path += "\" d=\"M 21.428572,23.571429 "
        "L 10.84984,18.213257 L 0.43866021,23.890134 L 2.2655767,12.173396 "
        "L -6.3506861,4.0260275 L 5.3571425,2.142857 L 10.443179,-8.5693712 "
        "L 15.852098,1.9835038 L 27.611704,3.5103513 L 19.246772,11.915557 "
        "L 21.428572,23.571429 z\""
        " transform=\"translate(";
    path += QString::number (x);
    path += ",11)\"/>";
    return path;
}

//http://code.google.com/apis/youtube/2.0/developers_guide_protocol.html
void ATOM::MediaGroup::addSummary (Node *p, Node *rating_node) {
    QString images;
    QString desc;
    QString title;
    QString player;
    QString ratings;
    int img_count = 0;
    if (rating_node) {
        Element *e = static_cast <Element *> (rating_node);
        QString nr = e->getAttribute ("average");
        if (!nr.isEmpty ()) {
            int rating = ((int) nr.toDouble ()) % 6;
            ratings = "<img region=\"rating\">"
                "<svg width=\"200\" height=\"40\">";
            for (int i = 0; i < 5; ++i)
                ratings += makeStar (10 + i * 40, rating > i);
            ratings += "</svg></img>";
        }
    }
    for (Node *c = firstChild (); c; c = c->nextSibling ()) {
        switch (c->id) {
        case id_node_media_title:
            title = c->innerText ();
            break;
        case id_node_media_description:
            desc = c->innerText ();
            break;
        case id_node_media_player:
            player = static_cast <Element *> (c)->getAttribute (StringPool::attr_url);
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
                images += QString (" dur=\"20\" transIn=\"fade\" fill=\"transition\" fit=\"meet\"/>");
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
        out << "<layout><root-layout width=\"400\" height=\"300\" background-color=\"#FFFFF0\"/>";
        if (!title.isEmpty ())
            out << "<region id=\"title\" left=\"20\" top=\"10\" height=\"18\" right=\"10\"/>";
        out << "<region id=\"image\" left=\"5\" top=\"40\" width=\"130\" bottom=\"30\"/>";
        if (!ratings.isEmpty ())
            out << "<region id=\"rating\" left=\"15\" width=\"100\" height=\"20\" bottom=\"5\"/>";
        out << "<region id=\"text\" left=\"140\" top=\"40\" bottom=\"10\" right=\"10\" fit=\"scroll\"/>"
            "</layout>"
            "<transition id=\"fade\" dur=\"0.3\" type=\"fade\"/>"
            "</head><body>"
            "<par><seq repeatCount=\"indefinite\">";
        out << images;
        out << "</seq>";
        if (!title.isEmpty ()) {
            if (!player.isEmpty ())
                out << "<a href=\"" << XMLStringlet(player) << "\" target=\"top\">";
            out << "<smilText region=\"title\"><div textFontWeight=\"bold\"";
            if (!player.isEmpty ())
                out << " textColor=\"blue\"";
            out << ">" << XMLStringlet (title) << "</div></smilText>";
            if (!player.isEmpty ())
                out << "</a>";
        }
        if (!ratings.isEmpty ())
            out << ratings;
        out << "<smilText region=\"text\">";
        out << XMLStringlet (desc);
        out << QString ("</smilText></par></body></smil>");
        QTextStream inxml (&buf, QIODevice::ReadOnly);
        KMPlayer::readXML (this, inxml, QString (), false);
        NodePtr n = lastChild();
        n->normalize ();
        n->auxiliary_node = true;
        removeChild (n);
        p->insertBefore (n, p->firstChild ());
    }
}

void ATOM::MediaContent::closed () {
    unsigned fsize = 0;
    TrieString fs ("fileSize");
    for (Attribute *a = attributes ()->first (); a; a = a->nextSibling ()) {
        if (a->name () == StringPool::attr_url)
            src = a->value();
        else if (a->name () == StringPool::attr_type)
            mimetype = a->value ();
        else if (a->name () == StringPool::attr_height)
            size.height = a->value ().toInt ();
        else if (a->name () == StringPool::attr_width)
            size.width = a->value ().toInt ();
        else if (a->name () == StringPool::attr_width)
            size.width = a->value ().toInt ();
        else if (a->name () == fs)
            fsize = a->value ().toInt ();
    }
    if (!mimetype.isEmpty ()) {
        title = mimetype;
        if (fsize > 0) {
            if (fsize > 1024 * 1024)
                title += QString (" (%1 Mb)").arg (fsize / (1024 * 1024));
            else
                title += QString (" (%1 kb)").arg (fsize / 1024);
        }
    }
    Mrl::closed ();
}
