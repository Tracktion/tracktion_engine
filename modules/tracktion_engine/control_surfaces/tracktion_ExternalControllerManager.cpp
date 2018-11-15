/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


struct ExternalControllerManager::EditTreeWatcher   : private ValueTree::Listener,
                                                      private Timer
{
    EditTreeWatcher (ExternalControllerManager& o, Edit& e) : owner (o), edit (e)
    {
        edit.state.addListener (this);
        startTimer (40);
    }

    ~EditTreeWatcher()
    {
        edit.state.removeListener (this);
    }

private:
    //==============================================================================
    ExternalControllerManager& owner;
    Edit& edit;

    Array<ValueTree, CriticalSection> pluginsToUpdate;
    Atomic<int> updateAux;

    void valueTreePropertyChanged (ValueTree& v, const Identifier& i) override
    {
        if (v.hasType (IDs::PLUGIN))
        {
            if (i == IDs::volume || i == IDs::pan)
                pluginsToUpdate.addIfNotAlreadyThere (v);
            else if (i == IDs::gain && v.getProperty (IDs::type) == AuxSendPlugin::xmlTypeName)
                updateAux.set (1);
        }
    }

    void valueTreeChildAdded (ValueTree&, ValueTree&) override        {}
    void valueTreeChildRemoved (ValueTree&, ValueTree&, int) override {}
    void valueTreeChildOrderChanged (ValueTree&, int, int) override   {}
    void valueTreeParentChanged (ValueTree&) override                 {}

    void timerCallback() override
    {
        {
            Array<ValueTree, CriticalSection> plugins;
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
    auto mcu = new MackieMCU (*this);
    addNewController (mcu);

    for (int i = 0; i < getXTCount(); ++i)
        addNewController (new MackieXT (*this, *mcu, i));

    refreshXTOrder();

    addNewController (new MackieC4 (*this));
    addNewController (new TranzportControlSurface (*this));
    addNewController (new AlphaTrackControlSurface (*this));
    addNewController (new NovationRemoteSl (*this));

   #if TRACKTION_ENABLE_AUTOMAP
    automap = new NovationAutomap (*this);
    addNewController (automap);
   #endif

   #if JUCE_DEBUG
    addNewController (new RemoteSLCompact (*this));
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

void ExternalControllerManager::addNewController (ControlSurface* cs)
{
    devices.add (new ExternalController (engine, cs));
}

#define FOR_EACH_DEVICE(x) \
    for (ExternalController* device : devices) { device->x; }

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

    FOR_EACH_DEVICE (currentEditChanged (currentEdit));
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

bool ExternalControllerManager::createCustomController (const String& name)
{
    CRASH_TRACER
    addNewController (new CustomControlSurface (*this, name));
    sendChangeMessage();
    return true;
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

StringArray ExternalControllerManager::getAllControllerNames()
{
    StringArray s;

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
void ExternalControllerManager::updateDeviceState()         { FOR_EACH_DEVICE (updateDeviceState()); }
void ExternalControllerManager::updateParameters()          { FOR_EACH_DEVICE (updateParameters()); }
void ExternalControllerManager::updateMarkers()             { FOR_EACH_DEVICE (updateMarkers()); }
void ExternalControllerManager::updateTrackRecordLights()   { FOR_EACH_DEVICE (updateTrackRecordLights()); }
void ExternalControllerManager::updatePunchLights()         { FOR_EACH_DEVICE (updatePunchLights()); }
void ExternalControllerManager::updateUndoLights()          { FOR_EACH_DEVICE (updateUndoLights()); }

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
            FOR_EACH_DEVICE (selectedPluginChanged());
        }
        else if (auto track = selectionManager.getFirstItemOfType<Track>())
        {
            int num = mapTrackNumToChannelNum (track->getIndexInEditTrackList());

            for (auto device : devices)
                if (device->followsTrackSelection)
                    if (num != -1 && num != device->channelStart)
                        device->changeFaderBank (num - device->channelStart, false);
        }
    }
}

void ExternalControllerManager::updateAllDevices()
{
    if (! isTimerRunning())
    {
        auto now = Time::getMillisecondCounter();

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

    lastUpdate = Time::getMillisecondCounter();

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
                    FOR_EACH_DEVICE (updateSoloAndMute (mappedChan, flags, isBright));
                }
            }

            ++i;
            return true;
        });

        FOR_EACH_DEVICE (soloCountChanged (blinkTimer->isBright && anySolo));
    }
}

