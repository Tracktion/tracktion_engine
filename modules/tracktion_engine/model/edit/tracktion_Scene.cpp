/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
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

int Scene::getIndex()
{
    return sceneList.getScenes().indexOf (this);
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
    scenesState.addListener (this);
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

SceneWatcher::RecordingState SceneWatcher::getRecordingState (ClipSlot& slot)
{
    auto dest = slot.getInputDestination();

    if (dest && dest->input.isRecording (dest->targetID))
    {
        auto epc = dest->input.edit.getTransport().getCurrentPlaybackContext();
        const auto currentTime = epc ? epc->getUnloopedPosition() : dest->input.edit.getTransport().getPosition();

        if (currentTime < dest->input.getPunchInTime (dest->targetID))
            return RecordingState::pending;
        else
            return RecordingState::recording;
    }
    return RecordingState::none;
}

void SceneWatcher::timerCallback()
{
    auto update = [&] (int trackIdx, int slotIdx, LaunchHandle::PlayState p, std::optional<LaunchHandle::QueueState> q, RecordingState r)
    {
        auto key = std::pair<int,int> (trackIdx, slotIdx);
        auto itr = lastStates.find (key);

        if (itr == lastStates.end())
        {
            lastStates[key] = { callback, p, q, r };
            listeners.call ([trackIdx, slotIdx] (Listener& l) { l.slotUpdated (trackIdx, slotIdx); });
        }
        else if (itr->second.playState != p || itr->second.queueState != q || itr->second.recordingState != r)
        {
            itr->second.lastSeen        = callback;
            itr->second.playState       = p;
            itr->second.queueState      = q;
            itr->second.recordingState  = r;
            listeners.call ([trackIdx, slotIdx] (Listener& l) { l.slotUpdated (trackIdx, slotIdx); });
        }
        else
        {
            itr->second.lastSeen    = callback;
        }
    };

    auto trackIdx = 0;

    for (auto at : getAudioTracks (edit))
    {
        auto slotIdx = 0;

        for (auto slot : at->getClipSlotList().getClipSlots())
        {
            if (auto r = getRecordingState (*slot); r != RecordingState::none)
            {
                update (trackIdx, slotIdx, LaunchHandle::PlayState::stopped, {}, r);
            }
            else if (auto c = slot->getClip())
            {
                if (auto lh = c->getLaunchHandle())
                {
                    auto p = lh->getPlayingStatus();
                    auto q = lh->getQueuedStatus();

                    update (trackIdx, slotIdx, p, q, r);
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

Scene* SceneList::insertScene (int idx)
{
    parent.addChild (juce::ValueTree (IDs::SCENE), idx, &edit.getUndoManager());

    for (auto at : getAudioTracks (edit))
        at->getClipSlotList().insertSlot (idx);

    return getScenes()[idx];
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
