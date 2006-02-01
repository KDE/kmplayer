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
 * the Free Software Foundation, Inc., 51 Franklin Steet, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 * until boost gets common, a more or less compatable one ..
 */

#ifndef _KMPLAYER_PLAYLIST_H_
#define _KMPLAYER_PLAYLIST_H_

#include <config.h>
#include <sys/time.h>

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
class Node;
class Mrl;
class ElementRuntime;

/*
 * Base class for objects that will be used as SharedPtr/WeakPtr pointers.
 * Item<T> keeps its own copy of the shared SharedData<T> as a weak refence.
 * \sa: self()
 */
template <class T>
class KMPLAYER_EXPORT Item {
    friend class SharedPtr<T>;
    friend class WeakPtr<T>;
public:
    typedef SharedPtr <T> SharedType;
    typedef WeakPtr <T> WeakType;

    virtual ~Item () {}

    SharedType self () const { return m_self; }
protected:
    Item ();
    WeakType m_self;
private:
    Item (const Item <T> &); // forbidden copy constructor
};

/**
 * Because of the m_self member of Item<T>, it's not allowed to assign a
 * Item<T>* directly to SharedPtr<Item<T>>. Item<T>* will then reside in
 * two independant SharedData<Item<T>> objects.
 * So specialize constructor and assignment operators to fetch the 
 * SharedData<Item<T>> from the Item<T>* instead of creating a new one
 */
#define ITEM_AS_POINTER(CLASS)                                         \
 template <> inline SharedPtr<CLASS>::SharedPtr (CLASS * t)            \
 : data (t ? t->m_self.data : 0L) {                                    \
    if (data)                                                          \
        data->addRef ();                                               \
 }                                                                     \
                                                                       \
 template <>                                                           \
 inline SharedPtr<CLASS> & SharedPtr<CLASS>::operator = (CLASS * t) {  \
    if (t) {                                                           \
        operator = (t->m_self);                                        \
    } else if (data) {                                                 \
        data->release ();                                              \
        data = 0L;                                                     \
    }                                                                  \
    return *this;                                                      \
 }                                                                     \
                                                                       \
 template <> inline WeakPtr<CLASS>::WeakPtr (CLASS * t)                \
 : data (t ? t->m_self.data : 0L) {                                    \
    if (data)                                                          \
        data->addWeakRef ();                                           \
 }                                                                     \
                                                                       \
 template <>                                                           \
 inline WeakPtr<CLASS> & WeakPtr<CLASS>::operator = (CLASS * t) {      \
    if (t) {                                                           \
        operator = (t->m_self);                                        \
    } else if (data) {                                                 \
        data->releaseWeak ();                                          \
        data = 0L;                                                     \
    }                                                                  \
    return *this;                                                      \
 }

/*
 * A shareable double linked list of ListNodeBase<T> nodes
 */
template <class T>
class KMPLAYER_EXPORT List : public Item <List <T> > {
public:
    List () {}
    List (typename Item<T>::SharedType f, typename Item<T>::SharedType l) 
        : m_first (f), m_last (l) {}
    ~List () { clear (); }

    typename Item<T>::SharedType first () const { return m_first; }
    typename Item<T>::SharedType last () const { return m_last; }
    void append (typename Item<T>::SharedType c);
    void insertBefore(typename Item<T>::SharedType c, typename Item<T>::SharedType b);
    void remove (typename Item<T>::SharedType c);
    void clear ();
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
class KMPLAYER_EXPORT ListNodeBase : public Item <T> {
    friend class List<T>;
public:
    virtual ~ListNodeBase () {}

    virtual const char * nodeName () const { return "#nodebase"; }
    virtual QString nodeValue () const { return QString (); }
    virtual void setNodeName (const QString &) {}
    virtual void setNodeValue (const QString &) {}

