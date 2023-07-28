//
//    ██████ ██   ██  ██████   ██████
//   ██      ██   ██ ██    ██ ██            ** Classy Header-Only Classes **
//   ██      ███████ ██    ██ ██
//   ██      ██   ██ ██    ██ ██           https://github.com/Tracktion/choc
//    ██████ ██   ██  ██████   ██████
//
//   CHOC is (C)2022 Tracktion Corporation, and is offered under the terms of the ISC license:
//
//   Permission to use, copy, modify, and/or distribute this software for any purpose with or
//   without fee is hereby granted, provided that the above copyright notice and this permission
//   notice appear in all copies. THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
//   WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
//   AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR
//   CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
//   WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
//   CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

#ifndef CHOC_AUDIOMIDIDISPATCHER_HEADER_INCLUDED
#define CHOC_AUDIOMIDIDISPATCHER_HEADER_INCLUDED

#include <functional>
#include "../containers/choc_Span.h"
#include "../containers/choc_SingleReaderSingleWriterFIFO.h"
#include "../platform/choc_HighResolutionSteadyClock.h"
#include "choc_SampleBuffers.h"
#include "choc_MIDI.h"

namespace choc::audio
{

//==============================================================================
/**
    Collects and synchronises incoming live audio and MIDI data, splitting it
    into suitably-aligned chunks for a callback to handle.

    This takes care of sub-dividing audio blocks between the times that MIDI messages
    fall, so that a client gets callbacks where all the MIDI messages lie at the
    start of the block, and don't need to worry about the timing of events within
    the block.
*/
struct AudioMIDIBlockDispatcher
{
    AudioMIDIBlockDispatcher() = default;

    //==============================================================================
    /// Prepares the dispatcher for running at the given sample-rate.
    /// This must be called before any other methods are used.
    void reset (double sampleRate, size_t midiFIFOCapacity = 1024);

    //==============================================================================
    /// Adds an incoming MIDI event to the queue. This can be called from any thread.
    void addMIDIEvent (choc::midi::ShortMessage);

    /// Adds an incoming MIDI event to the queue. This can be called from any thread.
    void addMIDIEvent (const choc::midi::Message&);

    /// Adds an incoming MIDI event to the queue. This can be called from any thread.
    void addMIDIEvent (const void* data, uint32_t size);

    /// Adds a juce::MidiMessage to the queue. This can be called from any thread.
    template <typename JUCECompatibleMIDIMessage>
    void addMIDIEvent (const JUCECompatibleMIDIMessage&);

    //==============================================================================
    /// Before calling processInChunks(), this must be called to provide the audio buffers.
    void setAudioBuffers (choc::buffer::ChannelArrayView<const float> input,
                          choc::buffer::ChannelArrayView<float> output);

    /// Before calling processInChunks(), this must be called to provide the audio buffers.
    void setAudioBuffers (const float* const* inputData, int numInputChannels,
                          float* const* outputData, int numOutputChannels, int numFrames);

    /// A function prototype which accepts a time-stamped MIDI event.
    using HandleMIDIMessageFn = std::function<void(uint32_t frame, choc::midi::ShortMessage)>;

    /// Before calling processInChunks(), this may be called to receive MIDI output events.
    void setMidiOutputCallback (HandleMIDIMessageFn);

    //==============================================================================
    /// This struct is given to a client callback to process.
    struct Block
    {
        choc::buffer::ChannelArrayView<const float> audioInput;
        choc::buffer::ChannelArrayView<float> audioOutput;
        choc::span<choc::midi::ShortMessage> midiMessages;
        const HandleMIDIMessageFn& onMidiOutputMessage;
    };

    /// After calling setAudioBuffers() to provide the audio channel data, call this
    /// to invoke a sequence of chunk callbacks on the function provided.
    /// The callback function provided must take a Block object as its parameter.
    template <typename Callback>
    void processInChunks (Callback&&);

