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
