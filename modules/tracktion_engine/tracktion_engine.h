/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/

/*******************************************************************************
 The block below describes the properties of this module, and is read by
 the Projucer to automatically generate project code that uses it.
 For details about the syntax and how to create or use a module, see the
 JUCE Module Format.txt file.


 BEGIN_JUCE_MODULE_DECLARATION

  ID:               tracktion_engine
  vendor:           Tracktion Corporation
  version:          1.0.0
  name:             The Tracktion audio engine
  description:      Classes for manipulating and playing Tracktion projects
  website:          http://www.tracktion.com
  license:          Proprietary

  dependencies:     juce_audio_devices juce_audio_utils juce_gui_extra juce_dsp

 END_JUCE_MODULE_DECLARATION

*******************************************************************************/

#pragma once

#if ! JUCE_PROJUCER_LIVE_BUILD

#include <sys/stat.h>
#include <memory>
#include <map>
#include <set>
#include <unordered_map>
#include <atomic>

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_dsp/juce_dsp.h>

#undef __TEXT

/** Config: TRACKTION_ENABLE_ARA
    Enables ARA support.
    You must have the ARA SDK in the search path in order to use this.
*/
#ifndef TRACKTION_ENABLE_ARA
 #define TRACKTION_ENABLE_ARA 0
#endif

/** Config: TRACKTION_ENABLE_REWIRE
    Enables ReWire support.
 */
#ifndef TRACKTION_ENABLE_REWIRE
 #define TRACKTION_ENABLE_REWIRE 0
#endif

/** Config: TRACKTION_ENABLE_AUTOMAP
    Enables Novation Automap controller support.
*/
#ifndef TRACKTION_ENABLE_AUTOMAP
 #define TRACKTION_ENABLE_AUTOMAP 0
#endif

/** Config: TRACKTION_ENABLE_VIDEO
    Enables video support.
*/
#ifndef TRACKTION_ENABLE_VIDEO
 #define TRACKTION_ENABLE_VIDEO 0
#endif

/** Config: TRACKTION_ENABLE_REX
    Enables Rex audio file support.
*/
#ifndef TRACKTION_ENABLE_REX
 #define TRACKTION_ENABLE_REX 0
#endif

/** Config: TRACKTION_ENABLE_CONTROL_SURFACES
    Enables external control surface support.
*/
#ifndef TRACKTION_ENABLE_CONTROL_SURFACES
 #define TRACKTION_ENABLE_CONTROL_SURFACES 0
#endif

/** Config: TRACKTION_ENABLE_CONTROL_SURFACE_MACKIEC4
    Enables support for the C4 control surface. If enabled, you'll also need
    to ensure the C4 Translator source files are in the header search path.
*/
#ifndef TRACKTION_ENABLE_CONTROL_SURFACE_MACKIEC4
 #define TRACKTION_ENABLE_CONTROL_SURFACE_MACKIEC4 0
#endif

/** Config: TRACKTION_ENABLE_TIMESTRETCH_ELASTIQUE
    Enables time-stretching with the Elastique library.
    You must have Elastique in your search path and link to it on mac/linux if you enable this.
*/
#ifndef TRACKTION_ENABLE_TIMESTRETCH_ELASTIQUE
 #define TRACKTION_ENABLE_TIMESTRETCH_ELASTIQUE 0
#endif

/** Config: TRACKTION_ENABLE_TIMESTRETCH_SOUNDTOUCH
    Enables time-stretching with the SoundTouch library.
*/
#ifndef TRACKTION_ENABLE_TIMESTRETCH_SOUNDTOUCH
 #define TRACKTION_ENABLE_TIMESTRETCH_SOUNDTOUCH 0
#endif

/** Config: TRACKTION_ENABLE_ABLETON_LINK
    Enables Ableton Link support.
    You must have Link in your search path if you enable this.
*/
#ifndef TRACKTION_ENABLE_ABLETON_LINK
 #define TRACKTION_ENABLE_ABLETON_LINK 0
#endif

/** Config: TRACKTION_UNIT_TESTS
    Enables Tracktion unit tests.
    If enabled, these will be added the UnitTestRunners under the "Tracktion" category.
*/
#ifndef TRACKTION_UNIT_TESTS
 #define TRACKTION_UNIT_TESTS 0
#endif

