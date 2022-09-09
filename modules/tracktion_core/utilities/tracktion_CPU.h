/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#ifdef _WIN32
 #include <intrin.h>
#else
 #include <x86intrin.h>
#endif

#if __has_include(<emmintrin.h>)
 #include <emmintrin.h>
#endif

namespace tracktion { inline namespace core
{

/** Returns the CPU cycle count, useful for benchmarking. */
inline std::uint64_t rdtsc()
{
    return __rdtsc();
}

/** Pauses the CPU for an instruction.
    Can be used in constructs like spin locks to allow other cores to progress.
*/
inline void pause()
{
   #if __has_include(<emmintrin.h>)
    _mm_pause();
   #else
    __asm__ __volatile__ ("yield");
   #endif
}

}} // namespace tracktion { inline namespace core
