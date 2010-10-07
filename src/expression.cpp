/**
  This file belong to the KMPlayer project, a movie player plugin for Konqueror
  Copyright (C) 2009  Koos Vriezen <koos.vriezen@gmail.com>

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
**/

#ifdef KMPLAYER_EXPR_DEBUG
#include <stdio.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <qurl.h>
#include "expression.h"

using namespace KMPlayer;

QString NodeValue::value () const {
    if (attr)
        return attr->value ();
    return node->nodeValue ();
}

namespace {

struct EvalState {
    EvalState (EvalState *p, const QString &root_tag=QString())
     : def_root_tag (root_tag), root (NULL), attr (NULL),
       process_list (NULL), parent (p),
       sequence (1), ref_count (0) {}

    void addRef () { ++ref_count; }
    void removeRef () { if (--ref_count == 0) delete this; }

    QString value () const {
        if (attr)
            return attr->value ();
        return root->nodeValue ();
    }

    QString def_root_tag;
    Node *root;
    Attribute *attr;
    Sequence *process_list;
    EvalState *parent;
    int sequence;
    int ref_count;
};

struct AST : public Expression {
    enum Type {
        TUnknown, TInteger, TBool, TFloat, TString
    };

    AST (EvalState *ev);
    virtual ~AST ();

    virtual bool toBool () const;
    virtual int toInt () const;
    virtual float toFloat () const;
    virtual QString toString () const;
    virtual Sequence *toSequence () const; // uncachable
    virtual Type type () const;
    virtual void setRoot (Node *root);
    void setRoot (Node *root, Attribute *a);
#ifdef KMPLAYER_EXPR_DEBUG
    virtual void dump () const;
#endif

    mutable int sequence;
    mutable EvalState *eval_state;
    AST *first_child;
    AST *next_sibling;
};

struct BoolBase : public AST {
    BoolBase (EvalState *ev) : AST (ev), b (false) {}

    virtual QString toString () const;
    virtual Type type () const;

    mutable bool b;
};

struct NumberBase : public AST {
    NumberBase (EvalState *ev) : AST (ev) {}

    virtual bool toBool () const;
};

struct IntegerBase : public NumberBase {
    IntegerBase (EvalState *ev) : NumberBase (ev), i (0) {}

    virtual float toFloat () const;
    virtual Type type () const;

    mutable int i;
};

struct Integer : public IntegerBase {
    Integer (EvalState *ev, int i_) : IntegerBase (ev) {
        i = i_;
    }

    virtual int toInt () const;
#ifdef KMPLAYER_EXPR_DEBUG
    virtual void dump () const;
#endif
};

struct Float : public AST {
    Float (EvalState *ev, float f_) : AST (ev), f (f_) {}

    bool toBool () const { return false; }
    int toInt () const { return (int) f; }
    float toFloat () const { return f; }
    virtual Type type () const;
#ifdef KMPLAYER_EXPR_DEBUG
    virtual void dump () const {
        fprintf (stderr, "Float %f", f);
        AST::dump();
    }
#endif

    float f;
};

struct StringBase : public AST {
    StringBase (EvalState *ev) : AST (ev) {}
    StringBase (EvalState *ev, const char *s, const char *e)
     : AST (ev),
       string (QString::fromUtf8 (e ? QByteArray (s, e - s).data () : s)) {}

    virtual bool toBool () const;
    virtual int toInt () const;
    virtual float toFloat () const;
    virtual Type type () const;

    mutable QString string;
};

struct Step : public StringBase {
    Step (EvalState *ev, bool context=false)
     : StringBase (ev),
       any_node (false), context_node (context),
       is_attr (false), start_contextual (false) {}
    Step (EvalState *ev, const char *s, const char *e, bool isattr=false)
     : StringBase (ev, s, e),
       any_node (string == "*"), context_node (false),
       is_attr (isattr), start_contextual (false) {}

    bool matches (Node *n);
    bool matches (Attribute *a);
    bool selected (Node *n, Attribute *a);
    bool anyPath () const { return string.isEmpty (); }
#ifdef KMPLAYER_EXPR_DEBUG
    virtual void dump () const {
        fprintf (stderr, "Step %c%s",
                is_attr ? '@' : ' ',
                context_node ? "." : string.toAscii ().constData ());
        AST::dump();
    }
#endif

    bool any_node;
    bool context_node;
    bool is_attr;
    bool start_contextual;
};

struct Identifier : public StringBase {
    Identifier (EvalState *ev, AST *steps) : StringBase (ev) {
        first_child = steps;
    }

    virtual bool toBool () const;
    virtual QString toString () const;
    virtual Sequence *toSequence () const;
    virtual Type type () const;
    void childByStep (Step *step, bool recurse) const;
    void childByPath (Step *step, bool recurse) const;
#ifdef KMPLAYER_EXPR_DEBUG
    virtual void dump () const {
        fprintf (stderr, "Identifier ");
        AST::dump();
    }
#endif
};

struct StringLiteral : public StringBase {
    StringLiteral (EvalState *ev, const char *s, const char *e)
     : StringBase (ev, s, e) {}

    virtual QString toString () const;
    virtual Type type () const;
#ifdef KMPLAYER_EXPR_DEBUG
    virtual void dump () const {
        fprintf (stderr, "StringLiteral %s", string.toAscii ().constData ());
        AST::dump();
    }
#endif
};

struct Contains : public BoolBase {
    Contains (EvalState *ev) : BoolBase (ev) {}

    virtual bool toBool () const;
};

struct Not : public BoolBase {
    Not (EvalState *ev) : BoolBase (ev) {}

    virtual bool toBool () const;
};

struct StartsWith: public BoolBase {
    StartsWith (EvalState *ev) : BoolBase (ev) {}

    virtual bool toBool () const;
};

struct Count : public IntegerBase {
    Count (EvalState *ev) : IntegerBase (ev) {}

