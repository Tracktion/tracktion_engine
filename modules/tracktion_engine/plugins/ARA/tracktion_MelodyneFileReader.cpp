/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
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
 #pragma clang diagnostic ignored "-Wgnu-zero-variadic-macro-arguments"
 #if __clang_major__ >= 10
  #pragma clang diagnostic ignored "-Wpragma-pack"
 #endif
#endif

#undef PRAGMA_ALIGN_SUPPORTED
#undef VST_FORCE_DEPRECATED
#define VST_FORCE_DEPRECATED 0

#ifndef JUCE_MSVC
 #define __cdecl
#endif

// If you get an error here, in order to build with ARA support you'll need
// to include the SDK in your header search paths!
#include "ARA_API/ARAVST3.h"
#include "ARA_Library/Dispatch/ARAHostDispatch.h"

#include "pluginterfaces/vst/ivstcomponent.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"

namespace ARA
{
    DEF_CLASS_IID (IMainFactory)
    DEF_CLASS_IID (IPlugInEntryPoint)
    DEF_CLASS_IID (IPlugInEntryPoint2)
}

#if JUCE_MSVC
 #pragma warning (pop)
#elif JUCE_CLANG
 #pragma clang diagnostic pop
#endif

namespace tracktion { inline namespace engine
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
        jassert (file.getFile().existsAsFile());
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
    ExternalPlugin* getPlugin()             { return melodyneInstance != nullptr ? melodyneInstance->plugin.get() : nullptr; }
    const ARAFactory* getARAFactory() const { return melodyneInstance != nullptr ? melodyneInstance->factory : nullptr; }