    typename Item<T>::SharedType nextSibling () const { return m_next; }
    typename Item<T>::SharedType previousSibling () const { return m_prev; }
protected:
    ListNodeBase () {}
    typename Item<T>::SharedType m_next;
    typename Item<T>::WeakType m_prev;
};

/*
 * ListNode for class T storage
 */
template <class T>
class ListNode : public ListNodeBase <ListNode <T> > {
public:
    ListNode (T d) : data (d) {}
    T data;
};

/*
 * Base class for double linked tree nodes having parent/siblings/children.
 * The linkage is a shared firstChild and weak parentNode.
 */
template <class T>
class KMPLAYER_EXPORT TreeNode : public ListNodeBase <T> {
public:
    virtual ~TreeNode () {}

    virtual void appendChild (typename Item<T>::SharedType c);

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
class KMPLAYER_EXPORT Attribute : public ListNodeBase <Attribute> {
public:
    KDE_NO_CDTOR_EXPORT Attribute () {}
    Attribute (const QString & n, const QString & v);
    KDE_NO_CDTOR_EXPORT ~Attribute () {}
    virtual const char * nodeName () const;
    virtual QString nodeValue () const;
    virtual void setNodeName (const QString &);
    virtual void setNodeValue (const QString &);
protected:
    QString name;
    QString value;
};

ITEM_AS_POINTER(KMPlayer::Attribute)

/**                                   a  b  0
 * Matrix for coordinate transforms   c  d  0
 *                                    tx ty 1     */
class Matrix {
    friend class SizeEvent;
    float a, b, c, d;
    int tx, ty; 
public:
    Matrix ();
    Matrix (const Matrix & matrix);
    Matrix (int xoff, int yoff, float xscale, float yscale);
    void getXY (int & x, int & y) const;
    void getXYWH (int & x, int & y, int & w, int & h) const;
    void transform (const Matrix & matrix);
    void scale (float sx, float sy);
    void translate (int x, int y);
    // void rotate (float phi); // add this when needed
};

/**
 * Object should scale according the passed Fit value in SizedEvent
 */
enum Fit {
    fit_fill,      // fill complete area, no aspect preservation
    fit_hidden,    // keep aspect and don't scale, cut off what doesn't fit
    fit_meet,      // keep aspect and scale so that the smallest size just fits
    fit_slice,     // keep aspect and scale so that the largest size just fits
    fit_scroll     // keep aspect and don't scale, add scollbars if needed
};

/*
 * A generic event type
 */
class KMPLAYER_EXPORT Event : public Item <Event> {
public:
    KDE_NO_CDTOR_EXPORT Event (unsigned int event_id) : m_event_id (event_id) {}
    KDE_NO_CDTOR_EXPORT virtual ~Event () {}
    KDE_NO_EXPORT unsigned int id () const { return m_event_id; }
protected:
    unsigned int m_event_id;
};

ITEM_AS_POINTER(KMPlayer::Event)

/**
 * Event signaling that attached region should be repainted
 */
class PaintEvent : public Event {
public:
    PaintEvent (QPainter & p, int x, int y, int w, int h);
    QPainter & painter;
    int x, y, w, h;
};

/**
 * Event signaling that attached region is resized
 */
class SizeEvent : public Event {
public:
    SizeEvent (int x, int y, int w, int h, Fit f, const Matrix & m=Matrix ());
    int x () const;
    int y () const;
    int w () const;
    int h () const;
    int _x, _y, _w, _h;
    Fit fit;
    Matrix matrix;
};

// Note, add rotations when needed
KDE_NO_EXPORT
inline int SizeEvent::x () const { return int (_x * matrix.a) + matrix.tx; }
KDE_NO_EXPORT
inline int SizeEvent::y () const { return int (_y * matrix.d) + matrix.ty; }
KDE_NO_EXPORT inline int SizeEvent::w () const { return int (_w * matrix.a); }
KDE_NO_EXPORT inline int SizeEvent::h () const { return int (_h * matrix.d); }

/**
 * Event signaling a pointer event
 */
class PointerEvent : public Event {
public:
    PointerEvent (unsigned int event_id, int x, int y);
    int x, y;
};

extern const unsigned int event_pointer_clicked;
extern const unsigned int event_pointer_moved;
extern const unsigned int event_paint;
extern const unsigned int event_sized;
extern const unsigned int event_postponed;

// convenient types
typedef Item<Node>::SharedType NodePtr;
typedef Item<Node>::WeakType NodePtrW;
typedef Item<Attribute>::SharedType AttributePtr;
typedef Item<Attribute>::WeakType AttributePtrW;
typedef Item<Event>::SharedType EventPtr;
typedef List<Node> NodeList;                 // eg. for Node's children
typedef Item<NodeList>::SharedType NodeListPtr;
typedef Item<NodeList>::WeakType NodeListPtrW;
ITEM_AS_POINTER(KMPlayer::NodeList)
typedef List<Attribute> AttributeList;       // eg. for Element's attributes
typedef Item<AttributeList>::SharedType AttributeListPtr;
ITEM_AS_POINTER(KMPlayer::AttributeList)
typedef ListNode<NodePtrW> NodeRefItem;      // Node for ref Nodes
ITEM_AS_POINTER(KMPlayer::NodeRefItem)
//typedef ListNode<NodePtr> NodeStoreItem;   // list stores Nodes
typedef NodeRefItem::SharedType NodeRefItemPtr;
typedef NodeRefItem::WeakType NodeRefItemPtrW;
typedef List<NodeRefItem> NodeRefList;       // ref nodes, eg. event listeners
typedef Item<NodeRefList>::SharedType NodeRefListPtr;
typedef Item<NodeRefList>::WeakType NodeRefListPtrW;
ITEM_AS_POINTER(KMPlayer::NodeRefList)
typedef Item<ElementRuntime>::SharedType ElementRuntimePtr;
typedef Item<ElementRuntime>::WeakType ElementRuntimePtrW;

/*
 * Weak ref of the listeners list from signaler and the listener node
 */
class KMPLAYER_EXPORT Connection {
    friend class Node;
public:
    KDE_NO_CDTOR_EXPORT ~Connection () { disconnect (); }
    void disconnect ();
private:
    Connection (NodeRefListPtr ls, NodePtr node);
    NodeRefListPtrW listeners;
    NodeRefItemPtrW listen_item;
};

typedef SharedPtr <Connection> ConnectionPtr;

/*
 * Base class for XML nodes. Provides a w3c's DOM like API
 *
 * Most severe traps with using SharedPtr/WeakPtr for tree nodes:
 * - pointer ends up in two independant shared objects (hopefully with explicit
 *   constructor for T* and template specialization for assignment of T* should
 *   be enough of defences ..)
 * - Node added two times (added ASSERT in appendChild/insertBefore)
 * - Node is destroyed before being stored in a SharedPtr with kmplayer usage
 *   of each object having a WeakPtr to itself (eg. be extremely careful with
 *   using m_self in the constructor, no SharedPtr storage yet)
 *
 * Livetime of an element is
 |-->state_activated<-->state_began<-->state_finished<-->state_deactivated-->|
  In scope            begin event    end event         Out scope
 */
class KMPLAYER_EXPORT Node : public TreeNode <Node> {
    friend class DocumentBuilder;
public:
    enum State {
        state_init, state_deferred,
        state_activated, state_began, state_finished, state_deactivated
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
    virtual bool expose () const;
    /**
     * If this node should be visible to the user
     */
    bool isEditable () const { return editable; }
    /**
     * If this node purpose is for storing runtime data only,
     * ie. node doesn't exist in the original document
     */
    bool auxiliaryNode () const { return auxiliary_node; }
    void setAuxiliaryNode (bool b) { auxiliary_node = b; }
    /**
     * Add node as listener for a certain event_id.
     * Return a NULL ptr if event_id is not supported.
     * \sa: Connection::disconnect()
     */
    ConnectionPtr connectTo (NodePtr node, unsigned int event_id);
    /*
     * Event send to this node, return true if handled
     */
    virtual bool handleEvent (EventPtr event);
    /*
     * Dispatch Event to all listeners of event->id()
     */
    void propagateEvent (EventPtr event);
    /**
     * Activates element, sets state to state_activated. Will call activate() on
     * firstChild or call deactivate().
     */
    virtual void activate ();
    /**
     * if state is between state_activated and state_deactivated
     */
    bool active () const
        { return state > state_deferred && state < state_deactivated; }
    /**
     * if state is between state_activated and state_finished
     */
    bool unfinished () const
        { return state > state_deferred && state < state_finished; }
    /**
     * Defers an activated, so possible playlists items can be added.
     */
    virtual void defer ();
    /**
     * Puts a defered element in activated again, calls activate() again 
     */
    virtual void undefer ();
    /**
     * Sets state to state_begin when active
     */
    virtual void begin ();
    /**
     * Sets state to state_finish when >= state_activated.
     * Notifies parent with a childDone call.
     */
    virtual void finish ();
    /**
     * Stops element, sets state to state_deactivated. Calls deactivate() on 
     * activated/defered children. May call childDone() when active() and not
     * finished yet.
     */
    virtual void deactivate ();
    /**
     * Resets element, calls deactivate() if state is state_activated and sets
     * state to state_init.
     */
    virtual void reset ();
    /**
     * Notification from child that it has began.
     */
    virtual void childBegan (NodePtr child);
    /**
     * Notification from child that it's finished. Will call deactive() on
     * child if it state is state_finished. Call activate() on nexSibling
     * or deactivate() if there is none.
     */
    virtual void childDone (NodePtr child);
    /**
     * Creates a new ElementRuntime object
     */
    virtual ElementRuntimePtr getRuntime ();
    virtual void clear ();
    void clearChildren ();
    void appendChild (NodePtr c);
    void insertBefore (NodePtr c, NodePtr b);
    void removeChild (NodePtr c);
    void replaceChild (NodePtr _new, NodePtr old);
    /*
     * Get rid of whitespace only text nodes
     */
    void normalize ();
    KDE_NO_EXPORT bool isDocument () const { return m_doc == m_self; }

