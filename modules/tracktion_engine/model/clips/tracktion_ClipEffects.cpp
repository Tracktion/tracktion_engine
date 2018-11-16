/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


static inline juce::int64 hashValueTree (juce::int64 startHash, const ValueTree& v)
{
    startHash ^= v.getType().toString().hashCode64() * (v.getParent().indexOf (v) + 1);

    for (int i = v.getNumProperties(); --i >= 0;)
        startHash ^= v[v.getPropertyName (i)].toString().hashCode64();

    for (int i = v.getNumChildren(); --i >= 0;)
        startHash ^= hashValueTree (startHash, v.getChild (i));

    return startHash;
}

static inline juce::int64 hashPlugin (const ValueTree& effectState, Plugin& plugin)
{
    CRASH_TRACER
    juce::int64 h = (effectState.getParent().indexOf (effectState) + 1) * 6356;

    for (int param = plugin.getNumAutomatableParameters(); --param >= 0;)
    {
        if (auto ap = plugin.getAutomatableParameter (param))
        {
            auto& ac = ap->getCurve();

            if (ac.getNumPoints() == 0)
            {
                h ^= (juce::int64) (ap->getCurrentValue() * 635);
            }
            else
            {
                for (int i = 0; i < ac.getNumPoints(); ++i)
                {
                    const auto p = ac.getPoint (i);
                    h ^= (juce::int64) (p.time * 63542) ^ (juce::int64) (p.value * 72634) ^ (juce::int64) (p.curve * 82635);
                }
            }
        }
    }

    return h;
}

//==============================================================================
struct ClipEffects::ClipPropertyWatcher  : private ValueTree::Listener,
                                           private AsyncUpdater
{
    ClipPropertyWatcher (ClipEffects& o) : clipEffects (o), clipState (o.clip.state)
    {
        clipState.addListener (this);
    }

private:
    ClipEffects& clipEffects;
    ValueTree clipState;

    void invalidateCache()
    {
        clipEffects.cachedClipProperties = nullptr;
        clipEffects.cachedHash = ClipEffects::hashNeedsRecaching;
    }

    void valueTreePropertyChanged (ValueTree& v, const Identifier& i) override
    {
        if (v == clipState)
        {
            using namespace IDs;

            if (matchesAnyOf (i, { start, length, offset }))
            {
                invalidateCache();
            }
            else if (matchesAnyOf (i, { speed, loopStart, loopLength, loopStartBeats, loopLengthBeats, autoTempo }))
            {
                invalidateCache();
                triggerAsyncUpdate();
            }
        }
    }

    void valueTreeChildAdded (ValueTree&, ValueTree&) override              {}
    void valueTreeChildRemoved (ValueTree&, ValueTree&, int) override       {}
    void valueTreeChildOrderChanged (ValueTree&, int, int) override         {}
    void valueTreeParentChanged (ValueTree&) override                       {}

    void handleAsyncUpdate() override
    {
        clipEffects.invalidateAllEffects();
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ClipPropertyWatcher)
};

ClipEffects::ClipEffects (const ValueTree& v, AudioClipBase& c)
    : ValueTreeObjectList<ClipEffect> (v), clip (c), state (v)
{
    rebuildObjects();
    clipPropertyWatcher = std::make_unique<ClipPropertyWatcher> (*this);
}

ClipEffects::~ClipEffects()
{
    freeObjects();
}

