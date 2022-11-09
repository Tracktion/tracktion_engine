/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion { inline namespace engine
{

//==============================================================================
// Files
//==============================================================================

/** Uses the ProjectManager to look up the file for an Edit. */
juce::File getEditFileFromProjectManager (Edit&);

/** Returns true if the ProjectItemID is being used for any of the Edit's elements. */
bool referencesProjectItem (Edit&, ProjectItemID);


//==============================================================================
// Edit
//==============================================================================

/** Inserts blank space in to an Edit, splitting clips if necessary. */
void insertSpaceIntoEdit (Edit&, TimeRange timeRangeToInsert);

/** Inserts a number of blank beats in to the Edit. */
void insertSpaceIntoEditFromBeatRange (Edit&, BeatRange);

//==============================================================================
// Tracks
//==============================================================================

/** Returns all the tracks in an Edit. */
juce::Array<Track*> getAllTracks (const Edit&);

/** Returns all of the non-foldered tracks in an Edit. */
juce::Array<Track*> getTopLevelTracks (const Edit&);

/** Returns the tracks of a given type in an Edit. */
template<typename TrackType>
juce::Array<TrackType*> getTracksOfType (const Edit&, bool recursive);

/** Returns all the AudioTracks in an Edit. */
juce::Array<AudioTrack*> getAudioTracks (const Edit&);

/** Returns all the ClipTracks in an Edit. */
juce::Array<ClipTrack*> getClipTracks (const Edit&);

/** Returns the total number of Tracks in an Edit. */
int getTotalNumTracks (const Edit&);

/** Returns the Track with a given ID if contained in the Edit. */
Track* findTrackForID (const Edit&, EditItemID);

/** Returns the Tracks for the given IDs in the Edit. */
juce::Array<Track*> findTracksForIDs (const Edit&, const juce::Array<EditItemID>&);

/** Returns the Track with a given state if contained in the Edit. */
Track* findTrackForState (const Edit&, const juce::ValueTree&);

/** Returns the first audio track in an Edit. */
AudioTrack* getFirstAudioTrack (const Edit&);

/** Returns all the tracks containing the selected tracks or TrackItems. */
juce::Array<Track*> findAllTracksContainingSelectedItems (const SelectableList&);

/** Returns the first ClipTrack from the selected tracks or clips. */
ClipTrack* findFirstClipTrackFromSelection (const SelectableList&);

/** Returns true if the Edit contains this Track. */
bool containsTrack (const Edit&, const Track&);

/** Returns the TrackOutput if the given track has one. */
TrackOutput* getTrackOutput (Track&);

/** Returns the set of tracks as a BigInteger with each bit corresponding to the
    array of all tracks in an Edit. Used in Renderer.
*/
juce::BigInteger toBitSet (const juce::Array<Track*>&);

/** Returns an Array of Track[s] corresponding to the set bits of all
    tracks in an Edit. Used in Renderer.
*/
juce::Array<Track*> toTrackArray (Edit&, const juce::BigInteger&);


//==============================================================================
// Clips
//==============================================================================

/** Returns the Clip with a given ID if contained in the Edit. */
Clip* findClipForID (const Edit&, EditItemID);

/** Returns the Clip for a given state if contained in the Edit. */
Clip* findClipForState (const Edit&, const juce::ValueTree&);

/** Returns true if an Edit contains a given clip. */
bool containsClip (const Edit&, Clip*);

/** Creates a unique copy of this clip with a new EditItemID. */
Clip::Ptr duplicateClip (const Clip&);

/** Calls a function for all TrackItems in an Edit. */
void visitAllTrackItems (const Edit&, std::function<bool (TrackItem&)>);

/** Returns the time range covered by the given items. */
TimeRange getTimeRangeForSelectedItems (const SelectableList&);

/** An enum to specify if gaps deleted should be closed or not. */
enum class CloseGap
{
    no, /**< Don't move up subsequent track content. */
    yes /**< Do move up subsequent track content. */
};

/** Deletes a time range of an Edit, optionally closing the gap. */
void deleteRegionOfTracks (Edit&, TimeRange rangeToDelete, bool onlySelectedTracks, CloseGap, SelectionManager*);

/** Deletes a time range of a Clip. */
void deleteRegionOfClip (Clip&, TimeRange rangeToDelete);

/** Deletes a time range of a Clip selection, optionally closing the gap. */
void deleteRegionOfSelectedClips (SelectionManager&, TimeRange rangeToDelete,
                                  CloseGap, bool moveAllSubsequentClipsOnTrack);

/** Splits the clips at a given time. */
SelectableList splitClips (const SelectableList& clips, TimePosition time);

/** Enum to dictate move clip behaviour. */
enum class MoveClipAction
{
    moveToEndOfLast,
    moveToStartOfNext,
    moveStartToCursor,
    moveEndToCursor
};

/** Moves the selected clips within their track. */
void moveSelectedClips (const SelectableList&, Edit&, MoveClipAction mode, bool alsoMoveAutomation);

/** Returns a list of clips. If any of these are collection clips, this will return their contained clips. */
SelectableList getClipSelectionWithCollectionClipContents (const SelectableList&);

/** Returns all clip effects */
juce::Array<ClipEffect*> getAllClipEffects (Edit& edit);

//==============================================================================
// MIDI
//==============================================================================

/** Returns the MidiNote with a given state. */
MidiNote* findNoteForState (const Edit&, const juce::ValueTree&);

/** Merges a set of MIDI clips in to one new one. */
juce::Result mergeMidiClips (juce::Array<MidiClip*>);


//==============================================================================
// Plugins
//==============================================================================

/** Returns all the plugins in a given Edit. */
Plugin::Array getAllPlugins (const Edit&, bool includeMasterVolume);

/** Returns the plugin with given state. */
Plugin::Ptr findPluginForState (const Edit&, const juce::ValueTree&);

/** Returns the plugin with given EditItemID. */
Plugin::Ptr findPluginForID (const Edit&, EditItemID);

/** Returns the track for the track which the plugin is located on. */
Track* getTrackContainingPlugin (const Edit&, const Plugin*);

/** Returns true if any plugins couldn't be loaded beacuse their files are missing. */
bool areAnyPluginsMissing (const Edit&);

/** Returns all of the instances of a specific RackType in an Edit. */
juce::Array<RackInstance*> getRackInstancesInEditForType (const RackType&);

/** Toggles the enabled state of all plugins in an Edit. */
void muteOrUnmuteAllPlugins (Edit&);

//==============================================================================
// Automation and parameters
//==============================================================================

/** Returns all AutomatableEditItems in an Edit. */
juce::Array<AutomatableEditItem*> getAllAutomatableEditItems (const Edit&);

/** Deletes the automation covered by the selected clips. */
void deleteAutomation (const SelectableList& selectedClips);

//==============================================================================
// Modifiers
//==============================================================================

/** Returns all the ModifierSources in an Edit. */
juce::Array<AutomatableParameter::ModifierSource*> getAllModifierSources (const Edit&);

/** Returns all the Modifiers in an Edit. */
juce::ReferenceCountedArray<Modifier> getAllModifiers (const Edit&);

/** Returns the Modifier with a given type and ID. */
template<typename ModifierType>
typename ModifierType::Ptr findModifierTypeForID (const Edit&, EditItemID);

/** Returns the Modifier with a given ID. */
Modifier::Ptr findModifierForID (const Edit&, EditItemID);

/** Returns the Modifier with a given ID if the RackType contains it. */
Modifier::Ptr findModifierForID (const RackType&, EditItemID);

/** Returns the Track containing a Modifier. */
Track* getTrackContainingModifier (const Edit&, const Modifier::Ptr&);


//==============================================================================
// Macros
//==============================================================================

/** Returns all the MacroParameterLists in an Edit. */
juce::Array<MacroParameterList*> getAllMacroParameterLists (const Edit&);

/** Returns all the MacroParameterElement in an Edit. */
juce::Array<MacroParameterElement*> getAllMacroParameterElements (const Edit&);


//==============================================================================
/** @internal */
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

/** @internal */
template<typename ModifierType>
inline typename ModifierType::Ptr findModifierTypeForID (const Edit& edit, EditItemID id)
{
    for (auto modifier : getAllModifiers (edit))
        if (modifier->itemID == id)
            if (auto asType = dynamic_cast<ModifierType*> (modifier))
                return typename ModifierType::Ptr (asType);

    return {};
}

}} // namespace tracktion { inline namespace engine