    /// This clears the output buffers which were configured via a call to setAudioBuffers().
    void clearOutputBuffers();

    /// This governs the granularity at which MIDI events are time-stamped, and hence
    /// determines the smallest chunk size into which the callbacks will be split.
    /// Smaller sizes will give more accurate MIDI timing at the expense of extra callback
    /// overhead when there are a lot of messages. In practice, 32 frames is less than
    /// a millisecond so probably good enough for most live situations.
    uint32_t midiTimingGranularityFrames = 32;

private:
    //==============================================================================
    choc::buffer::ChannelArrayView<float> nextOutputBlock;
    choc::buffer::ChannelArrayView<const float> nextInputBlock;
    HandleMIDIMessageFn midiOutputMessageCallback;

    using TimePoint = HighResolutionSteadyClock::time_point;
    using DurationType = std::chrono::duration<double, std::ratio<1, 1>>;
    static constexpr int32_t maxCatchUpFrames = 20000;

    DurationType frameDuration;
    TimePoint lastBlockTime;
    std::vector<uint32_t> midiMessageTimes;
    std::vector<choc::midi::ShortMessage> midiMessages;
    uint32_t chunkFrameOffset = 0;

    struct TimestampedMIDIMessage
    {
        TimePoint time;
        choc::midi::ShortMessage message;
    };

    choc::fifo::SingleReaderSingleWriterFIFO<TimestampedMIDIMessage> midiFIFO;

