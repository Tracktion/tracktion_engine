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

#ifndef CHOC_RTAUDIOPLAYER_HEADER_INCLUDED
#define CHOC_RTAUDIOPLAYER_HEADER_INCLUDED

#include <algorithm>
#include "choc_AudioMIDIPlayer.h"
#include "../../gui/choc_MessageLoop.h"
#include "../../text/choc_OpenSourceLicenseList.h"

#if CHOC_OSX
 #define __MACOSX_CORE__ 1
#elif CHOC_WINDOWS
 #define __WINDOWS_WASAPI__ 1
 #define __WINDOWS_MM__ 1
#elif CHOC_LINUX
 #define __LINUX_ALSA__
 #define __UNIX_JACK__ 1
#endif

#define INCLUDED_VIA_CHOC_RTAUDIO 1
#include "../../platform/choc_DisableAllWarnings.h"
#define UNICODE
#include "rtaudio/RtAudio.cpp"
#undef UNICODE
#include "rtaudio/RtMidi.cpp"
#include "../../platform/choc_ReenableAllWarnings.h"
#undef INCLUDED_VIA_CHOC_RTAUDIO

namespace choc::audio::io
{

//==============================================================================
/**  An AudioMIDIPlayer implementation that uses the RtAudio library to
 *   provide audio and MIDI input/output.
 *
 *   Note that when you use this class, you will probably need to also link to
 *   some platform-specific libraries, such as CoreAudio on macOS, or WASAPI on
 *   Windows. See the RtAudio documentation for details.
 *
 *   Also note: You must only include this header in your project ONCE!
 *   The RtAudio code isn't designed to be included multiple times, and will
 *   lead to linker errors if you do so.
 */
struct RtAudioMIDIPlayer  : public AudioMIDIPlayer
{
    /// When constructed, you can call getLastError() to find out if
    /// anything went wrong when trying to open the requested devices.
    /// The logMessage function can be provided if you want to be given log
    /// messages about the state of devices being opened/closed.
    RtAudioMIDIPlayer (const AudioDeviceOptions& o,
                       std::function<void(const std::string&)> logMessage = {})
        : AudioMIDIPlayer (o),
          log (std::move (logMessage))
    {
        if (openAudio())
        {
            ensureAllMIDIDevicesOpen();
            deviceListCheckTimer = choc::messageloop::Timer (4000u, [this] { return checkDeviceList(); });
        }
        else
        {
            if (lastError.empty())
                lastError = "Failed to open audio device";
        }
    }

    ~RtAudioMIDIPlayer() override
    {
        close();
    }

    //==============================================================================
    std::vector<uint32_t> getAvailableSampleRates() override   { return availableSampleRates; }
    std::vector<uint32_t> getAvailableBlockSizes() override    { return { 16, 32, 48, 64, 96, 128, 196, 224, 256, 320, 480, 512, 768, 1024, 1536, 2048 }; }

    std::vector<std::string> getAvailableAudioAPIs() override
    {
        std::vector<std::string> result;
        std::vector<RtAudio::Api> apis;
        RtAudio::getCompiledApi (apis);

        for (auto& api : apis)
            result.push_back (RtAudio::getApiDisplayName (api));

        return result;
    }

    std::vector<AudioDeviceInfo> getAvailableInputDevices() override
    {
        std::vector<AudioDeviceInfo> result;

        for (auto& device : getAudioDeviceList())
            if (device.inputChannels != 0)
                result.push_back ({ std::to_string (device.ID), device.name });

        return result;
    }

    std::vector<AudioDeviceInfo> getAvailableOutputDevices() override
    {
        std::vector<AudioDeviceInfo> result;

        for (auto& device : getAudioDeviceList())
            if (device.outputChannels != 0)
                result.push_back ({ std::to_string (device.ID), device.name });

        return result;
    }

    std::vector<std::string> getAvailableMIDIInputDevices() override
    {
        std::vector<std::string> result;
        RtMidiIn m;
        m.setErrorCallback (rtMidiErrorCallback, this);

        auto numPorts = m.getPortCount();

        for (unsigned int i = 0; i < numPorts; ++i)
            result.push_back (m.getPortName (i));

        return result;
    }

    std::vector<std::string> getAvailableMIDIOutputDevices() override
    {
        std::vector<std::string> result;
        RtMidiOut m;
        m.setErrorCallback (rtMidiErrorCallback, this);

        auto numPorts = m.getPortCount();

        for (unsigned int i = 0; i < numPorts; ++i)
            result.push_back (m.getPortName (i));

        return result;
    }

