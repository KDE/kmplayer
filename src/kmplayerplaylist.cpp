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

#include <qtextstream.h>
#include <kdebug.h>
#include <kurl.h>

#include "kmplayerplaylist.h"

#ifdef SHAREDPTR_DEBUG
int shared_data_count;
#endif

using namespace KMPlayer;

static Element * fromXMLDocumentGroup (ElementPtr & d, const QString & tag) {
    const char * const name = tag.latin1 ();
    if (!strcmp (name, "smil"))
        return new Smil (d);
    else if (!strcasecmp (name, "asx"))
        return new Asx (d);
    return 0L;
}

static Element * fromScheduleGroup (ElementPtr & d, const QString & tag) {
    if (!strcmp (tag.latin1 (), "par"))
        return new Par (d);
    else if (!strcmp (tag.latin1 (), "seq"))
        return new Seq (d);
    // else if (!strcmp (tag.latin1 (), "excl"))
    //    return new Seq (d, p);
    return 0L;
}

static Element * fromMediaContentGroup (ElementPtr & d, const QString & tag) {
    if (!strcmp (tag.latin1 (), "video") || !strcmp (tag.latin1 (), "audio"))
        return new MediaType (d, tag);
    // text, img, animation, textstream, ref, brush
    return 0L;
}

static Element * fromContentControlGroup (ElementPtr & d, const QString & tag) {
    if (!strcmp (tag.latin1 (), "switch"))
        return new Switch (d);
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
    for (ElementPtr e = first_element; e; e = e->nextSibling ())
        len++;
    return len;
}

ElementPtr NodeList::item (int i) const {
    ElementPtr elm;
    for (ElementPtr e = first_element; e; e = e->nextSibling (), i--)
        if (i == 0) {
            elm = e;
            break;
        }
    return elm;
}
    
//-----------------------------------------------------------------------------

Element::~Element () {
    clear ();
}

Document * Element::document () {
    return convertNode <Document> (m_doc);
}

Mrl * Element::mrl () {
    return dynamic_cast<Mrl*>(this);
}

const Mrl * Element::mrl () const {
    return dynamic_cast<const Mrl*>(this);
}

KDE_NO_EXPORT const char * Element::nodeName () const {
    return "element";
}

bool Element::expose () {
    return true;
}

void Element::clear () {
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
    m_first_attribute = 0L; // remove attributes
}

void Element::appendChild (ElementPtr c) {
    document()->m_tree_version++;
    if (!m_first_child) {
        m_first_child = m_last_child = c;
    } else {
        m_last_child->m_next = c;
        c->m_prev = m_last_child;
        m_last_child = c;
    }
    c->m_parent = m_self;
}

