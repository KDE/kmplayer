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
class Node;
class Mrl;
class RegionNode;
class ElementRuntime;
class ElementRuntimePrivate;
class ImageDataPrivate;

/*
 * Base class for objects that will be used as SharedPtr/WeakPtr pointers.
 * Item<T> keeps its own copy of the shared SharedData<T> as a weak refence.
 * \sa: self()
 */
template <class T>
class KMPLAYER_EXPORT Item {
public:
    typedef SharedPtr <T> SharedType;
    typedef WeakPtr <T> WeakType;

    virtual ~Item () {}

    virtual const char * nodeName () const { return "#item"; }
    virtual QString nodeValue () const { return QString (); }

    SharedType self () const { return m_self; }
protected:
    Item ();
    WeakType m_self;
};

/*
 * A double linked list of ListNode<T> or TreeNode<T> nodes
 */
template <class T>
class KMPLAYER_EXPORT List {
public:
    List () {}
    List (typename Item<T>::SharedType f, typename Item<T>::SharedType l) 
        : m_first (f), m_last (l) {}

    typename Item<T>::SharedType first () const { return m_first; }
    typename Item<T>::SharedType last () const { return m_last; }
    void append (typename KMPlayer::Item<T>::SharedType c);
    unsigned int length () const;
    typename Item<T>::SharedType item (int i) const;
protected:
    typename Item<T>::SharedType m_first;
    typename Item<T>::WeakType m_last;
};

/*
 * Base class for double linked list nodes of SharedPtr/WeakPtr objects.
 * The linkage is a shared nextSibling and a weak previousSibling.
 */
template <class T>
class KMPLAYER_EXPORT ListNode : public Item <T> {
    friend class List<T>;
public:
    virtual ~ListNode () {}

    typename Item<T>::SharedType nextSibling () const { return m_next; }
    typename Item<T>::SharedType previousSibling () const { return m_prev; }
protected:
    ListNode () {}
    typename Item<T>::SharedType m_next;
    typename Item<T>::WeakType m_prev;
};

/*
 * Base class for double linked tree nodes having parent/siblings/children.
 * The linkage is a shared firstChild and weak parentNode.
 */
template <class T>
class KMPLAYER_EXPORT TreeNode : public ListNode <T> {
public:
    virtual ~TreeNode () {}

    virtual void appendChild (typename KMPlayer::Item<T>::SharedType c);

    bool hasChildNodes () const { return m_first_child != 0L; }
    typename Item<T>::SharedType parentNode () const { return m_parent; }
    typename Item<T>::SharedType firstChild () const { return m_first_child; }
    typename Item<T>::SharedType lastChild () const { return m_last_child; }

protected:
    TreeNode () {}
    typename Item<T>::WeakType m_parent;
    typename Item<T>::SharedType m_first_child;
    typename Item<T>::WeakType m_last_child;
};

/**
 * Attribute having a name/value pair for use with Elements
 */
class KMPLAYER_EXPORT Attribute : public ListNode <Attribute> {
    friend class Element;
public:
    KDE_NO_CDTOR_EXPORT Attribute () {}
    Attribute (const QString & n, const QString & v);
    KDE_NO_CDTOR_EXPORT ~Attribute () {}
    const char * nodeName () const;
    QString nodeValue () const;
protected:
    QString name;
    QString value;
};

// convenient types
typedef Item<Node>::SharedType NodePtr;
typedef Item<Node>::WeakType NodePtrW;
typedef Item<Attribute>::SharedType AttributePtr;
typedef Item<Attribute>::WeakType AttributePtrW;
typedef Item<RegionNode>::SharedType RegionNodePtr;
typedef Item<RegionNode>::WeakType RegionNodePtrW;
typedef SharedPtr<ElementRuntime> ElementRuntimePtr;

/*
 * Base class for XML nodes. Provides a w3c's DOM like API
 */
