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
namespace
{
    template<typename PluginType>
    juce::Array<PluginType*> getAllPluginsOfType (Edit& edit)
    {
        juce::Array<PluginType*> plugins;
        
        // N.B. There is a bit of a hack here checking if the plugin is actually still in the Edit
        // as they are removed from the PluginCache async and we don't want to flush it every time
        // we call this method. This should probably be moved to an EditItemCache like Clips and Tracks
        for (auto p : edit.getPluginCache().getPlugins())
            if (auto pt = dynamic_cast<PluginType*> (p))
                if (pt->state.getParent().isValid() && pt->state.getRoot() == edit.state)
                    plugins.add (pt);
        
        return plugins;
    }

    using namespace tracktion::graph;

    int getSidechainBusID (EditItemID sidechainSourceID)
    {
        constexpr size_t sidechainMagicNum = 0xb2275e7216a2;
        return static_cast<int> (hash (sidechainMagicNum, sidechainSourceID.getRawID()));
    }

    int getRackInputBusID (EditItemID rackID)
    {
        constexpr size_t rackInputMagicNum = 0x7261636b496e;
        return static_cast<int> (hash (rackInputMagicNum, rackID.getRawID()));
    }

    int getRackOutputBusID (EditItemID rackID)
    {
        constexpr size_t rackOutputMagicNum = 0x7261636b4f7574;
        return static_cast<int> (hash (rackOutputMagicNum, rackID.getRawID()));
    }

    int getWaveInputDeviceBusID (EditItemID trackItemID)
    {
        constexpr size_t waveMagicNum = 0xc1abde;
        return static_cast<int> (hash (waveMagicNum, trackItemID.getRawID()));
    }

    int getMidiInputDeviceBusID (EditItemID trackItemID)
    {
        constexpr size_t midiMagicNum = 0x9a2762;
        return static_cast<int> (hash (midiMagicNum, trackItemID.getRawID()));
    }

    bool isSidechainSource (Track& t)
    {
        const auto itemID = t.itemID;

        for (auto p : t.edit.getPluginCache().getPlugins())
            if (p->getSidechainSourceID() == itemID)
                return true;

        return false;
    }

    constexpr int getTrackNumChannels()
    {
        return 2;
    }

    bool isUnityChannelMap (const std::vector<std::pair<int, int>>& channelMap)
    {
        for (auto mapping : channelMap)
            if (mapping.first != mapping.second)
                return false;

        return true;
    }

    std::vector<std::pair<int, int>> makeChannelMapRepeatingLastChannel (const juce::AudioChannelSet& source,
                                                                         const juce::AudioChannelSet& dest)
    {
        std::vector<std::pair<int, int>> map;
        
        for (int destNum = 0; destNum < dest.size(); ++destNum)
        {
            const int sourceNum = std::min (destNum, source.size() - 1);
            map.push_back ({ sourceNum, destNum });
        }
        
        return map;
    }

    AudioTrack* getTrackContainingTrackDevice (Edit& edit, WaveInputDevice& device)
    {
        for (auto t : getAudioTracks (edit))
            if (&t->getWaveInputDevice() == &device)
                return t;

        return nullptr;
    }

    AudioTrack* getTrackContainingTrackDevice (Edit& edit, MidiInputDevice& device)
    {
        for (auto t : getAudioTracks (edit))
            if (&t->getMidiInputDevice() == &device)
                return t;

        return nullptr;
    }

    int getNumChannelsFromDevice (OutputDevice& device)
    {
        if (auto waveDevice = dynamic_cast<WaveOutputDevice*> (&device))
            return waveDevice->getChannelSet().size();

        return 0;
    }

    juce::Array<RackInstance*> getInstancesForRack (RackType& type)
    {
        juce::Array<RackInstance*> instances;

        for (auto ri : getAllPluginsOfType<RackInstance> (type.edit))
            if (ri->type.get() == &type)
                instances.add (ri);
        
        return instances;
    }

    juce::Array<RackInstance*> getEnabledInstancesForRack (RackType& type)
    {
        auto instances = getInstancesForRack (type);
        instances.removeIf ([] (auto instance) { return ! instance->isEnabled(); });
        
        return instances;
    }

    // If we're rendering and try to render a track in a submix,
    // only render it if the parent track isn't included in the allowed tracks
    // This allows us to render tracks contained inside submixes without the
    // parent submix effects applied
    bool shouldRenderTrackInSubmix (Track& t, const CreateNodeParams& params)
    {
        jassert (t.isPartOfSubmix());
        
        if (! params.forRendering)
            return false;
        
        if (params.allowedTracks == nullptr)
            return false;
        
        for (auto allowedTrack : *params.allowedTracks)
            if (t.isAChildOf (*allowedTrack))
                return false;
        
        return true;
    }

    juce::Array<Track*> addImplicitSubmixChildTracks (const juce::Array<Track*> originalTracks)
    {
        if (originalTracks.isEmpty())
            return {};
     
        auto tracks = originalTracks;
        
        // Iterate all original tracks
        // If any tracks are submix tracks, check if their parents are included or any of their children
        // If not, add all children recusively
        // Ensure there are no duplicates
        for (auto track : originalTracks)
        {
            if (auto st = dynamic_cast<FolderTrack*> (track);
                st != nullptr && st->isSubmixFolder())
            {
                bool shouldSkip = false;
                
                // First check for parents
                for (auto potentialParent : originalTracks)
                {
                    if (track->isAChildOf (*potentialParent))
                    {
                        shouldSkip = true;
                        break;
                    }
                }

                // Then children
                for (auto potentialChild : originalTracks)
                {
                    if (potentialChild->isAChildOf (*track))
                    {
                        shouldSkip = true;
                        break;
                    }
                }
                
                if (shouldSkip)
                    continue;

                // Otherwise add all the children
                for (auto childTrack : st->getAllSubTracks (true))
                    tracks.addIfNotAlreadyThere (childTrack);
            }
        }
    
        return tracks;
    }

   #if TRACKTION_ENABLE_REALTIME_TIMESTRETCHING
    SpeedFadeDescription getSpeedFadeDescription (const AudioClipBase& clip)
    {
        if (clip.getFadeInBehaviour() == AudioClipBase::speedRamp
            || clip.getFadeOutBehaviour() == AudioClipBase::speedRamp)
        {
            SpeedFadeDescription desc;
            const auto clipPos = clip.getPosition();

            if (clip.getFadeInBehaviour() == AudioClipBase::speedRamp)
            {
                desc.inTimeRange = TimeRange (clipPos.getStart(), clip.getFadeIn());
                desc.fadeInType = clip.getFadeInType();
            }
            else
            {
                desc.inTimeRange = TimeRange (clipPos.getStart(), TimeDuration());
            }

            if (clip.getFadeOutBehaviour() == AudioClipBase::speedRamp)
            {
                desc.outTimeRange = TimeRange (clipPos.getEnd() - clip.getFadeOut(), clip.getFadeOut());
                desc.fadeOutType = clip.getFadeOutType();
            }
            else
            {
                desc.outTimeRange = TimeRange (clipPos.getEnd(), TimeDuration());
            }

            return desc;
        }

        return {};
    }

    std::optional<WarpMap> getWarpMap (const AudioClipBase& clip)
    {
        if (! clip.getWarpTime())
            return {};

        WarpMap map;

        for (auto m : clip.getWarpTimeManager().getMarkers())
            map.push_back ({ m->sourceTime, m->warpTime });

        return map;
    }

