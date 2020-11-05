/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#pragma once


namespace tracktion_graph
{

namespace node_player_utils
{
    /** Prepares a specific Node to be played and returns all the Nodes. */
    static std::vector<Node*> prepareToPlay (Node* node, Node* oldNode, double sampleRate, int blockSize)
    {
        if (node == nullptr)
            return {};
        
        // First give the Nodes a chance to transform
        transformNodes (*node);
        
        // Next, initialise all the nodes, this will call prepareToPlay on them and also
        // give them a chance to do things like balance latency
        const PlaybackInitialisationInfo info { sampleRate, blockSize, *node, oldNode };
        visitNodes (*node, [&] (Node& n) { n.initialise (info); }, false);
        
        // Then find all the nodes as it might have changed after initialisation
        return tracktion_graph::getNodes (*node, tracktion_graph::VertexOrdering::postordering);
    }
}

}
