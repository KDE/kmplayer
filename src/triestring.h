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

#ifndef _TRIE_STRING_H_
#define _TRIE_STRING_H_

#include <qstring.h>

namespace KMPlayer {

class TrieNode;

class KMPLAYER_EXPORT TrieString {
    TrieNode * node;
    friend bool operator == (const TrieString & s1, const TrieString & s2);
    friend bool operator == (const TrieString & s, const char * utf8);
    friend bool operator == (const char * utf8, const TrieString & s);
    friend bool operator != (const TrieString & s1, const TrieString & s2);
public:
    TrieString ();
    TrieString (const QString & s);
    TrieString (const char * utf8);
    TrieString (const TrieString & s);
    ~TrieString ();

    QString toString () const;
    bool isNull () const;
    void clear ();
    bool startsWith (const TrieString & s) const;
    bool startsWith (const char * str) const;
    TrieString & operator = (const TrieString & s);
    TrieString & operator = (const char * utf8);
    bool operator < (const TrieString & s) const;
};

inline TrieString::TrieString () : node (0L) {}

class KMPLAYER_EXPORT StringPool {
public:
    static void init();
    static void reset();

    static TrieString attr_id;
    static TrieString attr_name;
    static TrieString attr_src;
    static TrieString attr_url;
    static TrieString attr_href;
    static TrieString attr_width;
    static TrieString attr_height;
    static TrieString attr_top;
    static TrieString attr_left;
    static TrieString attr_bottom;
    static TrieString attr_right;
    static TrieString attr_title;
    static TrieString attr_begin;
    static TrieString attr_dur;
    static TrieString attr_end;
    static TrieString attr_region;
    static TrieString attr_target;
    static TrieString attr_type;
    static TrieString attr_value;
    static TrieString attr_fill;
};

inline bool TrieString::isNull () const {
    return !node;
}

inline bool operator == (const TrieString & s1, const TrieString & s2) {
    return s1.node == s2.node;
}

bool operator == (const TrieString & s, const char * utf8);

inline bool operator == (const char * utf8, const TrieString & s) {
    return s == utf8;
}

inline bool operator != (const TrieString & s1, const TrieString & s2) {
    return s1.node != s2.node;
}

void dumpTrie ();

} // namespace

#endif // _TRIE_STRING_H_
