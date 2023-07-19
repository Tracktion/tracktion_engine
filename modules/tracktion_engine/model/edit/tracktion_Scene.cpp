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

Scene::Scene (const juce::ValueTree& v, Edit& e)
    : state (v), edit (e)
{
    assert (v.hasType (IDs::SCENE));

    auto um = &edit.getUndoManager();
    name.referTo (state, IDs::name, um);
    colour.referTo (state, IDs::colour, um);
}


//==============================================================================
SceneList::SceneList (const juce::ValueTree& v, Edit& e)
    : ValueTreeObjectList (v),
      state (v),
      edit (e)
{
    assert (parent.hasType (IDs::SCENES));
    rebuildObjects();
}

SceneList::~SceneList()
{
    freeObjects();
}

//==============================================================================
juce::Array<Scene*> SceneList::getScenes()
{
    return objects;
}

void SceneList::ensureNumberOfScenes (int numScenes)
{
    for (int i = size(); i < numScenes; ++i)
        parent.appendChild (juce::ValueTree (IDs::SCENE), &edit.getUndoManager());
}

void SceneList::deleteScene (Scene& scene)
{
    assert (&scene.edit == &edit);
    parent.removeChild (scene.state, &edit.getUndoManager());
}

//==============================================================================
bool SceneList::isSuitableType (const juce::ValueTree& v) const
{
    return v.hasType (IDs::SCENE);
}

Scene* SceneList::createNewObject (const juce::ValueTree& v)
{
    return new Scene (v, edit);
}

void SceneList::deleteObject (Scene* scene)
{
    delete scene;
}

void SceneList::newObjectAdded (Scene*)
{
}

void SceneList::objectRemoved (Scene*)
{
}

void SceneList::objectOrderChanged()
{
}


}} // namespace tracktion { inline namespace engine