    virtual int toInt () const;
};

struct HoursFromTime : public IntegerBase {
    HoursFromTime (EvalState *ev) : IntegerBase (ev) {}

    virtual int toInt () const;
};

struct MinutesFromTime : public IntegerBase {
    MinutesFromTime (EvalState *ev) : IntegerBase (ev) {}

    virtual int toInt () const;
};

struct SecondsFromTime : public IntegerBase {
    SecondsFromTime (EvalState *ev) : IntegerBase (ev) {}

    virtual int toInt () const;
};

struct Last : public IntegerBase {
    Last (EvalState *ev) : IntegerBase (ev) {}

    virtual int toInt () const;
};

struct Number : public IntegerBase {
    Number (EvalState *ev) : IntegerBase (ev) {}

    virtual int toInt () const;
};

struct Position : public IntegerBase {
    Position (EvalState *ev) : IntegerBase (ev) {}

    virtual int toInt () const;
};

struct Concat : public StringBase {
    Concat (EvalState *ev) : StringBase (ev) {}

    virtual QString toString () const;
};

struct StringJoin : public StringBase {
    StringJoin (EvalState *ev) : StringBase (ev) {}

    virtual QString toString () const;
};

struct SubstringAfter : public StringBase {
    SubstringAfter (EvalState *ev) : StringBase (ev) {}

    virtual QString toString () const;
};

struct SubstringBefore : public StringBase {
    SubstringBefore (EvalState *ev) : StringBase (ev) {}

    virtual QString toString () const;
};

struct CurrentTime : public StringBase {
    CurrentTime (EvalState *ev) : StringBase (ev) {}

    virtual QString toString () const;
};

struct CurrentDate : public StringBase {
    CurrentDate (EvalState *ev) : StringBase (ev) {}

    virtual QString toString () const;
};

struct EscapeUri : public StringBase {
    EscapeUri (EvalState *ev) : StringBase (ev) {}

    virtual QString toString () const;
};

struct Sort : public AST {
    Sort (EvalState *ev) : AST (ev) {}

    virtual Sequence *toSequence () const;
};

struct Multiply : public NumberBase {
    Multiply (EvalState *ev, AST *children) : NumberBase (ev) {
        first_child = children;
    }

    virtual int toInt () const;
    virtual float toFloat () const;
    virtual Type type () const;
#ifdef KMPLAYER_EXPR_DEBUG
    virtual void dump () const;
#endif
};

struct Divide : public NumberBase {
    Divide (EvalState *ev, AST *children) : NumberBase (ev) {
        first_child = children;
    }

    virtual int toInt () const;
    virtual float toFloat () const;
    virtual Type type () const;
#ifdef KMPLAYER_EXPR_DEBUG
    virtual void dump () const;
#endif
};

struct Modulus : public NumberBase {
    Modulus (EvalState *ev, AST *children) : NumberBase (ev) {
        first_child = children;
    }

    virtual int toInt () const;
    virtual float toFloat () const;
    virtual Type type () const;
#ifdef KMPLAYER_EXPR_DEBUG
    virtual void dump () const;
#endif
};

struct Plus : public NumberBase {
    Plus (EvalState *ev, AST *children) : NumberBase (ev) {
        first_child = children;
    }

    virtual int toInt () const;
    virtual float toFloat () const;
    virtual Type type () const;
#ifdef KMPLAYER_EXPR_DEBUG
    virtual void dump () const;
#endif
};

struct Minus : public NumberBase {
    Minus (EvalState *ev, AST *children) : NumberBase (ev) {
        first_child = children;
    }

    virtual int toInt () const;
    virtual float toFloat () const;
    virtual Type type () const;
#ifdef KMPLAYER_EXPR_DEBUG
    virtual void dump () const;
#endif
};

struct Join : public AST {
    Join (EvalState *ev, AST *children) : AST (ev) {
        first_child = children;
    }

    virtual Sequence *toSequence () const;
#ifdef KMPLAYER_EXPR_DEBUG
    virtual void dump () const;
#endif
};

struct Comparison : public BoolBase {
    enum CompType {
        lt = 1, lteq, eq, noteq, gt, gteq, land, lor
    };

    Comparison (EvalState *ev, CompType ct, AST *children)
     : BoolBase (ev), comp_type (ct) {
        first_child = children;
    }

    virtual bool toBool () const;
#ifdef KMPLAYER_EXPR_DEBUG
    virtual void dump () const;
#endif

    CompType comp_type;
};

}

AST::AST (EvalState *ev)
 : sequence (0), eval_state (ev), first_child (NULL), next_sibling (NULL) {
    ev->addRef ();
}

AST::~AST () {
    while (first_child) {
        AST *tmp = first_child;
        first_child = first_child->next_sibling;
        delete tmp;
    }
    eval_state->removeRef ();
}

bool AST::toBool () const {
    return toInt ();
}

int AST::toInt () const {
    return 0;
}

float AST::toFloat () const {
    return 0.0;
}

QString AST::toString () const {
    switch (type ()) {
    case TBool:
        return toBool () ? "true" : "false";
    case TInteger:
        return QString::number (toInt ());
    case TFloat:
        return QString::number (toFloat ());
    default:
        return QString ();
    }
}

Sequence *AST::toSequence () const {
    return new Sequence;
}

AST::Type AST::type () const {
    return TUnknown;
}

void AST::setRoot (Node *root) {
    eval_state->root = root;
    eval_state->attr = NULL;
    eval_state->sequence++;
}

void AST::setRoot (Node *root, Attribute *a) {
    setRoot (root);
    eval_state->attr = a;
}

#ifdef KMPLAYER_EXPR_DEBUG
void AST::dump () const {
    if (first_child) {
        fprintf (stderr, "[ ");
        for (AST *child = first_child; child; child = child->next_sibling) {
            if (child != first_child)
                fprintf (stderr, ", ");
            child->dump();
        }
        fprintf (stderr, " ]");
    }
}
#endif

