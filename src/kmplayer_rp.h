/* This file is part of the KDE project
 *
 * Copyright (C) 2006-2007 Koos Vriezen <koos.vriezen@gmail.com>
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
const short id_node_wipe = 155;
const short id_node_fadein = 156;
const short id_node_fadeout = 157;
const short id_node_viewchange = 158;
const short id_node_animate = 159;

class Imfl : public Mrl {
public:
    Imfl (NodePtr & d);
    ~Imfl ();
    KDE_NO_EXPORT virtual const char * nodeName () const { return "imfl"; }
    virtual NodePtr childFromTag (const QString & tag);
    virtual void defer ();      // start loading the images if not yet done
    virtual void activate ();   // start timings, handle paint events
    virtual void finish ();     // end the timings
    virtual void deactivate (); // stop handling paint events
    virtual void childDone (NodePtr child); // for if no duration_timer set
    KDE_NO_EXPORT virtual bool expose () const { return false; }
    KDE_NO_EXPORT virtual bool isPlayable () const { return true; }
    virtual bool handleEvent (EventPtr event);
    virtual void accept (Visitor *);
    void repaint (); // called whenever something changes on image
    Fit fit;        // how to layout images
    Single width, height;     // cached attributes of head
    unsigned int duration; // cached attributes of head
    TimerInfoPtrW duration_timer;
    SurfacePtr surface;
    int needs_scene_img;
};

class KMPLAYER_NO_EXPORT TimingsBase  : public Element {
public:
    TimingsBase (NodePtr & d, const short id);
    KDE_NO_CDTOR_EXPORT ~TimingsBase () {}
    virtual void activate ();    // start the 'start_timer'
    virtual void begin ();       // start_timer has expired
    virtual void finish ();      // ?duration_timer has expired?
    virtual void deactivate ();  // disabled
    virtual bool handleEvent (EventPtr event);
    KDE_NO_EXPORT virtual bool expose () const { return false; }
    int progress;
    Single x, y, w, h;
    Single srcx, srcy, srcw, srch;
    NodePtrW target;
protected:
    void update (int percentage);
    unsigned int start, duration;
    int steps, curr_step;
    TimerInfoPtrW start_timer;
    TimerInfoPtrW duration_timer;
    TimerInfoPtrW update_timer;
    ConnectionPtr document_postponed;
};

class KMPLAYER_NO_EXPORT Crossfade : public TimingsBase {
public:
    KDE_NO_CDTOR_EXPORT Crossfade (NodePtr & d)
        : TimingsBase (d, id_node_crossfade) {}
    KDE_NO_CDTOR_EXPORT ~Crossfade () {}
    KDE_NO_EXPORT virtual const char * nodeName () const { return "crossfade"; }
    virtual void activate ();
    virtual void begin ();
    virtual void accept (Visitor *);
};

class KMPLAYER_NO_EXPORT Fadein : public TimingsBase {
public:
    KDE_NO_CDTOR_EXPORT Fadein (NodePtr & d) : TimingsBase(d, id_node_fadein) {}
    KDE_NO_CDTOR_EXPORT ~Fadein () {}
    KDE_NO_EXPORT virtual const char * nodeName () const { return "fadein"; }
    virtual void activate ();
    virtual void begin ();
    virtual void accept (Visitor *);
    unsigned int from_color;
};

class KMPLAYER_NO_EXPORT Fadeout : public TimingsBase {
public:
    KDE_NO_CDTOR_EXPORT Fadeout(NodePtr &d) : TimingsBase(d, id_node_fadeout) {}
    KDE_NO_CDTOR_EXPORT ~Fadeout () {}
    KDE_NO_EXPORT virtual const char * nodeName () const { return "fadeout"; }
    virtual void activate ();
    virtual void begin ();
    virtual void accept (Visitor *);
    unsigned int to_color;
};

class KMPLAYER_NO_EXPORT Fill : public TimingsBase {
public:
    KDE_NO_CDTOR_EXPORT Fill (NodePtr & d) : TimingsBase (d, id_node_fill) {}
    KDE_NO_CDTOR_EXPORT ~Fill () {}
    KDE_NO_EXPORT virtual const char * nodeName () const { return "fill"; }
    virtual void activate ();
    virtual void begin ();
    unsigned int fillColor () const { return color; }
    virtual void accept (Visitor *);
    unsigned int color;
};

class KMPLAYER_NO_EXPORT Wipe : public TimingsBase {
public:
    KDE_NO_CDTOR_EXPORT Wipe (NodePtr & d) : TimingsBase (d, id_node_wipe) {}
    KDE_NO_CDTOR_EXPORT ~Wipe () {}
    KDE_NO_EXPORT virtual const char * nodeName () const { return "wipe"; }
    virtual void activate ();
    virtual void begin ();
    virtual void accept (Visitor *);
    enum { dir_right, dir_left, dir_up, dir_down } direction;
};

class KMPLAYER_NO_EXPORT ViewChange : public TimingsBase {
public:
    KDE_NO_CDTOR_EXPORT ViewChange (NodePtr & d)
        : TimingsBase (d, id_node_viewchange) {}
    KDE_NO_CDTOR_EXPORT ~ViewChange () {}
    KDE_NO_EXPORT virtual const char * nodeName() const { return "viewchange"; }
    virtual void activate ();
    virtual void begin ();
    virtual void finish ();
    virtual void accept (Visitor *);
};

class KMPLAYER_NO_EXPORT Image : public RemoteObject, public Mrl {
    PostponePtr postpone_lock;
public:
    Image (NodePtr & d);
    ~Image ();
    KDE_NO_EXPORT virtual const char * nodeName () const { return "image"; }
    virtual void activate ();
    virtual void deactivate ();
    virtual void closed ();
    bool isReady (bool postpone_if_not = false); // is downloading ready
    CachedImage cached_img;
    //bool expose () const { return false; }
protected:
    virtual void remoteReady (QByteArray & data);
};

} // RP namespace

}  // KMPlayer namespace

#endif //_KMPLAYER_RP_H_

