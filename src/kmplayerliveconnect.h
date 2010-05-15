/**
 * Copyright (C) 2010 by Koos Vriezen <koos.vriezen@gmail.com>
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

#ifndef KMPLAYER_LIVECONNECT_H
#define KMPLAYER_LIVECONNECT_H

#include <kparts/browserextension.h>

class KMPlayerPart;
class JSCommandEntry;


class KMPLAYER_NO_EXPORT KMPlayerLiveConnectExtension :
                  public KParts::LiveConnectExtension
{
    Q_OBJECT
public:
    KMPlayerLiveConnectExtension (KMPlayerPart * parent);
    ~KMPlayerLiveConnectExtension ();

    // LiveConnect interface
    bool get (const unsigned long, const QString &,
            KParts::LiveConnectExtension::Type &, unsigned long &, QString &);
    bool put (const unsigned long, const QString &, const QString &);
    bool call (const unsigned long, const QString &,
            const QStringList &, KParts::LiveConnectExtension::Type &, 
            unsigned long &, QString &);
    void unregister (const unsigned long);
    void sendEvent(const unsigned long objid, const QString & event, const KParts::LiveConnectExtension::ArgList & args ) {
        emit partEvent(objid, event, args);
    }

    void enableFinishEvent (bool b = true) { m_enablefinish = b; }
    QString evaluate (const QString & script);
signals:
    void partEvent (const unsigned long, const QString &,
                    const KParts::LiveConnectExtension::ArgList &);
    void requestGet (const uint32_t, const QString &, QString *);
    void requestCall (const uint32_t, const QString &, const QStringList &, QString *);
public slots:
    void setSize (int w, int h);
    void started ();
    void finished ();
    void evaluate (const QString & script, bool store, QString & result);
private:
    KMPlayerPart * player;
    QString script_result;
    QString m_allow;
    QStringList redir_funcs;
    const JSCommandEntry * lastJSCommandEntry;
    unsigned int object_counter;
    bool m_started;
    bool m_enablefinish;
    bool m_evaluating;
    bool m_skip_put;
};

#endif