    KDE_NO_EXPORT NodeListPtr childNodes () const;
    void setState (State nstate);
    /*
     * Open tag is found by parser, attributes are set
     */
    virtual void opened ();
    /*
     * Close tag is found by parser, children are appended
     */
    virtual void closed ();
protected:
    Node (NodePtr & d, short _id=0);
    /*
     * Returns a listener list for event_id, or a null ptr if not supported.
     */
    virtual NodeRefListPtr listeners (unsigned int event_id);
    NodePtr m_doc;
public:
    State state;
    short id;
private:
    bool auxiliary_node;
protected:
    bool editable;
};

ITEM_AS_POINTER(KMPlayer::Node)

const short id_node_document = 1;
const short id_node_text = 5;
const short id_node_cdata = 6;

/*
 * Element node, XML node that can have attributes
 */
class KMPLAYER_EXPORT Element : public Node {
public:
    ~Element () {}
    void setAttributes (AttributeListPtr attrs);
    void setAttribute (const QString & name, const QString & value);
    QString getAttribute (const QString & name);
    KDE_NO_EXPORT AttributeListPtr attributes () const { return m_attributes; }
    virtual void clear ();
    virtual bool isElementNode () { return true; }
protected:
    Element (NodePtr & d, short id=0);
    AttributeListPtr m_attributes;
};

/**
 * Node that references another node
 */
class RefNode : public Node {
public:
    RefNode (NodePtr & d, NodePtr ref);
    virtual const char * nodeName () const { return tag_name.ascii (); }
    NodePtr refNode () const { return ref_node; }
    void setRefNode (const NodePtr ref);
protected:
    NodePtrW ref_node;
    QString tag_name;
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
    Mrl (NodePtr & d, short id=0);
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
    enum { Single = 0, Window } view_mode;
    bool resolved;
    bool bookmarkable;
};

/**
 * Document listener interface
 */
class KMPLAYER_EXPORT PlayListNotify {
public:
    virtual ~PlayListNotify () {}
    /**
     * Ask for setting this node current and playing a video/audio mrl
     * If returning false, the element will be set to finished
     */
    virtual bool requestPlayURL (NodePtr mrl) = 0;
    /**
     * Called by an unresolved Mrl, check if this node points to a playlist
     */
    virtual bool resolveURL (NodePtr mrl) = 0;
    /**
     * Ask for setting this node current and playing a video/audio mrl
     */
    virtual bool setCurrent (NodePtr mrl) = 0;
    /**
     * Element has activated or deactivated notification
     */
    virtual void stateElementChanged (NodePtr element) = 0;
    /**
     * Set element to which to send GUI events
     */
    virtual void setEventDispatcher (NodePtr element) = 0;
    /**
     * Request to show msg for informing the user
     */
    virtual void setInfoMessage (const QString & msg) = 0;
    /**
     * Some rectangle needs repainting
     */
    virtual void repaintRect (int x, int y, int w, int h) = 0;
    /**
     * move a rectangle
     */
    virtual void moveRect (int x, int y, int w, int h, int x1, int y1) = 0;
    /**
     * Sets the video widget postion and background color if bg not NULL
     */
    virtual void avWidgetSizes (int x, int y, int w, int h, unsigned int *bg)=0;
    /**
     * Ask for connection bitrates settings
     */
    virtual void bitRates (int & prefered, int & maximal) = 0;
    /**
     * Sets next call to Document::timer() or -1 to cancel a previous call
     */
    virtual void setTimeout (int ms) = 0;
};

/**
 * To have a somewhat synchronized time base, node having timers should use
 * this. Idea is that if a node still waiting for network data, it can hold
 * this time line.
 */
class TimerInfo : public ListNodeBase <TimerInfo> {
public:
    TimerInfo (NodePtr n, unsigned id, struct timeval & now, int ms);
    KDE_NO_CDTOR_EXPORT ~TimerInfo () {}
    NodePtrW node;
    unsigned event_id;
    struct timeval timeout;
    int milli_sec;
};

ITEM_AS_POINTER(KMPlayer::TimerInfo)
typedef Item <TimerInfo>::SharedType TimerInfoPtr;
typedef Item <TimerInfo>::WeakType TimerInfoPtrW;

/**
 * Event signaling a timer event
 */
class TimerEvent : public Event {
public:
    TimerEvent (TimerInfoPtr tinfo);
    TimerInfoPtrW timer_info;
    bool interval; // set to 'true' in 'Node::handleEvent()' to make it repeat
};

/**
 * Event signaling postponed or proceeded
 */
class PostponedEvent : public Event {
public:
    PostponedEvent (bool postponed);
    bool is_postponed; // postponed or proceeded
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
    virtual NodePtr childFromTag (const QString & tag);
    KDE_NO_EXPORT const char * nodeName () const { return "document"; }
    /**
     * Will return false if this document has child nodes
     */
    virtual bool isMrl ();
    virtual void defer ();
    virtual void undefer ();
    virtual void reset ();
    /**
     * Ask for TimerEvent for Node n in ms milli-seconds.
     * Returns weak ref to TimerInfo ptr, which is an item in the timers list
     */
    TimerInfoPtrW setTimeout (NodePtr n, int ms, unsigned id=0);
    void cancelTimer (TimerInfoPtr ti);
    /**
     * Will increment a postponed counter, while > 0, no TimerEvent's happen
     */
    void postpone ();
    void proceed ();
    /**
     * Called by PlayListNotify, creates TimerEvent on first item in timers. 
     * Returns true if to repeat this same timeout FIXME.
     */
    bool timer ();
    /**
     * Document has list of postponed listeners, eg. for running (gif)movies
     */
    virtual NodeRefListPtr listeners (unsigned int event_id);

