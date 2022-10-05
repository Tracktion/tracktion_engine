/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2019
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion { inline namespace engine
{

//==============================================================================
class HostedAudioDevice : public juce::AudioIODevice
{
public:
    HostedAudioDevice (HostedAudioDeviceInterface& aif, std::function<void (HostedAudioDevice*)> onDestroy_)
        : AudioIODevice ("Hosted Device", "Hosted Device"), audioIf (aif), onDestroy (onDestroy_)
    {}

    ~HostedAudioDevice() override
    {
        if (onDestroy)
            onDestroy (this);
    }

    juce::StringArray getOutputChannelNames() override      { return audioIf.getOutputChannelNames();   }
    juce::StringArray getInputChannelNames() override       { return audioIf.getInputChannelNames();    }
    juce::Array<double> getAvailableSampleRates() override  { return { audioIf.parameters.sampleRate }; }
    juce::Array<int> getAvailableBufferSizes() override     { return { audioIf.parameters.blockSize };  }
    int getDefaultBufferSize() override                     { return audioIf.parameters.blockSize;      }

    juce::String open (const juce::BigInteger& inputChannels,
                       const juce::BigInteger& outputChannels,
                       double sampleRate, int bufferSizeSamples) override
    {
        ignoreUnused (inputChannels, outputChannels, sampleRate, bufferSizeSamples);
        return {};
    }

    void close() override                                   {}
    void start (juce::AudioIODeviceCallback* callback_) override
    {
        callback = callback_;
        callback->audioDeviceAboutToStart (this);
    }

    void stop() override
    {
        callback->audioDeviceStopped();
        callback = nullptr;
    }

    bool isOpen() override                                  { return true;  }
    bool isPlaying() override                               { return true;  }
    juce::String getLastError() override                    { return {};    }
    int getCurrentBitDepth() override                       { return 16;    }
    int getOutputLatencyInSamples() override                { return 0;     }
    int getInputLatencyInSamples() override                 { return 0;     }
    bool hasControlPanel() const override                   { return false; }
    bool showControlPanel() override                        { return false; }
    bool setAudioPreprocessingEnabled (bool) override       { return false; }
    int getCurrentBufferSizeSamples() override              { return audioIf.parameters.blockSize;      }
    double getCurrentSampleRate() override                  { return audioIf.parameters.sampleRate;     }

    juce::BigInteger getActiveOutputChannels() const override
    {
        juce::BigInteger res;
        res.setRange (0, audioIf.parameters.outputChannels, true);
        return res;
    }

    juce::BigInteger getActiveInputChannels() const override
    {
        juce::BigInteger res;
        res.setRange (0, audioIf.parameters.inputChannels, true);
        return res;
    }

    void processBlock (juce::AudioBuffer<float>& buffer)
    {
        if (callback != nullptr)
            callback->audioDeviceIOCallbackWithContext (buffer.getArrayOfReadPointers(),
                                                        std::min (buffer.getNumChannels(), audioIf.parameters.inputChannels),
                                                        buffer.getArrayOfWritePointers(),
                                                        std::min (buffer.getNumChannels(), audioIf.parameters.outputChannels),
                                                        buffer.getNumSamples(),
                                                        {});
    }

    void settingsChanged()
    {
        if (callback != nullptr)
            callback->audioDeviceAboutToStart (this);
    }

private:
    HostedAudioDeviceInterface& audioIf;
    std::function<void (HostedAudioDevice*)> onDestroy;
    juce::AudioIODeviceCallback* callback = nullptr;
};

//==============================================================================
class HostedAudioDeviceType  : public juce::AudioIODeviceType
{
public:
    HostedAudioDeviceType (HostedAudioDeviceInterface& aif)
        : AudioIODeviceType ("Hosted Device"), audioIf (aif)
    {}

    ~HostedAudioDeviceType() override
    {
        if (audioIf.deviceType == this)
            audioIf.deviceType = nullptr;
    }

    void scanForDevices() override                                          {}
    juce::StringArray getDeviceNames (bool = false) const override          { return { "Hosted Device" }; }
    int getDefaultDeviceIndex (bool) const override                         { return 0; }
    int getIndexOfDevice (juce::AudioIODevice*, bool) const override        { return 0; }
    bool hasSeparateInputsAndOutputs() const override                       { return false; }

    juce::AudioIODevice* createDevice (const juce::String&, const juce::String&) override
    {
        auto device = new HostedAudioDevice (audioIf, [ptr = juce::WeakReference<HostedAudioDeviceType> (this)] (HostedAudioDevice* d)
                                                      {
                                                          if (ptr != nullptr)
                                                              ptr->devices.removeFirstMatchingValue (d);
                                                      });
        devices.add (device);
        return device;
    }

    void processBlock (juce::AudioBuffer<float>& buffer)
    {
        for (auto device : devices)
            device->processBlock (buffer);
    }

    void settingsChanged()
    {
        for (auto device : devices)
            device->settingsChanged();
    }

private:
    HostedAudioDeviceInterface& audioIf;
    juce::Array<HostedAudioDevice*> devices;

    JUCE_DECLARE_WEAK_REFERENCEABLE (HostedAudioDeviceType)
};

//==============================================================================
class HostedMidiInputDevice : public MidiInputDevice
{
public:
    HostedMidiInputDevice (HostedAudioDeviceInterface& aif)
        : MidiInputDevice (aif.engine, TRANS("MIDI Input"), TRANS("MIDI Input")), audioIf (aif)
    {
    }

    ~HostedMidiInputDevice() override
    {
        audioIf.midiInputs.removeFirstMatchingValue (this);
    }

    DeviceType getDeviceType() const override
    {
        return virtualMidiDevice;
    }

    InputDeviceInstance* createInstance (EditPlaybackContext& epc) override
    {
        return new HostedMidiInputDeviceInstance (*this, epc);
    }

    void loadProps() override
    {
        auto n = engine.getPropertyStorage().getXmlPropertyItem (SettingID::midiin, getName());
        MidiInputDevice::loadProps (n.get());
    }

    void saveProps() override
    {
        juce::XmlElement n ("SETTINGS");

        MidiInputDevice::saveProps (n);

        engine.getPropertyStorage().setXmlPropertyItem (SettingID::midiin, getName(), n);
    }

    void processBlock (juce::MidiBuffer& midi)
    {
        // Process the messages via the base class to update the keyboard state
        for (auto mm : midi)
            MidiInputDevice::handleIncomingMidiMessage (nullptr, mm.getMessage());

        // Pending messages will then be filled with the messages to process
        const juce::ScopedLock sl (pendingMidiMessagesMutex);

        for (auto instance : instances)
            if (auto hostedInstance = dynamic_cast<HostedMidiInputDeviceInstance*> (instance))
                hostedInstance->processBlock (pendingMidiMessages);

        pendingMidiMessages.clear();
    }

    using MidiInputDevice::handleIncomingMidiMessage;
    void handleIncomingMidiMessage (const juce::MidiMessage& m) override
    {
        const juce::ScopedLock sl (pendingMidiMessagesMutex);
        pendingMidiMessages.addEvent (m, 0);
    }

    juce::String openDevice() override { return {}; }
    void closeDevice() override {}

private:
    //==============================================================================
    class HostedMidiInputDeviceInstance : public MidiInputDeviceInstanceBase
    {
    public:
        HostedMidiInputDeviceInstance (HostedMidiInputDevice& owner_, EditPlaybackContext& epc)
            : MidiInputDeviceInstanceBase (owner_, epc)
        {
        }

        bool startRecording() override
        {
            // We need to keep a list of tracks the are being recorded to
            // here, since user may un-arm track to stop recording
            activeTracks.clear();

            for (auto destTrack : getTargetTracks())
                if (isRecordingActive (*destTrack))
                    activeTracks.add (destTrack);

            if (! recording)
            {
                getHostedMidiInputDevice().masterTimeUpdate (startTime.inSeconds());
                recording = true;
            }

            return recording;
        }

        void processBlock (juce::MidiBuffer& midi)
        {
            const auto globalStreamTime = edit.engine.getDeviceManager().getCurrentStreamTime();

            // N.B. This assumes that the number of samples processed per block is constant.
            // I.e. that there is no speed compensation set (which shouldn't be the case when
            // running as a plugin)
            for (auto mmm : midi)
            {
                const auto blockStreamTime = tracktion::graph::sampleToTime (mmm.samplePosition, sampleRate);

                auto msg = mmm.getMessage();
                msg.setTimeStamp (globalStreamTime + blockStreamTime);
                handleIncomingMidiMessage (std::move (msg));
            }
        }
        
    private:
        const double sampleRate = context.getSampleRate();

        HostedMidiInputDevice& getHostedMidiInputDevice() const   { return static_cast<HostedMidiInputDevice&> (owner); }
    };
    
    //==============================================================================
    HostedAudioDeviceInterface& audioIf;
    juce::MidiBuffer pendingMidiMessages;
    juce::CriticalSection pendingMidiMessagesMutex;
};

//==============================================================================
class HostedMidiOutputDevice : public MidiOutputDevice
{
public:
    HostedMidiOutputDevice (HostedAudioDeviceInterface& aif)
        : MidiOutputDevice (aif.engine, TRANS("MIDI Output"), -1), audioIf (aif)
    {
    }

    ~HostedMidiOutputDevice() override
    {
        audioIf.midiOutputs.removeFirstMatchingValue (this);
    }

    MidiOutputDeviceInstance* createInstance (EditPlaybackContext& epc) override
    {
        return new HostedMidiOutputDeviceInstance (*this, epc);
    }

    void processBlock (juce::MidiBuffer& midi)
    {
        for (auto m : toSend)
        {
            auto t = m.getTimeStamp() * audioIf.parameters.sampleRate;
            midi.addEvent (m, int (t));
        }
        
        toSend.clear();
    }

    void sendMessageNow (const juce::MidiMessage& message) override
    {
        toSend.addMidiMessage (message, 0, MidiMessageArray::notMPE);
        toSend.sortByTimestamp();
    }

private:
    class HostedMidiOutputDeviceInstance : public MidiOutputDeviceInstance
    {
    public:
        HostedMidiOutputDeviceInstance (HostedMidiOutputDevice& o, EditPlaybackContext& epc)
            : MidiOutputDeviceInstance (o, epc), outputDevice (o)
        {
        }

        bool sendMessages (MidiMessageArray& mma, TimePosition editTime) override
        {
            // Adjust these messages to be relative to time 0.0 which will be the next call to processBlock
            // The device delay is also subtracted as this will have been added when rendering
            const auto deltaTime = -editTime - outputDevice.getDeviceDelay();
            outputDevice.toSend.mergeFromAndClearWithOffset (mma, deltaTime.inSeconds());
            return true;
        }

        HostedMidiOutputDevice& outputDevice;
    };

    HostedAudioDeviceInterface& audioIf;

    MidiMessageArray toSend;
};
//==============================================================================
HostedAudioDeviceInterface::HostedAudioDeviceInterface (Engine& e)
    : engine (e)
{
}

void HostedAudioDeviceInterface::initialise (const Parameters& p)
{
    parameters = p;

    auto& dm = engine.getDeviceManager();

    // First check for an existing hosted device
    if (deviceType == nullptr)
        for (auto device : dm.deviceManager.getAvailableDeviceTypes())
            if (auto hostedAudioDeviceType = dynamic_cast<HostedAudioDeviceType*> (device))
                deviceType = hostedAudioDeviceType;

    // Otherwise add a new hosted device type
    if (deviceType == nullptr)
    {
        deviceType = new HostedAudioDeviceType (*this);
        dm.deviceManager.addAudioDeviceType (std::unique_ptr<HostedAudioDeviceType> (deviceType));
    }

    dm.deviceManager.setCurrentAudioDeviceType ("Hosted Device", true);
    dm.initialise (parameters.inputChannels, parameters.outputChannels);
    jassert (dm.deviceManager.getCurrentAudioDeviceType() == "Hosted Device");
    jassert (dm.deviceManager.getCurrentDeviceTypeObject() == deviceType);

    for (int i = 0; i < dm.getNumWaveOutDevices(); i++)
        if (auto wo = dm.getWaveOutDevice (i))
            wo->setEnabled (true);

    for (int i = 0; i < dm.getNumWaveInDevices(); i++)
        if (auto wi = dm.getWaveInDevice (i))
            wi->setStereoPair (false);

    for (int i = 0; i < dm.getNumWaveInDevices(); i++)
    {
        if (auto wi = dm.getWaveInDevice (i))
        {
            wi->setEndToEnd (true);
            wi->setEnabled (true);
        }
    }
}

void HostedAudioDeviceInterface::prepareToPlay (double sampleRate, int blockSize)
{
    auto newMaxChannels = std::max (parameters.inputChannels,
                                    parameters.outputChannels);

    if (parameters.sampleRate != sampleRate ||
        parameters.blockSize != blockSize   ||
        maxChannels != newMaxChannels)
    {
        maxChannels = newMaxChannels;
        parameters.sampleRate = sampleRate;
        parameters.blockSize  = blockSize;

        if (! parameters.fixedBlockSize)
        {
            inputFifo.setSize (maxChannels, blockSize * 4);
            outputFifo.setSize (maxChannels, blockSize * 4);

            outputFifo.writeSilence (blockSize);
        }

        if (deviceType != nullptr)
            deviceType->settingsChanged();
    }
}

void HostedAudioDeviceInterface::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    if (parameters.fixedBlockSize)
    {
        jassert (buffer.getNumSamples() == parameters.blockSize);

        for (auto input : midiInputs)
            if (auto hostedInput = dynamic_cast<HostedMidiInputDevice*> (input))
                hostedInput->processBlock (midi);

        midi.clear();

        if (deviceType != nullptr)
            deviceType->processBlock (buffer);

        for (auto output : midiOutputs)
            if (auto hostedOutput = dynamic_cast<HostedMidiOutputDevice*> (output))
                hostedOutput->processBlock (midi);
    }
    else
    {
        inputFifo.writeAudioAndMidi (buffer, midi);
        midi.clear();

        while (inputFifo.getNumSamplesAvailable() >= parameters.blockSize)
        {
            juce::MidiBuffer scratchMidi;
            AudioScratchBuffer scratch (buffer.getNumChannels(), parameters.blockSize);

            inputFifo.readAudioAndMidi (scratch.buffer, scratchMidi);

            for (auto input : midiInputs)
                if (auto hostedInput = dynamic_cast<HostedMidiInputDevice*> (input))
                    hostedInput->processBlock (scratchMidi);

            if (deviceType != nullptr)
                deviceType->processBlock (scratch.buffer);

            scratchMidi.clear();

            for (auto output : midiOutputs)
                if (auto hostedOutput = dynamic_cast<HostedMidiOutputDevice*> (output))
                    hostedOutput->processBlock (scratchMidi);

            outputFifo.writeAudioAndMidi (scratch.buffer, scratchMidi);
        }

        outputFifo.readAudioAndMidi (buffer, midi);
    }
}

bool HostedAudioDeviceInterface::isHostedMidiInputDevice (const MidiInputDevice& d)
{
    return dynamic_cast<const HostedMidiInputDevice*> (&d) != nullptr;
}

juce::StringArray HostedAudioDeviceInterface::getInputChannelNames()
{
    juce::StringArray res;

    for (int i = 0; i < parameters.inputChannels; i++)
    {
        if (i < parameters.inputNames.size())
            res.add (parameters.inputNames[i]);
        else
            res.add (juce::String (i + 1));
    }

    return res;
}

juce::StringArray HostedAudioDeviceInterface::getOutputChannelNames()
{
    juce::StringArray res;

    for (int i = 0; i < parameters.outputChannels; i++)
    {
        if (i < parameters.outputNames.size())
            res.add (parameters.outputNames[i]);
        else
            res.add (juce::String (i + 1));
    }

    return res;
}

MidiOutputDevice* HostedAudioDeviceInterface::createMidiOutput()
{
    auto device = new HostedMidiOutputDevice (*this);
    midiOutputs.add (device);
    return device;
}

MidiInputDevice* HostedAudioDeviceInterface::createMidiInput()
{
    auto device = new HostedMidiInputDevice (*this);
    midiInputs.add (device);
    return device;
}

}} // namespace tracktion { inline namespace engine
