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

#include "config-kmplayer.h"
#include <sys/time.h>

#include <qstring.h>

#include "kmplayer_def.h"
#include "kmplayertypes.h"
#include "kmplayershared.h"

typedef struct _cairo_surface cairo_surface_t;

class QTextStream;

namespace KMPlayer {

class Document;
class Node;
class TextNode;
class Posting;
class Mrl;
class Surface;
class ElementPrivate;
class Visitor;
class MediaInfo;

template <class T> class KMPLAYER_EXPORT GlobalShared {
    T **global;
    int refcount;
public:
    GlobalShared (T **glob);
    virtual ~GlobalShared () {}

    void ref () { refcount++; }
    void unref();
};

template <class T>
inline GlobalShared<T>::GlobalShared (T **glob) : global (glob), refcount (1) {
    *global = static_cast <T*> (this);
}

template <class T> inline void GlobalShared<T>::unref() {
    if (--refcount <= 0) {
        *global = NULL;
        delete this;
    }
}

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
 * two independent SharedData<Item<T>> objects.
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
    virtual void removeChild (typename Item<T>::SharedType c);

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
    Attribute (const TrieString & n, const QString & v);
    KDE_NO_CDTOR_EXPORT ~Attribute () {}
    TrieString name () const { return m_name; }
    QString value () const { return m_value; }
    void setName (const TrieString &);
    void setValue (const QString &);
protected:
    TrieString m_name;
    QString m_value;
};

ITEM_AS_POINTER(KMPlayer::Attribute)

/**
 * Object should scale according the passed Fit value in SizedEvent
 */
enum Fit {
    fit_default,   // fill hidden
    fit_fill,      // fill complete area, no aspect preservation
    fit_hidden,    // keep aspect and don't scale, cut off what doesn't fit
    fit_meet,      // keep aspect and scale so that the smallest size just fits
    fit_slice,     // keep aspect and scale so that the largest size just fits
    fit_scroll     // keep aspect and don't scale, add scollbars if needed
};

enum MessageType
{
    MsgEventTimer = 0,
    MsgEventClicked,
    MsgEventPointerMoved,
    MsgEventPointerInBounds,
    MsgEventPointerOutBounds,
    MsgEventStarting,
    MsgEventStarted,
    MsgEventStopped,
    MsgMediaFinished,
    // the above messages are ordered like SMIL timing events
    MsgMediaReady,
    MsgMediaUpdated,
    MsgEventPostponed,
    MsgSurfaceUpdate,
    MsgSurfaceAttach,
    MsgStateFreeze,
    MsgStateRewind,
    MsgChildFinished,

    MsgStartQueryMessage,
    MsgQueryMediaManager,
    MsgQueryRoleTiming,
    MsgQueryRoleDisplay,
    MsgQueryRoleChildDisplay,    // Mrl*
    MsgQueryRoleSizer,
    MsgQueryReceivers,
    MsgEndQueryMessage,

    MsgInfoString                // QString*
};

#define MsgUnhandled ((void *) 357L)

#define nodeMessageReceivers(node, msg)                                     \
    (NodeRefList*)(node)->message (MsgQueryReceivers, (void*)(long)(msg))

// convenient types
typedef void Role;
typedef Item<Node>::SharedType NodePtr;
typedef Item<Node>::WeakType NodePtrW;
typedef Item<Attribute>::SharedType AttributePtr;
typedef Item<Attribute>::WeakType AttributePtrW;
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
typedef List<NodeRefItem> NodeRefList;       // ref nodes, eg. event receivers
typedef Item<NodeRefList>::SharedType NodeRefListPtr;
typedef Item<NodeRefList>::WeakType NodeRefListPtrW;
ITEM_AS_POINTER(KMPlayer::NodeRefList)
typedef Item<Surface>::SharedType SurfacePtr;
typedef Item<Surface>::WeakType SurfacePtrW;

/*
 * Weak ref of the receivers list from signaler and the listener node
 */
class KMPLAYER_EXPORT Connection {
    friend class Node;
public:
    KDE_NO_CDTOR_EXPORT ~Connection () { disconnect (); }
    void disconnect ();
    NodePtrW connectee; // the one that will, when ever, trigger the event
private:
    Connection (NodeRefList *ls, Node *node, Node *invoker);
    NodeRefListPtrW listeners;
    NodeRefItemPtrW listen_item;
};

