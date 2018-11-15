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
class StepClip   : public Clip,
                   public juce::ChangeBroadcaster
{
public:
    StepClip (const juce::ValueTree&, EditItemID, ClipTrack&);
    ~StepClip();

    using Ptr = juce::ReferenceCountedObjectPtr<StepClip>;

    void cloneFrom (Clip*) override;

    //==============================================================================
    struct Channel  : public Selectable
    {
        Channel (StepClip&, const juce::ValueTree&);
        ~Channel() noexcept;

        bool operator== (const Channel&) const noexcept;

        juce::String getSelectableDescription() override;

        int getIndex() const;
        juce::String getDisplayName() const;

        StepClip& clip;
        juce::ValueTree state;

        juce::CachedValue<MidiChannel> channel;  // MIDI channel. Range: 1 - 16
        juce::CachedValue<int> noteNumber;       // Range: 0 - 127
        juce::CachedValue<int> noteValue;        // Most likely to be ranged from 0 to 127
        juce::CachedValue<juce::String> grooveTemplate, name;

    private:
        JUCE_LEAK_DETECTOR (Channel)
    };

    const juce::Array<Channel*>& getChannels() const noexcept;
    void removeChannel (int index);
    void insertNewChannel (int index);

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

        double getNoteLength() const; // e.g.: 16th note, calculated as 1.0 / 16.0
        void setNoteLength (double);

        juce::Array<int> getVelocities (int channel) const;
        void setVelocities (int channel, const juce::Array<int>&);

        int getVelocity (int channel, int index) const;
        void setVelocity (int channel, int index, int value);

        juce::Array<double> getGates (int channel) const;
        void setGates (int channel, const juce::Array<double>&);

        double getGate (int channel, int index) const;
        void setGate (int channel, int index, double value);

        /** Creates a snapshot of a pattern's notes, velocities and gates to avoid costly
            property parsing. Obviously if you change a property this will become invalid.
         */
        struct CachedPattern
        {
            CachedPattern (const Pattern&, int channel);

            bool getNote (int index) const noexcept;
            int getVelocity (int index) const noexcept;
            double getGate (int index) const noexcept;

            juce::BigInteger notes;
            juce::Array<int> velocities;
            juce::Array<double> gates;
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

        ~PatternInstance()
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
        maxNumNotes         = 128,

        minNumChannels      = 2,
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
    void setVolumeDb (float v)                      { volumeDb = juce::jlimit (-100.0f, 0.0f, v); }

    //==============================================================================
    /** Generate a MidiMessageSequence from either the entire clip or
        one of its pattern instances.

        @param[out] result          Result sequence to add the messages to
        @param[in] convertToSeconds Leave the timestamps in beats, or convert them to seconds while generating
        @param[in] instance         Specific pattern to generate, nullptr for the whole clip
    */
    void generateMidiSequence (juce::MidiMessageSequence&,
                               bool convertToSeconds = true,
                               PatternInstance* instance = nullptr);

    juce::Array<double> getBeatTimesOfPatternStarts() const;

    double getStartBeatOf (PatternInstance*);
    double getEndBeatOf (PatternInstance*);

    int getBeatsPerBar();

    //==============================================================================
    bool canGoOnTrack (Track&) override;
    juce::String getSelectableDescription() override;
    juce::Colour getDefaultColour() const override;

    bool isMidi() const override                        { return false; }
    bool canLoop() const override                       { return false; }
    bool isLooping() const override                     { return false; }
    bool beatBasedLooping() const override              { return true; }
    void setNumberOfLoops (int) override                {}
    void disableLooping() override                      {}
    double getLoopStartBeats() const override           { return 0.0; }
    double getLoopStart() const override                { return 0.0; }
    double getLoopLengthBeats() const override          { return 0.0; }
    double getLoopLength() const override               { return 0.0; }
    bool isMuted() const override                       { return mute; }
    void setMuted (bool m) override                     { mute = m; }

    AudioNode* createAudioNode (const CreateAudioNodeParams&) override;

    juce::CachedValue<bool> repeatSequence, mute;
    juce::CachedValue<float> volumeDb;

private:
    void generateMidiSequenceForChannels (juce::MidiMessageSequence&, bool convertToSeconds,
                                          const Pattern&, double startBeat,
                                          double clipStartBeat, double clipEndBeat, const TempoSequence&);

    //==============================================================================
    struct ChannelList;
    juce::ScopedPointer<ChannelList> channelList;
    PatternArray patternInstanceList;

    const PatternInstance::Ptr getPatternInstance (int index, bool repeatSequence) const;
    void updatePatternList();

    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;
    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override;
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override;

    //==============================================================================
    StepClip() = delete;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StepClip)
};

} // namespace tracktion_engine
