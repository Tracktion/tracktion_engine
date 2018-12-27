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


} // namespace tracktion_engine
