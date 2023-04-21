/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#if ! JUCE_PROJUCER_LIVE_BUILD


#if TRACKTION_ENABLE_ABLETON_LINK
 #include <juce_core/system/juce_TargetPlatform.h>

 // Ableton Link has to be included here before ReWire as ReWire seems to mess with some Windows defs
 #if (JUCE_WINDOWS || JUCE_MAC || JUCE_LINUX || JUCE_ANDROID)
     #if JUCE_MAC
      #define LINK_PLATFORM_MACOSX  1
     #endif

     #if JUCE_WINDOWS
      #define LINK_PLATFORM_WINDOWS 1
     #endif

     #if JUCE_LINUX || JUCE_ANDROID
      #define LINK_PLATFORM_LINUX 1
     #endif

     //==========================================================================
     // To use Link on desktop and Android, grab the open source repo from github
     // and add these folders to your project's header search paths:
     // [relative path from project]/AbletonLink/include
     // [relative path from project]/AbletonLink/modules/asio-standalone/asio/include
     //
     // If you're building on Android, you will also need the ifaddrs header and source
     // from here: https://github.com/libpd/abl_link/tree/master/external/android-ifaddrs
     // Add the folder they are in to your header search paths also.

     #if JUCE_CLANG // TODO: Ignore conversion errors on Windows too
      #pragma clang diagnostic push
      #pragma clang diagnostic ignored "-Wconversion"
      #pragma clang diagnostic ignored "-Wshadow"
      #pragma clang diagnostic ignored "-Wdeprecated-declarations"
     #endif

     #if JUCE_ANDROID
      #include <ifaddrs.h>
     #endif

     #include <ableton/Link.hpp>
     #include <ableton/link/HostTimeFilter.hpp>

     #if JUCE_ANDROID
      #include <ifaddrs.cpp>
     #endif

     #if JUCE_CLANG
      #pragma clang diagnostic pop
     #endif

    #undef NOMINMAX
#endif
#endif

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


//==============================================================================
//==============================================================================
#if TRACKTION_UNIT_TESTS
 #include <tracktion_core/tracktion_TestConfig.h>
#endif

#include <tracktion_graph/tracktion_graph.h>

#include <tracktion_graph/tracktion_graph/tracktion_TestUtilities.h>
#include <tracktion_graph/tracktion_graph/tracktion_TestNodes.h>
#include <tracktion_graph/tracktion_graph/tracktion_PlayHead.h>

//==============================================================================
//==============================================================================
#include "tracktion_engine.h"


//==============================================================================
//==============================================================================
#if __has_include(<samplerate.h>)
 #include <samplerate.h>
#else

#undef VERSION
#define PACKAGE ""
#define VERSION "0.1.9"
#define CPU_CLIPS_NEGATIVE 0
#define CPU_CLIPS_POSITIVE 0

#include "../3rd_party/choc/platform/choc_DisableAllWarnings.h"

#if __GNUC__
 #pragma GCC diagnostic ignored "-Wpedantic"
#endif

extern "C"
{
    #include "../3rd_party/libsamplerate/samplerate.h"
}

#if TRACKTION_BUILD_LIBSAMPLERATE
 extern "C"
 {
     #include "../3rd_party/libsamplerate/src_linear.c"
     #include "../3rd_party/libsamplerate/src_sinc.c"
     #include "../3rd_party/libsamplerate/src_zoh.c"
     #include "../3rd_party/libsamplerate/samplerate.c"
 }
#endif //TRACKTION_BUILD_LIBSAMPLERATE

#undef PACKAGE
#undef VERSION
#undef CPU_CLIPS_NEGATIVE
#undef CPU_CLIPS_POSITIVE

#include "../3rd_party/choc/platform/choc_ReenableAllWarnings.h"

#endif //__has_include(<samplerate.h>)


//==============================================================================
#if JUCE_LINUX || JUCE_WINDOWS
 #include <cstdarg>
#endif

#include <thread>
using namespace std::literals;

#include "playback/tracktion_MPEStartTrimmer.h"

#include "playback/graph/tracktion_TracktionEngineNode.h"
#include "playback/graph/tracktion_TracktionEngineNode.cpp"
#include "playback/graph/tracktion_TracktionNodePlayer.h"
#include "playback/graph/tracktion_MultiThreadedNodePlayer.h"

#include "playback/graph/tracktion_BenchmarkUtilities.h"

#include "playback/graph/tracktion_TrackMutingNode.h"

#include "playback/graph/tracktion_AuxSendNode.h"
#include "playback/graph/tracktion_AuxSendNode.cpp"

#include "playback/graph/tracktion_ClickNode.h"
#include "playback/graph/tracktion_ClickNode.cpp"

#include "playback/graph/tracktion_CombiningNode.h"
#include "playback/graph/tracktion_CombiningNode.cpp"

#include "playback/graph/tracktion_FadeInOutNode.h"
#include "playback/graph/tracktion_FadeInOutNode.cpp"

#include "playback/graph/tracktion_PluginNode.h"

