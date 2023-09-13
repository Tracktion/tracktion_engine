/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#if ! JUCE_PROJUCER_LIVE_BUILD

#include <future>
#include <chrono>

using namespace std::literals;

#ifdef __GNUC__
 #pragma GCC diagnostic push
 #pragma GCC diagnostic ignored "-Wfloat-equal"
#endif

#include "../../modules/tracktion_graph/tracktion_graph.h"
#include "../../modules/tracktion_core/utilities/tracktion_Benchmark.h"

#include "tracktion_engine.h"

#include "../../modules/tracktion_graph/tracktion_graph/tracktion_TestUtilities.h"

#include "timestretch/tracktion_TempoDetect.h"
#include "model/automation/modifiers/tracktion_ModifierInternal.h"

#include "model/edit/tracktion_OldEditConversion.h"
#include "model/edit/tracktion_EditItem.cpp"
#include "model/edit/tracktion_Edit.cpp"
#include "model/edit/tracktion_Edit.test.cpp"
#include "model/edit/tracktion_EditUtilities.cpp"
#include "model/edit/tracktion_SourceFileReference.cpp"
#include "model/clips/tracktion_Clip.cpp"

#include "model/automation/tracktion_AutomatableEditItem.cpp"
#include "model/automation/tracktion_AutomatableParameter.cpp"
#include "model/automation/tracktion_MacroParameter.cpp"
#include "model/automation/tracktion_AutomationCurve.cpp"
#include "model/automation/tracktion_AutomationRecordManager.cpp"
#include "model/automation/tracktion_MidiLearn.cpp"
#include "model/automation/tracktion_ParameterChangeHandler.cpp"
#include "model/automation/tracktion_ParameterControlMappings.cpp"
#include "model/automation/tracktion_Modifier.cpp"
#include "model/automation/modifiers/tracktion_ModifierCommon.cpp"
#include "model/automation/modifiers/tracktion_BreakpointOscillatorModifier.cpp"
#include "model/automation/modifiers/tracktion_EnvelopeFollowerModifier.cpp"
#include "model/automation/modifiers/tracktion_LFOModifier.cpp"
#include "model/automation/modifiers/tracktion_MIDITrackerModifier.cpp"
#include "model/automation/modifiers/tracktion_RandomModifier.cpp"
#include "model/automation/modifiers/tracktion_StepModifier.cpp"

#include "model/clips/tracktion_ArrangerClip.cpp"
#include "model/clips/tracktion_AudioClipBase.cpp"
#include "model/clips/tracktion_CompManager.cpp"
#include "model/clips/tracktion_WaveAudioClip.cpp"
#include "model/clips/tracktion_ChordClip.cpp"
#include "model/clips/tracktion_EditClip.cpp"
#include "model/clips/tracktion_MarkerClip.cpp"
#include "model/clips/tracktion_CollectionClip.cpp"
#include "model/clips/tracktion_ContainerClip.cpp"
#include "model/clips/tracktion_ContainerClip.test.cpp"
#include "model/clips/tracktion_MidiClip.cpp"
#include "model/clips/tracktion_MidiClip.test.cpp"
#include "model/clips/tracktion_StepClipChannel.cpp"
#include "model/clips/tracktion_StepClipPattern.cpp"
#include "model/clips/tracktion_StepClip.cpp"
#include "model/clips/tracktion_ClipEffects.cpp"
#include "model/clips/tracktion_ClipOwner.cpp"
#include "model/clips/tracktion_WarpTimeManager.cpp"

#ifdef __GNUC__
 #pragma GCC diagnostic pop
#endif

#endif
