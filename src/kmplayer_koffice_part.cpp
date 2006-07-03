/**
 * Copyright (C) 2002-2003 by Koos Vriezen <koos.vriezen@gmail.com>
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
 *  the Free Software Foundation, Inc., 51 Franklin Steet, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 **/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>

#include <qapplication.h>
#include <qcstring.h>
#include <qtimer.h>
#include <qmultilineedit.h>
#include <qpushbutton.h>
#include <qpopupmenu.h>
#include <qslider.h>
#include <qvaluelist.h>

#include <kprocess.h>
#include <kmessagebox.h>
#include <kaboutdata.h>
#include <kdebug.h>
#include <kconfig.h>
#include <kaction.h>
#include <kstandarddirs.h>
#include <kparts/factory.h>

#include "kmplayerpartbase.h"
#include "kmplayer_koffice_part.h"
#include "kmplayerview.h"
#include "kmplayerconfig.h"

#ifdef HAVE_KOFFICE

#include <qdom.h>
//#include <qmetaobject.h>
#include <qlayout.h>
#include <qptrlist.h>
#include <qpainter.h>
#include <koFrame.h>

class KMPlayerFactory : public KParts::Factory {
public:
    KMPlayerFactory ();
    virtual ~KMPlayerFactory ();
    virtual KParts::Part *createPartObject
        (QWidget *wparent, const char *wname, QObject *parent, const char *name,
         const char *className, const QStringList &args);
    static KInstance * instance () { return s_instance; }
private:
    static KInstance * s_instance;
};

K_EXPORT_COMPONENT_FACTORY (libkmplayerkofficepart, KMPlayerFactory)

KInstance *KMPlayerFactory::s_instance = 0;

KMPlayerFactory::KMPlayerFactory () {
    s_instance = new KInstance ("KMPlayerKofficePart");
}

KMPlayerFactory::~KMPlayerFactory () {
    delete s_instance;
}

KParts::Part *KMPlayerFactory::createPartObject
  (QWidget *wparent, const char *wname,
   QObject *parent, const char * name,
   const char * cls, const QStringList & args) {
    if (strstr (cls, "KoDocument"))
        return new KOfficeMPlayer (wparent, wname, parent, name);
    return 0L;
}

//-----------------------------------------------------------------------------


KOfficeMPlayer::KOfficeMPlayer (QWidget *parentWidget, const char *widgetName, QObject* parent, const char* name, bool singleViewMode) 
  : KoDocument (parentWidget, widgetName, parent, name, singleViewMode),
    m_config (new KConfig ("kmplayerrc")),
    m_player (new KMPlayer (parentWidget, 0L, 0L, 0L, m_config))
{
    setInstance (KMPlayerFactory::instance (), false);
    setReadWrite (false);
    m_player->init();
    m_player->setSource (m_player->sources () ["urlsource"]);
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

bool KOfficeMPlayer::loadOasis (const QDomDocument &, KoOasisStyles &, const QDomDocument &, KoStore *) {
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

KOfficeMPlayerView::KOfficeMPlayerView (KOfficeMPlayer* part, QWidget* parent, const char* name)
    : KoView (part, parent, name),
      m_view (static_cast <KMPlayer::View*> (part->player ()->view ())) {
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
