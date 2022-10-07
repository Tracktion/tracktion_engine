/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
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

static juce::StringArray getMidiDeviceNames (juce::Array<juce::MidiDeviceInfo> devices)
{
    juce::StringArray deviceNames;

    for (auto& d : devices)
        deviceNames.add (d.name);

    deviceNames.appendNumbersToDuplicates (true, false);
    
    return deviceNames;
}

//==============================================================================
TracktionEngineAudioDeviceManager::TracktionEngineAudioDeviceManager (Engine& e)
    : engine (e)
{
}

void TracktionEngineAudioDeviceManager::createAudioDeviceTypes (juce::OwnedArray<juce::AudioIODeviceType>& types)
{
    if (engine.getEngineBehaviour().addSystemAudioIODeviceTypes())
        juce::AudioDeviceManager::createAudioDeviceTypes (types);
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
        juce::StringArray channelNames (isInput ? device.getInputChannelNames() : device.getOutputChannelNames());

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
            
            if (canBeStereo && (isInput ? dm.isDeviceInChannelStereo (i) : dm.isDeviceOutChannelStereo (i)))
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

    bool operator== (const WaveDeviceList& other) const noexcept    { return deviceName == other.deviceName && inputs == other.inputs && outputs == other.outputs; }
    bool operator!= (const WaveDeviceList& other) const noexcept    { return ! operator== (other); }

    DeviceManager& dm;
    juce::String deviceName;
    std::vector<WaveDeviceDescription> inputs, outputs;
};


//==============================================================================
struct DeviceManager::ContextDeviceClearer : private juce::AsyncUpdater
{
    ContextDeviceClearer (DeviceManager& owner)
        : deviceManager (owner)
    {}

    ~ContextDeviceClearer() override
    {
        handleUpdateNowIfNeeded();
    }

    void triggerClearDevices()
    {
        if (juce::MessageManager::getInstance()->isThisTheMessageThread())
            clearDevices();
        else
            triggerAsyncUpdate();
    }

    void dispatchPendingUpdates()
    {
        JUCE_ASSERT_MESSAGE_THREAD
        handleUpdateNowIfNeeded();
    }

private:
    DeviceManager& deviceManager;

    void clearDevices()
    {
        deviceManager.clearAllContextDevices();
    }

    void handleAsyncUpdate() override
    {
        clearDevices();
    }
};


//==============================================================================
//==============================================================================
DeviceManager::DeviceManager (Engine& e) : engine (e)
{
    CRASH_TRACER

    contextDeviceClearer = std::make_unique<ContextDeviceClearer> (*this);

    deviceManager.addChangeListener (this);

    gDeviceManager = &deviceManager;
}

