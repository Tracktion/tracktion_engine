/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


static void getPossibleInputDeviceNames (Engine& e,
                                         StringArray& s, StringArray& a,
                                         BigInteger& hasAudio,
                                         BigInteger& hasMidi)
{
    auto& dm = e.getDeviceManager();

    for (int i = 0; i < dm.getNumInputDevices(); ++i)
    {
        if (auto in = dm.getInputDevice (i))
        {
            if (! in->isEnabled())
                continue;

            if (dynamic_cast<MidiInputDevice*> (in) != nullptr)
                hasMidi.setBit (s.size(), true);
            else
                hasAudio.setBit (s.size(), true);

            s.add (in->getName());
            a.add (in->getAlias());
        }
    }
}

static void getPossibleOutputDeviceNames (Engine& e,
                                          StringArray& s, StringArray& a,
                                          BigInteger& hasAudio,
                                          BigInteger& hasMidi)
{
    auto& dm = e.getDeviceManager();

    for (int i = 0; i < dm.getNumOutputDevices(); ++i)
    {
        if (auto out = dm.getOutputDeviceAt (i))
        {
            if (! out->isEnabled())
                continue;

            if (auto m = dynamic_cast<MidiOutputDevice*> (out))
            {
                if (m->isConnectedToExternalController())
                    continue;

                hasMidi.setBit (s.size(), true);
            }
            else
            {
                hasAudio.setBit (s.size(), true);
            }

            s.add (out->getName());
            a.add (out->getAlias());
        }
    }
}

//==============================================================================
/** The output device will create one of these via the InsertPlugin.
    During the render callback the buffer should be filled with data which will
    be sent to the external device.
*/
class InsertPlugin::InsertAudioNode  : public AudioNode
{
public:
    InsertAudioNode (InsertPlugin& o) : owner (o), plugin (&o) {}

    void getAudioNodeProperties (AudioNodeProperties& info) override
    {
        info.hasAudio = owner.hasAudio();
        info.hasMidi = owner.hasMidi();
        info.numberOfChannels = info.hasAudio ? 2 : 1;
    }

    void visitNodes (const VisitorFn& v) override               { v (*this); }
    bool isReadyToRender() override                             { return true; }

    bool purgeSubNodes (bool keepAudio, bool keepMidi) override
    {
        if (keepAudio && owner.hasAudio())
            return true;

        return keepMidi && owner.hasMidi();
    }

    void prepareAudioNodeToPlay (const PlaybackInitialisationInfo&) override {}

    void releaseAudioNodeResources() override                       {}
    void prepareForNextBlock (const AudioRenderContext&) override   {}
    void renderOver (const AudioRenderContext& rc) override         { callRenderAdding (rc); }
    void renderAdding (const AudioRenderContext& rc) override       { owner.fillSendBuffer (rc); }

private:
    InsertPlugin& owner;
    Plugin::Ptr plugin;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InsertAudioNode)
};

//==============================================================================
/** The return audio node hooks into the input device.
    After the call to renderOver or renderAdding, the buffer will contain the input
    data which should in turn be routed to the main audio node output.
*/
class InsertPlugin::InsertReturnAudioNode  : public SingleInputAudioNode
{
public:
    InsertReturnAudioNode (InsertPlugin& o, AudioNode* inputDeviceAudioNode)
        : SingleInputAudioNode (inputDeviceAudioNode), owner (o), plugin (&o)
    {
    }

    bool purgeSubNodes (bool keepAudio, bool keepMidi) override
    {
        if (keepAudio && owner.hasAudio())
            return true;

        return keepMidi && owner.hasMidi();
    }

    void renderOver (const AudioRenderContext& rc) override
    {
        callRenderAdding (rc);
    }

    void renderAdding (const AudioRenderContext& rc) override
    {
        const int numChans = rc.destBuffer->getNumChannels();
        const int numSamples = rc.destBuffer->getNumSamples();

        AudioRenderContext rc2 (rc);
        AudioScratchBuffer scratch (numChans, numSamples);

        rc2.destBuffer = &scratch.buffer;
        rc2.bufferStartSample = 0;
        scratch.buffer.clear();

        rc2.bufferForMidiMessages = &midiScratch;
        midiScratch.clear();

        SingleInputAudioNode::renderAdding (rc2);
        owner.fillReturnBuffer (rc2);
    }

    void releaseAudioNodeResources() override
    {
        midiScratch.clear();
    }

private:
    InsertPlugin& owner;
    Plugin::Ptr plugin;
    MidiMessageArray midiScratch;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InsertReturnAudioNode)
};

