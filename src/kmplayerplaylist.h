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

class QTextStream;
class QPixmap;
class QPainter;

namespace KMPlayer {

class Document;
class Element;
class Mrl;
class RegionNode;
class ElementRuntime;
class ImageDataPrivate;

typedef SharedPtr<Element> ElementPtr;
typedef WeakPtr<Element> ElementPtrW;
typedef SharedPtr<RegionNode> RegionNodePtr;
typedef WeakPtr<RegionNode> RegionNodePtrW;
typedef SharedPtr<ElementRuntime> ElementRuntimePtr;


/**
 * Node for layout hierarchy as found in SMIL document
 */
class RegionNode {
public:
    RegionNode (ElementPtr e);
    KDE_NO_CDTOR_EXPORT ~RegionNode () {}
    /**
     * paints background if background-color set and afterwards passes
     * the painter of attached_element's runtime
     */
    void paint (QPainter & p);
    /**
     * user clicked w/ the mouse on this region
     */
    void pointerClicked ();
    /**
     * user entered w/ the mouse this region
     */
    void pointerEntered ();
    /**
     * user left w/ the mouse this region
     */
    void pointerLeft ();
    /**
     * boolean for check if pointerEntered/pointerLeft should be called by View
     */
    bool has_mouse;
    /**
     * (Scaled) Dimensions set by viewer
     */
    int x, y, w, h;
    /**
     * Cached color
     */
    int background_color;
    /**
     * Whether background_color is valid
     */
    bool have_color;
    /**
     * Corresponding DOM node (SMIL::Region or SMIL::RootLayout)
     */
    ElementPtrW regionElement;
    /**
     * Attached Element for this region (only one max. ATM)
     */
    ElementPtrW attached_element;
    /**
     * Make this region and its sibling 0
     */
    void clearAll ();

