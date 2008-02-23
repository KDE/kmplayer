/**
 * Copyright (C) 2004 by Koos Vriezen <koos.vriezen@gmail.com>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License version 2 as published by the Free Software Foundation.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Steet, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 **/

#include "config-kmplayer.h"
#include <time.h>

#include <qtextstream.h>
#include <kdebug.h>
#include <kurl.h>
#ifdef HAVE_EXPAT
#include <expat.h>
#endif
#ifdef KMPLAYER_WITH_CAIRO
# include <cairo.h>
#endif
#include "kmplayerplaylist.h"
#include "kmplayer_asx.h"
#include "kmplayer_atom.h"
#include "kmplayer_rp.h"
#include "kmplayer_rss.h"
#include "kmplayer_smil.h"
#include "kmplayer_xspf.h"
#include "mediaobject.h"

#ifdef SHAREDPTR_DEBUG
int shared_data_count;
#endif

using namespace KMPlayer;

//-----------------------------------------------------------------------------

Node *KMPlayer::fromXMLDocumentTag (NodePtr & d, const QString & tag) {
    const char * const name = tag.latin1 ();
    if (!strcmp (name, "smil"))
        return new SMIL::Smil (d);
    else if (!strcasecmp (name, "asx"))
        return new ASX::Asx (d);
    else if (!strcasecmp (name, "imfl"))
        return new RP::Imfl (d);
    else if (!strcasecmp (name, "rss"))
        return new RSS::Rss (d);
    else if (!strcasecmp (name, "feed"))
        return new ATOM::Feed (d);
    else if (!strcasecmp (name, "playlist"))
        return new XSPF::Playlist (d);
    else if (!strcasecmp (name, "url"))
        return new GenericURL (d, QString ());
    else if (!strcasecmp (name, "mrl") ||
            !strcasecmp (name, "document"))
        return new GenericMrl (d);
    return 0L;
}

//-----------------------------------------------------------------------------

namespace {
    struct XMLStringlet {
        const QString str;
        XMLStringlet (const QString & s) : str (s) {}
    };
} // namespace

