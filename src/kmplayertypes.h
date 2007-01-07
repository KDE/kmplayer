/*
* Copyright (C) 2006  Koos Vriezen <koos.vriezen@gmail.com>
*
* This library is free software; you can redistribute it and/or
* modify it under the terms of the GNU Lesser General Public
* License as published by the Free Software Foundation; either
* version 2 of the License, or (at your option) any later version.
*
* This library is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
* Lesser General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public
* License along with this library; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef _KMPLAYER_TYPES_H_
#define _KMPLAYER_TYPES_H_

#include <stdint.h>
#include "kmplayer_def.h"

namespace KMPlayer {

/**
 * Type meant for screen coordinates
 */
class KMPLAYER_NO_EXPORT Single {
    int value;
    friend Single operator + (const Single s1, const Single s2);
    friend Single operator - (const Single s1, const Single s2);
    friend Single operator * (const Single s1, const Single s2);
    friend Single operator / (const Single s1, const Single s2);
    friend Single operator + (const Single s1, const int i);
    friend Single operator - (const Single s1, const int i);
    friend float operator * (const Single s, const float f);
    friend double operator * (const Single s, const double f);
    friend Single operator * (const int i, const Single s);
    friend float operator * (const float f, const Single s);
    friend double operator * (const double d, const Single s);
    friend Single operator / (const Single s, const int i);
    friend double operator / (const Single s, const double d);
    friend double operator / (const double d, const Single s);
    friend bool operator > (const Single s1, const Single s2);
    friend bool operator > (const Single s, const int i);
    friend bool operator > (const int i, const Single s);
    friend bool operator >= (const Single s1, const Single s2);
    friend bool operator == (const Single s1, const Single s2);
    friend bool operator != (const Single s1, const Single s2);
    friend bool operator < (const Single s1, const Single s2);
    friend bool operator < (const Single s, const int i);
    friend bool operator < (const int i, const Single s);
    friend bool operator <= (const Single s1, const Single s2);
    friend bool operator <= (const Single s, const int i);
#ifdef _KDEBUG_H_
    friend kdbgstream & operator << (kdbgstream &, Single s);
    friend kndbgstream & operator << (kndbgstream &, Single s);
#endif
    friend Single operator - (const Single s);
public:
    Single () : value (0) {}
    Single (const int v) : value (v << 8) {}
    Single (const float v) : value (int (256 * v)) {}
    Single (const double v) : value (int (256 * v)) {}
    Single & operator = (const Single & s) { value = s.value; return *this; }
    Single & operator = (const int v) { value = v << 8; return *this; }
    Single & operator = (const float v) { value = int (256 * v); return *this; }
    Single & operator = (const double v) { value = int(256 * v); return *this; }
    Single & operator += (const Single & s) { value += s.value; return *this; }
    Single & operator += (const int i) { value += (i << 8); return *this; }
    Single & operator -= (const Single & s) { value -= s.value; return *this; }
    Single & operator -= (const int i) { value -= (i << 8); return *this; }
    Single & operator *= (const Single & s);
    Single & operator *= (const float f) { value = int(value*f); return *this; }
    Single & operator /= (const int i) { value /= i; return *this; }
    operator int () const { return (value >> 8) + (value & 0xff > 127 ? 1 : 0);}
    operator double () const { return 1.0 * value / 256; }
};

/**                                   a  b  0
 * Matrix for coordinate transforms   c  d  0
 *                                    tx ty 1     */
class KMPLAYER_NO_EXPORT Matrix {
    friend class SizeEvent;
    float a, b, c, d;
    Single tx, ty; 
public:
    Matrix ();
    Matrix (const Matrix & matrix);
    Matrix (Single xoff, Single yoff, float xscale, float yscale);
    void getXY (Single & x, Single & y) const;
    void getXYWH (Single & x, Single & y, Single & w, Single & h) const;
    void transform (const Matrix & matrix);
    void scale (float sx, float sy);
    void translate (Single x, Single y);
    // void rotate (float phi); // add this when needed
};

