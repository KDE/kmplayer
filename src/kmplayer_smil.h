/* This file is part of the KDE project
 *
 * Copyright (C) 2005-2007 Koos Vriezen <koos.vriezen@gmail.com>
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

#ifndef _KMPLAYER_SMILL_H_
#define _KMPLAYER_SMILL_H_

#include "config-kmplayer.h"
#include <qstring.h>
#include <qstringlist.h>

#include "kmplayerplaylist.h"
#include "surface.h"

struct TransTypeInfo;

namespace KMPlayer {

class ImageMedia;

/*
 * Interpretation of sizes
 */
class KMPLAYER_NO_EXPORT SizeType {
public:
    SizeType ();
    SizeType (const QString & s, bool force_perc=false);
    void reset ();
    SizeType & operator = (const QString & s);
    SizeType & operator += (const SizeType & s);
    SizeType & operator -= (const SizeType & s);
    SizeType & operator /= (const int i)
        { perc_size /= i; abs_size /= i; return *this; }
    SizeType & operator *= (const float f)
        { perc_size *= f; abs_size *= f; return *this; }
    Single size (Single relative_to = 100) const;
    bool isSet () const { return isset; }
private:
    Single perc_size;
    Single abs_size;
    bool isset;
};

/**
 * For RegPoint, Region and MediaType, having sizes
 */
class KMPLAYER_NO_EXPORT CalculatedSizer {
public:
    KDE_NO_CDTOR_EXPORT CalculatedSizer () {}
    KDE_NO_CDTOR_EXPORT ~CalculatedSizer () {}

    void resetSizes ();
    void calcSizes (Node *, Single w, Single h,
            Single & xoff, Single & yoff, Single & w1, Single & h1);
    bool applyRegPoints (Node *, Single w, Single h,
            Single & xoff, Single & yoff, Single & w1, Single & h1);
    SizeType left, top, width, height, right, bottom;
    QString reg_point, reg_align;
    bool setSizeParam (const TrieString &name, const QString &value);
    void move (const SizeType &x, const SizeType &y);
};

/**
 * Live representation of a SMIL element having timings
 */
class KMPLAYER_NO_EXPORT Runtime {
public:
    enum TimingState {
        TimingsInit = 0, TimingsInitialized,
        timings_began, timings_started, TimingsTransIn, timings_paused,
        timings_stopped, timings_freezed
    };
    enum Fill {
        fill_default, fill_inherit, fill_remove, fill_freeze,
        fill_hold, fill_transition, fill_auto
    };
    enum DurationTime { BeginTime = 0, DurTime, EndTime, DurTimeLast };
    enum Duration {
        DurIndefinite = -1,
        DurTimer = (int) MsgEventTimer,
        DurActivated = (int) MsgEventClicked,
        DurInBounds = (int) MsgEventPointerInBounds,
        DurOutBounds = (int) MsgEventPointerOutBounds,
        DurStart = (int) MsgEventStarted,
        DurEnd = (int) MsgEventStopped,
        DurMedia = (int)MsgMediaFinished,
        DurTransition,
        DurLastDuration
    };
    Runtime (Element *e);
    ~Runtime ();
    /**
     * Called when element is pulled in scope, from Node::activate()
     */
    void start ();
    void tryFinish () { propagateStop (false); }
    void doFinish () { propagateStop (true); }
    void finish ();
    void startAndBeginNode (); // skip start timer (if any)
    /**
     * Reset all data, called from end() and init()
     */
    void initialize ();
    bool parseParam (const TrieString & name, const QString & value);
    TimingState state () const { return timingstate; }
    void message (MessageType msg, void *content=NULL);
    void *role (RoleType msg, void *content=NULL);
    /**
     * Duration items, begin/dur/end, length information or connected element
     */
    struct DurationItem {
        DurationItem ();
        DurationItem &operator = (const DurationItem &other);
        void clear();
        Duration durval;
        int offset;
        ConnectionLink connection;
        DurationItem *next;
    } durations [(const int) DurTimeLast];
    void setDuration ();
    bool started () const {
        return timingstate == timings_started || timingstate == timings_paused;
    }
    bool active () const {
        return timingstate >= timings_started && timingstate != timings_stopped;
    }
    void stopped ();
    KDE_NO_EXPORT DurationItem & beginTime () { return durations[BeginTime]; }
    KDE_NO_EXPORT DurationItem & durTime () { return durations[DurTime]; }
    KDE_NO_EXPORT DurationItem & endTime () { return durations [EndTime]; }

