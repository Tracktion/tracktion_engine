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

#ifndef CHOC_AUDIOMIDIPLAYER_HEADER_INCLUDED
#define CHOC_AUDIOMIDIPLAYER_HEADER_INCLUDED

#include <mutex>
#include "../choc_AudioMIDIBlockDispatcher.h"

namespace choc::audio::io
{

//==============================================================================
/**
 *   Contains properties to control the choice and setup of the audio devices when
 *   creating an AudioMIDIPlayer object.
 */
struct AudioDeviceOptions
{
    /// Preferred sample rate, or 0 to use the default.
    uint32_t sampleRate = 0;

    /// Preferred block size, or 0 to use the default.
    uint32_t blockSize = 0;

    /// Number of input channels required.
    uint32_t inputChannelCount = 0;

    /// Number of output channels required.
    uint32_t outputChannelCount = 2;

    /// Optional API to use (e.g. "CoreAudio", "WASAPI").
    /// Leave empty to use the default.
    /// @seeAudioMIDIPlayer::getAvailableAudioAPIs()
    std::string audioAPI;

    /// Optional input device ID - leave empty for a default.
    /// You can get these IDs from AudioMIDIPlayer::getAvailableInputDevices()
    std::string inputDeviceID;

    /// Optional output device ID - leave empty for a default.
    /// You can get these IDs from AudioMIDIPlayer::getAvailableOutputDevices()
    std::string outputDeviceID;

    /// This optional function can be supplied to control whether a particular
    /// MIDI input should be used. If a function is not provided, all MIDI inputs will be used.
    std::function<bool(const std::string& name)> shouldOpenMIDIInput;

    /// This optional function can be supplied to control whether a particular
    /// MIDI output should be used. If a function is not provided, all MIDI inputs will be used.
    std::function<bool(const std::string& name)> shouldOpenMIDIOutput;

    /// Some MIDI devices use a custom client name in their properties
    std::string midiClientName = "CHOC";
};

//==============================================================================
/**
 *  Details about an audio device, as returned by an AudioMIDIPlayer.
 */
struct AudioDeviceInfo
{
    std::string deviceID;  ///< The ID of the device, which can be used to set AudioDeviceOptions::inputDeviceID or outputDeviceID.
    std::string name;      ///< A human-readable name for the device.
};

//==============================================================================
/**
 *  A callback which can be attached to an AudioMIDIPlayer, to receive callbacks
 *  which process chunks of synchronised audio and MIDI data.
 */
struct AudioMIDICallback
{
    virtual ~AudioMIDICallback() = default;

    /// This will be invoked (on an unspecified thread) if the sample rate
    /// of the device changes while this callback is attached.
    virtual void sampleRateChanged (double newRate) = 0;

    /// This will be called once before a set of calls to processSubBlock() are
    /// made, to allow the client to do any setup work that's needed.
    virtual void startBlock() = 0;

    /// After a call to startBlock(), one or more calls to processSubBlock() will be
    /// made for chunks of the main block, providing any MIDI messages that should be
    /// handled at the start of that particular subsection of the block.
    /// If `replaceOutput` is true, the caller must overwrite any data in the audio
    /// output buffer. If it is false, the caller must add its output to any existing
    /// data in that buffer.
    virtual void processSubBlock (const choc::audio::AudioMIDIBlockDispatcher::Block&,
                                  bool replaceOutput) = 0;

    /// After enough calls to processSubBlock() have been made to process the whole
    /// block, a call to endBlock() allows the client to do any clean-up work necessary.
    virtual void endBlock() = 0;
};

//==============================================================================
/**
 *  A multi-client device abstraction that provides unified callbacks for processing
 *  blocks of audio and MIDI input/output.
 *
 *  This is of course just a virtual base class. See RtAudioMIDIPlayer or
 *  RenderingAudioMIDIPlayer for examples of concrete implementations.
 *
 *  To use one of these, just
 *   - create an instance of a concrete subclass, giving it your AudioDeviceOptions
 *     to tell it what devices and settings to use
 *   - check that it opened successfully by calling getLastError()
 *   - attach one or more AudioMIDICallback objects to it, which will then be
 *     called repeatedly to process the audio and MIDI data
*/
struct AudioMIDIPlayer
{
    virtual ~AudioMIDIPlayer() = default;

