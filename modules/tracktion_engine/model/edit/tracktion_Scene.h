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

//==============================================================================
/**
    A Scene is a collection of ClipSlots across tracks.
    This class provides an interface to interact with them all rather than having
    to iterate through each Track's SlotList finding the relevant ClipSlot index.
*/
class Scene
{
public:
    /** Creates a Scene for a given state. */
    Scene (const juce::ValueTree&, Edit&);

    juce::ValueTree state;
    Edit& edit;

    juce::CachedValue<juce::String> name;
    juce::CachedValue<juce::Colour> colour;
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

    /** Returns the Scenes in the Edit. */
    juce::Array<Scene*> getScenes();

    //==============================================================================
    /** Adds Scenes to ensure numScenes are preset in the list. */
    void ensureNumberOfScenes (int numScenes);

    /** Deletes a specific Scene. */
    void deleteScene (Scene&);

    juce::ValueTree state;  /**< The state of this SceneList. */
    Edit& edit;             /**< The Edit this SceneList belongs to. */

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
