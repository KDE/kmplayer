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
# define KMPLAYER_NO_EXPORT
# define KMPLAYER_EXPORT
# define KDE_NO_EXPORT
# define KDE_NO_CDTOR_EXPORT
#else
# include <config-kmplayer.h>
# include "kmplayer_def.h"
#endif
#include <stdio.h>
#include <stdlib.h>


#include "triestring.h"

namespace KMPlayer {

struct KMPLAYER_NO_EXPORT TrieNode {
    TrieNode (const char * s);
    ~TrieNode ();
    void unref ();
    void removeChild (TrieNode *);
    void dump (int lvl) {
        QString indent (QString ().fill (QChar ('.'), lvl));
        printf("%s%s len:%4d rc:%4d\n", indent.toAscii(), str, length, ref_count);
    }
    char * str;
    unsigned short length;
    unsigned short ref_count;
    TrieNode * parent;
    TrieNode * first_child;
    TrieNode * next_sibling;
};

}

using namespace KMPlayer;

static TrieNode * root_trie;

void dump (TrieNode * node, int lvl) {
    if (!node)
        return;
    node->dump (lvl);
    dump (node->first_child, lvl+2);
    if (node->next_sibling)
        dump (node->next_sibling, lvl);
}

KDE_NO_CDTOR_EXPORT TrieNode::TrieNode (const char * s)
  : str (s ? strdup (s) : 0L),
    length (s ? strlen (s) : 0),
    ref_count (1),
    parent (0L),
    first_child (0L),
    next_sibling (0L) {}

KDE_NO_CDTOR_EXPORT TrieNode::~TrieNode () {
    if (str)
        free (str);
}

KDE_NO_EXPORT void TrieNode::unref () {
    if (--ref_count <= 0 && !first_child)
        parent->removeChild (this);
}

KDE_NO_EXPORT void TrieNode::removeChild (TrieNode * node) {
    if (node == first_child) {
        first_child = node->next_sibling;
    } else {
        for (TrieNode *tn = first_child; tn; tn = tn->next_sibling)
            if (tn->next_sibling == node) {
                tn->next_sibling = node->next_sibling;
                break;
            }
    }
    delete node;
    if (!parent)
        return;
    if (!ref_count && !first_child)
        parent->removeChild (this); // can this happen ?
    else if (!ref_count && !first_child->next_sibling) { // merge with child
        char * tmp = first_child->str;
        first_child->length = first_child->length + length;
        first_child->str = (char *) malloc (first_child->length + 1);
        strcpy (first_child->str, str);
        strcat (first_child->str, tmp);
        free (tmp);
        first_child->parent = parent;
        first_child->next_sibling = next_sibling;
        if (parent->first_child == this) {
            parent->first_child = first_child;
        } else {
            for (TrieNode *n = parent->first_child; n; n = n->next_sibling)
                if (n->next_sibling == this) {
                    n->next_sibling = first_child;
                    break;
                }
        }
        delete this;
    }
}

static char * trieRetrieveString (TrieNode * node, int &len) {
    char *buf;
    if (node->parent) {
        len += node->length;
        buf = trieRetrieveString (node->parent, len);
        strcat (buf, node->str);
    } else {
        buf = (char *) malloc (len + 1);
        *buf = 0;
    }
    return buf;
}

static int trieStringCompare (TrieNode * node, const char * s, int &len) {
    int cmp = 0;
    if (!node)
        return !!s;
    if (node->parent && node->parent != root_trie)
        cmp = trieStringCompare (node->parent, s, len);
    if (!cmp) {
#ifdef TEST_TRIE
        printf( "compare %s %s %d\n", node->str, s + len, node->length);
#endif
        cmp = s ? strncmp (node->str, s + len, node->length) : 1;
        len += node->length;
    }
    return cmp;
}

static int trieStringCompare (TrieNode * n1, TrieNode * n2) {
    // pre n1 && n2 on same depth and not NIL
    int cmp = 0;
    if (n1->parent && n1->parent != root_trie)
        cmp = trieStringCompare (n1->parent, n2->parent);
    if (!cmp && n1 != n2) {
#ifdef TEST_TRIE
        printf( "compare %s %s", n1->str, n2->str);
#endif
        if (!n1->str)
            cmp = n2->str ? 1 : 0;
        else if (!n2->str)
            cmp = 1;
        else
            cmp = strcmp (n1->str, n2->str);
#ifdef TEST_TRIE
        printf( "=> %d\n", cmp);
#endif
    }
    return cmp;
}

static int trieStringStarts (TrieNode * node, const char * s, int & pos) {
    int cmp = -1; // -1 still matches, 0 no, 1 yes
    if (node->parent && node->parent != root_trie)
        cmp = trieStringStarts (node->parent, s, pos);
    if (cmp == -1) {
        for (int i = 0; i < node->length; i++)
            if (node->str[i] != s[pos + i])
                return !s[pos + i] ? 1 : 0;
        pos += node->length;
    }
    return cmp;
}

