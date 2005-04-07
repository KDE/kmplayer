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
typedef WeakPtr<ElementRuntime> ElementRuntimePtrW;

/**
 * Base for RegionRuntime and TimedRuntime, having events from regions
 */
class RegionSignalerRuntime : public QObject, public ElementRuntime {
    Q_OBJECT
signals:
    /**
     * mouse has clicked the region_node
     */
    void activateEvent ();
    /**
     * mouse has left the region_node
     */
    void outOfBoundsEvent ();
    /**
     * mouse has entered the region_node
     */
    void inBoundsEvent ();
    /**
     * element has stopped, usefull for 'endsync="elm_id"
     */
public slots:
    void emitActivateEvent () { emit activateEvent (); }
    void emitOutOfBoundsEvent () { emit outOfBoundsEvent (); }
    void emitInBoundsEvent () { emit inBoundsEvent (); }
protected:
    RegionSignalerRuntime (ElementPtr e);
};

/**
 * Live representation of a SMIL element having timings
 */
class TimedRuntime : public RegionSignalerRuntime {
    Q_OBJECT
public:
    enum TimingState {
        timings_reset = 0, timings_began, timings_started, timings_stopped
    };
    enum DurationTime { begin_time = 0, duration_time, end_time, durtime_last };
    enum Fill { fill_unknown, fill_freeze };

