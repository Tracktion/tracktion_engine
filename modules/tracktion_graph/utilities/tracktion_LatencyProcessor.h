/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#pragma once

namespace tracktion { inline namespace graph
{

//==============================================================================
//==============================================================================
struct LatencyProcessor
{
    LatencyProcessor() = default;
    
    /** Returns ture if the the sample rate, channels etc. are the same between the two objects. */
    bool hasSameConfigurationAs (const LatencyProcessor& o) const
    {
        return latencyNumSamples == o.latencyNumSamples
            && sampleRate == o.sampleRate
            && fifo.getNumChannels() == o.fifo.getNumChannels();
    }

    /** Returns ture if the the sample rate, channels etc. are those specified. */
    bool hasConfiguration (int numLatencySamples, double preparedSampleRate, int numberOfChannels) const
    {
        return latencyNumSamples == numLatencySamples
            && sampleRate == preparedSampleRate
            && fifo.getNumChannels() == (choc::buffer::ChannelCount) numberOfChannels;
    }

    int getLatencyNumSamples() const
    {
        return latencyNumSamples;
    }
    
    void setLatencyNumSamples (int numLatencySamples)
    {
        latencyNumSamples = numLatencySamples;
    }
    
    void prepareToPlay (double sampleRateToUse, int blockSize, int numChannels)
    {
        sampleRate = sampleRateToUse;
        latencyTimeSeconds = sampleToTime (latencyNumSamples, sampleRate);
        
        fifo.setSize ((choc::buffer::ChannelCount) numChannels, (choc::buffer::FrameCount) (latencyNumSamples + blockSize + 1));
        fifo.writeSilence ((choc::buffer::FrameCount) latencyNumSamples);
        jassert (fifo.getNumReady() == latencyNumSamples);
    }
    
    void writeAudio (choc::buffer::ChannelArrayView<float> src)
    {
        if (fifo.getNumChannels() == 0)
            return;

        jassert (fifo.getNumChannels() >= src.getNumChannels());
        fifo.write (src);
    }
    
    void writeMIDI (const tracktion_engine::MidiMessageArray& src)
    {
        midi.mergeFromWithOffset (src, latencyTimeSeconds);
    }

    void readAudioAdding (choc::buffer::ChannelArrayView<float> dst)
    {
        if (fifo.getNumChannels() == 0)
            return;

        jassert (fifo.getNumReady() >= (int) dst.getNumFrames());
        fifo.readAdding (dst);
    }

    void readAudioOverwriting (choc::buffer::ChannelArrayView<float> dst)
    {
        if (fifo.getNumChannels() == 0)
            return;

        jassert (fifo.getNumReady() >= (int) dst.getNumFrames());
        fifo.readOverwriting (dst);
    }

    void readMIDI (tracktion_engine::MidiMessageArray& dst, int numSamples)
    {
        // And read out any delayed items
        const double blockTimeSeconds = sampleToTime (numSamples, sampleRate);

        for (int i = midi.size(); --i >= 0;)
        {
           auto& m = midi[i];
           
           if (m.getTimeStamp() <= blockTimeSeconds)
           {
               dst.add (m);
               midi.remove (i);
               // TODO: This will deallocate, we need a MIDI message array that doesn't adjust its storage
           }
        }

        // Shuffle down remaining items by block time
        midi.addToTimestamps (-blockTimeSeconds);

        // Ensure there are no negative time messages
        for (auto& m : midi)
        {
           juce::ignoreUnused (m);
           jassert (m.getTimeStamp() >= 0.0);
        }
    }
    
    void clearAudio (int numSamples)
    {
        fifo.removeSamples (numSamples);
    }

    void clearMIDI (int numSamples)
    {
        // And read out any delayed items
        const double blockTimeSeconds = sampleToTime (numSamples, sampleRate);

        // TODO: This will deallocate, we need a MIDI message array that doesn't adjust its storage
        for (int i = midi.size(); --i >= 0;)
           if (midi[i].getTimeStamp() <= blockTimeSeconds)
               midi.remove (i);

        // Shuffle down remaining items by block time
        midi.addToTimestamps (-blockTimeSeconds);

        // Ensure there are no negative time messages
        for (auto& m : midi)
        {
           juce::ignoreUnused (m);
           jassert (m.getTimeStamp() >= 0.0);
        }
    }

private:
    int latencyNumSamples = 0;
    double sampleRate = 44100.0;
    double latencyTimeSeconds = 0.0;
    AudioFifo fifo { 1, 32 };
    tracktion_engine::MidiMessageArray midi;
};

}}