    NodePtrW rootLayout;
    List <TimerInfo> timers; //FIXME: make as connections
    struct timeval postponed_time;
    PlayListNotify * notify_listener;
    unsigned int m_tree_version;
private:
    NodeRefListPtr m_PostponedListeners;
    int postponed;
    int cur_timeout;
    bool intimer;
};

/**
 * Represents XML text, like "some text" in '<foo>some text</foo>'
 */
class KMPLAYER_EXPORT TextNode : public Node {
public:
    TextNode (NodePtr & d, const QString & s, short _id = id_node_text);
    KDE_NO_CDTOR_EXPORT ~TextNode () {}
    void appendText (const QString & s);
    const char * nodeName () const { return "#text"; }
    QString nodeValue () const;
    bool expose () const;
protected:
    QString text;
};

/**
 * Represents cdata sections, like "some text" in '<![CDATA[some text]]>'
 */
class KMPLAYER_EXPORT CData : public TextNode {
public:
    CData (NodePtr & d, const QString & s);
    KDE_NO_CDTOR_EXPORT ~CData () {}
    const char * nodeName () const { return "#cdata"; }
};

/**
 * Unrecognized tag by parent element or just some auxiliary node
 */
class DarkNode : public Element {
public:
    DarkNode (NodePtr & d, const QString & n, short id=0);
    KDE_NO_CDTOR_EXPORT ~DarkNode () {}
    const char * nodeName () const { return name.ascii (); }
    NodePtr childFromTag (const QString & tag);
    virtual bool expose () const;
protected:
    QString name;
};

//-----------------------------------------------------------------------------

/**
 * just some url, can get a SMIL, RSS, or ASX childtree
 */
class KMPLAYER_EXPORT GenericURL : public Mrl { 
public:
    GenericURL(NodePtr &d, const QString &s, const QString &n=QString::null);
    KDE_NO_EXPORT const char * nodeName () const { return "url"; }
    void closed ();
};

/**
 * Non url mrl
 */
class KMPLAYER_EXPORT GenericMrl : public Mrl { 
public:
    KDE_NO_CDTOR_EXPORT GenericMrl (NodePtr & d) : Mrl (d) {}
    GenericMrl(NodePtr &d, const QString &s, const QString &n=QString::null);
    KDE_NO_EXPORT const char * nodeName () const { return "mrl"; }
    void closed ();
    bool expose () const;
    /**
     * Will return false if this document has child nodes
     */
    bool isMrl ();
};

KMPLAYER_EXPORT
void readXML (NodePtr root, QTextStream & in, const QString & firstline);
KMPLAYER_EXPORT Node * fromXMLDocumentTag (NodePtr & d, const QString & tag);

template <class T>
inline Item<T>::Item () : m_self (static_cast <T*> (this), true) {}

template <class T> inline void List<T>::append(typename Item<T>::SharedType c) {
    if (!m_first) {
        m_first = m_last = c;
    } else {
        m_last->m_next = c;
        c->m_prev = m_last;
        m_last = c;
    }
}

template <class T> inline void List<T>::insertBefore(typename Item<T>::SharedType c, typename Item<T>::SharedType b) {
    if (!b) {
        append (c);
    } else {
        if (b->m_prev) {
            b->m_prev->m_next = c;
            c->m_prev = b->m_prev;
        } else {
            c->m_prev = 0L;
            m_first = c;
        }
        b->m_prev = c;
        c->m_next = b;
    }
}

template <class T> inline void List<T>::remove(typename Item<T>::SharedType c) {
    if (c->m_prev) {
        c->m_prev->m_next = c->m_next;
    } else
        m_first = c->m_next;
    if (c->m_next) {
        c->m_next->m_prev = c->m_prev;
        c->m_next = 0L;
    } else
        m_last = c->m_prev;
    c->m_prev = 0L;
}

template <class T> inline unsigned int List<T>::length () const {
    unsigned int count = 0;
    for (typename Item<T>::SharedType t = m_first; t; t = t->nextSibling ())
        count++;
    return count;
}

template <class T> inline void List<T>::clear () {
    m_first = m_last = 0L;
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

inline KDE_NO_EXPORT NodeListPtr Node::childNodes () const {
    return new NodeList (m_first_child, m_last_child);
}

}  // KMPlayer namespace

#endif //_KMPLAYER_PLAYLIST_H_
