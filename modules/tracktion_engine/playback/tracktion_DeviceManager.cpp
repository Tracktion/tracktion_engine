/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


}

juce::AudioDeviceManager* gDeviceManager = nullptr; // TODO

namespace tracktion_engine
{

static String mergeTwoNames (const String& s1, const String& s2)
{
    String nm;

    const String bracketed1 (s1.fromLastOccurrenceOf ("(", false, false)
                               .upToFirstOccurrenceOf (")", false, false).trim());

    const String bracketed2 (s2.fromLastOccurrenceOf ("(", false, false)
                               .upToFirstOccurrenceOf (")", false, false).trim());

    if ((! (bracketed1.isEmpty() || bracketed2.isEmpty()))
        && s1.endsWithChar (')') && s2.endsWithChar (')')
        && s1.upToLastOccurrenceOf ("(", false, false).trim()
             == s2.upToLastOccurrenceOf ("(", false, false).trim())
    {
        nm << s1.upToLastOccurrenceOf ("(", false, false).trim()
           << " (" << bracketed1 << " + " << bracketed2 << ")";
    }
    else
    {
        String endNum1, endNum2;

        for (int i = s1.length(); --i >= 0;)
            if (CharacterFunctions::isDigit (s1[i]))
                endNum1 = String::charToString (s1[i]) + endNum1;
            else
                break;

        for (int i = s2.length(); --i >= 0;)
            if (CharacterFunctions::isDigit (s2[i]))
                endNum2 = String::charToString (s2[i]) + endNum2;
            else
                break;

        if ((! (endNum1.isEmpty() || endNum2.isEmpty()))
            && s1.substring (0, s1.length() - endNum1.length())
               == s2.substring (0, s2.length() - endNum2.length()))
        {
            nm << s1.substring (0, s1.length() - endNum1.length())
               << endNum1 << " + " << endNum2;
        }
        else
        {
            nm << s1 << " + " << s2;
        }
    }

    return nm;
}

//==============================================================================
struct DeviceManager::WaveDeviceList
{
    WaveDeviceList (DeviceManager& dm_) : dm (dm_)
    {
        if (auto device = dm.deviceManager.getCurrentAudioDevice())
        {
            deviceName = device->getName();

            if (dm.engine.getEngineBehaviour().isDescriptionOfWaveDevicesSupported())
            {
                dm.engine.getEngineBehaviour().describeWaveDevices (inputs, *device, true);
                dm.engine.getEngineBehaviour().describeWaveDevices (outputs, *device, false);
            }
            else
            {
                describeStandardDevices (inputs, *device, true);
                describeStandardDevices (outputs, *device, false);
            }
        }
    }

    void describeStandardDevices (std::vector<WaveDeviceDescription>& descriptions, juce::AudioIODevice& device, bool isInput)
    {
        StringArray channelNames (isInput ? device.getInputChannelNames() : device.getOutputChannelNames());

        if (channelNames.size() == 2)
        {
            const String name (isInput ? TRANS("Input 123") : TRANS("Output 123"));
            channelNames.set (0, name.replace ("123", "1"));
            channelNames.set (1, name.replace ("123", "2"));
        }

        auto isDeviceEnabled = [this, isInput] (int index)
        {
            return isInput ? dm.isDeviceInEnabled (index) : dm.isDeviceOutEnabled (index);
        };

        for (int i = 0; i < channelNames.size(); ++i)
        {
            if (isInput ? dm.isDeviceInChannelStereo (i) : dm.isDeviceOutChannelStereo (i))
            {
                descriptions.push_back (WaveDeviceDescription (mergeTwoNames (channelNames[i], channelNames[i + 1]), i, i + 1, isDeviceEnabled (i) || isDeviceEnabled (i + 1)));
                ++i;
            }
            else
            {
                descriptions.push_back (WaveDeviceDescription (channelNames[i], i, -1, isDeviceEnabled (i)));
                ++i;

                if (i < channelNames.size())
                    descriptions.push_back (WaveDeviceDescription (channelNames[i], i, -1, isDeviceEnabled (i)));
            }
        }
    }

    bool operator== (const WaveDeviceList& other) const noexcept    { return deviceName == other.deviceName && inputs == other.inputs && outputs == other.outputs; }
    bool operator!= (const WaveDeviceList& other) const noexcept    { return ! operator== (other); }

