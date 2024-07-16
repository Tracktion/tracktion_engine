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

/**
    Holds tempo/beat information about an audio file.
    Used by AudioClipBase to sync it to the Edit.
*/
class LoopInfo
{
public:
    /** Creates an empty LoopInfo. */
    LoopInfo (Engine&);

    /** Creates a copy of another LoopInfo. */
    LoopInfo (const LoopInfo&);

    /** Creates a LoopInfo from some existing state.
        N.B. this references the given state, it doesn't make a deep copy.
    */
    LoopInfo (Engine&, const juce::ValueTree&, juce::UndoManager*);

    /** Creates a LoopInfo for an audio file. */
    LoopInfo (Engine&, const juce::File&);

    /** Creates a LoopInfo for a reader. */
    LoopInfo (Engine&, const juce::AudioFormatReader*, const juce::AudioFormat*);

    /** Creates a copy of another LoopInfo. */
    LoopInfo& operator= (const LoopInfo&);

    //==============================================================================
    /** Returns the tempo of the object. */
    double getBpm (const AudioFileInfo&) const;

    /** Returns the tempo of the object. */
    double getBeatsPerSecond (const AudioFileInfo&) const;

    /** Sets the tempo of the object. */
    void setBpm (double newBpm, const AudioFileInfo&);

    //==============================================================================
    /** Returns the denominator of the object. */
    int getDenominator() const;

    /** Returns the numerator of the object. */
    int getNumerator() const;

    /** Sets the denominator of the object. */
    void setDenominator (int newDenominator);

    /** Sets the numerator of the object. */
    void setNumerator (int newNumerator);

    /** Returns the number of beats. */
    double getNumBeats() const;

    /** Sets the number of beats. */
    void setNumBeats (double newNumBeats);

    //==============================================================================
    /** Returns true if this can be looped. */
    bool isLoopable() const;

    /** Returns true if this can be is a one-shot e.g. a single drum hit and therefore shouldn't be looped. */
    bool isOneShot() const;

    //==============================================================================
    /** Returns the root note of the object. */
    int getRootNote() const;

    /** Sets the root note of the object. */
    void setRootNote (int note);

    //==============================================================================
    /** Returns the sample number used as the start point in the file. */
    SampleCount getInMarker() const;

    /** Returns the sample number used as the end point in the file. */
    SampleCount getOutMarker() const;

    /** Sets the sample number to be used as the start point in the file. */
    void setInMarker (SampleCount);

    /** Sets the sample number to be used as the end point in the file. */
    void setOutMarker (SampleCount);

    //==============================================================================
    /** Enum to represet the type of loop point. */
    enum class LoopPointType
    {
        manual = 0, /**< A manual loop point */
        automatic   /**< An automatoc loop point */
    };

    struct LoopPoint
    {
        SampleCount pos = 0;                        /**< The sample position of the loop point. */
        LoopPointType type = LoopPointType::manual; /**< The type of the loop point. */

        /** Returns true if this is an automatic loop point. */
        bool isAutomatic() const        { return type == LoopPointType::automatic; }
    };

    /** Returns the number of loop points in the object. */
    int getNumLoopPoints() const;

    /** Returns the loop points at the given index. */
    LoopPoint getLoopPoint (int index) const;

    /** Adds a loop point at the given position. */
    void addLoopPoint (SampleCount, LoopPointType);

    /** Sets the loop point at the given index to a new position and type. */
    void changeLoopPoint (int index, SampleCount, LoopPointType);

    /** Removes the loop point at the given index. */
    void deleteLoopPoint (int index);

    /** Removes all the loop points. */
    void clearLoopPoints();

    /** Removes all the loop points of a given type. */
    void clearLoopPoints (LoopPointType);

    //==============================================================================
    /** Returns the number of tags. */
    int getNumTags() const;

    /** Removes all the tags. */
    void clearTags();

    /** Returns the tag at a given index. */
    juce::String getTag (int index) const;

    /** Adds a tag. This could be descriptive info like major/minor etc. */
    void addTag (const juce::String& tag);

    /** Adds multiple tags. @see addTag */
    void addTags (const juce::StringArray& tags);

    Engine& engine;         /**< The engine this belongs to. */
    juce::ValueTree state;  /**< @internal */

private:
    juce::CriticalSection lock;
    juce::UndoManager* um = nullptr;
    bool maintainParent = false;

    void init (const juce::AudioFormatReader*, const juce::AudioFormat*);
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

}} // namespace tracktion { inline namespace engine
