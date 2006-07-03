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

#ifndef KMPLAYER_KOFFICE_PART_H
#define KMPLAYER_KOFFICE_PART_H

#include <config.h>
#include <kmediaplayer/player.h>
#include <kparts/browserextension.h>
#include <kparts/factory.h>
#include <kurl.h>
#ifdef HAVE_KOFFICE
#include <koDocument.h>
#include <koView.h>
#endif //HAVE_KOFFICE
#include <qobject.h>
#include <qvaluelist.h>
#include <qstringlist.h>
#include <qguardedptr.h>
#include <qregexp.h>
#include "kmplayerview.h"
#include "kmplayersource.h"


class KProcess;
class KAboutData;
class KMPlayer;
class KInstance;
class KConfig;
class QIODevice;

#ifdef HAVE_KOFFICE
class KOfficeMPlayer;

class KOfficeMPlayerView : public KoView {
    Q_OBJECT
public:
    KOfficeMPlayerView (KOfficeMPlayer* part, QWidget* parent, const char* name = 0 );
    ~KOfficeMPlayerView ();
    void updateReadWrite(bool) {}
private:
    KMPlayer::View * m_view;
    QGuardedPtr <QWidget> m_oldparent;
};

class KOfficeMPlayer : public KoDocument {
    Q_OBJECT
public:
    KOfficeMPlayer (QWidget *parentWidget = 0, const char *widgetName = 0, QObject* parent = 0, const char* name = 0, bool singleViewMode = false);
    ~KOfficeMPlayer ();

    virtual void paintContent (QPainter& painter, const QRect& rect,
            bool transparent = false, double zoomX = 1.0, double zoomY = 1.0);
    virtual bool initDoc ();
    virtual bool loadXML (QIODevice *, const QDomDocument &);
    virtual bool loadOasis (const QDomDocument &, KoOasisStyles &, const QDomDocument &, KoStore *);
    virtual QDomDocument saveXML ();
    virtual QCString mimeType() const { return "application/x-kmplayer"; }

    KMPlayer * player () const { return m_player; }
protected:
    virtual KoView* createViewInstance (QWidget* parent, const char* name);
private:
    KConfig * m_config;
    KMPlayer * m_player;
    KOfficeMPlayerView * m_view; 
};
#endif //HAVE_KOFFICE

#endif
