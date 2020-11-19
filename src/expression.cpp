/*
    This file belong to the KMPlayer project, a movie player plugin for Konqueror
    SPDX-FileCopyrightText: 2009 Koos Vriezen <koos.vriezen@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <qurl.h>
#include "expression.h"

#include <QRegExp>

using namespace KMPlayer;

QString NodeValue::value () const {
    if (attr)
        return attr->value ();
    if (node)
        return node->nodeValue ();
    return string;
}

namespace KMPlayer {

struct ExprIterator {
    ExprIterator(ExprIterator* p) : cur_value(nullptr, nullptr), parent(p), position(0)
    {}
    virtual ~ExprIterator() {
        delete parent;
    }
    bool atEnd() const { return !cur_value.node && cur_value.string.isNull(); }
    virtual void next();
    NodeValue& current() { return cur_value; }

    NodeValue cur_value;
    ExprIterator* parent;
    int position;
private:
    ExprIterator(const ExprIterator& p);
};

}

Expression::iterator::~iterator() {
    delete iter;
}

void ExprIterator::next() {
    assert(!atEnd());
    cur_value = NodeValue(nullptr, nullptr);
    ++position;
}

Expression::iterator& Expression::iterator::operator =(const Expression::iterator& it) {
    if (iter != it.iter) {
        delete iter;
        iter = it.iter;
        it.iter = nullptr;
    }
    return *this;
}

bool Expression::iterator::operator ==(const Expression::iterator& it) const {
    if (iter == it.iter)
        return true;
    if (iter && it.iter)
        return iter->cur_value == it.iter->cur_value;
    if (!iter)
        return it.iter->atEnd();
    if (!it.iter)
        return iter->atEnd();
    return false;
}

Expression::iterator& Expression::iterator::operator ++() {
    if (iter && !iter->atEnd())
        iter->next();
    return *this;
}

NodeValue& Expression::iterator::operator*() {
    static NodeValue empty(nullptr, nullptr);
    if (iter)
        return iter->current();
    return empty;
}

NodeValue* Expression::iterator::operator->() {
    if (iter)
        return &iter->cur_value;
    return nullptr;
}

namespace {

struct EvalState {
    EvalState (EvalState *p, const QString &root_tag=QString())
     : def_root_tag (root_tag), root (nullptr),
       iterator(nullptr), parent (p),
       sequence (1), ref_count (0) {}

    void addRef () { ++ref_count; }
    void removeRef () { if (--ref_count == 0) delete this; }

    QString def_root_tag;
    NodeValue root;
    ExprIterator* iterator;
    EvalState *parent;
    int sequence;
    int ref_count;
};

struct AST : public Expression {
    enum Type {
        TUnknown, TInteger, TBool, TFloat, TString, TSequence
    };

    AST (EvalState *ev);
    ~AST () override;

    bool toBool () const override;
    int toInt () const override;
    float toFloat () const override;
    QString toString () const override;
    virtual ExprIterator* exprIterator(ExprIterator* parent) const;
    iterator begin() const override;
    iterator end() const override;
    virtual Type type(bool calc) const;
    void setRoot (Node *root) override;
    void setRoot (const NodeValue &value);
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

    QString toString () const override;
    Type type(bool calc) const override;

    mutable bool b;
};

struct IntegerBase : public AST {
    IntegerBase (EvalState *ev) : AST (ev), i (0) {}

    float toFloat () const override;
    Type type(bool calc) const override;

    mutable int i;
};

struct Integer : public IntegerBase {
    Integer (EvalState *ev, int i_) : IntegerBase (ev) {
        i = i_;
    }

    int toInt () const override;
#ifdef KMPLAYER_EXPR_DEBUG
    virtual void dump () const;
#endif
};

struct Float : public AST {
    Float (EvalState *ev, float f_) : AST (ev), f (f_) {}

    bool toBool () const override { return false; }
    int toInt () const override { return (int) f; }
    float toFloat () const override { return f; }
    Type type(bool calc) const override;
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
    StringBase (EvalState *ev, const QString& s)
     : AST (ev), string(s) {}

    bool toBool () const override;
    int toInt () const override;
    float toFloat () const override;
    Type type(bool calc) const override;

    mutable QString string;
};

struct SequenceBase : public StringBase {
    SequenceBase (EvalState *ev) : StringBase (ev) {}
    SequenceBase (EvalState *ev, const QString& s)
        : StringBase (ev, s) {}

    bool toBool () const override;
    QString toString () const override;
    Type type(bool calc) const override;
};

struct Step : public SequenceBase {
    enum Axes {
        AncestorAxis=0x01, AttributeAxis=0x02, ChildAxis=0x04,
        DescendantAxis=0x08, FollowingAxis=0x10, FollowingSiblingAxis=0x20,
        NamespaceAxis=0x40, ParentAxis=0x80, PrecedingAxis=0x100,
        PrecedingSiblingAxis=0x200, SelfAxis=0x400
    };
    enum NodeType {
        AnyType, TextType, ElementType
    };
    Step (EvalState *ev, const QString &s, int ax, NodeType nt)
        : SequenceBase (ev, s)
        , axes(ax)
        , node_type(nt)
        , context_node(ax == SelfAxis && s.isEmpty())
    {}
    ExprIterator* exprIterator(ExprIterator* parent) const override;
    bool matches (Node *n) const;
    bool matches (Attribute *a) const;
#ifdef KMPLAYER_EXPR_DEBUG
    virtual void dump () const {
        fprintf (stderr, "Step %c%s",
                (axes & AttributeAxis) ? '@' : ' ',
                context_node ? "." : string.toAscii ().constData ());
        AST::dump();
    }
#endif

    int axes;
    NodeType node_type;
    bool context_node;
};

struct Path : public SequenceBase {
    Path (EvalState *ev, AST *steps, bool context)
        : SequenceBase (ev), start_contextual (context) {
        first_child = steps;
    }

    ExprIterator* exprIterator(ExprIterator* parent) const override;
#ifdef KMPLAYER_EXPR_DEBUG
    virtual void dump () const {
        fprintf (stderr, "Path ");
        AST::dump();
    }
#endif
    bool start_contextual;
};

struct PredicateFilter : public SequenceBase {
    PredicateFilter (EvalState *ev, AST *children) : SequenceBase (ev) {
        first_child = children;
    }

    ExprIterator* exprIterator(ExprIterator* parent) const override;
#ifdef KMPLAYER_EXPR_DEBUG
    virtual void dump () const {
        fprintf (stderr, "Predicate ");
        first_child->dump();
        fprintf (stderr, "[");
        for (AST *n = first_child->next_sibling; n; n = n->next_sibling)
            n->dump();
        fprintf (stderr, "]");
    }
#endif
};

struct StringLiteral : public StringBase {
    StringLiteral (EvalState *ev, const QString& s)
     : StringBase (ev, s) {}

    QString toString () const override;
    Type type(bool calc) const override;
#ifdef KMPLAYER_EXPR_DEBUG
    virtual void dump () const {
        fprintf (stderr, "StringLiteral %s", string.toAscii ().constData ());
        AST::dump();
    }
#endif
};

struct Boolean : public BoolBase {
    Boolean(EvalState *ev) : BoolBase(ev) {}

    bool toBool() const override;
};

struct Contains : public BoolBase {
    Contains (EvalState *ev) : BoolBase (ev) {}

    bool toBool () const override;
};

struct Not : public BoolBase {
    Not (EvalState *ev) : BoolBase (ev) {}

    bool toBool () const override;
};

struct StartsWith: public BoolBase {
    StartsWith (EvalState *ev) : BoolBase (ev) {}

    bool toBool () const override;
};

struct Count : public IntegerBase {
    Count (EvalState *ev) : IntegerBase (ev) {}

    int toInt () const override;
};

struct HoursFromTime : public IntegerBase {
    HoursFromTime (EvalState *ev) : IntegerBase (ev) {}

    int toInt () const override;
};

struct MinutesFromTime : public IntegerBase {
    MinutesFromTime (EvalState *ev) : IntegerBase (ev) {}

    int toInt () const override;
};

struct SecondsFromTime : public IntegerBase {
    SecondsFromTime (EvalState *ev) : IntegerBase (ev) {}

    int toInt () const override;
};

struct Last : public IntegerBase {
    Last (EvalState *ev) : IntegerBase (ev) {}

    int toInt () const override;
};

struct Number : public IntegerBase {
    Number (EvalState *ev) : IntegerBase (ev) {}

    int toInt () const override;
};

struct Position : public IntegerBase {
    Position (EvalState *ev) : IntegerBase (ev) {}

    int toInt () const override;
};

struct StringLength : public IntegerBase {
    StringLength (EvalState *ev) : IntegerBase (ev) {}

    int toInt () const override;
};

struct Concat : public StringBase {
    Concat (EvalState *ev) : StringBase (ev) {}

    QString toString () const override;
};

struct StringJoin : public StringBase {
    StringJoin (EvalState *ev) : StringBase (ev) {}

    QString toString () const override;
};

struct SubstringAfter : public StringBase {
    SubstringAfter (EvalState *ev) : StringBase (ev) {}

    QString toString () const override;
};

struct SubstringBefore : public StringBase {
    SubstringBefore (EvalState *ev) : StringBase (ev) {}

    QString toString () const override;
};

struct CurrentTime : public StringBase {
    CurrentTime (EvalState *ev) : StringBase (ev) {}

    QString toString () const override;
};

struct CurrentDate : public StringBase {
    CurrentDate (EvalState *ev) : StringBase (ev) {}

    QString toString () const override;
};

struct EscapeUri : public StringBase {
    EscapeUri (EvalState *ev) : StringBase (ev) {}

    QString toString () const override;
};

/*struct Sort : public SequenceBase {
    Sort (EvalState *ev) : SequenceBase (ev) {}

    virtual Sequence *toSequence () const;
};*/

