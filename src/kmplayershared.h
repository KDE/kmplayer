/* This file is part of the KDE project
 *
 * Copyright (C) 2004 Koos Vriezen <koos.vriezen@xs4all.nl>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * until boost gets common, a more or less compatable one ..
 */

#ifndef _SHAREDPTR_H_
#define _SHAREDPTR_H_

// #define SHAREDPTR_DEBUG
// static int shared_data_count;

template <class T>
struct SharedData {
    SharedData (T * t, bool w) : use_count (w?0:1), weak_count (1), ptr (t) {
#ifdef SHAREDPTR_DEBUG
    std::cerr << "SharedData::SharedData use:" << use_count << " weak:" << weak_count << " total:" << ++shared_data_count << std::endl;
#endif
    }
#ifdef SHAREDPTR_DEBUG
    ~SharedData () { std::cerr << "SharedData::~SharedData" << " total:" << --shared_data_count << std::endl; }
#endif
    void addRef ();
    void addWeakRef ();
    void release ();
    void releaseWeak ();
    void dispose ();
    int use_count;
    int weak_count;
    T * ptr;
};

template <class T> inline void SharedData<T>::addRef () {
    use_count++;
    weak_count++;
#ifdef SHAREDPTR_DEBUG
    std::cerr << "SharedData::addRef use:" << use_count << " weak:" << weak_count << std::endl;
#endif
}

template <class T> inline void SharedData<T>::addWeakRef () {
    weak_count++;
#ifdef SHAREDPTR_DEBUG
    std::cerr << "SharedData::addWeakRef use:" << use_count << " weak:" << weak_count << std::endl;
#endif
}

template <class T> inline void SharedData<T>::releaseWeak () {
    ASSERT (weak_count > 0 && weak_count > use_count);
#ifdef SHAREDPTR_DEBUG
    std::cerr << "SharedData::releaseWeak use:" << use_count << " weak:" << weak_count-1 << std::endl;
#endif
    if (--weak_count <= 0) delete this;
}

template <class T> inline void SharedData<T>::release () {
    ASSERT (use_count > 0);
    if (--use_count <= 0) dispose (); 
#ifdef SHAREDPTR_DEBUG
    std::cerr << "SharedData::release use:" << use_count << " weak:" << weak_count << std::endl;
#endif
    releaseWeak ();
}

template <class T> inline void SharedData<T>::dispose () {
    ASSERT (use_count == 0);
#ifdef SHAREDPTR_DEBUG
    std::cerr << "SharedData::dispose use:" << use_count << " weak:" << weak_count << std::endl;
#endif
    delete ptr;
    ptr = 0;
}

template <class T> struct WeakPtr;

template <class T>
struct SharedPtr {
    SharedPtr () : data (0L) {};
    SharedPtr (T * t) : data (t ? new SharedData<T> (t, false) : 0L) {};
    SharedPtr (const SharedPtr<T> & s) : data (s.data) { if (data) data->addRef (); }
    SharedPtr (const WeakPtr <T> &);
    ~SharedPtr () { if (data) data->release (); }
    SharedPtr<T> & operator = (const SharedPtr<T> &);
    SharedPtr<T> & operator = (const WeakPtr<T> &);
    SharedPtr<T> & operator = (T *);
    T * operator -> () { return data ? data->ptr : 0L; }
    T * operator -> () const { return data ? data->ptr : 0L; }
    T & operator * () { return *data->ptr; }
    const T & operator * () const { return *data->ptr; }
    // operator bool () const { return data && data->ptr; }
    bool operator == (const SharedPtr<T> & s) const { return data == s.data; }
    bool operator == (const WeakPtr<T> & w) const;
    bool operator == (const T * t) const { return (!t && ! data) || (data && data->ptr == t); }
    bool operator != (const SharedPtr<T> & s) const { return data != s.data; }
    bool operator != (const WeakPtr<T> & w) const;
    bool operator != (const T * t) const { return !operator == (t); }
    operator T * () { return data ? data->ptr : 0L; }
    operator const T * () const { return data ? data->ptr : 0L; }
    mutable SharedData<T> * data;
};