static void appendASTChild (AST *p, AST *c) {
    if (!p->first_child)
        p->first_child = c;
    else
        for (AST *chld = p->first_child; chld; chld = chld->next_sibling)
            if (!chld->next_sibling) {
                chld->next_sibling = c;
                break;
            }
}

static AST *releaseLastASTChild (AST *p) {
    AST **chldptr = &p->first_child;
    while ((*chldptr)->next_sibling)
        chldptr = &(*chldptr)->next_sibling;
    AST *last = *chldptr;
    *chldptr = NULL;
    return last;
}

QString BoolBase::toString () const {
    return toBool () ? "true" : "false";
}

AST::Type BoolBase::type () const {
    return TBool;
}

bool NumberBase::toBool () const {
    int ii = toInt ();
    if (eval_state->parent) {
        Sequence *lst = eval_state->parent->process_list;
        if (lst) {
            int count = 0;
            for (NodeValueItem *n = lst->first (); n; n = n->nextSibling ())
                if (ii == ++count)
                    return eval_state->root == n->data.node &&
                        eval_state->attr == n->data.attr;
        }
        return false;
    }
    return ii;
}

float IntegerBase::toFloat () const {
    return toInt ();
}

AST::Type IntegerBase::type () const {
    return TInteger;
}

int Integer::toInt () const {
    return i;
}

#ifdef KMPLAYER_EXPR_DEBUG
void Integer::dump () const {
    fprintf (stderr, "Integer %d", i);
    AST::dump();
}
#endif

AST::Type Float::type () const {
    return TFloat;
}

bool StringBase::toBool () const {
    QString s = toString ();
    if (s.toLower () == "true")
        return true;
    if (s.toLower () == "false")
        return false;
    return s.toInt ();
}

int StringBase::toInt () const {
    return toString ().toInt ();
}

float StringBase::toFloat () const {
    return toString ().toFloat ();
}

AST::Type StringBase::type () const {
    return TString;
}

bool Step::matches (Node *n) {
    return any_node || string == n->nodeName ();
}

bool Step::matches (Attribute *a) {
    return any_node || string == a->name ();
}

bool Step::selected (Node *n, Attribute *a) {
    if (first_child) {
        first_child->setRoot (n, a);
        return first_child->toBool ();
    }
    return true;
}

bool Identifier::toBool () const {
    bool b = false;
    if (eval_state->parent) {
        sequence = eval_state->sequence;
        Sequence *lst = toSequence ();
        b = lst && lst->first ();
        delete lst;
    } else {
        b = StringBase::toBool ();
    }
    return b;
}

void Identifier::childByStep (Step *step, bool recurse) const {
    Sequence *lst = eval_state->process_list;
    NodeValueItem *last = lst->last ();
    Sequence recursive_lst;
    for (NodeValueItem *itm = lst->first (); itm; ) {
        NodeValueItem *next = itm == last ? NULL : itm->nextSibling ();

        Node *n = itm->data.node;
        if (step->is_attr) {
            Element *e = n->isElementNode() ? static_cast<Element *> (n) : NULL;
            Attribute *a = e ? e->attributes ().first () : NULL;
            for (; a; a = a->nextSibling ())
                if (step->matches (a))
                    lst->append (new NodeValueItem (NodeValue (n, a)));
            if (recurse)
                for (Node *c = n->firstChild(); c; c = c->nextSibling ())
                    recursive_lst.append (new NodeValueItem (c));
        } else if (step->context_node) {
            if (eval_state->parent)
                lst->append (new NodeValueItem (eval_state->parent->root));
        } else {
            for (Node *c = n->firstChild(); c; c = c->nextSibling ()) {
                if (step->matches (c))
                    lst->append (new NodeValueItem (c));
                if (recurse)
                    recursive_lst.append (new NodeValueItem (c));
            }
        }

        lst->remove (itm);
        itm = next;
    }
    if (recurse && recursive_lst.first ()) {
        eval_state->process_list = &recursive_lst;
        childByStep (step, recurse);
        for (NodeValueItem *r = recursive_lst.first (); r; r = r->nextSibling ())
            lst->append (new NodeValueItem (r->data));
        eval_state->process_list = lst;
    }
}

void Identifier::childByPath (Step *step, bool recurse) const {
    if (!step->anyPath ()) {
        childByStep (step, recurse);
        NodeValueItem *itm = eval_state->process_list->first ();
        if (itm && step->first_child) {
            Sequence *newlist = new Sequence;
            for (; itm; itm = itm->nextSibling ())
                if (step->selected (itm->data.node, itm->data.attr))
                    newlist->append (new NodeValueItem (itm->data));
            delete eval_state->process_list;
            eval_state->process_list = newlist;
        }
    }
    if (step->next_sibling)
        childByPath ((Step *) step->next_sibling, step->anyPath ());
}

QString Identifier::toString () const {
    if (eval_state->sequence != sequence) {
        Sequence *lst = toSequence ();
        int i = lst->length ();
        if (i == 1)
            string = lst->first ()->data.value ();
        else
            string = QString::number (i);
        delete lst;
        sequence = eval_state->sequence;
    }
    return string;
}

Sequence *Identifier::toSequence () const {
    Sequence *old = eval_state->process_list;
    Sequence *lst = new Sequence;
    eval_state->process_list = lst;
    EvalState *es = eval_state;
    if (!((Step *)first_child)->start_contextual)
        while (es->parent)
            es = es->parent;
    // TODO handle ..
    lst->append (new NodeValueItem (es->root));
    childByPath ((Step *) first_child, false);
    lst = eval_state->process_list;
    eval_state->process_list = old;
    return lst;
}

AST::Type Identifier::type () const {
    QString s = toString ();
    if (s.toLower () == "true" ||
            s.toLower () == "false")
        return TBool;
    bool ok;
    s.toInt (&ok);
    if (ok)
        return TInteger;
    s.toFloat (&ok);
    if (ok)
        return TFloat;
    return TString;
}

QString StringLiteral::toString () const {
    return string;
}

AST::Type StringLiteral::type () const {
    return TString;
}

