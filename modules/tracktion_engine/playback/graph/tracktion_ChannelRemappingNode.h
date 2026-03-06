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
