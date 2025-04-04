/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#include "../../playback/graph/tracktion_TracktionEngineNode.h"
#include "../../playback/graph/tracktion_TracktionNodePlayer.h"
#include "../../playback/graph/tracktion_SpeedRampWaveNode.h"
#include "../../playback/graph/tracktion_WaveNode.h"
#include "../../playback/graph/tracktion_PluginNode.h"
#include "../../playback/graph/tracktion_FadeInOutNode.h"
#include "../../playback/graph/tracktion_TimedMutingNode.h"

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
    AudioNodeRenderJob (Engine& e,
                        const AudioFile& dest, const AudioFile& src,
                        SampleCount blockSizeToUse = defaultBlockSize)
        : ClipEffectRenderJob (e, dest, src, blockSizeToUse)
    {
    }

    static juce::ReferenceCountedObjectPtr<AudioNodeRenderJob> create (Engine& e,
                                                                       const AudioFile& dest, const AudioFile& src,
                                                                       SampleCount blockSizeToUse)
    {
        return new AudioNodeRenderJob (e, dest, src, blockSizeToUse);
    }

    void initialise (std::unique_ptr<tracktion::graph::Node> nodeToUse,
                     double prerollTimeSeconds = 0)
    {
        prerollTime = prerollTimeSeconds;
        node = std::move (nodeToUse);
        jassert (node != nullptr);
    }

    bool setUpRender() override
    {
        CRASH_TRACER
        try
        {
            callBlocking ([this] { createAndPrepareRenderContext(); });
            return true;
        }
        catch (std::runtime_error& err)
        {
            TRACKTION_LOG_ERROR (err.what());
        }

        return false;
    }

    bool renderNextBlock() override
    {
        CRASH_TRACER
        return renderContext->render (progress) == juce::ThreadPoolJob::jobHasFinished;
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

    std::unique_ptr<tracktion::graph::Node> createWaveNodeForFile (const AudioFile& file, TimeRange timeRange)
    {
        return tracktion::graph::makeNode<WaveNode> (file, timeRange, TimeDuration(), TimeRange(), LiveClipLevel(), 1.0,
                                                     juce::AudioChannelSet::canonicalChannelSet (file.getInfo().numChannels),
                                                     juce::AudioChannelSet::stereo(),
                                                     processState,
                                                     EditItemID(), true);
    }

    tracktion::graph::PlayHead playHead;
    tracktion::graph::PlayHeadState playHeadState { playHead };
    ProcessState processState { playHeadState };

private:
    //==============================================================================
    struct RenderContext
    {
        RenderContext (std::unique_ptr<tracktion::graph::Node> nodeToUse,
                       ProcessState& processStateToUse,
                       const AudioFile& destination, const AudioFile& source,
                       int numDestChannels, SampleCount blockSizeToUse, double prerollTimeS)
            : blockSize (blockSizeToUse)
        {
            CRASH_TRACER
            jassert (source.isValid());
            jassert (numDestChannels > 0);
            streamRange = TimeRange (0s, TimeDuration::fromSeconds (source.getLength()));
            jassert (! streamRange.isEmpty());

            auto sourceInfo = source.getInfo();
            jassert (sourceInfo.numChannels > 0 && sourceInfo.sampleRate > 0.0 && sourceInfo.bitsPerSample > 0);

            AudioFile tempFile (*destination.engine,
                                destination.getFile().getSiblingFile ("temp_effect_" + juce::String::toHexString (juce::Random::getSystemRandom().nextInt64()))
                                .withFileExtension (destination.getFile().getFileExtension()));

            // need to strip AIFF metadata to write to wav files
            if (sourceInfo.metadata.getValue ("MetaDataSource", "None") == "AIFF")
                sourceInfo.metadata.clear();

            writer = std::make_unique<AudioFileWriter> (tempFile, destination.engine->getAudioFileFormatManager().getWavFormat(),
                                                        numDestChannels, sourceInfo.sampleRate,
                                                        std::max (16, sourceInfo.bitsPerSample),
                                                        sourceInfo.metadata, 0);

            sampleRate = writer->getSampleRate();

            // round preroll time to nearest block
            numPreBlocks = (int) std::ceil ((prerollTimeS * sourceInfo.sampleRate) / blockSize);
            auto prerollTimeRounded = TimeDuration::fromSeconds ((numPreBlocks * blockSizeToUse) / writer->getSampleRate());
            streamRange = streamRange.withStart (streamRange.getStart() - prerollTimeRounded);

            totalSamples = juce::roundToInt (streamRange.getLength().inSeconds() * sampleRate);

            processStateToUse.playHeadState.playHead.playSyncedToRange ({ 0, totalSamples });

            nodePlayer = std::make_unique<TracktionNodePlayer> (std::move (nodeToUse), processStateToUse, sampleRate, blockSize,
                                                                getPoolCreatorFunction (ThreadPoolStrategy::realTime));

            nodePlayer->setNumThreads (0);
            nodePlayer->prepareToPlay (sampleRate, (int) blockSize);

            audioBuffer = choc::buffer::ChannelArrayBuffer<float> ((choc::buffer::ChannelCount) numDestChannels, (choc::buffer::FrameCount) blockSize);
        }

        juce::ThreadPoolJob::JobStatus render (std::atomic<float>& progressToUpdate)
        {
            CRASH_TRACER
            auto samplesToDo = (choc::buffer::FrameCount) std::min (blockSize, (SampleCount) (totalSamples - samplesDone));

            tracktion::graph::Node::ProcessContext pc
            {
                samplesToDo,
                juce::Range<int64_t> (samplesDone, samplesDone + samplesToDo),

                {
                    audioBuffer.getStart (samplesToDo),
                    midiBuffer
                }
            };

            if (samplesToDo > 0)
            {
                pc.buffers.audio.clear();
                pc.buffers.midi.clear();

                auto misses = nodePlayer->process (pc);
                jassert (misses == 0);

                samplesDone += samplesToDo;
            }

            // run blocks through the engine and discard
            if (numPreBlocks-- > 0)
                return juce::ThreadPoolJob::jobNeedsRunningAgain;

            progressToUpdate = juce::jlimit (0.0f, 0.9f, (float) (0.9 * samplesDone / (double) totalSamples));

            // NB buffer gets trashed by this call
            if (samplesToDo <= 0 || ! writeChocBufferToAudioFormatWriter (pc.buffers.audio))
            {
                // complete render
                writer->closeForWriting();

                if (samplesToDo <= 0)
                    progressToUpdate = 1.0f;

                return juce::ThreadPoolJob::jobHasFinished;
            }

            return juce::ThreadPoolJob::jobNeedsRunningAgain;
        }

        bool writeChocBufferToAudioFormatWriter (const choc::buffer::ChannelArrayView<float>& source)
        {
            if (! writer->isOpen())
                return false;

            std::vector<float*> channels;
            channels.resize (source.getNumChannels());

            for (choc::buffer::ChannelCount i = 0; i < source.getNumChannels(); ++i)
                channels[i] = source.getChannel(i).data.data;

            juce::AudioBuffer<float> buffer (channels.data(), (int) source.getNumChannels(), (int) source.getNumFrames());
            return writer->appendBuffer (buffer, buffer.getNumSamples());
        }

        const SampleCount blockSize;
        std::unique_ptr<TracktionNodePlayer> nodePlayer;
        std::unique_ptr<AudioFileWriter> writer;
        double sampleRate = 0;
        TimeRange streamRange;
        int numPreBlocks = 0;
        int64_t samplesDone = 0, totalSamples = 0;
        choc::buffer::ChannelArrayBuffer<float> audioBuffer;
        MidiMessageArray midiBuffer;
    };

    std::unique_ptr<tracktion::graph::Node> node;
    std::unique_ptr<RenderContext> renderContext;
    double prerollTime = 0;

    void createAndPrepareRenderContext()
    {
        auto numChannels = node->getNodeProperties().numberOfChannels;

        renderContext = std::make_unique<RenderContext> (std::move (node), processState,
                                                         destination, source,
                                                         numChannels, blockSize, prerollTime);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioNodeRenderJob)
};

