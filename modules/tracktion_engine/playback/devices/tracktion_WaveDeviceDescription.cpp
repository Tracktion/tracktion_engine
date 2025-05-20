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
ChannelIndex::ChannelIndex() {}

ChannelIndex::ChannelIndex (int index, juce::AudioChannelSet::ChannelType c)
    : indexInDevice (index), channel (c)
{
}

ChannelIndex ChannelIndex::createMono (int indexInDevice)
{
    return ChannelIndex (indexInDevice, juce::AudioChannelSet::ChannelType (juce::AudioChannelSet::discreteChannel0 + indexInDevice));
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

static uint32_t countTotalSourceChannels (const std::vector<WaveDeviceDescription>& list)
{
    uint32_t total = 0;

    for (auto& device : list)
        for (auto& chan : device.channels)
            total = std::max (total, (uint32_t) chan.indexInDevice + 1);

    return total;
}

//==============================================================================
WaveDeviceDescription::WaveDeviceDescription() {}

WaveDeviceDescription::WaveDeviceDescription (const juce::String& nm, std::vector<ChannelIndex> chans, bool isEnabled)
    : name (nm), channels (std::move (chans)), enabled (isEnabled)
{
}

WaveDeviceDescription WaveDeviceDescription::withNumChannels (const juce::String& name, uint32_t firstChannelIndexInDevice, uint32_t numChannels, bool enabled)
{
    WaveDeviceDescription d;
    d.name = name;
    d.enabled = enabled;
    d.setNumChannels (firstChannelIndexInDevice, numChannels);
    return d;
}

void WaveDeviceDescription::setNumChannels (uint32_t newNumChannels)
{
    setNumChannels (getDeviceChannelRange().first, newNumChannels);
}

void WaveDeviceDescription::setNumChannels (uint32_t firstChannelIndexInDevice, uint32_t newNumChannels)
{
    channels.clear();

    if (newNumChannels == 2)
    {
        channels.push_back (ChannelIndex ((int) firstChannelIndexInDevice, juce::AudioChannelSet::left));
        channels.push_back (ChannelIndex ((int) firstChannelIndexInDevice + 1, juce::AudioChannelSet::right));
    }
    else if (newNumChannels > 0)
    {
        for (uint32_t i = 0; i < newNumChannels; ++i)
            channels.push_back (ChannelIndex::createMono ((int) (firstChannelIndexInDevice + i)));
    }
}

uint32_t WaveDeviceDescription::getNumChannels() const
{
    return static_cast<uint32_t> (channels.size());
}

std::pair<uint32_t, uint32_t> WaveDeviceDescription::getDeviceChannelRange() const
{
    uint32_t low = 0, high = 0, numCounted = 0;

    for (const auto& channel : channels)
    {
        if (channel.indexInDevice >= 0)
        {
            auto index = static_cast<uint32_t> (channel.indexInDevice);

            if (numCounted++ == 0)
            {
                low = index;
                high = low;
            }
            else
            {
                low = std::min (low, index);
                high = std::max (high, index);
            }
        }
    }

    if (numCounted == 0)
        return { 0, 0 };

    return { low, high + 1 };
}

bool WaveDeviceDescription::operator== (const WaveDeviceDescription& other) const
{
    return name == other.name && enabled == other.enabled && channels == other.channels;
}

bool WaveDeviceDescription::operator!= (const WaveDeviceDescription& other) const
{
    return ! operator== (other);
}

choc::value::Value WaveDeviceDescription::toJSON() const
{
    return choc::json::create ("enabled", enabled,
                               "channels", choc::value::createArray ((uint32_t) channels.size(),
                                               [&] (uint32_t index)
                                               {
                                                   auto& channel = channels[index];
                                                   return choc::json::create ("index", channel.indexInDevice,
                                                                              "type", juce::AudioChannelSet::getAbbreviatedChannelTypeName (channel.channel).toStdString());
                                               }));
}

WaveDeviceDescription WaveDeviceDescription::fromJSON (const choc::value::ValueView& json)
{
    std::vector<ChannelIndex> chans;

    for (const auto& channel : json["channels"])
    {
        auto index = channel["index"].getWithDefault<int> (-1);
        auto type = channel["type"].toString();
        chans.push_back (ChannelIndex (index, channelTypeFromAbbreviatedName (type)));
    }

    return { {}, std::move (chans), json["enabled"].getWithDefault<bool> (true) };
}

std::string WaveDeviceDescription::toString() const
{
    return choc::json::toString (toJSON());
}

WaveDeviceDescription WaveDeviceDescription::fromString (const std::string& s)
{
    return fromJSON (choc::json::parse (s));
}

//==============================================================================
WaveDeviceDescriptionList::WaveDeviceDescriptionList() = default;

static void describeStandardDevices (std::vector<WaveDeviceDescription>& descriptions, int totalChannelCount)
{
    for (int i = 0; i < totalChannelCount; ++i)
    {
        if (i + 1 < totalChannelCount)
        {
            descriptions.push_back (WaveDeviceDescription::withNumChannels ({}, (uint32_t) i, 2, true));
            ++i;
        }
        else
        {
            descriptions.push_back (WaveDeviceDescription::withNumChannels ({}, (uint32_t) i, 1, true));
        }
    }
}

bool WaveDeviceDescriptionList::initialiseFromCustomBehaviour (juce::AudioIODevice& device, EngineBehaviour& eb)
{
    if (eb.isDescriptionOfWaveDevicesSupported())
    {
        eb.describeWaveDevices (inputs, device, true);
        eb.describeWaveDevices (outputs, device, false);
        return true;
    }

    return false;
}

void WaveDeviceDescriptionList::initialiseFromDeviceDefault()
{
    describeStandardDevices (inputs, (int) deviceInputChannelNames.size());
    describeStandardDevices (outputs, (int) deviceOutputChannelNames.size());
}

bool WaveDeviceDescriptionList::initialiseFromLegacyState (const juce::XmlElement& xml)
{
    if (! xml.hasTagName ("AUDIODEVICE"))
        return false;

    juce::BigInteger outEnabled, inEnabled, outMonoChans, inStereoChans;
    outEnabled.setRange (0, 256, true);
    inEnabled.setRange (0, 256, true);

    outMonoChans.parseString (xml.getStringAttribute ("monoChansOut", outMonoChans.toString (2)), 2);
    inStereoChans.parseString (xml.getStringAttribute ("stereoChansIn", inStereoChans.toString (2)), 2);
    outEnabled.parseString (xml.getStringAttribute ("outEnabled", outEnabled.toString (2)), 2);
    inEnabled.parseString (xml.getStringAttribute ("inEnabled", inEnabled.toString (2)), 2);

    auto createDescriptions = [&] (std::vector<WaveDeviceDescription>& results, bool isInput)
    {
        auto isDeviceInChannelStereo   = [&] (int chan) { return inStereoChans[chan / 2]; };
        auto isDeviceOutChannelStereo  = [&] (int chan) { return ! outMonoChans[chan / 2]; };
        auto isDeviceInEnabled         = [&] (int chan) { return inEnabled[chan]; };
        auto isDeviceOutEnabled        = [&] (int chan) { return outEnabled[chan]; };
        auto isDeviceEnabled           = [&] (int chan) { return isInput ? isDeviceInEnabled (chan) : isDeviceOutEnabled (chan); };

        for (int i = outEnabled.getHighestBit() + 2; --i >= 0;)
        {
            if (isDeviceOutChannelStereo (i))
            {
                const int chan = i & ~1;
                const bool en = outEnabled[chan] || outEnabled[chan + 1];
                outEnabled.setBit (chan, en);
                outEnabled.setBit (chan + 1, en);
            }
        }

        for (int i = inEnabled.getHighestBit() + 2; --i >= 0;)
        {
            if (isDeviceInChannelStereo (i))
            {
                const int chan = i & ~1;
                const bool en = inEnabled[chan] || inEnabled[chan + 1];
                inEnabled.setBit (chan, en);
                inEnabled.setBit (chan + 1, en);
            }
        }

        const auto totalNumChannels = (int) (isInput ? deviceInputChannelNames
                                                     : deviceOutputChannelNames).size();

        for (int i = 0; i < totalNumChannels; ++i)
        {
            const bool canBeStereo = i < totalNumChannels - 1;

            if (canBeStereo && (isInput ? isDeviceInChannelStereo (i)
                                        : isDeviceOutChannelStereo (i)))
            {
                results.push_back (WaveDeviceDescription::withNumChannels ({}, (uint32_t) i, 2, isDeviceEnabled (i) || isDeviceEnabled (i + 1)));
                ++i;
            }
            else
            {
                results.push_back (WaveDeviceDescription::withNumChannels ({}, (uint32_t) i, 1, isDeviceEnabled (i)));
                ++i;

                if (i < totalNumChannels)
                    results.push_back (WaveDeviceDescription::withNumChannels ({}, (uint32_t) i, 1, isDeviceEnabled (i)));
            }
        }
    };

    createDescriptions (inputs, true);
    createDescriptions (outputs, false);
    return true;
}

bool WaveDeviceDescriptionList::initialiseFromState (const juce::XmlElement& xml)
{
    if (xml.hasTagName ("AUDIODEVICE_LAYOUT"))
    {
        try
        {
            auto state = xml.getStringAttribute ("state").toStdString();
            auto json = choc::json::parse (state);

            if (json.isObject())
            {
                inputs.clear();
                outputs.clear();

                for (const auto& input : json["inputs"])
                    inputs.push_back (WaveDeviceDescription::fromJSON (input));

                for (const auto& output : json["outputs"])
                    outputs.push_back (WaveDeviceDescription::fromJSON (output));

                sanityCheckList();
                return true;
            }
        }
        catch (const choc::json::ParseError& e)
        {
            TRACKTION_LOG ("WaveDeviceDescriptionList::initialiseFromState: " + std::string (e.what()));
        }
    }

    return false;
}

choc::value::Value WaveDeviceDescriptionList::toJSON() const
{
    return choc::json::create ("inputs",  choc::value::createArray ((uint32_t) inputs.size(),  [&] (uint32_t i) { return inputs[i].toJSON(); }),
                               "outputs", choc::value::createArray ((uint32_t) outputs.size(), [&] (uint32_t i) { return outputs[i].toJSON(); }));
}

juce::XmlElement WaveDeviceDescriptionList::toXML() const
{
    juce::XmlElement xml ("AUDIODEVICE_LAYOUT");
    xml.setAttribute ("state", choc::json::toString (toJSON()));
    return xml;
}

void WaveDeviceDescriptionList::initialise (Engine& engine, juce::AudioIODevice& device, const juce::XmlElement* state)
{
    deviceName = device.getName();
    deviceInputChannelNames = device.getInputChannelNames();
    deviceOutputChannelNames = device.getOutputChannelNames();

    [&]
    {
        if (initialiseFromCustomBehaviour (device, engine.getEngineBehaviour()))
            return;

        if (state != nullptr)
        {
            if (initialiseFromLegacyState (*state))
                return;

            if (initialiseFromState (*state))
                return;
        }

        initialiseFromDeviceDefault();
    }();

    sanityCheckList();
}

bool WaveDeviceDescriptionList::updateForDevice (juce::AudioIODevice& device)
{
    if (device.getName() != deviceName)
        return false;

    auto inputChannelNames  = device.getInputChannelNames();
    auto outputChannelNames = device.getOutputChannelNames();

    if (inputChannelNames.size() != deviceInputChannelNames.size()
         || outputChannelNames.size() != deviceOutputChannelNames.size())
        return false;

    deviceInputChannelNames = inputChannelNames;
    deviceOutputChannelNames = outputChannelNames;

    sanityCheckList();
    return true;
}

static WaveDeviceDescription* findMatchingDeviceInList (std::vector<WaveDeviceDescription>& list, const WaveDeviceDescription& d)
{
    for (auto& desc : list)
        for (auto& chan : desc.channels)
            if ((uint32_t) chan.indexInDevice >= d.getDeviceChannelRange().first
                  && (uint32_t) chan.indexInDevice < d.getDeviceChannelRange().second)
                 return std::addressof (desc);

    return nullptr;
}

WaveDeviceDescription* WaveDeviceDescriptionList::findMatchingDevice (const WaveDeviceDescription& d, bool isInput)
{
    return findMatchingDeviceInList (isInput ? inputs : outputs, d);
}

static void ensureFullChannelCoverage (std::vector<WaveDeviceDescription>& groups, uint32_t targetNumChannels)
{
    if (targetNumChannels == 0)
    {
        groups.clear();
        return;
    }

    for (auto& group : groups)
    {
        auto range = group.getDeviceChannelRange();

        if (range.second - range.first != group.getNumChannels())
            group.setNumChannels (range.first, group.getNumChannels());
    }

    auto fixFirstNonContiguousGroup = [&]() -> bool
    {
        uint32_t previousEnd = 0;

        for (auto group = groups.begin(); group != groups.end(); ++group)
        {
            auto range = group->getDeviceChannelRange();
            auto start = range.first;
            auto end = range.second;

            if (end > targetNumChannels)
            {
                if (targetNumChannels > start)
                    group->setNumChannels (start, targetNumChannels - start);
                else
                    groups.erase (group);

                return true;
            }

            if (start < previousEnd)
            {
                if (end > previousEnd)
                    group->setNumChannels (previousEnd, end - previousEnd);
                else
                    groups.erase (group);

                return true;
            }

            if (start > previousEnd)
            {
                uint32_t numToInsert = 1;

                if (start - previousEnd > 1 && (previousEnd & 1) == 0)
                    numToInsert = 2;

                groups.insert (group, WaveDeviceDescription::withNumChannels ({}, previousEnd, numToInsert, true));
                return true;
            }

            previousEnd = range.second;
        }

        if (targetNumChannels > previousEnd)
        {
            uint32_t numToInsert = 1;

            if (targetNumChannels - previousEnd > 1 && (previousEnd & 1) == 0)
                numToInsert = 2;

            groups.push_back (WaveDeviceDescription::withNumChannels ({}, previousEnd, numToInsert, true));
            return true;
        }

        return false;
    };

    while (fixFirstNonContiguousGroup())
    {}
}

static juce::String mergeChannelNames (const juce::StringArray& names)
{
    juce::String commonPrefix;
    juce::StringArray suffixes;

    for (auto& name : names)
    {
        juce::String prefix, suffix;

        if (name.containsChar ('(') && name.trim().endsWithChar (')'))
        {
            prefix = name.upToLastOccurrenceOf ("(", false, false).trim();
            suffix = name.fromLastOccurrenceOf ("(", false, false)
                         .upToFirstOccurrenceOf (")", false, false).trim();
        }
        else
        {
            for (int i = name.length(); --i >= 0;)
            {
                if (juce::CharacterFunctions::isDigit (name[i]))
                    suffix = juce::String::charToString (name[i]) + suffix;
                else
                    break;
            }

            prefix = name.dropLastCharacters (suffix.length()).trim();
        }

        if (prefix.isNotEmpty() && suffix.isNotEmpty())
        {
            if (commonPrefix.isEmpty() || commonPrefix == prefix)
            {
                commonPrefix = prefix;
                suffixes.add (suffix);
                continue;
            }
        }

        return names.joinIntoString (" + ");
    }

    return commonPrefix + " " + suffixes.joinIntoString (" + ");
}

static juce::String getDefaultChannelName (bool isInput, uint32_t index)
{
    return juce::String (isInput ? TRANS("Input 123") : TRANS("Output 123"))
            .replace ("123", std::to_string (index + 1));
}

static void replaceBareNumbersWithNames (juce::StringArray& channelNames, bool isInput)
{
    for (int i = 0; i < channelNames.size(); ++i)
        if (channelNames[i].trim() != juce::String (i + 1))
            return;

    for (int i = 0; i < channelNames.size(); ++i)
        if (channelNames[i].trim() == juce::String (i + 1))
            channelNames.set (i, getDefaultChannelName (isInput, (uint32_t) i));
}

static void refreshNamesInList (std::vector<WaveDeviceDescription>& descriptions, juce::StringArray channelNames, bool isInput)
{
    replaceBareNumbersWithNames (channelNames, isInput);

    for (auto& desc : descriptions)
    {
        juce::StringArray names;

        for (size_t i = 0; i < desc.channels.size(); ++i)
            if (desc.channels[i].indexInDevice >= 0)
                names.add (channelNames[desc.channels[i].indexInDevice]);

        desc.name = mergeChannelNames (names);

        std::sort (desc.channels.begin(), desc.channels.end(),
                   [] (const ChannelIndex& a, const ChannelIndex& b) { return a.indexInDevice < b.indexInDevice; });
    }
}

void WaveDeviceDescriptionList::sanityCheckList()
{
    ensureFullChannelCoverage (inputs,  (uint32_t) deviceInputChannelNames.size());
    ensureFullChannelCoverage (outputs, (uint32_t) deviceOutputChannelNames.size());

    refreshNamesInList (inputs, deviceInputChannelNames, true);
    refreshNamesInList (outputs, deviceOutputChannelNames, false);
}

uint32_t WaveDeviceDescriptionList::getTotalInputChannelCount() const  { return countTotalSourceChannels (inputs); }
uint32_t WaveDeviceDescriptionList::getTotalOutputChannelCount() const { return countTotalSourceChannels (outputs); }

bool WaveDeviceDescriptionList::setAllToNumChannels (std::vector<WaveDeviceDescription>& devices, uint32_t numChannels, bool isInput)
{
    auto setFirst = [&]
    {
        for (auto& device : devices)
            if (device.getNumChannels() != numChannels)
                return setChannelCountInDevice (device, isInput, numChannels);

        return false;
    };

    bool anyChanged = false;

    while (setFirst())
        anyChanged = true;

    return anyChanged;
}

bool WaveDeviceDescriptionList::setAllInputsToNumChannels (uint32_t numChannels)  { return setAllToNumChannels (inputs, numChannels, true); }
bool WaveDeviceDescriptionList::setAllOutputsToNumChannels (uint32_t numChannels) { return setAllToNumChannels (outputs, numChannels, false); }

bool WaveDeviceDescriptionList::setChannelCountInDevice (const WaveDeviceDescription& d, bool isInput, uint32_t newNumChannels)
{
    if (auto desc = findMatchingDevice (d, isInput))
    {
        desc->setNumChannels (newNumChannels);
        sanityCheckList();
        return true;
    }

    jassertfalse; // The description passed in didn't come from this list (or is an out-of-date version)
    return false;
}

void WaveDeviceDescriptionList::setDeviceEnabled (const WaveDeviceDescription& d, bool isInput, bool enabled)
{
    if (auto desc = findMatchingDevice (d, isInput))
    {
        desc->enabled = enabled;
        return;
    }

    jassertfalse; // The description passed in didn't come from this list (or is an out-of-date version)
}

juce::StringArray WaveDeviceDescriptionList::getPossibleChannelGroupsForDevice (const WaveDeviceDescription& d, uint32_t maxNumChannels, bool isInput) const
{
    juce::StringArray results;

    auto& names = isInput ? deviceInputChannelNames : deviceOutputChannelNames;

    for (uint32_t num = 1; num <= maxNumChannels; ++num)
    {
        auto listCopy = isInput ? inputs : outputs;

        if (auto desc = findMatchingDeviceInList (listCopy, d))
        {
            desc->setNumChannels (num);
            ensureFullChannelCoverage (listCopy, (uint32_t) names.size());
            refreshNamesInList (listCopy, names, isInput);
        }
        else
        {
            jassertfalse; // The description passed in didn't come from this list (or is an out-of-date version)
            break;
        }

        if (auto desc = findMatchingDeviceInList (listCopy, d))
        {
            if (desc->getNumChannels() != num)
                break;

            results.add (desc->name);
        }
        else
        {
            jassertfalse; // The description passed in didn't come from this list (or is an out-of-date version)
            break;
        }
    }

    return results;
}


}} // namespace tracktion { inline namespace engine