template <class T>
inline SharedPtr<T> & SharedPtr<T>::operator = (const SharedPtr<T> & s) {
    if (data != s.data) {
        SharedData<T> * tmp = data;
        data = s.data;
        if (data) data->addRef ();
        if (tmp) tmp->release ();
    }
    return *this;
}

template <class T> inline SharedPtr<T> & SharedPtr<T>::operator = (T * t) {
    if ((!data && t) || (data && data->ptr != t)) {
        if (data) data->release ();
        data = t ? new SharedData<T> (t, false) : 0L;
    }
    return *this;
}

template <class T>
struct WeakPtr {
    WeakPtr () : data (0L) {};
    WeakPtr (T * t) : data (t ? new SharedData<T> (t, true) : 0) {};
    WeakPtr (const WeakPtr<T> & s) : data (s.data) { if (data) data->addWeakRef (); }
    WeakPtr (const SharedPtr<T> & s) : data (s.data) { if (data) data->addWeakRef (); }
    ~WeakPtr () { if (data) data->releaseWeak (); }
    WeakPtr<T> & operator = (const WeakPtr<T> &);
    WeakPtr<T> & operator = (const SharedPtr<T> &);
    WeakPtr<T> & operator = (T *);
    T * operator -> () { return data ? data->ptr : 0L; }
    const T * operator -> () const { return data ? data->ptr : 0L; }
    T & operator * () { return *data->ptr; }
    const T & operator * () const { return *data->ptr; }
    // operator bool () const { return data && !!data->ptr; }
    bool operator == (const WeakPtr<T> & w) const { return data == w.data; }
    bool operator == (const SharedPtr<T> & s) const { return data == s.data; }
    bool operator == (const T * t) const { return (!t && !data) || (data && data.ptr == t); }
    bool operator != (const WeakPtr<T> & w) const { return data != w.data; }
    bool operator != (const SharedPtr<T> & s) const { return data != s.data; }
    operator T * () { return data ? data->ptr : 0L; }
    operator const T * () const { return data ? data->ptr : 0L; }
    mutable SharedData<T> * data;
};

template <class T>
inline WeakPtr<T> & WeakPtr<T>::operator = (const WeakPtr<T> & w) {
    if (data != w.data) {
        SharedData<T> * tmp = data;
        data = w.data;
        if (data) data->addWeakRef ();
        if (tmp) tmp->releaseWeak ();
    }
    return *this;
}

template <class T>
inline WeakPtr<T> & WeakPtr<T>::operator = (const SharedPtr<T> & s) {
    if (data != s.data) {
        SharedData<T> * tmp = data;
        data = s.data;
        if (data) data->addWeakRef ();
        if (tmp) tmp->releaseWeak ();
    }
    return *this;
}

template <class T>
inline WeakPtr<T> & WeakPtr<T>::operator = (T * t) {
    if (data) data->releaseWeak ();
    data = t ? new SharedData<T> (t, true) : 0L;
    return *this;
}

template <class T> inline SharedPtr<T>::SharedPtr (const WeakPtr <T> & w) : data (w.data) {
    if (data) data->addRef ();
}

template <class T>
inline SharedPtr<T> & SharedPtr<T>::operator = (const WeakPtr<T> & s) {
    if (data != s.data) {
        SharedData<T> * tmp = data;
        data = s.data;
        if (data) data->addRef ();
        if (tmp) tmp->release ();
    }
    return *this;
}

template <class T>
inline  bool SharedPtr<T>::operator == (const WeakPtr<T> & w) const {
    return data == w.data;
}

template <class T>
inline  bool SharedPtr<T>::operator != (const WeakPtr<T> & w) const {
    return data != w.data;
}

#endif
