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
    A MacroParameter is an AutomatableParameter which is a collection of
    Mappings. A Mapping is a reference to an AutomatableParameter with a range and curve.
*/
class MacroParameter : public AutomatableParameter,
                       public AutomatableParameter::ModifierSource
{
public:
    //==============================================================================
    /**
        An Assignment between a MacroParameter and an AutomatableParameter.
    */
    struct Assignment  : public AutomatableParameter::ModifierAssignment
    {
        Assignment (const juce::ValueTree&, const MacroParameter&);

        using Ptr = juce::ReferenceCountedObjectPtr<Assignment>;

        bool isForModifierSource (const AutomatableParameter::ModifierSource&) const override;

        const EditItemID macroParamID;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Assignment)
    };

    //==============================================================================
    MacroParameter (AutomatableEditItem&, Edit&, const juce::ValueTree&);
    ~MacroParameter();

    using Ptr = juce::ReferenceCountedObjectPtr<MacroParameter>;

    void initialise();

    juce::String getParameterName() const override         { return macroName; }

    Edit& edit;
    juce::ValueTree state;
    juce::CachedValue<float> value;
    juce::CachedValue<juce::String> macroName;

private:
    virtual void parameterChanged (float, bool byAutomation) override;
};

/** Searched the Edit for a MacroParameter with this ID. */
MacroParameter::Ptr getMacroParameterForID (Edit&, EditItemID);

//==============================================================================
/** */
class MacroParameterList    : public AutomatableEditItem,
                              public juce::ChangeBroadcaster
{
public:
    MacroParameterList (Edit&, const juce::ValueTree&);
    ~MacroParameterList();

    MacroParameter* createMacroParameter();
    void removeMacroParameter (MacroParameter&);
    void hideMacroParametersFromTracks() const;

    juce::ReferenceCountedArray<MacroParameter> getMacroParameters() const;

    juce::String getName() override     { return {}; }
    Track* getTrack() const;

    //==============================================================================
    void restorePluginStateFromValueTree (const juce::ValueTree&) override {}

    juce::ValueTree state;

private:
    struct List;
    juce::ValueTree parent;
    std::unique_ptr<List> list;
};

/** If this MacroParameterList belongs to an Plugin, this will return it. */
juce::ReferenceCountedObjectPtr<Plugin> getOwnerPlugin (MacroParameterList*);

//==============================================================================
/**
    Base class for elements which can contain macro parameters.
*/
class MacroParameterElement
{
public:
    /** Constructor. */
    MacroParameterElement (Edit&, const juce::ValueTree&);

    /** Destructor. */
    virtual ~MacroParameterElement() = default;

    /** Returns the number of macro parameters for this object. */
    int getNumMacroParameters() const                   { return macroParameterList.getMacroParameters().size(); }

    MacroParameterList macroParameterList;
};

} // namespace tracktion_engine