    /**
        Returns a tempo::Sequence with the key changes required for a clip to sync to the chord track.
    */
    std::optional<tempo::Sequence> getChordTrackSequenceIfRequired (AudioClipBase& clip)
    {
        if (! clip.getAutoPitch())
            return {};

        if (clip.getAutoPitchMode() != AudioClipBase::chordTrackMono)
            return {};

        if (auto pg = clip.getPatternGenerator())
        {
            // First get the properties that are static for the whole clip
            const auto clipRootKey = clip.getLoopInfo().getRootNote() % 12;
            const auto clipTransposeSemitones = clip.getTransposeSemiTones (false);
            const auto scale = static_cast<int> (pg->scaleType.get());

            // Next get the progression in Edit-time
            juce::OwnedArray<PatternGenerator::ProgressionItem> progression;
            pg->getFlattenedChordProgression (progression, true);

            // Then iterate the progression
            std::vector<tempo::KeyChange> keyChanges;
            auto editTempoSequencePosition = createPosition (clip.edit.tempoSequence);
            BeatPosition beatPos;

            for (auto p : progression)
            {
                // Find the key (pitch/scale) of the Edit
                editTempoSequencePosition.set (beatPos);
                const auto editKey = editTempoSequencePosition.getKey();

                const int scaleNote = editKey.pitch % 12;
                int chordTrackPitchDelta = 0;

                // If this section has a chord, find the pitch offset for it
                if (p->chordName.get().isNotEmpty())
                {
                    const int chordNote = p->getRootNote (scaleNote, Scale (static_cast<Scale::ScaleType> (editKey.scale)));
                    chordTrackPitchDelta = chordNote - scaleNote;
                }

                // Then find the base transposition from the Edit's key and clip's key
                int transposeBase = scaleNote - clipRootKey;

                while (transposeBase > 6)  transposeBase -= 12;
                while (transposeBase < -6) transposeBase += 12;

                // Shift by the section's octave
                transposeBase += p->octave * 12;

                // Put the three transposition sections back together and add it as a KeyChange
                const int finalPitch = transposeBase + chordTrackPitchDelta + clipTransposeSemitones;
                keyChanges.push_back ({ beatPos, { finalPitch, scale } });

                beatPos = beatPos + p->lengthInBeats;
            }

            // Finally copy tempo data from Edit's tempo sequence
            std::vector<tempo::TempoChange> tempoChanges;
            std::vector<tempo::TimeSigChange> timeSigChanges;

            {
                for (auto ts : clip.edit.tempoSequence.getTempos())
                    tempoChanges.push_back ({ ts->startBeatNumber.get(), ts->bpm.get(), ts->curve.get() });

                for (auto ts : clip.edit.tempoSequence.getTimeSigs())
                    timeSigChanges.push_back ({ ts->startBeatNumber.get(), ts->numerator.get(), ts->denominator.get(), ts->triplets.get() });
            }

            const bool useDenominator = clip.edit.engine.getEngineBehaviour().lengthOfOneBeatDependsOnTimeSignature();
            tempo::Sequence seq (std::move (tempoChanges), std::move (timeSigChanges), std::move (keyChanges),
                                    useDenominator ? tempo::LengthOfOneBeat::dependsOnTimeSignature
                                                   : tempo::LengthOfOneBeat::isAlwaysACrotchet);

            return seq;
        }

        return {};
    }
   #endif

//==============================================================================
//==============================================================================
std::unique_ptr<tracktion::graph::Node> createNodeForTrack (Track&, const CreateNodeParams&);

std::unique_ptr<tracktion::graph::Node> createPluginNodeForList (PluginList&, const TrackMuteState*, std::unique_ptr<Node>,
                                                                 tracktion::graph::PlayHeadState&, const CreateNodeParams&);

std::unique_ptr<tracktion::graph::Node> createPluginNodeForTrack (Track&, TrackMuteState&, std::unique_ptr<Node>,
                                                                 tracktion::graph::PlayHeadState&, const CreateNodeParams&);

std::unique_ptr<tracktion::graph::Node> createLiveInputNodeForDevice (InputDeviceInstance&, tracktion::graph::PlayHeadState&, const CreateNodeParams&);


//==============================================================================
//==============================================================================
std::unique_ptr<tracktion::graph::Node> createFadeNodeForClip (AudioClipBase& clip, std::unique_ptr<Node> node, const CreateNodeParams& params)
{
    auto fIn = clip.getFadeIn();
    auto fOut = clip.getFadeOut();

    if (fIn > 0_td || fOut > 0_td)
    {
        const bool speedIn = clip.getFadeInBehaviour() == AudioClipBase::speedRamp && fIn > 0_td;
        const bool speedOut = clip.getFadeOutBehaviour() == AudioClipBase::speedRamp && fOut > 0_td;

        auto pos = clip.getPosition();
        node = makeNode<FadeInOutNode> (std::move (node), params.processState,
                                        speedIn ? TimeRange (pos.getStart(), pos.getStart() + juce::jmin (TimeDuration::fromSeconds (0.003), fIn))
                                                : TimeRange (pos.getStart(), pos.getStart() + fIn),
                                        speedOut ? TimeRange (pos.getEnd() - juce::jmin (TimeDuration::fromSeconds (0.003), fOut), pos.getEnd())
                                                 : TimeRange (pos.getEnd() - fOut, pos.getEnd()),
                                        clip.getFadeInType(), clip.getFadeOutType(),
                                        true);
    }
    
    return node;
}

//==============================================================================
std::unique_ptr<tracktion::graph::Node> createNodeForAudioClip (AudioClipBase& clip, bool includeMelodyne, const CreateNodeParams& params)
{
    auto& playHeadState = params.processState.playHeadState;
    const AudioFile playFile (clip.getPlaybackFile());

    if (playFile.isNull())
        return {};

    // Check if ARA should be used
    if (clip.setupARA (false))
    {
        jassert (clip.melodyneProxy != nullptr);

        if (includeMelodyne)
            return makeNode<MelodyneNode> (clip, playHeadState.playHead, params.forRendering);

        return {}; // the ARA node creation will be handled by the track to allow live-play...
    }

    clip.melodyneProxy = nullptr;

    // Otherwise use audio file
    auto original = clip.getAudioFile();

    TimeDuration nodeOffset;
    double speed = 1.0;
    TimeRange loopRange;
    const bool usesTimestretchedProxy = clip.usesTimeStretchedProxy();

    // Trigger proxy render if it needs it
    clip.beginRenderingNewProxyIfNeeded();

    if (! usesTimestretchedProxy)
    {
        nodeOffset = clip.getPosition().getOffset();
        loopRange = clip.getLoopRange();
        speed = clip.getSpeedRatio();
    }

    if (clip.usesTimestretchedPreview())
    {
        auto& li = clip.getLoopInfo();

        if (li.getNumBeats() > 0.0 || li.getRootNote() != -1)
        {
            auto node = makeNode<TimeStretchingWaveNode> (clip, playHeadState);

            const auto sourceChannels = juce::AudioChannelSet::canonicalChannelSet (clip.getAudioFile().getNumChannels());
            const auto destChannels = juce::AudioChannelSet::canonicalChannelSet (std::max (2, sourceChannels.size()));

            if (sourceChannels.size() != destChannels.size())
                node = makeNode<ChannelRemappingNode> (std::move (node),
                                                       makeChannelMapRepeatingLastChannel (sourceChannels, destChannels),
                                                       false);
            
            return node;
        }
    }

    std::unique_ptr<Node> node;

   #if TRACKTION_ENABLE_REALTIME_TIMESTRETCHING
    if (clip.canUseProxy())
   #endif
    {
        if ((clip.getFadeInBehaviour() == AudioClipBase::speedRamp && clip.getFadeIn() != 0_td)
            || (clip.getFadeOutBehaviour() == AudioClipBase::speedRamp && clip.getFadeOut() != 0_td))
        {
            SpeedFadeDescription desc;
            const auto clipPos = clip.getPosition();

            if (clip.getFadeInBehaviour() == AudioClipBase::speedRamp)
            {
                desc.inTimeRange = TimeRange (clipPos.getStart(), clip.getFadeIn());
                desc.fadeInType = clip.getFadeInType();
            }
            else
            {
                desc.inTimeRange = TimeRange (clipPos.getStart(), TimeDuration());
            }

            if (clip.getFadeOutBehaviour() == AudioClipBase::speedRamp)
            {
                desc.outTimeRange = TimeRange (clipPos.getEnd() - clip.getFadeOut(), clip.getFadeOut());
                desc.fadeOutType = clip.getFadeOutType();
            }
            else
            {
                desc.outTimeRange = TimeRange (clipPos.getEnd(), TimeDuration());
            }

            node = tracktion::graph::makeNode<SpeedRampWaveNode> (playFile,
                                                                  clip.getEditTimeRange(),
                                                                  nodeOffset,
                                                                  loopRange,
                                                                  clip.getLiveClipLevel(),
                                                                  speed,
                                                                  clip.getActiveChannels(),
                                                                  juce::AudioChannelSet::canonicalChannelSet (std::max (2, clip.getActiveChannels().size())),
                                                                  params.processState,
                                                                  clip.itemID,
                                                                  params.forRendering,
                                                                  desc);
        }
        else
        {
            node = tracktion::graph::makeNode<WaveNode> (playFile,
                                                         clip.getEditTimeRange(),
                                                         nodeOffset,
                                                         loopRange,
                                                         clip.getLiveClipLevel(),
                                                         speed,
                                                         clip.getActiveChannels(),
                                                         juce::AudioChannelSet::canonicalChannelSet (std::max (2, clip.getActiveChannels().size())),
                                                         params.processState,
                                                         clip.itemID,
                                                         params.forRendering);
        }
    }
   #if TRACKTION_ENABLE_REALTIME_TIMESTRETCHING
    else
    {
        const auto timeStretcherMode = clip.getActualTimeStretchMode();
        const auto timeStretcherOpts = clip.elastiqueProOptions.get();

        const auto speedFadeDesc = getSpeedFadeDescription (clip);
        auto warpMap = getWarpMap (clip);
        std::optional<tempo::Sequence::Position> editTempoPosition (speedFadeDesc.isEmpty() ? std::optional<tempo::Sequence::Position>() : createPosition (clip.edit.tempoSequence));

        if (clip.getAutoTempo() || clip.getAutoPitch())
        {
            std::vector<tempo::TempoChange> tempos;
            std::vector<tempo::TimeSigChange> timeSigs;
            std::vector<tempo::KeyChange> keyChanges;
            auto syncTempo = WaveNodeRealTime::SyncTempo::no;
            auto syncPitch = WaveNodeRealTime::SyncPitch::no;

            auto wi = clip.getWaveInfo();
            auto& li = clip.getLoopInfo();

            if (clip.getAutoTempo() && li.getNumBeats() > 0 && wi.hashCode != 0)
            {
                tempos.push_back ({ 0_bp, li.getBpm (wi), 1.0 });
                timeSigs.push_back ({ 0_bp, li.getNumerator(), li.getDenominator(), false });
                syncTempo = WaveNodeRealTime::SyncTempo::yes;
            }
            else
            {
                tempos.push_back ({ 0_bp, 120.0, 0.0 });
                timeSigs.push_back ({ 0_bp, 4, 4, false });
            }

            if (clip.getAutoPitch() && li.getRootNote() != -1)
            {
                keyChanges.push_back ({ 0_bp, { li.getRootNote(), 0 } });
                syncPitch = WaveNodeRealTime::SyncPitch::yes;
            }

            tempo::Sequence seq (std::move (tempos),
                                 std::move (timeSigs),
                                 std::move (keyChanges),
                                 clip.edit.engine.getEngineBehaviour().lengthOfOneBeatDependsOnTimeSignature() ? tempo::LengthOfOneBeat::dependsOnTimeSignature
                                                                                                               : tempo::LengthOfOneBeat::isAlwaysACrotchet);

            node = makeNode<WaveNodeRealTime> (playFile,
                                               timeStretcherMode, timeStretcherOpts,
                                               BeatRange (clip.getStartBeat(), clip.getEndBeat()),
                                               clip.getOffsetInBeats(),
                                               BeatRange (clip.getLoopStartBeats(), clip.getLoopLengthBeats()),
                                               clip.getLiveClipLevel(),
                                               clip.getActiveChannels(),
                                               juce::AudioChannelSet::canonicalChannelSet (std::max (2, clip.getActiveChannels().size())),
                                               params.processState,
                                               clip.itemID,
                                               params.forRendering,
                                               clip.getResamplingQuality(),
                                               speedFadeDesc, std::move (editTempoPosition),
                                               std::move (warpMap),
                                               seq, syncTempo, syncPitch,
                                               getChordTrackSequenceIfRequired (clip));
        }
        else
        {
            node = makeNode<WaveNodeRealTime> (playFile,
                                               clip.getEditTimeRange(),
                                               clip.getPosition().getOffset(),
                                               clip.getLoopRange(),
                                               clip.getLiveClipLevel(),
                                               clip.getSpeedRatio(),
                                               clip.getActiveChannels(),
                                               juce::AudioChannelSet::canonicalChannelSet (std::max (2, clip.getActiveChannels().size())),
                                               params.processState,
                                               clip.itemID,
                                               params.forRendering,
                                               clip.getResamplingQuality(),
                                               speedFadeDesc, std::move (editTempoPosition),
                                               timeStretcherMode, timeStretcherOpts,
                                               clip.getPitchChange());
        }
    }
   #endif

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
    node = createFadeNodeForClip (clip, std::move (node), params);
    
    return node;
}

std::unique_ptr<tracktion::graph::Node> createNodeForMidiClip (MidiClip& clip, const TrackMuteState& trackMuteState, const CreateNodeParams& params)
{
    CRASH_TRACER
    const bool generateMPE = clip.getMPEMode();
    const auto timeBase = clip.canUseProxy() ? MidiList::TimeBase::seconds
                                             : MidiList::TimeBase::beatsRaw;

    const auto channels = generateMPE ? juce::Range<int> (2, 15)
                                      : juce::Range<int>::withStartAndLength (clip.getMidiChannel().getChannelNumber(), 1);

    if (timeBase == MidiList::TimeBase::beatsRaw)
    {
        std::vector<juce::MidiMessageSequence> sequences;
        sequences.emplace_back (clip.getSequence().exportToPlaybackMidiSequence (clip, timeBase, generateMPE));

        return graph::makeNode<LoopingMidiNode> (std::move (sequences),
                                                 channels,
                                                 generateMPE,
                                                 BeatRange (clip.getStartBeat(), clip.getEndBeat()),
                                                 BeatRange (clip.getLoopStartBeats(), clip.getLoopLengthBeats()),
                                                 clip.getOffsetInBeats(),
                                                 clip.getLiveClipLevel(),
                                                 params.processState,
                                                 clip.itemID,
                                                 clip.getQuantisation(),
                                                 clip.edit.engine.getGrooveTemplateManager().getTemplateByName (clip.getGrooveTemplate()),
                                                 clip.getGrooveStrength(),
                                                 [&trackMuteState]
                                                 {
                                                      if (! trackMuteState.shouldTrackBeAudible())
                                                         return ! trackMuteState.shouldTrackMidiBeProcessed();

                                                     return false;
                                                 });
    }

    // Use looped sequence in seconds time base
    const auto clipTimeRange = clip.getEditTimeRange();
    const juce::Range<double> editTimeRange { clipTimeRange.getStart().inSeconds(), clipTimeRange.getEnd().inSeconds() };

    std::vector<juce::MidiMessageSequence> sequences;
    sequences.emplace_back (clip.getSequenceLooped().exportToPlaybackMidiSequence (clip, timeBase, generateMPE));

    return graph::makeNode<MidiNode> (std::move (sequences),
                                      timeBase,
                                      channels,
                                      generateMPE,
                                      editTimeRange,
                                      clip.getLiveClipLevel(),
                                      params.processState,
                                      clip.itemID,
                                      [&trackMuteState]
                                      {
                                          if (! trackMuteState.shouldTrackBeAudible())
                                              return ! trackMuteState.shouldTrackMidiBeProcessed();

                                          return false;
                                      });
}

std::unique_ptr<tracktion::graph::Node> createNodeForStepClip (StepClip& clip, const TrackMuteState& trackMuteState, const CreateNodeParams& params)
{
    CRASH_TRACER

    std::unique_ptr<tracktion::graph::Node> node;

    std::vector<juce::MidiMessageSequence> sequences;

    for (int i = clip.usesProbability() ? 64 : 1; --i >= 0;)
    {
        juce::MidiMessageSequence sequence;
        clip.generateMidiSequence (sequence);
        sequences.push_back (sequence);
    }

    const auto clipRange = clip.getEditTimeRange();
    const juce::Range<double> editTimeRange (clipRange.getStart().inSeconds(), clipRange.getEnd().inSeconds());
    node = graph::makeNode<MidiNode> (std::move (sequences),
                                      MidiList::TimeBase::seconds,
                                      juce::Range<int> (1, 16),
                                      false,
                                      editTimeRange,
                                      clip.getLiveClipLevel(),
                                      params.processState,
                                      clip.itemID,
                                      [&trackMuteState]
                                      {
                                          if (! trackMuteState.shouldTrackBeAudible())
                                              return ! trackMuteState.shouldTrackMidiBeProcessed();

                                          return false;
                                      });

    if (node && ! clip.getListeners().isEmpty())
        node = makeNode<LiveMidiOutputNode> (clip, std::move (node));

    return node;
}

std::unique_ptr<tracktion::graph::Node> createNodeForClip (Clip& clip, const TrackMuteState& trackMuteState, const CreateNodeParams& params)
{
    if (auto audioClip = dynamic_cast<AudioClipBase*> (&clip))
        return createNodeForAudioClip (*audioClip, false, params);

    if (auto midiClip = dynamic_cast<MidiClip*> (&clip))
        return createNodeForMidiClip (*midiClip, trackMuteState, params);

    if (auto stepClip = dynamic_cast<StepClip*> (&clip))
        return createNodeForStepClip (*stepClip, trackMuteState, params);
    
    return {};
}

std::unique_ptr<tracktion::graph::Node> createNodeForClips (EditItemID trackID, const juce::Array<Clip*>& clips, const TrackMuteState& trackMuteState, const CreateNodeParams& params)
{
    // If there are no clips, we still need to send note-offs for clips that might have been deleted whilst still playing
    // In the future, this will be removed during the transform stage
    if (clips.size() == 0)
        return std::make_unique<CombiningNode> (trackID, params.processState);

    const bool clipsHaveLatency = [&]
    {
        if (params.includePlugins)
            for (auto clip : clips)
                if (params.allowedClips == nullptr || params.allowedClips->contains (clip))
                    if (auto pluginList = clip->getPluginList())
                        for (auto p : *pluginList)
                            if (p->getLatencySeconds() > 0.0)
                                return true;

        return false;
    }();

    // If any of the clips have latency, it's impossible to use a CombiningNode as it doesn't
    // continuously process Nodes which means the latency FIFO doesn't get flushed. So just
    // use a normal SummingNode instead
    if (clipsHaveLatency)
    {
        auto combiner = std::make_unique<SummingNode>();

        for (auto clip : clips)
            if (params.allowedClips == nullptr || params.allowedClips->contains (clip))
                if (auto clipNode = createNodeForClip (*clip, trackMuteState, params))
                    combiner->addInput (std::move (clipNode));

        return combiner;
    }

    if (clips.size() == 1)
    {
        auto clip = clips.getFirst();

        if (params.allowedClips == nullptr || params.allowedClips->contains (clip))
        {
            auto combiner = std::make_unique<CombiningNode> (trackID, params.processState);

            if (auto clipNode = createNodeForClip (*clip, trackMuteState, params))
                combiner->addInput (std::move (clipNode), clip->getPosition().time);

            return combiner;
        }
    }

    auto combiner = std::make_unique<CombiningNode> (trackID, params.processState);

    // Use a CombiningNode for most clips
    for (auto clip : clips)
        if (params.allowedClips == nullptr || params.allowedClips->contains (clip))
            if (auto clipNode = createNodeForClip (*clip, trackMuteState, params))
                combiner->addInput (std::move (clipNode), clip->getPosition().time);

    return combiner;
}

//==============================================================================
std::unique_ptr<tracktion::graph::Node> createNodeForFrozenAudioTrack (AudioTrack& track, tracktion::graph::PlayHeadState& playHeadState, const CreateNodeParams& params)
{
    jassert (! params.forRendering);

    const bool processMidiWhenMuted = track.state.getProperty (IDs::processMidiWhenMuted, false);
    auto trackMuteState = std::make_unique<TrackMuteState> (track, false, processMidiWhenMuted);
    auto node = tracktion::graph::makeNode<WaveNode> (AudioFile (track.edit.engine, TemporaryFileManager::getFreezeFileForTrack (track)),
                                                     TimeRange (TimePosition(), track.getLengthIncludingInputTracks()),
                                                     TimeDuration(), TimeRange(), LiveClipLevel(),
                                                     1.0, juce::AudioChannelSet::stereo(), juce::AudioChannelSet::stereo(),
                                                     params.processState,
                                                     track.itemID,
                                                     params.forRendering);

    // Plugins
    if (params.includePlugins)
        node = createPluginNodeForTrack (track, *trackMuteState, std::move (node), playHeadState, params);

    if (isSidechainSource (track))
        node = makeNode<SendNode> (std::move (node), getSidechainBusID (track.itemID));

    node = makeNode<TrackMutingNode> (std::move (trackMuteState), std::move (node), false);

    return node;
}

std::unique_ptr<tracktion::graph::Node> createARAClipsNode (const juce::Array<Clip*>& clips, const TrackMuteState&, const CreateNodeParams& params)
{
    juce::Array<AudioClipBase*> araClips;

    for (auto clip : clips)
        if (params.allowedClips == nullptr || params.allowedClips->contains (clip))
            if (auto acb = dynamic_cast<AudioClipBase*> (clip))
                if (acb->isUsingMelodyne() && acb->melodyneProxy != nullptr)
                    araClips.add (acb);

    if (araClips.size() == 0)
        return {};
    
    std::vector<std::unique_ptr<Node>> nodes;
    
    for (auto araClip : araClips)
        if (auto araNode = createNodeForAudioClip (*araClip, true, params))
            nodes.push_back (createFadeNodeForClip (*araClip, std::move (araNode), params));
    
    if (nodes.size() == 1)
        return std::move (nodes.front());
    
    return std::make_unique<SummingNode> (std::move (nodes));
}

std::unique_ptr<tracktion::graph::Node> createClipsNode (EditItemID trackID, const juce::Array<Clip*>& clips, const TrackMuteState& trackMuteState,
                                                         const CreateNodeParams& params)
{
    std::vector<std::unique_ptr<Node>> nodes;

    if (auto clipsNode = createNodeForClips (trackID, clips, trackMuteState, params))
        nodes.push_back (std::move (clipsNode));
    
    if (auto araNode = createARAClipsNode (clips, trackMuteState, params))
        nodes.push_back (std::move (araNode));
    
    if (nodes.empty())
        return {};
    
    if (nodes.size() == 1)
        return std::move (nodes.front());
    
    return std::make_unique<SummingNode> (std::move (nodes));
}

std::unique_ptr<tracktion::graph::Node> createLiveInputNodeForDevice (InputDeviceInstance& inputDeviceInstance, tracktion::graph::PlayHeadState& playHeadState,
                                                                     const CreateNodeParams& params)
{
    if (auto midiDevice = dynamic_cast<MidiInputDevice*> (&inputDeviceInstance.getInputDevice()))
    {
        if (midiDevice->isTrackDevice())
            if (auto sourceTrack = getTrackContainingTrackDevice (inputDeviceInstance.edit, *midiDevice))
                return makeNode<TrackMidiInputDeviceNode> (*midiDevice, makeNode<ReturnNode> (getMidiInputDeviceBusID (sourceTrack->itemID)), params.processState);

        if (HostedAudioDeviceInterface::isHostedMidiInputDevice (*midiDevice))
            return makeNode<HostedMidiInputDeviceNode> (inputDeviceInstance, *midiDevice, midiDevice->getMPESourceID(), playHeadState, params.processState);

        return makeNode<MidiInputDeviceNode> (inputDeviceInstance, *midiDevice, midiDevice->getMPESourceID(), playHeadState);
    }
    else if (auto waveDevice = dynamic_cast<WaveInputDevice*> (&inputDeviceInstance.getInputDevice()))
    {
        if (waveDevice->isTrackDevice())
            if (auto sourceTrack = getTrackContainingTrackDevice (inputDeviceInstance.edit, *waveDevice))
                return makeNode<TrackWaveInputDeviceNode> (*waveDevice, makeNode<ReturnNode> (getWaveInputDeviceBusID (sourceTrack->itemID)));

        // For legacy reasons, we always need a stereo output from our live inputs
        return makeNode<WaveInputDeviceNode> (inputDeviceInstance, *waveDevice,
                                              juce::AudioChannelSet::canonicalChannelSet (2));
    }

    return {};
}

std::unique_ptr<tracktion::graph::Node> createLiveInputsNode (AudioTrack& track, tracktion::graph::PlayHeadState& playHeadState, const CreateNodeParams& params)
{
    std::vector<std::unique_ptr<tracktion::graph::Node>> nodes;

    if (! params.forRendering)
        if (auto context = track.edit.getCurrentPlaybackContext())
            for (auto in : context->getAllInputs())
                if ((in->isLivePlayEnabled (track) || in->getInputDevice().isTrackDevice()) && in->isOnTargetTrack (track))
                    if (auto node = createLiveInputNodeForDevice (*in, playHeadState, params))
                        nodes.push_back (std::move (node));

    if (nodes.empty())
        return {};

    if (nodes.size() == 1)
        return std::move (nodes.front());

    return std::make_unique<SummingNode> (std::move (nodes));
}

std::unique_ptr<tracktion::graph::Node> createSidechainInputNodeForPlugin (Plugin& plugin, std::unique_ptr<Node> node)
{
    const auto sidechainSourceID = plugin.getSidechainSourceID();
    const bool usesSidechain = ! plugin.isMissing() && sidechainSourceID.isValid();

    if (! usesSidechain)
        return node;

    // This is complicated because the first two source channels will always be the track the plugin is on
    // Any additional channels will be from the sidechain source track
    // So we really have two channel maps, one from the plugin's track to the plugin and one from the sidechain track to the plugin
    std::vector<std::pair<int /*source channel*/, int /*dest channel*/>> directChannelMap, sidechainChannelMap;

    for (int i = 0; i < plugin.getNumWires(); ++i)
    {
        if (auto w = plugin.getWire (i))
        {
            const int sourceIndex = w->sourceChannelIndex;
            const int destIndex = w->destChannelIndex;

            if (sourceIndex < getTrackNumChannels())
                directChannelMap.emplace_back (sourceIndex, destIndex);
            else
                sidechainChannelMap.emplace_back (sourceIndex - getTrackNumChannels(), destIndex);
        }
    }

    if (directChannelMap.empty() && sidechainChannelMap.empty())
        return node;

    auto directInput = std::move (node);

    if (! isUnityChannelMap (directChannelMap))
        directInput = makeNode<ChannelRemappingNode> (std::move (directInput), directChannelMap, true);

    auto sidechainInput = makeNode<ReturnNode> (getSidechainBusID (sidechainSourceID));
    sidechainInput = makeNode<ChannelRemappingNode> (std::move (sidechainInput), std::move (sidechainChannelMap), false);

    if (directChannelMap.empty())
        return sidechainInput;

    auto sumNode = makeSummingNode ({ directInput.release(), sidechainInput.release() });

    return sumNode;
}

std::unique_ptr<tracktion::graph::Node> createNodeForPlugin (Plugin& plugin, const TrackMuteState* trackMuteState, std::unique_ptr<Node> node,
                                                             const CreateNodeParams& params)
{
    jassert (node != nullptr);

    if (plugin.isDisabled())
        return node;
    
    if (! plugin.isEnabled() && ! params.includeBypassedPlugins)
        return node;

    int maxNumChannels = -1;
    
    // If this plugin is on a track or clip and doesn't have a sidechain input we can limit the number of channels it uses
    if (plugin.getOwnerTrack() != nullptr || plugin.getOwnerClip() != nullptr)
        if (! plugin.getSidechainSourceID().isValid())
            maxNumChannels = 2;
    
    node = createSidechainInputNodeForPlugin (plugin, std::move (node));
    node = tracktion::graph::makeNode<PluginNode> (std::move (node),
                                                  plugin,
                                                  params.sampleRate, params.blockSize,
                                                  trackMuteState, params.processState,
                                                  params.forRendering, params.includeBypassedPlugins,
                                                  maxNumChannels);

    return node;
}

std::unique_ptr<tracktion::graph::Node> createNodeForRackInstance (RackInstance& rackInstance, std::unique_ptr<Node> node)
{
    jassert (node != nullptr);

    if (! rackInstance.isEnabled())
        return node;

    const auto rackInputID = getRackInputBusID (rackInstance.rackTypeID);
    const auto rackOutputID = getRackOutputBusID (rackInstance.rackTypeID);
    
    // The input to the instance is referenced by the dry signal path
    auto* inputNode = node.get();
    
    // Send
    // N.B. the channel indicies from the RackInstance start a 1 so we need to subtract this to get a 0-indexed channel
    RackInstanceNode::ChannelMap sendChannelMap;
    sendChannelMap[0] = { 0, rackInstance.leftInputGoesTo - 1, rackInstance.leftInDb };
    sendChannelMap[1] = { 1, rackInstance.rightInputGoesTo - 1, rackInstance.rightInDb };
    node = makeNode<RackInstanceNode> (std::move (node), std::move (sendChannelMap));
    node = makeNode<SendNode> (std::move (node), rackInputID);
    node = makeNode<ReturnNode> (makeNode<SinkNode> (std::move (node)), rackOutputID);

    // Return
    RackInstanceNode::ChannelMap returnChannelMap;
    returnChannelMap[0] = { rackInstance.leftOutputComesFrom - 1, 0, rackInstance.leftOutDb };
    returnChannelMap[1] = { rackInstance.rightOutputComesFrom - 1, 1, rackInstance.rightOutDb };
    node = makeNode<RackInstanceNode> (std::move (node), std::move (returnChannelMap));

    return makeNode<RackReturnNode> (std::move (node),
                                     [wetGain = rackInstance.wetGain] { return wetGain->getCurrentValue(); },
                                     inputNode,
                                     [dryGain = rackInstance.dryGain] { return dryGain->getCurrentValue(); });
}

std::unique_ptr<tracktion::graph::Node> createPluginNodeForList (PluginList& list, const TrackMuteState* trackMuteState, std::unique_ptr<Node> node,
                                                                tracktion::graph::PlayHeadState& playHeadState, const CreateNodeParams& params)
{
    for (auto p : list)
    {
        if (! params.forRendering && p->isFrozen())
            continue;
        
        if (auto meterPlugin = dynamic_cast<LevelMeterPlugin*> (p))
        {
            node = makeNode<LevelMeasurerProcessingNode> (std::move (node), *meterPlugin);
        }
        else if (auto sendPlugin = dynamic_cast<AuxSendPlugin*> (p))
        {
            if (sendPlugin->isEnabled())
                node = makeNode<AuxSendNode> (std::move (node), sendPlugin->busNumber, *sendPlugin,
                                              playHeadState, trackMuteState);
        }
        else if (auto returnPlugin = dynamic_cast<AuxReturnPlugin*> (p))
        {
            if (returnPlugin->isEnabled())
                node = makeNode<ReturnNode> (std::move (node), returnPlugin->busNumber);
        }
        else if (auto rackInstance = dynamic_cast<RackInstance*> (p))
        {
            node = createNodeForRackInstance (*rackInstance, std::move (node));
        }
        else if (auto insertPlugin = dynamic_cast<InsertPlugin*> (p))
        {
            node = makeNode<InsertSendReturnDependencyNode> (std::move (node), *insertPlugin);
            node = createNodeForPlugin (*p, trackMuteState, std::move (node), params);
        }
        else
        {
            node = createNodeForPlugin (*p, trackMuteState, std::move (node), params);
        }
    }

    return node;
}

std::unique_ptr<tracktion::graph::Node> createModifierNodeForList (ModifierList& list, Modifier::ProcessingPosition position, TrackMuteState* trackMuteState,
                                                                  std::unique_ptr<Node> node,
                                                                  tracktion::graph::PlayHeadState& playHeadState, const CreateNodeParams& params)
{
    for (auto& modifier : list.getModifiers())
    {
        if (modifier->getProcessingPosition() != position)
            continue;

        node = makeNode<ModifierNode> (std::move (node), modifier, params.sampleRate, params.blockSize,
                                       trackMuteState, playHeadState, params.forRendering);
    }
    
    return node;
}

std::unique_ptr<tracktion::graph::Node> createPluginNodeForTrack (Track& t, TrackMuteState& trackMuteState, std::unique_ptr<Node> node,
                                                                 tracktion::graph::PlayHeadState& playHeadState, const CreateNodeParams& params)
{
    node = createModifierNodeForList (t.getModifierList(), Modifier::ProcessingPosition::preFX,
                                      &trackMuteState, std::move (node), playHeadState, params);
    
    if (params.includePlugins)
        node = createPluginNodeForList (t.pluginList, &trackMuteState, std::move (node), playHeadState, params);
    
    node = createModifierNodeForList (t.getModifierList(), Modifier::ProcessingPosition::postFX,
                                      &trackMuteState, std::move (node), playHeadState, params);

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

std::unique_ptr<tracktion::graph::Node> createTrackCompNode (AudioTrack& at, std::unique_ptr<tracktion::graph::Node> node, const CreateNodeParams& params)
{
    if (at.getCompGroup() == -1)
        return node;
    
    if (auto tc = at.edit.getTrackCompManager().getTrackComp (&at))
    {
        const auto crossfadeTimeMs = at.edit.engine.getPropertyStorage().getProperty (SettingID::compCrossfadeMs, 20.0);
        const auto crossfadeTime = TimeDuration::fromSeconds (static_cast<double> (crossfadeTimeMs) / 1000.0);
        
        const auto nonMuteTimes = tc->getNonMuteTimes (at, crossfadeTime);
        const auto muteTimes = TrackCompManager::TrackComp::getMuteTimes (nonMuteTimes);
        
        if (muteTimes.isEmpty())
            return node;

        node = makeNode<TimedMutingNode> (std::move (node), std::move (muteTimes), params.processState.playHeadState);

        for (auto r : nonMuteTimes)
        {
            auto fadeIn = r.withLength (crossfadeTime) - 0.0001s;
            auto fadeOut = fadeIn.movedToEndAt (r.getEnd() + 0.0001s);

            if (! (fadeIn.isEmpty() && fadeOut.isEmpty()))
                node = makeNode<FadeInOutNode> (std::move (node),
                                                params.processState,
                                                TimeRange { fadeIn.getStart(), fadeIn.getEnd() },
                                                TimeRange { fadeOut.getStart(), fadeOut.getEnd() },
                                                AudioFadeCurve::convex,
                                                AudioFadeCurve::convex, false);
        }
    }
    
    return node;
}

std::unique_ptr<tracktion::graph::Node> createNodeForAudioTrack (AudioTrack& at, const CreateNodeParams& params)
{
    CRASH_TRACER
    jassert (at.isProcessing (false));
    auto& playHeadState = params.processState.playHeadState;

    if (! params.forRendering && at.isFrozen (AudioTrack::individualFreeze))
        return createNodeForFrozenAudioTrack (at, playHeadState, params);

    auto inputTracks = getDirectInputTracks (at);
    const bool processMidiWhenMuted = at.state.getProperty (IDs::processMidiWhenMuted, false);
    auto clipsMuteState = std::make_unique<TrackMuteState> (at, true, processMidiWhenMuted);
    auto trackMuteState = std::make_unique<TrackMuteState> (at, false, processMidiWhenMuted);

    const auto& clips = at.getClips();
    std::unique_ptr<Node> node = createClipsNode (at.itemID, clips, *clipsMuteState, params);
    
    if (node)
    {
        // When recording, clips should be muted but the plugin should still be audible so use two muting Nodes
        node = makeNode<TrackMutingNode> (std::move (clipsMuteState), std::move (node), true);
        
        node = createTrackCompNode (at, std::move (node), params);
    }
    
    auto liveInputNode = createLiveInputsNode (at, playHeadState, params);
    
    if (node && ! at.getListeners().isEmpty())
        node = makeNode<LiveMidiOutputNode> (at, std::move (node));
    
    if (node)
        node = makeNode<LiveMidiInjectingNode> (at, std::move (node));

    if (node == nullptr && inputTracks.isEmpty() && liveInputNode == nullptr)
    {
        // If there are synths on the track, create a stub Node to feed them
        for (auto plugin : at.pluginList)
        {
            if (plugin->producesAudioWhenNoAudioInput())
            {
                node = makeNode<SilentNode> (2);
                break;
            }
        }
        
        if (! node)
            return {};
    }
    
    if (liveInputNode)
    {
        if (node)
        {
            auto sumNode = std::make_unique<SummingNode>();
            sumNode->addInput (std::move (node));
            sumNode->addInput (std::move (liveInputNode));
            node = std::move (sumNode);
        }
        else
        {
            node = std::move (liveInputNode);
        }
    }
    
    if (! inputTracks.isEmpty())
    {
        auto sumNode = std::make_unique<SummingNode>();
        
        if (node)
            sumNode->addInput (std::move (node));

        for (auto inputTrack : inputTracks)
            if (auto n = createNodeForTrack (*inputTrack, params))
                sumNode->addInput (std::move (n));
        
        node = std::move (sumNode);
    }
    
    node = createPluginNodeForTrack (at, *trackMuteState, std::move (node), playHeadState, params);

    if (isSidechainSource (at))
        node = makeNode<SendNode> (std::move (node), getSidechainBusID (at.itemID));

    node = makeNode<TrackMutingNode> (std::move (trackMuteState), std::move (node), false);

    if (! params.forRendering)
    {
        if (at.getWaveInputDevice().isEnabled())
            node = makeNode<SendNode> (std::move (node), getWaveInputDeviceBusID (at.itemID));

        if (at.getMidiInputDevice().isEnabled())
            node = makeNode<SendNode> (std::move (node), getMidiInputDeviceBusID (at.itemID));
    }

    return node;
}

//==============================================================================
std::unique_ptr<tracktion::graph::Node> createNodeForSubmixTrack (FolderTrack& submixTrack, const CreateNodeParams& params)
{
    CRASH_TRACER
    jassert (submixTrack.isSubmixFolder());

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
    
    auto sumNode = std::make_unique<tracktion::graph::SummingNode>();
    sumNode->setDoubleProcessingPrecision (submixTrack.edit.engine.getPropertyStorage().getProperty (SettingID::use64Bit, false));

    // Create nodes for any submix tracks
    for (auto ft : subFolderTracks)
    {
        if (params.allowedTracks != nullptr && ! params.allowedTracks->contains (ft))
            continue;

        if (! ft->isProcessing (true))
            continue;

        if (ft->isSubmixFolder())
        {
            if (auto node = createNodeForSubmixTrack (*ft, params))
                sumNode->addInput (std::move (node));
        }
        else
        {
            for (auto at : ft->getAllAudioSubTracks (false))
                if (params.allowedTracks == nullptr || params.allowedTracks->contains (at))
                    if (auto node = createNodeForAudioTrack (*at, params))
                        sumNode->addInput (std::move (node));
        }
    }

    // Then add any audio tracks
    for (auto at : subAudioTracks)
        if (params.allowedTracks == nullptr || params.allowedTracks->contains (at))
            if (at->isProcessing (true))
                if (auto node = createNodeForAudioTrack (*at, params))
                    sumNode->addInput (std::move (node));

    if (sumNode->getDirectInputNodes().empty())
        return {};
    
    // Finally the effects
    std::unique_ptr<Node> node = std::move (sumNode);
    auto trackMuteState = std::make_unique<TrackMuteState> (submixTrack, false, false);

    node = createPluginNodeForTrack (submixTrack, *trackMuteState, std::move (node), params.processState.playHeadState, params);

    node = makeNode<TrackMutingNode> (std::move (trackMuteState), std::move (node), false);

    return node;
}

//==============================================================================
std::unique_ptr<tracktion::graph::Node> createNodeForTrack (Track& track, const CreateNodeParams& params)
{
    if (auto t = dynamic_cast<AudioTrack*> (&track))
    {
        if (! t->isProcessing (true))
            return {};

        if (! t->createsOutput())
            return {};

        if (t->isPartOfSubmix() && ! shouldRenderTrackInSubmix (*t, params))
            return {};

        if (! params.forRendering && t->isFrozen (Track::groupFreeze))
            return {};

        return createNodeForAudioTrack (*t, params);
    }

    if (auto t = dynamic_cast<FolderTrack*> (&track))
    {
        if (! t->isSubmixFolder())
            return {};

        if (t->isPartOfSubmix() && ! shouldRenderTrackInSubmix (*t, params))
            return {};

        if (t->getOutput() == nullptr)
            return {};

        return createNodeForSubmixTrack (*t, params);
    }

    return {};
}

//==============================================================================
std::unique_ptr<Node> createNodeForRackType (RackType& rackType, const CreateNodeParams& params)
{
    const auto rackInputID = getRackInputBusID (rackType.rackID);
    const auto rackOutputID = getRackOutputBusID (rackType.rackID);
    
    auto rackInputNode = makeNode<ReturnNode> (rackInputID);
    auto rackNode = RackNodeBuilder::createRackNode (rackType, params.sampleRate, params.blockSize, std::move (rackInputNode),
                                                     params.processState, params.forRendering);
    auto rackOutputNode = makeNode<SendNode> (std::move (rackNode), rackOutputID);

    return makeNode<SinkNode> (std::move (rackOutputNode));
}

std::vector<std::unique_ptr<Node>> createNodesForRacks (RackTypeList& rackTypeList, const CreateNodeParams& params)
{
    std::vector<std::unique_ptr<Node>> nodes;
    
    for (auto rackType : rackTypeList.getTypes())
        if (getEnabledInstancesForRack (*rackType).size() > 0)
            if (auto rackNode = createNodeForRackType (*rackType, params))
                nodes.push_back (std::move (rackNode));
    
    return nodes;
}

std::unique_ptr<Node> createRackNode (std::unique_ptr<Node> input, RackTypeList& rackTypeList, const CreateNodeParams& params)
{
    // Finally add the RackType Nodes
    auto rackNodes = createNodesForRacks (rackTypeList, params);
    
    if (rackNodes.empty())
        return input;
    
    auto sumNode = std::make_unique<SummingNode> (std::move (rackNodes));
    sumNode->addInput (std::move (input));
    input = std::move (sumNode);
    
    return input;
}

//==============================================================================
std::unique_ptr<Node> createInsertSendNode (InsertPlugin& insert, OutputDevice& device,
                                            tracktion::graph::PlayHeadState& playHeadState,
                                            const CreateNodeParams& params)
{
    if (insert.outputDevice != device.getName())
        return {};
    
    auto sendNode = makeNode<InsertSendNode> (insert);
    
    auto getInsertReturnNode = [&] () -> std::unique_ptr<Node>
    {
        if (insert.getReturnDeviceType() != InsertPlugin::noDevice)
            for (auto i : insert.edit.getAllInputDevices())
                if (i->owner.getName() == insert.inputDevice)
                    return makeNode<InsertReturnNode> (insert, createLiveInputNodeForDevice (*i, playHeadState, params));

        return {};
    };

    if (auto returnNode = getInsertReturnNode())
        return { makeSummingNode ({ returnNode.release(), sendNode.release() }) };

    return sendNode;
}

//==============================================================================
std::unique_ptr<tracktion::graph::Node> createGroupFreezeNodeForDevice (Edit& edit, OutputDevice& device, ProcessState& processState)
{
    CRASH_TRACER

    for (auto& freezeFile : TemporaryFileManager::getFrozenTrackFiles (edit))
    {
        const auto outId = TemporaryFileManager::getDeviceIDFromFreezeFile (edit, freezeFile);

        if (device.getDeviceID() == outId)
        {
            AudioFile af (edit.engine, freezeFile);
            const auto length = TimeDuration::fromSeconds (af.getLength());

            if (length <= 0.0s)
                return {};

            auto node = tracktion::graph::makeNode<WaveNode> (af, TimeRange (0.0s, length),
                                                             0.0s, TimeRange(), LiveClipLevel(),
                                                             1.0,
                                                             juce::AudioChannelSet::stereo(),
                                                             juce::AudioChannelSet::stereo(),
                                                             processState,
                                                             EditItemID::fromRawID ((uint64_t) device.getName().hash()),
                                                             false);

            return makeNode<TrackMutingNode> (std::make_unique<TrackMuteState> (edit), std::move (node), false);
        }
    }

    return {};
}

//==============================================================================
std::unique_ptr<tracktion::graph::Node> createNodeForDevice (EditPlaybackContext& epc, OutputDevice& device, PlayHeadState& playHeadState, std::unique_ptr<Node> node)
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

        return tracktion::graph::makeNode<ChannelRemappingNode> (std::move (node), channelMap, false);
    }
    else if (auto midiInstance = dynamic_cast<MidiOutputDeviceInstance*> (epc.getOutputFor (&device)))
    {
        return tracktion::graph::makeNode<MidiOutputDeviceInstanceInjectingNode> (*midiInstance, std::move (node), playHeadState.playHead);
    }
    