    TimingState timingstate;
    TimingState unpaused_state;
    int repeat_count;
    ConnectionList m_StartListeners;        // Element about to be started
    ConnectionList m_StartedListeners;      // Element is started
    ConnectionList m_StoppedListeners;      // Element stopped
    Posting *begin_timer;
    Posting *duration_timer;
    Posting *started_timer;
    Posting *stopped_timer;
    NodePtrW paused_by;
    unsigned int start_time;
    unsigned int finish_time;
    unsigned int paused_time;
    Fill fill;
    Fill fill_def;
    Fill fill_active;
    Element *element;
    int trans_in_dur;
private:
    void propagateStop (bool forced);
    void propagateStart ();
    int repeat;
};

class KMPLAYER_NO_EXPORT MouseListeners {
public:
    MouseListeners();

    ConnectionList *receivers (MessageType msg);

    ConnectionList m_ActionListeners;      // mouse clicked
    ConnectionList m_OutOfBoundsListeners; // mouse left
    ConnectionList m_InBoundsListeners;    // mouse entered
};

/**
 * Translates string to centi-seconds or 'special' high number
 */
bool parseTime (const QString & val, int & dur /*,const QString & dateformat*/);

class KMPLAYER_NO_EXPORT SmilTextProperties {
public:
    void init ();
    bool parseParam (const TrieString &name, const QString &value);
    void mask (const SmilTextProperties &props);

    QString font_family;
    int font_color;
    int background_color;
    enum { DirLtr, DirRtl, DirLtro, DirRtlo, DirInherit } text_direction;
    int font_size;
    enum Style {
        StyleNormal, StyleItalic, StyleOblique, StyleRevOblique, StyleInherit
    } font_style;
    enum { WeightNormal, WeightBold, WeightInherit } font_weight;
    enum { ModeAppend, ModeReplace, ModeInherit } text_mode;
    enum { PlaceStart, PlaceCenter, PlaceEnd, PlaceInherit } text_place;
    QString text_style;
    enum { Wrap, NoWrap, WrapInherit } text_wrap;
    enum { SpaceDefault, SpacePreserve } space;
    enum { WritingLrTb, WritingRlTb, WritingTbLr, WritingTbRl } text_writing;
    enum Align { AlignInherit, AlignLeft, AlignCenter, AlignRight } text_align;
};

//-----------------------------------------------------------------------------

