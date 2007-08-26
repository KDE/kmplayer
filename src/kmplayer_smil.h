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

#include <config.h>
#include <qobject.h>
#include <qstring.h>
#include <qstringlist.h>

#include "kmplayerplaylist.h"

class QTextStream;
class QImage;
class QPainter;

namespace KIO {
    class Job;
}

struct TransTypeInfo;

namespace KMPlayer {

struct KMPLAYER_NO_EXPORT ImageData {
    ImageData( const QString & img);
    ~ImageData();
    QImage *image;
private:
    QString url;
};

typedef SharedPtr <ImageData> ImageDataPtr;
typedef WeakPtr <ImageData> ImageDataPtrW;

struct KMPLAYER_NO_EXPORT CachedImage {
    void setUrl (const QString & url);
    bool isEmpty ();
    ImageDataPtr data;
};

class TextRuntimePrivate;

/*
 * Event signaled before the actual starting takes place. Use by SMIL::Excl
 * to stop possible other children
 */
class ToBeStartedEvent : public Event {
public:
    ToBeStartedEvent (NodePtr n);
    NodePtrW node;
};

/*
 * Interpretation of sizes
 */
class KMPLAYER_NO_EXPORT SizeType {
public:
    SizeType ();
    SizeType (const QString & s);
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
 * For RegPoint, RegionRuntime and MediaRuntime, having sizes
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
    bool setSizeParam (const TrieString &name, const QString &value, bool &dim);
    void move (const SizeType &x, const SizeType &y);
};

/**
 * Live representation of a SMIL element having timings
 */
class KMPLAYER_NO_EXPORT Runtime {
public:
    enum TimingState {
        timings_reset = 0, timings_began, timings_started, timings_stopped
    };
    enum DurationTime { begin_time = 0, duration_time, end_time, durtime_last };
    enum Duration {
        dur_infinite = -1, dur_timer = 0, dur_media,
        dur_activated, dur_inbounds, dur_outbounds,
        dur_end, dur_start, dur_last_dur
    };
    Runtime (NodePtr e);
    virtual ~Runtime ();
    /**
     * Called when element is pulled in scope, from Node::activate()
     */
    virtual void begin ();
    virtual void beginAndStart (); // skip start timer (if any)
    /**
     * Reset all data, called from end() and init()
     */
    virtual void reset ();
    virtual bool parseParam (const TrieString & name, const QString & value);
    TimingState state () const { return timingstate; }
    void propagateStop (bool forced);
    void propagateStart ();
    void processEvent (unsigned int event);
    /**
     * Duration items, begin/dur/end, length information or connected element
     */
    struct DurationItem {
        DurationItem () : durval (dur_timer), offset (0) {}
        Duration durval;
        int offset;
        ConnectionPtr connection;
    } durations [(const int) durtime_last];
    virtual void started ();
    virtual void stopped ();
    KDE_NO_EXPORT DurationItem & beginTime () { return durations[begin_time]; }
    KDE_NO_EXPORT DurationItem & durTime () { return durations[duration_time]; }
    KDE_NO_EXPORT DurationItem & endTime () { return durations [end_time]; }
private:
    void setDurationItem (DurationTime item, const QString & val);
public:
    TimingState timingstate;
protected:
    NodePtrW element;
    TimerInfoPtrW start_timer;
    TimerInfoPtrW duration_timer;
    int repeat_count;
};

/**
 * Some common runtime data for all mediatype classes
 */
class KMPLAYER_NO_EXPORT MediaTypeRuntime : public RemoteObject,public Runtime {
public:
    ~MediaTypeRuntime ();
    virtual void reset ();
    virtual void stopped ();
    virtual void postpone (bool b);
    virtual void clipStart ();
    virtual void clipStop ();
    PostponePtr postpone_lock;
    MediaTypeRuntime (NodePtr e);
protected:
    ConnectionPtr document_postponed;      // pause audio/video accordantly
};

/**
 * Data needed for audio/video clips
 */
class KMPLAYER_NO_EXPORT AudioVideoData : public MediaTypeRuntime {
public:
    AudioVideoData (NodePtr e);
    virtual bool isAudioVideo ();
    virtual bool parseParam (const TrieString & name, const QString & value);
    virtual void started ();
    virtual void postpone (bool b);
    virtual void clipStart ();
    virtual void clipStop ();
};

class KMPLAYER_NO_EXPORT ImageRuntime : public QObject,public MediaTypeRuntime {
    Q_OBJECT
public:
    ImageRuntime (NodePtr e);
    ~ImageRuntime ();
    virtual bool parseParam (const TrieString & name, const QString & value);
    virtual void postpone (bool b);
    virtual void clipStart ();
    virtual void clipStop ();
    QMovie * img_movie;
    CachedImage cached_img;
    int frame_nr;
protected:
    virtual void started ();
    virtual void remoteReady (QByteArray &);
private slots:
    void movieUpdated (const QRect &);
    void movieStatus (int);
    void movieResize (const QSize &);
};

/**
 * Data needed for text
 */
class KMPLAYER_NO_EXPORT TextRuntime : public MediaTypeRuntime {
public:
    TextRuntime (NodePtr e);
    ~TextRuntime ();
    void reset ();
    virtual bool parseParam (const TrieString & name, const QString & value);
    int font_size;
    unsigned int font_color;
    unsigned int background_color;
    int bg_opacity;
    enum { align_left, align_center, align_right } halign;
    QString text;
    TextRuntimePrivate * d;
protected:
    virtual void started ();
    virtual void remoteReady (QByteArray &);
};

/**
 * Stores runtime data of elements from animate group set/animate/..
 */
class KMPLAYER_NO_EXPORT AnimateGroupData : public Runtime {
public:
    KDE_NO_CDTOR_EXPORT ~AnimateGroupData () {}
    virtual bool parseParam (const TrieString & name, const QString & value);
    virtual void reset ();
protected:
    void restoreModification ();
    AnimateGroupData (NodePtr e);
    NodePtrW target_element;
    TrieString changed_attribute;
    QString change_to;
    int modification_id;
protected:
    virtual void stopped ();
};

/**
 * Stores runtime data of set element
 */
class KMPLAYER_NO_EXPORT SetData : public AnimateGroupData {
public:
    KDE_NO_CDTOR_EXPORT SetData (NodePtr e) : AnimateGroupData (e) {}
    KDE_NO_CDTOR_EXPORT ~SetData () {}
protected:
    virtual void started ();
};

/**
 * Stores runtime data of animate element
 */
class KMPLAYER_NO_EXPORT AnimateData : public AnimateGroupData {
public:
    AnimateData (NodePtr e);
    KDE_NO_CDTOR_EXPORT ~AnimateData () {}
    virtual bool parseParam (const TrieString & name, const QString & value);
    virtual void reset ();
    virtual void started ();
    virtual void stopped ();
    bool timerTick();
private:
    void applyStep ();
    TimerInfoPtrW anim_timer;
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

/**
 * Stores runtime data of animate element
 */
class KMPLAYER_NO_EXPORT AnimateMotionData : public AnimateGroupData {
public:
    AnimateMotionData (NodePtr e);
    ~AnimateMotionData ();
    virtual bool parseParam (const TrieString & name, const QString & value);
    virtual void reset ();
    virtual void started ();
    virtual void stopped ();
    bool timerTick();
private:
    bool checkTarget (Node *n);
    bool setInterval ();
    void applyStep ();
    bool getCoordinates (const QString &coord, SizeType &x, SizeType &y);
    TimerInfoPtrW anim_timer;
    enum { acc_none, acc_sum } accumulate;
    enum { add_replace, add_sum } additive;
    enum { calc_discrete, calc_linear, calc_paced, calc_spline } calcMode;
    QString change_from;
    QString change_by;
    QStringList values;
    float *keytimes;
    int keytime_count;
    QStringList splines;
    float control_point[4];
    unsigned int steps;
    unsigned int cur_step;
    unsigned int keytime_steps;
    unsigned int interval;
    SizeType begin_x, begin_y;
    SizeType cur_x, cur_y;
    SizeType delta_x, delta_y;
    SizeType end_x, end_y;
};

class KMPLAYER_NO_EXPORT MouseListeners {
public:
    MouseListeners();

