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

struct ExternalControllerManager::EditTreeWatcher   : private juce::ValueTree::Listener,
                                                      private juce::Timer,
                                                      private juce::AsyncUpdater
{
    EditTreeWatcher (ExternalControllerManager& o, Edit& e) : owner (o), edit (e)
    {
        edit.state.addListener (this);
        startTimer (40);
    }

    ~EditTreeWatcher() override
    {
        edit.state.removeListener (this);
    }

private:
    //==============================================================================
    ExternalControllerManager& owner;
    Edit& edit;

    juce::Array<juce::ValueTree, juce::CriticalSection> pluginsToUpdate;
    juce::Atomic<int> updateAux;

    void valueTreePropertyChanged (juce::ValueTree& v, const juce::Identifier& i) override
    {
        if (v.hasType (IDs::PLUGIN))
        {
            if (i == IDs::volume || i == IDs::pan)
                pluginsToUpdate.addIfNotAlreadyThere (v);
            else if (i == IDs::auxSendSliderPos && v.getProperty (IDs::type) == AuxSendPlugin::xmlTypeName)
                updateAux.set (1);
        }
        else if (v.hasType (IDs::MARKERCLIP))
        {
            triggerAsyncUpdate();
        }
    }

    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree& c) override
    {
        if (c.hasType (IDs::MARKERCLIP))
            triggerAsyncUpdate();
    }

    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree& c, int) override
    {
        if (c.hasType (IDs::MARKERCLIP))
            triggerAsyncUpdate();
    }

    void valueTreeChildOrderChanged (juce::ValueTree&, int, int) override   {}
    void valueTreeParentChanged (juce::ValueTree&) override                 {}

    void timerCallback() override
    {
        {
            juce::Array<juce::ValueTree, juce::CriticalSection> plugins;
            plugins.swapWith (pluginsToUpdate);

            for (int i = plugins.size(); --i >= 0;)
            {
                auto v = plugins.getUnchecked (i);

                if (auto mv = edit.getMasterVolumePlugin())
                {
                    if (mv->state == v)
                    {
                        owner.updateVolumePlugin (*mv);
                        continue;
                    }
                }

                if (auto p = edit.getPluginCache().getPluginFor (v))
                {
                    if (auto vp = dynamic_cast<VolumeAndPanPlugin*> (p.get()))
                        owner.updateVolumePlugin (*vp);
                    else if (auto vca = dynamic_cast<VCAPlugin*> (p.get()))
                        owner.updateVCAPlugin (*vca);
                }
            }
        }

        if (updateAux.compareAndSetBool (0, 1))
            owner.auxSendLevelsChanged();
    }

    void handleAsyncUpdate() override
    {
        owner.updateMarkers();
    }
};

//==============================================================================
ExternalControllerManager::ExternalControllerManager (Engine& e) : engine (e)
{
    blinkTimer.reset (new BlinkTimer (*this));
}

ExternalControllerManager::~ExternalControllerManager()
{
    jassert (currentEdit == nullptr); // should be cleared by this point.

    blinkTimer.reset();
    setCurrentEdit (nullptr, nullptr);
}

void ExternalControllerManager::initialise()
{
    CRASH_TRACER
    TRACKTION_LOG ("Creating Default Controllers...");

   #if TRACKTION_ENABLE_CONTROL_SURFACES
    auto controllers = engine.getEngineBehaviour().getDesiredControlSurfaces();

    if (controllers.mackieMCU)
    {
        auto mcu = new MackieMCU (*this);
        addNewController (mcu);

        for (int i = 0; i < getXTCount (mcu->deviceDescription); ++i)
            addNewController (new MackieXT (*this, *mcu, i));

        refreshXTOrder();
    }

   #if TRACKTION_ENABLE_CONTROL_SURFACE_MACKIEC4
    if (controllers.mackieC4)  addNewController (new MackieC4 (*this));
   #endif

    if (controllers.iconProG2)
    {
        auto icon = new IconProG2 (*this);
        addNewController (icon);
        for (int i = 0; i < getXTCount (icon->deviceDescription); ++i)
            addNewController (new MackieXT (*this, *icon, i));
    }

    if (controllers.tranzport)  addNewController (new TranzportControlSurface (*this));
    if (controllers.alphaTrack) addNewController (new AlphaTrackControlSurface (*this));
    if (controllers.remoteSL)   addNewController (new NovationRemoteSl (*this));

   #if TRACKTION_ENABLE_AUTOMAP
    if (controllers.automap)
    {
        automap = new NovationAutomap (*this);
        addNewController (automap);
    }
   #endif

   #if JUCE_DEBUG
    if (controllers.remoteSLCompact) addNewController (new RemoteSLCompact (*this));
   #endif

    // load the custom midi controllers
    TRACKTION_LOG ("Loading Custom Controllers...");

    for (auto mc : CustomControlSurface::getCustomSurfaces (*this))
        addNewController (mc);
   #endif

    sendChangeMessage();
}