//==============================================================================
InsertPlugin::InsertPlugin (PluginCreationInfo info)  : Plugin (info)
{
    auto um = getUndoManager();
    name.referTo (state, IDs::name, um);
    inputDevice.referTo (state, IDs::inputDevice, um);
    outputDevice.referTo (state, IDs::outputDevice, um);
    manualAdjustMs.referTo (state, IDs::manualAdjustMs, um);

    updateDeviceTypes();
}

InsertPlugin::~InsertPlugin()
{
    notifyListenersOfDeletion();
}

AudioNode* InsertPlugin::createSendAudioNode (OutputDevice& device)
{
    CRASH_TRACER
    AudioNode* node = nullptr;

    if (outputDevice == device.getName())
    {
        node = new InsertAudioNode (*this);

        auto getReturnNode = [this] () -> AudioNode*
        {
            if (returnDeviceType != noDevice)
                for (auto i : edit.getAllInputDevices())
                    if (i->owner.getName() == inputDevice)
                        return new InsertReturnAudioNode (*this, i->createLiveInputNode());

            return {};
        };

        if (AudioNode* returnNode = getReturnNode())
        {
            MixerAudioNode* mixer = new MixerAudioNode (false, false);
            mixer->addInput (node);
            mixer->addInput (returnNode);

            node = mixer;
        }
    }

    return node;
}

//==============================================================================
const char* InsertPlugin::xmlTypeName ("insert");

String InsertPlugin::getName()                                               { return name.get().isNotEmpty() ? name : TRANS("Insert Plugin"); }
String InsertPlugin::getPluginType()                                         { return xmlTypeName; }
String InsertPlugin::getShortName (int)                                      { return TRANS("Insert"); }
double InsertPlugin::getLatencySeconds()                                     { return latencySeconds; }
int InsertPlugin::getLatencySamples()                                        { return latencySamples; }
void InsertPlugin::getChannelNames (StringArray*, StringArray*)              {}
bool InsertPlugin::takesAudioInput()                                         { return true; }
bool InsertPlugin::takesMidiInput()                                          { return true; }
bool InsertPlugin::canBeAddedToClip()                                        { return false; }
bool InsertPlugin::needsConstantBufferSize()                                 { return true; }

void InsertPlugin::initialise (const PlaybackInitialisationInfo& info)
{
    sendBuffer.setSize (2, info.blockSizeSamples, false);
    sendBuffer.clear();

    returnBuffer.setSize (2, info.blockSizeSamples, false);
    returnBuffer.clear();

    initialiseWithoutStopping (info);
}

void InsertPlugin::initialiseWithoutStopping (const PlaybackInitialisationInfo& info)
{
    // This latency number is from trial and error, may need more testing
    latencySeconds = manualAdjustMs / 1000.0 + (double)info.blockSizeSamples / info.sampleRate;
    latencySamples = (int) (manualAdjustMs * info.sampleRate / 1000 + info.blockSizeSamples);
}

void InsertPlugin::deinitialise()
{
    sendBuffer.setSize (2, 32, false);
    returnBuffer.setSize (2, 32, false);

    sendMidiBuffer.clear();
    returnMidiBuffer.clear();
}

void InsertPlugin::applyToBuffer (const AudioRenderContext& fc)
{
    CRASH_TRACER
    // Fill send buffer with data
    if (sendDeviceType == audioDevice && fc.destBuffer != nullptr)
    {
        auto& src = *fc.destBuffer;
        const int numChans      = jmin (sendBuffer.getNumChannels(), src.getNumChannels());
        const int numSamples    = jmin (sendBuffer.getNumSamples(), src.getNumSamples());

        for (int i = numChans; --i >= 0;)
            sendBuffer.copyFrom (i, 0, src, i, fc.bufferStartSample, numSamples);
    }
    else if (sendDeviceType == midiDevice && fc.bufferForMidiMessages != nullptr)
    {
        sendMidiBuffer.mergeFromAndClear (*fc.bufferForMidiMessages);
    }

    // Clear the context buffers
    if (fc.bufferForMidiMessages != nullptr)
        fc.bufferForMidiMessages->clear();

    if (fc.destBuffer != nullptr)
        fc.destBuffer->clear (fc.bufferStartSample, fc.bufferNumSamples);

    if (sendDeviceType == noDevice)
        return;

    // Copy the return buffer into the context
    if (returnDeviceType == audioDevice && fc.destBuffer != nullptr)
    {
        auto& dest = *fc.destBuffer;
        const int numChans      = jmin (returnBuffer.getNumChannels(), dest.getNumChannels());
        const int numSamples    = jmin (returnBuffer.getNumSamples(), dest.getNumSamples());

        for (int i = numChans; --i >= 0;)
            dest.copyFrom (i, fc.bufferStartSample, returnBuffer, i, 0, numSamples);
    }
    else if (returnDeviceType == midiDevice && fc.bufferForMidiMessages != nullptr)
    {
        fc.bufferForMidiMessages->mergeFromAndClear (returnMidiBuffer);
    }
}

