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
    list.reset (new WireList (*this, state));

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

void PatchBayPlugin::getChannelNames (juce::StringArray* ins,
                                      juce::StringArray* outs)
{
    if (ins != nullptr)
    {
        auto inputPlugin = findPluginThatFeedsIntoThis();

        if (inputPlugin != nullptr && ! recursionCheck)
        {
            juce::StringArray out;
            recursionCheck = true;
            inputPlugin->getChannelNames (nullptr, &out);
            recursionCheck = false;
            ins->addArray (out);
        }
        else
        {
            getLeftRightChannelNames (ins);
        }
    }

    if (outs != nullptr)
    {
        auto outputPlugin = findPluginThatThisFeedsInto();

        if (outputPlugin != nullptr && ! recursionCheck)
        {
            juce::StringArray in;
            recursionCheck = true;
            outputPlugin->getChannelNames (&in, nullptr);
            recursionCheck = false;
            outs->addArray (in);
        }
        else
        {
            getLeftRightChannelNames (outs);
        }
    }
}

void PatchBayPlugin::initialise (const PluginInitialisationInfo&)
{
}

void PatchBayPlugin::deinitialise()
{
}

void PatchBayPlugin::applyToBuffer (const PluginRenderContext& fc)
{
    if (fc.destBuffer != nullptr)
    {
        SCOPED_REALTIME_CHECK

        int maxOutputChan = 1;
        AudioScratchBuffer scratch (2, fc.bufferNumSamples);
        auto& outputBuffer = scratch.buffer;
        outputBuffer.clear();

        {
            const juce::ScopedLock sl (list->arrayLock);

            for (auto w : list->objects)
            {
                maxOutputChan = std::max (w->destChannelIndex.get(), maxOutputChan);

                if (w->destChannelIndex < 2 && w->sourceChannelIndex < fc.destBuffer->getNumChannels())
                    outputBuffer.addFrom (w->destChannelIndex, 0,
                                          *fc.destBuffer, w->sourceChannelIndex, fc.bufferStartSample,
                                          fc.bufferNumSamples,
                                          dbToGain (w->gainDb));
            }
        }

        maxOutputChan = std::min (2, maxOutputChan + 1);
        fc.destBuffer->setSize (maxOutputChan, fc.destBuffer->getNumSamples(), false);

        for (int i = maxOutputChan; --i >= 0;)
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

}} // namespace tracktion { inline namespace engine
