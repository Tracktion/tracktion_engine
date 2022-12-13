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
/**
    Performs a tempo detection task on a background thread.
*/
class AudioClipBase::TempoDetectTask   : public ThreadPoolJobWithProgress
{
public:
    /** Creates a task for a given file. */
    TempoDetectTask (Engine& e, const juce::File& file)
        : ThreadPoolJobWithProgress (TRANS("Detecting tempo")),
          engine (e), sourceFile (file)
    {
    }

    /** Returns the bpm after a successful detection. */
    float getBpm()                                      { return bpm; }

    /** Returns true if the result was within a sensible range. */
    bool isResultSensible()                             { return isSensible; }

    //==============================================================================
    /** Performs the actual detection. */
    JobStatus runJob() override
    {
        std::unique_ptr<juce::AudioFormatReader> reader (AudioFileUtils::createReaderFor (engine, sourceFile));

        if (reader == nullptr)
            return jobHasFinished;

        auto numChannels = (int) reader->numChannels;
        auto numSamples = reader->lengthInSamples;
        auto sampleRate = reader->sampleRate;

        if (numSamples <= 0)
            return jobHasFinished;

        // main detection loop
        {
            TempoDetect detector (numChannels, sampleRate);
            const int blockSize = 65536;
            const bool useRightChan = numChannels > 1;

            // can't use an AudioScratchBuffer yet
            juce::AudioBuffer<float> buffer (numChannels, blockSize);

            auto numLeft = numSamples;
            SampleCount startSample = 0;

            while (numLeft > 0)
            {
                if (shouldExit())
                    return jobHasFinished;

                auto numThisTime = (int) std::min ((SampleCount) numLeft, (SampleCount) blockSize);
                reader->read (&buffer, 0, numThisTime, startSample, true, useRightChan);
                detector.processSection (buffer, numThisTime);

                startSample += numThisTime;
                numLeft -= numThisTime;
                progress = numLeft / (float) numSamples;
            }

            bpm = detector.finishAndDetect();
            isSensible = bpm > 0;
        }

        return jobHasFinished;
    }

    /** Returns the current progress. */
    float getCurrentTaskProgress() override             { return progress; }

private:
    //==============================================================================
    Engine& engine;
    juce::File sourceFile;
    float progress = 0;
    bool isSensible = false;
    float bpm = 12.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TempoDetectTask)
};


//==============================================================================
class ProxyGeneratorJob  : public AudioProxyGenerator::GeneratorJob
{
public:
    ProxyGeneratorJob (const AudioFile& o, const AudioFile& p,
                       AudioClipBase& acb, bool renderTimestretched)
        : GeneratorJob (p), engine (acb.edit.engine), original (o)
    {
        setName (TRANS("Creating Proxy") + ": " + acb.getName());

        if (renderTimestretched)
            proxyInfo = acb.createProxyRenderingInfo();
    }

private:
    Engine& engine;
    AudioFile original;
    std::unique_ptr<AudioClipBase::ProxyRenderingInfo> proxyInfo;

    bool render()
    {
        CRASH_TRACER

        AudioFile tempFile (engine, proxy.getFile()
                                     .getSiblingFile ("temp_proxy_" + juce::String::toHexString (juce::Random().nextInt64()))
                                     .withFileExtension (proxy.getFile().getFileExtension()));

        bool ok = render (tempFile);

        if (ok)
        {
            ok = proxy.deleteFile();
            (void) ok;
            jassert (ok);
            ok = tempFile.getFile().moveFileTo (proxy.getFile());
            jassert (ok);
        }

        tempFile.deleteFile();

        engine.getAudioFileManager().releaseFile (proxy);
        return ok;
    }

    bool render (const AudioFile& tempFile)
    {
        CRASH_TRACER
        AudioFileInfo sourceInfo (original.getInfo());

        // need to strip AIFF metadata to write to wav files
        if (sourceInfo.metadata.getValue ("MetaDataSource", "None") == "AIFF")
            sourceInfo.metadata.clear();

        AudioFileWriter writer (tempFile, engine.getAudioFileFormatManager().getWavFormat(),
                                sourceInfo.numChannels, sourceInfo.sampleRate,
                                std::max (16, sourceInfo.bitsPerSample),
                                sourceInfo.metadata, 0);

        return writer.isOpen()
                && (proxyInfo != nullptr ? proxyInfo->render (engine, original, writer, this, progress)
                                         : renderNormalSpeed  (writer));
    }

    bool renderNormalSpeed (AudioFileWriter& writer)
    {
        CRASH_TRACER
        std::unique_ptr<juce::AudioFormatReader> reader (AudioFileUtils::createReaderFor (engine, original.getFile()));

        if (reader == nullptr)
            return false;

        SampleCount sourceSample = 0;
        auto samplesToDo = (SampleCount) reader->lengthInSamples;

        while (! shouldExit())
        {
            auto numThisTime = (int) std::min (samplesToDo, (SampleCount) 65536);

            if (numThisTime <= 0)
                return true;

            if (! writer.writeFromAudioReader (*reader, sourceSample, numThisTime))
                break;

            samplesToDo -= numThisTime;
            sourceSample += numThisTime;

            progress = juce::jlimit (0.0f, 1.0f, (float) (sourceSample / (double) reader->lengthInSamples));
        }

        return false;
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProxyGeneratorJob)
};

//==============================================================================
AudioClipBase::AudioClipBase (const juce::ValueTree& v, EditItemID id, Type t, ClipTrack& targetTrack)
    : Clip (v, targetTrack, id, t),
      loopInfo (edit.engine, state.getOrCreateChildWithName (IDs::LOOPINFO, getUndoManager()), getUndoManager()),
      pluginList (targetTrack.edit),
      lastProxy (targetTrack.edit.engine)
{
    auto um = getUndoManager();

    level->dbGain.referTo (state, IDs::gain, um);
    level->pan.referTo (state, IDs::pan, um);
    level->mute.referTo (state, IDs::mute, um);
    channels.referTo (state, IDs::channels, um, juce::AudioChannelSet::stereo().getSpeakerArrangementAsString());

    if (channels.get().isEmpty())
        channels = juce::AudioChannelSet::stereo().getSpeakerArrangementAsString();

    fadeIn.referTo (state, IDs::fadeIn, um);
    fadeOut.referTo (state, IDs::fadeOut, um);

    fadeInType.referTo (state, IDs::fadeInType, um, AudioFadeCurve::linear);
    fadeOutType.referTo (state, IDs::fadeOutType, um, AudioFadeCurve::linear);
    autoCrossfade.referTo (state, IDs::autoCrossfade, um);

    fadeInBehaviour.referTo (state, IDs::fadeInBehaviour, um, gainFade);
    fadeOutBehaviour.referTo (state, IDs::fadeOutBehaviour, um, gainFade);

    loopStart.referTo (state, IDs::loopStart, um);
    loopLength.referTo (state, IDs::loopLength, um);

    loopStartBeats.referTo (state, IDs::loopStartBeats, um);
    loopLengthBeats.referTo (state, IDs::loopLengthBeats, um);

    resamplingQuality.referTo (state, IDs::resamplingQuality, um, ResamplingQuality::lagrange);
    proxyAllowed.referTo (state, IDs::proxyAllowed, um, true);
    transpose.referTo (state, IDs::transpose, um);
    pitchChange.referTo (state, IDs::pitchChange, um);

    beatSensitivity.referTo (state, IDs::beatSensitivity, um, 0.5f);

    timeStretchMode.referTo (state, IDs::elastiqueMode, um);
    elastiqueProOptions.referTo (state, IDs::elastiqueOptions, um);

    // Keep this in to handle old edits..
    if (state.getProperty (IDs::timeStretch))
        timeStretchMode = juce::VariantConverter<TimeStretcher::Mode>::fromVar (state.getProperty (IDs::stretchMode));

    timeStretchMode = TimeStretcher::checkModeIsAvailable (timeStretchMode);

    autoPitch.referTo (state, IDs::autoPitch, um);
    autoPitchMode.referTo (state, IDs::autoPitchMode, um);
    autoTempo.referTo (state, IDs::autoTempo, um);
    warpTime.referTo (state, IDs::warpTime, um);
    isReversed.referTo (state, IDs::isReversed, um);
    autoDetectBeats.referTo (state, IDs::autoDetectBeats, um);

    level->pan = juce::jlimit (-1.0f, 1.0f, static_cast<float> (level->pan.get()));
    checkFadeLengthsForOverrun();

    clipEffectsVisible.referTo (state, IDs::effectsVisible, nullptr);
    updateClipEffectsState();

    updateLeftRightChannelActivenessFlags();

    pluginList.setTrackAndClip (getTrack(), this);
    pluginList.initialise (state);

    asyncFunctionCaller.addFunction (updateCrossfadesFlag, [this]           { updateAutoCrossfades (false); });
    asyncFunctionCaller.addFunction (updateCrossfadesOverlappedFlag, [this] { updateAutoCrossfades (true); });

    auto pgen = state.getChildWithName (IDs::PATTERNGENERATOR);

    if (pgen.isValid())
        patternGenerator.reset (new PatternGenerator (*this, pgen));
}

AudioClipBase::~AudioClipBase()
{
    melodyneProxy = nullptr;

    if (renderJob != nullptr)
        renderJob->removeListener (this);
}

//==============================================================================
void AudioClipBase::initialise()
{
    Clip::initialise();

    setCurrentSourceFile (sourceFileReference.getFile());

    stopTimer(); // prevent proxy generation unless we're actually going to be played.

    if (shouldAttemptRender())
    {
        auto audioFile = RenderManager::getAudioFileForHash (edit.engine, edit.getTempDirectory (false), getHash());

        if (currentSourceFile != audioFile.getFile())
            setCurrentSourceFile (audioFile.getFile());
    }

    callBlocking ([this] { setLoopDefaults(); });
}