    std::string getLastError() override
    {
        return lastError;
    }


private:
    //==============================================================================
    //==============================================================================
    struct NamedMIDIIn
    {
        RtAudioMIDIPlayer* owner = {};
        std::unique_ptr<RtMidiIn> midiIn;
        std::string name;
    };

    struct NamedMIDIOut
    {
        std::unique_ptr<RtMidiOut> midiOut;
        std::string name;
    };

    std::unique_ptr<RtAudio> rtAudio;
    std::vector<std::unique_ptr<NamedMIDIIn>> rtMidiIns;
    std::vector<NamedMIDIOut> rtMidiOuts;

    choc::buffer::ChannelCount numInputChannels = {}, numOutputChannels = {};
    std::vector<const float*> inputChannelPointers;
    std::vector<float*> outputChannelPointers;
    uint32_t xruns = 0;
    std::vector<uint32_t> availableSampleRates;
    choc::messageloop::Timer deviceListCheckTimer;
    std::function<void(const std::string&)> log;
    std::string lastError;

    void start() override {}
    void stop() override {}

    void handleAudioError (RtAudioErrorType, const std::string& errorText)
    {
        lastError = errorText;
    }

    void handleStreamUpdate()
    {
        updateSampleRate (static_cast<uint32_t> (rtAudio->getStreamSampleRate()));
    }

    void handleMIDIError (NamedMIDIIn& m, RtMidiError::Type type, const std::string& errorText)
    {
        lastError = "MIDI device error: " + m.name + ": " + errorText;
        (void) type;
    }

    uint32_t chooseBestSampleRate() const
    {
        auto preferredRate = options.sampleRate > 0 ? options.sampleRate : 44100u;

        if (options.sampleRate > 0)
            for (auto rate : availableSampleRates)
                if (rate == preferredRate)
                    return rate;

        for (auto rate : availableSampleRates)
            if (rate >= 44100u)
                return rate;

        if (! availableSampleRates.empty())
            return availableSampleRates.back();

        return 44100;
    }

    void close()
    {
        deviceListCheckTimer = {};
        stop();
        rtMidiIns.clear();
        rtMidiOuts.clear();
        closeAudio();
    }

