/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion { inline namespace engine
{

//==============================================================================
// Projects and Files
//==============================================================================

/** Tries to find the project that contains this edit (but may return nullptr!) */
Project::Ptr getProjectForEdit (const Edit&);

/** Tries to find the project item that refers to this edit (but may return nullptr!) */
ProjectItem::Ptr getProjectItemForEdit (const Edit&);

/** Uses the ProjectManager to look up the file for an Edit. */
juce::File getEditFileFromProjectManager (Edit&);

/** Returns true if the ProjectItemID is being used for any of the Edit's elements. */
bool referencesProjectItem (Edit&, ProjectItemID);


//==============================================================================
// Edit
//==============================================================================

/// Inserts blank space in to an Edit, splitting clips if necessary.
void insertSpaceIntoEdit (Edit&, TimeRange timeRangeToInsert);

/// Inserts a number of blank beats in to the Edit.
void insertSpaceIntoEditFromBeatRange (Edit&, BeatRange);

/// Looks for an item in an edit with a given ID
EditItem* findEditItemForID (Edit&, EditItemID);

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

/** Returns the AudioTrack with a given ID if contained in the Edit. */
AudioTrack* findAudioTrackForID (const Edit&, EditItemID);

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

template<typename TrackItemType>
[[ nodiscard ]] juce::Array<TrackItemType*> getTrackItemsOfType (const Track&);

/** Returns the ClipOwner with a given ID if it can be found in the Edit. */
ClipOwner* findClipOwnerForID (const Edit&, EditItemID);


//==============================================================================
// ClipSlots
//==============================================================================
/** Returns the ClipSlot for the given ID. */
ClipSlot* findClipSlotForID (const Edit&, EditItemID);

/** Returns the index of the ClipSlot in the list it is owned by. */
int findClipSlotIndex (ClipSlot&);


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
juce::Result mergeMidiClips (juce::Array<MidiClip*>, SelectionManager* sm = nullptr);

/** Helper function to read a file to a number of MidiLists. */
juce::OwnedArray<MidiList> readFileToMidiList (juce::File midiFile, bool importAsNoteExpression);

/** Helper function to read a MIDI file and create a MidiClip from it.
    N.B. This will only use the first track in the file, any other tracks will be discarded.
    The clip will be positioned at 0 with a beat length the duration of the imported list.
*/
MidiClip::Ptr createClipFromFile (juce::File midiFile, ClipOwner&, bool importAsNoteExpression);


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
void muteOrUnmuteAllPlugins (const Edit&);

/// Performs a "MIDI panic" on the edit, by sending all-note-off messages
/// directly to all plugins in the edit, and optionally resetting them
/// to kill any reverb tails, etc.
void midiPanic (Edit&, bool resetPlugins);


/** Adds a new instance of the given plugin to the track's plugin list at the specified index. */
template<typename PluginType>
juce::ReferenceCountedObjectPtr<PluginType> insertNewPlugin (Track&, int index = 0);

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
// Inputs/recording
//==============================================================================
/** Returns the default set of recording parameters.
    This behaviour may not be desirable in which case it might make more sense
    to just construct your own RecordingParameters and pass them to
    InputDeviceInstance::prepareToRecord.
*/
InputDeviceInstance::RecordingParameters getDefaultRecordingParameters (const EditPlaybackContext&,
                                                                        TimePosition playStart,
                                                                        TimePosition punchIn);

/** Starts an InputDeviceInstance recording to the given target without any count-in etc.
    N.B. The input must already be assigned to the target and armed for the punch to start.
*/
juce::Result prepareAndPunchRecord (InputDeviceInstance&, EditItemID);

/** If the instance is currently recording, this will stop it and return any
    created clips or an error message.
*/
tl::expected<Clip::Array, juce::String> punchOutRecording (InputDeviceInstance&);

/** Returns true if any inputs are currently recording. */
bool isRecording (EditPlaybackContext&);

/** Creates an InputDeviceInstance::Destination on the destinationTrack from the sourceTrack if possible. */
InputDeviceInstance::Destination* assignTrackAsInput (AudioTrack& destinationTrack, const AudioTrack& sourceTrack, InputDevice::DeviceType);

//==============================================================================
// Lookup
//==============================================================================
/** Returns a reference to the Edit if accessible by the given object. */
template<typename Type>
    requires (std::is_same_v<Type, Edit>
              || requires (Type t) { t.edit; }
              || requires (Type t) { t.getEdit(); })
Edit& getEdit (Type& type)
{
    if constexpr (std::is_same_v<Type, Edit>)
        return type;
    else if constexpr (requires { type.edit; })
        return type.edit;
    else if constexpr (requires { type.getEdit(); })
        return type.getEdit();
}


TempoSequence& getTempoSequence (auto& type)        { return getEdit (type).tempoSequence; }

juce::UndoManager& getUndoManager (auto& type)      { return getEdit (type).getUndoManager(); }

juce::UndoManager* getUndoManager_p (auto& type)    { return &getUndoManager (type); }


//==============================================================================
/** @internal */
template<typename PluginType>
inline juce::ReferenceCountedObjectPtr<PluginType> insertNewPlugin (Track& track, int index)
{
    if (auto p = track.edit.getPluginCache().createNewPlugin (PluginType::create()))
    {
        if (auto pluginAsType = dynamic_cast<PluginType*> (p.get()))
        {
            track.pluginList.insertPlugin (*p, index, {});
            return pluginAsType;
        }
    }

    return {};
}

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
template<typename TrackItemType>
inline juce::Array<TrackItemType*> getTrackItemsOfType (const Track& track)
{
    juce::Array<TrackItemType*> items;
    const int numItems = track.getNumTrackItems();

    for (int i = 0; i < numItems; ++i)
        if (auto ti = dynamic_cast<TrackItemType*> (track.getTrackItem (i)))
            items.add (ti);

    return items;
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
