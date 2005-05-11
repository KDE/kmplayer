/* This file is part of the KDE project
 *
 * Copyright (C) 2005 Koos Vriezen <koos.vriezen@xs4all.nl>
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef _KMPLAYER_SMILL_H_
#define _KMPLAYER_SMILL_H_

#include <qobject.h>
#include <qstring.h>
#include <qstringlist.h>

#include "kmplayerplaylist.h"

class QTextStream;
class QPixmap;
class QPainter;

namespace KIO {
    class Job;
}

namespace KMPlayer {

class MediaTypeRuntimePrivate;
class ImageDataPrivate;
class TextDataPrivate;

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
class SizeType {
public:
    SizeType ();
    void reset ();
    SizeType & operator = (const QString & s);
    int size (int relative_to = 100);
private:
    int m_size;
    bool percentage;
};

/**
 * Base for RegionRuntime and MediaRuntime, having sizes
 */
class SizedRuntime {
public:
    void resetSizes ();
    void calcSizes (int w, int h, int & xoff, int & yoff, int & w1, int & h1);
    SizeType left, top, width, height, right, bottom;
protected:
    SizedRuntime ();
    bool setSizeParam (const QString & name, const QString & value);
};

/**
 * Live representation of a SMIL element having timings
 */
class TimedRuntime : public QObject, public ElementRuntime {
    Q_OBJECT
public:
    enum TimingState {
        timings_reset = 0, timings_began, timings_started, timings_stopped
    };
    enum DurationTime { begin_time = 0, duration_time, end_time, durtime_last };
    enum Fill { fill_unknown, fill_freeze };

    TimedRuntime (NodePtr e);
    virtual ~TimedRuntime ();
    virtual void begin ();
    virtual void end ();
    virtual void reset ();
    virtual QString setParam (const QString & name, const QString & value);
    TimingState state () const { return timingstate; }
    virtual void paint (QPainter &) {}
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
protected slots:
    void timerEvent (QTimerEvent *);
    virtual void started ();
    virtual void stopped ();
private:
    void setDurationItem (DurationTime item, const QString & val);
protected:
    TimingState timingstate;
    Fill fill;
    int start_timer;
    int dur_timer;
    int repeat_count;
};

/**
 * Runtime data for a region
 */
class RegionRuntime : public ElementRuntime, public SizedRuntime {
public:
    RegionRuntime (NodePtr e);
    KDE_NO_CDTOR_EXPORT ~RegionRuntime () {}
    virtual void begin ();
    virtual void end ();
    virtual void reset ();
    void paint (QPainter & p);
    virtual QString setParam (const QString & name, const QString & value);
    unsigned int background_color;
    bool have_bg_color;
private:
    bool active;
};

/**
 * Runtime data for 'par' group, will activate all children in started()
 */
class ParRuntime : public TimedRuntime {
    Q_OBJECT
public:
    ParRuntime (NodePtr e);
    virtual void started ();
    virtual void stopped ();
};

/**
 * Some common runtime data for all mediatype classes
 */
class MediaTypeRuntime : public TimedRuntime, public SizedRuntime {
    Q_OBJECT
protected:
    MediaTypeRuntime (NodePtr e);
    bool wget (const KURL & url);
    void killWGet ();
public:
    enum Fit { fit_fill, fit_hidden, fit_meet, fit_slice, fit_scroll };
    ~MediaTypeRuntime ();
    virtual void end ();
    virtual void started ();
    virtual void stopped ();
    virtual QString setParam (const QString & name, const QString & value);
protected:
    MediaTypeRuntimePrivate * mt_d;
    QString source_url;
    Fit fit;
    ConnectionPtr paint_connection;
    //(will be shielded by AudioVideoData's) ConnectionPtr sized_connection;
    ConnectionPtr activated_connection;
    ConnectionPtr outbounds_connection;
    ConnectionPtr inbounds_connection;
protected slots:
    virtual void slotResult (KIO::Job*);
    void slotData (KIO::Job*, const QByteArray& qb);
};

/**
 * Data needed for audio/video clips
 */
class AudioVideoData : public MediaTypeRuntime {
    Q_OBJECT
public:
    AudioVideoData (NodePtr e);
    virtual bool isAudioVideo ();
    virtual QString setParam (const QString & name, const QString & value);
    virtual void started ();
    virtual void stopped ();
    ConnectionPtr sized_connection;
};

/**
 * Data needed for an image
 */
class ImageData : public MediaTypeRuntime {
    Q_OBJECT
public:
    ImageData (NodePtr e);
    ~ImageData ();
    void paint (QPainter & p);
    virtual QString setParam (const QString & name, const QString & value);
    ImageDataPrivate * d;
protected slots:
    virtual void started ();
private slots:
    virtual void slotResult (KIO::Job*);
    void movieUpdated (const QRect &);
    void movieStatus (int);
    void movieResize (const QSize &);
};

