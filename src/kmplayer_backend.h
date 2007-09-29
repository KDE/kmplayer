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
    virtual ASYNC setURL (QString url);
    virtual ASYNC setSubTitleURL (QString url);
    virtual ASYNC play (int repeat_count);
    virtual ASYNC stop ();
    virtual ASYNC pause ();
    /* seek (pos, abs) seek position in deci-seconds */
    virtual ASYNC seek (int pos, bool absolute);
    virtual ASYNC hue (int h, bool absolute);
    virtual ASYNC saturation (int s, bool absolute);
    virtual ASYNC contrast (int c, bool absolute);
    virtual ASYNC brightness (int b, bool absolute);
    virtual ASYNC volume (int v, bool absolute);
    virtual ASYNC frequency (int f);
    virtual ASYNC quit ();
    virtual ASYNC setConfig (QByteArray);
    virtual ASYNC setAudioLang (int, QString);
    virtual ASYNC setSubtitle (int, QString);
    virtual bool isPlaying ();
private:
    BackendPrivate * d;
};

} // namespace

#endif //_KMPLAYER_BACKEND_H_
