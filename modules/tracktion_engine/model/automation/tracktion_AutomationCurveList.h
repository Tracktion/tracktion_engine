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
template<int minBeats>
struct MinBeatConstrainer
{
    template<typename T>
    static BeatDuration constrain (const T& v)
    {
        return std::max (v, BeatDuration::fromBeats (minBeats));
    }

    static BeatDuration constrain (const juce::var& v)
    {
        return constrain (juce::VariantConverter<BeatDuration>::fromVar (v));
    }
};

//==============================================================================
//==============================================================================
/** Contains the ID of an AutomatableEditItem and the paramID. */
struct AutomatableParameterID
{
    AutomatableParameterID() = default;
    AutomatableParameterID (EditItemID a, juce::String p) : automatableEditItemID (a), paramID (p) {}
    
    EditItemID automatableEditItemID;
    juce::String paramID;

    bool operator== (const AutomatableParameterID& other) const
    {
        return automatableEditItemID == other.automatableEditItemID && paramID == other.paramID;
    }
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

/** Holds information about the position of a clip. */
struct ClipPositionInfo
{
    ClipPositionBeats position;         /**< The position in beats. */
    std::optional<BeatRange> loopRange; /**< If the clip is looping, the start/length of the loop range. */
};

//==============================================================================
/** Holds a playhead for an AutomationCurveModifier type.
    This it automatically updated by the playback graph so UI should only read from it.
*/
struct AutomationCurvePlayhead
{
    /** Returns the last played position relative to the AutomationCurve if active. */
    std::optional<EditPosition> getPosition() const;

private:
    friend class AutomationCurveModifierSource;
    crill::seqlock_object<std::optional<EditPosition>> position;
};

//==============================================================================
//==============================================================================
/**
    An AutomationCurveModifier contains three curves to control automation:
    absolute, relative and scale.
    The first sets the base value of a parameter and the latter two the modifier value.
    Each curve has independent timing information so complex patterns can be set
    up with multiple curves looping at different intervals.
*/
class AutomationCurveModifier : public juce::ReferenceCountedObject,
                                public EditItem,
                                public Selectable,
                                public AutomatableParameter::ModifierSource,
                                private juce::ValueTree::Listener
{
public:
    //==============================================================================
    /** Constructs an AutomationCurveModifier for a given state and destination.
        Don't construct one of these directly use AutomationCurveList::addCurve
        @see AutomationCurveList for a detailed description of the delegates.
    */
    AutomationCurveModifier (Edit&,
                             const juce::ValueTree&,
                             AutomatableParameterID destID,
                             std::function<CurvePosition()> getCurvePositionDelegate,
                             std::function<ClipPositionInfo()> getClipPositionDelegate);

    /** Destructor. */
    ~AutomationCurveModifier() override;

    using Ptr = juce::ReferenceCountedObjectPtr<AutomationCurveModifier>;
    using Array = juce::ReferenceCountedArray<AutomationCurveModifier>;

    /** The ID of the AutomatableParameter this curve is modifying. */
    AutomatableParameterID getDestID() const;

    /** Re-assigns this to a new destination.
        The paramID has to be the same (as curve ranges etc. won't apply otherwise).
        N.B. There are limitations to what parameters can be assigned to (such as if
        they're on the same track etc.) so this returns false if the assignment
        couldn't be made.
    */
    bool setDestination (AutomatableEditItem&);

    //==============================================================================
    /** Holds the timing properties of a curve. */
    struct CurveTiming
    {
        juce::CachedValue<AtomicWrapperRelaxed<bool>> unlinked;
        juce::CachedValue<AtomicWrapperRelaxed<BeatPosition>> start;
        juce::CachedValue<AtomicWrapperRelaxed<BeatDuration, MinBeatConstrainer<1>>> length;

        juce::CachedValue<AtomicWrapperRelaxed<bool>> looping;
        juce::CachedValue<AtomicWrapperRelaxed<BeatPosition>> loopStart;
        juce::CachedValue<AtomicWrapperRelaxed<BeatDuration, MinBeatConstrainer<1>>> loopLength;
    };

    /** Returns the timing properties for a curve type. */
    CurveTiming& getCurveTiming (CurveModifierType);

    /** Holds the curve and limits of a curve type. */
    struct CurveInfo
    {
        CurveModifierType type;     /**< The type of curve. */
        AutomationCurve& curve;     /**< The actual curve. */
        juce::Range<float> limits;  /**< The limits of this curve. */
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

    /** Returns the position info of the owning clip. I.e. the offset and loop properties.
        These are used in "linked" mode to determine how the curve playhead loops.
    */
    ClipPositionInfo getClipPositionInfo() const;

    /** Remove/deletes this curve from its parent list. */
    void remove();

    //==============================================================================
    /** Returns a playhead for the automation curve. */
    std::shared_ptr<AutomationCurvePlayhead> getPlayhead (CurveModifierType);

    //==============================================================================
    /** Listener interface.
        N.B. Callbacks happen synchronously so you shouldn't block for long.
    */
    struct Listener
    {
        /** Destructor. */
        virtual ~Listener() = default;

        /** Called when one of the points on one of the curves changes. */
        virtual void curveChanged() = 0;

        /** Called when one of the curves CurveTiming::unlinked properties changes. */
        virtual void unlinkedStateChanged (CurveModifierType) {}
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
    void setPositionDelegate (std::function<CurvePosition()>);

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
    juce::ValueTree state;

private:
    AutomatableParameterID destID;

    AutomationCurve absoluteCurve { edit, AutomationCurve::TimeBase::beats };
    AutomationCurve relativeCurve { edit, AutomationCurve::TimeBase::beats };
    AutomationCurve scaleCurve { edit, AutomationCurve::TimeBase::beats };
    std::array<CurveTiming, 3> curveTimings;
    std::array<std::shared_ptr<AutomationCurvePlayhead>, 3> playheads;
    juce::Range<float> absoluteLimits;
    bool updateLimits = true;

    mutable RealTimeSpinLock positionDelegateMutex;
    std::function<CurvePosition()> getPositionDelegate;
    std::function<ClipPositionInfo()> getClipPositionDelegate;

    LambdaValueTreeAllEventListener stateListener { state };
    juce::ListenerList<Listener> listeners;

    void curveUnlinkedStateChanged (juce::ValueTree&);
    void updateModifierAssignment();
};

//==============================================================================
/** Contains the base and modifier values if they are active. */
struct BaseAndModValue
{
    std::optional<float> baseValue;
    std::optional<float> modValue;
};

/** Returns the base and modifier values this AutomationCurveModifier will apply at the given position.
    N.B. The position is relative to the the Edit, this internally applies any CurvePosition/CurveTiming.
    @see AutomationCurveModifier::getPosition()
    [[ message_thread ]]
*/
[[nodiscard]] BaseAndModValue getValuesAtEditPosition (AutomationCurveModifier&, AutomatableParameter&, EditPosition);

/** Returns the base and modifier values this AutomationCurveModifier will apply at the given position.
    N.B. The position is relative to the the AutomationCurveModifier, this
    internally applies any curve timing such as start/end and looping.
    [[ message_thread ]]
*/
[[nodiscard]] BaseAndModValue getValuesAtCurvePosition (AutomationCurveModifier&, AutomatableParameter&, EditPosition);

/** Similar to getValuesAtCurvePosition but doesn't apply the looping which can be useful when drawing the curves.
    [[ message_thread ]]
*/
[[nodiscard]] BaseAndModValue getValuesAtCurvePositionUnlooped (AutomationCurveModifier&, AutomatableParameter&, EditPosition);

//==============================================================================
/** Takes a position relative to the AutomationCurveModifier and applies the
    CurveTiming to it, giving a final position relative to the AutomationCurve.
    This effectively applies either the looping or start/length clipping.
    If the curve is in linked mode, this just returns the position unchanged.

    You shouldn't normally need to use this, it's mainly for the playback graph.
    [[thread_safe]]
*/
[[nodiscard]] std::optional<EditPosition> applyTimingToCurvePosition (AutomationCurveModifier&, CurveModifierType, EditPosition);

/** Converts a position relative to the Edit  and applies the
    CurvePosition and CurveTiming to it, giving a final position relative to
    the AutomationCurve.

    You shouldn't normally need to use this, it's mainly for the playback graph.
    [[thread_safe]]
*/
[[nodiscard]] std::optional<EditPosition> editPositionToCurvePosition (AutomationCurveModifier&, CurveModifierType, EditPosition);

/** Returns the default value used if there is no curve for the given type. */
[[nodiscard]] float getDefaultValue (AutomatableParameter&, CurveModifierType);

//==============================================================================
/** Returns true if any of the curves have any points. */
[[nodiscard]] bool hasAnyPoints (AutomationCurveModifier&);

/** Returns the destination parameter this curve is controlling. */
[[nodiscard]] AutomatableParameter::Ptr getParameter (const AutomationCurveModifier&);

/** Returns the AutomationCurveModifier with the given EditItemID. */
[[nodiscard]] AutomationCurveModifier::Ptr getAutomationCurveModifierForID (Edit&, EditItemID);


//==============================================================================
//==============================================================================
/**
    Manages a list of AutomationCurveModifiers which can be used to control the
    values and modifiers of AutomatableParameters.
*/
class AutomationCurveList
{
public:
    /** Constructs an AutomationCurveList for the given state.
        Two delegates should be provided which will be passes to the AutomationCurveModifiers.
        These should be real-time and thread-safe as they're called by multiple threads,
        including the audio thread.
        The CurvePosition should return where in the Edit this curve should be started at
        The ClipPositionInfo should return the clip's offset and loop range (if looping)
    */
    AutomationCurveList (Edit&, const juce::ValueTree&,
                         std::function<CurvePosition()> getPositionDelegate,
                         std::function<ClipPositionInfo()> getClipPositionDelegate);
    /** Destructor. */
    ~AutomationCurveList();

    /** Returns the list of AutomationCurveModifiers. */
    std::vector<AutomationCurveModifier::Ptr> getItems();

    /** Adds a new curve modifier assigned to destParam. */
    AutomationCurveModifier::Ptr addCurve (const AutomatableParameter& destParam);

    /** Removes the curve at the given index. */
    void removeCurve (int idx);

    //==============================================================================
    /**
        Listener interface.
    */
    struct Listener
    {
        virtual ~Listener() = default;

        /** Called when AutomationCurveModifier are added/removed or reordered.
            call AutomationCurveList::getItems() to get the new order.
        */
        virtual void itemsChanged() = 0;
    };

    /** Adds a Listener. */
    void addListener (Listener&);

    /** Removes a Listener. */
    void removeListener (Listener&);

private:
    class List;
    std::unique_ptr<List> list;
    juce::ListenerList<Listener> listeners;
};

//==============================================================================
/** Looks at the destinations on the given AutomationCurveModifier and tries to
    re-assign parameters to corresponding parameters via the newParent Clip.
    You shouldn't need to call this manually, it's done in several key places internally.
*/
void updateRelativeDestinationOrRemove (AutomationCurveList&, AutomationCurveModifier&, Clip& newParent);

/** Iterates over the given tree and gives new IDs to any AutomationCurveModifier states.
    You shouldn't need to call this manually, it's done automatically when clips are split.
    @see split
*/
void assignNewIDsToAutomationCurveModifiers (Edit&, juce::ValueTree&);

/** Removes any assignments in the given state if they don't have the given parameter as the destination.
    You shouldn't need to call this manually, it's used internally to clean up after copy/paste operations etc..
*/
void removeInvalidAutomationCurveModifiers (juce::ValueTree&, const AutomatableParameter&);

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
