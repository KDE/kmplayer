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

class AudioVideoData : public RegionData {
public:
    AudioVideoData (RegionNodePtr r, ElementPtr e);
    virtual bool isAudioVideo ();
    ElementPtrW av_element;
};

class ImageData : public QObject, public RegionData {
    Q_OBJECT
public:
    ImageData (RegionNodePtr r, ElementPtr e);
    ~ImageData ();
    void paint (QPainter & p);
    ElementPtrW image_element;
    QPixmap * image;
    ImageDataPrivate * d;
private slots:
    void slotResult (KIO::Job*);
    void slotData (KIO::Job*, const QByteArray& qb);
};

class TextData : public QObject, public RegionData {
    Q_OBJECT
public:
    TextData (RegionNodePtr r, ElementPtr e);
    ~TextData ();
    void paint (QPainter & p);
    ElementPtrW text_element;
    TextDataPrivate * d;
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

class MediaType : public Mrl {
public:
    MediaType (ElementPtr & d, const QString & t);
    ElementPtr childFromTag (const QString & tag);
    KDE_NO_EXPORT const char * nodeName () const { return m_type.latin1 (); }
    void opened ();
    void start ();
    void reset ();
    RegionNodePtrW region;
    QString m_type;
    int bitrate;
};

class AVMediaType : public MediaType {
public:
    AVMediaType (ElementPtr & d, const QString & t);
    RegionDataPtr getNewData (RegionNodePtr r);
    void start ();
    void stop ();
};

class ImageMediaType : public MediaType {
public:
    ImageMediaType (ElementPtr & d);
    RegionDataPtr getNewData (RegionNodePtr r);
    void start ();
    /**
     * cache the region data, so we load image only once
     */
    RegionDataPtr region_data;
};

class TextMediaType : public MediaType {
public:
    TextMediaType (ElementPtr & d);
    RegionDataPtr getNewData (RegionNodePtr r);
    void start ();
    /**
     * cache the region data, so we load image only once
     */
    RegionDataPtr region_data;
};

} // SMIL namespace

}  // KMPlayer namespace

#endif //_KMPLAYER_SMIL_H_
