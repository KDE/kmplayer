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

#include "kmplayerplaylist.h"

class QTextStream;
class QPixmap;
class QPainter;

namespace KIO {
    class Job;
}

namespace KMPlayer {

class ImageDataPrivate;
class TextDataPrivate;
typedef WeakPtr<ElementRuntime> ElementRuntimePtrW;

/**
 * Live representation of a SMIL element
 */
class ElementRuntime : public QObject {
    Q_OBJECT
public:
    enum DurationTime {
        begin_time = 0, duration_time, end_time, durtime_last
    };
    ElementRuntime (ElementPtr e);
    virtual ~ElementRuntime ();
    void setDurationItem (DurationTime item, const QString & val);
    /**
     * start, or restart in case of re-use, the durations
     */
    virtual void begin ();
    /**
     * forced killing of timers
     */
    void end ();
    virtual void paint (QPainter &) {}
    /**
     * If this element is attached to a region, region_node points to it
     */
    RegionNodePtrW region_node;
    /**
     * Duration items, begin/dur/end, length information or connected element
     */
    struct DurationItem {
        DurationItem () : durval (0) {}
        unsigned int durval;
        ElementRuntimePtrW connection;
    } durations [(const int) durtime_last];
signals:
    void activateEvent ();
    void outOfBoundsEvent ();
    void inBoundsEvent ();
public slots:
    void emitActivateEvent () { emit activateEvent (); }
    void emitOutOfBoundsEvent () { emit outOfBoundsEvent (); }
    void emitInBoundsEvent () { emit inBoundsEvent (); }
protected slots:
    void timerEvent (QTimerEvent *);
    void elementActivateEvent ();
    void elementOutOfBoundsEvent ();
    void elementInBoundsEvent ();
    /**
     * start_timer timer expired
     */
    virtual void started ();
    /**
     * duration_timer timer expired or no duration set after started
     */
    virtual void stopped ();
private:
    void processEvent (unsigned int event);
protected:
    ElementPtrW element;
    int start_timer;
    int dur_timer;
    int repeat_count;
    bool isstarted;
};

/**
 * Some common runtime data for all mediatype classes
 */
class MediaTypeRuntime : public ElementRuntime {
    Q_OBJECT
protected:
    KDE_NO_CDTOR_EXPORT MediaTypeRuntime (ElementPtr e) : ElementRuntime (e) {}
public:
    KDE_NO_CDTOR_EXPORT ~MediaTypeRuntime () {}
    /**
     * re-implement for regions
     */
    virtual void begin ();
};

class AudioVideoData : public MediaTypeRuntime {
    Q_OBJECT
public:
    AudioVideoData (ElementPtr e);
    virtual bool isAudioVideo ();
protected slots:
    /**
     * start_timer timer expired, start the audio/video clip
     */
    virtual void started ();
};

class ImageData : public MediaTypeRuntime {
    Q_OBJECT
public:
    ImageData (ElementPtr e);
    ~ImageData ();
    void paint (QPainter & p);
    ImageDataPrivate * d;
protected slots:
    /**
     * start_timer timer expired, repaint if we have an image
     */
    virtual void started ();
private slots:
    void slotResult (KIO::Job*);
    void slotData (KIO::Job*, const QByteArray& qb);
};

class TextData : public MediaTypeRuntime {
    Q_OBJECT
public:
    TextData (ElementPtr e);
    ~TextData ();
    void paint (QPainter & p);
    TextDataPrivate * d;
protected slots:
    /**
     * start_timer timer expired, repaint if we have text
     */
    virtual void started ();
private slots:
    void slotResult (KIO::Job*);
    void slotData (KIO::Job*, const QByteArray& qb);
};

/**
 * Stores runtime data of set element
 */
class SetData : public ElementRuntime {
    Q_OBJECT
public:
    KDE_NO_CDTOR_EXPORT SetData (ElementPtr e) : ElementRuntime (e) {}
    KDE_NO_CDTOR_EXPORT ~SetData () {}
protected slots:
    /**
     * start_timer timer expired, execute it
     */
    virtual void started ();
    /**
     * undo set execute
     */
    virtual void stopped ();
private:
    ElementPtrW target_element;
    RegionNodePtrW target_region;
    QString changed_attribute;
    QString old_value;
};

//-----------------------------------------------------------------------------

namespace SMIL {

class Head : public Element {
public:
    KDE_NO_CDTOR_EXPORT Head (ElementPtr & d) : Element (d) {}
    ElementPtr childFromTag (const QString & tag);
    KDE_NO_EXPORT const char * nodeName () const { return "head"; }
    bool expose ();
};

class Layout : public Element {
public:
    KDE_NO_CDTOR_EXPORT Layout (ElementPtr & d) : Element (d) {}
    ElementPtr childFromTag (const QString & tag);
    KDE_NO_EXPORT const char * nodeName () const { return "layout"; }
    void closed ();
    RegionNodePtr rootLayout;
};

class RegionBase : public Element {
protected:
    KDE_NO_CDTOR_EXPORT RegionBase (ElementPtr & d) : Element (d) {}
public:
    int x, y, w, h;
};

class Region : public RegionBase {
public:
    KDE_NO_CDTOR_EXPORT Region (ElementPtr & d) : RegionBase (d) {}
    KDE_NO_EXPORT const char * nodeName () const { return "region"; }
    ElementPtr childFromTag (const QString & tag);
};

class RootLayout : public RegionBase {
public:
    KDE_NO_CDTOR_EXPORT RootLayout (ElementPtr & d) : RegionBase (d) {}
    KDE_NO_EXPORT const char * nodeName () const { return "root-layout"; }
};

/**
 * Abstract base for all SMIL element having begin/dur/end/.. attributes
 */
class TimedElement : public Mrl {
public:
    KDE_NO_CDTOR_EXPORT ~TimedElement () {}
    ElementRuntimePtr getRuntime ();
    void start ();
    void stop ();
    void reset ();
protected:
    KDE_NO_CDTOR_EXPORT TimedElement (ElementPtr & d) : Mrl (d) {}
    ElementRuntimePtr runtime;
    virtual ElementRuntimePtr getNewRuntime () = 0;
};

/**
 * Abstract base for the group elements (par/seq/excl/..)
 */
class GroupBase : public TimedElement {
public:
    KDE_NO_CDTOR_EXPORT ~GroupBase () {}
    bool isMrl ();
protected:
    KDE_NO_CDTOR_EXPORT GroupBase (ElementPtr & d) : TimedElement (d) {}
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
    void start ();
    void stop ();
    void reset ();
    void childDone (ElementPtr child);
};

class Seq : public GroupBase {
public:
    KDE_NO_CDTOR_EXPORT Seq (ElementPtr & d) : GroupBase (d) {}
    ElementPtr childFromTag (const QString & tag);
    KDE_NO_EXPORT const char * nodeName () const { return "seq"; }
    void start ();
};

class Body : public Seq {
public:
    KDE_NO_CDTOR_EXPORT Body (ElementPtr & d) : Seq (d) {}
    ElementPtr childFromTag (const QString & tag);
    KDE_NO_EXPORT const char * nodeName () const { return "body"; }
};

class Switch : public Element {
public:
    KDE_NO_CDTOR_EXPORT Switch (ElementPtr & d) : Element (d) {}
    ElementPtr childFromTag (const QString & tag);
    KDE_NO_EXPORT const char * nodeName () const { return "switch"; }
    // Condition
    void start ();
    void stop ();
    void reset ();
    void childDone (ElementPtr child);
};

/**
 * Abstract base for the MediaType classes (video/audio/text/img/..)
 */
class MediaType : public TimedElement {
public:
    MediaType (ElementPtr & d, const QString & t);
    ElementPtr childFromTag (const QString & tag);
    KDE_NO_EXPORT const char * nodeName () const { return m_type.latin1 (); }
    void opened ();
    QString m_type;
    unsigned int bitrate;
};

class AVMediaType : public MediaType {
public:
    AVMediaType (ElementPtr & d, const QString & t);
    ElementRuntimePtr getNewRuntime ();
    void start ();
    void stop ();
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

class Set : public Element {
public:
    KDE_NO_CDTOR_EXPORT Set (ElementPtr & d) : Element (d) {}
    KDE_NO_EXPORT const char * nodeName () const { return "set"; }
    ElementRuntimePtr getRuntime ();
    void start ();
    void stop ();
    ElementRuntimePtr runtime;
};

} // SMIL namespace

}  // KMPlayer namespace

#endif //_KMPLAYER_SMIL_H_