void AudioClipBase::cloneFrom (Clip* c)
{
    if (auto other = dynamic_cast<AudioClipBase*> (c))
    {
        Clip::cloneFrom (other);

        const bool wasLooping = loopLengthBeats.get() > BeatDuration() || loopLength.get() > TimeDuration();

        level->dbGain       .setValue (other->level->dbGain, nullptr);
        level->pan          .setValue (other->level->pan, nullptr);
        level->mute         .setValue (other->level->mute, nullptr);
        channels            .setValue (other->channels, nullptr);
        fadeIn              .setValue (other->fadeIn, nullptr);
        fadeOut             .setValue (other->fadeOut, nullptr);
        fadeInType          .setValue (other->fadeInType, nullptr);
        fadeOutType         .setValue (other->fadeOutType, nullptr);
        autoCrossfade       .setValue (other->autoCrossfade, nullptr);
        fadeInBehaviour     .setValue (other->fadeInBehaviour, nullptr);
        fadeOutBehaviour    .setValue (other->fadeOutBehaviour, nullptr);
        loopStart           .setValue (other->loopStart, nullptr);
        loopLength          .setValue (other->loopLength, nullptr);
        loopStartBeats      .setValue (other->loopStartBeats, nullptr);
        loopLengthBeats     .setValue (other->loopLengthBeats, nullptr);
        transpose           .setValue (other->transpose, nullptr);
        pitchChange         .setValue (other->pitchChange, nullptr);
        beatSensitivity     .setValue (other->beatSensitivity, nullptr);
        timeStretchMode     .setValue (other->timeStretchMode, nullptr);
        elastiqueProOptions .setValue (other->elastiqueProOptions, nullptr);
        autoPitch           .setValue (other->autoPitch, nullptr);
        autoPitchMode       .setValue (other->autoPitchMode, nullptr);
        autoTempo           .setValue (other->autoTempo, nullptr);
        isReversed          .setValue (other->isReversed, nullptr);
        autoDetectBeats     .setValue (other->autoDetectBeats, nullptr);
        warpTime            .setValue (other->warpTime, nullptr);
        proxyAllowed        .setValue (other->proxyAllowed, nullptr);
        resamplingQuality   .setValue (other->resamplingQuality, nullptr);

        copyValueTree (loopInfo.state, other->loopInfo.state, nullptr);

        const bool isLooping = loopLengthBeats.get() > BeatDuration() || loopLength.get() > TimeDuration();

        if (! isLooping && wasLooping)
            disableLooping();

        Selectable::changed();
    }
}

void AudioClipBase::updateLeftRightChannelActivenessFlags()
{
    juce::String channelMask = channels;

    if (channelMask.isEmpty())
        activeChannels = juce::AudioChannelSet::disabled();

    if (channels == "r")        activeChannels.addChannel (juce::AudioChannelSet::right);
    else if (channels == "l")   activeChannels.addChannel (juce::AudioChannelSet::left);
    else                        activeChannels = channelSetFromSpeakerArrangmentString (channelMask);
}

void AudioClipBase::flushStateToValueTree()
{
    Clip::flushStateToValueTree();

    if (clipEffects != nullptr)
        clipEffects->flushStateToValueTree();
}

PatternGenerator* AudioClipBase::getPatternGenerator()
{
    if (! state.getChildWithName (IDs::PATTERNGENERATOR).isValid())
        state.addChild (juce::ValueTree (IDs::PATTERNGENERATOR), -1, &edit.getUndoManager());

    jassert (patternGenerator != nullptr);
    return patternGenerator.get();
}

//==============================================================================
void AudioClipBase::setTrack (ClipTrack* t)
{
    Clip::setTrack (t);

    pluginList.setTrackAndClip (track != nullptr ? getTrack() : nullptr, this);
}

bool AudioClipBase::canGoOnTrack (Track& t)
{
    return t.canContainAudio();
}

void AudioClipBase::changed()
{
    clearCachedAudioSegmentList();
    Clip::changed();

    createNewProxyAsync();

    if (melodyneProxy != nullptr)
        melodyneProxy->sourceClipChanged();
}

juce::Colour AudioClipBase::getDefaultColour() const
{
    return juce::Colours::red.withHue (0.0f);
}

//==============================================================================
TimeDuration AudioClipBase::getMaximumLength()
{
    if (! isLooping())
    {
        if (getSourceLength() <= 0_td)
            return 100000.0_td;

        if (getAutoTempo())
            return edit.tempoSequence.toTime (getStartBeat() + BeatDuration::fromBeats (loopInfo.getNumBeats()))
                     - getPosition().getStart();

        return getSourceLength() / speedRatio;
    }

    return Edit::getMaximumLength();
}

//==============================================================================
void AudioClipBase::setGainDB (float g)
{
    level->dbGain = juce::jlimit (-100.0f, 24.0f, g);
}

void AudioClipBase::setPan (float p)
{
    level->pan = std::abs (p) < 0.01 ? 0.0f
                                     : juce::jlimit (-1.0f, 1.0f, p);
}

//==============================================================================
void AudioClipBase::setLeftChannelActive (bool b)
{
    if (isLeftChannelActive() != b)
    {
        auto set = activeChannels;

        if (b)
        {
            set.addChannel (juce::AudioChannelSet::left);
        }
        else
        {
            set.removeChannel (juce::AudioChannelSet::left);

            if (set.size() == 0)
                set.addChannel (juce::AudioChannelSet::right);
        }

        channels = set.getSpeakerArrangementAsString();
    }
}

bool AudioClipBase::isLeftChannelActive() const
{
    return activeChannels.size() == 0 || activeChannels.getChannelIndexForType (juce::AudioChannelSet::left) != -1;
}

void AudioClipBase::setRightChannelActive (bool b)
{
    if (isRightChannelActive() != b)
    {
        auto set = activeChannels;

        if (b)
        {
            set.addChannel (juce::AudioChannelSet::right);
        }
        else
        {
            set.removeChannel (juce::AudioChannelSet::right);

            if (set.size() == 0)
                set.addChannel (juce::AudioChannelSet::left);
        }

        channels = set.getSpeakerArrangementAsString();
    }
}

bool AudioClipBase::isRightChannelActive() const
{
    return activeChannels.size() == 0 || activeChannels.getChannelIndexForType (juce::AudioChannelSet::right) != -1;
}

//==============================================================================
bool AudioClipBase::setFadeIn (TimeDuration in)
{
    auto len = getPosition().getLength();
    in = juce::jlimit (TimeDuration(), len, in);

    // check the fades don't overrun
    if (in + fadeOut > len)
    {
        const double scale = len / (in + fadeOut);
        fadeIn = in * scale;
        fadeOut = fadeOut * scale;
    }
    else if (fadeIn != in)
    {
        fadeIn = in;
        return false;
    }

    return false;
}

bool AudioClipBase::setFadeOut (TimeDuration out)
{
    auto len = getPosition().getLength();
    out = juce::jlimit (TimeDuration(), len, out);

    if (fadeIn + out > len)
    {
        const double scale = len / (fadeIn + out);
        fadeIn = fadeIn * scale;
        fadeOut = out * scale;
    }
    else if (fadeOut != out)
    {
        fadeOut = out;
        return false;
    }

    return false;
}

TimeDuration AudioClipBase::getFadeIn() const
{
    if (autoCrossfade && getOverlappingClip (ClipDirection::previous) != nullptr)
        return autoFadeIn;

    auto len = getPosition().getLength();

    if (fadeIn + fadeOut > len)
        return TimeDuration::fromSeconds (fadeIn * len.inSeconds() / (fadeIn + fadeOut));

    return fadeIn;
}

TimeDuration AudioClipBase::getFadeOut() const
{
    if (autoCrossfade && getOverlappingClip (ClipDirection::next) != nullptr)
        return autoFadeOut;

    auto len = getPosition().getLength();

    if (fadeIn + fadeOut > len)
        return TimeDuration::fromSeconds (fadeOut * len.inSeconds() / (fadeIn + fadeOut));

    return fadeOut;
}

void AudioClipBase::setFadeInType (AudioFadeCurve::Type t)
{
    if (t != fadeInType)
        fadeInType = t;
    else
        changed(); // keep this, in case they press a fade button twice
}

void AudioClipBase::setFadeOutType (AudioFadeCurve::Type t)
{
    if (t != fadeOutType)
        fadeOutType = t;
    else
        changed(); // keep this, in case they press a fade button twice
}

//==============================================================================
TimeStretcher::Mode AudioClipBase::getTimeStretchMode() const noexcept
{
    return timeStretchMode;
}

TimeStretcher::Mode AudioClipBase::getActualTimeStretchMode() const noexcept
{
    TimeStretcher::Mode ts = timeStretchMode;

    if (ts == TimeStretcher::disabled && (getAutoPitch() || getAutoTempo() || getPitchChange() != 0.0f))
        ts = TimeStretcher::defaultMode;

    return ts;
}

