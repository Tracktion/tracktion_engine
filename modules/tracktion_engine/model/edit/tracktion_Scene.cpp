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

Scene::Scene (const juce::ValueTree& v, SceneList& sl)
    : state (v), edit (sl.edit), sceneList (sl)
{
    assert (v.hasType (IDs::SCENE));

    auto um = &edit.getUndoManager();
    name.referTo (state, IDs::name, um);
    colour.referTo (state, IDs::colour, um);

    state.addListener (this);
}

Scene::~Scene()
{
    notifyListenersOfDeletion();
}

juce::String Scene::getSelectableDescription()
{
    return TRANS("Scene");
}

void Scene::valueTreePropertyChanged (juce::ValueTree& v, const juce::Identifier& i)
{
    if (v != state)
        return;

    if (i == IDs::name || i == IDs::colour)
        changed();
}

//==============================================================================
SceneWatcher::SceneWatcher (const juce::ValueTree& scenesState_, Edit& e)
    : scenesState (scenesState_), edit (e)
{
    checkForScenes();
}

void SceneWatcher::addListener (Listener* l)
{
    listeners.add (l);
}

void SceneWatcher::removeListener (Listener* l)
{
    listeners.remove (l);
}

void SceneWatcher::checkForScenes()
{
    if (scenesState.getNumChildren() == 0)
    {
        stopTimer();
        return;
    }

    if (! isTimerRunning())
        startTimerHz (30);
}

void SceneWatcher::timerCallback()
{
    auto trackIdx = 0;

    for (auto at : getAudioTracks (edit))
    {
        auto slotIdx = 0;

        for (auto slot : at->getClipSlotList().getClipSlots())
        {
            if (auto c = slot->getClip())
            {
                if (auto lh = c->getLaunchHandle())
                {
                    auto p = lh->getPlayingStatus();
                    auto q = lh->getQueuedStatus();

                    auto key = std::pair<int,int> (trackIdx, slotIdx);
                    auto itr = lastStates.find (key);

                    if (itr == lastStates.end())
                    {
                        lastStates[key] = { callback, p, q };
                        listeners.call ([trackIdx, slotIdx] (Listener& l) { l.slotUpdated (trackIdx, slotIdx); });
                    }
                    else if (itr->second.playState != p || itr->second.queueState != q)
                    {
                        itr->second.lastSeen    = callback;
                        itr->second.playState   = p;
                        itr->second.queueState  = q;
                        listeners.call ([trackIdx, slotIdx] (Listener& l) { l.slotUpdated (trackIdx, slotIdx); });
                    }
                    else
                    {
                        itr->second.lastSeen    = callback;
                    }
                }
            }

            slotIdx++;
        }

        trackIdx++;
    }

    for (auto itr = lastStates.cbegin(); itr != lastStates.cend();)
    {
        if (itr->second.lastSeen != callback)
        {
            listeners.call ([itr] (Listener& l) { l.slotUpdated (itr->first.first, itr->first.second); });
            lastStates.erase (itr++);
        }
        else
        {
            itr++;
        }
    }

    callback++;
}

void SceneWatcher::valueTreeChildAdded (juce::ValueTree& p, juce::ValueTree& c)
{
    if (p == scenesState && c.hasType (IDs::SCENE))
        checkForScenes();
}

void SceneWatcher::valueTreeChildRemoved (juce::ValueTree& p, juce::ValueTree& c, int)
{
    if (p == scenesState && c.hasType (IDs::SCENE))
        checkForScenes();
}


//==============================================================================
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

    assert (objects.size() >= numScenes);

    for (auto at : getAudioTracks (edit))
        at->getClipSlotList().ensureNumberOfSlots (objects.size());
}

void SceneList::deleteScene (Scene& scene)
{
    assert (&scene.edit == &edit);
    const int index = parent.indexOf (scene.state);
    parent.removeChild (index, &edit.getUndoManager());

    for (auto at : getAudioTracks (edit))
    {
        auto& csl = at->getClipSlotList();
        auto cs = csl.getClipSlots()[index];
        assert (cs != nullptr);
        
        if (auto clip = cs->getClip())
            clip->removeFromParent();
        
        csl.deleteSlot (*csl.getClipSlots()[index]);
    }
}

//==============================================================================
bool SceneList::isSuitableType (const juce::ValueTree& v) const
{
    return v.hasType (IDs::SCENE);
}

Scene* SceneList::createNewObject (const juce::ValueTree& v)
{
    return new Scene (v, *this);
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
