/* This file is part of the KDE project
 *
 * Copyright (C) 2006 Koos Vriezen <koos.vriezen@xs4all.nl>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Steet, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef _KMPLAYER_RP_H_
#define _KMPLAYER_RP_H_

#include <qobject.h>
#include <qstring.h>

#include "kmplayerplaylist.h"
#include "kmplayer_smil.h"

namespace KIO {
    class Job;
}

namespace KMPlayer {

/**
 * RealPix support classes
 */
namespace RP {

const short id_node_imfl = 150;
const short id_node_head = 151;
const short id_node_image = 152;
const short id_node_crossfade = 153;
const short id_node_fill = 154;

class Imfl : public Element {
public:
    KDE_NO_CDTOR_EXPORT Imfl (NodePtr & d) : Element (d, id_node_imfl) {}
    KDE_NO_CDTOR_EXPORT ~Imfl () {}
    KDE_NO_EXPORT virtual const char * nodeName () const { return "imfl"; }
    virtual NodePtr childFromTag (const QString & tag);
    virtual void activate ();   // start loading the images
    virtual void begin ();      // start timings
    virtual void deactivate (); // end the timings
    virtual bool expose () const { return false; }
    virtual bool handleEvent (EventPtr event);
    int x, y, w, h;
};

class TimingsBase  : public Element {
public:
    TimingsBase (NodePtr & d, const short id);
    KDE_NO_CDTOR_EXPORT ~TimingsBase () {}
    virtual void activate ();    // start the 'start_timer'
    virtual void begin ();       // start_timer has expired
    //virtual void finish ();       // ?duration_timer has expired?
    virtual void deactivate ();  // disabled
    virtual bool handleEvent (EventPtr event);
protected:
    NodePtrW target;
    int start, duration;
    TimerInfoPtrW start_timer;
};

class Crossfade : public TimingsBase {
public:
    KDE_NO_CDTOR_EXPORT Crossfade (NodePtr & d)
        : TimingsBase (d, id_node_crossfade) {}
    KDE_NO_CDTOR_EXPORT ~Crossfade () {}
    KDE_NO_EXPORT virtual const char * nodeName () const { return "crossfade"; }
    virtual void activate ();
    virtual void begin ();
    virtual bool expose () const { return false; }
};

class Fill : public TimingsBase {
public:
    KDE_NO_CDTOR_EXPORT Fill (NodePtr & d) : TimingsBase (d, id_node_fill) {}
    KDE_NO_CDTOR_EXPORT ~Fill () {}
    KDE_NO_EXPORT virtual const char * nodeName () const { return "fill"; }
    virtual void activate ();
    virtual void begin ();
    virtual bool expose () const { return false; }
};

class Image : public RemoteObject, public Mrl {
    Q_OBJECT
public:
    Image (NodePtr & d);
    ~Image ();
    KDE_NO_EXPORT virtual const char * nodeName () const { return "image"; }
    virtual void activate ();
    virtual void closed ();
    //bool expose () const { return false; }
protected:
    virtual void remoteReady ();
    ImageData * d;
};

} // RP namespace

}  // KMPlayer namespace

#endif //_KMPLAYER_RP_H_