//==============================================================================
void AudioClipBase::reverseLoopPoints()
{
    // Arrh! Another horrible hack! Because we need to reverse the loop points BEFORE we've actually
    // generated the new source file we don't have a valid WaveInfo object. We therefore have to bodge
    // this like the EditClips to find the number of samples. A better approach would be to have the
    // LoopInfo use only times, then they would be file agnostic and could be manipulated at any point.
    AudioFileInfo wi = getWaveInfo();

    if (isReversed)
        if (auto sourceItem = sourceFileReference.getSourceProjectItem())
            wi = AudioFile (edit.engine, sourceItem->getSourceFile()).getInfo();

    if (wi.lengthInSamples == 0)
        return;

    auto ratio = getSpeedRatio();
    const bool beatBased = getAutoTempo();

    if (beatBased)
    {
        auto bps = edit.tempoSequence.getBeatsPerSecondAt (getPosition().getStart());
        ratio = bps / std::max (1.0, loopInfo.getBeatsPerSecond (wi));
    }

    jassert (ratio >= 0.1);

    // To find the new offset we need to work out the time in the source file at the end of the clip.
    // Then we need to reverse that and set it as the new offset. This is complicated by the fact that
    // the loop length could be beat based.

    if (isLooping())
    {
        // Reverse white loop points...
        auto sourceEnd = toPosition (getSourceLength() / ratio);
        auto o = getLoopRange();
        auto n = TimeRange::between (toPosition (sourceEnd - o.getEnd()),
                                     toPosition (sourceEnd - o.getStart()));
        setLoopRange (n);

        // then the offset
        if (o.getLength() > TimeDuration())
        {
            auto clipOffset = std::fmod (getPosition().getOffset().inSeconds(), sourceEnd.inSeconds());
            auto numLoops = getPosition().getLength() / o.getLength();
            numLoops += clipOffset / o.getLength().inSeconds();
            numLoops = numLoops - (int) numLoops;

            auto posAtEnd = o.getStart() + (o.getLength() * numLoops);
            auto newOffset = sourceEnd - (posAtEnd - n.getStart());

            setOffset (toDuration (newOffset));
        }
    }
    else
    {
        // reverse offset
        auto sourceEnd = toPosition (getSourceLength() / ratio);
        auto newOffset = sourceEnd - (toPosition (getPosition().getLength()) - getPosition().getOffset());
        setOffset (newOffset);
    }

    // red in/out markers
    const SampleCount newIn = loopInfo.getOutMarker() > -1 ? (wi.lengthInSamples - loopInfo.getOutMarker()) : 0;
    const SampleCount newOut = loopInfo.getInMarker() == 0 ? -1 : (wi.lengthInSamples - loopInfo.getInMarker());

    loopInfo.setInMarker (newIn);
    loopInfo.setOutMarker (newOut);

    // beat loop points e.g. REX files
    for (int i = loopInfo.getNumLoopPoints(); --i >= 0;)
    {
        const LoopInfo::LoopPoint p = loopInfo.getLoopPoint (i);
        loopInfo.changeLoopPoint (i, wi.lengthInSamples - p.pos, p.type);
    }
}

void AudioClipBase::checkFadeLengthsForOverrun()
{
    auto len = getPosition().getLength();

    // check the fades don't overrun
    if (fadeIn + fadeOut > len)
    {
        const double scale = len / (fadeIn + fadeOut);
        fadeIn = fadeIn * scale;
        fadeOut = fadeOut * scale;
    }

    // also check the auto fades
    if (autoFadeIn + autoFadeOut > len)
    {
        const double scale = len / (autoFadeIn + autoFadeOut);
        autoFadeIn = autoFadeIn * scale;
        autoFadeOut = autoFadeOut * scale;
    }
}

AudioClipBase* AudioClipBase::getOverlappingClip (ClipDirection direction) const
{
    CRASH_TRACER

    if (auto ct = getClipTrack())
    {
        auto clips = ct->getClips();
        auto ourIndex = clips.indexOf (const_cast<AudioClipBase*> (this));

        if (direction == ClipDirection::next)
        {
            for (int i = ourIndex + 1; i < clips.size(); ++i)
                if (auto c = dynamic_cast<AudioClipBase*> (clips[i]))
                    if (getPosition().time.contains (c->getPosition().getStart() + TimeDuration::fromSeconds (0.001))
                         && ! getPosition().time.contains (c->getPosition().getEnd()))
                        return c;
        }
        else if (direction == ClipDirection::previous)
        {
            for (int i = ourIndex; --i >= 0;)
                if (auto c = dynamic_cast<AudioClipBase*> (clips[i]))
                    if (getPosition().time.contains (c->getPosition().getEnd() - TimeDuration::fromSeconds (0.001))
                         && ! getPosition().time.contains (c->getPosition().getStart()))
                        return c;
        }
    }

    return {};
}

void AudioClipBase::updateAutoCrossfadesAsync (bool updateOverlapped)
{
    asyncFunctionCaller.updateAsync (updateOverlapped ? updateCrossfadesOverlappedFlag
                                                      : updateCrossfadesFlag);
}

void AudioClipBase::updateAutoCrossfades (bool updateOverlapped)
{
    CRASH_TRACER

    auto prevClip = getOverlappingClip (ClipDirection::previous);
    auto nextClip = getOverlappingClip (ClipDirection::next);

    if (updateOverlapped)
    {
        if (prevClip != nullptr)
            prevClip->updateAutoCrossfades (false);

        if (nextClip != nullptr)
            nextClip->updateAutoCrossfades (false);
    }

    if (autoCrossfade)
    {
        autoFadeIn  = (prevClip != nullptr) ? (prevClip->getPosition().getEnd() - getPosition().getStart()) : fadeIn;
        autoFadeOut = (nextClip != nullptr) ? (getPosition().getEnd() - nextClip->getPosition().getStart()) : fadeOut;

        checkFadeLengthsForOverrun();
    }
}

void AudioClipBase::applyEdgeFades()
{
    const auto fade = TimeDuration::fromSeconds (0.005);

    if (fadeIn < fade)  setFadeIn (fade);
    if (fadeOut < fade) setFadeOut (fade);
}

void AudioClipBase::copyFadeToAutomation (bool useFadeIn, bool removeClipFade)
{
    CRASH_TRACER

    TimeRange fadeTime (TimePosition(), useFadeIn ? getFadeIn() : getFadeOut());

    if (useFadeIn)
        fadeTime = fadeTime.movedToStartAt (getPosition().getStart());
    else
        fadeTime = fadeTime.movedToEndAt (getPosition().getEnd());

    auto& ui = edit.engine.getUIBehaviour();

    if (fadeTime.isEmpty())
    {
        ui.showWarningMessage (TRANS("Could not create automation.")
                                + juce::newLine + juce::newLine
                                + TRANS("No fade found for this clip"));
        return;
    }

    auto at = dynamic_cast<AudioTrack*> (getTrack());

    if (at == nullptr)
        return;

    AutomatableParameter::Ptr param;

    if (auto vol = at->getVolumePlugin())
        param = vol->volParam;

    if (param == nullptr)
    {
        ui.showWarningMessage (TRANS("Could not create automation.")
                                 + juce::newLine + juce::newLine
                                 + TRANS("No volume plguin was found for this track, please insert one and try again"));
        return;
    }

    auto& oldCurve = param->getCurve();

    if (oldCurve.countPointsInRegion (fadeTime) > 0)
    {
        if (! ui.showOkCancelAlertBox (TRANS("Overwrite Existing Automation?"),
                                       TRANS("There is already automation in this region, applying the curve will overwrite it. Is this OK?")))
            return;
    }

    AutomationCurve curve;
    curve.setOwnerParameter (param.get());

    auto curveType = useFadeIn ? getFadeInType() : getFadeOutType();
    auto startValue = useFadeIn ? 0.0f : oldCurve.getValueAt (fadeTime.getStart());
    auto endValue   = useFadeIn ? oldCurve.getValueAt (fadeTime.getEnd()) : 0.0f;
    auto valueLimits = juce::Range<float>::between (startValue, endValue);

    switch (curveType)
    {
        case AudioFadeCurve::convex:
        case AudioFadeCurve::concave:
        case AudioFadeCurve::sCurve:
        {
            for (int i = 0; i < 10; ++i)
            {
                auto alpha = i / 9.0f;
                auto time = toPosition (fadeTime.getLength()) * alpha;

                if (! useFadeIn)
                    alpha = 1.0f - alpha;

                auto volCurveGain = AudioFadeCurve::alphaToGainForType (curveType, alpha);
                auto value = valueLimits.getStart() + (volCurveGain * valueLimits.getLength());
                curve.addPoint (time, (float) value, 0.0f);
            }

            break;
        }

        case AudioFadeCurve::linear:
        default:
        {
            curve.addPoint (TimePosition(), useFadeIn ? valueLimits.getStart() : valueLimits.getLength(), 0.0f);
            curve.addPoint (toPosition (fadeTime.getLength()), useFadeIn ? valueLimits.getLength() : valueLimits.getStart(), 0.0f);
            break;
        }
    }

    oldCurve.mergeOtherCurve (curve, fadeTime, TimePosition(), TimeDuration(), true, true);

    // also need to remove the point just before the first one we added
    if (useFadeIn && (oldCurve.countPointsInRegion ({ {}, fadeTime.getStart() + (fadeTime.getLength() * 0.09) }) == 2))
        oldCurve.removePoint (0);

    if (removeClipFade)
    {
        if (useFadeIn)
            setFadeIn ({});
        else
            setFadeOut ({});
    }

    at->setCurrentlyShownAutoParam (param);
}

void AudioClipBase::setLoopInfo (const LoopInfo& loopInfo_)
{
    loopInfo = loopInfo_;
}

void AudioClipBase::setNumberOfLoops (int num)
{
    if (! canLoop())
        num = 0;

    auto pos = getPosition();
    auto len = std::min (getSourceLength() / speedRatio, pos.getLength());

    if (len <= TimeDuration())
        return;

    if (autoTempo)
    {
        auto& ts = edit.tempoSequence;
        auto newStart = BeatPosition::fromBeats (pos.getOffset().inSeconds() * ts.getBeatsPerSecondAt (pos.getStart()));
        setLoopRangeBeats ({ newStart, newStart + getLengthInBeats() });
        setLength (pos.getLength() * num, true);
    }
    else
    {
        setLoopRange ({ toPosition (pos.getOffset()), toPosition (pos.getOffset()) + len });
        setLength (len * num, true);
    }

    setOffset ({});
}

