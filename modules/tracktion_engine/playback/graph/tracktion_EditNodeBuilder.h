/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#pragma once

namespace tracktion_engine
{

class TrackMuteState;

//==============================================================================
/**
    Contains options for Edit Node content creation.
*/
struct CreateNodeParams
{
    ProcessState& processState;
    double sampleRate = 44100.0;
    int blockSize = 256;
    const juce::Array<Clip*>* allowedClips = nullptr;
    const juce::Array<Track*>* allowedTracks = nullptr;
    bool forRendering = false;
    bool includePlugins = true;
    bool addAntiDenormalisationNoise = false;
};

//==============================================================================
struct EditNodeBuilder
{
    static std::function<std::unique_ptr<tracktion_graph::Node> (std::unique_ptr<tracktion_graph::Node>)> insertOptionalLastStageNode;
};

//==============================================================================
struct EditNodeContext
{
    std::unique_ptr<tracktion_graph::Node> node;
};

/** Creates a Node to play back an Edit. */
EditNodeContext createNodeForEdit (EditPlaybackContext&, const CreateNodeParams&);

/** Creates a Node to play back an Edit. */
EditNodeContext createNodeForEdit (Edit&, const CreateNodeParams&);


} // namespace tracktion_engine