    NodeRefListPtr listeners (unsigned int event_id);

    NodeRefListPtr m_ActionListeners;      // mouse clicked
    NodeRefListPtr m_OutOfBoundsListeners; // mouse left
    NodeRefListPtr m_InBoundsListeners;    // mouse entered
};

/**
 * Translates string to deci-seconds or 'special' high number
 */
bool parseTime (const QString & val, int & dur /*,const QString & dateformat*/);

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
const short id_node_audio_video = 121;
const short id_node_text = 122;
const short id_node_ref = 123;
const short id_node_brush = 124;
const short id_node_set = 132;
const short id_node_animate = 133;
const short id_node_title = 140;
const short id_node_param = 141;
const short id_node_meta = 142;
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
    NodePtr childFromTag (const QString & tag);
    KDE_NO_EXPORT const char * nodeName () const { return "smil"; }
    PlayType playType () { return play_type_video; }
    void activate ();
    void deactivate ();
    void closed ();
    void childDone (NodePtr child);
    bool expose () const;
    bool handleEvent (EventPtr event);
    void accept (Visitor *);
    void jump (const QString & id);
    static Smil * findSmilNode (Node * node);
    /**
     * Hack to mark the currently playing MediaType as finished
     * FIXME: think of a descent callback way for this
     */
    Mrl * linkNode ();
    NodePtrW current_av_media_type;
    NodePtrW layout_node;
};

/**
 * Represents optional 'head' tag of SMIL document as in
 * &lt;smil&gt;&lt;head/&gt;&lt;body/&gt;&lt;/smil&gt;
 */
class KMPLAYER_NO_EXPORT Head : public Element {
public:
    KDE_NO_CDTOR_EXPORT Head (NodePtr & d) : Element (d, id_node_head) {}
    NodePtr childFromTag (const QString & tag);
    KDE_NO_EXPORT const char * nodeName () const { return "head"; }
    void closed ();
    void childDone (NodePtr child);
    bool expose () const;
};

/**
 * Base class for SMIL::Region, SMIL::RootLayout and SMIL::Layout
 */
class KMPLAYER_NO_EXPORT RegionBase : public RemoteObject, public Element {
public:
    enum ShowBackground { ShowAlways, ShowWhenActive };