void ExternalControllerManager::shutdown()
{
    CRASH_TRACER
    jassert (currentEdit == nullptr && currentSelectionManager == nullptr);
    setCurrentEdit (nullptr, nullptr);
    devices.clear();
    automap = nullptr;
    currentEdit = nullptr;
}

ExternalController* ExternalControllerManager::addNewController (ControlSurface* cs)
{
    return devices.add (new ExternalController (engine, cs));
}

#define FOR_EACH_DEVICE(x) \
    for (auto device : devices) { device->x; }

#define FOR_EACH_ACTIVE_DEVICE(x) \
    for (auto device : devices) { if (device->isEnabled()) device->x; }

void ExternalControllerManager::setCurrentEdit (Edit* newEdit, SelectionManager* newSM)
{
    if (newEdit != currentEdit)
    {
        editTreeWatcher = nullptr;

        if (currentEdit != nullptr)
            currentEdit->getTransport().removeChangeListener (this);

        currentEdit = newEdit;

        if (currentEdit != nullptr)
        {
            currentEdit->getTransport().addChangeListener (this);
            editTreeWatcher = std::make_unique<EditTreeWatcher> (*this, *currentEdit);
        }
    }

    if (newSM != currentSelectionManager)
    {
        if (currentSelectionManager != nullptr)
            currentSelectionManager->removeChangeListener (this);

        currentSelectionManager = newSM;

        if (currentSelectionManager != nullptr)
            currentSelectionManager->addChangeListener (this);
    }

    FOR_EACH_ACTIVE_DEVICE (currentEditChanged (currentEdit));
}

bool ExternalControllerManager::isAttachedToEdit (const Edit* ed) const noexcept
{
    return currentEdit != nullptr && currentEdit == ed;
}

bool ExternalControllerManager::isAttachedToEdit (const Edit& e) const noexcept
{
    return isAttachedToEdit (&e);
}

SelectionManager* ExternalControllerManager::getSelectionManager() const noexcept
{
    return currentSelectionManager;
}

void ExternalControllerManager::detachFromEdit (Edit* ed)
{
    if (isAttachedToEdit (ed))
        setCurrentEdit (nullptr, nullptr);
}

void ExternalControllerManager::detachFromSelectionManager (SelectionManager* sm)
{
    if (currentSelectionManager == sm)
        setCurrentEdit (currentEdit, nullptr);
}

bool ExternalControllerManager::createCustomController (const juce::String& name, Protocol protocol)
{
    CRASH_TRACER

    int outPort = 9000, inPort = 8000;
    // Find free UDP ports for OSC input and output
    if (protocol == osc)
    {
        for (auto device : devices)
        {
            if (device->needsOSCSocket())
            {
                outPort = std::max (outPort, device->getOSCOutputPort() + 1);
                inPort  = std::max (inPort,  device->getOSCInputPort() + 1);
            }
        }
    }

    if (auto ec = addNewController (new CustomControlSurface (*this, name, protocol)))
    {
        if (protocol == osc)
        {
            ec->setOSCOutputPort (outPort);
            ec->setOSCInputPort (inPort);
        }
    }

    sendChangeMessage();
    return true;
}

ExternalController* ExternalControllerManager::addController (ControlSurface* c)
{
    CRASH_TRACER

    int outPort = 9000, inPort = 8000;
    // Find free UDP ports for OSC input and output
    if (c->needsOSCSocket)
    {
        for (auto device : devices)
        {
            if (device->needsOSCSocket())
            {
                outPort = std::max (outPort, device->getOSCOutputPort() + 1);
                inPort  = std::max (inPort,  device->getOSCInputPort() + 1);
            }
        }
    }

    if (auto ec = addNewController (c))
    {
        if (c->needsOSCSocket)
        {
            ec->setOSCOutputPort (outPort);
            ec->setOSCInputPort (inPort);
        }
        sendChangeMessage();
        return ec;
    }
    return nullptr;
}

