/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


#if TRACKTION_ENABLE_ARA

//==============================================================================
#if JUCE_MSVC
 #pragma warning (push, 0)
#elif JUCE_CLANG
 #pragma clang diagnostic push
 #pragma clang diagnostic ignored "-Wnon-virtual-dtor"
 #pragma clang diagnostic ignored "-Wreorder"
 #pragma clang diagnostic ignored "-Wunsequenced"
 #pragma clang diagnostic ignored "-Wint-to-pointer-cast"
 #pragma clang diagnostic ignored "-Wunused-parameter"
 #pragma clang diagnostic ignored "-Wconversion"
 #pragma clang diagnostic ignored "-Woverloaded-virtual"
 #pragma clang diagnostic ignored "-Wshadow"
#endif

#undef PRAGMA_ALIGN_SUPPORTED
#undef VST_FORCE_DEPRECATED
#define VST_FORCE_DEPRECATED 0

#ifndef JUCE_MSVC
 #define __cdecl
#endif

// If you get an error here, in order to build with ARA support you'll need
// to include the SDK in your header search paths!
#if JUCE_MAC
 #include "ARASDK/ARAAudioUnit.h"
#endif

#include "ARASDK/ARAVST2.h"
#include "ARASDK/ARAVST3.h"

#include "pluginterfaces/vst2.x/aeffectx.h"
#include "pluginterfaces/vst/ivstcomponent.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"

namespace ARA
{
    DEF_CLASS_IID (IMainFactory)
    DEF_CLASS_IID (IPlugInEntryPoint)
}

#if JUCE_MSVC
 #pragma warning (pop)
#elif JUCE_CLANG
 #pragma clang diagnostic pop
#endif

namespace tracktion_engine
{

using namespace ARA;

struct ARAClipPlayer  : private SelectableListener
{
    #include "tracktion_MelodyneInstanceFactory.h"
    #include "tracktion_ARAWrapperFunctions.h"
    #include "tracktion_ARAWrapperInterfaces.h"

    //==============================================================================
    ARAClipPlayer (Edit& ed, MelodyneFileReader& o, AudioClipBase& c)
      : owner (o),
        clip (c),
        file (c.getAudioFile()),
        edit (ed)
    {
        TRACKTION_ASSERT_MESSAGE_THREAD
        edit.tempoSequence.addSelectableListener (this);
    }

    ~ARAClipPlayer()
    {
        CRASH_TRACER
        TRACKTION_ASSERT_MESSAGE_THREAD

        edit.tempoSequence.removeSelectableListener (this);

        contentAnalyserChecker = nullptr;
        modelUpdater = nullptr;
        contentUpdater = nullptr;

        // Needs to happen before killing off ARA stuff
        if (auto p = getPlugin())
        {
            p->hideWindowForShutdown();

            if (auto pi = p->getAudioPluginInstance())
                pi->releaseResources();
        }

        if (auto doc = getDocument())
        {
            if (doc->dci != nullptr)
            {
                {
                    const ScopedDocumentEditor sde (*this, false);
                    playbackRegionAndSource = nullptr;
                }

                melodyneInstance = nullptr;
            }
        }
    }

    //==============================================================================
    Edit& getEdit()                         { return edit; }
    AudioClipBase& getClip()                { return clip; }
    ExternalPlugin* getPlugin() noexcept    { return melodyneInstance != nullptr ? melodyneInstance->plugin.get() : nullptr; }

    //==============================================================================
    bool initialise (ARAClipPlayer* clipToClone)
    {
        TRACKTION_ASSERT_MESSAGE_THREAD
        CRASH_TRACER

        if (auto doc = getDocument())
        {
            ExternalPlugin::Ptr p = MelodyneInstanceFactory::getInstance().createPlugin (edit);

            if (p == nullptr || getDocument() == nullptr)
                return false;

            melodyneInstance.reset (MelodyneInstanceFactory::getInstance().createInstance (*p, doc->dcRef));

            if (melodyneInstance == nullptr)
                return false;

            updateContent (clipToClone);

            return playbackRegionAndSource != nullptr
                     && playbackRegionAndSource->playbackRegion != nullptr;
        }

        return false;
    }

