/* This file is part of the KDE project
 *
 * Copyright (C) 2004 Koos Vriezen <koos.vriezen@xs4all.nl>
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
 *
 * until boost gets common, a more or less compatable one ..
 */

#ifndef _KMPLAYER_PLAYLIST_H_
#define _KMPLAYER_PLAYLIST_H_

#include <qstring.h>
#ifndef ASSERT
#define ASSERT Q_ASSERT
#endif

#include <kdemacros.h>

#undef KDE_NO_CDTOR_EXPORT
#undef KDE_NO_EXPORT
#ifndef KDE_EXPORT
  #define KDE_EXPORT
#endif
#if __GNUC__ - 0 > 3 || (__GNUC__ - 0 == 3 && __GNUC_MINOR__ - 0 > 3)
  #define KDE_NO_CDTOR_EXPORT __attribute__ ((visibility("hidden")))
  #define KDE_NO_EXPORT __attribute__ ((visibility("hidden")))
  #define KMPLAYER_EXPORT KDE_EXPORT
#elif __GNUC__ - 0 > 3 || (__GNUC__ - 0 == 3 && __GNUC_MINOR__ - 0 > 2)
  #define KDE_NO_CDTOR_EXPORT
  #define KDE_NO_EXPORT __attribute__ ((visibility("hidden")))
  #define KMPLAYER_EXPORT
#else
  #define KDE_NO_CDTOR_EXPORT
  #define KDE_NO_EXPORT
  #define KMPLAYER_EXPORT
#endif

#include "kmplayershared.h"

class QXmlAttributes;
class QTextStream;

