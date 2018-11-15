/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


PatchBayPlugin::Wire::Wire (const ValueTree& v, UndoManager* um)  : state (v)
{
    sourceChannelIndex.referTo (state, IDs::srcChan, um);
    destChannelIndex.referTo (state, IDs::dstChan, um);
    gainDb.referTo (state, IDs::gainDb, um);
}

struct PatchBayPlugin::WireList  : public ValueTreeObjectList<PatchBayPlugin::Wire, CriticalSection>,
                                   private AsyncUpdater
{
    WireList (PatchBayPlugin& pb, const ValueTree& parent)
        : ValueTreeObjectList<Wire, CriticalSection> (parent), patchbay (pb)
    {
        rebuildObjects();
    }

    ~WireList()
    {
        freeObjects();
    }

    bool isSuitableType (const ValueTree& v) const override { return v.hasType (IDs::CONNECTION); }
    Wire* createNewObject (const ValueTree& v) override     { return new Wire (v, patchbay.getUndoManager()); }
    void deleteObject (Wire* w) override                    { delete w; }

    void newObjectAdded (Wire*) override                    { triggerAsyncUpdate(); }
    void objectRemoved (Wire*) override                     { triggerAsyncUpdate(); }
    void objectOrderChanged() override                      {}
    void valueTreePropertyChanged (ValueTree&, const Identifier&) override  { triggerAsyncUpdate(); }

    void handleAsyncUpdate() override
    {
        patchbay.changed();
    }

    PatchBayPlugin& patchbay;
};

//==============================================================================
PatchBayPlugin::PatchBayPlugin (PluginCreationInfo info) : Plugin (info)
{
    list = new WireList (*this, state);

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

void PatchBayPlugin::getChannelNames (StringArray* ins, StringArray* outs)
{
    if (ins != nullptr)
    {
        auto inputPlugin = findPluginThatFeedsIntoThis();

        if (inputPlugin != nullptr && ! recursionCheck)
        {
            StringArray out;
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
            StringArray in;
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

void PatchBayPlugin::initialise (const PlaybackInitialisationInfo&)
{
}

void PatchBayPlugin::deinitialise()
{
}

void PatchBayPlugin::applyToBuffer (const AudioRenderContext& fc)
{
    if (fc.destBuffer != nullptr)
    {
        SCOPED_REALTIME_CHECK

        int maxOutputChan = 1;
        AudioScratchBuffer scratch (2, fc.bufferNumSamples);
        auto& outputBuffer = scratch.buffer;
        outputBuffer.clear();

        {
            const ScopedLock sl (list->arrayLock);

            for (auto w : list->objects)
            {
                maxOutputChan = jmax (w->destChannelIndex.get(), maxOutputChan);

                if (w->destChannelIndex < 2 && w->sourceChannelIndex < fc.destBuffer->getNumChannels())
                    outputBuffer.addFrom (w->destChannelIndex, 0,
                                          *fc.destBuffer, w->sourceChannelIndex, fc.bufferStartSample,
                                          fc.bufferNumSamples,
                                          dbToGain (w->gainDb));
            }
        }

        maxOutputChan = jmin (2, maxOutputChan + 1);
        fc.destBuffer->setSize (maxOutputChan, fc.destBuffer->getNumSamples(), false);

        for (int i = maxOutputChan; --i >= 0;)
            fc.destBuffer->copyFrom (i, fc.bufferStartSample, outputBuffer, i, 0, fc.bufferNumSamples);
    }
}

void PatchBayPlugin::makeConnection (int inputChannel, int outputChannel, float gainDb, UndoManager* um)
{
    for (auto w : list->objects)
        if (w->sourceChannelIndex == inputChannel && w->destChannelIndex == outputChannel)
            return;

    ValueTree w (IDs::CONNECTION);
    w.setProperty (IDs::srcChan, inputChannel, nullptr);
    w.setProperty (IDs::dstChan, outputChannel, nullptr);
    w.setProperty (IDs::gainDb, gainDb, nullptr);

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