KDE_NO_EXPORT void Element::insertBefore (ElementPtr c, ElementPtr b) {
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

void Element::removeChild (ElementPtr c) {
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

KDE_NO_EXPORT void Element::replaceChild (ElementPtr _new, ElementPtr old) {
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

ElementPtr Element::childFromTag (const QString &) {
    return ElementPtr ();
}

KDE_NO_EXPORT void Element::characterData (const QString & s) {
    document()->m_tree_version++;
    if (!m_last_child || strcmp (m_last_child->nodeName (), "#text"))
        appendChild ((new TextNode (m_doc, s))->self ());
    else
        convertNode <TextNode> (m_last_child)->appendText (s);
}

void Element::normalize () {
    ElementPtr e = firstChild ();
    while (e) {
        ElementPtr tmp = e->nextSibling ();
        if (!strcmp (e->nodeName (), "#text")) {
            if (e->nodeValue ().stripWhiteSpace ().isEmpty ())
                removeChild (e);
        } else
            e->normalize ();
        e = tmp;
    }
}

static void getInnerText (const ElementPtr p, QTextOStream & out) {
    for (ElementPtr e = p->firstChild (); e; e = e->nextSibling ()) {
        if (!strcmp (e->nodeName (), "#text"))
            out << e->nodeValue ();
        else
            getInnerText (e, out);
    }
}

QString Element::innerText () const {
    QString buf;
    QTextOStream out (&buf);
    getInnerText (self (), out);
    return buf;
}

static void getOuterXML (const ElementPtr p, QTextOStream & out) {
    if (!strcmp (p->nodeName (), "#text"))
        out << XMLStringlet (p->nodeValue ());
    else {
        out << QChar ('<') << XMLStringlet (p->nodeName ());
        for (ElementPtr a = p->attributes().item (0); a; a = a->nextSibling ())
            out << " " << XMLStringlet (a->nodeName ()) << "=\"" << XMLStringlet (a->nodeValue ()) << "\"";
        if (p->hasChildNodes ()) {
            out << QChar ('>');
            for (ElementPtr e = p->firstChild (); e; e = e->nextSibling ())
                getOuterXML (e, out);
            out << QString("</") << XMLStringlet (p->nodeName()) << QChar ('>');
        } else
            out << QString ("/>");
    }
}

QString Element::innerXML () const {
    QString buf;
    QTextOStream out (&buf);
    for (ElementPtr e = firstChild (); e; e = e->nextSibling ())
        getOuterXML (e, out);
    return buf;
}

QString Element::outerXML () const {
    QString buf;
    QTextOStream out (&buf);
    getOuterXML (self (), out);
    return buf;
}

void Element::setAttributes (const NodeList & attrs) {
    m_first_attribute = attrs.item (0);
    opened ();
}

bool Element::isMrl () {
    return false;
}

void Element::opened () {}

void Element::setAttribute (const QString & name, const QString & value) {
    ElementPtr last_attribute;
    for (ElementPtr e = m_first_attribute; e; e = e->nextSibling ()) {
        if (convertNode <Attribute> (e)->name == name) {
            convertNode <Attribute> (e)->value = value;
            return;
        }
        last_attribute = e;
    }
    if (last_attribute) {
        last_attribute->m_next = (new Attribute (m_doc, name, value))->self ();
        last_attribute->m_next->m_prev = last_attribute;
    } else
        m_first_attribute = (new Attribute (m_doc, name, value))->self ();
}

QString Element::getAttribute (const QString & name) {
    QString value;
    for (ElementPtr e = m_first_attribute; e; e = e->nextSibling ())
        if (!name.compare (e->nodeName ())) {
            value = e->nodeValue ();
            break;
        }
    return value;
}

QString Element::nodeValue () const {
    return QString::null;
}

Attribute::Attribute (ElementPtr & d, const QString & n, const QString & v)
  : Element (d), name (n), value (v) {}

QString Attribute::nodeValue () const {
    return value;
}

static bool hasMrlChildren (const ElementPtr & e) {
    for (ElementPtr c = e->firstChild (); c; c = c->nextSibling ())
        if (c->isMrl () || hasMrlChildren (c))
            return true;
    return false;
}

Mrl::Mrl (ElementPtr & d) : Element (d), cached_ismrl_version (~0), parsed (false), bookmarkable (true) {}

KDE_NO_CDTOR_EXPORT Mrl::Mrl () : cached_ismrl_version (~0), parsed (false) {}

Mrl::~Mrl () {}

bool Mrl::isMrl () {
    if (cached_ismrl_version != document()->m_tree_version) {
        cached_ismrl = !hasMrlChildren (m_self);
        cached_ismrl_version = document()->m_tree_version;
        if (!src.isEmpty()) {
            if (pretty_name.isEmpty ())
                pretty_name = src;
            for (ElementPtr e = parentNode (); e; e = e->parentNode ()) {
                Mrl * mrl = e->mrl ();
                if (mrl)
                    src = KURL (mrl->src, src).url ();
            }
        }
    }
    return cached_ismrl;
}

ElementPtr Mrl::childFromTag (const QString & tag) {
    Element * elm = fromXMLDocumentGroup (m_doc, tag);
    if (elm)
        return elm->self ();
    return ElementPtr ();
}

ElementPtr Mrl::realMrl () {
    return m_self;
}

//-----------------------------------------------------------------------------

Document::Document (const QString & s) : m_tree_version (0) {
    m_doc = this;
    m_self = m_doc;
    src = s;
}

KDE_NO_CDTOR_EXPORT Document::~Document () {
    kdDebug () << "~Document\n";
}

KDE_NO_EXPORT ElementPtr Document::childFromTag (const QString & tag) {
    Element * elm = fromXMLDocumentGroup (m_doc, tag);
    if (elm)
        return elm->self ();
    return ElementPtr ();
}

void Document::dispose () {
    clear ();
    m_doc = 0L;
}

bool Document::isMrl () {
    return Mrl::isMrl ();
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT TextNode::TextNode (ElementPtr & d, const QString & s)
 : Element (d), text (s) {}

void TextNode::appendText (const QString & s) {
    text += s;
}

QString TextNode::nodeValue () const {
    return text;
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT Title::Title (ElementPtr & d) : Element (d) {}

KDE_NO_EXPORT bool Title::expose () {
    return false;
}

//-----------------------------------------------------------------------------

KDE_NO_EXPORT ElementPtr Smil::childFromTag (const QString & tag) {
    if (!strcmp (tag.latin1 (), "body"))
        return (new Body (m_doc))->self ();
    // else if head
    return ElementPtr ();
}

//-----------------------------------------------------------------------------

KDE_NO_EXPORT ElementPtr Body::childFromTag (const QString & tag) {
    Element * elm = fromScheduleGroup (m_doc, tag);
    if (!elm) elm = fromMediaContentGroup (m_doc, tag);
    if (!elm) elm = fromContentControlGroup (m_doc, tag);
    if (elm)
        return elm->self ();
    return ElementPtr ();
}

//-----------------------------------------------------------------------------

KDE_NO_EXPORT ElementPtr Par::childFromTag (const QString & tag) {
    Element * elm = fromScheduleGroup (m_doc, tag);
    if (!elm) elm = fromMediaContentGroup (m_doc, tag);
    if (!elm) elm = fromContentControlGroup (m_doc, tag);
    if (elm)
        return elm->self ();
    return ElementPtr ();
}

//-----------------------------------------------------------------------------

KDE_NO_EXPORT ElementPtr Seq::childFromTag (const QString & tag) {
    Element * elm = fromScheduleGroup (m_doc, tag);
    if (!elm) elm = fromMediaContentGroup (m_doc, tag);
    if (!elm) elm = fromContentControlGroup (m_doc, tag);
    if (elm)
        return elm->self ();
    return ElementPtr ();
}

//-----------------------------------------------------------------------------

KDE_NO_EXPORT ElementPtr Switch::childFromTag (const QString & tag) {
    Element * elm = fromContentControlGroup (m_doc, tag);
    if (!elm) elm = fromMediaContentGroup (m_doc, tag);
    if (elm)
        return elm->self ();
    return ElementPtr ();
}

bool Switch::isMrl () {
    return false; // TODO eval conditions on children and choose one
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT MediaType::MediaType (ElementPtr & d, const QString & t)
    : Mrl (d), m_type (t), bitrate (0) {}

KDE_NO_EXPORT ElementPtr MediaType::childFromTag (const QString & tag) {
    Element * elm = fromContentControlGroup (m_doc, tag);
    if (elm)
        return elm->self ();
    return ElementPtr ();
}

KDE_NO_EXPORT void MediaType::opened () {
    for (ElementPtr a = m_first_attribute; a; a = a->nextSibling ()) {
        const char * cname = a->nodeName ();
        if (!strcmp (cname, "system-bitrate"))
            bitrate = a->nodeValue ().toInt ();
        else if (!strcmp (cname, "src"))
            src = a->nodeValue ();
        else if (!strcmp (cname, "type"))
            mimetype = a->nodeValue ();
        else
            kdError () << "Warning: unhandled MediaType attr: " << cname << "=" << a->nodeValue () << endl;
    }
    kdDebug () << "MediaType attr found bitrate: " << bitrate << " src: " << (src.isEmpty() ? "-" : src) << " type: " << (mimetype.isEmpty() ? "-" : mimetype) << endl;
}

//-----------------------------------------------------------------------------

KDE_NO_EXPORT ElementPtr Asx::childFromTag (const QString & tag) {
    const char * name = tag.latin1 ();
    if (!strcasecmp (name, "entry"))
        return (new Entry (m_doc))->self ();
    else if (!strcasecmp (name, "entryref"))
        return (new EntryRef (m_doc))->self ();
    else if (!strcasecmp (name, "title"))
        return (new Title (m_doc))->self ();
    return ElementPtr ();
}

KDE_NO_EXPORT bool Asx::isMrl () {
    if (cached_ismrl_version != document ()->m_tree_version) {
        for (ElementPtr e = firstChild (); e; e = e->nextSibling ())
            if (!strcmp (e->nodeName (), "title"))
                pretty_name = e->innerText ();
    }
    return Mrl::isMrl ();
}
//-----------------------------------------------------------------------------

KDE_NO_EXPORT ElementPtr Entry::childFromTag (const QString & tag) {
    const char * name = tag.latin1 ();
    if (!strcasecmp (name, "ref"))
        return (new Ref (m_doc))->self ();
    else if (!strcasecmp (name, "title"))
        return (new Title (m_doc))->self ();
    return ElementPtr ();
}

KDE_NO_EXPORT bool Entry::isMrl () {
    if (cached_ismrl_version != document ()->m_tree_version) {
        QString pn;
        src.truncate (0);
        bool foundone = false;
        for (ElementPtr e = firstChild (); e; e = e->nextSibling ()) {
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

KDE_NO_EXPORT ElementPtr Entry::realMrl () {
    for (ElementPtr e = firstChild (); e; e = e->nextSibling ())
        if (e->isMrl ())
            return e;
    return m_self;
}

//-----------------------------------------------------------------------------

KDE_NO_EXPORT void Ref::opened () {
    for (ElementPtr a = m_first_attribute; a; a = a->nextSibling ()) {
        if (!strcasecmp (a->nodeName (), "href"))
            src = a->nodeValue ();
        else
            kdError () << "Warning: unhandled Ref attr: " << a->nodeName () << "=" << a->nodeValue () << endl;

    }
    kdDebug () << "Ref attr found src: " << src << endl;
}

//-----------------------------------------------------------------------------

KDE_NO_EXPORT void EntryRef::opened () {
    for (ElementPtr a = m_first_attribute; a; a = a->nextSibling ()) {
        if (!strcasecmp (a->nodeName (), "href"))
            src = a->nodeValue ();
        else
            kdError () << "unhandled EntryRef attr: " << a->nodeName () << "=" << a->nodeValue () << endl;
    }
    kdDebug () << "EntryRef attr found src: " << src << endl;
}

//-----------------------------------------------------------------------------

GenericURL::GenericURL (ElementPtr & d, const QString & s, const QString & name)
 : Mrl (d) {
    src = s;
    pretty_name = name;
}

GenericMrl::GenericMrl (ElementPtr & d, const QString & s, const QString & name)
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
    DocumentBuilder (ElementPtr d) : position (0), m_doc (d), equal_seen (false), in_dbl_quote (false), in_sngl_quote (false), have_error (false) {}
    virtual ~DocumentBuilder () {};
    bool parse (QTextStream & d);
protected:
    virtual bool startTag (const QString & tag, const NodeList & attr)=0;
    virtual bool endTag (const QString & tag)=0;
    virtual bool characterData (const QString & data)= 0;
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
    ElementPtr m_doc;
    ElementPtr m_first_attribute, m_last_attribute;
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
    ElementPtr m_elm;
    ElementPtr m_root;
public:
    KMPlayerDocumentBuilder (ElementPtr d);
    ~KMPlayerDocumentBuilder () {}
protected:
    bool startTag (const QString & tag, const NodeList & attr);
    bool endTag (const QString & tag);
    bool characterData (const QString & data);
};

void readXML (ElementPtr root, QTextStream & in, const QString & firstline) {
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
    if (m_first_attribute) {
        m_last_attribute->m_next = (new Attribute (m_doc, attr_name, attr_value))->self();
        m_last_attribute->m_next->m_prev = m_last_attribute;
        m_last_attribute = m_last_attribute->m_next;
    } else
        m_first_attribute = m_last_attribute = (new Attribute (m_doc, attr_name, attr_value))->self();
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
        have_error = startTag (tagname, NodeList (m_first_attribute));
        if (closed)
            have_error &= endTag (tagname);
        //kdDebug () << "readTag " << tagname << " closed:" << closed << " ok:" << have_error << endl;
    }
    m_state = m_state->next; // pop Element or PI
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
                        m_first_attribute = m_last_attribute = 0L;
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

KMPlayerDocumentBuilder::KMPlayerDocumentBuilder (ElementPtr d)
 : DocumentBuilder (d->document ()->self ()), m_ignore_depth (0),
   m_elm (d), m_root (d) {}

bool KMPlayerDocumentBuilder::startTag (const QString & tag, const NodeList & attr) {
    if (m_ignore_depth) {
        m_ignore_depth++;
        //kdDebug () << "Warning: ignored tag " << tag.latin1 () << " ignore depth = " << m_ignore_depth << endl;
    } else {
        ElementPtr e = m_elm->childFromTag (tag);
        if (e) {
            //kdDebug () << "Found tag " << tag.latin1 () << endl;
            e->setAttributes (attr);
            m_elm->appendChild (e);
            m_elm = e;
        } else {
            m_ignore_depth = 1;
            kdDebug () << "Warning: unknown tag " << tag.latin1 () << " ignore depth = " << m_ignore_depth << endl;
        }
    }
    return true;
}

bool KMPlayerDocumentBuilder::endTag (const QString & tag) {
    if (m_ignore_depth) { // endtag to ignore
        m_ignore_depth--;
        kdDebug () << "Warning: ignored end tag " << " ignore depth = " << m_ignore_depth <<  endl;
    } else {  // endtag
        if (m_elm == m_root) {
            kdDebug () << "m_elm == m_doc, stack underflow " << endl;
            return false;
        }
        //kdDebug () << "end tag " << tag.latin1 () << endl;
        m_elm = m_elm->parentNode ();
    }
    return true;
}

bool KMPlayerDocumentBuilder::characterData (const QString & data) {
    if (!m_ignore_depth)
        m_elm->characterData (data);
    //kdDebug () << "characterData " << d.latin1() << endl;
    return true;
}
