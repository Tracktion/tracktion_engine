/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#pragma once

namespace tracktion::inline engine
{

class TrackMuteState;

//==============================================================================
/**
    Contains options for Edit Node content creation.
*/
struct CreateNodeParams
{
    ProcessState& processState;                         /**< The process state of the graph. */
    double sampleRate = 44100.0;                        /**< The sample rate to use. */
    int blockSize = 256;                                /**< The block size to use. */
    const juce::Array<Clip*>* allowedClips = nullptr;   /**< The clips to include. If nullptr, all clips will be included. */
    juce::Array<Track*>* allowedTracks = nullptr;       /**< The tracks to include. If nullptr, all tracks will be included. */
    bool forRendering = false;                          /**< If the node is for rendering or not. In renders, freeze files won't be used. */
    bool includePlugins = true;                         /**< Whether to include track plugins. */
    bool includeMasterPlugins = true;                   /**< Whether to include master plugins, fades and volume. */
    bool includeBypassedPlugins = true;                 /**< If false, bypassed plugins will be completely ommited from the graph. */
    bool implicitlyIncludeSubmixChildTracks = true;     /**< If true, child track in submixes will be included regardless of the allowedTracks param. Only relevent when forRendering is also true. */
    juce::Array<EditItemID> tracksToProcessWhileMuted = {};  /**< Tracks whose contents keep processing while the track is muted, so they can feed sidechains, aux buses and racks without being heard. Used by stem renders. @see Renderer::Parameters::tracksToProcessWhileMuted */
    bool allowClipSlots = true;                         /**< If true, track's clip slots will be included, set to false to disable these (which will use a slightly more efficient Node). */
    bool readAheadTimeStretchNodes = false;             /**< TEMPORARY: If true, real-time time-stretch Nodes will use a larger buffer and background thread to reduce audio CPU use. */

    /** If set, this will be called for each output device to give an opportunity to add an
        additional final Node. Unlike EditNodeBuilder::insertOptionalLastStageNode this is
        carried per graph build (set via EditPlaybackContext) so different Edits can each have
        their own callback. The callback receives the OutputDevice being built, the
        CreateNodeParams (for sample rate/block size/ProcessState) and the current output Node,
        and returns the Node to use (e.g. a transient generator Plugin summed into the output).
    */
    std::function<std::unique_ptr<graph::Node> (OutputDevice&,
                                                const CreateNodeParams&,
                                                std::unique_ptr<graph::Node>)> insertOptionalLastStageNodeForDevice = {};
};

//==============================================================================
struct EditNodeBuilder
{
    /** If set, this will be called to give an opportunity to add an additional final
        node which could be used to add copy-protection noise or similar.
    */
    static std::function<std::unique_ptr<graph::Node> (std::unique_ptr<tracktion::graph::Node>)> insertOptionalLastStageNode;
};

//==============================================================================
/** Creates a Node to play back an Edit with live inputs and outputs. */
std::unique_ptr<tracktion::graph::Node> createNodeForEdit (EditPlaybackContext&, std::atomic<double>& audibleTimeToUpdate, const CreateNodeParams&);

/** Creates a Node to render an Edit. */
std::unique_ptr<tracktion::graph::Node> createNodeForEdit (Edit&, const CreateNodeParams&);

/** Wraps a Plugin as a generator Node (driven by a silent input) and sums it into the supplied
    output Node. Intended to be returned from CreateNodeParams::insertOptionalLastStageNodeForDevice
    to add a transient plugin (e.g. a browser-hosted generator) to a device's output. If the plugin
    is null the output Node is returned unchanged. */
std::unique_ptr<tracktion::graph::Node> createGeneratorPluginNode (const Plugin::Ptr&,
                                                                   const CreateNodeParams&,
                                                                   std::unique_ptr<tracktion::graph::Node> output);


} // namespace tracktion::inline engine
