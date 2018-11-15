/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
{

/**
*/
class LoopInfo
{
public:
    LoopInfo();
    LoopInfo (const LoopInfo&);
    LoopInfo (const juce::ValueTree&, juce::UndoManager*);
    LoopInfo (const juce::File&);
    LoopInfo (const juce::AudioFormatReader*, const juce::AudioFormat*);

    LoopInfo& operator= (const LoopInfo&);

    //==============================================================================
    double getBpm (const AudioFileInfo&) const;
    double getBeatsPerSecond (const AudioFileInfo&) const;
    void setBpm (double newBpm, const AudioFileInfo&);

    //==============================================================================
    int getDenominator() const;
    int getNumerator() const;
    void setDenominator (int newDenominator);
    void setNumerator (int newNumerator);
    double getNumBeats() const;
    void setNumBeats (double newNumBeats);

    //==============================================================================
    bool isLoopable() const;
    bool isOneShot() const;

    //==============================================================================
    int getRootNote() const;
    void setRootNote (int note);

    //==============================================================================
    juce::int64 getInMarker() const;
    juce::int64 getOutMarker() const;

    void setInMarker (juce::int64 newPos);
    void setOutMarker (juce::int64 newPos);

    //==============================================================================
    enum class LoopPointType
    {
        manual = 0,
        automatic
    };

    struct LoopPoint
    {
        juce::int64 pos = 0;
        LoopPointType type = LoopPointType::manual;

        bool isAutomatic() const        { return type == LoopPointType::automatic; }
    };

    int getNumLoopPoints() const;
    LoopPoint getLoopPoint (int index) const;
    void addLoopPoint (juce::int64 pos, LoopPointType type);
    void changeLoopPoint (int index, juce::int64 pos, LoopPointType type);
    void deleteLoopPoint (int index);
    void clearLoopPoints();
    void clearLoopPoints (LoopPointType);

    //==============================================================================
    int getNumTags() const;
    void clearTags();
    juce::String getTag (int index) const;

    void addTag (const juce::String& tag);
    void addTags (const juce::StringArray& tags);

    juce::ValueTree state;

private:
    juce::CriticalSection lock;
    juce::UndoManager* um = nullptr;
    bool maintainParent = false;

    void init (Engine&, const juce::AudioFormatReader*, const juce::AudioFormat*);
    void initialiseMissingProps();
    void duplicateIfShared();
    LoopInfo& copyFrom (const juce::ValueTree&);
    void removeChildIfEmpty (const juce::Identifier&);
    juce::ValueTree getLoopPoints() const;
    juce::ValueTree getOrCreateLoopPoints();
    juce::ValueTree getTags() const;
    juce::ValueTree getOrCreateTags();
    void setBpm (double newBpm, double currentBpm);

    template<typename Type>
    void setProp (const juce::Identifier& i, Type v)
    {
        duplicateIfShared();
        const juce::ScopedLock sl (lock);
        state.setProperty (i, v, um);
    }

    template<typename Type>
    Type getProp (const juce::Identifier& i) const
    {
        const juce::ScopedLock sl (lock);
        return state.getProperty (i);
    }

    JUCE_LEAK_DETECTOR (LoopInfo)
};

} // namespace tracktion_engine