namespace SMIL {

const short id_node_smil = 100;
const short id_node_head = 101;
const short id_node_layout = 103;
const short id_node_root_layout = 104;
const short id_node_region = 105;
const short id_node_regpoint = 106;
const short id_node_transition = 107;
const short id_node_body = 110;
const short id_node_par = 111;
const short id_node_seq = 112;
const short id_node_switch = 113;
const short id_node_excl = 114;
const short id_node_img = 120;
const short id_node_text = 122;
const short id_node_ref = 123;
const short id_node_brush = 124;
const short id_node_smil_text = 125;
const short id_node_text_styling = 126;
const short id_node_set = 132;
const short id_node_animate = 133;
const short id_node_title = 140;
const short id_node_param = 141;
const short id_node_meta = 142;
const short id_node_priorityclass = 143;
const short id_node_div = 144;
const short id_node_span = 145;
const short id_node_p = 146;
const short id_node_anchor = 150;
const short id_node_area = 151;
const short id_node_first = id_node_smil;
const short id_node_first_timed_mrl = id_node_body;
const short id_node_last_timed_mrl = id_node_animate;
const short id_node_first_mediatype = id_node_img;
const short id_node_last_mediatype = id_node_brush;
const short id_node_first_group = id_node_body;
const short id_node_last_group = id_node_excl;
const short id_node_last = 200; // reserve 100 ids

/**
 * '<smil>' tag
 */
class Smil : public Mrl {
public:
    KDE_NO_CDTOR_EXPORT Smil (NodePtr & d) : Mrl (d, id_node_smil) {}
    Node *childFromTag (const QString & tag);
    KDE_NO_EXPORT const char * nodeName () const { return "smil"; }
    PlayType playType () { return play_type_video; }
    void activate ();
    void deactivate ();
    void closed ();
    bool expose () const;
    void message (MessageType msg, void *content=NULL);
    void accept (Visitor *v) { v->visit (this); }
    void jump (const QString & id);
    static Smil * findSmilNode (Node * node);

    NodePtrW layout_node;
};

/**
 * Represents optional 'head' tag of SMIL document as in
 * &lt;smil&gt;&lt;head/&gt;&lt;body/&gt;&lt;/smil&gt;
 */
class KMPLAYER_NO_EXPORT Head : public Element {
public:
    KDE_NO_CDTOR_EXPORT Head (NodePtr & d) : Element (d, id_node_head) {}
    Node *childFromTag (const QString & tag);
    KDE_NO_EXPORT const char * nodeName () const { return "head"; }
    void closed ();
    void message (MessageType msg, void *content=NULL);
    bool expose () const;
};

/**
 * Defines region layout, should reside below 'head' element
 */
class KMPLAYER_NO_EXPORT Layout : public Element {
public:
    Layout (NodePtr & d);
    Node *childFromTag (const QString & tag);
    KDE_NO_EXPORT const char * nodeName () const { return "layout"; }
    void closed ();
    void message (MessageType msg, void *content=NULL);
    void accept (Visitor *v) { v->visit (this); }
    bool expose () const { return false; }

    NodePtrW root_layout;
};

/**
 * Base class for SMIL::Region, SMIL::RootLayout and SMIL::Layout
 */
class KMPLAYER_NO_EXPORT RegionBase : public Element {
public:
    enum ShowBackground { ShowAlways, ShowWhenActive };

    ~RegionBase ();
    virtual bool expose () const { return false; }
    virtual void activate ();
    virtual void deactivate ();
    virtual void parseParam (const TrieString & name, const QString & value);
    virtual void message (MessageType msg, void *content=NULL);
    virtual void *role (RoleType msg, void *content=NULL);
    virtual void accept (Visitor *v) { v->visit (this); }
    /**
     * repaints region, calls scheduleRepaint(x,y,w,h) on view
     */
    void repaint ();
    void repaint (const SRect & rect);

    SurfacePtrW region_surface;
    MediaInfo *media_info;
    CalculatedSizer sizes;

