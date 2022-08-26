/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#if ! JUCE_PROJUCER_LIVE_BUILD

#include "tracktion_engine.h"

#if TRACKTION_ENABLE_TIMESTRETCH_ELASTIQUE
 #ifdef __GNUC__
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wextra-semi"
 #endif

  // If you get a build error here you'll need to add the Elastique SDK to your header search path!
  #include "elastique_pro/incl/elastiqueProV3API.h"

 #ifdef __GNUC__
  #pragma GCC diagnostic pop
 #endif
#endif

#ifdef __clang__
 #pragma clang diagnostic push
 #pragma clang diagnostic ignored "-Wsign-conversion"
 #pragma clang diagnostic ignored "-Wunused-parameter"
 #pragma clang diagnostic ignored "-Wshadow"
 #pragma clang diagnostic ignored "-Wshadow-all"
 #pragma clang diagnostic ignored "-Wmacro-redefined"
 #pragma clang diagnostic ignored "-Wconversion"
 #pragma clang diagnostic ignored "-Wunused"
 #pragma clang diagnostic ignored "-Wcast-align"
 #if __has_warning("-Wzero-as-null-pointer-constant")
  #pragma clang diagnostic ignored "-Wzero-as-null-pointer-constant"
 #endif
 #pragma clang diagnostic ignored "-Wextra-semi"
 #pragma clang diagnostic ignored "-Wextra-semi"
 #pragma clang diagnostic ignored "-Wmissing-prototypes"
#endif

#ifdef __GNUC__
 #pragma GCC diagnostic push
 #pragma GCC diagnostic ignored "-Wsign-conversion"
 #pragma GCC diagnostic ignored "-Wshadow"
 #if ! __clang__
  #pragma GCC diagnostic ignored "-Wunused-but-set-variable"
  #pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
 #endif
 #pragma GCC diagnostic ignored "-Wunused-variable"
 #pragma GCC diagnostic ignored "-Wunused-parameter"
 #pragma GCC diagnostic ignored "-Wpedantic"
 #pragma GCC diagnostic ignored "-Wunknown-pragmas"
#endif

#ifdef JUCE_MSVC
 #pragma warning (push)
 #pragma warning (disable: 4005 4189 4189 4267 4702 4458 4100)
#endif

#include "timestretch/tracktion_TimeStretch.cpp"
#include "timestretch/tracktion_TimeStretch.test.cpp"

namespace tracktion { inline namespace engine
{
    #include "3rd_party/soundtouch/source/SoundTouch/BPMDetect.cpp"
    #undef max
    #include "3rd_party/soundtouch/source/SoundTouch/PeakFinder.cpp"
    #undef max
    #include "3rd_party/soundtouch/source/SoundTouch/FIFOSampleBuffer.cpp"

   #if TRACKTION_ENABLE_TIMESTRETCH_SOUNDTOUCH
    #include "3rd_party/soundtouch/source/SoundTouch/AAFilter.cpp"
    #undef PI
    #include "3rd_party/soundtouch/source/SoundTouch/cpu_detect_x86.cpp"
    #include "3rd_party/soundtouch/source/SoundTouch/FIRFilter.cpp"
    #include "3rd_party/soundtouch/source/SoundTouch/InterpolateCubic.cpp"
    #include "3rd_party/soundtouch/source/SoundTouch/InterpolateLinear.cpp"
    #include "3rd_party/soundtouch/source/SoundTouch/InterpolateShannon.cpp"
    #include "3rd_party/soundtouch/source/SoundTouch/mmx_optimized.cpp"
    #include "3rd_party/soundtouch/source/SoundTouch/RateTransposer.cpp"
    #include "3rd_party/soundtouch/source/SoundTouch/SoundTouch.cpp"
    #include "3rd_party/soundtouch/source/SoundTouch/sse_optimized.cpp"
    #include "3rd_party/soundtouch/source/SoundTouch/TDStretch.cpp"
   #endif
}} // namespace tracktion { inline namespace engine

#ifdef JUCE_MSVC
 #pragma warning (pop)
#endif

#ifdef __clang__
 #pragma clang diagnostic pop
#endif

#ifdef __GNUC__
 #pragma GCC diagnostic pop
#endif

#endif
