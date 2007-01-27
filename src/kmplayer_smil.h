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

#ifdef HAVE_CAIRO
# include <cairo.h>
#endif

class QTextStream;
class QImage;
class QPainter;

namespace KIO {
    class Job;
}

namespace KMPlayer {

struct KMPLAYER_NO_EXPORT ImageData {
    ImageData( const QString & img);
    ~ImageData();
    bool isEmpty ();
    Single width ();
    Single height ();
#ifdef HAVE_CAIRO
    cairo_pattern_t * cairoImage (Single w, Single h, cairo_surface_t *cs);
    cairo_pattern_t * cairoImage (cairo_surface_t *cs);
    cairo_pattern_t * cairo_image;
#endif
    QImage * image;
private:
    Single w, h;
    QString url;
};

typedef SharedPtr <ImageData> ImageDataPtr;
typedef WeakPtr <ImageData> ImageDataPtrW;

struct KMPLAYER_NO_EXPORT CachedImage {
    void setUrl (const QString & url);
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
    void reset ();
    SizeType & operator = (const QString & s);
    Single size (Single relative_to = 100);
    bool isSet () const { return isset; }
private:
    Single m_size;
    bool percentage;
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
    bool setSizeParam (const QString & name, const QString & value);
};

/**
 * Live representation of a SMIL element having timings
 */
class KMPLAYER_NO_EXPORT TimedRuntime {
public:
    enum TimingState {
        timings_reset = 0, timings_began, timings_started, timings_stopped
    };
    enum DurationTime { begin_time = 0, duration_time, end_time, durtime_last };
    enum Fill { fill_unknown, fill_freeze };