String InsertPlugin::getSelectableDescription()
{
    return TRANS("Insert Plugin");
}

void InsertPlugin::restorePluginStateFromValueTree (const ValueTree& v)
{
    if (v.hasProperty (IDs::name))
        name = v.getProperty (IDs::name).toString();

    if (v.hasProperty (IDs::outputDevice))
        outputDevice = v.getProperty (IDs::outputDevice).toString();

    if (v.hasProperty (IDs::inputDevice))
        inputDevice = v.getProperty (IDs::inputDevice).toString();
}

void InsertPlugin::updateDeviceTypes()
{
    CRASH_TRACER
    TRACKTION_ASSERT_MESSAGE_THREAD

    StringArray devices, aliases;
    BigInteger hasAudio, hasMidi;

    auto setDeviceType = [] (DeviceType& deviceType, BigInteger& audio, BigInteger& midi, int index)
    {
        if (audio[index])       deviceType = audioDevice;
        else if (midi[index])   deviceType = midiDevice;
        else                    deviceType = noDevice;
    };

    getPossibleInputDeviceNames (engine, devices, aliases, hasAudio, hasMidi);
    setDeviceType (returnDeviceType, hasAudio, hasMidi, devices.indexOf (inputDevice.get()));

    getPossibleOutputDeviceNames (engine, devices, aliases, hasAudio, hasMidi);
    setDeviceType (sendDeviceType, hasAudio, hasMidi, devices.indexOf (outputDevice.get()));

    propertiesChanged();
    changed();
}

void InsertPlugin::getPossibleDeviceNames (Engine& e,
                                           StringArray& s, StringArray& a,
                                           BigInteger& hasAudio,
                                           BigInteger& hasMidi,
                                           bool forInput)
{
    if (forInput)
        getPossibleInputDeviceNames (e, s, a, hasAudio, hasMidi);
    else
        getPossibleOutputDeviceNames (e, s, a, hasAudio, hasMidi);
}

bool InsertPlugin::hasAudio() const       { return sendDeviceType == audioDevice  || returnDeviceType == audioDevice; }
bool InsertPlugin::hasMidi() const        { return sendDeviceType == midiDevice   || returnDeviceType == midiDevice; }

void InsertPlugin::fillSendBuffer (const AudioRenderContext& rc)
{
    CRASH_TRACER
    if (sendDeviceType == audioDevice)
    {
        if (rc.destBuffer == nullptr)
            return;

        auto& dest = *rc.destBuffer;
        const int numChans      = jmin (sendBuffer.getNumChannels(), dest.getNumChannels());
        const int numSamples    = jmin (sendBuffer.getNumSamples(), dest.getNumSamples());

        for (int i = numChans; --i >= 0;)
            dest.copyFrom (i, rc.bufferStartSample,
                           sendBuffer, i, 0, numSamples);
    }
    else if (sendDeviceType == midiDevice)
    {
        if (rc.bufferForMidiMessages != nullptr)
            rc.bufferForMidiMessages->mergeFromAndClear (sendMidiBuffer);
    }
}

void InsertPlugin::fillReturnBuffer (const AudioRenderContext& rc)
{
    CRASH_TRACER
    if (returnDeviceType == audioDevice)
    {
        if (rc.destBuffer == nullptr)
            return;

        auto& src = *rc.destBuffer;
        const int numChans      = jmin (returnBuffer.getNumChannels(), src.getNumChannels());
        const int numSamples    = jmin (returnBuffer.getNumSamples(), src.getNumSamples());

        for (int i = numChans; --i >= 0;)
            returnBuffer.copyFrom (i, 0, src, i, rc.bufferStartSample, numSamples);
    }
    else if (returnDeviceType == midiDevice)
    {
        if (rc.bufferForMidiMessages != nullptr)
            returnMidiBuffer.mergeFromAndClear (*rc.bufferForMidiMessages);
    }
}

void InsertPlugin::valueTreePropertyChanged (ValueTree& v, const Identifier& i)
{
    if (v == state)
    {
        auto update = [&i] (CachedValue<String>& deviceName) -> bool
        {
            if (i != deviceName.getPropertyID())
                return false;

            deviceName.forceUpdateOfCachedValue();
            return true;
        };

        if (update (outputDevice) || update (inputDevice))
            updateDeviceTypes();
    }

    Plugin::valueTreePropertyChanged (v, i);
}