    bool openAudio()
    {
        close();
        lastError = {};

        rtAudio = std::make_unique<RtAudio> (getAPIToUse(),
                                             [this] (RtAudioErrorType type, const std::string& errorText) { handleAudioError (type, errorText); },
                                             [this] () { handleStreamUpdate (); });

        auto devices = getAudioDeviceList();

        auto getDeviceForID = [&] (unsigned long defaultDeviceID, const std::string& requestedID, bool isInput) -> RtAudio::DeviceInfo*
        {
            if (! requestedID.empty())
            {
                for (auto& i : devices)
                    if (isInput ? (i.inputChannels > 0) : (i.outputChannels > 0))
                        if (std::to_string (i.ID) == requestedID)
                            return std::addressof (i);

                // If there's no such ID, try to find a matching name as a fallback
                for (auto& i : devices)
                    if (isInput ? (i.inputChannels > 0) : (i.outputChannels > 0))
                        if (i.name == requestedID)
                            return std::addressof (i);
            }

            for (auto& i : devices)
                if (i.ID == defaultDeviceID)
                    return std::addressof (i);

            return nullptr;
        };

        auto inputDeviceInfo  = getDeviceForID (rtAudio->getDefaultInputDevice(), options.inputDeviceID, true);
        auto outputDeviceInfo = getDeviceForID (rtAudio->getDefaultOutputDevice(), options.outputDeviceID, false);

        // If we found valid devices, clear any warnings from device enumeration, as these are
        // probably irrelevant to the device we're after.
        if (outputDeviceInfo != nullptr || inputDeviceInfo != nullptr)
            lastError = {};

        updateAvailableSampleRateList (inputDeviceInfo, outputDeviceInfo);

        options.outputDeviceID = outputDeviceInfo ? std::to_string (outputDeviceInfo->ID) : std::string();
        options.inputDeviceID  = inputDeviceInfo ? std::to_string (inputDeviceInfo->ID) : std::string();
        auto outputDeviceName = outputDeviceInfo ? outputDeviceInfo->name : std::string();
        auto inputDeviceName  = inputDeviceInfo ? inputDeviceInfo->name : std::string();

        RtAudio::StreamParameters inParams, outParams;

        if (options.inputChannelCount == 0)
        {
            inputDeviceInfo = nullptr;
        }
        else
        {
            if (inputDeviceInfo != nullptr)
            {
                numInputChannels = static_cast<choc::buffer::ChannelCount> (std::min (options.inputChannelCount,
                                                                                      inputDeviceInfo->inputChannels));
                inputChannelPointers.resize (numInputChannels);
                inParams.deviceId = inputDeviceInfo->ID;
                inParams.nChannels = static_cast<unsigned int> (numInputChannels);
                inParams.firstChannel = 0;
            }
        }

        if (options.outputChannelCount == 0)
        {
            outputDeviceInfo = nullptr;
        }
        else
        {
            CHOC_ASSERT (outputDeviceInfo != nullptr);
            numOutputChannels = static_cast<choc::buffer::ChannelCount> (std::min (options.outputChannelCount,
                                                                                   outputDeviceInfo->outputChannels));
            outputChannelPointers.resize (numOutputChannels);
            outParams.deviceId = outputDeviceInfo->ID;
            outParams.nChannels = static_cast<unsigned int> (numOutputChannels);
            outParams.firstChannel = 0;
        }

        auto framesPerBuffer = static_cast<unsigned int> (options.blockSize);

        if (framesPerBuffer == 0)
            framesPerBuffer = 128;

        RtAudio::StreamOptions streamOptions;
        streamOptions.flags = RTAUDIO_NONINTERLEAVED | RTAUDIO_SCHEDULE_REALTIME | RTAUDIO_ALSA_USE_DEFAULT;

        auto error = rtAudio->openStream (outputDeviceInfo != nullptr ? std::addressof (outParams) : nullptr,
                                          inputDeviceInfo != nullptr ? std::addressof (inParams) : nullptr,
                                          RTAUDIO_FLOAT32,
                                          (unsigned int) chooseBestSampleRate(),
                                          std::addressof (framesPerBuffer),
                                          rtAudioCallback,
                                          this, std::addressof (streamOptions));

        if (error != RTAUDIO_NO_ERROR)
        {
            if (lastError.empty())
                lastError = rtAudio->getErrorText();

            options.audioAPI = {};
            options.outputDeviceID = {};
            options.inputDeviceID = {};
            options.sampleRate = {};
            options.blockSize = {};
            options.inputChannelCount = {};
            options.outputChannelCount = {};

            return false;
        }

        options.audioAPI = RtAudio::getApiDisplayName (rtAudio->getCurrentApi());
        options.sampleRate = static_cast<uint32_t> (rtAudio->getStreamSampleRate());
        options.blockSize = static_cast<uint32_t> (framesPerBuffer);
        options.inputChannelCount = static_cast<uint32_t> (numInputChannels);
        options.outputChannelCount = static_cast<uint32_t> (numOutputChannels);

        ensureAllMIDIDevicesOpen();

        rtAudio->startStream();

        if (log)
            log ("Audio API: " + options.audioAPI
                  + ", Output device: " + outputDeviceName
                  + ", Input device: " + inputDeviceName
                  + ", Rate: " + std::to_string (options.sampleRate)
                  + "Hz, Block size: " + std::to_string (options.blockSize)
                  + " frames, Latency: " + std::to_string (rtAudio->getStreamLatency())
                  + " frames, Output channels: " + std::to_string (options.outputChannelCount)
                  + ", Input channels: " + std::to_string (options.inputChannelCount));

        return true;
    }

    void closeAudio()
    {
        if (rtAudio != nullptr)
        {
            rtAudio->closeStream();
            rtAudio.reset();
        }

        lastError = {};
        xruns = 0;
        numInputChannels = {};
        numOutputChannels = {};
        updateAvailableSampleRateList (nullptr, nullptr);
    }

    RtAudio::Api getAPIToUse() const
    {
        if (! options.audioAPI.empty())
        {
            std::vector<RtAudio::Api> apis;
            RtAudio::getCompiledApi (apis);

            for (auto api : apis)
                if (RtAudio::getApiDisplayName (api) == options.audioAPI)
                    return api;
        }

        return RtAudio::Api::UNSPECIFIED;
    }

    //==============================================================================
    void handleOutgoingMidiMessage (const void* data, uint32_t length) override
    {
        for (auto& out : rtMidiOuts)
            out.midiOut->sendMessage (static_cast<const unsigned char*> (data), length);
    }

    static int rtAudioCallback (void* output, void* input, unsigned int numFrames,
                                double streamTime, RtAudioStreamStatus status, void* userData)
    {
        return static_cast<RtAudioMIDIPlayer*> (userData)
                ->audioCallback (static_cast<float*> (output), static_cast<const float*> (input),
                                 static_cast<choc::buffer::FrameCount> (numFrames), streamTime, status);
    }