    RegionNodePtr nextSibling;
    RegionNodePtr firstChild;
};

class KMPLAYER_EXPORT NodeList {
    ElementPtrW first_element;
public:
    NodeList (ElementPtr e) : first_element (e) {}
    ~NodeList () {}
    int length ();
    ElementPtr item (int i) const;
};

class KMPLAYER_EXPORT Element {
    friend class DocumentBuilder;
public:
    enum State {
        state_init, state_deferred, state_started, state_finished
    };
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
    QString outerXML () const;
    void setAttributes (const NodeList & attrs);
    virtual const char * nodeName () const;
    virtual QString nodeValue () const;
    /**
     * If this is a derived Mrl object and has a SRC attribute
     */
    virtual bool isMrl ();
    /**
     * If this node should be visible to the user
     */
    virtual bool expose ();
    /**
     * Start element, sets started to true, can call requestPlayURL.
     */
    virtual void start ();
    /**
     * Defers a started, so possible playlists items can be added.
     */
    virtual void defer ();
    /**
     * Puts a defered element in started again, may call start again if 
     * child elements were added.
     */
    virtual void undefer ();
    /**
     * Stops element, sets started to false and finished to true.
     * Notifies parent with a childDone call
     */
    virtual void stop ();
    /**
     * Resets element, sets started to false and finished to false.
     */
    virtual void reset ();
    /**
     * Notification from child that it's finished.
     */
    virtual void childDone (ElementPtr child);
    /**
     * Creates a new ElementRuntime object
     */
    virtual ElementRuntimePtr getRuntime ();
    void clear ();
    void appendChild (ElementPtr c);
    void insertBefore (ElementPtr c, ElementPtr b);
    void removeChild (ElementPtr c);
    void replaceChild (ElementPtr _new, ElementPtr old);
    /*
     * Get rid of whitespace only text nodes
     */
    void normalize ();
    /*
     * Open tag is found by parser, attributes are set
     */
    virtual void opened ();
    /*
     * Close tag is found by parser, children are appended
     */
    virtual void closed ();
    KDE_NO_EXPORT bool isDocument () const { return m_doc == m_self; }
    KDE_NO_EXPORT bool hasChildNodes () const { return m_first_child != 0L; }
    KDE_NO_EXPORT ElementPtr parentNode () const { return m_parent; }
    KDE_NO_EXPORT ElementPtr firstChild () const { return m_first_child; }
    KDE_NO_EXPORT ElementPtr lastChild () const { return m_last_child; }
    KDE_NO_EXPORT ElementPtr nextSibling () const { return m_next; }
    KDE_NO_EXPORT ElementPtr previousSibling () const { return m_prev; }
    KDE_NO_EXPORT NodeList attributes () const { return m_first_attribute; }
    KDE_NO_EXPORT NodeList childNodes () const { return m_first_child; }
    void setAttribute (const QString & name, const QString & value);
    QString getAttribute (const QString & name);
    /**
     * If not assigned to a Shared pointer, this will result in self destruction
     */
    KDE_NO_EXPORT ElementPtr self () const { return m_self; }
protected:
    Element (ElementPtr & d);
    /**
     * Constructor that doesn't set the document and is for Document
     * only due to a problem with m_self and m_doc
     */
    Element ();
    ElementPtr m_doc;
    ElementPtrW m_parent;
    ElementPtr m_next;
    ElementPtrW m_prev;
    ElementPtr m_first_child;
    ElementPtrW m_last_child;
    ElementPtr m_first_attribute;
    ElementPtrW m_self;
public:
    State state;
    void setState (State nstate);
private:
    unsigned int defer_tree_version;
};

template <class T>
KDE_NO_EXPORT inline T * convertNode (ElementPtr e) {
    return static_cast <T *> (e.ptr ());
}
        
class KMPLAYER_EXPORT Attribute : public Element {
    friend class Element;
public:
    Attribute (ElementPtr & d, const QString & n, const QString & v);
    KDE_NO_CDTOR_EXPORT ~Attribute () {}
    const char * nodeName () const;
    QString nodeValue () const;
    bool expose ();
protected:
    QString name;
    QString value;
};

class KMPLAYER_EXPORT Mrl : public Element {
protected:
    Mrl (ElementPtr & d);
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
    virtual void start ();
    QString src;
    QString pretty_name;
    QString mimetype;
    bool parsed;
    bool bookmarkable;
};

/**
 * Document listener interface
 */
class KMPLAYER_EXPORT PlayListNotify {
public:
    virtual ~PlayListNotify () {}
    /**
     * Ask for playing a video/audio mrl inside region
     * If returning false, the element will be set to finished
     */
    virtual bool requestPlayURL (ElementPtr mrl, RegionNodePtr region) = 0;
    /**
     * Element has started or stopped notification
     */
    virtual void stateElementChanged (ElementPtr element) = 0;
    /**
     * Some region needs repainting, eg. a timer expired
     */
    virtual void repaintRegion (RegionNodePtr region) = 0;
};

class KMPLAYER_EXPORT Document : public Mrl {
public:
    Document (const QString &, PlayListNotify * notify = 0L);
    ~Document ();
    ElementPtr getElementById (const QString & id);
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
    RegionNodePtrW rootLayout;
    PlayListNotify * notify_listener;
    unsigned int m_tree_version;
};

class KMPLAYER_EXPORT TextNode : public Element {
public:
    TextNode (ElementPtr & d, const QString & s);
    KDE_NO_CDTOR_EXPORT ~TextNode () {}
    void appendText (const QString & s);
    const char * nodeName () const { return "#text"; }
    QString nodeValue () const;
    bool expose ();
protected:
    QString text;
};

class DarkNode : public Element {
public:
    DarkNode (ElementPtr & d, const QString & n);
    KDE_NO_CDTOR_EXPORT ~DarkNode () {}
    const char * nodeName () const { return name.ascii (); }
    ElementPtr childFromTag (const QString & tag);
    virtual bool expose ();
protected:
    QString name;
};

class Title : public DarkNode {
public:
    Title (ElementPtr & d);
    KDE_NO_CDTOR_EXPORT ~Title () {}
};

//-----------------------------------------------------------------------------

class Smil : public Mrl {
public:
    KDE_NO_CDTOR_EXPORT Smil (ElementPtr & d) : Mrl (d) {}
    ElementPtr childFromTag (const QString & tag);
    KDE_NO_EXPORT const char * nodeName () const { return "smil"; }
    bool isMrl ();
    void start ();
    void childDone (ElementPtr child);
    /**
     * Hack to mark the currently playing MediaType as finished
     * FIXME: think of a descent callback way for this
     */
    ElementPtr realMrl ();
    ElementPtr current_av_media_type;
};

//-----------------------------------------------------------------------------

namespace ASX {

class Asx : public Mrl {
public:
    KDE_NO_CDTOR_EXPORT Asx (ElementPtr & d) : Mrl (d) {}
    ElementPtr childFromTag (const QString & tag);
    KDE_NO_EXPORT const char * nodeName () const { return "ASX"; }
    /**
     * True if no mrl children
     */
    bool isMrl ();
};

class Entry : public Mrl {
public:
    KDE_NO_CDTOR_EXPORT Entry (ElementPtr & d) : Mrl (d) {}
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
    KDE_NO_CDTOR_EXPORT Ref (ElementPtr & d) : Mrl (d) {}
    //ElementPtr childFromTag (const QString & tag);
    void opened ();
    KDE_NO_EXPORT const char * nodeName () const { return "Ref"; }
};

class EntryRef : public Mrl {
public:
    KDE_NO_CDTOR_EXPORT EntryRef (ElementPtr & d) : Mrl (d) {}
    //ElementPtr childFromTag (const QString & tag);
    void opened ();
    KDE_NO_EXPORT const char * nodeName () const { return "EntryRef"; }
};

} // ASX namespace

//-----------------------------------------------------------------------------

class KMPLAYER_EXPORT GenericURL : public Mrl { //just some url, can get a SMIL or ASX childtree
public:
    GenericURL(ElementPtr &d, const QString &s, const QString &n=QString::null);
    KDE_NO_EXPORT const char * nodeName () const { return "GenericURL"; }
};

class KMPLAYER_EXPORT GenericMrl : public Mrl { // non url mrl
public:
    KDE_NO_CDTOR_EXPORT GenericMrl (ElementPtr & d) : Mrl (d) {}
    GenericMrl(ElementPtr &d, const QString &s, const QString &n=QString::null);
    KDE_NO_EXPORT const char * nodeName () const { return "GenericMrl"; }
    /**
     * Will return false if this document has child nodes
     */
    bool isMrl ();
};

void readXML (ElementPtr root, QTextStream & in, const QString & firstline);

}  // KMPlayer namespace

#endif //_KMPLAYER_PLAYLIST_H_
