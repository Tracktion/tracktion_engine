/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2019
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion_engine
{

//==============================================================================
class HostedAudioDevice : public AudioIODevice
{
public:
    HostedAudioDevice (HostedAudioDeviceInterface& aif, std::function<void (HostedAudioDevice*)> onDestroy_)
        : AudioIODevice ("Hosted Device", "Hosted Device"), audioIf (aif), onDestroy (onDestroy_)
    {}

    ~HostedAudioDevice()
    {
        if (onDestroy)
            onDestroy (this);
    }

    StringArray getOutputChannelNames() override            { return audioIf.getOutputChannelNames();   }
    StringArray getInputChannelNames() override             { return audioIf.getInputChannelNames();    }
    Array<double> getAvailableSampleRates() override        { return { audioIf.parameters.sampleRate }; }
    Array<int> getAvailableBufferSizes() override           { return { audioIf.parameters.blockSize };  }
    int getDefaultBufferSize() override                     { return audioIf.parameters.blockSize;      }

    String open (const BigInteger& inputChannels, const BigInteger& outputChannels, double sampleRate, int bufferSizeSamples) override
    {
        ignoreUnused (inputChannels, outputChannels, sampleRate, bufferSizeSamples);
        return {};
    }

    void close() override                                   {}
    void start (AudioIODeviceCallback* callback_) override
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
    String getLastError() override                          { return {};    }
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
        BigInteger res;
        res.setRange (0, audioIf.parameters.outputChannels, true);
        return res;
    }

    juce::BigInteger getActiveInputChannels() const override
    {
        BigInteger res;
        res.setRange (0, audioIf.parameters.inputChannels, true);
        return res;
    }

    void processBlock (AudioBuffer<float>& buffer)
    {
        if (callback != nullptr)
            callback->audioDeviceIOCallback (buffer.getArrayOfReadPointers(),
                                             jmin (buffer.getNumChannels(), audioIf.parameters.inputChannels),
                                             buffer.getArrayOfWritePointers(),
                                             jmin (buffer.getNumChannels(), audioIf.parameters.inputChannels),
                                             buffer.getNumSamples());
    }

    void settingsChanged()
    {
        if (callback != nullptr)
            callback->audioDeviceAboutToStart (this);
    }

private:
    HostedAudioDeviceInterface& audioIf;
    std::function<void (HostedAudioDevice*)> onDestroy;
    AudioIODeviceCallback* callback = nullptr;
};

//==============================================================================
class HostedAudioDeviceType : public AudioIODeviceType
{
public:
    HostedAudioDeviceType (HostedAudioDeviceInterface& aif)
        : AudioIODeviceType ("Hosted Device"), audioIf (aif)
    {}

    ~HostedAudioDeviceType()
    {
        if (audioIf.deviceType == this)
            audioIf.deviceType = nullptr;
    }

    void scanForDevices() override                                          {                                           }
    StringArray getDeviceNames (bool = false) const override                { return { "Hosted Device" };               }
    int getDefaultDeviceIndex (bool) const override                         { return 0;                                 }
    int getIndexOfDevice (AudioIODevice*, bool) const override              { return 0;                                 }
    bool hasSeparateInputsAndOutputs() const override                       { return false;                             }
    AudioIODevice* createDevice (const String&, const String&) override
    {
        auto device = new HostedAudioDevice (audioIf, [ptr = WeakReference<HostedAudioDeviceType> (this)] (HostedAudioDevice* d)
                                                      {
                                                          if (ptr != nullptr)
                                                              ptr->devices.removeFirstMatchingValue (d);
                                                      });
        devices.add (device);
        return device;
    }

    void processBlock (AudioBuffer<float>& buffer)
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

    ~HostedMidiInputDevice()
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

    void processBlock (MidiBuffer& midi)
    {
        for (auto instance : instances)
            if (auto* hostedInstance = dynamic_cast<HostedMidiInputDeviceInstance*> (instance))
                hostedInstance->processBlock (midi);
    }

    void handleIncomingMidiMessage (const juce::MidiMessage&) override {}
    juce::String openDevice() override { return {}; }
    void closeDevice() override {}

private:
    //==============================================================================
    class HostedMidiInputAudioNode : public AudioNode
    {
    public:
        HostedMidiInputAudioNode (MidiBuffer& midi_) : midi (midi_) {}

