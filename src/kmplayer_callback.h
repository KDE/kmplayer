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

#ifndef _KMPLAYER_CALLBACK_H_
#define _KMPLAYER_CALLBACK_H_

#include <dcopobject.h>
#include <qstringlist.h>

namespace KMPlayer {

class CallbackProcess;

class Callback : public DCOPObject {
    K_DCOP
public:
    enum StatusCode { stat_addurl = 0, stat_newtitle, stat_hasvideo };
    Callback (CallbackProcess *);
k_dcop:
    ASYNC statusMessage (int code, QString msg);
    ASYNC errorMessage (int code, QString msg);
    ASYNC subMrl (QString mrl, QString title);
    ASYNC finished ();
    ASYNC playing ();
    ASYNC started (QCString dcopname, QByteArray data);
    ASYNC movieParams (int length, int width, int height, float aspect, QStringList alang, QStringList slang);
    ASYNC moviePosition (int position);
    ASYNC loadingProgress (int percentage);
    ASYNC toggleFullScreen ();
private:
    CallbackProcess * m_process;
};

} // namespace

#endif //_KMPLAYER_CALLBACK_H_
