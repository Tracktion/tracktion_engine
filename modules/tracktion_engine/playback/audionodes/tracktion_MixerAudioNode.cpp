/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


struct MultiCPU
{
    static void addDoublesToBuffer (const AudioRenderContext& rc, const AudioBuffer<double>& doubles)
    {
        for (int j = rc.destBuffer->getNumChannels(); --j >= 0;)
        {
            const double* src = doubles.getReadPointer (j);
            float* dest = rc.destBuffer->getWritePointer (j, rc.bufferStartSample);

            for (int i = 0; i < rc.bufferNumSamples; ++i)
                *dest++ += (float) *src++;
        }
    }

    static void addBufferToDoubles (juce::AudioBuffer<float>& buffer, double* const* dest, int numSamples)
    {
        for (int i = 0; i < buffer.getNumChannels(); ++i)
        {
            double* dst = dest[i];
            const float* src = buffer.getReadPointer (i, 0);

            for (int j = 0; j < numSamples; ++j)
                *dst++ += (double) *src++;
        }
    }

    //==============================================================================
    struct ParallelMixOperation
    {
        ParallelMixOperation (const AudioRenderContext& context, OwnedArray<AudioNode>& inputs)
            : nodes (inputs), rc (context) {}

        LinkedListPointer<ParallelMixOperation> nextListItem;

        OwnedArray<AudioNode>& nodes;
        CriticalSection outputBufferLock;
        Atomic<int> nextNodeToPop, pendingNodes;
        WaitableEvent pendingNodeChange;

        const AudioRenderContext& rc;
        double** buffer64 = nullptr;

        void processNode (AudioNode* node, juce::AudioBuffer<float>& buffer)
        {
            processNode (*node, buffer);

            if (--pendingNodes == 0)
                pendingNodeChange.signal();
        }

        void perform()
        {
            pendingNodes = nodes.size();
            nextNodeToPop = nodes.size();
            auto& threadPool = *MultiCPU::MixerThreadPool::getInstanceWithoutCreating();

            threadPool.addOperation (this);

            AudioScratchBuffer scratchBuffer (rc.destBuffer != nullptr ? rc.destBuffer->getNumChannels() : 256,
                                              rc.destBuffer != nullptr ? rc.destBuffer->getNumSamples()  : 1);

            while (auto node = popNextNode())
                processNode (node, scratchBuffer.buffer);

            pendingNodeChange.wait();

            threadPool.removeOperation (this);
        }

        AudioNode* popNextNode()
        {
            const int i = --nextNodeToPop;
            return i >= 0 ? nodes.getUnchecked(i) : nullptr;
        }

    private:
        void processNode (AudioNode& node, juce::AudioBuffer<float>& buffer)
        {
            MidiMessageArray midiBuffer;

            buffer.setSize (rc.destBuffer != nullptr ? rc.destBuffer->getNumChannels() : 256,
                            rc.destBuffer != nullptr ? rc.destBuffer->getNumSamples()  : 1);

            node.renderOver (AudioRenderContext (rc.playhead,
                                                 rc.streamTime,
                                                 rc.destBuffer != nullptr ? &buffer : nullptr,
                                                 rc.destBufferChannels,
                                                 0, rc.bufferNumSamples,
                                                 &midiBuffer, rc.midiBufferOffset,
                                                 rc.continuity, rc.isRendering));

            const ScopedLock sl (outputBufferLock);

            if (buffer64 != nullptr)
            {
                addBufferToDoubles (buffer, buffer64, rc.bufferNumSamples);
            }
            else if (auto* dest = rc.destBuffer)
            {
                const int numToCopy = jmin (buffer.getNumChannels(), dest->getNumChannels());

                for (int i = 0; i < numToCopy; ++i)
                    dest->addFrom (i, rc.bufferStartSample, buffer, i, 0, rc.bufferNumSamples);
            }

            if (rc.bufferForMidiMessages != nullptr)
                rc.bufferForMidiMessages->mergeFromAndClear (midiBuffer);
        }
    };

    //==============================================================================
    struct MixerThreadPool  : private DeletedAtShutdown
    {
        MixerThreadPool() {}

        ~MixerThreadPool()
        {
            threads.clear();
            clearSingletonInstance();
        }

        void addOperation (ParallelMixOperation* op)
        {
            {
                const ScopedLock sl (opLock);
                opList.insertNext (op);
            }

            for (auto thread : threads)
                thread->notify();
        }