void AudioClipBase::disableLooping()
{
    auto pos = getPosition();

    if (autoTempo)
    {
        pos.time = pos.time.withEnd (getTimeOfRelativeBeat (loopLengthBeats));
        pos.offset = toDuration (getTimeOfRelativeBeat (toDuration (loopStartBeats.get())) - toDuration (pos.getStart()));
    }
    else
    {
        pos.time = pos.time.withEnd (pos.time.getStart() + loopLength.get());
        pos.offset = toDuration (loopStart);
    }

    setLoopRange ({});
    setPosition (pos);
    
    if (getPosition().getLength() > getMaximumLength())
        setLength (getMaximumLength(), true);
}

TimeRange AudioClipBase::getLoopRange() const
{
    if (! beatBasedLooping())
        return { loopStart, loopStart + loopLength };

    auto bps = edit.tempoSequence.getBeatsPerSecondAt (getPosition().getStart());

    return { TimePosition::fromSeconds (loopStartBeats.get().inBeats() / bps),
             TimePosition::fromSeconds ((loopStartBeats + loopLengthBeats).inBeats() / bps) };
}

TimePosition AudioClipBase::getLoopStart() const
{
    if (! beatBasedLooping())
        return loopStart;

    return TimePosition::fromSeconds (loopStartBeats.get().inBeats() / edit.tempoSequence.getBeatsPerSecondAt (getPosition().getStart()));
}

TimeDuration AudioClipBase::getLoopLength() const
{
    if (! beatBasedLooping())
        return loopLength;

    return TimeDuration::fromSeconds (loopLengthBeats.get().inBeats() / edit.tempoSequence.getBeatsPerSecondAt (getPosition().getStart()));
}

BeatPosition AudioClipBase::getLoopStartBeats() const
{
    if (beatBasedLooping())
        return loopStartBeats;

    return BeatPosition::fromBeats (loopStart.get().inSeconds() * edit.tempoSequence.getBeatsPerSecondAt (getPosition().getStart()));
}

BeatDuration AudioClipBase::getLoopLengthBeats() const
{
    if (beatBasedLooping())
        return loopLengthBeats;

    return BeatDuration::fromBeats (loopLength.get().inSeconds() * edit.tempoSequence.getBeatsPerSecondAt (getPosition().getStart()));
}

void AudioClipBase::setLoopRange (TimeRange newRange)
{
    if (autoTempo)
    {
        auto pos = getPosition();
        auto& ts = edit.tempoSequence;
        auto newStart = BeatPosition::fromBeats (newRange.getStart().inSeconds() * ts.getBeatsPerSecondAt (pos.getStart()));
        auto newLength = ts.toBeats (pos.getStart() + newRange.getLength()) - ts.toBeats (pos.getStart());
        setLoopRangeBeats ({ newStart, newStart + newLength });
    }
    else
    {
        auto sourceLen = getSourceLength();
        jassert (sourceLen > 0s);

        // limits the number of times longer than the source file length the loop length can be
        const double maxMultiplesOfSourceLengthForLooping = 50.0;

        auto newStart  = juce::jlimit (0_tp, toPosition (sourceLen) / getSpeedRatio(), newRange.getStart());
        auto newLength = juce::jlimit (0_td, sourceLen * maxMultiplesOfSourceLengthForLooping / getSpeedRatio(), newRange.getLength());

        if (loopStart != newStart || loopLength != newLength)
        {
            loopStart = newStart;
            loopLength = newLength;
        }
    }
}

void AudioClipBase::setLoopRangeBeats (BeatRange newRangeBeats)
{
    auto newStartBeat  = juce::jlimit (BeatPosition(), BeatPosition::fromBeats (loopInfo.getNumBeats()), newRangeBeats.getStart());
    auto newLengthBeat = juce::jlimit (BeatDuration(), BeatDuration::fromBeats (loopInfo.getNumBeats() * 2), newRangeBeats.getLength());

    if (loopStartBeats != newStartBeat || loopLengthBeats != newLengthBeat)
    {
        Clip::setSpeedRatio (1.0);
        setAutoTempo (true);
        
        loopStartBeats  = newStartBeat;
        loopLengthBeats = newLengthBeat;
    }
}

void AudioClipBase::setAutoDetectBeats (bool b)
{
    autoDetectBeats = b;
    setLoopInfo (autoDetectBeatMarkers (loopInfo, b, beatSensitivity));
}

void AudioClipBase::setBeatSensitivity (float s)
{
    beatSensitivity = s;
    setLoopInfo (autoDetectBeatMarkers (loopInfo, autoDetectBeats, s));
}

void AudioClipBase::pitchTempoTrackChanged()
{
    clearCachedAudioSegmentList();
    createNewProxyAsync();

    if (melodyneProxy != nullptr)
        melodyneProxy->sourceClipChanged();
}

void AudioClipBase::clearCachedAudioSegmentList()
{
    if (! edit.isLoading())
        TRACKTION_ASSERT_MESSAGE_THREAD

    audioSegmentList.reset();
}

const AudioSegmentList& AudioClipBase::getAudioSegmentList()
{
    if (! edit.isLoading())
        TRACKTION_ASSERT_MESSAGE_THREAD

    if (audioSegmentList == nullptr)
        audioSegmentList = AudioSegmentList::create (*this, false, false);

    return *audioSegmentList;
}

void AudioClipBase::setResamplingQuality (ResamplingQuality rq)
{
    resamplingQuality = rq;
}

ResamplingQuality AudioClipBase::getResamplingQuality() const
{
    return resamplingQuality;
}

//==============================================================================
void AudioClipBase::setSpeedRatio (double r)
{
    if (! autoTempo)
    {
        auto factor = getSpeedRatio() / r;
        auto newLoopStart = getLoopStart() * factor;
        auto newLoopLen   = getLoopLength() * factor;

        Clip::setSpeedRatio (r);
        setLoopRange ({ newLoopStart, newLoopStart + newLoopLen });
    }
}

bool AudioClipBase::isUsingMelodyne() const
{
    return TimeStretcher::isMelodyne (timeStretchMode);
}

void AudioClipBase::loadMelodyneState()
{
    setupARA (true);
}

void AudioClipBase::showMelodyneWindow()
{
    if (melodyneProxy != nullptr)
        melodyneProxy->showPluginWindow();
}

void AudioClipBase::hideMelodyneWindow()
{
    if (melodyneProxy != nullptr)
        melodyneProxy->hidePluginWindow();
}

void AudioClipBase::melodyneConvertToMIDI()
{
    if (melodyneProxy != nullptr)
    {
        juce::MidiMessageSequence m (melodyneProxy->getAnalysedMIDISequence());

        if (m.getNumEvents() > 0)
        {
            juce::UndoManager* um = nullptr;

            juce::ValueTree midiClip (IDs::MIDICLIP);
            midiClip.setProperty (IDs::name, getName(), um);
            midiClip.setProperty (IDs::start, getPosition().getStart().inSeconds(), um);
            midiClip.setProperty (IDs::length, getPosition().getLength().inSeconds(), um);

            juce::ValueTree ms (IDs::SEQUENCE);
            ms.setProperty (IDs::ver, 1, um);
            ms.setProperty (IDs::channelNumber, 1, um);

            midiClip.addChild (ms, -1, um);

            auto& ts = edit.tempoSequence;

            for (int i = 0; i < m.getNumEvents(); ++i)
            {
                auto& e = *m.getEventPointer (i);

                if (e.noteOffObject != nullptr)
                {
                    juce::ValueTree note (IDs::NOTE);
                    note.setProperty ("p", e.message.getNoteNumber(), um);
                    note.setProperty ("v", e.message.getVelocity(), um);
                    note.setProperty ("b", ts.toBeats (TimePosition::fromSeconds (e.message.getTimeStamp())).inBeats(), um);
                    note.setProperty ("l", (ts.toBeats (TimePosition::fromSeconds (e.noteOffObject->message.getTimeStamp()))
                                            - ts.toBeats (TimePosition::fromSeconds (e.message.getTimeStamp()))).inBeats(), um);

                    ms.addChild (note, -1, um);
                }
            }

            if (auto t = getClipTrack())
                t->insertClipWithState (midiClip, getName(), Type::midi,
                                        { getPosition().time, {} }, true, false);
        }
        else
        {
            edit.engine.getUIBehaviour().showWarningMessage (TRANS("No MIDI notes were found by the plugin!"));
        }
    }
}

void AudioClipBase::setTimeStretchMode (TimeStretcher::Mode mode)
{
    timeStretchMode = TimeStretcher::checkModeIsAvailable (mode);

    if (isLooping() && ! canLoop())
        disableLooping();
}

WarpTimeManager& AudioClipBase::getWarpTimeManager() const
{
    if (warpTimeManager == nullptr)
    {
        CRASH_TRACER
        WarpTimeManager::Ptr ptr = edit.engine.getWarpTimeFactory().getWarpTimeManager (*this);
        warpTimeManager = dynamic_cast<WarpTimeManager*> (ptr.get());
        jassert (warpTimeManager != nullptr);
    }

    return *warpTimeManager;
}

int AudioClipBase::getTransposeSemiTones (bool includeAutoPitch) const
{
    if (autoPitch && includeAutoPitch)
    {
        int pitch = edit.pitchSequence.getPitchAt (getPosition().getStart() + TimeDuration::fromSeconds (0.0001)).getPitch();
        int transposeBase = pitch - loopInfo.getRootNote();

        while (transposeBase > 6)  transposeBase -= 12;
        while (transposeBase < -6) transposeBase += 12;

        return transpose + transposeBase;
    }

    return transpose;
}