struct SubSequence : public SequenceBase {
    SubSequence (EvalState *ev) : SequenceBase (ev) {}

    ExprIterator* exprIterator(ExprIterator* parent) const override;
};

struct Tokenize : public SequenceBase {
    Tokenize (EvalState *ev) : SequenceBase (ev) {}

    ExprIterator* exprIterator(ExprIterator* parent) const override;
};

struct Multiply : public AST {
    Multiply (EvalState *ev, AST *children) : AST (ev) {
        first_child = children;
    }

    int toInt () const override;
    float toFloat () const override;
    Type type(bool calc) const override;
#ifdef KMPLAYER_EXPR_DEBUG
    virtual void dump () const;
#endif
};

struct Divide : public AST {
    Divide (EvalState *ev, AST *children) : AST (ev) {
        first_child = children;
    }

    int toInt () const override;
    float toFloat () const override;
    Type type(bool calc) const override;
#ifdef KMPLAYER_EXPR_DEBUG
    virtual void dump () const;
#endif
};

struct Modulus : public AST {
    Modulus (EvalState *ev, AST *children) : AST (ev) {
        first_child = children;
    }

    int toInt () const override;
    float toFloat () const override;
    Type type(bool calc) const override;
#ifdef KMPLAYER_EXPR_DEBUG
    virtual void dump () const;
#endif
};

struct Plus : public AST {
    Plus (EvalState *ev, AST *children) : AST (ev) {
        first_child = children;
    }

    int toInt () const override;
    float toFloat () const override;
    Type type(bool calc) const override;
#ifdef KMPLAYER_EXPR_DEBUG
    virtual void dump () const;
#endif
};

struct Minus : public AST {
    Minus (EvalState *ev, AST *children) : AST (ev) {
        first_child = children;
    }

    int toInt () const override;
    float toFloat () const override;
    Type type(bool calc) const override;
#ifdef KMPLAYER_EXPR_DEBUG
    virtual void dump () const;
#endif
};