void ExternalControllerManager::deleteController (ExternalController* c)
{
    CRASH_TRACER

    if (devices.contains (c))
    {
       #if TRACKTION_ENABLE_AUTOMAP && TRACKTION_ENABLE_CONTROL_SURFACES
        if (c->controlSurface.get() == automap)
            automap = nullptr;
       #endif

        c->deleteController();
        devices.removeObject (c);
        sendChangeMessage();
    }
}

juce::StringArray ExternalControllerManager::getAllControllerNames()
{
    juce::StringArray s;

    for (auto ec : devices)
        s.add (ec->getName());

    return s;
}

ExternalController* ExternalControllerManager::getActiveCustomController()
{
    for (auto ec : devices)
        if (ec->isEnabled() && ec->isDeletable())
            return ec;

    return {};
}

void ExternalControllerManager::midiInOutDevicesChanged()   { FOR_EACH_DEVICE (midiInOutDevicesChanged()); }
void ExternalControllerManager::updateDeviceState()         { FOR_EACH_ACTIVE_DEVICE (updateDeviceState()); }
void ExternalControllerManager::updateParameters()          { FOR_EACH_ACTIVE_DEVICE (updateParameters()); }
void ExternalControllerManager::updateMarkers()             { FOR_EACH_ACTIVE_DEVICE (updateMarkers()); }
void ExternalControllerManager::updateTrackRecordLights()   { FOR_EACH_ACTIVE_DEVICE (updateTrackRecordLights()); }
void ExternalControllerManager::updatePunchLights()         { FOR_EACH_ACTIVE_DEVICE (updatePunchLights()); }
void ExternalControllerManager::updateScrollLights()        { FOR_EACH_ACTIVE_DEVICE (updateScrollLights()); }
void ExternalControllerManager::updateUndoLights()          { FOR_EACH_ACTIVE_DEVICE (updateUndoLights()); }

void ExternalControllerManager::changeListenerCallback (ChangeBroadcaster* source)
{
    CRASH_TRACER

    if (auto tc = dynamic_cast<TransportControl*> (source))
    {
        playStateChanged (tc->isPlaying());
        recordStateChanged (tc->isRecording());
    }
    else if (currentSelectionManager != nullptr)
    {
        auto& selectionManager = *currentSelectionManager;

        if (selectionManager.getNumObjectsSelected() == 1 && selectionManager.containsType<Plugin>())
        {
            FOR_EACH_ACTIVE_DEVICE (selectedPluginChanged());
        }
        else if (auto track = selectionManager.getFirstItemOfType<Track>())
        {
            int num = mapTrackNumToChannelNum (track->getIndexInEditTrackList());

            for (auto device : devices)
                if (device->isEnabled())
                    if (device->followsTrackSelection)
                        if (num != -1 && num != device->channelStart)
                            device->changeFaderBank (num - device->channelStart, false);
        }
        FOR_EACH_ACTIVE_DEVICE (updateTrackSelectLights());
    }
}

void ExternalControllerManager::updateAllDevices()
{
    if (! isTimerRunning())
    {
        auto now = juce::Time::getMillisecondCounter();

        if (now - lastUpdate > 250)
        {
            lastUpdate = now;

            updateDeviceState();
            updateParameters();
            updateMarkers();
        }
        else
        {
            startTimer (250 - (int) (now - lastUpdate));
        }
    }
}

void ExternalControllerManager::timerCallback()
{
    CRASH_TRACER
    stopTimer();

    lastUpdate = juce::Time::getMillisecondCounter();

    updateDeviceState();
    updateParameters();
    updateMarkers();
}

//==============================================================================
ExternalControllerManager::BlinkTimer::BlinkTimer (ExternalControllerManager& e) : ecm (e)
{
    startTimer (750);
}

void ExternalControllerManager::BlinkTimer::timerCallback()
{
    isBright = ! isBright;
    ecm.blinkNow();
}

void ExternalControllerManager::blinkNow()
{
    updateMuteSoloLights (true);
}