    void contentHasChanged()
    {
        CRASH_TRACER
        updateContent (nullptr);
        owner.sendChangeMessage();
    }

    void selectableObjectChanged (Selectable*) override
    {
        if (auto doc = getDocument())
        {
            if (doc->musicalContext != nullptr)
            {
                doc->beginEditing (true);
                doc->musicalContext->update();
                doc->endEditing (true);
            }
        }
    }

    void selectableObjectAboutToBeDeleted (Selectable*) override {}

    //==============================================================================
    void updateContent (ARAClipPlayer* clipToClone)
    {
        CRASH_TRACER
        TRACKTION_ASSERT_MESSAGE_THREAD

        if (MessageManager::getInstance()->isThisTheMessageThread()
            && getEdit().getTransport().isAllowedToReallocate())
        {
            contentUpdater = nullptr;
            internalUpdateContent (clipToClone);
        }
        else
        {
            if (contentUpdater == nullptr)
            {
                contentUpdater = std::make_unique<ContentUpdater> (*this);
            }
            else
            {
                if (! contentUpdater->isTimerRunning()) //To avoid resetting it
                    contentUpdater->startTimer (100);
            }
        }
    }

    //==============================================================================
    MidiMessageSequence getAnalysedMIDISequence()
    {
        CRASH_TRACER

        const int midiChannel = 1;
        MidiMessageSequence result;

        if (auto doc = getDocument())
        {
            const ARADocumentControllerInterface* dci = doc->dci;
            ARADocumentControllerRef dcRef = doc->dcRef;
            ARAAudioSourceRef audioSourceRef = playbackRegionAndSource->audioSource->audioSourceRef;

            if (dci->isAudioSourceContentAvailable (dcRef, audioSourceRef, kARAContentTypeNotes))
            {
                ARAContentReaderRef contentReaderRef = dci->createAudioSourceContentReader (dcRef, audioSourceRef, kARAContentTypeNotes, nullptr);
                int numEvents = (int) dci->getContentReaderEventCount (dcRef, contentReaderRef);

                for (int i = 0; i < numEvents; ++i)
                {
                    if (auto note = static_cast<const ARAContentNote*> (dci->getContentReaderDataForEvent (dcRef, contentReaderRef, i)))
                    {
                        if (note->pitchNumber != kARAInvalidPitchNumber)
                        {
                            result.addEvent (MidiMessage::noteOn  (midiChannel, note->pitchNumber, static_cast<float> (note->volume)),
                                             note->startPosition);

                            result.addEvent (MidiMessage::noteOff (midiChannel, note->pitchNumber),
                                             note->startPosition + note->noteDuration);
                        }
                    }
                }

                dci->destroyContentReader (dcRef, contentReaderRef);
            }

            result.updateMatchedPairs();
        }

        return result;
    }

    //==============================================================================
    void startProcessing()  { TRACKTION_ASSERT_MESSAGE_THREAD if (playbackRegionAndSource != nullptr) playbackRegionAndSource->enable(); }
    void stopProcessing()   { TRACKTION_ASSERT_MESSAGE_THREAD if (playbackRegionAndSource != nullptr) playbackRegionAndSource->disable(); }

    class ContentAnalyser  : public Timer
    {
    public:
        ContentAnalyser (const ARAClipPlayer& p)  : pimpl (p)
        {
            startTimer (100);
        }

        bool isAnalysing() const noexcept       { return analysingContent; }

