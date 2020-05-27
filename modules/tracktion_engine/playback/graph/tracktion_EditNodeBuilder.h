/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion_engine
{

//==============================================================================
/**
    Contains options for Edit Node content creation.
*/
struct CreateNodeParams
{
    const juce::Array<Clip*>* allowedClips = nullptr;
    const juce::Array<Track*>* allowedTracks = nullptr;
    bool forRendering = false;
    bool includePlugins = true;
    bool addAntiDenormalisationNoise = false;
};

//==============================================================================
/** Creates a Node to play back an Edit. */
std::unique_ptr<tracktion_graph::Node> createNodeForEdit (Edit&, tracktion_graph::PlayHeadState&, const CreateNodeParams&);


} // namespace tracktion_engine
