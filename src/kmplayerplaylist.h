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

#include <config.h>
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
class ElementRuntimePrivate;
class ImageDataPrivate;
class NodeList;

typedef SharedPtr<Element> ElementPtr;
typedef WeakPtr<Element> ElementPtrW;
typedef SharedPtr<RegionNode> RegionNodePtr;
typedef WeakPtr<RegionNode> RegionNodePtrW;
typedef SharedPtr<ElementRuntime> ElementRuntimePtr;

/*
 * Base class of all tree nodes. Provides a w3c's DOM like API
 */
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
     * Start element, sets state to state_started. Will call start() on
     * firstChild or call stop().
     */
    virtual void start ();
    /**
     * Defers a started, so possible playlists items can be added.
     */
    virtual void defer ();
    /**
     * Puts a defered element in started again, may call start() again if 
     * child elements were added.
     */
    virtual void undefer ();
    /**
     * Stops element, sets state to state_finished. Calls stop() on 
     * started/defered children. Notifies parent with a childDone call.
     */
    virtual void stop ();
    /**
     * Resets element, calls stop() if state is state_started and sets
     * state to state_init.
     */
    virtual void reset ();
    /**
     * Notification from child that it's finished. Call start() on nexSibling
     * or stop() if there is none.
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
    KDE_NO_EXPORT NodeList attributes () const;
    KDE_NO_EXPORT NodeList childNodes () const;
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

/**
 * Node for layout hierarchy as found in SMIL document
 */
class RegionNode {
public:
    RegionNode (ElementPtr e);
    KDE_NO_CDTOR_EXPORT ~RegionNode () {}
    /**
     * paints background if background-color attr. is set and afterwards passes
     * the painter of attached_element's runtime
     */
    void paint (QPainter & p, int x, int y, int h, int w);
    /**
     * repaints region, calls update(x,y,w,h) on view
     */
    void repaint ();
    /**
     * calculate bounds given the outer coordinates (absolute)
     */
    void setSize (int x, int y, int w, int h, bool keep_aspect);
    /**
     * calculate bounds given scale factors and offset (absolute) and
     * x,y,w,h element's values
     */
    void scaleRegion (float sx, float sy, int xoff, int yoff);
    /**
     * calculate the relative x,y,w,h on the child region elements
     * given this element's w and h value
     * and child's left/top/right/width/height/bottom attributes
     */
    void calculateChildBounds ();
    /**
     * user clicked w/ the mouse on this region, returns true if handled
     */
    bool pointerClicked (int x, int y);
    /**
     * user moved the mouse over this region
     */
    bool pointerMoved (int x, int y);
    /**
     * boolean for check if pointerEntered/pointerLeft should be called by View
     */
    bool has_mouse;
    /**
     * (Scaled) Dimensions set by setSize
     */
    int x, y, w, h;
    /**
     * Scale factors
     */
    float xscale, yscale;
    /**
     * z-order of this region
     */
    int z_order;
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

    RegionNodePtrW self;
    RegionNodePtr nextSibling;
    RegionNodePtr firstChild;
};

/**
 * Base class representing live time of elements
 */
class ElementRuntime {
public:
    virtual ~ElementRuntime ();
    /**
     * calls reset() and pulls in the attached_element's attributes
     */
    virtual void init ();
    /**
     * Called when element is pulled in scope, from start()
     */
    virtual void begin () {}
    /**
     * Called when element gets out of scope, from reset()
     */
    virtual void end () {}
    /**
     * Reset all data, called from end() and init()
     */
    virtual void reset ();
    virtual void paint (QPainter &) {}
    /**
     * change behaviour of this runtime, returns old value
     */
    virtual QString setParam (const QString & name, const QString & value);
    /**
     * get the current value of param name that's set by setParam(name,value)
     */
    virtual QString param (const QString & name);
    /**
     * If this element is attached to a region, region_node points to it
     */
    RegionNodePtrW region_node;
protected:
    ElementRuntime (ElementPtr e);
    ElementPtrW element;
private:
    ElementRuntimePrivate * d;
};

/**
 * Base class for Region and RootLayout
 */
