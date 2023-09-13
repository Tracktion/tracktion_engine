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
using namespace std::literals;

#ifdef __GNUC__
 #pragma GCC diagnostic push
 #pragma GCC diagnostic ignored "-Wfloat-equal"
#endif

#include "tracktion_engine.h"

#include <tracktion_graph/tracktion_graph.h>

#include "utilities/tracktion_TestUtilities.h"

#include "playback/graph/tracktion_TracktionEngineNode.h"
#include "playback/graph/tracktion_TracktionNodePlayer.h"
#include "playback/graph/tracktion_MultiThreadedNodePlayer.h"
#include "playback/graph/tracktion_NodeRenderContext.h"
#include "playback/graph/tracktion_EditNodeBuilder.h"

#include "model/tracks/tracktion_TrackUtils.cpp"
#include "model/tracks/tracktion_Track.cpp"
#include "model/tracks/tracktion_FolderTrack.cpp"
#include "model/tracks/tracktion_AudioTrack.cpp"
#include "model/tracks/tracktion_ArrangerTrack.cpp"
#include "model/tracks/tracktion_AutomationTrack.cpp"
#include "model/tracks/tracktion_ChordTrack.cpp"
#include "model/tracks/tracktion_ClipTrack.cpp"
#include "model/tracks/tracktion_MarkerTrack.cpp"
#include "model/tracks/tracktion_MasterTrack.cpp"
#include "model/tracks/tracktion_TempoTrack.cpp"
#include "model/tracks/tracktion_EditTime.cpp"
#include "model/tracks/tracktion_EditTime.test.cpp"
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
#include "model/export/tracktion_Renderer.test.cpp"
#include "model/export/tracktion_RenderManager.cpp"
#include "model/export/tracktion_ArchiveFile.cpp"
#include "model/export/tracktion_RenderOptions.cpp"
#include "model/clips/tracktion_EditClipRenderJob.cpp"
#include "model/clips/tracktion_AudioSegmentList.cpp"
#include "audio_files/tracktion_LoopInfo.cpp"
#include "audio_files/tracktion_LoopInfo.test.cpp"

#include "project/tracktion_ProjectItemID.cpp"
#include "project/tracktion_ProjectItem.cpp"
#include "project/tracktion_Project.cpp"
#include "project/tracktion_ProjectManager.cpp"
#include "project/tracktion_ProjectSearchIndex.cpp"

#ifdef __GNUC__
 #pragma GCC diagnostic pop
#endif

#endif
