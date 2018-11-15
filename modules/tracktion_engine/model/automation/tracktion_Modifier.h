/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
{

/** Base class for objects which need to know about the global Edit time every block.
    The Edit holds an array of these which will be called before every render callback.
*/
struct ModifierTimer
{
    virtual ~ModifierTimer() = default;
    virtual void updateStreamTime (PlayHead&, EditTimeRange streamTime, int numSamples) = 0;
};

//==============================================================================
struct Modifier : public AutomatableEditItem,
                  public Selectable,
                  public AutomatableParameter::ModifierSource,
                  public juce::ReferenceCountedObject
{
    using Ptr = juce::ReferenceCountedObjectPtr<Modifier>;
    using Array = juce::ReferenceCountedArray<Modifier>;

    Modifier (Edit&, const juce::ValueTree&);
    ~Modifier();

    void remove();

    /** Call this once after construction to connect it to the audio graph. */
    virtual void initialise() = 0;

    /** Must return the current value of the modifier. */
    virtual float getCurrentValue() = 0;

    /** Must return a new ModifierAssignment for a given state. */
    virtual AutomatableParameter::ModifierAssignment* createAssignment (const juce::ValueTree&) = 0;

    //==============================================================================
    /** Can return an array of names represeting MIDI inputs. */
    virtual juce::StringArray getMidiInputNames()               { return {}; };

    /** Can return an array of names represeting audio inputs. */
    virtual juce::StringArray getAudioInputNames()              { return {}; };

    virtual AudioNode* createPreFXAudioNode (AudioNode* input)  { return input; }
    virtual AudioNode* createPostFXAudioNode (AudioNode* input) { return input; }

    virtual void initialise (const PlaybackInitialisationInfo&) {}
    virtual void deinitialise() {}
    virtual void applyToBuffer (const AudioRenderContext&) {}

    //==============================================================================
    bool baseClassNeedsInitialising() const noexcept        { return initialiseCount == 0; }
    void baseClassInitialise (const PlaybackInitialisationInfo&);
    void baseClassDeinitialise();

    //==============================================================================
    static juce::StringArray getEnabledNames();

    //==============================================================================
    juce::ValueTree state;
    juce::CachedValue<juce::Colour> colour;

    juce::CachedValue<float> enabled;
    AutomatableParameter::Ptr enabledParam;

private:
    int initialiseCount = 0;
};

//==============================================================================
class ModifierList  : public juce::ChangeBroadcaster,
                      private ValueTreeObjectList<Modifier>
{
public:
    ModifierList (Edit&, const juce::ValueTree&);
    ~ModifierList();

    static bool isModifier (const juce::Identifier&);

    juce::ReferenceCountedArray<Modifier> getModifiers() const;

    //==============================================================================
    juce::ReferenceCountedObjectPtr<Modifier> insertModifier (juce::ValueTree, int index, SelectionManager*);

    //==============================================================================
    bool isSuitableType (const juce::ValueTree&) const override;
    Modifier* createNewObject (const juce::ValueTree&) override;
    void deleteObject (Modifier*) override;

    void newObjectAdded (Modifier*) override;
    void objectRemoved (Modifier*) override;
    void objectOrderChanged() override;

    Edit& edit;
    juce::ValueTree state;
};

//==============================================================================
/** Returns the modifers of a specific type. */
template<typename ModifierType>
juce::ReferenceCountedArray<ModifierType> getModifiersOfType (const ModifierList& ml)
{
    juce::ReferenceCountedArray<ModifierType> mods;

    for (auto m : ml.getModifiers())
        if (auto type = dynamic_cast<ModifierType*> (m))
            mods.add (type);

    return mods;
}

/** Returns a Modifier if it can be found in the list. */
Modifier::Ptr findModifierForID (ModifierList&, EditItemID);

} // namespace tracktion_engine