class KMPLAYER_NO_EXPORT SRect {
    Single _x, _y, _w, _h;
public:
    SRect () {}
    SRect (Single a, Single b, Single w, Single h)
        : _x (a), _y (b), _w (w), _h (h) {}
    Single x () const { return _x; }
    Single y () const { return _y; }
    Single width () const { return _w; }
    Single height () const { return _h; }
    SRect unite (const SRect & r) const;
    SRect intersect (const SRect & r) const;
    bool isValid () const { return _w >= Single (0) && _h >= Single (0); }
    bool operator == (const SRect & r) const;
    bool operator != (const SRect & r) const;
};

//-----------------------------------------------------------------------------

#ifdef _KDEBUG_H_
inline kdbgstream & operator << (kdbgstream & dbg, Single s) {
    dbg << (int)s;
    if (s.value & 0xff)
        dbg << " " << (s.value & 0xff) << "/" << 256;
    return dbg;
}

inline kndbgstream & operator << (kndbgstream & dbg, Single) { return dbg; }
#endif

inline Single & Single::operator *= (const Single & s) {
    value = (((int64_t)value) * s.value) >> 8;
    return *this;
}

inline Single operator + (const Single s1, const Single s2) {
    Single s;
    s.value = s1.value + s2.value;
    return s;
}

inline Single operator - (const Single s1, const Single s2) {
    Single s;
    s.value = s1.value - s2.value;
    return s;
}

inline Single operator * (const Single s1, const Single s2) {
    Single s;
    s.value = (((int64_t)s1.value) * s2.value) >> 8;
    return s;
}

inline Single operator / (const Single s1, const Single s2) {
    Single s;
    s.value = ((int64_t)s1.value << 8) / s2.value;
    return s;
}

inline Single operator + (const Single s, const int i) {
    return s + Single (i);
}

inline Single operator - (const Single s, const int i) {
    return s - Single (i);
}

inline Single operator * (const int i, const Single s) {
    Single s1;
    s1.value = s.value * i;
    return s1;
}

inline Single operator * (const Single s, const int i) {
    return i * s;
}
inline float operator * (const Single s, const float f) {
    return s.value * f / 256;
}

inline double operator * (const Single s, const double d) {
    return s.value * d / 256;
}

inline float operator * (const float f, const Single s) {
    return s.value * f / 256;
}

inline double operator * (const double d, const Single s) {
    return s.value * d / 256;
}

inline Single operator / (const Single s, const int i) {
    Single s1;
    s1.value = s.value / i;
    return s1;
}

inline double operator / (const Single s, const double d) {
    return (s.value / d ) / 256;
}

inline double operator / (const double d, const Single s) {
    return (d * 256 / s.value);
}

inline bool
operator > (const Single s1, const Single s2) { return s1.value > s2.value; }

inline bool
operator > (const Single s, const int i) { return s > Single (i); }

inline bool
operator > (const int i, const Single s) { return Single (i) > s; }

inline bool
operator >= (const Single s1, const Single s2) { return s1.value >= s2.value; }

inline bool
operator == (const Single s1, const Single s2) { return s1.value == s2.value; }

inline bool
operator != (const Single s1, const Single s2) { return s1.value != s2.value; }

inline bool
operator < (const Single s1, const Single s2) { return s1.value < s2.value; }

inline bool
operator < (const Single s, const int i) { return s < Single (i); }

inline bool
operator < (const int i, const Single s) { return Single (i) < s; }

inline bool
operator <= (const Single s1, const Single s2) { return s1.value <= s2.value; }

inline bool
operator <= (const Single s, const int i) { return s <= Single (i); }

inline Single operator - (const Single s) {
    Single s1;
    s1.value = -s.value;
    return s1;
}

//-----------------------------------------------------------------------------

inline bool SRect::operator == (const SRect & r) const {
    return _x == r._x && _y == r._y && _w == r._w && _h == r._h;
}

inline bool SRect::operator != (const SRect & r) const { return !(*this == r); }

}  // KMPlayer namespace

#endif //_KMPLAYER_TYPES_H_
