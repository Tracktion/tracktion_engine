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
    Manages the Midi learn state and Listener interface to notify subclasses when the state changes.
*/
class MidiLearnState
{
public:
    MidiLearnState (Engine&);
    ~MidiLearnState();

    void setActive (bool shouldBeActive);
    bool isActive()                             { return learnActive; }

    enum ChangeType
    {
        added,
        removed
    };

    void assignmentChanged (ChangeType t);

    struct ScopedChangeCaller
    {
        ScopedChangeCaller (MidiLearnState&, ChangeType);
        ~ScopedChangeCaller();

        MidiLearnState& state;
        ChangeType type;
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ScopedChangeCaller)
    };

    //==============================================================================
    /** Subclass this to be notified when the MidiLearn state changes.
        No need to add yourself as a listener, these will be automatically added and
        removed with the lifetime of the class.
    */
    struct Listener
    {
        Listener (MidiLearnState&);
        virtual ~Listener();

        virtual void midiLearnStatusChanged (bool isActive) = 0;
        virtual void midiLearnAssignmentChanged (ChangeType) {}

        MidiLearnState& ownerState;
    };

    Engine& engine;

private:
    juce::ListenerList<Listener> listeners;
    bool learnActive = false;

    void addListener (Listener* l)      { listeners.add (l); }
    void removeListener (Listener* l)   { listeners.remove (l); }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiLearnState)
};

//==============================================================================
/** Base class for things that can be assigned to external control surfaces, not plugin paramters.

    Components can check the base class methods to see if they are currently assignable as
    this will depend on factors such as selected track ranges etc. If they are assignable they
    should provide some information as to what action they will be assigned to.
*/
class MidiAssignable
{
public:
    //==============================================================================
    struct Assignment
    {
        Assignment() : id (CustomControlSurface::none) {}
        Assignment (juce::String n, CustomControlSurface::ActionID i) : name (n), id (i) {}

        juce::String name;
        CustomControlSurface::ActionID id;
    };

    //==============================================================================
    MidiAssignable (Engine&);
    virtual ~MidiAssignable() {}

    /** Adds an assignment to the list. */
    void addAssignent (Assignment);

    /** This should return any assignments that should be shown by the pop-up component. */
    juce::Array<Assignment>& getAssignments()       { return assignemnts; }

    /** Should return true if only one action can be assigned from this control. */
    bool isSingleAssignment() const noexcept        { return assignemnts.size() == 1; }

    /** Builds a menu of actions that can be assigned by this control. The menu IDs should be
        the CustomControlSurface::ActionID they represent for easy assignment triggering.
    */
    void buildMenu (juce::PopupMenu&);

    /** If you've shown a custom menu this will be called with the result which you should handle appropriately.
        This uses the result number as a CustomControlSurface action Id and adds it to the
        currently focused edit's change handler.
    */
    void handleMenuResult (int menuResult);

    /** Returns the fader index which varies depending on the track num and selected bank etc.
        This is added on to track specific actions automatically when they are assigned.
    */
    int getFaderIndex();

    //==============================================================================
    /** Should return true if the control is currently assignable. */
    virtual bool isAssignable() = 0;

    /** Should return a brief description such as "Track 1 Aux Send" or "Track 4 Record Arm" */
    virtual juce::String getDescription() = 0;

    /** Should return the track that this control represents or nullptr if it is track independant. */
    virtual Track::Ptr getControlledTrack()            { return {}; }

    Engine& engine;

private:
    juce::Array<Assignment> assignemnts;
};

} // namespace tracktion_engine
