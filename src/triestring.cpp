/**
  This file belong to the KMPlayer project, a movie player plugin for Konqueror
  Copyright (C) 2007  Koos Vriezen <koos.vriezen@gmail.com>

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

#ifdef TEST_TRIE
# define KMPLAYERCOMMON_EXPORT
#else
# include <config-kmplayer.h>
#endif
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <vector>

#include "kmplayercommon_log.h"
#include "triestring.h"

namespace KMPlayer {

struct TrieNode
{
    enum { MaxPacked = sizeof (void*) };

    TrieNode() : ref_count(0), length(0), parent(nullptr), buffer(nullptr)
    {}
    TrieNode(TrieNode* p, const char* s, size_t len)
        : ref_count(0), length(0)
    {
        update(p, s, len);
    }
    ~TrieNode()
    {
        if (length > MaxPacked)
            free(buffer);
    }
    void update(TrieNode* p, const char* s, size_t len)
    {
        char* old = length > MaxPacked ? buffer : nullptr;
        parent = p;
        length = len;
        if (len <= MaxPacked) {
            if ((unsigned)::abs(s - packed) > len)
                memcpy(packed, s, len);
            else
                memmove(packed, s, len);
        } else {
            buffer = (char*)malloc(len);
            memcpy(buffer, s, len);
        }
        if (old)
            free(old);
    }

    int ref_count;
    unsigned length;
    TrieNode* parent;
    std::vector<TrieNode*> children;

    union {
        char packed[MaxPacked];
        char* buffer;
    };
};

}

using namespace KMPlayer;

static char* trieCharPtr(TrieNode* n)
{
    return n->length <= TrieNode::MaxPacked ? n->packed : n->buffer;
}

static char* trieRetrieveString (TrieNode* node, int& len)
{
    char *buf;
    if (node->parent) {
        len += node->length;
        int p = len;
        buf = trieRetrieveString(node->parent, len);
        memcpy(buf+len-p, trieCharPtr(node), node->length);
    } else {
        buf = (char*)malloc(len + 1);
        buf[len] = 0;
    }
    return buf;
}

static TrieNode* trieRoot()
{
    static TrieNode* trie_root;
    if (!trie_root)
        trie_root = new TrieNode();
    return trie_root;
}

static void dump(TrieNode* n, int indent)
{
    for (int i =0; i < indent; ++i)
        fprintf(stderr, " ");
    fprintf(stderr, "'");
    for (unsigned i = 0; i < n->length; ++i)
        fprintf(stderr, "%c", trieCharPtr(n)[i]);
    fprintf(stderr, "'\n");
    for (unsigned i = 0; i < n->children.size(); ++i)
        dump(n->children[i], indent+2);
}

static int trieSameGenerationCompare(TrieNode* n1, TrieNode* n2)
{
    if (n1->parent == n2->parent)
        return memcmp(trieCharPtr(n1), trieCharPtr(n2), std::min(n1->length, n2->length));
    return trieSameGenerationCompare(n1->parent, n2->parent);
}

static int trieCompare(TrieNode* n1, TrieNode* n2)
{
    if (n1 == n2)
        return 0;
    int depth1 = 0, depth2 = 0;
    for (TrieNode* n = n1; n; n = n->parent)
        depth1++;
    if (!depth1)
        return n2 ? -1 : 0;
    for (TrieNode* n = n2; n; n = n->parent)
        depth2++;
    if (!depth2)
        return 1;
    if (depth1 != depth2) {
        int dcmp = depth1 > depth2 ? 1 : -1;
        for (; depth1 > depth2; --depth1)
            n1 = n1->parent;
        for (; depth2 > depth1; --depth2)
            n2 = n2->parent;
        if (n1 == n2)
            return dcmp;
    }
    return trieSameGenerationCompare(n1, n2);
}

static int trieStringCompare(TrieNode* node, const char* s, int& pos, int len)
{
    int cmp = 0;
    if (node->parent)
        cmp = trieStringCompare (node->parent, s, pos, len);
    if (!cmp) {
        if (pos > len)
            return 1;
        if (pos == len)
            return node->length ? 1 : 0;
        if (len - pos < node->length) {
            cmp = memcmp(trieCharPtr(node), s + pos, len - pos);
            if (!cmp)
                cmp = 1;
        } else {
            cmp = memcmp(trieCharPtr(node), s + pos, node->length);
        }
        pos += node->length;
    }
    return cmp;
}

static int trieStringCompare(TrieNode* node, const char* s)
{
    if (!node)
        return !!s;
    if (!s)
        return 1;
    int pos = 0;
    int len = strlen(s);
    int cmp = trieStringCompare(node, s, pos, len);
#ifdef TEST_TRIE
    fprintf(stderr, "== %s -> (%d %d) %d\n", s, pos, len, cmp);
#endif
    if (cmp)
        return cmp;
    if (pos == len)
        return 0;
    return pos < len ? -1 : 1;
}

//first index in range [first,last) which does not compare less than c
static int trieLowerBound(const TrieNode* n, int begin, int end, char c)
{
    if (begin == end)
        return end;
    if (begin == end - 1)
        return trieCharPtr(n->children[begin])[0] >= c ? begin : end;
    int i = (begin + end)/2;
    char c1 = trieCharPtr(n->children[i])[0];
    if (c == c1)
        return i;
    if (c < c1)
        return trieLowerBound(n, begin, i, c);
    return trieLowerBound(n, i + 1, end, c);
}

static TrieNode* trieInsert(TrieNode* parent, const char* s, size_t len)
{
    TrieNode* node;

    if (!*s)
        return parent;

    unsigned idx = trieLowerBound(parent, 0, parent->children.size(), s[0]);
    if (idx < parent->children.size()) {
        node = parent->children[idx];
        char* s2 = trieCharPtr(node);
        if (s[0] == s2[0]) {
            if (node->length == len
                    && !memcmp((void*)s, trieCharPtr(node), len)) {
                return node;
            }
            for (unsigned i = 1; i < node->length; ++i) {
                if (i == len) {
                    TrieNode* rep = new TrieNode(parent, s, i);
                    parent->children[idx] = rep;
                    node->update(rep, s2 + i, node->length - i);
                    rep->children.push_back(node);
                    return rep;
                } else if (s[i] != s2[i]) {
                    TrieNode* rep = new TrieNode(parent, s2, i);
                    TrieNode* child = new TrieNode(rep, s + i, len - i);
                    bool cmp = s2[i] < s[i];
                    node->update(rep, s2 + i, node->length - i);
                    if (cmp) {
                        rep->children.push_back(node);
                        rep->children.push_back(child);
                    } else {
                        rep->children.push_back(child);
                        rep->children.push_back(node);
                    }
                    parent->children[idx] = rep;
                    return child;
                }
            }
            return trieInsert(node, s + node->length, len - node->length);
        } else if (s[0] < s2[0]) {
            node = new TrieNode(parent, s, len);
            parent->children.insert(parent->children.begin()+idx, node);
            return node;
        }
    }
    node = new TrieNode(parent, s, len);
    parent->children.push_back(node);
    return node;
}

static void trieRemove(TrieNode* node)
{
    if (node->children.size() > 1)
        return;
    TrieNode* parent = node->parent;
    if (!parent)
        return;
    char* s = trieCharPtr(node);
    assert(*s);
    unsigned idx = trieLowerBound(parent, 0, parent->children.size(), s[0]);
    assert(parent->children[idx] == node);
    if (node->children.size()) {
        TrieNode* child = node->children[0];
        char* s1 = (char*)malloc(child->length + node->length);
        memcpy(s1, s, node->length);
        memcpy(s1 + node->length, trieCharPtr(child), child->length);
        child->update(parent, s1, child->length + node->length);
        free(s1);
        parent->children[idx] = child;
        delete node;
    } else {
        parent->children.erase(parent->children.begin() + idx);
        delete node;
        if (!parent->ref_count)
            trieRemove(parent);
    }
}

static int trieStringStarts(TrieNode* node, const char* s, int& pos)
{
    int cmp = -1; // -1 still matches, 0 no, 1 yes
    if (node->parent)
        cmp = trieStringStarts(node->parent, s, pos);
    if (cmp == -1) {
        char* s1 = trieCharPtr(node);
        for (unsigned i = 0; i < node->length; ++i)
            if (s1[i] != s[pos + i])
                return !s[pos + i] ? 1 : 0;
        pos += node->length;
    }
    return cmp;
}

TrieString::TrieString (const QString& s) : node(nullptr)
{
    if (!s.isNull()) {
        const QByteArray ba = s.toUtf8();
        node = trieInsert(trieRoot(), ba.constData(), ba.length());
        ++node->ref_count;
    }
}

TrieString::TrieString(const char* s)
    : node(!s ? nullptr : trieInsert(trieRoot(), s, strlen(s)))
{
    if (node)
        ++node->ref_count;
}

TrieString::TrieString(const char* s, int len)
    : node(!s ? nullptr : trieInsert(trieRoot(), s, len))
{
    if (node)
        ++node->ref_count;
}

TrieString::TrieString(const TrieString& s) : node(s.node)
{
    if (node)
        ++node->ref_count;
}

TrieString::~TrieString()
{
    if (node && !--node->ref_count) {
#ifdef TEST_TRIE
        fprintf(stderr, "delete %s\n", qPrintable(toString()));
#endif
        trieRemove(node);
    }
}

TrieString& TrieString::operator=(const char* s)
{
    if (node && !--node->ref_count) {
#ifdef TEST_TRIE
        fprintf(stderr, "= delete %s\n", qPrintable(toString()));
#endif
        trieRemove(node);
    }
    node = !s ? nullptr : trieInsert(trieRoot(), s, strlen(s));
    if (node)
        ++node->ref_count;
    return *this;
}

TrieString& TrieString::operator=(const TrieString& s)
{
    if (s.node != node) {
        if (s.node)
            ++s.node->ref_count;
        if (node && !--node->ref_count) {
#ifdef TEST_TRIE
            fprintf(stderr, "= delete %s\n", qPrintable(toString()));
#endif
            trieRemove(node);
        }
        node = s.node;
    }
    return *this;
}

bool TrieString::operator<(const TrieString& s) const
{
    return trieCompare(node, s.node) < 0;
}

bool KMPlayer::operator==(const TrieString& t, const char* s)
{
    return trieStringCompare(t.node, s) == 0;
}

bool TrieString::startsWith(const TrieString& s) const
{
    for (TrieNode* n = node; n; n = n->parent)
        if (n == s.node)
            return true;
    return s.node ? false : true;
}

bool TrieString::startsWith(const char* str) const
{
    if (!node)
        return !str ? true : false;
    if (!str)
        return true;
    int pos = 0;
    return trieStringStarts(node, str, pos) != 0;
}

QString TrieString::toString() const
{
    if (!node)
        return QString();
    int len = 0;
    char* buf = trieRetrieveString(node, len);
    QString s = QString::fromUtf8(buf);
    free(buf);
    return s;
}

void TrieString::clear()
{
    if (node && !--node->ref_count) {
#ifdef TEST_TRIE
        fprintf(stderr, "clear delete %s\n", qPrintable(toString()));
#endif
        trieRemove(node);
    }
    node = nullptr;
}


TrieString Ids::attr_id;
TrieString Ids::attr_name;
TrieString Ids::attr_src;
TrieString Ids::attr_url;
TrieString Ids::attr_href;
TrieString Ids::attr_width;
TrieString Ids::attr_height;
TrieString Ids::attr_top;
TrieString Ids::attr_left;
TrieString Ids::attr_bottom;
TrieString Ids::attr_right;
TrieString Ids::attr_title;
TrieString Ids::attr_begin;
TrieString Ids::attr_dur;
TrieString Ids::attr_end;
TrieString Ids::attr_region;
TrieString Ids::attr_target;
TrieString Ids::attr_type;
TrieString Ids::attr_value;
TrieString Ids::attr_fill;
TrieString Ids::attr_fit;

void Ids::init() {
    attr_width = "width";
    attr_value = "value";
    attr_url = "url";
    attr_type = "type";
    attr_top = "top";
    attr_title = "title";
    attr_target = "target";
    attr_src = "src";
    attr_right = "right";
    attr_region = "region";
    attr_name = "name";
    attr_left = "left";
    attr_id = "id";
    attr_href = "href";
    attr_height = "height";
    attr_fit = "fit";
    attr_fill = "fill";
    attr_end = "end";
    attr_dur = "dur";
    attr_bottom = "bottom";
    attr_begin = "begin";
}

void Ids::reset() {
    attr_id.clear ();
    attr_name.clear ();
    attr_src.clear ();
    attr_url.clear ();
    attr_href.clear ();
    attr_width.clear ();
    attr_height.clear ();
    attr_top.clear ();
    attr_left.clear ();
    attr_bottom.clear ();
    attr_right.clear ();
    attr_title.clear ();
    attr_begin.clear ();
    attr_dur.clear ();
    attr_end.clear ();
    attr_region.clear ();
    attr_target.clear ();
    attr_type.clear ();
    attr_value.clear ();
    attr_fill.clear ();
    attr_fit.clear ();
    if (trieRoot()->children.size()) {
        qCWarning(LOG_KMPLAYER_COMMON) << "Trie not empty";
        dumpTrie ();
    //} else {
        //delete root_trie;
        //root_trie = 0;
    }
}

void KMPlayer::dumpTrie () {
    dump(trieRoot(), 0);
}

#ifdef TEST_TRIE
// g++ triestring.cpp -o triestring -I$QTDIR/include -L$QTDIR/lib -lqt-mt -g -DTEST_TRIE

int main (int, char **) {
    Ids::init();
    {
    TrieString s1;
    TrieString s1_1(QString ("region"));
    s1 = s1_1;
    TrieString s2 (QString ("regionName"));
    TrieString s3 (QString ("regPoint"));
    TrieString s4 (QString ("regAlign"));
    TrieString s6 (QString ("freeze"));
    TrieString s7 (QString ("fit"));
    {
    TrieString s7_1 (QString ("fit"));
    TrieString s5 (QString ("fill"));
    dump (root_trie, 0);
    }
    dump (root_trie, 0);
    TrieString s5 (QString ("fill"));
    TrieString s8 (QString ("fontPtSize"));
    TrieString s9 (QString ("fontSize"));
    TrieString s10 (QString ("fontFace"));
    TrieString s11 (QString ("fontColor"));
    TrieString s12 (QString ("hAlign"));
    TrieString s13 (QString ("region"));
    TrieString s14 (QString ("ref"));
    TrieString s15 (QString ("head"));
    dump (root_trie, 0);
    QString qs1 = s1.toString ();
    QString qs2 = s2.toString ();
    printf ("%s\n%s\n", qs1.toAscii(), qs2.toAscii());
    printf("equal %s %s %d\n", qs2.toAscii(), "regionName", s2 == "regionName");
    printf("equal %s %s %d\n", qs2.toAscii(), "zegionName", s2 == "zegionName");
    printf("equal %s %s %d\n", qs2.toAscii(), "reqionName", s2 == "reqionName");
    printf("equal %s %s %d\n", qs2.toAscii(), "regiinName", s2 == "regiinName");
    printf("equal %s %s %d\n", qs2.toAscii(), "regionNeme", s2 == "regionNeme");
    printf("%s < %s %d\n", qs2.toAscii(), "regionName", s2 < TrieString("regionName"));
    printf("%s < %s %d\n", qs2.toAscii(), "zegion", s2 < TrieString("zegion"));
    printf("%s < %s %d\n", qs2.toAscii(), "req", s2 < TrieString("req"));
    printf("%s < %s %d\n", qs2.toAscii(), "regiinName", s2 < TrieString("regiinName"));
    printf("%s < %s %d\n", qs2.toAscii(), "regionNeme", s2 < TrieString("regionNeme"));
    printf("%s startsWith %s %d\n", s1.toString().toAscii(), "region", s1.startsWith ("region"));
    printf("%s startsWith %s %d\n", qs2.toAscii(), "region", s2.startsWith ("region"));
    printf("%s startsWith %s %d\n", qs2.toAscii(), "regi", s2.startsWith ("regi"));
    printf("%s startsWith %s %d\n", qs2.toAscii(), "regian", s2.startsWith ("regian"));
    printf("%s startsWith %s %d\n", qs2.toAscii(), "regio", s2.startsWith ("regio"));
    printf("%s startsWith %s %d\n", qs2.toAscii(), "zegio", s2.startsWith ("zegio"));
    printf("%s startsWith %s %d\n", qs2.toAscii(), "r", s2.startsWith ("r"));
    printf("%s startsWith %s %d\n", qs2.toAscii(), "q", s2.startsWith ("q"));
    TrieString fnt ("font");
    printf("%s startsWith %s %d\n", s8.toString().toAscii(), fnt.toString().toAscii(), s8.startsWith(fnt));
    printf("%s startsWith %s %d\n", s8.toString().toAscii(), s14.toString().toAscii(), s8.startsWith(s14));
    }
    dump (root_trie, 0);
    Ids::reset();
    return 0;
}
#endif
