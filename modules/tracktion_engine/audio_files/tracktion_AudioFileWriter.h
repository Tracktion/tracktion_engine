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

//==============================================================================
/**
    Smart wrapper for writing to an audio file.
 
    Internally this opens a File for writng and provides some helper methods to
    append to it and free the file handle when done.
*/
class AudioFileWriter   : public juce::ReferenceCountedObject
{
public:
    AudioFileWriter (const AudioFile& file,
                     juce::AudioFormat* formatToUse,
                     int numChannels,
                     double sampleRate,
                     int bitsPerSample,
                     const juce::StringPairArray& metadata,
                     int quality);

    /** Destructor, calls closeForWriting. */
    ~AudioFileWriter();

    //==============================================================================
    /** Returns true if the file is open and ready to write to. */
    bool isOpen() const noexcept;
    
    /** Returns the sample rate of the writer, should only be called on an open writer. */
    double getSampleRate() const noexcept;
    
    /** Returns the num channels of the writer, should only be called on an open writer. */
    int getNumChannels() const noexcept;

    //==============================================================================
    /** Appends an AudioBuffer to the file. */
    bool appendBuffer (juce::AudioBuffer<float>& buffer, int numSamples);

    /** Appends an block of samples to the file. */
    bool appendBuffer (const int** buffer, int numSamples);

    /** Appends a block of samples to the file from an audio format reader. */
    bool writeFromAudioReader (juce::AudioFormatReader&, SampleCount startSample, SampleCount numSamples);

    /** Deletes the writer and releases the file handle. */
    void closeForWriting();

    AudioFile file;

private:
    int samplesUntilFlush;
    std::unique_ptr<juce::AudioFormatWriter> writer;
    juce::CriticalSection writerLock;
};

}} // namespace tracktion { inline namespace engine