//==============================================================================
} namespace juce {

template <>
struct VariantConverter<tracktion_engine::ClipEffect::EffectType>
{
    static tracktion_engine::ClipEffect::EffectType fromVar (const var& v)
    {
        using namespace tracktion_engine;

        auto s = v.toString();

        if (s == "volume")              return ClipEffect::EffectType::volume;
        if (s == "fadeInOut")           return ClipEffect::EffectType::fadeInOut;
        if (s == "tapeStartStop")       return ClipEffect::EffectType::tapeStartStop;
        if (s == "stepVolume")          return ClipEffect::EffectType::stepVolume;
        if (s == "pitchShift")          return ClipEffect::EffectType::pitchShift;
        if (s == "warpTime")            return ClipEffect::EffectType::warpTime;
        if (s == "normalise")           return ClipEffect::EffectType::normalise;
        if (s == "makeMono")            return ClipEffect::EffectType::makeMono;
        if (s == "reverse")             return ClipEffect::EffectType::reverse;
        if (s == "invert")              return ClipEffect::EffectType::invert;
        if (s == "filter")              return ClipEffect::EffectType::filter;

        return ClipEffect::EffectType::none;
    }

    static var toVar (const tracktion_engine::ClipEffect::EffectType& t)
    {
        using namespace tracktion_engine;

        switch (t)
        {
            case ClipEffect::EffectType::none:              return {};
            case ClipEffect::EffectType::volume:            return "volume";
            case ClipEffect::EffectType::fadeInOut:         return "fadeInOut";
            case ClipEffect::EffectType::tapeStartStop:     return "tapeStartStop";
            case ClipEffect::EffectType::stepVolume:        return "stepVolume";
            case ClipEffect::EffectType::pitchShift:        return "pitchShift";
            case ClipEffect::EffectType::warpTime:          return "warpTime";
            case ClipEffect::EffectType::normalise:         return "normalise";
            case ClipEffect::EffectType::makeMono:          return "makeMono";
            case ClipEffect::EffectType::reverse:           return "reverse";
            case ClipEffect::EffectType::invert:            return "invert";
            case ClipEffect::EffectType::filter:            return "filter";
        }

        return {};
    }
};

} namespace tracktion_engine {

//==============================================================================
struct ClipEffect::ClipEffectRenderJob  : public ReferenceCountedObject
{
    using Ptr = ReferenceCountedObjectPtr<ClipEffectRenderJob>;

    ClipEffectRenderJob (Engine& e, const AudioFile& dest, const AudioFile& src)
        : engine (e), destination (dest), source (src)
    {
    }

    virtual ~ClipEffectRenderJob() {}

    /** Subclasses should override this to set-up their render process.
        Return true if the set-up completed successfully and the rest of the render callbacks
        should be called, false if there was a problem and the render should be stopped.
    */
    virtual bool setUpRender() = 0;

    /** During a render process this will be repeatedly called.
        Return true once all the blocks have completed, false if this needs to be called again.
    */
    virtual bool renderNextBlock() = 0;

    /** This is called once after all the render blocks have completed.
        Subclasses should override this to finish off their render by closing files and etc.
        returning true if everything completed successfully, false otherwise.
    */
    virtual bool completeRender() = 0;

    Engine& engine;
    const AudioFile destination, source;
    std::atomic<float> progress { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ClipEffectRenderJob)
};

//==============================================================================
ClipEffect::ClipEffect (const ValueTree& v, ClipEffects& o)
    : edit (o.clip.edit), state (v), clipEffects (o)
{
    state.addListener (this);
}

ValueTree ClipEffect::create (EffectType t)
{
    ValueTree v (IDs::EFFECT);
    v.setProperty (IDs::type, VariantConverter<ClipEffect::EffectType>::toVar (t), nullptr);

    return v;
}

void ClipEffect::createEffectAndAddToValueTree (Edit& edit, ValueTree parent, ClipEffect::EffectType effectType, int index)
{
    auto& undoManager = edit.getUndoManager();

    if (effectType == EffectType::filter)
    {
        if (auto af = edit.engine.getUIBehaviour()
                        .showMenuAndCreatePlugin (Plugin::Type::effectPlugins, edit))
        {
            af->setProcessingEnabled (false);
            af->flushPluginStateToValueTree();

            ValueTree v (IDs::EFFECT);
            v.setProperty (IDs::type, VariantConverter<ClipEffect::EffectType>::toVar (EffectType::filter), nullptr);
            v.addChild (af->state, -1, nullptr);

            if (parent.isValid())
                parent.addChild (v, index, &undoManager);
        }
        else if (parent.getNumChildren() == 0)
        {
            auto state = parent.getParent();

            if (state.isValid())
            {
                state.removeChild (parent, &undoManager);
                state.removeProperty (IDs::effectsVisible, &undoManager);
            }
        }
    }
    else
    {
        if (parent.isValid())
            parent.addChild (ClipEffect::create (effectType), index, &undoManager);
    }
}

String ClipEffect::getTypeDisplayName (EffectType t)
{
    switch (t)
    {
        case EffectType::volume:            return TRANS("Volume");
        case EffectType::fadeInOut:         return TRANS("Fade In/Out");
        case EffectType::tapeStartStop:     return TRANS("Tape Stop/Start");
        case EffectType::stepVolume:        return TRANS("Step Volume");
        case EffectType::pitchShift:        return TRANS("Pitch Shift");
        case EffectType::warpTime:          return TRANS("Warp Time");
        case EffectType::normalise:         return TRANS("Normalise");
        case EffectType::makeMono:          return TRANS("Mono");
        case EffectType::reverse:           return TRANS("Reverse");
        case EffectType::invert:            return TRANS("Phase Invert");
        case EffectType::filter:            return TRANS("Plugin");
        default:                            return "<" + TRANS ("None") + ">";
    }
}

void ClipEffect::addEffectsToMenu (PopupMenu& m)
{
    auto addItems = [&m] (StringRef heading, Array<EffectType> t)
    {
        m.addSectionHeader (heading);

        for (auto e : t)
            m.addItem ((int) e, getTypeDisplayName (e));
    };

    addItems (TRANS("Volume"),      { EffectType::volume, EffectType::fadeInOut, EffectType::stepVolume });
    addItems (TRANS("Time/Pitch"),  { EffectType::pitchShift, EffectType::tapeStartStop, EffectType::warpTime });
    addItems (TRANS("Effects"),     { EffectType::filter, EffectType::reverse });
    addItems (TRANS("Mastering"),   { EffectType::normalise, EffectType::makeMono, EffectType::invert });
}

ClipEffect::EffectType ClipEffect::getType() const
{
    return VariantConverter<ClipEffect::EffectType>::fromVar (state[IDs::type]);
}

juce::int64 ClipEffect::getIndividualHash() const
{
    return hashValueTree (0, state);
}

juce::int64 ClipEffect::getHash() const
{
    auto parent = state.getParent();
    auto index = parent.indexOf (state);
    juce::int64 hash = index ^ (juce::int64) clipEffects.clip.itemID.getRawID();

    for (int i = 0; i <= index; ++i)
        if (auto ce = clipEffects.getClipEffect (parent.getChild (i)))
            hash ^= ce->getIndividualHash() * (i + 1);

    jassert (hash != 0);
    return hash;
}

AudioFile ClipEffect::getSourceFile() const
{
    if (auto ce = clipEffects.getClipEffect (state.getSibling (-1)))
        return ce->getDestinationFile();

    return AudioFile (clipEffects.clip.getOriginalFile());
}

AudioFile ClipEffect::getDestinationFile() const
{
    if (destinationFile.isNull())
    {
        const File tempDir (clipEffects.clip.edit.getTempDirectory (true));

        // TODO: unifying the logic around proxy file naming
        destinationFile = AudioFile (tempDir.getChildFile (AudioClipBase::getClipProxyPrefix()
                                                            + "0_" + clipEffects.clip.itemID.toString()
                                                            + "_" + String::toHexString (getHash())
                                                            + ".wav"));
    }

    return destinationFile;
}

AudioClipBase& ClipEffect::getClip()
{
    return clipEffects.clip;
}

UndoManager& ClipEffect::getUndoManager()
{
    return clipEffects.clip.edit.getUndoManager();
}

bool ClipEffect::isUsingFile (const AudioFile& af) const
{
    return getDestinationFile() == af;
}

void ClipEffect::valueTreeChanged()
{
    clipEffects.effectChanged();
}

void ClipEffect::invalidateDestination()
{
    destinationFile = AudioFile();
    sourceChanged();
}

//==============================================================================
/** Takes an AudioNode and renders it to a file. */
struct AudioNodeRenderJob  : public ClipEffect::ClipEffectRenderJob
{
    AudioNodeRenderJob (Engine& e, AudioNode* n,
                        const AudioFile& dest, const AudioFile& src,
                        int blockSizeToUse = 32768, double prerollTimeSeconds = 0)
        : ClipEffectRenderJob (e, dest, src), node (n),
          blockSize (blockSizeToUse), prerollTime (prerollTimeSeconds)
    {
        jassert (node != nullptr);
    }

    bool setUpRender() override
    {
        CRASH_TRACER
        callBlocking ([this] { createAndPrepareRenderContext(); });
        return true;
    }

    bool renderNextBlock() override
    {
        CRASH_TRACER

        return renderContext->render (*node, progress) == ThreadPoolJob::jobHasFinished;
    }

    bool completeRender() override
    {
        bool ok = progress == 1.0f;

        if (ok)
        {
            ok = destination.deleteFile();
            jassert (ok);
            ok = renderContext->writer->file.getFile().moveFileTo (destination.getFile());
            jassert (ok);
        }

        renderContext->writer->file.deleteFile();

        return ok;
    }

    struct RenderContext
    {
        RenderContext (const AudioFile& destination, const AudioFile& source,
                       int blockSizeToUse = 32768, double prerollTimeS = 0)
            : blockSize (blockSizeToUse)
        {
            CRASH_TRACER
            jassert (source.isValid());
            streamRange = { 0.0, source.getLength() };
            jassert (! streamRange.isEmpty());

            auto sourceInfo = source.getInfo();
            jassert (sourceInfo.numChannels > 0 && sourceInfo.sampleRate > 0.0 && sourceInfo.bitsPerSample > 0);

            AudioFile tempFile (destination.getFile().getSiblingFile ("temp_effect_" + String::toHexString (Random::getSystemRandom().nextInt64()))
                                .withFileExtension (destination.getFile().getFileExtension()));

            // need to strip AIFF metadata to write to wav files
            if (sourceInfo.metadata.getValue ("MetaDataSource", "None") == "AIFF")
                sourceInfo.metadata.clear();

            writer.reset (new AudioFileWriter (tempFile, Engine::getInstance().getAudioFileFormatManager().getWavFormat(),
                                               sourceInfo.numChannels, sourceInfo.sampleRate,
                                               jmax (16, sourceInfo.bitsPerSample),
                                               sourceInfo.metadata, 0));

            renderingBuffer.reset (new juce::AudioBuffer<float> (writer->writer->getNumChannels(), blockSize + 256));
            auto renderingBufferChannels = AudioChannelSet::canonicalChannelSet (renderingBuffer->getNumChannels());

            // now prepare the render context
            rc.reset (new AudioRenderContext (localPlayhead, streamRange,
                                              renderingBuffer.get(),
                                              renderingBufferChannels, 0, blockSize,
                                              nullptr, 0.0,
                                              AudioRenderContext::playheadJumped, true));

            // round pre roll timr to nearest block
            numPreBlocks = (int) std::ceil ((prerollTimeS * sourceInfo.sampleRate) / blockSize);

            auto prerollTimeRounded = numPreBlocks * blockSizeToUse / writer->writer->getSampleRate();

            streamTime = -prerollTimeRounded;

            auto prerollStart = streamRange.getStart() - prerollTimeRounded;

            localPlayhead.setPosition (prerollStart);
            localPlayhead.playLockedToEngine ({ prerollStart, streamRange.getEnd() });
        }

        ThreadPoolJob::JobStatus render (AudioNode& audioNode, std::atomic<float>& progressToUpdate)
        {
            CRASH_TRACER
            auto blockLength = blockSize / writer->writer->getSampleRate();
            juce::int64 samplesToWrite = roundToInt ((streamRange.getEnd() - streamTime) * writer->writer->getSampleRate());
            auto blockEnd = jmin (streamTime + blockLength, streamRange.getEnd());
            rc->streamTime = { streamTime, blockEnd };

            // run blocks through the engine and discard
            if (numPreBlocks-- > 0)
            {
                audioNode.prepareForNextBlock (*rc);
                audioNode.renderOver (*rc);

                rc->continuity = AudioRenderContext::contiguous;
                streamTime = blockEnd;

                return ThreadPoolJob::jobNeedsRunningAgain;
            }

            const int numSamplesDone = (int) jmin (samplesToWrite, (juce::int64) blockSize);
            rc->bufferNumSamples = numSamplesDone;

            if (numSamplesDone > 0)
            {
                audioNode.prepareForNextBlock (*rc);
                audioNode.renderOver (*rc);
            }

            rc->continuity = AudioRenderContext::contiguous;
            streamTime = blockEnd;

            const float prog = (float) ((streamTime - streamRange.getStart()) / streamRange.getLength()) * 0.9f;
            progressToUpdate = jlimit (0.0f, 0.9f, prog);

            // NB buffer gets trashed by this call
            if (numSamplesDone <= 0 || ! writer->isOpen()
                 || ! writer->appendBuffer (*renderingBuffer, numSamplesDone))
            {
                // complete render
                localPlayhead.stop();
                writer->closeForWriting();

                if (numSamplesDone <= 0)
                    progressToUpdate = 1.0f;

                return ThreadPoolJob::jobHasFinished;
            }

            return ThreadPoolJob::jobNeedsRunningAgain;
        }

        const int blockSize;
        int numPreBlocks = 0;
        std::unique_ptr<AudioFileWriter> writer;
        PlayHead localPlayhead;
        std::unique_ptr<juce::AudioBuffer<float>> renderingBuffer;

        std::unique_ptr<AudioRenderContext> rc;
        EditTimeRange streamRange;
        double streamTime = 0;
    };

    std::unique_ptr<AudioNode> node;
    std::unique_ptr<RenderContext> renderContext;
    const int blockSize = 32768;
    const double prerollTime = 0;


    void createAndPrepareRenderContext()
    {
        renderContext.reset (new RenderContext (destination, source, blockSize, prerollTime));

        {
            AudioNodeProperties props;
            node->getAudioNodeProperties (props);
        }

        {
            Array<AudioNode*> allNodes;
            allNodes.add (node.get());

            PlaybackInitialisationInfo info =
            {
                0.0,
                renderContext->writer->writer->getSampleRate(),
                renderContext->blockSize,
                &allNodes,
                renderContext->rc->playhead
            };

            node->prepareAudioNodeToPlay (info);
        }
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioNodeRenderJob)
};

//==============================================================================
struct BlockBasedRenderJob : public ClipEffect::ClipEffectRenderJob
{
    BlockBasedRenderJob (Engine& e, const AudioFile& dest, const AudioFile& src, double sourceLengthSeconds)
        : ClipEffect::ClipEffectRenderJob (e, dest, src), sourceLength (sourceLengthSeconds)
    {
    }

    bool setUpRender() override
    {
        CRASH_TRACER
        auto sourceInfo = source.getInfo();
        jassert (sourceInfo.numChannels > 0 && sourceInfo.sampleRate > 0.0 && sourceInfo.bitsPerSample > 0);

        // need to strip AIFF metadata to write to wav files
        if (sourceInfo.metadata.getValue ("MetaDataSource", "None") == "AIFF")
            sourceInfo.metadata.clear();

        reader.reset (AudioFileUtils::createReaderFor (source.getFile()));

        if (reader == nullptr || reader->lengthInSamples == 0)
            return false;

        sourceLengthSamples = (juce::int64) (sourceLength * reader->sampleRate);

        writer.reset (new AudioFileWriter (destination, engine.getAudioFileFormatManager().getWavFormat(),
                                           sourceInfo.numChannels, sourceInfo.sampleRate,
                                           jmax (16, sourceInfo.bitsPerSample),
                                           sourceInfo.metadata, 0));

        return writer->isOpen();
    }

    bool completeRender() override
    {
        CRASH_TRACER
        reader = nullptr;
        writer = nullptr;

        return true;
    }

protected:
    std::unique_ptr<AudioFormatReader> reader;
    std::unique_ptr<AudioFileWriter> writer;

    double sourceLength = 0;
    juce::int64 position = 0;
    juce::int64 sourceLengthSamples = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BlockBasedRenderJob)
};

