/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#pragma once

#if defined (__arm__) || defined (__arm64__) || defined (__aarch64__) || defined (_M_ARM) || defined (_M_ARM64)
 #define TRACKTION_ARM 1
#endif

#ifdef _WIN32
 #include <intrin.h>
#elif TRACKTION_ARM
 // Use clang built-in
#else
 #include <x86intrin.h>
#endif

#if TRACKTION_ARM
 // Use asm yield
#elif __has_include(<emmintrin.h>)
 #include <emmintrin.h>
#endif

namespace tracktion { inline namespace core
{

/** Returns the CPU cycle count, useful for benchmarking. */
inline std::uint64_t rdtsc()
{
   #if TRACKTION_ARM && ! defined (_MSC_VER)
    std::uint64_t result;
    __asm __volatile("mrs %0, CNTVCT_EL0" : "=&r" (result));
    return result;
   #elif defined __has_builtin
    #if __has_builtin(__builtin_readcyclecounter)
     return __builtin_readcyclecounter();
    #else
     return __rdtsc();
    #endif
   #elif TRACKTION_ARM
    // TODO - supposed to use __rdpmccntr64(), but it doesn't work and no idea why...
    return 0;
   #else
    return __rdtsc();
   #endif
}

/** Pauses the CPU for an instruction.
    Can be used in constructs like spin locks to allow other cores to progress.
*/
inline void pause()
{
   #if TRACKTION_ARM && ! defined (_MSC_VER)
    __asm__ __volatile__ ("yield");
   #elif TRACKTION_ARM
    __yield();
   #elif __has_include(<emmintrin.h>)
    _mm_pause();
   #else
    static_assert (false, "Unknown platform");
   #endif
}

}} // namespace tracktion { inline namespace core
