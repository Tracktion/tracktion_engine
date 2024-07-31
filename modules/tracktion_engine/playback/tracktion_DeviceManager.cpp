/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
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

    loadSettings();
    finishedInitialising = true;
    rescanMidiDeviceList();
    rescanWaveDeviceList();
    updateNumCPUs();

    deviceManager.addAudioCallback (this);

    midiRescanIntervalSeconds = engine.getPropertyStorage().getProperty (SettingID::midiScanIntervalSeconds, 4);
    restartMidiCheckTimer();
}

void DeviceManager::closeDevices()
{
    CRASH_TRACER
    TRACKTION_ASSERT_MESSAGE_THREAD

    lastMIDIDeviceList.reset();
    lastAvailableWaveDeviceList.reset();

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

void DeviceManager::setMidiDeviceScanIntervalSeconds (int intervalSeconds)
{
    midiRescanIntervalSeconds = intervalSeconds;
    engine.getPropertyStorage().setProperty (SettingID::midiScanIntervalSeconds, intervalSeconds);
    restartMidiCheckTimer();
}

void DeviceManager::restartMidiCheckTimer()
{
    if (usesHardwareMidiDevices() && midiRescanIntervalSeconds != 0)
        startTimer (midiRescanIntervalSeconds * 1000);
    else
        stopTimer();
}

void DeviceManager::rescanMidiDeviceList()
{
    onlyRescanMidiOnHardwareChange = false;
    startTimer (5);
}

void DeviceManager::timerCallback()
{
    applyNewMidiDeviceList();
}

static constexpr const char* allMidiInsName = "All MIDI Ins";
static constexpr const char* allMidiInsID = "all_midi_in";

static juce::StringArray getVirtualDeviceIDs (Engine& engine)
{
    juce::StringArray virtualDeviceIDs;
    virtualDeviceIDs.addTokens (engine.getPropertyStorage().getProperty (SettingID::virtualmididevices).toString(), ";", {});
    virtualDeviceIDs.removeEmptyStrings();
    virtualDeviceIDs.removeString (allMidiInsName);
    virtualDeviceIDs.removeString (allMidiInsID);
    virtualDeviceIDs.insert (0, allMidiInsID);
    return virtualDeviceIDs;
}

static void setVirtualDeviceIDs (Engine& engine, const juce::StringArray& list)
{
    engine.getPropertyStorage().setProperty (SettingID::virtualmididevices, list.joinIntoString (";"));
}

struct DeviceManager::MIDIDeviceList
{
    juce::Array<juce::MidiDeviceInfo> midiIns, midiOuts;

    std::shared_ptr<MidiInputDevice> hostedMidiIn;
    std::shared_ptr<MidiOutputDevice> hostedMidiOut;
    std::vector<std::shared_ptr<MidiOutputDevice>> physicalMidiOuts;
    std::vector<std::shared_ptr<PhysicalMidiInputDevice>> physicalMidiIns;
    std::vector<std::shared_ptr<VirtualMidiInputDevice>> virtualMidiIns;
    std::vector<bool> physicalMidiOutsEnabled, physicalMidiInsEnabled, virtualMidiInsEnabled;

    MIDIDeviceList() = default;

    MIDIDeviceList (Engine& sourceEngine,
                    HostedAudioDeviceInterface* sourceHostedAudioDeviceInterface,
                    bool useHardwareDevices)
    {
        if (sourceHostedAudioDeviceInterface)
        {
            hostedMidiOut = std::unique_ptr<MidiOutputDevice> (sourceHostedAudioDeviceInterface->createMidiOutput());
            hostedMidiIn = std::unique_ptr<MidiInputDevice> (sourceHostedAudioDeviceInterface->createMidiInput());
        }

        if (useHardwareDevices)
        {
            scanHardwareDevices (sourceEngine);

            for (auto& info : midiOuts)
            {
                auto d = std::make_shared<MidiOutputDevice> (sourceEngine, info);
                physicalMidiOuts.push_back (d);
                physicalMidiOutsEnabled.push_back (d->isEnabled());
            }

            for (auto& info : midiIns)
            {
                auto d = std::make_shared<PhysicalMidiInputDevice> (sourceEngine, info);
                physicalMidiIns.push_back (d);
                physicalMidiInsEnabled.push_back (d->isEnabled());
            }

           #if 0 // JUCE_MAC && JUCE_DEBUG
            // This is causing problems on macOS as creating the device will cause the OS to send an async
            // "devices changed" callback which will create the device again etc. in an infinite loop
            auto d = std::make_unique<SoftwareMidiOutputDevice> (engine, "Tracktion MIDI Device");
            physicalMidiOuts.push_back (d);
            physicalMidiOutsEnabled.push_back (d->isEnabled());
           #endif
        }

        for (auto& v : getVirtualDeviceIDs (sourceEngine))
        {
            bool isAllMidiIn = (v == allMidiInsID);
            auto deviceName = isAllMidiIn ? juce::String (allMidiInsName) : v;
            auto deviceID   = isAllMidiIn ? juce::String (allMidiInsID) : ("vmidiin_" + juce::String::toHexString (v.hashCode()));
            auto d = std::make_shared<VirtualMidiInputDevice> (sourceEngine, deviceName, InputDevice::virtualMidiDevice, deviceID, isAllMidiIn);
            virtualMidiIns.push_back (d);
            virtualMidiInsEnabled.push_back (d->isEnabled());
        }
    }

    void scanHardwareDevices (Engine& e)
    {
        StopwatchTimer total;
        static constexpr double maxAcceptableTimeForAutoScanning = 0.2;

        {
            StopwatchTimer timer;
            midiIns = juce::MidiInput::getAvailableDevices();
            auto scanTime = timer.getSeconds();

            if (scanTime > maxAcceptableTimeForAutoScanning)
            {
                TRACKTION_LOG ("MIDI input scan took " + juce::String (scanTime) + " seconds. Disabling auto-scanning");
                e.getDeviceManager().setMidiDeviceScanIntervalSeconds (0);
            }
        }

        {
            StopwatchTimer timer;
            midiOuts = juce::MidiOutput::getAvailableDevices();
            auto scanTime = timer.getSeconds();

            if (scanTime > maxAcceptableTimeForAutoScanning)
            {
                TRACKTION_LOG ("MIDI output scan took " + juce::String (scanTime) + " seconds. Disabling auto-scanning");
                e.getDeviceManager().setMidiDeviceScanIntervalSeconds (0);
            }
        }

        auto comparator = [] (const juce::MidiDeviceInfo& a, const juce::MidiDeviceInfo& b)
        {
            return a.identifier < b.identifier;
        };

        std::sort (midiIns.begin(), midiIns.end(), comparator);
        std::sort (midiOuts.begin(), midiOuts.end(), comparator);

        static bool hasReported = false;

        if (! hasReported || total.getSeconds() > 0.1)
        {
            hasReported = true;
            TRACKTION_LOG_DEVICE ("MIDI Input devices scanned in: " + total.getDescription());
        }
    }

    bool hasHardwareChanged (const MIDIDeviceList& other) const
    {
        return midiIns != other.midiIns
            || midiOuts != other.midiOuts;
    }

    bool hasSameEnablement (const MIDIDeviceList& other) const
    {
        return physicalMidiOutsEnabled == other.physicalMidiOutsEnabled
            && physicalMidiInsEnabled == other.physicalMidiInsEnabled
            && virtualMidiInsEnabled == other.virtualMidiInsEnabled;
    }

    bool operator== (const MIDIDeviceList& other) const
    {
        return midiIns == other.midiIns
            && midiOuts == other.midiOuts
            && hostedMidiIn == other.hostedMidiIn
            && hostedMidiOut == other.hostedMidiOut
            && physicalMidiOuts == other.physicalMidiOuts
            && physicalMidiIns == other.physicalMidiIns
            && virtualMidiIns == other.virtualMidiIns
            && hasSameEnablement (other);
    }

    bool operator!= (const MIDIDeviceList& other) const
    {
        return ! operator== (other);
    }

    void harvestExistingDevices (MIDIDeviceList& current)
    {
        if (hostedMidiIn && current.hostedMidiIn)
            hostedMidiIn = current.hostedMidiIn;

        if (hostedMidiOut && current.hostedMidiOut)
            hostedMidiOut = current.hostedMidiOut;

        for (auto& d : physicalMidiIns)
            for (auto& old : current.physicalMidiIns)
                if (old->getDeviceID() == d->getDeviceID())
                    d = old;

        for (auto& d : virtualMidiIns)
            for (auto& old : current.virtualMidiIns)
                if (old->getDeviceID() == d->getDeviceID())
                    d = old;

        for (auto& d : physicalMidiOuts)
            for (auto& old : current.physicalMidiOuts)
                if (old->getDeviceID() == d->getDeviceID())
                    d = old;
    }

    std::vector<std::shared_ptr<MidiInputDevice>> getAllIns()
    {
        std::vector<std::shared_ptr<MidiInputDevice>> list;

        if (hostedMidiIn)
            list.push_back (hostedMidiIn);

        for (auto& d : physicalMidiIns)
            list.push_back (d);

        for (auto& d : virtualMidiIns)
            list.push_back (d);

        return list;
    }

    std::vector<std::shared_ptr<MidiOutputDevice>> getAllOuts()
    {
        std::vector<std::shared_ptr<MidiOutputDevice>> list;

        if (hostedMidiOut)
            list.push_back (hostedMidiOut);

        for (auto& d : physicalMidiOuts)
            list.push_back (d);

        return list;
    }
};

void DeviceManager::applyNewMidiDeviceList()
{
    CRASH_TRACER
    TRACKTION_ASSERT_MESSAGE_THREAD

    restartMidiCheckTimer();

    if (lastMIDIDeviceList == nullptr)
        lastMIDIDeviceList = std::make_unique<MIDIDeviceList>();

    auto newList = std::make_unique<MIDIDeviceList> (engine,
                                                     hostedAudioDeviceInterface.get(),
                                                     usesHardwareMidiDevices());

    if (onlyRescanMidiOnHardwareChange
         && ! newList->hasHardwareChanged (*lastMIDIDeviceList))
        return;

    onlyRescanMidiOnHardwareChange = true;

    auto& storage = engine.getPropertyStorage();

    auto newDefaultOut = storage.getProperty (SettingID::defaultMidiOutDevice).toString();
    auto newDefaultIn = storage.getProperty (SettingID::defaultMidiInDevice).toString();

    bool defaultsChanged = (defaultMidiOutID != newDefaultOut
                            || defaultMidiInID != newDefaultIn);

    if (! defaultsChanged && *newList == *lastMIDIDeviceList)
        return;

    defaultMidiOutID = newDefaultOut;
    defaultMidiInID  = newDefaultIn;

    bool enablementChanged = ! newList->hasSameEnablement (*lastMIDIDeviceList);

    newList->harvestExistingDevices (*lastMIDIDeviceList);

    auto newMidiIns  = newList->getAllIns();
    auto newMidiOuts = newList->getAllOuts();

    lastMIDIDeviceList = std::move (newList);

    if (! (defaultsChanged || enablementChanged))
    {
        const std::shared_lock sl (midiInputsMutex);

        if (newMidiIns == midiInputs && newMidiOuts == midiOutputs)
            return;
    }

    TRACKTION_LOG ("Updating MIDI I/O devices");

    for (auto mi : newMidiIns)
        TRACKTION_LOG_DEVICE ("Found MIDI in: " + mi->getDeviceID() + " (\"" + mi->getName() + "\")" + (mi->isEnabled() ? " (enabled)" : ""));

    for (auto mo : newMidiOuts)
        TRACKTION_LOG_DEVICE ("Found MIDI out: " + mo->getDeviceID() + " (\"" + mo->getName() + "\")" + (mo->isEnabled() ? " (enabled)" : ""));

    for (auto& d : newMidiOuts)
    {
        if (d->isEnabled())
        {
            auto error = d->openDevice();

            if (! error.isEmpty())
            {
                d->setEnabled (false);
                engine.getUIBehaviour().showWarningMessage (error);
            }
        }
    }

    for (auto& d : newMidiIns)
    {
        if (d->isEnabled())
        {
            auto error = d->openDevice();

            if (! error.isEmpty())
            {
                d->setEnabled (false);
                engine.getUIBehaviour().showWarningMessage (error);
            }
        }
    }

    clearAllContextDevices();

    {
        const std::unique_lock sl (midiInputsMutex);

        std::swap (newMidiIns, midiInputs);
        std::swap (newMidiOuts, midiOutputs);
    }

    reloadAllContextDevices();

    int enabledMidiOuts = 0;

    for (auto mo : midiOutputs)
    {
        if (mo->isEnabled())
            ++enabledMidiOuts;
        else
            mo->closeDevice();
    }

    {
        const std::shared_lock sl (midiInputsMutex);

        for (auto mi : midiInputs)
            if (! mi->isEnabled())
                mi->closeDevice();
    }

    const bool hasEnabledMidiDefaultDevs = storage.getProperty (SettingID::hasEnabledMidiDefaultDevs, false);
    storage.setProperty (SettingID::hasEnabledMidiDefaultDevs, true);
    storage.flushSettingsToDisk();

    checkDefaultDevicesAreValid();

    if (enabledMidiOuts == 0 && ! hasEnabledMidiDefaultDevs)
    {
        for (auto& d : midiOutputs)
        {
            if (! isMicrosoftGSSynth (*d))
            {
                d->setEnabled (true);
                break;
            }
        }
    }

    sendChangeMessage();

    engine.getExternalControllerManager().midiInOutDevicesChanged();
}

void DeviceManager::injectMIDIMessageToDefaultDevice (const juce::MidiMessage& m)
{
    if (auto input = getDefaultMidiInDevice())
        input->handleIncomingMidiMessage (nullptr, m);
}

void DeviceManager::broadcastMessageToAllVirtualDevices (MidiInputDevice& source, const juce::MidiMessage& m)
{
    const std::shared_lock sl (contextLock);

    for (auto d : midiInputs)
        if (auto vmd = dynamic_cast<VirtualMidiInputDevice*> (d.get()))
            vmd->handleMessageFromPhysicalDevice (source, m);
}

bool DeviceManager::usesHardwareMidiDevices()
{
    return hostedAudioDeviceInterface == nullptr
            || hostedAudioDeviceInterface->parameters.useMidiDevices;
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

void DeviceManager::dispatchPendingUpdates()
{
    handleUpdateNowIfNeeded();
    prepareToStartCaller->handleUpdateNowIfNeeded();
}

juce::Result DeviceManager::createVirtualMidiDevice (const juce::String& name)
{
    CRASH_TRACER
    TRACKTION_ASSERT_MESSAGE_THREAD

    auto virtualDeviceIDs = getVirtualDeviceIDs (engine);

    if (virtualDeviceIDs.contains (name))
        return juce::Result::fail (TRANS("Name is already in use!"));

    virtualDeviceIDs.add (name);
    setVirtualDeviceIDs (engine, virtualDeviceIDs);

    rescanMidiDeviceList();

    return juce::Result::ok();
}

void DeviceManager::deleteVirtualMidiDevice (VirtualMidiInputDevice& vmi)
{
    CRASH_TRACER
    TRACKTION_ASSERT_MESSAGE_THREAD

    auto deviceID = vmi.getDeviceID();

    engine.getPropertyStorage().removePropertyItem (SettingID::virtualmidiin, deviceID);

    auto virtualDeviceIDs = getVirtualDeviceIDs (engine);
    virtualDeviceIDs.removeString (deviceID);
    setVirtualDeviceIDs (engine, virtualDeviceIDs);

    rescanMidiDeviceList();
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

    sanityCheckEnabledChannels();
    reloadAllContextDevices();
    saveSettings();
    checkDefaultDevicesAreValid();
    sendChangeMessage();

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
    defaultWaveOutID = storage.getPropertyItem (SettingID::defaultWaveOutDevice, currentDeviceType, defaultWaveOutID);
    defaultWaveInID  = storage.getPropertyItem (SettingID::defaultWaveInDevice, currentDeviceType, defaultWaveInID);

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
        if (auto allMidi = findInputDeviceForID (allMidiInsID);
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
std::vector<WaveOutputDevice*> DeviceManager::getWaveOutputDevices()
{
    dispatchPendingUpdates();
    return { waveOutputs.begin(), waveOutputs.end() };
}

std::vector<WaveInputDevice*> DeviceManager::getWaveInputDevices()
{
    dispatchPendingUpdates();
    return { waveInputs.begin(), waveInputs.end() };
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
    return (int) midiInputs.size();
}

std::shared_ptr<MidiInputDevice> DeviceManager::getMidiInDevice (int index) const
{
    const std::shared_lock sl (midiInputsMutex);

    if (index >= 0 && index < (int) midiInputs.size())
        return midiInputs[(size_t) index];

    return {};
}

std::vector<std::shared_ptr<MidiInputDevice>> DeviceManager::getMidiInDevices() const
{
    const std::shared_lock sl (midiInputsMutex);
    return midiInputs;
}

std::shared_ptr<MidiInputDevice> DeviceManager::findMidiInputDeviceForID (const juce::String& deviceID) const
{
    const std::shared_lock sl (midiInputsMutex);

    for (auto& m : midiInputs)
        if (m->getDeviceID() == deviceID)
            return m;

    return {};
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
        return getMidiInDevice (index - getNumWaveInDevices()).get();

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
            return d.get();

    return {};
}

InputDevice* DeviceManager::findInputDeviceWithName (const juce::String& name) const
{
    for (auto d : waveInputs)
        if (d->getName() == name)
            return d;

    const std::shared_lock sl (midiInputsMutex);

    for (auto d : midiInputs)
        if (d->getName() == name)
            return d.get();

    return {};
}

OutputDevice* DeviceManager::findOutputDeviceForID (const juce::String& id) const
{
    for (auto d : waveOutputs)
        if (d->getDeviceID() == id)
            return d;

    for (auto d : midiOutputs)
        if (d->getDeviceID() == id)
            return d.get();

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
            return d.get();

    return {};
}

int DeviceManager::getRecordAdjustmentSamples()
{
     if (auto d = deviceManager.getCurrentAudioDevice())
         return d->getInputLatencyInSamples() + d->getOutputLatencyInSamples();

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

                for (auto c : activeContexts)
                    c->nextBlockStarted();

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
        defaultWaveOutID = engine.getPropertyStorage().getPropertyItem (SettingID::defaultWaveOutDevice, device->getTypeName(), defaultWaveOutID);
        defaultWaveInID  = engine.getPropertyStorage().getPropertyItem (SettingID::defaultWaveInDevice, device->getTypeName(), defaultWaveInID);

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
