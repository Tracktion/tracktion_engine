/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
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

    static ChannelIndex createMono (int indexInDevice);

    bool operator== (const ChannelIndex&) const;
    bool operator!= (const ChannelIndex&) const;

    int indexInDevice = -1; // Index of this channel in the device's full channel list
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
/** Describes a group of audio channels from a physical device, which are to be
    treated as a WaveInputDevice or WaveOutputDevice.
*/
struct WaveDeviceDescription
{
    /// Creates an invalid device description.
    WaveDeviceDescription();

    /// Creates a WaveDeviceDescription for a given set of channels.
    WaveDeviceDescription (const juce::String& name, std::vector<ChannelIndex>, bool isEnabled);

    /// Creates a canonical WaveDeviceDescription for a number of channels
    static WaveDeviceDescription withNumChannels (const juce::String& name, uint32_t firstChannelIndexInDevice,
                                                  uint32_t numChannels, bool enabled);

    choc::value::Value toJSON() const;
    static WaveDeviceDescription fromJSON (const choc::value::ValueView&);
    std::string toString() const;
    static WaveDeviceDescription fromString (const std::string&);

    void setNumChannels (uint32_t firstChannelIndexInDevice, uint32_t newNumChannels);
    void setNumChannels (uint32_t newNumChannels);
    uint32_t getNumChannels() const;
    std::pair<uint32_t, uint32_t> getDeviceChannelRange() const;

    bool operator== (const WaveDeviceDescription&) const;
    bool operator!= (const WaveDeviceDescription&) const;

    juce::String name;
    std::vector<ChannelIndex> channels;
    bool enabled = true;
};

//==============================================================================
struct WaveDeviceDescriptionList
{
    WaveDeviceDescriptionList();

    void initialise (Engine&, juce::AudioIODevice&, const juce::XmlElement*);
    bool updateForDevice (juce::AudioIODevice&);

    juce::XmlElement toXML() const;
    choc::value::Value toJSON() const;

    uint32_t getTotalInputChannelCount() const;
    uint32_t getTotalOutputChannelCount() const;

    bool setChannelCountInDevice (const WaveDeviceDescription&, bool isInput, uint32_t newNumChannels);
    void setDeviceEnabled (const WaveDeviceDescription&, bool isInput, bool enabled);
    bool setAllInputsToNumChannels (uint32_t numChannels);
    bool setAllOutputsToNumChannels (uint32_t numChannels);
    juce::StringArray getPossibleChannelGroupsForDevice (const WaveDeviceDescription&, uint32_t maxNumChannels, bool isInput) const;

    bool operator== (const WaveDeviceDescriptionList& other) const noexcept    { return deviceName == other.deviceName && inputs == other.inputs && outputs == other.outputs; }
    bool operator!= (const WaveDeviceDescriptionList& other) const noexcept    { return ! operator== (other); }

    juce::String deviceName;
    std::vector<WaveDeviceDescription> inputs, outputs;

private:
    juce::StringArray deviceInputChannelNames, deviceOutputChannelNames;

    bool initialiseFromCustomBehaviour (juce::AudioIODevice&, EngineBehaviour&);
    void initialiseFromDeviceDefault();
    bool initialiseFromState (const juce::XmlElement&);
    bool initialiseFromLegacyState (const juce::XmlElement&);
    bool setAllToNumChannels (std::vector<WaveDeviceDescription>&, uint32_t numChannels, bool isInput);
    void sanityCheckList();
    WaveDeviceDescription* findMatchingDevice (const WaveDeviceDescription&, bool isInput);
};


}} // namespace tracktion { inline namespace engine
