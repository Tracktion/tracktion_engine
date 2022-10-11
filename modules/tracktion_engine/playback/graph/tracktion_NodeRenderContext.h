/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#pragma once

namespace tracktion { inline namespace engine
{

//==============================================================================
/**
    Holds the state of an audio render procedure so it can be rendered in blocks.
*/
class NodeRenderContext
{
public:
    /** Creates a context to render a Node. */
    NodeRenderContext (Renderer::RenderTask&, Renderer::Parameters&,
                       std::unique_ptr<tracktion::graph::Node>,
                       std::unique_ptr<tracktion::graph::PlayHead>,
                       std::unique_ptr<tracktion::graph::PlayHeadState>,
                       std::unique_ptr<ProcessState>,
                       juce::AudioFormatWriter::ThreadedWriter::IncomingDataReceiver* sourceToUpdate);
    
    /** Destructor. */
    ~NodeRenderContext();

    //==============================================================================
    /** Returns the opening status of the render.
        If something went wrong during set-up this will contain the error message to display.
    */
    juce::Result getStatus() const noexcept                { return status; }

    /** Renders the next block of audio. Returns true when finished, false if it needs to run again. */
    bool renderNextBlock (std::atomic<float>& progressToUpdate);

    //==============================================================================
    /** Renders the MIDI of an Edit to a sequence. */
    static juce::String renderMidi (Renderer::RenderTask&, Renderer::Parameters&,
                                    std::unique_ptr<tracktion::graph::Node>,
                                    std::unique_ptr<tracktion::graph::PlayHead>,
                                    std::unique_ptr<tracktion::graph::PlayHeadState>,
                                    std::unique_ptr<ProcessState>,
                                    std::atomic<float>& progressToUpdate);

private:
    //==============================================================================
    struct Ditherers
    {
        Ditherers (int num, int bitDepth)
        {
            while (ditherers.size() < num)
                ditherers.add ({});

            for (auto& d : ditherers)
                d.reset (bitDepth);
        }

        void apply (juce::AudioBuffer<float>& buffer, int numSamples)
        {
            auto numChannels = buffer.getNumChannels();

            for (int i = 0; i < numChannels; ++i)
                ditherers.getReference (i).process (buffer.getWritePointer (i), numSamples);
        }

        juce::Array<Ditherer> ditherers;
    };

    //==============================================================================
    Renderer::RenderTask& owner;
    Renderer::Parameters r, originalParams;
    bool needsToNormaliseAndTrim = false;
    
    std::unique_ptr<tracktion::graph::PlayHead> playHead;
    std::unique_ptr<tracktion::graph::PlayHeadState> playHeadState;
    std::unique_ptr<ProcessState> processState;
    std::unique_ptr<TracktionNodePlayer> nodePlayer;
    
    int numOutputChans = 0;
    std::unique_ptr<AudioFileWriter> writer;
    Plugin::Array plugins;
    juce::Result status;

    //==============================================================================
    Ditherers ditherers;
    MidiMessageArray midiBuffer;

    const float thresholdForStopping { dbToGain (-70.0f) };
    TimeDuration blockLength;
    int numPreRenderBlocks = 0;
    int numLatencySamplesToDrop = 0;
    int realTimePerBlock = 0;

    double lastTime = 0;
    static const int sleepCounterMax = 100;
    int sleepCounter = 0;

    std::unique_ptr<tempo::Sequence::Position> currentTempoPosition;
    float peak = 0;
    double rmsTotal = 0;
    int64_t rmsNumSamps = 0;
    int precount = 0;
    TimePosition streamTime;

    int64_t samplesTrimmed = 0;
    bool hasStartedSavingToFile = 0;
    int64_t samplesToWrite = 0, numSamplesWrittenToSource = 0;

    std::unique_ptr<juce::TemporaryFile> intermediateFile;
    juce::AudioFormatWriter::ThreadedWriter::IncomingDataReceiver* sourceToUpdate;

    //==============================================================================
    enum class WriteResult
    {
        succeeded,
        failed
    };
    
    WriteResult writeAudioBlock (choc::buffer::ChannelArrayView<float>);
};

}} // namespace tracktion { inline namespace engine
