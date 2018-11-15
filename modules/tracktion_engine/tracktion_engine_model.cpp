/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


#if ! JUCE_PROJUCER_LIVE_BUILD

#include <future>

#include "tracktion_engine.h"

namespace tracktion_engine
{

using namespace juce;

#include "timestretch/tracktion_TempoDetect.h"
#include "model/automation/modifiers/tracktion_ModifierInternal.h"

#include "model/edit/tracktion_OldEditConversion.h"
#include "model/edit/tracktion_EditItem.cpp"
#include "model/edit/tracktion_Edit.cpp"
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

#include "model/clips/tracktion_AudioClipBase.cpp"
#include "model/clips/tracktion_CompManager.cpp"
#include "model/clips/tracktion_WaveAudioClip.cpp"
#include "model/clips/tracktion_ChordClip.cpp"
#include "model/clips/tracktion_EditClip.cpp"
#include "model/clips/tracktion_MarkerClip.cpp"
#include "model/clips/tracktion_CollectionClip.cpp"
#include "model/clips/tracktion_MidiClip.cpp"
#include "model/clips/tracktion_StepClipChannel.cpp"
#include "model/clips/tracktion_StepClipPattern.cpp"
#include "model/clips/tracktion_StepClip.cpp"
#include "model/clips/tracktion_ClipEffects.cpp"
#include "model/clips/tracktion_WarpTimeManager.cpp"

#include "model/tracks/tracktion_TrackUtils.cpp"
#include "model/tracks/tracktion_Track.cpp"
#include "model/tracks/tracktion_FolderTrack.cpp"
#include "model/tracks/tracktion_AudioTrack.cpp"
#include "model/tracks/tracktion_AutomationTrack.cpp"
#include "model/tracks/tracktion_ChordTrack.cpp"
#include "model/tracks/tracktion_ClipTrack.cpp"
#include "model/tracks/tracktion_MarkerTrack.cpp"
#include "model/tracks/tracktion_TempoTrack.cpp"
#include "model/tracks/tracktion_EditTimeRange.cpp"
#include "model/tracks/tracktion_TrackItem.cpp"
#include "model/tracks/tracktion_TrackOutput.cpp"
#include "model/tracks/tracktion_TrackCompManager.cpp"

#include "model/edit/tracktion_GrooveTemplate.cpp"
#include "model/edit/tracktion_MarkerManager.cpp"
#include "model/edit/tracktion_PitchSequence.cpp"
#include "model/edit/tracktion_PitchSetting.cpp"
#include "model/edit/tracktion_QuantisationType.cpp"
#include "model/edit/tracktion_TempoSequence.cpp"
#include "model/edit/tracktion_TempoSetting.cpp"
#include "model/edit/tracktion_TimecodeDisplayFormat.cpp"
#include "model/edit/tracktion_TimeSigSetting.cpp"
#include "model/edit/tracktion_EditSnapshot.cpp"
#include "model/edit/tracktion_EditFileOperations.cpp"
#include "model/edit/tracktion_EditInsertPoint.cpp"

#include "model/export/tracktion_Exportable.cpp"
#include "model/export/tracktion_ExportJob.cpp"
#include "model/export/tracktion_Renderer.cpp"
#include "model/export/tracktion_RenderManager.cpp"
#include "model/export/tracktion_ArchiveFile.cpp"
#include "model/export/tracktion_RenderOptions.cpp"
#include "model/clips/tracktion_EditClipRenderJob.cpp"
#include "model/clips/tracktion_AudioSegmentList.cpp"
#include "audio_files/tracktion_LoopInfo.cpp"

#include "project/tracktion_ProjectItemID.cpp"
#include "project/tracktion_ProjectItem.cpp"
#include "project/tracktion_Project.cpp"
#include "project/tracktion_ProjectManager.cpp"
#include "project/tracktion_ProjectSearchIndex.cpp"

}

#endif
