/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/

namespace tracktion_engine
{

//==============================================================================
// Files
//==============================================================================

/** Uses the ProjectManager to look up the file for an Edit. */
juce::File getEditFileFromProjectManager (Edit&);

/** Returns true if the ProjectItemID is being used for any of the Edit's elements. */
bool referencesProjectItem (Edit&, ProjectItemID);


//==============================================================================
// Tracks
//==============================================================================

/** */
juce::Array<Track*> getAllTracks (const Edit&);

/** */
juce::Array<Track*> getTopLevelTracks (const Edit&);

/** */
template<typename TrackType>
juce::Array<TrackType*> getTracksOfType (const Edit&, bool recursive);

/** */
juce::Array<AudioTrack*> getAudioTracks (const Edit&);

/** */
juce::Array<ClipTrack*> getClipTracks (const Edit&);

/** */
int getTotalNumTracks (const Edit&);

/** */
Track* findTrackForID (const Edit&, EditItemID);

/** */
Track* findTrackForState (const Edit&, const juce::ValueTree&);

/** */
AudioTrack* getFirstAudioTrack (const Edit&);

/** */
juce::Array<Track*> findAllTracksContainingSelectedItems (const SelectableList&);

/** */
ClipTrack* findFirstClipTrackFromSelection (const SelectableList&);

/** */
bool containsTrack (const Edit&, const Track&);


//==============================================================================
// Clips
//==============================================================================

/** */
Clip* findClipForID (const Edit&, EditItemID);

/** */
Clip* findClipForState (const Edit&, const juce::ValueTree&);

/** */
bool containsClip (const Edit&, Clip*);

/** Creates a unique copy of this clip with a new EditItemID. */
Clip::Ptr duplicateClip (const Clip&);

/** */
void visitAllTrackItems (const Edit&, std::function<bool (TrackItem&)>);

/** */
EditTimeRange getTimeRangeForSelectedItems (const SelectableList&);

/** */
void deleteRegionOfTracks (Edit&, EditTimeRange rangeToDelete, bool onlySelectedTracks, bool closeGap, SelectionManager*);

/** */
void deleteRegionOfClip (Clip&, EditTimeRange rangeToDelete);

/** */
void deleteRegionOfSelectedClips (SelectionManager&, EditTimeRange rangeToDelete,
                                  bool closeGap, bool moveAllSubsequentClipsOnTrack);

/** */
SelectableList splitClips (const SelectableList& clips, double time);

/** */
enum class MoveClipAction
{
    moveToEndOfLast,
    moveToStartOfNext,
    moveStartToCursor,
    moveEndToCursor
};

/** */
void moveSelectedClips (const SelectableList&, Edit&, MoveClipAction mode, bool alsoMoveAutomation);

/** */
SelectableList getClipSelectionWithCollectionClipContents (const SelectableList&);

//==============================================================================
// MIDI
//==============================================================================

/** */
MidiNote* findNoteForState (const Edit&, const juce::ValueTree&);

/** */
void mergeMidiClips (juce::Array<MidiClip*> clips);


//==============================================================================
// Plugins
//==============================================================================

/** */
Plugin::Array getAllPlugins (const Edit&, bool includeMasterVolume);

/** */
Plugin::Ptr findPluginForState (const Edit&, const juce::ValueTree&);

/** */
Track* getTrackContainingPlugin (const Edit&, const Plugin*);

/** */
bool areAnyPluginsMissing (const Edit&);


//==============================================================================
// Automation and parameters
//==============================================================================

/** */
juce::Array<AutomatableEditItem*> getAllAutomatableEditItems (const Edit&);

/** */
void deleteAutomation (const SelectableList& selectedClips);

//==============================================================================
// Modifiers
//==============================================================================

/** */
juce::Array<AutomatableParameter::ModifierSource*> getAllModifierSources (const Edit&);

/** */
juce::ReferenceCountedArray<Modifier> getAllModifiers (const Edit&);

/** */
template<typename ModifierType>
typename ModifierType::Ptr findModifierTypeForID (const Edit&, EditItemID);
Modifier::Ptr findModifierForID (const Edit&, EditItemID);

/** */
Track* getTrackContainingModifier (const Edit&, const Modifier::Ptr&);


//==============================================================================
// Macros
//==============================================================================

/** */
juce::Array<MacroParameterList*> getAllMacroParameterLists (const Edit&);

/** */
juce::Array<MacroParameterElement*> getAllMacroParameterElements (const Edit&);


//==============================================================================
template<typename TrackType>
inline juce::Array<TrackType*> getTracksOfType (const Edit& edit, bool recursive)
{
    juce::Array<TrackType*> result;
    result.ensureStorageAllocated (32);

    edit.visitAllTracks ([&] (Track& t)
                         {
                             if (auto type = dynamic_cast<TrackType*> (&t))
                                 result.add (type);

                             return true;
                         }, recursive);

    return result;
}

template<typename ModifierType>
inline typename ModifierType::Ptr findModifierTypeForID (const Edit& edit, EditItemID id)
{
    for (auto modifier : getAllModifiers (edit))
        if (modifier->itemID == id)
            if (auto asType = dynamic_cast<ModifierType*> (modifier))
                return typename ModifierType::Ptr (asType);

    return {};
}

} // namespace tracktion_engine