    static void rtMidiCallback (double, std::vector<unsigned char>* message, void* userData)
    {
        auto& m = *static_cast<NamedMIDIIn*> (userData);
        m.owner->midiCallback (m, message->data(), static_cast<uint32_t> (message->size()));
    }

    static void rtMidiErrorCallback (RtMidiError::Type type, const std::string& errorText, void* userData)
    {
        auto& m = *static_cast<NamedMIDIIn*> (userData);
        m.owner->handleMIDIError (m, type, errorText);
    }

    int audioCallback (float* output, const float* input, choc::buffer::FrameCount numFrames,
                       double streamTime, RtAudioStreamStatus status)
    {
        (void) streamTime;

        if ((status & RTAUDIO_INPUT_OVERFLOW) || (status & RTAUDIO_OUTPUT_UNDERFLOW))
            ++xruns;

        for (uint32_t i = 0; i < numInputChannels; ++i)
            inputChannelPointers[i] = input + numFrames * i;

        for (uint32_t i = 0; i < numOutputChannels; ++i)
            outputChannelPointers[i] = output + numFrames * i;

        auto inputView = choc::buffer::createChannelArrayView (inputChannelPointers.data(), numInputChannels, numFrames);
        auto outputView = choc::buffer::createChannelArrayView (outputChannelPointers.data(), numOutputChannels, numFrames);

        process (inputView, outputView, true);

        return 0;
    }

    void midiCallback (NamedMIDIIn& m, const void* data, uint32_t size)
    {
        addMIDIEvent (m.name.c_str(), data, size);
    }

    std::vector<RtAudio::DeviceInfo> getAudioDeviceList() const
    {
        std::vector<RtAudio::DeviceInfo> list;

        for (auto i : rtAudio->getDeviceIds())
            list.push_back (rtAudio->getDeviceInfo (i));

        return list;
    }

    bool checkDeviceList()
    {
        ensureAllMIDIDevicesOpen();
        return true;
    }

    void updateAvailableSampleRateList (RtAudio::DeviceInfo* inputDeviceInfo, RtAudio::DeviceInfo* outputDeviceInfo)
    {
        std::vector<unsigned int> rates;

        if (inputDeviceInfo != nullptr && outputDeviceInfo != nullptr)
        {
            auto inRates = inputDeviceInfo->sampleRates;
            auto outRates = outputDeviceInfo->sampleRates;
            std::sort (inRates.begin(), inRates.end());
            std::sort (outRates.begin(), outRates.end());

            std::set_intersection (inRates.begin(), inRates.end(),
                                   outRates.begin(), outRates.end(),
                                   std::back_inserter (rates));
        }
        else if (inputDeviceInfo != nullptr)
        {
            rates = inputDeviceInfo->sampleRates;
        }
        else if (outputDeviceInfo != nullptr)
        {
            rates = outputDeviceInfo->sampleRates;
        }

        std::sort (rates.begin(), rates.end());
        rates.erase (std::unique (rates.begin(), rates.end()), rates.end());

        if (rates.empty())
            availableSampleRates = { 44100, 48000 };
        else
            availableSampleRates = rates;
    }

    std::unique_ptr<NamedMIDIIn> openMIDIIn (unsigned int portNum)
    {
        static constexpr unsigned int queueSize = 512;

        auto m = std::make_unique<NamedMIDIIn>();
        m->owner = this;
        m->midiIn = std::make_unique<RtMidiIn> (RtMidi::Api::UNSPECIFIED, options.midiClientName, queueSize);
        m->midiIn->setCallback (rtMidiCallback, m.get());
        m->midiIn->setErrorCallback (rtMidiErrorCallback, m.get());
        m->midiIn->openPort (portNum, options.midiClientName + " Input");
        m->name = m->midiIn->getPortName (portNum);
        return m;
    }

    bool isMIDIInOpen (const std::string& name) const
    {
        for (auto& m : rtMidiIns)
            if (m->name == name)
                return true;

        return false;
    }

    NamedMIDIOut openMIDIOut (unsigned int portNum)
    {
        NamedMIDIOut m;
        m.midiOut = std::make_unique<RtMidiOut> (RtMidi::Api::UNSPECIFIED, options.midiClientName);
        m.midiOut->setErrorCallback (rtMidiErrorCallback, this);
        m.midiOut->openPort (portNum, options.midiClientName + " Input");
        m.name = m.midiOut->getPortName (portNum);
        return m;
    }

    bool isMIDIOutOpen (const std::string& name) const
    {
        for (auto& m : rtMidiOuts)
            if (m.name == name)
                return true;

        return false;
    }