//==============================================================================
class WarpTimeEffectRenderJob :   public BlockBasedRenderJob
{
public:
    WarpTimeEffectRenderJob (Engine& e, const AudioFile& dest, const AudioFile& src,
                             double sourceLengthSeconds,
                             WarpTimeManager& wtm, AudioClipBase& c)
        : BlockBasedRenderJob (e, dest, src, sourceLengthSeconds),
          clip (c), warpTimeManager (wtm)
    {
        CRASH_TRACER
        auto tm = clip.getTimeStretchMode();
        proxyInfo.reset (new AudioClipBase::ProxyRenderingInfo());
        proxyInfo->clipTime     = { 0.0, wtm.getWarpEndMarkerTime() };
        proxyInfo->speedRatio   = 1.0;
        proxyInfo->mode         = (tm != TimeStretcher::disabled && tm != TimeStretcher::melodyne)
                                        ? tm : TimeStretcher::defaultMode;
    }

    bool renderNextBlock() override
    {
        CRASH_TRACER

        // Create this here to ensure the source file is valid
        jassert (source.isValid());
        proxyInfo->audioSegmentList = AudioSegmentList::create (clip, warpTimeManager, source);

        if (proxyInfo != nullptr
              && writer->isOpen()
              && proxyInfo->render (engine, source, *writer, nullptr, progress))
            return true;

        // if we fail here just copy the source to the destination to avoid loops
        source.getFile().copyFileTo (destination.getFile());
        jassertfalse;
        return true;
    }

private:
    std::unique_ptr<AudioClipBase::ProxyRenderingInfo> proxyInfo;
    AudioClipBase& clip;
    WarpTimeManager& warpTimeManager;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WarpTimeEffectRenderJob)
};