QTextStream & operator << (QTextStream & out, const XMLStringlet & txt) {
    int len = int (txt.str.length ());
    for (int i = 0; i < len; ++i) {
        if (txt.str [i] == QChar ('<')) {
            out <<  "&lt;";
        } else if (txt.str [i] == QChar ('>')) {
            out <<  "&gt;";
        } else if (txt.str [i] == QChar ('"')) {
            out <<  "&quot;";
        } else if (txt.str [i] == QChar ('&')) {
            out <<  "&amp;";
        } else
            out << txt.str [i];
    }
    return out;
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT
Connection::Connection (NodeRefListPtr ls, NodePtr node, NodePtr inv)
 : connectee (inv), listeners (ls) {
    if (listeners) {
        NodeRefItemPtr nci = new NodeRefItem (node);
        listeners->append (nci);
        listen_item = nci;
    }
}

KDE_NO_EXPORT void Connection::disconnect () {
    if (listen_item && listeners)
        listeners->remove (listen_item);
    listen_item = 0L;
    listeners = 0L;
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT
TimerEvent::TimerEvent (int ms, unsigned eid)
 : Event (event_timer), event_id (eid), milli_sec (ms), interval (false) {}

//-----------------------------------------------------------------------------

Matrix::Matrix () : a (1.0), b (0.0), c (0.0), d (1.0), tx (0), ty (0) {}

Matrix::Matrix (const Matrix & m)
 : a (m.a), b (m.b), c (m.c), d (m.d), tx (m.tx), ty (m.ty) {}

Matrix::Matrix (Single xoff, Single yoff, float xscale, float yscale)
 : a (xscale), b (0.0), c (0.0), d (yscale), tx (xoff), ty (yoff) {}

void Matrix::getXY (Single & x, Single & y) const {
    x = Single (x * a) + tx;
    y = Single (y * d) + ty;
}

void Matrix::getXYWH (Single & x, Single & y, Single & w, Single & h) const {
    getXY (x, y);
    w *= a;
    h *= d;
}

void Matrix::invXYWH (Single & x, Single & y, Single & w, Single & h) const {
    if (a > 0.00001 && d > 0.00001) {
        w /= a;
        h /= d;
        x = Single ((x - tx) / a);
        y = Single ((y - ty) / d);
    } else {
        kWarning () << "Not invering " << a << ", " << d << " scale";
    }
}

void Matrix::transform (const Matrix & matrix) {
    // TODO: rotate
    a *= matrix.a;
    d *= matrix.d;
    tx = Single (tx * matrix.a) + matrix.tx;
    ty = Single (ty * matrix.d) + matrix.ty;
}

void Matrix::scale (float sx, float sy) {
    a *= sx;
    d *= sy;
    tx *= sx;
    ty *= sy;
}

void Matrix::translate (Single x, Single y) {
    tx += x;
    ty += y;
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT Node::Node (NodePtr & d, short _id)
 : m_doc (d), state (state_init), id (_id),
   auxiliary_node (false), editable (true) {}

Node::~Node () {
    clear ();
}

Document * Node::document () {
    return convertNode <Document> (m_doc);
}

Mrl * Node::mrl () {
    return 0L;
}

const char * Node::nodeName () const {
    return "node";
}

void Node::setState (State nstate) {
    if (state != nstate) {
        State old = state;
        state = nstate;
        if (document ()->notify_listener)
            document()->notify_listener->stateElementChanged (this, old, state);
    }
}

bool Node::expose () const {
    return true;
}

void Node::activate () {
    //kDebug () << nodeName () << " Node::activate";
    setState (state_activated);
    if (firstChild ())
        firstChild ()->activate (); // activate only the first
    else
        finish (); // a quicky :-)
}

void Node::begin () {
    if (active ()) {
        setState (state_began);
    } else
        kError () << nodeName() << " begin call on not active element" << endl;
}

void Node::defer () {
    if (active ()) {
        setState (state_deferred);
    } else
        kError () << "Node::defer () call on not activated element" << endl;
}

void Node::undefer () {
    if (state == state_deferred) {
        setState (state_activated);
        activate ();
    } else
        kWarning () << nodeName () << " call on not deferred element";
}

void Node::finish () {
    if (active ()) {
        setState (state_finished);
        if (m_parent)
            m_parent->childDone (this);
        else
            deactivate (); // document deactivates itself on finish
    } else
        kWarning () <<"Node::finish () call on not active element";
}

void Node::deactivate () {
    //kDebug () << nodeName () << " Node::deactivate";
    bool need_finish (unfinished ());
    setState (state_deactivated);
    for (NodePtr e = firstChild (); e; e = e->nextSibling ()) {
        if (e->state > state_init && e->state < state_deactivated)
            e->deactivate ();
        else
            break; // remaining not yet activated
    }
    if (need_finish && m_parent)
        m_parent->childDone (this);
}

void Node::reset () {
    //kDebug () << nodeName () << " Node::reset";
    if (active ())
        deactivate ();
    setState (state_init);
    for (NodePtr e = firstChild (); e; e = e->nextSibling ()) {
        if (e->state != state_init)
            e->reset ();
        // else
        //    break; // rest not activated yet
    }
}

void Node::childBegan (NodePtr /*child*/) {
}

void Node::childDone (NodePtr child) {
    //kDebug () << nodeName () << child.ptr ();
    if (unfinished ()) {
        if (child->state == state_finished)
            child->deactivate ();
        if (child->nextSibling ())
            child->nextSibling ()->activate ();
        else
            finish (); // we're done
    }
}

void Node::clear () {
    clearChildren ();
}

void Node::clearChildren () {
    if (m_doc)
        document()->m_tree_version++;
    while (m_first_child != m_last_child) {
        // avoid stack abuse with 10k children derefing each other
        m_last_child->m_parent = 0L;
        m_last_child = m_last_child->m_prev;
        m_last_child->m_next = 0L;
    }
    if (m_first_child)
        m_first_child->m_parent = 0L;
    m_first_child = m_last_child = 0L;
}

void Node::appendChild (NodePtr c) {
    document()->m_tree_version++;
    ASSERT (!c->parentNode ());
    TreeNode<Node>::appendChild (c);
}

void Node::insertBefore (NodePtr c, NodePtr b) {
    if (!b) {
        appendChild (c);
    } else {
        ASSERT (!c->parentNode ());
        document()->m_tree_version++;
        if (b->m_prev) {
            b->m_prev->m_next = c;
            c->m_prev = b->m_prev;
        } else {
            c->m_prev = 0L;
            m_first_child = c;
        }
        b->m_prev = c;
        c->m_next = b;
        c->m_parent = this;
    }
}

void Node::removeChild (NodePtr c) {
    document()->m_tree_version++;
    TreeNode <Node>::removeChild (c);
}

KDE_NO_EXPORT void Node::replaceChild (NodePtr _new, NodePtr old) {
    document()->m_tree_version++;
    if (old->m_prev) {
        old->m_prev->m_next = _new;
        _new->m_prev = old->m_prev;
        old->m_prev = 0L;
    } else {
        _new->m_prev = 0L;
        m_first_child = _new;
    }
    if (old->m_next) {
        old->m_next->m_prev = _new;
        _new->m_next = old->m_next;
        old->m_next = 0L;
    } else {
        _new->m_next = 0L;
        m_last_child = _new;
    }
    _new->m_parent = this;
    old->m_parent = 0L;
}

NodePtr Node::childFromTag (const QString &) {
    return NodePtr ();
}

KDE_NO_EXPORT void Node::characterData (const QString & s) {
    document()->m_tree_version++;
    if (!m_last_child || m_last_child->id != id_node_text)
        appendChild (new TextNode (m_doc, s));
    else
        convertNode <TextNode> (m_last_child)->appendText (s);
}

void Node::normalize () {
    NodePtr e = firstChild ();
    while (e) {
        NodePtr tmp = e->nextSibling ();
        if (!e->isElementNode () && e->id == id_node_text) {
            QString val = e->nodeValue ().simplifyWhiteSpace ();
            if (val.isEmpty ())
                removeChild (e);
            else
                convertNode <TextNode> (e)->setText (val);
        } else
            e->normalize ();
        e = tmp;
    }
}

static void getInnerText (const NodePtr p, QTextOStream & out) {
    for (NodePtr e = p->firstChild (); e; e = e->nextSibling ()) {
        if (e->id == id_node_text || e->id == id_node_cdata)
            out << e->nodeValue ();
        else
            getInnerText (e, out);
    }
}

QString Node::innerText () const {
    QString buf;
    QTextOStream out (&buf);
    getInnerText (m_self, out);
    return buf;
}

static void getOuterXML (const NodePtr p, QTextOStream & out, int depth) {
    if (!p->isElementNode ()) { // #text or #cdata
        if (p->id == id_node_cdata)
            out << "<![CDATA[" << p->nodeValue () << "]]>" << QChar ('\n');
        else
            out << XMLStringlet (p->nodeValue ()) << QChar ('\n');
    } else {
        Element * e = convertNode <Element> (p);
        QString indent (QString ().fill (QChar (' '), depth));
        out << indent << QChar ('<') << XMLStringlet (e->nodeName ());
        for (AttributePtr a = e->attributes()->first(); a; a = a->nextSibling())
            out << " " << XMLStringlet (a->name ().toString ()) <<
                "=\"" << XMLStringlet (a->value ()) << "\"";
        if (e->hasChildNodes ()) {
            out << QChar ('>') << QChar ('\n');
            for (NodePtr c = e->firstChild (); c; c = c->nextSibling ())
                getOuterXML (c, out, depth + 1);
            out << indent << QString("</") << XMLStringlet (e->nodeName()) <<
                QChar ('>') << QChar ('\n');
        } else
            out << QString ("/>") << QChar ('\n');
    }
}

QString Node::innerXML () const {
    QString buf;
    QTextOStream out (&buf);
    for (NodePtr e = firstChild (); e; e = e->nextSibling ())
        getOuterXML (e, out, 0);
    return buf;
}

QString Node::outerXML () const {
    QString buf;
    QTextOStream out (&buf);
    getOuterXML (m_self, out, 0);
    return buf;
}

Node::PlayType Node::playType () {
    return play_type_none;
}

void Node::opened () {}

void Node::closed () {}

NodeRefListPtr Node::listeners (unsigned int /*event_id*/) {
    return NodeRefListPtr ();
}

bool Node::handleEvent (Event * /*event*/) { return false; }

KDE_NO_EXPORT void Node::propagateEvent (EventPtr event) {
    NodeRefListPtr nl = listeners (event->id ());
    if (nl)
        for (NodeRefItemPtr c = nl->first(); c; c = c->nextSibling ())
            if (c->data)
                c->data->handleEvent (event);
}

void Node::accept (Visitor * v) {
    v->visit (this);
}

KDE_NO_EXPORT
ConnectionPtr Node::connectTo (NodePtr node, unsigned int evt_id) {
    NodeRefListPtr nl = listeners (evt_id);
    if (nl)
        return ConnectionPtr (new Connection (nl, node, this));
    return ConnectionPtr ();
}

QString Node::nodeValue () const {
    return QString ();
}

Surface *Node::getSurface (Mrl *) {
    return NULL;
}

//-----------------------------------------------------------------------------

RefNode::RefNode (NodePtr & d, NodePtr ref)
 : Node (d) {
    setRefNode (ref);
}

void RefNode::setRefNode (const NodePtr ref) {
    ref_node = ref;
    if (ref_node)
        tag_name = QString ("&%1").arg (ref_node->nodeName ()).toUtf8 ();
}

//-----------------------------------------------------------------------------

namespace {
    struct KMPLAYER_NO_EXPORT ParamValue {
        QString val;
        QStringList  * modifications;
        ParamValue (const QString & v) : val (v), modifications (0L) {}
        ~ParamValue () { delete modifications; }
        QString value ();
        void setValue (const QString & v) { val = v; }
    };
    typedef QMap <TrieString, ParamValue *> ParamMap;
}

namespace KMPlayer {
    class KMPLAYER_NO_EXPORT ElementPrivate {
    public:
        ~ElementPrivate ();
        ParamMap params;
        void clear ();
    };
}

KDE_NO_EXPORT QString ParamValue::value () {
    return modifications && modifications->size ()
        ? modifications->back () : val;
}

KDE_NO_CDTOR_EXPORT ElementPrivate::~ElementPrivate () {
    clear ();
}

KDE_NO_EXPORT void ElementPrivate::clear () {
    const ParamMap::iterator e = params.end ();
    for (ParamMap::iterator i = params.begin (); i != e; ++i)
        delete i.data ();
    params.clear ();
}

Element::Element (NodePtr & d, short id)
    : Node (d, id), m_attributes (new AttributeList), d (new ElementPrivate) {}

Element::~Element () {
    delete d;
}

void Element::setParam (const TrieString &param, const QString &val, int *mid) {
    ParamValue * pv = d->params [param];
    if (!pv) {
        pv = new ParamValue (mid ? QString() : val);
        d->params.insert (param, pv);
    }
    if (mid) {
        if (!pv->modifications)
            pv->modifications = new QStringList;
        if (*mid >= 0 && *mid < int (pv->modifications->size ())) {
            (*pv->modifications) [*mid] = val;
        } else {
            *mid = pv->modifications->size ();
            pv->modifications->push_back (val);
        }
    } else
        pv->setValue (val);
    parseParam (param, val);
}

QString Element::param (const TrieString & name) {
    ParamValue * pv = d->params [name];
    if (pv)
        return pv->value ();
    return QString ();
}

void Element::resetParam (const TrieString & param, int mid) {
    ParamValue * pv = d->params [param];
    if (pv && pv->modifications) {
        if (int (pv->modifications->size ()) > mid && mid > -1) {
            (*pv->modifications) [mid] = QString ();
            while (pv->modifications->size () > 0 &&
                    pv->modifications->back ().isNull ())
                pv->modifications->pop_back ();
        }
        QString val = pv->value ();
        if (pv->modifications->size () == 0) {
            delete pv->modifications;
            pv->modifications = 0L;
            val = pv->value ();
            if (val.isNull ()) {
                delete pv;
                d->params.remove (param);
            }
        }
        parseParam (param, val);
    } else
        kError () << "resetting " << param.toString() << " that doesn't exists" << endl;
}

void Element::setAttribute (const TrieString & name, const QString & value) {
    for (AttributePtr a = m_attributes->first (); a; a = a->nextSibling ())
        if (name == a->name ()) {
            a->setValue (value);
            return;
        }
    m_attributes->append (new Attribute (name, value));
}

QString Element::getAttribute (const TrieString & name) {
    for (AttributePtr a = m_attributes->first (); a; a = a->nextSibling ())
        if (name == a->name ())
            return a->value ();
    return QString ();
}

void Element::init () {
    d->clear();
    for (AttributePtr a = attributes ()->first (); a; a = a->nextSibling ())
        setParam (a->name (), a->value ());
}

void Element::reset () {
    d->clear();
    Node::reset ();
}

void Element::clear () {
    m_attributes = new AttributeList; // remove attributes
    d->clear();
    Node::clear ();
}

void Element::setAttributes (AttributeListPtr attrs) {
    m_attributes = attrs;
}

//-----------------------------------------------------------------------------

Attribute::Attribute (const TrieString & n, const QString & v)
  : m_name (n), m_value (v) {}

void Attribute::setName (const TrieString & n) {
    m_name = n;
}

void Attribute::setValue (const QString & v) {
    m_value = v;
}

//-----------------------------------------------------------------------------

static bool hasMrlChildren (const NodePtr & e) {
    for (NodePtr c = e->firstChild (); c; c = c->nextSibling ())
        if (c->isPlayable () || hasMrlChildren (c))
            return true;
    return false;
}

Mrl::Mrl (NodePtr & d, short id)
    : Element (d, id), cached_ismrl_version (~0),
      media_object (NULL),
      aspect (0), repeat (0),
      view_mode (SingleMode),
      resolved (false), bookmarkable (true), access_granted (false) {}

Mrl::~Mrl () {
    if (media_object)
        media_object->destroy ();
}

Node::PlayType Mrl::playType () {
    if (cached_ismrl_version != document()->m_tree_version) {
        bool ismrl = !hasMrlChildren (this);
        cached_play_type = ismrl ? play_type_unknown : play_type_none;
        cached_ismrl_version = document()->m_tree_version;
    }
    return cached_play_type;
}

QString Mrl::absolutePath () {
    QString path = src;
    if (!path.isEmpty()) {
        for (NodePtr e = parentNode (); e; e = e->parentNode ()) {
            Mrl * mrl = e->mrl ();
            if (mrl && !mrl->src.isEmpty () && mrl->src != src) {
                path = KURL (mrl->absolutePath (), src).url ();
                break;
            }
        }
    }
    return path;
}

NodePtr Mrl::childFromTag (const QString & tag) {
    Node * elm = fromXMLDocumentTag (m_doc, tag);
    if (elm)
        return elm;
    return NodePtr ();
}

Mrl * Mrl::linkNode () {
    return this;
}

Mrl * Mrl::mrl () {
    return this;
}

void Mrl::endOfFile () {
    if (state == state_deferred &&
            !isPlayable () && firstChild ()) { // if backend added child links
        state = state_activated;
        firstChild ()->activate ();
    } else
        finish ();
}

void Mrl::activate () {
    resolved |= linkNode ()->resolved;
    if (!resolved && document ()->notify_listener)
        resolved = document ()->notify_listener->resolveURL (this);
    if (!resolved) {
        setState (state_deferred);
        return;
    } else
        linkNode ()->resolved = true;
    if (!isPlayable ()) {
        Element::activate ();
        return;
    }
    setState (state_activated);
    begin ();
}

void Mrl::begin () {
    kDebug () << nodeName () << src << this;
    if (document ()->notify_listener) {
        if (linkNode () != this) {
            linkNode ()->activate ();
            if (linkNode ()->unfinished ())
                setState (state_began);
        } else if (!src.isEmpty ()) {
            if (!media_object)
                media_object = document ()->notify_listener->mediaManager()->createMedia (MediaManager::AudioVideo, this);
            if (media_object->play ())
                setState (state_began);
            else
                deactivate ();
        } else
            deactivate (); // nothing to activate
    }
}

void Mrl::defer () {
    if (media_object)
        media_object->pause ();
    Node::defer ();
}

void Mrl::undefer () {
    if (media_object) {
        media_object->unpause ();
        setState (state_began);
    } else {
        Node::undefer ();
    }
}

void Mrl::deactivate () {
    if (media_object) {
        media_object->destroy ();
        media_object = NULL;
    }
    Node::deactivate ();
}

Surface *Mrl::getSurface (Mrl *mrl) {
    for (NodePtr p = parentNode (); p; p = p->parentNode ())
        if (p->mrl ())
            return p->getSurface (mrl);
    return NULL;
}

bool Mrl::handleEvent (Event *) {
    return false;
}

void Mrl::parseParam (const TrieString & para, const QString & val) {
    if (para == StringPool::attr_src && !src.startsWith ("#")) {
        QString abs = absolutePath ();
        if (abs != src)
            src = val;
        else
            src = KURL (abs, val).url ();
        for (NodePtr c = firstChild (); c; c = c->nextSibling ())
            if (c->mrl () && c->mrl ()->opener.ptr () == this) {
                removeChild (c);
                c->reset();
            }
        resolved = false;
    }
}

//----------------------%<-----------------------------------------------------

Surface::Surface (NodePtr n, const SRect & r)
  : node (n),
    bounds (r),
    xscale (1.0), yscale (1.0),
    background_color (0),
    dirty (false)
#ifdef KMPLAYER_WITH_CAIRO
    , surface (0L)
#endif
{}

Surface::~Surface() {
#ifdef KMPLAYER_WITH_CAIRO
    if (surface)
        cairo_surface_destroy (surface);
#endif
}

void Surface::remove () {
    Surface *sp = parentNode ().ptr ();
    if (sp) {
        sp->markDirty ();
        sp->removeChild (this);
    }
}

void Surface::markDirty () {
    for (Surface *s = this; s; s = s->parentNode ().ptr ())
        s->dirty = true;
}

//-----------------------------------------------------------------------------

EventData::EventData (Node *t, Event *e, EventData *n)
 : target (t), event (e), next (n) {}

//-----------------------------------------------------------------------------

Postpone::Postpone (NodePtr doc) : m_doc (doc) {
    if (m_doc)
        m_doc->document ()->timeOfDay (postponed_time);
}

Postpone::~Postpone () {
    if (m_doc)
        m_doc->document ()->proceed (postponed_time);
}

//-----------------------------------------------------------------------------

static NodePtr dummy_element;

Document::Document (const QString & s, PlayListNotify * n)
 : Mrl (dummy_element, id_node_document),
   notify_listener (n),
   m_tree_version (0),
   m_PostponedListeners (new NodeRefList),
   cur_timeout (-1),
   event_queue (NULL),
   cur_event (NULL) {
    m_doc = m_self; // just-in-time setting fragile m_self to m_doc
    src = s;
    editable = false;
}

Document::~Document () {
    kDebug () << "~Document " << src;
}

static NodePtr getElementByIdImpl (NodePtr n, const QString & id, bool inter) {
    NodePtr elm;
    if (!n->isElementNode ())
        return elm;
    Element * e = convertNode <Element> (n);
    if (e->getAttribute (StringPool::attr_id) == id)
        return n;
    for (NodePtr c = e->firstChild (); c; c = c->nextSibling ()) {
        if (!inter && c->mrl () && c->mrl ()->opener == n)
            continue;
        if ((elm = getElementByIdImpl (c, id, inter)))
            break;
    }
    return elm;
}

NodePtr Document::getElementById (const QString & id) {
    return getElementByIdImpl (this, id, true);
}

NodePtr Document::getElementById (NodePtr n, const QString & id, bool inter) {
    return getElementByIdImpl (n, id, inter);
}

NodePtr Document::childFromTag (const QString & tag) {
    Node * elm = fromXMLDocumentTag (m_doc, tag);
    if (elm)
        return elm;
    return 0L;
}

void Document::dispose () {
    clear ();
    m_doc = 0L;
}

void Document::activate () {
    first_event_time.tv_sec = 0;
    last_event_time = 0;
    Mrl::activate ();
}

void Document::defer () {
    if (resolved)
        postpone_lock = postpone ();
    Mrl::defer ();
}

void Document::undefer () {
    postpone_lock = 0L;
    Mrl::undefer ();
}

void Document::reset () {
    Mrl::reset ();
    if (event_queue) {
        if (notify_listener)
            notify_listener->setTimeout (-1);
        while (event_queue) {
            EventData *ed = event_queue;
            event_queue = ed->next;
            delete ed;
        }
        cur_timeout = -1;
    }
    postpone_lock = 0L;
}

static inline
int diffTime (const struct timeval & tv1, const struct timeval & tv2) {
    //kDebug () << "diffTime sec:" << ((tv1.tv_sec - tv2.tv_sec) * 1000) << " usec:" << ((tv1.tv_usec - tv2.tv_usec) /1000);
    return (tv1.tv_sec - tv2.tv_sec) * 1000 + (tv1.tv_usec - tv2.tv_usec) /1000;
}

static inline void addTime (struct timeval & tv, int ms) {
    tv.tv_sec += (tv.tv_usec + ms*1000) / 1000000;
    tv.tv_usec = (tv.tv_usec + ms*1000) % 1000000;
}

/*static inline void subtractTime (struct timeval & tv, int ms) {
    int sec = ms / 1000;
    int msec = ms % 1000;
    tv.tv_sec -= sec;
    if (tv.tv_usec / 1000 >= msec) {
        tv.tv_usec -= msec * 1000;
    } else {
        tv.tv_sec--;
        tv.tv_usec = 1000000 - (msec - tv.tv_usec / 1000 );
    }
}*/

void Document::timeOfDay (struct timeval & tv) {
    gettimeofday (&tv, 0L);
    if (!first_event_time.tv_sec) {
        first_event_time = tv;
        last_event_time = 0;
    } else
        last_event_time = diffTime (tv, first_event_time) / 100;
}

void Document::insertEvent (Node *n, Event *e, const struct timeval &tv) {
    if (!notify_listener)
        return;
    EventData *prev = NULL;
    EventData *ed = event_queue;
    for (; ed && diffTime (ed->timeout, tv) <= 0; ed = ed->next)
        prev = ed;
    ed = new EventData (n, e, ed);
    ed->timeout = tv;
    if (prev)
        prev->next = ed;
    else
        event_queue = ed;
    //kDebug () << "setTimeout " << ms << " at:" << pos << " tv:" << tv.tv_sec << "." << tv.tv_usec;
}

void Document::setNextTimeout (const struct timeval &now) {
    if (!cur_event) {              // if we're not processing events
        int timeout = 0x7FFFFFFF;
        if (event_queue && active ()) {
            if (!postpone_ref)     // not paused
                timeout = diffTime (event_queue->timeout, now);
            else                   // paused, allow non-timer events
                for (EventData *ed = event_queue; ed; ed = ed->next)
                    if (event_timer != ed->event->id ()) {
                        timeout = diffTime (ed->timeout, now);
                        break;
                    }
        }
        timeout = 0x7FFFFFFF != timeout ? (timeout > 0 ? timeout : 0) : -1;
        if (timeout != cur_timeout) {
            cur_timeout = timeout;
            notify_listener->setTimeout (cur_timeout);
        }
    }
}

Event *Document::postEvent (Node *n, Event *e) {
    int ms = e->id () == event_timer ? static_cast <TimerEvent *> (e)->milli_sec : 0;
    struct timeval now, tv;
    timeOfDay (now);
    tv = now;
    addTime (tv, ms);
    insertEvent (n, e, tv);
    if (postpone_ref || event_queue && event_queue->event.ptr () == e)
        setNextTimeout (now);
    return e;
}

void Document::cancelEvent (Event *e) {
    EventData *prev = NULL;
    if (cur_event && cur_event->event.ptr () == e) {
        cur_event->event = NULL;
    } else {
        for (EventData *ed = event_queue; ed; ed = ed->next) {
            if (e == ed->event.ptr ()) {
                if (prev) {
                    prev->next = ed->next;
                } else {
                    event_queue = ed->next;
                    struct timeval now;
                    timeOfDay (now);
                    setNextTimeout (now);
                }
                delete ed;
                return;
            }
            prev = ed;
        }
        kError () << "Event not found";
    }
}

void Document::timer () {
    struct timeval now;
    cur_event = event_queue;
    if (cur_event) {
        NodePtrW guard = this;
        struct timeval start = cur_event->timeout;

        // handle max 100 timeouts with timeout set to now
        for (int i = 0; cur_event && !postpone_ref && i < 100 && active (); ++i) {
            event_queue = cur_event->next;
            if (!cur_event->target) {
                // some part of document has gone and didn't remove timer
                kError () << "spurious timer" << endl;
            } else {
                EventData *ed = cur_event;
                cur_event->target->handleEvent (cur_event->event.ptr ());
                if (!guard) {
                    delete ed;
                    return;
                }
                if (cur_event->event && cur_event->event->id () == event_timer) {
                    TimerEvent *te = static_cast <TimerEvent *> (cur_event->event.ptr ());
                    if (te->interval) {
                        te->interval = false; // reset interval
                        addTime (cur_event->timeout, te->milli_sec);
                        insertEvent (cur_event->target,
                                cur_event->event.ptr (), cur_event->timeout);
                    }
                }
            }
            delete cur_event;
            cur_event = event_queue;
            if (!cur_event || diffTime (cur_event->timeout, start) > 5)
                break;
        }
        cur_event = NULL;
        timeOfDay (now);
    }
    setNextTimeout (now);
}

PostponePtr Document::postpone () {
    if (postpone_ref)
        return postpone_ref;
    kDebug () << "postpone";
    PostponePtr p = new Postpone (this);
    postpone_ref = p;
    propagateEvent (new PostponedEvent (true));
    struct timeval now;
    timeOfDay (now);
    setNextTimeout (now);
    return p;
}

void Document::proceed (const struct timeval & postponed_time) {
    kDebug () << "proceed";
    postpone_ref = NULL;
    struct timeval now;
    timeOfDay (now);
    if (event_queue && notify_listener) {
        int diff = diffTime (now, postponed_time);
        if (diff > 0) {
            for (EventData *ed = event_queue; ed; ed = ed->next)
                if (ed->event->id () == event_timer)
                    addTime (ed->timeout, diff);
        }
    }
    propagateEvent (new PostponedEvent (false));
    setNextTimeout (now);
}

Surface *Document::getSurface (Mrl *mrl) {
    if (notify_listener)
        return notify_listener->getSurface (mrl);
    return NULL;
}

NodeRefListPtr Document::listeners (unsigned int id) {
    if (id == event_postponed)
        return m_PostponedListeners;
    return Mrl::listeners (id);
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT TextNode::TextNode (NodePtr & d, const QString & s, short i)
 : Node (d, i), text (s) {}

void TextNode::appendText (const QString & s) {
    text += s;
}

QString TextNode::nodeValue () const {
    return text;
}

KDE_NO_EXPORT bool TextNode::expose () const {
    return false;
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT CData::CData (NodePtr & d, const QString & s)
 : TextNode (d, s, id_node_cdata) {}

//-----------------------------------------------------------------------------

DarkNode::DarkNode (NodePtr & d, const QByteArray &n, short id)
 : Element (d, id), name (n) {
}

NodePtr DarkNode::childFromTag (const QString & tag) {
    return new DarkNode (m_doc, tag.toUtf8 ());
}

KDE_NO_EXPORT bool DarkNode::expose () const {
    return false;
}

//-----------------------------------------------------------------------------

GenericURL::GenericURL (NodePtr & d, const QString & s, const QString & name)
 : Mrl (d, id_node_playlist_item) {
    src = s;
    if (!src.isEmpty ())
        setAttribute (StringPool::attr_src, src);
    pretty_name = name;
}

KDE_NO_EXPORT void GenericURL::closed () {
    if (src.isEmpty ())
        src = getAttribute (StringPool::attr_src);
}

//-----------------------------------------------------------------------------

GenericMrl::GenericMrl (NodePtr & d, const QString &s, const QString &name, const QByteArray &tag)
 : Mrl (d, id_node_playlist_item), node_name (tag) {
    src = s;
    if (!src.isEmpty ())
        setAttribute (StringPool::attr_src, src);
    pretty_name = name;
    if (!name.isEmpty ())
        setAttribute (StringPool::attr_name, name);
}

void GenericMrl::closed () {
    if (src.isEmpty ()) {
        src = getAttribute (StringPool::attr_src);
        if (src.isEmpty ())
            src = getAttribute (StringPool::attr_url);
    }
    if (pretty_name.isEmpty ())
        pretty_name = getAttribute (StringPool::attr_name);
}

bool GenericMrl::expose () const {
    return !pretty_name.isEmpty () || //return false if no title and only one
        previousSibling () || nextSibling ();
}

//-----------------------------------------------------------------------------

namespace KMPlayer {

class KMPLAYER_NO_EXPORT DocumentBuilder {
    int m_ignore_depth;
    bool m_set_opener;
    bool m_root_is_first;
    NodePtr m_node;
    NodePtr m_root;
public:
    DocumentBuilder (NodePtr d, bool set_opener);
    ~DocumentBuilder () {}
    bool startTag (const QString & tag, AttributeListPtr attr);
    bool endTag (const QString & tag);
    bool characterData (const QString & data);
    bool cdataData (const QString & data);
#ifdef HAVE_EXPAT
    void cdataStart ();
    void cdataEnd ();
private:
    bool in_cdata;
    QString cdata;
#endif
};

} // namespace KMPlayer

DocumentBuilder::DocumentBuilder (NodePtr d, bool set_opener)
 : m_ignore_depth (0), m_set_opener (set_opener), m_root_is_first (false)
 , m_node (d), m_root (d)
#ifdef HAVE_EXPAT
 , in_cdata (false)
#endif
{}

bool DocumentBuilder::startTag(const QString &tag, AttributeListPtr attr) {
    if (m_ignore_depth) {
        m_ignore_depth++;
        //kDebug () << "Warning: ignored tag " << tag.latin1 () << " ignore depth = " << m_ignore_depth;
    } else {
        NodePtr n = m_node->childFromTag (tag);
        if (!n) {
            kDebug () << "Warning: unknown tag " << tag.latin1 ();
            NodePtr doc = m_root->document ();
            n = new DarkNode (doc, tag.toUtf8 ());
        }
        //kDebug () << "Found tag " << tag;
        if (n->isElementNode ())
            convertNode <Element> (n)->setAttributes (attr);
        if (m_node == n && m_node == m_root)
            m_root_is_first = true;
        else
            m_node->appendChild (n);
        if (m_set_opener && m_node == m_root) {
            Mrl * mrl = n->mrl ();
            if (mrl)
                mrl->opener = m_root;
        }
        n->opened ();
        m_node = n;
    }
    return true;
}

bool DocumentBuilder::endTag (const QString & tag) {
    if (m_ignore_depth) { // endtag to ignore
        m_ignore_depth--;
        kDebug () << "Warning: ignored end tag " << " ignore depth = " << m_ignore_depth;
    } else {  // endtag
        NodePtr n = m_node;
        while (n) {
            if (!strcasecmp (n->nodeName (), tag.local8Bit ().data ()) &&
                    (m_root_is_first || n != m_root)) {
                while (n != m_node) {
                    kWarning() << m_node->nodeName () << " not closed";
                    if (m_root == m_node->parentNode ())
                        break;
                    m_node->closed ();
                    m_node = m_node->parentNode ();
                }
                break;
            }
            if (n == m_root) {
                if (n == m_node) {
                    kError () << "m_node == m_doc, stack underflow " << endl;
                    return false;
                }
                kWarning () << "endtag: no match " << tag.local8Bit ();
                break;
            } else
                 kWarning () << "tag " << tag << " not " << n->nodeName ();
            n = n ->parentNode ();
        }
        //kDebug () << "end tag " << tag;
        m_node->closed ();
        m_node = m_node->parentNode ();
    }
    return true;
}

bool DocumentBuilder::characterData (const QString & data) {
    if (!m_ignore_depth) {
#ifdef HAVE_EXPAT
        if (in_cdata)
            cdata += data;
        else
#endif
            m_node->characterData (data);
    }
    //kDebug () << "characterData " << d.latin1();
    return true;
}

bool DocumentBuilder::cdataData (const QString & data) {
    if (!m_ignore_depth) {
        NodePtr d = m_node->document ();
        m_node->appendChild (new CData (d, data));
    }
    //kDebug () << "cdataData " << d.latin1();
    return true;
}

#ifdef HAVE_EXPAT

void DocumentBuilder::cdataStart () {
    cdata.truncate (0);
    in_cdata = true;
}

void DocumentBuilder::cdataEnd () {
    cdataData (cdata);
    cdata.truncate (0);
    in_cdata = false;
}

static void startTag (void *data, const char * tag, const char **attr) {
    DocumentBuilder * builder = static_cast <DocumentBuilder *> (data);
    AttributeListPtr attributes = new AttributeList;
    if (attr && attr [0]) {
        for (int i = 0; attr[i]; i += 2)
            attributes->append (new Attribute (QString::fromUtf8 (attr [i]), QString::fromUtf8 (attr [i+1])));
    }
    builder->startTag (QString::fromUtf8 (tag), attributes);
}

static void endTag (void *data, const char * tag) {
    DocumentBuilder * builder = static_cast <DocumentBuilder *> (data);
    builder->endTag (QString::fromUtf8 (tag));
}

static void characterData (void *data, const char *s, int len) {
    DocumentBuilder * builder = static_cast <DocumentBuilder *> (data);
    char * buf = new char [len + 1];
    strncpy (buf, s, len);
    buf[len] = 0;
    builder->characterData (QString::fromUtf8 (buf));
    delete [] buf;
}

static void cdataStart (void *data) {
    DocumentBuilder * builder = static_cast <DocumentBuilder *> (data);
    builder->cdataStart ();
}

static void cdataEnd (void *data) {
    DocumentBuilder * builder = static_cast <DocumentBuilder *> (data);
    builder->cdataEnd ();
}

KMPLAYER_EXPORT
void KMPlayer::readXML (NodePtr root, QTextStream & in, const QString & firstline, bool set_opener) {
    bool ok = true;
    DocumentBuilder builder (root, set_opener);
    XML_Parser parser = XML_ParserCreate (0L);
    XML_SetUserData (parser, &builder);
    XML_SetElementHandler (parser, startTag, endTag);
    XML_SetCharacterDataHandler (parser, characterData);
    XML_SetCdataSectionHandler (parser, cdataStart, cdataEnd);
    if (!firstline.isEmpty ()) {
        QString str (firstline + QChar ('\n'));
        QCString buf = str.utf8 ();
        ok = XML_Parse(parser, buf, strlen (buf), false) != XML_STATUS_ERROR;
        if (!ok)
            kWarning () << XML_ErrorString(XML_GetErrorCode(parser)) << " at " << XML_GetCurrentLineNumber(parser) << " col " << XML_GetCurrentColumnNumber(parser);
    }
    if (ok && !in.atEnd ()) {
        QCString buf = in.read ().utf8 ();
        ok = XML_Parse(parser, buf, strlen (buf), true) != XML_STATUS_ERROR;
        if (!ok)
            kWarning () << XML_ErrorString(XML_GetErrorCode(parser)) << " at " << XML_GetCurrentLineNumber(parser) << " col " << XML_GetCurrentColumnNumber(parser);
    }
    XML_ParserFree(parser);
    root->normalize ();
    //return ok;
}

//-----------------------------------------------------------------------------
#else // HAVE_EXPAT

namespace {

class KMPLAYER_NO_EXPORT SimpleSAXParser {
public:
    SimpleSAXParser (DocumentBuilder & b) : builder (b), position (0), m_attributes (new AttributeList), equal_seen (false), in_dbl_quote (false), in_sngl_quote (false), have_error (false), no_entitity_look_ahead (false), have_next_char (false) {}
    virtual ~SimpleSAXParser () {};
    bool parse (QTextStream & d);
private:
    QTextStream * data;
    DocumentBuilder & builder;
    int position;
    QChar next_char;
    enum Token { tok_empty, tok_text, tok_white_space, tok_angle_open,
        tok_equal, tok_double_quote, tok_single_quote, tok_angle_close,
        tok_slash, tok_exclamation, tok_amp, tok_hash, tok_semi_colon,
        tok_question_mark, tok_cdata_start };
    enum State {
        InTag, InStartTag, InPITag, InDTDTag, InEndTag, InAttributes, InContent, InCDATA, InComment
    };
    struct TokenInfo {
        TokenInfo () : token (tok_empty) {}
        Token token;
        QString string;
        SharedPtr <TokenInfo> next;
    };
    typedef SharedPtr <TokenInfo> TokenInfoPtr;
    struct StateInfo {
        StateInfo (State s, SharedPtr <StateInfo> n) : state (s), next (n) {}
        State state;
        QString data;
        SharedPtr <StateInfo> next;
    };
    SharedPtr <StateInfo> m_state;
    TokenInfoPtr next_token, token, prev_token;
    // for element reading
    QString tagname;
    AttributeListPtr m_attributes;
    QString attr_name, attr_value;
    QString cdata;
    bool equal_seen;
    bool in_dbl_quote;
    bool in_sngl_quote;
    bool have_error;
    bool no_entitity_look_ahead;
    bool have_next_char;

    bool readTag ();
    bool readEndTag ();
    bool readAttributes ();
    bool readPI ();
    bool readDTD ();
    bool readCDATA ();
    bool readComment ();
    bool nextToken ();
    void push ();
    void push_attribute ();
};

} // namespace

KMPLAYER_EXPORT
void KMPlayer::readXML (NodePtr root, QTextStream & in, const QString & firstline, bool set_opener) {
    DocumentBuilder builder (root, set_opener);
    SimpleSAXParser parser (builder);
    if (!firstline.isEmpty ()) {
        QString str (firstline + QChar ('\n'));
        QTextStream fl_in (&str, IO_ReadOnly);
        parser.parse (fl_in);
    }
    if (!in.atEnd ())
        parser.parse (in);
    for (NodePtr e = root; e; e = e->parentNode ())
        e->closed ();
    //doc->normalize ();
    //kDebug () << root->outerXML ();
}

void SimpleSAXParser::push () {
    if (next_token->string.length ()) {
        prev_token = token;
        token = next_token;
        if (prev_token)
            prev_token->next = token;
        next_token = TokenInfoPtr (new TokenInfo);
        //kDebug () << "push " << token->string;
    }
}

void SimpleSAXParser::push_attribute () {
    //kDebug () << "attribute " << attr_name.latin1 () << "=" << attr_value.latin1 ();
    m_attributes->append (new Attribute (attr_name, attr_value));
    attr_name.truncate (0);
    attr_value.truncate (0);
    equal_seen = in_sngl_quote = in_dbl_quote = false;
}

bool SimpleSAXParser::nextToken () {
    TokenInfoPtr cur_token = token;
    while (!data->atEnd () && cur_token == token && !(token && token->next)) {
        if (have_next_char)
            have_next_char = false;
        else
            *data >> next_char;
        bool append_char = true;
        if (next_char.isSpace ()) {
            if (next_token->token != tok_white_space)
                push ();
            next_token->token = tok_white_space;
        } else if (!next_char.isLetterOrNumber ()) {
            if (next_char == QChar ('#')) {
                //if (next_token->token == tok_empty) { // check last item on stack &
                    push ();
                    next_token->token = tok_hash;
                //}
            } else if (next_char == QChar ('/')) {
                push ();
                next_token->token = tok_slash;
            } else if (next_char == QChar ('!')) {
                push ();
                next_token->token = tok_exclamation;
            } else if (next_char == QChar ('?')) {
                push ();
                next_token->token = tok_question_mark;
            } else if (next_char == QChar ('<')) {
                push ();
                next_token->token = tok_angle_open;
            } else if (next_char == QChar ('>')) {
                push ();
                next_token->token = tok_angle_close;
            } else if (next_char == QChar (';')) {
                push ();
                next_token->token = tok_semi_colon;
            } else if (next_char == QChar ('=')) {
                push ();
                next_token->token = tok_equal;
            } else if (next_char == QChar ('"')) {
                push ();
                next_token->token = tok_double_quote;
            } else if (next_char == QChar ('\'')) {
                push ();
                next_token->token = tok_single_quote;
            } else if (next_char == QChar ('&')) {
                push ();
                if (no_entitity_look_ahead) {
                    have_next_char = true;
                    break;
                }
                append_char = false;
                no_entitity_look_ahead = true;
                TokenInfoPtr tmp = token;
                TokenInfoPtr prev_tmp = prev_token;
                if (nextToken () && token->token == tok_text &&
                        nextToken () && token->token == tok_semi_colon) {
                    if (prev_token->string == QString ("amp"))
                        token->string = QChar ('&');
                    else if (prev_token->string == QString ("lt"))
                        token->string = QChar ('<');
                    else if (prev_token->string == QString ("gt"))
                        token->string = QChar ('>');
                    else if (prev_token->string == QString ("quot"))
                        token->string = QChar ('"');
                    else if (prev_token->string == QString ("apos"))
                        token->string = QChar ('\'');
                    else if (prev_token->string == QString ("copy"))
                        token->string = QChar (169);
                    else
                        token->string = QChar ('?');// TODO lookup more ..
                    token->token = tok_text;
                    if (tmp) { // cut out the & xxx ; tokens
                        tmp->next = token;
                        token = tmp;
                    }
                    //kDebug () << "entity found "<<prev_token->string;
                } else if (token->token == tok_hash &&
                        nextToken () && token->token == tok_text &&
                        nextToken () && token->token == tok_semi_colon) {
                    //kDebug () << "char entity found " << prev_token->string << prev_token->string.toInt (0L, 16);
                    token->token = tok_text;
                    if (!prev_token->string.startsWith (QChar ('x')))
                        token->string = QChar (prev_token->string.toInt ());
                    else
                        token->string = QChar (prev_token->string.mid (1).toInt (0L, 16));
                    if (tmp) { // cut out the '& # xxx ;' tokens
                        tmp->next = token;
                        token = tmp;
                    }
                } else {
                    token = tmp; // restore and insert the lost & token
                    tmp = TokenInfoPtr (new TokenInfo);
                    tmp->token = tok_amp;
                    tmp->string += QChar ('&');
                    tmp->next = token->next;
                    if (token)
                        token->next = tmp;
                    else
                        token = tmp; // hmm
                }
                no_entitity_look_ahead = false;
                prev_token = prev_tmp;
            } else if (next_token->token != tok_text) {
                push ();
                next_token->token = tok_text;
            }
        } else if (next_token->token != tok_text) {
            push ();
            next_token->token = tok_text;
        }
        if (append_char)
            next_token->string += next_char;
        if (next_token->token == tok_text &&
                next_char == QChar ('[' ) && next_token->string == "[CDATA[") {
            next_token->token = tok_cdata_start;
            break;
        }
    }
    if (token == cur_token) {
        if (token && token->next) {
            prev_token = token;
            token = token->next;
        } else if (next_token->string.length ()) {
            push (); // last token
        } else
            return false;
        return true;
    }
    return true;
}

bool SimpleSAXParser::readAttributes () {
    bool closed = false;
    while (true) {
        if (!nextToken ()) return false;
        //kDebug () << "readAttributes " << token->string.latin1();
        if ((in_dbl_quote && token->token != tok_double_quote) ||
                    (in_sngl_quote && token->token != tok_single_quote)) {
            attr_value += token->string;
        } else if (token->token == tok_equal) {
            if (attr_name.isEmpty ())
                return false;
            if (equal_seen)
                attr_value += token->string; // EQ=a=2c ???
            //kDebug () << "equal_seen";
            equal_seen = true;
        } else if (token->token == tok_white_space) {
            if (!attr_value.isEmpty ())
                push_attribute ();
        } else if (token->token == tok_single_quote) {
            if (!equal_seen)
                attr_name += token->string; // D'OH=xxx ???
            else if (in_sngl_quote) { // found one
                push_attribute ();
            } else if (attr_value.isEmpty ())
                in_sngl_quote = true;
            else
                attr_value += token->string;
        } else if (token->token == tok_double_quote) {
            if (!equal_seen)
                attr_name += token->string; // hmm
            else if (in_dbl_quote) { // found one
                push_attribute ();
            } else if (attr_value.isEmpty ())
                in_dbl_quote = true;
            else
                attr_value += token->string;
            //kDebug () << "in_dbl_quote:"<< in_dbl_quote;
        } else if (token->token == tok_slash) {
            TokenInfoPtr mark_token = token;
            if (nextToken () &&
                    (token->token != tok_white_space || nextToken()) &&//<e / >
                    token->token == tok_angle_close) {
            //kDebug () << "close mark:";
                closed = true;
                break;
            } else {
                token = mark_token;
            //kDebug () << "not end mark:"<< equal_seen;
                if (equal_seen)
                    attr_value += token->string; // ABBR=w/o ???
                else
                    attr_name += token->string;
            }
        } else if (token->token == tok_angle_close) {
            if (!attr_name.isEmpty ())
                push_attribute ();
            break;
        } else if (equal_seen) {
            attr_value += token->string;
        } else {
            attr_name += token->string;
        }
    }
    m_state = m_state->next;
    if (m_state->state == InPITag) {
        if (tagname == QString ("xml")) {
            /*const AttributeMap::const_iterator e = attr.end ();
            for (AttributeMap::const_iterator i = attr.begin (); i != e; ++i)
                if (!strcasecmp (i.key ().latin1 (), "encoding"))
                  kDebug () << "encodeing " << i.data().latin1();*/
        }
    } else {
        have_error = builder.startTag (tagname, m_attributes);
        if (closed)
            have_error &= builder.endTag (tagname);
        //kDebug () << "readTag " << tagname << " closed:" << closed << " ok:" << have_error;
    }
    m_state = m_state->next; // pop Node or PI
    return true;
}

bool SimpleSAXParser::readPI () {
    // TODO: <?xml .. encoding="ENC" .. ?>
    if (!nextToken ()) return false;
    if (token->token == tok_text && !token->string.compare ("xml")) {
        m_state = new StateInfo (InAttributes, m_state);
        return readAttributes ();
    } else {
        while (nextToken ())
            if (token->token == tok_angle_close) {
                m_state = m_state->next;
                return true;
            }
    }
    return false;
}

bool SimpleSAXParser::readDTD () {
    //TODO: <!ENTITY ..>
    if (!nextToken ()) return false;
    if (token->token == tok_text && token->string.startsWith (QString ("--"))) {
        m_state = new StateInfo (InComment, m_state->next); // note: pop DTD
        return readComment ();
    }
    //kDebug () << "readDTD: " << token->string.latin1 ();
    if (token->token == tok_cdata_start) {
        m_state = new StateInfo (InCDATA, m_state->next); // note: pop DTD
        if (token->next) {
            cdata = token->next->string;
            token->next = 0;
        } else {
            cdata = next_token->string;
            next_token->string.truncate (0);
            next_token->token = tok_empty;
        }
        return readCDATA ();
    }
    while (nextToken ())
        if (token->token == tok_angle_close) {
            m_state = m_state->next;
            return true;
        }
    return false;
}

bool SimpleSAXParser::readCDATA () {
    while (!data->atEnd ()) {
        *data >> next_char;
        if (next_char == QChar ('>') && cdata.endsWith (QString ("]]"))) {
            cdata.truncate (cdata.length () - 2);
            m_state = m_state->next;
            if (m_state->state == InContent)
                have_error = builder.cdataData (cdata);
            else if (m_state->state == InAttributes) {
                if (equal_seen)
                    attr_value += cdata;
                else
                    attr_name += cdata;
            }
            cdata.truncate (0);
            return true;
        }
        cdata += next_char;
    }
    return false;
}

bool SimpleSAXParser::readComment () {
    while (nextToken ()) {
        if (token->token == tok_angle_close && prev_token)
            if (prev_token->string.endsWith (QString ("--"))) {
                m_state = m_state->next;
                return true;
            }
    }
    return false;
}

bool SimpleSAXParser::readEndTag () {
    if (!nextToken ()) return false;
    if (token->token == tok_white_space)
        if (!nextToken ()) return false;
    tagname = token->string;
    if (!nextToken ()) return false;
    if (token->token == tok_white_space)
        if (!nextToken ()) return false;
    if (token->token != tok_angle_close)
        return false;
    have_error = builder.endTag (tagname);
    m_state = m_state->next;
    return true;
}

// TODO: <!ENTITY ..> &#1234;
bool SimpleSAXParser::readTag () {
    if (!nextToken ()) return false;
    if (token->token == tok_exclamation) {
        m_state = new StateInfo (InDTDTag, m_state->next);
    //kDebug () << "readTag: " << token->string.latin1 ();
        return readDTD ();
    }
    if (token->token == tok_white_space)
        if (!nextToken ()) return false; // allow '< / foo', '<  foo', '< ? foo'
    if (token->token == tok_question_mark) {
        m_state = new StateInfo (InPITag, m_state->next);
        return readPI ();
    }
    if (token->token == tok_slash) {
        m_state = new StateInfo (InEndTag, m_state->next);
        return readEndTag ();
    }
    if (token->token != tok_text)
        return false; // FIXME entities
    tagname = token->string;
    //kDebug () << "readTag " << tagname.latin1();
    m_state = new StateInfo (InAttributes, m_state);
    return readAttributes ();
}

bool SimpleSAXParser::parse (QTextStream & d) {
    data = &d;
    if (!next_token) {
        next_token = TokenInfoPtr (new TokenInfo);
        m_state = new StateInfo (InContent, m_state);
    }
    bool ok = true;
    bool in_character_data = false;
    QString white_space;
    while (ok) {
        switch (m_state->state) {
            case InTag:
                ok = readTag ();
                break;
            case InPITag:
                ok = readPI ();
                break;
            case InDTDTag:
                ok = readDTD ();
                break;
            case InEndTag:
                ok = readEndTag ();
                break;
            case InAttributes:
                ok = readAttributes ();
                break;
            case InCDATA:
                ok = readCDATA ();
                break;
            case InComment:
                ok = readComment ();
                break;
            default:
                if ((ok = nextToken ())) {
                    if (token->token == tok_angle_open) {
                        attr_name.truncate (0);
                        attr_value.truncate (0);
                        m_attributes = new AttributeList;
                        equal_seen = in_sngl_quote = in_dbl_quote = false;
                        m_state = new StateInfo (InTag, m_state);
                        ok = readTag ();
                        in_character_data = false;
                        white_space.truncate (0);
                    } else if (token->token == tok_white_space) {
                        white_space += token->string;
                    } else {
                        if (!white_space.isEmpty ()) {
                            if (!in_character_data) {
                                int pos = white_space.lastIndexOf (QChar ('\n'));
                                if (pos > -1)
                                    white_space = white_space.mid (pos + 1);
                            }
                            have_error = builder.characterData (white_space);
                            white_space.truncate (0);
                        }
                        have_error = builder.characterData (token->string);
                        in_character_data = true;
                    }
                }
        }
        if (!m_state)
            return true; // end document
    }
    return false; // need more data
}

#endif // HAVE_EXPAT
