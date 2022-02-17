/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion { inline namespace graph
{

//==============================================================================
/**
*/
class AudioFifo
{
public:
    AudioFifo (choc::buffer::ChannelCount numChannels,
               choc::buffer::FrameCount numFrames)
      : fifo ((int) numFrames)
    {
        setSize (numChannels, numFrames);
    }

    void setSize (choc::buffer::ChannelCount numChannels,
                  choc::buffer::FrameCount numFrames)
    {
        fifo.setTotalSize ((int) numFrames);
        buffer.resize ({ numChannels, numFrames });
    }

    int getFreeSpace() const noexcept                               { return fifo.getFreeSpace(); }
    int getNumReady() const noexcept                                { return fifo.getNumReady(); }

    choc::buffer::ChannelCount getNumChannels() const noexcept      { return buffer.getNumChannels(); }
    void reset() noexcept                                           { fifo.reset(); }

    void ensureFreeSpace (int numFrames)
    {
        auto freeSpace = getFreeSpace();

        if (numFrames > freeSpace)
        {
            auto required = numFrames - freeSpace;
            jassert (required <= getNumReady());
            fifo.finishedRead (required);
        }
    }

    bool write (choc::buffer::ChannelArrayView<float> block)
    {
        jassert (block.getNumChannels() <= buffer.getNumChannels());

        int numSamples = (int) block.getNumFrames();
        int start1, size1, start2, size2;
        fifo.prepareToWrite (numSamples, start1, size1, start2, size2);

        if (size1 + size2 < numSamples)
            return false;

        auto fifo1 = buffer.getFrameRange ({ (choc::buffer::FrameCount) start1, (choc::buffer::FrameCount) (start1 + size1) });
        auto block1 = block.getStart ((choc::buffer::FrameCount) size1);
        copyIntersectionAndClearOutside (fifo1, block1);

        if (size2 != 0)
        {
            auto fifo2 = buffer.getFrameRange ({ (choc::buffer::FrameCount) start2, (choc::buffer::FrameCount) (start2 + size2) });
            auto block2 = block.getFrameRange ({ (choc::buffer::FrameCount) size1, (choc::buffer::FrameCount) (size1 + size2) });
            copyIntersectionAndClearOutside (fifo2, block2);
        }

        fifo.finishedWrite (size1 + size2);
        return true;
    }

    bool writeSilence (choc::buffer::FrameCount numFrames)
    {
        if (numFrames == 0)
            return true;

        int start1, size1, start2, size2;
        fifo.prepareToWrite ((int) numFrames, start1, size1, start2, size2);

        if (size1 + size2 < (int) numFrames)
            return false;

        choc::buffer::FrameRange fifo1 { (choc::buffer::FrameCount) start1, (choc::buffer::FrameCount) (start1 + size1) };
        buffer.getFrameRange (fifo1).clear();

        if (size2 != 0)
        {
            choc::buffer::FrameRange fifo2 { (choc::buffer::FrameCount) start2, (choc::buffer::FrameCount) (start2 + size2) };
            buffer.getFrameRange (fifo2).clear();
        }

        fifo.finishedWrite (size1 + size2);
        return true;
    }

    bool readAdding (choc::buffer::ChannelArrayView<float> dest)
    {
        jassert (dest.getNumChannels() <= buffer.getNumChannels());

        auto numFrames = (int) dest.getNumFrames();
        int start1, size1, start2, size2;
        fifo.prepareToRead (numFrames, start1, size1, start2, size2);

        if (size1 + size2 < numFrames)
            return false;

        auto fifo1 = buffer.getFrameRange ({ (choc::buffer::FrameCount) start1, (choc::buffer::FrameCount) (start1 + size1) });
        auto block1 = dest.getStart ((choc::buffer::FrameCount) size1);
        add (block1, fifo1);

        if (size2 != 0)
        {
            auto fifo2 = buffer.getFrameRange ({ (choc::buffer::FrameCount) start2, (choc::buffer::FrameCount) (start2 + size2) });
            auto block2 = dest.getFrameRange ({ (choc::buffer::FrameCount) size1, (choc::buffer::FrameCount) (size1 + size2) });
            add (block2, fifo2);
        }

        fifo.finishedRead (size1 + size2);
        return true;
    }

    bool readOverwriting (choc::buffer::ChannelArrayView<float> dest)
    {
        jassert (dest.getNumChannels() <= buffer.getNumChannels());

        auto numFrames = (int) dest.getNumFrames();
        int start1, size1, start2, size2;
        fifo.prepareToRead (numFrames, start1, size1, start2, size2);

        if (size1 + size2 < numFrames)
            return false;

        auto fifo1 = buffer.getFrameRange ({ (choc::buffer::FrameCount) start1, (choc::buffer::FrameCount) (start1 + size1) });
        auto block1 = dest.getStart ((choc::buffer::FrameCount) size1);
        copy (block1, fifo1);

        if (size2 != 0)
        {
            auto fifo2 = buffer.getFrameRange ({ (choc::buffer::FrameCount) start2, (choc::buffer::FrameCount) (start2 + size2) });
            auto block2 = dest.getFrameRange ({ (choc::buffer::FrameCount) size1, (choc::buffer::FrameCount) (size1 + size2) });
            copy (block2, fifo2);
        }

        fifo.finishedRead (size1 + size2);
        return true;
    }
    
    void removeSamples (int numSamples)
    {
        fifo.finishedRead (numSamples);
    }

private:
    juce::AbstractFifo fifo;
    choc::buffer::ChannelArrayBuffer<float> buffer;

    JUCE_DECLARE_NON_COPYABLE (AudioFifo)
};

}} // namespace tracktion