    return {};
}

std::unique_ptr<tracktion::graph::Node> createMasterPluginsNode (Edit& edit, tracktion::graph::PlayHeadState& playHeadState, std::unique_ptr<Node> node, const CreateNodeParams& params)
{
    if (! params.includeMasterPlugins)
        return node;

    auto& tempoModList = edit.getTempoTrack()->getModifierList();
    auto& masterModList = edit.getMasterTrack()->getModifierList();
    node = createModifierNodeForList (tempoModList, Modifier::ProcessingPosition::preFX,
                                      nullptr, std::move (node), playHeadState, params);
    node = createModifierNodeForList (masterModList, Modifier::ProcessingPosition::preFX,
                                      nullptr, std::move (node), playHeadState, params);

    node = createPluginNodeForList (edit.getMasterPluginList(), nullptr, std::move (node), playHeadState, params);

    node = createModifierNodeForList (tempoModList, Modifier::ProcessingPosition::postFX,
                                      nullptr, std::move (node), playHeadState, params);
    node = createModifierNodeForList (masterModList, Modifier::ProcessingPosition::postFX,
                                      nullptr, std::move (node), playHeadState, params);

    if (auto masterVolPlugin = edit.getMasterVolumePlugin())
        node = createNodeForPlugin (*masterVolPlugin, nullptr, std::move (node), params);
    
    return node;
}