namespace KMPlayer {

class Document;
class Element;
class Mrl;

typedef SharedPtr<Element> ElementPtr;
typedef WeakPtr<Element> ElementPtrW;

class KMPLAYER_EXPORT Element {
public:
    virtual ~Element ();
    Document * document ();
    /**
     * Return a dynamic cast to a Mrl pointer
     * \sa isMrl()
     */
    Mrl * mrl ();
    const Mrl * mrl () const;
    virtual ElementPtr childFromTag (const QString & tag);
    void characterData (const QString & s);
    QString innerText () const;
    QString innerXML () const;
    virtual void setAttributes (const QXmlAttributes &);
    virtual const char * nodeName () const;
    /**
     * If this is a derived Mrl object and has a SRC attribute
     */
    virtual bool isMrl ();
    /**
     * If this node should be visible to the user
     */
    virtual bool expose ();
    void appendChild (ElementPtr c);
    void insertBefore (ElementPtr c, ElementPtr b);
    void removeChild (ElementPtr c);
    void replaceChild (ElementPtr _new, ElementPtr old);
    /*
     * Get rid of whitespace only text nodes
     */
    void normalize ();
    /*
     * Close tag is found by parser
     */
    virtual void closed ();
    KDE_NO_EXPORT bool isDocument () const { return m_doc == m_self; }
    KDE_NO_EXPORT bool hasChildNodes () const { return m_first_child != 0L; }
    KDE_NO_EXPORT ElementPtr parentNode () const { return m_parent; }
    KDE_NO_EXPORT ElementPtr firstChild () const { return m_first_child; }
    KDE_NO_EXPORT ElementPtr lastChild () const { return m_last_child; }
    KDE_NO_EXPORT ElementPtr nextSibling () const { return m_next; }
    KDE_NO_EXPORT ElementPtr previousSibling () const { return m_prev; }
    /**
     * If not assigned to a Shared pointer, this will result in self destruction
     */
    KDE_NO_EXPORT ElementPtr self () const { return m_self; }
protected:
    KDE_NO_CDTOR_EXPORT Element (ElementPtr d) : m_doc (d), m_self (this) {}
    KDE_NO_CDTOR_EXPORT Element () {} // for Document
    void clear ();
    ElementPtr m_doc;
    ElementPtrW m_parent;
    ElementPtr m_next;
    ElementPtrW m_prev;
    ElementPtr m_first_child;
    ElementPtrW m_last_child;
    ElementPtrW m_self;
};

class KMPLAYER_EXPORT Mrl : public Element {
protected:
    Mrl (ElementPtr d);
    Mrl (); // for Document
    ElementPtr childFromTag (const QString & tag);
    unsigned int cached_ismrl_version;
    bool cached_ismrl;
public:
    ~Mrl ();
    bool isMrl ();
    /*
     * If this Mrl hides a child Mrl, return that one or else this one 
     */ 
    virtual ElementPtr realMrl ();
    QString src;
    QString pretty_name;
    QString mimetype;
    bool parsed;
};

class KMPLAYER_EXPORT Document : public Mrl {
public:
    Document (const QString &);
    ~Document ();
    /** All nodes have shared pointers to Document,
     * so explicitly dispose it (calls clean and set m_doc to 0L)
     * */
    void dispose ();
    ElementPtr childFromTag (const QString & tag);
    KDE_NO_EXPORT const char * nodeName () const { return "document"; }
    /**
     * Will return false if this document has child nodes
     */
    bool isMrl ();
    unsigned int m_tree_version;
};

class KMPLAYER_EXPORT TextNode : public Element {
public:
    TextNode (ElementPtr d, const QString & s);
    KDE_NO_CDTOR_EXPORT ~TextNode () {}
    void appendText (const QString & s);
    KDE_NO_EXPORT const char * nodeName () const { return "#text"; }
    QString text;
};

class Title : public Element {
public:
    Title (ElementPtr d);
    KDE_NO_CDTOR_EXPORT ~Title () {}
    KDE_NO_EXPORT const char * nodeName () const { return "title"; }
    virtual bool expose ();
};

//-----------------------------------------------------------------------------

class Smil : public Element {
public:
    KDE_NO_CDTOR_EXPORT Smil (ElementPtr d) : Element (d) {}
    ElementPtr childFromTag (const QString & tag);
    KDE_NO_EXPORT const char * nodeName () const { return "smil"; }
};

class Body : public Element {
public:
    KDE_NO_CDTOR_EXPORT Body (ElementPtr d) : Element (d) {}
    ElementPtr childFromTag (const QString & tag);
    KDE_NO_EXPORT const char * nodeName () const { return "body"; }
};

class Par : public Element {
public:
    KDE_NO_CDTOR_EXPORT Par (ElementPtr d) : Element (d) {}
    ElementPtr childFromTag (const QString & tag);
    KDE_NO_EXPORT const char * nodeName () const { return "par"; }
};

class Seq : public Element {
public:
    KDE_NO_CDTOR_EXPORT Seq (ElementPtr d) : Element (d) {}
    ElementPtr childFromTag (const QString & tag);
    KDE_NO_EXPORT const char * nodeName () const { return "seq"; }
};

class Switch : public Mrl {
public:
    KDE_NO_CDTOR_EXPORT Switch (ElementPtr d) : Mrl (d) {}
    ElementPtr childFromTag (const QString & tag);
    KDE_NO_EXPORT const char * nodeName () const { return "switch"; }
    /**
     * Will return false if no children or none has a condition that evals true
     */
    bool isMrl ();
    // Condition
};

class MediaType : public Mrl {
public:
    MediaType (ElementPtr d, const QString & t);
    ElementPtr childFromTag (const QString & tag);
    KDE_NO_EXPORT const char * nodeName () const { return m_type.latin1 (); }
    void setAttributes (const QXmlAttributes &);
    QString m_type;
    int bitrate;
};

//-----------------------------------------------------------------------------

class Asx : public Mrl {
public:
    KDE_NO_CDTOR_EXPORT Asx (ElementPtr d) : Mrl (d) {}
    ElementPtr childFromTag (const QString & tag);
    KDE_NO_EXPORT const char * nodeName () const { return "ASX"; }
    /**
     * True if no mrl children
     */
    bool isMrl ();
};

class Entry : public Mrl {
public:
    KDE_NO_CDTOR_EXPORT Entry (ElementPtr d) : Mrl (d) {}
    ElementPtr childFromTag (const QString & tag);
    KDE_NO_EXPORT const char * nodeName () const { return "Entry"; }
    /**
     * True if has a Ref child
     */
    bool isMrl ();
    /**
     * Returns Ref child if isMrl() return true
     */
    virtual ElementPtr realMrl ();
};

class Ref : public Mrl {
public:
    KDE_NO_CDTOR_EXPORT Ref (ElementPtr d) : Mrl (d) {}
    //ElementPtr childFromTag (const QString & tag);
    void setAttributes (const QXmlAttributes &);
    KDE_NO_EXPORT const char * nodeName () const { return "Ref"; }
};

class EntryRef : public Mrl {
public:
    KDE_NO_CDTOR_EXPORT EntryRef (ElementPtr d) : Mrl (d) {}
    //ElementPtr childFromTag (const QString & tag);
    void setAttributes (const QXmlAttributes &);
    KDE_NO_EXPORT const char * nodeName () const { return "EntryRef"; }
};

//-----------------------------------------------------------------------------

class KMPLAYER_EXPORT GenericURL : public Mrl { //just some url, can get a SMIL or ASX childtree
public:
    KDE_NO_CDTOR_EXPORT GenericURL (ElementPtr d) : Mrl (d) {}
    GenericURL(ElementPtr d, const QString &s, const QString &n=QString::null);
    //void setAttributes (const QXmlAttributes &);
    KDE_NO_EXPORT const char * nodeName () const { return "GenericURL"; }
    /**
     * Will return false if this document has child nodes
     */
    bool isMrl ();
};

void readXML (ElementPtr root, QTextStream & in, const QString & firstline);

}  // namespace

#endif //_KMPLAYER_PLAYLIST_H_
