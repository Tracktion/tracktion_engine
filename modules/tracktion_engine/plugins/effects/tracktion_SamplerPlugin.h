/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
{

class SamplerPlugin  : public Plugin,
                       private juce::AsyncUpdater
{
public:
    SamplerPlugin (PluginCreationInfo);
    ~SamplerPlugin();

    //==============================================================================
    int getNumSounds() const;
    juce::String getSoundName (int index) const;
    void setSoundName (int index, const juce::String& name);
    AudioFile getSoundFile (int index) const;
    juce::String getSoundMedia (int index) const;
    int getKeyNote (int index) const;
    int getMinKey (int index) const;
    int getMaxKey (int index) const;
    float getSoundGainDb (int index) const;
    float getSoundPan (int index) const;
    bool isSoundOpenEnded (int index) const;
    double getSoundStartTime (int index) const;
    double getSoundLength (int index) const;
    void setSoundExcerpt (int index, double start, double length);

    // returns an error
    juce::String addSound (const juce::String& sourcePathOrProjectID, const juce::String& name,
                           double startTime, double length, float gainDb);
    void removeSound (int index);
    void setSoundParams (int index, int keyNote, int minNote, int maxNote);
    void setSoundGains (int index, float gainDb, float pan);
    void setSoundOpenEnded (int index, bool isOpenEnded);
    void setSoundMedia (int index, const juce::String& sourcePathOrProjectID);

    void playNotes (const juce::BigInteger& keysDown);
    void allNotesOff();

    //==============================================================================
    static const char* getPluginName()                  { return NEEDS_TRANS("Sampler"); }
    static const char* xmlTypeName;

    juce::String getName() override                     { return TRANS("Sampler"); }
    juce::String getPluginType() override               { return xmlTypeName; }
    juce::String getShortName (int) override            { return "Smplr"; }
    juce::String getSelectableDescription() override    { return TRANS("Sampler"); }
    bool needsConstantBufferSize() override             { return false; }

    int getNumOutputChannelsGivenInputs (int numInputChannels) override { return juce::jmin (numInputChannels, 2); }
    void initialise (const PlaybackInitialisationInfo&) override;
    void deinitialise() override;
    void applyToBuffer (const AudioRenderContext&) override;

    //==============================================================================
    bool takesMidiInput() override                      { return true; }
    bool takesAudioInput() override                     { return true; }
    bool producesAudioWhenNoAudioInput() override       { return true; }
    bool hasNameForMidiNoteNumber (int note, int midiChannel, juce::String& name) override;

    juce::Array<ReferencedItem> getReferencedItems() override;
    void reassignReferencedItem (const ReferencedItem&, ProjectItemID newID, double newStartTime) override;
    void sourceMediaChanged() override;

    void restorePluginStateFromValueTree (const juce::ValueTree&) override;

    //==============================================================================
    struct SamplerSound
    {
        SamplerSound (SamplerPlugin&, const juce::String& sourcePathOrProjectID, const juce::String& name,
                      double startTime, double length, float gainDb);

        void setExcerpt (double startTime, double length);
        void refreshFile();

        SamplerPlugin& owner;
        juce::String source;
        juce::String name;
        int keyNote = -1, minNote = 0, maxNote = 0;
        int fileStartSample = 0, fileLengthSamples = 0;
        bool openEnded = false;
        float gainDb = 0, pan = 0;
        double startTime = 0, length = 0;
        AudioFile audioFile;
        juce::AudioBuffer<float> audioData { 2, 64 };

    private:
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SamplerSound)
    };

private:
    //==============================================================================
    struct SampledNote;

    juce::Colour colour;
    juce::CriticalSection lock;
    juce::ReferenceCountedArray<SampledNote> playingNotes;
    juce::OwnedArray<SamplerSound> soundList;
    juce::BigInteger highlightedNotes;

    juce::ValueTree getSound (int index) const;

    void valueTreeChanged() override;
    void handleAsyncUpdate() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SamplerPlugin)
};

} // namespace tracktion_engine
