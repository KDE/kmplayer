/**
 * Copyright (C) 2003 by Koos Vriezen <koos.vriezen@gmail.com>
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

#ifndef _KMPLAYER_BACKEND_H_
#define _KMPLAYER_BACKEND_H_

#include <dcopobject.h>

namespace KMPlayer {

class BackendPrivate;

class Backend : public DCOPObject {
    K_DCOP
public:
    Backend ();
    virtual ~Backend ();
k_dcop:
    virtual ASYNC setURL (unsigned long wid, QString url);
    virtual ASYNC setSubTitleURL (unsigned long wid, QString url);
    virtual ASYNC play (unsigned long wid, int repeat_count);
    virtual ASYNC stop (unsigned long wid);
    virtual ASYNC pause (unsigned long wid);
    /* seek (pos, abs) seek position in deci-seconds */
    virtual ASYNC seek (unsigned long wid, int pos, bool absolute);
    virtual ASYNC hue (unsigned long wid, int h, bool absolute);
    virtual ASYNC saturation (unsigned long wid, int s, bool absolute);
    virtual ASYNC contrast (unsigned long wid, int c, bool absolute);
    virtual ASYNC brightness (unsigned long wid, int b, bool absolute);
    virtual ASYNC volume (unsigned long wid, int v, bool absolute);
    virtual ASYNC frequency (unsigned long wid, int f);
    virtual ASYNC quit ();
    virtual ASYNC setConfig (QByteArray);
    virtual ASYNC setAudioLang (unsigned long wid, int, QString);
    virtual ASYNC setSubtitle (unsigned long wid, int, QString);
    virtual bool isPlaying (unsigned long wid);
private:
    BackendPrivate * d;
};

} // namespace

#endif //_KMPLAYER_BACKEND_H_