struct Join : public SequenceBase {
    Join (EvalState *ev, AST *children) : SequenceBase (ev) {
        first_child = children;
    }

    ExprIterator* exprIterator(ExprIterator* parent) const override;
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

    bool toBool () const override;
#ifdef KMPLAYER_EXPR_DEBUG
    virtual void dump () const;
#endif

    CompType comp_type;
};

}


AST::AST (EvalState *ev)
 : sequence (0), eval_state (ev), first_child (nullptr), next_sibling (nullptr) {
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
    switch (type(false)) {
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

ExprIterator* AST::exprIterator(ExprIterator* parent) const {
    // beware of recursion w/ toString
    struct ValueIterator : public ExprIterator {
        ValueIterator(ExprIterator* p, const QString& s) : ExprIterator(p) {
            cur_value = NodeValue(s);
        }
    };
    return new ValueIterator(parent, toString());
}

Expression::iterator AST::begin() const {
    return iterator(exprIterator(nullptr));
}

Expression::iterator AST::end() const {
    return iterator();
}

AST::Type AST::type(bool) const {
    return TUnknown;
}

void AST::setRoot (Node *root) {
    setRoot (NodeValue (root));
}

void AST::setRoot (const NodeValue& value) {
    eval_state->root = value;
    eval_state->sequence++;
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

static AST *releaseASTChildren (AST *p) {
    AST *child = p->first_child;
    p->first_child = nullptr;
    return child;
}

static AST *releaseLastASTChild (AST *p) {
    AST **chldptr = &p->first_child;
    while ((*chldptr)->next_sibling)
        chldptr = &(*chldptr)->next_sibling;
    AST *last = *chldptr;
    *chldptr = nullptr;
    return last;
}

QString BoolBase::toString () const {
    return toBool () ? "true" : "false";
}

AST::Type BoolBase::type(bool) const {
    return TBool;
}

float IntegerBase::toFloat () const {
    return toInt ();
}

AST::Type IntegerBase::type(bool) const {
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

AST::Type Float::type(bool) const {
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

AST::Type StringBase::type(bool) const {
    return TString;
}

bool Step::matches (Node *n) const {
    if (string.isEmpty()) {
        if (AnyType == node_type)
            return true;
        if (ElementType == node_type)
            return n->isElementNode();
        if (TextType == node_type)
            return !strcmp("#text", n->nodeName());
    }
    return string == n->nodeName ();
}

bool Step::matches (Attribute *a) const {
    return string.isEmpty() || string == a->name ();
}

bool SequenceBase::toBool () const {
    bool b = false;
    if (eval_state->iterator) {
        ExprIterator* it = exprIterator(nullptr);
        b = !it->atEnd();
        delete it;
    } else {
        b = StringBase::toBool ();
    }
    return b;
}

QString SequenceBase::toString () const {
    if (eval_state->sequence != sequence) {
        string.clear();
        ExprIterator* it = exprIterator(nullptr);
        if (!it->atEnd()) {
            string = it->cur_value.value();
            while (!it->atEnd()) {
                it->next();
            }
        }
        if (it->position != 1)
            string = QString::number(it->position);
        sequence = eval_state->sequence;
        delete it;
    }
    return string;
}

AST::Type SequenceBase::type(bool calc) const {
    if (calc) {
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
    return TSequence;
}

ExprIterator* Step::exprIterator(ExprIterator* parent) const {

    struct ChildrenIterator : public ExprIterator {
        ChildrenIterator(ExprIterator* p) : ExprIterator(p) {
            pullNext();
        }
        void pullNext() {
            for (; !parent->atEnd(); parent->next())
                if (parent->cur_value.node && parent->cur_value.node->firstChild()) {
                    cur_value = NodeValue(parent->cur_value.node->firstChild());
                    return;
                }
            cur_value = NodeValue(nullptr, nullptr);
        }
        void next() override {
            assert(cur_value.node);
            cur_value.node = cur_value.node->nextSibling();
            if (!cur_value.node) {
                parent->next();
                pullNext();
            }
            ++position;
        }
    };
    struct SiblingIterator : public ExprIterator {
        const bool forward;
        SiblingIterator(ExprIterator* p, bool fw) : ExprIterator(p), forward(fw) {
            cur_value = p->cur_value;
            pullNext();
        }
        void pullNext() {
            while (!parent->atEnd()) {
                if (forward && cur_value.node->nextSibling()) {
                    cur_value.node = cur_value.node->nextSibling();
                    return;
                } else if (!forward && cur_value.node->previousSibling()) {
                    cur_value.node = cur_value.node->previousSibling();
                    return;
                }
                parent->next();
                cur_value = parent->cur_value;
            }
            cur_value = NodeValue(nullptr, nullptr);
        }
        void next() override {
            assert(!atEnd());
            pullNext();
            ++position;
        }
    };
    struct DescendantIterator : public ChildrenIterator {
        DescendantIterator(ExprIterator* p) : ChildrenIterator(p)
        {}
        void next() override {
            assert(cur_value.node);
            if (cur_value.node->firstChild()) {
                cur_value.node = cur_value.node->firstChild();
                return;
            }
            if (cur_value.node->nextSibling()) {
                cur_value.node = cur_value.node->nextSibling();
                return;
            }
            for (Node* n = cur_value.node->parentNode(); n && n != parent->cur_value.node; n = n->parentNode())
                if (n->nextSibling()) {
                    cur_value.node = n->nextSibling();
                    return;
                }
            parent->next();
            pullNext();
            ++position;
        }
    };
    struct StepIterator : public ExprIterator {
        const Step* step;

        StepIterator(ExprIterator* p, const Step* s)
         : ExprIterator(p), step(s) {
            pullNext();
        }
        bool nextAttribute(Attribute *a) {
            for (; a; a = a->nextSibling ())
                if (step->matches(a)) {
                    cur_value.attr = a;
                    return true;
                }
            cur_value.attr = nullptr;
            return false;
        }
        void pullNext() {
            while (!parent->atEnd()) {
                Node* n = parent->cur_value.node;
                assert(n);
                if (!n)
                    continue; //FIXME
                if (step->axes & Step::AttributeAxis) {
                    if (n->isElementNode()) {
                        Element* e = static_cast<Element*>(n);
                        if (nextAttribute(e->attributes().first())) {
                            cur_value.node = n;
                            return;
                        }
                    }
                } else if (step->matches(n)) {
                    cur_value.node = n;
                    return;
                }
                parent->next();
            }
            cur_value.node = nullptr;
        }
        void next() override {
            assert(!atEnd());
            if ((step->axes & Step::AttributeAxis)
                    && cur_value.attr
                    && nextAttribute(cur_value.attr->nextSibling())) {
                ++position;
                return;
            }
            parent->next();
            pullNext();
            ++position;
        }
    };
    if (context_node)
        return parent;
    ExprIterator* it = parent;
    if (axes & DescendantAxis)
        it = new DescendantIterator(parent);
    else if (axes & FollowingSiblingAxis || axes & PrecedingSiblingAxis)
        it = new SiblingIterator(parent, axes & FollowingSiblingAxis);
    else if (!(axes & AttributeAxis))
        it = new ChildrenIterator(parent);
    return new StepIterator(it, this);
}

ExprIterator* Path::exprIterator(ExprIterator* parent) const {
    struct PathIterator : public ExprIterator {
        bool contextual;
        PathIterator(ExprIterator* parent, NodeValue& c)
         : ExprIterator(parent), contextual(false) {
            cur_value = c;
        }
        void next() override {
            assert(!atEnd());
            if (!contextual || parent->atEnd()) {
                cur_value = NodeValue(nullptr, nullptr);
            } else {
                parent->next();
                cur_value = parent->cur_value;
            }
            ++position;
        }
    };

    EvalState *es = eval_state;
    if (!start_contextual) {
        while (es->parent)
            es = es->parent;
    }
    ExprIterator* it = new PathIterator(parent, es->root);
    for (AST *s = first_child; s; s = s->next_sibling) {
        if (it->atEnd())
            return it;
        it = s->exprIterator(it);
    }
    return it;
}

ExprIterator* PredicateFilter::exprIterator(ExprIterator* parent) const {
    struct PredicateIterator : public ExprIterator {
        AST *ast;
        PredicateIterator(ExprIterator* parent, AST* a)
         : ExprIterator(parent), ast(a) {
            pullNext();
        }
        void pullNext() {
            while (!parent->atEnd()) {
                //while (ast) {
                    ast->setRoot(parent->cur_value);
                    ast->eval_state->iterator = parent;
                    cur_value = parent->cur_value;
                    bool res = ast->toBool();
                    ast->eval_state->iterator = nullptr;
                    if (res) {
                        return;
                    }
                    //ast = ast->next_sibling;
               // }
                if (parent->atEnd())
                    break;
                parent->next();
            }
            cur_value = NodeValue(nullptr, nullptr);
        }
        void next() override {
            assert(!atEnd());
            parent->next();
            pullNext();
            ++position;
        }
    };
    if (!first_child)
        return parent;
    ExprIterator *it = first_child->exprIterator(parent); //step
    if (!first_child->next_sibling)
        return it;
    return new PredicateIterator(it, const_cast<AST*>(first_child)->next_sibling);
}

QString StringLiteral::toString () const {
    return string;
}

AST::Type StringLiteral::type(bool) const {
    return TString;
}

bool Boolean::toBool() const {
    if (eval_state->sequence != sequence) {
        sequence = eval_state->sequence;
        b = false;
        if (first_child) {
            switch (first_child->type(false)) {
            case TInteger:
            case TFloat:
                b = first_child->toInt() != 0;
                break;
            case TString:
                b = !first_child->toString().isEmpty();
                break;
            default:
                b = first_child->toBool();
            }
        }
    }
    return b;
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
                b = eval_state->root.value ().startsWith (first_child->toString ());
        }
    }
    return b;
}

int Count::toInt () const {
    if (eval_state->sequence != sequence) {
        sequence = eval_state->sequence;
        i = 0;
        if (first_child) {
            ExprIterator* it = first_child->exprIterator(nullptr);
            while (!it->atEnd())
                it->next();
            i = it->position;
            delete it;
        } else if (eval_state->iterator) {
            while (!eval_state->iterator->atEnd())
                eval_state->iterator->next();
            i = eval_state->iterator->position;
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
        if (eval_state->iterator) {
            const NodeValue& v = eval_state->iterator->cur_value;
            if (v.node) {
                if (v.attr) {
                    if (v.node->isElementNode())
                        i = static_cast<Element *> (v.node)->attributes().length();
                } else if (v.node->parentNode()) {
                    i = 0;
                    for (Node* n = v.node->parentNode()->firstChild(); n; n = n->nextSibling())
                        ++i;
                }
            }
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
        if (eval_state->iterator)
            i = eval_state->iterator->position + 1;
    }
    return i;
}

int StringLength::toInt () const {
    if (eval_state->sequence != sequence) {
        sequence = eval_state->sequence;
        if (first_child)
            i = first_child->toString ().length ();
        else if (eval_state->parent)
            i = eval_state->root.value ().length ();
        else
            i = 0;
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
            ExprIterator* it = child->exprIterator(nullptr);
            if (!it->atEnd()) {
                QString sep;
                if (child->next_sibling)
                    sep = child->next_sibling->toString();
                string = it->cur_value.value ();
                it->next();
                for (; !it->atEnd(); it->next())
                    string += sep + it->cur_value.value();
            }
            delete it;
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
        time_t t = time(nullptr);
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
        time_t t = time(nullptr);
        struct tm *lt = localtime(&t);
        if (lt && strftime (buf, sizeof (buf), "%a, %d %b %Y %z", lt))
            string = buf;
        sequence = eval_state->sequence;
    }
    return string;
}

/*static void sortList (Sequence *lst, Expression *expr) {
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
        Expression* exp = evaluateExpr(first_child->toString().toUtf8());
        if (exp) {
            exp->setRoot (eval_state->root.node);
            Sequence *lst = exp->toSequence ();
            if (lst->first () && first_child->next_sibling) {
                Expression *sort_exp =
                    evaluateExpr(first_child->next_sibling->toString().toUtf8());
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
*/
ExprIterator* SubSequence::exprIterator(ExprIterator* parent) const
{
    struct SubSequenceIterator : public ExprIterator {
        int start;
        int length;
        SubSequenceIterator(ExprIterator* p, const AST* a)
            : ExprIterator(a ? a->exprIterator(p) : p), length(-1) {
            if (parent && a->next_sibling) {
                a = a->next_sibling;
                start = a->toInt();
                if (start < 1)
                    start = 1;
                if (a->next_sibling)
                    length = a->next_sibling->toInt();
                for (; !parent->atEnd(); parent->next()) {
                    if (parent->position + 1 == start)
                        break;
                }
                if (!parent->atEnd())
                    cur_value = parent->cur_value;
            }
        }
        void next() override {
            assert(!parent->atEnd());
            parent->next();
            if (length < 0 || parent->position + 1 < start + length)
                cur_value = parent->cur_value;
            else
                cur_value = NodeValue(nullptr, nullptr);
            ++position;
        }
    };
    return new SubSequenceIterator(parent, first_child);
}

ExprIterator* Tokenize::exprIterator(ExprIterator* parent) const
{
    struct TokenizeIterator : public ExprIterator {
        QString string;
        QRegExp reg_expr;
        int reg_pos;
        TokenizeIterator(ExprIterator* p, const AST* a) : ExprIterator(p), reg_pos(0) {
            if (a && a->next_sibling) {
                string = a->toString();
                reg_expr = QRegExp(a->next_sibling->toString());
                pullNext();
            }
        }
        void pullNext() {
            if (reg_pos >= 0) {
                reg_pos = reg_expr.indexIn(string, reg_pos);
                if (reg_pos >= 0) {
                    int len = reg_expr.matchedLength();
                    cur_value = NodeValue(string.mid (reg_pos, len));
                    reg_pos += len;
                }
            }
            if (reg_pos < 0)
                cur_value = NodeValue(nullptr, nullptr);
        }
        void next() override {
            assert(!atEnd());
            pullNext();
            ++position;
        }
    };
    return new TokenizeIterator(parent, first_child);
}

#define BIN_OP_TO_INT(NAME,OP)                                           \
    AST *second_child = first_child->next_sibling;                       \
    AST::Type t1 = first_child->type(true);                              \
    AST::Type t2 = second_child->type(true);                             \
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
    AST::Type t1 = ast->first_child->type(true);
    AST::Type t2 = ast->first_child->next_sibling->type(true);
    if (t1 == t2 && (AST::TInteger == t1 || AST::TFloat == t1))
        return t1;
    if ((AST::TInteger == t1 && AST::TFloat == t2) ||
            (AST::TInteger == t2 && AST::TFloat == t1))
        return AST::TFloat;
    return AST::TUnknown;
}

AST::Type Multiply::type(bool) const {
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

AST::Type Divide::type(bool) const {
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
    AST::Type t1 = first_child->type(true);
    AST::Type t2 = first_child->next_sibling->type(true);
    if (t1 == t2 && (TInteger == t1 || TFloat == t1))
        return first_child->toInt () % first_child->next_sibling->toInt ();
    return 0;
}

float Modulus::toFloat () const {
    return toInt ();
}

AST::Type Modulus::type(bool) const {
    AST::Type t1 = first_child->type(true);
    AST::Type t2 = first_child->next_sibling->type(true);
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

AST::Type Plus::type(bool) const {
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

AST::Type Minus::type(bool) const {
    return binaryASTType (this);
}

#ifdef KMPLAYER_EXPR_DEBUG
void Minus::dump () const {
    fprintf (stderr, "- [");
    AST::dump();
    fprintf (stderr, " ]");
}
#endif

ExprIterator* Join::exprIterator(ExprIterator* parent) const {
    struct JoinIterator : public ExprIterator {
        const AST *ast;
        ExprIterator* it;
        JoinIterator(ExprIterator* p, const AST* a) : ExprIterator(p), ast(a), it(nullptr) {
            pullNext();
        }
        ~JoinIterator() override {
            delete it;
        }
        void pullNext() {
            if (it && it->atEnd()) {
                delete it;
                it = nullptr;
            }
            while (!it && ast) {
                it = ast->exprIterator(nullptr);
                ast = ast->next_sibling;
                if (it->atEnd()) {
                    delete it;
                    it = nullptr;
                }
            }
            if (it)
                cur_value = it->cur_value;
            else
                cur_value = NodeValue(nullptr, nullptr);
        }
        void next() override {
            assert(!atEnd());
            it->next();
            pullNext();
            ++position;
        }
    };
    return new JoinIterator(parent, first_child);
}

#ifdef KMPLAYER_EXPR_DEBUG
void Join::dump () const {
    fprintf (stderr, "| [");
    AST::dump();
    fprintf (stderr, " ]");
}
#endif

bool Comparison::toBool () const {
    AST::Type t1 = first_child->type(true);
    AST::Type t2 = first_child->next_sibling->type(true);
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
        if (t1 == AST::TString || t2 == AST::TString) {
            return first_child->toString () ==
                first_child->next_sibling->toString ();
        }
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


struct Parser {
    enum { TEof=-1, TDouble=-2, TLong=-3, TIdentifier=-4, TWhiteSpace=-5 };

    const char *source;
    const char *cur;

    int cur_token;
    long long_value;
    double double_value;
    QString str_value;
    QString error;

    Parser(const char* s) : source(s), cur(source) {}
    void nextToken(bool skip_whitespace=true);
    void setError(const char* err) {
        fprintf(stderr, "Error at %d: %s\n", cur-source, err);
    }
};

void Parser::nextToken(bool skip_whitespace) {
    const char* begin = cur;
    bool is_num = false;
    bool is_fractal = false;
    while (true) {
        char c = *cur;
        switch (c) {
        case 0:
            if (begin == cur) {
                cur_token = TEof;
                return;
            }
            goto interp;
        case ' ':
        case '\t':
        case '\n':
        case '\r':
            if (begin == cur) {
                if (!skip_whitespace) {
                    cur_token = TWhiteSpace;
                    ++cur;
                    return;
                }
                ++begin;
                break;
            }
            goto interp;
        case '.':
            if (is_num && !is_fractal) {
                is_fractal = true;
                break;
            }
            // fall through
        default:
            if ((is_num || begin == cur) && c >= '0' && c <= '9') {
                is_num = true;
                break;
            }
            if (is_num && !(c >= '0' && c <= '9'))
                goto interp;
            if (!((c >= 'a' && c <= 'z')
                        || (c >= 'A' && c <= 'Z')
                        || (cur != begin && c >= '0' && c <= '9')
                        || c == '_'
                        || (cur != begin && c == '-')
                        || (unsigned char)c > 127)) {
                if (begin == cur) {
                    cur_token = c;
                    ++cur;
                    return;
                }
                goto interp;
            }
        }
        cur++;
    }
interp:
    if (is_num && cur - begin < 63) {
        char buf[64];
        memcpy(buf, begin, cur - begin);
        buf[cur - begin] = 0;
        if (is_fractal) {
            cur_token = TDouble;
            double_value = strtod(buf, nullptr);
        } else {
            cur_token = TLong;
            long_value = strtol(buf, nullptr, 10);
        }
    } else {
        cur_token = TIdentifier;
        str_value = QString::fromUtf8(QByteArray(begin, cur-begin));
    }
}

static bool parseStatement (Parser *parser, AST *ast);


static bool parsePredicates (Parser *parser, AST *ast) {
    AST pred (new EvalState (ast->eval_state));
    while (true) {
        if (parseStatement (parser, &pred)) {
            if (']' != parser->cur_token)
                return false;
            if (pred.first_child) {
                AST* child = releaseASTChildren(&pred);
                assert(!child->next_sibling);
                switch (child->type(false)) {
                    case AST::TBool:
                        break;
                    case AST::TInteger:
                    case AST::TFloat:
                        child->next_sibling = new Position(pred.eval_state);
                        child = new Comparison(pred.eval_state, Comparison::eq, child);
                        break;
                    default: {
                        AST* bfunc = new Boolean(pred.eval_state);
                        bfunc->first_child = child;
                        child = bfunc;
                        break;
                    }
                }
                appendASTChild(ast, child);
            }
        } else if (']' != parser->cur_token) {
            return false;
        }
        parser->nextToken();
        if ('[' != parser->cur_token)
            break;
        parser->nextToken();
    }
    return true;
}

static bool parseStep (Parser *parser, AST *ast) {
#ifdef KMPLAYER_EXPR_DEBUG
    fprintf (stderr, "%s enter str:'%s'\n", __FUNCTION__, parser->cur);
#endif
    AST *entry = nullptr;
    int axes = Step::ChildAxis;
    if ('/' == parser->cur_token) {
        axes = Step::DescendantAxis;
        parser->nextToken();
    }
    QString ns_identifier;
    QString identifier;
    Step::NodeType node_type = Step::ElementType;
    int prev_token = Parser::TEof;
    while (true ) {
        switch (parser->cur_token) {
        case '@':
            axes &= ~Step::ChildAxis;
            axes |= Step::AttributeAxis;
            break;
        case '.':
            if ( axes & Step::SelfAxis) {
                axes &= ~Step::SelfAxis;
                axes |= Step::ParentAxis;
            } else {
                axes &= ~Step::ChildAxis;
                axes |= Step::SelfAxis;
            }
            node_type = Step::AnyType;
            break;
        case '*':
            //identifier = "*";
            break;
        case ':':
            if (prev_token == ':') {
                axes &= ~(Step::ChildAxis | Step::NamespaceAxis);
                if (ns_identifier.startsWith("ancestor")) {
                    axes |= Step::AncestorAxis;
                    if (ns_identifier.endsWith("self"))
                        axes |= Step::SelfAxis;
                } else if (ns_identifier == "attribute") {
                    axes |= Step::AttributeAxis;
                } else if (ns_identifier == "child") {
                    axes |= Step::ChildAxis;
                } else if (ns_identifier.startsWith("descendant")) {
                    axes |= Step::DescendantAxis;
                    if (ns_identifier.endsWith("self"))
                        axes |= Step::SelfAxis;
                } else if (ns_identifier == "following") {
                    axes |= Step::FollowingAxis;
                } else if (ns_identifier == "following-sibling") {
                    axes |= Step::FollowingSiblingAxis;
                } else if (ns_identifier == "namespace") {
                    axes |= Step::NamespaceAxis;
                } else if (ns_identifier == "parent") {
                    axes |= Step::ParentAxis;
                } else if (ns_identifier == "preceding") {
                    axes |= Step::PrecedingAxis;
                } else if (ns_identifier == "preceding-sibling") {
                    axes |= Step::PrecedingSiblingAxis;
                } else if (ns_identifier == "self") {
                    axes |= Step::SelfAxis;
                }
                ns_identifier.clear();
            } else {
                axes |= Step::NamespaceAxis;
                ns_identifier = identifier;
                identifier.clear();
            }
            break;
        case Parser::TIdentifier:
            identifier = parser->str_value;
            break;
        default:
            goto location_done;
        }
        prev_token = parser->cur_token;
        parser->nextToken(false);
    }
location_done:
    if (Parser::TWhiteSpace == parser->cur_token)
        parser->nextToken();
    if (!ns_identifier.isEmpty() && !(axes & Step::SelfAxis) && identifier != "*")  // FIXME namespace support
        identifier = ns_identifier + ':' + identifier;
    if ('(' == parser->cur_token) {
        parser->nextToken();
        if (')' != parser->cur_token) {
            parser->setError("Expected )");
            return false;
        }
        parser->nextToken();
        if (identifier == "text") {
            node_type = Step::TextType;
        } else if (identifier == "node") {
            node_type = Step::AnyType;
        } else {
            parser->setError("Expected 'text' or 'node'");
            return false;
        }
        identifier.clear();
    }
    entry = new Step(ast->eval_state, identifier, axes, node_type);
    AST fast (ast->eval_state);
    if ('[' == parser->cur_token) {
        parser->nextToken();
        if (!parsePredicates (parser, &fast))
            return false;
        entry->next_sibling = releaseASTChildren (&fast);
        entry = new PredicateFilter (ast->eval_state, entry);
    }
    appendASTChild (ast, entry);
#ifdef KMPLAYER_EXPR_DEBUG
    fprintf (stderr, "%s success end:'%s'\n", __FUNCTION__, parser->cur);
#endif
    return true;
}

static bool parsePath (Parser *parser, AST *ast) {
#ifdef KMPLAYER_EXPR_DEBUG
    fprintf (stderr, "%s enter str:'%s'\n", __FUNCTION__, parser->cur);
#endif
    Path path (ast->eval_state, nullptr, false);
    bool has_any = false;

    bool start_contextual =  '/' != parser->cur_token;
    if ('/' == parser->cur_token) {
        parser->nextToken();
    } else if (!ast->eval_state->parent
            && !ast->eval_state->def_root_tag.isEmpty ()) {
        appendASTChild (&path, new Step (ast->eval_state,
                    ast->eval_state->def_root_tag, Step::ChildAxis, Step::ElementType));
    }
    if (parseStep (parser, &path)) {
        has_any = true;
        while ('/' == parser->cur_token) {
            parser->nextToken();
            if (!parseStep (parser, &path))
                break;
        }
    }
    if (has_any) {
        appendASTChild (ast, new Path (ast->eval_state, releaseASTChildren (&path), start_contextual));
#ifdef KMPLAYER_EXPR_DEBUG
        fprintf (stderr, "%s success end:'%s'\n", __FUNCTION__, parser->cur);
#endif
        return true;
    }
    return false;
}

static bool parseFunction (Parser *parser, const QString &name, AST *ast) {
#ifdef KMPLAYER_EXPR_DEBUG
    fprintf (stderr, "%s enter str:'%s'\n", __FUNCTION__, parser->cur);
#endif
    AST fast (ast->eval_state);
    while (Parser::TEof != parser->cur_token) {
        switch (parser->cur_token) {
        case ')': {
            parser->nextToken();
            AST *func = nullptr;
            if (name == "boolean")
                func = new Boolean(ast->eval_state);
            else if (name == "concat")
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
            //else if (name == "sort")
                //func = new Sort (ast->eval_state);
            else if (name == "starts-with")
                func = new StartsWith (ast->eval_state);
            else if (name == "string-join")
                func = new StringJoin (ast->eval_state);
            else if (name == "string-length")
                func = new StringLength (ast->eval_state);
            else if (name == "subsequence")
                func = new SubSequence (ast->eval_state);
            else if (name == "substring-after")
                func = new SubstringAfter (ast->eval_state);
            else if (name == "substring-before")
                func = new SubstringBefore (ast->eval_state);
            else if (name == "tokenize")
                func = new Tokenize (ast->eval_state);
            else if (name == "escape-uri")
                func = new EscapeUri (ast->eval_state);
            else
                return false;
            appendASTChild (ast, func);
            func->first_child = releaseASTChildren (&fast);
#ifdef KMPLAYER_EXPR_DEBUG
            fprintf (stderr, "%s succes str:'%s'\n", __FUNCTION__, parser->cur);
#endif
            return true;
        }
        case ',':
            parser->nextToken();
            break;
        default:
            if (!parseStatement (parser, &fast))
                return false;
        }
    }
    return false;
}

static bool parseFactor (Parser *parser, AST *ast) {
#ifdef KMPLAYER_EXPR_DEBUG
    fprintf (stderr, "%s enter str:'%s'\n", __FUNCTION__, parser->cur);
#endif
    AST fast (ast->eval_state);
    int sign = 1;
    if ('+' == parser->cur_token || '-' == parser->cur_token) {
        sign = '-' == parser->cur_token ? -1 : 1;
        parser->nextToken();
    }
    switch (parser->cur_token) {
    case Parser::TEof:
        return true;
    case '(':
        parser->nextToken();
        if (!parseStatement (parser, &fast))
            return false;
        if (')' != parser->cur_token)
            return false;
        parser->nextToken();
        break;
    case '.':
    case '@':
    case '*':
    case '/':
        if (!parsePath (parser, &fast))
            return false;
        break;
    case '\'':
    case '"': {
        const char* s = parser->cur;
        while (*s && *s != parser->cur_token)
            ++s;
        if (!*s) {
            parser->setError("expected string literal");
            parser->cur = s;
            parser->cur_token = Parser::TEof;
            return false;
        }
        appendASTChild(&fast, new StringLiteral(ast->eval_state, QString::fromUtf8(QByteArray(parser->cur, s - parser->cur))));
        parser->cur = ++s;
        parser->nextToken();
#ifdef KMPLAYER_EXPR_DEBUG
        fprintf (stderr, "%s success end:'%s' cur token %c\n", __FUNCTION__, parser->cur, parser->cur_token);
#endif
        break;
    }
    case Parser::TDouble:
        appendASTChild (&fast, new Float (ast->eval_state, (float)(sign * parser->double_value)));
        parser->nextToken();
        break;
    case Parser::TLong:
        appendASTChild (&fast,  new Integer (ast->eval_state, (int)(sign * parser->long_value)));
        parser->nextToken();
        break;
    case Parser::TIdentifier: {
        QString str = parser->str_value;
        const char* cur = parser->cur;
        parser->nextToken();
        if ('(' == parser->cur_token) {
            parser->nextToken();
            if (!parseFunction (parser, str, &fast))
                return false;
        } else {
            parser->cur = cur;
            parser->cur_token = Parser::TIdentifier;
            parser->str_value = str;
            if (!parsePath (parser, &fast))
                return false;
        }
        break;
    }
    default:
        return false;
    }
    if ('[' == parser->cur_token) {
        parser->nextToken();
        if (!parsePredicates (parser, &fast))
            return false;
        appendASTChild (ast,
                new PredicateFilter (ast->eval_state, releaseASTChildren (&fast)));
    } else {
        appendASTChild (ast, releaseASTChildren (&fast));
    }
    return true;
}

static bool parseTerm (Parser *parser, AST *ast) {
#ifdef KMPLAYER_EXPR_DEBUG
    fprintf (stderr, "%s enter str:'%s'\n", __FUNCTION__, parser->cur);
#endif
    if (parseFactor (parser, ast)) {
        while (true) {
            int op = 0;
            if ('*' == parser->cur_token) {
                op = '*';
            } else if (Parser::TIdentifier == parser->cur_token) {
                if ( parser->str_value == "div")
                    op = '/';
                else if ( parser->str_value == "mod")
                    op = '%';
            }
            if (!op)
                break;
            parser->nextToken();
            AST tmp (ast->eval_state);
            if (parseFactor (parser, &tmp)) {
                AST *chlds = releaseLastASTChild (ast);
                chlds->next_sibling = releaseASTChildren (&tmp);
                appendASTChild (ast,
                        op == '*'
                        ? (AST *) new Multiply (ast->eval_state, chlds)
                        : op == '/'
                        ? (AST *) new Divide (ast->eval_state, chlds)
                        : (AST *) new Modulus (ast->eval_state, chlds));
            } else {
                parser->setError("expected factor");
                return false;
            }
        }
#ifdef KMPLAYER_EXPR_DEBUG
        fprintf (stderr, "%s success end:'%s'\n", __FUNCTION__, parser->cur);
#endif
        return true;
    }
    return false;
}

static bool parseExpression (Parser *parser, AST *ast) {
#ifdef KMPLAYER_EXPR_DEBUG
    fprintf (stderr, "%s enter str:'%s'\n", __FUNCTION__, parser->cur);
#endif
    if (parseTerm (parser, ast)) {
        while (true) {
            int op = parser->cur_token;
            if (op != '+' && op != '-' && op != '|')
                break;
            parser->nextToken();
            AST tmp (ast->eval_state);
            if (parseTerm (parser, &tmp)) {
                AST *chlds = releaseLastASTChild (ast);
                chlds->next_sibling = releaseASTChildren (&tmp);
                appendASTChild (ast, op == '+'
                        ? (AST *) new Plus (ast->eval_state, chlds)
                        :  op == '-'
                        ? (AST *) new Minus (ast->eval_state, chlds)
                        : (AST *) new Join (ast->eval_state, chlds));
            } else {
                parser->setError("expected term");
                return false;
            }
        }
#ifdef KMPLAYER_EXPR_DEBUG
        fprintf (stderr, "%s success end:'%s'\n", __FUNCTION__, parser->cur);
#endif
        return true;
    }
    return false;
}

static bool parseStatement (Parser *parser, AST *ast) {
#ifdef KMPLAYER_EXPR_DEBUG
    fprintf (stderr, "%s enter str:'%s'\n", __FUNCTION__, parser->cur);
#endif
    if (parseExpression (parser, ast)) {
        bool skip_next_token = false;
        enum EComparison {
            err=-1, asign,
            lt=Comparison::lt, lteq, eq, noteq, gt, gteq, land, lor
        } comparison = err;
        switch (parser->cur_token) {
        case '<':
            parser->nextToken();
            if (parser->cur_token == '=') {
                comparison = lteq;
            } else {
                skip_next_token = true;
                comparison = lt;
            }
            break;
        case '>':
            parser->nextToken();
            if ('=' == parser->cur_token) {
                comparison = gteq;
            } else {
                skip_next_token = true;
                comparison = gt;
            }
            break;
        case '=':
            comparison = eq;
            break;
        case '!':
            parser->nextToken();
            if ('=' == parser->cur_token) {
                parser->setError("expected =");
                return false;
            }
            comparison = noteq;
            break;
        case Parser::TIdentifier:
            if (parser->str_value == "and")
                comparison = land;
            else if (parser->str_value == "or")
                comparison = lor;
            break;
        default:
            return true;
        }
        AST tmp (ast->eval_state);
        if (!skip_next_token)
            parser->nextToken();
        if (parseExpression (parser, &tmp)) {
            AST *chlds = releaseLastASTChild (ast);
            chlds->next_sibling = releaseASTChildren (&tmp);
            appendASTChild (ast, new Comparison (ast->eval_state,
                        (Comparison::CompType)comparison, chlds));
        } else {
            parser->setError("expected epression");
            return false;
        }

#ifdef KMPLAYER_EXPR_DEBUG
        fprintf (stderr, "%s success end:'%s'\n", __FUNCTION__, parser->cur);
#endif
        return true;
    }
    return false;
}

Expression* KMPlayer::evaluateExpr(const QByteArray& expr, const QString &root) {
    EvalState *eval_state = new EvalState (nullptr, root);
    AST ast (eval_state);
    Parser parser(expr.constData());
    parser.nextToken ();
    if (parseStatement (&parser, &ast)) {
#ifdef KMPLAYER_EXPR_DEBUG
        ast.dump();
        fprintf (stderr, "\n");
#endif
        return releaseASTChildren (&ast);
    }
    return nullptr;
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
