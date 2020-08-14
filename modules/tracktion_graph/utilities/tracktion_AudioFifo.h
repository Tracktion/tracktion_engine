/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion_graph
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

    bool write (juce::dsp::AudioBlock<float> block)
    {
        const int internalBufferNumChannels = buffer.getNumChannels();
        const int inputNumChannels = (int) block.getNumChannels();

        jassert (inputNumChannels <= internalBufferNumChannels); // Needs to be prepared with at least the num input channels!
        int numSamples = (int) block.getNumSamples();
        int start1, size1, start2, size2;
        fifo.prepareToWrite (numSamples, start1, size1, start2, size2);

        if (size1 + size2 < numSamples)
            return false;

        for (int i = inputNumChannels; --i >= 0;)
        {
            buffer.copyFrom (i, start1, block.getChannelPointer ((size_t) i), size1);
            buffer.copyFrom (i, start2, block.getChannelPointer ((size_t) i) + size1, size2);
        }
        
        // Clear additional channels
        for (int i = internalBufferNumChannels; --i >= inputNumChannels;)
        {
            buffer.clear (i, start1, size1);
            buffer.clear (i, start2, size2);
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

    bool readAdding (const juce::dsp::AudioBlock<float>& dest)
    {
        const int internalBufferNumChannels = buffer.getNumChannels();
        const int destNumChannels = (int) dest.getNumChannels();
        
        jassert (destNumChannels <= internalBufferNumChannels);
        const int numSamples = (int) dest.getNumSamples();
        int start1, size1, start2, size2;
        fifo.prepareToRead (numSamples, start1, size1, start2, size2);

        if ((size1 + size2) < numSamples)
            return false;

        juce::dsp::AudioBlock<float> sourceBlock (buffer);
        dest.add (sourceBlock.getSubBlock ((size_t) start1, (size_t) size1));

        if (size2 > 0)
            dest.getSubBlock ((size_t) size1, (size_t) size2).add (sourceBlock.getSubBlock ((size_t) start2, (size_t) size2));

        fifo.finishedRead (size1 + size2);
        return true;
    }
    
    void removeSamples (int numSamples)
    {
        fifo.finishedRead (numSamples);
    }

private:
    juce::AbstractFifo fifo;
    juce::AudioBuffer<float> buffer;

    JUCE_DECLARE_NON_COPYABLE (AudioFifo)
};

} // namespace tracktion_graph
