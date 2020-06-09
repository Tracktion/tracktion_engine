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
std::unique_ptr<tracktion_graph::Node> createNodeForTrack (Track&, tracktion_graph::PlayHeadState&,
                                                           const CreateNodeParams&);

std::unique_ptr<tracktion_graph::Node> createPluginNodeForList (PluginList&, const TrackMuteState*, std::unique_ptr<Node>,
                                                                tracktion_graph::PlayHeadState&, const CreateNodeParams&);


//==============================================================================
//==============================================================================
std::unique_ptr<tracktion_graph::Node> createNodeForAudioClip (AudioClipBase& clip, tracktion_graph::PlayHeadState& playHeadState, const CreateNodeParams& params)
{
    const AudioFile playFile (clip.getPlaybackFile());

    if (playFile.isNull())
        return {};

    //TODO: Melodyne setup

    auto original = clip.getAudioFile();

    double nodeOffset = 0.0, speed = 1.0;
    EditTimeRange loopRange;
    const bool usesTimestretchedProxy = clip.usesTimeStretchedProxy();

    // Trigger proxy render if it needs it
    clip.beginRenderingNewProxyIfNeeded();

    if (! usesTimestretchedProxy)
    {
        nodeOffset = clip.getPosition().getOffset();
        loopRange = clip.getLoopRange();
        speed = clip.getSpeedRatio();
    }
    
    //TODO:
    // Timestretched previewing node
    // Sub-sample speed ramp audio node

    auto node = tracktion_graph::makeNode<WaveNode> (playFile,
                                                     clip.getEditTimeRange(),
                                                     nodeOffset,
                                                     loopRange,
                                                     clip.getLiveClipLevel(),
                                                     speed,
                                                     clip.getActiveChannels(),
                                                     playHeadState,
                                                     params.forRendering);
    
    // Plugins
    if (params.includePlugins)
    {
        if (auto pluginList = clip.getPluginList())
        {
            for (auto p : *pluginList)
                p->initialiseFully();

            node = createPluginNodeForList (*pluginList, nullptr, std::move (node), playHeadState, params);
        }
    }

    // Create FadeInOutNode
    {
        auto fIn = clip.getFadeIn();
        auto fOut = clip.getFadeOut();

        if (fIn > 0.0 || fOut > 0.0)
        {
            const bool speedIn = clip.getFadeInBehaviour() == AudioClipBase::speedRamp && fIn > 0.0;
            const bool speedOut = clip.getFadeOutBehaviour() == AudioClipBase::speedRamp && fOut > 0.0;

            auto pos = clip.getPosition();
            node = makeNode<FadeInOutNode> (std::move (node), playHeadState,
                                            speedIn ? EditTimeRange (pos.getStart(), pos.getStart() + juce::jmin (0.003, fIn))
                                                    : EditTimeRange (pos.getStart(), pos.getStart() + fIn),
                                            speedOut ? EditTimeRange (pos.getEnd() - juce::jmin (0.003, fOut), pos.getEnd())
                                                     : EditTimeRange (pos.getEnd() - fOut, pos.getEnd()),
                                            clip.getFadeInType(), clip.getFadeOutType(),
                                            true);
        }
    }
    
    return node;
}

std::unique_ptr<tracktion_graph::Node> createNodeForMidiClip (MidiClip& clip, const TrackMuteState& trackMuteState, tracktion_graph::PlayHeadState& playHeadState, const CreateNodeParams&)
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
                                                clip.itemID,
                                                [trackMuteState]
                                                {
                                                    if (! trackMuteState.shouldTrackBeAudible())
                                                        return ! trackMuteState.shouldTrackMidiBeProcessed();
                                                    
                                                    return false;
                                                });
}

std::unique_ptr<tracktion_graph::Node> createNodeForStepClip (StepClip& clip, const TrackMuteState& trackMuteState, tracktion_graph::PlayHeadState& playHeadState, const CreateNodeParams&)
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
                                                clip.itemID,
                                                [trackMuteState]
                                                {
                                                    if (! trackMuteState.shouldTrackBeAudible())
                                                        return ! trackMuteState.shouldTrackMidiBeProcessed();
                                                    
                                                    return false;
                                                });
}

