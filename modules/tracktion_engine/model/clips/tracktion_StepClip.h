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
*/
class StepClip   : public Clip,
                   public juce::ChangeBroadcaster
{
public:
    StepClip (const juce::ValueTree&, EditItemID, ClipTrack&);
    ~StepClip() override;

    using Ptr = juce::ReferenceCountedObjectPtr<StepClip>;

    void cloneFrom (Clip*) override;

    //==============================================================================
    struct Channel  : public Selectable
    {
        Channel (StepClip&, const juce::ValueTree&);
        ~Channel() noexcept override;

        bool operator== (const Channel&) const noexcept;

        juce::String getSelectableDescription() override;

        int getIndex() const;
        juce::String getDisplayName() const;

        bool usesGrooveStrength() const;

        StepClip& clip;
        juce::ValueTree state;

        juce::CachedValue<MidiChannel> channel;  // MIDI channel. Range: 1 - 16
        juce::CachedValue<int> noteNumber;       // Range: 0 - 127
        juce::CachedValue<int> noteValue;        // Most likely to be ranged from 0 to 127
        juce::CachedValue<juce::String> grooveTemplate, name;
        juce::CachedValue<float> grooveStrength;

    private:
        JUCE_LEAK_DETECTOR (Channel)
    };

    const juce::Array<Channel*>& getChannels() const noexcept;
    void removeChannel (int index);
    void insertNewChannel (int index);

    bool usesProbability();

    //==============================================================================
    /**
    */
    struct Pattern
    {
        Pattern (StepClip&, const juce::ValueTree&) noexcept;
        Pattern (const Pattern&) noexcept;

        juce::String getName() const;
        void setName (const juce::String&);

        bool getNote (int channel, int index) const noexcept;
        void setNote (int channel, int index, bool value);

        void clear();
        void clearChannel (int channel);
        void insertChannel (int channel);
        void removeChannel (int channel);
        void randomiseChannel (int channel);
        void randomiseSteps();
        void shiftChannel (int channel, bool toTheRight);
        void toggleAtInterval (int channel, int interval);

        juce::BigInteger getChannel (int index) const;
        void setChannel (int index, const juce::BigInteger&);

        int getNumNotes() const;
        void setNumNotes (int);

        /** Returns the length of one step as a fraction of a beat. */
        BeatDuration getNoteLength() const;

        /** Sets the length of one step as a fraction of a beat. */
        void setNoteLength (BeatDuration);

        juce::Array<int> getVelocities (int channel) const;
        void setVelocities (int channel, const juce::Array<int>&);

        int getVelocity (int channel, int index) const;
        void setVelocity (int channel, int index, int value);

        juce::Array<double> getGates (int channel) const;
        void setGates (int channel, const juce::Array<double>&);

        double getGate (int channel, int index) const;
        void setGate (int channel, int index, double value);

        juce::Array<float> getProbabilities (int channel) const;
        void setProbabilities (int channel, const juce::Array<float>&);

        float getProbability (int channel, int index) const;
        void setProbability (int channel, int index, float value);

        /** Creates a snapshot of a pattern's notes, velocities and gates to avoid costly
            property parsing. Obviously if you change a property this will become invalid.
         */
        struct CachedPattern
        {
            CachedPattern (const Pattern&, int channel);

            bool getNote (int index) const noexcept;
            int getVelocity (int index) const noexcept;
            double getGate (int index) const noexcept;
            float getProbability (int index) const noexcept;

            juce::BigInteger notes;
            juce::Array<int> velocities;
            juce::Array<double> gates;
            juce::Array<float> probabilities;
            JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CachedPattern)
        };

        StepClip& clip;
        juce::ValueTree state;

    private:
        Pattern& operator= (const Pattern&) = delete;
        JUCE_LEAK_DETECTOR (Pattern)
    };

    //==============================================================================
    /**
    */
    struct PatternInstance  : public Selectable,
                              public juce::ReferenceCountedObject
    {
        PatternInstance (StepClip& c, int index)
            : clip (c), patternIndex (index) {}

        ~PatternInstance() override
        {
            notifyListenersOfDeletion();
        }

        using Ptr = juce::ReferenceCountedObjectPtr<PatternInstance>;

        StepClip& clip;
        int patternIndex;

        Pattern getPattern() const;
        int getSequenceIndex() const;

        juce::String getSelectableDescription() override;

    private:
        PatternInstance() = delete;
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PatternInstance)
    };

    //==============================================================================
    enum Defaults
    {
        defaultNumNotes     = 16,
        defaultNumChannels  = 8,
        defaultMidiChannel  = 1,
        defaultNoteNumber   = 60,
        defaultNoteValue    = 96,

        minNumNotes         = 2,
        maxNumNotes         = 512,

        minNumChannels      = 1,
        maxNumChannels      = 60
    };

    //==============================================================================
    using PatternArray = juce::ReferenceCountedArray<PatternInstance>;

    PatternArray getPatternSequence() const;
    void setPatternSequence (const PatternArray&);

    void insertVariation (int patternIndex, int insertIndex);
    void removeVariation (int variationIndex);
    void removeAllVariations();
    void createDefaultPatternIfEmpty();
    void setPatternForVariation (int variationIndex, int newPatternIndex);

    //==============================================================================
    int insertPattern (const Pattern&, int index);
    int insertNewPattern (int index);
    void removePattern (int patternIndex);
    void removeUnusedPatterns();

    //==============================================================================
    bool getCell (int patternIndex, int channelIndex, int noteIndex);
    void setCell (int patternIndex, int channelIndex, int noteIndex, bool value);

    //==============================================================================
    Pattern getPattern (int index);
    int getNumPatterns() const;
    juce::Array<Pattern> getPatterns();

    //==============================================================================
    float getVolumeDb() const                       { return level->dbGain.get(); }
    void setVolumeDb (float v)                      { level->dbGain = juce::jlimit (-100.0f, 0.0f, v); }

    //==============================================================================
    /** Generate a MidiMessageSequence from either the entire clip or
        one of its pattern instances.

        @param[out] result          Result sequence to add the messages to
        @param[in] convertToSeconds Leave the timestamps in beats, or convert them to seconds while generating
        @param[in] instance         Specific pattern to generate, nullptr for the whole clip
    */
    void generateMidiSequence (juce::MidiMessageSequence& result,
                               bool convertToSeconds = true,
                               PatternInstance* instance = nullptr);

    juce::Array<BeatPosition> getBeatTimesOfPatternStarts() const;

    BeatPosition getStartBeatOf (PatternInstance*);
    BeatPosition getEndBeatOf (PatternInstance*);

    int getBeatsPerBar();

    void resizeClipForPatternInstances();

    //==============================================================================
    bool canGoOnTrack (Track&) override;
    juce::String getSelectableDescription() override;
    juce::Colour getDefaultColour() const override;

    bool isMidi() const override                        { return false; }
    bool beatBasedLooping() const override              { return true; }
    bool isMuted() const override                       { return level->mute.get(); }
    void setMuted (bool m) override                     { level->mute = m; }

    LiveClipLevel getLiveClipLevel();

    juce::CachedValue<bool> repeatSequence;

private:
    void generateMidiSequenceForChannels (juce::MidiMessageSequence&, bool convertToSeconds,
                                          const Pattern&, BeatPosition startBeat,
                                          BeatPosition clipStartBeat, BeatPosition clipEndBeat, const TempoSequence&);

    //==============================================================================
    struct ChannelList;
    std::unique_ptr<ChannelList> channelList;
    PatternArray patternInstanceList;
    std::shared_ptr<ClipLevel> level { std::make_shared<ClipLevel>() };

    const PatternInstance::Ptr getPatternInstance (int index, bool repeatSequence) const;
    void updatePatternList();

    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;
    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override;
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override;
    void valueTreeChildOrderChanged (juce::ValueTree&, int, int) override;

    //==============================================================================
    StepClip() = delete;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StepClip)
};

}} // namespace tracktion { inline namespace engine