//==============================================================================
VolumeEffect::VolumeEffect (const ValueTree& v, ClipEffects& o)
    : ClipEffect (v, o)
{
    auto volState = state.getChildWithName (IDs::PLUGIN);

    if (! volState.isValid())
    {
        volState = VolumeAndPanPlugin::create();
        volState.setProperty (IDs::volume, decibelsToVolumeFaderPosition (0.0f), nullptr);
        state.addChild (volState, -1, nullptr);
    }

    plugin = new VolumeAndPanPlugin (edit, volState, false);
}

ReferenceCountedObjectPtr<ClipEffect::ClipEffectRenderJob> VolumeEffect::createRenderJob (const AudioFile& sourceFile, double sourceLength)
{
    CRASH_TRACER
    EditTimeRange timeRange (0.0, sourceLength);
    jassert (! timeRange.isEmpty());

    auto n = new WaveAudioNode (sourceFile, timeRange, 0.0, {}, {}, 1.0, AudioChannelSet::stereo());

    return new AudioNodeRenderJob (edit.engine, plugin->createAudioNode (n, false),
                                   getDestinationFile(), sourceFile);
}

bool VolumeEffect::hasProperties()
{
    return true;
}

void VolumeEffect::propertiesButtonPressed (SelectionManager& sm)
{
    if (plugin != nullptr)
        sm.selectOnly (*plugin);
}

juce::int64 VolumeEffect::getIndividualHash() const
{
    return plugin != nullptr ? hashPlugin (state, *plugin) : 0;
}

void VolumeEffect::valueTreePropertyChanged (ValueTree& v, const Identifier& i)
{
    // This is the automation writing back the AttachedValue so we need to ignore it
    if (plugin == nullptr || (plugin->isAutomationNeeded()
                               && v == plugin->state
                               && (matchesAnyOf (i, { IDs::volume, IDs::pan }))))
        return;

    ClipEffect::valueTreePropertyChanged (v, i);
}

void VolumeEffect::valueTreeChanged()
{
    ClipEffect::valueTreeChanged();

    inhibitor = std::make_unique<ClipEffects::RenderInhibitor> (clipEffects);
    startTimer (500);
}

void VolumeEffect::timerCallback()
{
    if (Component::isMouseButtonDownAnywhere())
        return;

    inhibitor = nullptr;
    stopTimer();
}

//==============================================================================
FadeInOutEffect::FadeInOutEffect (const ValueTree& v, ClipEffects& o)
    : ClipEffect (v, o)
{
    auto um = &getUndoManager();
    fadeIn.referTo (state, IDs::fadeIn, um);
    fadeOut.referTo (state, IDs::fadeOut, um);

    fadeInType.referTo (state, IDs::fadeInType, um, AudioFadeCurve::linear);
    fadeOutType.referTo (state, IDs::fadeOutType, um, AudioFadeCurve::linear);
}

void FadeInOutEffect::setFadeIn (double in)
{
    const double l = clipEffects.getEffectsLength() * clipEffects.getSpeedRatioEstimate();
    in = jlimit (0.0, l, in);

    if (in + fadeOut > l)
    {
        const double scale = l / (in + fadeOut);
        fadeIn = in * scale;
        fadeOut = fadeOut * scale;
    }
    else
    {
        fadeIn = in;
    }
}

void FadeInOutEffect::setFadeOut (double out)
{
    const double l = clipEffects.getEffectsLength() * clipEffects.getSpeedRatioEstimate();
    out = jlimit (0.0, l, out);

    if (fadeIn + out > l)
    {
        const double scale = l / (fadeIn + out);
        fadeIn = fadeIn * scale;
        fadeOut = out * scale;
    }
    else
    {
        fadeOut = out;
    }
}

ReferenceCountedObjectPtr<ClipEffect::ClipEffectRenderJob> FadeInOutEffect::createRenderJob (const AudioFile& sourceFile, double sourceLength)
{
    CRASH_TRACER
    AudioFile destFile (getDestinationFile());
    EditTimeRange timeRange (0.0, sourceLength);
    jassert (! timeRange.isEmpty());

    AudioNode* n = new WaveAudioNode (sourceFile, timeRange, 0.0, {}, {}, 1.0, AudioChannelSet::stereo());
    int blockSize = 32768;

    auto speedRatio = clipEffects.getSpeedRatioEstimate();
    auto effectRange = clipEffects.getEffectsRange();
    effectRange = { effectRange.getStart() * speedRatio,
                    effectRange.getEnd() * speedRatio };

    const EditTimeRange fadeInRange (effectRange.getStart(), effectRange.getStart() + fadeIn);
    const EditTimeRange fadeOutRange (effectRange.getEnd() - fadeOut, effectRange.getEnd());

    switch (getType())
    {
        case EffectType::fadeInOut:
            if (fadeIn > 0.0 || fadeOut > 0.0)
                n = new FadeInOutAudioNode (n, fadeInRange, fadeOutRange, fadeInType, fadeOutType);

            break;

        case EffectType::tapeStartStop:
            if (fadeIn > 0.0 || fadeOut > 0.0)
            {
                n = new SpeedRampAudioNode (n, fadeInRange, fadeOutRange, fadeInType, fadeOutType);
                blockSize = 128;
            }

            break;

        default:
            jassertfalse;
            break;
    }

    n = new TimedMutingAudioNode (n, { EditTimeRange (0.0, effectRange.getStart()),
                                       EditTimeRange (effectRange.getEnd(), sourceLength) });

    return new AudioNodeRenderJob (edit.engine, n, destFile, sourceFile, blockSize);
}

juce::int64 FadeInOutEffect::getIndividualHash() const
{
    auto effectRange = clipEffects.getEffectsRange();

    return ClipEffect::getIndividualHash()
        ^ (juce::int64) (clipEffects.getSpeedRatioEstimate() * 6345.2)
        ^ (juce::int64) (effectRange.getStart() * 3526.9)
        ^ (juce::int64) (effectRange.getEnd() * 53625.3);
}

//==============================================================================
StepVolumeEffect::StepVolumeEffect (const ValueTree& v, ClipEffects& o)
    : ClipEffect (v, o)
{
    CRASH_TRACER
    const bool newEffect = ! (state.hasProperty (IDs::noteLength) && state.hasProperty (IDs::crossfadeLength));

    auto um = &getUndoManager();
    noteLength.referTo (state, IDs::noteLength, um, 0.25);
    crossfade.referTo (state, IDs::crossfadeLength, um, 0.01);
    pattern.referTo (state, IDs::pattern, um);

    if (newEffect && ! edit.isLoading())
    {
        state.setProperty (IDs::noteLength, noteLength.get(), um);
        state.setProperty (IDs::crossfadeLength, crossfade.get(), um);

        Pattern (*this).toggleAtInterval (2);
    }
}

