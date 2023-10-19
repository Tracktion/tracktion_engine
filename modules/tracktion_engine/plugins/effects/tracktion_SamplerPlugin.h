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

// BEATCONNECT MODIFICATION START
enum class FilterType {
    noFilter = 0,
    lowpass,
    highpass,
    bandpass,
    notch,
    allpass,
    lowshelf,
    highshelf,
    peak
};
// BEATCONNECT MODIFICATION END

class SamplerPlugin  : public Plugin,
// BEATCONNECT MODIFICATION START
                       // private juce::AsyncUpdater
                       public juce::AsyncUpdater
// BEATCONNECT MODIFICATION END
{
public:
    SamplerPlugin (PluginCreationInfo);
    ~SamplerPlugin() override;

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
    // BEATCONNECT MODIFICATION START
    FilterType getFilterType(const int index) const;
    double getFilterFrequency(const int index) const;
    double getFilterGain(const int index) const;
    // BEATCONNECT MODIFICATION END

    // returns an error
    juce::String addSound (const juce::String& sourcePathOrProjectID, const juce::String& name,
                            double startTime, double length, float gainDb,
                            int keyNote = 72, int minNote = 72 - 24, int maxNote = 72 + 24, bool openEnded = true
                            // BEATCONNECT MODIFICATION START
                            , int filterType = 0, double filterFrequency = 0, double filterGain = 0
                            // BEATCONNECT MODIFICATION END
                            );
    void removeSound (int index);
    void setSoundParams (int index, int keyNote, int minNote, int maxNote);
    // BEATCONNECT MODIFICATION START
    void setFilterType(const int index, FilterType filterType);
    void setFilterFrequency (const int index, const double gain);
    void setFilterGain (const int index, const double filterGain);
    void setSoundFilter (const int index, const FilterType filterType, 
        const double filterFrequency, const double filterGain = 0);
    // BEATCONNECT MODIFICATION END
    void setSoundGains (int index, float gainDb, float pan);
    void setSoundOpenEnded (int index, bool isOpenEnded);
    void setSoundMedia (int index, const juce::String& sourcePathOrProjectID);

    void playNotes (const juce::BigInteger& keysDown);
    void allNotesOff();

    //==============================================================================
    static const char* getPluginName()                  { return NEEDS_TRANS("Sampler"); }
    static const char* xmlTypeName;
    // BEATCONNECT MODIFICATION START
    static const char* uniqueId;
    // BEATCONNECT MODIFICATION END

    juce::String getName() override                     { return TRANS("Sampler"); }
    juce::String getPluginType() override               { return xmlTypeName; }
    juce::String getShortName (int) override            { return "Smplr"; }
    juce::String getSelectableDescription() override    { return TRANS("Sampler"); }
    // BEATCONNECT MODIFICATION START
    juce::String getUniqueId() override                 { return uniqueId; }
    virtual juce::String getVendor() override           { return "BeatConnect"; }
    // BEATCONNECT MODIFICATION END
    bool isSynth() override                             { return true; }
    bool needsConstantBufferSize() override             { return false; }

    int getNumOutputChannelsGivenInputs (int numInputChannels) override { return juce::jmin (numInputChannels, 2); }
    void initialise (const PluginInitialisationInfo&) override;
    void deinitialise() override;
    void applyToBuffer (const PluginRenderContext&) override;

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
        // BEATCONNECT MODIFICATION START
        FilterType filterType;
        double filterFrequency = 0, filterGain = 0;
        // BEATCONNECT MODIFICATION END
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



}} // namespace tracktion { inline namespace engine
