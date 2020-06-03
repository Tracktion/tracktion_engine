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
std::unique_ptr<tracktion_graph::Node> createNodeForAudioClip (AudioClipBase& clip, tracktion_graph::PlayHeadState& playHeadState, const CreateNodeParams& params)
{
    //TODO: Fade in/out, speed in/out
    const AudioFile playFile (clip.getPlaybackFile());

    if (playFile.isNull())
        return {};

    //TODO: Melodyne setup

    auto original = clip.getAudioFile();

    double nodeOffset = 0.0, speed = 1.0;
    EditTimeRange loopRange;
    const bool usesTimestretchedProxy = clip.usesTimeStretchedProxy();

    //TODO:
    // Trigger proxy render if it needs it (if original file is invalid or playFile sample rate is == 0.0)

    if (! usesTimestretchedProxy)
    {
        nodeOffset = clip.getPosition().getOffset();
        loopRange = clip.getLoopRange();
        speed = clip.getSpeedRatio();
    }
    
    //TODO:
    // Timestretched previewing node
    // Sub-sample speed ramp audio node

    return tracktion_graph::makeNode<WaveNode> (playFile,
                                                clip.getEditTimeRange(),
                                                nodeOffset,
                                                loopRange,
                                                clip.getLiveClipLevel(),
                                                speed,
                                                clip.getActiveChannels(),
                                                playHeadState,
                                                params.forRendering);
}

std::unique_ptr<tracktion_graph::Node> createNodeForMidiClip (MidiClip& clip, tracktion_graph::PlayHeadState& playHeadState, const CreateNodeParams&)
{
    CRASH_TRACER
    const bool generateMPE = clip.getMPEMode();
    
    juce::MidiMessageSequence sequence;
    clip.getSequenceLooped().exportToPlaybackMidiSequence (sequence, clip, generateMPE);

    auto channels = generateMPE ? juce::Range<int> (2, 15)
                                : juce::Range<int>::withStartAndLength (clip.getMidiChannel().getChannelNumber(), 1);
    
    return tracktion_graph::makeNode<MidiNode> (std::move (sequence),
                                                channels,
                                                generateMPE,
                                                clip.getEditTimeRange(),
                                                clip.getLiveClipLevel(),
                                                playHeadState,
                                                clip.itemID);
}

std::unique_ptr<tracktion_graph::Node> createNodeForStepClip (StepClip& clip, tracktion_graph::PlayHeadState& playHeadState, const CreateNodeParams&)
{
    CRASH_TRACER
    juce::MidiMessageSequence sequence;
    clip.generateMidiSequence (sequence);

    //TODO: Cache these in the StepClip
    auto level = std::make_shared<ClipLevel>();
    level->dbGain.referTo (clip.volumeDb.getValueTree(), clip.volumeDb.getPropertyID(), clip.volumeDb.getUndoManager(), clip.volumeDb.getDefault());
    level->mute.referTo (clip.mute.getValueTree(), clip.mute.getPropertyID(), clip.mute.getUndoManager(), clip.mute.getDefault());

    return tracktion_graph::makeNode<MidiNode> (std::move (sequence),
                                                juce::Range<int> (1, 16),
                                                false,
                                                clip.getEditTimeRange(),
                                                LiveClipLevel (level),
                                                playHeadState,
                                                clip.itemID);
}

std::unique_ptr<tracktion_graph::Node> createNodeForClip (Clip& clip, tracktion_graph::PlayHeadState& playHeadState, const CreateNodeParams& params)
{
    if (auto audioClip = dynamic_cast<AudioClipBase*> (&clip))
        return createNodeForAudioClip (*audioClip, playHeadState, params);

    if (auto midiClip = dynamic_cast<MidiClip*> (&clip))
        return createNodeForMidiClip (*midiClip, playHeadState, params);

    if (auto stepClip = dynamic_cast<StepClip*> (&clip))
        return createNodeForStepClip (*stepClip, playHeadState, params);
    
    return {};
}

//==============================================================================
std::unique_ptr<tracktion_graph::Node> createNodeForFrozenAudioTrack (AudioTrack&, tracktion_graph::PlayHeadState&, const CreateNodeParams& params)
{
    jassert (params.forRendering);
    
    return {};
}

std::unique_ptr<tracktion_graph::SummingNode> createClipsNode (const juce::Array<Clip*>& clips, tracktion_graph::PlayHeadState& playHeadState, const CreateNodeParams& params)
{
    auto summingNode = std::make_unique<tracktion_graph::SummingNode>();

    for (auto clip : clips)
        if (params.allowedClips == nullptr || params.allowedClips->contains (clip))
            summingNode->addInput (createNodeForClip (*clip, playHeadState, params));

    if (summingNode->getDirectInputNodes().empty())
        return {};
    
    return summingNode;
}

std::unique_ptr<tracktion_graph::Node> createNodeForPlugin (Plugin& plugin, std::unique_ptr<Node> node,
                                                            tracktion_graph::PlayHeadState& playHeadState, const CreateNodeParams& params)
{
    jassert (node != nullptr);

    if (plugin.isDisabled())
        return node;

    auto& deviceManager = plugin.edit.engine.getDeviceManager();;
    double sampleRate = deviceManager.getSampleRate();
    int blockSize = deviceManager.getBlockSize();
    node = tracktion_graph::makeNode<PluginNode> (std::move (node),
                                                  plugin,
                                                  sampleRate, blockSize,
                                                  playHeadState, params.forRendering);
    
    //TODO:
    // Side-chain send
    // Fine-grain automation

    return node;
}

