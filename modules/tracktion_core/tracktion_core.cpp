/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#ifdef TRACKTION_CORE_H_INCLUDED
 /* When you add this cpp file to your project, you mustn't include it in a file where you've
    already included any other headers - just put it inside a file on its own, possibly with your config
    flags preceding it, but don't include anything else. That also includes avoiding any automatic prefix
    header files that the compiler may be using.
 */
 #error "Incorrect use of JUCE cpp file"
#endif


//==============================================================================
#include <juce_audio_formats/juce_audio_formats.h>

using namespace std::literals;

//==============================================================================
#include "tracktion_TestConfig.h"
#include "utilities/tracktion_Benchmark.h"

//==============================================================================
#include "tracktion_core.h"

//==============================================================================
#include "utilities/tracktion_Tempo.test.cpp"
#include "utilities/tracktion_Time.test.cpp"
#include "utilities/tracktion_TimeRange.test.cpp"
