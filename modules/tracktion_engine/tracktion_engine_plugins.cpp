/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#if ! JUCE_PROJUCER_LIVE_BUILD

#ifdef __APPLE__
 #define Point CarbonDummyPointName
 #define AudioBuffer DummyAudioBufferName
 #include <AudioUnit/AudioUnit.h>
 #include <AudioUnit/AUComponent.h>
 #undef AudioBuffer
 #undef Point
#endif

#include <atomic>
#include <numeric>
#include <chrono>
using namespace std::literals;

#if TRACKTION_UNIT_TESTS
 #include <tracktion_core/tracktion_TestConfig.h>
#endif

#include <tracktion_graph/tracktion_graph.h>

#include <tracktion_graph/tracktion_graph/tracktion_TestUtilities.h>
#include <tracktion_graph/tracktion_graph/tracktion_TestNodes.h>

#include "tracktion_engine.h"

#include "playback/graph/tracktion_TracktionEngineNode.h"
#include "playback/graph/tracktion_PluginNode.h"
#include "playback/graph/tracktion_TrackMutingNode.h"
#include "playback/graph/tracktion_RackNode.h"

#include "playback/audionodes/tracktion_AudioNode.h"

#include "model/automation/modifiers/tracktion_ModifierInternal.h"

#include "plugins/tracktion_Plugin.cpp"
#include "plugins/tracktion_PluginList.cpp"
#include "plugins/tracktion_PluginManager.cpp"
#include "plugins/tracktion_PluginWindowState.cpp"

#include "plugins/external/tracktion_ExternalAutomatableParameter.h"
#include "plugins/external/tracktion_ExternalPluginBlacklist.h"
#include "plugins/external/tracktion_ExternalPlugin.cpp"

#include "plugins/internal/tracktion_AuxReturn.cpp"
#include "plugins/internal/tracktion_AuxSend.cpp"
#include "plugins/internal/tracktion_FreezePoint.cpp"
#include "plugins/internal/tracktion_InsertPlugin.cpp"
#include "plugins/internal/tracktion_LevelMeter.cpp"
#include "plugins/internal/tracktion_RackInstance.cpp"
#include "plugins/internal/tracktion_RackType.cpp"
#include "plugins/internal/tracktion_ReWirePlugin.cpp"
#include "plugins/internal/tracktion_TextPlugin.cpp"
#include "plugins/internal/tracktion_VCA.cpp"
#include "plugins/internal/tracktion_VolumeAndPan.cpp"
#include "plugins/internal/tracktion_InternalPlugins.test.cpp"

#include "plugins/effects/tracktion_Chorus.cpp"
#include "plugins/effects/tracktion_Compressor.cpp"
#include "plugins/effects/tracktion_Delay.cpp"
#include "plugins/effects/tracktion_FourOscPlugin.cpp"
#include "plugins/effects/tracktion_LatencyPlugin.cpp"
#include "plugins/effects/tracktion_Equaliser.cpp"
#include "plugins/effects/tracktion_ImpulseResponsePlugin.cpp"
#include "plugins/effects/tracktion_LowPass.cpp"
#include "plugins/effects/tracktion_MidiModifier.cpp"
#include "plugins/effects/tracktion_MidiPatchBay.cpp"
#include "plugins/effects/tracktion_PatchBay.cpp"
#include "plugins/effects/tracktion_Phaser.cpp"
#include "plugins/effects/tracktion_PitchShift.cpp"
#include "plugins/effects/tracktion_Reverb.cpp"
#include "plugins/effects/tracktion_SamplerPlugin.cpp"
#include "plugins/effects/tracktion_ToneGenerator.cpp"

#include "plugins/ARA/tracktion_MelodyneFileReader.cpp"

#include "plugins/tracktion_Plugins.test.cpp"

#endif
