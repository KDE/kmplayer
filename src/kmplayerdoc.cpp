/***************************************************************************
                          kmplayerdoc.cpp  -  description
                             -------------------
    begin                : Sat Dec  7 16:14:51 CET 2002
    copyright            : (C) 2002 by Koos Vriezen
    email                : 
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <qdir.h>
#include <qwidget.h>

#include <klocale.h>
#include <kmessagebox.h>
#include <kio/job.h>
#include <kio/netaccess.h>

#include "kmplayerdoc.h"
#include "kmplayer.h"
#include "kmplayerview.h"

QList<KMPlayerView> *KMPlayerDoc::pViewList = 0L;

KMPlayerDoc::KMPlayerDoc(QWidget *parent, const char *name) : QObject(parent, name)
{
    if(!pViewList)
    {
        pViewList = new QList<KMPlayerView>();
    }

    pViewList->setAutoDelete(true);
}

KMPlayerDoc::~KMPlayerDoc()
{
}

void KMPlayerDoc::addView(KMPlayerView *view)
{
    pViewList->append(view);
}

void KMPlayerDoc::removeView(KMPlayerView *view)
{
    pViewList->remove(view);
}
void KMPlayerDoc::setURL(const KURL &url)
{
    doc_url=url;
}

const KURL& KMPlayerDoc::URL() const
{
    return doc_url;
}

void KMPlayerDoc::slotUpdateAllViews(KMPlayerView *sender)
{
    KMPlayerView *w;
    if(pViewList)
    {
        for(w=pViewList->first(); w!=0; w=pViewList->next())
        {
            if(w!=sender)
                w->repaint();
        }
    }

}

void KMPlayerDoc::closeDocument()
{
    deleteContents();
}

bool KMPlayerDoc::newDocument()
{
    modified=false;
    doc_url.setFileName(i18n("Untitled"));
    m_width = -1;
    m_height = -1;
    m_aspect = 0.0;

    return true;
}

bool KMPlayerDoc::openDocument(const KURL& url, const char * /*format*/ /*=0*/)
{
    if (url.isValid ()) {
        QString tmpfile;
        KIO::NetAccess::download (url, tmpfile);

        KIO::NetAccess::removeTempFile (tmpfile);
    }

    modified=false;
    return true;
}

void KMPlayerDoc::deleteContents()
{
}

#include "kmplayerdoc.moc"