std::unique_ptr<tracktion_graph::Node> createNodeForClip (Clip& clip, const TrackMuteState& trackMuteState,
                                                          tracktion_graph::PlayHeadState& playHeadState, const CreateNodeParams& params)
{
    if (auto audioClip = dynamic_cast<AudioClipBase*> (&clip))
        return createNodeForAudioClip (*audioClip, playHeadState, params);

    if (auto midiClip = dynamic_cast<MidiClip*> (&clip))
        return createNodeForMidiClip (*midiClip, trackMuteState, playHeadState, params);

    if (auto stepClip = dynamic_cast<StepClip*> (&clip))
        return createNodeForStepClip (*stepClip, trackMuteState, playHeadState, params);
    
    return {};
}

//==============================================================================
std::unique_ptr<tracktion_graph::Node> createNodeForFrozenAudioTrack (AudioTrack&, tracktion_graph::PlayHeadState&, const CreateNodeParams& params)
{
    jassert (params.forRendering);
    
    return {};
}

std::unique_ptr<tracktion_graph::SummingNode> createClipsNode (const juce::Array<Clip*>& clips, const TrackMuteState& trackMuteState,
                                                               tracktion_graph::PlayHeadState& playHeadState, const CreateNodeParams& params)
{
    auto summingNode = std::make_unique<tracktion_graph::SummingNode>();

    for (auto clip : clips)
        if (params.allowedClips == nullptr || params.allowedClips->contains (clip))
            summingNode->addInput (createNodeForClip (*clip, trackMuteState, playHeadState, params));

    if (summingNode->getDirectInputNodes().empty())
        return {};
    
    return summingNode;
}

std::unique_ptr<tracktion_graph::Node> createNodeForPlugin (Plugin& plugin, const TrackMuteState* trackMuteState, std::unique_ptr<Node> node,
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
                                                  trackMuteState, playHeadState, params.forRendering);
    
    //TODO:
    // Side-chain send
    // Fine-grain automation

    return node;
}

std::unique_ptr<tracktion_graph::Node> createPluginNodeForList (PluginList& list, const TrackMuteState* trackMuteState, std::unique_ptr<Node> node,
                                                                tracktion_graph::PlayHeadState& playHeadState, const CreateNodeParams& params)
{
    for (auto p : list)
    {
        if (auto sendPlugin = dynamic_cast<AuxSendPlugin*> (p))
            node = makeNode<SendNode> (std::move (node), sendPlugin->busNumber, [sendPlugin] { return volumeFaderPositionToGain (sendPlugin->gain->getCurrentValue()); });
        else if (auto returnPlugin = dynamic_cast<AuxReturnPlugin*> (p))
            node = makeNode<ReturnNode> (std::move (node), returnPlugin->busNumber);
        else
            node = createNodeForPlugin (*p, trackMuteState, std::move (node), playHeadState, params);
    }

    return node;
}

std::unique_ptr<tracktion_graph::Node> createPluginNodeForTrack (AudioTrack& at, const TrackMuteState& trackMuteState, std::unique_ptr<Node> node,
                                                                 tracktion_graph::PlayHeadState& playHeadState, const CreateNodeParams& params)
{
    //TODO:
    // Pre-fx modifier node
    // Post-fx modifier node

    node = createPluginNodeForList (at.pluginList, &trackMuteState, std::move (node), playHeadState, params);

    return node;
}

juce::Array<Track*> getDirectInputTracks (AudioTrack& at)
{
    juce::Array<Track*> inputTracks;

    for (auto track : getAudioTracks (at.edit))
        if (! track->isPartOfSubmix() && track != &at && track->getOutput().outputsToDestTrack (at))
            inputTracks.add (track);

    for (auto track : getTracksOfType<FolderTrack> (at.edit, true))
        if (! track->isPartOfSubmix() && track->getOutput() != nullptr && track->getOutput()->outputsToDestTrack (at))
            inputTracks.add (track);

    return inputTracks;
}