    /// If something failed when creating the device, this will return an error
    /// string, or an empty string if everything is ok.
    virtual std::string getLastError() = 0;

    //==============================================================================
    /// Attaches a callback to this device.
    void addCallback (AudioMIDICallback&);
    /// Removes a previously-attached callback to this device.
    void removeCallback (AudioMIDICallback&);

    //==============================================================================
    /// The options that this device was created with.
    AudioDeviceOptions options;

    /// Provide this callback if you want to know when the options
    /// are changed (e.g. the sample rate). No guarantees about which
    /// thread may call it.
    std::function<void()> deviceOptionsChanged;

    /// Returns a list of values that AudioDeviceOptions::audioAPI
    /// could be given.
    virtual std::vector<std::string> getAvailableAudioAPIs() = 0;

    /// Returns a list of sample rates that this device could be opened with.
    virtual std::vector<uint32_t> getAvailableSampleRates() = 0;

    /// Returns a list of block sizes that could be used to open this device.
    virtual std::vector<uint32_t> getAvailableBlockSizes() = 0;

    /// Returns a list of devices that could be used for
    /// AudioDeviceOptions::inputDeviceID
    virtual std::vector<AudioDeviceInfo> getAvailableInputDevices() = 0;

    /// Returns a list of devices that could be used for
    /// AudioDeviceOptions::outputDeviceID
    virtual std::vector<AudioDeviceInfo> getAvailableOutputDevices() = 0;

    /// Returns a list of MIDI input devices
    virtual std::vector<std::string> getAvailableMIDIInputDevices() = 0;
    /// Returns a list of MIDI output devices
    virtual std::vector<std::string> getAvailableMIDIOutputDevices() = 0;

    //==============================================================================
    // These methods are used to inject MIDI events to the queue, which will be
    // delivered to the callback in the next process() call.

    /// Adds an incoming MIDI event to the queue. This can be called from any thread.
    template <typename StorageType>
    void addMIDIEvent (AudioMIDIBlockDispatcher::MIDIDeviceID, const choc::midi::Message<StorageType>&);

    /// Adds an incoming MIDI event to the queue. This can be called from any thread.
    void addMIDIEvent (AudioMIDIBlockDispatcher::MIDIDeviceID, const void* data, uint32_t size);

    /// Adds a juce::MidiMessage to the queue. This can be called from any thread.
    template <typename JUCECompatibleMIDIMessage>
    void addMIDIEvent (AudioMIDIBlockDispatcher::MIDIDeviceID, const JUCECompatibleMIDIMessage&);

protected:
    //==============================================================================
    /// This is an abstract base class, so you don't construct one of them directly.
    /// To get one, use a concrete subclass like RtAudioMIDIPlayer or RenderingAudioMIDIPlayer.
    AudioMIDIPlayer (const AudioDeviceOptions&);

    std::vector<AudioMIDICallback*> callbacks;
    std::mutex callbackLock;
    AudioMIDIBlockDispatcher dispatcher;
    uint32_t prerollFrames = 0;

    virtual void start() = 0;
    virtual void stop() = 0;
    virtual void handleOutgoingMidiMessage (const void* data, uint32_t length) = 0;

