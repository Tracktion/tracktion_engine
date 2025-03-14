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
                                public AutomatableParameter::ModifierSource,
                                private juce::ValueTree::Listener
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
    /** Returns the position this curve occupies.
        N.B. This is dynamic and is usually determined by an owning clip position or
        launched clip start time. If you're drawing the curve, you should use the raw
        times of the curve points.

        In linked mode, this generally relates to the clip's position.
        In unlinked/free mode the loop and start/length and loop start/length
        determine the curve's position.
    */
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
    /** Listener interface.
        N.B. Callbacks happen synchronously so you shouldn't block for long.
    */
    struct Listener
    {
        /** Destructor. */
        virtual ~Listener() = default;

        /** Called when something that affects the CurvePosition has changed so any
            cached iterators etc. will need to be refreshed.
        */
        virtual void positionChanged() = 0;

        /** Called when one of the points on one of the curves changes. */
        virtual void curveChanged() = 0;
    };

    /** Adds a listener. */
    void addListener (Listener&);

    /** Removes a previously added listener. */
    void removeListener (Listener&);

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

    //==============================================================================
    /** @internal */
    void callPositionChangedListeners();

private:
    AutomationCurve absoluteCurve { edit, AutomationCurve::TimeBase::beats };
    AutomationCurve relativeCurve { edit, AutomationCurve::TimeBase::beats };
    AutomationCurve scaleCurve { edit, AutomationCurve::TimeBase::beats };

    std::function<CurvePosition()> getPositionDelegate;
    LambdaValueTreeAllEventListener stateListener { state, [this] { changed(); listeners.call (&Listener::curveChanged);  } };
    juce::ListenerList<Listener> listeners;
};

/** Contains the base and modifier values if they are active. */
struct BaseAndModValue
{
    std::optional<float> baseValue;
    std::optional<float> modValue;
};

/** Returns the base and modifier values this AutomationCurveModifier will apply at the given position.
    N.B. The position is relative to the the Edit, this internally applies any CurvePosition.
    @see AutomationCurveModifier::getPosition()
    [[ message_thread ]]
*/
[[nodiscard]] BaseAndModValue getValuesAtEditPosition (AutomationCurveModifier&, AutomatableParameter&, EditPosition);

[[nodiscard]] BaseAndModValue getValuesAtCurvePosition (AutomationCurveModifier&, AutomatableParameter&, EditPosition);

//ddd [[nodiscard]] BaseAndModValue getValuesAtCurvePositionUnlooped (AutomationCurveModifier&, AutomatableParameter&, EditPosition);

//==============================================================================
/** Returns the destination parameter this curve is controlling. */
[[nodiscard]] AutomatableParameter::Ptr getParameter (const AutomationCurveModifier&);

/** Returns the AutomationCurveModifier with the given EditItemID. */
[[nodiscard]] AutomationCurveModifier::Ptr getAutomationCurveModifierForID (Edit&, EditItemID);


//==============================================================================
//==============================================================================
class AutomationCurveList
{
public:
    AutomationCurveList (Edit&, const juce::ValueTree&,
                         ValueTreePropertyChangedListener,
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


//==============================================================================
//        _        _           _  _
//     __| |  ___ | |_   __ _ (_)| | ___
//    / _` | / _ \| __| / _` || || |/ __|
//   | (_| ||  __/| |_ | (_| || || |\__ \ _  _  _
//    \__,_| \___| \__| \__,_||_||_||___/(_)(_)(_)
//
//   Code beyond this point is implementation detail...
//
//==============================================================================

namespace detail
{
    void processValue (AutomatableParameter&, CurveModifierType, float curveValue, float& baseValue, float& modValue);
}

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
