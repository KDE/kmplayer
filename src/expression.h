/**
  This file belong to the KMPlayer project, a movie player plugin for Konqueror
  Copyright (C) 2009  Koos Vriezen <koos.vriezen@gmail.com>

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
**/

#ifndef _KMPLAYER_EXPRESSION_H_
#define _KMPLAYER_EXPRESSION_H_

#include "kmplayerplaylist.h"

namespace KMPlayer {

class Expression : public VirtualVoid {
public:
    virtual bool toBool (Node *state) const = 0;
    virtual int toInt (Node *state) const = 0;
    virtual float toFloat (Node *state) const = 0;
    virtual QString toString (Node *state) const = 0;
    virtual NodeRefList *toNodeList (Node *root) const = 0;
};

Expression *evaluateExpr (const QString &expr);

}

#endif
