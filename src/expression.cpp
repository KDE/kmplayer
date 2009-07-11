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
#include "expression.h"

using namespace KMPlayer;

namespace {

struct EvalState {
    EvalState () : context (NULL), sequence (1), ref_count (0) {}

    void addRef () { ++ref_count; }
    void removeRef () { if (--ref_count == 0) delete this; }

    Node *context;
    int sequence;
    int ref_count;
};

struct AST : public Expression {
    enum Type {
        TUnknown, TInteger, TBool, TFloat, TString
    };

    AST (EvalState *ev);
    virtual ~AST ();

    virtual bool toBool (Node *state) const;
    virtual int toInt (Node *state) const;
    virtual float toFloat (Node *state) const;
    virtual QString toString (Node *state) const;
    virtual NodeRefList *toNodeList (Node *state) const;
    virtual Type type (Node *state) const;
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

    virtual QString toString (Node *state) const;
    virtual Type type (Node *state) const;

    mutable bool b;
};

struct IntegerBase : public AST {
    IntegerBase (EvalState *ev) : AST (ev), i (0) {}

    virtual bool toBool (Node *node) const;
    virtual float toFloat (Node *state) const;
    virtual Type type (Node *state) const;

    mutable int i;
};

struct Integer : public IntegerBase {
    Integer (EvalState *ev, int i_) : IntegerBase (ev) {
        i = i_;
    }

    virtual int toInt (Node *state) const;
#ifdef KMPLAYER_EXPR_DEBUG
    virtual void dump () const;
#endif
};

struct Float : public AST {
    Float (EvalState *ev, float f_) : AST (ev), f (f_) {}

    bool toBool (Node *) const { return false; }
    int toInt (Node *) const { return (int) f; }
    float toFloat (Node *) const { return f; }
    virtual Type type (Node *state) const;
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
     : AST (ev), string (e ? QString (QByteArray (s, e - s)) : QString (s)) {}

    virtual bool toBool (Node *state) const;
    virtual int toInt (Node *state) const;
    virtual float toFloat (Node *state) const;
    virtual Type type (Node *state) const;

    mutable QString string;
};

struct Step : public StringBase {
    Step (EvalState *ev) : StringBase (ev),any_path (true) {}
    Step (EvalState *ev, const char *s, const char *e)
     : StringBase (ev, s, e), any_path (false) {}

    bool matches (Node *n);
    bool anyPath () const { return any_path; }
#ifdef KMPLAYER_EXPR_DEBUG
    virtual void dump () const {
        fprintf (stderr, "Step %s", string.toAscii ().data ());
        AST::dump();
    }
#endif

    bool any_path;
};

struct Identifier : public StringBase {
    Identifier (EvalState *ev, AST *steps) : StringBase (ev) {
        first_child = steps;
    }

    virtual QString toString (Node *state) const;
    virtual NodeRefList *toNodeList (Node *state) const;
    virtual Type type (Node *state) const;
    bool childByPath (Node *node, Step *path, NodeRefList *lst) const;
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

    virtual QString toString (Node *state) const;
    virtual Type type (Node *state) const;
#ifdef KMPLAYER_EXPR_DEBUG
    virtual void dump () const {
        fprintf (stderr, "StringLiteral %s", string.toAscii ().data ());
        AST::dump();
    }
#endif
};

struct HoursFromTime : public IntegerBase {
    HoursFromTime (EvalState *ev) : IntegerBase (ev) {}

    virtual int toInt (Node *state) const;
};

struct MinutesFromTime : public IntegerBase {
    MinutesFromTime (EvalState *ev) : IntegerBase (ev) {}

    virtual int toInt (Node *state) const;
};

struct SecondsFromTime : public IntegerBase {
    SecondsFromTime (EvalState *ev) : IntegerBase (ev) {}

    virtual int toInt (Node *state) const;
};

struct CurrentTime : public StringBase {
    CurrentTime (EvalState *ev) : StringBase (ev) {}

    virtual QString toString (Node *state) const;
};

struct CurrentDate : public StringBase {
    CurrentDate (EvalState *ev) : StringBase (ev) {}

    virtual QString toString (Node *state) const;
};

struct Multiply : public AST {
    Multiply (EvalState *ev, AST *children) : AST (ev) {
        first_child = children;
    }

    virtual int toInt (Node *state) const;
    virtual float toFloat (Node *state) const;
    virtual Type type (Node *state) const;
#ifdef KMPLAYER_EXPR_DEBUG
    virtual void dump () const;
#endif
};

struct Divide : public AST {
    Divide (EvalState *ev, AST *children) : AST (ev) {
        first_child = children;
    }

