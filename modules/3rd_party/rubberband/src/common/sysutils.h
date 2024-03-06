/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Rubber Band Library
    An audio time-stretching and pitch-shifting library.
    Copyright 2007-2023 Particular Programs Ltd.

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.

    Alternatively, if you have a valid commercial licence for the
    Rubber Band Library obtained by agreement with the copyright
    holders, you may redistribute and/or modify it under the terms
    described in that licence.

    If you wish to distribute code using the Rubber Band Library
    under terms other than those of the GNU General Public License,
    you must obtain a valid commercial licence before doing so.
*/

#ifndef RUBBERBAND_SYSUTILS_H
#define RUBBERBAND_SYSUTILS_H

#ifdef __clang__
#  define R__ __restrict__
#else
#  ifdef __GNUC__
#    define R__ __restrict__
#  endif
#endif

#ifdef _MSC_VER
#  if _MSC_VER < 1800
#    include "float_cast/float_cast.h"
#  endif
#  ifndef R__
#    define R__ __restrict
#  endif
#endif 

#ifndef R__
#  define R__
#endif

#ifdef __clang__
#  define RTENTRY__ __attribute__((annotate("realtime")))
#else
#  define RTENTRY__
#endif

#if defined(_MSC_VER)
#  include <malloc.h>
#  include <process.h>
#  define alloca _alloca
#  define getpid _getpid
#else
#  if defined(__MINGW32__)
#    include <malloc.h>
#  elif defined(__GNUC__)
#    ifndef alloca
#      define alloca __builtin_alloca
#    endif
#  elif defined(HAVE_ALLOCA_H)
#    include <alloca.h>
#  else
#    ifndef __USE_MISC
#    define __USE_MISC
#    endif
#    include <stdlib.h>
#  endif
#endif

#if defined(_MSC_VER) && _MSC_VER < 1700
#  define uint8_t unsigned __int8
#  define uint16_t unsigned __int16
#  define uint32_t unsigned __int32
#elif defined(_MSC_VER)
#  define ssize_t long
#  include <stdint.h>
#else
#  include <stdint.h>
#endif

#include <math.h>

namespace RubberBand {

#ifdef PROCESS_SAMPLE_TYPE
typedef PROCESS_SAMPLE_TYPE process_t;
#else
typedef double process_t;
#endif

extern const char *system_get_platform_tag();
extern bool system_is_multiprocessor();

#ifdef _WIN32
struct timeval { long tv_sec; long tv_usec; };
void gettimeofday(struct timeval *p, void *tz);
#endif // _WIN32

} // end namespace

// The following should be functions in the RubberBand namespace, really

#ifdef _WIN32

namespace RubberBand {
extern void system_memorybarrier();
}
#define MBARRIER() RubberBand::system_memorybarrier()

#else // !_WIN32

#include <stdio.h>

#ifdef __APPLE__
#  if defined __MAC_10_12
#    define MBARRIER() __sync_synchronize()
#  else
#    include <libkern/OSAtomic.h>
#    define MBARRIER() OSMemoryBarrier()
#  endif
#else
#  if (__GNUC__ > 4) || (__GNUC__ == 4 && __GNUC_MINOR__ >= 1)
#    define MBARRIER() __sync_synchronize()
#  else
namespace RubberBand {
extern void system_memorybarrier();
}
#    define MBARRIER() ::RubberBand::system_memorybarrier()
#  endif
#endif

#endif // !_WIN32

#ifdef NO_THREADING
#  undef MBARRIER
#  define MBARRIER() 
#endif // NO_THREADING

#endif

