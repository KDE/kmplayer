/**
 * Copyright (C) 2002-2003 by Koos Vriezen <koos ! vriezen ? xs4all ! nl>
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>

#include <iostream>

#include <qapplication.h>
#include <qcstring.h>
#include <qregexp.h>
#include <qcursor.h>
#include <qtimer.h>
#include <qmultilineedit.h>
#include <qpair.h>
#include <qpushbutton.h>
#include <qpopupmenu.h>
#include <qslider.h>
#include <qvaluelist.h>

#include <kprocess.h>
#include <kprocctrl.h>
#include <klibloader.h>
#include <kmessagebox.h>
#include <kaboutdata.h>
#include <kdebug.h>
#include <kconfig.h>
#include <kaction.h>
#include <kprotocolmanager.h>
#include <kfiledialog.h>
#include <kstandarddirs.h>

#include "kmplayer_part.h"
#include "kmplayerview.h"
#include "kmplayerconfig.h"
#include <qfile.h>

//merge KProtocolManager::proxyForURL and KProtocolManager::slaveProtocol
static bool revmatch(const char *host, const char *nplist) {
    if (host == 0) return false;

    const char *hptr = host + strlen( host ) - 1;
    const char *nptr = nplist + strlen( nplist ) - 1;
    const char *shptr = hptr;

    while (nptr >= nplist) {
        if (*hptr != *nptr) {
            hptr = shptr;

            // Try to find another domain or host in the list
            while (--nptr >= nplist && *nptr != ',' && *nptr != ' ');

            // Strip out multiple spaces and commas
            while (--nptr >= nplist && (*nptr == ',' || *nptr == ' '));
        } else {
            if (nptr == nplist || nptr[-1] == ',' || nptr[-1] == ' ')
                return true;
            hptr--;
            nptr--;
        }
    }
    return false;
}

static bool proxyForURL (KURL & url, QString & proxy) {
    QString protocol = url.protocol ();
    bool protocol_hack = false;
    if (protocol != "http" || protocol != "https" || protocol != "ftp") {
        protocol_hack = true;
        url.setProtocol ("http");
    }
    proxy = KProtocolManager::proxyForURL (url);
    if (protocol_hack)
        url.setProtocol (protocol);
    if (!proxy.isEmpty() && proxy != QString::fromLatin1 ("DIRECT")) {
        QString noProxy = KProtocolManager::noProxyFor ();
        KProtocolManager::ProxyType type = KProtocolManager::proxyType();
        bool useRevProxy = ((type == KProtocolManager::ManualProxy ||
                             type == KProtocolManager::EnvVarProxy) &&
                            KProtocolManager::useReverseProxy ());
        bool isRevMatch = false;

        if (!noProxy.isEmpty()) {
            QString qhost = url.host().lower();
            const char *host = qhost.latin1();
            QString qno_proxy = noProxy.stripWhiteSpace().lower();
            const char *no_proxy = qno_proxy.latin1();
            isRevMatch = revmatch(host, no_proxy);
            // If the hostname does not contain a dot, check if
            // <local> is part of noProxy.
            if (!isRevMatch && host && (strchr(host, '.') == NULL))
                isRevMatch = revmatch("<local>", no_proxy);
        }
        if ((!useRevProxy && !isRevMatch) || (useRevProxy && isRevMatch))
            return true;
    }
    return false;
}

#ifdef HAVE_KOFFICE

#include <qdom.h>
#include <qmetaobject.h>
#include <qptrlist.h>
#include <qpainter.h>
#include <koFrame.h>

KOfficeMPlayer::KOfficeMPlayer (QWidget *parentWidget, const char *widgetName, QObject* parent, const char* name, bool singleViewMode) 
  : KoDocument (parentWidget, widgetName, parent, name, singleViewMode),
    m_config (new KConfig ("kmplayerrc")),
    m_player (new KMPlayer (parentWidget, m_config))
{
    setInstance (KMPlayerFactory::instance (), false);
    setReadWrite (false);
    m_player->setSource (m_player->urlSource ());
    //setWidget (view);
}

KOfficeMPlayer::~KOfficeMPlayer () {
    kdDebug() << "KOfficeMPlayer::~KOfficeMPlayer" << /*kdBacktrace() <<*/ endl;
}

void KOfficeMPlayer::paintContent (QPainter& p, const QRect& r, bool, double, double) {
    p.fillRect (r, QBrush (QColor (0, 0, 0)));
}

bool KOfficeMPlayer::initDoc() {
    kdDebug() << "KOfficeMPlayer::initDoc" << endl;
    return true;
}

bool KOfficeMPlayer::loadXML (QIODevice *, const QDomDocument & doc) {
    QDomNode node = doc.firstChild ();
    if (node.isNull ()) return true;
    kdDebug() << "KOfficeMPlayer::loadXML " << node.nodeName () << endl; 
    node = node.firstChild ();
    if (node.isNull ()) return true;
    kdDebug() << "KOfficeMPlayer::loadXML " << node.nodeName () << endl; 
    node = node.firstChild ();
    if (node.isNull () || !node.isText ()) return true;
    m_player->setURL (KURL (node.toText ().data ()));
    return true;
}

QDomDocument KOfficeMPlayer::saveXML() {
    QDomDocument doc = createDomDocument ("kmplayer", QString::number(1.0));
    QDomElement docelm = doc.documentElement();
    docelm.setAttribute ("editor", "KMPlayer");
    docelm.setAttribute ("mime", "application/x-kmplayer");
    QDomElement url = doc.createElement ("url");
    url.appendChild (doc.createTextNode (m_player->url ().url ()));
    doc.appendChild (url);
    return doc;
}

KoView* KOfficeMPlayer::createViewInstance (QWidget* parent, const char* name) {
    kdDebug() << "KOfficeMPlayer::createViewInstance" << endl;
    return new KOfficeMPlayerView (this, parent);
}

#include <qlayout.h>
KOfficeMPlayerView::KOfficeMPlayerView (KOfficeMPlayer* part, QWidget* parent, const char* name)
    : KoView (part, parent, name),
      m_view (static_cast <KMPlayerView*> (part->player ()->view ())) {
    kdDebug() << "KOfficeMPlayerView::KOfficeMPlayerView this:" << this << " parent:" << parent << endl;
    m_oldparent = static_cast <QWidget*> (m_view->parent());
    m_view->reparent (this, QPoint (0, 0));
    QVBoxLayout * box = new QVBoxLayout (this, 0, 0);
    box->addWidget (m_view);
}

KOfficeMPlayerView::~KOfficeMPlayerView () {
    kdDebug() << "KOfficeMPlayerView::~KOfficeMPlayerView this:" << this << endl;
    m_view->reparent (m_oldparent, QPoint (0, 0));
}

#include "kmplayer_koffice_part.moc"
#endif
