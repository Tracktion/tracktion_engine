/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#pragma once

#ifdef _WIN32
 #include <intrin.h>
#elif defined (__arm__) || defined (__arm64__) || defined (__aarch64__)
 // Use clang built-in
#else
 #include <x86intrin.h>
#endif

#if defined (__arm__) || defined (__arm64__) || defined (__aarch64__)
 // Use asm yield
#elif __has_include(<emmintrin.h>)
 #include <emmintrin.h>
#endif

namespace tracktion { inline namespace core
{

/** Returns the CPU cycle count, useful for benchmarking. */
inline std::uint64_t rdtsc()
{
   #if defined __has_builtin
    #if __has_builtin(__builtin_readcyclecounter)
     return __builtin_readcyclecounter();
    #else
     return __rdtsc();
    #endif
   #else
    return __rdtsc();
   #endif
}

/** Pauses the CPU for an instruction.
    Can be used in constructs like spin locks to allow other cores to progress.
*/
inline void pause()
{
   #if defined (__arm__) || defined (__arm64__) || defined (__aarch64__)
    __asm__ __volatile__ ("yield");
   #elif __has_include(<emmintrin.h>)
    _mm_pause();
   #else
    static_assert (false, "Unknown platform");
   #endif
}

}} // namespace tracktion { inline namespace core
