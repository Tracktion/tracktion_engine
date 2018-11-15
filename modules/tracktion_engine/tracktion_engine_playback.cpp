/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


#if ! JUCE_PROJUCER_LIVE_BUILD

#if TRACKTION_ENABLE_REWIRE
extern "C"
{
    #define Button CarbonButton
    #define Component CarbonComponent
    #define MemoryBlock CarbonMemoryBlock

   #if MAC
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Wdeprecated"
    #pragma clang diagnostic ignored "-Wconversion"
   #endif

    #include "ReWire/ReWireSDK/ReWire.c"
    #include "ReWire/ReWireSDK/ReWireAPI.c"
    #include "ReWire/ReWireSDK/ReWireDeviceAPI.c"

   #if MAC
    #pragma clang diagnostic pop
   #endif

    #undef Button
    #undef Component
    #undef MemoryBlock
    #undef check
}
#endif

#define JUCE_CORE_INCLUDE_JNI_HELPERS 1 // Required for Ableton Link on Android

#include "tracktion_engine.h"

#if JUCE_LINUX || JUCE_WINDOWS
 #include <cstdarg>
#endif

namespace tracktion_engine
{

using namespace juce;

#include "playback/tracktion_DeviceManager.cpp"
#include "playback/tracktion_EditPlaybackContext.cpp"
#include "playback/tracktion_EditInputDevices.cpp"
#include "playback/tracktion_LevelMeasurer.cpp"
#include "playback/tracktion_MidiNoteDispatcher.cpp"
#include "playback/tracktion_TransportControl.cpp"
#include "playback/tracktion_AbletonLink.cpp"

#include "playback/tracktion_MPEStartTrimmer.h"

#include "playback/audionodes/tracktion_AudioNode.cpp"
#include "playback/audionodes/tracktion_BufferingAudioNode.cpp"
#include "playback/audionodes/tracktion_ClickNode.cpp"
#include "playback/audionodes/tracktion_CombiningAudioNode.cpp"
#include "playback/audionodes/tracktion_FadeInOutAudioNode.cpp"
#include "playback/audionodes/tracktion_HissingAudioNode.cpp"
#include "playback/audionodes/tracktion_MidiAudioNode.cpp"
#include "playback/audionodes/tracktion_MixerAudioNode.cpp"
#include "playback/audionodes/tracktion_PlayHeadAudioNode.cpp"
#include "playback/audionodes/tracktion_WaveAudioNode.cpp"

#include "playback/devices/tracktion_InputDevice.cpp"
#include "playback/devices/tracktion_MidiInputDevice.cpp"
#include "playback/devices/tracktion_PhysicalMidiInputDevice.cpp"
#include "playback/devices/tracktion_VirtualMidiInputDevice.cpp"
#include "playback/devices/tracktion_MidiOutputDevice.cpp"
#include "playback/devices/tracktion_OutputDevice.cpp"
#include "playback/devices/tracktion_WaveInputDevice.cpp"
#include "playback/devices/tracktion_WaveOutputDevice.cpp"

static inline void sprintf (char* dest, size_t maxLength, const char* format, ...) noexcept
{
    va_list list;
    va_start (list, format);

   #if JUCE_WINDOWS
    _vsnprintf (dest, maxLength, format, list);
   #else
    vsnprintf (dest, maxLength, format, list);
   #endif
}

#include "control_surfaces/tracktion_ControlSurface.cpp"
#include "control_surfaces/tracktion_ExternalControllerManager.cpp"
#include "control_surfaces/tracktion_ExternalController.cpp"
#include "control_surfaces/tracktion_CustomControlSurface.cpp"

#if TRACKTION_ENABLE_CONTROL_SURFACES
 #if TRACKTION_ENABLE_CONTROL_SURFACE_MACKIEC4
  #include "Mackie/C4Translator.h"
  #include "Mackie/C4Translator.cpp"
 #endif

 #include "control_surfaces/types/tracktion_AlphaTrack.cpp"
 #include "control_surfaces/types/tracktion_MackieC4.cpp"
 #include "control_surfaces/types/tracktion_MackieMCU.cpp"
 #include "control_surfaces/types/tracktion_MackieXT.cpp"
 #include "control_surfaces/types/tracktion_NovationRemoteSl.cpp"
 #include "control_surfaces/types/tracktion_RemoteSLCompact.cpp"
 #include "control_surfaces/types/tracktion_Tranzport.cpp"
#endif

}

#if TRACKTION_ENABLE_CONTROL_SURFACES
 #include "control_surfaces/types/tracktion_NovationAutomap.cpp"
#endif

#endif
