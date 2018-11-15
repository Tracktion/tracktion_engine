/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
{

//==============================================================================
/**
*/
class AudioFifo
{
public:
    AudioFifo (int channels, int numSamples)
        : fifo (numSamples), buffer (channels, numSamples)
    {
    }

    void setSize (int numChannels, int numSamples)
    {
        fifo.setTotalSize (numSamples);
        buffer.setSize (numChannels, numSamples);
    }

    int getFreeSpace() const noexcept       { return fifo.getFreeSpace(); }
    int getNumReady() const noexcept        { return fifo.getNumReady(); }
    int getNumChannels() const noexcept     { return buffer.getNumChannels(); }
    void reset() noexcept                   { fifo.reset(); }

    void ensureFreeSpace (int numSamples)
    {
        const int freeSpace = getFreeSpace();

        if (numSamples > freeSpace)
        {
            const int samplesRequired = numSamples - freeSpace;
            jassert (samplesRequired <= getNumReady());
            fifo.finishedRead (samplesRequired);
        }
    }

    bool write (const juce::AudioBuffer<float>& src)
    {
        return write (src.getArrayOfReadPointers(), src.getNumSamples());
    }

    bool write (const juce::AudioBuffer<float>& src, int startSample, int numSamples)
    {
        if (numSamples <= 0)
            return true;

        int start1, size1, start2, size2;
        fifo.prepareToWrite (numSamples, start1, size1, start2, size2);

        if (size1 + size2 < numSamples)
            return false;

        for (int i = juce::jmin (buffer.getNumChannels(), src.getNumChannels()); --i >= 0;)
        {
            buffer.copyFrom (i, start1, src.getReadPointer (i, startSample), size1);
            buffer.copyFrom (i, start2, src.getReadPointer (i, startSample) + size1, size2);
        }

        for (int i = juce::jmax (buffer.getNumChannels(), src.getNumChannels());
             --i >= juce::jmin (buffer.getNumChannels(), src.getNumChannels());)
        {
            buffer.clear (i, start1, size1);
            buffer.clear (i, start2, size1 + size2);
        }

        fifo.finishedWrite (size1 + size2);
        return true;
    }

    bool write (const float* const* data, int numSamples)
    {
        if (numSamples <= 0)
            return true;

        int start1, size1, start2, size2;
        fifo.prepareToWrite (numSamples, start1, size1, start2, size2);

        if (size1 + size2 < numSamples)
            return false;

        for (int i = buffer.getNumChannels(); --i >= 0;)
        {
            buffer.copyFrom (i, start1, data[i], size1);
            buffer.copyFrom (i, start2, data[i] + size1, size2);
        }

        fifo.finishedWrite (size1 + size2);
        return true;
    }

    bool writeSilence (int numSamples)
    {
        if (numSamples <= 0)
            return true;

        int start1, size1, start2, size2;
        fifo.prepareToWrite (numSamples, start1, size1, start2, size2);

        if (size1 + size2 < numSamples)
            return false;

        buffer.clear (start1, size1);
        buffer.clear (start2, size2);

        fifo.finishedWrite (size1 + size2);

        return true;
    }

    bool read (juce::AudioBuffer<float>& dest, int startSampleInDestBuffer)
    {
        return read (dest, startSampleInDestBuffer, dest.getNumSamples());
    }

    bool read (juce::AudioBuffer<float>& dest, int startSampleInDestBuffer, int numSamples)
    {
        int start1, size1, start2, size2;
        fifo.prepareToRead (numSamples, start1, size1, start2, size2);

        if (size1 + size2 < numSamples)
            return false;

        for (int i = juce::jmin (buffer.getNumChannels(), dest.getNumChannels()); --i >= 0;)
        {
            dest.copyFrom (i, startSampleInDestBuffer, buffer, i, start1, size1);
            dest.copyFrom (i, startSampleInDestBuffer + size1, buffer, i, start2, size2);
        }
        for (int i = dest.getNumChannels(); --i >= buffer.getNumChannels();)
        {
            dest.copyFrom (i, startSampleInDestBuffer, buffer, buffer.getNumChannels() - 1, start1, size1);
            dest.copyFrom (i, startSampleInDestBuffer + size1, buffer, buffer.getNumChannels() - 1, start2, size2);
        }

        fifo.finishedRead (size1 + size2);
        return true;
    }

    bool readAdding (juce::AudioBuffer<float>& dest, int startSampleInDestBuffer)
    {
        return readAdding (dest, startSampleInDestBuffer, dest.getNumSamples());
    }

    bool readAdding (juce::AudioBuffer<float>& dest, int startSampleInDestBuffer, int numSamples)
    {
        int start1, size1, start2, size2;
        fifo.prepareToRead (numSamples, start1, size1, start2, size2);

        if ((size1 + size2) < numSamples)
            return false;

        for (int i = buffer.getNumChannels(); --i >= 0;)
        {
            dest.addFrom (i, startSampleInDestBuffer, buffer, i, start1, size1);
            dest.addFrom (i, startSampleInDestBuffer + size1, buffer, i, start2, size2);
        }
        for (int i = dest.getNumChannels(); --i >= buffer.getNumChannels();)
        {
            dest.addFrom (i, startSampleInDestBuffer, buffer, buffer.getNumChannels() - 1, start1, size1);
            dest.addFrom (i, startSampleInDestBuffer + size1, buffer, buffer.getNumChannels() - 1, start2, size2);
        }

        fifo.finishedRead (size1 + size2);
        return true;
    }

private:
    juce::AbstractFifo fifo;
    juce::AudioBuffer<float> buffer;

    JUCE_DECLARE_NON_COPYABLE (AudioFifo)
};

} // namespace tracktion_engine