bool Contains::toBool () const {
    if (eval_state->sequence != sequence) {
        sequence = eval_state->sequence;
        b = false;
        if (first_child) {
            AST *s = first_child->next_sibling;
            if (s)
                b = first_child->toString ().indexOf (s->toString ()) > -1;
        }
    }
    return b;
}

bool Not::toBool () const {
    if (eval_state->sequence != sequence) {
        sequence = eval_state->sequence;
        b = first_child ? !first_child->toBool () : true;
    }
    return b;
}

bool StartsWith::toBool () const {
    if (eval_state->sequence != sequence) {
        sequence = eval_state->sequence;
        b = false;
        if (first_child) {
            AST *s = first_child->next_sibling;
            if (s)
                b = first_child->toString ().startsWith (s->toString ());
            else if (eval_state->parent)
                b = eval_state->value ().startsWith (first_child->toString ());
        }
    }
    return b;
}

int Count::toInt () const {
    if (eval_state->sequence != sequence) {
        sequence = eval_state->sequence;
        if (first_child) {
            Sequence *lst = first_child->toSequence ();
            i = lst->length ();
            delete lst;
        } else {
            i = 0;
        }
    }
    return i;
}

int HoursFromTime::toInt () const {
    if (eval_state->sequence != sequence) {
        if (first_child) {
            QString s = first_child->toString ();
            int p = s.indexOf (':');
            if (p > -1)
                i = s.left (p).toInt ();
        }
        sequence = eval_state->sequence;
    }
    return i;
}

int MinutesFromTime::toInt () const {
    if (eval_state->sequence != sequence) {
        if (first_child) {
            QString s = first_child->toString ();
            int p = s.indexOf (':');
            if (p > -1) {
                int q = s.indexOf (':', p + 1);
                if (q > -1)
                    i = s.mid (p + 1, q - p - 1).toInt ();
            }
        }
        sequence = eval_state->sequence;
    }
    return i;
}

int SecondsFromTime::toInt () const {
    if (eval_state->sequence != sequence) {
        if (first_child) {
            QString s = first_child->toString ();
            int p = s.indexOf (':');
            if (p > -1) {
                p = s.indexOf (':', p + 1);
                if (p > -1) {
                    int q = s.indexOf (' ', p + 1);
                    if (q > -1)
                        i = s.mid (p + 1, q - p - 1).toInt ();
                }
            }
        }
        sequence = eval_state->sequence;
    }
    return i;
}

int Last::toInt () const {
    if (eval_state->sequence != sequence) {
        sequence = eval_state->sequence;
        if (eval_state->parent) {
            Sequence *lst = eval_state->parent->process_list;
            if (lst)
                i = lst->length ();
        }
    }
    return i;
}

int Number::toInt () const {
    if (eval_state->sequence != sequence) {
        sequence = eval_state->sequence;
        if (first_child)
            i = first_child->toInt ();
    }
    return i;
}

int Position::toInt () const {
    if (eval_state->sequence != sequence) {
        sequence = eval_state->sequence;
        if (eval_state->parent) {
            Sequence *lst = eval_state->parent->process_list;
            Node *r = eval_state->root;
            if (lst) {
                i = 0;
                for (NodeValueItem *n = lst->first(); n; n = n->nextSibling()) {
                    i++;
                    if (r == n->data.node)
                        break;
                }
            }
        }
    }
    return i;
}

QString Concat::toString () const {
    if (eval_state->sequence != sequence) {
        sequence = eval_state->sequence;
        string.clear ();
        for (AST *child = first_child; child; child = child->next_sibling)
            string += child->toString ();
    }
    return string;
}

QString EscapeUri::toString () const {
    if (eval_state->sequence != sequence) {
        sequence = eval_state->sequence;
        string.clear ();
        if (first_child)
            string = QUrl::toPercentEncoding (first_child->toString ());
    }
    return string;
}

QString StringJoin::toString () const {
    if (eval_state->sequence != sequence) {
        sequence = eval_state->sequence;
        string.clear ();
        AST *child = first_child;
        if (child) {
            Sequence *lst = child->toSequence ();
            NodeValueItem *n = lst->first();
            if (n) {
                QString sep;
                if (child->next_sibling)
                    sep = child->next_sibling->toString ();
                string = n->data.value ();
                for (n = n->nextSibling (); n; n = n->nextSibling ())
                    string += sep + n->data.value ();
            }
            delete lst;
        }
    }
    return string;
}

QString SubstringAfter::toString () const {
    if (eval_state->sequence != sequence) {
        sequence = eval_state->sequence;
        string.clear ();
        AST *child = first_child;
        if (child) {
            AST *next = child->next_sibling;
            if (next) {
                QString s = child->toString ();
                QString t = next->toString ();
                int p = s.indexOf (t);
                if (p > -1)
                    string = s.mid (p + t.length ());
            }
        }
    }
    return string;
}

QString SubstringBefore::toString () const {
    if (eval_state->sequence != sequence) {
        sequence = eval_state->sequence;
        string.clear ();
        AST *child = first_child;
        if (child) {
            AST *next = child->next_sibling;
            if (next) {
                QString s = child->toString ();
                QString t = next->toString ();
                int p = s.indexOf (t);
                if (p > -1)
                    string = s.left (p);
            }
        }
    }
    return string;
}

QString CurrentTime::toString () const {
    if (eval_state->sequence != sequence) {
        char buf[200];
        time_t t = time(NULL);
        struct tm *lt = localtime(&t);
        if (lt && strftime (buf, sizeof (buf), "%H:%M:%S %z", lt))
            string = buf;
        sequence = eval_state->sequence;
    }
    return string;
}

QString CurrentDate::toString () const {
    if (eval_state->sequence != sequence) {
        char buf[200];
        time_t t = time(NULL);
        struct tm *lt = localtime(&t);
        if (lt && strftime (buf, sizeof (buf), "%a, %d %b %Y %z", lt))
            string = buf;
        sequence = eval_state->sequence;
    }
    return string;
}