void ExternalControllerManager::updateMuteSoloLights (bool onlyUpdateFlashingLights)
{
    if (currentEdit != nullptr)
    {
        int i = 0;
        const bool isBright = blinkTimer->isBright;
        bool anySolo = false;

        currentEdit->visitAllTracksRecursive ([&, this] (Track& t)
        {
            if (t.isSolo (false))
                anySolo = true;

            auto mappedChan = mapTrackNumToChannelNum (i);

            if (mappedChan >= 0)
            {
                const auto flags = t.getMuteAndSoloLightState();

                if ((flags & (Track::soloFlashing | Track::muteFlashing)) != 0
                     || ! onlyUpdateFlashingLights)
                {
                    FOR_EACH_ACTIVE_DEVICE (updateSoloAndMute (mappedChan, flags, isBright));
                }
            }

            ++i;
            return true;
        });

        FOR_EACH_ACTIVE_DEVICE (soloCountChanged (blinkTimer->isBright && anySolo));
    }
}

//==============================================================================
void ExternalControllerManager::moveFader (int channelNum, float newSliderPos)
{
    auto chan = mapTrackNumToChannelNum (channelNum);

    FOR_EACH_ACTIVE_DEVICE (moveFader (chan, newSliderPos));
}

void ExternalControllerManager::movePanPot (int channelNum, float newPan)
{
    auto chan = mapTrackNumToChannelNum (channelNum);

    FOR_EACH_ACTIVE_DEVICE (movePanPot (chan, newPan));
}

void ExternalControllerManager::updateVolumePlugin (VolumeAndPanPlugin& vp)
{
    if (auto t = dynamic_cast<AudioTrack*> (vp.getOwnerTrack()))
    {
        if (t->getVolumePlugin() == &vp)
        {
            auto chan = mapTrackNumToChannelNum (t->getIndexInEditTrackList());

            for (auto c : devices)
            {
                if (c->isEnabled())
                {
                    c->moveFader (chan, vp.getSliderPos());
                    c->movePanPot (chan, vp.getPan());
                }
            }
        }
    }
    else
    {
        if (vp.edit.getMasterVolumePlugin().get() == &vp)
        {
            float left, right;
            getGainsFromVolumeFaderPositionAndPan (vp.getSliderPos(), vp.getPan(), getDefaultPanLaw(), left, right);
            left  = gainToVolumeFaderPosition (left);
            right = gainToVolumeFaderPosition (right);

            FOR_EACH_ACTIVE_DEVICE (moveMasterFaders (left, right));
        }
    }
}

void ExternalControllerManager::updateVCAPlugin (VCAPlugin& vca)
{
    CRASH_TRACER

    if (auto t = dynamic_cast<FolderTrack*> (vca.getOwnerTrack()))
    {
        if (t->getVCAPlugin() == &vca)
        {
            auto chan = mapTrackNumToChannelNum (t->getIndexInEditTrackList());

            for (auto c : devices)
            {
                if (c->isEnabled())
                {
                    c->moveFader (chan, vca.getSliderPos());
                    c->movePanPot (chan, 0.0f);
                }
            }
        }
    }
}

void ExternalControllerManager::moveMasterFaders (float newLeftPos, float newRightPos)
{
    CRASH_TRACER
    FOR_EACH_ACTIVE_DEVICE (moveMasterFaders (newLeftPos, newRightPos));
}

void ExternalControllerManager::soloCountChanged (bool anySoloTracks)
{
    CRASH_TRACER
    FOR_EACH_ACTIVE_DEVICE (soloCountChanged (anySoloTracks));
}

void ExternalControllerManager::playStateChanged (bool isPlaying)
{
    CRASH_TRACER
    FOR_EACH_ACTIVE_DEVICE (playStateChanged (isPlaying));
}

void ExternalControllerManager::recordStateChanged (bool isRecording)
{
    CRASH_TRACER
    FOR_EACH_ACTIVE_DEVICE (recordStateChanged (isRecording));
}

void ExternalControllerManager::automationModeChanged (bool isReading, bool isWriting)
{
    CRASH_TRACER
    FOR_EACH_ACTIVE_DEVICE (automationModeChanged (isReading, isWriting));
}

void ExternalControllerManager::channelLevelChanged (int channel, float l, float r)
{
    CRASH_TRACER
    const int cn = mapTrackNumToChannelNum (channel);
    FOR_EACH_ACTIVE_DEVICE (channelLevelChanged (cn, l, r));
}

void ExternalControllerManager::masterLevelsChanged (float leftLevel, float rightLevel)
{
    CRASH_TRACER
    FOR_EACH_ACTIVE_DEVICE (masterLevelsChanged (leftLevel, rightLevel));
}

