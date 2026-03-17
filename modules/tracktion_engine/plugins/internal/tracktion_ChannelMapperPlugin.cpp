/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion::inline engine {

static const juce::Identifier channelMapPropID ("channelMap");
static const juce::Identifier numInputsPropID ("numInputChannels");
static const juce::Identifier numOutputsPropID ("numOutputChannels");

//==============================================================================
const char* ChannelMapperPlugin::xmlTypeName ("channelMapper");

juce::ValueTree ChannelMapperPlugin::create()
{
    return createValueTree (IDs::PLUGIN,
                            IDs::type, xmlTypeName);
}

ChannelMapperPlugin::ChannelMapperPlugin (PluginCreationInfo info)
    : Plugin (info)
{
    auto um = getUndoManager();
    channelMapState.referTo (state, channelMapPropID, um);
    numInputChannels.referTo (state, numInputsPropID, um, 2);
    numOutputChannels.referTo (state, numOutputsPropID, um, 2);

    updateMapFromState();
}

ChannelMapperPlugin::~ChannelMapperPlugin()
{
    notifyListenersOfDeletion();
}

int ChannelMapperPlugin::getNumOutputChannelsGivenInputs (int)
{
    return numOutputChannels;
}

void ChannelMapperPlugin::initialise (const PluginInitialisationInfo&)
{
    updateMapFromState();
}

void ChannelMapperPlugin::deinitialise()
{
}

void ChannelMapperPlugin::applyToBuffer (const PluginRenderContext& rc)
{
    if (rc.destBuffer == nullptr)
        return;

    auto& dest = *rc.destBuffer;
    auto numSamples = rc.bufferNumSamples;
    auto startSample = rc.bufferStartSample;
    auto numDestChannels = dest.getNumChannels();

    if (currentMap.isEmpty() || numSamples == 0)
        return;

    // Work with a temporary copy to handle overlapping source/dest
    juce::AudioBuffer<float> temp (numDestChannels, numSamples);
    temp.clear();

    for (const auto& e : currentMap.entries)
    {
        if (e.source < numDestChannels && e.dest < numDestChannels)
            temp.addFrom (e.dest, 0, dest, e.source, startSample, numSamples, e.gain);
    }

    // Copy back
    for (int ch = 0; ch < numDestChannels; ++ch)
        dest.copyFrom (ch, startSample, temp, ch, 0, numSamples);
}

void ChannelMapperPlugin::restorePluginStateFromValueTree (const juce::ValueTree& v)
{
    copyPropertiesToCachedValues (v, channelMapState, numInputChannels, numOutputChannels);
    updateMapFromState();
}

//==============================================================================
ChannelMap ChannelMapperPlugin::getChannelMap() const
{
    return currentMap;
}

void ChannelMapperPlugin::setChannelMap (const ChannelMap& newMap)
{
    currentMap = newMap;

    // Serialize: "src1:dst1,src2:dst2,..."
    juce::String s;

    for (size_t i = 0; i < newMap.entries.size(); ++i)
    {
        if (i > 0)
            s << ",";

        s << newMap.entries[i].source << ":" << newMap.entries[i].dest;
    }

    channelMapState = s;
}

void ChannelMapperPlugin::setNumInputChannels (int n)
{
    numInputChannels = juce::jlimit (1, 8, n);
}

void ChannelMapperPlugin::setNumOutputChannels (int n)
{
    numOutputChannels = juce::jlimit (1, 8, n);
}

void ChannelMapperPlugin::updateMapFromState()
{
    currentMap = {};
    auto s = channelMapState.get();

    if (s.isEmpty())
    {
        // Default to identity mapping
        currentMap = ChannelMap::identity (juce::jmin (numInputChannels.get(), numOutputChannels.get()));
        return;
    }

    for (auto& token : juce::StringArray::fromTokens (s, ",", ""))
    {
        auto parts = juce::StringArray::fromTokens (token.trim(), ":", "");

        if (parts.size() == 2)
        {
            int src = parts[0].getIntValue();
            int dst = parts[1].getIntValue();
            currentMap.entries.push_back ({ src, dst });
        }
    }
}

} // namespace tracktion::inline engine