class KMPLAYER_EXPORT Node : public TreeNode <Node> {
    friend class DocumentBuilder;
public:
    enum State {
        state_init, state_deferred, state_activated, state_deactivated
    };
    virtual ~Node ();
    Document * document ();
    /**
     * Return a dynamic cast to a Mrl pointer
     * \sa isMrl()
     */
    Mrl * mrl ();
    const Mrl * mrl () const;
    virtual NodePtr childFromTag (const QString & tag);
    void characterData (const QString & s);
    QString innerText () const;
    QString innerXML () const;
    QString outerXML () const;
    virtual const char * nodeName () const;
    virtual QString nodeValue () const;
    /**
     * If this is a derived Mrl object and has a SRC attribute
     */
    virtual bool isMrl ();
    virtual bool isElementNode () { return false; }
    /**
     * If this node should be visible to the user
     */
    virtual bool expose ();
    /**
     * Activates element, sets state to state_activated. Will call activate() on
     * firstChild or call deactivate().
     */
    virtual void activate ();
    /**
     * Defers an activated, so possible playlists items can be added.
     */
    virtual void defer ();
    /**
     * Puts a defered element in activated again, may call activate() again if 
     * child elements were added.
     */
    virtual void undefer ();
    /**
     * Stops element, sets state to state_deactivated. Calls deactivate() on 
     * activated/defered children. Notifies parent with a childDone call.
     */
    virtual void deactivate ();
    /**
     * Resets element, calls deactivate() if state is state_activated and sets
     * state to state_init.
     */
    virtual void reset ();
    /**
     * Notification from child that it's finished. Call activate() on nexSibling
     * or deactivate() if there is none.
     */
    virtual void childDone (NodePtr child);
    /**
     * Creates a new ElementRuntime object
     */
    virtual ElementRuntimePtr getRuntime ();
    virtual void clear ();
    void appendChild (NodePtr c);
    void insertBefore (NodePtr c, NodePtr b);
    void removeChild (NodePtr c);
    void replaceChild (NodePtr _new, NodePtr old);
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

    KDE_NO_EXPORT List <Node> childNodes () const;
protected:
    Node (NodePtr & d);
    NodePtr m_doc;
public:
    State state;
    void setState (State nstate);
private:
    unsigned int defer_tree_version;
};

/*
 * Element node, XML node that can have attributes
 */
class KMPLAYER_EXPORT Element : public Node {
    //friend class DocumentBuilder;
public:
    ~Element () {}
    void setAttributes (List <Attribute> attrs);
    void setAttribute (const QString & name, const QString & value);
    QString getAttribute (const QString & name);
    KDE_NO_EXPORT List<Attribute> attributes () const { return m_attributes; }
    virtual void clear ();
    virtual bool isElementNode () { return true; }
protected:
    Element (NodePtr & d);
    List<Attribute> m_attributes;
};

/**
 * Node for layout hierarchy as found in SMIL document
 */
