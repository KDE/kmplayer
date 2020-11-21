/*
    This file belong to the KMPlayer project, a movie player plugin for Konqueror
    SPDX-FileCopyrightText: 2009 Koos Vriezen <koos.vriezen@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef _KMPLAYER_EXPRESSION_H_
#define _KMPLAYER_EXPRESSION_H_

#include "kmplayerplaylist.h"

namespace KMPlayer {

class NodeValue {
public:
    NodeValue (Node *n, Attribute *a=nullptr) : node (n), attr (a) {}
    NodeValue (const QString &s) : node (nullptr), attr (nullptr), string (s) {}

    QString value () const;
    bool operator ==(const NodeValue& v) const {
        return (node && node == v.node && attr == v.attr)
            || (!node && string == v.string);
    }

    Node *node;
    Attribute *attr;
    QString string;
};

class ExprIterator;

class Expression : public VirtualVoid {
public:
    class iterator {
        mutable ExprIterator* iter;
    public:
        iterator(ExprIterator* it=nullptr) : iter(it) {}
        iterator(const iterator& it) : iter(it.iter) { it.iter = nullptr; }
        ~iterator();
        iterator& operator =(const iterator& it);
        bool operator ==(const iterator& it) const;
        bool operator !=(const iterator& it) const { return !(*this == it); }
        iterator& operator ++();
        NodeValue& operator*();
        NodeValue* operator->();
    };
    virtual bool toBool () const = 0;
    virtual int toInt () const = 0;
    virtual float toFloat () const = 0;
    virtual QString toString () const = 0;
    virtual iterator begin() const = 0;
    virtual iterator end() const = 0;
    virtual void setRoot (Node *root) = 0;
};

Expression* evaluateExpr(const QByteArray& expr, const QString& root = QString());

}

#endif