LoopInfo AudioClipBase::autoDetectBeatMarkers (const LoopInfo& current, bool autoBeat, float sens) const
{
    LoopInfo res = current;

    for (int i = res.getNumLoopPoints(); --i >= 0;)
        if (res.getLoopPoint(i).isAutomatic())
            res.deleteLoopPoint (i);

    if (autoBeat)
    {
        if (auto reader = std::unique_ptr<juce::AudioFormatReader> (AudioFileUtils::createReaderFor (edit.engine, getCurrentSourceFile())))
        {
            const auto start = loopInfo.getInMarker();
            const auto end = (loopInfo.getOutMarker() == -1) ? reader->lengthInSamples
                                                             : loopInfo.getOutMarker();

            BeatDetect detect;
            detect.setSensitivity (sens);
            detect.setSampleRate (reader->sampleRate);

            if ((end - start) > reader->sampleRate)
            {
                auto blockLength = detect.getBlockSize();
                auto blockSize = choc::buffer::Size::create (reader->numChannels, blockLength);
                auto pos = start;

                choc::buffer::ChannelArrayBuffer<float> buffer (blockSize);

                while (pos + blockLength < end)
                {
                    if (! reader->read (buffer.getView().data.channels,
                                        (int) reader->numChannels, pos, (int) blockLength))
                        break;

                    detect.audioProcess (buffer);
                    pos += blockLength;
                }

                for (auto beat : detect.getBeats())
                    res.addLoopPoint (start + beat, LoopInfo::LoopPointType::automatic);
            }
        }
    }

    return res;
}

bool AudioClipBase::performTempoDetect()
{
    TempoDetectTask tempoDetectTask (edit.engine, getCurrentSourceFile());

    edit.engine.getUIBehaviour().runTaskWithProgressBar (tempoDetectTask);

    if (! tempoDetectTask.isResultSensible())
        return false;

    const AudioFileInfo wi = AudioFile (edit.engine, getCurrentSourceFile()).getInfo();
    loopInfo.setBpm (tempoDetectTask.getBpm(), wi);

    return true;
}

juce::StringArray AudioClipBase::getRootNoteChoices (Engine& e)
{
    juce::StringArray s;
    s.add ("<" + TRANS("None") + ">");

    for (int i = 0; i < 12; ++i)
        s.add (Pitch::getPitchAsString (e, i));

    return s;
}

juce::StringArray AudioClipBase::getPitchChoices()
{
    juce::StringArray s;

    const int numSemitones = isUsingMelodyne() ? 12 : 24;

    if (loopInfo.getRootNote() == -1)
    {
        for (int i = numSemitones; i >= 1; i--)
            s.add ("+" + juce::String (i));

        s.add ("0");

        for (int i = 1; i <= numSemitones; ++i)
            s.add ("-" + juce::String (i));
    }
    else
    {
        const int base = autoPitch ? edit.pitchSequence.getPitchAt (getPosition().getStart()).getPitch()
                                   : loopInfo.getRootNote();

        for (int i = numSemitones; i >= 1; i--)
            s.add ("+" + juce::String (i) + " : " + Pitch::getPitchAsString (edit.engine, base + i));

        s.add ("0 : " + Pitch::getPitchAsString (edit.engine, base));

        for (int i = 1; i <= numSemitones; ++i)
            s.add ("-" + juce::String (i) + " : " + Pitch::getPitchAsString (edit.engine, base - i));
    }

    return s;
}

void AudioClipBase::enableEffects (bool enable, bool warn)
{
    auto v = state.getChildWithName (IDs::EFFECTS);
    auto um = getUndoManager();

    if (enable)
    {
        if (! v.isValid())
        {
            state.addChild (ClipEffects::create(), -1, um);
            clipEffectsVisible = true;
        }
    }
    else if (v.isValid())
    {
        if (! warn || edit.engine.getUIBehaviour().showOkCancelAlertBox (TRANS("Remove Clip Effects"),
                                                                         TRANS("Are you sure you want to remove all clip effects?")))
        {
            state.removeChild (v, um);
            state.removeProperty (IDs::effectsVisible, um);
        }
    }
}

void AudioClipBase::addEffect (const juce::ValueTree& effectsTree)
{
    auto v = state.getChildWithName (IDs::EFFECTS);
    jassert (v.isValid());

    if (v.isValid())
        v.addChild (effectsTree, -1, getUndoManager());
}

//==============================================================================
TimePosition AudioClipBase::clipTimeToSourceFileTime (TimePosition t)
{
    if (getAutoTempo())
    {
        if (isLooping())
        {
            auto b = toPosition (getBeatOfRelativeTime (toDuration (t)) - getStartBeat());
            const auto end = toPosition (getLoopLengthBeats());

            while (b > end)
                b = b - getLoopLengthBeats();

            b = b + BeatDuration::fromBeats (getPosition().getOffset().inSeconds()) + toDuration (getLoopStartBeats());
            return TimePosition::fromSeconds (b.inBeats() / loopInfo.getBeatsPerSecond (getAudioFile().getInfo()));
        }

        auto b = getBeatOfRelativeTime (toDuration (t)) - getStartBeat() + getOffsetInBeats();

        return TimePosition::fromSeconds (b.inBeats() / loopInfo.getBeatsPerSecond (getAudioFile().getInfo()));
    }

    if (isLooping())
    {
        const auto end = toPosition (getLoopLength());

        while (t > end)
            t = t - getLoopLength();

        return (t + getPosition().getOffset() + toDuration (getLoopStart())) * getSpeedRatio();
    }

    return (t + getPosition().getOffset()) * getSpeedRatio();
}

void AudioClipBase::addMark (TimePosition relCursorPos)
{
    if (auto sourceItem = sourceFileReference.getSourceProjectItem())
    {
        auto marks = sourceItem->getMarkedPoints();
        marks.add (clipTimeToSourceFileTime (relCursorPos));
        sourceItem->setMarkedPoints (marks);
    }
}

void AudioClipBase::moveMarkTo (TimePosition relCursorPos)
{
    if (auto sourceItem = sourceFileReference.getSourceProjectItem())
    {
        auto marks = sourceItem->getMarkedPoints();

        juce::Array<TimePosition> rescaled;
        juce::Array<int> index;
        getRescaledMarkPoints (rescaled, index);

        int indexOfNearest = -1;
        auto nearestDiff = Edit::getMaximumEditEnd();

        for (int i = rescaled.size(); --i >= 0;)
        {
            auto diff = TimePosition::fromSeconds (std::abs ((rescaled[i] - toDuration (relCursorPos)).inSeconds()));

            if (diff < nearestDiff)
            {
                nearestDiff = diff;
                indexOfNearest = index[i];
            }
        }

        if (indexOfNearest != -1)
        {
            marks.set (indexOfNearest, clipTimeToSourceFileTime (relCursorPos));
            sourceItem->setMarkedPoints (marks);
        }
    }
}

void AudioClipBase::deleteMark (TimePosition relCursorPos)
{
    if (auto sourceItem = sourceFileReference.getSourceProjectItem())
    {
        auto marks = sourceItem->getMarkedPoints();

        juce::Array<TimePosition> rescaled;
        juce::Array<int> index;
        getRescaledMarkPoints (rescaled, index);

        int indexOfNearest = -1;
        auto nearestDiff = Edit::getMaximumEditEnd();

        for (int i = rescaled.size(); --i >= 0;)
        {
            auto diff = TimePosition::fromSeconds (std::abs ((rescaled[i] - toDuration (relCursorPos)).inSeconds()));

            if (diff < nearestDiff)
            {
                nearestDiff = diff;
                indexOfNearest = index[i];
            }
        }

        if (indexOfNearest != -1)
        {
            marks.remove (indexOfNearest);
            sourceItem->setMarkedPoints (marks);
        }
    }
}

bool AudioClipBase::canSnapToOriginalBWavTime()
{
    return getAudioFile().getMetadata()[juce::WavAudioFormat::bwavTimeReference].isNotEmpty();
}

void AudioClipBase::snapToOriginalBWavTime()
{
    auto f = getAudioFile();
    juce::String bwavTime (f.getMetadata()[juce::WavAudioFormat::bwavTimeReference]);

    if (bwavTime.isNotEmpty())
    {
        auto t = TimePosition::fromSeconds (bwavTime.getLargeIntValue() / f.getSampleRate());

        setStart (t + getPosition().getOffset(), false, true);
    }
}

//==============================================================================
juce::Array<Exportable::ReferencedItem> AudioClipBase::getReferencedItems()
{
    juce::Array<Exportable::ReferencedItem> results;

    Exportable::ReferencedItem item;
    item.firstTimeUsed = 0;
    item.lengthUsed = 0;

    if (! getAutoTempo())
    {
        auto speed = getSpeedRatio();

        if (! isLooping())
        {
            item.firstTimeUsed = (getPosition().getOffset() * speed).inSeconds();
            item.lengthUsed    = (getPosition().getLength() * speed).inSeconds();
        }
        else
        {
            item.firstTimeUsed = (getLoopStart() * speed).inSeconds();
            item.lengthUsed    = (getLoopLength() * speed).inSeconds();
        }
    }

    if (hasAnyTakes())
    {
        for (auto takeID : getTakes())
        {
            item.itemID = takeID;
            results.add (item);
        }

        jassert (! results.isEmpty());
    }
    else
    {
        item.itemID = ProjectItemID (sourceFileReference.source.get());
        results.add (item);
    }

    if (getAutoTempo())
    {
        for (auto& ref : results)
        {
            auto wi = edit.engine.getAudioFileManager().getAudioFile (ref.itemID).getInfo();

            if (wi.sampleRate > 0)
            {
                ref.firstTimeUsed = 0;
                ref.lengthUsed = wi.getLengthInSeconds();
            }
        }
    }

    return results;
}

