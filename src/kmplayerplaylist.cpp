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
#ifdef KMPLAYER_WITH_EXPAT
#include <expat.h>
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
KMPLAYER_EXPORT int shared_data_count;
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

QTextStream &KMPlayer::operator << (QTextStream &out, const XMLStringlet &txt) {
    int len = int (txt.str.size ());
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

Connection::Connection (Node *invoker, Node *receiver)
 : connectee (invoker), connecter (receiver) {
#ifdef KMPLAYER_TEST_CONNECTION
    connection_counter++;
#endif
}

ConnectionLink::ConnectionLink () : connection (NULL) {}

ConnectionLink::~ConnectionLink () {
    disconnect ();
}

bool ConnectionLink::connect (Node *send, MessageType msg, Node *rec) {
    disconnect ();
    ConnectionList *list = nodeMessageReceivers (send, msg);
    if (list) {
        connection = new Connection (send, rec);
        connection->list = list;
        connection->link = &connection;
        connection->prev = list->link_last;
        connection->next = NULL;
        if (list->link_last)
            list->link_last->next = connection;
        else
            list->link_first = connection;
        list->link_last = connection;
    }
    return list;
}

void ConnectionLink::disconnect () const {
    if (connection) {
        Connection *tmp = connection;
        if (tmp->prev)
            tmp->prev->next = tmp->next;
        else
            tmp->list->link_first = tmp->next;
        if (tmp->next)
            tmp->next->prev = tmp->prev;
        else
            tmp->list->link_last = tmp->prev;
        *tmp->link = NULL;
        if (tmp->list->link_next == tmp)
            tmp->list->link_next = tmp->next;
        delete tmp;
    }
    ASSERT (!connection);
}

void ConnectionLink::assign (const ConnectionLink *link) const {
    disconnect ();
    connection = link->connection;
    link->connection = NULL;
    if (connection)
        connection->link = &connection;
}

Node *ConnectionLink::signaler () const {
    return connection ? connection->connectee.ptr () : NULL;
}

ConnectionList::ConnectionList ()
    : link_first (NULL), link_last (NULL), link_next (NULL) {}

ConnectionList::~ConnectionList () {
    clear ();
}

void ConnectionList::clear () {
    while (link_first) {
        Connection *tmp = link_first;
        link_first = tmp->next;
        *tmp->link = NULL;
        delete tmp;
    }
    link_last = link_next = NULL;
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT TimerPosting::TimerPosting (int ms, unsigned eid)
 : Posting (NULL, MsgEventTimer),
   event_id (eid),
   milli_sec (ms),
   interval (false) {}

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

void Matrix::getWH (Single &w, Single &h) const {
    w *= a;
    h *= d;
}

IRect Matrix::toScreen (const SRect &rect) const {
    return IRect (
            (int) (Single (rect.x () * a) + tx),
            (int) (Single (rect.y () * d) + ty),
            (int) (rect.width () * a),
            (int) (rect.height () * d));
}

SRect Matrix::toUser (const IRect &rect) const {
    if (a > 0.00001 && d > 0.00001) {
        return SRect (
                Single ((Single (rect.x ()) - tx) / a),
                Single ((Single (rect.y ()) - ty) / d),
                rect.width () / a,
                rect.height () / d);
    } else {
        kWarning () << "Not invering " << a << ", " << d << " scale";
        return SRect ();
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
   auxiliary_node (false), editable (true), open (false) {}

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
    if (state != nstate && (state_init == nstate || state != state_resetting)) {
        State old = state;
        state = nstate;
        if (document ()->notify_listener)
            document()->notify_listener->stateElementChanged (this, old, state);
    }
}

bool Node::expose () const {
    return true;
}

QString Node::caption () const {
    return QString ();
}

void Node::setCaption (const QString &) {
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
        if (firstChild () && firstChild ()->state > state_init) {
            state = state_began;
        } else {
            setState (state_activated);
            activate ();
        }
    } else
        kWarning () << nodeName () << " call on not deferred element";
}

void Node::finish () {
    if (active ()) {
        setState (state_finished);
        if (m_parent)
            document ()->post (m_parent, new Posting (this, MsgChildFinished));
        else
            deactivate (); // document deactivates itself on finish
    } else
        kWarning () <<"Node::finish () call on not active element";
}

void Node::deactivate () {
    //kDebug () << nodeName () << " Node::deactivate";
    bool need_finish (unfinished ());
    if (state_resetting != state)
        setState (state_deactivated);
    for (NodePtr e = firstChild (); e; e = e->nextSibling ()) {
        if (e->state > state_init && e->state < state_deactivated)
            e->deactivate ();
        else
            break; // remaining not yet activated
    }
    if (need_finish && m_parent && m_parent->active ())
        document ()->post (m_parent, new Posting (this, MsgChildFinished));
}

void Node::reset () {
    //kDebug () << nodeName () << " Node::reset";
    if (active ()) {
        setState (state_resetting);
        deactivate ();
    }
    setState (state_init);
    for (NodePtr e = firstChild (); e; e = e->nextSibling ()) {
        if (e->state != state_init)
            e->reset ();
        // else
        //    break; // rest not activated yet
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

template <>
void TreeNode<Node>::appendChild (Node *c) {
    static_cast <Node *> (this)->document()->m_tree_version++;
    ASSERT (!c->parentNode ());
    appendChildImpl (c);
}

template <>
KDE_NO_EXPORT void TreeNode<Node>::insertBefore (Node *c, Node *b) {
    ASSERT (!c->parentNode ());
    static_cast <Node *> (this)->document()->m_tree_version++;
    insertBeforeImpl (c, b);
}

template <>
void TreeNode<Node>::removeChild (NodePtr c) {
    static_cast <Node *> (this)->document()->m_tree_version++;
    removeChildImpl (c);
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

Node *Node::childFromTag (const QString &) {
    return NULL;
}

KDE_NO_EXPORT void Node::characterData (const QString & s) {
    document()->m_tree_version++;
    if (!m_last_child || m_last_child->id != id_node_text)
        appendChild (new TextNode (m_doc, s));
    else
        convertNode <TextNode> (m_last_child)->appendText (s);
}

void Node::normalize () {
    Node *e = firstChild ();
    while (e) {
        Node *tmp = e->nextSibling ();
        if (!e->isElementNode () && e->id == id_node_text) {
            QString val = e->nodeValue ().simplifyWhiteSpace ();
            if (val.isEmpty ())
                removeChild (e);
            else
                static_cast <TextNode *> (e)->setText (val);
        } else
            e->normalize ();
        e = tmp;
    }
}

static void getInnerText (const Node *p, QTextOStream & out) {
    for (Node *e = p->firstChild (); e; e = e->nextSibling ()) {
        if (e->id == id_node_text || e->id == id_node_cdata)
            out << e->nodeValue ();
        else
            getInnerText (e, out);
    }
}

QString Node::innerText () const {
    QString buf;
    QTextOStream out (&buf);
    getInnerText (this, out);
    return buf;
}

static void getOuterXML (const Node *p, QTextOStream & out, int depth) {
    if (!p->isElementNode ()) { // #text or #cdata
        if (p->id == id_node_cdata)
            out << "<![CDATA[" << p->nodeValue () << "]]>" << QChar ('\n');
        else
            out << XMLStringlet (p->nodeValue ()) << QChar ('\n');
    } else {
        const Element *e = static_cast <const Element *> (p);
        QString indent (QString ().fill (QChar (' '), depth));
        out << indent << QChar ('<') << XMLStringlet (e->nodeName ());
        for (Attribute *a = e->attributes()->first(); a; a = a->nextSibling())
            out << " " << XMLStringlet (a->name ().toString ()) <<
                "=\"" << XMLStringlet (a->value ()) << "\"";
        if (e->hasChildNodes ()) {
            out << QChar ('>') << QChar ('\n');
            for (Node *c = e->firstChild (); c; c = c->nextSibling ())
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
    for (Node *e = firstChild (); e; e = e->nextSibling ())
        getOuterXML (e, out, 0);
    return buf;
}

QString Node::outerXML () const {
    QString buf;
    QTextOStream out (&buf);
    getOuterXML (this, out, 0);
    return buf;
}

Node::PlayType Node::playType () {
    return play_type_none;
}

void Node::opened () {
    open = true;
}

void Node::closed () {
    open = false;
}

void Node::message (MessageType msg, void *content) {
    switch (msg) {

    case MsgChildFinished: {
        Posting *post = (Posting *) content;
        if (unfinished ()) {
            if (post->source->state == state_finished)
                post->source->deactivate ();
            if (post->source && post->source->nextSibling ())
                post->source->nextSibling ()->activate ();
            else
                finish (); // we're done
        }
        break;
    }

    default:
        break;
    }
}

void *Node::role (RoleType msg, void *content) {
    switch (msg) {
    case RoleReady:
        return MsgBool (true);
    default:
        break;
    }
    return NULL;
}

KDE_NO_EXPORT void Node::deliver (MessageType msg, void *content) {
    ConnectionList *nl = nodeMessageReceivers (this, msg);
    if (nl)
        for (Connection *c = nl->first(); c; c = nl->next ())
            if (c->connecter)
                c->connecter->message (msg, content);
}

void Node::accept (Visitor * v) {
    v->visit (this);
}

QString Node::nodeValue () const {
    return QString ();
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

void Element::setParam (const TrieString &name, const QString &val, int *mid) {
    ParamValue * pv = d->params [name];
    if (!pv) {
        pv = new ParamValue (mid ? getAttribute (name) : val);
        d->params.insert (name, pv);
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
    } else {
        pv->setValue (val);
    }
    parseParam (name, val);
}

QString Element::param (const TrieString & name) {
    ParamValue * pv = d->params [name];
    if (pv)
        return pv->value ();
    return getAttribute (name);
}

void Element::resetParam (const TrieString &name, int mid) {
    ParamValue * pv = d->params [name];
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
            if (val.isNull ()) {
                delete pv;
                d->params.remove (name);
            }
        }
        parseParam (name, val);
    } else
        kError () << "resetting " << name.toString() << " that doesn't exists" << endl;
}

void Element::setAttribute (const TrieString & name, const QString & value) {
    for (Attribute *a = m_attributes->first (); a; a = a->nextSibling ())
        if (name == a->name ()) {
            if (value.isNull ())
                m_attributes->remove (a);
            else
                a->setValue (value);
            return;
        }
    if (!value.isNull ())
        m_attributes->append (new Attribute (TrieString (), name, value));
}

QString Element::getAttribute (const TrieString & name) {
    for (Attribute *a = m_attributes->first (); a; a = a->nextSibling ())
        if (name == a->name ())
            return a->value ();
    return QString ();
}

void Element::init () {
    d->clear();
    for (Attribute *a = attributes ()->first (); a; a = a->nextSibling ())
        parseParam (a->name (), a->value ());
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

void Element::accept (Visitor * v) {
    v->visit (this);
}

//-----------------------------------------------------------------------------

Attribute::Attribute (const TrieString &ns, const TrieString &n, const QString &v)
  : m_namespace (ns), m_name (n), m_value (v) {}

void Attribute::setName (const TrieString & n) {
    m_name = n;
}

void Attribute::setValue (const QString & v) {
    m_value = v;
}

//-----------------------------------------------------------------------------

QString Title::caption () const {
    return title;
}

void Title::setCaption (const QString &) {
}

//-----------------------------------------------------------------------------

static bool hasMrlChildren (const NodePtr & e) {
    for (Node *c = e->firstChild (); c; c = c->nextSibling ())
        if (c->isPlayable () || hasMrlChildren (c))
            return true;
    return false;
}

Mrl::Mrl (NodePtr & d, short id)
    : Title (d, id), cached_ismrl_version (~0),
      media_info (NULL),
      aspect (0), repeat (0),
      view_mode (SingleMode),
      resolved (false), bookmarkable (true), access_granted (false) {}

Mrl::~Mrl () {
    if (media_info)
        delete media_info;
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
    if (!path.isEmpty() && !path.startsWith ("tv:/")) {
        for (Node *e = parentNode (); e; e = e->parentNode ()) {
            Mrl * mrl = e->mrl ();
            if (mrl && !mrl->src.isEmpty () && mrl->src != src) {
                path = KURL (mrl->absolutePath (), src).url ();
                break;
            }
        }
    }
    return path;
}

Node *Mrl::childFromTag (const QString & tag) {
    Node * elm = fromXMLDocumentTag (m_doc, tag);
    if (elm)
        return elm;
    return NULL;
}

Mrl * Mrl::linkNode () {
    return this;
}

Mrl * Mrl::mrl () {
    return this;
}

void Mrl::message (MessageType msg, void *content) {
    switch (msg) {
    case MsgMediaReady:
        linkNode ()->resolved = true;
        if (state == state_deferred) {
            if (isPlayable ()) {
                setState (state_activated);
                begin ();
            } else {
                Element::activate ();
            }
        }
        break;

    case MsgMediaFinished:
        if (state == state_deferred &&
                !isPlayable () && firstChild ()) {//if backend added child links
            state = state_activated;
            firstChild ()->activate ();
        } else
            finish ();
        break;

    default:
        break;
    }
    Node::message (msg, content);
}

void *Mrl::role (RoleType msg, void *content) {
    switch (msg) {

    case RoleChildDisplay:
        for (Node *p = parentNode (); p; p = p->parentNode ())
            if (p->mrl ())
                return p->role (msg, content);
        return NULL;

    default:
        break;
    }
    return Node::role (msg, content);
}

void Mrl::activate () {
    resolved |= linkNode ()->resolved;
    if (!resolved && linkNode () == this && isPlayable ()) {
        setState (state_deferred);
        media_info = new MediaInfo (this, MediaManager::AudioVideo);
        resolved = media_info->wget (absolutePath ());
    } else if (isPlayable ()) {
        setState (state_activated);
        begin ();
    } else {
        Element::activate ();
    }
}

void Mrl::begin () {
    kDebug () << nodeName () << src << this;
    if (linkNode () != this) {
        linkNode ()->activate ();
        if (linkNode ()->unfinished ())
            setState (state_began);
    } else if (!src.isEmpty ()) {
        if (!media_info)
            media_info = new MediaInfo (this, MediaManager::AudioVideo);
        if (!media_info->media)
            media_info->create ();
        if (media_info->media->play ())
            setState (state_began);
        else
            deactivate ();
    } else {
        deactivate (); // nothing to activate
    }
}

void Mrl::defer () {
    if (media_info && media_info->media)
        media_info->media->pause ();
    Node::defer ();
}

void Mrl::undefer () {
    if (media_info && media_info->media) {
        media_info->media->unpause ();
        setState (state_began);
    } else {
        Node::undefer ();
    }
}

void Mrl::deactivate () {
    if (media_info) {
        delete media_info;
        media_info = NULL;
    }
    Node::deactivate ();
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

unsigned int Mrl::parseTimeString (const QString &ts) {
    QString s (ts);
    int multiply[] = { 1, 60, 60 * 60, 24 * 60 * 60, 0 };
    int mpos = 0;
    double d = 0;
    while (!s.isEmpty () && multiply[mpos]) {
        int p = s.lastIndexOf (QChar (':'));
        QString t = p >= 0 ? s.mid (p + 1) : s;
        d += multiply[mpos++] * t.toDouble();
        s = p >= 0 ? s.left (p) : QString ();
    }
    if (d > 0.01)
        return (unsigned int) (d * 100);
    return 0;
}

//----------------------%<-----------------------------------------------------

EventData::EventData (Node *t, Posting *e, EventData *n)
 : target (t), event (e), next (n) {}

EventData::~EventData () {
    delete event;
}
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
   event_queue (NULL),
   paused_queue (NULL),
   cur_event (NULL),
   cur_timeout (-1) {
    m_doc = m_self; // just-in-time setting fragile m_self to m_doc
    src = s;
    editable = false;
}

Document::~Document () {
    kDebug () << "~Document " << src;
}

static Node *getElementByIdImpl (Node *n, const QString & id, bool inter) {
    NodePtr elm;
    if (!n->isElementNode ())
        return NULL;
    Element *e = static_cast <Element *> (n);
    if (e->getAttribute (StringPool::attr_id) == id)
        return n;
    for (Node *c = e->firstChild (); c; c = c->nextSibling ()) {
        if (!inter && c->mrl () && c->mrl ()->opener.ptr () == n)
            continue;
        if ((elm = getElementByIdImpl (c, id, inter)))
            break;
    }
    return elm;
}

Node *Document::getElementById (const QString & id) {
    return getElementByIdImpl (this, id, true);
}

Node *Document::getElementById (Node *n, const QString & id, bool inter) {
    return getElementByIdImpl (n, id, inter);
}

Node *Document::childFromTag (const QString & tag) {
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

KDE_NO_CDTOR_EXPORT UpdateEvent::UpdateEvent (Document *doc, unsigned int skip)
 : skipped_time (skip) {
    struct timeval tv;
    doc->timeOfDay (tv);
    cur_event_time = doc->last_event_time;
}

//-----------------------------------------------------------------------------
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
    } else {
        last_event_time = diffTime (tv, first_event_time);
    }
}

static bool postponedSensible (MessageType msg) {
    return msg == MsgEventTimer ||
        msg == MsgEventStarted ||
        msg == MsgEventStopped;
}

void Document::insertPosting (Node *n, Posting *e, const struct timeval &tv) {
    if (!notify_listener)
        return;
    bool postponed_sensible = postponedSensible (e->message);
    EventData *prev = NULL;
    EventData *ed = event_queue;
    for (; ed; ed = ed->next) {
        int diff = diffTime (ed->timeout, tv);
        bool psens = postponedSensible (ed->event->message);
        if ((diff > 0 && postponed_sensible == psens) || (!postponed_sensible && psens))
            break;
        prev = ed;
    }
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
        if (event_queue && active () &&
                (!postpone_ref || !postponedSensible (event_queue->event->message)))
            timeout = diffTime (event_queue->timeout, now);
        timeout = 0x7FFFFFFF != timeout ? (timeout > 0 ? timeout : 0) : -1;
        if (timeout != cur_timeout) {
            cur_timeout = timeout;
            notify_listener->setTimeout (cur_timeout);
        }
    }
}

void Document::updateTimeout () {
    if (!postpone_ref && event_queue && notify_listener) {
        struct timeval now;
        if (cur_event)
            now = cur_event->timeout;
        else
            timeOfDay (now);
        setNextTimeout (now);
    }
}

Posting *Document::post (Node *n, Posting *e) {
    int ms = e->message == MsgEventTimer
        ? static_cast<TimerPosting *>(e)->milli_sec
        : 0;
    struct timeval now, tv;
    if (cur_event)
        now = cur_event->timeout;
    else
        timeOfDay (now);
    tv = now;
    addTime (tv, ms);
    insertPosting (n, e, tv);
    if (postpone_ref || event_queue->event == e)
        setNextTimeout (now);
    return e;
}

static EventData *findPosting (EventData *queue, EventData **prev, const Posting *e) {
    *prev = NULL;
    for (EventData *ed = queue; ed; ed = ed->next) {
        if (e == ed->event)
            return ed;
        *prev = ed;
    }
    return NULL;
}

void Document::cancelPosting (Posting *e) {
    if (cur_event && cur_event->event == e) {
        delete cur_event->event;
        cur_event->event = NULL;
    } else {
        EventData *prev;
        EventData **queue = &event_queue;
        EventData *ed = findPosting (event_queue, &prev, e);
        if (!ed) {
            ed = findPosting (paused_queue, &prev, e);
            queue = &paused_queue;
        }
        if (ed) {
            if (prev) {
                prev->next = ed->next;
            } else {
                *queue = ed->next;
                if (!cur_event && queue == &event_queue) {
                    struct timeval now;
                    if (event_queue) // save a sys call
                        timeOfDay (now);
                    setNextTimeout (now);
                }
            }
            delete ed;
        } else {
            kError () << "Posting not found";
        }
    }
}

void Document::pausePosting (Posting *e) {
    if (cur_event && cur_event->event == e) {
        paused_queue = new EventData (cur_event->target, cur_event->event, paused_queue);
        paused_queue->timeout = cur_event->timeout;
        cur_event->event = NULL;
    } else {
        EventData *prev;
        EventData *ed = findPosting (event_queue, &prev, e);
        if (ed) {
            if (prev)
                prev->next = ed->next;
            else
                event_queue = ed->next;
            ed->next = paused_queue;
            paused_queue = ed;
        } else {
            kError () << "pauseEvent not found";
        }
    }
}

void Document::unpausePosting (Posting *e, int ms) {
    EventData *prev;
    EventData *ed = findPosting (paused_queue, &prev, e);
    if (ed) {
        if (prev)
            prev->next = ed->next;
        else
            paused_queue = ed->next;
        addTime (ed->timeout, ms);
        insertPosting (ed->target, ed->event, ed->timeout);
        ed->event = NULL;
        delete ed;
    } else {
        kError () << "pausePosting not found";
    }
}

void Document::timer () {
    struct timeval now;
    cur_event = event_queue;
    if (cur_event) {
        NodePtrW guard = this;
        struct timeval start = cur_event->timeout;
        timeOfDay (now);

        // handle max 100 timeouts with timeout set to now
        for (int i = 0; i < 100 && active (); ++i) {
            if (postpone_ref && postponedSensible (cur_event->event->message))
                break;
            // remove from queue
            event_queue = cur_event->next;

            if (!cur_event->target) {
                // some part of document has gone and didn't remove timer
                kError () << "spurious timer" << endl;
            } else {
                EventData *ed = cur_event;
                cur_event->target->message (cur_event->event->message, cur_event->event);
                if (!guard) {
                    delete ed;
                    return;
                }
                if (cur_event->event && cur_event->event->message == MsgEventTimer) {
                    TimerPosting *te = static_cast <TimerPosting *> (cur_event->event);
                    if (te->interval) {
                        te->interval = false; // reset interval
                        addTime (cur_event->timeout, te->milli_sec);
                        insertPosting (cur_event->target,
                                cur_event->event,
                                cur_event->timeout);
                        cur_event->event = NULL;
                    }
                }
            }
            delete cur_event;
            cur_event = event_queue;
            if (!cur_event || diffTime (cur_event->timeout, start) > 5)
                break;
        }
        cur_event = NULL;
    }
    setNextTimeout (now);
}

PostponePtr Document::postpone () {
    if (postpone_ref)
        return postpone_ref;
    kDebug () << "postpone";
    PostponePtr p = new Postpone (this);
    postpone_ref = p;
    PostponedEvent event (true);
    deliver (MsgEventPostponed, &event);
    if (notify_listener)
        notify_listener->enableRepaintUpdaters (false, 0);
    if (!cur_event) {
        struct timeval now;
        if (event_queue) // save a sys call
            timeOfDay (now);
        setNextTimeout (now);
    }
    return p;
}

void Document::proceed (const struct timeval &postponed_time) {
    kDebug () << "proceed";
    postpone_ref = NULL;
    struct timeval now;
    timeOfDay (now);
    int diff = diffTime (now, postponed_time);
    if (event_queue) {
        for (EventData *ed = event_queue; ed; ed = ed->next)
            if (ed->event && postponedSensible (ed->event->message))
                addTime (ed->timeout, diff);
        setNextTimeout (now);
    }
    if (notify_listener)
        notify_listener->enableRepaintUpdaters (true, diff);
    PostponedEvent event (false);
    deliver (MsgEventPostponed, &event);
}

void *Document::role (RoleType msg, void *content) {
    if (RoleReceivers == msg) {
        MessageType m = (MessageType) (long) content;
        if (MsgEventPostponed == m)
            return &m_PostponedListeners;
    }
    return Mrl::role (msg, content);
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

Node *DarkNode::childFromTag (const QString & tag) {
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
    title = name;
}

KDE_NO_EXPORT void GenericURL::closed () {
    if (src.isEmpty ())
        src = getAttribute (StringPool::attr_src);
    Mrl::closed ();
}

//-----------------------------------------------------------------------------

GenericMrl::GenericMrl (NodePtr & d, const QString &s, const QString &name, const QByteArray &tag)
 : Mrl (d, id_node_playlist_item), node_name (tag) {
    src = s;
    if (!src.isEmpty ())
        setAttribute (StringPool::attr_src, src);
    title = name;
    if (!name.isEmpty ())
        setAttribute (StringPool::attr_name, name);
}

void GenericMrl::closed () {
    if (src.isEmpty ()) {
        src = getAttribute (StringPool::attr_src);
        if (src.isEmpty ())
            src = getAttribute (StringPool::attr_url);
    }
    if (title.isEmpty ())
        title = getAttribute (StringPool::attr_name);
    Mrl::closed ();
}

bool GenericMrl::expose () const {
    return !title.isEmpty () || //return false if no title and only one
        previousSibling () || nextSibling ();
}

//-----------------------------------------------------------------------------

void Visitor::visit (Element *elm) {
    visit (static_cast <Node *> (elm));
}

void Visitor::visit (TextNode *text) {
    visit (static_cast <Node *> (text));
}

//-----------------------------------------------------------------------------

CacheAllocator::CacheAllocator (size_t s)
    : pool ((void**) malloc (10 * sizeof (void *))), size (s), count (0) {}

void *CacheAllocator::alloc () {
    return count ? pool[--count] : malloc (size);
}

void CacheAllocator::dealloc (void *p) {
    if (count < 10)
        pool[count++] = p;
    else
        free (p);
}

KMPLAYER_EXPORT CacheAllocator *KMPlayer::shared_data_cache_allocator = NULL;

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
#ifdef KMPLAYER_WITH_EXPAT
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
#ifdef KMPLAYER_WITH_EXPAT
 , in_cdata (false)
#endif
{}

bool DocumentBuilder::startTag(const QString &tag, AttributeListPtr attr) {
    if (m_ignore_depth) {
        m_ignore_depth++;
        //kDebug () << "Warning: ignored tag " << tag.latin1 () << " ignore depth = " << m_ignore_depth;
    } else {
        NodePtr n = m_node ? m_node->childFromTag (tag) : NULL;
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
    if (!m_ignore_depth && m_node) {
#ifdef KMPLAYER_WITH_EXPAT
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

#ifdef KMPLAYER_WITH_EXPAT

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
            attributes->append (new Attribute (
                        TrieString(),
                        QString::fromUtf8 (attr [i]),
                        QString::fromUtf8 (attr [i+1])));
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
        QByteArray ba = str.utf8 ();
        char *buf = ba.data();
        ok = XML_Parse(parser, buf, strlen (buf), false) != XML_STATUS_ERROR;
        if (!ok)
            kWarning () << XML_ErrorString(XML_GetErrorCode(parser)) << " at " << XML_GetCurrentLineNumber(parser) << " col " << XML_GetCurrentColumnNumber(parser);
    }
    if (ok && !in.atEnd ()) {
        QByteArray ba = in.read ().utf8 ();
        char *buf = ba.data();
        ok = XML_Parse(parser, buf, strlen (buf), true) != XML_STATUS_ERROR;
        if (!ok)
            kWarning () << XML_ErrorString(XML_GetErrorCode(parser)) << " at " << XML_GetCurrentLineNumber(parser) << " col " << XML_GetCurrentColumnNumber(parser);
    }
    XML_ParserFree(parser);
    root->normalize ();
    //return ok;
}

//-----------------------------------------------------------------------------
#else // KMPLAYER_WITH_EXPAT

namespace {

class KMPLAYER_NO_EXPORT SimpleSAXParser {
    enum Token { tok_empty, tok_text, tok_white_space, tok_angle_open,
        tok_equal, tok_double_quote, tok_single_quote, tok_angle_close,
        tok_slash, tok_exclamation, tok_amp, tok_hash, tok_colon,
        tok_semi_colon, tok_question_mark, tok_cdata_start };
public:
    struct TokenInfo {
        TokenInfo () : token (tok_empty) {}
        void *operator new (size_t);
        void operator delete (void *);
        Token token;
        QString string;
        SharedPtr <TokenInfo> next;
    };
    typedef SharedPtr <TokenInfo> TokenInfoPtr;
    SimpleSAXParser (DocumentBuilder & b) : builder (b), position (0), m_attributes (new AttributeList), equal_seen (false), in_dbl_quote (false), in_sngl_quote (false), have_error (false), no_entitity_look_ahead (false), have_next_char (false) {}
    virtual ~SimpleSAXParser () {};
    bool parse (QTextStream & d);
private:
    QTextStream * data;
    DocumentBuilder & builder;
    int position;
    QChar next_char;
    enum State {
        InTag, InStartTag, InPITag, InDTDTag, InEndTag, InAttributes, InContent, InCDATA, InComment
    };
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
    QString attr_namespace, attr_name, attr_value;
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

static CacheAllocator token_pool (sizeof (SimpleSAXParser::TokenInfo));

inline void *SimpleSAXParser::TokenInfo::operator new (size_t) {
    return token_pool.alloc ();
}

inline void SimpleSAXParser::TokenInfo::operator delete (void *p) {
    token_pool.dealloc (p);
}

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
    for (NodePtr e = root; e; e = e->parentNode ()) {
        if (e->open)
            break;
        e->closed ();
    }
    //doc->normalize ();
    //kDebug () << root->outerXML ();
}

void SimpleSAXParser::push () {
    if (next_token->string.size ()) {
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
    m_attributes->append(new Attribute (attr_namespace, attr_name, attr_value));
    attr_namespace.clear ();
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
            } else if (InAttributes == m_state->state &&
                    next_char == QChar (':')) {
                push ();
                next_token->token = tok_colon;
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
        } else if (next_token->string.size ()) {
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
        } else if (token->token == tok_colon) {
            if (equal_seen) {
                attr_value += token->string;
            } else {
                attr_namespace = attr_name;
                attr_name.clear();
            }
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
            cdata.truncate (cdata.size () - 2);
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

#endif // KMPLAYER_WITH_EXPAT