typedef SharedPtr <Connection> ConnectionPtr;

/*
 * Base class for XML nodes. Provides a w3c's DOM like API
 *
 * Most severe traps with using SharedPtr/WeakPtr for tree nodes:
 * - pointer ends up in two independent shared objects (hopefully with
 *   template specialization for constructor for T* and assignment of T* should
 *   be enough of defences ..)
 * - Node added two times (added ASSERT in appendChild/insertBefore)
 * - Node is destroyed before being stored in a SharedPtr with kmplayer usage
 *   of each object having a WeakPtr to itself (eg. be extremely careful with
 *   using m_self in the constructor, no SharedPtr storage yet)
 *
 * Livetime of an element is
 |-->state_activated<-->state_began<-->state_finished-->state_deactivated-->|
  In scope            begin event    end event         Out scope
 */
class KMPLAYER_EXPORT Node : public TreeNode <Node> {
    friend class DocumentBuilder;
public:
    enum State {
        state_init, state_deferred,
        state_activated, state_began, state_finished, state_deactivated
    };
    enum PlayType {
        play_type_none, play_type_unknown, play_type_info,
        play_type_image, play_type_audio, play_type_video
    };
    virtual ~Node ();
    Document * document ();
    virtual Mrl * mrl ();
    virtual NodePtr childFromTag (const QString & tag);
    void characterData (const QString & s);
    QString innerText () const;
    QString innerXML () const;
    QString outerXML () const;
    virtual const char * nodeName () const;
    virtual QString nodeValue () const;
    virtual void setNodeName (const QString &) {}

    /**
     * If this is a derived Mrl object and has a SRC attribute
     */
    virtual PlayType playType ();
    bool isPlayable () { return playType () > play_type_none; }
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
    ConnectionPtr connectTo (Node *node, MessageType msg);
    /*
     * Event send to this node, return true if handled
     */
    virtual void *message (MessageType msg, void *content=NULL);
    /*
     * Dispatch Event to all connectorss of MessageType
     */
    void deliver (MessageType msg, void *content);
    /**
     * Alternative to event handling is the Visitor pattern
     */
    virtual void accept (Visitor *);
    /**
     * Activates element, sets state to state_activated. Will call activate() on
     * firstChild or call deactivate().
     */
    virtual void activate ();
    /**
     * if state is between state_activated and state_deactivated
     */
    bool active () const
        { return state >= state_deferred && state < state_deactivated; }
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
     * Puts a deferred element in activated again, calls activate() again
     */
    virtual void undefer ();
    /**
     * Sets state to state_begin when active
     */
    virtual void begin ();
    /**
     * Sets state to state_finish when >= state_activated.
     * Notifies parent with a MsgChildFinished message.
     */
    virtual void finish ();
    /**
     * Stops element, sets state to state_deactivated. Calls deactivate() on
     * activated/deferred children. May call childDone() when active() and not
     * finished yet.
     */
    virtual void deactivate ();
    /**
     * Resets element, calls deactivate() if state is state_activated and sets
     * state to state_init.
     */
    virtual void reset ();
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
const short id_node_record_document = 2;
const short id_node_grab_document = 3;
const short id_node_text = 5;
const short id_node_cdata = 6;

const short id_node_group_node = 25;
const short id_node_playlist_document = 26;
const short id_node_playlist_item = 27;
const short id_node_param = 28;
const short id_node_html_object = 29;
const short id_node_html_embed = 30;

/*
 * Element node, XML node that can have attributes
 */
class KMPLAYER_EXPORT Element : public Node {
public:
    ~Element ();
    void setAttributes (AttributeListPtr attrs);
    void setAttribute (const TrieString & name, const QString & value);
    QString getAttribute (const TrieString & name);
    KDE_NO_EXPORT AttributeListPtr attributes () const { return m_attributes; }
    virtual void init ();
    virtual void reset ();
    virtual void clear ();
    virtual bool isElementNode () { return true; }
    virtual void accept (Visitor * v);
    /**
     * Params are like attributes, but meant to be set dynamically. Caller may
     * pass a modification id, that it can use to restore the old value.
     * Param will be auto removed on deactivate
     */
    void setParam (const TrieString &para, const QString &val, int * mod_id=0L);
    QString param (const TrieString & para);
    void resetParam (const TrieString & para, int mod_id);
    /**
     * Called from (re)setParam for specialized interpretation of params
     **/
    virtual void parseParam (const TrieString &, const QString &) {}
protected:
    Element (NodePtr & d, short id=0);
    AttributeListPtr m_attributes;
private:
    ElementPrivate * d;
};

/**
 * Node that references another node
 */
class RefNode : public Node {
public:
    RefNode (NodePtr & d, NodePtr ref);
    virtual const char * nodeName () const { return tag_name.data (); }
    NodePtr refNode () const { return ref_node; }
    void setRefNode (const NodePtr ref);
protected:
    NodePtrW ref_node;
    QByteArray tag_name;
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
    void parseParam (const TrieString &, const QString &);
    unsigned int cached_ismrl_version;
    PlayType cached_play_type;
public:
    ~Mrl ();
    PlayType playType ();
    /*
     * The original node (or this) having the URL, needed for playlist expansion
     */
    virtual Mrl * linkNode ();
    virtual Mrl * mrl ();
    QString absolutePath ();