static void sortList (Sequence *lst, Expression *expr) {
    NodeValueItem *cur = lst->first ();
    Sequence lt;
    Sequence gt;
    expr->setRoot (cur->data.node);
    QString str = expr->toString ();
    for (NodeValueItem *itm = cur->nextSibling (); itm; ) {
        NodeValueItem *next = itm->nextSibling ();
        expr->setRoot (itm->data.node);
        int cmp = str.compare (expr->toString ());
        if (cmp < 0) {
            NodeValueItemPtr s = itm;
            lst->remove (itm);
            gt.append (itm);
        } else if (cmp > 0) {
            NodeValueItemPtr s = itm;
            lst->remove (itm);
            lt.append (itm);
        }
        itm = next;
    }
    if (lt.first ()) {
        sortList (&lt, expr);
        lst->splice (lst->first (), lt);
    }
    if (gt.first ()) {
        sortList (&gt, expr);
        lst->splice (NULL, gt);
    }
}

Sequence *Sort::toSequence () const {
    if (first_child) {
        Expression *exp = evaluateExpr (first_child->toString ());
        if (exp) {
            exp->setRoot (eval_state->root);
            Sequence *lst = exp->toSequence ();
            if (lst->first () && first_child->next_sibling) {
                Expression *sort_exp =
                    evaluateExpr (first_child->next_sibling->toString ());
                if (sort_exp) {
                    sortList (lst, sort_exp);
                    delete sort_exp;
                }
            }
            delete exp;
            return lst;
        }
    }
    return AST::toSequence ();
}

#define BIN_OP_TO_INT(NAME,OP)                                           \
    AST *second_child = first_child->next_sibling;                       \
    AST::Type t1 = first_child->type ();                                 \
    AST::Type t2 = second_child->type ();                                \
    if (AST::TInteger == t1 && AST::TInteger == t2)                      \
        return first_child->toInt() OP second_child->toInt();            \
    if (AST::TInteger == t1 && AST::TFloat == t2)                        \
        return (int) (first_child->toInt() OP                            \
                second_child->toFloat());                                \
    if (AST::TFloat == t1 && AST::TInteger == t2)                        \
        return (int) (first_child->toFloat() OP                          \
                second_child->toInt());                                  \
    if (AST::TFloat == t1 && AST::TFloat == t2)                          \
        return (int) (first_child->toFloat() OP                          \
                second_child->toFloat());                                \
    return 0

int Multiply::toInt () const {
    BIN_OP_TO_INT(multiply, *);
}

float Multiply::toFloat () const {
    return first_child->toFloat () * first_child->next_sibling->toFloat ();
}

static AST::Type binaryASTType (const AST *ast) {
    AST::Type t1 = ast->first_child->type ();
    AST::Type t2 = ast->first_child->next_sibling->type ();
    if (t1 == t2 && (AST::TInteger == t1 || AST::TFloat == t1))
        return t1;
    if ((AST::TInteger == t1 && AST::TFloat == t2) ||
            (AST::TInteger == t2 && AST::TFloat == t1))
        return AST::TFloat;
    return AST::TUnknown;
}

AST::Type Multiply::type () const {
    return binaryASTType (this);
}

#ifdef KMPLAYER_EXPR_DEBUG
void Multiply::dump () const {
    fprintf (stderr, "* [");
    AST::dump();
    fprintf (stderr, " ]");
}
#endif

int Divide::toInt () const {
    BIN_OP_TO_INT(divide,/);
}

float Divide::toFloat () const {
    return first_child->toFloat () / first_child->next_sibling->toFloat ();
}

AST::Type Divide::type () const {
    return binaryASTType (this);
}

#ifdef KMPLAYER_EXPR_DEBUG
void Divide::dump () const {
    fprintf (stderr, "/ [");
    AST::dump();
    fprintf (stderr, " ]");
}
#endif

int Modulus::toInt () const {
    AST::Type t1 = first_child->type ();
    AST::Type t2 = first_child->next_sibling->type ();
    if (t1 == t2 && (TInteger == t1 || TFloat == t1))
        return first_child->toInt () % first_child->next_sibling->toInt ();
    return 0;
}

float Modulus::toFloat () const {
    return toInt ();
}

AST::Type Modulus::type () const {
    AST::Type t1 = first_child->type ();
    AST::Type t2 = first_child->next_sibling->type ();
    if (t1 == t2 && (TInteger == t1 || TFloat == t1))
        return TInteger;
    return TUnknown;
}

#ifdef KMPLAYER_EXPR_DEBUG
void Modulus::dump () const {
    fprintf (stderr, "%% [");
    AST::dump();
    fprintf (stderr, " ]");
}
#endif

int Plus::toInt ()  const{
    BIN_OP_TO_INT(plus,+);
}

float Plus::toFloat () const {
    return first_child->toFloat () + first_child->next_sibling->toFloat ();
}

AST::Type Plus::type () const {
    return binaryASTType (this);
}

#ifdef KMPLAYER_EXPR_DEBUG
void Plus::dump () const {
    fprintf (stderr, "+ [");
    AST::dump();
    fprintf (stderr, " ]");
}
#endif

int Minus::toInt () const {
    return first_child->toInt() - first_child->next_sibling->toInt();
}

float Minus::toFloat () const {
    return first_child->toFloat () - first_child->next_sibling->toFloat ();
}

AST::Type Minus::type () const {
    return binaryASTType (this);
}

#ifdef KMPLAYER_EXPR_DEBUG
void Minus::dump () const {
    fprintf (stderr, "- [");
    AST::dump();
    fprintf (stderr, " ]");
}
#endif

Sequence *Join::toSequence () const {
    if (first_child) {
        Sequence *lst = first_child->toSequence ();
        if (first_child->next_sibling) {
            Sequence *l2 = first_child->next_sibling->toSequence ();
            lst->splice (NULL, *l2);
            delete l2;
        }
        return lst;
    }
    return AST::toSequence ();
}

