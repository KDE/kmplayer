/**
 * Copyright (C) 2006 by Koos Vriezen <koos.vriezen@gmail.com>
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

#include <kurl.h>

#include "kmplayercommon_log.h"
#include "kmplayer_opml.h"
#include "expression.h"

using namespace KMPlayer;


KDE_NO_EXPORT Node *OPML::Opml::childFromTag (const QString & tag)
{
    QByteArray ba = tag.toLatin1 ();
    const char *name = ba.constData ();
    if (!strcasecmp (name, "head"))
        return new Head (m_doc);
    else if (!strcasecmp (name, "body"))
        return new Body (m_doc);
    return nullptr;
}

void OPML::Opml::closed ()
{
    Expression *expr = evaluateExpr ("/head/title");
    if (expr) {
        expr->setRoot (this);
        title = expr->toString ();
        delete expr;
    }
    Element::closed ();
}

void *OPML::Opml::role (RoleType msg, void *content)
{
    if (RolePlaylist == msg)
        return !title.isEmpty () ? (PlaylistRole *) this : nullptr;
    return Element::role (msg, content);
}

//--------------------------%<-------------------------------------------------

Node *OPML::Head::childFromTag (const QString & tag)
{
    QByteArray ba = tag.toLatin1 ();
    const char *name = ba.constData ();
    if (!strcasecmp (name, "title"))
        return new DarkNode (m_doc, name, id_node_title);
    else if (!strcasecmp (name, "dateCreated"))
        return new DarkNode (m_doc, name, id_node_ignore);
    return nullptr;
}

//--------------------------%<-------------------------------------------------

Node *OPML::Body::childFromTag (const QString & tag)
{
    QByteArray ba = tag.toLatin1 ();
    const char *name = ba.constData ();
    if (!strcasecmp (name, "outline"))
        return new Outline (m_doc);
    return nullptr;
}

//--------------------------%<-------------------------------------------------

void OPML::Outline::closed ()
{
    src = getAttribute ("xmlUrl").trimmed ();
    title = getAttribute ("title").trimmed ();
    Mrl::closed ();
}
