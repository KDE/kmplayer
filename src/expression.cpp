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

struct AST : public ExpressionResult {
    enum Type {
        TUnknown, TInteger, TBool, TFloat, TString
    };

    AST () : first_child (NULL), next_sibling (NULL) {}
    virtual ~AST ();

    virtual bool toBool (Node *state) const;
    virtual int toInt (Node *state) const;
    virtual float toFloat (Node *state) const;
    virtual QString toString (Node *state) const;
    virtual Type type (Node *state) const;
#ifdef KMPLAYER_EXPR_DEBUG
    virtual void dump () const;
#endif

    AST *first_child;
    AST *next_sibling;
};

struct IntegerBase : public AST {
    virtual float toFloat (Node *state) const;
    virtual Type type (Node *state) const;
};

struct Integer : public IntegerBase {
    Integer (int i_) : i (i_) {}

    virtual int toInt (Node *state) const;
#ifdef KMPLAYER_EXPR_DEBUG
    virtual void dump () const;
#endif

    int i;
};

struct Float : public AST {
    Float (float f_) : f (f_) {}

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
    virtual bool toBool (Node *state) const;
    virtual int toInt (Node *state) const;
    virtual float toFloat (Node *state) const;
    virtual Type type (Node *state) const;
};

struct Identifier : public StringBase {
    Identifier (const char *s, const char *e)
       : str (e ? QString (QByteArray (s, e -s)) : QString (s)) {}

    virtual QString toString (Node *state) const;
    virtual Type type (Node *state) const;
#ifdef KMPLAYER_EXPR_DEBUG
    virtual void dump () const {
        fprintf (stderr, "Identifier %s", str.toAscii ().data ());
        AST::dump();
    }
#endif

    QString str;
};

struct StringLiteral : public StringBase {
    StringLiteral (const char *s, const char *e)
       : str (e ? QString (QByteArray (s, e -s)) : QString (s)) {}

    virtual QString toString (Node *state) const;
    virtual Type type (Node *state) const;
#ifdef KMPLAYER_EXPR_DEBUG
    virtual void dump () const {
        fprintf (stderr, "StringLiteral %s", str.toAscii ().data ());
        AST::dump();
    }
#endif

    QString str;
};

struct HoursFromTime : public IntegerBase {
    virtual int toInt (Node *state) const;
};

struct MinutesFromTime : public IntegerBase {
    virtual int toInt (Node *state) const;
};

struct SecondsFromTime : public IntegerBase {
    virtual int toInt (Node *state) const;
};

struct CurrentTime : public StringBase {
    virtual QString toString (Node *state) const;
};

struct CurrentDate : public StringBase {
    virtual QString toString (Node *state) const;
};