        void timerCallback() override
        {
            CRASH_TRACER

            ARADocument* doc = pimpl.getDocument();

            if (doc == nullptr)
            {
                analysingContent = false;
                return;
            }

            const ARADocumentControllerInterface* dci = doc->dci;
            ARADocumentControllerRef dcRef = doc->dcRef;
            ARAAudioSourceRef audioSourceRef = nullptr;

            if (pimpl.playbackRegionAndSource != nullptr)
                if (pimpl.playbackRegionAndSource->audioSource != nullptr)
                    audioSourceRef = pimpl.playbackRegionAndSource->audioSource->audioSourceRef;

            if (dci != nullptr && dcRef != nullptr && audioSourceRef != nullptr)
            {
                if (firstCall)
                {
                    firstCall = false;

                    ARAContentType types[] = { kARAContentTypeSignatures, kARAContentTypeTempoEntries };
                    dci->requestAudioSourceContentAnalysis (dcRef, audioSourceRef, (ARASize) numElementsInArray (types), types);
                }

                analysingContent = dci->isAudioSourceContentAnalysisIncomplete (dcRef, audioSourceRef, kARAContentTypeSignatures) != kARAFalse
                                || dci->isAudioSourceContentAnalysisIncomplete (dcRef, audioSourceRef, kARAContentTypeTempoEntries) != kARAFalse;
            }
            else
            {
                analysingContent = false;
            }
        }

    private:
        const ARAClipPlayer& pimpl;
        volatile bool analysingContent = false;
        bool firstCall = false;

        ContentAnalyser() = delete;
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ContentAnalyser)
    };

    friend class ContentAnalyser;

    std::unique_ptr<ContentAnalyser> contentAnalyserChecker;

    bool isAnalysingContent() const
    {
        jassert (contentAnalyserChecker->isTimerRunning());
        return contentAnalyserChecker->isAnalysing();
    }

    ARADocument* getDocument() const;

private:
    //==============================================================================
    MelodyneFileReader& owner;
    AudioClipBase& clip;
    const AudioFile file;
    Edit& edit;

    std::unique_ptr<MelodyneInstance> melodyneInstance;
    std::unique_ptr<PlaybackRegionAndSource> playbackRegionAndSource;
    int64 currentHashCode = 0;

    //==============================================================================
    struct ScopedDocumentEditor
    {
        ScopedDocumentEditor (ARAClipPlayer& o, bool restartModelUpdaterLater)
            : owner (o), restartTimerLater (restartModelUpdaterLater)
        {
            if (restartTimerLater)
                owner.modelUpdater = nullptr;

            owner.getDocument()->beginEditing (false);
        }

        ~ScopedDocumentEditor()
        {
            if (auto doc = owner.getDocument())
            {
                doc->endEditing (false);

                if (restartTimerLater)
                    owner.modelUpdater = std::make_unique<ModelUpdater> (*doc);
            }
        }

    private:
        ARAClipPlayer& owner;
        const bool restartTimerLater;

        JUCE_DECLARE_NON_COPYABLE (ScopedDocumentEditor)
    };

    //==============================================================================
    /** NB: Must delete the old objects *after* creating the new ones, because Melodyne crashes
            if you deselect a play region and then try to select a different one.
            But doing it in the opposite order seems to work ok.
    */
    void recreateTrack (ARAClipPlayer* clipToClone)
    {
        CRASH_TRACER
        TRACKTION_ASSERT_MESSAGE_THREAD

        jassert (melodyneInstance != nullptr);
        jassert (melodyneInstance->factory != nullptr);
        jassert (melodyneInstance->extensionInstance != nullptr);

        auto oldTrack = std::move (playbackRegionAndSource);

        playbackRegionAndSource = std::make_unique<PlaybackRegionAndSource> (*getDocument(), clip, *melodyneInstance->factory,
                                                                             *melodyneInstance->extensionInstance,
                                                                             String::toHexString (currentHashCode),
                                                                             clipToClone != nullptr ? clipToClone->playbackRegionAndSource.get() : nullptr);

        if (oldTrack != nullptr)
        {
            const ScopedDocumentEditor sde (*this, false);
            oldTrack = nullptr;
        }
    }

