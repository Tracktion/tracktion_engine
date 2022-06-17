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
ChannelIndex::ChannelIndex() {}

ChannelIndex::ChannelIndex (int index, juce::AudioChannelSet::ChannelType c)
    : indexInDevice (index), channel (c)
{
}

bool ChannelIndex::operator== (const ChannelIndex& other) const  { return indexInDevice == other.indexInDevice && channel == other.channel; }
bool ChannelIndex::operator!= (const ChannelIndex& other) const  { return ! operator== (other); }

juce::String createDescriptionOfChannels (const std::vector<ChannelIndex>& channels)
{
    juce::MemoryOutputStream desc;
    bool isFirst = true;

    for (const auto& ci : channels)
    {
        if (! isFirst)
            desc << ", ";
        else
            isFirst = false;

        desc << ci.indexInDevice << " (" << juce::AudioChannelSet::getAbbreviatedChannelTypeName (ci.channel) << ")";
    }

    return desc.toString();
}

juce::AudioChannelSet createChannelSet (const std::vector<ChannelIndex>& channels)
{
    juce::AudioChannelSet channelSet;

    for (const auto& ci : channels)
        channelSet.addChannel (ci.channel);

    return channelSet;
}

juce::AudioChannelSet::ChannelType channelTypeFromAbbreviatedName (const juce::String& abbreviatedName)
{
    struct NamedChannelTypeCache
    {
        NamedChannelTypeCache()
        {
            for (int i = 0; i < juce::AudioChannelSet::discreteChannel0; ++i)
            {
                const auto channelType = static_cast<juce::AudioChannelSet::ChannelType> (i);
                auto name = juce::AudioChannelSet::getAbbreviatedChannelTypeName (channelType);

                if (name.isNotEmpty())
                    map[name] = channelType;
            }
        }

        std::map<juce::String, juce::AudioChannelSet::ChannelType> map;
    };

    static NamedChannelTypeCache cache;
    const auto result = cache.map.find (abbreviatedName);

    if (result != cache.map.end())
        return result->second;

    return juce::AudioChannelSet::unknown;
}

juce::AudioChannelSet channelSetFromSpeakerArrangmentString (const juce::String& arrangement)
{
    juce::AudioChannelSet cs;

    for (auto& channel : juce::StringArray::fromTokens (arrangement, false))
    {
        const auto ct = channelTypeFromAbbreviatedName (channel);

        if (ct != juce::AudioChannelSet::unknown)
            cs.addChannel (ct);
    }

    return cs;
}

//==============================================================================
WaveDeviceDescription::WaveDeviceDescription() {}

WaveDeviceDescription::WaveDeviceDescription (const juce::String& nm, int l, int r, bool isEnabled)
    : name (nm), enabled (isEnabled)
{
    if (l >= 0) channels.push_back (ChannelIndex (l, juce::AudioChannelSet::left));
    if (r >= 0) channels.push_back (ChannelIndex (r, juce::AudioChannelSet::right));
}

WaveDeviceDescription::WaveDeviceDescription (const juce::String& nm, const ChannelIndex* chans, int numChans, bool isEnabled)
    : name (nm), channels (chans, chans + numChans), enabled (isEnabled)
{}

bool WaveDeviceDescription::operator== (const WaveDeviceDescription& other) const
{
    return name == other.name && enabled == other.enabled && channels == other.channels;
}

bool WaveDeviceDescription::operator!= (const WaveDeviceDescription& other) const
{
    return ! operator== (other);
}

}} // namespace tracktion { inline namespace engine
