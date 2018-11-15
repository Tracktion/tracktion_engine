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
/**
*/
class MidiClip  : public Clip
{
public:
    //==============================================================================
    MidiClip() = delete;
    MidiClip (const juce::ValueTree&, EditItemID, ClipTrack&);
    ~MidiClip();

    using Ptr = juce::ReferenceCountedObjectPtr<MidiClip>;

    void cloneFrom (Clip*) override;

    AudioTrack* getAudioTrack() const;

    //==============================================================================
    AudioNode* createAudioNode (const CreateAudioNodeParams&) override;

    MidiList& getSequence() const noexcept;
    MidiList& getSequenceLooped();
    MidiList* createSequenceLooped (MidiList& sourceSequence);

    const SelectedMidiEvents* getSelectedEvents() const             { return selectedEvents; }

    void scaleVerticallyToFit();

    bool hasValidSequence() const noexcept                          { return channelSequence.size() > 0; }

    MidiChannel getMidiChannel() const                              { return hasValidSequence() ? getSequence().getMidiChannel() : MidiChannel(); }
    void setMidiChannel (MidiChannel newChannel)                    { getSequence().setMidiChannel (newChannel); }

    /** Sets whether the clip should send MPE MIDI rather than single channel. */
    void setMPEMode (bool shouldUseMPE)                             { mpeMode = shouldUseMPE; }
    bool getMPEMode() const noexcept                                { return mpeMode; }

    /** Returns true if this clip represents a rhythm instrument (e.g. MIDI channel 10) */
    bool isRhythm() const noexcept                                  { return getMidiChannel().getChannelNumber() == 10; }

    //==============================================================================
    QuantisationType& getQuantisation() const noexcept              { return *quantisation; }
    void setQuantisation (const QuantisationType& newType);

    juce::String getGrooveTemplate() const noexcept                 { return grooveTemplate; }
    void setGrooveTemplate (const juce::String& templateName)       { grooveTemplate = templateName; }

    //==============================================================================
    void mergeInMidiSequence (juce::MidiMessageSequence&, MidiList::NoteAutomationType);

    void addTake (juce::MidiMessageSequence&, MidiList::NoteAutomationType);

    /** This will extend the start time backwards, moving the notes along if this takes the offset below 0.0 */
    void extendStart (double newStartTime);

    void trimBeyondEnds (bool beyondStart, bool beyondEnd, juce::UndoManager*);

    /** Lengthens or shortens a note to match the start of the next note in the given array.
        If the note is the last in the sequence, it will use the maxEndBeat as its end.

        @note notesToUse must be in ascending note start order.
    */
    void legatoNote (MidiNote& note, const juce::Array<MidiNote*>& notesToUse,
                     double maxEndBeat, juce::UndoManager&);

    //==============================================================================
    float getVolumeDb() const                       { return volumeDb; }
    void setVolumeDb (float v)                      { volumeDb = juce::jlimit (-100.0f, 0.0f, v); }

    bool isSendingBankChanges() const noexcept      { return sendBankChange; }
    void setSendingBankChanges (bool sendBank);

    bool isMuted() const override                   { return mute; }
    void setMuted (bool m) override                 { mute = m; }

    //==============================================================================
    void initialise() override;
    bool isMidi() const override                    { return true; }
    void rescale (double pivotTimeInEdit, double factor) override;
    bool canGoOnTrack (Track&) override;
    juce::String getSelectableDescription() override;
    juce::Colour getDefaultColour() const override;

    void clearTakes() override;
    bool hasAnyTakes() const override               { return channelSequence.size() > 1; }
    int getNumTakes (bool includeComps) override;
    juce::StringArray getTakeDescriptions() const override;
    void setCurrentTake (int takeIndex) override;
    int getCurrentTake() const override             { return currentTake; }
    bool isCurrentTakeComp() override;
    void deleteAllUnusedTakesConfirmingWithUser();
    Clip::Array unpackTakes (bool toNewTracks) override;
    MidiList* getTakeSequence (int takeIndex) const { return channelSequence[takeIndex]; }

    bool canLoop() const override                   { return true; }
    bool isLooping() const override                 { return loopLengthBeats > 0.0; }
    bool beatBasedLooping() const override          { return isLooping(); }
    void setNumberOfLoops (int) override;
    void disableLooping() override;
    void setLoopRange (EditTimeRange) override;
    void setLoopRangeBeats (juce::Range<double>) override;
    double getLoopStartBeats() const override       { return loopStartBeats; }
    double getLoopLengthBeats() const override      { return loopLengthBeats; }
    double getLoopStart() const override;
    double getLoopLength() const override;

    enum class LoopedSequenceType : int
    {
        loopRangeDefinesAllRepetitions          = 0,    /**< The looped sequence is the same for all repititions including the first. */
        loopRangeDefinesSubsequentRepetitions   = 1     /**< The first section is the whole sequence, subsequent repitions are determined by the loop range. */
    };

    juce::CachedValue<LoopedSequenceType> loopedSequenceType;

    MidiCompManager& getCompManager();

    //==============================================================================
    PatternGenerator* getPatternGenerator() override;
    void pitchTempoTrackChanged() override;

    //==============================================================================
    /** Temporarily limits the notes to use. */
    struct ScopedEventsList
    {
        ScopedEventsList (MidiClip&, SelectedMidiEvents*);
        ~ScopedEventsList();

    private:
        MidiClip& clip;
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ScopedEventsList)
    };

protected:
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;
    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override;
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override;

private:
    //==============================================================================
    friend class MidiNote;

    //==============================================================================
    juce::OwnedArray<MidiList> channelSequence;

    juce::CachedValue<int> currentTake;
    juce::CachedValue<float> volumeDb;
    juce::CachedValue<double> loopStartBeats, loopLengthBeats, originalLength;
    juce::ScopedPointer<QuantisationType> quantisation;
    juce::CachedValue<bool> sendPatch, mute, sendBankChange, mpeMode;
    juce::CachedValue<juce::String> grooveTemplate;

    bool shouldWarnAboutMultiChannel = false;
    SelectedMidiEvents* selectedEvents = nullptr;

    mutable juce::ScopedPointer<MidiList> cachedLoopedSequence;
    MidiCompManager::Ptr midiCompManager;

    //==============================================================================
    void setSelectedEvents (SelectedMidiEvents* events)     { selectedEvents = events; }

    //==============================================================================
    MidiList* getMidiListForState (const juce::ValueTree&);
    void clearCachedLoopSequence();

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiClip)
};

} // namespace tracktion_engine

namespace juce
{
    template <>
    struct VariantConverter<tracktion_engine::MidiClip::LoopedSequenceType>
    {
        static tracktion_engine::MidiClip::LoopedSequenceType fromVar (const var& v)   { return (tracktion_engine::MidiClip::LoopedSequenceType) static_cast<int> (v); }
        static var toVar (tracktion_engine::MidiClip::LoopedSequenceType v)            { return static_cast<int> (v); }
    };
}