    void internalUpdateContent (ARAClipPlayer* clipToClone)
    {
        CRASH_TRACER
        TRACKTION_ASSERT_MESSAGE_THREAD

        if (auto doc = getDocument())
        {
            jassert (doc->dci != nullptr);

            contentAnalyserChecker = nullptr;
            modelUpdater = nullptr; // Can't be editing the document in any way while restoring

            juce::int64 newHashCode = file.getHash()
                                       ^ file.getFile().getLastModificationTime().toMilliseconds()
                                       ^ (juce::int64) clip.itemID.getRawID();

            if (currentHashCode != newHashCode)
            {
                currentHashCode = newHashCode;
                const ScopedDocumentEditor sde (*this, true);

                recreateTrack (clipToClone);
            }
            else
            {
                if (playbackRegionAndSource != nullptr
                     && playbackRegionAndSource->playbackRegion != nullptr)
                {
                    const ScopedDocumentEditor sde (*this, true);
                    playbackRegionAndSource->playbackRegion->updateRange();
                }
            }

            modelUpdater = std::make_unique<ModelUpdater> (*doc);

            if (contentAnalyserChecker == nullptr)
                contentAnalyserChecker = std::make_unique<ContentAnalyser> (*this);
        }
    }

    //==============================================================================
    struct ContentUpdater : public Timer
    {
        ContentUpdater (ARAClipPlayer& p) noexcept : owner (p) { startTimer (100); }

        ARAClipPlayer& owner;

        void timerCallback() override
        {
            CRASH_TRACER

            if (owner.getEdit().getTransport().isAllowedToReallocate())
            {
                owner.internalUpdateContent (nullptr);
                stopTimer();
            }
        }

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ContentUpdater)
    };

    std::unique_ptr<ContentUpdater> contentUpdater;

    //==============================================================================
    struct ModelUpdater : private Timer
    {
        ModelUpdater (ARADocument& d) noexcept : document (d) { startTimer (3000); }

        ARADocument& document;

        void timerCallback() override
        {
            CRASH_TRACER
            if (document.dci != nullptr && document.dcRef != nullptr)
                document.dci->notifyModelUpdates (document.dcRef);
        }

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ModelUpdater)
    };

    std::unique_ptr<ModelUpdater> modelUpdater;

    //==============================================================================
    ARAClipPlayer() = delete;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ARAClipPlayer)
};

//==============================================================================
MelodyneFileReader::MelodyneFileReader (Edit& ed, AudioClipBase& clip)
{
    TRACKTION_ASSERT_MESSAGE_THREAD
    CRASH_TRACER

    player = new ARAClipPlayer (ed, *this, clip);

    if (! player->initialise (nullptr))
        player = nullptr;
}

MelodyneFileReader::MelodyneFileReader (Edit& ed, AudioClipBase& clip, MelodyneFileReader& other)
{
    TRACKTION_ASSERT_MESSAGE_THREAD
    CRASH_TRACER

    if (other.player != nullptr)
    {
        player = new ARAClipPlayer (ed, *this, clip);

        if (! player->initialise (other.player))
            player = nullptr;
    }

    jassert (player != nullptr);
}

MelodyneFileReader::~MelodyneFileReader()
{
    TRACKTION_ASSERT_MESSAGE_THREAD
    CRASH_TRACER

    if (player != nullptr)
        if (auto plugin = player->getPlugin())
            if (auto pi = plugin->getAudioPluginInstance())
                pi->setPlayHead (nullptr);

    std::unique_ptr<ARAClipPlayer> toDestroy (player);
}

//==============================================================================
void MelodyneFileReader::showPluginWindow()
{
    if (auto p = getPlugin())
        p->showWindowExplicitly();
}

ExternalPlugin* MelodyneFileReader::getPlugin()
{
    if (isValid())
        return player->getPlugin();

    return {};
}

//==============================================================================
bool MelodyneFileReader::isAnalysingContent()
{
    return player != nullptr && player->isAnalysingContent();
}

void MelodyneFileReader::sourceClipChanged()
{
    if (player != nullptr)
        player->updateContent (nullptr);
}

//==============================================================================
MidiMessageSequence MelodyneFileReader::getAnalysedMIDISequence()
{
    if (player != nullptr)
        return player->getAnalysedMIDISequence();

    return {};
}

//==============================================================================
class MelodyneFileReader::MelodyneAudioNode : public AudioNode
{
public:
    MelodyneAudioNode (AudioClipBase& c, LiveClipLevel level)
      : clip (c), clipLevel (level), clipPtr (&c),
        melodyneProxy (c.melodyneProxy),
        fileInfo (clip.getAudioFile().getInfo())
    {
    }

