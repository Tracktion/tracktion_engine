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