/**
 * Data needed for text
 */
class TextData : public MediaTypeRuntime {
    Q_OBJECT
public:
    TextData (NodePtr e);
    ~TextData ();
    void paint (QPainter & p);
    void end ();
    virtual QString setParam (const QString & name, const QString & value);
    TextDataPrivate * d;
protected slots:
    virtual void started ();
private slots:
    virtual void slotResult (KIO::Job*);
};

/**
 * Stores runtime data of elements from animate group set/animate/..
 */
class AnimateGroupData : public TimedRuntime {
    Q_OBJECT
public:
    KDE_NO_CDTOR_EXPORT ~AnimateGroupData () {}
    virtual QString setParam (const QString & name, const QString & value);
protected:
    KDE_NO_CDTOR_EXPORT AnimateGroupData (NodePtr e) : TimedRuntime (e) {}
    NodePtrW target_element;
    NodePtrW target_region;
    QString changed_attribute;
    QString change_to;
    QString old_value;
};

/**
 * Stores runtime data of set element
 */
class SetData : public AnimateGroupData {
    Q_OBJECT
public:
    KDE_NO_CDTOR_EXPORT SetData (NodePtr e) : AnimateGroupData (e) {}
    KDE_NO_CDTOR_EXPORT ~SetData () {}
protected slots:
    virtual void started ();
    virtual void stopped ();
};

/**
 * Stores runtime data of animate element
 */
class AnimateData : public AnimateGroupData {
    Q_OBJECT
public:
    AnimateData (NodePtr e);
    KDE_NO_CDTOR_EXPORT ~AnimateData () {}
    virtual QString setParam (const QString & name, const QString & value);
    virtual void reset ();
protected slots:
    virtual void started ();
    virtual void stopped ();
    void timerEvent (QTimerEvent *);
private:
    int anim_timer;
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

//-----------------------------------------------------------------------------

namespace SMIL {

/**
 * Represents optional 'head' tag of SMIL document as in
 * &lt;smil&gt;&lt;head/&gt;&lt;body/&gt;&lt;/smil&gt;
 */
class Head : public Element {
public:
    KDE_NO_CDTOR_EXPORT Head (NodePtr & d) : Element (d) {}
    NodePtr childFromTag (const QString & tag);
    KDE_NO_EXPORT const char * nodeName () const { return "head"; }
    void closed ();
    bool expose () const;
};

/**
 * Defines region layout, should reside below 'head' element
 */
class Layout : public RegionBase {
public:
    KDE_NO_CDTOR_EXPORT Layout (NodePtr & d) : RegionBase (d) {}
    NodePtr childFromTag (const QString & tag);
    KDE_NO_EXPORT const char * nodeName () const { return "layout"; }
    void activate ();
    void closed ();
    virtual bool handleEvent (EventPtr event);
    /**
     * recursively calculates dimensions of this and child regions
     */
    void updateLayout ();

    NodePtrW rootLayout;
};

/**
 * Represents a rectangle on the viewing area
 */
class Region : public RegionBase {
public:
    Region (NodePtr & d);
    KDE_NO_EXPORT const char * nodeName () const { return "region"; }
    NodePtr childFromTag (const QString & tag);
    void calculateBounds (int w, int h);
    virtual bool handleEvent (EventPtr event);
    virtual NodeRefListPtr listeners (unsigned int event_id);
private:
    NodeRefListPtr m_ActionListeners;      // mouse clicked
    NodeRefListPtr m_OutOfBoundsListeners; // mouse left
    NodeRefListPtr m_InBoundsListeners;    // mouse entered
    /**
     * boolean for check if pointerEntered/pointerLeft should be called by View
     */
    bool has_mouse;
};

/**
 * Represents the root area for the other regions
 */
class RootLayout : public RegionBase {
public:
    KDE_NO_CDTOR_EXPORT RootLayout (NodePtr & d) : RegionBase (d) {}
    KDE_NO_EXPORT const char * nodeName () const { return "root-layout"; }
};

/**
 * Base for all SMIL media elements having begin/dur/end/.. attributes
 */
class TimedMrl : public Mrl {
public:
    KDE_NO_CDTOR_EXPORT ~TimedMrl () {}
    ElementRuntimePtr getRuntime ();
    void activate ();
    void deactivate ();
    void reset ();
    void childDone (NodePtr child);
    virtual bool handleEvent (EventPtr event);
protected:
    TimedMrl (NodePtr & d);
    virtual NodeRefListPtr listeners (unsigned int event_id);
    virtual ElementRuntimePtr getNewRuntime ();

