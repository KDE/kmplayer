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
#define ASSERT Q_ASSERT
#include "kmplayershared.h"

#include <kdemacros.h>

#undef KDE_NO_CDTOR_EXPORT
#undef KDE_NO_EXPORT
#if __GNUC__ - 0 > 3 || (__GNUC__ - 0 == 3 && __GNUC_MINOR__ - 0 > 3)
  #define KDE_NO_CDTOR_EXPORT __attribute__ ((visibility("hidden")))
  #define KDE_NO_EXPORT __attribute__ ((visibility("hidden")))
#elif __GNUC__ - 0 > 3 || (__GNUC__ - 0 == 3 && __GNUC_MINOR__ - 0 > 2)
  #define KDE_NO_CDTOR_EXPORT
  #define KDE_NO_EXPORT __attribute__ ((visibility("hidden")))
#else
  #define KDE_NO_CDTOR_EXPORT
  #define KDE_NO_EXPORT
#endif

class QXmlAttributes;
class Document;
class Element;
class Mrl;

typedef SharedPtr<Element> ElementPtr;
typedef WeakPtr<Element> ElementPtrW;

class Element {
public:
    virtual ~Element ();
    Document * document ();
    /**
     * Return a cast to a Mrl pointer, make sure to check type first
     * \sa isMrl()
     */
    Mrl * mrl ();
    const Mrl * mrl () const;
    virtual ElementPtr childFromTag (ElementPtr doc, const QString & tag);
    virtual void setAttributes (const QXmlAttributes &);
    virtual const char * tagName () const;
    /**
     * If this is a derived Mrl object and has a SRC attribute
     */
    virtual bool isMrl ();
    void appendChild (ElementPtr c);
    void insertBefore (ElementPtr c, ElementPtr b);
    void removeChild (ElementPtr c);
    void replaceChild (ElementPtr _new, ElementPtr old);
    KDE_NO_EXPORT bool hasChildNodes () const { return (bool) m_first_child; }
    KDE_NO_EXPORT ElementPtr parentNode () { return m_parent; }
    KDE_NO_EXPORT ElementPtr firstChild () { return m_first_child; }
    KDE_NO_EXPORT ElementPtr lastChild () { return m_last_child; }
    KDE_NO_EXPORT ElementPtr nextSibling () { return m_next; }
    KDE_NO_EXPORT ElementPtr previousSibling () { return m_prev; }
    KDE_NO_EXPORT ElementPtr self () { return m_self; }
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

class Mrl : public Element {
protected:
    KDE_NO_CDTOR_EXPORT Mrl (ElementPtr d) : Element (d), parsed (false) {}
    KDE_NO_CDTOR_EXPORT Mrl () : parsed (false) {} // for Document
public:
    ~Mrl ();
    bool isMrl ();
    QString src;
    QString mimetype;
    bool parsed;
};

class Document : public Mrl {
public:
    Document (const QString &);
    ~Document ();
    /** All nodes have shared pointers to Document,
     * so explicitly dispose it (calls clean and set m_doc to 0L)
     * */
    void dispose ();
    ElementPtr childFromTag (ElementPtr d, const QString & tag);
    KDE_NO_EXPORT const char * tagName () const { return "document"; }
    /**
     * Will return false if this document has child nodes
     */
    bool isMrl ();
private:
    ElementPtr m_current;
};

//-----------------------------------------------------------------------------

class Smil : public Element {
public:
    KDE_NO_CDTOR_EXPORT Smil (ElementPtr d) : Element (d) {}
    ElementPtr childFromTag (ElementPtr d, const QString & tag);
    KDE_NO_EXPORT const char * tagName () const { return "smil"; }
};

class Body : public Element {
public:
    KDE_NO_CDTOR_EXPORT Body (ElementPtr d) : Element (d) {}
    ElementPtr childFromTag (ElementPtr d, const QString & tag);
    KDE_NO_EXPORT const char * tagName () const { return "body"; }
};

class Par : public Element {
public:
    KDE_NO_CDTOR_EXPORT Par (ElementPtr d) : Element (d) {}
    ElementPtr childFromTag (ElementPtr d, const QString & tag);
    KDE_NO_EXPORT const char * tagName () const { return "par"; }
};

class Seq : public Element {
public:
    KDE_NO_CDTOR_EXPORT Seq (ElementPtr d) : Element (d) {}
    ElementPtr childFromTag (ElementPtr d, const QString & tag);
    KDE_NO_EXPORT const char * tagName () const { return "seq"; }
};

class Switch : public Mrl {
public:
    KDE_NO_CDTOR_EXPORT Switch (ElementPtr d) : Mrl (d) {}
    ElementPtr childFromTag (ElementPtr d, const QString & tag);
    KDE_NO_EXPORT const char * tagName () const { return "switch"; }
    /**
     * Will return false if no children or none has a condition that evals true
     */
    bool isMrl ();
    // Condition
};

class MediaType : public Mrl {
public:
    MediaType (ElementPtr d, const QString & t);
    ElementPtr childFromTag (ElementPtr d, const QString & tag);
    KDE_NO_EXPORT const char * tagName () const { return m_type.latin1 (); }
    void setAttributes (const QXmlAttributes &);
    QString m_type;
    int bitrate;
};

//-----------------------------------------------------------------------------

class Asx : public Element {
public:
    KDE_NO_CDTOR_EXPORT Asx (ElementPtr d) : Element (d) {}
    ElementPtr childFromTag (ElementPtr d, const QString & tag);
    KDE_NO_EXPORT const char * tagName () const { return "ASX"; }
};

class Entry : public Element {
public:
    KDE_NO_CDTOR_EXPORT Entry (ElementPtr d) : Element (d) {}
    ElementPtr childFromTag (ElementPtr d, const QString & tag);
    KDE_NO_EXPORT const char * tagName () const { return "Entry"; }
};

class Ref : public Mrl {
public:
    KDE_NO_CDTOR_EXPORT Ref (ElementPtr d) : Mrl (d) {}
    //ElementPtr childFromTag (ElementPtr d, const QString & tag);
    void setAttributes (const QXmlAttributes &);
    KDE_NO_EXPORT const char * tagName () const { return "Ref"; }
};

class EntryRef : public Mrl {
public:
    KDE_NO_CDTOR_EXPORT EntryRef (ElementPtr d) : Mrl (d) {}
    //ElementPtr childFromTag (ElementPtr d, const QString & tag);
    void setAttributes (const QXmlAttributes &);
    KDE_NO_EXPORT const char * tagName () const { return "EntryRef"; }
};

//-----------------------------------------------------------------------------

class GenericURL : public Mrl { //just some url, can get a SMIL or ASX childtree
public:
    KDE_NO_CDTOR_EXPORT GenericURL (ElementPtr d) : Mrl (d) {}
    GenericURL (ElementPtr d, const QString & s);
    ElementPtr childFromTag (ElementPtr d, const QString & tag);
    //void setAttributes (const QXmlAttributes &);
    KDE_NO_EXPORT const char * tagName () const { return "GenericURL"; }
    /**
     * Will return false if this document has child nodes
     */
    bool isMrl ();
};

#endif //_KMPLAYER_PLAYLIST_H_