std::unique_ptr<tracktion::graph::Node> createMasterFadeInOutNode (Edit& edit, std::unique_ptr<Node> node, const CreateNodeParams& params)
{
    if (! params.includeMasterPlugins)
        return node;
    
    if (edit.masterFadeIn > 0_td || edit.masterFadeOut > 0_td)
    {
        auto length = edit.getLength();
        return makeNode<FadeInOutNode> (std::move (node), params.processState,
                                        TimeRange { TimePosition(), edit.masterFadeIn },
                                        TimeRange { toPosition (length) - edit.masterFadeOut, length },
                                        edit.masterFadeInType.get(),
                                        edit.masterFadeOutType.get(),
                                        true);
    }

    return node;
}

}

//==============================================================================
std::unique_ptr<tracktion::graph::Node> createNodeForEdit (EditPlaybackContext& epc, std::atomic<double>& audibleTimeToUpdate, const CreateNodeParams& params)
{
    Edit& edit = epc.edit;
    auto& playHeadState = params.processState.playHeadState;
    auto insertPlugins = getAllPluginsOfType<InsertPlugin> (edit);
    
    using TrackNodeVector = std::vector<std::unique_ptr<tracktion::graph::Node>>;
    std::map<OutputDevice*, TrackNodeVector> deviceNodes;
    std::vector<OutputDevice*> devicesWithFrozenNodes;

    for (auto t : getAllTracks (edit))
    {
        if (params.allowedTracks != nullptr && ! params.allowedTracks->contains (t))
            continue;

        if (auto output = getTrackOutput (*t))
        {
            if (auto device = output->getOutputDevice (false))
            {
                if (! device->isEnabled())
                    continue;
                
                if (! params.forRendering && t->isFrozen (Track::groupFreeze))
                {
                    if (std::find (devicesWithFrozenNodes.begin(), devicesWithFrozenNodes.end(), device)
                        != devicesWithFrozenNodes.end())
                       continue;

                    if (auto node = createGroupFreezeNodeForDevice (edit, *device, params.processState))
                    {
                        deviceNodes[device].push_back (std::move (node));
                        devicesWithFrozenNodes.push_back (device);
                    }
                }
                else if (auto node = createNodeForTrack (*t, params))
                {
                    deviceNodes[device].push_back (std::move (node));
                }
            }
        }
    }

    // Add deviceNodes for any devices only being used by InsertPlugins
    for (auto ins : insertPlugins)
    {
        if (ins->getSendDeviceType() != InsertPlugin::noDevice)
        {
            if (auto device = edit.engine.getDeviceManager().findOutputDeviceWithName (ins->outputDevice))
            {
                auto& trackNodeVector = deviceNodes[device];
                juce::ignoreUnused (trackNodeVector);
                // We don't need to add anything to the vector, just ensure the device is in the map
            }
        }
    }

    // Add deviceNodes for any devices only being used by the click track
    for (int i = edit.engine.getDeviceManager().getNumOutputDevices(); --i >= 0;)
    {
        if (auto device = edit.engine.getDeviceManager().getOutputDeviceAt (i))
        {
            if (! edit.isClickTrackDevice (*device))
                continue;
            
            auto& trackNodeVector = deviceNodes[device];
            juce::ignoreUnused (trackNodeVector);
            // We don't need to add anything to the vector, just ensure the device is in the map
        }
    }


    auto outputNode = std::make_unique<tracktion::graph::SummingNode>();
        
    for (auto& deviceAndTrackNode : deviceNodes)
    {
        auto device = deviceAndTrackNode.first;
        jassert (device != nullptr);
        auto tracksVector = std::move (deviceAndTrackNode.second);
        
        auto sumNode = std::make_unique<SummingNode> (std::move (tracksVector));
        sumNode->setDoubleProcessingPrecision (edit.engine.getPropertyStorage().getProperty (SettingID::use64Bit, false));

        // Create nodes for any insert plugins
        bool deviceIsBeingUsedAsInsert = false;

        for (auto ins : insertPlugins)
        {
            if (ins->isFrozen())
                continue;
            
            if (ins->outputDevice != device->getName())
                continue;

            if (auto sendNode = createInsertSendNode (*ins, *device, playHeadState, params))
            {
                sumNode->addInput (std::move (sendNode));
                deviceIsBeingUsedAsInsert = true;
            }
        }
        
        std::unique_ptr<Node> node = std::move (sumNode);

        if (! deviceIsBeingUsedAsInsert)
        {
            if (edit.engine.getDeviceManager().getDefaultWaveOutDevice() == device)
                node = createMasterPluginsNode (edit, playHeadState, std::move (node), params);
            
            node = createMasterFadeInOutNode (edit, std::move (node), params);
            node = EditNodeBuilder::insertOptionalLastStageNode (std::move (node));

            if (edit.getIsPreviewEdit() && node != nullptr)
                if (auto previewMeasurer = edit.getPreviewLevelMeasurer())
                    node = makeNode<SharedLevelMeasuringNode> (std::move (previewMeasurer), std::move (node));
        }
        
        if (edit.isClickTrackDevice (*device))
        {
            auto clickAndTracksNode = makeSummingNode ({ node.release(),
                                                         makeNode<ClickNode> (edit, getNumChannelsFromDevice (*device),
                                                                              device->isMidi(), playHeadState.playHead).release() });
            node = std::move (clickAndTracksNode);
        }

        if (auto outputDeviceNode = createNodeForDevice (epc, *device, playHeadState, std::move (node)))
            outputNode->addInput (std::move (outputDeviceNode));
    }
    
    std::unique_ptr<Node> finalNode (std::move (outputNode));
    finalNode = makeNode<LevelMeasuringNode> (std::move (finalNode), epc.masterLevels);
    finalNode = createRackNode (std::move (finalNode), edit.getRackList(), params);
    finalNode = makeNode<PlayHeadPositionNode> (params.processState, std::move (finalNode), audibleTimeToUpdate);
    
    return finalNode;
}

