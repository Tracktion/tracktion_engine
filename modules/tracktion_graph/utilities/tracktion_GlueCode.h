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
/** Converts a choc::buffer::BufferView to a juce::dsp::AudioBlock<float>. */
template <typename SampleType, template<typename> typename LayoutType>
juce::dsp::AudioBlock<float> toAudioBlock (const choc::buffer::BufferView<SampleType, LayoutType>& view)
{
    return juce::dsp::AudioBlock<float> (view.data.channels,
                                         (size_t) view.size.numChannels,
                                         (size_t) view.data.offset,
                                         (size_t) view.size.numFrames);
}

/** Converts a choc::buffer to a juce::dsp::AudioBlock<float>. */
template<typename ChocBufferType>
juce::dsp::AudioBlock<float> toAudioBlock (const ChocBufferType& buffer)
{
    return toAudioBlock (buffer.getView());
}

/** Creates a juce::AudioBuffer from a juce::AudioBlock. */
inline juce::AudioBuffer<float> createAudioBuffer (const juce::dsp::AudioBlock<float>& block)
{
    constexpr int maxNumChannels = 128;
    const int numChannels = std::min (maxNumChannels, (int) block.getNumChannels());
    float* chans[maxNumChannels] = {};

    for (int i = 0; i < numChannels; ++i)
        chans[i] = block.getChannelPointer ((size_t) i);

    return juce::AudioBuffer<float> (chans, numChannels, (int) block.getNumSamples());
}

/** Creates a juce::AudioBuffer from a choc::buffer::BufferView. */
template <typename SampleType, template<typename> typename LayoutType>
inline juce::AudioBuffer<float> createAudioBuffer (const choc::buffer::BufferView<SampleType, LayoutType>& view)
{
    return createAudioBuffer (toAudioBlock (view));
}

/** Creates a juce::AudioBuffer from a choc::buffer. */
template<typename ChocBufferType>
inline juce::AudioBuffer<float> createAudioBuffer (const ChocBufferType& buffer)
{
    return createAudioBuffer (toAudioBlock (buffer));
}

/** This type of conversion isn't possible as you can't get an array of pointers from an AudioBlock.
    Use the original block source to create the view instead.
*/
template<typename SampleType>
choc::buffer::BufferView<SampleType, choc::buffer::SeparateChannelLayout> toBufferView (const juce::dsp::AudioBlock<SampleType>&);

/** Converts a juce::AudioBuffer<SampleType> to a choc::buffer::BufferView. */
template<typename SampleType>
inline choc::buffer::BufferView<SampleType, choc::buffer::SeparateChannelLayout> toBufferView (juce::AudioBuffer<SampleType>& buffer)
{
    return choc::buffer::createChannelArrayView (buffer.getArrayOfWritePointers(),
                                                 (choc::buffer::ChannelCount) buffer.getNumChannels(),
                                                 (choc::buffer::FrameCount) buffer.getNumSamples());
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


} // namespace tracktion_graph