StepVolumeEffect::~StepVolumeEffect()
{
    notifyListenersOfDeletion();
}

int StepVolumeEffect::getMaxNumNotes()
{
    auto& ts = edit.tempoSequence;
    auto& c = getClip();
    auto pos = c.getPosition();

    auto startTime = pos.getStartOfSource();
    auto startBeat = ts.timeToBeats (startTime);
    auto endBeat = ts.timeToBeats (startTime + (c.getSourceLength() / clipEffects.getSpeedRatioEstimate()));

    return (int) std::ceil ((endBeat - startBeat) / noteLength);
}

ReferenceCountedObjectPtr<ClipEffect::ClipEffectRenderJob> StepVolumeEffect::createRenderJob (const AudioFile& sourceFile, double sourceLength)
{
    CRASH_TRACER
    jassert (sourceLength > 0);

    auto destFile = getDestinationFile();
    EditTimeRange timeRange (0.0, sourceLength);

    auto speedRatio = clipEffects.getSpeedRatioEstimate();
    auto effectRange = clipEffects.getEffectsRange();

    auto fade = crossfade.get();
    auto halfCrossfade = fade / 2.0;
    Array<EditTimeRange> nonMuteTimes;

    // Calculate non-mute times
    {
        const StepVolumeEffect::Pattern p (*this);
        auto cache = p.getPattern();
        auto& ts = edit.tempoSequence;
        auto& c = getClip();
        auto pos = c.getPosition();

        auto length = noteLength.get();
        auto startTime = pos.getStart();

        auto startBeat = ts.timeToBeats (pos.getStart() + effectRange.getStart());
        auto endBeat = ts.timeToBeats (pos.getEnd());
        auto numNotes = jmin (p.getNumNotes(), (int) std::ceil ((endBeat - startBeat) / length));

        auto beat = startBeat;

        for (int i = 0; i <= numNotes; ++i)
        {
            if (! cache[i])
            {
                beat += length;
                continue;
            }

            auto s = ts.beatsToTime (beat) - startTime;
            beat += length;
            auto e = ts.beatsToTime (beat) - startTime;

            nonMuteTimes.add ({ s - halfCrossfade,
                                e + halfCrossfade });
        }

        // Strip adjacent times
        auto lastTime = nonMuteTimes.getLast();

        for (int i = nonMuteTimes.size() - 1; --i >= 0;)
        {
            auto& thisTime = nonMuteTimes.getReference (i);

            if (thisTime.getEnd() >= lastTime.getStart())
            {
                thisTime.end = lastTime.getEnd();
                nonMuteTimes.remove (i + 1);
            }

            lastTime = thisTime;
        }

        // Scale everything by the speed ratio
        for (auto& t : nonMuteTimes)
            t = t.rescaled (0.0, speedRatio);
    }

    auto waveNode = new WaveAudioNode (sourceFile, timeRange, 0.0, {}, {}, 1.0, AudioChannelSet::stereo());
    auto compNode = TrackCompManager::createTrackCompAudioNode (waveNode, TrackCompManager::TrackComp::getMuteTimes (nonMuteTimes), nonMuteTimes, fade);

    return new AudioNodeRenderJob (edit.engine, compNode, destFile, sourceFile);
}

bool StepVolumeEffect::hasProperties()
{
    return true;
}

void StepVolumeEffect::propertiesButtonPressed (SelectionManager& sm)
{
    sm.selectOnly (this);
}

//==============================================================================
String StepVolumeEffect::getSelectableDescription()
{
    return TRANS("Step Volume Effect Editor");
}

juce::int64 StepVolumeEffect::getIndividualHash() const
{
    auto effectRange = clipEffects.getEffectsRange();

    return ClipEffect::getIndividualHash()
        ^ (juce::int64) (clipEffects.getSpeedRatioEstimate() * 6345.2)
        ^ (juce::int64) (effectRange.getStart() * 3526.9)
        ^ (juce::int64) (effectRange.getEnd() * 53625.3);
}

//==============================================================================
StepVolumeEffect::Pattern::Pattern (StepVolumeEffect& o) : effect (o), state (o.state)
{
}

StepVolumeEffect::Pattern::Pattern (const Pattern& other) noexcept
    : effect (other.effect), state (other.state)
{
}

bool StepVolumeEffect::Pattern::getNote (int index) const noexcept
{
    return getPattern()[index];
}

void StepVolumeEffect::Pattern::setNote (int index, bool value)
{
    if (getNote (index) != value)
    {
        auto p = getPattern();
        p.setBit (index, value);
        setPattern (p);
    }
}

BigInteger StepVolumeEffect::Pattern::getPattern() const noexcept
{
    BigInteger b;
    b.parseString (state[IDs::pattern].toString(), 2);
    return b;
}

void StepVolumeEffect::Pattern::setPattern (const BigInteger& b) noexcept
{
    state.setProperty (IDs::pattern, b.toString (2), &effect.getUndoManager());
}

void StepVolumeEffect::Pattern::clear()
{
    setPattern (BigInteger());
}

int StepVolumeEffect::Pattern::getNumNotes() const
{
    return getPattern().getHighestBit();
}

void StepVolumeEffect::Pattern::shiftChannel (bool toTheRight)
{
    BigInteger c (getPattern());

    // NB: Notes are added in reverse order
    if (toTheRight)
        c <<= 1;
    else
        c >>= 1;

    setPattern (c);
}

void StepVolumeEffect::Pattern::toggleAtInterval (int interval)
{
    BigInteger c (getPattern());

    for (int i = effect.getMaxNumNotes(); --i >= 0;)
        c.setBit (i, (i % interval) == 0);

    setPattern (c);
}

void StepVolumeEffect::Pattern::randomiseChannel()
{
    clear();

    Random r;

    for (int i = 0; i < effect.getMaxNumNotes(); ++i)
        setNote (i, r.nextBool());
}

//==============================================================================
PitchShiftEffect::PitchShiftEffect (const ValueTree& v, ClipEffects& o)
    : ClipEffect (v, o)
{
    CRASH_TRACER
    auto pitchState = state.getChildWithName (IDs::PLUGIN);

    if (! pitchState.isValid())
    {
        pitchState = PitchShiftPlugin::create();
        state.addChild (pitchState, -1, nullptr);
    }

    plugin = new PitchShiftPlugin (edit, pitchState);
}

ReferenceCountedObjectPtr<ClipEffect::ClipEffectRenderJob> PitchShiftEffect::createRenderJob (const AudioFile& sourceFile, double sourceLength)
{
    CRASH_TRACER
    const EditTimeRange timeRange (0.0, sourceLength);
    jassert (! timeRange.isEmpty());

    auto n = new WaveAudioNode (sourceFile, timeRange, 0.0, {}, {}, 1.0, AudioChannelSet::stereo());

    // Use 1.0 second of preroll to be safe. We can't ask the plugin since it
    // may not be initialized yet
    return new AudioNodeRenderJob (edit.engine, plugin->createAudioNode (n, false),
                                   getDestinationFile(), sourceFile, 512, 1.0);
}