#include "playback/graph/tracktion_InsertSendNode.h"
#include "playback/graph/tracktion_InsertReturnNode.h"
#include "playback/graph/tracktion_InsertSendNode.cpp"
#include "playback/graph/tracktion_InsertReturnNode.cpp"

#include "playback/graph/tracktion_LevelMeasurerProcessingNode.h"
#include "playback/graph/tracktion_LevelMeasuringNode.h"
#include "playback/graph/tracktion_LevelMeasuringNode.cpp"

#include "playback/graph/tracktion_LiveMidiInjectingNode.h"
#include "playback/graph/tracktion_LiveMidiInjectingNode.cpp"

#include "playback/graph/tracktion_LiveMidiOutputNode.h"
#include "playback/graph/tracktion_LiveMidiOutputNode.cpp"

#include "playback/graph/tracktion_LoopingMidiNode.h"
#include "playback/graph/tracktion_LoopingMidiNode.cpp"
#include "playback/graph/tracktion_LoopingMidiNode.test.cpp"

#include "playback/graph/tracktion_MelodyneNode.h"
#include "playback/graph/tracktion_MelodyneNode.cpp"

#include "playback/graph/tracktion_MidiNode.h"
#include "playback/graph/tracktion_MidiNode.cpp"

#include "playback/graph/tracktion_MidiOutputDeviceInstanceInjectingNode.h"
#include "playback/graph/tracktion_MidiOutputDeviceInstanceInjectingNode.cpp"

#include "playback/graph/tracktion_WaveNode.h"
#include "playback/graph/tracktion_WaveNode.cpp"

#include "playback/graph/tracktion_PlayHeadPositionNode.h"
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

#include "playback/graph/tracktion_HostedMidiInputDeviceNode.h"
#include "playback/graph/tracktion_HostedMidiInputDeviceNode.cpp"

#include "playback/graph/tracktion_WaveInputDeviceNode.h"
#include "playback/graph/tracktion_WaveInputDeviceNode.cpp"

#include "playback/graph/tracktion_EditNodeBuilder.h"
#include "playback/graph/tracktion_EditNodeBuilder.cpp"
#include "playback/graph/tracktion_EditNodeBuilder.test.cpp"

#include "playback/graph/tracktion_NodeRenderContext.h"
#include "playback/graph/tracktion_NodeRenderContext.cpp"

#include "playback/graph/tracktion_NodeRendering.test.cpp"

#include "playback/graph/tracktion_WaveNode.test.cpp"
#include "playback/graph/tracktion_MidiNode.test.cpp"
#include "playback/graph/tracktion_RackBenchmarks.test.cpp"

#include "playback/tracktion_DeviceManager.cpp"
#include "playback/tracktion_EditPlaybackContext.cpp"
#include "playback/tracktion_EditInputDevices.cpp"
#include "playback/tracktion_LevelMeasurer.cpp"
#include "playback/tracktion_MidiNoteDispatcher.cpp"
#include "playback/tracktion_TransportControl.test.cpp"
#include "playback/tracktion_TransportControl.cpp"
#include "playback/tracktion_AbletonLink.cpp"

#include "playback/audionodes/tracktion_AudioNode.h"
#include "playback/audionodes/tracktion_CombiningAudioNode.h"
#include "playback/audionodes/tracktion_WaveAudioNode.h"
#include "playback/audionodes/tracktion_TrackCompAudioNode.h"
#include "playback/audionodes/tracktion_SpeedRampAudioNode.h"
#include "playback/audionodes/tracktion_PluginAudioNode.h"
#include "playback/audionodes/tracktion_FadeInOutAudioNode.h"

#include "playback/audionodes/tracktion_AudioNode.cpp"
#include "playback/audionodes/tracktion_FadeInOutAudioNode.cpp"
#include "playback/audionodes/tracktion_WaveAudioNode.cpp"
#include "playback/audionodes/tracktion_CombiningAudioNode.cpp"

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

#ifdef JUCE_MSVC
 #pragma warning (disable: 4996)
#endif

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
  namespace tracktion { inline namespace engine
  {
    #include "Mackie/C4Translator.h"
    #include "Mackie/C4Translator.cpp"
  }} // namespace tracktion { inline namespace engine
 #endif

 #include "control_surfaces/types/tracktion_AlphaTrack.cpp"
 #if TRACKTION_ENABLE_CONTROL_SURFACE_MACKIEC4
  #include "control_surfaces/types/tracktion_MackieC4.cpp"
 #endif
 #include "control_surfaces/types/tracktion_MackieMCU.cpp"
 #include "control_surfaces/types/tracktion_MackieXT.cpp"
 #include "control_surfaces/types/tracktion_IconProG2.cpp"
 #include "control_surfaces/types/tracktion_NovationRemoteSl.cpp"
 #include "control_surfaces/types/tracktion_RemoteSLCompact.cpp"
 #include "control_surfaces/types/tracktion_Tranzport.cpp"
#endif

#if TRACKTION_ENABLE_CONTROL_SURFACES
 #include "control_surfaces/types/tracktion_NovationAutomap.cpp"
#endif

#endif
