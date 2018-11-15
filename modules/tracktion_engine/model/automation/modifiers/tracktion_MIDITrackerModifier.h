/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
{

/** */
class MIDITrackerModifier   : public Modifier,
                              private ValueTreeAllEventListener
{
public:
    //==============================================================================
    MIDITrackerModifier (Edit&, const juce::ValueTree&);
    ~MIDITrackerModifier();

    using Ptr = juce::ReferenceCountedObjectPtr<MIDITrackerModifier>;
    using Array = juce::ReferenceCountedArray<MIDITrackerModifier>;

    void initialise() override;
    juce::String getName() override                     { return TRANS("MIDI Tracker Modifier"); }

    //==============================================================================
    /** Returns the current value of the modifier. */
    float getCurrentValue() override;

    /** Returns the MIDI value, either a pitch or velocity. */
    int getCurrentMIDIValue() const noexcept;

    AutomatableParameter::ModifierAssignment* createAssignment (const juce::ValueTree&) override;

    juce::StringArray getMidiInputNames() override;
    AudioNode* createPreFXAudioNode (AudioNode*) override;

    void applyToBuffer (const AudioRenderContext&) override;

    //==============================================================================
    struct Assignment   : public AutomatableParameter::ModifierAssignment
    {
        Assignment (const juce::ValueTree&, const MIDITrackerModifier&);

        bool isForModifierSource (const ModifierSource&) const override;
		juce::ReferenceCountedObjectPtr<MIDITrackerModifier> getMIDITrackerModifier() const;

        const EditItemID midiTrackerModifierID;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Assignment)
    };

    //==============================================================================
    juce::String getSelectableDescription() override    { return getName(); }

    //==============================================================================
    enum Type
    {
        pitch          = 0,
        velocity       = 1
    };

    static juce::StringArray getTypeNames();

    enum Mode
    {
        absolute       = 0,
        relative       = 1
    };

    static juce::StringArray getModeNames();

    /** Returns a ValueTree representing the absolute nodes of the map.
        This will have 5 children each with a midi and value property which you can adjust.
    */
    juce::ValueTree getNodeState() const                { return nodeState; }

    juce::CachedValue<float> type, mode, relativeRoot, relativeSpread;

    AutomatableParameter::Ptr typeParam, modeParam, relativeRootParam, relativeSpreadParam;

private:
    struct ModifierAudioNode;

    juce::ValueTree nodeState;

    LambdaTimer changedTimer;
    std::atomic<float> currentModifierValue { 0.0f };
    std::atomic<int> currentMIDIValue { 0 };

    struct Map { std::pair<int, float> node[5]; } currentMap;
    const juce::SpinLock mapLock;

    void refreshCurrentValue();
    void updateMapFromTree();
    void updateValueFromMap (int);

    void midiEvent (const juce::MidiMessage&);

    void valueTreeChanged() override;
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;
};

} // namespace tracktion_engine