bool PitchShiftEffect::hasProperties()
{
    return true;
}

void PitchShiftEffect::propertiesButtonPressed (SelectionManager& sm)
{
    if (plugin != nullptr)
        sm.selectOnly (*plugin);
}

juce::int64 PitchShiftEffect::getIndividualHash() const
{
    return hashPlugin (state, *plugin);
}

void PitchShiftEffect::valueTreePropertyChanged (ValueTree& v, const Identifier& i)
{
    // This is the automation writing back the AttachedValue so we need to ignore it
    if (plugin != nullptr
        && v == plugin->state
        && matchesAnyOf (i, { IDs::semitonesUp })
        && plugin->isAutomationNeeded())
        return;

    ClipEffect::valueTreePropertyChanged (v, i);
}

void PitchShiftEffect::valueTreeChanged()
{
    ClipEffect::valueTreeChanged();

    inhibitor = std::make_unique<ClipEffects::RenderInhibitor> (clipEffects);
    startTimer (250);
}

void PitchShiftEffect::timerCallback()
{
    if (Component::isMouseButtonDownAnywhere())
        return;

    inhibitor = nullptr;
    stopTimer();
}

//==============================================================================
WarpTimeEffect::WarpTimeEffect (const ValueTree& v, ClipEffects& o)
    : ClipEffect (v, o)
{
    CRASH_TRACER
    warpTimeManager = new WarpTimeManager (edit, {}, state);
    editLoadedCallback = std::make_unique<Edit::LoadFinishedCallback<WarpTimeEffect>> (*this, edit);
}

ReferenceCountedObjectPtr<ClipEffect::ClipEffectRenderJob> WarpTimeEffect::createRenderJob (const AudioFile& sourceFile, double sourceLength)
{
    CRASH_TRACER
    return new WarpTimeEffectRenderJob (edit.engine, getDestinationFile(), sourceFile,
                                        sourceLength, *warpTimeManager, getClip());
}

juce::int64 WarpTimeEffect::getIndividualHash() const
{
    return warpTimeManager->getHash();
}

void WarpTimeEffect::sourceChanged()
{
    warpTimeManager->setSourceFile (getSourceFile());
}

void WarpTimeEffect::editFinishedLoading()
{
    sourceChanged();
    editLoadedCallback = nullptr;
}

//==============================================================================
struct PluginUnloadInhibitor    : private Timer
{
    PluginUnloadInhibitor (Plugin::Ptr p, std::function<void (void)> cb)
        : plugin (p), callback (std::move (cb))
    {
        if (plugin->isProcessingEnabled())
            callback();
    }

    ~PluginUnloadInhibitor()
    {
        if (count > 0)
            unload();
    }

    void increase()
    {
        if (count++ == 0)
            load();
    }

    void increaseForJob (int ms, ReferenceCountedObjectPtr<AudioNodeRenderJob> job)
    {
        if (! isTimerRunning())
            increase();

        jobs.add (job);
        startTimer (ms);
    }

    void decrease()
    {
        if (--count == 0)
            unload();
    }

    void timerCallback() override
    {
        for (int i = jobs.size(); --i >= 0;)
        {
            if (jobs[i]->progress >= 1.0f)
                jobs.remove (i);
            else
                return;
        }

        decrease();
        stopTimer();
    }

    void load()
    {
        callBlocking ([this]() { plugin->setProcessingEnabled (true); callback(); });
    }

    void unload()
    {
        callBlocking ([this]() { plugin->setProcessingEnabled (false); callback(); });
    }

    int count = 0;
    Plugin::Ptr plugin;
    ReferenceCountedArray<AudioNodeRenderJob> jobs;
    std::function<void(void)> callback;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginUnloadInhibitor)
};

//==============================================================================
ScopedPluginUnloadInhibitor::ScopedPluginUnloadInhibitor (PluginUnloadInhibitor& o) : owner (o)  { owner.increase(); }
ScopedPluginUnloadInhibitor::~ScopedPluginUnloadInhibitor()                                      { owner.decrease(); }

//==============================================================================
PluginEffect::PluginEffect (const ValueTree& v, ClipEffects& o)
    : ClipEffect (v, o)
{
    CRASH_TRACER
    auto um = &getUndoManager();
    currentCurve.referTo (state, IDs::currentCurve, um);
    lastHash.referTo (state, IDs::hash, nullptr);

    auto pluginState = state.getChildWithName (IDs::PLUGIN);
    jassert (pluginState.isValid());

    if (pluginState.isValid())
    {
        pluginState.setProperty (IDs::process, false, nullptr); // always restore plugin to non-processing state on load
        callBlocking ([this, pluginState]() { plugin = edit.getPluginCache().getOrCreatePluginFor (pluginState); });
    }

    if (plugin != nullptr)
    {
        auto loadCallback = [this]()
        {
            if (plugin != nullptr)
            {
                if (plugin->isProcessingEnabled())
                {
                    for (int i = 0; i < plugin->getNumAutomatableParameters(); i++)
                        plugin->getAutomatableParameter (i)->addListener (this);
                }
                else
                {
                    for (int i = 0; i < plugin->getNumAutomatableParameters(); i++)
                        plugin->getAutomatableParameter (i)->removeListener (this);
                }
            }
        };

        pluginUnloadInhibitor = std::make_unique<PluginUnloadInhibitor> (plugin, loadCallback);
    }
}

ReferenceCountedObjectPtr<ClipEffect::ClipEffectRenderJob> PluginEffect::createRenderJob (const AudioFile& sourceFile,
                                                                                          double sourceLength)
{
    CRASH_TRACER

    const ScopedPluginUnloadInhibitor lock (*pluginUnloadInhibitor);

    const EditTimeRange timeRange (0.0, sourceLength);
    jassert (! timeRange.isEmpty());

    AudioNode* n = new WaveAudioNode (sourceFile, timeRange, 0.0, {}, {}, 1.0, AudioChannelSet::stereo());

    if (plugin != nullptr)
    {
        plugin->setProcessingEnabled (true);
        n = plugin->createAudioNode (n, false);
    }

    // Use 1.0 second of preroll to be safe. We can't ask the plugin since it
    // may not be initialized yet
    auto job = new AudioNodeRenderJob (edit.engine, n, getDestinationFile(), sourceFile, 512, 1.0);

    if (pluginUnloadInhibitor != nullptr)
        pluginUnloadInhibitor->increaseForJob (30 * 1000, job);

    return job;
}

void PluginEffect::flushStateToValueTree()
{
    if (plugin != nullptr)
        plugin->flushPluginStateToValueTree();
}

bool PluginEffect::hasProperties()
{
    return true;
}

void PluginEffect::propertiesButtonPressed (SelectionManager& sm)
{
    if (plugin != nullptr)
    {
        sm.selectOnly (*plugin);

        if (auto ep = dynamic_cast<ExternalPlugin*> (plugin.get()))
            ep->showWindowExplicitly();
    }
}

