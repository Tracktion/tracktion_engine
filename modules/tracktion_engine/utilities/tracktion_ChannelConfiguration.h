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
/// Describes a channel of audio by specifying the channel index in a device's
/// audio buffer and the purpose as an AudioChannelSet::ChannelType e.g. left,
/// right, leftSurround etc.
struct ChannelIndex
{
    /// Creates a default, invalid ChannelIndex.
    ChannelIndex();

    /// Creates a ChannelIndex for a given index and a channel type.
    ChannelIndex (int indexInDevice, juce::AudioChannelSet::ChannelType);

    /// Creates a discrete (mono) channel at the given index.
    static ChannelIndex createMono (int indexInDevice);

    bool operator== (const ChannelIndex&) const;
    bool operator!= (const ChannelIndex&) const;

    int indexInDevice = -1;
    juce::AudioChannelSet::ChannelType channel = juce::AudioChannelSet::unknown;
};

/// Returns the ChannelType for an abbreviated name.
/// @see AudioChannelSet::getAbbreviatedChannelTypeName.
juce::AudioChannelSet::ChannelType channelTypeFromAbbreviatedName (const juce::String&);

/// Creates an AudioChannelSet from a list of abbreviated channel names.
/// E.g. "L R"
juce::AudioChannelSet channelSetFromSpeakerArrangementString (const juce::String&);


//==============================================================================
/// Represents a configuration of audio channels.
///
/// This is a wrapper around a list of ChannelIndex objects, optimized to avoid
/// heap allocations for typical channel counts (up to 8 channels).
///
/// Use the factory methods to create common configurations:
/// @code
/// auto mono = ChannelConfiguration::mono();
/// auto stereo = ChannelConfiguration::stereo();
/// auto surround = ChannelConfiguration::surround5_1();
/// auto custom = ChannelConfiguration::discreteChannels (16);
/// @endcode
class ChannelConfiguration
{
public:
    //==============================================================================
    /// Creates an empty channel configuration.
    ChannelConfiguration() = default;

    /// Creates a channel configuration from a list of channel indices.
    explicit ChannelConfiguration (std::vector<ChannelIndex> channels);

    /// Creates a channel configuration from a span of channel indices.
    ChannelConfiguration (const ChannelIndex* channels, size_t numChannels);

    //==============================================================================
    // Factory methods for common configurations

    /// Creates a mono configuration with a single channel.
    static ChannelConfiguration mono (int deviceChannelIndex = 0);

    /// Creates a left-only configuration (single left channel).
    static ChannelConfiguration left (int deviceChannelIndex = 0);

    /// Creates a right-only configuration (single right channel).
    static ChannelConfiguration right (int deviceChannelIndex = 1);

    /// Creates a stereo configuration with left and right channels.
    static ChannelConfiguration stereo (int firstDeviceChannelIndex = 0);

    /// Creates a 5.1 surround configuration (L, R, C, LFE, Ls, Rs).
    static ChannelConfiguration surround5_1 (int firstDeviceChannelIndex = 0);

    /// Creates a 7.1 surround configuration (L, R, C, LFE, Ls, Rs, Lrs, Rrs).
    static ChannelConfiguration surround7_1 (int firstDeviceChannelIndex = 0);

    /// Creates a configuration with discrete (unnamed) channels.
    static ChannelConfiguration discreteChannels (int numChannels, int firstDeviceChannelIndex = 0);

    /// Creates a canonical configuration for a given number of channels.
    /// 1=mono, 2=stereo, 6=5.1, 8=7.1, otherwise discrete.
    static ChannelConfiguration canonical (int numChannels, int firstDeviceChannelIndex = 0);

    /// Creates a configuration from an AudioChannelSet and starting device index.
    static ChannelConfiguration fromChannelSet (const juce::AudioChannelSet&, int firstDeviceChannelIndex = 0);

    //==============================================================================
    // Accessors

