/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion::inline engine {

//==============================================================================
/// Describes a group of audio channels from a physical device, which are to be
/// treated as a WaveInputDevice or WaveOutputDevice.
struct WaveDeviceDescription
{
    /// Creates an invalid device description.
    WaveDeviceDescription();

    /// Creates a WaveDeviceDescription for a given set of channels.
    WaveDeviceDescription (const juce::String& name, ChannelConfiguration, bool isEnabled);

    /// Creates a canonical WaveDeviceDescription for a number of channels
    static WaveDeviceDescription withNumChannels (const juce::String& name, uint32_t firstChannelIndexInDevice,
                                                  uint32_t numChannels, bool enabled);

    choc::value::Value toJSON() const;
    static WaveDeviceDescription fromJSON (const choc::value::ValueView&);
    std::string toString() const;
    static WaveDeviceDescription fromString (const std::string&);

    void setNumChannels (uint32_t firstChannelIndexInDevice, uint32_t newNumChannels);
    void setNumChannels (uint32_t newNumChannels);
    void setChannelConfiguration (ChannelConfiguration newChannels);
    uint32_t getNumChannels() const;
    std::pair<uint32_t, uint32_t> getDeviceChannelRange() const;

    bool operator== (const WaveDeviceDescription&) const;
    bool operator!= (const WaveDeviceDescription&) const;

    juce::String name;
    ChannelConfiguration channels;
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
    bool setAllToNumChannels (std::vector<WaveDeviceDescription>&, uint32_t numChannels);
    void sanityCheckList();
    WaveDeviceDescription* findMatchingDevice (const WaveDeviceDescription&, bool isInput);
};


} // namespace tracktion::inline engine