//==============================================================================
void ExternalControllerManager::moveFader (int channelNum, float newSliderPos)
{
    auto chan = mapTrackNumToChannelNum (channelNum);

    FOR_EACH_DEVICE (moveFader (chan, newSliderPos));
}

void ExternalControllerManager::movePanPot (int channelNum, float newPan)
{
    auto chan = mapTrackNumToChannelNum (channelNum);

    FOR_EACH_DEVICE (movePanPot (chan, newPan));
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
                c->moveFader (chan, vp.getSliderPos());
                c->movePanPot (chan, vp.getPan());
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

            FOR_EACH_DEVICE (moveMasterFaders (left, right));
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
                c->moveFader (chan, vca.getSliderPos());
                c->movePanPot (chan, 0.0f);
            }
        }
    }
}

void ExternalControllerManager::moveMasterFaders (float newLeftPos, float newRightPos)
{
    CRASH_TRACER
    FOR_EACH_DEVICE (moveMasterFaders (newLeftPos, newRightPos));
}

void ExternalControllerManager::soloCountChanged (bool anySoloTracks)
{
    CRASH_TRACER
    FOR_EACH_DEVICE (soloCountChanged (anySoloTracks));
}

void ExternalControllerManager::playStateChanged (bool isPlaying)
{
    CRASH_TRACER
    FOR_EACH_DEVICE (playStateChanged (isPlaying));
}

void ExternalControllerManager::recordStateChanged (bool isRecording)
{
    CRASH_TRACER
    FOR_EACH_DEVICE (recordStateChanged (isRecording));
}

void ExternalControllerManager::automationModeChanged (bool isReading, bool isWriting)
{
    CRASH_TRACER
    FOR_EACH_DEVICE (automationModeChanged (isReading, isWriting));
}

void ExternalControllerManager::channelLevelChanged (int channel, float level)
{
    CRASH_TRACER
    const int cn = mapTrackNumToChannelNum (channel);
    FOR_EACH_DEVICE (channelLevelChanged (cn, level));
}

void ExternalControllerManager::masterLevelsChanged (float leftLevel, float rightLevel)
{
    CRASH_TRACER
    FOR_EACH_DEVICE (masterLevelsChanged (leftLevel, rightLevel));
}

void ExternalControllerManager::timecodeChanged (int barsOrHours, int beatsOrMinutes, int ticksOrSeconds,
                                                 int millisecs, bool isBarsBeats, bool isFrames)
{
    CRASH_TRACER
    FOR_EACH_DEVICE (timecodeChanged (barsOrHours, beatsOrMinutes, ticksOrSeconds, millisecs, isBarsBeats, isFrames));
}

void ExternalControllerManager::snapChanged (bool isOn)
{
    CRASH_TRACER
    FOR_EACH_DEVICE (snapChanged (isOn));
}

void ExternalControllerManager::loopChanged (bool isOn)
{
    CRASH_TRACER
    FOR_EACH_DEVICE (loopChanged (isOn));
}

void ExternalControllerManager::clickChanged (bool isOn)
{
    CRASH_TRACER
    FOR_EACH_DEVICE (clickChanged (isOn));
}

