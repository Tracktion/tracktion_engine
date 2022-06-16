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
class QuantisationType  : private juce::ValueTree::Listener
{
public:
    //==============================================================================
    QuantisationType();
    QuantisationType (const juce::ValueTree&, juce::UndoManager*);
    ~QuantisationType() override;

    QuantisationType (const QuantisationType&);
    QuantisationType& operator= (const QuantisationType&);
    bool operator== (const QuantisationType&) const;

    void applyQuantisationToSequence (juce::MidiMessageSequence&, Edit&, TimePosition sequenceStartOffset);

    //==============================================================================
    /** this type may represent "no quantising" */
    bool isEnabled() const;

    static juce::StringArray getAvailableQuantiseTypes (bool translated);
    static juce::String getDefaultType (bool translated);

    void setType (const juce::String& newTypeName)          { typeName = newTypeName; }
    juce::String getType (bool translated) const;

    /** Returns the TimecodeSnapType level for the current quantisation type.
        Because the TimecodeSnapType level means something different if the time sig is
        triplets, this also needs to return the triplet type.
    */
    int getTimecodeSnapTypeLevel (bool& isTriplet) const noexcept;

    /** the proportion 0..1 of the amount to quantise stuff by. */
    float getProportion() const                             { return proportion; }
    void setProportion (float prop);

    /** If false, quantising a note on will keep the note its original length.
        If true (the default), note offs will be quantised to the same grid.
    */
    bool isQuantisingNoteOffs() const                       { return quantiseNoteOffs; }
    void setIsQuantisingNoteOffs (bool isQuantising)        { quantiseNoteOffs = isQuantising; }

    //==============================================================================
    BeatPosition roundBeatToNearest (BeatPosition beatNumber) const;
    BeatPosition roundBeatUp (BeatPosition beatNumber) const;
    BeatPosition roundBeatToNearestNonZero (BeatPosition beatNumber) const;

    TimePosition roundToNearest (TimePosition, const Edit&) const;
    TimePosition roundUp (TimePosition, const Edit&) const;

    juce::ValueTree state;
    juce::CachedValue<juce::String> typeName;
    juce::CachedValue<float> proportion;
    juce::CachedValue<bool> quantiseNoteOffs;

private:
    int typeIndex = 0;
    double fractionOfBeat = 0;

    void initialiseCachedValues (juce::UndoManager*);

    void updateType();
    void updateFraction();
    TimePosition roundTo (TimePosition, double adjustment, const Edit&) const;
    BeatPosition roundToBeat (BeatPosition beatNumber, double adjustment) const;

    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;
    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override {}
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override {}
    void valueTreeChildOrderChanged (juce::ValueTree&, int, int) override {}
    void valueTreeParentChanged (juce::ValueTree&) override {}
};

}} // namespace tracktion { inline namespace engine