    int z_order;
    unsigned int background_color;
    int bg_opacity;
    QString background_image;
    ShowBackground show_background;
    Fit fit;
    ConnectionList m_AttachedMediaTypes;   // active attached mediatypes
    /**
     * boolean for check if pointerEntered/pointerLeft should be called by View
     */
    bool has_mouse;
protected:
    RegionBase (NodePtr & d, short id);
    PostponePtr postpone_lock;               // pause while loading bg image
    void dataArrived (); // image downloaded
};

/**
 * Represents the root area for the other regions
 */
class KMPLAYER_NO_EXPORT RootLayout : public RegionBase {
public:
    KDE_NO_CDTOR_EXPORT RootLayout (NodePtr & d)
        : RegionBase (d, id_node_root_layout) {}
    ~RootLayout ();
    void closed ();
    void deactivate ();
    void message (MessageType msg, void *content=NULL);
    void *role (RoleType msg, void *content=NULL);
    KDE_NO_EXPORT const char * nodeName () const { return "root-layout"; }
};

/**
 * Represents a rectangle on the viewing area
 */
class KMPLAYER_NO_EXPORT Region : public RegionBase {
public:
    Region (NodePtr & d);
    ~Region ();
    void deactivate ();
    KDE_NO_EXPORT const char * nodeName () const { return "region"; }
    Node *childFromTag (const QString & tag);
    void message (MessageType msg, void *content=NULL);
    void *role (RoleType msg, void *content=NULL);
private:
    MouseListeners mouse_listeners;
};

/**
 * Represents a regPoint element for alignment inside regions
 */
class KMPLAYER_NO_EXPORT RegPoint : public Element {
public:
    KDE_NO_CDTOR_EXPORT RegPoint (NodePtr & d) : Element(d, id_node_regpoint) {}
    KDE_NO_CDTOR_EXPORT ~RegPoint () {}
    KDE_NO_EXPORT const char * nodeName () const { return "regPoint"; }
    KDE_NO_EXPORT bool expose () const { return false; }
    void parseParam (const TrieString & name, const QString & value);
    CalculatedSizer sizes;
};

/**
 * Represents a transition element for starting media types
 */
class KMPLAYER_NO_EXPORT Transition : public Element {
public:
    enum TransType {
        TransTypeNone = 0,
        BarWipe, IrisWipe, ClockWipe, SnakeWipe, // required, TODO
        BoxWipe, FourBoxWipe, BarnDoorWipe, DiagonalWipe, BowTieWipe,
        MiscDiagonalWipe, VeeWipe, BarnVeeWipe, ZigZagWipe, BarnZigZagWipe,
        TriangleWipe, ArrowHeadWipe, PentagonWipe, HexagonWipe, EllipseWipe,
        EyeWipe, RoundRectWipe, StarWipe, MiscShapeWipe,
        PinWheelWipe, SingleSweepWipe, FanWipe, DoubleFanWipe,
        DoubleSweepWipe, SaloonDoorWipe, WindShieldWipe,
        SpiralWipe, ParallelSnakesWipe, BoxSnakesWipe, WaterFallWipe,
        PushWipe, SideWipe, Fade,
        TransLast
    };
    enum TransSubType {
        SubTransTypeNone = 0,
        SubLeftToRight, SubTopToBottom, SubTopLeft, SubTopRight,
        SubBottomRight, SubBottomLeft,
        SubTopCenter, SubRightCenter, SubBottomCenter, SubLeftCenter,
        SubCornersIn, SubCornersOut,
        SubCircle, SubVertical, SubHorizontal,
        SubFromLeft, SubFromTop, SubFromRight, SubFromBottom,
        SubCrossfade, SubFadeToColor, SubFadeFromColor,
        SubRectangle, SubDiamond,
        SubClockwiseTwelve, SubClockwiseThree, SubClockwiseSix,
        SubClockwiseNine,
         // and lots more .. TODO
        SubTransLast
    };
    Transition (NodePtr & d);
    KDE_NO_CDTOR_EXPORT ~Transition () {}
    void activate ();
    KDE_NO_EXPORT void accept (Visitor * v) { v->visit (this); }
    KDE_NO_EXPORT const char * nodeName () const { return "transition"; }
    void parseParam (const TrieString & name, const QString & value);
    KDE_NO_EXPORT bool expose () const { return false; }
    bool supported ();
    TransType type;
    TransSubType sub_type;
    TransTypeInfo *type_info;
    enum { dir_forward, dir_reverse } direction;
    int dur; // centi seconds
    float start_progress, end_progress;
    unsigned int fade_color;
};

/**
 * Abstract base for the group elements (par/seq/excl/..)
 */
class KMPLAYER_NO_EXPORT GroupBase : public Element {
public:
    ~GroupBase ();
    Node *childFromTag (const QString & tag);
    PlayType playType () { return play_type_none; }
    void parseParam (const TrieString &name, const QString &value);
    void init ();
    void finish ();
    void activate ();
    void deactivate ();
    void reset ();
    bool expose () const { return false; }
    void message (MessageType msg, void *content=NULL);
    void *role (RoleType msg, void *content=NULL);
    void setJumpNode (NodePtr);
    Runtime *runtime;
protected:
    GroupBase (NodePtr & d, short id);
    NodePtrW jump_node;
};

/**
 * A Par represents parallel processing of all its children
 */
class KMPLAYER_NO_EXPORT Par : public GroupBase {
public:
    KDE_NO_CDTOR_EXPORT Par (NodePtr & d) : GroupBase (d, id_node_par) {}
    KDE_NO_EXPORT const char * nodeName () const { return "par"; }
    void begin ();
    void reset ();
    void message (MessageType msg, void *content=NULL);
    void *role (RoleType msg, void *content=NULL);
    KDE_NO_EXPORT void accept (Visitor * v) { v->visit (this); }
};

/**
 * A Seq represents sequential processing of all its children
 */
class KMPLAYER_NO_EXPORT Seq : public GroupBase {
public:
    KDE_NO_CDTOR_EXPORT Seq (NodePtr & d) : GroupBase(d, id_node_seq) {}
    KDE_NO_EXPORT const char * nodeName () const { return "seq"; }
    void begin ();
    void message (MessageType msg, void *content=NULL);
    void *role (RoleType msg, void *content=NULL);
    KDE_NO_EXPORT void accept (Visitor * v) { v->visit (this); }
    ConnectionLink starting_connection;
    ConnectionLink trans_connection;
protected:
    KDE_NO_CDTOR_EXPORT Seq (NodePtr & d, short id) : GroupBase(d, id) {}
};

/**
 * Represents the 'body' tag of SMIL document as in
 * &lt;smil&gt;&lt;head/&gt;&lt;body/&gt;&lt;/smil&gt;
 */
class KMPLAYER_NO_EXPORT Body : public Seq {
public:
    KDE_NO_CDTOR_EXPORT Body (NodePtr & d) : Seq (d, id_node_body) {}
    KDE_NO_EXPORT const char * nodeName () const { return "body"; }
};

/**
 * An Excl represents exclusive processing of one of its children
 */
class KMPLAYER_NO_EXPORT Excl : public GroupBase {
public:
    Excl (NodePtr & d);
    ~Excl ();
    KDE_NO_EXPORT const char * nodeName () const { return "excl"; }
    Node *childFromTag (const QString & tag);
    void begin ();
    void deactivate ();
    void message (MessageType msg, void *content=NULL);
    KDE_NO_EXPORT void accept (Visitor * v) { v->visit (this); }

