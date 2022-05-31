/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#include "../../playback/audionodes/tracktion_AudioNode.h"
#include "../../playback/audionodes/tracktion_WaveAudioNode.h"
#include "../../playback/audionodes/tracktion_TrackCompAudioNode.h"
#include "../../playback/audionodes/tracktion_SpeedRampAudioNode.h"
#include "../../playback/audionodes/tracktion_PluginAudioNode.h"
#include "../../playback/audionodes/tracktion_FadeInOutAudioNode.h"

namespace tracktion { inline namespace engine
{

static inline HashCode hashValueTree (HashCode startHash, const juce::ValueTree& v)
{
    startHash ^= v.getType().toString().hashCode64() * (v.getParent().indexOf (v) + 1);

    for (int i = v.getNumProperties(); --i >= 0;)
        startHash ^= v[v.getPropertyName (i)].toString().hashCode64();

    for (int i = v.getNumChildren(); --i >= 0;)
        startHash ^= hashValueTree (startHash, v.getChild (i));

    return startHash;
}

static inline HashCode hashPlugin (const juce::ValueTree& effectState, Plugin& plugin)
{
    CRASH_TRACER
    HashCode h = juce::String (effectState.getParent().indexOf (effectState) + 1).hashCode64();

    for (int param = plugin.getNumAutomatableParameters(); --param >= 0;)
    {
        if (auto ap = plugin.getAutomatableParameter (param))
        {
            auto& ac = ap->getCurve();

            if (ac.getNumPoints() == 0)
            {
                h = (juce::String (h) + juce::String (ap->getCurrentValue())).hashCode64();
            }
            else
            {
                for (int i = 0; i < ac.getNumPoints(); ++i)
                {
                    const auto p = ac.getPoint (i);
                    auto pointH = juce::String (p.time.inSeconds()) + juce::String (p.value) + juce::String (p.curve);
                    h = (juce::String (h) + pointH).hashCode64();
                }
            }
        }
    }

    return h;
}

//==============================================================================
struct ClipEffects::ClipPropertyWatcher  : private juce::ValueTree::Listener,
                                           private juce::AsyncUpdater
{
    ClipPropertyWatcher (ClipEffects& o) : clipEffects (o), clipState (o.clip.state)
    {
        clipState.addListener (this);
    }

private:
    ClipEffects& clipEffects;
    juce::ValueTree clipState;

    void invalidateCache()
    {
        clipEffects.cachedClipProperties = nullptr;
        clipEffects.cachedHash = ClipEffects::hashNeedsRecaching;
    }

    void valueTreePropertyChanged (juce::ValueTree& v, const juce::Identifier& i) override
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

    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override        {}
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override {}
    void valueTreeChildOrderChanged (juce::ValueTree&, int, int) override         {}
    void valueTreeParentChanged (juce::ValueTree&) override                       {}

    void handleAsyncUpdate() override
    {
        clipEffects.invalidateAllEffects();
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ClipPropertyWatcher)
};

ClipEffects::ClipEffects (const juce::ValueTree& v, AudioClipBase& c)
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
}} // namespace tracktion { inline namespace engine