        void getAudioNodeProperties (AudioNodeProperties& p) override
        {
            p.hasAudio = false;
            p.hasMidi  = true;
            p.numberOfChannels = 0;
        }

        void prepareAudioNodeToPlay (const PlaybackInitialisationInfo& info) override
        {
            sampleRate = info.sampleRate;
        }

        bool purgeSubNodes (bool keepAudio, bool keepMidi) override
        {
            ignoreUnused (keepAudio);
            return keepMidi;
        }

        void releaseAudioNodeResources() override       {}
        void visitNodes (const VisitorFn& v) override   { v (*this); }
        bool isReadyToRender() override                 { return true; }

        void renderOver (const AudioRenderContext& rc) override
        {
            rc.clearMidiBuffer();
            callRenderAdding (rc);
        }

        void renderAdding (const AudioRenderContext& rc) override
        {
            if (rc.bufferForMidiMessages != nullptr)
            {
                MidiBuffer::Iterator itr (midi);

                MidiMessage msg;
                int pos = 0;
                while (itr.getNextEvent (msg, pos))
                {
                    msg.setTimeStamp (pos / sampleRate + rc.midiBufferOffset);
                    rc.bufferForMidiMessages->addMidiMessage (msg, mpeSource);
                }
            }
        }

    private:
        MidiBuffer& midi;
        double sampleRate = 44100.0;
        MidiMessageArray::MPESourceID mpeSource { MidiMessageArray::createUniqueMPESourceID() };
    };

    //==============================================================================
    class HostedMidiInputDeviceInstance : public MidiInputDeviceInstanceBase
    {
    public:
        HostedMidiInputDeviceInstance (HostedMidiInputDevice& owner_, EditPlaybackContext& epc)
            : MidiInputDeviceInstanceBase (owner_, epc), owner (owner_), context (epc)
        {
            ignoreUnused (owner, context);
        }

        bool startRecording() override              { return false; }
        AudioNode* createLiveInputNode() override   { return new HostedMidiInputAudioNode (midi); }
        void processBlock (MidiBuffer& m)           { midi = m; }

    private:
        HostedMidiInputDevice& owner;
        EditPlaybackContext& context;
        MidiBuffer midi;
    };
    
    //==============================================================================
    HostedAudioDeviceInterface& audioIf;
};

//==============================================================================
class HostedMidiOutputDevice : public MidiOutputDevice
{
public:
    HostedMidiOutputDevice (HostedAudioDeviceInterface& aif)
        : MidiOutputDevice (aif.engine, TRANS("MIDI Output"), -1), audioIf (aif)
    {
    }

    ~HostedMidiOutputDevice()
    {
        audioIf.midiOutputs.removeFirstMatchingValue (this);
    }

    MidiOutputDeviceInstance* createInstance (EditPlaybackContext& epc) override
    {
        return new HostedMidiOutputDeviceInstance (*this, epc);
    }

    void processBlock (MidiBuffer& midi)
    {
        for (auto m : toSend)
        {
            auto t = m.getTimeStamp() * audioIf.parameters.sampleRate;
            midi.addEvent (m, int (t));
        }
        toSend.clear ();
    }

    void sendMessageNow (const MidiMessage& message) override
    {
        toSend.addMidiMessage (message, 0, MidiMessageArray::notMPE);
        toSend.sortByTimestamp();
    }

private:
    class HostedMidiOutputDeviceInstance : public MidiOutputDeviceInstance
    {
    public:
        HostedMidiOutputDeviceInstance (HostedMidiOutputDevice& o, EditPlaybackContext& epc)
            : MidiOutputDeviceInstance (o, epc), owner (o)
        {
        }

        bool sendMessages (PlayHead& playhead, MidiMessageArray& mma, EditTimeRange streamTime) override
        {
            auto editTime = playhead.streamTimeToEditWindow (streamTime);
            mma.addToTimestamps (-editTime.editRange1.getStart() - owner.getDeviceDelay());
            owner.toSend.mergeFromAndClear (mma);
            return true;
        }

        HostedMidiOutputDevice& owner;
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
    int newMaxChannels = jmax (parameters.inputChannels, parameters.outputChannels);

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

void HostedAudioDeviceInterface::processBlock (AudioBuffer<float>& buffer, MidiBuffer& midi)
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
            MidiBuffer scratchMidi;
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

}