void AudioClipBase::reassignReferencedItem (const ReferencedItem& item,
                                            ProjectItemID newItemID, double newStartTime)
{
    juce::ignoreUnused (item);

    if (getReferencedItems().size() == 1 && item == getReferencedItems().getFirst())
    {
        sourceFileReference.setToProjectFileReference (newItemID);

        if (! isLooping())
            setOffset (getPosition().getOffset() - TimeDuration::fromSeconds ((newStartTime / getSpeedRatio())));
        else
            loopStart = loopStart - TimeDuration::fromSeconds ((newStartTime / getSpeedRatio()));
    }
    else
    {
        jassertfalse;
    }
}

juce::Array<ProjectItemID> AudioClipBase::getTakes() const
{
    jassert (! hasAnyTakes());
    return {};
}

//==============================================================================
juce::String AudioClipBase::canAddClipPlugin (const Plugin::Ptr& p) const
{
    if (p != nullptr)
    {
        if (! p->canBeAddedToClip())
            return TRANS("Can't add this kind of plugin to a clip!");

        if (pluginList.size() >= edit.engine.getEngineBehaviour().getEditLimits().maxPluginsOnClip)
            return TRANS("Can't add any more plugins to this clip!");
    }

    return {};
}

bool AudioClipBase::addClipPlugin (const Plugin::Ptr& p, SelectionManager& sm)
{
    if (canAddClipPlugin (p).isEmpty())
    {
        pluginList.insertPlugin (p, -1, &sm);
        return true;
    }

    return false;
}

Plugin::Array AudioClipBase::getAllPlugins()
{
    return pluginList.getPlugins();
}

void AudioClipBase::sendMirrorUpdateToAllPlugins (Plugin& p) const
{
    pluginList.sendMirrorUpdateToAllPlugins (p);
}

//==============================================================================
bool AudioClipBase::setupARA (bool dontPopupErrorMessages)
{
    TRACKTION_ASSERT_MESSAGE_THREAD
    static bool araReentrancyCheck = false;

    if (araReentrancyCheck)
        return true;

    const juce::ScopedValueSetter<bool> svs (araReentrancyCheck, true);

   #if TRACKTION_ENABLE_ARA
    if (isUsingMelodyne())
    {
        if (melodyneProxy == nullptr)
        {
            TRACKTION_LOG ("Created ARA reader!");
            melodyneProxy = new MelodyneFileReader (edit, *this);
        }

        if (melodyneProxy != nullptr && melodyneProxy->isValid())
            return true;

        if (! dontPopupErrorMessages)
        {
            TRACKTION_LOG_ERROR ("Failed setting up ARA for audio clip!");

            if (TimeStretcher::isMelodyne (timeStretchMode)
                  && edit.engine.getPluginManager().getARACompatiblePlugDescriptions().size() <= 0)
            {
                TRACKTION_LOG_ERROR ("No ARA-compatible plugins were found!");

                edit.engine.getUIBehaviour().showWarningMessage (TRANS ("This audio clip is setup with Melodyne's time-stretching, but there aren't any ARA-compatible plugins available!")
                                                                 + "\n\n"
                                                                 + TRANS ("If you know you have ARA-compatible plugins installed, they must be scanned and part of the list of known plugins!"));
            }
        }
    }
   #endif

    juce::ignoreUnused (dontPopupErrorMessages);
    return false;
}

LiveClipLevel AudioClipBase::getLiveClipLevel()
{
    return { level };
}

//==============================================================================
void AudioClipBase::markAsDirty()
{
    lastRenderJobFailed = false;
    createNewProxyAsync(); // Do this asyncronously to avoid recursion
}

void AudioClipBase::updateSourceFile()
{
    CRASH_TRACER

    if (! isInitialised)
        return;

    TRACKTION_ASSERT_MESSAGE_THREAD

    // check to see if our source file already exists, it may have been created by another clip
    // if it does exist, we will just use that, otherwise we need to start our own render operation
    const AudioFile audioFile (RenderManager::getAudioFileForHash (edit.engine, edit.getTempDirectory (false), getHash()));

    if (getCurrentSourceFile() != audioFile.getFile())
        setCurrentSourceFile (audioFile.getFile());

    if (renderJob != nullptr && renderJob->proxy == audioFile && (! renderJob->shouldExit()))
        return;

    if (! (audioFile.getFile().existsAsFile() || audioFile.isValid()))
    {
        renderSource();
    }
    else if (renderJob != nullptr)
    {
        renderJob->removeListener (this);
        renderJob = nullptr;
    }
}

void AudioClipBase::renderSource()
{
    TRACKTION_ASSERT_MESSAGE_THREAD
    jassert (isInitialised);

    const AudioFile audioFile (edit.engine, getCurrentSourceFile());
    const bool isValid = audioFile.isValid();

    if (audioFile.getFile().existsAsFile() && isValid)
        return;

    if (! isValid)
    {
        // need to render
        bool needsToChangeJob = renderJob == nullptr
                                 || (renderJob != nullptr && (renderJob->proxy != audioFile));

        if (needsToChangeJob)
        {
            if (renderJob != nullptr)
                renderJob->removeListener (this);

            renderJob = getRenderJob (audioFile);

            if (renderJob != nullptr)
                renderJob->addListener (this);

            changed(); // updates the thumbnail progress
        }
    }
    else
    {
        // either finished or file exists
        if (renderJob != nullptr)
        {
            renderJob->removeListener (this);
            renderJob = nullptr;
        }

        renderComplete();
    }
}

void AudioClipBase::renderComplete()
{
    TRACKTION_ASSERT_MESSAGE_THREAD
    // Updates the thumbnail message
    changed();

    if (clipEffects != nullptr)
        clipEffects->notifyListenersOfRenderCompletion();
}

//==============================================================================
juce::Array<TimePosition> AudioClipBase::getRescaledMarkPoints() const
{
    juce::Array<TimePosition> rescaled;
    juce::Array<int> index;
    getRescaledMarkPoints (rescaled, index);
    return rescaled;
}

void AudioClipBase::getRescaledMarkPoints (juce::Array<TimePosition>& times, juce::Array<int>& index) const
{
    if (auto sourceItem = sourceFileReference.getSourceProjectItem())
    {
        if (getAutoTempo())
        {
            auto beats = sourceItem->getMarkedPoints();
            auto afi = getAudioFile().getInfo();

            for (int i = 0; i < beats.size(); ++i)
                beats.set (i, beats[i] * loopInfo.getBeatsPerSecond (afi));

            if (isLooping())
            {
                auto loopLen = getLoopLengthBeats();
                auto clipLen = getLengthInBeats();
                auto b = loopLen - getOffsetInBeats();

                for (int i = 0; i < beats.size(); ++i)
                {
                    auto newB = BeatDuration::fromBeats (beats[i].inSeconds() - (toPosition (getOffsetInBeats()) - getLoopStartBeats()).inBeats());

                    if (newB > BeatDuration() && newB < b)
                    {
                        times.add (getTimeOfRelativeBeat (newB));
                        index.add (i);
                    }
                }

                while (b < clipLen)
                {
                    for (int i = 0; i < beats.size(); ++i)
                    {
                        auto newB = BeatDuration::fromBeats (beats[i].inSeconds() + (toPosition (b) - getLoopStartBeats()).inBeats());

                        if (newB >= b && newB < b + loopLen)
                        {
                            times.add (getTimeOfRelativeBeat (newB));
                            index.add (i);
                        }
                    }

                    b = b + loopLen;
                }
            }
            else
            {
                for (int i = 0; i < beats.size(); ++i)
                {
                    auto newT = getTimeOfRelativeBeat (BeatPosition::fromBeats (beats[i].inSeconds()) - toPosition (getOffsetInBeats()));

                    if (newT >= TimePosition())
                    {
                        times.add (newT);
                        index.add (i);
                    }
                }
            }
        }
        else
        {
            if (isLooping())
            {
                auto origTimes = sourceItem->getMarkedPoints();

                for (int i = origTimes.size(); --i >= 0;)
                    origTimes.set (i, origTimes[i] / speedRatio - getPosition().getOffset() - toDuration (getLoopStart()));

                const auto loopLen = getLoopLength();
                const auto clipLen = getPosition().getLength();
                auto t = loopLen - getPosition().getOffset();

                for (int i = 0; i < origTimes.size(); ++i)
                {
                    if (origTimes[i] >= TimePosition() && origTimes[i] < toPosition (t))
                    {
                        times.add(origTimes[i]);
                        index.add(i);
                    }
                }

                while (t < clipLen)
                {
                    for (int i = 0; i < origTimes.size(); ++i)
                    {
                        auto newT = toDuration (origTimes[i] + t + getPosition().getOffset());

                        if (newT >= t && newT < t + loopLen)
                        {
                            times.add(toPosition (newT));
                            index.add(i);
                        }
                    }

                    t = t + loopLen;
                }
            }
            else
            {
                times = sourceItem->getMarkedPoints();

                for (int i = 0; i < times.size(); ++i)
                {
                    times.set(i, times[i] / speedRatio - getPosition().getOffset());
                    index.add(i);
                }
            }
        }
    }
}

//==============================================================================
bool AudioClipBase::isUsingFile (const AudioFile& af)
{
    if (getPlaybackFile() == af || getAudioFile() == af)
        return true;

    if (clipEffects != nullptr)
        return clipEffects->isUsingFile (af);

    return false;
}

void AudioClipBase::setUsesProxy (bool canUseProxy) noexcept
{
    proxyAllowed = canUseProxy;
    stopTimer();
}

bool AudioClipBase::usesTimeStretchedProxy() const
{
   #if TRACKTION_ENABLE_REALTIME_TIMESTRETCHING
    if (! proxyAllowed)
        return false;
   #endif

    return getAutoTempo() || getAutoPitch()
           || getPitchChange() != 0.0f
           || isUsingMelodyne()
           || (std::abs (getSpeedRatio() - 1.0) > 0.00001
               && TimeStretcher::canProcessFor (timeStretchMode));
}

AudioClipBase::ProxyRenderingInfo::ProxyRenderingInfo() {}
AudioClipBase::ProxyRenderingInfo::~ProxyRenderingInfo() {}