    virtual void activate ();
    virtual void begin ();
    virtual void defer ();
    virtual void undefer ();
    virtual void deactivate ();
    virtual void *message (MessageType msg, void *content=NULL);

    /**
     * If this Mrl is top node of external document, opener has the
     * location in SCR. Typically that's the parent of this node.
     */
    NodePtrW opener; //if this node is top node of external document,
    MediaInfo *media_info;
    QString src;
    QString pretty_name;
    QString mimetype;
    Single width;
    Single height;
    float aspect;
    int repeat;
    enum { SingleMode = 0, WindowMode } view_mode;
    bool resolved;
    bool bookmarkable;
    bool access_granted;
};

/**
 * Document listener interface
 */
class KMPLAYER_EXPORT PlayListNotify {
public:
    virtual ~PlayListNotify () {}

    /**
     * Element has activated or deactivated notification
     */
    virtual void stateElementChanged (Node * element, Node::State old_state, Node::State new_state) = 0;
    /**
     * Ask for connection bitrates settings
     */
    virtual void bitRates (int & preferred, int & maximal) = 0;
    /**
     * Sets next call to Document::timer() or -1 to cancel a previous call
     */
    virtual void setTimeout (int ms) = 0;
    /**
     * Request to open url with mimetype
     */
    virtual void openUrl (const KUrl &, const QString &t, const QString &srv)=0;
    /**
     * Change repaint updaters
     */
    virtual void addRepaintUpdater (Node *node)=0;
    virtual void removeRepaintUpdater (Node *node)=0;
    virtual void enableRepaintUpdaters (bool enable, unsigned int off_time)=0;
};

class KMPLAYER_NO_EXPORT Surface : public TreeNode <Surface> {
public:
    Surface (NodePtr node, const SRect & rect);
    ~Surface();

    virtual SurfacePtr createSurface (NodePtr owner, const SRect & rect) = 0;
    virtual IRect toScreen (Single x, Single y, Single w, Single h) = 0;
    virtual void resize (const SRect & rect) = 0;
    virtual void repaint () = 0;
    virtual void repaint (const SRect &rect) = 0;
    void remove ();                // remove from parent, mark ancestors dirty
    void markDirty ();             // mark this and ancestors dirty

