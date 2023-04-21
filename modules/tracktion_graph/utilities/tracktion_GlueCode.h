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
/** Creates a juce::AudioBuffer from a choc::buffer::BufferView. */
inline juce::AudioBuffer<float> toAudioBuffer (choc::buffer::ChannelArrayView<float> view)
{
    return juce::AudioBuffer<float> (view.data.channels, (int) view.getNumChannels(), (int) view.data.offset, (int) view.getNumFrames());
}

/** Converts a juce::AudioBuffer<SampleType> to a choc::buffer::BufferView. */
template<typename SampleType>
inline choc::buffer::BufferView<SampleType, choc::buffer::SeparateChannelLayout> toBufferView (juce::AudioBuffer<SampleType>& buffer)
{
    return choc::buffer::createChannelArrayView (buffer.getArrayOfWritePointers(),
                                                 (choc::buffer::ChannelCount) buffer.getNumChannels(),
                                                 (choc::buffer::FrameCount) buffer.getNumSamples());
}

//==============================================================================
/** Converts a choc::midi event to a juce::MidiMessage */
inline juce::MidiMessage toMidiMessage (const choc::midi::Sequence::Event& e)
{
    return { e.message.data(), (int) e.message.length(), e.timeStamp };
}

//==============================================================================
/** Mutiplies a choc::buffer::BufferView by a juce::SmoothedValue. */
template<typename BufferViewType, typename SampleType, typename SmoothingType>
void multiplyBy (BufferViewType& view, juce::SmoothedValue<SampleType, SmoothingType>& value) noexcept
{
    if (! value.isSmoothing())
    {
        choc::buffer::applyGain (view, value.getTargetValue());
    }
    else
    {
        const auto numChannels = view.getNumChannels();
        const auto numFrames = view.getNumFrames();
        
        for (choc::buffer::FrameCount i = 0; i < numFrames; ++i)
        {
            const auto scaler = value.getNextValue();

            for (choc::buffer::ChannelCount ch = 0; ch < numChannels; ++ch)
                view.getSample (ch, i) *= scaler;
        }
    }
}

/** Returns a FrameRange with a start and length. */
inline choc::buffer::FrameRange frameRangeWithStartAndLength (choc::buffer::FrameCount start, choc::buffer::FrameCount length)
{
    return { start, start + length };
}

/** Returns a ChannelRange with a start and length. */
inline choc::buffer::ChannelRange channelRangeWithStartAndLength (choc::buffer::ChannelCount start, choc::buffer::ChannelCount length)
{
    return { start, start + length };
}


//==============================================================================
/** Checks that the channels have valid pointers if they have a non-zero number of frames. */
template <typename SampleType, template<typename> typename LayoutType>
void sanityCheckView (const choc::buffer::BufferView<SampleType, LayoutType>& view)
{
    if (view.getNumFrames() == 0)
        return;
    
    for (choc::buffer::ChannelCount channel = 0; channel < view.getNumChannels(); ++channel)
        jassert (view.getIterator (channel).sample != nullptr);
}

/** Adds two buffers applying a gain. */
template <typename DestBuffer, typename SourceBuffer, typename GainType>
static void add (DestBuffer&& dest, const SourceBuffer& source, GainType gain)
{
    auto size = source.getSize();
    CHOC_ASSERT (size == dest.getSize());

    for (decltype (size.numChannels) chan = 0; chan < size.numChannels; ++chan)
    {
        auto src = source.getIterator (chan);
        auto dst = dest.getIterator (chan);

        for (decltype (size.numFrames) i = 0; i < size.numFrames; ++i)
        {
            *dst += static_cast<decltype (dst.get())> (src.get() * gain);
            ++dst;
            ++src;
        }
    }
}

/** Adds two buffers applying a smoothed gain. */
template <typename DestBuffer, typename SourceBuffer, typename GainType>
static void addApplyingGainRamp (DestBuffer&& dest, const SourceBuffer& source, GainType startGain, GainType endGain)
{
    auto size = source.getSize();
    CHOC_ASSERT (size == dest.getSize());
    
    const auto delta = (endGain - startGain) / size.numFrames;

    for (decltype (size.numChannels) chan = 0; chan < size.numChannels; ++chan)
    {
        auto src = source.getIterator (chan);
        auto dst = dest.getIterator (chan);
        auto gain = startGain;

        for (decltype (size.numFrames) i = 0; i < size.numFrames; ++i)
        {
            *dst += static_cast<decltype (dst.get())> (src.get() * gain);
            ++dst;
            ++src;
            gain += delta;
        }
    }
}

/** Copies the contents of one view or buffer to a destination.
    If the source and dest point to the same buffer, this will skip the copy.
    This will assert if the two views do not have exactly the same size.
*/
template <typename DestBuffer, typename SourceBuffer>
static void copyIfNotAliased (DestBuffer&& dest, const SourceBuffer& source)
{
    auto size = source.getSize();
    CHOC_ASSERT (size == dest.getSize());

    for (decltype (size.numChannels) chan = 0; chan < size.numChannels; ++chan)
    {
        auto src = source.getIterator (chan);
        auto dst = dest.getIterator (chan);
        
        if (src.sample == dst.sample)
            continue;

        for (decltype (size.numFrames) i = 0; i < size.numFrames; ++i)
        {
            *dst = static_cast<decltype (dst.get())> (src.get());
            ++dst;
            ++src;
        }
    }

}

}} // namespace tracktion

#ifndef DOXYGEN
template<>
struct std::hash<juce::MidiMessageSequence>
{
    size_t operator() (const juce::MidiMessageSequence& sequence) const noexcept
    {
        size_t seed = 0;

        for (auto meh : sequence)
        {
            auto& me = meh->message;
            tracktion::hash_combine (seed, me.getTimeStamp());
            tracktion::hash_range (seed, me.getRawData(), me.getRawData() + me.getRawDataSize());
        }

        return seed;
    }
};

template<>
struct std::hash<std::vector<juce::MidiMessageSequence>>
{
    size_t operator() (const std::vector<juce::MidiMessageSequence>& sequences) const noexcept
    {
        return tracktion::hash_range (sequences);
    }
};
#endif
