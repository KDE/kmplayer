/*
    SPDX-FileCopyrightText: 2006 Koos Vriezen <koos.vriezen@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include "config-kmplayer.h"

#include <kurl.h>

#include "kmplayercommon_log.h"
#include "kmplayer_opml.h"
#include "expression.h"

using namespace KMPlayer;


Node *OPML::Opml::childFromTag (const QString & tag)
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
