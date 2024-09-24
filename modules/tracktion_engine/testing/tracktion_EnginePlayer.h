/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#pragma once

#include "../../3rd_party/nanorange/tracktion_nanorange.hpp"
#include "../../tracktion_graph/utilities/tracktion_GlueCode.h"

///@internal
namespace tracktion::inline engine
{

#ifndef DOXYGEN
namespace test_utilities
{

//==============================================================================
//==============================================================================
class EnginePlayer
{
public:
    EnginePlayer (Engine& e, HostedAudioDeviceInterface::Parameters p)
        : engine (e), params (p), output (static_cast<size_t> (p.outputChannels))
    {
        audioIO.initialise (params);
        audioIO.prepareToPlay (params.sampleRate, params.blockSize);
        engine.getDeviceManager().dispatchPendingUpdates();
    }

    ~EnginePlayer()
    {
        engine.getDeviceManager().deviceManager.closeAudioDevice();
        engine.getDeviceManager().removeHostedAudioDeviceInterface();
        assert (! engine.getDeviceManager().isHostedAudioDeviceInterfaceInUse());
    }

    juce::AudioBuffer<float> process (std::integral auto numSamples)
    {
        juce::AudioBuffer<float> inputAudio (params.inputChannels, static_cast<int> (numSamples));
        inputAudio.clear();
        return process (inputAudio);
    }

    juce::AudioBuffer<float> process (juce::AudioBuffer<float>& inputAudio)
    {
        juce::MidiBuffer inputMidi;
        return process (inputAudio, inputMidi);
    }

    juce::AudioBuffer<float> process (juce::AudioBuffer<float>& inputAudio, juce::MidiBuffer& inputMidi)
    {
        assert (inputAudio.getNumChannels() == params.inputChannels);

        const auto inputBuffer = toBufferView (inputAudio);

        const auto totalNumFrames = inputBuffer.getNumFrames();
        choc::buffer::ChannelArrayBuffer<float> processBuffer (choc::buffer::Size::create (std::max (inputAudio.getNumChannels(), params.outputChannels), totalNumFrames));
        choc::buffer::copyIntersectionAndClearOutside (processBuffer, inputBuffer);

        for (choc::buffer::FrameCount startFrame = 0;;)
        {
            const auto numFramesThisTime = std::min (totalNumFrames - startFrame, static_cast<choc::buffer::FrameCount> (params.blockSize));
            auto subBlock = processBuffer.getFrameRange ({ startFrame, startFrame + numFramesThisTime });

            auto subAudioBuffer = toAudioBuffer (subBlock);
            audioIO.processBlock (subAudioBuffer, inputMidi);

            startFrame += numFramesThisTime;

            if (startFrame == totalNumFrames)
                break;
        }

        numSamplesProcessed += static_cast<int> (totalNumFrames);

        juce::AudioBuffer<float> outputBlock;
        outputBlock.makeCopyOf (toAudioBuffer (processBuffer.getChannelRange ({ 0, static_cast<choc::buffer::ChannelCount> (params.outputChannels) })));

        for (auto chan : nano::iota_view (static_cast<choc::buffer::ChannelCount> (0),
                                          static_cast<choc::buffer::ChannelCount> (output.size())))
        {
            auto srcChannelData = processBuffer.getChannel (chan).data.data;
            std::copy_n (srcChannelData, totalNumFrames, std::back_inserter (output[static_cast<size_t> (chan)]));
        }

        return outputBlock;
    }

    choc::buffer::ChannelArrayBuffer<float> getOutput() const
    {
        choc::buffer::ChannelArrayBuffer<float> destBuffer (choc::buffer::Size::create (output.size(), numSamplesProcessed));

        for (auto chan : nano::iota_view (static_cast<size_t> (0), output.size()))
            choc::buffer::copy (destBuffer.getChannel (static_cast<choc::buffer::ChannelCount> (chan)),
                                choc::buffer::createMonoView (output[chan].data(), numSamplesProcessed));

        return destBuffer;
    }

    HostedAudioDeviceInterface::Parameters getParams() const
    {
        return params;
    }

    Engine& engine;

private:
    HostedAudioDeviceInterface::Parameters params;
    HostedAudioDeviceInterface& audioIO { engine.getDeviceManager().getHostedAudioDeviceInterface() };
    std::vector<std::vector<float>> output;
    int numSamplesProcessed = 0;
};


//==============================================================================
//==============================================================================
///@internal
void waitForFileToBeMapped (const AudioFile&);

//==============================================================================
///@internal
inline void processLoopedBack (EnginePlayer& player, std::integral auto numFramesToProcess,
                               std::vector<AudioFile> filesToMap = {})
{
    for (auto& af : filesToMap)
        test_utilities::waitForFileToBeMapped (af);

    using namespace choc::buffer;
    const auto numInputChannels = static_cast<ChannelCount> (player.getParams().inputChannels);
    FrameCount startFrame = 0;
    ChannelArrayBuffer<float> scratchBuffer (choc::buffer::Size::create (numInputChannels, player.getParams().blockSize));
    scratchBuffer.clear();

    for (;;)
    {
        const auto numFramesLeft = static_cast<choc::buffer::FrameCount> (numFramesToProcess) - startFrame;

        if (numFramesLeft == 0)
            break;

        const auto numFramesThisTime = std::min (scratchBuffer.getNumFrames(), numFramesLeft);

        auto blockInput = scratchBuffer.getStart (numFramesThisTime);
        auto bufferInput = toAudioBuffer (blockInput);
        auto bufferOutput = player.process (bufferInput);
        copyIntersectionAndClearOutside (scratchBuffer, toBufferView (bufferOutput));

        startFrame += numFramesThisTime;
    }
}

//==============================================================================
///@internal
inline std::unique_ptr<EnginePlayer> createEnginePlayer (Edit& e, HostedAudioDeviceInterface::Parameters p,
                                                         std::vector<AudioFile> filesToMap = {})
{
    assert (e.getEditRole() == Edit::forEditing);
    auto player = std::make_unique<EnginePlayer>  (e.engine, std::move (p));

    e.dispatchPendingUpdatesSynchronously();
    e.getTransport().ensureContextAllocated();

    for (auto& af : filesToMap)
        test_utilities::waitForFileToBeMapped (af);

    e.getTransport().play (false);

    return player;
}

//==============================================================================
///@internal
inline void waitForFileToBeMapped (const AudioFile& af)
{
    using namespace std::literals;
    assert (af.engine);

    for (;;)
    {
        if (af.engine->getAudioFileManager().cache.hasMappedReader (af, 0))
            return;

        std::this_thread::sleep_for (100ms);
    }
}

}

} // namespace tracktion::inline engine
#endif