void ExternalControllerManager::timecodeChanged (int barsOrHours, int beatsOrMinutes, int ticksOrSeconds,
                                                 int millisecs, bool isBarsBeats, bool isFrames)
{
    CRASH_TRACER
    FOR_EACH_ACTIVE_DEVICE (timecodeChanged (barsOrHours, beatsOrMinutes, ticksOrSeconds, millisecs, isBarsBeats, isFrames));
}

void ExternalControllerManager::snapChanged (bool isOn)
{
    CRASH_TRACER
    FOR_EACH_ACTIVE_DEVICE (snapChanged (isOn));
}

void ExternalControllerManager::loopChanged (bool isOn)
{
    CRASH_TRACER
    FOR_EACH_ACTIVE_DEVICE (loopChanged (isOn));
}

void ExternalControllerManager::clickChanged (bool isOn)
{
    CRASH_TRACER
    FOR_EACH_ACTIVE_DEVICE (clickChanged (isOn));
}

void ExternalControllerManager::editPositionChanged (Edit* ed, TimePosition newCursorPosition)
{
    if (ed != nullptr)
    {
        CRASH_TRACER
        juce::String parts[4];
        ed->getTimecodeFormat().getPartStrings (TimecodeDuration::fromSecondsOnly (toDuration (newCursorPosition)),
                                                ed->tempoSequence,
                                                false, parts);

        if (ed->getTimecodeFormat().isBarsBeats())
        {
            timecodeChanged (parts[2].getIntValue(),
                             parts[1].getIntValue(),
                             parts[0].getIntValue(),
                             0,
                             ed->getTimecodeFormat().isBarsBeats(),
                             ed->getTimecodeFormat().isSMPTE());
        }
        else
        {
            timecodeChanged (parts[3].getIntValue(),
                             parts[2].getIntValue(),
                             parts[1].getIntValue(),
                             parts[0].getIntValue(),
                             ed->getTimecodeFormat().isBarsBeats(),
                             ed->getTimecodeFormat().isSMPTE());
        }
    }
}


void ExternalControllerManager::auxSendLevelsChanged()
{
    CRASH_TRACER
    FOR_EACH_ACTIVE_DEVICE (auxSendLevelsChanged());
}

//==============================================================================
void ExternalControllerManager::userMovedFader (int channelNum, float newSliderPos, bool delta)
{
    CRASH_TRACER
    auto track = getChannelTrack (channelNum);

    if (auto at = dynamic_cast<AudioTrack*> (track))
    {
        if (auto vp = at->getVolumePlugin())
            vp->setSliderPos (delta ? vp->getSliderPos() + newSliderPos : newSliderPos);
        else
            moveFader (mapTrackNumToChannelNum (channelNum), decibelsToVolumeFaderPosition (0.0f));
    }

    if (auto ft = dynamic_cast<FolderTrack*> (track))
    {
        if (auto vca = ft->getVCAPlugin())
            vca->setSliderPos (delta ? vca->getSliderPos() + delta : newSliderPos);
        else if (auto vp = ft->getVolumePlugin())
            vp->setSliderPos (delta ? vp->getSliderPos() + delta : newSliderPos);
        else
            moveFader (mapTrackNumToChannelNum (channelNum), decibelsToVolumeFaderPosition (0.0f));
    }
}

void ExternalControllerManager::userMovedMasterFader (Edit* ed, float newLevel, bool delta)
{
    if (ed != nullptr)
    {
        if (delta)
            ed->setMasterVolumeSliderPos (ed->getMasterSliderPosParameter()->getCurrentValue() + newLevel);
        else
            ed->setMasterVolumeSliderPos (newLevel);
    }
}

void ExternalControllerManager::userMovedMasterPanPot (Edit* ed, float newLevel)
{
    if (ed != nullptr)
        ed->setMasterPanPos (newLevel);
}

void ExternalControllerManager::userMovedPanPot (int channelNum, float newPan, bool delta)
{
    auto track = getChannelTrack (channelNum);
    
    if (auto t = dynamic_cast<AudioTrack*> (track))
    {
        if (auto vp = t->getVolumePlugin())
            vp->setPan (delta ? vp->getPan() + newPan : newPan);
    }
    else if (auto ft = dynamic_cast<FolderTrack*> (track))
    {
        if (auto vp = ft->getVolumePlugin())
            vp->setPan (delta ? vp->getPan() + newPan : newPan);
    }
}