    //==============================================================================
    bool initialise (ARAClipPlayer* clipToClone)
    {
        TRACKTION_ASSERT_MESSAGE_THREAD
        CRASH_TRACER

        if (auto doc = getDocument())
        {
            ExternalPlugin::Ptr p = MelodyneInstanceFactory::getInstance (edit.engine).createPlugin (edit);

            if (p == nullptr || getDocument() == nullptr)
                return false;

            melodyneInstance.reset (MelodyneInstanceFactory::getInstance (edit.engine).createInstance (*p, doc->dcRef));

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

        if (juce::MessageManager::getInstance()->isThisTheMessageThread()
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
    juce::MidiMessageSequence getAnalysedMIDISequence()
    {
        CRASH_TRACER

        const int midiChannel = 1;
        juce::MidiMessageSequence result;

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
                            result.addEvent (juce::MidiMessage::noteOn  (midiChannel, note->pitchNumber, static_cast<float> (note->volume)),
                                             note->startPosition);

                            result.addEvent (juce::MidiMessage::noteOff (midiChannel, note->pitchNumber),
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
    void setViewSelection()
    {
        if (playbackRegionAndSource != nullptr)
            playbackRegionAndSource->setViewSelection();
    }

    //==============================================================================
    void startProcessing()  { TRACKTION_ASSERT_MESSAGE_THREAD if (playbackRegionAndSource != nullptr) playbackRegionAndSource->enable(); }
    void stopProcessing()   { TRACKTION_ASSERT_MESSAGE_THREAD if (playbackRegionAndSource != nullptr) playbackRegionAndSource->disable(); }

    class ContentAnalyser
    {
    public:
        ContentAnalyser (const ARAClipPlayer& p)  : pimpl (p)
        {
        }

        bool isAnalysing()
        {
            callBlocking ([this] { updateAnalysingContent(); });

            return analysingContent;
        }

        void updateAnalysingContent()
        {
            CRASH_TRACER

            auto doc = pimpl.getDocument();

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
                    auto araFactory = pimpl.getARAFactory();
                    for (ARAContentType contentType : { kARAContentTypeBarSignatures, kARAContentTypeTempoEntries })
                    {
                        for (int i = 0; i < (int) araFactory->analyzeableContentTypesCount; i++)
                        {
                            if (araFactory->analyzeableContentTypes[i] == contentType)
                            {
                                typesBeingAnalyzed.push_back (contentType);
                                break;
                            }
                        }
                    }

                    if (!typesBeingAnalyzed.empty())
                        dci->requestAudioSourceContentAnalysis (dcRef, audioSourceRef, (ARASize)typesBeingAnalyzed.size(), typesBeingAnalyzed.data());
                    
                    firstCall = false;
                }

                analysingContent = false;
                for (ARAContentType contentType : typesBeingAnalyzed)
                {
                    analysingContent = (dci->isAudioSourceContentAnalysisIncomplete (dcRef, audioSourceRef, contentType) != kARAFalse);
                    if (analysingContent)
                        break;
                }
            }
            else
            {
                analysingContent = false;
            }
        }

    private:
        const ARAClipPlayer& pimpl;
        std::vector<ARAContentType> typesBeingAnalyzed;
        volatile bool analysingContent = false;
        bool firstCall = true;

        ContentAnalyser() = delete;
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ContentAnalyser)
    };

    friend class ContentAnalyser;

    std::unique_ptr<ContentAnalyser> contentAnalyserChecker;

    bool isAnalysingContent() const
    {
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
    HashCode currentHashCode = 0;

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
                                                                             juce::String::toHexString (currentHashCode),
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

            HashCode newHashCode = file.getHash()
                                    ^ file.getFile().getLastModificationTime().toMilliseconds()
                                    ^ static_cast<HashCode> (clip.itemID.getRawID());

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
    struct ContentUpdater  : public juce::Timer
    {
        ContentUpdater (ARAClipPlayer& p) : owner (p) { startTimer (100); }

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
    struct ModelUpdater  : private juce::Timer
    {
        ModelUpdater (ARADocument& d) : document (d) { startTimer (3000); }

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

    player.reset (new ARAClipPlayer (ed, *this, clip));

    if (! player->initialise (nullptr))
        player = nullptr;
}

MelodyneFileReader::MelodyneFileReader (Edit& ed, AudioClipBase& clip, MelodyneFileReader& other)
{
    TRACKTION_ASSERT_MESSAGE_THREAD
    CRASH_TRACER

    if (other.player != nullptr)
    {
        player.reset (new ARAClipPlayer (ed, *this, clip));

        if (! player->initialise (other.player.get()))
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

    auto toDestroy = std::move (player);
}

//==============================================================================
void MelodyneFileReader::showPluginWindow()
{
    if (player != nullptr)
        player->setViewSelection();

    if (auto p = getPlugin())
        p->showWindowExplicitly();
}

void MelodyneFileReader::hidePluginWindow()
{
    if (auto p = getPlugin())
        p->hideWindowForShutdown();
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
juce::MidiMessageSequence MelodyneFileReader::getAnalysedMIDISequence()
{
    if (player != nullptr)
        return player->getAnalysedMIDISequence();

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

            visitAllTrackItems (edit, [] (TrackItem& i)
            {
                if (auto c = dynamic_cast<AudioClipBase*> (&i))
                    c->loadMelodyneState();

                return true;
            });

            araDocument->endRestoringState();
        }
    }

    Edit& edit;
    std::unique_ptr<ARAClipPlayer::ARADocument> araDocument;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Pimpl)
};

ARADocumentHolder::ARADocumentHolder (Edit& e, const juce::ValueTree& v)
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
        pimpl.reset (new Pimpl (edit));
        callBlocking ([this]() { pimpl->initialise(); });
    }

    return pimpl.get();
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

}} // namespace tracktion { inline namespace engine

#else

//==============================================================================
namespace tracktion { inline namespace engine
{

struct ARADocumentHolder::Pimpl {};
struct ARAClipPlayer {};

MelodyneFileReader::MelodyneFileReader (Edit&, AudioClipBase&) {}
MelodyneFileReader::MelodyneFileReader (Edit&, AudioClipBase&, MelodyneFileReader&) {}
MelodyneFileReader::~MelodyneFileReader() {}

void MelodyneFileReader::cleanUpOnShutdown()                        {}
ExternalPlugin* MelodyneFileReader::getPlugin()                     { return {}; }
void MelodyneFileReader::showPluginWindow()                         {}
void MelodyneFileReader::hidePluginWindow()                         {}
bool MelodyneFileReader::isAnalysingContent()                       { return false; }
juce::MidiMessageSequence MelodyneFileReader::getAnalysedMIDISequence()   { return {}; }
void MelodyneFileReader::sourceClipChanged()                        {}

ARADocumentHolder::ARADocumentHolder (Edit& e, const juce::ValueTree&) : edit (e) { juce::ignoreUnused (edit); }
ARADocumentHolder::~ARADocumentHolder() {}
ARADocumentHolder::Pimpl* ARADocumentHolder::getPimpl()             { return {}; }
void ARADocumentHolder::flushStateToValueTree() {}

}} // namespace tracktion { inline namespace engine

#endif