static TrieNode * trieInsert (const char * s) {
    if (!root_trie)
        root_trie = new TrieNode (0L);
    //printf("trieInsert %s\n", s);
    //dumpTrie();
    TrieNode * parent = root_trie;
    for (TrieNode * c = parent->first_child; c; c = c->first_child) {
        TrieNode * prev = c;
        for (TrieNode * n = prev; n; n = n->next_sibling) {
            if (n->str[0] == s[0]) { // insert here
                int i = 1;
                for (; i < n->length; i++) {
                    if (n->str[i] != s[i]) { // break here
                        // insert new node so strings to n remain valid
                        bool bigger = n->str[i] < s[i];
                        char *tmp = n->str;
                        n->str = strdup (tmp + i);
                        n->length -= i;
                        tmp[i] = 0;
                        TrieNode * node = new TrieNode (tmp);
                        free (tmp);
                        node->parent = parent;
                        node->next_sibling = n->next_sibling;
                        if (prev != n)
                            prev->next_sibling = node;
                        else
                            parent->first_child = node;
                        n->parent = node;
                        TrieNode * snode;
                        if (!s[i]) {
                            node->first_child = n;
                            n->next_sibling = 0L;
                            snode = node; // s is complete in node
                        } else {
                            snode = new TrieNode (s+i);
                            snode->parent = node;
                            if (bigger) { // set n before snode
                                node->first_child = n;
                                n->next_sibling = snode;
                            } else {      // set snode before n
                                node->first_child = snode;
                                snode->next_sibling = n;
                                n->next_sibling = 0L;
                            }
                            node->ref_count--;
                        }
                        return snode;
                    }
                }
                if (s[i]) { // go one level deeper with s+i
                    s = s + i;
                    c = n;
                    prev = 0;
                    break;
                } // else n and s are equal
                n->ref_count++;
                return n;
            } else if (n->str[0] > s[0]) { // insert before
                TrieNode * node = new TrieNode (s);
                node->parent = parent;
                node->next_sibling = n;
                if (prev != n)
                    prev->next_sibling = node;
                else
                    parent->first_child = node;
                return node;
            }
            prev = n;
        }
        if (prev) { // insert after
            TrieNode * node = new TrieNode (s);
            node->parent = parent;
            prev->next_sibling = node;
            return node;
        }
        parent = c;
    }
    // hit an empty first_child, add s as first_child
    TrieNode * node = new TrieNode (s);
    parent->first_child = node;
    node->parent = parent;
    return node;
}

TrieString::TrieString (const QString & s)
  : node (s.isEmpty () ? 0L : trieInsert (s.utf8 ().data ()))
{}

TrieString::TrieString (const char * utf8)
  : node (!utf8 ? 0L : trieInsert (utf8))
{}

TrieString::TrieString (const TrieString & s) : node (s.node) {
    if (node)
        node->ref_count++;
}

TrieString::~TrieString () {
    if (node)
        node->unref ();
}

bool TrieString::startsWith (const TrieString & s) const {
    for (TrieNode * n = node; n; n = n->parent)
        if (n == s.node)
            return true;
    return s.node ? false : true;
}

bool TrieString::startsWith (const char * str) const {
    if (!node)
        return !str ? true : false;
    if (!str)
        return true;
    int pos = 0;
    return trieStringStarts (node, str, pos) != 0;
}

void TrieString::clear () {
    if (node)
        node->unref ();
    node = 0L;
}

TrieString & TrieString::operator = (const TrieString & s) {
    if (s.node != node) {
        if (s.node)
            s.node->ref_count++;
        if (node)
            node->unref ();
        node = s.node;
    }
    return *this;
}

TrieString & TrieString::operator = (const char * utf8) {
    if (node)
        node->unref ();
    node = !utf8 ? 0L : trieInsert (utf8);
    return *this;
}

QString TrieString::toString () const {
    QString s;
    if (node) {
        int len = 0;
        char *utf8 = trieRetrieveString (node, len);
        s = QString::fromUtf8 (utf8);
        free (utf8);
    }
    return s;
}

bool TrieString::operator < (const TrieString & s) const {
    if (node == s.node)
        return false;
    int depth1 = 0, depth2 = 0;
    for (TrieNode * n = node; n; n = n->parent)
        depth1++;
    if (!depth1)
        return s.node ? true : false;
    for (TrieNode * n = s.node; n; n = n->parent)
        depth2++;
    if (!depth2)
        return false;
    TrieNode * n1 = node;
    TrieNode * n2 = s.node;
    while (depth1 > depth2) {
        if (n1 == n2)
            return false;
        n1 = n1->parent;
        depth1--;
    }
    while (depth2 > depth1) {
        if (n1 == n2)
            return true;
        n2 = n2->parent;
        depth2--;
    }
    int cmp = trieStringCompare (n1, n2);
    if (cmp)
        return cmp < 0;
    return depth1 < depth2;
}

bool KMPlayer::operator == (const TrieString & s1, const char * s2) {
    int len = 0;
    return !trieStringCompare (s1.node, s2, len);
}


TrieString StringPool::attr_id;
TrieString StringPool::attr_name;
TrieString StringPool::attr_src;
TrieString StringPool::attr_url;
TrieString StringPool::attr_href;
TrieString StringPool::attr_width;
TrieString StringPool::attr_height;
TrieString StringPool::attr_top;
TrieString StringPool::attr_left;
TrieString StringPool::attr_bottom;
TrieString StringPool::attr_right;
TrieString StringPool::attr_title;
TrieString StringPool::attr_begin;
TrieString StringPool::attr_dur;
TrieString StringPool::attr_end;
TrieString StringPool::attr_region;
TrieString StringPool::attr_target;
TrieString StringPool::attr_type;
TrieString StringPool::attr_value;
TrieString StringPool::attr_fill;

void StringPool::init() {
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
    attr_fill = "fill";
    attr_end = "end";
    attr_dur = "dur";
    attr_bottom = "bottom";
    attr_begin = "begin";
}

void StringPool::reset() {
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
    if (root_trie->first_child) {
        qWarning ("Trie not empty");
        dumpTrie ();
    } else {
        delete root_trie;
        root_trie = 0;
    }
}

void KMPlayer::dumpTrie () {
    dump (root_trie, 0);
}

#ifdef TEST_TRIE
// g++ triestring.cpp -o triestring -I$QTDIR/include -L$QTDIR/lib -lqt-mt -g -DTEST_TRIE

int main (int, char **) {
    StringPool::init();
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
    StringPool::reset();
    return 0;
}
#endif