void ExternalControllerManager::userMovedAux (int channelNum, int auxNum, float newPosition)
{
    if (auto t = dynamic_cast<AudioTrack*> (getChannelTrack (channelNum)))
        if (auto aux = t->getAuxSendPlugin (auxNum))
            aux->setGainDb (volumeFaderPositionToDB (newPosition));
}

void ExternalControllerManager::userPressedAux (int channelNum, int auxNum)
{
    if (auto t = dynamic_cast<AudioTrack*> (getChannelTrack (channelNum)))
        if (auto aux = t->getAuxSendPlugin (auxNum))
            aux->setMute (! aux->isMute());
}

void ExternalControllerManager::userMovedQuickParam (float newLevel)
{
    if (currentSelectionManager != nullptr)
        if (auto f = currentSelectionManager->getFirstItemOfType<Plugin>())
            if (auto param = f->getQuickControlParameter())
                param->midiControllerMoved (newLevel);
}

void ExternalControllerManager::userPressedSolo (int channelNum)
{
    if (auto t = getChannelTrack (channelNum))
        t->setSolo (! t->isSolo (false));
}

void ExternalControllerManager::userPressedSoloIsolate (int channelNum)
{
    if (auto t = getChannelTrack (channelNum))
        t->setSoloIsolate (! t->isSoloIsolate (false));
}

void ExternalControllerManager::userPressedMute (int channelNum, bool muteVolumeControl)
{
    if (auto t = getChannelTrack (channelNum))
    {
        if (muteVolumeControl)
        {
            if (auto at = dynamic_cast<AudioTrack*> (t))
                if (auto vp = at->getVolumePlugin())
                    vp->muteOrUnmute();
        }
        else
        {
            t->setMute (! t->isMuted (false));
        }
    }
}

void ExternalControllerManager::userSelectedTrack (int channelNum)
{
    if (auto t = getChannelTrack (channelNum))
    {
        if (currentSelectionManager != nullptr)
        {
            if (currentSelectionManager->isSelected (t))
                currentSelectionManager->deselect (t);
            else
                currentSelectionManager->addToSelection (t);
        }
    }
}

void ExternalControllerManager::userSelectedClipInTrack (int channelNum)
{
    if (currentSelectionManager != nullptr)
        if (auto t = getChannelTrack (channelNum))
            if (t->getNumTrackItems() > 0)
                if (auto ti = t->getTrackItem (0))
                    currentSelectionManager->selectOnly (ti);
}

void ExternalControllerManager::userSelectedPluginInTrack (int channelNum)
{
    if (currentSelectionManager != nullptr)
        if (auto t = getChannelTrack (channelNum))
            if (auto f = t->pluginList.getPlugins().getFirst())
                currentSelectionManager->selectOnly (f.get());
}


int ExternalControllerManager::getNumChannelTracks() const
{
    int trackNum = 0;

    if (currentEdit != nullptr && isVisibleOnControlSurface)
    {
        currentEdit->visitAllTracksRecursive ([&] (Track& t)
        {
            if (isVisibleOnControlSurface (t))
                ++trackNum;

            return true;
        });
    }

    return trackNum;
}

Track* ExternalControllerManager::getChannelTrack (int index) const
{
    Track* result = nullptr;

    if (currentEdit != nullptr && isVisibleOnControlSurface)
    {
        int i = 0, trackNum = 0;

        currentEdit->visitAllTracksRecursive ([&] (Track& t)
        {
            if (isVisibleOnControlSurface (t))
            {
                if (trackNum == index)
                {
                    result = &t;
                    return false;
                }

                ++trackNum;
            }

            ++i;
            return true;
        });
    }

    return result;
}

int ExternalControllerManager::mapTrackNumToChannelNum (int index) const
{
    int result = -1;

    if (currentEdit != nullptr && isVisibleOnControlSurface)
    {
        if (index >= 0)
        {
            int i = 0, trackNum = 0;

            currentEdit->visitAllTracksRecursive ([&] (Track& t)
            {
                if (isVisibleOnControlSurface (t))
                {
                    if (i == index)
                    {
                        result = trackNum;
                        return false;
                    }

                    ++trackNum;
                }

                ++i;
                return true;
            });
        }
    }

    return result;
}

bool ExternalControllerManager::shouldTrackBeColoured (int channelNum)
{
    if (! devices.isEmpty())
    {
        auto cn = mapTrackNumToChannelNum (channelNum);

        for (auto& d : devices)
            if (d->shouldTrackBeColoured (cn))
                return true;
    }

    return false;
}

