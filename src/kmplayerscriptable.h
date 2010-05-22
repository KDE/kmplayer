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

#ifndef KMPLAYER_SCRIPTABLE_H
#define KMPLAYER_SCRIPTABLE_H

#include <kparts/scriptableextension.h>

class KMPlayerPart;


class KMPLAYER_NO_EXPORT KMPlayerScriptableExtension :
                  public KParts::ScriptableExtension
{
    Q_OBJECT
public:
    KMPlayerScriptableExtension (KMPlayerPart * parent);
    ~KMPlayerScriptableExtension ();

    virtual QVariant rootObject ();
    virtual QVariant callAsFunction (KParts::ScriptableExtension* caller,
            quint64 objId, const ArgList& args);
    virtual QVariant callFunctionReference (KParts::ScriptableExtension* caller,
            quint64 objId, const QString& f, const ArgList& args);
    virtual QVariant callAsConstructor (KParts::ScriptableExtension* caller,
            quint64 objId, const ArgList& args);
    virtual bool hasProperty (KParts::ScriptableExtension* caller,
            quint64 objId, const QString& propName);
    virtual QVariant get (KParts::ScriptableExtension* caller,
            quint64 objId, const QString& propName);
    virtual bool put (KParts::ScriptableExtension* caller,
            quint64 objId, const QString& propName, const QVariant& value);
    virtual bool removeProperty (KParts::ScriptableExtension* caller,
            quint64 objId, const QString& propName);
    virtual bool enumerateProperties (KParts::ScriptableExtension* caller,
            quint64 objId, QStringList* result);
    virtual bool setException (KParts::ScriptableExtension* caller,
            const QString& message);
    virtual void acquire (quint64 objid);
    virtual void release (quint64 objid);

signals:
    void requestRoot (uint64_t *);
    void requestGet (const QVariant &object, const QString &, QVariant&, bool*);
    void requestCall (const QVariant &object, const QString &, const QVariantList&, QVariant&, bool*);

public slots:
    void evaluate (const QString &script, QVariant &result);
    void objectCall (const QVariant &obj, const QString &func,
            const QVariantList &args, QVariant &result);
    void objectGet (const QVariant &obj, const QString &func, QVariant &res);
    void hostRoot (QVariant &result);
    void acquireObject (const QVariant &object);
    void releaseObject (const QVariant &object);

private:
    KMPlayerPart * player;
};

#endif