juce::int64 PluginEffect::getIndividualHash() const
{
    jassert (plugin != nullptr);

    // only calculate a hash if the plugin is loaded. If the plugin is unloaded,
    // the hash can't change
    if (plugin != nullptr && (plugin->isProcessingEnabled() || lastHash == 0))
    {
        const ScopedPluginUnloadInhibitor lock (*pluginUnloadInhibitor);
        lastHash = hashPlugin (state, *plugin);
    }

    return lastHash;
}

void PluginEffect::valueTreePropertyChanged (ValueTree& v, const Identifier& i)
{
    // This is the automation writing back the AttachedValue so we need to ignore it
    if (plugin != nullptr
        && v == plugin->state
        && plugin->isAutomationNeeded())
        return;

    if (matchesAnyOf (i, { IDs::hash, IDs::process }))
        return;

    ClipEffect::valueTreePropertyChanged (v, i);
}

void PluginEffect::valueTreeChanged()
{
    // Override this to avoid the plugin's properties constantly triggering re-renders
}

void PluginEffect::timerCallback()
{
    if (Component::isMouseButtonDownAnywhere())
        return;

    inhibitor = nullptr;
    stopTimer();
}

void PluginEffect::curveHasChanged (AutomatableParameter&)
{
    ClipEffect::valueTreeChanged();

    if (inhibitor == nullptr)
        inhibitor = std::make_unique<ClipEffects::RenderInhibitor> (clipEffects);

    startTimer (250);
}

//==============================================================================
struct NormaliseEffect::NormaliseRenderJob : public BlockBasedRenderJob
{
    NormaliseRenderJob (Engine& e, const AudioFile& dest, const AudioFile& src, double sourceLength, double gain)
        : BlockBasedRenderJob (e, dest, src, sourceLength), maxGain (gain) {}

    bool renderNextBlock() override
    {
        CRASH_TRACER

        if (! calculatedGain)
        {
            calculatedGain = true;

            float lmin, lmax, rmin, rmax;
            reader->readMaxLevels (0, sourceLengthSamples, lmin, lmax, rmin, rmax);

            auto maxLevel = jmax (-lmin, lmax, -rmin, rmax);
            gainFactor = dbToGain (float (maxGain)) / maxLevel;
        }

        auto todo = (int) jmin (32768ll, sourceLengthSamples - position);

        AudioScratchBuffer scratch ((int) reader->numChannels, todo);

        reader->read (&scratch.buffer, 0, todo, position, true, true);

        scratch.buffer.applyGain (0, todo, gainFactor);

        writer->appendBuffer (scratch.buffer, todo);

        position += todo;
        progress = float (position) / float (sourceLengthSamples);

        return position >= sourceLengthSamples;
    }

    const double maxGain = 1.0;

    bool calculatedGain = false;
    float gainFactor = 0.0f;
};

NormaliseEffect::NormaliseEffect (const ValueTree& v, ClipEffects& o)
    : ClipEffect (v, o)
{
    auto um = &getUndoManager();
    maxLevelDB.referTo (state, IDs::level, um, 0.0);
}

NormaliseEffect::~NormaliseEffect()
{
    notifyListenersOfDeletion();
}

ReferenceCountedObjectPtr<ClipEffect::ClipEffectRenderJob> NormaliseEffect::createRenderJob (const AudioFile& sourceFile, double sourceLength)
{
    CRASH_TRACER

    return new NormaliseRenderJob (edit.engine, getDestinationFile(),
                                   sourceFile, sourceLength, maxLevelDB.get());
}

bool NormaliseEffect::hasProperties()
{
    return true;
}

void NormaliseEffect::propertiesButtonPressed (SelectionManager& sm)
{
    sm.selectOnly (this);
}

String NormaliseEffect::getSelectableDescription()
{
    return TRANS("Normalise Effect Editor");
}

//==============================================================================
struct MakeMonoEffect::MakeMonoRenderJob : public BlockBasedRenderJob
{
    MakeMonoRenderJob (Engine& e, const AudioFile& dest, const AudioFile& src, double sourceLength, SrcChannels srcCh)
        : BlockBasedRenderJob (e, dest, src, sourceLength), srcChannels (srcCh) {}

    bool setUpRender() override
    {
        CRASH_TRACER
        auto sourceInfo = source.getInfo();
        jassert (sourceInfo.numChannels > 0 && sourceInfo.sampleRate > 0.0 && sourceInfo.bitsPerSample > 0);

        // need to strip AIFF metadata to write to wav files
        if (sourceInfo.metadata.getValue ("MetaDataSource", "None") == "AIFF")
            sourceInfo.metadata.clear();

        reader.reset (AudioFileUtils::createReaderFor (source.getFile()));

        if (reader == nullptr || reader->lengthInSamples == 0)
            return false;

        sourceLengthSamples = (juce::int64) (sourceLength * reader->sampleRate);

        writer.reset (new AudioFileWriter (destination, engine.getAudioFileFormatManager().getWavFormat(),
                                           1, sourceInfo.sampleRate,
                                           jmax (16, sourceInfo.bitsPerSample),
                                           sourceInfo.metadata, 0));

        return writer->isOpen();
    }

    bool renderNextBlock() override
    {
        CRASH_TRACER
        auto todo = (int) jmin (32768ll, sourceLengthSamples - position);

        AudioScratchBuffer input ((int) reader->numChannels, todo);
        reader->read (&input.buffer, 0, todo, position, true, true);

        if (reader->numChannels == 1)
        {
            writer->appendBuffer (input.buffer, todo);
        }
        else
        {
            AudioScratchBuffer output (1, todo);

            if (srcChannels == chLR)
            {
                output.buffer.copyFrom (0, 0, input.buffer.getReadPointer (0), todo, 0.5f);
                output.buffer.addFrom (0, 0, input.buffer.getReadPointer (1), todo, 0.5f);
            }
            else if (srcChannels == chL)
            {
                output.buffer.copyFrom (0, 0, input.buffer.getReadPointer (0), todo);
            }
            else if (srcChannels == chR)
            {
                output.buffer.copyFrom (0, 0, input.buffer.getReadPointer (1), todo);
            }
            else
            {
                jassertfalse;
            }

            writer->appendBuffer (output.buffer, todo);
        }

        position += todo;
        progress = float(position) / float(sourceLengthSamples);

        return position >= sourceLengthSamples;
    }

    const SrcChannels srcChannels;
};

MakeMonoEffect::MakeMonoEffect (const ValueTree& v, ClipEffects& o)
    : ClipEffect (v, o)
{
    auto um = &getUndoManager();
    channels.referTo (state, IDs::channels, um, 0);
}

MakeMonoEffect::~MakeMonoEffect()
{
    notifyListenersOfDeletion();
}

ReferenceCountedObjectPtr<ClipEffect::ClipEffectRenderJob> MakeMonoEffect::createRenderJob (const AudioFile& sourceFile, double sourceLength)
{
    CRASH_TRACER
    return new MakeMonoRenderJob (edit.engine, getDestinationFile(), sourceFile,
                                  sourceLength, (SrcChannels) channels.get());
}