    struct ConnectionItem {
        ConnectionItem (ConnectionItem *n) : next (n) {}
        ConnectionLink link;
        ConnectionItem *next;
    } *started_event_list;
    ConnectionLink stopped_connection;
    NodeRefList priority_queue;
    NodePtrW cur_node;
};

/**
 * A PriorityClass groups children within an Excl element
 */
class KMPLAYER_NO_EXPORT PriorityClass : public Element {
public:
    KDE_NO_CDTOR_EXPORT PriorityClass (NodePtr &d)
        : Element (d, id_node_priorityclass) {}
    KDE_NO_EXPORT const char * nodeName () const { return "priorityClass"; }
    Node *childFromTag (const QString & tag);
    void init ();
    void parseParam (const TrieString &, const QString &);
    void message (MessageType msg, void *content=NULL);
    KDE_NO_EXPORT void accept (Visitor * v) { v->visit (this); }

    enum { PeersStop, PeersPause, PeersDefer, PeersNever } peers;
    enum { HigherStop, HigherPause } higher;
    enum { LowerDefer, LowerNever } lower;
    enum { PauseDisplayDisable, PauseDisplayHide, PauseDisplayShow } pause_display;
};

/*
 * An automatic selection between child elements based on a condition
 */
class KMPLAYER_NO_EXPORT Switch : public GroupBase {
public:
    KDE_NO_CDTOR_EXPORT Switch (NodePtr &d) : GroupBase (d, id_node_switch) {}
    KDE_NO_EXPORT const char * nodeName () const { return "switch"; }
    // Condition
    void begin ();
    void deactivate ();
    void reset ();
    void message (MessageType msg, void *content=NULL);
    KDE_NO_EXPORT void accept (Visitor * v) { v->visit (this); }

