/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
{

class AudioScratchBuffer
{
    struct BufferList;
    struct Buffer;
    Buffer* allocatedBuffer; // NB: keep this member first, as it needs to be initialised before buffer.

public:
    AudioScratchBuffer (int numChans, int numSamples);
    AudioScratchBuffer (const juce::AudioBuffer<float>& bufferToCopy);
    ~AudioScratchBuffer() noexcept;

    juce::AudioBuffer<float>& buffer;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioScratchBuffer)
};

} // namespace tracktion_engine