    void ensureAllMIDIDevicesOpen()
    {
        ensureAllMIDIInputsOpen();
        ensureAllMIDIOutputsOpen();
    }

    void ensureAllMIDIInputsOpen()
    {
        try
        {
            std::vector<std::string> newInputs;

            for (auto& m : getAvailableMIDIInputDevices())
                if (! options.shouldOpenMIDIInput || options.shouldOpenMIDIInput (m))
                    newInputs.push_back (m);

            for (auto i = rtMidiIns.begin(); i != rtMidiIns.end();)
            {
                if (std::find (newInputs.begin(), newInputs.end(), (*i)->name) == newInputs.end())
                {
                    if (log)
                        log ("Closing MIDI input: " + (*i)->name);

                    i = rtMidiIns.erase(i);
                }
                else
                {
                    ++i;
                }
            }

            for (unsigned int i = 0; i < newInputs.size(); ++i)
            {
                if (! isMIDIInOpen (newInputs[i]))
                {
                    if (log)
                        log ("Opening MIDI input: " + newInputs[i]);

                    rtMidiIns.push_back (openMIDIIn (i));
                }
            }
        }
        catch (const RtMidiError& e)
        {
            e.printMessage();
        }
    }

    void ensureAllMIDIOutputsOpen()
    {
        try
        {
            std::vector<std::string> newOutputs;

            for (auto& m : getAvailableMIDIOutputDevices())
                if (! options.shouldOpenMIDIOutput || options.shouldOpenMIDIOutput (m))
                    newOutputs.push_back (m);

            for (auto i = rtMidiOuts.begin(); i != rtMidiOuts.end();)
            {
                if (std::find (newOutputs.begin(), newOutputs.end(), i->name) == newOutputs.end())
                {
                    if (log)
                        log ("Closing MIDI output: " + i->name);

                    i = rtMidiOuts.erase(i);
                }
                else
                {
                    ++i;
                }
            }

            for (unsigned int i = 0; i < newOutputs.size(); ++i)
            {
                if (! isMIDIOutOpen (newOutputs[i]))
                {
                    if (log)
                        log ("Opening MIDI output: " + newOutputs[i]);

                    rtMidiOuts.push_back (openMIDIOut (i));
                }
            }
        }
        catch (const RtMidiError& e)
        {
            e.printMessage();
        }
    }
};


CHOC_REGISTER_OPEN_SOURCE_LICENCE(RtAudio, R"(
==============================================================================
RtAudio license:

RtAudio provides a common API (Application Programming Interface)
for realtime audio input/output across Linux (native ALSA, Jack,
and OSS), Macintosh OS X (CoreAudio and Jack), and Windows
(DirectSound, ASIO and WASAPI) operating systems.

RtAudio GitHub site: https://github.com/thestk/rtaudio
RtAudio WWW site: http://www.music.mcgill.ca/~gary/rtaudio/

RtAudio: realtime audio i/o C++ classes
Copyright (c) 2001-2023 Gary P. Scavone

Permission is hereby granted, free of charge, to any person
obtaining a copy of this software and associated documentation files
(the "Software"), to deal in the Software without restriction,
including without limitation the rights to use, copy, modify, merge,
publish, distribute, sublicense, and/or sell copies of the Software,
and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

Any person wishing to distribute modifications to the Software is
asked to send the modifications to the original developer so that
they can be incorporated into the canonical version.  This is,
however, not a binding provision of this license.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR
ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
)")

CHOC_REGISTER_OPEN_SOURCE_LICENCE(RtMidi, R"(
==============================================================================
RtMidi license:

This class implements some common functionality for the realtime
MIDI input/output subclasses RtMidiIn and RtMidiOut.

RtMidi GitHub site: https://github.com/thestk/rtmidi
RtMidi WWW site: http://www.music.mcgill.ca/~gary/rtmidi/

RtMidi: realtime MIDI i/o C++ classes
Copyright (c) 2003-2023 Gary P. Scavone

Permission is hereby granted, free of charge, to any person
obtaining a copy of this software and associated documentation files
(the "Software"), to deal in the Software without restriction,
including without limitation the rights to use, copy, modify, merge,
publish, distribute, sublicense, and/or sell copies of the Software,
and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

Any person wishing to distribute modifications to the Software is
asked to send the modifications to the original developer so that
they can be incorporated into the canonical version.  This is,
however, not a binding provision of this license.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR
ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
)")

} // namespace choc::audio::io

#endif // CHOC_RTAUDIOPLAYER_HEADER_INCLUDED