    Node *chosenOne ();
private:
    NodePtrW chosen_one;
};

class KMPLAYER_NO_EXPORT LinkingBase : public Element {
public:
    KDE_NO_CDTOR_EXPORT ~LinkingBase () {}
    void deactivate ();
    KDE_NO_EXPORT bool expose () const { return false; }
    void parseParam (const TrieString & name, const QString & value);
    ConnectionLink mediatype_attach;
    QString href;
    QString target;
    enum { show_new, show_replace } show;
protected:
    LinkingBase (NodePtr & d, short id);
};

class KMPLAYER_NO_EXPORT Anchor : public LinkingBase {
public:
    Anchor (NodePtr & d);
    KDE_NO_CDTOR_EXPORT ~Anchor () {}
    void activate ();
    void message (MessageType msg, void *content=NULL);
    void *role (RoleType msg, void *content=NULL);
    KDE_NO_EXPORT const char * nodeName () const { return "a"; }
    Node *childFromTag (const QString & tag);
    KDE_NO_EXPORT void accept (Visitor * v) { v->visit (this); }
};

class KMPLAYER_NO_EXPORT Area : public LinkingBase {
public:
    Area (NodePtr & d, const QString & tag);
    ~Area ();
    void activate ();
    KDE_NO_EXPORT const char * nodeName () const { return tag.ascii (); }
    KDE_NO_EXPORT void accept (Visitor * v) { v->visit (this); }
    void parseParam (const TrieString & name, const QString & value);
    void *role (RoleType msg, void *content=NULL);
    SizeType * coords;
    int nr_coords;
    const QString tag;
    MouseListeners mouse_listeners;
};

/**
 * Abstract base for the MediaType classes (video/audio/text/img/..)
 */
class KMPLAYER_NO_EXPORT MediaType : public Mrl {
public:
    MediaType (NodePtr & d, const QString & t, short id);
    ~MediaType ();

    Node *childFromTag (const QString & tag);
    KDE_NO_EXPORT const char * nodeName () const { return m_type.latin1 (); }
    virtual void closed ();
    virtual void init ();
    virtual void activate ();
    virtual void deactivate ();
    virtual void defer ();
    virtual void undefer ();
    virtual void begin ();
    virtual void finish ();
    virtual void reset ();
    SRect calculateBounds ();
    virtual void parseParam (const TrieString & name, const QString & value);
    virtual void message (MessageType msg, void *content=NULL);
    virtual void *role (RoleType msg, void *content=NULL);
    virtual void accept (Visitor *v) { v->visit (this); }
    bool expose () const { return false; }

    Surface *surface ();

    Runtime *runtime;
    SurfacePtrW sub_surface;
    NodePtrW external_tree; // if src points to playlist, the resolved top node
    NodePtrW trans_in;
    NodePtrW trans_out;
    NodePtrW active_trans;
    NodePtrW region_node;
    QString m_type;
    CalculatedSizer sizes;
    CalculatedSizer *pan_zoom;
    Fit fit;
    unsigned int background_color;
    int opacity;
    int bg_opacity;
    unsigned int bitrate;
    unsigned int trans_start_time;
    unsigned int trans_end_time;
    float trans_gain;
    Posting *trans_out_timer;
    enum { sens_opaque, sens_transparent, sens_percentage } sensitivity;
    bool trans_out_active;
    bool has_mouse;

protected:
    virtual void clipStart ();
    virtual void clipStop ();