/** Config: TRACKTION_CHECK_FOR_SLOW_RENDERING
    Enabling this adds additional checks to the audio pipeline to help debug performance problems if operations take longer than real time.
*/
#ifndef TRACKTION_CHECK_FOR_SLOW_RENDERING
 #define TRACKTION_CHECK_FOR_SLOW_RENDERING 0
#endif


//==============================================================================
#ifndef TRACKTION_LOG_ENABLED
 #define TRACKTION_LOG_ENABLED 1
#endif

#if TRACKTION_LOG_ENABLED
 #define TRACKTION_LOG(a)        juce::Logger::writeToLog(a)
 #define TRACKTION_LOG_ERROR(a)  juce::Logger::writeToLog (juce::String ("*** ERROR: ") + a);
#else
 #define TRACKTION_LOG(a)        {}
 #define TRACKTION_LOG_ERROR(a)  {}
#endif

//==============================================================================
#define TRACKTION_ASSERT_MESSAGE_THREAD \
    jassert (juce::MessageManager::getInstance()->currentThreadHasLockedMessageManager());

//==============================================================================
namespace tracktion_engine
{
    class Engine;
    class DeviceManager;
    class MidiProgramManager;
    class GrooveTemplateManager;
    class Edit;
    class Track;
    class Clip;
    class Plugin;
    class AudioNode;
    struct AudioRenderContext;
    class AudioFile;
    class PlayHead;
    class Project;
    class InputDevice;
    class OutputDevice;
    class WaveInputDevice;
    class MidiInputDevice;
    class FolderTrack;
    class ClipTrack;
    class AutomationTrack;
    class ChordTrack;
    class MarkerTrack;
    class TempoTrack;
    struct TrackInsertPoint;
    struct TrackList;
    class TrackCompManager;
    class CompFactory;
    class WarpTimeFactory;
    class TempoSequence;
    class TempoSequencePosition;
    class WarpTimeManager;
    class ControlSurface;
    struct AudioFileInfo;
    class LoopInfo;
    class RenderOptions;
    class AutomatableParameter;
    class AutomatableParameterTree;
    class MacroParameterList;
    class MelodyneFileReader;
    struct ARADocumentHolder;
    class ClipEffects;
    class WaveAudioClip;
    class CollectionClip;
    class MidiClip;
    class EditClip;
    class MidiList;
    class SelectedMidiEvents;
    class MarkerManager;
    class TransportControl;
    class AbletonLink;
    class ParameterControlMappings;
    class ParameterChangeHandler;
    class AutomationRecordManager;
    class RenderManager;
    class EditPlaybackContext;
    class EditInputDevices;
    class InputDeviceInstance;
    class GrooveTemplate;
    class MidiOutputDevice;
    class LevelMeterPlugin;
    class VolumeAndPanPlugin;
    class VCAPlugin;
    class NovationAutomap;
    class ExternalController;
    class EditInsertPoint;
    class AudioFileManager;
    class AudioClipBase;
    class AudioTrack;
    class PluginList;
    class RackType;
    class RackInstance;
    class MidiControllerParser;
    class MidiInputDeviceInstanceBase;
    struct RetrospectiveMidiBuffer;
    struct MidiMessageArray;
    struct ModifierTimer;
    class MidiLearnState;
    struct EditDeleter;
    struct ActiveEdits;
    class AudioFileFormatManager;
    class AutomatableEditItem;
    class RecordingThumbnailManager;
    class WaveInputRecordingThread;
    class SearchOperation;
    class ProjectManager;
    class ExternalAutomatableParameter;
    class ExternalPlugin;
    struct PluginWindowState;
    class PluginInstanceWrapper;
    struct LiveClipLevel;
    struct ARAClipPlayer;
    class RackEditorWindow;
    class PitchShiftPlugin;
    struct PluginUnloadInhibitor;
    class ChordClip;
    struct TimecodeSnapType;
    class MidiNote;
    class MackieXT;
    class ParameterisableDragDropSource;
    class AutomationCurveSource;
    class MacroParameter;
    struct Modifier;
    class MidiTimecodeGenerator;
    class MidiClockGenerator;
    class MidiOutputDeviceInstance;
    class WaveInputDeviceInstance;
    class WaveOutputDeviceInstance;
    struct RetrospectiveRecordBuffer;
    class Clipboard;
    class PropertyStorage;
}