std::unique_ptr<tracktion_graph::Node> createNodeForAudioTrack (AudioTrack& at, tracktion_graph::PlayHeadState& playHeadState, const CreateNodeParams& params)
{
    CRASH_TRACER
    jassert (at.isProcessing (false));

    if (! params.forRendering && at.isFrozen (AudioTrack::individualFreeze))
        return createNodeForFrozenAudioTrack (at, playHeadState, params);

    auto inputTracks = getDirectInputTracks (at);
    const bool muteForInputsWhenRecording = inputTracks.isEmpty();
    const bool processMidiWhenMuted = at.state.getProperty (IDs::processMidiWhenMuted, false);
    auto trackMuteState = std::make_unique<TrackMuteState> (at, muteForInputsWhenRecording, processMidiWhenMuted);

    const auto& clips = at.getClips();
    auto clipsNode = createClipsNode (clips, *trackMuteState, playHeadState, params);
    
    if (clipsNode == nullptr && inputTracks.isEmpty())
        return {};
    
    std::unique_ptr<Node> node = std::move (clipsNode);
    
    if (! inputTracks.isEmpty())
    {
        auto sumNode = std::make_unique<SummingNode>();
        
        if (node)
            sumNode->addInput (std::move (node));

        for (auto inputTrack : inputTracks)
            if (auto n = createNodeForTrack (*inputTrack, playHeadState, params))
                sumNode->addInput (std::move (n));
        
        node = std::move (sumNode);
    }
    
    node = createPluginNodeForTrack (at, *trackMuteState, std::move (node), playHeadState, params);
    node = makeNode<TrackMutingNode> (std::move (trackMuteState), std::move (node));

    return node;
    
    //TODO:
    // Input tracks
    // ARA clips
    // Track comps
    // Live midi out
    // Live midi in
    // Track devices (track outputs to inputs)
}

//==============================================================================
std::unique_ptr<tracktion_graph::Node> createNodeForSubmixTrack (FolderTrack& submixTrack, tracktion_graph::PlayHeadState& playHeadState, const CreateNodeParams& params)
{
    CRASH_TRACER
    jassert (submixTrack.isSubmixFolder());
    jassert (! submixTrack.isPartOfSubmix());

    juce::Array<AudioTrack*> subAudioTracks;
    juce::Array<FolderTrack*> subFolderTracks;

    for (auto t : submixTrack.getAllSubTracks (false))
    {
        if (auto ft = dynamic_cast<AudioTrack*> (t))
            subAudioTracks.add (ft);
        
        if (auto ft = dynamic_cast<FolderTrack*> (t))
            subFolderTracks.add (ft);
    }

    if (subAudioTracks.isEmpty() && subFolderTracks.isEmpty())
        return {};
    
    auto sumNode = std::make_unique<tracktion_graph::SummingNode>();

    // Create nodes for any submix tracks
    for (auto ft : subFolderTracks)
    {
        if (! ft->isProcessing (true))
            continue;

        if (ft->isSubmixFolder())
        {
            if (auto node = createNodeForSubmixTrack (*ft, playHeadState, params))
                sumNode->addInput (std::move (node));
        }
        else
        {
            for (auto at : ft->getAllAudioSubTracks (false))
                if (params.allowedTracks == nullptr || params.allowedTracks->contains (at))
                    if (auto node = createNodeForAudioTrack (*at, playHeadState, params))
                        sumNode->addInput (std::move (node));
        }
    }

    // Then add any audio tracks
    for (auto at : subAudioTracks)
        if (params.allowedTracks == nullptr || params.allowedTracks->contains (at))
            if (at->isProcessing (true))
                if (auto node = createNodeForAudioTrack (*at, playHeadState, params))
                    sumNode->addInput (std::move (node));

    if (sumNode->getDirectInputNodes().empty())
        return {};
    
    // Finally the effects
    std::unique_ptr<Node> node = std::move (sumNode);
    auto trackMuteState = std::make_unique<TrackMuteState> (submixTrack, false, false);

    if (params.includePlugins)
        node = createPluginNodeForList (submixTrack.pluginList, trackMuteState.get(), std::move (node), playHeadState, params);

    node = makeNode<TrackMutingNode> (std::move (trackMuteState), std::move (node));

    return node;
}