DeviceManager::~DeviceManager()
{
    gDeviceManager = nullptr;

    CRASH_TRACER
    deviceManager.removeChangeListener (this);
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

    defaultMidiOutIndex = storage.getProperty (SettingID::defaultMidiOutDevice);
    defaultMidiInIndex = storage.getProperty (SettingID::defaultMidiInDevice);

    bool openHardwareMidi = hostedAudioDeviceInterface == nullptr || hostedAudioDeviceInterface->parameters.useMidiDevices;

    TRACKTION_LOG ("Finding MIDI I/O");
    if (openHardwareMidi)
        lastMidiInNames = getMidiDeviceNames (juce::MidiInput::getAvailableDevices());
    
    if (openHardwareMidi)
        lastMidiOutNames = getMidiDeviceNames (juce::MidiOutput::getAvailableDevices());
    
    int enabledMidiIns = 0, enabledMidiOuts = 0;

    // create all the devices before initialising them..
    for (int i = 0; i < lastMidiOutNames.size(); ++i)  midiOutputs.add (new MidiOutputDevice (engine, lastMidiOutNames[i], i));
    for (int i = 0; i < lastMidiInNames.size(); ++i)   midiInputs.add (new PhysicalMidiInputDevice (engine, lastMidiInNames[i], i));

    if (hostedAudioDeviceInterface != nullptr)
    {
        midiOutputs.add (hostedAudioDeviceInterface->createMidiOutput());
        midiInputs.add (hostedAudioDeviceInterface->createMidiInput());
    }

   #if JUCE_MAC && JUCE_DEBUG
    if (openHardwareMidi)
        midiOutputs.add (new SoftwareMidiOutputDevice (engine, "Tracktion MIDI Device"));
   #endif

    juce::StringArray virtualDevices;
    virtualDevices.addTokens (storage.getProperty (SettingID::virtualmididevices).toString(), ";", {});
    virtualDevices.removeEmptyStrings();

    for (int i = 0; i < virtualDevices.size(); ++i)
        midiInputs.add (new VirtualMidiInputDevice (engine, virtualDevices[i], InputDevice::virtualMidiDevice));

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
        const juce::ScopedLock sl (contextLock);

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

    auto midiIns  = getMidiDeviceNames (juce::MidiInput::getAvailableDevices());
    auto midiOuts = getMidiDeviceNames (juce::MidiOutput::getAvailableDevices());

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

void DeviceManager::resetToDefaults (bool deviceSettings, bool resetInputDevices, bool resetOutputDevices, bool latencySettings, bool mixSettings)
{
    TRACKTION_LOG ("Returning audio settings to defaults");
    deviceManager.removeAudioCallback (this);

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
    deviceManager.addAudioCallback (this);
    TransportControl::restartAllTransports (engine, false);
    SelectionManager::refreshAllPropertyPanels();
}

juce::Result DeviceManager::createVirtualMidiDevice (const juce::String& name)
{
    CRASH_TRACER
    TRACKTION_ASSERT_MESSAGE_THREAD

    {
        juce::StringArray virtualDevices;
        virtualDevices.addTokens (engine.getPropertyStorage().getProperty (SettingID::virtualmididevices).toString(), ";", {});
        virtualDevices.removeEmptyStrings();

        if (virtualDevices.contains (name))
            return juce::Result::fail (TRANS("Name is already in use!"));
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

    return juce::Result::ok();
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

    const juce::ScopedValueSetter<bool> v (reentrant, true);

    TRACKTION_LOG ("Rebuilding Wave Device List...");

    contextDeviceClearer->dispatchPendingUpdates();
    TransportControl::stopAllTransports (engine, false, true);

    ContextDeviceListRebuilder deviceRebuilder (*this);

    deviceManager.removeAudioCallback (this);

    waveInputs.clear();
    waveOutputs.clear();

    sanityCheckEnabledChannels();

    lastWaveDeviceList.reset (new WaveDeviceList (*this));

    for (const auto& d : lastWaveDeviceList->inputs)
    {
        auto wi = new WaveInputDevice (engine, d.name, TRANS("Wave Audio Input"), d.channels, InputDevice::waveDevice);
        wi->enabled = d.enabled;
        waveInputs.add (wi);

        TRACKTION_LOG_DEVICE ("Wave In: " + wi->getName() + (wi->isEnabled() ? " (enabled): " : ": ")
                              + createDescriptionOfChannels (wi->deviceChannels));
    }

    for (auto wi : waveInputs)
        wi->initialiseDefaultAlias();

    for (const auto& d : lastWaveDeviceList->outputs)
    {
        auto wo = new WaveOutputDevice (engine, d.name, d.channels);
        wo->enabled = d.enabled;
        waveOutputs.add (wo);

        TRACKTION_LOG_DEVICE ("Wave Out: " + wo->getName() + (wo->isEnabled() ? " (enabled): " : ": ")
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
    TRACKTION_LOG ("Default Wave Out: " + (wo != nullptr ? wo->getName() : juce::String()));

    auto mo = getDefaultMidiOutDevice();
    TRACKTION_LOG ("Default MIDI Out: " + (mo != nullptr ? mo->getName() : juce::String()));

    auto wi = getDefaultWaveInDevice();
    TRACKTION_LOG ("Default Wave In: " + (wi != nullptr ? wi->getName() : juce::String()));

    auto mi = getDefaultMidiInDevice();
    TRACKTION_LOG ("Default MIDI In: " + (mi != nullptr ? mi->getName() : juce::String()));
   #endif

    sendChangeMessage();

    TransportControl::restartAllTransports (engine, false);

    checkDefaultDevicesAreValid();
    deviceManager.addAudioCallback (this);
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
    defaultWaveOutIndex = storage.getPropertyItem (SettingID::defaultWaveOutDevice, currentDeviceType, 0);
    defaultWaveInIndex = storage.getPropertyItem (SettingID::defaultWaveInDevice, currentDeviceType, 0);

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

    if (getDefaultWaveOutDevice() == nullptr
         || ! getDefaultWaveOutDevice()->isEnabled())
    {
        defaultWaveOutIndex = -1;

        for (int i = 0; i < getNumWaveOutDevices(); ++i)
        {
            if (getWaveOutDevice(i) != nullptr && getWaveOutDevice(i)->isEnabled())
            {
                defaultWaveOutIndex = i;
                break;
            }
        }

        if (defaultWaveOutIndex >= 0)
            storage.setPropertyItem (SettingID::defaultWaveOutDevice, deviceManager.getCurrentAudioDeviceType(), defaultWaveOutIndex);
    }

    if (getDefaultMidiOutDevice() == nullptr
         || ! getDefaultMidiOutDevice()->isEnabled())
    {
        defaultMidiOutIndex = -1;

        for (int i = 0; i < getNumMidiOutDevices(); ++i)
        {
            if (getMidiOutDevice(i) != nullptr && getMidiOutDevice(i)->isEnabled())
            {
                defaultMidiOutIndex = i;
                break;
            }
        }

        if (defaultMidiOutIndex >= 0)
            storage.setProperty (SettingID::defaultMidiOutDevice, defaultMidiOutIndex);
    }

    if (getDefaultWaveInDevice() == nullptr
        || ! getDefaultWaveInDevice()->isEnabled())
    {
        defaultWaveInIndex = -1;

        for (int i = 0; i < getNumWaveInDevices(); ++i)
        {
            if (getWaveInDevice(i) != nullptr && getWaveInDevice(i)->isEnabled())
            {
                defaultWaveInIndex = i;
                break;
            }
        }

        if (defaultWaveInIndex >= 0)
            storage.setPropertyItem (SettingID::defaultWaveInDevice, deviceManager.getCurrentAudioDeviceType(), defaultWaveInIndex);
    }

    if (getDefaultMidiInDevice() == nullptr
        || ! getDefaultMidiInDevice()->isEnabled())
    {
        defaultMidiInIndex = -1;

        for (int i = 0; i < getNumMidiInDevices(); ++i)
        {
            if (getMidiInDevice(i) != nullptr && getMidiInDevice(i)->isEnabled())
            {
                defaultMidiInIndex = i;
                break;
            }
        }

        if (defaultMidiInIndex >= 0)
            storage.setProperty (SettingID::defaultMidiInDevice, defaultMidiInIndex);
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

void DeviceManager::setDefaultWaveOutDevice (int index)
{
    if (auto wod = getWaveOutDevice (index))
    {
        if (wod->isEnabled())
        {
            defaultWaveOutIndex = index;
            engine.getPropertyStorage().setPropertyItem (SettingID::defaultWaveOutDevice,
                                                         deviceManager.getCurrentAudioDeviceType(), index);
        }
    }

    checkDefaultDevicesAreValid();
    rebuildWaveDeviceList();
}

void DeviceManager::setDefaultWaveInDevice (int index)
{
    if (auto wod = getWaveInDevice (index))
    {
        if (wod->isEnabled())
        {
            defaultWaveInIndex = index;
            engine.getPropertyStorage().setPropertyItem (SettingID::defaultWaveInDevice,
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
    if (midiOutputs[index] != nullptr && midiOutputs[index]->isEnabled())
    {
        defaultMidiOutIndex = index;
        engine.getPropertyStorage().setProperty (SettingID::defaultMidiOutDevice, defaultMidiOutIndex);
    }

    checkDefaultDevicesAreValid();
    rebuildWaveDeviceList();
}

void DeviceManager::setDefaultMidiInDevice (int index)
{
    if (midiInputs[index] != nullptr && midiInputs[index]->isEnabled())
    {
        defaultMidiInIndex = index;
        engine.getPropertyStorage().setProperty (SettingID::defaultMidiInDevice, defaultMidiInIndex);
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
    const juce::ScopedLock sl (midiInputs.getLock());

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

void DeviceManager::audioDeviceIOCallbackWithContext (const float* const* inputChannelData, int numInputChannels,
                                                      float* const* outputChannelData, int totalNumOutputChannels,
                                                      int numSamples,
                                                      const juce::AudioIODeviceCallbackContext&)
{
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
    jassert (numSamples <= maxBlockSize);

    CRASH_TRACER
    juce::FloatVectorOperations::disableDenormalisedNumberSupport();

    {
       #if JUCE_ANDROID
        const ScopedSteadyLoad load (steadyLoadContext, numSamples);
       #endif

        const auto startTimeTicks = juce::Time::getHighResolutionTicks();

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
                const juce::ScopedLock sl (contextLock);

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

        {
            const auto timeWindowSec = numSamples / static_cast<float> (currentSampleRate);

            const auto currentCpuUtilisation = float (juce::Time::getHighResolutionTicks() - startTimeTicks)
                                                / juce::Time::getHighResolutionTicksPerSecond() / timeWindowSec;

            cpuAvg += currentCpuUtilisation;
            cpuMin = std::min (cpuMin, currentCpuUtilisation);
            cpuMax = std::max (cpuMax, currentCpuUtilisation);

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

void DeviceManager::audioDeviceAboutToStart (juce::AudioIODevice* device)
{
    juce::FloatVectorOperations::disableDenormalisedNumberSupport();
    contextDeviceClearer->dispatchPendingUpdates();

    streamTime = 0;
    currentCpuUsage = 0.0f;
    maxBlockSize = device->getCurrentBufferSizeSamples();
    currentSampleRate = device->getCurrentSampleRate();
    currentLatencyMs  = maxBlockSize * 1000.0f / currentSampleRate;
    outputLatencyTime = device->getOutputLatencyInSamples() / currentSampleRate;
    defaultWaveOutIndex = engine.getPropertyStorage().getPropertyItem (SettingID::defaultWaveOutDevice, device->getTypeName(), 0);
    defaultWaveInIndex = engine.getPropertyStorage().getPropertyItem (SettingID::defaultWaveInDevice, device->getTypeName(), 0);

    inputChannelsScratch.realloc (device->getInputChannelNames().size());
    outputChannelsScratch.realloc (device->getOutputChannelNames().size());

    if (waveDeviceListNeedsRebuilding())
        rebuildWaveDeviceList();

    reloadAllContextDevices();

    const juce::ScopedLock sl (contextLock);

    for (auto c : activeContexts)
        c->resyncToGlobalStreamTime ({ streamTime, streamTime + device->getCurrentBufferSizeSamples() / currentSampleRate }, currentSampleRate);

    if (globalOutputAudioProcessor != nullptr)
        globalOutputAudioProcessor->prepareToPlay (currentSampleRate, device->getCurrentBufferSizeSamples());

    if (device->getCurrentBufferSizeSamples() > 0)
        cpuAvgCounter = cpuReportingInterval = std::max (1, static_cast<int> (device->getCurrentSampleRate())
                                                             / device->getCurrentBufferSizeSamples());
    else
        cpuAvgCounter = cpuReportingInterval = 1;

    jassert (currentSampleRate > 0.0);
    
   #if JUCE_ANDROID
    steadyLoadContext.setSampleRate (device->getCurrentSampleRate());
   #endif
}

void DeviceManager::audioDeviceStopped()
{
    currentCpuUsage = 0.0f;
    contextDeviceClearer->triggerClearDevices();

    if (globalOutputAudioProcessor != nullptr)
        globalOutputAudioProcessor->releaseResources();
}

void DeviceManager::updateNumCPUs()
{
    const juce::ScopedLock sl (deviceManager.getAudioCallbackLock());
    const juce::ScopedLock cl (contextLock);

    for (auto c : activeContexts)
        c->updateNumCPUs();
}

void DeviceManager::addContext (EditPlaybackContext* c)
{
    TRACKTION_ASSERT_MESSAGE_THREAD

    double lastStreamTime;

    {
        const juce::ScopedLock sl (contextLock);
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
    const juce::ScopedLock sl (contextLock);
    activeContexts.removeAllInstancesOf (c);
}

void DeviceManager::clearAllContextDevices()
{
    const juce::ScopedLock sl (contextLock);

    for (auto c : activeContexts)
        const EditPlaybackContext::ScopedDeviceListReleaser rebuilder (*c, false);
}

void DeviceManager::reloadAllContextDevices()
{
    const juce::ScopedLock sl (contextLock);

    for (auto c : activeContexts)
        const EditPlaybackContext::ScopedDeviceListReleaser rebuilder (*c, true);
}

void DeviceManager::setGlobalOutputAudioProcessor (juce::AudioProcessor* newProcessor)
{
    const juce::ScopedLock sl (deviceManager.getAudioCallbackLock());
    globalOutputAudioProcessor.reset (newProcessor);

    if (globalOutputAudioProcessor != nullptr)
        if (auto* audioIODevice = deviceManager.getCurrentAudioDevice())
            globalOutputAudioProcessor->prepareToPlay (currentSampleRate, audioIODevice->getCurrentBufferSizeSamples());
}

}} // namespace tracktion { inline namespace engine
