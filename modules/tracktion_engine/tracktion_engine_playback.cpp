/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
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

#if ENABLE_EXPERIMENTAL_TRACKTION_GRAPH
 #if TRACKTION_UNIT_TESTS
  #include <tracktion_graph/tracktion_graph_TestConfig.h>
 #endif

 #include <tracktion_graph/tracktion_graph.h>

 #include <tracktion_graph/tracktion_graph/tracktion_graph_tests_Utilities.h>
 #include <tracktion_graph/tracktion_graph/tracktion_graph_tests_TestNodes.h>
#endif

#include "tracktion_engine.h"

#if JUCE_LINUX || JUCE_WINDOWS
 #include <cstdarg>
#endif

#include <thread>

#include "playback/tracktion_MPEStartTrimmer.h"

#if ENABLE_EXPERIMENTAL_TRACKTION_GRAPH
 #include "playback/graph/tracktion_TracktionEngineNode.h"
 #include "playback/graph/tracktion_TracktionEngineNode.cpp"
 #include "playback/graph/tracktion_TracktionNodePlayer.h"

 #include "playback/graph/tracktion_ClickNode.h"
 #include "playback/graph/tracktion_ClickNode.cpp"

 #include "playback/graph/tracktion_CombiningNode.h"
 #include "playback/graph/tracktion_CombiningNode.cpp"

 #include "playback/graph/tracktion_FadeInOutNode.h"
 #include "playback/graph/tracktion_FadeInOutNode.cpp"

 #include "playback/graph/tracktion_InsertSendNode.h"
 #include "playback/graph/tracktion_InsertSendNode.cpp"
 #include "playback/graph/tracktion_InsertReturnNode.h"
 #include "playback/graph/tracktion_InsertReturnNode.cpp"

 #include "playback/graph/tracktion_LevelMeasurerProcessingNode.h"
 #include "playback/graph/tracktion_LevelMeasuringNode.h"
 #include "playback/graph/tracktion_LevelMeasuringNode.cpp"

 #include "playback/graph/tracktion_LiveMidiInjectingNode.h"
 #include "playback/graph/tracktion_LiveMidiInjectingNode.cpp"

 #include "playback/graph/tracktion_LiveMidiOutputNode.h"
 #include "playback/graph/tracktion_LiveMidiOutputNode.cpp"

 #include "playback/graph/tracktion_MelodyneNode.h"
 #include "playback/graph/tracktion_MelodyneNode.cpp"

 #include "playback/graph/tracktion_MidiNode.h"
 #include "playback/graph/tracktion_MidiNode.cpp"

 #include "playback/graph/tracktion_MidiOutputDeviceInstanceInjectingNode.h"
 #include "playback/graph/tracktion_MidiOutputDeviceInstanceInjectingNode.cpp"

 #include "playback/graph/tracktion_WaveNode.h"
 #include "playback/graph/tracktion_WaveNode.cpp"

 #include "playback/graph/tracktion_TrackMutingNode.h"
 #include "playback/graph/tracktion_PlayHeadPositionNode.h"
 #include "playback/graph/tracktion_PluginNode.h"
 #include "playback/graph/tracktion_ModifierNode.h"
 #include "playback/graph/tracktion_RackInstanceNode.h"
 #include "playback/graph/tracktion_RackInstanceNode.cpp"
 #include "playback/graph/tracktion_RackNode.h"
 #include "playback/graph/tracktion_RackNode.cpp"
 #include "playback/graph/tracktion_RackNode.test.cpp"
 #include "playback/graph/tracktion_RackReturnNode.h"
 #include "playback/graph/tracktion_RackReturnNode.cpp"
 #include "playback/graph/tracktion_PluginNode.cpp"
 #include "playback/graph/tracktion_ModifierNode.cpp"

 #include "playback/graph/tracktion_TrackMutingNode.cpp"

 #include "playback/graph/tracktion_TimedMutingNode.h"
 #include "playback/graph/tracktion_TimedMutingNode.cpp"

 #include "playback/graph/tracktion_TimeStretchingWaveNode.h"
 #include "playback/graph/tracktion_TimeStretchingWaveNode.cpp"

 #include "playback/graph/tracktion_TrackMidiInputDeviceNode.h"
 #include "playback/graph/tracktion_TrackMidiInputDeviceNode.cpp"

 #include "playback/graph/tracktion_TrackWaveInputDeviceNode.h"
 #include "playback/graph/tracktion_TrackWaveInputDeviceNode.cpp"

 #include "playback/graph/tracktion_SharedLevelMeasuringNode.h"
 #include "playback/graph/tracktion_SharedLevelMeasuringNode.cpp"

 #include "playback/graph/tracktion_SpeedRampWaveNode.h"
 #include "playback/graph/tracktion_SpeedRampWaveNode.cpp"

 #include "playback/graph/tracktion_MidiInputDeviceNode.h"
 #include "playback/graph/tracktion_MidiInputDeviceNode.cpp"

 #include "playback/graph/tracktion_WaveInputDeviceNode.h"
 #include "playback/graph/tracktion_WaveInputDeviceNode.cpp"

 #include "playback/graph/tracktion_EditNodeBuilder.h"
 #include "playback/graph/tracktion_EditNodeBuilder.cpp"
 #include "playback/graph/tracktion_EditNodeBuilder.test.cpp"

 #include "playback/graph/tracktion_NodeRenderContext.h"
 #include "playback/graph/tracktion_NodeRenderContext.cpp"

 #include "playback/graph/tracktion_NodeRendering.test.cpp"
 
 #include "playback/graph/tracktion_tests_WaveNode.cpp"
 #include "playback/graph/tracktion_tests_MidiNode.cpp"
 #include "playback/graph/tracktion_tests_RackNode.cpp"
#endif

using namespace juce;

#include "playback/tracktion_DeviceManager.cpp"
#include "playback/tracktion_EditPlaybackContext.cpp"
#include "playback/tracktion_EditInputDevices.cpp"
#include "playback/tracktion_LevelMeasurer.cpp"
#include "playback/tracktion_MidiNoteDispatcher.cpp"
#include "playback/tracktion_tests_TransportControl.cpp"
#include "playback/tracktion_TransportControl.cpp"
#include "playback/tracktion_AbletonLink.cpp"

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
#include "playback/devices/tracktion_WaveDeviceDescription.cpp"
#include "playback/devices/tracktion_WaveInputDevice.cpp"
#include "playback/devices/tracktion_WaveOutputDevice.cpp"

#include "playback/tracktion_HostedAudioDevice.cpp"

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
  namespace tracktion_engine
  {
    #include "Mackie/C4Translator.h"
    #include "Mackie/C4Translator.cpp"
  }
 #endif

 #include "control_surfaces/types/tracktion_AlphaTrack.cpp"
 #include "control_surfaces/types/tracktion_MackieC4.cpp"
 #include "control_surfaces/types/tracktion_MackieMCU.cpp"
 #include "control_surfaces/types/tracktion_MackieXT.cpp"
 #include "control_surfaces/types/tracktion_NovationRemoteSl.cpp"
 #include "control_surfaces/types/tracktion_RemoteSLCompact.cpp"
 #include "control_surfaces/types/tracktion_Tranzport.cpp"
#endif

#if TRACKTION_ENABLE_CONTROL_SURFACES
 #include "control_surfaces/types/tracktion_NovationAutomap.cpp"
#endif

#endif
