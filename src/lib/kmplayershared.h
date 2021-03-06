/*
    SPDX-FileCopyrightText: 2004 Koos Vriezen <koos.vriezen@xs4all.nl>

    SPDX-License-Identifier: LGPL-2.0-or-later

    until boost gets common, a more or less compatable one ..
 */

#ifndef _SHAREDPTR_H_
#define _SHAREDPTR_H_

//#define SHAREDPTR_DEBUG

#ifdef SHAREDPTR_DEBUG
extern int shared_data_count;
#include <iostream>
#endif

#include "kmplayercommon_export.h"

namespace KMPlayer {

class KMPLAYERCOMMON_EXPORT CacheAllocator {
    void **pool;
    size_t size;
    int count;
public:
    CacheAllocator (size_t s);

    void *alloc ();
    void dealloc (void *p);
};

extern CacheAllocator *shared_data_cache_allocator;

/**
 *  Shared data for SharedPtr and WeakPtr objects.
 **/
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
    static void *operator new (size_t);
    static void operator delete (void *);
    void addRef ();
    void addWeakRef ();
    void release ();
    void releaseWeak ();
    void dispose ();
    int use_count;
    int weak_count;
    T * ptr;
};

template <class T> inline void *SharedData<T>::operator new (size_t s) {
    if (!shared_data_cache_allocator)
        shared_data_cache_allocator = new CacheAllocator (s);
    return shared_data_cache_allocator->alloc ();
}

template <class T> inline void SharedData<T>::operator delete (void *p) {
    shared_data_cache_allocator->dealloc (p);
}

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
    Q_ASSERT (weak_count > 0 && weak_count > use_count);
#ifdef SHAREDPTR_DEBUG
    std::cerr << "SharedData::releaseWeak use:" << use_count << " weak:" << weak_count-1 << std::endl;
#endif
    if (--weak_count <= 0) delete this;
}

template <class T> inline void SharedData<T>::release () {
    Q_ASSERT (use_count > 0);
    if (--use_count <= 0) dispose (); 
#ifdef SHAREDPTR_DEBUG
    std::cerr << "SharedData::release use:" << use_count << " weak:" << weak_count << std::endl;
#endif
    releaseWeak ();
}

template <class T> inline void SharedData<T>::dispose () {
    Q_ASSERT (use_count == 0);
#ifdef SHAREDPTR_DEBUG
    std::cerr << "SharedData::dispose use:" << use_count << " weak:" << weak_count << std::endl;
#endif
    T *p = ptr;
    ptr = nullptr;
    delete p;
}

template <class T> struct WeakPtr;

/**
 * Shared class based on boost shared
 * This makes it possible to share pointers w/o having to worry about
 * memory leaks. A pointer gets deleted as soon as the last Shared pointer
 * gets destroyed. As such, never use (or be extremely carefull) not to
 * use pointers or references to shared objects
 **/
template <class T>
struct SharedPtr {
    SharedPtr () : data (nullptr) {};
    SharedPtr (T *t) : data (t ? new SharedData<T> (t, false) : nullptr) {}
    SharedPtr (const SharedPtr<T> & s) : data (s.data) { if (data) data->addRef (); }
    SharedPtr (const WeakPtr <T> &);
    ~SharedPtr () { if (data) data->release (); }
    SharedPtr<T> & operator = (const SharedPtr<T> &);
    SharedPtr<T> & operator = (const WeakPtr<T> &);
    SharedPtr<T> & operator = (T *);
    T * ptr () const { return data ? data->ptr : nullptr; }
    T * operator -> () { return data ? data->ptr : nullptr; }
    T * operator -> () const { return data ? data->ptr : nullptr; }
    T & operator * () { return *data->ptr; }
    const T & operator * () const { return *data->ptr; }
    // operator bool () const { return data && data->ptr; }
    bool operator == (const SharedPtr<T> & s) const { return data == s.data; }
    bool operator == (const WeakPtr<T> & w) const;
    bool operator == (const T * t) const { return (!t && !data) || (data && data->ptr == t); }
    bool operator == (T * t) const { return (!t && !data) || (data && data->ptr == t); }
    bool operator != (const SharedPtr<T> & s) const { return data != s.data; }
    bool operator != (const WeakPtr<T> & w) const;
    bool operator != (const T * t) const { return !operator == (t); }
    operator T * () { return data ? data->ptr : nullptr; }
    operator const T * () const { return data ? data->ptr : nullptr; }
    mutable SharedData<T> * data;
};

template <class T>
bool operator == (T * t, SharedPtr <T> & s) {
    return (!t && !s.data) || (s.data && s.data->ptr == t);
}

template <class T>
bool operator == (const T * t, SharedPtr <T> & s) {
    return (!t && !s.data) || (s.data && s.data->ptr == t);
}

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
        data = t ? new SharedData<T> (t, false) : nullptr;
    }
    return *this;
}

/**
 * Weak version of SharedPtr. This will also have access to the SharedData
 * pointer, only these object wont prevent destruction of the shared
 * pointer, hence weak references
 */
template <class T>
struct WeakPtr {
    WeakPtr () : data (nullptr) {};
    WeakPtr (T * t) : data (t ? new SharedData<T> (t, true) : 0) {}
    WeakPtr (T * t, bool /*b*/) : data (t ? new SharedData<T> (t, true) : nullptr) {}
    WeakPtr (const WeakPtr<T> & s) : data (s.data) { if (data) data->addWeakRef (); }
    WeakPtr (const SharedPtr<T> & s) : data (s.data) { if (data) data->addWeakRef (); }
    ~WeakPtr () { if (data) data->releaseWeak (); }
    WeakPtr<T> & operator = (const WeakPtr<T> &);
    WeakPtr<T> & operator = (const SharedPtr<T> &);
    WeakPtr<T> & operator = (T *);
    T * ptr () const { return data ? data->ptr : nullptr; }
    T * operator -> () { return data ? data->ptr : nullptr; }
    const T * operator -> () const { return data ? data->ptr : 0L; }
    T & operator * () { return *data->ptr; }
    const T & operator * () const { return *data->ptr; }
    // operator bool () const { return data && !!data->ptr; }
    bool operator == (const WeakPtr<T> & w) const { return data == w.data; }
    bool operator == (const SharedPtr<T> & s) const { return data == s.data; }
    bool operator == (const T * t) const { return (!t && !data) || (data && data->ptr == t); }
    bool operator == (T * t) const { return (!t && !data) || (data && data->ptr == t); }
    bool operator != (const WeakPtr<T> & w) const { return data != w.data; }
    bool operator != (const SharedPtr<T> & s) const { return data != s.data; }
    operator T * () { return data ? data->ptr : nullptr; }
    operator const T * () const { return data ? data->ptr : nullptr; }
    mutable SharedData<T> * data;
};

template <class T>
bool operator == (T * t, WeakPtr <T> & s) {
    return (!t && !s.data) || (s.data && s.data->ptr == t);
}

template <class T>
bool operator == (const T * t, WeakPtr <T> & s) {
    return (!t && !s.data) || (s.data && s.data->ptr == t);
}

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
    data = t ? new SharedData<T> (t, true) : nullptr;
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

}

#endif