std::unique_ptr<tracktion_graph::Node> createPluginNodeForList (PluginList& list, std::unique_ptr<Node> node,
                                                                tracktion_graph::PlayHeadState& playHeadState, const CreateNodeParams& params)
{
    for (auto p : list)
        if (! p->mustBePlayedLiveWhenOnAClip())
            node = createNodeForPlugin (*p, std::move (node), playHeadState, params);

    return node;
}

std::unique_ptr<tracktion_graph::Node> createPluginNodeForTrack (AudioTrack& at, std::unique_ptr<Node> node,
                                                                 tracktion_graph::PlayHeadState& playHeadState, const CreateNodeParams& params)
{
    //TODO:
    // Pre-fx modifier node
    // Post-fx modifier node

    node = createPluginNodeForList (at.pluginList, std::move (node), playHeadState, params);

    return node;
}

std::unique_ptr<tracktion_graph::Node> createNodeForAudioTrack (AudioTrack& at, tracktion_graph::PlayHeadState& playHeadState, const CreateNodeParams& params)
{
    CRASH_TRACER
    jassert (at.isProcessing (false));

    if (! params.forRendering && at.isFrozen (AudioTrack::individualFreeze))
        return createNodeForFrozenAudioTrack (at, playHeadState, params);

    const auto& clips = at.getClips();

    auto clipsNode = createClipsNode (clips, playHeadState, params);
    
    if (clipsNode == nullptr)
        return {};
    
    return createPluginNodeForTrack (at, std::move (clipsNode), playHeadState, params);
    
    //TODO:
    // Input tracks
    // ARA clips
    // Track comps
    // Live midi out
    // Live midi in
    // Track devices (track outputs to inputs)
}

//==============================================================================
std::unique_ptr<tracktion_graph::Node> createNodeForSubmixTrack (FolderTrack& ft, tracktion_graph::PlayHeadState&, CreateNodeParams)
{
    jassert (ft.isSubmixFolder());
    jassert (! ft.isPartOfSubmix());

    return {};
}

//==============================================================================
std::unique_ptr<tracktion_graph::Node> createNodeForDevice (OutputDevice& device, std::unique_ptr<Node> node)
{
    if (auto waveDevice = dynamic_cast<WaveOutputDevice*> (&device))
    {
        std::vector<std::pair<int /*source channel*/, int /*dest channel*/>> channelMap;
        int sourceIndex = 0;

        for (const auto& channel : waveDevice->getChannels())
        {
            if (channel.indexInDevice != -1)
                channelMap.push_back (std::make_pair (sourceIndex, channel.indexInDevice));
            
            ++sourceIndex;
        }

        return tracktion_graph::makeNode<ChannelRemappingNode> (std::move (node), channelMap, false);
    }
    
    //TODO: MIDI devices...
    return {};
}

//==============================================================================
std::unique_ptr<tracktion_graph::Node> createNodeForEdit (Edit& edit, tracktion_graph::PlayHeadState& playHeadState, const CreateNodeParams& params)
{
    using TrackNodeVector = std::vector<std::unique_ptr<tracktion_graph::Node>>;
    std::map<OutputDevice*, TrackNodeVector> deviceNodes;
    
    // Create Nodes for AudioTracks
    for (auto t : getAudioTracks (edit))
    {
        if (! t->isProcessing (true))
            continue;
        
        if (! t->createsOutput())
            continue;
        
        if (t->isPartOfSubmix())
            continue;

        if (t->isFrozen (Track::groupFreeze))
            continue;
                
        if (auto device = t->getOutput().getOutputDevice (false))
            if (auto node = createNodeForAudioTrack (*t, playHeadState, params))
                deviceNodes[device].push_back (std::move (node));
    }
    
    // Then Nodes for any submix tracks
    for (auto t : getTracksOfType<FolderTrack> (edit, true))
    {
        if (! t->isSubmixFolder())
            continue;

        if (t->isPartOfSubmix())
            continue;
        
        if (t->getOutput() == nullptr)
            continue;
        
        if (auto output = t->getOutput())
            if (auto device = output->getOutputDevice (false))
                if (auto node = createNodeForSubmixTrack (*t, playHeadState, params))
                    deviceNodes[device].push_back (std::move (node));
    }

    //TODO:
    // Group frozen tracks
    // Insert plugins
    // Master plugins
    // Optional last stage
    // Master fade in/out
    // Preview level measurer
    
    auto outputNode = std::make_unique<tracktion_graph::SummingNode>();
        
    for (auto& deviceAndTrackNode : deviceNodes)
    {
        auto device = deviceAndTrackNode.first;
        jassert (device != nullptr);
        auto tracksVector = std::move (deviceAndTrackNode.second);
        
        auto tracksNode = tracktion_graph::makeNode<tracktion_graph::SummingNode> (std::move (tracksVector));
        
        if (edit.isClickTrackDevice (*device))
        {
            //TODO: Add click node (with mute) to device input
        }

        outputNode->addInput (createNodeForDevice (*device, std::move (tracksNode)));
    }
        
    return outputNode;
}


}
