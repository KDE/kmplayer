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
#include "triestring.h"

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
    friend float operator / (const Single s, const float f);
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
    friend Single operator - (const Single s);
public:
    Single () : value (0) {}
    Single (const int v) : value (v << 8) {}
    Single (const float v) : value (int (256 * v)) {}
    Single (const double v) : value (int (256 * v)) {}
    Single & operator = (const Single s) { value = s.value; return *this; }
    Single & operator = (const int v) { value = v << 8; return *this; }
    Single & operator = (const float v) { value = int (256 * v); return *this; }
    Single & operator = (const double v) { value = int(256 * v); return *this; }
    Single & operator += (const Single s) { value += s.value; return *this; }
    Single & operator += (const int i) { value += (i << 8); return *this; }
    Single & operator -= (const Single s) { value -= s.value; return *this; }
    Single & operator -= (const int i) { value -= (i << 8); return *this; }
    Single & operator *= (const Single s);
    Single & operator *= (const float f) { value = int(value*f); return *this; }
    Single & operator /= (const int i) { value /= i; return *this; }
    Single & operator /= (const float f);
    operator int () const { return value >> 8; }
    operator double () const { return 1.0 * value / 256; }
};

template <class T>
class KMPLAYER_NO_EXPORT Point {
public:
    Point ();
    Point (T _x, T _y);

    bool operator == (const Point<T> &p) const;
    bool operator != (const Point<T> &p) const;

    T x;
    T y;
};

template <class T>
class KMPLAYER_NO_EXPORT Size {
public:
    Size ();
    Size (T w, T h);

    bool isEmpty () const;
    bool operator == (const Size<T> &s) const;
    bool operator != (const Size<T> &s) const;

    T width;
    T height;
};

template <class T>
class KMPLAYER_NO_EXPORT Rect {
public:
    Rect ();
    Rect (T a, T b, T w, T h);
    Rect (T a, T b, const Size<T> &s);
    Rect (const Point<T> &point, const Size<T> &s);
    T x () const;
    T y () const;
    T width () const;
    T height () const;
    Rect<T> unite (const Rect<T> &r) const;
    Rect<T> intersect (const Rect<T> &r) const;
    bool operator == (const Rect<T> &r) const;
    bool operator != (const Rect<T> &r) const;
    bool isEmpty () const;

    Point<T> point;
    Size<T> size;
};

typedef Size<Single> SSize;
typedef Rect<Single> SRect;

typedef Size<int> ISize;
typedef Point<int> IPoint;
typedef Rect<int> IRect;
template <> Rect<int> Rect<int>::intersect (const Rect<int> &r) const;


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
    void getWH (Single & w, Single & h) const;
    IRect toScreen (const SRect &rect) const;
    SRect toUser (const IRect &rect) const;
    void transform (const Matrix & matrix);
    void scale (float sx, float sy);
    void translate (Single x, Single y);
    // void rotate (float phi); // add this when needed
};

//-----------------------------------------------------------------------------

#ifdef _KDEBUG_H_
# ifndef KDE_NO_DEBUG_OUTPUT
inline QDebug & operator << (QDebug & dbg, Single s) {
    dbg << (double) (s);
    return dbg;
}
# else
inline QDebug & operator << (QDebug & dbg, Single) {
    return dbg;
}
# endif
#endif

inline Single & Single::operator *= (const Single s) {
    value = (((int64_t)value) * s.value) >> 8;
    return *this;
}

inline Single & Single::operator /= (const float f) {
    value = (int) (value / f);
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

inline float operator / (const Single s, const float f) {
    return (s.value / f ) / 256;
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

template <class T> inline Point<T>::Point () : x (0), y (0) {}

template <class T> inline Point<T>::Point (T _x, T _y) : x (_x), y (_y) {}

template <class T>
inline bool Point<T>::Point::operator == (const Point<T> &p) const {
    return x == p.x && y == p.y;
}

template <class T>
inline bool Point<T>::Point::operator != (const Point<T> &p) const {
    return !(*this == p);
}

//-----------------------------------------------------------------------------

template <class T> inline Size<T>::Size () : width (0), height (0) {}

template <class T> inline Size<T>::Size (T w, T h) : width (w), height (h) {}

template <class T> inline bool Size<T>::isEmpty () const {
    return width <= 0 || height <= 0;
}

template <class T>
inline bool Size<T>::Size::operator == (const Size<T> &s) const {
    return width == s.width && height == s.height;
}

template <class T>
inline bool Size<T>::Size::operator != (const Size<T> &s) const {
    return !(*this == s);
}

//-----------------------------------------------------------------------------

template <class T> inline Rect<T>::Rect () {}

template <class T> inline  Rect<T>::Rect (T a, T b, T w, T h)
 : point (a, b), size (w, h) {}

template <class T> inline Rect<T>::Rect (T a, T b, const Size<T> &s)
 : point (a, b), size (s) {}

template <class T> inline Rect<T>::Rect (const Point<T> &pnt, const Size<T> &s)
 : point (pnt), size (s) {}

template <class T> inline T Rect<T>::x () const { return point.x; }

template <class T> inline T Rect<T>::y () const { return point.y; }

template <class T> inline T Rect<T>::width () const { return size.width; }

template <class T> inline T Rect<T>::height () const { return size.height; }

template <class T> inline bool Rect<T>::operator == (const Rect<T> &r) const {
    return point == r.point && size == r.size;
}

template <class T> inline bool Rect<T>::operator != (const Rect<T> &r) const {
    return !(*this == r);
}

template <class T> inline bool Rect<T>::isEmpty () const {
    return size.isEmpty ();
}

template <class T> inline Rect<T> Rect<T>::unite (const Rect<T> &r) const {
    if (size.isEmpty ())
        return r;
    if (r.size.isEmpty ())
        return *this;
    T a (point.x < r.point.x ? point.x : r.point.x);
    T b (point.y < r.point.y ? point.y : r.point.y);
    return Rect<T> (a, b,
            ((point.x + size.width < r.point.x + r.size.width)
             ? r.point.x + r.size.width : point.x + size.width) - a,
            ((point.y + size.height < r.point.y + r.size.height)
             ? r.point.y + r.size.height : point.y + size.height) - b);
}

template <class T> inline Rect<T> Rect<T>::intersect (const Rect<T> &r) const {
    T a (point.x < r.point.x ? r.point.x : point.x);
    T b (point.y < r.point.y ? r.point.y : point.y);
    return Rect<T> (a, b,
            ((point.x + size.width < r.point.x + r.size.width)
             ? point.x + size.width : r.point.x + r.size.width) - a,
            ((point.y + size.height < r.point.y + r.size.height)
             ? point.y + size.height : r.point.y + r.size.height) - b);
}

}  // KMPlayer namespace

#endif //_KMPLAYER_TYPES_H_