    NodePtrW node;
    SRect bounds;                  // bounds in in parent coord.
    float xscale, yscale;          // internal scaling
    unsigned int background_color; // rgba background color
    bool dirty;                    // a decendant is removed
#ifdef KMPLAYER_WITH_CAIRO
    cairo_surface_t *surface;
#endif
};

ITEM_AS_POINTER(KMPlayer::Surface)

/*
 *  A generic type for posting messages
 **/
class KMPLAYER_EXPORT Posting {
public:
    KDE_NO_CDTOR_EXPORT Posting (Node *n, MessageType msg)
        : source (n), message (msg) {}
    KDE_NO_CDTOR_EXPORT virtual ~Posting () {}
    NodePtrW source;
    MessageType message;
};

/**
 * Posting signaling a timer event
 */
class KMPLAYER_NO_EXPORT TimerPosting : public Posting {
public:
    TimerPosting (int ms, unsigned eid=0);
    unsigned event_id;
    int milli_sec;
    bool interval; // set to 'true' in 'Node::message()' to make it repeat
};

class KMPLAYER_NO_EXPORT UpdateEvent {
public:
    UpdateEvent (Document *, unsigned int off_time);
    unsigned int cur_event_time;
    unsigned int skipped_time;
};

/**
 * Event signaling postponed or proceeded
 */
class KMPLAYER_NO_EXPORT PostponedEvent {
public:
    PostponedEvent (bool postponed);
    bool is_postponed; // postponed or proceeded
};

/**
 * Postpone object representing a postponed document
 * During its livetime, no TimerEvent's happen
 */
class KMPLAYER_NO_EXPORT Postpone {
    friend class Document;
    struct timeval postponed_time;
    NodePtrW m_doc;
    Postpone (NodePtr doc);
public:
    ~Postpone ();
};

typedef SharedPtr <Postpone> PostponePtr;
typedef WeakPtr <Postpone> PostponePtrW;

struct KMPLAYER_NO_EXPORT EventData {
    EventData (Node *t, Posting *e, EventData *n);
    ~EventData ();

    NodePtrW target;
    Posting *event;
    struct timeval timeout;

    EventData *next;
};

/**
 * The root of the DOM tree
 */
class KMPLAYER_EXPORT Document : public Mrl {
    friend class Postpone;
public:
    Document (const QString &, PlayListNotify * notify = 0L);
    ~Document ();
    Node *getElementById (const QString & id);
    Node *getElementById (Node *start, const QString & id, bool inter_doc);
    /** All nodes have shared pointers to Document,
     * so explicitly dispose it (calls clear and set m_doc to 0L)
     * */
    void dispose ();
    virtual NodePtr childFromTag (const QString & tag);
    KDE_NO_EXPORT const char * nodeName () const { return "document"; }
    virtual void activate ();
    virtual void defer ();
    virtual void undefer ();
    virtual void reset ();

    Posting *post (Node *n, Posting *event);
    void cancelPosting (Posting *event);
    void pausePosting (Posting *e);
    void unpausePosting (Posting *e, int ms);

    void timeOfDay (struct timeval &);
    PostponePtr postpone ();
    /**
     * Called by PlayListNotify, processes events in event_queue with timeout set to now
     */
    void timer ();
    void updateTimeout ();
    /**
     * Document has list of postponed receivers, eg. for running (gif)movies
     */
    virtual void *message (MessageType msg, void *content=NULL);

    PlayListNotify *notify_listener;
    unsigned int m_tree_version;
    unsigned int last_event_time;
private:
    void proceed (const struct timeval & postponed_time);
    void insertPosting (Node *n, Posting *e, const struct timeval &tv);
    void setNextTimeout (const struct timeval &now);

