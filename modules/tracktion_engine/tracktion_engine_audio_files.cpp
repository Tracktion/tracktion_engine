/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/

#if ! JUCE_PROJUCER_LIVE_BUILD

extern "C"
{
   #if TRACKTION_ENABLE_REX
    #define Button CarbonButton
    #define Component CarbonComponent
    #define MemoryBlock CarbonMemoryBlock
    #include "REX/REX.c"
    #undef Button
    #undef Component
    #undef MemoryBlock
    #undef check
   #endif
}

#include "tracktion_engine.h"

#include <string>

namespace tracktion_engine
{

#include "audio_files/formats/tracktion_FloatAudioFileFormat.cpp"
#include "audio_files/formats/tracktion_RexFileFormat.cpp"
#include "audio_files/formats/tracktion_LAMEManager.cpp"

#include "audio_files/tracktion_Thumbnail.cpp"
#include "audio_files/tracktion_AudioFileCache.cpp"
#include "audio_files/tracktion_AudioFile.cpp"
#include "audio_files/tracktion_AudioFileUtils.cpp"
#include "audio_files/tracktion_AudioFormatManager.cpp"

#include "midi/tracktion_MidiList.cpp"
#include "midi/tracktion_MidiProgramManager.cpp"
#include "midi/tracktion_Musicality.cpp"
#include "midi/tracktion_SelectedMidiEvents.cpp"

}

#endif
