/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion::inline engine {

//==============================================================================
/**
    A single-producer, single-consumer fifo of audio samples.

    The reader and writer may be on different threads. The storage is a
    choc::buffer::ChannelArrayBuffer rather than a juce::AudioBuffer because the
    latter keeps a shared "hasBeenCleared" flag which both sides would touch on
    every call, giving a data race outside of the sample regions that the
    AbstractFifo keeps disjoint.
*/
class AudioFifo
{
public:
    AudioFifo (int channels, int numSamples)
        : fifo (numSamples)
    {
        setSize (channels, numSamples);
    }

    void setSize (int numChannels, int numSamples)
    {
        fifo.setTotalSize (numSamples);
        buffer.resize ({ (choc::buffer::ChannelCount) numChannels,
                         (choc::buffer::FrameCount) numSamples });
    }

    int getFreeSpace() const noexcept       { return fifo.getFreeSpace(); }
    int getNumReady() const noexcept        { return fifo.getNumReady(); }
    int getNumChannels() const noexcept     { return (int) buffer.getNumChannels(); }
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

        const auto srcView = toSourceView (src).getFrameRange (frameRange (startSample, numSamples));

        // Any fifo channels the source doesn't have are cleared
        choc::buffer::copyIntersectionAndClearOutside (buffer.getFrameRange (frameRange (start1, size1)),
                                                       srcView.getStart ((choc::buffer::FrameCount) size1));

        if (size2 != 0)
            choc::buffer::copyIntersectionAndClearOutside (buffer.getFrameRange (frameRange (start2, size2)),
                                                           srcView.getFrameRange (frameRange (size1, size2)));

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

        const auto srcView = choc::buffer::createChannelArrayView (data,
                                                                   buffer.getNumChannels(),
                                                                   (choc::buffer::FrameCount) numSamples);

        choc::buffer::copy (buffer.getFrameRange (frameRange (start1, size1)),
                            srcView.getStart ((choc::buffer::FrameCount) size1));

        if (size2 != 0)
            choc::buffer::copy (buffer.getFrameRange (frameRange (start2, size2)),
                                srcView.getFrameRange (frameRange (size1, size2)));

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

        buffer.getFrameRange (frameRange (start1, size1)).clear();

        if (size2 != 0)
            buffer.getFrameRange (frameRange (start2, size2)).clear();

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

        const auto destView = toDestView (dest).getFrameRange (frameRange (startSampleInDestBuffer, numSamples));

        copyRegionTo (destView.getStart ((choc::buffer::FrameCount) size1), start1, size1);

        if (size2 != 0)
            copyRegionTo (destView.getFrameRange (frameRange (size1, size2)), start2, size2);

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

        const auto destView = toDestView (dest).getFrameRange (frameRange (startSampleInDestBuffer, numSamples));

        addRegionTo (destView.getStart ((choc::buffer::FrameCount) size1), start1, size1);

        if (size2 != 0)
            addRegionTo (destView.getFrameRange (frameRange (size1, size2)), start2, size2);

        fifo.finishedRead (size1 + size2);
        return true;
    }

private:
    juce::AbstractFifo fifo;
    choc::buffer::ChannelArrayBuffer<float> buffer;

    static choc::buffer::FrameRange frameRange (int start, int length)
    {
        return { (choc::buffer::FrameCount) start,
                 (choc::buffer::FrameCount) (start + length) };
    }

    static choc::buffer::ChannelArrayView<const float> toSourceView (const juce::AudioBuffer<float>& b)
    {
        return choc::buffer::createChannelArrayView (b.getArrayOfReadPointers(),
                                                     (choc::buffer::ChannelCount) b.getNumChannels(),
                                                     (choc::buffer::FrameCount) b.getNumSamples());
    }

    static choc::buffer::ChannelArrayView<float> toDestView (juce::AudioBuffer<float>& b)
    {
        return choc::buffer::createChannelArrayView (b.getArrayOfWritePointers(),
                                                     (choc::buffer::ChannelCount) b.getNumChannels(),
                                                     (choc::buffer::FrameCount) b.getNumSamples());
    }

    /** Returns a fifo channel to use for a destination channel, duplicating the last
        one if the destination has more channels than the fifo.
    */
    choc::buffer::MonoView<float> getSourceChannel (choc::buffer::ChannelCount destChannel,
                                                    int start, int numSamples) const
    {
        jassert (buffer.getNumChannels() > 0);
        const auto channel = std::min (destChannel, buffer.getNumChannels() - 1);

        return buffer.getChannel (channel).getFrameRange (frameRange (start, numSamples));
    }

    void copyRegionTo (choc::buffer::ChannelArrayView<float> dest, int start, int numSamples) const
    {
        for (choc::buffer::ChannelCount channel = 0; channel < dest.getNumChannels(); ++channel)
            choc::buffer::copy (dest.getChannel (channel), getSourceChannel (channel, start, numSamples));
    }

    void addRegionTo (choc::buffer::ChannelArrayView<float> dest, int start, int numSamples) const
    {
        for (choc::buffer::ChannelCount channel = 0; channel < dest.getNumChannels(); ++channel)
            choc::buffer::add (dest.getChannel (channel), getSourceChannel (channel, start, numSamples));
    }

    JUCE_DECLARE_NON_COPYABLE (AudioFifo)
};

} // namespace tracktion::inline engine