    NodeRefListPtr m_StartedListeners;      // Element about to be started
    NodeRefListPtr m_StoppedListeners;      // Element stopped
    ElementRuntimePtr runtime;
};

/**
 * Abstract base for the group elements (par/seq/excl/..)
 */
class GroupBase : public TimedMrl {
public:
    KDE_NO_CDTOR_EXPORT ~GroupBase () {}
    bool isMrl ();
protected:
    KDE_NO_CDTOR_EXPORT GroupBase (NodePtr & d) : TimedMrl (d) {}
};

/**
 * A Par represents parallel processing of all its children
 */
class Par : public GroupBase {
public:
    KDE_NO_CDTOR_EXPORT Par (NodePtr & d) : GroupBase (d) {}
    NodePtr childFromTag (const QString & tag);
    KDE_NO_EXPORT const char * nodeName () const { return "par"; }
    void activate ();
    void deactivate ();
    void reset ();
    void childDone (NodePtr child);
    ElementRuntimePtr getNewRuntime ();
};

/**
 * A Seq represents sequential processing of all its children
 */
class Seq : public GroupBase {
public:
    KDE_NO_CDTOR_EXPORT Seq (NodePtr & d) : GroupBase (d) {}
    NodePtr childFromTag (const QString & tag);
    KDE_NO_EXPORT const char * nodeName () const { return "seq"; }
    void activate ();
};

/**
 * Represents the 'body' tag of SMIL document as in
 * &lt;smil&gt;&lt;head/&gt;&lt;body/&gt;&lt;/smil&gt;
 */
class Body : public Seq {
public:
    KDE_NO_CDTOR_EXPORT Body (NodePtr & d) : Seq (d) {}
    KDE_NO_EXPORT const char * nodeName () const { return "body"; }
};

/**
 * An Excl represents exclusive processing of one of its children
 */
class Excl : public GroupBase {
public:
    KDE_NO_CDTOR_EXPORT Excl (NodePtr & d) : GroupBase (d) {}
    NodePtr childFromTag (const QString & tag);
    KDE_NO_EXPORT const char * nodeName () const { return "excl"; }
    void activate ();
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
class Switch : public Element {
public:
    KDE_NO_CDTOR_EXPORT Switch (NodePtr & d) : Element (d) {}
    NodePtr childFromTag (const QString & tag);
    KDE_NO_EXPORT const char * nodeName () const { return "switch"; }
    // Condition
    void activate ();
    void deactivate ();
    void reset ();
    void childDone (NodePtr child);
};

/**
 * Abstract base for the MediaType classes (video/audio/text/img/..)
 */
class MediaType : public TimedMrl {
public:
    MediaType (NodePtr & d, const QString & t);
    NodePtr childFromTag (const QString & tag);
    KDE_NO_EXPORT const char * nodeName () const { return m_type.latin1 (); }
    void opened ();
    void activate ();
    virtual bool handleEvent (EventPtr event);
    QString m_type;
    unsigned int bitrate;
protected:
    NodeRefListPtr listeners (unsigned int event_id);
    NodeRefListPtr m_ActionListeners;      // mouse clicked
    NodeRefListPtr m_OutOfBoundsListeners; // mouse left
    NodeRefListPtr m_InBoundsListeners;    // mouse entered
};

class AVMediaType : public MediaType {
public:
    AVMediaType (NodePtr & d, const QString & t);
    ElementRuntimePtr getNewRuntime ();
    void activate ();
    void deactivate ();
    virtual bool handleEvent (EventPtr event);
};

class ImageMediaType : public MediaType {
public:
    ImageMediaType (NodePtr & d);
    ElementRuntimePtr getNewRuntime ();
};

class TextMediaType : public MediaType {
public:
    TextMediaType (NodePtr & d);
    ElementRuntimePtr getNewRuntime ();
};

class Set : public TimedMrl {
public:
    KDE_NO_CDTOR_EXPORT Set (NodePtr & d) : TimedMrl (d) {}
    KDE_NO_EXPORT const char * nodeName () const { return "set"; }
    virtual ElementRuntimePtr getNewRuntime ();
    virtual void activate ();
    bool expose () const { return false; }
    bool isMrl () { return false; }
};

class Animate : public TimedMrl {
public:
    KDE_NO_CDTOR_EXPORT Animate (NodePtr & d) : TimedMrl (d) {}
    KDE_NO_EXPORT const char * nodeName () const { return "animate"; }
    virtual ElementRuntimePtr getNewRuntime ();
    bool expose () const { return false; }
    bool isMrl () { return false; }
};

class Param : public Element {
public:
    KDE_NO_CDTOR_EXPORT Param (NodePtr & d) : Element (d) {}
    KDE_NO_EXPORT const char * nodeName () const { return "param"; }
    void activate ();
    bool expose () const { return false; }
};

} // SMIL namespace

}  // KMPlayer namespace

#endif //_KMPLAYER_SMIL_H_