        void removeOperation (ParallelMixOperation* op)
        {
            const ScopedLock sl (opLock);
            opList.remove (op);
        }

        void setNumThreads (int num)
        {
            if (threads.size() != num)
            {
                const ScopedLock sl (opLock);

                threads.clear();

                while (threads.size() < num)
                    threads.add (new MixerThread (*this));
            }
        }

        bool process (juce::AudioBuffer<float>& buffer)
        {
            ParallelMixOperation* op = nullptr;
            AudioNode* node = nullptr;

            {
                const ScopedLock sl (opLock);

                for (op = opList.get(); op != nullptr; op = op->nextListItem)
                {
                    if (auto n = op->popNextNode())
                    {
                        node = n;
                        break;
                    }
                }
            }

            if (node != nullptr)
            {
                op->processNode (node, buffer);
                return true;
            }

            return false;
        }

        JUCE_DECLARE_SINGLETON_SINGLETHREADED_MINIMAL (MixerThreadPool);

        //==============================================================================
        struct MixerThread : public Thread
        {
            MixerThread (MixerThreadPool& mt)
               : Thread ("mixer"), owner (mt)
            {
                startThread (Thread::realtimeAudioPriority);
            }

            ~MixerThread()
            {
                stopThread (5000);
            }

            void run() override
            {
                FloatVectorOperations::disableDenormalisedNumberSupport();
                buffer.setSize (2, 1024);

                while (! threadShouldExit())
                {
                    if (! owner.process (buffer))
                        wait (1000);
                }
            }

        private:
            juce::AudioBuffer<float> buffer;
            MixerThreadPool& owner;
        };

        CriticalSection opLock;
        OwnedArray<MixerThread> threads;

        LinkedListPointer<ParallelMixOperation> opList;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MixerThreadPool)
    };
};

JUCE_IMPLEMENT_SINGLETON (MultiCPU::MixerThreadPool);



//==============================================================================
MixerAudioNode::MixerAudioNode (bool shouldUse64Bit, bool multiCpu)
    : use64bitMixing (shouldUse64Bit),
      canUseMultiCpu (multiCpu),
      shouldUseMultiCpu (multiCpu)
{
}

void MixerAudioNode::addInput (AudioNode* inputNode)
{
    if (inputNode != nullptr)
    {
        AudioNodeProperties info;
        info.hasAudio = false;
        info.hasMidi = false;
        info.numberOfChannels = 0;

        inputNode->getAudioNodeProperties (info);

        hasAudio |= info.hasAudio;
        hasMidi |= info.hasMidi;
        maxNumberOfChannels = jmax (maxNumberOfChannels, info.numberOfChannels);

        inputs.add (inputNode);
    }
}

void MixerAudioNode::clear()
{
    inputs.clearQuick (true);
    hasMidi = false;
    hasAudio = false;
    maxNumberOfChannels = 0;
}

bool MixerAudioNode::purgeSubNodes (bool keepAudio, bool keepMidi)
{
    for (int i = inputs.size(); --i >= 0;)
        if (! inputs.getUnchecked (i)->purgeSubNodes (keepAudio, keepMidi))
            inputs.remove (i);

    return inputs.size() > 0;
}

void MixerAudioNode::getAudioNodeProperties (AudioNodeProperties& info)
{
    // update our idea of whether the inputs have audio/midi, as they might have changed
    // since being added..
    hasMidi = false;
    hasAudio = false;
    maxNumberOfChannels = 0;

    for (int i = inputs.size(); --i >= 0;)
    {
        AudioNodeProperties nodeProps;
        nodeProps.hasAudio = false;
        nodeProps.hasMidi = false;
        nodeProps.numberOfChannels = 0;

        inputs.getUnchecked (i)->getAudioNodeProperties (nodeProps);

        hasAudio |= nodeProps.hasAudio;
        hasMidi |= nodeProps.hasMidi;
        maxNumberOfChannels = jmax (maxNumberOfChannels, nodeProps.numberOfChannels);
    }

    info.hasAudio = hasAudio;
    info.hasMidi = hasMidi;
    info.numberOfChannels = maxNumberOfChannels;
}

void MixerAudioNode::visitNodes (const VisitorFn& v)
{
    v (*this);

    for (auto input : inputs)
        input->visitNodes (v);
}