    MouseListeners mouse_listeners;
    ConnectionList m_MediaAttached;        // mouse entered
    ConnectionList m_TransformedIn;        // transIn ready
    ConnectionLink region_paint;           // attached region needs painting
    ConnectionLink region_attach;          // attached to region
    ConnectionLink document_postponed;     // pause audio/video accordantly
    PostponePtr postpone_lock;
};

class KMPLAYER_NO_EXPORT RefMediaType : public MediaType {
public:
    RefMediaType (NodePtr &doc, const QString &tag);
    Node *childFromTag (const QString & tag);
    virtual void begin ();
    virtual void finish ();
    virtual void accept (Visitor *);
    virtual bool expose () const;
    virtual void parseParam (const TrieString &, const QString &);
    virtual void clipStart ();
};

class KMPLAYER_NO_EXPORT ImageMediaType : public MediaType {
public:
    ImageMediaType (NodePtr & d);
    Node *childFromTag (const QString & tag);
    PlayType playType () { return play_type_image; }
    virtual void activate ();
    virtual void accept (Visitor *);
    virtual void parseParam (const TrieString &, const QString &);
    virtual void message (MessageType msg, void *content=NULL);
};

class KMPLAYER_NO_EXPORT TextMediaType : public MediaType {
public:
    TextMediaType (NodePtr & d);
    PlayType playType () { return play_type_info; }
    virtual void init ();
    virtual void accept (Visitor *);
    virtual void parseParam (const TrieString &, const QString &);

    QString font_name;
    int font_size;
    unsigned int font_color;
    enum { align_left, align_center, align_right } halign;
};

class KMPLAYER_NO_EXPORT Brush : public MediaType {
public:
    Brush (NodePtr & d);
    virtual void accept (Visitor *);
    virtual void parseParam (const TrieString &, const QString &);
    unsigned int color;
};

class KMPLAYER_NO_EXPORT SmilText : public Element {
public:
    SmilText (NodePtr &doc);
    ~SmilText ();
    virtual void init ();
    virtual void activate ();
    virtual void begin ();
    virtual void finish ();
    virtual void deactivate ();
    virtual void reset ();
    virtual bool expose () const { return false; }
    KDE_NO_EXPORT const char *nodeName () const { return "smilText"; }
    Node *childFromTag (const QString & tag);
    virtual void parseParam (const TrieString &name, const QString &value);
    virtual void message (MessageType msg, void *content=NULL);
    virtual void *role (RoleType msg, void *content=NULL);
    virtual void accept (Visitor *v) { v->visit (this); }

    Surface *surface ();

    SmilTextProperties props;
    SurfacePtrW text_surface;
    NodePtrW region_node;
    ConnectionLink region_attach;
    MouseListeners mouse_listeners;
    Runtime *runtime;
};

class KMPLAYER_NO_EXPORT TextFlow : public Element {
public:
    TextFlow (NodePtr &doc, short id, const QByteArray &tag);
    ~TextFlow ();

    virtual void init ();
    virtual void activate ();
    virtual bool expose () const { return false; }
    KDE_NO_EXPORT const char *nodeName () const { return tag.data (); }
    Node *childFromTag (const QString &tag);
    virtual void parseParam (const TrieString &name, const QString &value);
    virtual void accept (Visitor *v) { v->visit (this); }