    ~MelodyneAudioNode()
    {
        CRASH_TRACER

        if (auto ep = melodyneProxy->getPlugin())
            if (auto p = ep->getAudioPluginInstance())
                p->setPlayHead (nullptr);

        playhead = nullptr;
        melodyneProxy = nullptr;
    }

    void getAudioNodeProperties (AudioNodeProperties& info) override
    {
        info.hasAudio           = true;
        info.numberOfChannels   = fileInfo.numChannels;
    }

    void prepareAudioNodeToPlay (const PlaybackInitialisationInfo& info) override
    {
        CRASH_TRACER

        if (auto plugin = melodyneProxy->getPlugin())
        {
            if (auto p = plugin->getAudioPluginInstance())
            {
                //NB: Reducing the number of calls to juce::AudioProcessor::prepareToPlay() keeps Celemony happy;
                //    the VST3 version of their plugin relies heavily on calls to the inconspicuous IComponent::setActive()!
                if (p->getSampleRate() != info.sampleRate
                     || p->getBlockSize() != info.blockSizeSamples)
                {
                    plugin->initialise (info);
                }

                p->setPlayHead (nullptr);
                playhead = std::make_unique<MelodynePlayhead> (*plugin, info.playhead);
                p->setPlayHead (playhead.get());

                desc = p->getPluginDescription();
            }
        }
    }

    void releaseAudioNodeResources() override
    {
        // Don't release the plugin here to avoid duplicate calls to VST3's IComponent::setActive (false) which causes problems
    }

    void visitNodes (const VisitorFn& v) override               { v (*this); }
    bool purgeSubNodes (bool keepAudio, bool) override          { return keepAudio; }
    bool isReadyToRender() override                             { return ! melodyneProxy->isAnalysingContent(); }
    void renderAdding (const AudioRenderContext& rc) override   { callRenderOver (rc); }
    void renderOver (const AudioRenderContext& rc) override     { invokeSplitRender (rc, *this); }

    void renderSection (const AudioRenderContext& rc, EditTimeRange editTimeRange)
    {
        CRASH_TRACER

        if (rc.destBuffer == nullptr || melodyneProxy == nullptr || rc.bufferNumSamples == 0)
            return;

        if (auto plugin = melodyneProxy->getPlugin())
        {
            if (auto pluginInstance = plugin->getAudioPluginInstance())
            {
                if (pluginInstance->getPlayHead() != playhead.get())
                    pluginInstance->setPlayHead (playhead.get()); //This is needed in case the ExternalPlugin back-end has swapped the playhead out

                midiMessages.clear();

                if (auto* arr = rc.bufferForMidiMessages)
                {
                    auto sampleRate = pluginInstance->getSampleRate();

                    for (auto& m : *arr)
                        midiMessages.addEvent (m, (int) jmin ((int64) 0x7fffffff, (int64) (m.getTimeStamp() *  sampleRate)));
                }

                playhead->setCurrentInfo (editTimeRange.getStart(), rc.playhead.isPlaying());

                auto numChans = jmax (1, rc.destBuffer->getNumChannels());
                auto maxChans = jmax (1, desc.numInputChannels, desc.numOutputChannels);

                if (numChans == 1 && desc.numInputChannels == 2)
                {
                    // if we're getting a mono in and need stereo, dupe the channel..
                    rc.destBuffer->setSize (maxChans, rc.destBuffer->getNumSamples(), true);
                    rc.destBuffer->copyFrom (1, rc.bufferStartSample, *rc.destBuffer, 0, rc.bufferStartSample, rc.bufferNumSamples);
                }
                else if (numChans == 2 && desc.numInputChannels == 1)
                {
                    // if we're getting a stereo in and need mono, average the input..
                    rc.destBuffer->addFrom (0, rc.bufferStartSample, *rc.destBuffer, 1, rc.bufferStartSample, rc.bufferNumSamples);
                    rc.destBuffer->applyGain (0, rc.bufferStartSample, rc.bufferNumSamples, 0.5f);
                    rc.destBuffer->setSize (maxChans, rc.destBuffer->getNumSamples(), true);
                }
                else
                {
                    rc.destBuffer->setSize (maxChans, rc.destBuffer->getNumSamples(), true);

                    if (maxChans > numChans)
                        for (int i = numChans; i < maxChans; ++i)
                            rc.destBuffer->clear (i, rc.bufferStartSample, rc.bufferNumSamples);
                }

                juce::AudioBuffer<float> asb (rc.destBuffer->getArrayOfWritePointers(), maxChans,
                                              rc.bufferStartSample, rc.bufferNumSamples);

                pluginInstance->processBlock (asb, midiMessages);

                float gains[2];

                if (asb.getNumChannels() > 1)
                    clipLevel.getLeftAndRightGains (gains[0], gains[1]);
                else
                    gains[0] = gains[1] = clipLevel.getGainIncludingMute();

                if (rc.playhead.isUserDragging())
                {
                    gains[0] *= 0.4f;
                    gains[1] *= 0.4f;
                }

                for (int i = 0; i < asb.getNumChannels(); ++i)
                {
                    const float gain = gains[i & 1];

                    if (gain != 1.0f)
                        asb.applyGain (i, 0, asb.getNumSamples(), gain);
                }
            }
        }
    }

private:
    AudioClipBase& clip;
    LiveClipLevel clipLevel;
    Clip::Ptr clipPtr;
    MelodyneFileReader::Ptr melodyneProxy;
    const AudioFileInfo fileInfo;
    MidiBuffer midiMessages;
    PluginDescription desc;