//==============================================================================
std::unique_ptr<tracktion_graph::Node> createNodeForTrack (Track& track, tracktion_graph::PlayHeadState& playHeadState, const CreateNodeParams& params)
{
    if (auto t = dynamic_cast<AudioTrack*> (&track))
    {
        if (! t->isProcessing (true))
            return {};

        if (! t->createsOutput())
            return {};

        if (t->isPartOfSubmix())
            return {};

        if (t->isFrozen (Track::groupFreeze))
            return {};

        return createNodeForAudioTrack (*t, playHeadState, params);
    }

    if (auto t = dynamic_cast<FolderTrack*> (&track))
    {
        if (! t->isSubmixFolder())
            return {};

        if (t->isPartOfSubmix())
            return {};

        if (t->getOutput() == nullptr)
            return {};

        return createNodeForSubmixTrack (*t, playHeadState, params);
    }

    return {};
}


//==============================================================================
std::unique_ptr<tracktion_graph::Node> createNodeForDevice (EditPlaybackContext& epc, OutputDevice& device, PlayHeadState& playHeadState, std::unique_ptr<Node> node)
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
    else if (auto midiInstance = dynamic_cast<MidiOutputDeviceInstance*> (epc.getOutputFor (&device)))
    {
        return tracktion_graph::makeNode<MidiOutputDeviceInstanceInjectingNode> (*midiInstance, std::move (node), playHeadState.playHead);
    }
    
    return {};
}

std::unique_ptr<tracktion_graph::Node> createMasterPluginsNode (Edit& edit, OutputDevice& device, tracktion_graph::PlayHeadState& playHeadState, std::unique_ptr<Node> node, const CreateNodeParams& params)
{
    if (edit.engine.getDeviceManager().getDefaultWaveOutDevice() != &device)
        return node;
    
    node = createPluginNodeForList (edit.getMasterPluginList(), nullptr, std::move (node), playHeadState, params);

    if (auto masterVolPlugin = edit.getMasterVolumePlugin())
        node = createNodeForPlugin (*masterVolPlugin, nullptr, std::move (node), playHeadState, params);
    
    return node;
}

std::unique_ptr<tracktion_graph::Node> createMasterFadeInOutNode (Edit& edit, tracktion_graph::PlayHeadState& playHeadState, std::unique_ptr<Node> node)
{
    if (edit.masterFadeIn > 0 || edit.masterFadeOut > 0)
    {
        auto length = edit.getLength();
        return makeNode<FadeInOutNode> (std::move (node), playHeadState,
                                        EditTimeRange { 0.0, edit.masterFadeIn },
                                        EditTimeRange { length - edit.masterFadeOut, length },
                                        edit.masterFadeInType.get(),
                                        edit.masterFadeOutType.get(),
                                        true);
    }

    return node;
}

//==============================================================================
EditNodeContext createNodeForEdit (EditPlaybackContext& epc, tracktion_graph::PlayHeadState& playHeadState, const CreateNodeParams& params)
{
    Edit& edit = epc.edit;
    using TrackNodeVector = std::vector<std::unique_ptr<tracktion_graph::Node>>;
    std::map<OutputDevice*, TrackNodeVector> deviceNodes;

    for (auto t : getAllTracks (edit))
        if (params.allowedTracks == nullptr || params.allowedTracks->contains (t))
            if (auto output = getTrackOutput (*t))
                if (auto device = output->getOutputDevice (false))
                    if (auto node = createNodeForTrack (*t, playHeadState, params))
                        deviceNodes[device].push_back (std::move (node));

    //TODO:
    // Group frozen tracks
    // Insert plugins
    // Optional last stage
    // Preview level measurer
    // ClickTrack (with mute)
    
    auto outputNode = std::make_unique<tracktion_graph::SummingNode>();
        
    for (auto& deviceAndTrackNode : deviceNodes)
    {
        auto device = deviceAndTrackNode.first;
        jassert (device != nullptr);
        auto tracksVector = std::move (deviceAndTrackNode.second);
        
        auto node = tracktion_graph::makeNode<tracktion_graph::SummingNode> (std::move (tracksVector));
        node = createMasterPluginsNode (edit, *device, playHeadState, std::move (node), params);
        node = createMasterFadeInOutNode (edit, playHeadState, std::move (node));
        
        if (edit.isClickTrackDevice (*device))
        {
            //TODO: Add click node (with mute) to device input
        }

        outputNode->addInput (createNodeForDevice (epc, *device, playHeadState, std::move (node)));
    }
    
    std::unique_ptr<Node> finalNode (std::move (outputNode));
        
    return { std::move (finalNode) };
}


}