    SmilTextProperties props;
    QByteArray tag;
};

class KMPLAYER_NO_EXPORT AnimateGroup : public Element {
public:
    ~AnimateGroup ();
    virtual void init ();
    virtual void activate ();
    virtual void finish ();
    virtual void deactivate ();
    virtual void reset ();
    virtual bool expose () const { return false; }
    virtual void parseParam (const TrieString & name, const QString & value);
    virtual void message (MessageType msg, void *content=NULL);
    virtual void *role (RoleType msg, void *content=NULL);
    Runtime *runtime;
protected:
    void restoreModification ();
    NodePtr targetElement ();
    AnimateGroup (NodePtr &d, short _id);
    NodePtrW target_element;
    TrieString changed_attribute;
    QString target_id;
    QString change_to;
    int modification_id;
};

class KMPLAYER_NO_EXPORT Set : public AnimateGroup {
public:
    KDE_NO_CDTOR_EXPORT Set (NodePtr & d) : AnimateGroup (d, id_node_set) {}
    virtual void begin ();
    KDE_NO_EXPORT const char * nodeName () const { return "set"; }
    PlayType playType () { return play_type_none; }
};

class KMPLAYER_NO_EXPORT Animate : public AnimateGroup {
public:
    Animate(NodePtr &doc);
    virtual void init ();
    virtual void begin ();
    virtual void finish ();
    virtual void deactivate ();
    virtual void parseParam (const TrieString & name, const QString & value);
    virtual void message (MessageType msg, void *content=NULL);
    virtual void accept (Visitor *v) { v->visit (this); }
    KDE_NO_EXPORT const char * nodeName () const { return "animate"; }
    PlayType playType () { return play_type_none; }

    Posting *anim_timer;
private:
    void applyStep ();
    bool timerTick();

    enum { acc_none, acc_sum } accumulate;
    enum { add_replace, add_sum } additive;
    int change_by;
    enum { calc_discrete, calc_linear, calc_paced, calc_spline } calcMode;
    QString change_from;
    QStringList change_values;
    int steps;
    float change_delta, change_to_val, change_from_val;
    QString change_from_unit;
};

class KMPLAYER_NO_EXPORT AnimateMotion : public AnimateGroup {
public:
    struct Point2D {
        float x;
        float y;
    };
    AnimateMotion (NodePtr & d);
    ~AnimateMotion ();
    virtual void init ();
    virtual void begin ();
    virtual void finish ();
    virtual void deactivate ();
    virtual void parseParam (const TrieString & name, const QString & value);
    virtual void message (MessageType msg, void *content=NULL);
    virtual void accept (Visitor *v) { v->visit (this); }
    KDE_NO_EXPORT const char * nodeName () const { return "animateMotion"; }
    PlayType playType () { return play_type_none; }

    Posting *anim_timer;
private:
    bool timerTick (unsigned int cur_time);
    bool checkTarget (Node *n);
    bool setInterval ();
    void applyStep ();
    bool getCoordinates (const QString &coord, SizeType &x, SizeType &y);
    enum { acc_none, acc_sum } accumulate;
    enum { add_replace, add_sum } additive;
    enum { calc_discrete, calc_linear, calc_paced, calc_spline } calcMode;
    QString change_from;
    QString change_by;
    QStringList values;
    float *keytimes;
    Point2D *spline_table;
    int keytime_count;
    QStringList splines;
    float control_point[4];
    unsigned int keytime_steps;
    unsigned int interval;
    unsigned int interval_start_time;
    unsigned int interval_end_time;
    SizeType begin_x, begin_y;
    SizeType cur_x, cur_y;
    SizeType delta_x, delta_y;
    SizeType end_x, end_y;
};

// TODO animateColor transitionFilter

class KMPLAYER_NO_EXPORT Param : public Element {
public:
    KDE_NO_CDTOR_EXPORT Param (NodePtr & d) : Element (d, id_node_param) {}
    KDE_NO_EXPORT const char * nodeName () const { return "param"; }
    void activate ();
    bool expose () const { return false; }
};

} // SMIL namespace

}  // KMPlayer namespace

#endif //_KMPLAYER_SMIL_H_
