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
#include "surface.h"

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
const short id_node_first = id_node_imfl;
const short id_node_last = 160;

class Imfl : public Mrl
{
public:
    Imfl (NodePtr & d);
    ~Imfl () override;
    const char * nodeName () const override { return "imfl"; }
    Node *childFromTag (const QString & tag) override;
    void closed () override;
    void defer () override;      // start loading the images if not yet done
    void activate () override;   // start timings, handle paint events
    void finish () override;     // end the timings
    void deactivate () override; // stop handling paint events
    PlayType playType () override { return play_type_image; }
    void message (MessageType msg, void *content=nullptr) override;
    void accept (Visitor *) override;
    Surface *surface ();
    void repaint (); // called whenever something changes on image
    Fit fit;        // how to layout images
    unsigned int duration; // cached attributes of head
    Posting *duration_timer;
    SurfacePtrW rp_surface;
    int needs_scene_img;
};

class TimingsBase  : public Element
{
public:
    TimingsBase (NodePtr & d, const short id);
    ~TimingsBase () override {}
    void activate () override;    // start the 'start_timer'
    void begin () override;       // start_timer has expired
    void finish () override;      // ?duration_timer has expired?
    void deactivate () override;  // disabled
    void message (MessageType msg, void *content=nullptr) override;
    int progress;
    Single x, y, w, h;
    Single srcx, srcy, srcw, srch;
    NodePtrW target;
protected:
    void update (int percentage);
    void cancelTimers ();
    unsigned int start, duration;
    int steps, curr_step;
    Posting *start_timer;
    Posting *duration_timer;
    Posting *update_timer;
    ConnectionLink document_postponed;
};

class Crossfade : public TimingsBase
{
public:
    Crossfade (NodePtr & d)
        : TimingsBase (d, id_node_crossfade) {}
    ~Crossfade () override {}
    const char * nodeName () const override { return "crossfade"; }
    void activate () override;
    void begin () override;
    void accept (Visitor *) override;
};

class Fadein : public TimingsBase
{
public:
    Fadein (NodePtr & d) : TimingsBase(d, id_node_fadein) {}
    ~Fadein () override {}
    const char * nodeName () const override { return "fadein"; }
    void activate () override;
    void begin () override;
    void accept (Visitor *) override;
    unsigned int from_color;
};

class Fadeout : public TimingsBase
{
public:
    Fadeout(NodePtr &d) : TimingsBase(d, id_node_fadeout) {}
    ~Fadeout () override {}
    const char * nodeName () const override { return "fadeout"; }
    void activate () override;
    void begin () override;
    void accept (Visitor *) override;
    unsigned int to_color;
};

class Fill : public TimingsBase
{
public:
    Fill (NodePtr & d) : TimingsBase (d, id_node_fill) {}
    ~Fill () override {}
    const char * nodeName () const override { return "fill"; }
    void activate () override;
    void begin () override;
    unsigned int fillColor () const { return color; }
    void accept (Visitor *) override;
    unsigned int color;
};

class Wipe : public TimingsBase
{
public:
    Wipe (NodePtr & d) : TimingsBase (d, id_node_wipe) {}
    ~Wipe () override {}
    const char * nodeName () const override { return "wipe"; }
    void activate () override;
    void begin () override;
    void accept (Visitor *) override;
    enum { dir_right, dir_left, dir_up, dir_down } direction;
};

class ViewChange : public TimingsBase
{
public:
    ViewChange (NodePtr & d)
        : TimingsBase (d, id_node_viewchange) {}
    ~ViewChange () override {}
    const char * nodeName() const override { return "viewchange"; }
    void activate () override;
    void begin () override;
    void finish () override;
    void accept (Visitor *) override;
};

class Image : public Mrl
{
    PostponePtr postpone_lock;
public:
    Image (NodePtr & d);
    ~Image () override;
    const char * nodeName () const override { return "image"; }
    void activate () override;
    void begin () override;
    void deactivate () override;
    void closed () override;
    void message (MessageType msg, void *content=nullptr) override;
    bool isReady (bool postpone_if_not = false); // is downloading ready
    Surface *surface ();
    SurfacePtrW img_surface;
protected:
    void dataArrived ();
};

} // RP namespace

}  // KMPlayer namespace

#endif //_KMPLAYER_RP_H_

