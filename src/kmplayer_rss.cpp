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
#include <qtextedit.h>
#include <qapplication.h>
#include <qpainter.h>
#include <qpixmap.h>
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
    edit = new QTextEdit;
    edit->setGeometry (0, 0, w, h/2);
    edit->setReadOnly (true);
    edit->setHScrollBarMode (QScrollView::AlwaysOff);
    edit->setVScrollBarMode (QScrollView::AlwaysOff);
    edit->setFrameShape (QFrame::NoFrame);
    edit->setFrameShadow (QFrame::Plain);
    x = y = 0;
    w = h = 50;
    PlayListNotify * n = document()->notify_listener;
    if (n)
        n->setEventDispatcher (m_self);
    Mrl::activate ();
}

void RSS::Item::deactivate () {
    delete edit;
    edit = 0L;
    PlayListNotify * n = document()->notify_listener;
    if (n)
        n->setEventDispatcher (NodePtr ());
    Mrl::deactivate ();
}

bool RSS::Item::handleEvent (EventPtr event) {
    if (event->id () == event_sized) {
        SizeEvent * e = static_cast <SizeEvent *> (event.ptr ());
        x = e->x;
        y = e->y;
        w = e->w;
        h = e->h;
        //kdDebug () << "item size " << x << "," << y << " " << w << "x" << h << endl;
        PlayListNotify * n = document()->notify_listener;
        unsigned int bgcolor = QApplication::palette ().color (QPalette::Normal, QColorGroup::Base).rgb ();
        if (n)
            n->avWidgetSizes (0, h/2, w, h/2, &bgcolor); // just in case, reserve some for video
    } else if (event->id () == event_paint) {
        QPainter & p = static_cast <PaintEvent*> (event.ptr())->painter;
        for (NodePtr c = firstChild (); c; c = c->nextSibling ())
            if (c->id == id_node_description)
                edit->setText (c->innerText ());
        edit->adjustSize ();
        QPixmap pix = QPixmap::grabWidget (edit, 0, 0, w, h/2);
        //kdDebug () << "item paint " << x << "," << y << " " << w << "x" << h << endl;
        p.drawPixmap (0, 0, pix);
    } else
        return Mrl::handleEvent (event);
    return true;
}

void RSS::Enclosure::closed () {
    src = getAttribute (QString::fromLatin1 ("url"));
}
