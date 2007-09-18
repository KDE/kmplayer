/* This file is part of the KDE project
 *
 * Copyright (C) 2006 Koos Vriezen <koos.vriezen@xs4all.nl>
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
 * the Free Software Foundation, Inc., 51 Franklin Steet, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 * until boost gets common, a more or less compatable one ..
 */

#ifndef _KMPLAYER_DEF_H_
#define _KMPLAYER_DEF_H_

#include <config.h>
#ifndef ASSERT
#define ASSERT Q_ASSERT
#endif

#include <kdemacros.h>

#undef KDE_NO_CDTOR_EXPORT
#undef KDE_NO_EXPORT
#ifndef KDE_EXPORT
  #define KDE_EXPORT
#endif
#if __GNUC__ - 0 > 3 && __GNUC_MINOR__ - 0 > 1
# define KMPLAYER_NO_EXPORT __attribute__ ((visibility("hidden")))
# define KMPLAYER_EXPORT __attribute__ ((visibility("default")))
# define KDE_NO_CDTOR_EXPORT
# define KDE_NO_EXPORT
#elif __GNUC__ - 0 > 3 || (__GNUC__ - 0 == 3 && __GNUC_MINOR__ - 0 > 3)
  #if __GNUC__ - 0 > 3
    #define KMPLAYER_NO_EXPORT __attribute__ ((visibility("hidden")))
  #else
    #define KMPLAYER_NO_EXPORT
  #endif
  #define KDE_NO_CDTOR_EXPORT __attribute__ ((visibility("hidden")))
  #define KDE_NO_EXPORT __attribute__ ((visibility("hidden")))
  #define KMPLAYER_EXPORT __attribute__ ((visibility("default")))
#elif __GNUC__ - 0 > 3 || (__GNUC__ - 0 == 3 && __GNUC_MINOR__ - 0 > 2)
  #define KDE_NO_CDTOR_EXPORT
  #define KDE_NO_EXPORT __attribute__ ((visibility("hidden")))
  #define KMPLAYER_EXPORT
  #define KMPLAYER_NO_EXPORT
#else
  #define KDE_NO_CDTOR_EXPORT
  #define KDE_NO_EXPORT
  #define KMPLAYER_EXPORT
  #define KMPLAYER_NO_EXPORT
#endif


#endif //_KMPLAYER_DEF_H_
