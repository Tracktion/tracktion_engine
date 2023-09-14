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

class SceneList;

//==============================================================================
/**
    A Scene is a collection of ClipSlots across tracks.
    This class provides an interface to interact with them all rather than having
    to iterate through each Track's SlotList finding the relevant ClipSlot index.
*/
class Scene : public Selectable,
              private juce::ValueTree::Listener
{
public:
    /** Creates a Scene for a given state. */
    Scene (const juce::ValueTree&, SceneList&);

    /** Destructor. */
    ~Scene() override;

    juce::ValueTree state;
    Edit& edit;

    juce::CachedValue<juce::String> name;
    juce::CachedValue<juce::Colour> colour;

    SceneList& sceneList;

    /** @internal */
    juce::String getSelectableDescription() override;

private:
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;
};

//==============================================================================
//==============================================================================
/**
    Notifies UI components of changes to the scenes and clips
*/
class SceneWatcher : public juce::Timer
{
public:
    SceneWatcher (Edit&);

    struct Listener
    {
        virtual ~Listener() {};
        virtual void slotUpdated (int, int) {}
    };

    void addListener (Listener* l);
    void removeListener (Listener* l);

private:
    void timerCallback() override;

    Edit& edit;
    uint32_t callback = 0;

    struct Value
    {
        uint32_t lastSeen = 0;
        LaunchHandle::PlayState playState;
        std::optional<LaunchHandle::QueueState> queueState;
    };

    std::map<std::pair<int, int>, Value> lastStates;

    juce::ListenerList<Listener> listeners;
};

//==============================================================================
//==============================================================================
/**
    Represents the Scenes in an Edit.
*/
class SceneList : private ValueTreeObjectList<Scene>
{
public:
    /** Creates a SceneList for an Edit with a given state. */
    SceneList (const juce::ValueTree&, Edit&);
    /** Destructor. */
    ~SceneList() override;

    /** Returns the number of Scenes in the Edit. */
    int getNumScenes() { return size(); }

    /** Returns the Scenes in the Edit. */
    juce::Array<Scene*> getScenes();

    //==============================================================================
    /** Adds Scenes to ensure numScenes are preset in the list.
        N.B. This also adds ClipSlots to all tracks to ensure they have the same
        number of slots.
    */
    void ensureNumberOfScenes (int numScenes);

    /** Deletes a specific Scene. */
    void deleteScene (Scene&);

    juce::ValueTree state;  /**< The state of this SceneList. */
    Edit& edit;             /**< The Edit this SceneList belongs to. */

    SceneWatcher sceneWatcher { edit };

private:
    //==============================================================================
    /** @internal */
    bool isSuitableType (const juce::ValueTree&) const override;
    /** @internal */
    Scene* createNewObject (const juce::ValueTree&) override;
    /** @internal */
    void deleteObject (Scene*) override;

    /** @internal */
    void newObjectAdded (Scene*) override;
    /** @internal */
    void objectRemoved (Scene*) override;
    /** @internal */
    void objectOrderChanged() override;
};

}} // namespace tracktion { inline namespace engine
