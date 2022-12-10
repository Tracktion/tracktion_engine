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

/** Base class for objects which need to know about the global Edit time every block.
    The Edit holds an array of these which will be called before every render callback.
*/
struct ModifierTimer
{
    virtual ~ModifierTimer() = default;
    virtual void updateStreamTime (TimePosition editTime, int numSamples) = 0;
};

//==============================================================================
/**
    Bass class for parameter Modifiers.
    
    Modifiers are capable of controlling automatable parameters by making assignments
    from Modifiers to AutomatableParameters.
    They work similarly to AutomationCurves but can be all kinds of modification such
    as LFOs, random and envelopes.
 
    @see LFOModifier, EnvelopeFollowerModifier, RandomModifier
*/
struct Modifier : public AutomatableEditItem,
                  public Selectable,
                  public AutomatableParameter::ModifierSource,
                  public juce::ReferenceCountedObject
{
    using Ptr = juce::ReferenceCountedObjectPtr<Modifier>;
    using Array = juce::ReferenceCountedArray<Modifier>;

    /** Creates a Modifier for a given state. */
    Modifier (Edit&, const juce::ValueTree&);
    /** Destructor. */
    ~Modifier();

    /** Removes this Modifier from its parent Track. */
    void remove();

    /** Call this once after construction to connect it to the audio graph. */
    virtual void initialise() = 0;

    /** Must return the current value of the modifier.
        @see getValueAt, getValues
    */
    virtual float getCurrentValue() = 0;

    /** Must return a new ModifierAssignment for a given state. */
    virtual AutomatableParameter::ModifierAssignment* createAssignment (const juce::ValueTree&) = 0;

    //==============================================================================
    /** Can return an array of names represeting MIDI inputs. */
    virtual juce::StringArray getMidiInputNames()               { return {}; }

    /** Can return an array of names represeting audio inputs. */
    virtual juce::StringArray getAudioInputNames()              { return {}; }

    /** Determines the position in the FX chain where the modifier should be processed. */
    enum class ProcessingPosition
    {
        none,   /**< The Modifier needs no processing. */
        preFX,  /**< The Modifier is processed before the plugn chain. */
        postFX  /**< The Modifier is processed after the plugn chain. */
    };
    
    /** Should return the position in the plugin chain that this Modifier should be processed. */
    virtual ProcessingPosition getProcessingPosition()          { return ProcessingPosition::none; }

    //==============================================================================
    /** Sub classes should implement this to initialise the Modifier. */
    virtual void initialise (double /*sampleRate*/, int /*blockSizeSamples*/) {}
    /** Sub classes should implement this to deinitialise the Modifier. */
    virtual void deinitialise() {}
    /** Sub classes should implement this to process the Modifier. */
    virtual void applyToBuffer (const PluginRenderContext&) {}

    //==============================================================================
    /** Returns true if the Modifier needs initialising. */
    bool baseClassNeedsInitialising() const noexcept            { return initialiseCount == 0; }
    /** Initialises the Modifier. */
    void baseClassInitialise (double sampleRate, int blockSizeSamples);
    /** Deinitialises the Modifier. */
    void baseClassDeinitialise();
    /** Updates internal value history and calls the subclass's applyToBuffer method. */
    void baseClassApplyToBuffer (const PluginRenderContext&);

    //==============================================================================
    /** The max number of seconds of modifier value history that is stored. */
    static constexpr TimeDuration maxHistoryTime = TimeDuration::fromSeconds (3.0);
    
    /** Returns the edit time of the current value.
        @see getCurrentValue, getValueAt
        [[ audio_thread ]]
    */
    TimePosition getCurrentTime() const;

    /** Returns the value of the at a given time in the past.
        [[ audio_thread ]]
    */
    float getValueAt (TimeDuration numSecondsBeforeNow) const;

    /** Returns a vector of previous sample values.
        N.B. you might not get back as many seconds as requested with numSecondsBeforeNow.
        [[ message_thread ]]
    */
    std::vector<float> getValues (TimeDuration numSecondsBeforeNow) const;
    
    //==============================================================================
    juce::ValueTree state;                  /**< Modifier internal state. */
    juce::CachedValue<juce::Colour> colour; /**< Colour property. */

    juce::CachedValue<float> enabled;       /**< Enabled property. */
    AutomatableParameter::Ptr enabledParam; /**< Parameter to change the enabled state. */

    /** Returns the sample rate the Modifier has been initialised with. */
    double getSampleRate() const                                { return sampleRate; }

protected:
    /** Subclasses can call this to update the edit time of the current value. */
    void setEditTime (TimePosition newEditTime)                 { lastEditTime = newEditTime; }
    
private:
    int initialiseCount = 0;
    double sampleRate = 44100.0;
    std::atomic<TimePosition> lastEditTime { TimePosition() };
    
    class ValueFifo;
    std::unique_ptr<ValueFifo> valueFifo, messageThreadValueFifo;
    mutable choc::fifo::SingleReaderSingleWriterFIFO<std::pair<int, float>> valueFifoQueue;
};

//==============================================================================
/**
    Holds a list of Modifiers that have been added to a Track.
*/
class ModifierList  : public juce::ChangeBroadcaster,
                      private ValueTreeObjectList<Modifier>
{
public:
    /** Creates a ModifierList for an Edit and given state.
        Usually this is created by a Track @see Track::getModifierList.
    */
    ModifierList (Edit&, const juce::ValueTree&);
    /** Destructor. */
    ~ModifierList() override;

    /** Tests whether the Identifier is of a known Modifier type. */
    static bool isModifier (const juce::Identifier&);

    /** Returns all the Modifiers in the list. */
    juce::ReferenceCountedArray<Modifier> getModifiers() const;

    //==============================================================================
    /** Adds a Modifier from a state at a given index. */
    juce::ReferenceCountedObjectPtr<Modifier> insertModifier (juce::ValueTree, int index, SelectionManager*);

    //==============================================================================
    /** @internal */
    bool isSuitableType (const juce::ValueTree&) const override;
    /** @internal */
    Modifier* createNewObject (const juce::ValueTree&) override;
    /** @internal */
    void deleteObject (Modifier*) override;

    /** @internal */
    void newObjectAdded (Modifier*) override;
    /** @internal */
    void objectRemoved (Modifier*) override;
    /** @internal */
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

}} // namespace tracktion { inline namespace engine
