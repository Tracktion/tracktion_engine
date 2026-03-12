/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#pragma once

namespace tracktion { inline namespace engine
{

//==============================================================================
/// Represents a mapping from source channels to destination channels.
///
/// Each entry maps a source channel index to a destination channel index.
/// Multiple sources can map to the same destination (they will be summed).
/// A single source can map to multiple destinations (it will be duplicated).
struct ChannelMap
{
    /// Creates an empty channel map.
    ChannelMap() = default;

    /// Creates a channel map from a vector of source->dest pairs.
    explicit ChannelMap (std::vector<std::pair<int, int>> mappings)
        : entries (std::move (mappings)) {}

    /// Creates an identity mapping for the given number of channels.
    static ChannelMap identity (int numChannels)
    {
        ChannelMap map;

        for (int i = 0; i < numChannels; ++i)
            map.entries.push_back ({ i, i });

        return map;
    }

    /// Creates a mapping that duplicates a mono source to stereo.
    static ChannelMap monoToStereo()
    {
        return ChannelMap ({ { 0, 0 }, { 0, 1 } });
    }

    /// Creates a mapping that sums stereo to mono.
    static ChannelMap stereoToMono()
    {
        return ChannelMap ({ { 0, 0 }, { 1, 0 } });
    }

    /// Creates an appropriate conversion mapping between channel counts.
    /// Rules:
    ///   - Same count: identity mapping
    ///   - Mono to stereo: duplicate channel 0 to both outputs
    ///   - Stereo to mono: sum both channels to output 0
    ///   - General upmix: identity for existing channels, duplicate last to fill remaining
    ///   - General downmix: identity for channels that fit, sum remaining into last dest channel
    static ChannelMap conversion (int fromChannels, int toChannels)
    {
        if (fromChannels == toChannels)
            return identity (fromChannels);

        if (fromChannels == 1 && toChannels == 2)
            return monoToStereo();

        if (fromChannels == 2 && toChannels == 1)
            return stereoToMono();

        ChannelMap map;

        if (fromChannels < toChannels)
        {
            // Upmix: identity for existing, duplicate last channel to fill
            for (int i = 0; i < fromChannels; ++i)
                map.entries.push_back ({ i, i });

            for (int i = fromChannels; i < toChannels; ++i)
                map.entries.push_back ({ fromChannels - 1, i });
        }
        else
        {
            // Downmix: identity for channels that fit, sum remaining into last dest
            for (int i = 0; i < toChannels; ++i)
                map.entries.push_back ({ i, i });

            for (int i = toChannels; i < fromChannels; ++i)
                map.entries.push_back ({ i, toChannels - 1 });
        }

        return map;
    }

    /// Creates a mapping that duplicates a source to all destination channels.
    static ChannelMap duplicateToChannels (int sourceChannel, int numDestChannels)
    {
        ChannelMap map;

        for (int i = 0; i < numDestChannels; ++i)
            map.entries.push_back ({ sourceChannel, i });

        return map;
    }

    /// Returns true if the map is empty.
    bool isEmpty() const noexcept               { return entries.empty(); }

    /// Returns the number of mappings.
    size_t size() const noexcept                { return entries.size(); }

    /// Returns true if this is an identity mapping (each channel maps to itself).
    bool isIdentity() const noexcept
    {
        for (const auto& [src, dest] : entries)
            if (src != dest)
                return false;
        return true;
    }

    /// Returns the maximum destination channel index + 1 (i.e., the required output channel count).
    int getRequiredOutputChannels() const noexcept
    {
        int maxDest = 0;

        for (const auto& [src, dest] : entries)
            maxDest = std::max (maxDest, dest + 1);

        return maxDest;
    }

    /// The source -> destination mappings.
    std::vector<std::pair<int, int>> entries;
};

}} // namespace tracktion::inline engine