struct Multiply : public AST {
    Multiply (AST *children) {
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
    Divide (AST *children) {
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
    Modulus (AST *children) {
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
    Plus (AST *children) {
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
    Minus (AST *children) {
        first_child = children;
    }

    virtual int toInt (Node *state) const;
    virtual float toFloat (Node *state) const;
    virtual Type type (Node *state) const;
#ifdef KMPLAYER_EXPR_DEBUG
    virtual void dump () const;
#endif
};

struct Comparison : public AST {
    enum CompType {
        lt = 1, lteq, eq, noteq, gt, gteq, land, lor
    };

    Comparison (CompType ct, AST *children) : comp_type (ct) {
        first_child = children;
    }

    virtual bool toBool (Node *state) const;
    virtual Type type (Node *state) const;
#ifdef KMPLAYER_EXPR_DEBUG
    virtual void dump () const;
#endif

    CompType comp_type;
};

}

AST::~AST () {
    while (first_child) {
        AST *tmp = first_child;
        first_child = first_child->next_sibling;
        delete tmp;
    }
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

AST::Type AST::type (Node *) const {
    return TUnknown;
}

#ifdef KMPLAYER_EXPR_DEBUG
void AST::dump () const {
    for (AST *child = first_child; child; child = child->next_sibling) {
        if (child != first_child)
            fprintf (stderr, ", ");
        child->dump();
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

float IntegerBase::toFloat (Node *state) const {
    return toInt (state);
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

QString Identifier::toString (Node *state) const {
    if (state) {
        Node *n = state->childByName (str);
        if (n)
            return n->nodeValue ();
    }
    return QString ();
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
    return str;
}

AST::Type StringLiteral::type (Node *) const {
    return TString;
}

int HoursFromTime::toInt (Node *state) const {
    if (first_child) {
        QString s = first_child->toString (state);
        int p = s.indexOf (':');
        if (p > -1)
            return s.left (p).toInt ();
    }
    return 0;
}

int MinutesFromTime::toInt (Node *state) const {
    if (first_child) {
        QString s = first_child->toString (state);
        int p = s.indexOf (':');
        if (p > -1) {
            int q = s.indexOf (':', p + 1);
            if (q > -1)
                return s.mid (p + 1, q - p - 1).toInt ();
        }
    }
    return 0;
}

int SecondsFromTime::toInt (Node *state) const {
    if (first_child) {
        QString s = first_child->toString (state);
        int p = s.indexOf (':');
        if (p > -1) {
            p = s.indexOf (':', p + 1);
            if (p > -1) {
                int q = s.indexOf (' ', p + 1);
                if (q > -1)
                    return s.mid (p + 1, q - p - 1).toInt ();
            }
        }
    }
    return 0;
}

QString CurrentTime::toString (Node *) const {
    char str[200];
    time_t t = time(NULL);
    struct tm *lt = localtime(&t);
    if (lt && strftime (str, sizeof (str), "%H:%M:%S %z", lt))
        return str;
    return QString ();
}

QString CurrentDate::toString (Node *) const {
    char str[200];
    time_t t = time(NULL);
    struct tm *lt = localtime(&t);
    if (lt && strftime (str, sizeof (str), "%a, %d %b %Y %z", lt))
        return str;
    return QString ();
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

AST::Type Comparison::type (Node *) const {
    return TBool;
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

static bool parseExpression (const char *str, const char **end, AST *ast);


static bool parseSpace (const char *str, const char **end) {
#ifdef KMPLAYER_EXPR_DEBUG
    qDebug ("%s enter str:'%s'\n", __FUNCTION__, str);
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
    qDebug ("%s enter str:'%s'\n", __FUNCTION__, str);
#endif
    bool decimal = false;
    bool sign = false;
    const char *begin = parseSpace (str, end) ? *end : str;
    const char *s = begin;
    *end = NULL;
    if (*s == '\'' || *s == '"') {
        while (*s && *s != *begin)
            ++s;
        appendASTChild (ast, new StringLiteral (begin + 1, s));
        *end = s + 1;
        return true;
    }
    for (; *s; ++s) {
        if (*s == '.') {
            if (decimal)
                return false;
            decimal = true;
        } else if (s == begin && (*s == '+' || *s == '-')) {
            sign = true;
        } else if (!(*s >= '0' && *s <= '9')) {
            break;
        }
        *end = s;
    }
    if (*end && (!sign || *end > begin) && *end - begin < 64) {
        char buf[64];
        ++(*end);
        memcpy (buf, begin, *end - begin);
        buf[*end - begin] = 0;
        appendASTChild (ast, decimal
                ? (AST *) new Float (strtof (buf, NULL))
                : (AST *) new Integer (strtol (buf, NULL, 10)));
#ifdef KMPLAYER_EXPR_DEBUG
        qDebug ("%s success end:'%s'\n", __FUNCTION__, *end);
#endif
        return true;
    }
    return false;
}

static bool parseIdentifier (const char *str, const char **end, AST *ast) {
#ifdef KMPLAYER_EXPR_DEBUG
    qDebug ("%s enter str:'%s'\n", __FUNCTION__, str);
#endif
    const char *begin = parseSpace (str, end) ? *end : str;
    *end = NULL;
    for (const char *s = begin; *s; ++s) {
        if (!(*s >= 'a' && *s <= 'z') &&
                !(*s >= 'A' && *s <= 'Z') &&
                *s != '_' &&
                *s != '-' &&
                *s != '@' &&
                *s != '/' &&
                !(s > begin && (*s == '*' || (*s >= '0' && *s <= '9'))))
            break;
        *end = s;
    }
    if (*end) {
        ++(*end);
        appendASTChild (ast, new Identifier (begin, *end));
#ifdef KMPLAYER_EXPR_DEBUG
        qDebug ("%s success end:'%s'\n", __FUNCTION__, *end);
#endif
        return true;
    }
    return false;
}

static bool parseGroup (const char *str, const char **end, AST *ast) {
#ifdef KMPLAYER_EXPR_DEBUG
    qDebug ("%s enter str:'%s'\n", __FUNCTION__, str);
#endif
    const char *begin = parseSpace (str, end) ? *end : str;
    if (!*begin || *begin != '(')
        return false;
    if (!parseExpression (begin + 1, end, ast))
        return false;
    str = *end;
    str = parseSpace (str, end) ? *end : str;
    if (!*str || *str != ')')
        return false;
    *end = str + 1;
#ifdef KMPLAYER_EXPR_DEBUG
    qDebug ("%s success end:'%s'\n", __FUNCTION__, *end);
#endif
    return true;
}

static bool parseFunction (const char *str, const char **end, AST *ast) {
#ifdef KMPLAYER_EXPR_DEBUG
    qDebug ("%s enter str:'%s'\n", __FUNCTION__, str);
#endif
    AST fast;
    if (parseIdentifier (str, end, &fast)) {
        str = *end;
        if (parseSpace (str, end))
            str = *end;
        if (*str && *(str++) == '(') {
            while (parseExpression (str, end, fast.first_child)) {
                str = *end;
                if (parseSpace (str, end))
                    str = *end;
                if (!*str || !*str == ',')
                    break;
            }
            if (parseSpace (str, end))
                str = *end;
            if (*str && *(str++) == ')') {
                AST *func = NULL;
                if (((Identifier*)fast.first_child)->str == "hours-from-time")
                    func = new HoursFromTime ();
                else if (((Identifier*)fast.first_child)->str == "minutes-from-time")
                    func = new MinutesFromTime ();
                else if (((Identifier*)fast.first_child)->str == "seconds-from-time")
                    func = new SecondsFromTime ();
                else if (((Identifier*)fast.first_child)->str == "current-time")
                    func = new CurrentTime ();
                else if (((Identifier*)fast.first_child)->str == "current-date")
                    func = new CurrentDate ();
                else
                    return false;
                appendASTChild (ast, func);
                func->first_child = fast.first_child->first_child;
                fast.first_child->first_child = NULL;
                *end = str;
                return true;
            }
        }
    }
    return false;
}

static bool parseFactor (const char *str, const char **end, AST *ast) {
#ifdef KMPLAYER_EXPR_DEBUG
    qDebug ("%s enter str:'%s'\n", __FUNCTION__, str);
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
    qDebug ("%s enter str:'%s'\n", __FUNCTION__, str);
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
                AST tmp;
                if (parseFactor (str + 1, end, &tmp)) {
                    AST *chlds = ast->first_child;
                    chlds->next_sibling = tmp.first_child;
                    tmp.first_child = NULL;
                    ast->first_child = NULL;
                    appendASTChild (ast,
                            op == '*'
                            ? (AST *) new Multiply (chlds)
                            : op == '/'
                            ? (AST *) new Divide (chlds)
                            : (AST *) new Modulus (chlds));
                    str = *end;
                }
            } else {
                *end = str;
                break;
            }
        }
#ifdef KMPLAYER_EXPR_DEBUG
        qDebug ("%s success end:'%s'\n", __FUNCTION__, *end);
#endif
        return true;
    }
    return false;
}

static bool parseExpression (const char *str, const char **end, AST *ast) {
#ifdef KMPLAYER_EXPR_DEBUG
    qDebug ("%s enter str:'%s'\n", __FUNCTION__, str);
#endif
    if (parseTerm (str, end, ast)) {
        char op;
        str = *end;
        while (true) {
            str = parseSpace (str, end) ? *end : str;
            op = *str;
            if (op == '+' || op == '-') {
                AST tmp;
                if (parseTerm (str + 1, end, &tmp)) {
                    AST *chlds = ast->first_child;
                    chlds->next_sibling = tmp.first_child;
                    tmp.first_child = NULL;
                    ast->first_child = NULL;
                    appendASTChild (ast, op == '+'
                            ? (AST *) new Plus (chlds) : (AST *) new Minus (chlds));
                    str = *end;
                }
            } else {
                *end = str;
                break;
            }
        }
#ifdef KMPLAYER_EXPR_DEBUG
        qDebug ("%s success end:'%s'\n", __FUNCTION__, *end);
#endif
        return true;
    }
    return false;
}

static bool parseStatement (const char *str, const char **end, AST *ast) {
#ifdef KMPLAYER_EXPR_DEBUG
    qDebug ("%s enter str:'%s'\n", __FUNCTION__, str);
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
            AST tmp;
            if (parseExpression (str, end, &tmp)) {
                AST *chlds = ast->first_child;
                chlds->next_sibling = tmp.first_child;
                tmp.first_child = NULL;
                ast->first_child = NULL;
                appendASTChild (ast,
                      new Comparison ((Comparison::CompType)comparison, chlds));
            }
        }

#ifdef KMPLAYER_EXPR_DEBUG
        qDebug ("%s success end:'%s'\n", __FUNCTION__, *end);
#endif
        return true;
    }
    return false;
}

ExpressionResult *KMPlayer::evaluateExpr (const QString &expr) {
    AST ast;
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