//==============================================================================
#include "utilities/tracktion_AppFunctions.h"
#include "utilities/tracktion_Identifiers.h"
#include "utilities/tracktion_ValueTreeUtilities.h"
#include "utilities/tracktion_CrashTracer.h"
#include "utilities/tracktion_AsyncFunctionUtils.h"
#include "utilities/tracktion_CpuMeasurement.h"
#include "utilities/tracktion_ConstrainedCachedValue.h"
#include "utilities/tracktion_FileUtilities.h"
#include "utilities/tracktion_AudioUtilities.h"
#include "utilities/tracktion_AudioScratchBuffer.h"
#include "utilities/tracktion_AudioFadeCurve.h"
#include "utilities/tracktion_Spline.h"
#include "utilities/tracktion_Ditherer.h"
#include "selection/tracktion_Selectable.h"
#include "selection/tracktion_SelectableClass.h"
#include "selection/tracktion_SelectionManager.h"
#include "model/tracks/tracktion_EditTimeRange.h"
#include "utilities/tracktion_BackgroundJobs.h"
#include "utilities/tracktion_MiscUtilities.h"
#include "utilities/tracktion_TemporaryFileManager.h"
#include "utilities/tracktion_PluginComponent.h"
#include "utilities/tracktion_BinaryData.h"
#include "utilities/tracktion_SettingID.h"
#include "utilities/tracktion_MouseHoverDetector.h"
#include "utilities/tracktion_CurveEditor.h"

#include "project/tracktion_ProjectItemID.h"

#include "playback/tracktion_PlayHead.h"
#include "playback/audionodes/tracktion_AudioNode.h"

//==============================================================================
#include "model/automation/tracktion_AutomationCurve.h"
#include "model/edit/tracktion_EditItem.h"
#include "model/automation/tracktion_AutomatableParameterTree.h"
#include "model/automation/tracktion_AutomatableParameter.h"
#include "model/automation/tracktion_AutomatableEditItem.h"
#include "model/automation/tracktion_MacroParameter.h"
#include "model/automation/tracktion_Modifier.h"
#include "model/automation/modifiers/tracktion_ModifierCommon.h"
#include "model/automation/modifiers/tracktion_BreakpointOscillatorModifier.h"
#include "model/automation/modifiers/tracktion_EnvelopeFollowerModifier.h"
#include "model/automation/modifiers/tracktion_LFOModifier.h"
#include "model/automation/modifiers/tracktion_MIDITrackerModifier.h"
#include "model/automation/modifiers/tracktion_RandomModifier.h"
#include "model/automation/modifiers/tracktion_StepModifier.h"

#include "model/export/tracktion_Exportable.h"

#include "control_surfaces/tracktion_ExternalControllerManager.h"

#include "midi/tracktion_Musicality.h"
#include "midi/tracktion_MidiNote.h"
#include "midi/tracktion_MidiMessageArray.h"
#include "midi/tracktion_ActiveNoteList.h"

#include "plugins/tracktion_PluginWindowState.h"
#include "plugins/tracktion_Plugin.h"
#include "plugins/tracktion_PluginList.h"
#include "plugins/tracktion_PluginManager.h"

#include "project/tracktion_ProjectItem.h"
#include "project/tracktion_ProjectSearchIndex.h"
#include "project/tracktion_Project.h"
#include "project/tracktion_ProjectManager.h"

#include "utilities/tracktion_PropertyStorage.h"
#include "utilities/tracktion_UIBehaviour.h"
#include "utilities/tracktion_EngineBehaviour.h"
#include "utilities/tracktion_Engine.h"
#include "utilities/tracktion_Pitch.h"

#include "playback/tracktion_LevelMeasurer.h"

#include "plugins/external/tracktion_VSTXML.h"
#include "plugins/external/tracktion_ExternalPlugin.h"

#include "plugins/internal/tracktion_VCA.h"
#include "plugins/internal/tracktion_VolumeAndPan.h"
#include "plugins/internal/tracktion_RackType.h"
#include "plugins/internal/tracktion_RackInstance.h"
#include "plugins/internal/tracktion_AuxReturn.h"
#include "plugins/internal/tracktion_AuxSend.h"
#include "plugins/effects/tracktion_Equaliser.h"