AudioFile AudioClipBase::getProxyFileToCreate (bool renderTimestretched)
{
    if (renderTimestretched)
        return TemporaryFileManager::getFileForCachedClipRender (*this, getProxyHash());

    return TemporaryFileManager::getFileForCachedFileRender (edit, getHash());
}

//==============================================================================
struct StretchSegment
{
    static constexpr int maxNumChannels = 8;
    
    StretchSegment (Engine& engine, const AudioFile& file,
                    const AudioClipBase::ProxyRenderingInfo& info,
                    double sampleRate, const AudioSegmentList::Segment& s)
        : segment (s),
          fileInfo (file.getInfo()),
          crossfadeSamples ((int) tracktion::toSamples (info.audioSegmentList->getCrossfadeLength(), sampleRate)),
          numChannelsToUse (juce::jlimit (1, maxNumChannels, fileInfo.numChannels))
    {
        CRASH_TRACER
        reader = engine.getAudioFileManager().cache.createReader (file);

        if (reader != nullptr)
        {
            auto sampleRange = segment.getSampleRange();

            if (segment.isFollowedBySilence())
            {
                reader->setReadPosition (sampleRange.getStart());
            }
            else
            {
                reader->setLoopRange (sampleRange);
                reader->setReadPosition (0);
            }

            timestretcher.initialise (fileInfo.sampleRate, outputBufferSize, numChannelsToUse,
                                      info.mode, info.options, false);
            jassert (timestretcher.isInitialised()); // Have you enabled a TimeStretcher mode?

            timestretcher.setSpeedAndPitch ((float) (1.0 / segment.getStretchRatio()),
                                            segment.getTranspose());
        }
    }

    void renderNextBlock (juce::AudioBuffer<float>& buffer, TimeRange editTime, int numSamples)
    {
        if (reader == nullptr)
            return;

        CRASH_TRACER

        auto loopRange = segment.getRange();

        if (! editTime.overlaps (loopRange))
            return;

        int start = 0;

        if (loopRange.getEnd() < editTime.getEnd())
            numSamples = std::max (0, (int) (numSamples * (loopRange.getEnd() - editTime.getStart()).inSeconds()
                                              / editTime.getLength().inSeconds()));

        if (loopRange.getStart() > editTime.getStart())
        {
            auto skip = juce::jlimit (0, numSamples, (int) (numSamples * (loopRange.getStart() - editTime.getStart()).inSeconds() / editTime.getLength().inSeconds()));
            start += skip;
            numSamples -= skip;
        }

        while (numSamples > 0)
        {
            auto numReady = std::min (numSamples, readySamplesEnd - readySamplesStart);

            if (numReady > 0)
            {
                for (int i = 0; i < buffer.getNumChannels(); ++i)
                    buffer.addFrom (i, start, fifo,
                                    std::min (i, fifo.getNumChannels() - 1),
                                    readySamplesStart, numReady);

                readySamplesStart += numReady;
                start += numReady;
                numSamples -= numReady;
            }
            else
            {
                auto blockSize = fillNextBlock();
                renderFades (blockSize);

                readySampleOutputPos += blockSize;
            }
        }
    }

    int fillNextBlock()
    {
        CRASH_TRACER
        float* outs[maxNumChannels] = {};
        
        for (int i = 0; i < numChannelsToUse; ++i)
            outs[i] = fifo.getWritePointer (i);

        const int needed = timestretcher.getFramesNeeded();
        int numRead = 0;
        
        if (needed >= 0)
        {
            AudioScratchBuffer scratch (numChannelsToUse, needed);
            scratch.buffer.clear();
            auto bufferChannels = juce::AudioChannelSet::canonicalChannelSet (numChannelsToUse);
            auto sourceChannelsToUse = bufferChannels;

            if (needed > 0)
            {
               #if JUCE_DEBUG
                jassert (reader->readSamples (needed, scratch.buffer, bufferChannels, 0, sourceChannelsToUse, 5000));
               #else
                reader->readSamples (needed, scratch.buffer, bufferChannels, 0, sourceChannelsToUse, 5000);
               #endif
            }

            const float* ins[maxNumChannels] = {};
            
            for (int i = 0; i < numChannelsToUse; ++i)
                ins[i] = scratch.buffer.getReadPointer (i);

            numRead = timestretcher.processData (ins, needed, outs);
        }
        else
        {
            jassert (needed == -1);
            numRead = timestretcher.flush (outs);
        }

        readySamplesStart = 0;
        readySamplesEnd = numRead;
        
        return numRead;
    }

    void renderFades (int numSamples)
    {
        CRASH_TRACER
        auto renderedEnd = readySampleOutputPos + numSamples;

        if (segment.hasFadeIn())
            if (readySampleOutputPos < crossfadeSamples)
                renderFade (0, crossfadeSamples, false, numSamples);

        if (segment.hasFadeOut())
        {
            auto fadeOutStart = (SampleCount) (segment.getSampleRange().getLength() / segment.getStretchRatio()) - crossfadeSamples;

            if (renderedEnd > fadeOutStart)
                renderFade (fadeOutStart, fadeOutStart + crossfadeSamples + 2, true, numSamples);
        }
    }

    void renderFade (SampleCount start, SampleCount end, bool isFadeOut, int numSamples)
    {
        float alpha1 = 0.0f, alpha2 = 1.0f;
        auto renderedEnd = readySampleOutputPos + numSamples;

        if (end > renderedEnd)
        {
            alpha2 = (renderedEnd - start) / (float) (end - start);
            end = renderedEnd;
        }

        if (start < readySampleOutputPos)
        {
            alpha1 = alpha2 * (readySampleOutputPos - start) / (float) (end - start);
            start = readySampleOutputPos;
        }

        if (end > start)
        {
            if (isFadeOut)
            {
                alpha1 = 1.0f - alpha1;
                alpha2 = 1.0f - alpha2;
            }

            AudioFadeCurve::applyCrossfadeSection (fifo,
                                                   (int) (start - readySampleOutputPos),
                                                   (int) (end - start),
                                                   AudioFadeCurve::convex, alpha1, alpha2);
        }
    }

    const AudioSegmentList::Segment& segment;
    TimeStretcher timestretcher;

    AudioFileInfo fileInfo;
    AudioFileCache::Reader::Ptr reader;

    const int outputBufferSize = 1024;
    int readySamplesStart = 0, readySamplesEnd = 0;
    SampleCount readySampleOutputPos = 0;
    const int crossfadeSamples, numChannelsToUse;
    juce::AudioBuffer<float> fifo { numChannelsToUse, outputBufferSize };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StretchSegment)
};

//==============================================================================
std::unique_ptr<AudioClipBase::ProxyRenderingInfo> AudioClipBase::createProxyRenderingInfo()
{
    auto p = std::make_unique<ProxyRenderingInfo>();
    p->audioSegmentList = AudioSegmentList::create (*this, true, true);
    p->clipTime = getEditTimeRange();
    p->speedRatio = getSpeedRatio();
    p->mode = (timeStretchMode != TimeStretcher::disabled && timeStretchMode != TimeStretcher::melodyne)
                 ? timeStretchMode
                 : TimeStretcher::defaultMode;
    p->options = elastiqueProOptions;

    return p;
}

bool AudioClipBase::ProxyRenderingInfo::render (Engine& engine, const AudioFile& sourceFile, AudioFileWriter& writer,
                                                juce::ThreadPoolJob* const& job, std::atomic<float>& progress) const
{
    CRASH_TRACER

    if (audioSegmentList->getSegments().isEmpty() || ! sourceFile.isValid())
        return false;

    juce::OwnedArray<StretchSegment> segments;

    auto sampleRate = sourceFile.getSampleRate();

    for (auto& segment : audioSegmentList->getSegments())
        segments.add (new StretchSegment (engine, sourceFile, *this, sampleRate, segment));

    const int samplesPerBlock = 1024;
    juce::AudioBuffer<float> buffer (sourceFile.getNumChannels(), samplesPerBlock);
    double time = 0.0;

    auto numBlocks = 1 + (int) (clipTime.getLength().inSeconds() * sampleRate / samplesPerBlock);

    for (int i = 0; i < numBlocks; ++i)
    {
        if (job != nullptr && job->shouldExit())
            return false;

        buffer.clear();

        auto endTime = time + samplesPerBlock / sampleRate;
        const auto editTime = TimeRange (TimePosition::fromSeconds (time), TimePosition::fromSeconds (endTime));
        time = endTime;

        for (auto s : segments)
            s->renderNextBlock (buffer, editTime, samplesPerBlock);

        if (! writer.appendBuffer (buffer, samplesPerBlock))
            return false;

        progress = i / (float) numBlocks;
    }

    return true;
}

AudioFile AudioClipBase::getPlaybackFile()
{
    // this needs to return the same file right from the first call, if it's a rendered file then obviously it won't exist but we need to return it anyway
    const AudioFile af (getAudioFile());

    if (canUseProxy() && ! af.isNull())
    {
        const bool timestretched = usesTimeStretchedProxy();

        if (timestretched || af.getInfo().needsCachedProxy)
            return getProxyFileToCreate (timestretched);
    }

    return af;
}

AudioFileInfo AudioClipBase::getWaveInfo()
{
    // if the source needs to render we'll just have to bodge return a WaveInfo so the AudioSegmentList gives us a consistent hash
    // this is of course a massive hack because it assumes that the rendered file will have the same sample rate etc.

    if (needsRender())
        if (auto sourceItem = sourceFileReference.getSourceProjectItem())
            return AudioFile (edit.engine, sourceItem->getSourceFile()).getInfo();

    return getAudioFile().getInfo();
}

