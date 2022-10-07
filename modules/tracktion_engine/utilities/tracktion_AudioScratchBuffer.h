/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion { inline namespace engine
{

/**
    An audio scratch buffer that has pooled storage.
    Create one of these on the stack to use as a temporary buffer.
*/
class AudioScratchBuffer
{
    struct BufferList;
    struct Buffer;
    Buffer* allocatedBuffer; // NB: keep this member first, as it needs to be initialised before buffer.

public:
    /** Creates a buffer for a given number of channels and samples. */
    AudioScratchBuffer (int numChans, int numSamples);

    /** Creates a buffer copying an existing AudioBuffer in to it. */
    AudioScratchBuffer (const juce::AudioBuffer<float>& bufferToCopy);

    /** Destructor. */
    ~AudioScratchBuffer() noexcept;

    /** The buffer to use. Don't reassign or resize this. */
    juce::AudioBuffer<float>& buffer;

    /** Initialises the internal buffer list. */
    static void initialise();

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioScratchBuffer)
};

}} // namespace tracktion { inline namespace engine
