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
/**
    A clip that can contain multiple other clips and mix their output together.

    This makes it possible to group, move, add effects etc. to a number of clips
    easily.
*/
class ContainerClip  : public AudioClipBase,
                       public ClipOwner
{
public:
    /** Creates a ContainerClip from a given state. @see ClipOwner::insertWaveClip. */
    ContainerClip (const juce::ValueTree&, EditItemID, ClipOwner&);
    
    /** Destructor. */
    ~ContainerClip() override;

    using Ptr = juce::ReferenceCountedObjectPtr<ContainerClip>;

    //==============================================================================
    /** @internal */
    juce::ValueTree& getClipOwnerState() override;
    /** @internal */
    Selectable* getClipOwnerSelectable() override;
    /** @internal */
    Edit& getClipOwnerEdit() override;

    //==============================================================================
    /** @internal */
    juce::File getOriginalFile() const override                 { return {}; }
    /** @internal */
    bool isUsingFile (const AudioFile&) override;

    //==============================================================================
    /** @internal */
    void initialise() override;
    /** @internal */
    void cloneFrom (Clip*) override;

    //==============================================================================
    /** @internal */
    juce::String getSelectableDescription() override;
    /** @internal */
    bool isMidi() const override;

    /** @internal */
    TimeDuration getSourceLength() const override;
    /** @internal */
    HashCode getHash() const override;

    /** @internal */
    void setLoopDefaults() override;
    /** @internal */
    void setLoopRangeBeats (BeatRange) override;

    //==============================================================================
    /** @internal */
    void flushStateToValueTree() override;
    /** @internal */
    void pitchTempoTrackChanged() override;

private:
    //==============================================================================
    juce::ValueTree clipListState;

    void clipCreated (Clip&) override;
    void clipAddedOrRemoved() override;
    void clipOrderChanged() override;
    void clipPositionChanged() override;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ContainerClip)
};

}} // namespace tracktion { inline namespace engine
