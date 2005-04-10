/**
 * Copyright (C) 2004 by Koos Vriezen <koos ! vriezen ? xs4all ! nl>
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
 *  the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 *  Boston, MA 02111-1307, USA.
 **/

#include <config.h>
#include <qtextstream.h>
#include <qcolor.h>
#include <kdebug.h>
#include <kurl.h>

#include "kmplayerplaylist.h"

#ifdef SHAREDPTR_DEBUG
int shared_data_count;
#endif

using namespace KMPlayer;

//-----------------------------------------------------------------------------

static Node * fromXMLDocumentGroup (NodePtr & d, const QString & tag) {
    const char * const name = tag.latin1 ();
    if (!strcmp (name, "smil"))
        return new Smil (d);
    else if (!strcasecmp (name, "asx"))
        return new ASX::Asx (d);
    return 0L;
}

//-----------------------------------------------------------------------------

namespace KMPlayer {
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

int NodeList::length () {
    int len = 0;
    for (NodePtr e = first_element; e; e = e->nextSibling ())
        len++;
    return len;
}

NodePtr NodeList::item (int i) const {
    NodePtr elm;
    for (NodePtr e = first_element; e; e = e->nextSibling (), i--)
        if (i == 0) {
            elm = e;
            break;
        }
    return elm;
}
    
//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT Node::Node (NodePtr & d)
 : m_doc (d), state (state_init) {}

Node::~Node () {
    clear ();
}

Document * Node::document () {
    return convertNode <Document> (m_doc);
}

Mrl * Node::mrl () {
    return dynamic_cast<Mrl*>(this);
}

const Mrl * Node::mrl () const {
    return dynamic_cast<const Mrl*>(this);
}

KDE_NO_EXPORT const char * Node::nodeName () const {
    return "element";
}

void Node::setState (State nstate) {
    if (state != nstate) {
        state = nstate;
        if (document ()->notify_listener)
            document ()->notify_listener->stateElementChanged (self ());
    }
}

bool Node::expose () {
    return true;
}

void Node::activate () {
    kdDebug () << nodeName () << " Node::activate" << endl;
    setState (state_activated);
    if (firstChild ())
        firstChild ()->activate (); // activate only the first
    else
        deactivate (); // nothing to activate
}

void Node::defer () {
    if (state == state_activated) {
        setState (state_deferred);
        defer_tree_version = document ()->m_tree_version;
    } else
        kdError () << "Node::defer () call on not activated element" << endl;
}

void Node::undefer () {
    if (state == state_deferred) {
        setState (state_activated);
        if (defer_tree_version != document ()->m_tree_version)
            activate ();
    } else
        kdWarning () <<"Node::undefer () call on not defered element"<< endl;
}

void Node::deactivate () {
    kdDebug () << nodeName () << " Node::deactivate" << endl;
    setState (state_deactivated);
    for (NodePtr e = firstChild (); e; e = e->nextSibling ()) {
        if (e->state > state_init && e->state < state_deactivated)
            e->deactivate ();
        else
            break; // not yet activated
    }
    if (m_parent)
        m_parent->childDone (m_self);
}

void Node::reset () {
    kdDebug () << nodeName () << " Node::reset" << endl;
    if (state == state_activated)
        deactivate ();
    setState (state_init);
    for (NodePtr e = firstChild (); e; e = e->nextSibling ()) {
        if (e->state != state_init)
            e->reset ();
        else
            break; // rest not activated yet
    }
}

void Node::childDone (NodePtr child) {
    kdDebug () << nodeName () << " Node::childDone" << endl;
    if (state == state_activated) {
        if (child->nextSibling ())
            child->nextSibling ()->activate ();
        else
            deactivate (); // we're done
    }
}

void Node::clear () {
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
    TreeNode<Node>::appendChild (c);
}

KDE_NO_EXPORT void Node::insertBefore (NodePtr c, NodePtr b) {
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
    c->m_parent = m_self;
}

void Node::removeChild (NodePtr c) {
    document()->m_tree_version++;
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
    _new->m_parent = m_self;
    old->m_parent = 0L;
}

NodePtr Node::childFromTag (const QString &) {
    return NodePtr ();
}

KDE_NO_EXPORT void Node::characterData (const QString & s) {
    document()->m_tree_version++;
    if (!m_last_child || strcmp (m_last_child->nodeName (), "#text"))
        appendChild ((new TextNode (m_doc, s))->self ());
    else
        convertNode <TextNode> (m_last_child)->appendText (s);
}

void Node::normalize () {
    NodePtr e = firstChild ();
    while (e) {
        NodePtr tmp = e->nextSibling ();
        if (!strcmp (e->nodeName (), "#text")) {
            if (e->nodeValue ().stripWhiteSpace ().isEmpty ())
                removeChild (e);
        } else
            e->normalize ();
        e = tmp;
    }
}

static void getInnerText (const NodePtr p, QTextOStream & out) {
    for (NodePtr e = p->firstChild (); e; e = e->nextSibling ()) {
        if (!strcmp (e->nodeName (), "#text"))
            out << e->nodeValue ();
        else
            getInnerText (e, out);
    }
}

QString Node::innerText () const {
    QString buf;
    QTextOStream out (&buf);
    getInnerText (self (), out);
    return buf;
}

static void getOuterXML (const NodePtr p, QTextOStream & out) {
    if (!p->isElementNode ()) // #text
        out << XMLStringlet (p->nodeValue ());
    else {
        Element * e = convertNode <Element> (p);
        out << QChar ('<') << XMLStringlet (e->nodeName ());
        for (AttributePtr a=e->attributes()->firstChild();a; a=a->nextSibling())
            out << " " << XMLStringlet (a->nodeName ()) << "=\"" << XMLStringlet (a->nodeValue ()) << "\"";
        if (e->hasChildNodes ()) {
            out << QChar ('>');
            for (NodePtr c = e->firstChild (); c; c = c->nextSibling ())
                getOuterXML (c, out);
            out << QString("</") << XMLStringlet (e->nodeName()) << QChar ('>');
        } else
            out << QString ("/>");
    }
}

QString Node::innerXML () const {
    QString buf;
    QTextOStream out (&buf);
    for (NodePtr e = firstChild (); e; e = e->nextSibling ())
        getOuterXML (e, out);
    return buf;
}

QString Node::outerXML () const {
    QString buf;
    QTextOStream out (&buf);
    getOuterXML (self (), out);
    return buf;
}

bool Node::isMrl () {
    return false;
}

void Node::opened () {}

void Node::closed () {}

QString Node::nodeValue () const {
    return QString::null;
}

//-----------------------------------------------------------------------------

Element::Element (NodePtr & d)
    : Node (d), m_attributes (AttributePtr (new Attribute)) {}

void Element::setAttribute (const QString & name, const QString & value) {
    const char * name_latin = name.latin1 ();
    for (AttributePtr a = m_attributes->firstChild (); a; a = a->nextSibling ())
        if (!strcmp (name_latin, a->nodeName ())) {
            static_cast <Attribute *> (a.ptr ())->value = value;
            return;
        }
    m_attributes->appendChild ((new Attribute (name, value))->self ());
}

QString Element::getAttribute (const QString & name) {
    QString value;
    for (AttributePtr a = m_attributes->firstChild (); a; a = a->nextSibling ())
        if (!name.compare (a->nodeName ())) {
            value = a->nodeValue ();
            break;
        }
    return value;
}

void Element::clear () {
    m_attributes = AttributePtr (new Attribute); // remove attributes
    Node::clear ();
}

void Element::setAttributes (AttributePtr attrs) {
    m_attributes = attrs;
}

Attribute::Attribute (const QString & n, const QString & v)
  : name (n), value (v) {}

QString Attribute::nodeValue () const {
    return value;
}

const char * Attribute::nodeName () const {
    return name.ascii ();
}

//-----------------------------------------------------------------------------

static bool hasMrlChildren (const NodePtr & e) {
    for (NodePtr c = e->firstChild (); c; c = c->nextSibling ())
        if (c->isMrl () || hasMrlChildren (c))
            return true;
    return false;
}

Mrl::Mrl (NodePtr & d) : Element (d), cached_ismrl_version (~0), width (0), height (0), aspect (0), parsed (false), bookmarkable (true) {}

Mrl::~Mrl () {}

bool Mrl::isMrl () {
    if (cached_ismrl_version != document()->m_tree_version) {
        cached_ismrl = !hasMrlChildren (m_self);
        cached_ismrl_version = document()->m_tree_version;
        if (!src.isEmpty()) {
            if (pretty_name.isEmpty ())
                pretty_name = src;
            for (NodePtr e = parentNode (); e; e = e->parentNode ()) {
                Mrl * mrl = e->mrl ();
                if (mrl)
                    src = KURL (mrl->src, src).url ();
            }
        }
    }
    return cached_ismrl;
}

NodePtr Mrl::childFromTag (const QString & tag) {
    Node * elm = fromXMLDocumentGroup (m_doc, tag);
    if (elm)
        return elm->self ();
    return NodePtr ();
}

NodePtr Mrl::realMrl () {
    return m_self;
}

void Mrl::activate () {
    if (!isMrl ()) {
        Element::activate ();
        return;
    }
    kdDebug () << "Mrl::activate" << endl;
    setState (state_activated);
    if (document ()->notify_listener && !src.isEmpty ())
        document ()->notify_listener->requestPlayURL (m_self, RegionNodePtr ());
    else
        deactivate (); // nothing to activate
}

//-----------------------------------------------------------------------------

namespace KMPlayer {
    static NodePtr dummy_element;
}

Document::Document (const QString & s, PlayListNotify * n)
 : Mrl (dummy_element), notify_listener (n), m_tree_version (0) {
    m_doc = m_self; // just-in-time setting fragile m_self to m_doc
    src = s;
}

KDE_NO_CDTOR_EXPORT Document::~Document () {
    kdDebug () << "~Document\n";
}

static NodePtr getElementByIdImpl (NodePtr n, const QString & id) {
    NodePtr elm;
    if (!n->isElementNode ())
        return elm;
    Element * e = convertNode <Element> (n);
    if (e->getAttribute ("id") == id)
        return n;
    for (NodePtr c = e->firstChild (); c; c = c->nextSibling ())
        if ((elm = getElementByIdImpl (c, id)))
            break;
    return elm;
}

NodePtr Document::getElementById (const QString & id) {
    return getElementByIdImpl (m_self, id);
}

KDE_NO_EXPORT NodePtr Document::childFromTag (const QString & tag) {
    Node * elm = fromXMLDocumentGroup (m_doc, tag);
    if (elm)
        return elm->self ();
    return NodePtr ();
}

void Document::dispose () {
    clear ();
    m_doc = 0L;
}

bool Document::isMrl () {
    return Mrl::isMrl ();
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT TextNode::TextNode (NodePtr & d, const QString & s)
 : Node (d), text (s) {}

void TextNode::appendText (const QString & s) {
    text += s;
}

QString TextNode::nodeValue () const {
    return text;
}

KDE_NO_EXPORT bool TextNode::expose () {
    return false;
}

//-----------------------------------------------------------------------------

DarkNode::DarkNode (NodePtr & d, const QString & n) : Element (d), name (n) {
}

NodePtr DarkNode::childFromTag (const QString & tag) {
    return (new DarkNode (m_doc, tag))->self ();
}

KDE_NO_EXPORT bool DarkNode::expose () {
    return false;
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT Title::Title (NodePtr & d)
    : DarkNode (d, QString ("title")) {}

//-----------------------------------------------------------------------------

KDE_NO_EXPORT NodePtr ASX::Asx::childFromTag (const QString & tag) {
    const char * name = tag.latin1 ();
    if (!strcasecmp (name, "entry"))
        return (new ASX::Entry (m_doc))->self ();
    else if (!strcasecmp (name, "entryref"))
        return (new ASX::EntryRef (m_doc))->self ();
    else if (!strcasecmp (name, "title"))
        return (new Title (m_doc))->self ();
    return NodePtr ();
}

KDE_NO_EXPORT bool ASX::Asx::isMrl () {
    if (cached_ismrl_version != document ()->m_tree_version) {
        for (NodePtr e = firstChild (); e; e = e->nextSibling ())
            if (!strcmp (e->nodeName (), "title"))
                pretty_name = e->innerText ();
    }
    return Mrl::isMrl ();
}
//-----------------------------------------------------------------------------

KDE_NO_EXPORT NodePtr ASX::Entry::childFromTag (const QString & tag) {
    const char * name = tag.latin1 ();
    if (!strcasecmp (name, "ref"))
        return (new ASX::Ref (m_doc))->self ();
    else if (!strcasecmp (name, "title"))
        return (new Title (m_doc))->self ();
    return NodePtr ();
}

KDE_NO_EXPORT bool ASX::Entry::isMrl () {
    if (cached_ismrl_version != document ()->m_tree_version) {
        QString pn;
        src.truncate (0);
        bool foundone = false;
        for (NodePtr e = firstChild (); e; e = e->nextSibling ()) {
            if (e->isMrl () && !e->hasChildNodes ()) {
                if (foundone) {
                    src.truncate (0);
                    pn.truncate (0);
                } else {
                    src = e->mrl ()->src;
                    pn = e->mrl ()->pretty_name;
                }
                foundone = true;
            } else if (!strcmp (e->nodeName (), "title"))
                pretty_name = e->innerText ();
        }
        if (pretty_name.isEmpty ())
            pretty_name = pn;
        cached_ismrl_version = document()->m_tree_version;
    }
    return !src.isEmpty ();
}

KDE_NO_EXPORT NodePtr ASX::Entry::realMrl () {
    for (NodePtr e = firstChild (); e; e = e->nextSibling ())
        if (e->isMrl ())
            return e;
    return m_self;
}

//-----------------------------------------------------------------------------

KDE_NO_EXPORT void ASX::Ref::opened () {
    for (AttributePtr a = m_attributes->firstChild(); a; a = a->nextSibling()) {
        if (!strcasecmp (a->nodeName (), "href"))
            src = a->nodeValue ();
        else
            kdError () << "Warning: unhandled Ref attr: " << a->nodeName () << "=" << a->nodeValue () << endl;

    }
    kdDebug () << "Ref attr found src: " << src << endl;
}

//-----------------------------------------------------------------------------

KDE_NO_EXPORT void ASX::EntryRef::opened () {
    for (AttributePtr a = m_attributes->firstChild(); a; a = a->nextSibling()) {
        if (!strcasecmp (a->nodeName (), "href"))
            src = a->nodeValue ();
        else
            kdError () << "unhandled EntryRef attr: " << a->nodeName () << "=" << a->nodeValue () << endl;
    }
    kdDebug () << "EntryRef attr found src: " << src << endl;
}

//-----------------------------------------------------------------------------

GenericURL::GenericURL (NodePtr & d, const QString & s, const QString & name)
 : Mrl (d) {
    src = s;
    pretty_name = name;
}

GenericMrl::GenericMrl (NodePtr & d, const QString & s, const QString & name)
 : Mrl (d) {
    src = s;
    pretty_name = name;
}

bool GenericMrl::isMrl () {
    if (cached_ismrl_version != document()->m_tree_version) {
        cached_ismrl = !hasMrlChildren (m_self);
        cached_ismrl_version = document()->m_tree_version;
    }
    return cached_ismrl;
}

//-----------------------------------------------------------------------------

namespace KMPlayer {

class DocumentBuilder {
public:
    DocumentBuilder (NodePtr d) : position (0), m_doc (d), equal_seen (false), in_dbl_quote (false), in_sngl_quote (false), have_error (false) {}
    virtual ~DocumentBuilder () {};
    bool parse (QTextStream & d);
protected:
    virtual bool startTag (const QString & tag, AttributePtr attr) = 0;
    virtual bool endTag (const QString & tag) = 0;
    virtual bool characterData (const QString & data) = 0;
private:
    QTextStream * data;
    int position;
    QChar next_char;
    enum Token { tok_empty, tok_text, tok_white_space, tok_angle_open, tok_equal, tok_double_quote, tok_single_quote, tok_angle_close, tok_slash, tok_exclamation, tok_amp, tok_hash, tok_semi_colon, tok_question_mark };
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
    NodePtr m_doc;
    AttributePtr m_attributes;
    QString attr_name, attr_value;
    QString cdata;
    bool equal_seen;
    bool in_dbl_quote;
    bool in_sngl_quote;
    bool have_error;

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

class KMPlayerDocumentBuilder : public DocumentBuilder {
    int m_ignore_depth;
    NodePtr m_node;
    NodePtr m_root;
public:
    KMPlayerDocumentBuilder (NodePtr d);
    ~KMPlayerDocumentBuilder () {}
protected:
    bool startTag (const QString & tag, AttributePtr attr);
    bool endTag (const QString & tag);
    bool characterData (const QString & data);
};

void readXML (NodePtr root, QTextStream & in, const QString & firstline) {
    KMPlayerDocumentBuilder builder (root);
    if (!firstline.isEmpty ()) {
        QString str (firstline + QChar ('\n'));
        QTextStream fl_in (&str, IO_ReadOnly);
        builder.parse (fl_in);
    }
    builder.parse (in);
    //doc->normalize ();
    //kdDebug () << root->outerXML ();
}

} // namespace

void DocumentBuilder::push () {
    if (next_token->string.length ()) {
        prev_token = token;
        token = next_token;
        if (prev_token)
            prev_token->next = token;
        next_token = TokenInfoPtr (new TokenInfo);
        //kdDebug () << "push " << token->string << endl;
    }
}

void DocumentBuilder::push_attribute () {
    //kdDebug () << "attribute " << attr_name.latin1 () << "=" << attr_value.latin1 () << endl;
    m_attributes->appendChild ((new Attribute (attr_name, attr_value))->self());
    attr_name.truncate (0);
    attr_value.truncate (0);
    equal_seen = in_sngl_quote = in_dbl_quote = false;
}

bool DocumentBuilder::nextToken () {
    if (token && token->next) {
        token = token->next;
        //kdDebug () << "nextToken: token->next found\n";
        return true;
    }
    TokenInfoPtr cur_token = token;
    while (!data->atEnd () && cur_token == token) {
        *data >> next_char;
        bool append_char = true;
        if (next_char.isSpace ()) {
            if (next_token->token != tok_white_space)
                push ();
            next_token->token = tok_white_space;
        } else if (!next_char.isLetterOrNumber ()) {
            if (next_char == QChar ('#')) {
                if (next_token->token == tok_empty) { // check last item on stack &
                    push ();
                    next_token->token = tok_hash;
                }
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
                append_char = false;
                TokenInfoPtr tmp = token;
                if (nextToken () && token->token == tok_text &&
                        nextToken () && token->token == tok_semi_colon) {
                    if (prev_token->string == QString ("amp"))
                        token->string = QChar ('&');
                    else if (prev_token->string == QString ("lt"))
                        token->string = QChar ('<');
                    else if (prev_token->string == QString ("gt"))
                        token->string = QChar ('>');
                    else if (prev_token->string == QString ("quote"))
                        token->string = QChar ('"');
                    else if (prev_token->string == QString ("apos"))
                        token->string = QChar ('\'');
                    else if (prev_token->string == QString ("copy"))
                        token->string = QChar (169);
                    else
                        token->string = QChar ('?');// TODO lookup more ..
                    if (tmp) { // cut out the & xxx ; tokens
                        tmp->next = token;
                        token = tmp;
                    }
                    token->token = tok_text;
                    //kdDebug () << "entity found "<<prev_token->string << endl;
                } else if (token->token == tok_hash &&
                        nextToken () && token->token == tok_text && 
                        nextToken () && token->token == tok_semi_colon) {
                    //kdDebug () << "char entity found " << prev_token->string << prev_token->string.toInt (0L, 16) << endl;
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
    }
    if (token == cur_token) {
        if (next_token->string.length ()) {
            push (); // last token
            return true;
        }
        return false;
    }
    return true;
}

bool DocumentBuilder::readAttributes () {
    bool closed = false;
    while (true) {
        if (!nextToken ()) return false;
        //kdDebug () << "readAttributes " << token->string.latin1() << endl;
        if ((in_dbl_quote && token->token != tok_double_quote) ||
                    (in_sngl_quote && token->token != tok_single_quote)) {
            attr_value += token->string;
        } else if (token->token == tok_equal) {
            if (attr_name.isEmpty ())
                return false;
            if (equal_seen)
                attr_value += token->string; // EQ=a=2c ???
            //kdDebug () << "equal_seen"<< endl;
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
            //kdDebug () << "in_dbl_quote:"<< in_dbl_quote << endl;
        } else if (token->token == tok_slash) {
            TokenInfoPtr mark_token = token;
            if (nextToken () &&
                    (token->token != tok_white_space || nextToken()) &&//<e / >
                    token->token == tok_angle_close) {
            //kdDebug () << "close mark:"<< endl;
                closed = true;
                break;
            } else {
                token = mark_token;
            //kdDebug () << "not end mark:"<< equal_seen << endl;
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
                  kdDebug () << "encodeing " << i.data().latin1() << endl;*/
        }
    } else {
        have_error = startTag (tagname, m_attributes);
        if (closed)
            have_error &= endTag (tagname);
        //kdDebug () << "readTag " << tagname << " closed:" << closed << " ok:" << have_error << endl;
    }
    m_state = m_state->next; // pop Node or PI
    return true;
}

bool DocumentBuilder::readPI () {
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

bool DocumentBuilder::readDTD () {
    //TODO: <!ENTITY ..>
    if (!nextToken ()) return false;
    if (token->token == tok_text && token->string.startsWith (QString ("--"))) {
        m_state = new StateInfo (InComment, m_state->next); // note: pop DTD
        return readComment ();
    }
    //kdDebug () << "readDTD: " << token->string.latin1 () << endl;
    if (token->token == tok_text && token->string.startsWith (QString ("[CDATA["))) {
        m_state = new StateInfo (InCDATA, m_state->next); // note: pop DTD
        cdata.truncate (0);
        return readCDATA ();
    }
    while (nextToken ())
        if (token->token == tok_angle_close) {
            m_state = m_state->next;
            return true;
        }
    return false;
}

bool DocumentBuilder::readCDATA () {
    while (!data->atEnd ()) {
        *data >> next_char;
        if (next_char == QChar ('>') && cdata.endsWith (QString ("]]"))) {
            cdata.truncate (cdata.length () - 2);
            m_state = m_state->next;
            if (m_state->state == InContent)
                have_error = characterData (cdata);
            else if (m_state->state == InAttributes) {
                if (equal_seen)
                    attr_value += cdata;
                else
                    attr_name += cdata;
            }
            return true;
        }
        cdata += next_char;
    }
    return false;
}

bool DocumentBuilder::readComment () {
    while (nextToken ()) {
        if (token->token == tok_angle_close && prev_token)
            if (prev_token->string.endsWith (QString ("--"))) {
                m_state = m_state->next;
                return true;
            }
    }
    return false;
}

bool DocumentBuilder::readEndTag () {
    if (!nextToken ()) return false;
    if (token->token == tok_white_space)
        if (!nextToken ()) return false;
    tagname = token->string;
    if (!nextToken ()) return false;
    if (token->token == tok_white_space)
        if (!nextToken ()) return false;
    if (token->token != tok_angle_close)
        return false;
    have_error = endTag (tagname);
    m_state = m_state->next;
    return true;
}

// TODO: <!ENTITY ..> &#1234;
bool DocumentBuilder::readTag () {
    if (!nextToken ()) return false;
    if (token->token == tok_exclamation) {
        m_state = new StateInfo (InDTDTag, m_state->next);
    //kdDebug () << "readTag: " << token->string.latin1 () << endl;
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
    //kdDebug () << "readTag " << tagname.latin1() << endl;
    m_state = new StateInfo (InAttributes, m_state);
    return readAttributes ();
}

bool DocumentBuilder::parse (QTextStream & d) {
    data = &d;
    if (!next_token) {
        next_token = TokenInfoPtr (new TokenInfo);
        m_state = new StateInfo (InContent, m_state);
    }
    bool ok = true;
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
                        m_attributes = AttributePtr (new Attribute);
                        equal_seen = in_sngl_quote = in_dbl_quote = false;
                        m_state = new StateInfo (InTag, m_state);
                        ok = readTag ();
                    } else
                        have_error = characterData (token->string);
                }
        }
        if (!m_state)
            return true; // end document
    }
    return false; // need more data
}

KMPlayerDocumentBuilder::KMPlayerDocumentBuilder (NodePtr d)
 : DocumentBuilder (d->document ()->self ()), m_ignore_depth (0),
   m_node (d), m_root (d) {}

bool KMPlayerDocumentBuilder::startTag(const QString & tag, AttributePtr attr) {
    if (m_ignore_depth) {
        m_ignore_depth++;
        //kdDebug () << "Warning: ignored tag " << tag.latin1 () << " ignore depth = " << m_ignore_depth << endl;
    } else {
        NodePtr n = m_node->childFromTag (tag);
        if (!n) {
            kdDebug () << "Warning: unknown tag " << tag.latin1 () << endl;
            NodePtr doc = m_root->document ()->self ();
            n = (new DarkNode (doc, tag))->self ();
        }
        //kdDebug () << "Found tag " << tag.latin1 () << endl;
        if (n->isElementNode ())
            convertNode <Element> (n)->setAttributes (attr);
        m_node->appendChild (n);
        n->opened ();
        m_node = n;
    }
    return true;
}

bool KMPlayerDocumentBuilder::endTag (const QString & /*tag*/) {
    if (m_ignore_depth) { // endtag to ignore
        m_ignore_depth--;
        kdDebug () << "Warning: ignored end tag " << " ignore depth = " << m_ignore_depth <<  endl;
    } else {  // endtag
        if (m_node == m_root) {
            kdDebug () << "m_node == m_doc, stack underflow " << endl;
            return false;
        }
        //kdDebug () << "end tag " << tag.latin1 () << endl;
        m_node->closed ();
        m_node = m_node->parentNode ();
    }
    return true;
}

bool KMPlayerDocumentBuilder::characterData (const QString & data) {
    if (!m_ignore_depth)
        m_node->characterData (data);
    //kdDebug () << "characterData " << d.latin1() << endl;
    return true;
}
