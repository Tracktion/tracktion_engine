/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#pragma once

namespace tracktion { inline namespace engine
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
    bool addAntiDenormalisationNoise = false;           /**< Whether to add low level anti-denormalisation noise to the output. */
    bool includeBypassedPlugins = true;                 /**< If false, bypassed plugins will be completely ommited from the graph. */
    bool implicitlyIncludeSubmixChildTracks = true;     /**< If true, chid track in submixes will be included regardless of the allowedTracks param. Only relevent when forRendering is also true. */
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


}} // namespace tracktion { inline namespace engine