bool MakeMonoEffect::hasProperties()
{
    return true;
}

void MakeMonoEffect::propertiesButtonPressed (SelectionManager& sm)
{
    sm.selectOnly (this);
}

String MakeMonoEffect::getSelectableDescription()
{
    return TRANS("Make Mono Editor");
}

//==============================================================================
struct ReverseEffect::ReverseRenderJob  : public BlockBasedRenderJob
{
    ReverseRenderJob (Engine& e, const AudioFile& dest, const AudioFile& src, double sourceLength)
        : BlockBasedRenderJob (e, dest, src, sourceLength) {}

    bool renderNextBlock() override
    {
        CRASH_TRACER
        auto todo = (int) jmin (32768ll, sourceLengthSamples - position);

        AudioScratchBuffer scratch ((int) reader->numChannels, todo);

        reader->read (&scratch.buffer, 0, todo, sourceLengthSamples - position - todo, true, true);

        scratch.buffer.reverse (0, todo);

        writer->appendBuffer (scratch.buffer, todo);

        position += todo;
        progress = float(position) / float(sourceLengthSamples);

        return position >= sourceLengthSamples;
    }
};

ReverseEffect::ReverseEffect (const ValueTree& v, ClipEffects& o)
    : ClipEffect (v, o)
{
}

ReferenceCountedObjectPtr<ClipEffect::ClipEffectRenderJob> ReverseEffect::createRenderJob (const AudioFile& sourceFile, double sourceLength)
{
    CRASH_TRACER

    return new ReverseRenderJob (edit.engine, getDestinationFile(), sourceFile, sourceLength);
}

//==============================================================================
struct InvertEffect::InvertRenderJob : public BlockBasedRenderJob
{
    InvertRenderJob (Engine& e, const AudioFile& dest, const AudioFile& src, double sourceLength)
        : BlockBasedRenderJob (e, dest, src, sourceLength) {}

    bool renderNextBlock() override
    {
        CRASH_TRACER
        auto todo = (int) jmin (32768ll, sourceLengthSamples - position);

        AudioScratchBuffer scratch ((int) reader->numChannels, todo);

        reader->read (&scratch.buffer, 0, todo, position, true, true);

        scratch.buffer.applyGain (0, todo, -1.0f);

        writer->appendBuffer (scratch.buffer, todo);

        position += todo;
        progress = float (position) / float (sourceLengthSamples);

        return position >= sourceLengthSamples;
    }
};

InvertEffect::InvertEffect (const ValueTree& v, ClipEffects& o)
    : ClipEffect (v, o)
{
}

ReferenceCountedObjectPtr<ClipEffect::ClipEffectRenderJob> InvertEffect::createRenderJob (const AudioFile& sourceFile, double sourceLength)
{
    CRASH_TRACER
    return new InvertRenderJob (edit.engine, getDestinationFile(), sourceFile, sourceLength);
}

//==============================================================================
ClipEffect* ClipEffect::create (const ValueTree& v, ClipEffects& ce)
{
    switch (VariantConverter<ClipEffect::EffectType>::fromVar (v[IDs::type]))
    {
        case EffectType::none:              jassertfalse; return {};
        case EffectType::volume:            return new VolumeEffect (v, ce);
        case EffectType::fadeInOut:
        case EffectType::tapeStartStop:     return new FadeInOutEffect (v, ce);
        case EffectType::stepVolume:        return new StepVolumeEffect (v, ce);
        case EffectType::pitchShift:        return new PitchShiftEffect (v, ce);
        case EffectType::warpTime:          return new WarpTimeEffect (v, ce);
        case EffectType::normalise:         return new NormaliseEffect (v, ce);
        case EffectType::makeMono:          return new MakeMonoEffect (v, ce);
        case EffectType::reverse:           return new ReverseEffect (v, ce);
        case EffectType::invert:            return new InvertEffect (v, ce);
        case EffectType::filter:            return new PluginEffect (v, ce);
        default:                            jassertfalse; return {};
    }
}

//==============================================================================
struct AggregateJob  : public RenderManager::Job
{
    AggregateJob (Engine& e, const AudioFile& destFile, const AudioFile& source,
                  ReferenceCountedArray<ClipEffect::ClipEffectRenderJob> j)
        : Job (e, destFile),
          sourceFile (source), lastFile (source.getFile()),
          jobs (std::move (j)), originalNumTasks (jobs.size())
    {
    }

    bool setUpRender() override
    {
        return true;
    }

    bool renderNextBlock() override
    {
        CRASH_TRACER
        if (! sourceFile.isValid())
        {
            Thread::sleep (100);
            return false;
        }

        if (currentJob == nullptr)
        {
            currentJob = jobs.removeAndReturn (0);

            if (currentJob != nullptr)
                if (! currentJob->setUpRender())
                    return true;
        }
        else
        {
            if (currentJob->renderNextBlock())
            {
                if (! currentJob->completeRender())
                    return true;

                auto& afm = engine.getAudioFileManager();

                afm.releaseFile (currentJob->destination);

                if (! currentJob->destination.isNull())
                    callBlocking ([this, &afm]()
                                  {
                                      afm.validateFile (currentJob->destination, true);
                                      jassert (currentJob->destination.isValid());
                                  });

                lastFile = currentJob->destination.getFile();
                currentJob = nullptr;
                ++numJobsCompleted;
            }
        }

        const float jobShare = 1.0f / jmax (1, originalNumTasks);
        progress = (numJobsCompleted * jobShare) + (jobShare * (currentJob != nullptr ? currentJob->progress.load() : 0.0f));

        return currentJob == nullptr && jobs.isEmpty();
    }

    bool completeRender() override
    {
        return lastFile.copyFileTo (proxy.getFile()) && jobs.isEmpty();
    }

    const AudioFile sourceFile;
    File lastFile;
    ReferenceCountedArray<ClipEffect::ClipEffectRenderJob> jobs;
    ClipEffect::ClipEffectRenderJob::Ptr currentJob;
    const int originalNumTasks;
    int numJobsCompleted = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AggregateJob)
};

//==============================================================================
RenderManager::Job::Ptr ClipEffects::createRenderJob (const AudioFile& destFile, const AudioFile& sourceFile) const
{
    CRASH_TRACER
    clip.edit.getTransport().forceOrphanFreezeAndProxyFilesPurge();

    const double length = sourceFile.getLength();
    AudioFile inputFile (sourceFile);
    ReferenceCountedArray<ClipEffect::ClipEffectRenderJob> jobs;

    for (auto ce : objects)
    {
        const AudioFile af (ce->getDestinationFile());

        if (af.getFile().existsAsFile() && af.isValid())
        {
            inputFile = af;
        }
        else if (ClipEffect::ClipEffectRenderJob::Ptr j = ce->createRenderJob (inputFile, length))
        {
            inputFile = j->destination;
            jobs.add (j);
        }
    }

    AudioFile firstFile (jobs.isEmpty() ? inputFile : jobs.getFirst()->source);

    return new AggregateJob (clip.edit.engine, destFile, firstFile, std::move (jobs));
}
