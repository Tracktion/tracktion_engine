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
//==============================================================================
LiveMidiOutputNode::LiveMidiOutputNode (AudioTrack& at, std::unique_ptr<tracktion_graph::Node> inputNode)
    : track (&at), trackPtr (at), input (std::move (inputNode))
{
    jassert (input);
    pendingMessages.reserve (50);
    dispatchingMessages.reserve (50);
}

LiveMidiOutputNode::LiveMidiOutputNode (Clip& c, std::unique_ptr<tracktion_graph::Node> inputNode)
    : clipPtr (c), input (std::move (inputNode))
{
    jassert (input);
    pendingMessages.reserve (50);
    dispatchingMessages.reserve (50);
}

//==============================================================================
tracktion_graph::NodeProperties LiveMidiOutputNode::getNodeProperties()
{
    auto props = input->getNodeProperties();
    props.nodeID = 0;

    return props;
}

std::vector<tracktion_graph::Node*> LiveMidiOutputNode::getDirectInputNodes()
{
    return { input.get() };
}

void LiveMidiOutputNode::prepareToPlay (const tracktion_graph::PlaybackInitialisationInfo&)
{
}

bool LiveMidiOutputNode::isReadyToProcess()
{
    return input->hasProcessed();
}

void LiveMidiOutputNode::process (const ProcessContext& pc)
{
    auto sourceBuffers = input->getProcessedOutput();
    auto destAudioBlock = pc.buffers.audio;
    auto& destMidiBlock = pc.buffers.midi;
    jassert (sourceBuffers.audio.getNumChannels() == destAudioBlock.getNumChannels());

    destMidiBlock.copyFrom (sourceBuffers.midi);
    destAudioBlock.copyFrom (sourceBuffers.audio);

    bool needToUpdate = false;
    
    {
        const juce::ScopedLock sl (lock);

        for (auto& m : sourceBuffers.midi)
            pendingMessages.add (m);

        needToUpdate = ! pendingMessages.isEmpty();
    }
    
    if (needToUpdate)
        triggerAsyncUpdate();
}

//==============================================================================
void LiveMidiOutputNode::handleAsyncUpdate()
{
    {
        const juce::ScopedLock sl (lock);
        pendingMessages.swapWith (dispatchingMessages);
    }

    if (track != nullptr)
        for (auto& m : dispatchingMessages)
            track->getListeners().call (&AudioTrack::Listener::recordedMidiMessageSentToPlugins, *track, m);

    if (clipPtr != nullptr)
        for (auto& m : dispatchingMessages)
            clipPtr->getListeners().call (&Clip::Listener::midiMessageGenerated, *clipPtr, m);

    dispatchingMessages.clear();
}

} // namespace tracktion_engine