#ifdef KMPLAYER_EXPR_DEBUG
void Join::dump () const {
    fprintf (stderr, "| [");
    AST::dump();
    fprintf (stderr, " ]");
}
#endif

bool Comparison::toBool () const {
    AST::Type t1 = first_child->type ();
    AST::Type t2 = first_child->next_sibling->type ();
    switch (comp_type) {
    case lt:
        return first_child->toFloat () < first_child->next_sibling->toFloat ();
    case lteq:
        return first_child->toInt () <= first_child->next_sibling->toInt ();
    case gt:
        return first_child->toFloat () > first_child->next_sibling->toFloat ();
    case gteq:
        return first_child->toInt () >= first_child->next_sibling->toInt ();
    case eq:
        if (t1 == AST::TString || t2 == AST::TString)
            return first_child->toString () ==
                first_child->next_sibling->toString ();
        return first_child->toInt () == first_child->next_sibling->toInt ();
    case noteq:
        return first_child->toInt () != first_child->next_sibling->toInt ();
    case land:
        return first_child->toBool () && first_child->next_sibling->toBool ();
    case lor:
        return first_child->toBool () || first_child->next_sibling->toBool ();
    }
    return false;
}

#ifdef KMPLAYER_EXPR_DEBUG
void Comparison::dump () const {
    switch (comp_type) {
    case lt:
        fprintf (stderr, "< [");
        break;
    case lteq:
        fprintf (stderr, "<= [");
        break;
    case gt:
        fprintf (stderr, "> [");
        break;
    case gteq:
        fprintf (stderr, ">= [");
        break;
    case eq:
        fprintf (stderr, "== [");
        break;
    case noteq:
        fprintf (stderr, "!= [");
        break;
    case land:
        fprintf (stderr, "&& [");
        break;
    case lor:
        fprintf (stderr, "|| [");
    }
    AST::dump();
    fprintf (stderr, " ]");
}
#endif

static bool parseStatement (const char *str, const char **end, AST *ast);


static bool parseSpace (const char *str, const char **end) {
#ifdef KMPLAYER_EXPR_DEBUG
    fprintf (stderr, "%s enter str:'%s'\n", __FUNCTION__, str);
#endif
    *end = NULL;
    for (const char *s = str; *s; ++s) {
        switch (*s) {
            case ' ':
            case '\t':
            case '\n':
            case '\r':
                *end = s;
                break;
        }
        if (*end != s)
            break;
    }
    if (*end) {
        ++(*end);
        return true;
    }
    return false;
}

static bool parseLiteral (const char *str, const char **end, AST *ast) {
#ifdef KMPLAYER_EXPR_DEBUG
    fprintf (stderr, "%s enter str:'%s'\n", __FUNCTION__, str);
#endif
    if (parseSpace (str, end))
        str = *end;
    const char *s = str;
    if (*s == '\'' || *s == '"') {
        ++s;
        while (*s && *s != *str)
            ++s;
        if (*s) {
            appendASTChild (ast, new StringLiteral (ast->eval_state, ++str, s));
            *end = s + 1;
#ifdef KMPLAYER_EXPR_DEBUG
            fprintf (stderr, "%s success end:'%s'\n", __FUNCTION__, *end);
#endif
            return true;
        }
    } else {
        bool decimal = false;
        bool sign = false;
        *end = NULL;
        for (; *s; ++s) {
            if (*s == '.') {
                if (decimal)
                    return false;
                decimal = true;
            } else if (s == str && (*s == '+' || *s == '-')) {
                sign = true;
            } else if (!(*s >= '0' && *s <= '9')) {
                break;
            }
            *end = s;
        }
        if (*end && (!sign || *end > str) && *end - str < 64 &&
                (!decimal || *end > str)) {
            char buf[64];
            ++(*end);
            memcpy (buf, str, *end - str);
            buf[*end - str] = 0;
            appendASTChild (ast, decimal
               ? (AST *) new Float (ast->eval_state, strtof (buf, NULL))
               : (AST *) new Integer (ast->eval_state, strtol (buf, NULL, 10)));
#ifdef KMPLAYER_EXPR_DEBUG
            fprintf (stderr, "%s success end:'%s'\n", __FUNCTION__, *end);
#endif
            return true;
        }
    }
    return false;
}

static bool parseStep (const char *str, const char **end, AST *ast) {
#ifdef KMPLAYER_EXPR_DEBUG
    fprintf (stderr, "%s enter str:'%s'\n", __FUNCTION__, str);
#endif
    Step *entry = NULL;
    const char *s = str;
    if (*s == '.') {
        ++s;
        if (s && *s == '.') { // TODO
            entry = new Step (ast->eval_state, true);
            ++s;
        } else {
            entry = new Step (ast->eval_state, true);
        }
    } else if (*s == '/') {
        entry = new Step (ast->eval_state);
    } else {
        bool is_attr = *s == '@';
        if (is_attr)
            s = ++str;
        for (; *s; ++s)
            if (!((*s >= 'a' && *s <= 'z') ||
                        (*s >= 'A' && *s <= 'Z') ||
                        *s == '_' ||
                        *s == '*' ||
                        (s > str && (*s == '-' || (*s >= '0' && *s <= '9')))))
                break;
        if (str == s)
            return false;
        entry = new Step (ast->eval_state, str, s, is_attr);
        if (*s == '[') {
            AST pred (new EvalState (ast->eval_state));
            if (parseStatement (s + 1, end, &pred)) {
                str = *end;
                if (parseSpace (str, end))
                    str = *end;
                if (*str == ']') {
                    entry->first_child = pred.first_child;
                    pred.first_child = NULL;
                    s = ++str;
                }
            }
        }
    }
    if (entry) {
        appendASTChild (ast, entry);
        *end = s;
#ifdef KMPLAYER_EXPR_DEBUG
        fprintf (stderr, "%s success end:'%s'\n", __FUNCTION__, *end);
#endif
        return true;
    }
    return false;
}