void MixerAudioNode::prepareAudioNodeToPlay (const PlaybackInitialisationInfo& info)
{
    for (auto input : inputs)
        input->prepareAudioNodeToPlay (info);

    if (use64bitMixing)
        set64bitBufferSize (info.blockSizeSamples, jmax (2, maxNumberOfChannels));

    shouldUseMultiCpu = canUseMultiCpu
                         && inputs.size() > 1
                         && MultiCPU::MixerThreadPool::getInstance()->threads.size() > 0;
}

bool MixerAudioNode::isReadyToRender()
{
    for (auto input : inputs)
        if (! input->isReadyToRender())
            return false;

    return true;
}

void MixerAudioNode::releaseAudioNodeResources()
{
    for (auto input : inputs)
        input->releaseAudioNodeResources();
}

void MixerAudioNode::multiCpuRender (const AudioRenderContext& rc)
{
    if ((hasAudio || hasMidi) && inputs.size() > 0)
    {
        MultiCPU::ParallelMixOperation parallelOp (rc, inputs);

        parallelOp.buffer64 = nullptr;

        if (use64bitMixing && rc.destBuffer != nullptr)
        {
            set64bitBufferSize (rc.bufferNumSamples, jmax (2, rc.destBuffer->getNumChannels()));

            parallelOp.buffer64 = temp64bitBuffer.getArrayOfWritePointers();

            for (int j = rc.destBuffer->getNumChannels(); --j >= 0;)
            {
                double* dest = parallelOp.buffer64[j];
                const float* src = rc.destBuffer->getReadPointer (j, rc.bufferStartSample);

                for (int i = 0; i < rc.bufferNumSamples; ++i)
                     *dest++ = *src++;
            }

            parallelOp.perform();

            MultiCPU::addDoublesToBuffer (rc, temp64bitBuffer);
        }
        else
        {
            parallelOp.perform();
        }
    }
}

void MixerAudioNode::renderOver (const AudioRenderContext& rc)
{
    if (inputs.size() == 1)
        inputs.getUnchecked (0)->renderOver (rc);
    else
        callRenderAdding (rc);
}

void MixerAudioNode::renderAdding (const AudioRenderContext& rc)
{
    if ((hasAudio || hasMidi) && inputs.size() > 1)
    {
        if (shouldUseMultiCpu)
        {
            multiCpuRender (rc);
            return;
        }

        AudioRenderContext localContext (rc);

        if (use64bitMixing && rc.destBuffer != nullptr)
        {
            set64bitBufferSize (rc.bufferNumSamples, 2);

            for (int i = rc.destBuffer->getNumChannels(); --i >= 0;)
                temp64bitBuffer.clear (i, 0, rc.bufferNumSamples);

            for (auto input : inputs)
            {
                input->renderOver (localContext);

                jassert (rc.destBuffer->getNumChannels() <= 2);

                if (! rc.destBuffer->hasBeenCleared())
                {
                    for (int c = rc.destBuffer->getNumChannels(); --c >= 0;)
                    {
                        const float* src = rc.destBuffer->getReadPointer (c, rc.bufferStartSample);
                        double* dest = temp64bitBuffer.getWritePointer (c);

                        for (int j = rc.bufferNumSamples; --j >= 0;)
                            *dest++ += *src++;
                    }
                }
            }

            rc.destBuffer->clear();
            MultiCPU::addDoublesToBuffer (rc, temp64bitBuffer);
        }
        else
        {
            for (auto input : inputs)
                input->renderAdding (localContext);
        }
    }
    else if (inputs.size() == 1)
    {
        inputs.getUnchecked(0)->renderAdding (rc);
    }
}

void MixerAudioNode::prepareForNextBlock (const AudioRenderContext& rc)
{
    for (auto n : inputs)
        n->prepareForNextBlock (rc);
}

void MixerAudioNode::set64bitBufferSize (int samples, int numChans)
{
    if (samples > temp64bitBuffer.getNumSamples() || numChans > temp64bitBuffer.getNumChannels())
        temp64bitBuffer.setSize (numChans, samples, false, false, true);
}

void MixerAudioNode::updateNumCPUs (Engine& e)
{
    CRASH_TRACER
    MultiCPU::MixerThreadPool::getInstance()
        ->setNumThreads (e.getEngineBehaviour().getNumberOfCPUsToUseForAudio() - 1);
}