    /** This class is a necessary bodge due to ARA needing to be told that we're playing, even if we aren't,
        so it can generate audio while its graph is being modified!
    */
    class MelodynePlayhead  : public juce::AudioPlayHead
    {
    public:
        MelodynePlayhead (ExternalPlugin& p, PlayHead& ph)  : plugin (p), playhead (ph)
        {
            currentPos = std::make_unique<TempoSequencePosition> (plugin.edit.tempoSequence);
            loopStart  = std::make_unique<TempoSequencePosition> (plugin.edit.tempoSequence);
            loopEnd    = std::make_unique<TempoSequencePosition> (plugin.edit.tempoSequence);
        }

        /** Must be called before processing audio/MIDI */
        void setCurrentInfo (double currentTimeSeconds, bool playing)
        {
            time = currentTimeSeconds;
            isPlaying = playing;
        }

        bool getCurrentPosition (CurrentPositionInfo& result) override
        {
            zerostruct (result);
            result.frameRate = getFrameRate();

            auto& transport = plugin.edit.getTransport();

            result.isPlaying        = isPlaying;
            result.isLooping        = playhead.isLooping();
            result.isRecording      = transport.isRecording();
            result.editOriginTime   = transport.getTimeWhenStarted();

            if (result.isLooping)
            {
                loopStart->setTime (playhead.getLoopTimes().start);
                result.ppqLoopStart     = loopStart->getPPQTime();

                loopEnd->setTime (playhead.getLoopTimes().end);
                result.ppqLoopEnd       = loopEnd->getPPQTime();
            }

            result.timeInSeconds    = time;
            result.timeInSamples    = (int64) (time * plugin.getAudioPluginInstance()->getSampleRate());

            currentPos->setTime (time);
            const auto& tempo = currentPos->getCurrentTempo();
            result.bpm                  = tempo.bpm;
            result.timeSigNumerator     = tempo.numerator;
            result.timeSigDenominator   = tempo.denominator;

            result.ppqPositionOfLastBarStart = currentPos->getPPQTimeOfBarStart();
            result.ppqPosition = jmax (result.ppqPositionOfLastBarStart, currentPos->getPPQTime());

            return true;
        }

    private:
        ExternalPlugin& plugin;
        PlayHead& playhead;
        std::unique_ptr<TempoSequencePosition> currentPos, loopStart, loopEnd;
        double time = 0;
        bool isPlaying = false;

