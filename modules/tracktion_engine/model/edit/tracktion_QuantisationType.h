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
class QuantisationType  : private juce::ValueTree::Listener
{
public:
    //==============================================================================
    QuantisationType();
    QuantisationType (const juce::ValueTree&, juce::UndoManager*);
    ~QuantisationType();

    QuantisationType (const QuantisationType&);
    QuantisationType& operator= (const QuantisationType&);

    void applyQuantisationToSequence (juce::MidiMessageSequence&, Edit&, double sequenceStartOffset);

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
    double roundBeatToNearest (double beatNumber) const;
    double roundBeatUp (double beatNumber) const;
    double roundBeatToNearestNonZero (double beatNumber) const;

    double roundToNearest (double time, const Edit& edit) const;
    double roundUp (double time, const Edit& edit) const;

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
    double roundTo (double time, double adjustment, const Edit&) const;
    double roundToBeat (double beatNumber, double adjustment) const;

    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;
    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override {}
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override {}
    void valueTreeChildOrderChanged (juce::ValueTree&, int, int) override {}
    void valueTreeParentChanged (juce::ValueTree&) override {}
};

} // namespace tracktion_engine