    TimedRuntime (NodePtr e);
    virtual ~TimedRuntime ();
    /**
     * Called when element is pulled in scope, from Node::activate()
     */
    virtual void begin ();
    /**
     * Called when element gets out of scope, from Node::reset()
     */
    virtual void end ();
    /**
     * Reset all data, called from end() and init()
     */
    virtual void reset ();
    virtual bool parseParam (const QString & name, const QString & value);
    TimingState state () const { return timingstate; }
    void propagateStop (bool forced);
    void propagateStart ();
    void processEvent (unsigned int event);
    /**
     * Duration items, begin/dur/end, length information or connected element
     */
    struct DurationItem {
        DurationItem () : durval (0) {}
        unsigned int durval;
        ConnectionPtr connection;
    } durations [(const int) durtime_last];
    virtual void started ();
    virtual void stopped ();
private:
    void setDurationItem (DurationTime item, const QString & val);
public:
    TimingState timingstate;
    Fill fill;
protected:
    NodePtrW element;
    TimerInfoPtrW start_timer;
    TimerInfoPtrW dur_timer;
    int repeat_count;
};

/**
 * Some common runtime data for all mediatype classes
 */
class KMPLAYER_NO_EXPORT MediaTypeRuntime : public RemoteObject, public TimedRuntime {
public:
    ~MediaTypeRuntime ();
    virtual void end ();
    virtual void stopped ();
    virtual bool parseParam (const QString & name, const QString & value);
    virtual void postpone (bool b);
    virtual void clipStart () {}
    virtual void clipStop () {}
    CalculatedSizer sizes;
    PostponePtr postpone_lock;
    Fit fit;
protected:
    MediaTypeRuntime (NodePtr e);
    ConnectionPtr document_postponed;      // pauze audio/video accordantly
};

/**
 * Data needed for audio/video clips
 */
class KMPLAYER_NO_EXPORT AudioVideoData : public MediaTypeRuntime {
public:
    AudioVideoData (NodePtr e);
    virtual bool isAudioVideo ();
    virtual bool parseParam (const QString & name, const QString & value);
    virtual void started ();
    virtual void stopped ();
    virtual void postpone (bool b);
    virtual void clipStart ();
    virtual void clipStop ();
};

class KMPLAYER_NO_EXPORT ImageRuntime : public QObject,public MediaTypeRuntime {
    Q_OBJECT
public:
    ImageRuntime (NodePtr e);
    ~ImageRuntime ();
    virtual bool parseParam (const QString & name, const QString & value);
    virtual void postpone (bool b);
    QMovie * img_movie;
    CachedImage cached_img;
    int frame_nr;
protected:
    virtual void started ();
    virtual void stopped ();
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
    virtual bool parseParam (const QString & name, const QString & value);
    int font_size;
    unsigned int font_color;
    unsigned int background_color;
    bool transparent;
    QString text;
    TextRuntimePrivate * d;
protected:
    virtual void started ();
    virtual void remoteReady (QByteArray &);
};

/**
 * Stores runtime data of elements from animate group set/animate/..
 */
class KMPLAYER_NO_EXPORT AnimateGroupData : public TimedRuntime {
public:
    KDE_NO_CDTOR_EXPORT ~AnimateGroupData () {}
    virtual bool parseParam (const QString & name, const QString & value);
    virtual void reset ();
protected:
    void restoreModification ();
    AnimateGroupData (NodePtr e);
    NodePtrW target_element;
    QString changed_attribute;
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
    virtual bool parseParam (const QString & name, const QString & value);
    virtual void reset ();
    virtual void started ();
    virtual void stopped ();
    void timerTick();
private:
    TimerInfoPtrW anim_timer;
    enum { acc_none, acc_sum } accumulate;
    enum { add_replace, add_sum } additive;
    int change_by;
    enum { calc_discrete, calc_linear, calc_paced } calcMode;
    QString change_from;
    QStringList change_values;
    int steps;
    float change_delta, change_to_val, change_from_val;
    QString change_from_unit;
};

/**
 * Translates string to deci-seconds or 'special' high number
 */
bool parseTime (const QString & val, unsigned int & dur /*, const QString & dateformat*/);

//-----------------------------------------------------------------------------

namespace SMIL {

const short id_node_smil = 100;
const short id_node_head = 101;
const short id_node_body = 102;
const short id_node_layout = 103;
const short id_node_root_layout = 104;
const short id_node_region = 105;
const short id_node_regpoint = 106;
const short id_node_transition = 107;
const short id_node_par = 110;
const short id_node_seq = 111;
const short id_node_switch = 112;
const short id_node_excl = 113;
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
const short id_node_first_timed_mrl = id_node_par;
const short id_node_last_timed_mrl = id_node_animate;
const short id_node_last = 200; // reserve 100 ids

inline bool isTimedMrl (const NodePtr & n) {
    return n->id >= id_node_first_timed_mrl && n->id <= id_node_last_timed_mrl;
}

/**
 * '<smil>' tag
 */
class Smil : public Mrl {
public:
    KDE_NO_CDTOR_EXPORT Smil (NodePtr & d) : Mrl (d, id_node_smil) {}
    NodePtr childFromTag (const QString & tag);
    KDE_NO_EXPORT const char * nodeName () const { return "smil"; }
    bool isPlayable ();
    void activate ();
    void deactivate ();
    void closed ();
    void childDone (NodePtr child);
    bool expose () const;
    void accept (Visitor *);
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
class KMPLAYER_NO_EXPORT RegionBase : public Element {
public:
    ~RegionBase ();
    bool expose () const { return false; }
    void activate ();
    void childDone (NodePtr child);
    void deactivate ();
    virtual bool handleEvent (EventPtr event);
    virtual void parseParam (const QString & name, const QString & value);
    virtual NodeRefListPtr listeners (unsigned int event_id);
    /**
     * repaints region, calls scheduleRepaint(x,y,w,h) on view
     */
    void repaint ();
    /**
     * calculate the relative x,y,w,h on the child region elements
     * given this element's w and h value
     * and child's left/top/right/width/height/bottom attributes
     */
    virtual void updateDimensions (SurfacePtr parent_surface);

    SurfacePtrW surface;
    CalculatedSizer sizes;