class RegionNode : public TreeNode <RegionNode> {
public:
    RegionNode (NodePtr e);
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
    NodePtrW regionElement;
    /**
     * Attached Element for this region (only one max. ATM)
     */
    NodePtrW attached_element;
    /**
     * Make this region and its sibling 0
     */
    void clearAll ();
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
     * Called when element is pulled in scope, from Node::activate()
     */
    virtual void begin () {}
    /**
     * Called when element gets out of scope, from Node::reset()
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
    ElementRuntime (NodePtr e);
    NodePtrW element;
private:
    ElementRuntimePrivate * d;
};

/**
 * Base class for Region and RootLayout
 */
class RegionBase : public Element {
protected:
    KDE_NO_CDTOR_EXPORT RegionBase (NodePtr & d) : Element (d) {}
    ElementRuntimePtr runtime;
public:
    virtual ElementRuntimePtr getRuntime ();
    /**
     * recursively calculates dimensions of this and child regions
     */
    void updateLayout ();
    int x, y, w, h;
};

template <class T>
inline KDE_NO_EXPORT T * convertNode (NodePtr e) {
    return static_cast <T *> (e.ptr ());
}
        
/**
 * Element representing a playable link, like URL to a movie or playlist.
 */
class KMPLAYER_EXPORT Mrl : public Element {
protected:
    Mrl (NodePtr & d);
    NodePtr childFromTag (const QString & tag);
    unsigned int cached_ismrl_version;
    bool cached_ismrl;
public:
    ~Mrl ();
    bool isMrl ();
    /*
     * If this Mrl hides a child Mrl, return that one or else this one 
     */ 
    virtual NodePtr realMrl ();
    /*
     * Reimplement to callback with requestPlayURL if isMrl()
     */ 
    virtual void activate ();
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
    virtual bool requestPlayURL (NodePtr mrl, RegionNodePtr region) = 0;
    /**
     * Element has activated or deactivated notification
     */
    virtual void stateElementChanged (NodePtr element) = 0;
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
    NodePtr getElementById (const QString & id);
    /** All nodes have shared pointers to Document,
     * so explicitly dispose it (calls clean and set m_doc to 0L)
     * */
    void dispose ();
    NodePtr childFromTag (const QString & tag);
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
class KMPLAYER_EXPORT TextNode : public Node {
public:
    TextNode (NodePtr & d, const QString & s);
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
    DarkNode (NodePtr & d, const QString & n);
    KDE_NO_CDTOR_EXPORT ~DarkNode () {}
    const char * nodeName () const { return name.ascii (); }
    NodePtr childFromTag (const QString & tag);
    virtual bool expose ();
protected:
    QString name;
};

/**
 * Title element as found in ASX
 */
class Title : public DarkNode {
public:
    Title (NodePtr & d);
    KDE_NO_CDTOR_EXPORT ~Title () {}
};

//-----------------------------------------------------------------------------

/**
 * '<smil'> tag
 */
class Smil : public Mrl {
public:
    KDE_NO_CDTOR_EXPORT Smil (NodePtr & d) : Mrl (d) {}
    NodePtr childFromTag (const QString & tag);
    KDE_NO_EXPORT const char * nodeName () const { return "smil"; }
    bool isMrl ();
    void activate ();
    void deactivate ();
    /**
     * Hack to mark the currently playing MediaType as finished
     * FIXME: think of a descent callback way for this
     */
    NodePtr realMrl ();
    NodePtr current_av_media_type;
};

//-----------------------------------------------------------------------------

namespace ASX {

/**
 * '<ASX>' tag
 */
class Asx : public Mrl {
public:
    KDE_NO_CDTOR_EXPORT Asx (NodePtr & d) : Mrl (d) {}
    NodePtr childFromTag (const QString & tag);
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
    KDE_NO_CDTOR_EXPORT Entry (NodePtr & d) : Mrl (d) {}
    NodePtr childFromTag (const QString & tag);
    KDE_NO_EXPORT const char * nodeName () const { return "Entry"; }
    /**
     * True if has a Ref child
     */
    bool isMrl ();
    /**
     * Returns Ref child if isMrl() return true
     */
    virtual NodePtr realMrl ();
};

/**
 * Ref tag as found in ASX for URL item in playlist item
 */
class Ref : public Mrl {
public:
    KDE_NO_CDTOR_EXPORT Ref (NodePtr & d) : Mrl (d) {}
    //NodePtr childFromTag (const QString & tag);
    void opened ();
    KDE_NO_EXPORT const char * nodeName () const { return "Ref"; }
};

/**
 * EntryRef tag as found in ASX for shortcut of Entry plus Ref playlist item
 */
class EntryRef : public Mrl {
public:
    KDE_NO_CDTOR_EXPORT EntryRef (NodePtr & d) : Mrl (d) {}
    //NodePtr childFromTag (const QString & tag);
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
    GenericURL(NodePtr &d, const QString &s, const QString &n=QString::null);
    KDE_NO_EXPORT const char * nodeName () const { return "GenericURL"; }
};

/**
 * Non url mrl
 */
class KMPLAYER_EXPORT GenericMrl : public Mrl { 
public:
    KDE_NO_CDTOR_EXPORT GenericMrl (NodePtr & d) : Mrl (d) {}
    GenericMrl(NodePtr &d, const QString &s, const QString &n=QString::null);
    KDE_NO_EXPORT const char * nodeName () const { return "GenericMrl"; }
    /**
     * Will return false if this document has child nodes
     */
    bool isMrl ();
};

void readXML (NodePtr root, QTextStream & in, const QString & firstline);

template <class T> inline Item<T>::Item () : m_self (static_cast <T*> (this)) {}

template <class T> inline void List<T>::append(typename Item<T>::SharedType c) {
    if (!m_first) {
        m_first = m_last = c;
    } else {
        m_last->m_next = c;
        c->m_prev = m_last;
        m_last = c;
    }
}

template <class T> inline unsigned int List<T>::length () const {
    unsigned int count = 0;
    for (typename Item<T>::SharedType t = m_first; t; t->nextSibling ())
        count++;
    return count;
}

template <class T>
inline typename Item<T>::SharedType List<T>::item (int i) const {
    for (typename Item<T>::SharedType t = m_first; t; t = t->nextSibling(), --i)
        if (i == 0)
            return t;
    return Item<T>::SharedType ();
}

template <class T>
inline void TreeNode<T>::appendChild (typename Item<T>::SharedType c) {
    if (!m_first_child) {
        m_first_child = m_last_child = c;
    } else {
        m_last_child->m_next = c;
        c->m_prev = m_last_child;
        m_last_child = c;
    }
    c->m_parent = Item<T>::m_self;
}

inline KDE_NO_EXPORT List <Node> Node::childNodes () const {
    return List <Node> (m_first_child, m_last_child);
}

}  // KMPlayer namespace

#endif //_KMPLAYER_PLAYLIST_H_