    PostponePtrW postpone_ref;
    PostponePtr postpone_lock;
    NodeRefListPtr m_PostponedListeners;
    EventData *event_queue;
    EventData *paused_queue;
    EventData *cur_event;
    int cur_timeout;
    struct timeval first_event_time;
};

namespace SMIL {
    class Smil;
    class Layout;
    class RegionBase;
    class Seq;
    class Par;
    class Excl;
    class Transition;
    class Animate;
    class AnimateMotion;
    class MediaType;
    class ImageMediaType;
    class TextMediaType;
    class RefMediaType;
    class Brush;
    class SmilText;
    class TextFlow;
    class PriorityClass;
    class Anchor;
    class Area;
}
namespace RP {
    class Imfl;
    class Crossfade;
    class Fadein;
    class Fadeout;
    class Fill;
    class Wipe;
    class ViewChange;
    class Animate;
}

class KMPLAYER_NO_EXPORT Visitor {
public:
    KDE_NO_CDTOR_EXPORT Visitor () {}
    KDE_NO_CDTOR_EXPORT virtual ~Visitor () {}
    virtual void visit (Node *) {}
    virtual void visit (TextNode *);
    virtual void visit (Element *);
    virtual void visit (SMIL::Smil *) {}
    virtual void visit (SMIL::Layout *);
    virtual void visit (SMIL::RegionBase *);
    virtual void visit (SMIL::Seq *);
    virtual void visit (SMIL::Par *);
    virtual void visit (SMIL::Excl *);
    virtual void visit (SMIL::Transition *);
    virtual void visit (SMIL::Animate *);
    virtual void visit (SMIL::AnimateMotion *);
    virtual void visit (SMIL::PriorityClass *);
    virtual void visit (SMIL::MediaType *);
    virtual void visit (SMIL::ImageMediaType *);
    virtual void visit (SMIL::TextMediaType *);
    virtual void visit (SMIL::RefMediaType *);
    virtual void visit (SMIL::Brush *);
    virtual void visit (SMIL::SmilText *);
    virtual void visit (SMIL::TextFlow *);
    virtual void visit (SMIL::Anchor *);
    virtual void visit (SMIL::Area *);
    virtual void visit (RP::Imfl *) {}
    virtual void visit (RP::Crossfade *) {}
    virtual void visit (RP::Fadein *) {}
    virtual void visit (RP::Fadeout *) {}
    virtual void visit (RP::Fill *) {}
    virtual void visit (RP::Wipe *) {}
    virtual void visit (RP::ViewChange *) {}
    virtual void visit (RP::Animate *) {}
};

/**
 * Represents XML text, like "some text" in '<foo>some text</foo>'
 */
class KMPLAYER_EXPORT TextNode : public Node {
public:
    TextNode (NodePtr & d, const QString & s, short _id = id_node_text);
    KDE_NO_CDTOR_EXPORT ~TextNode () {}
    void appendText (const QString & s);
    void setText (const QString & txt) { text = txt; }
    const char * nodeName () const { return "#text"; }
    void accept (Visitor *v) { v->visit (this); }
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
class KMPLAYER_EXPORT DarkNode : public Element {
public:
    DarkNode (NodePtr & d, const QByteArray &n, short id=0);
    KDE_NO_CDTOR_EXPORT ~DarkNode () {}
    const char * nodeName () const { return name.data (); }
    NodePtr childFromTag (const QString & tag);
    virtual bool expose () const;
protected:
    QByteArray name;
};

//-----------------------------------------------------------------------------

/**
 * just some url, can get a SMIL, RSS, or ASX childtree
 */
class KMPLAYER_EXPORT GenericURL : public Mrl {
public:
    GenericURL(NodePtr &d, const QString &s, const QString &n=QString ());
    KDE_NO_EXPORT const char * nodeName () const { return "url"; }
    void closed ();
};

/**
 * Non url mrl
 */
class KMPLAYER_EXPORT GenericMrl : public Mrl {
public:
    KDE_NO_CDTOR_EXPORT GenericMrl (NodePtr & d) : Mrl (d), node_name ("mrl") {}
    GenericMrl(NodePtr &d, const QString &s, const QString &name=QString (), const QByteArray &tag=QByteArray ("mrl"));
    KDE_NO_EXPORT const char * nodeName () const { return node_name.data (); }
    void closed ();
    bool expose () const;
    QByteArray node_name;
};

KMPLAYER_EXPORT
void readXML (NodePtr root, QTextStream & in, const QString & firstline, bool set_opener=true);
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
    return typename Item<T>::SharedType ();
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

template <class T>
inline void TreeNode<T>::removeChild (typename Item<T>::SharedType c) {
    if (c->m_prev) {
        c->m_prev->m_next = c->m_next;
    } else
        m_first_child = c->m_next;
    if (c->m_next) {
        c->m_next->m_prev = c->m_prev;
        c->m_next = 0L;
    } else
        m_last_child = c->m_prev;
    c->m_prev = 0L;
    c->m_parent = 0L;
}

inline KDE_NO_EXPORT NodeListPtr Node::childNodes () const {
    return new NodeList (m_first_child, m_last_child);
}

}  // KMPlayer namespace

#endif //_KMPLAYER_PLAYLIST_H_