HashCode AudioClipBase::getProxyHash()
{
    jassert (usesTimeStretchedProxy());

    auto clipPos = getPosition();

    HashCode hash = getHash()
                     ^ static_cast<HashCode> (timeStretchMode.get())
                     ^ elastiqueProOptions->toString().hashCode64()
                     ^ (7342847 * static_cast<HashCode> (pitchChange * 199.0))
                     ^ static_cast<HashCode> (clipPos.getLength().inSeconds() * 10005.0)
                     ^ static_cast<HashCode> (clipPos.getOffset().inSeconds() * 9997.0)
                     ^ static_cast<HashCode> (getLoopStart().inSeconds() * 8971.0)
                     ^ static_cast<HashCode> (getLoopLength().inSeconds() * 7733.0)
                     ^ static_cast<HashCode> (getSpeedRatio() * 877.0);

    auto needsPlainStretch = [&]() { return std::abs (getSpeedRatio() - 1.0) > 0.00001 || (getPitchChange() != 0.0f); };

    if (getAutoTempo() || getAutoPitch() || needsPlainStretch())
    {
        auto& segmentList = getAudioSegmentList();
        int i = 0;

        for (auto& segment : segmentList.getSegments())
            hash ^= static_cast<HashCode> (segment.getHashCode() * (i++ + 0.1));
    }

    return hash;
}

void AudioClipBase::beginRenderingNewProxyIfNeeded()
{
    if (! canUseProxy())
        return;
    
    if (isTimerRunning())
    {
        startTimer (1);
        return;
    }
    
    const AudioFile playFile (getPlaybackFile());

    if (playFile.isNull())
        return;

    auto original = getAudioFile();

    if (shouldAttemptRender() && ! original.isValid())
        createNewProxyAsync();

    if (usesTimeStretchedProxy() || original.getInfo().needsCachedProxy)
        if (playFile.getSampleRate() <= 0.0)
            createNewProxyAsync();
}

//==============================================================================
void AudioClipBase::jobFinished (RenderManager::Job& job, bool completedOk)
{
    TRACKTION_ASSERT_MESSAGE_THREAD

    if (&job == renderJob.get())
    {
        lastRenderJobFailed = ! completedOk;
        renderJob->removeListener (this);
        renderJob = nullptr;

        renderComplete();
    }
}

//==============================================================================
void AudioClipBase::createNewProxyAsync()
{
    if (canUseProxy())
        startTimer (600);
}

void AudioClipBase::cancelCurrentRender()
{
    if (renderJob != nullptr)
        renderJob->cancelJob();
}

void AudioClipBase::timerCallback()
{
    if (edit.isLoading()
         || ! edit.getTransport().isAllowedToReallocate())
        return;

    // if the source file hasn't been rendered yet we need to delay this
    if (shouldAttemptRender())
    {
        updateSourceFile();

        if (! getAudioFile().isValid())
        {
            createNewProxyAsync();
            return;
        }
    }

    stopTimer();

    if (! canUseProxy())
        return;

    const bool isTimeStretched = usesTimeStretchedProxy();

    const AudioFile originalFile (getAudioFile());
    const AudioFile newProxy (getPlaybackFile());

    const bool proxyChanged = lastProxy != newProxy;

    if (proxyChanged || ! newProxy.getFile().exists())
    {
        if (proxyChanged
             && lastProxy != originalFile
             && lastProxy.getFile().isAChildOf (edit.getTempDirectory (false))
             && ! edit.areAnyClipsUsingFile (lastProxy))
            edit.engine.getAudioFileManager().proxyGenerator.deleteProxy (lastProxy);

        lastProxy = newProxy;

        if (isTimeStretched || newProxy != originalFile)
        {
            edit.engine.getAudioFileManager().proxyGenerator
                .beginJob (new ProxyGeneratorJob (getAudioFile(), newProxy, *this, isTimeStretched));
        }

        if (proxyChanged || newProxy.getFile().exists())
        {
            Selectable::changed(); // force update of waveforms
            edit.restartPlayback();
        }
    }
}

void AudioClipBase::valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& id)
{
    if (tree == state)
    {
        if (id == IDs::fadeInType || id == IDs::fadeOutType
            || id == IDs::fadeInBehaviour || id == IDs::fadeOutBehaviour
            || id == IDs::fadeIn || id == IDs::fadeOut
            || id == IDs::loopStart || id == IDs::loopLength
            || id == IDs::loopStartBeats || id == IDs::loopLengthBeats
            || id == IDs::transpose || id == IDs::pitchChange
            || id == IDs::elastiqueMode || id == IDs::autoPitch
            || id == IDs::elastiqueOptions || id == IDs::warpTime
            || id == IDs::effectsVisible || id == IDs::autoPitchMode)
        {
            if (id == IDs::warpTime)
            {
                warpTime.forceUpdateOfCachedValue();
                
                if (! getWarpTime())
                {
                    if (shouldAttemptRender())
                        updateSourceFile();
                    else
                        setCurrentSourceFile (getOriginalFile());
                }
            }
            
            changed();
        }
        else if (id == IDs::gain)
        {
            changed();
        }
        else if (id == IDs::pan || id == IDs::mute
                 || id == IDs::autoCrossfade)
        {
            changed();

            if (id == IDs::mute)
            {
                if (auto f = track->getParentFolderTrack())
                    f->setDirtyClips();
            }
            else if (id == IDs::autoCrossfade)
            {
                updateAutoCrossfadesAsync (true);
            }
        }
        else if (id == IDs::autoTempo)
        {
            autoTempo.forceUpdateOfCachedValue();
            updateAutoTempoState();
        }
        else if (id == IDs::isReversed)
        {
            isReversed.forceUpdateOfCachedValue();
            updateReversedState();
        }
        else if (id == IDs::channels)
        {
            channels.forceUpdateOfCachedValue();
            updateLeftRightChannelActivenessFlags();
            changed();
        }
        else if (id == IDs::proxyAllowed)
        {
            propertiesChanged();
        }
        else
        {
            Clip::valueTreePropertyChanged (tree, id);
        }
    }
    else if (tree.hasType (IDs::WARPMARKER))
    {
        if (id == IDs::warpTime || id == IDs::sourceTime)
            changed();
    }
    else if (tree.hasType (IDs::LOOPINFO))
    {
        if (isInitialised)
            changed();
    }
    else
    {
        Clip::valueTreePropertyChanged (tree, id);
    }
}

void AudioClipBase::valueTreeChildAdded (juce::ValueTree& parent, juce::ValueTree& child)
{
    if (parent == state)
    {
        if (child.hasType (IDs::PLUGIN))
            Selectable::changed();
        else if (child.hasType (IDs::EFFECTS))
            updateClipEffectsState();
        else if (child.hasType (IDs::PATTERNGENERATOR))
            patternGenerator.reset (new PatternGenerator (*this, child));
    }
    else if (parent.hasType (IDs::LOOPINFO) || child.hasType (IDs::WARPMARKER))
    {
        changed();
    }
    else
    {
        Clip::valueTreeChildAdded (parent, child);
    }
}

void AudioClipBase::valueTreeChildRemoved (juce::ValueTree& parent, juce::ValueTree& child, int oldIndex)
{
    if (parent == state)
    {
        if (child.hasType (IDs::PLUGIN))
            Selectable::changed();
        else if (child.hasType (IDs::EFFECTS))
            updateClipEffectsState();
        else if (child.hasType (IDs::PATTERNGENERATOR))
            patternGenerator = nullptr;
    }
    else if (parent.hasType (IDs::LOOPINFO) || child.hasType (IDs::WARPMARKER))
    {
        changed();
    }
    else
    {
        Clip::valueTreeChildRemoved (parent, child, oldIndex);
    }
}

void AudioClipBase::valueTreeChildOrderChanged (juce::ValueTree& parent, int oldIndex, int newIndex)
{
    Clip::valueTreeChildOrderChanged (parent, oldIndex, newIndex);
}

void AudioClipBase::valueTreeParentChanged (juce::ValueTree& child)
{
    updateAutoCrossfadesAsync (true);

    Clip::valueTreeParentChanged (child);
}

void AudioClipBase::updateReversedState()
{
    setCurrentSourceFile (getOriginalFile());

    if (isReversed)
        updateSourceFile();

    if (! getUndoManager()->isPerformingUndoRedo())
        reverseLoopPoints();

    changed();
    SelectionManager::refreshAllPropertyPanels();
}

void AudioClipBase::updateAutoTempoState()
{
    if (isLooping())
    {
        auto bps = edit.tempoSequence.getBeatsPerSecondAt (getPosition().getStart());

        if (autoTempo)
        {
            // convert time based looping to beat based looping
            loopStartBeats  = BeatPosition::fromBeats (loopStart.get().inSeconds()  * bps);
            loopLengthBeats = BeatDuration::fromBeats (loopLength.get().inSeconds() * bps);

            loopStart  = 0_tp;
            loopLength = 0_td;
        }
        else
        {
            // convert beat based looping to time based looping
            loopStart  = TimePosition::fromSeconds (loopStartBeats.get().inBeats()  / bps);
            loopLength = TimeDuration::fromSeconds (loopLengthBeats.get().inBeats() / bps);

            loopStartBeats  = 0_bp;
            loopLengthBeats = 0_bd;
        }

        changed();
    }
}

void AudioClipBase::updateClipEffectsState()
{
    auto v = state.getChildWithName (IDs::EFFECTS);

    if (v.isValid() && canHaveEffects())
    {
        if (clipEffects == nullptr)
        {
            clipEffects = std::make_unique<ClipEffects> (v, *this);
            changed();
        }
    }
    else if (clipEffects != nullptr)
    {
        clipEffects = nullptr;

        if (auto sourceItem = sourceFileReference.getSourceProjectItem())
            setCurrentSourceFile (sourceItem->getSourceFile());

        changed();
    }

    markAsDirty();
}

}} // namespace tracktion { inline namespace engine