    /// Returns true if this configuration has no channels.
    bool isEmpty() const noexcept                           { return channels.empty(); }

    /// Returns the number of channels in this configuration.
    size_t size() const noexcept                            { return channels.size(); }

    /// Returns the number of channels in this configuration.
    int getNumChannels() const noexcept                     { return static_cast<int> (channels.size()); }

    /// Returns a pointer to the channel data.
    const ChannelIndex* data() const noexcept               { return channels.data(); }

    /// Returns the channel at the given index.
    const ChannelIndex& operator[] (size_t index) const     { return channels[index]; }

    /// Returns an iterator to the beginning.
    auto begin() noexcept                                   { return channels.begin(); }
    auto begin() const noexcept                             { return channels.begin(); }

    /// Returns an iterator to the end.
    auto end() noexcept                                     { return channels.end(); }
    auto end() const noexcept                               { return channels.end(); }

    //==============================================================================
    // Queries

    /// Returns true if this is a mono configuration (single channel).
    bool isMono() const noexcept                            { return channels.size() == 1; }

    /// Returns true if this is a stereo configuration (exactly 2 channels, L+R).
    bool isStereo() const noexcept;

    /// Returns true if this configuration has 1 or 2 channels.
    bool isMonoOrStereo() const noexcept                    { return channels.size() <= 2; }

    /// Returns true if this configuration has more than 2 channels.
    bool isMultichannel() const noexcept                    { return channels.size() > 2; }

    /// Returns true if this configuration contains the given channel (exact match).
    bool contains (const ChannelIndex&) const noexcept;

    /// Returns true if this configuration contains a channel with the given device index.
    bool containsDeviceChannel (int deviceChannelIndex) const noexcept;

    /// Returns the range of device channel indices [first, one-past-last) used by this configuration.
    std::pair<int, int> getChannelRange() const;

    /// Returns a new configuration containing only channels whose device indices
    /// also appear in the other configuration.
    ChannelConfiguration intersection (const ChannelConfiguration& other) const;

    /// Creates a JUCE AudioChannelSet from this configuration.
    juce::AudioChannelSet toChannelSet() const;

    //==============================================================================
    // Modification

    /// Clears all channels from this configuration.
    void clear() noexcept                                   { channels.clear(); }

    /// Adds a channel to this configuration.
    void addChannel (ChannelIndex channel)                  { channels.push_back (channel); }

    /// Sets the number of channels, creating discrete channels starting from a given index.
    void setNumChannels (int numChannels, int firstDeviceChannelIndex = 0);

    //==============================================================================
    // Comparison

    bool operator== (const ChannelConfiguration& other) const;
    bool operator!= (const ChannelConfiguration& other) const   { return ! operator== (other); }

    //==============================================================================
    // Serialization

    /// Serializes this configuration to JSON.
    choc::value::Value toJSON() const;

    /// Creates a configuration from JSON.
    static ChannelConfiguration fromJSON (const choc::value::ValueView&);

    /// Serializes this configuration to a string.
    std::string toString() const;

    /// Creates a configuration from a string.
    static ChannelConfiguration fromString (std::string_view);

    /// Creates a human-readable description of the channels.
    juce::String getDescription() const;

    /// Returns the preset name that matches this configuration, or "From Edit" if no match.
    juce::String getMatchingPresetName() const;

private:
    //==============================================================================
    // Use SmallVector with capacity for 8 channels to avoid heap allocation
    // for common mono, stereo, and surround configurations
    choc::SmallVector<ChannelIndex, 8> channels;
};

//==============================================================================
/// A named channel configuration preset for UI selection.
struct ChannelConfigurationPreset
{
    juce::String name;
    ChannelConfiguration config;  ///< Empty config means "auto-detect from source"
};

/// Returns the standard presets for channel layout selection.
std::vector<ChannelConfigurationPreset> getChannelConfigurationPresets();

}} // namespace tracktion { inline namespace engine