static bool parseIdentifier (const char *str, const char **end, AST *ast) {
#ifdef KMPLAYER_EXPR_DEBUG
    fprintf (stderr, "%s enter str:'%s'\n", __FUNCTION__, str);
#endif
    Identifier ident (ast->eval_state, NULL);
    bool has_any = false;

    if (parseSpace (str, end))
        str = *end;
    if (!str)
        return false;
    bool start_contextual = *str != '/';
    if (*str == '/')
        ++str;
    else if (!ast->eval_state->parent &&
            !ast->eval_state->def_root_tag.isEmpty ())
        appendASTChild (&ident, new Step (ast->eval_state,
                  ast->eval_state->def_root_tag.toAscii ().constData (), NULL));
    if (parseStep (str, end, &ident)) {
        str = *end;
        has_any = true;
        ((Step *) ident.first_child)->start_contextual = start_contextual;
        if (*str == '/') {
            ++str;
            while (parseStep (str, end, &ident)) {
                str = *end;
                if (*str != '/')
                    break;
                ++str;
            }
        }
    }
    *end = str;
    if (has_any) {
        appendASTChild (ast, new Identifier (ast->eval_state, ident.first_child));
        ident.first_child = NULL;
#ifdef KMPLAYER_EXPR_DEBUG
        fprintf (stderr, "%s success end:'%s'\n", __FUNCTION__, *end);
#endif
        return true;
    }
    return false;
}

static bool parseGroup (const char *str, const char **end, AST *ast) {
#ifdef KMPLAYER_EXPR_DEBUG
    fprintf (stderr, "%s enter str:'%s'\n", __FUNCTION__, str);
#endif
    const char *begin = parseSpace (str, end) ? *end : str;
    if (!*begin || *begin != '(')
        return false;
    if (!parseStatement (begin + 1, end, ast))
        return false;
    str = *end;
    str = parseSpace (str, end) ? *end : str;
    if (!*str || *str != ')')
        return false;
    *end = str + 1;
#ifdef KMPLAYER_EXPR_DEBUG
    fprintf (stderr, "%s success end:'%s'\n", __FUNCTION__, *end);
#endif
    return true;
}

static bool parseFunction (const char *str, const char **end, AST *ast) {
#ifdef KMPLAYER_EXPR_DEBUG
    fprintf (stderr, "%s enter str:'%s'\n", __FUNCTION__, str);
#endif
    AST fast (ast->eval_state);
    const char *begin = parseSpace (str, end) ? *end : str;
    *end = NULL;
    for (const char *s = begin; *s; ++s) {
        if (!(*s >= 'a' && *s <= 'z') &&
                !(*s >= 'A' && *s <= 'Z') &&
                *s != '_' &&
                !(s > begin && (*s == '-' || (*s >= '0' && *s <= '9'))))
            break;
        *end = s;
    }
    if (*end) {
        ++(*end);
        QString name (QByteArray (begin, *end - begin));
        str = *end;
        if (parseSpace (str, end))
            str = *end;
        if (*str && *(str++) == '(') {
            if (parseSpace (str, end))
                str = *end;
            while (*str != ')' && parseStatement (str, end, &fast)) {
                str = *end;
                if (parseSpace (str, end))
                    str = *end;
                if (!*str || *str != ',')
                    break;
                str++;
            }
            if (parseSpace (str, end))
                str = *end;
            if (*str && *(str++) == ')') {
                AST *func = NULL;
                if (name == "concat")
                    func = new Concat (ast->eval_state);
                else if (name == "contains")
                    func = new Contains (ast->eval_state);
                else if (name == "count")
                    func = new Count (ast->eval_state);
                else if (name == "hours-from-time")
                    func = new HoursFromTime (ast->eval_state);
                else if (name == "minutes-from-time")
                    func = new MinutesFromTime (ast->eval_state);
                else if (name == "seconds-from-time")
                    func = new SecondsFromTime (ast->eval_state);
                else if (name == "current-time")
                    func = new CurrentTime (ast->eval_state);
                else if (name == "current-date")
                    func = new CurrentDate (ast->eval_state);
                else if (name == "last")
                    func = new Last (ast->eval_state);
                else if (name == "not")
                    func = new Not (ast->eval_state);
                else if (name == "number")
                    func = new Number (ast->eval_state);
                else if (name == "position")
                    func = new Position (ast->eval_state);
                else if (name == "sort")
                    func = new Sort (ast->eval_state);
                else if (name == "starts-with")
                    func = new StartsWith (ast->eval_state);
                else if (name == "string-join")
                    func = new StringJoin (ast->eval_state);
                else if (name == "substring-after")
                    func = new SubstringAfter (ast->eval_state);
                else if (name == "substring-before")
                    func = new SubstringBefore (ast->eval_state);
                else if (name == "escape-uri")
                    func = new EscapeUri (ast->eval_state);
                else
                    return false;
                appendASTChild (ast, func);
                func->first_child = fast.first_child;
                fast.first_child = NULL;
                *end = str;
#ifdef KMPLAYER_EXPR_DEBUG
                fprintf (stderr, "%s succes str:'%s'\n", __FUNCTION__, *end);
#endif
                return true;
            }
        }
    }
    return false;
}

static bool parseFactor (const char *str, const char **end, AST *ast) {
#ifdef KMPLAYER_EXPR_DEBUG
    fprintf (stderr, "%s enter str:'%s'\n", __FUNCTION__, str);
#endif
    return parseGroup (str, end, ast) ||
            parseFunction (str, end, ast) ||
            parseLiteral (str, end, ast) ||
            parseIdentifier (str, end, ast);
}

struct Keyword {
    const char *keyword;
    short length;
    short id;
};

static Keyword *parseKeyword (const char *str, const char **end, Keyword *lst) {
    for (int i = 0; lst[i].keyword; ++i)
        if (!strncmp (str, lst[i].keyword, lst[i].length)) {
            if (parseSpace (str + lst[i].length, end))
                return lst + i;
        }
    return 0;
}

