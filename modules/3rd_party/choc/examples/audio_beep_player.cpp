//
//    ██████ ██   ██  ██████   ██████
//   ██      ██   ██ ██    ██ ██            ** Classy Header-Only Classes **
//   ██      ███████ ██    ██ ██
//   ██      ██   ██ ██    ██ ██           https://github.com/Tracktion/choc
//    ██████ ██   ██  ██████   ██████
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

#include <iostream>
#include <thread>
#include <chrono>
#include "../choc/audio/io/choc_RtAudioPlayer.h"
#include "../choc/audio/choc_Oscillators.h"
#include "../choc/gui/choc_MessageLoop.h"

/**
 * CHOC Audio Beep Player Example
 *
 * This example demonstrates how to use the RtAudioMIDIPlayer to play a simple beep sound
 * using a sine wave oscillator. It shows:
 *
 * - Setting up audio device options
 * - Creating an RtAudioMIDIPlayer instance
 * - Implementing an AudioMIDICallback to generate audio
 * - Using the choc::oscillator::Sine class to generate a sine wave
 * - Playing the beep for a specified duration
 *
 * The beep plays a 440Hz sine wave (A4 note) for 2 seconds.
 */

struct BeepGenerator    : public choc::audio::io::AudioMIDICallback
{
    BeepGenerator() : sineWave()
    {
        sineWave.setFrequency (440.0f, 44100.0f);
    }

    void sampleRateChanged (double newRate) override
    {
        std::cout << "Sample rate changed to: " << newRate << " Hz" << std::endl;
        sineWave.setFrequency (440.0f, static_cast<float> (newRate));
        maxSamples = static_cast<uint32_t> (2 * newRate);
    }

    void startBlock() override
    {
    }

    void processSubBlock (const choc::audio::AudioMIDIBlockDispatcher::Block& block,
                          bool replaceOutput) override
    {
        auto& output = block.audioOutput;

        if (! isPlaying)
        {
            if (replaceOutput)
                output.clear();

            return;
        }

        auto numFrames = output.getNumFrames();
        auto numChannels = output.getNumChannels();

        for (uint32_t frame = 0; frame < numFrames; ++frame)
        {
            if (samplesPlayed >= maxSamples)
            {
                isPlaying = false;

                for (uint32_t channel = 0; channel < numChannels; ++channel)
                    output.getSample (channel, frame) = 0.0f;

                continue;
            }

            auto sample = sineWave.getSample() * 0.1f;

            for (uint32_t channel = 0; channel < numChannels; ++channel)
            {
                if (replaceOutput)
                    output.getSample (channel, frame) = sample;
                else
                    output.getSample (channel, frame) += sample;
            }

            ++samplesPlayed;
        }
    }

    void endBlock() override
    {
    }

    bool isStillPlaying() const         { return isPlaying; }

private:
    choc::oscillator::Sine<float> sineWave;
    bool isPlaying = true;
    uint32_t samplesPlayed = 0;
    uint32_t maxSamples = 44100 * 2;
};

int main()
{
    std::cout << "CHOC Audio Beep Player Example" << std::endl;
    std::cout << "===============================" << std::endl;
    std::cout << "This example will play a 440Hz beep for 2 seconds using RtAudioMIDIPlayer." << std::endl;
    std::cout << std::endl;

    choc::audio::io::AudioDeviceOptions options;
    options.sampleRate = 44100;
    options.blockSize = 512;
    options.outputChannelCount = 2;
    options.inputChannelCount = 0;

    auto logMessage = [] (const std::string& message)
    {
        std::cout << "[Audio] " << message << std::endl;
    };

    choc::audio::io::RtAudioMIDIPlayer player (options, logMessage);

    auto error = player.getLastError();
    if (! error.empty())
    {
        std::cerr << "Error creating audio player: " << error << std::endl;
        return 1;
    }

    std::cout << "Audio setup complete:" << std::endl;
    std::cout << "  Sample rate: " << player.options.sampleRate << " Hz" << std::endl;
    std::cout << "  Block size: " << player.options.blockSize << " samples" << std::endl;
    std::cout << "  Output channels: " << player.options.outputChannelCount << std::endl;
    std::cout << std::endl;

    auto beepGenerator = std::make_unique<BeepGenerator>();
    player.addCallback (*beepGenerator);

    std::cout << "Playing beep (440Hz sine wave for 2 seconds)..." << std::endl;

    choc::messageloop::initialise();

    auto startTime = std::chrono::steady_clock::now();

    while (beepGenerator->isStillPlaying())
    {
        std::this_thread::sleep_for (std::chrono::milliseconds (50));

        auto elapsed = std::chrono::steady_clock::now() - startTime;

        if (elapsed > std::chrono::seconds (3))
        {
            std::cout << "Timeout reached, stopping..." << std::endl;
            break;
        }
    }

    std::cout << "Beep finished!" << std::endl;

    player.removeCallback (*beepGenerator);
    beepGenerator.reset();

    return 0;
}