#include "model/edit/tracktion_EditSnapshot.h"
#include "model/edit/tracktion_EditFileOperations.h"
#include "model/edit/tracktion_EditInsertPoint.h"
#include "model/tracks/tracktion_TrackItem.h"
#include "model/tracks/tracktion_Track.h"
#include "model/edit/tracktion_TimeSigSetting.h"
#include "model/edit/tracktion_TempoSetting.h"
#include "model/edit/tracktion_TempoSequence.h"
#include "model/edit/tracktion_TimecodeDisplayFormat.h"
#include "model/edit/tracktion_PitchSetting.h"
#include "model/edit/tracktion_PitchSequence.h"
#include "model/edit/tracktion_Edit.h"

#include "playback/tracktion_TransportControl.h"
#include "playback/tracktion_AbletonLink.h"

#include "control_surfaces/tracktion_ControlSurface.h"
#include "control_surfaces/tracktion_ExternalController.h"

#include "model/clips/tracktion_AudioSegmentList.h"
#include "audio_files/tracktion_LoopInfo.h"
#include "audio_files/tracktion_AudioFile.h"
#include "model/edit/tracktion_SourceFileReference.h"
#include "model/clips/tracktion_Clip.h"
#include "model/edit/tracktion_EditUtilities.h"

#include "audio_files/tracktion_AudioFileCache.h"
#include "audio_files/tracktion_Thumbnail.h"
#include "audio_files/tracktion_SmartThumbnail.h"
#include "audio_files/tracktion_AudioProxyGenerator.h"
#include "audio_files/tracktion_AudioFileManager.h"
#include "audio_files/tracktion_AudioFileWriter.h"

#include "model/clips/tracktion_CompManager.h"

#include "audio_files/tracktion_AudioFormatManager.h"
#include "audio_files/tracktion_AudioFileUtils.h"
#include "audio_files/tracktion_AudioFifo.h"
#include "audio_files/tracktion_RecordingThumbnailManager.h"
#include "audio_files/formats/tracktion_FloatAudioFileFormat.h"
#include "audio_files/formats/tracktion_LAMEManager.h"
#include "audio_files/formats/tracktion_RexFileFormat.h"

#include "midi/tracktion_MidiProgramManager.h"
#include "midi/tracktion_MidiControllerEvent.h"
#include "midi/tracktion_MidiSysexEvent.h"
#include "midi/tracktion_MidiExpression.h"
#include "midi/tracktion_MidiChannel.h"
#include "midi/tracktion_MidiList.h"
#include "midi/tracktion_SelectedMidiEvents.h"

#include "model/automation/tracktion_AutomationRecordManager.h"
#include "model/automation/tracktion_ParameterChangeHandler.h"
#include "model/automation/tracktion_ParameterControlMappings.h"

#include "playback/devices/tracktion_OutputDevice.h"

#include "model/tracks/tracktion_TrackOutput.h"
#include "model/tracks/tracktion_ClipTrack.h"
#include "model/tracks/tracktion_AudioTrack.h"

#include "timestretch/tracktion_BeatDetect.h"
#include "timestretch/tracktion_TimeStretch.h"

#include "model/export/tracktion_ArchiveFile.h"
#include "model/export/tracktion_ExportJob.h"
#include "model/export/tracktion_ReferencedMaterialList.h"
#include "model/export/tracktion_Renderer.h"
#include "model/export/tracktion_RenderManager.h"

#include "playback/audionodes/tracktion_WaveAudioNode.h"

#include "model/edit/tracktion_QuantisationType.h"

#include "model/clips/tracktion_WarpTimeManager.h"
#include "model/clips/tracktion_AudioClipBase.h"
#include "model/clips/tracktion_ChordClip.h"
#include "model/clips/tracktion_ClipEffects.h"
#include "model/clips/tracktion_CollectionClip.h"
#include "model/clips/tracktion_MarkerClip.h"
#include "model/clips/tracktion_MidiClip.h"
#include "model/clips/tracktion_ReverseRenderJob.h"
#include "model/clips/tracktion_StepClip.h"
#include "model/clips/tracktion_WarpTimeRenderJob.h"
#include "model/clips/tracktion_WaveAudioClip.h"

#include "model/edit/tracktion_GrooveTemplate.h"
#include "model/edit/tracktion_MarkerManager.h"

#include "model/clips/tracktion_EditClip.h"

