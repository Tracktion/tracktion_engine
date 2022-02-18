/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion { inline namespace engine
{

//==============================================================================
//==============================================================================
LiveMidiInjectingNode::LiveMidiInjectingNode (AudioTrack& at, std::unique_ptr<tracktion::graph::Node> inputNode)
    : track (at), input (std::move (inputNode))
{
    setOptimisations ({ tracktion::graph::ClearBuffers::no,
                        tracktion::graph::AllocateAudioBuffer::no });

    track->addListener (this);
}

LiveMidiInjectingNode::~LiveMidiInjectingNode()
{
    track->removeListener (this);
}

//==============================================================================
tracktion::graph::NodeProperties LiveMidiInjectingNode::getNodeProperties()
{
    auto props = input->getNodeProperties();
    props.hasMidi = true;
    hash_combine (props.nodeID, track->itemID.getRawID());
    
    return props;
}

std::vector<tracktion::graph::Node*> LiveMidiInjectingNode::getDirectInputNodes()
{
    return { input.get() };
}

void LiveMidiInjectingNode::prepareToPlay (const tracktion::graph::PlaybackInitialisationInfo& info)
{
    if (info.rootNodeToReplace == nullptr)
        return;
    
    auto nodeIDToLookFor = getNodeProperties().nodeID;
    
    if (nodeIDToLookFor == 0)
        return;

    auto visitor = [this, nodeIDToLookFor] (Node& node)
    {
        if (auto other = dynamic_cast<LiveMidiInjectingNode*> (&node))
        {
            if (other->getNodeProperties().nodeID == nodeIDToLookFor
                && other->track == track)
            {
                const juce::ScopedLock sl2 (other->liveMidiLock);
                liveMidiMessages.swapWith (other->liveMidiMessages);
                midiSourceID = other->midiSourceID;
            }
        }
    };
    visitNodes (*info.rootNodeToReplace, visitor, true);
}

bool LiveMidiInjectingNode::isReadyToProcess()
{
    return input->hasProcessed();
}

void LiveMidiInjectingNode::process (ProcessContext& pc)
{
    auto sourceBuffers = input->getProcessedOutput();
    auto& destMidiBlock = pc.buffers.midi;
    jassert (sourceBuffers.audio.getSize() == pc.buffers.audio.getSize());

    destMidiBlock.copyFrom (sourceBuffers.midi);
    setAudioOutput (input.get(), sourceBuffers.audio);

    const juce::ScopedLock sl (liveMidiLock);
    
    if (liveMidiMessages.isEmpty())
        return;
    
    destMidiBlock.mergeFromAndClear (liveMidiMessages);
}

//==============================================================================
void LiveMidiInjectingNode::injectMessage (MidiMessageArray::MidiMessageWithSource mm)
{
    mm.setTimeStamp (0.0);

    const juce::ScopedLock sl (liveMidiLock);
    liveMidiMessages.add (std::move (mm));
}

//==============================================================================
void LiveMidiInjectingNode::injectLiveMidiMessage (AudioTrack& at, const MidiMessageArray::MidiMessageWithSource& mm, bool& wasUsed)
{
    if (&at != track.get())
        return;

    injectMessage (mm);
    wasUsed = true;
}

}} // namespace tracktion { inline namespace engine