    TimedRuntime (ElementPtr e);
    virtual ~TimedRuntime ();
    virtual void begin ();
    virtual void end ();
    virtual void reset ();
    virtual QString setParam (const QString & name, const QString & value);
    TimingState state () const { return timingstate; }
    virtual void paint (QPainter &) {}
    void propagateStop (bool forced);
    void propagateStart ();
    /**
     * Duration items, begin/dur/end, length information or connected element
     */
    struct DurationItem {
        DurationItem () : durval (0) {}
        unsigned int durval;
        ElementRuntimePtrW connection;
    } durations [(const int) durtime_last];
signals:
    void elementStopped ();
    void elementAboutToStart (ElementPtr child);
public slots:
    void emitElementStopped () { emit elementStopped (); }
protected slots:
    void timerEvent (QTimerEvent *);
    void elementActivateEvent ();
    void elementOutOfBoundsEvent ();
    void elementInBoundsEvent ();
    void elementHasStopped ();
    virtual void started ();
    virtual void stopped ();
private:
    void processEvent (unsigned int event);
    void setDurationItem (DurationTime item, const QString & val);
    void breakConnection (DurationTime item);
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
class RegionRuntime : public RegionSignalerRuntime {
public:
    RegionRuntime (ElementPtr e);
    KDE_NO_CDTOR_EXPORT ~RegionRuntime () {}
    virtual void begin ();
    virtual void end ();
    virtual void reset ();
    void paint (QPainter & p);
    virtual QString setParam (const QString & name, const QString & value);
    unsigned int background_color;
    QString left;
    QString top;
    QString width;
    QString height;
    QString right;
    QString bottom;
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
    ParRuntime (ElementPtr e);
    virtual void started ();
    virtual void stopped ();
};

/**
 * Runtime data for 'excl' group, taking care of only one child can run
 */
class ExclRuntime : public TimedRuntime {
    Q_OBJECT
public:
    ExclRuntime (ElementPtr e);
    virtual void begin ();
    virtual void reset ();
private slots:
    void elementAboutToStart (ElementPtr child);
};

/**
 * Some common runtime data for all mediatype classes
 */
class MediaTypeRuntime : public TimedRuntime {
    Q_OBJECT
protected:
    MediaTypeRuntime (ElementPtr e);
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
    AudioVideoData (ElementPtr e);
    virtual bool isAudioVideo ();
    virtual QString setParam (const QString & name, const QString & value);
    virtual void started ();
};

/**
 * Data needed for an image
 */
class ImageData : public MediaTypeRuntime {
    Q_OBJECT
public:
    ImageData (ElementPtr e);
    ~ImageData ();
    void paint (QPainter & p);
    virtual QString setParam (const QString & name, const QString & value);
    ImageDataPrivate * d;
protected slots:
    virtual void started ();
private slots:
    virtual void slotResult (KIO::Job*);
};

/**
 * Data needed for text
 */
class TextData : public MediaTypeRuntime {
    Q_OBJECT
public:
    TextData (ElementPtr e);
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
    KDE_NO_CDTOR_EXPORT AnimateGroupData (ElementPtr e) : TimedRuntime (e) {}
    ElementPtrW target_element;
    RegionNodePtrW target_region;
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
    KDE_NO_CDTOR_EXPORT SetData (ElementPtr e) : AnimateGroupData (e) {}
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
    AnimateData (ElementPtr e);
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
    KDE_NO_CDTOR_EXPORT Head (ElementPtr & d) : Element (d) {}
    ElementPtr childFromTag (const QString & tag);
    KDE_NO_EXPORT const char * nodeName () const { return "head"; }
    void closed ();
    bool expose ();
};

/**
 * Defines region layout, should reside below 'head' element
 */
class Layout : public Element {
public:
    KDE_NO_CDTOR_EXPORT Layout (ElementPtr & d) : Element (d) {}
    ElementPtr childFromTag (const QString & tag);
    KDE_NO_EXPORT const char * nodeName () const { return "layout"; }
    void activate ();
    void closed ();
    RegionNodePtr regionRootLayout;
    ElementPtrW rootLayout;
};

/**
 * Represents a rectangle on the viewing area
 */
class Region : public RegionBase {
public:
    KDE_NO_CDTOR_EXPORT Region (ElementPtr & d) : RegionBase (d) {}
    KDE_NO_EXPORT const char * nodeName () const { return "region"; }
    ElementPtr childFromTag (const QString & tag);
    void calculateBounds (int w, int h);
};

/**
 * Represents the root area for the other regions
 */
class RootLayout : public RegionBase {
public:
    KDE_NO_CDTOR_EXPORT RootLayout (ElementPtr & d) : RegionBase (d) {}
    KDE_NO_EXPORT const char * nodeName () const { return "root-layout"; }
};

/**
 * Abstract base for all SMIL media elements having begin/dur/end/.. attributes
 */
class TimedMrl : public Mrl {
public:
    KDE_NO_CDTOR_EXPORT ~TimedMrl () {}
    ElementRuntimePtr getRuntime ();
    void activate ();
    void deactivate ();
    void reset ();
    void childDone (ElementPtr child);
protected:
    KDE_NO_CDTOR_EXPORT TimedMrl (ElementPtr & d) : Mrl (d) {}
    ElementRuntimePtr runtime;
    virtual ElementRuntimePtr getNewRuntime () = 0;
};

/**
 * Abstract base for all SMIL element having begin/dur/end/.. attributes
 */
class TimedElement : public Element {
public:
    KDE_NO_CDTOR_EXPORT ~TimedElement () {}
    ElementRuntimePtr getRuntime ();
    void activate ();
    void deactivate ();
    void reset ();
protected:
    KDE_NO_CDTOR_EXPORT TimedElement (ElementPtr & d) : Element (d) {}
    ElementRuntimePtr runtime;
    virtual ElementRuntimePtr getNewRuntime () = 0;
};

/**
 * Abstract base for the group elements (par/seq/excl/..)
 */
class GroupBase : public TimedMrl {
public:
    KDE_NO_CDTOR_EXPORT ~GroupBase () {}
    bool isMrl ();
protected:
    KDE_NO_CDTOR_EXPORT GroupBase (ElementPtr & d) : TimedMrl (d) {}
    virtual ElementRuntimePtr getNewRuntime ();
};

/**
 * A Par represents parallel processing of all its children
 */
class Par : public GroupBase {
public:
    KDE_NO_CDTOR_EXPORT Par (ElementPtr & d) : GroupBase (d) {}
    ElementPtr childFromTag (const QString & tag);
    KDE_NO_EXPORT const char * nodeName () const { return "par"; }
    void activate ();
    void deactivate ();
    void reset ();
    void childDone (ElementPtr child);
    ElementRuntimePtr getNewRuntime ();
};

/**
 * A Seq represents sequential processing of all its children
 */
class Seq : public GroupBase {
public:
    KDE_NO_CDTOR_EXPORT Seq (ElementPtr & d) : GroupBase (d) {}
    ElementPtr childFromTag (const QString & tag);
    KDE_NO_EXPORT const char * nodeName () const { return "seq"; }
    void activate ();
};

/**
 * Represents the 'body' tag of SMIL document as in
 * &lt;smil&gt;&lt;head/&gt;&lt;body/&gt;&lt;/smil&gt;
 */
class Body : public Seq {
public:
    KDE_NO_CDTOR_EXPORT Body (ElementPtr & d) : Seq (d) {}
    KDE_NO_EXPORT const char * nodeName () const { return "body"; }
};

/**
 * An Excl represents exclusive processing of one of its children
 */
class Excl : public GroupBase {
public:
    KDE_NO_CDTOR_EXPORT Excl (ElementPtr & d) : GroupBase (d) {}
    ElementPtr childFromTag (const QString & tag);
    KDE_NO_EXPORT const char * nodeName () const { return "excl"; }
    void activate ();
    void childDone (ElementPtr child);
    virtual ElementRuntimePtr getNewRuntime ();
};

/*
 * An automatic selection between child elements based on a condition
 */
class Switch : public Element {
public:
    KDE_NO_CDTOR_EXPORT Switch (ElementPtr & d) : Element (d) {}
    ElementPtr childFromTag (const QString & tag);
    KDE_NO_EXPORT const char * nodeName () const { return "switch"; }
    // Condition
    void activate ();
    void deactivate ();
    void reset ();
    void childDone (ElementPtr child);
};

/**
 * Abstract base for the MediaType classes (video/audio/text/img/..)
 */
class MediaType : public TimedMrl {
public:
    MediaType (ElementPtr & d, const QString & t);
    ElementPtr childFromTag (const QString & tag);
    KDE_NO_EXPORT const char * nodeName () const { return m_type.latin1 (); }
    void opened ();
    void activate ();
    QString m_type;
    unsigned int bitrate;
};

class AVMediaType : public MediaType {
public:
    AVMediaType (ElementPtr & d, const QString & t);
    ElementRuntimePtr getNewRuntime ();
    void activate ();
    void deactivate ();
};

class ImageMediaType : public MediaType {
public:
    ImageMediaType (ElementPtr & d);
    ElementRuntimePtr getNewRuntime ();
};

class TextMediaType : public MediaType {
public:
    TextMediaType (ElementPtr & d);
    ElementRuntimePtr getNewRuntime ();
};

class Set : public TimedElement {
public:
    KDE_NO_CDTOR_EXPORT Set (ElementPtr & d) : TimedElement (d) {}
    KDE_NO_EXPORT const char * nodeName () const { return "set"; }
    virtual ElementRuntimePtr getNewRuntime ();
    virtual void activate ();
    bool expose () { return false; }
};

class Animate : public TimedElement {
public:
    KDE_NO_CDTOR_EXPORT Animate (ElementPtr & d) : TimedElement (d) {}
    KDE_NO_EXPORT const char * nodeName () const { return "animate"; }
    virtual ElementRuntimePtr getNewRuntime ();
    bool expose () { return false; }
};

class Param : public Element {
public:
    KDE_NO_CDTOR_EXPORT Param (ElementPtr & d) : Element (d) {}
    KDE_NO_EXPORT const char * nodeName () const { return "param"; }
    void activate ();
    bool expose () { return false; }
};

} // SMIL namespace

}  // KMPlayer namespace

#endif //_KMPLAYER_SMIL_H_
