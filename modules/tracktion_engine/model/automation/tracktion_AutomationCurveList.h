/*
,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion::inline engine
{

//==============================================================================
//==============================================================================
/** Contains the ID of an AutomatableEditItem and the paramID. */
struct AutomatableParameterID
{
    EditItemID automatableEditItemID;
    juce::String paramID;
};


//==============================================================================
//==============================================================================
/** Determines the behaviour of a curve. */
enum class CurveModifierType
{
    absolute,   // overwrites
    relative,   // additive
    scale       // multiplicative
};

/** Converts a CurveModifierType to a string if possible. */
juce::String toString (CurveModifierType);

/** Converts a string to a CurveModifierType if possible. */
std::optional<CurveModifierType> curveModifierTypeFromString (juce::String);


//==============================================================================
//==============================================================================
/** A position of a curve in an Edit. */
struct CurvePosition
{
    EditPosition curveStart;    /**< The start position of the curve in the Edit (similar to an offset). */
    EditTimeRange clipRange;    /**< The valid range of the curve, it will be disabled outside this. */
};


//==============================================================================
//==============================================================================
class AutomationCurveModifier : public juce::ReferenceCountedObject,
                                public EditItem,
                                public Selectable,
                                public AutomatableParameter::ModifierSource
{
public:
    //==============================================================================
    AutomationCurveModifier (Edit&,
                             const juce::ValueTree&,
                             AutomatableParameterID destID,
                             std::function<CurvePosition()> getPositionDelegate);

    /** Destructor. */
    ~AutomationCurveModifier() override;

    using Ptr = juce::ReferenceCountedObjectPtr<AutomationCurveModifier>;
    using Array = juce::ReferenceCountedArray<AutomationCurveModifier>;

    //==============================================================================
    /** Holds the curve and limits of a curve type. */
    struct CurveInfo
    {
        CurveModifierType type;     /*< The type of curve. */
        AutomationCurve& curve;     /*< The actual curve. */
        juce::Range<float> limits;  /*< The limits of this curve. */
    };

    /** Returns the CurveInfo for a given curve type. */
    CurveInfo getCurve (CurveModifierType);

    //==============================================================================
    /** Returns the position this curve occupies. */
    CurvePosition getPosition() const;

    /** Remove/deletes this curve from its parent list. */
    void remove();

    //==============================================================================
    /** @internal */
    juce::ValueTree state;

    /** The ID of the AutomatableParameter this curve is modifying. */
    const AutomatableParameterID destID;

    // @todo:
    // - linked
    // - start/length
    // - looped
    // - loop start/length

    //==============================================================================
    /** @internal */
    juce::String getName() const override;
    /** @internal */
    juce::String getSelectableDescription() override;

    //==============================================================================
    /** @internal */
    class Assignment  : public AutomatableParameter::ModifierAssignment
    {
    public:
        Assignment (AutomationCurveModifier&, const juce::ValueTree&);

        /** Must return true if this assigment is for the given source. */
        bool isForModifierSource (const ModifierSource&) const override;

    private:
        const EditItemID automationCurveModifierID;
    };

private:
    AutomationCurve absoluteCurve { edit, AutomationCurve::TimeBase::beats };
    AutomationCurve relativeCurve { edit, AutomationCurve::TimeBase::beats };
    AutomationCurve scaleCurve { edit, AutomationCurve::TimeBase::beats };

    std::function<CurvePosition()> getPositionDelegate;
    LambdaValueTreeAllEventListener stateListener { state, [this] { changed(); } };
};

//==============================================================================
/** Returns the destination parameter this curve is controlling. */
AutomatableParameter::Ptr getParameter (const AutomationCurveModifier&);

/** Returns the AutomationCurveModifier with the given EditItemID. */
AutomationCurveModifier::Ptr getAutomationCurveModifierForID (Edit&, EditItemID);


//==============================================================================
//==============================================================================
class AutomationCurveList
{
public:
    AutomationCurveList (Edit&, const juce::ValueTree&,
                         std::function<CurvePosition()> getPositionDelegate);
    ~AutomationCurveList();

    std::vector<AutomationCurveModifier::Ptr> getItems();

    AutomationCurveModifier::Ptr addCurve (const AutomatableParameter& destParam);

    //==============================================================================
    struct Listener
    {
        virtual ~Listener() = default;

        /** Called when AutomationCurveModifier are added/removed or reordered.
            call AutomationCurveList::getItems() to get the new order.
        */
        virtual void itemsChanged() = 0;
    };

    void addListener (Listener&);
    void removeListener (Listener&);

private:
    class List;
    std::unique_ptr<List> list;
    juce::ListenerList<Listener> listeners;
};

} // namespace tracktion::inline engine


//==============================================================================
//==============================================================================
namespace juce
{
    template<>
    struct VariantConverter<tracktion::engine::CurveModifierType>
    {
        static tracktion::engine::CurveModifierType fromVar (const var& v)
        {
            using namespace tracktion::engine;
            return curveModifierTypeFromString (v.toString()).value_or (CurveModifierType::absolute);
        }

        static var toVar (tracktion::engine::CurveModifierType v)
        {
            return toString (v);
        }
    };
}