    ~RegionBase ();
    bool expose () const { return false; }
    void activate ();
    void childDone (NodePtr child);
    void deactivate ();
    virtual void parseParam (const TrieString & name, const QString & value);
    /**
     * repaints region, calls scheduleRepaint(x,y,w,h) on view
     */
    void repaint ();
    void repaint (const SRect & rect);
    /**
     * calculate the relative x,y,w,h on the child region elements
     * given this element's w and h value
     * and child's left/top/right/width/height/bottom attributes
     */
    virtual void updateDimensions ();
    void boundsUpdate (); // recalculates and repaint old and new bounds

    virtual SurfacePtr surface ();
    SurfacePtrW region_surface;
    CachedImage cached_img;
    CalculatedSizer sizes;

    Single x, y, w, h;     // unscaled values
    int z_order;
    unsigned int background_color;
    QString background_image;
    ShowBackground show_background;
protected:
    RegionBase (NodePtr & d, short id);
    PostponePtr postpone_lock;               // pause while loading bg image
    virtual void remoteReady (QByteArray &); // image downloaded
};

/**
 * Defines region layout, should reside below 'head' element
 */
class KMPLAYER_NO_EXPORT Layout : public RegionBase {
public:
    Layout (NodePtr & d);
    NodePtr childFromTag (const QString & tag);
    KDE_NO_EXPORT const char * nodeName () const { return "layout"; }
    void activate ();
    void closed ();
    virtual void accept (Visitor *);
    /**
     * recursively calculates dimensions of this and child regions
     */
    virtual void updateDimensions ();
    virtual SurfacePtr surface ();

    NodePtrW rootLayout;
};

/**
 * Represents a rectangle on the viewing area
 */
class KMPLAYER_NO_EXPORT Region : public RegionBase {
public:
    Region (NodePtr & d);
    KDE_NO_EXPORT const char * nodeName () const { return "region"; }
    NodePtr childFromTag (const QString & tag);
    void calculateBounds (Single w, Single h);
    virtual NodeRefListPtr listeners (unsigned int event_id);
    virtual void accept (Visitor *);
    /**
     * boolean for check if pointerEntered/pointerLeft should be called by View
     */
    bool has_mouse;
    NodeRefListPtr m_AttachedMediaTypes;   // active attached mediatypes
private:
    MouseListeners mouse_listeners;
};

/**
 * Represents the root area for the other regions
 */
class KMPLAYER_NO_EXPORT RootLayout : public RegionBase {
public:
    KDE_NO_CDTOR_EXPORT RootLayout (NodePtr & d)
        : RegionBase (d, id_node_root_layout) {}
    KDE_NO_EXPORT const char * nodeName () const { return "root-layout"; }
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
    int dur; // deci seconds
    float start_progress, end_progress;
    unsigned int fade_color;
};

/**
 * Base for all SMIL media elements having begin/dur/end/.. attributes
 */
class KMPLAYER_NO_EXPORT TimedMrl : public Mrl {
public:
    enum Fill {
        fill_default, fill_inherit, fill_remove, fill_freeze,
        fill_hold, fill_transition, fill_auto
    };
    ~TimedMrl ();
    void closed ();
    void activate ();
    void begin ();
    void finish ();
    void deactivate ();
    void reset ();
    bool expose () const { return false; }
    void childBegan (NodePtr child);
    void childDone (NodePtr child);
    virtual bool handleEvent (EventPtr event);
    virtual NodeRefListPtr listeners (unsigned int event_id);
    KDE_NO_EXPORT void accept (Visitor * v) { v->visit (this); }
    void init ();
    virtual void parseParam (const TrieString &, const QString &);
    Runtime * runtime ();
    static Runtime::DurationItem * getDuration (NodePtr n);
    static bool isTimedMrl (const NodePtr & n);
    static bool keepContent (NodePtr n);
    static Fill getDefaultFill (NodePtr n);
    unsigned int begin_time;
    unsigned int finish_time;
    Fill fill;
    Fill fill_def;
    Fill fill_active;
protected:
    TimedMrl (NodePtr & d, short id);
    virtual Runtime * getNewRuntime ();