#include "selection/tracktion_Clipboard.h"

#include "playback/audionodes/tracktion_FadeInOutAudioNode.h"
#include "playback/audionodes/tracktion_TimedMutingAudioNode.h"

#include "model/tracks/tracktion_TrackUtils.h"
#include "model/tracks/tracktion_AutomationTrack.h"
#include "model/tracks/tracktion_ChordTrack.h"
#include "model/tracks/tracktion_FolderTrack.h"
#include "model/tracks/tracktion_MarkerTrack.h"
#include "model/tracks/tracktion_TempoTrack.h"
#include "model/tracks/tracktion_TrackCompManager.h"
#include "model/export/tracktion_RenderOptions.h"
#include "model/clips/tracktion_EditClipRenderJob.h"

#include "playback/devices/tracktion_InputDevice.h"
#include "playback/devices/tracktion_MidiInputDevice.h"
#include "playback/devices/tracktion_PhysicalMidiInputDevice.h"
#include "playback/devices/tracktion_VirtualMidiInputDevice.h"
#include "playback/devices/tracktion_MidiOutputDevice.h"
#include "playback/devices/tracktion_WaveInputDevice.h"
#include "playback/devices/tracktion_WaveOutputDevice.h"

#if JUCE_ANDROID
 #include "playback/tracktion_ScopedSteadyLoad.h"
#endif

#include "playback/tracktion_DeviceManager.h"
#include "playback/tracktion_MidiNoteDispatcher.h"
#include "playback/tracktion_EditPlaybackContext.h"
#include "playback/tracktion_EditInputDevices.h"

#include "playback/audionodes/tracktion_BufferingAudioNode.h"
#include "playback/audionodes/tracktion_ClickNode.h"
#include "playback/audionodes/tracktion_ClickMutingNode.h"
#include "playback/audionodes/tracktion_CombiningAudioNode.h"
#include "playback/audionodes/tracktion_HissingAudioNode.h"
#include "playback/audionodes/tracktion_MidiAudioNode.h"
#include "playback/audionodes/tracktion_MixerAudioNode.h"
#include "playback/audionodes/tracktion_PlayHeadAudioNode.h"
#include "playback/audionodes/tracktion_SidechainAudioNode.h"
#include "playback/audionodes/tracktion_TrackMutingAudioNode.h"
#include "playback/audionodes/tracktion_SpeedRampAudioNode.h"

#include "plugins/internal/tracktion_LevelMeter.h"
#include "plugins/internal/tracktion_FreezePoint.h"
#include "plugins/internal/tracktion_InsertPlugin.h"
#include "plugins/internal/tracktion_ReWirePlugin.h"
#include "plugins/internal/tracktion_TextPlugin.h"

#include "plugins/effects/tracktion_Compressor.h"
#include "plugins/effects/tracktion_Delay.h"
#include "plugins/effects/tracktion_Chorus.h"
#include "plugins/effects/tracktion_LowPass.h"
#include "plugins/effects/tracktion_MidiModifier.h"
#include "plugins/effects/tracktion_MidiPatchBay.h"
#include "plugins/effects/tracktion_PatchBay.h"
#include "plugins/effects/tracktion_Phaser.h"
#include "plugins/effects/tracktion_PitchShift.h"
#include "plugins/effects/tracktion_Reverb.h"
#include "plugins/effects/tracktion_SamplerPlugin.h"

#include "plugins/ARA/tracktion_MelodyneFileReader.h"

#if TRACKTION_ENABLE_CONTROL_SURFACES
 #include "control_surfaces/types/tracktion_NovationAutomap.h"
#endif

#include "control_surfaces/tracktion_CustomControlSurface.h"

#if TRACKTION_ENABLE_CONTROL_SURFACES
 #include "control_surfaces/types/tracktion_AlphaTrack.h"
 #include "control_surfaces/types/tracktion_MackieC4.h"
 #include "control_surfaces/types/tracktion_MackieMCU.h"
 #include "control_surfaces/types/tracktion_MackieXT.h"
 #include "control_surfaces/types/tracktion_NovationRemoteSl.h"
 #include "control_surfaces/types/tracktion_RemoteSLCompact.h"
 #include "control_surfaces/types/tracktion_Tranzport.h"
#endif

#include "model/automation/tracktion_MidiLearn.h"

#endif