std::unique_ptr<tracktion::graph::Node> createNodeForEdit (Edit& edit, const CreateNodeParams& originalParams)
{
    std::vector<std::unique_ptr<tracktion::graph::Node>> trackNodes;
    auto params = originalParams;
    auto& playHeadState = params.processState.playHeadState;
    
    if (params.implicitlyIncludeSubmixChildTracks && params.allowedTracks != nullptr)
        *params.allowedTracks = addImplicitSubmixChildTracks (*params.allowedTracks);

    for (auto t : getAllTracks (edit))
    {
        if (params.allowedTracks != nullptr && ! params.allowedTracks->contains (t))
            continue;

        // Skip tracks that don't output to a device or feed in to other tracks
        if (auto output = getTrackOutput (*t))
        {
            if (output->getDestinationTrack() != nullptr)
                continue;
        }
        else
        {
            continue;
        }

        if (auto node = createNodeForTrack (*t, params))
            trackNodes.push_back (std::move (node));
    }

    auto sumNode = std::make_unique<SummingNode> (std::move (trackNodes));
    sumNode->setDoubleProcessingPrecision (edit.engine.getPropertyStorage().getProperty (SettingID::use64Bit, false));

    auto node = std::unique_ptr<Node> (std::move (sumNode));
    node = createMasterPluginsNode (edit, playHeadState, std::move (node), params);
    node = createMasterFadeInOutNode (edit, std::move (node), params);
    node = createRackNode (std::move (node), edit.getRackList(), params);

    return node;
}

std::function<std::unique_ptr<tracktion::graph::Node> (std::unique_ptr<tracktion::graph::Node>)> EditNodeBuilder::insertOptionalLastStageNode
    = [] (std::unique_ptr<tracktion::graph::Node> input) { return input; };

}} // namespace tracktion { inline namespace engine