    DeviceManager& dm;
    String deviceName;
    std::vector<WaveDeviceDescription> inputs, outputs;
};


//==============================================================================
DeviceManager::DeviceManager (Engine& e) : engine (e)
{
    CRASH_TRACER

    setInternalBufferMultiplier (engine.getPropertyStorage().getProperty (SettingID::internalBuffer, 1));

    deviceManager.addChangeListener (this);

    gDeviceManager = &deviceManager;
}

DeviceManager::~DeviceManager()
{
    gDeviceManager = nullptr;

    CRASH_TRACER
    deviceManager.removeChangeListener (this);
}

String DeviceManager::getDefaultAudioDeviceName (bool translated)
{
    return translated ? ("(" + TRANS("Default audio output") + ")")
                      : "(default audio output)";
}

String DeviceManager::getDefaultMidiDeviceName (bool translated)
{
    return translated ? ("(" + TRANS("Default MIDI output") + ")")
                      : "(default MIDI output)";
}

void DeviceManager::closeDevices()
{
    CRASH_TRACER
    TRACKTION_ASSERT_MESSAGE_THREAD

    jassert (activeContexts.isEmpty());
    clearAllContextDevices();

    deviceManager.removeAudioCallback (this);

    midiOutputs.clear();
    midiInputs.clear();
    waveInputs.clear();
    waveOutputs.clear();
}

static bool isMicrosoftGSSynth (MidiOutputDevice& mo)
{
   #if JUCE_WINDOWS
    return mo.getName().containsIgnoreCase ("Microsoft GS ");
   #else
    (void) mo;  return false;
   #endif
}

DeviceManager::ContextDeviceListRebuilder::ContextDeviceListRebuilder (DeviceManager& d) : dm (d)
{
    dm.clearAllContextDevices();
}

DeviceManager::ContextDeviceListRebuilder::~ContextDeviceListRebuilder()
{
    dm.reloadAllContextDevices();
}

void DeviceManager::initialiseMidi()
{
    CRASH_TRACER
    TRACKTION_ASSERT_MESSAGE_THREAD
    ContextDeviceListRebuilder deviceRebuilder (*this);

    midiInputs.clear();
    midiOutputs.clear();

    auto& storage = engine.getPropertyStorage();

    defaultMidiIndex = storage.getProperty (SettingID::defaultMidiDevice);

    TRACKTION_LOG ("Finding MIDI I/O");
    lastMidiInNames = MidiInput::getDevices();
    lastMidiInNames.appendNumbersToDuplicates (true, false);

    lastMidiOutNames = MidiOutput::getDevices();
    lastMidiOutNames.appendNumbersToDuplicates (true, false);

    int enabledMidiIns = 0, enabledMidiOuts = 0;

    // create all the devices before initialising them..
    for (int i = 0; i < lastMidiOutNames.size(); ++i)  midiOutputs.add (new MidiOutputDevice (engine, lastMidiOutNames[i], i));
    for (int i = 0; i < lastMidiInNames.size(); ++i)   midiInputs.add (new PhysicalMidiInputDevice (engine, lastMidiInNames[i], i));

   #if JUCE_MAC && JUCE_DEBUG
    midiOutputs.add (new MidiOutputDevice (engine, "Tracktion MIDI Device", -1));
   #endif

    StringArray virtualDevices;
    virtualDevices.addTokens (storage.getProperty (SettingID::virtualmididevices).toString(), ";", {});
    virtualDevices.removeEmptyStrings();

    for (int i = 0; i < virtualDevices.size(); ++i)
        midiInputs.add (new VirtualMidiInputDevice (engine, virtualDevices[i], InputDevice::virtualMidiDevice));

    for (auto mo : midiOutputs)
    {
        TRACKTION_LOG ("MIDI output: " + mo->getName() + (mo->isEnabled() ? " (enabled)" : ""));

        JUCE_TRY
        {
            mo->setEnabled (mo->isEnabled());

            if (mo->isEnabled())
                ++enabledMidiOuts;
        }
        JUCE_CATCH_EXCEPTION
    }

    for (auto mi : midiInputs)
    {
        TRACKTION_LOG ("MIDI input: " + mi->getName() + (mi->isEnabled() ? " (enabled)" : ""));

        JUCE_TRY
        {
            mi->setEnabled (mi->isEnabled());
            mi->initialiseDefaultAlias();

            if (mi->isEnabled())
                ++enabledMidiIns;
        }
        JUCE_CATCH_EXCEPTION
    }

    const bool hasEnabledMidiDefaultDevs = storage.getProperty (SettingID::hasEnabledMidiDefaultDevs, false);
    storage.setProperty (SettingID::hasEnabledMidiDefaultDevs, true);
    storage.flushSettingsToDisk();

    if (enabledMidiOuts + enabledMidiIns == 0 && ! hasEnabledMidiDefaultDevs)
    {
        if (auto firstIn = midiInputs[0])
            firstIn->setEnabled (true);

        if (auto firstOut = midiOutputs[0])
            if (! isMicrosoftGSSynth (*firstOut))
                firstOut->setEnabled (true);
    }
}

void DeviceManager::initialise (int defaultNumInputs, int defaultNumOutputs)
{
    defaultNumInputChannelsToOpen = defaultNumInputs;
    defaultNumOutputChannelsToOpen = defaultNumOutputs;

    initialiseMidi();
    loadSettings();
    finishedInitialising = true;
    rebuildWaveDeviceList();
    updateNumCPUs();
}

void DeviceManager::changeListenerCallback (ChangeBroadcaster*)
{
    CRASH_TRACER

    if (! rebuildWaveDeviceListIfNeeded())
    {
        // force all plugins to be restarted, to cope with changes in rate + buffer size
        const ScopedLock sl (contextLock);

        for (auto c : activeContexts)
            c->edit.restartPlayback();
    }
}

bool DeviceManager::rebuildWaveDeviceListIfNeeded()
{
    if (! waveDeviceListNeedsRebuilding())
        return false;

    rebuildWaveDeviceList();
    return true;
}

void DeviceManager::rescanMidiDeviceList (bool forceRescan)
{
    CRASH_TRACER

    auto midiIns = MidiInput::getDevices();
    auto midiOuts = MidiOutput::getDevices();
    midiIns.appendNumbersToDuplicates (true, false);
    midiOuts.appendNumbersToDuplicates (true, false);

    if (lastMidiOutNames != midiOuts || lastMidiInNames != midiIns || forceRescan)
        initialiseMidi();
}

bool DeviceManager::isMSWavetableSynthPresent() const
{
    for (auto mo : midiOutputs)
        if (mo->isEnabled() && isMicrosoftGSSynth (*mo))
            return true;

    return false;
}

void DeviceManager::resetToDefaults (bool resetInputDevices, bool resetOutputDevices)
{
    TRACKTION_LOG ("Returning audio settings to defaults");
    deviceManager.removeAudioCallback (this);

    auto& storage = engine.getPropertyStorage();

    storage.removeProperty (SettingID::audio_device_setup);
    storage.removePropertyItem (SettingID::audiosettings, deviceManager.getCurrentAudioDeviceType());

    storage.setProperty (SettingID::cpu, SystemStats::getNumCpus());
    updateNumCPUs();
    storage.setProperty (SettingID::maxLatency, 5.0f);
    storage.setProperty (SettingID::lowLatencyBuffer, 5.8f);
    storage.setProperty (SettingID::internalBuffer, 1);
    setInternalBufferMultiplier (1);

    storage.setProperty (SettingID::use64Bit, false);
    storage.setProperty (SettingID::showOnlyEnabledDevices, false);

    if (resetInputDevices)
        for (auto wid : waveInputs)
            wid->resetToDefault();

    if (resetOutputDevices)
        for (auto wod : waveOutputs)
            wod->resetToDefault();

    loadSettings();
    deviceManager.addAudioCallback (this);
    TransportControl::restartAllTransports (engine, false);
    SelectionManager::refreshAllPropertyPanels();
}

Result DeviceManager::createVirtualMidiDevice (const String& name)
{
    CRASH_TRACER
    TRACKTION_ASSERT_MESSAGE_THREAD

    {
        StringArray virtualDevices;
        virtualDevices.addTokens (engine.getPropertyStorage().getProperty (SettingID::virtualmididevices).toString(), ";", {});
        virtualDevices.removeEmptyStrings();

        if (virtualDevices.contains (name))
            return Result::fail (TRANS("Name is already in use!"));
    }

    {
        ContextDeviceListRebuilder deviceRebuilder (*this);

        auto vmi = new VirtualMidiInputDevice (engine, name, InputDevice::virtualMidiDevice);
        midiInputs.add (vmi);

        vmi->setEnabled (true);
        vmi->initialiseDefaultAlias();
        vmi->saveProps();
    }

    VirtualMidiInputDevice::refreshDeviceNames (engine);
    sendChangeMessage();

    return Result::ok();
}

void DeviceManager::deleteVirtualMidiDevice (VirtualMidiInputDevice* vmi)
{
    CRASH_TRACER
    TRACKTION_ASSERT_MESSAGE_THREAD
    ContextDeviceListRebuilder deviceRebuilder (*this);

    engine.getPropertyStorage().removePropertyItem (SettingID::virtualmidiin, vmi->getName());
    midiInputs.removeObject (vmi);
    VirtualMidiInputDevice::refreshDeviceNames (engine);
    sendChangeMessage();
}

void DeviceManager::sanityCheckEnabledChannels()
{
    for (int i = outEnabled.getHighestBit() + 2; --i >= 0;)
    {
        if (isDeviceOutChannelStereo (i))
        {
            const int chan = i & ~1;
            const bool en = outEnabled[chan] || outEnabled[chan + 1];
            outEnabled.setBit (chan, en);
            outEnabled.setBit (chan + 1, en);
        }
    }

    for (int i = inEnabled.getHighestBit() + 2; --i >= 0;)
    {
        if (isDeviceInChannelStereo (i))
        {
            const int chan = i & ~1;
            const bool en = inEnabled[chan] || inEnabled[chan + 1];
            inEnabled.setBit (chan, en);
            inEnabled.setBit (chan + 1, en);
        }
    }

    if (currentSampleRate < 22050 || currentSampleRate > 200000)
        currentSampleRate = 44100.0;
}

bool DeviceManager::waveDeviceListNeedsRebuilding()
{
    WaveDeviceList newList (*this);
    return lastWaveDeviceList == nullptr || newList != *lastWaveDeviceList;
}

void DeviceManager::rebuildWaveDeviceList()
{
    CRASH_TRACER
    TRACKTION_ASSERT_MESSAGE_THREAD

    static bool reentrant = false;
    jassert (! reentrant);

    if (reentrant)
        return;

    const ScopedValueSetter<bool> v (reentrant, true);

    TRACKTION_LOG ("Rebuilding Wave Device List...");

    TransportControl::stopAllTransports (engine, false, true);

    ContextDeviceListRebuilder deviceRebuilder (*this);

    deviceManager.removeAudioCallback (this);

    waveInputs.clear();
    waveOutputs.clear();

    sanityCheckEnabledChannels();

    lastWaveDeviceList = new WaveDeviceList (*this);

    for (const auto& d : lastWaveDeviceList->inputs)
    {
        auto wi = new WaveInputDevice (engine, d.name, TRANS("Wave Audio Input"), d.channels, InputDevice::waveDevice);
        wi->enabled = d.enabled;
        waveInputs.add (wi);

        TRACKTION_LOG ("Wave In: " + wi->getName() + (wi->isEnabled() ? " (enabled): " : ": ")
                        + createDescriptionOfChannels (wi->deviceChannels));
    }

    for (auto wi : waveInputs)
        wi->initialiseDefaultAlias();

    for (const auto& d : lastWaveDeviceList->outputs)
    {
        auto wo = new WaveOutputDevice (engine, d.name, d.channels);
        wo->enabled = d.enabled;
        waveOutputs.add (wo);

        TRACKTION_LOG ("Wave Out: " + wo->getName() + (wo->isEnabled() ? " (enabled): " : ": ")
                        + createDescriptionOfChannels (wo->deviceChannels));
    }

    activeOutChannels.clear();

    for (auto wo : waveOutputs)
        if (wo->isEnabled())
            for (const auto& ci : wo->getChannels())
                activeOutChannels.setBit (ci.indexInDevice);

    saveSettings();
    checkDefaultDevicesAreValid();

  #if TRACKTION_LOG_ENABLED
    auto wo = getDefaultWaveOutDevice();
    TRACKTION_LOG ("Default Wave: " + (wo != nullptr ? wo->getName() : String()));

    auto mo = getDefaultMidiOutDevice();
    TRACKTION_LOG ("Default MIDI: " + (mo != nullptr ? mo->getName() : String()));
  #endif

    sendChangeMessage();

    TransportControl::restartAllTransports (engine, false);

    checkDefaultDevicesAreValid();
    deviceManager.addAudioCallback (this);
}

void DeviceManager::setSpeedCompensation (double plusOrMinus)
{
    speedCompensation = jlimit (-10.0, 10.0, plusOrMinus);
}

void DeviceManager::setInternalBufferMultiplier (int multiplier)
{
    internalBufferMultiplier = jlimit (1, 10, multiplier);

    const ScopedLock sl (contextLock);

    for (auto c : activeContexts)
    {
        c->clearNodes();
        c->edit.restartPlayback();
    }
}

void DeviceManager::loadSettings()
{
    String error;
    auto& storage = engine.getPropertyStorage();

    {
        auto audioXml = storage.getXmlProperty (SettingID::audio_device_setup);

        CRASH_TRACER
        if (audioXml != nullptr)
            error = deviceManager.initialise (defaultNumInputChannelsToOpen, defaultNumOutputChannelsToOpen, audioXml.get(), true);
        else
            error = deviceManager.initialiseWithDefaultDevices (defaultNumInputChannelsToOpen, defaultNumOutputChannelsToOpen);

        if (error.isNotEmpty())
            TRACKTION_LOG_ERROR ("AudioDeviceManager init: " + error);
    }

    outMonoChans.clear();
    inStereoChans.clear();
    outEnabled.clear();
    outEnabled.setBit (0);
    outEnabled.setBit (1);
    inEnabled.clear();
    inEnabled.setBit (0);
    inEnabled.setBit (1);

    if (! engine.getEngineBehaviour().isDescriptionOfWaveDevicesSupported())   //else UI will take care about inputs/outputs names and their mapping to device channels
    {
        if (auto n = storage.getXmlPropertyItem (SettingID::audiosettings, deviceManager.getCurrentAudioDeviceType()))
        {
            outMonoChans.parseString (n->getStringAttribute ("monoChansOut", outMonoChans.toString (2)), 2);
            inStereoChans.parseString (n->getStringAttribute ("stereoChansIn", inStereoChans.toString (2)), 2);
            outEnabled.parseString (n->getStringAttribute ("outEnabled", outEnabled.toString (2)), 2);
            inEnabled.parseString (n->getStringAttribute ("inEnabled", inEnabled.toString (2)), 2);
        }
    }

    TRACKTION_LOG ("Audio block size: " + String (getBlockSize()) + "  Rate: " + String ((int) getSampleRate()));
}

void DeviceManager::saveSettings()
{
    auto& storage = engine.getPropertyStorage();

    if (auto audioXml = std::unique_ptr<XmlElement> (deviceManager.createStateXml()))
        storage.setXmlProperty (SettingID::audio_device_setup, *audioXml);

    if (! engine.getEngineBehaviour().isDescriptionOfWaveDevicesSupported())
    {
        if (deviceManager.getCurrentAudioDevice() != nullptr)
        {
            XmlElement n ("AUDIODEVICE");

            n.setAttribute ("outEnabled", outEnabled.toString (2));
            n.setAttribute ("inEnabled", inEnabled.toString (2));
            n.setAttribute ("monoChansOut", outMonoChans.toString (2));
            n.setAttribute ("stereoChansIn", inStereoChans.toString (2));

            storage.setXmlPropertyItem (SettingID::audiosettings, deviceManager.getCurrentAudioDeviceType(), n);
        }
    }
}

void DeviceManager::checkDefaultDevicesAreValid()
{
    if (! finishedInitialising)
        return;

    auto& storage = engine.getPropertyStorage();

    if (getDefaultWaveOutDevice() == nullptr
         || ! getDefaultWaveOutDevice()->isEnabled())
    {
        defaultWaveIndex = -1;

        for (int i = 0; i < getNumWaveOutDevices(); ++i)
        {
            if (getWaveOutDevice(i) != 0 && getWaveOutDevice(i)->isEnabled())
            {
                defaultWaveIndex = i;
                break;
            }
        }

        if (defaultWaveIndex >= 0)
            storage.setPropertyItem (SettingID::defaultWaveDevice, deviceManager.getCurrentAudioDeviceType(), defaultWaveIndex);
    }

    if (getDefaultMidiOutDevice() == nullptr
         || ! getDefaultMidiOutDevice()->isEnabled())
    {
        defaultMidiIndex = -1;

        for (int i = 0; i < getNumMidiOutDevices(); ++i)
        {
            if (getMidiOutDevice(i) != 0 && getMidiOutDevice(i)->isEnabled())
            {
                defaultMidiIndex = i;
                break;
            }
        }

        if (defaultMidiIndex >= 0)
            storage.setProperty (SettingID::defaultMidiDevice, defaultMidiIndex);
    }
}

double DeviceManager::getSampleRate() const
{
    if (auto device = deviceManager.getCurrentAudioDevice())
        return device->getCurrentSampleRate();

    return 44100;
}

int DeviceManager::getBitDepth() const
{
    if (auto device = deviceManager.getCurrentAudioDevice())
        return device->getCurrentBitDepth();

    return 16;
}

int DeviceManager::getBlockSize() const
{
    if (auto device = deviceManager.getCurrentAudioDevice())
        return device->getCurrentBufferSizeSamples();

    return 16;
}

double DeviceManager::getBlockSizeMs() const
{
    return getBlockSize() * 1000.0 / getSampleRate();
}

void DeviceManager::setDefaultWaveOutDevice (int index)
{
    if (auto wod = getWaveOutDevice (index))
    {
        if (wod->isEnabled())
        {
            defaultWaveIndex = index;
            engine.getPropertyStorage().setPropertyItem (SettingID::defaultWaveDevice,
                                                         deviceManager.getCurrentAudioDeviceType(), index);
        }
    }

    checkDefaultDevicesAreValid();
    rebuildWaveDeviceList();
}

void DeviceManager::setDeviceOutChannelStereo (int chan, bool isStereoPair)
{
    chan &= ~1;

    if (outMonoChans[chan / 2] == isStereoPair)
    {
        outMonoChans.setBit (chan / 2, ! isStereoPair);

        if (isStereoPair)
        {
            const bool en = outEnabled[chan] || outEnabled[chan + 1];
            outEnabled.setBit (chan, en);
            outEnabled.setBit (chan + 1, en);
        }

        rebuildWaveDeviceList();
    }

    checkDefaultDevicesAreValid();
}

void DeviceManager::setDeviceInChannelStereo (int chan, bool isStereoPair)
{
    chan &= ~1;

    if (inStereoChans[chan / 2] != isStereoPair)
    {
        inStereoChans.setBit (chan / 2, isStereoPair);

        if (isStereoPair)
        {
            const bool en = inEnabled[chan] || inEnabled[chan + 1];
            inEnabled.setBit (chan, en);
            inEnabled.setBit (chan + 1, en);
        }
    }

    rebuildWaveDeviceList();
}

void DeviceManager::setWaveOutChannelsEnabled (const std::vector<ChannelIndex>& channels, bool b)
{
    bool needsRebuilding = false;

    for (const auto& ci : channels)
    {
        if (outEnabled[ci.indexInDevice] != b)
        {
            needsRebuilding = true;
            outEnabled.setBit (ci.indexInDevice, b);
        }
    }

    if (needsRebuilding)
        rebuildWaveDeviceList();
}

void DeviceManager::setWaveInChannelsEnabled (const std::vector<ChannelIndex>& channels, bool b)
{
    bool needsRebuilding = false;

    for (const auto& ci : channels)
    {
        if (inEnabled[ci.indexInDevice] != b)
        {
            needsRebuilding = true;
            inEnabled.setBit (ci.indexInDevice, b);
        }
    }

    if (needsRebuilding)
        rebuildWaveDeviceList();
}

void DeviceManager::setDefaultMidiOutDevice (int index)
{
    if (midiOutputs[index] != 0 && midiOutputs[index]->isEnabled())
    {
        defaultMidiIndex = index;
        engine.getPropertyStorage().setProperty (SettingID::defaultMidiDevice, defaultMidiIndex);
    }

    checkDefaultDevicesAreValid();
    rebuildWaveDeviceList();
}

int DeviceManager::getNumMidiInDevices() const
{
    return midiInputs.size();
}

MidiInputDevice* DeviceManager::getMidiInDevice (int index) const
{
    TRACKTION_ASSERT_MESSAGE_THREAD
    return midiInputs[index];
}

void DeviceManager::broadcastStreamTimeToMidiDevices (double timeToBroadcast)
{
    const ScopedLock sl (midiInputs.getLock());

    for (auto mi : midiInputs)
        if (mi->isEnabled())
            mi->masterTimeUpdate (timeToBroadcast);
}

int DeviceManager::getNumInputDevices() const
{
    return getNumWaveInDevices() + getNumMidiInDevices();
}

InputDevice* DeviceManager::getInputDevice (int index) const
{
    if (index >= getNumWaveInDevices())
        return getMidiInDevice (index - getNumWaveInDevices());

    return getWaveInDevice (index);
}

int DeviceManager::getNumOutputDevices() const
{
    return getNumWaveOutDevices() + getNumMidiOutDevices();
}

OutputDevice* DeviceManager::getOutputDeviceAt (int index) const
{
    if (index >= getNumWaveOutDevices())
        return getMidiOutDevice (index - getNumWaveOutDevices());

    return getWaveOutDevice (index);
}

OutputDevice* DeviceManager::findOutputDeviceForID (const String& id) const
{
    for (auto d : waveOutputs)
        if (d->getDeviceID() == id)
            return d;

    for (auto d : midiOutputs)
        if (d->getDeviceID() == id)
            return d;

    return {};
}

OutputDevice* DeviceManager::findOutputDeviceWithName (const String& name) const
{
    if (name == getDefaultAudioDeviceName (false))     return getDefaultWaveOutDevice();
    if (name == getDefaultMidiDeviceName (false))      return getDefaultMidiOutDevice();

    for (auto d : waveOutputs)
        if (d->getName() == name)
            return d;

    for (auto d : midiOutputs)
        if (d->getName() == name)
            return d;

    return {};
}

int DeviceManager::getRecordAdjustmentSamples()
{
    if (auto d = deviceManager.getCurrentAudioDevice())
        return d->getOutputLatencyInSamples() + d->getInputLatencyInSamples();

    return 0;
}

double DeviceManager::getRecordAdjustmentMs()
{
    if (auto d = deviceManager.getCurrentAudioDevice())
        return getRecordAdjustmentSamples() * 1000.0 / d->getCurrentSampleRate();

    return 0.0;
}

double DeviceManager::getOutputLatencySeconds() const
{
    return outputLatencyTime;
}

void DeviceManager::audioDeviceIOCallback (const float** inputChannelData, int numInputChannels,
                                           float** outputChannelData, int totalNumOutputChannels,
                                           int numSamples)
{
    CRASH_TRACER
    FloatVectorOperations::disableDenormalisedNumberSupport();

    {
       #if JUCE_ANDROID
        const ScopedSteadyLoad load (steadyLoadContext, numSamples);
       #endif

        const auto startTimeTicks = Time::getHighResolutionTicks();

        if (currentCpuUsage > cpuLimitBeforeMuting)
        {
            for (int i = 0; i < totalNumOutputChannels; ++i)
                if (auto dest = outputChannelData[i])
                    FloatVectorOperations::clear (dest, numSamples);

            currentCpuUsage = jmin (0.9, currentCpuUsage * 0.99);
            Thread::sleep (1);
        }
        else
        {
            broadcastStreamTimeToMidiDevices (streamTime + outputLatencyTime);
            EditTimeRange blockStreamTime;

            {
                SCOPED_REALTIME_CHECK
                const ScopedLock sl (contextLock);

                for (auto wi : waveInputs)
                    wi->consumeNextAudioBlock (inputChannelData, numInputChannels, numSamples, streamTime);

                for (int i = totalNumOutputChannels; --i >= 0;)
                    if (auto dest = outputChannelData[i])
                        FloatVectorOperations::clear (dest, numSamples);

                double blockLength = numSamples / currentSampleRate;

                if (speedCompensation != 0.0)
                    blockLength *= (1.0 + (speedCompensation * 0.01));

                blockStreamTime = { streamTime, streamTime + blockLength };

                for (auto c : activeContexts)
                    c->fillNextAudioBlock (blockStreamTime, outputChannelData, numSamples);
            }

           #if JUCE_MAC
            for (int i = totalNumOutputChannels; --i >= 0;)
                if (auto* dest = outputChannelData[i])
                    if (activeOutChannels[i])
                        for (int j = 0; j < numSamples; ++j)
                            if (! std::isnormal (dest[j]))
                                dest[j] = 0;
           #endif

            streamTime = blockStreamTime.getEnd();
            currentCpuUsage = deviceManager.getCpuUsage();
        }

        if (globalOutputAudioProcessor != nullptr)
        {
            AudioBuffer<float> ab (outputChannelData, totalNumOutputChannels, numSamples);
            MidiBuffer mb;
            globalOutputAudioProcessor->processBlock (ab, mb);
        }

        {
            const auto timeWindowSec = numSamples / static_cast<float> (currentSampleRate);

            const auto currentCpuUtilisation = float (Time::getHighResolutionTicks() - startTimeTicks) / Time::getHighResolutionTicksPerSecond() / timeWindowSec;

            cpuAvg += currentCpuUtilisation;
            cpuMin = jmin (cpuMin, currentCpuUtilisation);
            cpuMax = jmax (cpuMax, currentCpuUtilisation);

            if (currentCpuUtilisation > 1)
                ++glitchCntr;

            if (--cpuAvgCounter == 0)
            {
                cpuAvg /= static_cast<float> (cpuReportingInterval);

                cpuUsageListeners.call (&CPUUsageListener::reportCPUUsage, cpuAvg, cpuMin, cpuMax, glitchCntr);

                cpuAvg = 0;
                cpuMin = 1;
                cpuMax = 0;
                glitchCntr = 0;
                cpuAvgCounter = cpuReportingInterval;
            }
        }
    }
}

void DeviceManager::audioDeviceAboutToStart (AudioIODevice* device)
{
    FloatVectorOperations::disableDenormalisedNumberSupport();

    streamTime = 0;
    currentCpuUsage = 0.0f;
    currentSampleRate = device->getCurrentSampleRate();
    currentLatencyMs  = device->getCurrentBufferSizeSamples() * 1000.0f / currentSampleRate;
    outputLatencyTime = device->getOutputLatencyInSamples() / currentSampleRate;
    defaultWaveIndex = engine.getPropertyStorage().getPropertyItem (SettingID::defaultWaveDevice, device->getTypeName(), 0);

    if (waveDeviceListNeedsRebuilding())
        rebuildWaveDeviceList();

    reloadAllContextDevices();

    const ScopedLock sl (contextLock);

    for (auto c : activeContexts)
    {
        c->playhead.deviceManagerPositionUpdate (streamTime, streamTime + device->getCurrentBufferSizeSamples() / currentSampleRate);
        c->playhead.setPosition (c->transport.getCurrentPosition());
    }

    if (globalOutputAudioProcessor != nullptr)
        globalOutputAudioProcessor->prepareToPlay (currentSampleRate, device->getCurrentBufferSizeSamples());

    cpuAvgCounter = cpuReportingInterval = jmax (1, static_cast<int> (device->getCurrentSampleRate()) / device->getCurrentBufferSizeSamples());

   #if JUCE_ANDROID
    steadyLoadContext.setSampleRate (device->getCurrentSampleRate());
   #endif
}

void DeviceManager::audioDeviceStopped()
{
    currentCpuUsage = 0.0f;
    callBlocking ([this] { clearAllContextDevices(); });

    if (globalOutputAudioProcessor != nullptr)
        globalOutputAudioProcessor->releaseResources();
}

void DeviceManager::updateNumCPUs()
{
    const ScopedLock sl (deviceManager.getAudioCallbackLock());
    MixerAudioNode::updateNumCPUs (engine);
}

void DeviceManager::addContext (EditPlaybackContext* c)
{
    TRACKTION_ASSERT_MESSAGE_THREAD

    double lastStreamTime;

    {
        const ScopedLock sl (contextLock);
        lastStreamTime = streamTime;
        activeContexts.addIfNotAlreadyThere (c);
    }

    for (int i = 200; --i >= 0;)
    {
        Thread::sleep (1);
        if (lastStreamTime != streamTime)
            break;
    }
}

void DeviceManager::removeContext (EditPlaybackContext* c)
{
    const ScopedLock sl (contextLock);
    activeContexts.removeAllInstancesOf (c);
}

void DeviceManager::clearAllContextDevices()
{
    const ScopedLock sl (contextLock);

    for (auto c : activeContexts)
        const EditPlaybackContext::ScopedDeviceListReleaser rebuilder (*c, false);
}

void DeviceManager::reloadAllContextDevices()
{
    const ScopedLock sl (contextLock);

    for (auto c : activeContexts)
        const EditPlaybackContext::ScopedDeviceListReleaser rebuilder (*c, true);
}

void DeviceManager::setGlobalOutputAudioProcessor (juce::AudioProcessor* newProcessor)
{
    const ScopedLock sl (deviceManager.getAudioCallbackLock());
    globalOutputAudioProcessor.reset (newProcessor);

    if (globalOutputAudioProcessor != nullptr)
        if (auto* audioIODevice = deviceManager.getCurrentAudioDevice())
            globalOutputAudioProcessor->prepareToPlay (currentSampleRate, audioIODevice->getCurrentBufferSizeSamples());
}