    Single x, y, w, h;     // unscaled values
    int z_order;
    unsigned int background_color;
protected:
    RegionBase (NodePtr & d, short id);
    NodeRefListPtr m_SizeListeners;        // region resized
    NodeRefListPtr m_PaintListeners;       // region need repainting
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
    virtual bool handleEvent (EventPtr event);
    virtual void accept (Visitor *);
    /**
     * recursively calculates dimensions of this and child regions
     */
    virtual void updateDimensions (SurfacePtr parent_surface);

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
    virtual bool handleEvent (EventPtr event);
    virtual NodeRefListPtr listeners (unsigned int event_id);
    virtual void accept (Visitor *);
    /**
     * boolean for check if pointerEntered/pointerLeft should be called by View
     */
    bool has_mouse;
private:
    NodeRefListPtr m_ActionListeners;      // mouse clicked
    NodeRefListPtr m_OutOfBoundsListeners; // mouse left
    NodeRefListPtr m_InBoundsListeners;    // mouse entered
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
    void parseParam (const QString & name, const QString & value);
    CalculatedSizer sizes;
};

/**
 * Represents a transition element for starting media types
 */
class KMPLAYER_NO_EXPORT Transition : public Element {
public:
    KDE_NO_CDTOR_EXPORT Transition (NodePtr & d)
        : Element (d, id_node_transition) {}
    ~Transition ();
    KDE_NO_EXPORT const char * nodeName () const { return "transition"; }
    KDE_NO_EXPORT bool expose () const { return false; }
};

/**
 * Base for all SMIL media elements having begin/dur/end/.. attributes
 */
class KMPLAYER_NO_EXPORT TimedMrl : public Mrl {
public:
    ~TimedMrl ();
    void closed ();
    void activate ();
    void begin ();
    void finish ();
    void deactivate ();
    void reset ();
    bool expose () const;
    void childBegan (NodePtr child);
    void childDone (NodePtr child);
    virtual bool handleEvent (EventPtr event);
    virtual NodeRefListPtr listeners (unsigned int event_id);
    KDE_NO_EXPORT void accept (Visitor * v) { v->visit (this); }
    void init ();
    virtual void parseParam (const QString &, const QString &);
    TimedRuntime * timedRuntime ();
protected:
    TimedMrl (NodePtr & d, short id);
    virtual TimedRuntime * getNewRuntime ();