juce::Colour ExternalControllerManager::getTrackColour (int channelNum)
{
    juce::Colour c;

    if (! devices.isEmpty())
    {
        auto cn = mapTrackNumToChannelNum (channelNum);
        FOR_EACH_ACTIVE_DEVICE (getTrackColour (cn, c));
    }

    return c;
}

void ExternalControllerManager::repaintTrack (int channelNum)
{
    if (auto t = getChannelTrack (channelNum))
        t->changed();
}

bool ExternalControllerManager::shouldPluginBeColoured (Plugin* plugin)
{
    for (auto* d : devices)
        if (d->shouldPluginBeColoured (plugin))
            return true;

    return false;
}

juce::Colour ExternalControllerManager::getPluginColour (Plugin* plugin)
{
    juce::Colour c;
    FOR_EACH_ACTIVE_DEVICE (getPluginColour (plugin, c));
    return c;
}

void ExternalControllerManager::repaintPlugin (Plugin& plugin)
{
    if (auto c = PluginComponent::getComponentFor (plugin))
        c->updateColour();
}

int ExternalControllerManager::getXTCount (const juce::String& desc)
{
    if (desc == "Mackie Control Universal")
        return engine.getPropertyStorage().getProperty (SettingID::xtCount);
    
    return engine.getPropertyStorage().getPropertyItem (SettingID::xtCount, desc);
}

void ExternalControllerManager::setXTCount (const juce::String& desc, int after)
{
    CRASH_TRACER
    juce::ignoreUnused (desc, after);

   #if TRACKTION_ENABLE_CONTROL_SURFACES
    for (int devIdx = 0; devIdx < devices.size(); devIdx++)
    {
        auto device = devices[devIdx];
        if (auto mcu = device->getControlSurfaceIfType<MackieMCU>(); mcu != nullptr && mcu->deviceDescription == desc)
        {
            int before = getXTCount (desc);
            int diff = after - before;

            if (diff > 0)
            {
                for (int i = 0; i < diff; ++i)
                    devices.insert (devIdx + before + i + 1, new ExternalController (engine, new MackieXT (*this, *mcu, before + i)));
            }
            else if (diff < 0)
            {
                for (int i = 0; i < std::abs (diff); ++i)
                    devices.remove (devIdx + before - i);
            }

            if (desc == "Mackie Control Universal")
                engine.getPropertyStorage().setProperty (SettingID::xtCount, after);
            else
                engine.getPropertyStorage().setPropertyItem (SettingID::xtCount, desc, after);
        }
        refreshXTOrder();
        sendChangeMessage();
    }
   #endif
}

void ExternalControllerManager::refreshXTOrder()
{
    CRASH_TRACER

   #if TRACKTION_ENABLE_CONTROL_SURFACES
    for (auto device : devices)
    {
        if (auto mcu = device->getControlSurfaceIfType<MackieMCU>())
        {
            MackieXT* xt[MackieMCU::maxNumSurfaces - 1] = {};

            int offset = devices.indexOf (device);
            for (int i = 0; i < getXTCount (mcu->deviceDescription); ++i)
                xt[i] = devices[offset + 1 + i] ? devices[offset + 1 + i]->getControlSurfaceIfType<MackieXT>() : nullptr;

            juce::StringArray indices;
            if (mcu->deviceDescription == "Mackie Control Universal")
                indices.addTokens (engine.getPropertyStorage().getProperty (SettingID::xtIndices, "0 1 2 3").toString(), false);
            else
                indices.addTokens (engine.getPropertyStorage().getPropertyItem (SettingID::xtIndices, mcu->deviceDescription, "0 1 2 3").toString(), false);

            for (int i = indices.size(); --i >= 0;)
                if (indices[i].getIntValue() > getXTCount (mcu->deviceDescription))
                    indices.remove(i);

            mcu->setDeviceIndex (indices.indexOf ("0"));

            if (xt[0] != nullptr) xt[0]->setDeviceIndex (indices.indexOf ("1"));
            if (xt[1] != nullptr) xt[1]->setDeviceIndex (indices.indexOf ("2"));
            if (xt[2] != nullptr) xt[2]->setDeviceIndex (indices.indexOf ("3"));
        }
    }
   #endif
}

}} // namespace tracktion { inline namespace engine