    virtual int toInt (Node *state) const;
    virtual float toFloat (Node *state) const;
    virtual Type type (Node *state) const;
#ifdef KMPLAYER_EXPR_DEBUG
    virtual void dump () const;
#endif
};

struct Modulus : public AST {
    Modulus (EvalState *ev, AST *children) : AST (ev) {
        first_child = children;
    }

    virtual int toInt (Node *state) const;
    virtual float toFloat (Node *state) const;
    virtual Type type (Node *state) const;
#ifdef KMPLAYER_EXPR_DEBUG
    virtual void dump () const;
#endif
};

struct Plus : public AST {
    Plus (EvalState *ev, AST *children) : AST (ev) {
        first_child = children;
    }

    virtual int toInt (Node *state) const;
    virtual float toFloat (Node *state) const;
    virtual Type type (Node *state) const;
#ifdef KMPLAYER_EXPR_DEBUG
    virtual void dump () const;
#endif
};

struct Minus : public AST {
    Minus (EvalState *ev, AST *children) : AST (ev) {
        first_child = children;
    }

    virtual int toInt (Node *state) const;
    virtual float toFloat (Node *state) const;
    virtual Type type (Node *state) const;
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

    virtual bool toBool (Node *state) const;
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

bool AST::toBool (Node *state) const {
    return toInt (state);
}

int AST::toInt (Node *) const {
    return 0;
}

float AST::toFloat (Node *) const {
    return 0.0;
}

QString AST::toString (Node *state) const {
    switch (type (state)) {
    case TBool:
        return toBool (state) ? "true" : "false";
    case TInteger:
        return QString::number (toInt (state));
    case TFloat:
        return QString::number (toFloat (state));
    default:
        return QString ();
    }
}

NodeRefList *AST::toNodeList (Node *) const {
    return new NodeRefList;
}

AST::Type AST::type (Node *) const {
    return TUnknown;
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

QString BoolBase::toString (Node *state) const {
    return toBool (state) ? "true" : "false";
}

AST::Type BoolBase::type (Node *) const {
    return TBool;
}

float IntegerBase::toFloat (Node *state) const {
    return toInt (state);
}

bool IntegerBase::toBool (Node *n) const {
    if (eval_state->context == n->parentNode ()) {
        int ii = toInt (n) - 1;
        int count = 0;
        for (Node *c = eval_state->context->firstChild(); c; c=c->nextSibling())
            if (!strcmp (c->nodeName (), n->nodeName ()) && ii == count++)
                return n == c;
        return false;
    }
    return i;
}

AST::Type IntegerBase::type (Node *) const {
    return TInteger;
}

int Integer::toInt (Node *) const {
    return i;
}

#ifdef KMPLAYER_EXPR_DEBUG
void Integer::dump () const {
    fprintf (stderr, "Integer %d", i);
    AST::dump();
}
#endif

AST::Type Float::type (Node *) const {
    return TFloat;
}

bool StringBase::toBool (Node *state) const {
    QString s = toString (state);
    if (s.toLower () == "true")
        return true;
    if (s.toLower () == "false")
        return false;
    return s.toInt ();
}

int StringBase::toInt (Node *state) const {
    return toString (state).toInt ();
}

float StringBase::toFloat (Node *state) const {
    return toString (state).toFloat ();
}

AST::Type StringBase::type (Node *) const {
    return TString;
}

bool Step::matches (Node *n) {
    return string == n->nodeName () && (!first_child || first_child->toBool(n));
}

bool Identifier::childByPath (Node *node, Step *path, NodeRefList *lst) const {
    if (!node)
        return false;
    if (!path) {
        lst->append (new NodeRefItem (node));
        return true;
    }
    bool b = false;
    for (Node *c = node->firstChild (); c; c = b ? NULL : c->nextSibling ()) {
        if (path->anyPath ()) {
            b = childByPath (c, (Step *) path->next_sibling, lst);
            if (!b)
                b = childByPath (c, path, lst);
        } else {
            Node *ctx = eval_state->context;
            eval_state->context = node;
            if (path->matches (c))
                b = childByPath (c, (Step *) path->next_sibling, lst);
            eval_state->context = ctx;
        }
    }
    return b;
}

QString Identifier::toString (Node *state) const {
    if (eval_state->sequence != sequence) {
        NodeRefList *lst = toNodeList (state);
        if (lst && lst->first ())
            string = lst->first ()->data->nodeValue ();
        delete lst;
        sequence = eval_state->sequence;
    }
    return string;
}

NodeRefList *Identifier::toNodeList (Node *state) const {
    NodeRefList *lst = new NodeRefList;
    childByPath (state, (Step *) first_child, lst);
    return lst;
}

AST::Type Identifier::type (Node *state) const {
    QString s = toString (state);
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

QString StringLiteral::toString (Node *) const {
    return string;
}

AST::Type StringLiteral::type (Node *) const {
    return TString;
}

int HoursFromTime::toInt (Node *state) const {
    if (eval_state->sequence != sequence) {
        if (first_child) {
            QString s = first_child->toString (state);
            int p = s.indexOf (':');
            if (p > -1)
                i = s.left (p).toInt ();
        }
        sequence = eval_state->sequence;
    }
    return i;
}

int MinutesFromTime::toInt (Node *state) const {
    if (eval_state->sequence != sequence) {
        if (first_child) {
            QString s = first_child->toString (state);
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

int SecondsFromTime::toInt (Node *state) const {
    if (eval_state->sequence != sequence) {
        if (first_child) {
            QString s = first_child->toString (state);
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

QString CurrentTime::toString (Node *) const {
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

QString CurrentDate::toString (Node *) const {
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

#define BIN_OP_TO_INT(NAME,OP)                                           \
    AST *second_child = first_child->next_sibling;                       \
    AST::Type t1 = first_child->type (state);                            \
    AST::Type t2 = second_child->type (state);                           \
    if (AST::TInteger == t1 && AST::TInteger == t2)                      \
        return first_child->toInt(state) OP second_child->toInt(state);  \
    if (AST::TInteger == t1 && AST::TFloat == t2)                        \
        return (int) (first_child->toInt(state) OP                       \
                second_child->toFloat(state));                           \
    if (AST::TFloat == t1 && AST::TInteger == t2)                        \
        return (int) (first_child->toFloat(state) OP                     \
                second_child->toInt(state));                             \
    if (AST::TFloat == t1 && AST::TFloat == t2)                          \
        return (int) (first_child->toFloat(state) OP                     \
                second_child->toFloat(state));                           \
    return 0

int Multiply::toInt (Node *state) const {
    BIN_OP_TO_INT(multiply, *);
}

float Multiply::toFloat (Node *state) const {
    return first_child->toFloat (state) *
        first_child->next_sibling->toFloat (state);
}

static AST::Type binaryASTType (const AST *ast, Node *state) {
    AST::Type t1 = ast->first_child->type (state);
    AST::Type t2 = ast->first_child->next_sibling->type (state);
    if (t1 == t2 && (AST::TInteger == t1 || AST::TFloat == t1))
        return t1;
    if ((AST::TInteger == t1 && AST::TFloat == t2) ||
            (AST::TInteger == t2 && AST::TFloat == t1))
        return AST::TFloat;
    return AST::TUnknown;
}

AST::Type Multiply::type (Node *state) const {
    return binaryASTType (this, state);
}

#ifdef KMPLAYER_EXPR_DEBUG
void Multiply::dump () const {
    fprintf (stderr, "* [");
    AST::dump();
    fprintf (stderr, " ]");
}
#endif

int Divide::toInt (Node *state) const {
    BIN_OP_TO_INT(divide,/);
}

float Divide::toFloat (Node *state) const {
    return first_child->toFloat (state) /
        first_child->next_sibling->toFloat (state);
}

AST::Type Divide::type (Node *state) const {
    return binaryASTType (this, state);
}

#ifdef KMPLAYER_EXPR_DEBUG
void Divide::dump () const {
    fprintf (stderr, "/ [");
    AST::dump();
    fprintf (stderr, " ]");
}
#endif

int Modulus::toInt (Node *state) const {
    AST::Type t1 = first_child->type (state);
    AST::Type t2 = first_child->next_sibling->type (state);
    if (t1 == t2 && (TInteger == t1 || TFloat == t1))
        return first_child->toInt (state) %
            first_child->next_sibling->toInt (state);
    return 0;
}

float Modulus::toFloat (Node *state) const {
    return toInt (state);
}

AST::Type Modulus::type (Node *state) const {
    AST::Type t1 = first_child->type (state);
    AST::Type t2 = first_child->next_sibling->type (state);
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

int Plus::toInt (Node *state)  const{
    BIN_OP_TO_INT(plus,+);
}

float Plus::toFloat (Node *state) const {
    return first_child->toFloat (state) +
        first_child->next_sibling->toFloat (state);
}

AST::Type Plus::type (Node *state) const {
    return binaryASTType (this, state);
}

#ifdef KMPLAYER_EXPR_DEBUG
void Plus::dump () const {
    fprintf (stderr, "+ [");
    AST::dump();
    fprintf (stderr, " ]");
}
#endif

int Minus::toInt (Node *state) const {
    return first_child->toInt(state) - first_child->next_sibling->toInt(state);
}

float Minus::toFloat (Node *state) const {
    return first_child->toFloat (state) -
        first_child->next_sibling->toFloat (state);
}

AST::Type Minus::type (Node *state) const {
    return binaryASTType (this, state);
}

#ifdef KMPLAYER_EXPR_DEBUG
void Minus::dump () const {
    fprintf (stderr, "- [");
    AST::dump();
    fprintf (stderr, " ]");
}
#endif

bool Comparison::toBool (Node *state) const {
    AST::Type t1 = first_child->type (state);
    AST::Type t2 = first_child->next_sibling->type (state);
    switch (comp_type) {
    case lt:
        return first_child->toFloat (state) <
            first_child->next_sibling->toFloat (state);
    case lteq:
        return first_child->toInt (state) <=
            first_child->next_sibling->toInt (state);
    case gt:
        return first_child->toFloat (state) >
            first_child->next_sibling->toFloat (state);
    case gteq:
        return first_child->toInt (state) >=
            first_child->next_sibling->toInt (state);
    case eq:
        if (t1 == t2 && t1 == AST::TString)
            return first_child->toString (state) ==
                first_child->next_sibling->toString (state);
        return first_child->toInt (state) ==
            first_child->next_sibling->toInt (state);
    case noteq:
        return first_child->toInt (state) !=
            first_child->next_sibling->toInt (state);
    case land:
        return first_child->toBool (state) &&
            first_child->next_sibling->toBool (state);
    case lor:
        return first_child->toBool (state) ||
            first_child->next_sibling->toBool (state);
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
        if (*end && (!sign || *end > str) && *end - str < 64) {
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
    if (*s == '/') {
        entry = new Step (ast->eval_state);
    } else {
        for (; *s; ++s)
            if (!((*s >= 'a' && *s <= 'z') ||
                        (*s >= 'A' && *s <= 'Z') ||
                        *s == '_' ||
                        *s == '*' ||
                        (s > str && (*s == '-' || (*s >= '0' && *s <= '9')))))
                break;
        if (str == s)
            return false;
        entry = new Step (ast->eval_state, str, s);
        AST pred (ast->eval_state);
        if (*s == '[' && parseStatement (s + 1, end, &pred)) {
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
    if (entry) {
        appendASTChild (ast, entry);
        *end = s;
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
    if (*str == '/')
        ++str;
    else
        appendASTChild (&ident, new Step (ast->eval_state, "data", NULL));
    while (parseStep (str, end, &ident)) {
        str = *end;
        has_any = true;
        if (*str != '/')
            break;
        ++str;
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
            while (parseStatement (str, end, &fast)) {
                str = *end;
                if (parseSpace (str, end))
                    str = *end;
                if (!*str || *str != ',')
                    break;
            }
            if (parseSpace (str, end))
                str = *end;
            if (*str && *(str++) == ')') {
                AST *func = NULL;
                if (name == "hours-from-time")
                    func = new HoursFromTime (ast->eval_state);
                else if (name == "minutes-from-time")
                    func = new MinutesFromTime (ast->eval_state);
                else if (name == "seconds-from-time")
                    func = new SecondsFromTime (ast->eval_state);
                else if (name == "current-time")
                    func = new CurrentTime (ast->eval_state);
                else if (name == "current-date")
                    func = new CurrentDate (ast->eval_state);
                else
                    return false;
                appendASTChild (ast, func);
                func->first_child = fast.first_child;
                fast.first_child = NULL;
                *end = str;
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
                    AST *chlds = ast->first_child;
                    chlds->next_sibling = tmp.first_child;
                    tmp.first_child = NULL;
                    ast->first_child = NULL;
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
            if (op == '+' || op == '-') {
                AST tmp (ast->eval_state);
                if (parseTerm (str + 1, end, &tmp)) {
                    AST *chlds = ast->first_child;
                    chlds->next_sibling = tmp.first_child;
                    tmp.first_child = NULL;
                    ast->first_child = NULL;
                    appendASTChild (ast, op == '+'
                            ? (AST *) new Plus (ast->eval_state, chlds)
                            : (AST *) new Minus (ast->eval_state, chlds));
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
            if (*(++str) && *str == '=')
                comparison = lteq;
            else
                comparison = lt;
            break;
        case '>':
            if (*(++str) && *str == '=')
                comparison = gteq;
            else
                comparison = gt;
            break;
        case '=':
            comparison = eq;
            break;
        case '!':
            if (*(++str) && *str == '=')
                comparison = noteq;
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
            if (comparison == lteq || comparison == gteq || comparison == noteq)
                ++str;
            AST tmp (ast->eval_state);
            if (parseExpression (str, end, &tmp)) {
                AST *chlds = ast->first_child;
                chlds->next_sibling = tmp.first_child;
                tmp.first_child = NULL;
                ast->first_child = NULL;
                appendASTChild (ast, new Comparison (ast->eval_state,
                            (Comparison::CompType)comparison, chlds));
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

Expression *KMPlayer::evaluateExpr (const QString &expr) {
    EvalState *eval_state = new EvalState;
    AST ast (eval_state);
    const char *end;
    if (parseStatement (expr.toAscii ().data (), &end, &ast)) {
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
