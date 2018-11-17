/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


#if ! JUCE_PROJUCER_LIVE_BUILD

#include "tracktion_engine.h"

#if TRACKTION_ENABLE_TIMESTRETCH_ELASTIQUE
 // If you get a build error here you'll need to add the Elastique SDK to your header search path!
 #include "elastique_pro/incl/elastiqueProV3API.h"
#endif

namespace tracktion_engine
{

using namespace juce;

#ifdef __clang__
 #pragma clang diagnostic push
 #pragma clang diagnostic ignored "-Wsign-conversion"
 #pragma clang diagnostic ignored "-Wunused-parameter"
 #pragma clang diagnostic ignored "-Wshadow"
 #pragma clang diagnostic ignored "-Wmacro-redefined"
 #pragma clang diagnostic ignored "-Wconversion"
 #pragma clang diagnostic ignored "-Wunused"
#endif

#ifdef JUCE_MSVC
 #pragma warning (push)
 #pragma warning (disable: 4005 4189 4189 4267 4702 4458 4100)
#endif

#include "timestretch/tracktion_TimeStretch.cpp"

#include "3rd_party/soundtouch/source/SoundTouch/BPMDetect.cpp"
#include "3rd_party/soundtouch/source/SoundTouch/PeakFinder.cpp"
#include "3rd_party/soundtouch/source/SoundTouch/FIFOSampleBuffer.cpp"

#if TRACKTION_ENABLE_TIMESTRETCH_SOUNDTOUCH
 #include "3rd_party/soundtouch/source/SoundTouch/AAFilter.cpp"
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

#ifdef JUCE_MSVC
 #pragma warning (pop)
#endif

#ifdef __clang__
 #pragma clang diagnostic pop
#endif

}

#endif
