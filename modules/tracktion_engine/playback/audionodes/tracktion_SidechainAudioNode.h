/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
{

class SidechainSendAudioNode  : public SingleInputAudioNode
{
public:
    SidechainSendAudioNode (AudioNode* input, EditItemID sourceTrackID)
        : SingleInputAudioNode (input), srcTrackID (sourceTrackID)
    {
        jassert (input != nullptr);
    }

    void prepareAudioNodeToPlay (const PlaybackInitialisationInfo& info) override
    {
        input->prepareAudioNodeToPlay (info);

        latencySeconds = info.blockSizeSamples / info.sampleRate;

        currentBlock.setSize (2, info.blockSizeSamples);
        currentBlock.clear();

        lastBlock.setSize (2, info.blockSizeSamples);
        lastBlock.clear();
    }

    void prepareForNextBlock (const AudioRenderContext& rc) override
    {
        input->prepareForNextBlock (rc);

        for (int i = currentBlock.getNumChannels(); --i >= 0;)
            lastBlock.copyFrom (i, 0, currentBlock, i, rc.bufferStartSample, currentBlock.getNumSamples());

        lastMidiMessages.swapWith (currentMidiMessages);
    }

    void renderOver (const AudioRenderContext& rc) override
    {
        input->renderOver (rc);
        currentMidiMessages.swapWith (*rc.bufferForMidiMessages);
        jassert (currentBlock.getNumChannels() == lastBlock.getNumChannels());
        const int numChans = juce::jmin (rc.destBuffer->getNumChannels(), currentBlock.getNumChannels());

        // Copy as many channels as we have
        for (int i = numChans; --i >= 0;)
        {
            currentBlock.copyFrom (i, 0, *rc.destBuffer, i, rc.bufferStartSample, rc.bufferNumSamples);
            rc.destBuffer->copyFrom (i, rc.bufferStartSample, lastBlock, i, 0, rc.bufferNumSamples);
        }

        // Silence any left over
        for (int i = rc.destBuffer->getNumChannels(); --i >= numChans;)
            rc.destBuffer->clear (i, rc.bufferStartSample, rc.bufferNumSamples);

        rc.bufferForMidiMessages->swapWith (lastMidiMessages);
    }

    void renderAdding (const AudioRenderContext& rc) override
    {
        callRenderOver (rc);
    }

    const EditItemID srcTrackID;

    juce::AudioBuffer<float> lastBlock;

private:
    MidiMessageArray currentMidiMessages, lastMidiMessages;
    juce::AudioBuffer<float> currentBlock;
    double latencySeconds = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SidechainSendAudioNode)
};

//==============================================================================
class SidechainReceiveAudioNode : public SingleInputAudioNode
{
public:
    SidechainReceiveAudioNode (Plugin* p, AudioNode* source)
        : SingleInputAudioNode (source), plugin (p)
    {
        jassert (input != nullptr);

        sidechainSourceID = plugin->getSidechainSourceID();
        pluginInputs = plugin->getInputChannelNames().size();

        for (int i = 0; i < plugin->getNumWires(); ++i)
            if (auto* w = plugin->getWire (i))
                routes.add ({ w->sourceChannelIndex, w->destChannelIndex });
    }

    void getAudioNodeProperties (AudioNodeProperties& info) override
    {
        input->getAudioNodeProperties (info);

        info.numberOfChannels = juce::jmax (pluginInputs, info.numberOfChannels);
    }

    void prepareAudioNodeToPlay (const PlaybackInitialisationInfo& info) override
    {
        input->prepareAudioNodeToPlay (info);

        juce::Array<SidechainSendAudioNode*> result;

        for (auto* node : *info.rootNodes)
            node->visitNodes ([&] (AudioNode& node)
                              {
                                  if (auto srcNode = dynamic_cast<SidechainSendAudioNode*> (&node))
                                      if (sidechainSourceID == srcNode->srcTrackID)
                                          result.add (srcNode);
                              });

        jassert (result.size() == 1);
        send = result.size() == 1 ? result.getFirst() : nullptr;
    }