class RegionBase : public Element {
protected:
    KDE_NO_CDTOR_EXPORT RegionBase (ElementPtr & d) : Element (d) {}
    ElementRuntimePtr runtime;
public:
    virtual ElementRuntimePtr getRuntime ();
    /**
     * recursively calculates dimensions of this and child regions
     */
    void updateLayout ();
    int x, y, w, h;
};

/**
 * Element wrapper to give it list functionality
 */
class KMPLAYER_EXPORT NodeList {
    ElementPtrW first_element;
public:
    NodeList (ElementPtr e) : first_element (e) {}
    ~NodeList () {}
    int length ();
    ElementPtr item (int i) const;
};

template <class T>
inline KDE_NO_EXPORT T * convertNode (ElementPtr e) {
    return static_cast <T *> (e.ptr ());
}
        
/**
 * Element's attributes
 */
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

/**
 * Element representing a playable link, like URL to a movie or playlist.
 */
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
    /*
     * Reimplement to callback with requestPlayURL if isMrl()
     */ 
    virtual void start ();
    QString src;
    QString pretty_name;
    QString mimetype;
    int width;
    int height;
    float aspect;
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
     * Some rectangle needs repainting
     */
    virtual void repaintRect (int x, int y, int w, int h) = 0;
    /**
     * Sets the video widget postion and background color if bg not NULL
     */
    virtual void avWidgetSizes (RegionNode * region, unsigned int * bg) = 0;
};

/**
 * The root of the DOM tree
 */
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

/**
 * Represents XML text, like "some text" in '<foo>some text</foo>'
 */
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

/**
 * Unrecognized tag by parent element
 */
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

/**
 * Title element as found in ASX
 */
class Title : public DarkNode {
public:
    Title (ElementPtr & d);
    KDE_NO_CDTOR_EXPORT ~Title () {}
};

//-----------------------------------------------------------------------------

/**
 * '<smil'> tag
 */
class Smil : public Mrl {
public:
    KDE_NO_CDTOR_EXPORT Smil (ElementPtr & d) : Mrl (d) {}
    ElementPtr childFromTag (const QString & tag);
    KDE_NO_EXPORT const char * nodeName () const { return "smil"; }
    bool isMrl ();
    void start ();
    void stop ();
    /**
     * Hack to mark the currently playing MediaType as finished
     * FIXME: think of a descent callback way for this
     */
    ElementPtr realMrl ();
    ElementPtr current_av_media_type;
};

//-----------------------------------------------------------------------------

namespace ASX {

/**
 * '<ASX>' tag
 */
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

/**
 * Entry tag as found in ASX for playlist item
 */
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

/**
 * Ref tag as found in ASX for URL item in playlist item
 */
class Ref : public Mrl {
public:
    KDE_NO_CDTOR_EXPORT Ref (ElementPtr & d) : Mrl (d) {}
    //ElementPtr childFromTag (const QString & tag);
    void opened ();
    KDE_NO_EXPORT const char * nodeName () const { return "Ref"; }
};

/**
 * EntryRef tag as found in ASX for shortcut of Entry plus Ref playlist item
 */
class EntryRef : public Mrl {
public:
    KDE_NO_CDTOR_EXPORT EntryRef (ElementPtr & d) : Mrl (d) {}
    //ElementPtr childFromTag (const QString & tag);
    void opened ();
    KDE_NO_EXPORT const char * nodeName () const { return "EntryRef"; }
};

} // ASX namespace

//-----------------------------------------------------------------------------

/**
 * just some url, can get a SMIL or ASX childtree
 */
class KMPLAYER_EXPORT GenericURL : public Mrl { 
public:
    GenericURL(ElementPtr &d, const QString &s, const QString &n=QString::null);
    KDE_NO_EXPORT const char * nodeName () const { return "GenericURL"; }
};

/**
 * Non url mrl
 */
class KMPLAYER_EXPORT GenericMrl : public Mrl { 
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

inline KDE_NO_EXPORT NodeList Element::attributes () const { return m_first_attribute; }
inline KDE_NO_EXPORT NodeList Element::childNodes () const { return m_first_child; }

}  // KMPlayer namespace

#endif //_KMPLAYER_PLAYLIST_H_
