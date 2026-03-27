/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion::inline engine {

PatchBayPlugin::Wire::Wire (const juce::ValueTree& v, juce::UndoManager* um)  : state (v)
{
    sourceChannelIndex.referTo (state, IDs::srcChan, um);
    destChannelIndex.referTo (state, IDs::dstChan, um);
    gainDb.referTo (state, IDs::gainDb, um);
}

struct PatchBayPlugin::WireList  : public ValueTreeObjectList<PatchBayPlugin::Wire, juce::CriticalSection>,
                                   private juce::AsyncUpdater
{
    WireList (PatchBayPlugin& pb, const juce::ValueTree& parentTree)
        : ValueTreeObjectList<Wire, juce::CriticalSection> (parentTree), patchbay (pb)
    {
        rebuildObjects();
    }

    ~WireList() override
    {
        freeObjects();
    }

    bool isSuitableType (const juce::ValueTree& v) const override { return v.hasType (IDs::CONNECTION); }
    Wire* createNewObject (const juce::ValueTree& v) override     { return new Wire (v, patchbay.getUndoManager()); }
    void deleteObject (Wire* w) override                          { delete w; }

    void newObjectAdded (Wire*) override                    { triggerAsyncUpdate(); }
    void objectRemoved (Wire*) override                     { triggerAsyncUpdate(); }
    void objectOrderChanged() override                      {}
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override  { triggerAsyncUpdate(); }

    void handleAsyncUpdate() override
    {
        patchbay.changed();
    }

    PatchBayPlugin& patchbay;
};

//==============================================================================
PatchBayPlugin::PatchBayPlugin (PluginCreationInfo info) : Plugin (info)
{
    list = std::make_unique<WireList> (*this, state);
    listenToParentList();

    if (info.isNewPlugin)
        for (int i = 0; i < 2; ++i)
            makeConnection (i, i, 0.0f, nullptr);
}

PatchBayPlugin::~PatchBayPlugin()
{
    notifyListenersOfDeletion();
}

const char* PatchBayPlugin::xmlTypeName = "patchbay";

int PatchBayPlugin::getNumWires() const                           { return list->objects.size(); }
PatchBayPlugin::Wire* PatchBayPlugin::getWire (int index) const   { return list->objects[index]; }

int PatchBayPlugin::getNumOutputChannelsGivenInputs (int numInputChannels)
{
    int maxChan = numInputChannels;

    const juce::ScopedLock sl (list->arrayLock);

    for (auto w : list->objects)
        if (w->sourceChannelIndex < numInputChannels)
            maxChan = std::max (maxChan, w->destChannelIndex.get() + 1);

    return maxChan;
}

ChannelConfiguration PatchBayPlugin::getMainBusInputChannelConfiguration() const
{
    return {};
}

int PatchBayPlugin::getNumInputChannelsFromChain() const
{
    auto track = dynamic_cast<AudioTrack*> (getOwnerTrack());

    if (track == nullptr)
        return 2;

    auto config = track->getChannelConfiguration();
    int numChans = config.isEmpty() ? 2 : config.getNumChannels();

    for (auto p : track->pluginList)
    {
        if (p == this)
            break;

        numChans = p->getNumOutputChannelsGivenInputs (numChans);
    }

    return numChans;
}

int PatchBayPlugin::getNumOutputChannelsFromChain() const
{
    auto track = dynamic_cast<AudioTrack*> (getOwnerTrack());
    int numInputChans = getNumInputChannelsFromChain();

    if (track == nullptr)
        return numInputChans;

    // Check only the immediately next plugin
    bool foundSelf = false;

    for (auto p : track->pluginList)
    {
        if (p == this)
        {
            foundSelf = true;
            continue;
        }

        if (foundSelf)
        {
            auto busConfig = p->getMainBusInputChannelConfiguration();

            if (! busConfig.isEmpty())
                return std::max (numInputChans, busConfig.getNumChannels());

            return numInputChans;
        }
    }

    // No next plugin — check track output destination
    if (auto destTrack = track->getOutput().getDestinationTrack())
    {
        auto destConfig = destTrack->getChannelConfiguration();

        if (! destConfig.isEmpty())
            return std::max (numInputChans, destConfig.getNumChannels());
    }
    else if (auto outDev = dynamic_cast<WaveOutputDevice*> (track->getOutput().getOutputDevice (true)))
    {
        return std::max (numInputChans, outDev->getChannels().getNumChannels());
    }

    return numInputChans;
}

void PatchBayPlugin::getChannelNames (juce::StringArray* ins,
                                      juce::StringArray* outs)
{
    auto addNames = [] (juce::StringArray* arr, int count)
    {
        if (arr == nullptr)
            return;

        if (count == 1)
        {
            arr->add (TRANS("Mono"));
        }
        else if (count == 2)
        {
            arr->add (TRANS("Left"));
            arr->add (TRANS("Right"));
        }
        else
        {
            for (int i = 0; i < count; ++i)
                arr->add (TRANS("Channel") + " " + juce::String (i + 1));
        }
    };

    addNames (ins, getNumInputChannelsFromChain());
    addNames (outs, getNumOutputChannelsFromChain());
}

void PatchBayPlugin::initialise (const PluginInitialisationInfo&)
{
    listenToParentList();
    cacheInputAndOutputPlugins();
}

void PatchBayPlugin::deinitialise()
{
}

void PatchBayPlugin::applyToBuffer (const PluginRenderContext& fc)
{
    if (fc.destBuffer != nullptr)
    {
        SCOPED_REALTIME_CHECK

        int numInputChans = fc.destBuffer->getNumChannels();
        int numOutputChans = numInputChans;

        {
            const juce::ScopedLock sl (list->arrayLock);

            for (auto w : list->objects)
                numOutputChans = std::max (numOutputChans, w->destChannelIndex.get() + 1);
        }

        AudioScratchBuffer scratch (numOutputChans, fc.bufferNumSamples);
        auto& outputBuffer = scratch.buffer;
        outputBuffer.clear();

        {
            const juce::ScopedLock sl (list->arrayLock);

            for (auto w : list->objects)
            {
                if (w->sourceChannelIndex < numInputChans
                    && w->destChannelIndex < numOutputChans)
                    outputBuffer.addFrom (w->destChannelIndex, 0,
                                          *fc.destBuffer, w->sourceChannelIndex, fc.bufferStartSample,
                                          fc.bufferNumSamples,
                                          dbToGain (w->gainDb));
            }
        }

        fc.destBuffer->setSize (numOutputChans, fc.destBuffer->getNumSamples(), false);

        for (int i = numOutputChans; --i >= 0;)
            fc.destBuffer->copyFrom (i, fc.bufferStartSample, outputBuffer, i, 0, fc.bufferNumSamples);
    }
}

void PatchBayPlugin::makeConnection (int inputChannel, int outputChannel,
                                     float gainDb, juce::UndoManager* um)
{
    for (auto w : list->objects)
        if (w->sourceChannelIndex == inputChannel && w->destChannelIndex == outputChannel)
            return;

    auto w = createValueTree (IDs::CONNECTION,
                              IDs::srcChan, inputChannel,
                              IDs::dstChan, outputChannel,
                              IDs::gainDb, gainDb);

    state.addChild (w, -1, um);
}

void PatchBayPlugin::breakConnection (int inputChannel, int outputChannel)
{
    for (auto w : list->objects)
    {
        if (w->sourceChannelIndex == inputChannel && w->destChannelIndex == outputChannel)
        {
            state.removeChild (w->state, getUndoManager());
            break;
        }
    }
}

void PatchBayPlugin::valueTreeParentChanged (juce::ValueTree& v)
{
    ValueTreeAllEventListener::valueTreeParentChanged (v);

    if (v == state)
        listenToParentList();
}

void PatchBayPlugin::listenToParentList()
{
    auto parent = state.getParent();

    if (parent.isValid())
        parentListener = std::make_unique<LambdaValueTreeAllEventListener> (parent, [this] { changed(); });
    else
        parentListener.reset();
}

void PatchBayPlugin::cacheInputAndOutputPlugins()
{
    inputPlugin     = findPluginThatFeedsIntoThis().get();
    outputPlugin    = findPluginThatThisFeedsInto().get();
}


} // namespace tracktion::inline engine
