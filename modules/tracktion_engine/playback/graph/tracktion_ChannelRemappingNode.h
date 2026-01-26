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


//==============================================================================
/// A Node that remaps audio channels from input to output.
///
/// This node supports two modes of operation:
///
/// 1. EXPLICIT MAPPING MODE:
///    Uses a ChannelMap to explicitly route source channels to destination channels.
///    - Multiple sources can map to the same destination (summed together)
///    - A single source can map to multiple destinations (duplicated)
///    - MIDI is always passed through
///
///    Example: Mono to stereo duplication:
///    @code
///    auto node = makeNode<ChannelRemappingNode> (input, ChannelMap::monoToStereo());
///    @endcode
///
/// 2. PROCESSOR PASSTHROUGH MODE:
///    Wraps a processor node (e.g., PluginNode) and handles channel count mismatches:
///
///    - PASSTHROUGH (input > processor): Channels beyond the processor's capacity
///      pass through unchanged from the original input.
///      Example: 5.1 through stereo plugin - L/R processed, C/LFE/Ls/Rs pass through.
///
///    - EXPANSION (input < processor): The processor outputs more channels than
///      the input provided. Output is the processor's channel count.
///      Example: Mono through stereo plugin - output is stereo.
///
///    @code
///    auto pluginNode = makeNode<PluginNode> (input, plugin, ...);
///    auto node = makeNode<ChannelRemappingNode> (pluginNode, inputConfig, processorConfig);
///    @endcode
class ChannelRemappingNode final : public tracktion::graph::Node
{
public:
    //==============================================================================
    /// Creates a ChannelRemappingNode in explicit mapping mode.
    ///
    /// @param inputNode    The input node to remap channels from.
    /// @param channelMap   The channel mapping to apply.
    ChannelRemappingNode (std::unique_ptr<tracktion::graph::Node> inputNode,
                          ChannelMap channelMap);

    /// Creates a ChannelRemappingNode in processor passthrough mode.
    ///
    /// @param processorNode    The processor node (e.g., PluginNode) that has already
    ///                         been created with its input. This node takes ownership.
    /// @param inputConfig      The channel configuration of the original input.
    /// @param processorConfig  The channel configuration the processor handles.
    ChannelRemappingNode (std::unique_ptr<tracktion::graph::Node> processorNode,
                          ChannelConfiguration inputConfig,
                          ChannelConfiguration processorConfig);

    //==============================================================================
    tracktion::graph::NodeProperties getNodeProperties() override;
    std::vector<Node*> getDirectInputNodes() override;
    bool isReadyToProcess() override;
    void prepareToPlay (const tracktion::graph::PlaybackInitialisationInfo&) override;
    void process (ProcessContext&) override;

private:
    //==============================================================================
    enum class Mode { ExplicitMapping, ProcessorPassthrough };

    Mode mode;
    std::unique_ptr<tracktion::graph::Node> input;

    // Explicit mapping mode
    ChannelMap channelMap;

    // Processor passthrough mode
    ChannelConfiguration inputChannelConfig;
    ChannelConfiguration processorChannelConfig;
    tracktion::graph::Node* originalInputNode = nullptr;

    //==============================================================================
    void processExplicitMapping (ProcessContext&);
    void processProcessorPassthrough (ProcessContext&);
};

}} // namespace tracktion { inline namespace engine