    void updateSampleRate (uint32_t);
    void process (choc::buffer::ChannelArrayView<const float> input,
                  choc::buffer::ChannelArrayView<float> output, bool replaceOutput);
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

inline AudioMIDIPlayer::AudioMIDIPlayer (const AudioDeviceOptions& o) : options (o)
{
    // There seem to be a lot of devices which glitch as they start up, so this silent
    // preroll time gets us past that before the first block of real audio is sent.
    // Would be nice to know when it's actually needed, but hey..
    prerollFrames = 20000;

    dispatcher.setMidiOutputCallback ([this] (uint32_t, choc::midi::MessageView m)
    {
        if (auto len = m.length())
            handleOutgoingMidiMessage (m.data(), len);
    });

    dispatcher.reset (options.sampleRate);
    callbacks.reserve (16);
}

inline void AudioMIDIPlayer::addCallback (AudioMIDICallback& c)
{
    {
        const std::scoped_lock lock (callbackLock);

        if (std::find (callbacks.begin(), callbacks.end(), std::addressof (c)) != callbacks.end())
            return;
    }

    if (options.sampleRate != 0)
        c.sampleRateChanged (options.sampleRate);

    bool needToStart = false;

    {
        const std::scoped_lock lock (callbackLock);
        needToStart = callbacks.empty();
        callbacks.push_back (std::addressof (c));
    }

    if (needToStart)
        start();
}

inline void AudioMIDIPlayer::removeCallback (AudioMIDICallback& c)
{
    bool needToStop = false;

    {
        const std::scoped_lock lock (callbackLock);

        if (auto i = std::find (callbacks.begin(), callbacks.end(), std::addressof (c)); i != callbacks.end())
            callbacks.erase (i);

        needToStop = callbacks.empty();
    }

    if (needToStop)
        stop();
}

inline void AudioMIDIPlayer::updateSampleRate (uint32_t newRate)
{
    if (options.sampleRate != newRate)
    {
        options.sampleRate = newRate;

        if (newRate != 0)
        {
            const std::scoped_lock lock (callbackLock);

            for (auto c : callbacks)
                c->sampleRateChanged (newRate);

            dispatcher.reset (options.sampleRate);
        }

        if (deviceOptionsChanged)
            deviceOptionsChanged();
    }
}

inline void AudioMIDIPlayer::process (choc::buffer::ChannelArrayView<const float> input,
                                      choc::buffer::ChannelArrayView<float> output,
                                      bool replaceOutput)
{
    if (prerollFrames != 0)
    {
        prerollFrames -= std::min (prerollFrames, input.getNumFrames());

        if (replaceOutput)
            output.clear();

        return;
    }

    const std::scoped_lock lock (callbackLock);

    if (callbacks.empty())
    {
        if (replaceOutput)
            output.clear();

        return;
    }

    for (auto c : callbacks)
        c->startBlock();

    dispatcher.setAudioBuffers (input, output);

    dispatcher.processInChunks ([this, replaceOutput] (const AudioMIDIBlockDispatcher::Block& block)
    {
        bool replace = replaceOutput;

        for (auto c : callbacks)
        {
            c->processSubBlock (block, replace);
            replace = false;
        }
    });

    for (auto c : callbacks)
        c->endBlock();
}

inline void AudioMIDIPlayer::addMIDIEvent (AudioMIDIBlockDispatcher::MIDIDeviceID deviceID, const void* data, uint32_t size)
{
    dispatcher.addMIDIEvent (deviceID, data, size);
}

template <typename StorageType>
inline void AudioMIDIPlayer::addMIDIEvent (AudioMIDIBlockDispatcher::MIDIDeviceID deviceID, const choc::midi::Message<StorageType>& message)
{
    dispatcher.addMIDIEvent (deviceID, message);
}

template <typename JUCECompatibleMIDIMessage>
void AudioMIDIPlayer::addMIDIEvent (AudioMIDIBlockDispatcher::MIDIDeviceID deviceID, const JUCECompatibleMIDIMessage& message)
{
    dispatcher.addMIDIEvent (deviceID, message);
}

} // namespace choc::audio::io

#endif // CHOC_AUDIOMIDIPLAYER_HEADER_INCLUDED
