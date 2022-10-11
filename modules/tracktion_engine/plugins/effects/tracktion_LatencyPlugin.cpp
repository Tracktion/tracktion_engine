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
//==============================================================================
//==============================================================================
/** Simple delay register.
*/
class LatencyPlugin::DelayRegister
{
public:
    //==============================================================================
    /** Creates an empty DelayRegister. */
    DelayRegister() = default;

    /** Sets the max size of the register.
        After setting this, calls to setSize should not be larger than maxSize.
     */
    void setMaxSize (int maxSize)
    {
        if (++maxSize != maxBufferSize)
        {
            bufferIndex = 0;
            buffer.malloc ((size_t) maxSize);
            maxBufferSize = maxSize;
            bufferSize = std::min (bufferSize, maxBufferSize);
        }

        clear();
    }

    /** Clears the buffer. */
    void clear() noexcept
    {
        buffer.clear ((size_t) maxBufferSize);
    }

    /** Sets the size of delay in samples. */
    void setSize (int newSize)
    {
        lastBufferSize = bufferSize;
        jassert (newSize < maxBufferSize);

        if (newSize < bufferSize)
            bufferIndex = 0;

        bufferSize = std::min (maxBufferSize, newSize);
    }

    /** Gets the buffer size. */
    int getSize() const
    {
        return bufferSize;
    }

    /** Processes a number of samples, replacing the contents with delayed samples. */
    void processSamples (float* samples, int numSamples)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            ++bufferWritePos;
            bufferWritePos = bufferWritePos % maxBufferSize;
            buffer[bufferWritePos] = samples[i];

            int bufferReadPos = bufferWritePos - bufferSize;

            if (bufferReadPos < 0)
                bufferReadPos += maxBufferSize;

            samples[i] = buffer[bufferReadPos];
        }
    }

    /** Processes a number of samples, crossfading the current and last read positions to avoid glitches. */
    void processSamplesSmoothed (float* samples, int numSamples)
    {
        if (bufferSize == lastBufferSize)
        {
            processSamples (samples, numSamples);
            return;
        }

        const float gainDelta = 1.0f / numSamples;
        float newGain = 0.0f;
        float oldGain = 1.0f;

        for (int i = 0; i < numSamples; ++i)
        {
            ++bufferWritePos;
            bufferWritePos = bufferWritePos % maxBufferSize;
            buffer[bufferWritePos] = samples[i];

            int bufferReadPos = bufferWritePos - bufferSize;
            int bufferReadOld = bufferWritePos - lastBufferSize;

            if (bufferReadPos < 0)  bufferReadPos += maxBufferSize;
            if (bufferReadOld < 0)  bufferReadOld += maxBufferSize;

            const float newSample = buffer[(int) bufferReadPos];
            const float oldSample = buffer[(int) bufferReadOld];

            samples[i] = (newSample * newGain) + (oldSample * oldGain);

            newGain += gainDelta;
            oldGain -= gainDelta;
        }

        lastBufferSize = bufferSize;
    }

private:
    //==============================================================================
    juce::HeapBlock<float> buffer;
    int bufferIndex = 0, maxBufferSize = 0, bufferWritePos = 0;
    int bufferSize = 0, lastBufferSize = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DelayRegister)
};

//==============================================================================
//==============================================================================
const char* LatencyPlugin::xmlTypeName ("latencyTester");

LatencyPlugin::LatencyPlugin (PluginCreationInfo info)
    : Plugin (info)
{
    delayCompensator[0] = std::make_unique<DelayRegister>();
    delayCompensator[1] = std::make_unique<DelayRegister>();

    latencyTimeSeconds.setConstrainer ([] (float in) { return juce::jlimit (0.0f, 5.0f, in); });
    latencyTimeSeconds.referTo (state, IDs::time, getUndoManager(), 0.0f);
    applyLatency.referTo (state, IDs::apply, getUndoManager(), true);

    playbackRestartTimer.setCallback ([this]
                                      {
                                          edit.restartPlayback();
                                          playbackRestartTimer.stopTimer();
                                      });
}

LatencyPlugin::~LatencyPlugin()
{
    notifyListenersOfDeletion();
}

void LatencyPlugin::initialise (const PluginInitialisationInfo&)
{
    jassert (sampleRate > 0.0);

    const int maxDelaySamples = (int) std::ceil (5.0 * sampleRate);

    for (int i = juce::numElementsInArray (delayCompensator); --i >= 0;)
        delayCompensator[i]->setMaxSize (maxDelaySamples);
}

void LatencyPlugin::deinitialise()
{
    for (int i = juce::numElementsInArray (delayCompensator); --i >= 0;)
        delayCompensator[i]->clear();
}

void LatencyPlugin::applyToBuffer (const PluginRenderContext& rc)
{
    if (! applyLatency.get())
        return;

    if (rc.destBuffer == nullptr)
        return;

    const int delayCompensationSamples =  juce::roundToInt (latencyTimeSeconds.get() * (float) sampleRate);

    if (delayCompensationSamples != 0)
    {
        const int numChannels = std::min (rc.destBuffer->getNumChannels(),
                                          juce::numElementsInArray (delayCompensator));
        float* const* samples = rc.destBuffer->getArrayOfWritePointers();

        for (int i = 0; i < numChannels; ++i)
        {
            delayCompensator[i]->setSize (delayCompensationSamples);
            delayCompensator[i]->processSamples (samples[i] + rc.bufferStartSample, rc.bufferNumSamples);
        }
    }
}

void LatencyPlugin::restorePluginStateFromValueTree (const juce::ValueTree& v)
{
    if (v.hasProperty (IDs::time))
        latencyTimeSeconds = v[IDs::time];

    if (v.hasProperty (IDs::apply))
        applyLatency = v[IDs::apply];
}

void LatencyPlugin::valueTreePropertyChanged (juce::ValueTree& v, const juce::Identifier& i)
{
    if (v == state && i == IDs::time)
        playbackRestartTimer.startTimer (50);

    Plugin::valueTreePropertyChanged (v, i);
}

}} // namespace tracktion { inline namespace engine
