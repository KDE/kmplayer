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

#include <qdom.h>
#include <qxml.h>
#include <qfile.h>
#include <qregexp.h>
#include <qtextstream.h>
#include <kdebug.h>
#include "kmplayerplaylist.h"

static Element * fromXMLDocumentGroup (ElementPtr d, const QString & tag) {
    const char * const name = tag.latin1 ();
    if (!strcmp (name, "smil"))
        return new Smil (d);
    else if (!strcasecmp (name, "asx"))
        return new Asx (d);
    return 0L;
}

static Element * fromScheduleGroup (ElementPtr d, const QString & tag) {
    if (!strcmp (tag.latin1 (), "par"))
        return new Par (d);
    else if (!strcmp (tag.latin1 (), "seq"))
        return new Seq (d);
    // else if (!strcmp (tag.latin1 (), "excl"))
    //    return new Seq (d, p);
    return 0L;
}

static Element * fromMediaContentGroup (ElementPtr d, const QString & tag) {
    if (!strcmp (tag.latin1 (), "video") || !strcmp (tag.latin1 (), "audio"))
        return new MediaType (d, tag);
    // text, img, animation, textstream, ref, brush
    return 0L;
};

static Element * fromContentControlGroup (ElementPtr d, const QString & tag) {
    if (!strcmp (tag.latin1 (), "switch"))
        return new Switch (d);
    return 0L;
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT Element::~Element () {
    clear ();
}

Document * Element::document () {
    return static_cast<Document*>(static_cast<Element *>(m_doc));
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

KDE_NO_EXPORT bool Element::expose () {
    return true;
}

KDE_NO_EXPORT void Element::clear () {
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

KDE_NO_EXPORT void Element::removeChild (ElementPtr c) {
    document()->m_tree_version++;
    if (c->m_prev) {
        c->m_prev->m_next = c->m_next;
        c->m_prev = 0L;
    } else
        m_first_child = c->m_next;
    if (c->m_next) {
        c->m_next->m_prev = c->m_prev;
        c->m_next = 0L;
    } else
        m_last_child = c->m_prev;
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

KDE_NO_EXPORT ElementPtr Element::childFromTag (const QString &) {
    return 0L;
}

KDE_NO_EXPORT void Element::characterData (const QString & s) {
    document()->m_tree_version++;
    kdDebug () << !m_last_child << (!m_last_child || strcmp (m_last_child->nodeName (), "#text")) << (m_last_child ? m_last_child->nodeName () : "-") << endl;
    if (!m_last_child || strcmp (m_last_child->nodeName (), "#text"))
        appendChild ((new TextNode (m_doc, s))->self ());
    else
        static_cast <TextNode *> (static_cast <Element *> (m_last_child))->appendText (s);
}

static void getInnerText (const ElementPtr p, QTextOStream & out) {
    for (ElementPtr e = p->firstChild (); e; e = e->nextSibling ()) {
        if (!strcmp (e->nodeName (), "#text"))
            out << (static_cast<TextNode*>(static_cast<Element *>(e)))->text;
        else
            out << QChar ('<') << e->nodeName () << QChar ('>'); //TODO attr.
        getInnerText (e, out);
    }
}

QString Element::innerText () const {
    QString buf;
    QTextOStream out (&buf);
    getInnerText (self (), out);
    return buf;
}

KDE_NO_EXPORT void Element::setAttributes (const QXmlAttributes & atts) {
    document()->m_tree_version++;
    for (int i = 0; i < atts.length (); i++)
        kdDebug () << " " << atts.qName (i) << "=" << atts.value (i) << endl;
}

bool Element::isMrl () {
    return false;
}

static bool hasMrlChildren (const ElementPtr & e) {
    for (ElementPtr c = e->firstChild (); c; c = c->nextSibling ())
        if (c->isMrl () || hasMrlChildren (c))
            return true;
    return false;
}

Mrl::Mrl (ElementPtr d) : Element (d), cached_ismrl_version (~0), parsed (false) {}

KDE_NO_CDTOR_EXPORT Mrl::Mrl () : cached_ismrl_version (~0), parsed (false) {}

KDE_NO_CDTOR_EXPORT Mrl::~Mrl () {}

bool Mrl::isMrl () {
    if (cached_ismrl_version != document()->m_tree_version) {
        cached_ismrl = !hasMrlChildren (m_self);
        cached_ismrl_version = document()->m_tree_version;
    }
    return cached_ismrl;
}

KDE_NO_EXPORT ElementPtr Mrl::childFromTag (const QString & tag) {
    Element * elm = fromXMLDocumentGroup (m_doc, tag);
    if (elm)
        return elm->self ();
    return 0L;
}

KDE_NO_EXPORT ElementPtr Mrl::realMrl () {
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
};

KDE_NO_EXPORT ElementPtr Document::childFromTag (const QString & tag) {
    Element * elm = fromXMLDocumentGroup (m_doc, tag);
    if (elm)
        return elm->self ();
    return 0L;
}

void Document::dispose () {
    clear ();
    m_doc = 0L;
}

bool Document::isMrl () {
    return Mrl::isMrl ();
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT TextNode::TextNode (ElementPtr d, const QString & s)
 : Element (d), text (s) {}

void TextNode::appendText (const QString & s) {
    text += s;
}
//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT Title::Title (ElementPtr d) : Element (d) {}

KDE_NO_EXPORT bool Title::expose () {
    return false;
}

//-----------------------------------------------------------------------------

KDE_NO_EXPORT ElementPtr Smil::childFromTag (const QString & tag) {
    if (!strcmp (tag.latin1 (), "body"))
        return (new Body (m_doc))->self ();
    // else if head
    return 0L;
}

//-----------------------------------------------------------------------------

KDE_NO_EXPORT ElementPtr Body::childFromTag (const QString & tag) {
    Element * elm = fromScheduleGroup (m_doc, tag);
    if (!elm) elm = fromMediaContentGroup (m_doc, tag);
    if (!elm) elm = fromContentControlGroup (m_doc, tag);
    if (elm)
        return elm->self ();
    return 0L;
}

//-----------------------------------------------------------------------------

KDE_NO_EXPORT ElementPtr Par::childFromTag (const QString & tag) {
    Element * elm = fromScheduleGroup (m_doc, tag);
    if (!elm) elm = fromMediaContentGroup (m_doc, tag);
    if (!elm) elm = fromContentControlGroup (m_doc, tag);
    if (elm)
        return elm->self ();
    return 0L;
}

//-----------------------------------------------------------------------------

KDE_NO_EXPORT ElementPtr Seq::childFromTag (const QString & tag) {
    Element * elm = fromScheduleGroup (m_doc, tag);
    if (!elm) elm = fromMediaContentGroup (m_doc, tag);
    if (!elm) elm = fromContentControlGroup (m_doc, tag);
    if (elm)
        return elm->self ();
    return 0L;
}

//-----------------------------------------------------------------------------

KDE_NO_EXPORT ElementPtr Switch::childFromTag (const QString & tag) {
    Element * elm = fromContentControlGroup (m_doc, tag);
    if (!elm) elm = fromMediaContentGroup (m_doc, tag);
    if (elm)
        return elm->self ();
    return 0L;
}

bool Switch::isMrl () {
    return false; // TODO eval conditions on children and choose one
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT MediaType::MediaType (ElementPtr d, const QString & t)
    : Mrl (d), m_type (t), bitrate (0) {}

KDE_NO_EXPORT ElementPtr MediaType::childFromTag (const QString & tag) {
    Element * elm = fromContentControlGroup (m_doc, tag);
    if (elm)
        return elm->self ();
    return 0L;
}

KDE_NO_EXPORT void MediaType::setAttributes (const QXmlAttributes & atts) {
    for (int i = 0; i < atts.length (); i++) {
        const char * attr = atts.qName (i).latin1();
        if (!strcmp (attr, "system-bitrate"))
            bitrate = atts.value (i).toInt ();
        else if (!strcmp (attr, "src"))
            src = atts.value (i);
        else if (!strcmp (attr, "type"))
            mimetype = atts.value (i);
        else
            kdError () << "Warning: unhandled MediaType attr: " << attr << "=" << atts.value (i) << endl;
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
    return 0L;
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
    return 0L;
}

KDE_NO_EXPORT bool Entry::isMrl () {
    if (cached_ismrl_version != document ()->m_tree_version) {
        src.truncate (0);
        bool foundone = false;
        for (ElementPtr e = firstChild (); e; e = e->nextSibling ()) {
            if (e->isMrl () && !e->hasChildNodes ()) {
                if (foundone)
                    src.truncate (0);
                else
                    src = e->mrl ()->src;
                foundone = true;
            } else if (!strcmp (e->nodeName (), "title"))
                pretty_name = e->innerText ();
        }
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

KDE_NO_EXPORT void Ref::setAttributes (const QXmlAttributes & atts) {
    for (int i = 0; i < atts.length (); i++)
        if (!strcasecmp (atts.qName (i).latin1(), "href"))
            src = atts.value (i);
        else
            kdError () << "Warning: unhandled Ref attr: " << atts.qName (i) << "=" << atts.value (i) << endl;
    kdDebug () << "Ref attr found src: " << src << endl;
}

//-----------------------------------------------------------------------------

KDE_NO_EXPORT void EntryRef::setAttributes (const QXmlAttributes & atts) {
    for (int i = 0; i < atts.length (); i++)
        if (!strcasecmp (atts.qName (i).latin1(), "href"))
            src = atts.value (i);
        else
            kdError () << "unhandled EntryRef attr: " << atts.qName (i) << "=" << atts.value (i) << endl;
    kdDebug () << "EntryRef attr found src: " << src << endl;
}

//-----------------------------------------------------------------------------

GenericURL::GenericURL (ElementPtr d, const QString & s, const QString & name)
 : Mrl (d) {
    src = s;
    pretty_name = name;
}

bool GenericURL::isMrl () {
    return Mrl::isMrl ();
}

//-----------------------------------------------------------------------------

class MMXmlContentHandler : public QXmlDefaultHandler {
public:
    KDE_NO_CDTOR_EXPORT MMXmlContentHandler (ElementPtr d) : m_start (d), m_elm (d), m_ignore_depth (0) {}
    KDE_NO_CDTOR_EXPORT ~MMXmlContentHandler () {}
    KDE_NO_EXPORT bool startDocument () {
        return m_start->self () == m_elm;
    }
    KDE_NO_EXPORT bool endDocument () {
        return m_start->self () == m_elm;
    }
    KDE_NO_EXPORT bool startElement (const QString &, const QString &, const QString & tag,
            const QXmlAttributes & atts) {
        if (m_ignore_depth) {
            kdDebug () << "Warning: ignored tag " << tag << endl;
            m_ignore_depth++;
        } else {
            ElementPtr e = m_elm->childFromTag (tag);
            if (e) {
                kdDebug () << "Found tag " << tag << endl;
                e->setAttributes (atts);
                m_elm->appendChild (e);
                m_elm = e;
            } else {
                kdError () << "Warning: unknown tag " << tag << endl;
                m_ignore_depth = 1;
            }
        }
        return true;
    }
    KDE_NO_EXPORT bool endElement (const QString & /*nsURI*/,
                   const QString & /*tag*/, const QString & /*fqtag*/) {
        if (m_ignore_depth) {
            // kdError () << "Warning: ignored end tag " << endl;
            m_ignore_depth--;
            return true;
        }
        if (m_elm == m_start->self ()) {
            kdError () << "m_elm == m_start, stack underflow " << endl;
            return false;
        }
        // kdError () << "end tag " << endl;
        m_elm = m_elm->parentNode ();
        return true;
    }
    KDE_NO_EXPORT bool characters (const QString & ch) {
        if (m_ignore_depth)
            return true;
        if (!m_elm)
            return false;
        m_elm->characterData (ch);
        return true;
    }
    KDE_NO_EXPORT bool fatalError (const QXmlParseException & ex) {
        kdError () << "fatal error " << ex.message () << endl;
        return true;
    }
    ElementPtr m_start;
    ElementPtr m_elm;
    int m_ignore_depth;
};

class FilteredInputSource : public QXmlInputSource {
    QTextStream  & textstream;
    QString buffer;
    int pos;
public:
    KDE_NO_CDTOR_EXPORT FilteredInputSource (QTextStream & ts, const QString & b) : textstream (ts), buffer (b), pos (0) {}
    KDE_NO_CDTOR_EXPORT ~FilteredInputSource () {}
    KDE_NO_EXPORT QString data () { return textstream.read (); }
    void fetchData ();
    QChar next ();
};

KDE_NO_EXPORT QChar FilteredInputSource::next () {
    if (pos + 8 >= (int) buffer.length ())
        fetchData ();
    if (pos >= (int) buffer.length ())
        return QXmlInputSource::EndOfData;
    QChar ch = buffer.at (pos++);
    if (ch == QChar ('&')) {
        QRegExp exp (QString ("\\w+;"));
        if (buffer.find (exp, pos) != pos) {
            buffer = QString ("&amp;") + buffer.mid (pos);
            pos = 1;
        }
    }
    return ch;
}

KDE_NO_EXPORT void FilteredInputSource::fetchData () {
    if (pos > 0)
        buffer = buffer.mid (pos);
    pos = 0;
    if (textstream.atEnd ())
        return;
    buffer += textstream.readLine ();
}

void readXML (ElementPtr root, QTextStream & in, const QString & firstline) {
    QXmlSimpleReader reader;
    MMXmlContentHandler content_handler (root);
    FilteredInputSource input_source (in, firstline);
    reader.setContentHandler (&content_handler);
    reader.setErrorHandler (&content_handler);
    reader.parse (&input_source);
}