    void fetchMIDIBlockFromFIFO (uint32_t);
};


//==============================================================================
//        _        _           _  _
//     __| |  ___ | |_   __ _ (_)| | ___
//    / _` | / _ \| __| / _` || || |/ __|
//   | (_| ||  __/| |_ | (_| || || |\__ \ _  _  _
//    \__,_| \___| \__| \__,_||_||_||___/(_)(_)(_)
//
//   Code beyond this point is implementation detail...
//
//==============================================================================

inline void AudioMIDIBlockDispatcher::reset (double sampleRate, size_t midiFIFOCapacity)
{
    midiMessageTimes.clear();
    midiMessageTimes.reserve (midiFIFOCapacity);
    midiMessages.clear();
    midiMessages.reserve (midiFIFOCapacity);
    midiFIFO.reset (midiFIFOCapacity);
    frameDuration = DurationType (1.0 / sampleRate);
}

inline void AudioMIDIBlockDispatcher::addMIDIEvent (choc::midi::ShortMessage message)
{
    midiFIFO.push ({ HighResolutionSteadyClock::now(), message });
}

inline void AudioMIDIBlockDispatcher::addMIDIEvent (const choc::midi::Message& message)
{
    if (message.isShortMessage())
        addMIDIEvent (message.getShortMessage());
}

inline void AudioMIDIBlockDispatcher::addMIDIEvent (const void* data, uint32_t size)
{
    if (size < 4)
        addMIDIEvent (choc::midi::ShortMessage (data, size));
    else
        addMIDIEvent (choc::midi::Message (data, size));
}

template <typename JUCECompatibleMIDIMessage>
void AudioMIDIBlockDispatcher::addMIDIEvent (const JUCECompatibleMIDIMessage& message)
{
    addMIDIEvent (message.getRawData(),
                  static_cast<uint32_t> (message.getRawDataSize()));
}

inline void AudioMIDIBlockDispatcher::setAudioBuffers (choc::buffer::ChannelArrayView<const float> input,
                                                       choc::buffer::ChannelArrayView<float> output)
{
    CHOC_ASSERT (input.getNumFrames() == output.getNumFrames());
    nextInputBlock = input;
    nextOutputBlock = output;
}

inline void AudioMIDIBlockDispatcher::setAudioBuffers (const float* const* inputData, int numInputChannels,
                                                       float* const* outputData, int numOutputChannels, int numFrames)
{
    setAudioBuffers (choc::buffer::createChannelArrayView (inputData,
                                                           static_cast<choc::buffer::ChannelCount> (numInputChannels),
                                                           static_cast<choc::buffer::FrameCount> (numFrames)),
                     choc::buffer::createChannelArrayView (outputData,
                                                           static_cast<choc::buffer::ChannelCount> (numOutputChannels),
                                                           static_cast<choc::buffer::FrameCount> (numFrames)));
}

inline void AudioMIDIBlockDispatcher::setMidiOutputCallback (AudioMIDIBlockDispatcher::HandleMIDIMessageFn callback)
{
    if (! callback)
    {
        midiOutputMessageCallback = {};
        return;
    }

    midiOutputMessageCallback = [this, callback = std::move (callback)] (uint32_t frame, choc::midi::ShortMessage m)
                                {
                                    callback (frame + chunkFrameOffset, m);
                                };
}

template <typename Callback>
void AudioMIDIBlockDispatcher::processInChunks (Callback&& process)
{
    chunkFrameOffset = 0;
    const auto numFrames = nextOutputBlock.getNumFrames();
    CHOC_ASSERT (numFrames == nextInputBlock.getNumFrames());
    fetchMIDIBlockFromFIFO (numFrames);

    if (auto totalNumMIDIMessages = static_cast<uint32_t> (midiMessageTimes.size()))
    {
        auto frameRange = nextOutputBlock.getFrameRange();
        uint32_t midiStart = 0;

        while (frameRange.start < frameRange.end)
        {
            auto chunkToDo = frameRange;
            auto endOfMIDI = midiStart;

            while (endOfMIDI < totalNumMIDIMessages)
            {
                auto eventTime = midiMessageTimes[endOfMIDI];

                if (eventTime > chunkToDo.start)
                {
                    chunkToDo.end = eventTime;
                    break;
                }

                ++endOfMIDI;
            }

            process (Block { nextInputBlock.getFrameRange (chunkToDo),
                             nextOutputBlock.getFrameRange (chunkToDo),
                             choc::span<const choc::midi::ShortMessage> (midiMessages.data() + midiStart,
                                                                         midiMessages.data() + endOfMIDI),
                             midiOutputMessageCallback });

            chunkFrameOffset += chunkToDo.size();
            frameRange.start = chunkToDo.end;
            midiStart = endOfMIDI;
        }
    }
    else
    {
        process (Block { nextInputBlock, nextOutputBlock, {}, midiOutputMessageCallback });
    }

    CHOC_ASSERT(chunkFrameOffset <= numFrames);
}

inline void AudioMIDIBlockDispatcher::clearOutputBuffers()
{
    nextOutputBlock.clear();
}

inline void AudioMIDIBlockDispatcher::fetchMIDIBlockFromFIFO (uint32_t numFramesNeeded)
{
    midiMessages.clear();
    midiMessageTimes.clear();

    auto blockStartTime = lastBlockTime;
    lastBlockTime = HighResolutionSteadyClock::now();

    if (midiFIFO.getUsedSlots() != 0)
    {
        TimestampedMIDIMessage m;

        while (midiFIFO.pop (m))
        {
            auto timeWithinBlock = DurationType (m.time - blockStartTime);
            auto frameIndex = static_cast<int32_t> (timeWithinBlock.count() / frameDuration.count());

            if (frameIndex < 0)
            {
                if (frameIndex < -maxCatchUpFrames)
                    continue;

                frameIndex = 0;
            }
            else if (frameIndex > static_cast<int32_t> (numFramesNeeded))
            {
                frameIndex = static_cast<int32_t> (numFramesNeeded) - 1;
            }

            auto snappedIndex = static_cast<uint32_t> (frameIndex) % midiTimingGranularityFrames;
            midiMessageTimes.push_back (snappedIndex);
            midiMessages.push_back (m.message);
        }
    }
}

} // namespace choc::audio

#endif // CHOC_AUDIOMIDIDISPATCHER_HEADER_INCLUDED