    NodeRefListPtr m_StartListeners;        // Element about to be started
    NodeRefListPtr m_StartedListeners;      // Element is started
    NodeRefListPtr m_StoppedListeners;      // Element stopped
    Runtime * m_runtime;
};

KDE_NO_EXPORT inline Runtime * TimedMrl::runtime () {
    if (!m_runtime)
        m_runtime = getNewRuntime ();
    return m_runtime;
}

KDE_NO_EXPORT inline bool TimedMrl::isTimedMrl (const NodePtr & n) {
    return n &&
        n->id >= id_node_first_timed_mrl &&
        n->id <= id_node_last_timed_mrl;
}

/**
 * Abstract base for the group elements (par/seq/excl/..)
 */
class KMPLAYER_NO_EXPORT GroupBase : public TimedMrl {
public:
    KDE_NO_CDTOR_EXPORT ~GroupBase () {}
    NodePtr childFromTag (const QString & tag);
    PlayType playType () { return play_type_none; }
    void finish ();
    void deactivate ();
    void setJumpNode (NodePtr);
protected:
    KDE_NO_CDTOR_EXPORT GroupBase (NodePtr & d, short id) : TimedMrl (d, id) {}
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
    void childDone (NodePtr child);
};

/**
 * A Seq represents sequential processing of all its children
 */
class KMPLAYER_NO_EXPORT Seq : public GroupBase {
public:
    KDE_NO_CDTOR_EXPORT Seq (NodePtr & d) : GroupBase(d, id_node_seq) {}
    KDE_NO_EXPORT const char * nodeName () const { return "seq"; }
    void begin ();
    void childDone (NodePtr child);
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
    KDE_NO_CDTOR_EXPORT Excl (NodePtr & d) : GroupBase (d, id_node_excl) {}
    KDE_NO_EXPORT const char * nodeName () const { return "excl"; }
    void begin ();
    void deactivate ();
    void childDone (NodePtr child);
    virtual bool handleEvent (EventPtr event);
private:
    typedef ListNode <ConnectionPtr> ConnectionStoreItem;
    List <ConnectionStoreItem> started_event_list;
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
    void childDone (NodePtr child);
    NodePtrW chosenOne;
};

class KMPLAYER_NO_EXPORT LinkingBase : public Element {
public:
    KDE_NO_CDTOR_EXPORT ~LinkingBase () {}
    void deactivate ();
    KDE_NO_EXPORT bool expose () const { return false; }
    void parseParam (const TrieString & name, const QString & value);
    ConnectionPtr mediatype_activated;
    ConnectionPtr mediatype_attach;
    QString href;
    enum { show_new, show_replace } show;
protected:
    LinkingBase (NodePtr & d, short id);
};

class KMPLAYER_NO_EXPORT Anchor : public LinkingBase {
public:
    Anchor (NodePtr & d);
    KDE_NO_CDTOR_EXPORT ~Anchor () {}
    void activate ();
    void childDone (NodePtr child);
    KDE_NO_EXPORT const char * nodeName () const { return "a"; }
    NodePtr childFromTag (const QString & tag);
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
    NodeRefListPtr listeners (unsigned int event_id);
    SizeType * coords;
    int nr_coords;
    const QString tag;
    MouseListeners mouse_listeners;
};

/**
 * Abstract base for the MediaType classes (video/audio/text/img/..)
 */
class KMPLAYER_NO_EXPORT MediaType : public TimedMrl {
public:
    MediaType (NodePtr & d, const QString & t, short id);
    NodePtr childFromTag (const QString & tag);
    KDE_NO_EXPORT const char * nodeName () const { return m_type.latin1 (); }
    void closed ();
    void activate ();
    void deactivate ();
    void begin ();
    void finish ();
    void childDone (NodePtr child);
    virtual SurfacePtr getSurface (NodePtr node);
    /* (new) sub-region or NULL if not displayed */
    SurfacePtr surface ();
    void resetSurface ();
    SRect calculateBounds ();
    void boundsUpdate (); // recalculates and repaint old and new bounds
    virtual void parseParam (const TrieString & name, const QString & value);
    virtual bool handleEvent (EventPtr event);
    NodeRefListPtr listeners (unsigned int event_id);
    bool needsVideoWidget (); // for 'video' and 'ref' nodes
    SurfacePtrW sub_surface;
    NodePtrW external_tree; // if src points to playlist, the resolved top node
    NodePtrW trans_in;
    NodePtrW trans_out;
    NodePtrW active_trans;
    NodePtrW region_node;
    QString m_type;
    CalculatedSizer sizes;
    Fit fit;
    int opacity;
    unsigned int bitrate;
    unsigned int trans_step;
    unsigned int trans_steps;
    enum { sens_opaque, sens_transparent, sens_percentage } sensitivity;
    bool trans_out_active;
protected:
    MouseListeners mouse_listeners;
    NodeRefListPtr m_MediaAttached;        // mouse entered
    ConnectionPtr region_paint;            // attached region needs painting
    ConnectionPtr region_mouse_enter;      // attached region has mouse entered
    ConnectionPtr region_mouse_leave;      // attached region has mouse left
    ConnectionPtr region_mouse_click;      // attached region is clicked
    ConnectionPtr region_attach;           // attached to region
    TimerInfoPtrW trans_timer;
    TimerInfoPtrW trans_out_timer;
};

class KMPLAYER_NO_EXPORT AVMediaType : public MediaType {
public:
    AVMediaType (NodePtr & d, const QString & t);
    NodePtr childFromTag (const QString & tag);
    virtual Runtime * getNewRuntime ();
    virtual void defer ();
    virtual void undefer ();
    virtual void endOfFile ();
    virtual void accept (Visitor *);
    virtual bool expose () const;
};

class KMPLAYER_NO_EXPORT ImageMediaType : public MediaType {
public:
    ImageMediaType (NodePtr & d);
    Runtime * getNewRuntime ();
    NodePtr childFromTag (const QString & tag);
    PlayType playType () { return play_type_image; }
    virtual void accept (Visitor *);
};

class KMPLAYER_NO_EXPORT TextMediaType : public MediaType {
public:
    TextMediaType (NodePtr & d);
    Runtime * getNewRuntime ();
    PlayType playType () { return play_type_info; }
    virtual void accept (Visitor *);
};

class KMPLAYER_NO_EXPORT RefMediaType : public MediaType {
public:
    RefMediaType (NodePtr & d);
    Runtime * getNewRuntime ();
    virtual void accept (Visitor *);
};

class KMPLAYER_NO_EXPORT Brush : public MediaType {
public:
    Brush (NodePtr & d);
    virtual void accept (Visitor *);
    virtual Runtime * getNewRuntime ();
};

class KMPLAYER_NO_EXPORT Set : public TimedMrl {
public:
    KDE_NO_CDTOR_EXPORT Set (NodePtr & d) : TimedMrl (d, id_node_set) {}
    KDE_NO_EXPORT const char * nodeName () const { return "set"; }
    virtual Runtime * getNewRuntime ();
    PlayType playType () { return play_type_none; }
};

class KMPLAYER_NO_EXPORT Animate : public TimedMrl {
public:
    KDE_NO_CDTOR_EXPORT Animate (NodePtr & d) : TimedMrl (d, id_node_animate) {}
    KDE_NO_EXPORT const char * nodeName () const { return "animate"; }
    virtual Runtime * getNewRuntime ();
    PlayType playType () { return play_type_none; }
    bool handleEvent (EventPtr event);
};

class KMPLAYER_NO_EXPORT AnimateMotion : public TimedMrl {
public:
    KDE_NO_CDTOR_EXPORT AnimateMotion (NodePtr & d)
        : TimedMrl (d, id_node_animate) {}
    KDE_NO_EXPORT const char * nodeName () const { return "animateMotion"; }
    virtual Runtime *getNewRuntime ();
    PlayType playType () { return play_type_none; }
    bool handleEvent (EventPtr event);
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