    void renderAdding (const AudioRenderContext& rc) override
    {
        callRenderOver (rc);
    }

    void renderOver (const AudioRenderContext& rc) override
    {
        input->renderOver (rc);

        AudioScratchBuffer inputBufferCopy (*rc.destBuffer);

        jassert (rc.destBuffer->getNumChannels() == pluginInputs);
        rc.destBuffer->setSize (pluginInputs, rc.destBuffer->getNumSamples());
        rc.destBuffer->clear();

        auto getSrcBuffer = [this, &inputBufferCopy] (int idx) -> const float*
        {
            if (idx < 2)
                return inputBufferCopy.buffer.getReadPointer (idx);

            if (send != nullptr)
                return send->lastBlock.getReadPointer (idx - 2);

            return {};
        };

        for (auto r : routes)
            if (const float* srcData = getSrcBuffer (r.src))
                rc.destBuffer->addFrom (r.dst, rc.bufferStartSample, srcData, rc.destBuffer->getNumSamples());
    }

private:
    struct Route
    {
        Route (int src_ = 0, int dst_ = 0) : src (src_), dst (dst_) {}
        int src;
        int dst;
    };

    Plugin* plugin = nullptr;
    EditItemID sidechainSourceID;
    int pluginInputs = 0;
    SidechainSendAudioNode* send = nullptr;
    juce::Array<Route> routes;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SidechainReceiveAudioNode)
};

//==============================================================================
class SidechainReceiveWrapperAudioNode  : public SingleInputAudioNode
{
public:
    SidechainReceiveWrapperAudioNode (AudioNode* input_)
        : SingleInputAudioNode (input_)
    {
        jassert (input != nullptr);
    }

    void getAudioNodeProperties (AudioNodeProperties& info) override
    {
        input->getAudioNodeProperties (info);
        workBuffer.setSize (info.numberOfChannels, 512);
    }

    void prepareAudioNodeToPlay (const PlaybackInitialisationInfo& info) override
    {
        input->prepareAudioNodeToPlay (info);

        workBuffer.setSize (workBuffer.getNumChannels(), info.blockSizeSamples);
    }

    void renderAdding (const AudioRenderContext& rc) override
    {
        callRenderOver (rc);
    }

    void renderOver (const AudioRenderContext& rc) override
    {
        if (workBuffer.getNumChannels() == rc.destBuffer->getNumChannels())
        {
            input->renderOver (rc);
            return;
        }

        const int numChannels   = juce::jmax (workBuffer.getNumChannels(), rc.destBuffer->getNumChannels());
        const int numSamples    = rc.destBuffer->getNumSamples();

        if (workBuffer.getNumSamples() < numSamples || workBuffer.getNumChannels() < numChannels)
            workBuffer.setSize (numChannels, numSamples, false, false, true);

        for (int c = rc.destBuffer->getNumChannels(); --c >= 0;)
            workBuffer.copyFrom (c, 0, *rc.destBuffer, c, rc.bufferStartSample, numSamples);

        AudioRenderContext rc2 (rc);
        rc2.destBuffer = &workBuffer;
        rc2.bufferStartSample = 0;

        input->renderOver (rc2);

        // Make sure renderOver isn't changing the channel count
        jassert (rc2.destBuffer->getNumChannels() == workBuffer.getNumChannels());

        for (int c = juce::jmin (rc.destBuffer->getNumChannels(), workBuffer.getNumChannels()); --c >= 0;)
            rc.destBuffer->copyFrom (c, rc.bufferStartSample, workBuffer, c, 0, numSamples);
    }

private:
    juce::AudioBuffer<float> workBuffer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SidechainReceiveWrapperAudioNode)
};

} // namespace tracktion_engine