        AudioPlayHead::FrameRateType getFrameRate() const
        {
            switch (plugin.edit.getTimecodeFormat().getFPS())
            {
                case 24:    return AudioPlayHead::fps24;
                case 25:    return AudioPlayHead::fps25;
                case 29:    return AudioPlayHead::fps30drop;
                case 30:    return AudioPlayHead::fps30;
                default:    break;
            }

            return AudioPlayHead::fps30; //Just to cope with it.
        }

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MelodynePlayhead)
    };

    std::unique_ptr<MelodynePlayhead> playhead;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MelodyneAudioNode)
};

AudioNode* MelodyneFileReader::createAudioNode (LiveClipLevel level)
{
    CRASH_TRACER
    if (player != nullptr)
        return new MelodyneAudioNode (player->getClip(), level);

    return {};
}

void MelodyneFileReader::cleanUpOnShutdown()
{
    ARAClipPlayer::MelodyneInstanceFactory::shutdown();
}

//==============================================================================
struct ARADocumentHolder::Pimpl
{
    Pimpl (Edit& e)  : edit (e) {}

    void initialise()
    {
        TRACKTION_ASSERT_MESSAGE_THREAD
        araDocument.reset (ARAClipPlayer::createDocument (edit));

        if (araDocument != nullptr)
        {
            araDocument->beginRestoringState (edit.araDocument->lastState);

            visitAllTrackItems (edit, [this] (TrackItem& i)
            {
                if (auto c = dynamic_cast<AudioClipBase*> (&i))
                    c->loadMelodyneState (edit);

                return true;
            });

            araDocument->endRestoringState();
        }
    }

    Edit& edit;
    std::unique_ptr<ARAClipPlayer::ARADocument> araDocument;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Pimpl)
};

ARADocumentHolder::ARADocumentHolder (Edit& e, const ValueTree& v)
    : edit (e), lastState (v)
{
}

ARADocumentHolder::~ARADocumentHolder()
{
    TRACKTION_ASSERT_MESSAGE_THREAD
    CRASH_TRACER
    pimpl = nullptr;
}

ARADocumentHolder::Pimpl* ARADocumentHolder::getPimpl()
{
    if (pimpl == nullptr)
    {
        CRASH_TRACER
        pimpl = new Pimpl (edit);
        callBlocking ([this]() { pimpl->initialise(); });
    }

    return pimpl;
}

void ARADocumentHolder::flushStateToValueTree()
{
    TRACKTION_ASSERT_MESSAGE_THREAD

    if (pimpl != nullptr)
        if (pimpl->araDocument != nullptr)
            pimpl->araDocument->flushStateToValueTree (lastState);
}

ARAClipPlayer::ARADocument* ARAClipPlayer::getDocument() const
{
    if (auto l = edit.araDocument.get())
        if (auto p = l->getPimpl())
            return p->araDocument.get();

    return {};
}

} // namespace tracktion_engine

#else

//==============================================================================
namespace tracktion_engine
{

struct ARADocumentHolder::Pimpl {};
struct ARAClipPlayer {};

MelodyneFileReader::MelodyneFileReader (Edit&, AudioClipBase&) {}
MelodyneFileReader::MelodyneFileReader (Edit&, AudioClipBase&, MelodyneFileReader&) {}
MelodyneFileReader::~MelodyneFileReader() {}

void MelodyneFileReader::cleanUpOnShutdown()                        {}
ExternalPlugin* MelodyneFileReader::getPlugin()                     { return {}; }
void MelodyneFileReader::showPluginWindow()                         {}
bool MelodyneFileReader::isAnalysingContent()                       { return false; }
MidiMessageSequence MelodyneFileReader::getAnalysedMIDISequence()   { return {}; }
AudioNode* MelodyneFileReader::createAudioNode (LiveClipLevel)      { return {}; }
void MelodyneFileReader::sourceClipChanged()                        {}

ARADocumentHolder::ARADocumentHolder (Edit& e, const ValueTree&) : edit (e) { juce::ignoreUnused (edit); }
ARADocumentHolder::~ARADocumentHolder() {}
ARADocumentHolder::Pimpl* ARADocumentHolder::getPimpl()             { return {}; }
void ARADocumentHolder::flushStateToValueTree() {}

} // namespace tracktion_engine

#endif
