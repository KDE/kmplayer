/**
 * Copyright (C) 2003 by Koos Vriezen <koos ! vriezen () xs4all ! nl>
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

#ifndef _KMPLAYER_BACKEND_H_
#define _KMPLAYER_BACKEND_H_

#include <dcopobject.h>

class KMPlayerBackendPrivate;

class KMPlayerBackend : public DCOPObject {
    K_DCOP
public:
    KMPlayerBackend ();
    virtual ~KMPlayerBackend ();
k_dcop:
    virtual ASYNC setURL (QString url);
    virtual ASYNC play ();
    virtual ASYNC stop ();
    virtual ASYNC pause ();
    virtual ASYNC seek (int pos, bool absolute);
    virtual ASYNC hue (int h, bool absolute);
    virtual ASYNC saturation (int s, bool absolute);
    virtual ASYNC contrast (int c, bool absolute);
    virtual ASYNC brightness (int b, bool absolute);
    virtual ASYNC volume (int v, bool absolute);
    virtual ASYNC quit ();
private:
    KMPlayerBackendPrivate * d;
};

#endif //_KMPLAYER_BACKEND_H_
