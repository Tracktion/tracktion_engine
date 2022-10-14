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
/** Describes a channel of a WaveInputDevice or WaveOutputDevice by specifying the
    channel index in the global device's audio buffer and the purpose as an
    AudioChannelSet::ChannelType e.g. left, right, leftSurround etc.
*/
struct ChannelIndex
{
    /** Creates a default, invalid ChannelIndex. */
    ChannelIndex();

    /** Creates a ChannelIndex for a given index and a channel type. */
    ChannelIndex (int indexInDevice, juce::AudioChannelSet::ChannelType);

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
/** Describes a WaveDevice from which the WaveOutputDevice and
    WaveInputDevice lists will be built.
*/
struct WaveDeviceDescription
{
    /** Creates an invalid device description. */
    WaveDeviceDescription();

    /** Creates a WaveDevieDescription from left and right channel indicies. */
    WaveDeviceDescription (const juce::String& name, int leftChanIndex, int rightChanIndex, bool isEnabled);

    /** Creates a WaveDeviceDescription for a given set of channels. */
    WaveDeviceDescription (const juce::String& nm, const ChannelIndex* channels, int numChannels, bool isEnabled);

    bool operator== (const WaveDeviceDescription&) const;
    bool operator!= (const WaveDeviceDescription&) const;

    juce::String name;
    std::vector<ChannelIndex> channels;
    bool enabled = true;
};

}} // namespace tracktion { inline namespace engine