//==============================================================================
struct BlockBasedRenderJob : public ClipEffect::ClipEffectRenderJob
{
    BlockBasedRenderJob (Engine& e, const AudioFile& dest, const AudioFile& src, TimeDuration sourceLength)
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

        sourceLengthSamples = static_cast<SampleCount> (sourceLengthSeconds.inSeconds() * reader->sampleRate);

        writer = std::make_unique<AudioFileWriter> (destination, engine.getAudioFileFormatManager().getWavFormat(),
                                                    sourceInfo.numChannels, sourceInfo.sampleRate,
                                                    std::max (16, sourceInfo.bitsPerSample),
                                                    sourceInfo.metadata, 0);

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

    TimeDuration sourceLengthSeconds;
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
                             TimeDuration sourceLength,
                             WarpTimeManager& wtm, AudioClipBase& c)
        : BlockBasedRenderJob (e, dest, src, sourceLength),
          clip (c), warpTimeManager (wtm)
    {
        CRASH_TRACER
        auto tm = clip.getTimeStretchMode();
        proxyInfo = std::make_unique<AudioClipBase::ProxyRenderingInfo>();
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
                                                                                                TimeDuration sourceLength)
{
    CRASH_TRACER
    auto timeRange = TimeRange (0s, sourceLength);
    jassert (! timeRange.isEmpty());

    auto sourceInfo = sourceFile.getInfo();
    const int blockSize = 128;

    auto job = AudioNodeRenderJob::create (edit.engine, getDestinationFile(), sourceFile, blockSize);

    auto waveNode = job->createWaveNodeForFile (sourceFile, timeRange);

    job->initialise (std::make_unique<PluginNode> (std::move (waveNode), plugin,
                                                   sourceInfo.sampleRate, job->blockSize, nullptr,
                                                   job->processState,
                                                   true, false, -1));
    return job;
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
                                                                                                   TimeDuration sourceLength)
{
    CRASH_TRACER
    AudioFile destFile (getDestinationFile());
    TimeRange timeRange (0s, sourceLength);
    jassert (! timeRange.isEmpty());

    auto job = AudioNodeRenderJob::create (edit.engine, destFile, sourceFile, 128);
    auto n = job->createWaveNodeForFile (sourceFile, timeRange);

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
                n = tracktion::graph::makeNode<FadeInOutNode> (std::move (n),
                                                               job->processState,
                                                               fadeInRange, fadeOutRange,
                                                               fadeInType, fadeOutType,
                                                               true);

            break;

        case EffectType::tapeStartStop:
            if (fadeIn.get() > TimeDuration() || fadeOut.get() > TimeDuration())
                n = tracktion::graph::makeNode<SpeedRampWaveNode> (sourceFile, timeRange, TimeDuration(), TimeRange(), LiveClipLevel(), 1.0,
                                                                   juce::AudioChannelSet::canonicalChannelSet (sourceFile.getInfo().numChannels),
                                                                   juce::AudioChannelSet::stereo(),
                                                                   job->processState,
                                                                   EditItemID(), true,
                                                                   SpeedFadeDescription { fadeInRange, fadeOutRange, fadeInType, fadeOutType });

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

    n = tracktion::graph::makeNode<TimedMutingNode> (std::move (n),
                                                     juce::Array<TimeRange> { TimeRange (0s, effectRange.getStart()),
                                                                              TimeRange (effectRange.getEnd(), sourceLength) },
                                                     job->playHeadState);

    job->initialise (std::move (n));
    return job;
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
                                                                                                    TimeDuration sourceLength)
{
    CRASH_TRACER

    auto destFile = getDestinationFile();
    TimeRange timeRange (0s, sourceLength);
    jassert (! timeRange.isEmpty());

    auto speedRatio = clipEffects.getSpeedRatioEstimate();
    auto effectRange = clipEffects.getEffectsRange();

    auto fade = TimeDuration::fromSeconds (crossfade.get());
    auto halfCrossfade = fade / 2.0;
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

    auto job = AudioNodeRenderJob::create (edit.engine, destFile, sourceFile, AudioNodeRenderJob::defaultBlockSize);

    auto node = job->createWaveNodeForFile (sourceFile, timeRange);

    auto muteTimes = TrackCompManager::TrackComp::getMuteTimes (nonMuteTimes);

    if (! muteTimes.isEmpty())
    {
        node = tracktion::graph::makeNode<TimedMutingNode> (std::move (node), muteTimes, job->playHeadState);

        for (auto r : nonMuteTimes)
        {
            auto fadeIn = r.withLength (fade).withStart (r.getStart() - 0.0001s);
            auto fadeOut = fadeIn.movedToEndAt (r.getEnd() + 0.0001s);

            if (! (fadeIn.isEmpty() && fadeOut.isEmpty()))
                node = tracktion::graph::makeNode<FadeInOutNode> (std::move (node), job->processState,
                                                                  fadeIn, fadeOut,
                                                                  AudioFadeCurve::convex,
                                                                  AudioFadeCurve::convex, false);
        }
    }

    job->initialise (std::move (node));
    return job;
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

juce::ReferenceCountedObjectPtr<ClipEffect::ClipEffectRenderJob> PitchShiftEffect::createRenderJob (const AudioFile& sourceFile, TimeDuration sourceLength)
{
    CRASH_TRACER
    TimeRange timeRange (0s, sourceLength);
    jassert (! timeRange.isEmpty());

    const int blockSize = 512;

    auto job = AudioNodeRenderJob::create (edit.engine, getDestinationFile(), sourceFile, blockSize);

    auto node = job->createWaveNodeForFile (sourceFile, timeRange);

    // Use 1.0 second of preroll to be safe. We can't ask the plugin since it
    // may not be initialized yet
    job->initialise (std::make_unique<PluginNode> (std::move (node), plugin,
                                                   sourceFile.getInfo().sampleRate, job->blockSize, nullptr,
                                                   job->processState,
                                                   true, false, -1),
                     1.0);

    return job;
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
                                                                                                  TimeDuration sourceLength)
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
            auto job = jobs[i];

            if (job == nullptr)
                continue;

            if (job->progress.load() >= 1.0f)
                jobs.remove (i);
            else
                return;
        }

        decrease();
        stopTimer();
    }

    void load()
    {
        callBlockingCatching ([this] { plugin->setProcessingEnabled (true); callback(); });
    }

    void unload()
    {
        callBlockingCatching ([this] { plugin->setProcessingEnabled (false); callback(); });
    }

    int count = 0;
    Plugin::Ptr plugin;
    juce::ReferenceCountedArray<AudioNodeRenderJob> jobs;
    std::function<void()> callback;

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
        // Exception will be caught by Edit constructor
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
                                                                                                TimeDuration sourceLength)
{
    CRASH_TRACER

    const ScopedPluginUnloadInhibitor lock (*pluginUnloadInhibitor);

    TimeRange timeRange (0s, sourceLength);
    jassert (! timeRange.isEmpty());

    const int blockSize = 512;
    auto job = AudioNodeRenderJob::create (edit.engine, getDestinationFile(), sourceFile, blockSize);

    auto n = job->createWaveNodeForFile (sourceFile, timeRange);

    if (plugin != nullptr)
    {
        plugin->setProcessingEnabled (true);

        n = std::make_unique<PluginNode> (std::move (n), plugin,
                                          job->processState.sampleRate, job->blockSize, nullptr,
                                          job->processState,
                                          true, false, -1);
    }

    if (pluginUnloadInhibitor != nullptr)
        pluginUnloadInhibitor->increaseForJob (30 * 1000, job);

    // Use 1.0 second of preroll to be safe. We can't ask the plugin since it
    // may not be initialized yet
    job->initialise (std::move (n), 1.0);

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
    NormaliseRenderJob (Engine& e, const AudioFile& dest, const AudioFile& src, TimeDuration sourceLength, double gain)
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

juce::ReferenceCountedObjectPtr<ClipEffect::ClipEffectRenderJob> NormaliseEffect::createRenderJob (const AudioFile& sourceFile, TimeDuration sourceLength)
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
    MakeMonoRenderJob (Engine& e, const AudioFile& dest, const AudioFile& src, TimeDuration sourceLength, SrcChannels srcCh)
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

        sourceLengthSamples = static_cast<SampleCount> (sourceLengthSeconds.inSeconds() * reader->sampleRate);

        writer = std::make_unique<AudioFileWriter> (destination, engine.getAudioFileFormatManager().getWavFormat(),
                                                    1, sourceInfo.sampleRate,
                                                    std::max (16, sourceInfo.bitsPerSample),
                                                    sourceInfo.metadata, 0);

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

juce::ReferenceCountedObjectPtr<ClipEffect::ClipEffectRenderJob> MakeMonoEffect::createRenderJob (const AudioFile& sourceFile, TimeDuration sourceLength)
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
    ReverseRenderJob (Engine& e, const AudioFile& dest, const AudioFile& src, TimeDuration sourceLength)
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
                                                                                                 TimeDuration sourceLength)
{
    CRASH_TRACER
    return new ReverseRenderJob (edit.engine, getDestinationFile(), sourceFile, sourceLength);
}

//==============================================================================
struct InvertEffect::InvertRenderJob : public BlockBasedRenderJob
{
    InvertRenderJob (Engine& e, const AudioFile& dest, const AudioFile& src, TimeDuration sourceLength)
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
                                                                                                TimeDuration sourceLength)
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
                    callBlockingCatching ([&afm, fileToValidate = currentJob->destination]
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

    auto length = TimeDuration::fromSeconds (sourceFile.getLength());
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
