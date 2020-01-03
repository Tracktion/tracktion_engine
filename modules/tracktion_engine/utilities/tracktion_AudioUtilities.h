/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion_engine
{

float dbToGain (float db) noexcept;
float gainToDb (float gain) noexcept;

// returns "n dB" and handles -INF
juce::String gainToDbString (float gain, float infLevel = -96.0f, int decPlaces = 2);
float dbStringToDb (const juce::String& dbStr);
float dbStringToGain (const juce::String& dbStr);

juce::String getPanString (float pan);

juce::String getSemitonesAsString (double semitones);

void sanitiseValues (juce::AudioBuffer<float>&,
                     int startSample, int numSamples,
                     float maxAbsValue,
                     float minAbsThreshold = 1.0f / 262144.0f);

void addAntiDenormalisationNoise (juce::AudioBuffer<float>&, int start, int numSamples) noexcept;

void resetFP() noexcept;
bool hasFloatingPointDenormaliseOccurred() noexcept;
void zeroDenormalisedValuesIfNeeded (juce::AudioBuffer<float>&) noexcept;

bool isAudioDataAlmostSilent (const float* data, int num);
float getAudioDataMagnitude (const float* data, int num);

void convertIntsToFloats (juce::AudioBuffer<float>&);
void convertFloatsToInts (juce::AudioBuffer<float>&);

inline void yieldGUIThread() noexcept
{
   #if JUCE_WINDOWS
    juce::Thread::yield();
   #endif
}

//==============================================================================
/** All laws have been designed to be equal-power, excluding linear respectively */
enum PanLaw
{
    PanLawDefault           = -1,
    PanLawLinear            = 0,
    PanLaw2point5dBCenter   = 1,   // NB: don't change these numbers, they're stored in the edit.
    PanLaw3dBCenter         = 2,
    PanLaw4point5dBCenter   = 3,
    PanLaw6dBCenter         = 4
};

PanLaw getDefaultPanLaw() noexcept;
void setDefaultPanLaw (PanLaw);
juce::StringArray getPanLawChoices (bool includeDefault) noexcept;

//==============================================================================
float decibelsToVolumeFaderPosition (float dB) noexcept;
float volumeFaderPositionToDB (float position) noexcept;
float volumeFaderPositionToGain (float position) noexcept;
float gainToVolumeFaderPosition (float gain) noexcept;

void getGainsFromVolumeFaderPositionAndPan (float volSliderPos, float pan, PanLaw lawToUse,
                                            float& leftGain, float& rightGain) noexcept;

//================================================================================================
class AudioMidiFifo
{
public:
    AudioMidiFifo (int channels = 2, int maxSize = 1024)
    {
        setSize (channels, maxSize);
    }

    void setSize (int channels, int maxSize)
    {
        fifo.setTotalSize (maxSize + 1);
        audioBuffer.setSize (channels, maxSize + 1);

        clear();
    }

    void clear()
    {
        fifo.reset();
        audioBuffer.clear();
        midiBuffer.clear();
    }

    int getNumSamplesAvailable()    { return fifo.getNumReady();    }
    int getNumSamplesFree()         { return fifo.getFreeSpace();   }

    void writeSilence (int numSamples)
    {
        jassert (getNumSamplesFree() >= numSamples);

        int start1, size1, start2, size2;
        fifo.prepareToWrite (numSamples, start1, size1, start2, size2);

        if (size1 > 0)
            audioBuffer.clear (start1, size1);
        if (size2 > 0)
            audioBuffer.clear (start2, size2);

        fifo.finishedWrite (size1 + size2);
    }

    void writeAudioAndMidi (const juce::AudioSampleBuffer& audioSrc, const juce::MidiBuffer& midiSrc)
    {
        jassert (getNumSamplesFree() >= audioSrc.getNumSamples());
        jassert (audioSrc.getNumChannels() == audioBuffer.getNumChannels());

        midiBuffer.clear();
        midiBuffer.addEvents (midiSrc, 0, audioSrc.getNumSamples(), fifo.getNumReady());

        int start1, size1, start2, size2;
        fifo.prepareToWrite (audioSrc.getNumSamples(), start1, size1, start2, size2);

        int channels = juce::jmin (audioBuffer.getNumChannels(), audioSrc.getNumChannels());
        for (int ch = 0; ch < channels; ch++)
        {
            if (size1 > 0)
                audioBuffer.copyFrom (ch, start1, audioSrc, ch, 0, size1);
            if (size2 > 0)
                audioBuffer.copyFrom (ch, start2, audioSrc, ch, size1, size2);
        }

        fifo.finishedWrite (size1 + size2);
    }

    void readAudioAndMidi (juce::AudioSampleBuffer& audioDst, juce::MidiBuffer& midiDst)
    {
        jassert (getNumSamplesAvailable() >= audioDst.getNumSamples());
        jassert (audioDst.getNumChannels() == audioBuffer.getNumChannels());

        midiDst.addEvents (midiBuffer, 0, audioDst.getNumSamples(), 0);

        // Move all the remaining midi events forward by the number of samples removed
        juce::MidiBuffer temp;
        temp.addEvents (midiBuffer, audioDst.getNumSamples(), fifo.getNumReady(), -audioDst.getNumSamples());
        midiBuffer = temp;

        int start1, size1, start2, size2;
        fifo.prepareToRead (audioDst.getNumSamples(), start1, size1, start2, size2);

        int numCh = juce::jmin (audioBuffer.getNumChannels(), audioDst.getNumChannels());
        for (int ch = 0; ch < numCh; ch++)
        {
            if (size1 > 0)
                audioDst.copyFrom (ch, 0, audioBuffer, ch, start1, size1);
            if (size2 > 0)
                audioDst.copyFrom (ch, size1, audioBuffer, ch, start2, size2);
        }

        fifo.finishedRead (size1 + size2);
    }

 private:
    juce::AbstractFifo fifo {1};
    juce::AudioSampleBuffer audioBuffer;
    juce::MidiBuffer midiBuffer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioMidiFifo)
};

//================================================================================================
// Takes a snapshot of a buffer state, and then case be used later
// to tell if the buffer has had it's channels reallocated
class AudioBufferSnapshot
{
public:
    AudioBufferSnapshot (juce::AudioSampleBuffer& b)
        : buffer (b)
    {
        numChannels = buffer.getNumChannels();
        numSamples = buffer.getNumSamples();

        auto readPointers = buffer.getArrayOfReadPointers();
        for (int i = 0; i < std::min (10, numChannels); i++)
            channels[i] = readPointers[i];
    }

    bool hasBufferBeenReallocated()
    {
        if (numChannels != buffer.getNumChannels()
            || numSamples != buffer.getNumSamples())
            return true;

        auto readPointers = buffer.getArrayOfReadPointers();
        for (int i = 0; i < std::min (10, numChannels); i++)
            if (channels[i] != readPointers[i])
                return true;

        return false;
    }

private:
    juce::AudioSampleBuffer& buffer;
    int numChannels = 0, numSamples = 0;
    const float* channels[10] = { nullptr }; // assume buffers have no more than 10 channels
};

inline void clearChannels (juce::AudioSampleBuffer& buffer, int startChannel, int endChannel = -1, int startSample = 0, int endSample = -1)
{
    if (endChannel == -1)
        endChannel = buffer.getNumChannels();
    if (endSample == -1)
        endSample = buffer.getNumSamples();

    for (int ch = startChannel; ch < endChannel; ch++)
        buffer.clear (ch, startSample, endSample);
}

} // namespace tracktion_engine
