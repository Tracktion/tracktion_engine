/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
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

//==============================================================================
/** Describes a channel of a WaveInputDevice or WaveOutputDevice by specifying the
    channel index in the global device's audio buffer and the purpose as an
    AudioChannelSet::ChannelType e.g. left, right, leftSurround etc.
*/
struct ChannelIndex
{
    /** Creates a default, invalid ChannelIndex. */
    ChannelIndex() noexcept = default;

    /** Creates a ChannelIndex for a given index and a channel type. */
    ChannelIndex (int indexInDevice, juce::AudioChannelSet::ChannelType) noexcept;

    bool operator== (const ChannelIndex&) const;
    bool operator!= (const ChannelIndex&) const;

    int indexInDevice = -1; // Number of input or output in device
    juce::AudioChannelSet::ChannelType channel = juce::AudioChannelSet::unknown;
};

/** Creates a String description of the channels. Useful for debugging. */
juce::String createDescriptionOfChannels (const std::vector<ChannelIndex>&);

/** Creates an AudioChannelSet for a list of ChannelIndexes. */
juce::AudioChannelSet createChannelSet (const std::vector<ChannelIndex>&);

/** Returns the ChannelType for an abbreviated name.
    @see AudioChannelSet::getAbbreviatedChannelTypeName.
*/
juce::AudioChannelSet::ChannelType channelTypeFromAbbreviatedName (const juce::String&);

/** Creates an AudioChannelSet from a list of abbreviated channel names.
    E.g. "L R"
*/
juce::AudioChannelSet channelSetFromSpeakerArrangmentString (const juce::String&);


//==============================================================================
/** Describes a WaveDevice from which the WaveOutputDevice and WaveInputDevice lists will be built.
*/
struct WaveDeviceDescription
{
    /** Creates an invalid device description. */
    WaveDeviceDescription() = default;

    /** Creates a WaveDevieDescription from left and right channel indicies. */
    WaveDeviceDescription (const juce::String& nm, int l, int r, bool isEnabled)
        : name (nm), enabled (isEnabled)
    {
        if (l != -1) channels.push_back (ChannelIndex (l, juce::AudioChannelSet::left));
        if (r != -1) channels.push_back (ChannelIndex (r, juce::AudioChannelSet::right));
    }

    /** Creates a WaveDeviceDescription for a given set of channels. */
    WaveDeviceDescription (const juce::String& nm, const ChannelIndex* channels_, int numChannels_, bool isEnabled)
        : name (nm), channels (channels_, channels_ + numChannels_), enabled (isEnabled)
    {}

    bool operator== (const WaveDeviceDescription& other) const noexcept
    {
        return name == other.name && enabled == other.enabled && channels == other.channels;
    }

    juce::String name;
    std::vector<ChannelIndex> channels;
    bool enabled = true;
};

} // namespace tracktion_engine