static bool parseTerm (const char *str, const char **end, AST *ast) {
#ifdef KMPLAYER_EXPR_DEBUG
    fprintf (stderr, "%s enter str:'%s'\n", __FUNCTION__, str);
#endif
    if (parseFactor (str, end, ast)) {
        char op;
        str = *end;
        while (true) {
            str = parseSpace (str, end) ? *end : str;
            op = 0;
            if ('*' == *str) {
                op = '*';
            } else {
                Keyword keywords[] = {
                    { "div", 3, '/' }, { "mod", 3, '%' }, { NULL, 0, 0 }
                };
                Keyword *k = parseKeyword (str, end, keywords);
                if (k) {
                    op = (char) k->id;
                    str += k->length;
                }
            }
            if (op) {
                AST tmp (ast->eval_state);
                if (parseFactor (str + 1, end, &tmp)) {
                    AST *chlds = releaseLastASTChild (ast);
                    chlds->next_sibling = tmp.first_child;
                    tmp.first_child = NULL;
                    appendASTChild (ast,
                            op == '*'
                            ? (AST *) new Multiply (ast->eval_state, chlds)
                            : op == '/'
                            ? (AST *) new Divide (ast->eval_state, chlds)
                            : (AST *) new Modulus (ast->eval_state, chlds));
                    str = *end;
                }
            } else {
                *end = str;
                break;
            }
        }
#ifdef KMPLAYER_EXPR_DEBUG
        fprintf (stderr, "%s success end:'%s'\n", __FUNCTION__, *end);
#endif
        return true;
    }
    return false;
}

static bool parseExpression (const char *str, const char **end, AST *ast) {
#ifdef KMPLAYER_EXPR_DEBUG
    fprintf (stderr, "%s enter str:'%s'\n", __FUNCTION__, str);
#endif
    if (parseTerm (str, end, ast)) {
        char op;
        str = *end;
        while (true) {
            str = parseSpace (str, end) ? *end : str;
            op = *str;
            if (op == '+' || op == '-' || op == '|') {
                AST tmp (ast->eval_state);
                if (parseTerm (str + 1, end, &tmp)) {
                    AST *chlds = releaseLastASTChild (ast);
                    chlds->next_sibling = tmp.first_child;
                    tmp.first_child = NULL;
                    appendASTChild (ast, op == '+'
                            ? (AST *) new Plus (ast->eval_state, chlds)
                            :  op == '-'
                                ? (AST *) new Minus (ast->eval_state, chlds)
                                : (AST *) new Join (ast->eval_state, chlds));
                    str = *end;
                }
            } else {
                *end = str;
                break;
            }
        }
#ifdef KMPLAYER_EXPR_DEBUG
        fprintf (stderr, "%s success end:'%s'\n", __FUNCTION__, *end);
#endif
        return true;
    }
    return false;
}

static bool parseStatement (const char *str, const char **end, AST *ast) {
#ifdef KMPLAYER_EXPR_DEBUG
    fprintf (stderr, "%s enter str:'%s'\n", __FUNCTION__, str);
#endif
    if (parseExpression (str, end, ast)) {
        enum EComparison {
            err=-1, asign,
            lt=Comparison::lt, lteq, eq, noteq, gt, gteq, land, lor
        } comparison = err;
        str = *end;
        if (parseSpace (str, end))
            str = *end;
        switch (*str) {
        case '<':
            if (*(++str) && *str == '=') {
                comparison = lteq;
                ++str;
            } else {
                comparison = lt;
            }
            break;
        case '>':
            if (*(++str) && *str == '=') {
                comparison = gteq;
                ++str;
            } else {
                comparison = gt;
            }
            break;
        case '=':
            comparison = eq;
            ++str;
            break;
        case '!':
            if (*(++str) && *str == '=') {
                comparison = noteq;
                ++str;
            }
            break;
        default: {
            Keyword keywords[] = {
                { "and", 3, (short) land }, { "or", 2, (short) lor },
                { NULL, 0, 0 }
            };
            Keyword *k = parseKeyword (str, end, keywords);
            if (k) {
                comparison = (EComparison) k->id;
                str += k->length;
            }
        }
        }
        if (err != comparison) {
            AST tmp (ast->eval_state);
            if (parseExpression (str, end, &tmp)) {
                AST *chlds = releaseLastASTChild (ast);
                chlds->next_sibling = tmp.first_child;
                tmp.first_child = NULL;
                appendASTChild (ast, new Comparison (ast->eval_state,
                            (Comparison::CompType)comparison, chlds));
                str = *end;
            }
        }
        *end = str;

#ifdef KMPLAYER_EXPR_DEBUG
        fprintf (stderr, "%s success end:'%s'\n", __FUNCTION__, *end);
#endif
        return true;
    }
    return false;
}

Expression *KMPlayer::evaluateExpr (const QString &expr, const QString &root) {
    EvalState *eval_state = new EvalState (NULL, root);
    AST ast (eval_state);
    const char *end;
    if (parseStatement (expr.toUtf8 ().constData (), &end, &ast)) {
        AST *res = ast.first_child;
#ifdef KMPLAYER_EXPR_DEBUG
        ast.dump();
        fprintf (stderr, "\n");
#endif
        ast.first_child = NULL;

        return res;
    }
    return NULL;
}
/*
int main (int argc, char **argv) {
    AST ast;
    const char *end;
    if (argc < 2) {
        fprintf (stderr, "Usage %s <expr>\n", argv[0]);
        return 1;
    }
    printf ("expr '%s' parsed:%d\n", argv[1], parseStatement (argv[1], &end, &ast));
    ast.dump();
    if (ast.first_child)
        printf ("\ni:%d f:%.4f b:%d s:%s\n",
                ast.first_child->toInt(), ast.first_child->toFloat(), ast.first_child->toBool(), ast.first_child->toString());
    return 0;
}*/
