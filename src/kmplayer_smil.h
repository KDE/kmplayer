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

/**
 * Live representation of a SMIL MediaType element
 */
class ElementRuntime : public QObject {
    Q_OBJECT
public:
    virtual ~ElementRuntime ();
    /**
     * start, or restart in case of re-use, the durations
     */
    void begin ();
    /**
     * forced killing of timers
     */
    void end ();
    virtual void paint (QPainter &) {}
    /**
     * If this element is attached to a region, region_node points to it
     */
    RegionNodePtrW region_node;
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
    /**
     * start_timer timer expired
     */
    virtual void started ();
protected:
    ElementRuntime (ElementPtr & e);
    ElementPtrW media_element;
    int start_timer;
    int dur_timer;
    bool isstarted;
};

class AudioVideoData : public ElementRuntime {
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

class ImageData : public ElementRuntime {
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

class TextData : public ElementRuntime {
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
 * A Par represends parallel processing of all its children
 */
class Par : public Element {
public:
    KDE_NO_CDTOR_EXPORT Par (ElementPtr & d) : Element (d) {}
    ElementPtr childFromTag (const QString & tag);
    KDE_NO_EXPORT const char * nodeName () const { return "par"; }
    void start ();
    void stop ();
    void reset ();
    void childDone (ElementPtr child);
};

class Seq : public Element {
public:
    KDE_NO_CDTOR_EXPORT Seq (ElementPtr & d) : Element (d) {}
    ElementPtr childFromTag (const QString & tag);
    KDE_NO_EXPORT const char * nodeName () const { return "seq"; }
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
class MediaType : public Mrl {
public:
    MediaType (ElementPtr & d, const QString & t);
    ElementPtr childFromTag (const QString & tag);
    KDE_NO_EXPORT const char * nodeName () const { return m_type.latin1 (); }
    ElementRuntimePtr getRuntime ();
    void opened ();
    void start ();
    void reset ();
    /**
     * Called from the ElementRuntime when 'start' attribute expires
     * so audio/video clip should start
     */
    void timed_start ();
    /**
     * Called from the ElementRuntime when 'dur' (or 'end' - 'start' 
     * attribute expires to mark us finished
     */
    void timed_end ();
    virtual ElementRuntimePtr getNewRuntime () = 0;
    RegionNodePtrW region;
    ElementRuntimePtr runtime;
    QString m_type;
    unsigned int bitrate;
    unsigned int begin_time;
    unsigned int end_time;
    unsigned int duration_time;
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

} // SMIL namespace

}  // KMPlayer namespace

#endif //_KMPLAYER_SMIL_H_
