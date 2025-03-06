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

/** Contains the ID of an AutomatableEditItem and the paramID. */
struct AutomatableParameterID
{
    EditItemID automatableEditItemID;
    juce::String paramID;
};

enum class CurveModifierType
{
    absolute,
    relative,   // additive
    scale       // multiplicative
};

struct CurvePosition
{
    EditPosition curveStart;
    EditTimeRange clipRange;
};

class AutomationCurveModifier : public juce::ReferenceCountedObject,
                                public EditItem,
                                public Selectable,
                                public AutomatableParameter::ModifierSource
{
public:
    AutomationCurveModifier (Edit&,
                             const juce::ValueTree&,
                             AutomatableParameterID destID,
                             std::function<CurvePosition()> getPositionDelegate);

    /** Destructor. */
    ~AutomationCurveModifier() override;

    using Ptr = juce::ReferenceCountedObjectPtr<AutomationCurveModifier>;
    using Array = juce::ReferenceCountedArray<AutomationCurveModifier>;

    /** Returns the curve. */
    AutomationCurve& getCurve();
    // - absolute/relative(additive)/scale(multiplicative)
    // - linked
    // - start/length
    // - looped
    // - loop start/length

    /** Returns the position this curve occupies. */
    CurvePosition getPosition() const;

    /** Remove/deletes this curve from its parent list. */
    void remove();

    /** @internal */
    juce::ValueTree state;

    /** The ID of the AutomatableParameter this curve is modifying. */
    const AutomatableParameterID destID;

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
    AutomationCurve curve;
    std::function<CurvePosition()> getPositionDelegate;
    //ddd listen to changes in curve and update AutomationIterator
};

AutomatableParameter::Ptr getParameter (const AutomationCurveModifier&);

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