namespace juce {

template <>
struct VariantConverter<tracktion::engine::ClipEffect::EffectType>
{
    static tracktion::engine::ClipEffect::EffectType fromVar (const var& v)
    {
        using namespace tracktion::engine;

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

    static var toVar (const tracktion::engine::ClipEffect::EffectType& t)
    {
        using namespace tracktion::engine;

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

} namespace tracktion { inline namespace engine {

//==============================================================================
struct ClipEffect::ClipEffectRenderJob  : public juce::ReferenceCountedObject
{
    using Ptr = juce::ReferenceCountedObjectPtr<ClipEffectRenderJob>;

    static constexpr SampleCount defaultBlockSize = 1024;

    ClipEffectRenderJob (Engine& e,
                         const AudioFile& dest, const AudioFile& src,
                         SampleCount blockSizeToUse)
        : engine (e), destination (dest),
          source (src), blockSize (blockSizeToUse)
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
    const SampleCount blockSize;
    std::atomic<float> progress { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ClipEffectRenderJob)
};

//==============================================================================
ClipEffect::ClipEffect (const juce::ValueTree& v, ClipEffects& o)
    : edit (o.clip.edit), state (v), clipEffects (o), destinationFile (edit.engine)
{
    state.addListener (this);
}

juce::ValueTree ClipEffect::create (EffectType t)
{
    return createValueTree (IDs::EFFECT,
                            IDs::type, juce::VariantConverter<ClipEffect::EffectType>::toVar (t));
}

void ClipEffect::createEffectAndAddToValueTree (Edit& edit, juce::ValueTree parent, ClipEffect::EffectType effectType, int index)
{
    auto& undoManager = edit.getUndoManager();

    if (effectType == EffectType::filter)
    {
        if (auto af = edit.engine.getUIBehaviour()
                        .showMenuAndCreatePlugin (Plugin::Type::effectPlugins, edit))
        {
            af->setProcessingEnabled (false);
            af->flushPluginStateToValueTree();

            auto v = createValueTree (IDs::EFFECT,
                                      IDs::type, juce::VariantConverter<ClipEffect::EffectType>::toVar (EffectType::filter));

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

juce::String ClipEffect::getTypeDisplayName (EffectType t)
{
    switch (t)
    {
        case EffectType::volume:            return TRANS("Volume");
        case EffectType::fadeInOut:         return TRANS("Fade in/out");
        case EffectType::tapeStartStop:     return TRANS("Tape stop/start");
        case EffectType::stepVolume:        return TRANS("Step volume");
        case EffectType::pitchShift:        return TRANS("Pitch shift");
        case EffectType::warpTime:          return TRANS("Warp time");
        case EffectType::normalise:         return TRANS("Normalise");
        case EffectType::makeMono:          return TRANS("Mono");
        case EffectType::reverse:           return TRANS("Reverse");
        case EffectType::invert:            return TRANS("Phase Invert");
        case EffectType::filter:            return TRANS("Plugin");
        case EffectType::none:
        default:                            return "<" + TRANS ("None") + ">";
    }
}

void ClipEffect::addEffectsToMenu (juce::PopupMenu& m)
{
    auto addItems = [&m] (juce::StringRef heading, juce::Array<EffectType> t)
    {
        m.addSectionHeader (heading);

        for (auto e : t)
            m.addItem ((int) e, getTypeDisplayName (e));
    };

    addItems (TRANS("Volume"),      { EffectType::fadeInOut, EffectType::stepVolume, EffectType::volume });
    addItems (TRANS("Time/Pitch"),  { EffectType::pitchShift, EffectType::tapeStartStop, EffectType::warpTime });
    addItems (TRANS("Effects"),     { EffectType::filter, EffectType::reverse });
    addItems (TRANS("Mastering"),   { EffectType::makeMono, EffectType::normalise, EffectType::invert });
}

ClipEffect::EffectType ClipEffect::getType() const
{
    return juce::VariantConverter<ClipEffect::EffectType>::fromVar (state[IDs::type]);
}

HashCode ClipEffect::getIndividualHash() const
{
    return hashValueTree (0, state);
}

HashCode ClipEffect::getHash() const
{
    auto parent = state.getParent();
    auto index = parent.indexOf (state);
    HashCode hash = index ^ static_cast<HashCode> (clipEffects.clip.itemID.getRawID());

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

    return AudioFile (clipEffects.clip.edit.engine, clipEffects.clip.getOriginalFile());
}

AudioFile ClipEffect::getDestinationFile() const
{
    if (destinationFile.isNull())
        destinationFile = TemporaryFileManager::getFileForCachedFileRender (edit, getHash());

    return destinationFile;
}

AudioClipBase& ClipEffect::getClip()
{
    return clipEffects.clip;
}

juce::UndoManager& ClipEffect::getUndoManager()
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
    destinationFile = AudioFile (edit.engine);
    sourceChanged();
}

//==============================================================================
/** Takes an AudioNode and renders it to a file. */
struct AudioNodeRenderJob  : public ClipEffect::ClipEffectRenderJob
{
    AudioNodeRenderJob (Engine& e, AudioNode* n,
                        const AudioFile& dest, const AudioFile& src,
                        SampleCount blockSizeToUse = defaultBlockSize,
                        double prerollTimeSeconds = 0)
        : ClipEffectRenderJob (e, dest, src, blockSizeToUse),
          node (n), prerollTime (prerollTimeSeconds)
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
        return renderContext->render (*node, progress) == juce::ThreadPoolJob::jobHasFinished;
    }

    bool completeRender() override
    {
        bool ok = progress == 1.0f;

        if (ok)
        {
            ok = destination.deleteFile();
            (void) ok;
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
                       SampleCount blockSizeToUse, double prerollTimeS)
            : blockSize (blockSizeToUse)
        {
            CRASH_TRACER
            jassert (source.isValid());
            streamRange = { 0.0, source.getLength() };
            jassert (! streamRange.isEmpty());

            auto sourceInfo = source.getInfo();
            jassert (sourceInfo.numChannels > 0 && sourceInfo.sampleRate > 0.0 && sourceInfo.bitsPerSample > 0);

            AudioFile tempFile (*destination.engine,
                                destination.getFile().getSiblingFile ("temp_effect_" + juce::String::toHexString (juce::Random::getSystemRandom().nextInt64()))
                                .withFileExtension (destination.getFile().getFileExtension()));

            // need to strip AIFF metadata to write to wav files
            if (sourceInfo.metadata.getValue ("MetaDataSource", "None") == "AIFF")
                sourceInfo.metadata.clear();

            writer.reset (new AudioFileWriter (tempFile, destination.engine->getAudioFileFormatManager().getWavFormat(),
                                               sourceInfo.numChannels, sourceInfo.sampleRate,
                                               std::max (16, sourceInfo.bitsPerSample),
                                               sourceInfo.metadata, 0));

            renderingBuffer = std::make_unique<juce::AudioBuffer<float>> (writer->getNumChannels(), (int) blockSize + 256);
            auto renderingBufferChannels = juce::AudioChannelSet::canonicalChannelSet (renderingBuffer->getNumChannels());

            // now prepare the render context
            rc.reset (new AudioRenderContext (localPlayhead, streamRange,
                                              renderingBuffer.get(),
                                              renderingBufferChannels, 0, (int) blockSize,
                                              nullptr, 0.0,
                                              AudioRenderContext::playheadJumped, true));

            // round pre roll timr to nearest block
            numPreBlocks = (int) std::ceil ((prerollTimeS * sourceInfo.sampleRate) / blockSize);

            auto prerollTimeRounded = numPreBlocks * blockSizeToUse / writer->getSampleRate();

            streamTime = -prerollTimeRounded;

            auto prerollStart = streamRange.getStart() - prerollTimeRounded;

            localPlayhead.setPosition (prerollStart);
            localPlayhead.playLockedToEngine ({ prerollStart, streamRange.getEnd() });
        }

        juce::ThreadPoolJob::JobStatus render (AudioNode& audioNode, std::atomic<float>& progressToUpdate)
        {
            CRASH_TRACER
            auto blockLength = blockSize / writer->getSampleRate();
            SampleCount samplesToWrite = juce::roundToInt ((streamRange.getEnd() - streamTime) * writer->getSampleRate());
            auto blockEnd = std::min (streamTime + blockLength, streamRange.getEnd());
            rc->streamTime = { streamTime, blockEnd };

            // run blocks through the engine and discard
            if (numPreBlocks-- > 0)
            {
                audioNode.prepareForNextBlock (*rc);
                audioNode.renderOver (*rc);

                rc->continuity = AudioRenderContext::contiguous;
                streamTime = blockEnd;

                return juce::ThreadPoolJob::jobNeedsRunningAgain;
            }

            auto numSamplesDone = (int) std::min (samplesToWrite, blockSize);
            rc->bufferNumSamples = numSamplesDone;

            if (numSamplesDone > 0)
            {
                audioNode.prepareForNextBlock (*rc);
                audioNode.renderOver (*rc);
            }

            rc->continuity = AudioRenderContext::contiguous;
            streamTime = blockEnd;

            const float prog = (float) ((streamTime - streamRange.getStart()) / streamRange.getLength()) * 0.9f;
            progressToUpdate = juce::jlimit (0.0f, 0.9f, prog);

            // NB buffer gets trashed by this call
            if (numSamplesDone <= 0 || ! writer->isOpen()
                 || ! writer->appendBuffer (*renderingBuffer, numSamplesDone))
            {
                // complete render
                localPlayhead.stop();
                writer->closeForWriting();

                if (numSamplesDone <= 0)
                    progressToUpdate = 1.0f;

                return juce::ThreadPoolJob::jobHasFinished;
            }

            return juce::ThreadPoolJob::jobNeedsRunningAgain;
        }

        const SampleCount blockSize;
        int numPreBlocks = 0;
        std::unique_ptr<AudioFileWriter> writer;
        PlayHead localPlayhead;
        std::unique_ptr<juce::AudioBuffer<float>> renderingBuffer;

        std::unique_ptr<AudioRenderContext> rc;
        legacy::EditTimeRange streamRange;
        double streamTime = 0;
    };

    std::unique_ptr<AudioNode> node;
    std::unique_ptr<RenderContext> renderContext;
    const double prerollTime = 0;

    void createAndPrepareRenderContext()
    {
        renderContext.reset (new RenderContext (destination, source, blockSize, prerollTime));

        {
            AudioNodeProperties props;
            node->getAudioNodeProperties (props);
        }

        {
            juce::Array<AudioNode*> allNodes;
            allNodes.add (node.get());

            PlaybackInitialisationInfo info =
            {
                0.0,
                renderContext->writer->getSampleRate(),
                (int) renderContext->blockSize,
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
    BlockBasedRenderJob (Engine& e, const AudioFile& dest, const AudioFile& src, double sourceLength)
        : ClipEffect::ClipEffectRenderJob (e, dest, src, defaultBlockSize),
          sourceLengthSeconds (sourceLength)
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

        reader.reset (AudioFileUtils::createReaderFor (engine, source.getFile()));

        if (reader == nullptr || reader->lengthInSamples == 0)
            return false;

        sourceLengthSamples = static_cast<SampleCount> (sourceLengthSeconds * reader->sampleRate);

        writer.reset (new AudioFileWriter (destination, engine.getAudioFileFormatManager().getWavFormat(),
                                           sourceInfo.numChannels, sourceInfo.sampleRate,
                                           std::max (16, sourceInfo.bitsPerSample),
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
    std::unique_ptr<juce::AudioFormatReader> reader;
    std::unique_ptr<AudioFileWriter> writer;

    double sourceLengthSeconds = 0;
    SampleCount position = 0, sourceLengthSamples = 0;

    SampleCount getNumSamplesForCurrentBlock() const
    {
        return std::min (blockSize, sourceLengthSamples - position);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BlockBasedRenderJob)
};

//==============================================================================
class WarpTimeEffectRenderJob :   public BlockBasedRenderJob
{
public:
    WarpTimeEffectRenderJob (Engine& e, const AudioFile& dest, const AudioFile& src,
                             double sourceLength,
                             WarpTimeManager& wtm, AudioClipBase& c)
        : BlockBasedRenderJob (e, dest, src, sourceLength),
          clip (c), warpTimeManager (wtm)
    {
        CRASH_TRACER
        auto tm = clip.getTimeStretchMode();
        proxyInfo.reset (new AudioClipBase::ProxyRenderingInfo());
        proxyInfo->clipTime     = { {}, wtm.getWarpEndMarkerTime() };
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
VolumeEffect::VolumeEffect (const juce::ValueTree& v, ClipEffects& o)
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

juce::ReferenceCountedObjectPtr<ClipEffect::ClipEffectRenderJob> VolumeEffect::createRenderJob (const AudioFile& sourceFile,
                                                                                                double sourceLength)
{
    CRASH_TRACER
    legacy::EditTimeRange timeRange (0.0, sourceLength);
    jassert (! timeRange.isEmpty());

    auto n = new WaveAudioNode (sourceFile, timeRange, 0.0, {}, {},
                                1.0, juce::AudioChannelSet::stereo());

    return new AudioNodeRenderJob (edit.engine, new PluginAudioNode (plugin, n, false),
                                   getDestinationFile(), sourceFile, 128);
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

HashCode VolumeEffect::getIndividualHash() const
{
    return plugin != nullptr ? hashPlugin (state, *plugin) : 0;
}

void VolumeEffect::valueTreePropertyChanged (juce::ValueTree& v, const juce::Identifier& i)
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
    if (juce::Component::isMouseButtonDownAnywhere())
        return;

    inhibitor = nullptr;
    stopTimer();
}

//==============================================================================
FadeInOutEffect::FadeInOutEffect (const juce::ValueTree& v, ClipEffects& o)
    : ClipEffect (v, o)
{
    auto um = &getUndoManager();
    fadeIn.referTo (state, IDs::fadeIn, um);
    fadeOut.referTo (state, IDs::fadeOut, um);

    fadeInType.referTo (state, IDs::fadeInType, um, AudioFadeCurve::linear);
    fadeOutType.referTo (state, IDs::fadeOutType, um, AudioFadeCurve::linear);
}

void FadeInOutEffect::setFadeIn (TimeDuration in)
{
    const auto l = clipEffects.getEffectsLength() * clipEffects.getSpeedRatioEstimate();
    in = juce::jlimit (TimeDuration(), l, in);

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

void FadeInOutEffect::setFadeOut (TimeDuration out)
{
    const auto l = clipEffects.getEffectsLength() * clipEffects.getSpeedRatioEstimate();
    out = juce::jlimit (TimeDuration(), l, out);

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

juce::ReferenceCountedObjectPtr<ClipEffect::ClipEffectRenderJob> FadeInOutEffect::createRenderJob (const AudioFile& sourceFile,
                                                                                                   double sourceLength)
{
    CRASH_TRACER
    AudioFile destFile (getDestinationFile());
    legacy::EditTimeRange timeRange (0.0, sourceLength);
    jassert (! timeRange.isEmpty());

    AudioNode* n = new WaveAudioNode (sourceFile, timeRange, 0.0, {}, {},
                                      1.0, juce::AudioChannelSet::stereo());
    auto blockSize = AudioNodeRenderJob::defaultBlockSize;

    auto speedRatio = clipEffects.getSpeedRatioEstimate();
    auto effectRange = clipEffects.getEffectsRange();
    effectRange = { effectRange.getStart() * speedRatio,
                    effectRange.getEnd() * speedRatio };

    const TimeRange fadeInRange (effectRange.getStart(), effectRange.getStart() + fadeIn);
    const TimeRange fadeOutRange (effectRange.getEnd() - fadeOut, effectRange.getEnd());

    switch (getType())
    {
        case EffectType::fadeInOut:
            if (fadeIn.get() > TimeDuration() || fadeOut.get() > TimeDuration())
                n = new FadeInOutAudioNode (n,
                                            toEditTimeRange (fadeInRange), toEditTimeRange (fadeOutRange),
                                            fadeInType, fadeOutType);
            
            break;
            
        case EffectType::tapeStartStop:
            if (fadeIn.get() > TimeDuration() || fadeOut.get() > TimeDuration())
            {
                n = new SpeedRampAudioNode (n,
                                            toEditTimeRange (fadeInRange), toEditTimeRange (fadeOutRange),
                                            fadeInType, fadeOutType);
                blockSize = 128;
            }
            
            break;
            
        case EffectType::none:
        case EffectType::volume:
        case EffectType::stepVolume:
        case EffectType::pitchShift:
        case EffectType::warpTime:
        case EffectType::normalise:
        case EffectType::makeMono:
        case EffectType::reverse:
        case EffectType::invert:
        case EffectType::filter:
        default:
            jassertfalse;
            break;
    }

    n = new TimedMutingAudioNode (n, { legacy::EditTimeRange ({}, effectRange.getStart().inSeconds()),
                                       legacy::EditTimeRange (effectRange.getEnd().inSeconds(), sourceLength) });

    return new AudioNodeRenderJob (edit.engine, n, destFile, sourceFile, blockSize);
}

HashCode FadeInOutEffect::getIndividualHash() const
{
    auto effectRange = clipEffects.getEffectsRange();

    return ClipEffect::getIndividualHash()
             ^ static_cast<HashCode> (clipEffects.getSpeedRatioEstimate() * 6345.2)
             ^ static_cast<HashCode> (effectRange.getStart().inSeconds() * 3526.9)
             ^ static_cast<HashCode> (effectRange.getEnd().inSeconds() * 53625.3);
}

//==============================================================================
StepVolumeEffect::StepVolumeEffect (const juce::ValueTree& v, ClipEffects& o)
    : ClipEffect (v, o)
{
    CRASH_TRACER
    const bool newEffect = ! (state.hasProperty (IDs::noteLength) && state.hasProperty (IDs::crossfadeLength));

    auto um = &getUndoManager();
    noteLength.referTo (state, IDs::noteLength, um, BeatDuration::fromBeats (0.25));
    crossfade.referTo (state, IDs::crossfadeLength, um, 0.01);
    pattern.referTo (state, IDs::pattern, um);

    if (newEffect && ! edit.isLoading())
    {
        state.setProperty (IDs::noteLength, noteLength.get().inBeats(), um);
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
    auto startBeat = ts.toBeats (startTime);
    auto endBeat = ts.toBeats (startTime + (c.getSourceLength() / clipEffects.getSpeedRatioEstimate()));

    return (int) std::ceil ((endBeat - startBeat) / noteLength);
}

juce::ReferenceCountedObjectPtr<ClipEffect::ClipEffectRenderJob> StepVolumeEffect::createRenderJob (const AudioFile& sourceFile,
                                                                                                    double sourceLength)
{
    CRASH_TRACER
    jassert (sourceLength > 0);

    auto destFile = getDestinationFile();
    legacy::EditTimeRange timeRange (0.0, sourceLength);

    auto speedRatio = clipEffects.getSpeedRatioEstimate();
    auto effectRange = clipEffects.getEffectsRange();

    auto fade = crossfade.get();
    auto halfCrossfade = TimeDuration::fromSeconds (fade / 2.0);
    juce::Array<TimeRange> nonMuteTimes;

    // Calculate non-mute times
    {
        const StepVolumeEffect::Pattern p (*this);
        auto cache = p.getPattern();
        auto& ts = edit.tempoSequence;
        auto& c = getClip();
        auto pos = c.getPosition();

        auto length = noteLength.get();
        auto startTime = pos.getStart();

        auto startBeat = ts.toBeats (pos.getStart() + toDuration (effectRange.getStart()));
        auto endBeat = ts.toBeats (pos.getEnd());
        auto numNotes = std::min (p.getNumNotes(), (int) std::ceil ((endBeat - startBeat) / length));

        auto beat = startBeat;

        for (int i = 0; i <= numNotes; ++i)
        {
            if (! cache[i])
            {
                beat = beat + length;
                continue;
            }

            auto s = ts.toTime (beat) - toDuration (startTime);
            beat = beat + length;
            auto e = ts.toTime (beat) - toDuration (startTime);

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
                thisTime = thisTime.withEnd (lastTime.getEnd());
                nonMuteTimes.remove (i + 1);
            }

            lastTime = thisTime;
        }

        // Scale everything by the speed ratio
        for (auto& t : nonMuteTimes)
            t = t.rescaled (TimePosition(), speedRatio);
    }

    auto waveNode = new WaveAudioNode (sourceFile, timeRange, 0.0, {}, {},
                                       1.0, juce::AudioChannelSet::stereo());
    auto compNode = createTrackCompAudioNode (waveNode,
                                              TrackCompManager::TrackComp::getMuteTimes (nonMuteTimes),
                                              nonMuteTimes, TimeDuration::fromSeconds (fade));

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
juce::String StepVolumeEffect::getSelectableDescription()
{
    return TRANS("Step Volume Effect Editor");
}

HashCode StepVolumeEffect::getIndividualHash() const
{
    auto effectRange = clipEffects.getEffectsRange();

    return ClipEffect::getIndividualHash()
            ^ static_cast<HashCode> (clipEffects.getSpeedRatioEstimate() * 6345.2)
            ^ static_cast<HashCode> (effectRange.getStart().inSeconds() * 3526.9)
            ^ static_cast<HashCode> (effectRange.getEnd().inSeconds() * 53625.3);
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

juce::BigInteger StepVolumeEffect::Pattern::getPattern() const noexcept
{
    juce::BigInteger b;
    b.parseString (state[IDs::pattern].toString(), 2);
    return b;
}

void StepVolumeEffect::Pattern::setPattern (const juce::BigInteger& b) noexcept
{
    state.setProperty (IDs::pattern, b.toString (2), &effect.getUndoManager());
}

void StepVolumeEffect::Pattern::clear()
{
    setPattern ({});
}

int StepVolumeEffect::Pattern::getNumNotes() const
{
    return getPattern().getHighestBit();
}

void StepVolumeEffect::Pattern::shiftChannel (bool toTheRight)
{
    auto c = getPattern();

    // NB: Notes are added in reverse order
    if (toTheRight)
        c <<= 1;
    else
        c >>= 1;

    setPattern (c);
}

void StepVolumeEffect::Pattern::toggleAtInterval (int interval)
{
    auto c = getPattern();

    for (int i = effect.getMaxNumNotes(); --i >= 0;)
        c.setBit (i, (i % interval) == 0);

    setPattern (c);
}

void StepVolumeEffect::Pattern::randomiseChannel()
{
    clear();
    juce::Random r;

    for (int i = 0; i < effect.getMaxNumNotes(); ++i)
        setNote (i, r.nextBool());
}

//==============================================================================
PitchShiftEffect::PitchShiftEffect (const juce::ValueTree& v, ClipEffects& o)
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

void PitchShiftEffect::initialise()
{
    if (plugin != nullptr)
        for (auto ap : plugin->getAutomatableParameters())
            ap->updateStream();
}

juce::ReferenceCountedObjectPtr<ClipEffect::ClipEffectRenderJob> PitchShiftEffect::createRenderJob (const AudioFile& sourceFile, double sourceLength)
{
    CRASH_TRACER
    const legacy::EditTimeRange timeRange (0.0, sourceLength);
    jassert (! timeRange.isEmpty());

    auto n = new WaveAudioNode (sourceFile, timeRange, 0.0, {}, {},
                                1.0, juce::AudioChannelSet::stereo());

    // Use 1.0 second of preroll to be safe. We can't ask the plugin since it
    // may not be initialized yet
    return new AudioNodeRenderJob (edit.engine, new PluginAudioNode (plugin, n, false),
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

HashCode PitchShiftEffect::getIndividualHash() const
{
    return hashPlugin (state, *plugin);
}

void PitchShiftEffect::valueTreePropertyChanged (juce::ValueTree& v, const juce::Identifier& i)
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
    if (juce::Component::isMouseButtonDownAnywhere())
        return;

    inhibitor = nullptr;
    stopTimer();
}

//==============================================================================
WarpTimeEffect::WarpTimeEffect (const juce::ValueTree& v, ClipEffects& o)
    : ClipEffect (v, o)
{
    CRASH_TRACER
    warpTimeManager = new WarpTimeManager (edit, AudioFile (edit.engine), state);
    editLoadedCallback = std::make_unique<Edit::LoadFinishedCallback<WarpTimeEffect>> (*this, edit);
}

juce::ReferenceCountedObjectPtr<ClipEffect::ClipEffectRenderJob> WarpTimeEffect::createRenderJob (const AudioFile& sourceFile,
                                                                                                  double sourceLength)
{
    CRASH_TRACER
    return new WarpTimeEffectRenderJob (edit.engine, getDestinationFile(), sourceFile,
                                        sourceLength, *warpTimeManager, getClip());
}

HashCode WarpTimeEffect::getIndividualHash() const
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
struct PluginUnloadInhibitor    : private juce::Timer
{
    PluginUnloadInhibitor (Plugin::Ptr p, std::function<void (void)> cb)
        : plugin (p), callback (std::move (cb))
    {
        if (plugin->isProcessingEnabled())
            callback();
    }

    ~PluginUnloadInhibitor() override
    {
        if (count > 0)
            unload();
    }

    void increase()
    {
        if (count++ == 0)
            load();
    }

    void increaseForJob (int ms, juce::ReferenceCountedObjectPtr<AudioNodeRenderJob> job)
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
    juce::ReferenceCountedArray<AudioNodeRenderJob> jobs;
    std::function<void(void)> callback;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginUnloadInhibitor)
};

//==============================================================================
ScopedPluginUnloadInhibitor::ScopedPluginUnloadInhibitor (PluginUnloadInhibitor& o) : owner (o)  { owner.increase(); }
ScopedPluginUnloadInhibitor::~ScopedPluginUnloadInhibitor()                                      { owner.decrease(); }

//==============================================================================
PluginEffect::PluginEffect (const juce::ValueTree& v, ClipEffects& o)
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

juce::ReferenceCountedObjectPtr<ClipEffect::ClipEffectRenderJob> PluginEffect::createRenderJob (const AudioFile& sourceFile,
                                                                                                double sourceLength)
{
    CRASH_TRACER

    const ScopedPluginUnloadInhibitor lock (*pluginUnloadInhibitor);

    const legacy::EditTimeRange timeRange (0.0, sourceLength);
    jassert (! timeRange.isEmpty());

    AudioNode* n = new WaveAudioNode (sourceFile, timeRange, 0.0, {}, {},
                                      1.0, juce::AudioChannelSet::stereo());

    if (plugin != nullptr)
    {
        plugin->setProcessingEnabled (true);
        n = new PluginAudioNode (plugin, n, false);
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

        plugin->showWindowExplicitly();
    }
}

HashCode PluginEffect::getIndividualHash() const
{
    jassert (plugin != nullptr);

    // only calculate a hash if the plugin is loaded. If the plugin is unloaded,
    // the hash can't change
    if (plugin != nullptr && (plugin->isProcessingEnabled() || lastHash == 0))
    {
        const ScopedPluginUnloadInhibitor lock (*pluginUnloadInhibitor);
        lastHash = (juce::int64) hashPlugin (state, *plugin);
    }

    return lastHash;
}

void PluginEffect::valueTreePropertyChanged (juce::ValueTree& v, const juce::Identifier& i)
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
    if (juce::Component::isMouseButtonDownAnywhere())
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

            auto maxLevel = juce::jmax (-lmin, lmax, -rmin, rmax);
            gainFactor = dbToGain (float (maxGain)) / maxLevel;
        }

        auto todo = (int) getNumSamplesForCurrentBlock();

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

NormaliseEffect::NormaliseEffect (const juce::ValueTree& v, ClipEffects& o)
    : ClipEffect (v, o)
{
    auto um = &getUndoManager();
    maxLevelDB.referTo (state, IDs::level, um, 0.0);
}

NormaliseEffect::~NormaliseEffect()
{
    notifyListenersOfDeletion();
}

juce::ReferenceCountedObjectPtr<ClipEffect::ClipEffectRenderJob> NormaliseEffect::createRenderJob (const AudioFile& sourceFile, double sourceLength)
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

juce::String NormaliseEffect::getSelectableDescription()
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

        reader.reset (AudioFileUtils::createReaderFor (engine, source.getFile()));

        if (reader == nullptr || reader->lengthInSamples == 0)
            return false;

        sourceLengthSamples = static_cast<SampleCount> (sourceLengthSeconds * reader->sampleRate);

        writer.reset (new AudioFileWriter (destination, engine.getAudioFileFormatManager().getWavFormat(),
                                           1, sourceInfo.sampleRate,
                                           std::max (16, sourceInfo.bitsPerSample),
                                           sourceInfo.metadata, 0));

        return writer->isOpen();
    }

    bool renderNextBlock() override
    {
        CRASH_TRACER
        auto todo = (int) getNumSamplesForCurrentBlock();

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

MakeMonoEffect::MakeMonoEffect (const juce::ValueTree& v, ClipEffects& o)
    : ClipEffect (v, o)
{
    auto um = &getUndoManager();
    channels.referTo (state, IDs::channels, um, 0);
}

MakeMonoEffect::~MakeMonoEffect()
{
    notifyListenersOfDeletion();
}

juce::ReferenceCountedObjectPtr<ClipEffect::ClipEffectRenderJob> MakeMonoEffect::createRenderJob (const AudioFile& sourceFile, double sourceLength)
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

juce::String MakeMonoEffect::getSelectableDescription()
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
        auto todo = (int) getNumSamplesForCurrentBlock();

        AudioScratchBuffer scratch ((int) reader->numChannels, todo);

        reader->read (&scratch.buffer, 0, todo, sourceLengthSamples - position - todo, true, true);

        scratch.buffer.reverse (0, todo);

        writer->appendBuffer (scratch.buffer, todo);

        position += todo;
        progress = float(position) / float(sourceLengthSamples);

        return position >= sourceLengthSamples;
    }
};

ReverseEffect::ReverseEffect (const juce::ValueTree& v, ClipEffects& o)
    : ClipEffect (v, o)
{
}

juce::ReferenceCountedObjectPtr<ClipEffect::ClipEffectRenderJob> ReverseEffect::createRenderJob (const AudioFile& sourceFile,
                                                                                                 double sourceLength)
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
        auto todo = (int) getNumSamplesForCurrentBlock();

        AudioScratchBuffer scratch ((int) reader->numChannels, todo);

        reader->read (&scratch.buffer, 0, todo, position, true, true);

        scratch.buffer.applyGain (0, todo, -1.0f);

        writer->appendBuffer (scratch.buffer, todo);

        position += todo;
        progress = float (position) / float (sourceLengthSamples);

        return position >= sourceLengthSamples;
    }
};

InvertEffect::InvertEffect (const juce::ValueTree& v, ClipEffects& o)
    : ClipEffect (v, o)
{
}

juce::ReferenceCountedObjectPtr<ClipEffect::ClipEffectRenderJob> InvertEffect::createRenderJob (const AudioFile& sourceFile,
                                                                                                double sourceLength)
{
    CRASH_TRACER
    return new InvertRenderJob (edit.engine, getDestinationFile(), sourceFile, sourceLength);
}

//==============================================================================
ClipEffect* ClipEffect::create (const juce::ValueTree& v, ClipEffects& ce)
{
    switch (juce::VariantConverter<ClipEffect::EffectType>::fromVar (v[IDs::type]))
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
                  juce::ReferenceCountedArray<ClipEffect::ClipEffectRenderJob> j)
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
            juce::Thread::sleep (100);
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
                    callBlocking ([&afm, fileToValidate = currentJob->destination]
                                  {
                                      afm.validateFile (fileToValidate, true);
                                      jassert (fileToValidate.isValid());
                                  });

                lastFile = currentJob->destination.getFile();
                currentJob = nullptr;
                ++numJobsCompleted;
            }
        }

        const float jobShare = 1.0f / std::max (1, originalNumTasks);
        progress = (numJobsCompleted * jobShare) + (jobShare * (currentJob != nullptr ? currentJob->progress.load() : 0.0f));

        return currentJob == nullptr && jobs.isEmpty();
    }

    bool completeRender() override
    {
        return lastFile.copyFileTo (proxy.getFile()) && jobs.isEmpty();
    }

    const AudioFile sourceFile;
    juce::File lastFile;
    juce::ReferenceCountedArray<ClipEffect::ClipEffectRenderJob> jobs;
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
    juce::ReferenceCountedArray<ClipEffect::ClipEffectRenderJob> jobs;

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

}} // namespace tracktion { inline namespace engine