    NodeRefListPtr m_StartedListeners;      // Element about to be started
    NodeRefListPtr m_StoppedListeners;      // Element stopped
    TimedRuntime * runtime;
};

KDE_NO_EXPORT inline TimedRuntime * TimedMrl::timedRuntime () {
    if (!runtime)
        runtime = getNewRuntime ();
    return runtime;
}

/**
 * Abstract base for the group elements (par/seq/excl/..)
 */
class KMPLAYER_NO_EXPORT GroupBase : public TimedMrl {
public:
    KDE_NO_CDTOR_EXPORT ~GroupBase () {}
    bool isPlayable ();
    void finish ();
    void deactivate ();
protected:
    KDE_NO_CDTOR_EXPORT GroupBase (NodePtr & d, short id) : TimedMrl (d, id) {}
};

/**
 * A Par represents parallel processing of all its children
 */
class KMPLAYER_NO_EXPORT Par : public GroupBase {
public:
    KDE_NO_CDTOR_EXPORT Par (NodePtr & d) : GroupBase (d, id_node_par) {}
    NodePtr childFromTag (const QString & tag);
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
    NodePtr childFromTag (const QString & tag);
    KDE_NO_EXPORT const char * nodeName () const { return "seq"; }
    void begin ();
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
    NodePtr childFromTag (const QString & tag);
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
    NodePtr childFromTag (const QString & tag);
    KDE_NO_EXPORT const char * nodeName () const { return "switch"; }
    bool isPlayable ();
    // Condition
    void activate ();
    void deactivate ();
    void reset ();
    void childDone (NodePtr child);
    Mrl * linkNode ();
    NodePtrW chosenOne;
};

class KMPLAYER_NO_EXPORT Anchor : public Element {
public:
    Anchor (NodePtr & d);
    KDE_NO_CDTOR_EXPORT ~Anchor () {}
    void activate ();
    void deactivate ();
    void childDone (NodePtr child);
    KDE_NO_EXPORT const char * nodeName () const { return "a"; }
    NodePtr childFromTag (const QString & tag);
    KDE_NO_EXPORT void accept (Visitor * v) { v->visit (this); }
    KDE_NO_EXPORT bool expose () const { return false; }
    void parseParam (const QString & name, const QString & value);
    ConnectionPtr mediatype_activated;
    QString href;
    enum { show_new, show_replace } show;
};

/**
 * Abstract base for the MediaType classes (video/audio/text/img/..)
 */
class KMPLAYER_NO_EXPORT MediaType : public TimedMrl {
public:
    MediaType (NodePtr & d, const QString & t, short id);
    NodePtr childFromTag (const QString & tag);
    KDE_NO_EXPORT const char * nodeName () const { return m_type.latin1 (); }
    void opened ();
    void activate ();
    void deactivate ();
    void begin ();
    void finish ();
    bool expose () const;
    void childDone (NodePtr child);
    virtual SurfacePtr getSurface (NodePtr node);
    virtual bool handleEvent (EventPtr event);
    NodeRefListPtr listeners (unsigned int event_id);
    void positionVideoWidget (); // for 'video' and 'ref' nodes
    NodePtrW external_tree; // if src points to playlist, the resolved top node
    NodePtrW trans_in;
    NodePtrW trans_out;
    NodePtrW region_node;
    QString m_type;
    unsigned int bitrate;
protected:
    NodeRefListPtr m_ActionListeners;      // mouse clicked
    NodeRefListPtr m_OutOfBoundsListeners; // mouse left
    NodeRefListPtr m_InBoundsListeners;    // mouse entered
    ConnectionPtr region_sized;            // attached region is sized
    ConnectionPtr region_paint;            // attached region needs painting
    ConnectionPtr region_mouse_enter;      // attached region has mouse entered
    ConnectionPtr region_mouse_leave;      // attached region has mouse left
    ConnectionPtr region_mouse_click;      // attached region is clicked
};

class KMPLAYER_NO_EXPORT AVMediaType : public MediaType {
public:
    AVMediaType (NodePtr & d, const QString & t);
    NodePtr childFromTag (const QString & tag);
    virtual TimedRuntime * getNewRuntime ();
    virtual void defer ();
    virtual void undefer ();
    virtual void finish ();
    virtual bool handleEvent (EventPtr event);
    virtual void accept (Visitor *);
};

class KMPLAYER_NO_EXPORT ImageMediaType : public MediaType {
public:
    ImageMediaType (NodePtr & d);
    TimedRuntime * getNewRuntime ();
    NodePtr childFromTag (const QString & tag);
    virtual void accept (Visitor *);
};

class KMPLAYER_NO_EXPORT TextMediaType : public MediaType {
public:
    TextMediaType (NodePtr & d);
    TimedRuntime * getNewRuntime ();
    virtual void accept (Visitor *);
};

class KMPLAYER_NO_EXPORT RefMediaType : public MediaType {
public:
    RefMediaType (NodePtr & d);
    TimedRuntime * getNewRuntime ();
    virtual bool handleEvent (EventPtr event);
    virtual void accept (Visitor *);
};

class KMPLAYER_NO_EXPORT Brush : public MediaType {
public:
    Brush (NodePtr & d);
    virtual void accept (Visitor *);
};

class KMPLAYER_NO_EXPORT Set : public TimedMrl {
public:
    KDE_NO_CDTOR_EXPORT Set (NodePtr & d) : TimedMrl (d, id_node_set) {}
    KDE_NO_EXPORT const char * nodeName () const { return "set"; }
    virtual TimedRuntime * getNewRuntime ();
    bool isPlayable () { return false; }
};

class KMPLAYER_NO_EXPORT Animate : public TimedMrl {
public:
    KDE_NO_CDTOR_EXPORT Animate (NodePtr & d) : TimedMrl (d, id_node_animate) {}
    KDE_NO_EXPORT const char * nodeName () const { return "animate"; }
    virtual TimedRuntime * getNewRuntime ();
    bool isPlayable () { return false; }
    bool handleEvent (EventPtr event);
};

// TODO animateMotion animateColor transitionFilter

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
