/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    You may use this code under the terms of the GPL v3 - see LICENCE.md for details.
    For the technical preview this file cannot be licensed commercially.
*/

juce::AudioDeviceManager* gDeviceManager = nullptr; // TODO

namespace tracktion { inline namespace engine
{

#if TRACKTION_LOG_DEVICES
 #define TRACKTION_LOG_DEVICE(text) TRACKTION_LOG(text)
#else
 #define TRACKTION_LOG_DEVICE(text)
#endif

static juce::String mergeTwoNames (const juce::String& s1, const juce::String& s2)
{
    juce::String nm;

    auto bracketed1 = s1.fromLastOccurrenceOf ("(", false, false)
                        .upToFirstOccurrenceOf (")", false, false).trim();

    auto bracketed2 = s2.fromLastOccurrenceOf ("(", false, false)
                        .upToFirstOccurrenceOf (")", false, false).trim();

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
        juce::String endNum1, endNum2;

        for (int i = s1.length(); --i >= 0;)
            if (juce::CharacterFunctions::isDigit (s1[i]))
                endNum1 = juce::String::charToString (s1[i]) + endNum1;
            else
                break;

        for (int i = s2.length(); --i >= 0;)
            if (juce::CharacterFunctions::isDigit (s2[i]))
                endNum2 = juce::String::charToString (s2[i]) + endNum2;
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

static bool isMicrosoftGSSynth (MidiOutputDevice& mo)
{
   #if JUCE_WINDOWS
    return mo.getName().containsIgnoreCase ("Microsoft GS ");
   #else
    (void) mo;  return false;
   #endif
}

//==============================================================================
DeviceManager::TracktionEngineAudioDeviceManager::TracktionEngineAudioDeviceManager (Engine& e) : engine (e) {}

void DeviceManager::TracktionEngineAudioDeviceManager::createAudioDeviceTypes (juce::OwnedArray<juce::AudioIODeviceType>& types)
{
    if (engine.getEngineBehaviour().addSystemAudioIODeviceTypes())
        juce::AudioDeviceManager::createAudioDeviceTypes (types);
}

//==============================================================================
struct DeviceManager::AvailableWaveDeviceList
{
    AvailableWaveDeviceList (DeviceManager& d) : dm (d)
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
        auto channelNames = isInput ? device.getInputChannelNames()
                                    : device.getOutputChannelNames();

        if (channelNames.size() == 2)
        {
            juce::String name (isInput ? TRANS("Input 123") : TRANS("Output 123"));
            channelNames.set (0, name.replace ("123", "1"));
            channelNames.set (1, name.replace ("123", "2"));
        }

        auto isDeviceEnabled = [this, isInput] (int index)
        {
            return isInput ? dm.isDeviceInEnabled (index) : dm.isDeviceOutEnabled (index);
        };

        for (int i = 0; i < channelNames.size(); ++i)
        {
            const bool canBeStereo = i < channelNames.size() - 1;

            if (canBeStereo && (isInput ? dm.isDeviceInChannelStereo (i)
                                        : dm.isDeviceOutChannelStereo (i)))
            {
                descriptions.push_back (WaveDeviceDescription (mergeTwoNames (channelNames[i], channelNames[i + 1]),
                                                               i, i + 1, isDeviceEnabled (i) || isDeviceEnabled (i + 1)));
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

    bool operator== (const AvailableWaveDeviceList& other) const noexcept    { return deviceName == other.deviceName && inputs == other.inputs && outputs == other.outputs; }
    bool operator!= (const AvailableWaveDeviceList& other) const noexcept    { return ! operator== (other); }

    DeviceManager& dm;
    juce::String deviceName;
    std::vector<WaveDeviceDescription> inputs, outputs;
};


//==============================================================================
struct DeviceManager::PrepareToStartCaller  : public juce::AsyncUpdater
{
    PrepareToStartCaller (DeviceManager& owner) : deviceManager (owner) {}

    void handleAsyncUpdate() override
    {
        deviceManager.prepareToStart();
    }

    DeviceManager& deviceManager;
};

//==============================================================================
//==============================================================================
DeviceManager::DeviceManager (Engine& e) : engine (e)
{
    CRASH_TRACER

    prepareToStartCaller = std::make_unique<PrepareToStartCaller> (*this);

    deviceManager.addChangeListener (this);

    gDeviceManager = &deviceManager;
}

DeviceManager::~DeviceManager()
{
    gDeviceManager = nullptr;

    CRASH_TRACER
    deviceManager.removeChangeListener (this);
}

void DeviceManager::initialise (int defaultNumInputs, int defaultNumOutputs)
{
    defaultNumInputChannelsToOpen = defaultNumInputs;
    defaultNumOutputChannelsToOpen = defaultNumOutputs;

    rescanMidiDeviceList();
    loadSettings();
    finishedInitialising = true;
    rescanWaveDeviceList();
    updateNumCPUs();

    deviceManager.addAudioCallback (this);
}

void DeviceManager::closeDevices()
{
    CRASH_TRACER
    TRACKTION_ASSERT_MESSAGE_THREAD

    jassert (activeContexts.isEmpty());
    clearAllContextDevices();

    deviceManager.removeAudioCallback (this);

    midiOutputs.clear();

    {
        const std::unique_lock sl (midiInputsMutex);
        midiInputs.clear();
    }

    waveInputs.clear();
    waveOutputs.clear();
}

void DeviceManager::resetToDefaults (bool deviceSettings, bool resetInputDevices,
                                     bool resetOutputDevices, bool latencySettings, bool mixSettings)
{
    TRACKTION_LOG ("Returning audio settings to defaults");

    auto& storage = engine.getPropertyStorage();

    if (deviceSettings)
    {
        storage.removeProperty (SettingID::audio_device_setup);
        storage.removePropertyItem (SettingID::audiosettings, deviceManager.getCurrentAudioDeviceType());
    }

    if (latencySettings)
    {
        storage.setProperty (SettingID::maxLatency, 5.0f);
        storage.setProperty (SettingID::lowLatencyBuffer, 5.8f);
    }

    if (mixSettings)
    {
        storage.setProperty (SettingID::cpu, juce::SystemStats::getNumCpus());
        updateNumCPUs();

        storage.setProperty (SettingID::use64Bit, false);
    }

    if (resetInputDevices)
        for (auto wid : waveInputs)
            wid->resetToDefault();

    if (resetOutputDevices)
        for (auto wod : waveOutputs)
            wod->resetToDefault();

    loadSettings();
    TransportControl::restartAllTransports (engine, false);
    SelectionManager::refreshAllPropertyPanels();
}

void DeviceManager::rescanMidiDeviceList()
{
    CRASH_TRACER
    TRACKTION_ASSERT_MESSAGE_THREAD

    auto midiIns  = juce::MidiInput::getAvailableDevices();
    auto midiOuts = juce::MidiOutput::getAvailableDevices();

    if (lastMidiIns == midiIns && lastMidiOuts == midiOuts)
        return;

    TRACKTION_LOG ("Updating MIDI I/O devices");

    clearAllContextDevices();

    {
        const std::unique_lock sl (midiInputsMutex);
        midiInputs.clear();
    }

    midiOutputs.clear();

    auto& storage = engine.getPropertyStorage();

    defaultMidiOutID = storage.getProperty (SettingID::defaultMidiOutDevice);
    defaultMidiInID  = storage.getProperty (SettingID::defaultMidiInDevice);

    bool openHardwareMidi = hostedAudioDeviceInterface == nullptr || hostedAudioDeviceInterface->parameters.useMidiDevices;

    if (openHardwareMidi)
        lastMidiIns = midiIns;

    if (openHardwareMidi)
        lastMidiOuts = midiOuts;

    int enabledMidiIns = 0, enabledMidiOuts = 0;

    // create all the devices before initialising them..
    for (auto& info : lastMidiOuts)
        midiOutputs.add (new MidiOutputDevice (engine, info));

    {
        const std::unique_lock sl (midiInputsMutex);

        for (auto& info : lastMidiIns)
            midiInputs.add (new PhysicalMidiInputDevice (engine, info));
    }

    if (hostedAudioDeviceInterface != nullptr)
    {
        midiOutputs.add (hostedAudioDeviceInterface->createMidiOutput());

        const std::unique_lock sl (midiInputsMutex);
        midiInputs.add (hostedAudioDeviceInterface->createMidiInput());
    }

   #if 0 // JUCE_MAC && JUCE_DEBUG
    // This is causing problems on macOS as creating the device will cause the OS to send an async
    // "devices changed" callback which will create the device again etc. in an infinite loop
    if (openHardwareMidi)
        midiOutputs.add (new SoftwareMidiOutputDevice (engine, "Tracktion MIDI Device"));
   #endif

    juce::StringArray virtualDeviceNames;
    virtualDeviceNames.addTokens (storage.getProperty (SettingID::virtualmididevices).toString(), ";", {});
    virtualDeviceNames.removeEmptyStrings();

    virtualDeviceNames.removeString (allMidiInsName);
    virtualDeviceNames.insert (0, allMidiInsName);

    {
        const std::unique_lock sl (midiInputsMutex);

        for (int i = 0; i < virtualDeviceNames.size(); ++i)
        {
            auto vmd = new VirtualMidiInputDevice (engine, virtualDeviceNames[i], InputDevice::virtualMidiDevice);
            midiInputs.add (vmd);

            if (vmd->getName() == allMidiInsName)
            {
                vmd->setEnabled (true);
                vmd->useAllInputs = true;
            }
        }
    }

    for (auto mo : midiOutputs)
    {
        TRACKTION_LOG_DEVICE ("MIDI output: " + mo->getName() + (mo->isEnabled() ? " (enabled)" : ""));

        JUCE_TRY
        {
            mo->setEnabled (mo->isEnabled());

            if (mo->isEnabled())
                ++enabledMidiOuts;
        }
        JUCE_CATCH_EXCEPTION
    }

    {
        const std::shared_lock sl (midiInputsMutex);

        for (auto mi : midiInputs)
        {
            TRACKTION_LOG_DEVICE ("MIDI input: " + mi->getName() + (mi->isEnabled() ? " (enabled)" : ""));

            JUCE_TRY
            {
                mi->setEnabled (mi->isEnabled());
                mi->initialiseDefaultAlias();

                if (mi->isEnabled())
                    ++enabledMidiIns;
            }
            JUCE_CATCH_EXCEPTION
        }
    }

    const bool hasEnabledMidiDefaultDevs = storage.getProperty (SettingID::hasEnabledMidiDefaultDevs, false);
    storage.setProperty (SettingID::hasEnabledMidiDefaultDevs, true);
    storage.flushSettingsToDisk();

    if (enabledMidiOuts + enabledMidiIns == 0 && ! hasEnabledMidiDefaultDevs)
    {
        {
            const std::shared_lock sl (midiInputsMutex);

            if (auto firstIn = midiInputs[0])
                firstIn->setEnabled (true);
        }

        if (auto firstOut = midiOutputs[0])
            if (! isMicrosoftGSSynth (*firstOut))
                firstOut->setEnabled (true);
    }

    reloadAllContextDevices();
    sendChangeMessage();
}

void DeviceManager::injectMIDIMessageToDefaultDevice (const juce::MidiMessage& m)
{
    if (auto input = getDefaultMidiInDevice())
        input->handleIncomingMidiMessage (nullptr, m);
}

HostedAudioDeviceInterface& DeviceManager::getHostedAudioDeviceInterface()
{
    if (hostedAudioDeviceInterface == nullptr)
        hostedAudioDeviceInterface = std::make_unique<HostedAudioDeviceInterface> (engine);

    return *hostedAudioDeviceInterface;
}

bool DeviceManager::isHostedAudioDeviceInterfaceInUse() const
{
    return hostedAudioDeviceInterface != nullptr
        && deviceManager.getCurrentAudioDeviceType() == "Hosted Device";
}

void DeviceManager::removeHostedAudioDeviceInterface()
{
    for (auto device : deviceManager.getAvailableDeviceTypes())
    {
        if (device->getTypeName() == "Hosted Device")
        {
            deviceManager.removeAudioDeviceType (device);
            break;
        }
    }

    hostedAudioDeviceInterface.reset();
}

juce::String DeviceManager::getDefaultAudioOutDeviceName (bool translated)
{
    return translated ? ("(" + TRANS("Default audio output") + ")")
                      : "(default audio output)";
}

juce::String DeviceManager::getDefaultMidiOutDeviceName (bool translated)
{
    return translated ? ("(" + TRANS("Default MIDI output") + ")")
                      : "(default MIDI output)";
}

juce::String DeviceManager::getDefaultAudioInDeviceName (bool translated)
{
    return translated ? ("(" + TRANS("Default audio input") + ")")
                      : "(default audio input)";
}

juce::String DeviceManager::getDefaultMidiInDeviceName (bool translated)
{
    return translated ? ("(" + TRANS("Default MIDI input") + ")")
                      : "(default MIDI input)";
}

// This is a change in the juce::AudioDeviceManager
void DeviceManager::changeListenerCallback (ChangeBroadcaster*)
{
    CRASH_TRACER
    saveSettings();
    rescanWaveDeviceList();
}

bool DeviceManager::isMSWavetableSynthPresent() const
{
    for (auto mo : midiOutputs)
        if (mo->isEnabled() && isMicrosoftGSSynth (*mo))
            return true;

    return false;
}

juce::Result DeviceManager::createVirtualMidiDevice (const juce::String& name)
{
    CRASH_TRACER
    TRACKTION_ASSERT_MESSAGE_THREAD

    {
        juce::StringArray virtualDeviceNames;
        virtualDeviceNames.addTokens (engine.getPropertyStorage().getProperty (SettingID::virtualmididevices).toString(), ";", {});
        virtualDeviceNames.removeEmptyStrings();

        if (virtualDeviceNames.contains (name))
            return juce::Result::fail (TRANS("Name is already in use!"));
    }

    auto vmi = new VirtualMidiInputDevice (engine, name, InputDevice::virtualMidiDevice);

    {
        const std::unique_lock sl (midiInputsMutex);
        midiInputs.add (vmi);
    }

    vmi->setEnabled (true);
    vmi->initialiseDefaultAlias();
    vmi->saveProps();

    reloadAllContextDevices();

    VirtualMidiInputDevice::refreshDeviceNames (engine);
    sendChangeMessage();

    return juce::Result::ok();
}

void DeviceManager::deleteVirtualMidiDevice (VirtualMidiInputDevice* vmi)
{
    CRASH_TRACER
    TRACKTION_ASSERT_MESSAGE_THREAD

    engine.getPropertyStorage().removePropertyItem (SettingID::virtualmidiin, vmi->getName());

    {
        const std::unique_lock sl (midiInputsMutex);
        midiInputs.removeObject (vmi);
    }

    VirtualMidiInputDevice::refreshDeviceNames (engine);
    reloadAllContextDevices();
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

void DeviceManager::rescanWaveDeviceList()
{
    auto newList = std::make_unique<AvailableWaveDeviceList> (*this);

    if (lastAvailableWaveDeviceList == nullptr || *newList != *lastAvailableWaveDeviceList)
    {
        lastAvailableWaveDeviceList = std::move (newList);
        triggerAsyncUpdate();
    }
}

void DeviceManager::handleAsyncUpdate()
{
    CRASH_TRACER
    TRACKTION_ASSERT_MESSAGE_THREAD

    static bool reentrant = false;
    jassert (! reentrant);

    if (reentrant)
        return;

    const juce::ScopedValueSetter<bool> v (reentrant, true);

    TRACKTION_LOG ("Rebuilding Wave Device List...");

    prepareToStartCaller->handleUpdateNowIfNeeded();

    if (lastAvailableWaveDeviceList == nullptr)
        lastAvailableWaveDeviceList = std::make_unique<AvailableWaveDeviceList> (*this);

    juce::OwnedArray<WaveInputDevice>  newWaveInputs;
    juce::OwnedArray<WaveOutputDevice> newWaveOutputs;
    juce::BigInteger newActiveOutChannels;

    for (auto& d : lastAvailableWaveDeviceList->inputs)
    {
        auto wi = new WaveInputDevice (engine, TRANS("Wave Audio Input"), d, InputDevice::waveDevice);
        newWaveInputs.add (wi);

        TRACKTION_LOG_DEVICE ("Wave In: " + wi->getName() + (wi->isEnabled() ? " (enabled): " : ": ")
                               + createDescriptionOfChannels (wi->deviceChannels));
    }

    for (auto& d : lastAvailableWaveDeviceList->outputs)
    {
        auto wo = new WaveOutputDevice (engine, d);
        newWaveOutputs.add (wo);

        if (wo->isEnabled())
            for (const auto& ci : wo->getChannels())
                newActiveOutChannels.setBit (ci.indexInDevice);

        TRACKTION_LOG_DEVICE ("Wave Out: " + wo->getName() + (wo->isEnabled() ? " (enabled): " : ": ")
                               + createDescriptionOfChannels (wo->deviceChannels));
    }

    clearAllContextDevices();

    {
        const std::unique_lock sl (contextLock);
        newWaveInputs.swapWith (waveInputs);
        newWaveOutputs.swapWith (waveOutputs);
        newActiveOutChannels.swapWith (activeOutChannels);
    }

    for (auto wi : waveInputs)
        wi->initialiseDefaultAlias();

    sanityCheckEnabledChannels();
    reloadAllContextDevices();
    saveSettings();
    checkDefaultDevicesAreValid();

   #if TRACKTION_LOG_ENABLED
    auto wo = getDefaultWaveOutDevice();
    TRACKTION_LOG ("Default Wave Out: " + (wo != nullptr ? wo->getName() : juce::String()));

    auto mo = getDefaultMidiOutDevice();
    TRACKTION_LOG ("Default MIDI Out: " + (mo != nullptr ? mo->getName() : juce::String()));

    auto wi = getDefaultWaveInDevice();
    TRACKTION_LOG ("Default Wave In: " + (wi != nullptr ? wi->getName() : juce::String()));

    auto mi = getDefaultMidiInDevice();
    TRACKTION_LOG ("Default MIDI In: " + (mi != nullptr ? mi->getName() : juce::String()));
   #endif

    sendChangeMessage();
}

void DeviceManager::loadSettings()
{
    juce::String error;
    auto& storage = engine.getPropertyStorage();

    {
        CRASH_TRACER
        if (isHostedAudioDeviceInterfaceInUse())
        {
            error = deviceManager.initialise (defaultNumInputChannelsToOpen,
                                              defaultNumOutputChannelsToOpen,
                                              nullptr, false, "Hosted Device", nullptr);
        }
        else
        {
            auto audioXml = storage.getXmlProperty (SettingID::audio_device_setup);

            if (audioXml != nullptr)
                error = deviceManager.initialise (defaultNumInputChannelsToOpen,
                                                  defaultNumOutputChannelsToOpen,
                                                  audioXml.get(), true);
            else
                error = deviceManager.initialiseWithDefaultDevices (defaultNumInputChannelsToOpen,
                                                                    defaultNumOutputChannelsToOpen);
        }

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

    auto currentDeviceType = deviceManager.getCurrentAudioDeviceType();
    defaultWaveOutID = storage.getPropertyItem (SettingID::defaultWaveOutDevice, currentDeviceType);
    defaultWaveInID  = storage.getPropertyItem (SettingID::defaultWaveInDevice, currentDeviceType);

    TRACKTION_LOG ("Audio block size: " + juce::String (getBlockSize())
                    + "  Rate: " + juce::String ((int) getSampleRate()));
}

void DeviceManager::saveSettings()
{
    auto& storage = engine.getPropertyStorage();

    if (auto audioXml = deviceManager.createStateXml())
        storage.setXmlProperty (SettingID::audio_device_setup, *audioXml);

    if (! engine.getEngineBehaviour().isDescriptionOfWaveDevicesSupported())
    {
        if (deviceManager.getCurrentAudioDevice() != nullptr)
        {
            juce::XmlElement n ("AUDIODEVICE");

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

    if (getDefaultWaveOutDevice() == nullptr || ! getDefaultWaveOutDevice()->isEnabled())
    {
        for (auto d : waveOutputs)
        {
            if (d->isEnabled())
            {
                setDefaultWaveOutDevice (d->getDeviceID());
                break;
            }
        }
    }

    if (getDefaultWaveInDevice() == nullptr || ! getDefaultWaveInDevice()->isEnabled())
    {
        for (auto d : waveInputs)
        {
            if (d->isEnabled())
            {
                setDefaultWaveInDevice (d->getDeviceID());
                break;
            }
        }
    }

    if (getDefaultMidiOutDevice() == nullptr || ! getDefaultMidiOutDevice()->isEnabled())
    {
        for (auto d : midiOutputs)
        {
            if (d->isEnabled())
            {
                setDefaultMidiOutDevice (d->getDeviceID());
                break;
            }
        }
    }

    if (getDefaultMidiInDevice() == nullptr || ! getDefaultMidiInDevice()->isEnabled())
    {
        if (auto allMidi = findInputDeviceForID (allMidiInsName);
            allMidi != nullptr && allMidi->isEnabled())
        {
            setDefaultMidiInDevice (allMidi->getDeviceID());
        }
        else
        {
            const std::shared_lock sl (midiInputsMutex);

            for (auto d : midiInputs)
            {
                if (d->isEnabled())
                {
                    setDefaultMidiInDevice (d->getDeviceID());
                    break;
                }
            }
        }
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

    return 256;
}

double DeviceManager::getBlockSizeMs() const
{
    return getBlockSize() * 1000.0 / getSampleRate();
}

TimeDuration DeviceManager::getBlockLength() const
{
    return TimeDuration::fromSamples (getBlockSize(), getSampleRate());
}

WaveInputDevice* DeviceManager::getDefaultWaveInDevice() const      { return dynamic_cast<WaveInputDevice*> (findInputDeviceForID (getDefaultWaveInDeviceID())); }
WaveOutputDevice* DeviceManager::getDefaultWaveOutDevice() const    { return dynamic_cast<WaveOutputDevice*> (findOutputDeviceForID (getDefaultWaveOutDeviceID())); }
MidiInputDevice* DeviceManager::getDefaultMidiInDevice() const      { return dynamic_cast<MidiInputDevice*> (findInputDeviceForID (getDefaultMidiInDeviceID())); }
MidiOutputDevice* DeviceManager::getDefaultMidiOutDevice() const    { return dynamic_cast<MidiOutputDevice*> (findOutputDeviceForID (getDefaultMidiOutDeviceID())); }

void DeviceManager::setDefaultWaveOutDevice (juce::String deviceID)
{
    if (defaultWaveOutID != deviceID)
    {
        if (auto d = findOutputDeviceForID (deviceID))
        {
            if (d->isEnabled())
            {
                defaultWaveOutID = deviceID;
                engine.getPropertyStorage().setPropertyItem (SettingID::defaultWaveOutDevice,
                                                             deviceManager.getCurrentAudioDeviceType(),
                                                             deviceID);
                rescanWaveDeviceList();
            }
        }
    }
}

void DeviceManager::setDefaultWaveInDevice (juce::String deviceID)
{
    if (defaultWaveInID != deviceID)
    {
        if (auto d = findInputDeviceForID (deviceID))
        {
            if (d->isEnabled())
            {
                defaultWaveInID = deviceID;
                engine.getPropertyStorage().setPropertyItem (SettingID::defaultWaveInDevice,
                                                             deviceManager.getCurrentAudioDeviceType(),
                                                             deviceID);
                rescanWaveDeviceList();
            }
        }
    }
}

void DeviceManager::setDefaultMidiOutDevice (juce::String deviceID)
{
    if (defaultMidiOutID != deviceID)
    {
        if (auto d = findOutputDeviceForID (deviceID))
        {
            if (d->isEnabled())
            {
                defaultMidiOutID = deviceID;
                engine.getPropertyStorage().setProperty (SettingID::defaultMidiOutDevice, deviceID);
                rescanMidiDeviceList();
            }
        }
    }
}

void DeviceManager::setDefaultMidiInDevice (juce::String deviceID)
{
    if (defaultMidiInID != deviceID)
    {
        if (auto d = findInputDeviceForID (deviceID))
        {
            if (d->isEnabled())
            {
                defaultMidiInID = deviceID;
                engine.getPropertyStorage().setProperty (SettingID::defaultMidiInDevice, deviceID);
                rescanMidiDeviceList();
            }
        }
    }
}

void DeviceManager::setDeviceOutChannelStereo (int chan, bool isStereoPair)
{
    chan &= ~1;

    if (isDeviceOutChannelStereo (chan) != isStereoPair)
    {
        outMonoChans.setBit (chan / 2, ! isStereoPair);

        if (isStereoPair)
        {
            const bool en = outEnabled[chan] || outEnabled[chan + 1];
            outEnabled.setBit (chan, en);
            outEnabled.setBit (chan + 1, en);
        }

        rescanWaveDeviceList();
    }
}

void DeviceManager::setDeviceInChannelStereo (int chan, bool isStereoPair)
{
    chan &= ~1;

    if (isDeviceInChannelStereo (chan) != isStereoPair)
    {
        inStereoChans.setBit (chan / 2, isStereoPair);

        if (isStereoPair)
        {
            const bool en = inEnabled[chan] || inEnabled[chan + 1];
            inEnabled.setBit (chan, en);
            inEnabled.setBit (chan + 1, en);
        }

        rescanWaveDeviceList();
    }
}

void DeviceManager::setWaveOutChannelsEnabled (const std::vector<ChannelIndex>& channels, bool b)
{
    for (auto& ci : channels)
    {
        if (outEnabled[ci.indexInDevice] != b)
        {
            outEnabled.setBit (ci.indexInDevice, b);
            rescanWaveDeviceList();
        }
    }
}

void DeviceManager::setWaveInChannelsEnabled (const std::vector<ChannelIndex>& channels, bool b)
{
    for (auto& ci : channels)
    {
        if (inEnabled[ci.indexInDevice] != b)
        {
            inEnabled.setBit (ci.indexInDevice, b);
            rescanWaveDeviceList();
        }
    }
}

int DeviceManager::getNumMidiInDevices() const
{
    const std::shared_lock sl (midiInputsMutex);
    return midiInputs.size();
}

MidiInputDevice* DeviceManager::getMidiInDevice (int index) const
{
    TRACKTION_ASSERT_MESSAGE_THREAD
    const std::shared_lock sl (midiInputsMutex);
    return midiInputs[index];
}

void DeviceManager::broadcastStreamTimeToMidiDevices (double timeToBroadcast)
{
    const std::shared_lock sl (midiInputsMutex);

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

InputDevice* DeviceManager::findInputDeviceForID (const juce::String& id) const
{
    for (auto d : waveInputs)
        if (d->getDeviceID() == id)
            return d;

    const std::shared_lock sl (midiInputsMutex);

    for (auto d : midiInputs)
        if (d->getDeviceID() == id)
            return d;

    return {};
}

InputDevice* DeviceManager::findInputDeviceWithName (const juce::String& name) const
{
    if (name == getDefaultAudioInDeviceName (false))     return getDefaultWaveInDevice();
    if (name == getDefaultMidiInDeviceName (false))      return getDefaultMidiInDevice();

    for (auto d : waveInputs)
        if (d->getName() == name)
            return d;

    const std::shared_lock sl (midiInputsMutex);

    for (auto d : midiInputs)
        if (d->getName() == name)
            return d;

    return {};
}

OutputDevice* DeviceManager::findOutputDeviceForID (const juce::String& id) const
{
    for (auto d : waveOutputs)
        if (d->getDeviceID() == id)
            return d;

    for (auto d : midiOutputs)
        if (d->getDeviceID() == id)
            return d;

    return {};
}

OutputDevice* DeviceManager::findOutputDeviceWithName (const juce::String& name) const
{
    if (name == getDefaultAudioOutDeviceName (false))     return getDefaultWaveOutDevice();
    if (name == getDefaultMidiOutDeviceName (false))      return getDefaultMidiOutDevice();

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

PerformanceMeasurement::Statistics DeviceManager::getCPUStatistics() const
{
    return performanceStats.load();
}

void DeviceManager::restCPUStatistics()
{
    clearStatsFlag.store (true, std::memory_order_relaxed);
}

void DeviceManager::audioDeviceIOCallbackWithContext (const float* const* inputChannelData, int numInputChannels,
                                                      float* const* outputChannelData, int totalNumOutputChannels,
                                                      int numSamples,
                                                      const juce::AudioIODeviceCallbackContext&)
{
    CRASH_TRACER
    juce::FloatVectorOperations::disableDenormalisedNumberSupport();

   #if JUCE_ANDROID
    const ScopedSteadyLoad load (steadyLoadContext, numSamples);
   #endif

    if (isSuspended)
    {
        for (int i = 0; i < totalNumOutputChannels; ++i)
            if (auto dest = outputChannelData[i])
                juce::FloatVectorOperations::clear (dest, numSamples);

        return;
    }

    // Some interfaces ask for blocks larger than the current buffer size so in
    // these cases we need to render the buffer in chunks
    if (numSamples <= maxBlockSize)
    {
        audioDeviceIOCallbackInternal (inputChannelData, numInputChannels,
                                       outputChannelData, totalNumOutputChannels,
                                       numSamples);
        return;
    }

    for (int sampleStartIndex = 0;;)
    {
        const auto numThisTime = std::min (numSamples, maxBlockSize);

        for (int i = 0; i < numInputChannels; ++i)
            inputChannelsScratch[i] = inputChannelData[i] + sampleStartIndex;

        for (int i = 0; i < totalNumOutputChannels; ++i)
            outputChannelsScratch[i] = outputChannelData[i] + sampleStartIndex;

        audioDeviceIOCallbackInternal (inputChannelsScratch, numInputChannels,
                                       outputChannelsScratch, totalNumOutputChannels,
                                       numThisTime);

        numSamples -= numThisTime;
        sampleStartIndex += numThisTime;

        if (numSamples == 0)
            break;
    }
}

void DeviceManager::audioDeviceIOCallbackInternal (const float* const* inputChannelData, int numInputChannels,
                                                   float* const* outputChannelData, int totalNumOutputChannels,
                                                   int numSamples)
{
    {
        engine.getAudioFileManager().cache.nextBlockStarted();

        if (clearStatsFlag.exchange (false))
            performanceMeasurement.getStatisticsAndReset();

        const ScopedPerformanceMeasurement spm (performanceMeasurement);

        if (currentCpuUsage > cpuLimitBeforeMuting)
        {
            for (int i = 0; i < totalNumOutputChannels; ++i)
                if (auto dest = outputChannelData[i])
                    juce::FloatVectorOperations::clear (dest, numSamples);

            currentCpuUsage = std::min (0.9, currentCpuUsage * 0.99);
        }
        else
        {
            broadcastStreamTimeToMidiDevices (streamTime + outputLatencyTime);
            juce::Range<double> blockStreamTime;

            {
                SCOPED_REALTIME_CHECK
                const std::shared_lock sl (contextLock);

                for (auto wi : waveInputs)
                    wi->consumeNextAudioBlock (inputChannelData, numInputChannels, numSamples, streamTime);

                for (int i = totalNumOutputChannels; --i >= 0;)
                    if (auto dest = outputChannelData[i])
                        juce::FloatVectorOperations::clear (dest, numSamples);

                double blockLength = numSamples / currentSampleRate;

                blockStreamTime = { streamTime, streamTime + blockLength };

                for (auto c : activeContexts)
                    c->fillNextNodeBlock (outputChannelData, totalNumOutputChannels, numSamples);
            }

            for (int i = totalNumOutputChannels; --i >= 0;)
                if (auto* dest = outputChannelData[i])
                    if (activeOutChannels[i])
                        for (int j = 0; j < numSamples; ++j)
                            if (! std::isnormal (dest[j]))
                                dest[j] = 0;

            streamTime = blockStreamTime.getEnd();
            currentCpuUsage = deviceManager.getCpuUsage();
        }

        if (globalOutputAudioProcessor != nullptr)
        {
            juce::AudioBuffer<float> ab (outputChannelData, totalNumOutputChannels, numSamples);
            juce::MidiBuffer mb;
            globalOutputAudioProcessor->processBlock (ab, mb);
        }

        // Output clipping
        if (outputClippingEnabled.load (std::memory_order_relaxed))
        {
            bool hasClipped = false;

            for (int i = totalNumOutputChannels; --i >= 0;)
            {
                if (auto dest = outputChannelData[i])
                {
                    if (activeOutChannels[i])
                    {
                        for (int j = 0; j < numSamples; ++j)
                        {
                            auto samp = dest[j];

                            if (samp < -1.0f)
                            {
                                samp = -1.0f;
                                hasClipped = true;
                            }
                            else if (samp > 1.0f)
                            {
                                samp = 1.0f;
                                hasClipped = true;
                            }
                            else
                            {
                                continue;
                            }

                            dest[j] = samp;
                        }
                    }
                }
            }

            if (hasClipped)
                outputHasClipped.store (true, std::memory_order_relaxed);
        }
    }

    performanceStats.store (performanceMeasurement.getStatistics());
}

void DeviceManager::audioDeviceAboutToStart (juce::AudioIODevice* device)
{
    streamTime = 0;
    currentCpuUsage = 0.0f;

    if (globalOutputAudioProcessor != nullptr)
        globalOutputAudioProcessor->prepareToPlay (device->getCurrentSampleRate(), device->getCurrentBufferSizeSamples());

   #if JUCE_ANDROID
    steadyLoadContext.setSampleRate (device->getCurrentSampleRate());
   #endif

    // A lot of the prep has to happen on the message thread, so we'll suspend
    // the callbacks until prepareToStart() has been called
    isSuspended = true;
    prepareToStartCaller->triggerAsyncUpdate();
}

void DeviceManager::prepareToStart()
{
    if (auto device = deviceManager.getCurrentAudioDevice())
    {
        maxBlockSize = device->getCurrentBufferSizeSamples();
        currentSampleRate = device->getCurrentSampleRate();
        jassert (currentSampleRate > 0.0);
        currentLatencyMs  = maxBlockSize * 1000.0f / currentSampleRate;
        outputLatencyTime = device->getOutputLatencyInSamples() / currentSampleRate;
        defaultWaveOutID = engine.getPropertyStorage().getPropertyItem (SettingID::defaultWaveOutDevice, device->getTypeName());
        defaultWaveInID  = engine.getPropertyStorage().getPropertyItem (SettingID::defaultWaveInDevice, device->getTypeName());

        inputChannelsScratch.realloc (device->getInputChannelNames().size());
        outputChannelsScratch.realloc (device->getOutputChannelNames().size());

        {
            const std::shared_lock sl (contextLock);

            for (auto c : activeContexts)
            {
                const EditPlaybackContext::ScopedDeviceListReleaser rebuilder (*c, true);
                c->resyncToGlobalStreamTime ({ streamTime, streamTime + device->getCurrentBufferSizeSamples() / currentSampleRate }, currentSampleRate);
                c->edit.restartPlayback();
            }
        }

        isSuspended = false;
    }
}

void DeviceManager::audioDeviceStopped()
{
    isSuspended = true;
    currentCpuUsage = 0.0f;

    if (globalOutputAudioProcessor != nullptr)
        globalOutputAudioProcessor->releaseResources();
}

void DeviceManager::updateNumCPUs()
{
    const juce::ScopedLock sl (deviceManager.getAudioCallbackLock());
    const std::shared_lock cl (contextLock);

    for (auto c : activeContexts)
        c->updateNumCPUs();
}

//==============================================================================
void DeviceManager::enableOutputClipping (bool clipOutput)
{
    outputClippingEnabled.store (clipOutput, std::memory_order_relaxed);
}

bool DeviceManager::hasOutputClipped (bool reset)
{
    const auto hasClipped = outputHasClipped.load (std::memory_order_relaxed);

    if (hasClipped && reset)
        outputHasClipped.store (false, std::memory_order_relaxed);

    return hasClipped;
}

void DeviceManager::addContext (EditPlaybackContext* c)
{
    TRACKTION_ASSERT_MESSAGE_THREAD

    double lastStreamTime;

    {
        const std::unique_lock sl (contextLock);
        lastStreamTime = streamTime;
        c->resyncToGlobalStreamTime ({ lastStreamTime, lastStreamTime + getBlockSize() / currentSampleRate }, currentSampleRate);
        activeContexts.addIfNotAlreadyThere (c);
    }

    for (int i = 200; --i >= 0;)
    {
        juce::Thread::sleep (1);
        if (lastStreamTime != streamTime)
            break;
    }
}

void DeviceManager::removeContext (EditPlaybackContext* c)
{
    const std::unique_lock sl (contextLock);
    activeContexts.removeAllInstancesOf (c);
}

void DeviceManager::clearAllContextDevices()
{
    const std::shared_lock sl (contextLock);

    for (auto c : activeContexts)
        const EditPlaybackContext::ScopedDeviceListReleaser rebuilder (*c, false);
}

void DeviceManager::reloadAllContextDevices()
{
    const std::shared_lock sl (contextLock);

    for (auto c : activeContexts)
    {
        const EditPlaybackContext::ScopedDeviceListReleaser rebuilder (*c, true);
        c->edit.restartPlayback();
    }
}

void DeviceManager::setGlobalOutputAudioProcessor (std::unique_ptr<juce::AudioProcessor> newProcessor)
{
    if (newProcessor != nullptr)
        newProcessor->prepareToPlay (getSampleRate(), getBlockSize());

    {
        const juce::ScopedLock sl (deviceManager.getAudioCallbackLock());
        std::swap (globalOutputAudioProcessor, newProcessor);
    }

    newProcessor.reset();
}

}} // namespace tracktion { inline namespace engine