void ExternalControllerManager::editPositionChanged (Edit* ed, double newCursorPosition)
{
    if (ed != nullptr)
    {
        CRASH_TRACER
        String parts[4];
        ed->getTimecodeFormat().getPartStrings (newCursorPosition,
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
    FOR_EACH_DEVICE (auxSendLevelsChanged());
}

//==============================================================================
void ExternalControllerManager::userMovedFader (int channelNum, float newSliderPos)
{
    CRASH_TRACER

    if (auto at = dynamic_cast<AudioTrack*> (getChannelTrack (channelNum)))
    {
        if (auto vp = at->getVolumePlugin())
            vp->setSliderPos (newSliderPos);
        else
            moveFader (mapTrackNumToChannelNum (channelNum), decibelsToVolumeFaderPosition (0.0f));
    }

    if (auto ft = dynamic_cast<FolderTrack*> (getChannelTrack (channelNum)))
    {
        if (auto vca = ft->getVCAPlugin())
            vca->setSliderPos (newSliderPos);
        else
            moveFader (mapTrackNumToChannelNum (channelNum), decibelsToVolumeFaderPosition (0.0f));
    }
}

void ExternalControllerManager::userMovedMasterFader (Edit* ed, float newLevel)
{
    if (ed != nullptr)
        ed->setMasterVolumeSliderPos (newLevel);
}

void ExternalControllerManager::userMovedMasterPanPot (Edit* ed, float newLevel)
{
    if (ed != nullptr)
        ed->setMasterPanPos (newLevel);
}

void ExternalControllerManager::userMovedPanPot (int channelNum, float newPan)
{
    if (auto t = dynamic_cast<AudioTrack*> (getChannelTrack (channelNum)))
        if (auto vp = t->getVolumePlugin())
            vp->setPan (newPan);
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

Colour ExternalControllerManager::getTrackColour (int channelNum)
{
    Colour c;

    if (! devices.isEmpty())
    {
        auto cn = mapTrackNumToChannelNum (channelNum);
        FOR_EACH_DEVICE (getTrackColour (cn, c));
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

Colour ExternalControllerManager::getPluginColour (Plugin* plugin)
{
    Colour c;
    FOR_EACH_DEVICE (getPluginColour (plugin, c));
    return c;
}

void ExternalControllerManager::repaintPlugin (Plugin& plugin)
{
    if (auto c = PluginComponent::getComponentFor (plugin))
        c->updateColour();
}

int ExternalControllerManager::getXTCount()
{
    return engine.getPropertyStorage().getProperty (SettingID::xtCount);
}

void ExternalControllerManager::setXTCount (int after)
{
    CRASH_TRACER
    juce::ignoreUnused (after);

   #if TRACKTION_ENABLE_CONTROL_SURFACES
    if (auto first = devices.getFirst())
    {
        if (auto mcu = first->getControlSurfaceIfType<MackieMCU>())
        {
            int before = getXTCount();
            int diff = after - before;

            if (diff > 0)
            {
                for (int i = 0; i < diff; ++i)
                    devices.insert (before + i + 1, new ExternalController (engine, new MackieXT (*this, *mcu, before + i)));
            }
            else if (diff < 0)
            {
                for (int i = 0; i < std::abs (diff); ++i)
                    devices.remove (before - i);
            }

            engine.getPropertyStorage().setProperty (SettingID::xtCount, after);
            refreshXTOrder();
            sendChangeMessage();
        }
    }
   #endif
}

void ExternalControllerManager::refreshXTOrder()
{
    CRASH_TRACER

   #if TRACKTION_ENABLE_CONTROL_SURFACES
    if (auto first = devices.getFirst())
    {
        if (auto mcu = first->getControlSurfaceIfType<MackieMCU>())
        {
            MackieXT* xt [MackieMCU::maxNumSurfaces + 1] = {};

            for (int i = 1; i < MackieMCU::maxNumSurfaces; ++i)
                xt[i - 1] = devices[i] ? devices[i]->getControlSurfaceIfType<MackieXT>() : nullptr;

            StringArray indices;
            indices.addTokens (engine.getPropertyStorage().getProperty (SettingID::xtIndices, "0 1 2 3").toString(), false);

            for (int i = indices.size(); --i >= 0;)
                if (indices[i].getIntValue() > getXTCount())
                    indices.remove(i);

            mcu->setDeviceIndex (indices.indexOf ("0"));

            if (xt[0]) xt[0]->setDeviceIndex (indices.indexOf ("1"));
            if (xt[1]) xt[1]->setDeviceIndex (indices.indexOf ("2"));
            if (xt[2]) xt[2]->setDeviceIndex (indices.indexOf ("3"));
        }
    }
    else
    {
        jassertfalse;
    }
   #endif
}
