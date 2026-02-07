/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
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

#if ! JUCE_PLUGINHOST_ARA
namespace ARA
{
    DEF_CLASS_IID (IMainFactory)
    DEF_CLASS_IID (IPlugInEntryPoint)
    DEF_CLASS_IID (IPlugInEntryPoint2)
}
#endif

#if JUCE_MSVC
 #pragma warning (pop)
#elif JUCE_CLANG
 #pragma clang diagnostic pop
#endif

namespace tracktion { inline namespace engine
{

using namespace ARA;

struct ARAClipPlayer  : private Selectable::Listener
{
    #include "tracktion_ARAPluginFactory.h"
    #include "tracktion_ARAWrapperFunctions.h"
    #include "tracktion_ARAWrapperInterfaces.h"

    //==============================================================================
    ARAClipPlayer (Edit& ed, ARAFileReader& o, AudioClipBase& c)
      : Selectable::Listener (ed.tempoSequence), owner (o),
        clip (c),
        file (c.getAudioFile()),
        edit (ed)
    {
        TRACKTION_ASSERT_MESSAGE_THREAD
        jassert (file.getFile().existsAsFile());
    }

    ~ARAClipPlayer()
    {
        CRASH_TRACER
        TRACKTION_ASSERT_MESSAGE_THREAD

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

                araInstance = nullptr;
            }
        }
    }

    //==============================================================================
    Edit& getEdit()                         { return edit; }
    AudioClipBase& getClip()                { return clip; }
    ExternalPlugin* getPlugin()             { return araInstance != nullptr ? araInstance->plugin.get() : nullptr; }
    const ARAFactory* getARAFactory() const { return araInstance != nullptr ? araInstance->factory : nullptr; }

    //==============================================================================
    bool initialise (ARAClipPlayer* clipToClone)
    {
        TRACKTION_ASSERT_MESSAGE_THREAD
        CRASH_TRACER

        if (auto doc = getDocument())
        {
            auto desc = clip.araPluginDescription.get();
            ExternalPlugin::Ptr p;

            if (desc.name.isNotEmpty())
            {
                auto& pluginFactory = ARAPluginFactory::getInstance (edit.engine, desc);
                p = pluginFactory.createPlugin (edit, desc);

                if (p == nullptr || getDocument() == nullptr)
                    return false;

                araInstance.reset (pluginFactory.createInstance (*p, doc->dcRef));
            }
            else
            {
                auto* pluginFactory = ARAPluginFactory::getDefaultInstance (edit.engine);

                if (pluginFactory == nullptr)
                    return false;

                p = pluginFactory->createPlugin (edit);

                if (p == nullptr || getDocument() == nullptr)
                    return false;

                // Save the resolved description so the UI shows the correct plugin
                // and future sessions don't need to re-resolve
                clip.araPluginDescription.setValue (p->desc, nullptr);

                araInstance.reset (pluginFactory->createInstance (*p, doc->dcRef));
            }

            if (araInstance == nullptr)
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
                const ARADocument::ScopedEdit scope (*doc, true);
                doc->musicalContext->update();
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

            // Try reading from audio modification first (contains user edits e.g. Melodyne),
            // falling back to audio source (original analysis) if not available.
            ARAContentReaderRef contentReaderRef = nullptr;

            if (playbackRegionAndSource->audioModification != nullptr)
            {
                ARAAudioModificationRef audioModRef = playbackRegionAndSource->audioModification->audioModificationRef;

                if (audioModRef != nullptr
                    && dci->isAudioModificationContentAvailable (dcRef, audioModRef, kARAContentTypeNotes))
                {
                    contentReaderRef = dci->createAudioModificationContentReader (dcRef, audioModRef, kARAContentTypeNotes, nullptr);
                }
            }

            if (contentReaderRef == nullptr)
            {
                ARAAudioSourceRef audioSourceRef = playbackRegionAndSource->audioSource->audioSourceRef;

                if (dci->isAudioSourceContentAvailable (dcRef, audioSourceRef, kARAContentTypeNotes))
                    contentReaderRef = dci->createAudioSourceContentReader (dcRef, audioSourceRef, kARAContentTypeNotes, nullptr);
            }

            if (contentReaderRef != nullptr)
            {
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
    juce::MemoryBlock storeARAArchiveForCopy()
    {
        if (auto doc = getDocument())
        {
            if (playbackRegionAndSource != nullptr
                && playbackRegionAndSource->audioSource != nullptr
                && playbackRegionAndSource->audioModification != nullptr)
            {
                return doc->storeObjectsForCopy (playbackRegionAndSource->audioSource->audioSourceRef,
                                                 playbackRegionAndSource->audioModification->audioModificationRef);
            }
        }

        return {};
    }

    void restoreARAArchiveForPaste (const juce::MemoryBlock& data,
                                    const juce::String& archivedSourceID,
                                    const juce::String& archivedModID)
    {
        if (auto doc = getDocument())
        {
            if (playbackRegionAndSource != nullptr
                && playbackRegionAndSource->audioSource != nullptr
                && playbackRegionAndSource->audioModification != nullptr)
            {
                auto currentSourceID = getAudioSourcePersistentID();
                auto currentModID = getAudioModificationPersistentID();

                doc->restoreObjectsForPaste (data, archivedSourceID, currentSourceID,
                                             archivedModID, currentModID);
            }
        }
    }

    juce::String getAudioSourcePersistentID() const
    {
        if (playbackRegionAndSource != nullptr && playbackRegionAndSource->audioSource != nullptr)
            return clip.getAudioFile().getHashString() + "_" + clip.itemID.toString();

        return {};
    }

    juce::String getAudioModificationPersistentID() const
    {
        if (playbackRegionAndSource != nullptr && playbackRegionAndSource->audioModification != nullptr)
            return juce::String::toHexString (currentHashCode);

        return {};
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
    ARAFileReader& owner;
    AudioClipBase& clip;
    const AudioFile file;
    Edit& edit;

    std::unique_ptr<ARAInstance> araInstance;
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
    /** NB: Must delete the old objects *after* creating the new ones, because some ARA plugins crash
            if you deselect a play region and then try to select a different one.
            But doing it in the opposite order seems to work ok.
    */
    void recreateTrack (ARAClipPlayer* clipToClone)
    {
        CRASH_TRACER
        TRACKTION_ASSERT_MESSAGE_THREAD

        jassert (araInstance != nullptr);
        jassert (araInstance->factory != nullptr);
        jassert (araInstance->extensionInstance != nullptr);

        auto oldTrack = std::move (playbackRegionAndSource);

        playbackRegionAndSource = std::make_unique<PlaybackRegionAndSource> (*getDocument(), clip, *araInstance->factory,
                                                                             *araInstance->extensionInstance,
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
ARAFileReader::ARAFileReader (Edit& ed, AudioClipBase& clip)
{
    TRACKTION_ASSERT_MESSAGE_THREAD
    CRASH_TRACER

    player = std::make_unique<ARAClipPlayer> (ed, *this, clip);

    if (! player->initialise (nullptr))
        player = nullptr;
}

ARAFileReader::ARAFileReader (Edit& ed, AudioClipBase& clip, ARAFileReader& other)
{
    TRACKTION_ASSERT_MESSAGE_THREAD
    CRASH_TRACER

    if (other.player != nullptr)
    {
        player = std::make_unique<ARAClipPlayer> (ed, *this, clip);

        if (! player->initialise (other.player.get()))
            player = nullptr;
    }

    jassert (player != nullptr);
}

ARAFileReader::~ARAFileReader()
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
void ARAFileReader::showPluginWindow()
{
    if (player != nullptr)
        player->setViewSelection();

    if (auto p = getPlugin())
        p->showWindowExplicitly();
}

void ARAFileReader::hidePluginWindow()
{
    if (auto p = getPlugin())
        p->hideWindowForShutdown();
}

ExternalPlugin* ARAFileReader::getPlugin()
{
    if (isValid())
        return player->getPlugin();

    return {};
}

//==============================================================================
bool ARAFileReader::isAnalysingContent()
{
    return player != nullptr && player->isAnalysingContent();
}

void ARAFileReader::sourceClipChanged()
{
    if (player != nullptr)
    {
        player->updateContent (nullptr);

        // Also update musical context (e.g. when chord track changes)
        if (auto doc = player->getDocument())
        {
            if (doc->musicalContext != nullptr)
            {
                const ARAClipPlayer::ARADocument::ScopedEdit scope (*doc, true);
                doc->musicalContext->update();
            }
        }
    }
}

void ARAFileReader::contentHasChanged()
{
    if (player != nullptr)
        player->contentHasChanged();
}

juce::MemoryBlock ARAFileReader::storeARAArchiveForCopy()
{
    if (player != nullptr)
        return player->storeARAArchiveForCopy();

    return {};
}

void ARAFileReader::restoreARAArchiveForPaste (const juce::MemoryBlock& data,
                                               const juce::String& archivedSourceID,
                                               const juce::String& archivedModID)
{
    if (player != nullptr)
        player->restoreARAArchiveForPaste (data, archivedSourceID, archivedModID);
}

juce::String ARAFileReader::getAudioSourcePersistentID() const
{
    if (player != nullptr)
        return player->getAudioSourcePersistentID();

    return {};
}

juce::String ARAFileReader::getAudioModificationPersistentID() const
{
    if (player != nullptr)
        return player->getAudioModificationPersistentID();

    return {};
}

//==============================================================================
juce::MidiMessageSequence ARAFileReader::getAnalysedMIDISequence()
{
    if (player != nullptr)
        return player->getAnalysedMIDISequence();

    return {};
}

void ARAFileReader::cleanUpOnShutdown()
{
    ARAClipPlayer::ARAPluginFactory::shutdown();
}

//==============================================================================
struct ARADocumentHolder::Pimpl
{
    Pimpl (Edit& e)  : edit (e) {}

    void initialise()
    {
        TRACKTION_ASSERT_MESSAGE_THREAD

        auto& state = edit.getARADocument().lastState;

        // Collect which plugin types are needed by scanning clips
        std::set<juce::String> neededPluginKeys;
        juce::HashMap<juce::String, juce::PluginDescription> pluginDescsByKey;

        visitAllTrackItems (edit, [&] (TrackItem& i)
        {
            if (auto c = dynamic_cast<AudioClipBase*> (&i))
            {
                auto desc = c->araPluginDescription.get();

                if (desc.name.isNotEmpty())
                {
                    auto key = desc.createIdentifierString();
                    neededPluginKeys.insert (key);
                    pluginDescsByKey.set (key, desc);
                }
            }

            return true;
        });

        // Also check for saved ARAPLUGIN children in the state
        for (int i = 0; i < state.getNumChildren(); ++i)
        {
            auto child = state.getChild (i);

            if (child.hasType (IDs::ARAPLUGIN))
            {
                auto key = child.getProperty (IDs::id).toString();

                if (key.isNotEmpty() && neededPluginKeys.find (key) == neededPluginKeys.end())
                    neededPluginKeys.insert (key);
            }
        }

        // If no clips specify a plugin but old-format state has data, use default plugin
        // Prefer Melodyne since legacy clips were always Melodyne
        if (neededPluginKeys.empty())
        {
            auto defaultDescs = edit.engine.getPluginManager().getARACompatiblePlugDescriptions();

            if (! defaultDescs.isEmpty())
            {
                auto preferred = ARAClipPlayer::ARAPluginFactory::findPreferredDefault (defaultDescs);
                auto key = preferred.createIdentifierString();
                neededPluginKeys.insert (key);
                pluginDescsByKey.set (key, preferred);
            }
        }

        // Create documents for each needed plugin type
        for (auto& key : neededPluginKeys)
        {
            juce::PluginDescription desc;

            if (pluginDescsByKey.contains (key))
            {
                desc = pluginDescsByKey[key];
            }
            else
            {
                // Try to find the description from available plugins
                for (auto& d : edit.engine.getPluginManager().getARACompatiblePlugDescriptions())
                {
                    if (d.createIdentifierString() == key)
                    {
                        desc = d;
                        break;
                    }
                }
            }

            if (desc.name.isEmpty())
                continue;

            auto* doc = ARAClipPlayer::createDocument (edit, desc);

            if (doc != nullptr)
                araDocuments[key] = std::unique_ptr<ARAClipPlayer::ARADocument> (doc);
        }

        // Load clip ARA states FIRST — plugin instances must be bound
        // before any beginEditing calls on the document controller
        visitAllTrackItems (edit, [] (TrackItem& i)
        {
            if (auto c = dynamic_cast<AudioClipBase*> (&i))
                c->loadARAState();

            return true;
        });

        // Restore state for each document (now that all instances are bound)
        for (auto& [key, doc] : araDocuments)
        {
            // Look for per-plugin state child
            auto pluginState = state.getChildWithProperty (IDs::id, key);

            if (pluginState.isValid() && pluginState.hasType (IDs::ARAPLUGIN))
            {
                // Create a temporary ARADOCUMENT-typed tree with the data property
                juce::ValueTree tempState (IDs::ARADOCUMENT);
                tempState.setProperty ("data", pluginState.getProperty ("data"), nullptr);
                doc->beginRestoringState (tempState);
            }
            else if (state.hasProperty ("data"))
            {
                // Old format: single data property on ARADOCUMENT itself (backward compat)
                doc->beginRestoringState (state);
            }
        }

        // End restoring for each document
        for (auto& [key, doc] : araDocuments)
            doc->endRestoringState();

        // Notify plugins that musical context content is now available
        for (auto& [key, doc] : araDocuments)
        {
            if (doc->musicalContext != nullptr)
            {
                const ARAClipPlayer::ARADocument::ScopedEdit scope (*doc, true);
                doc->musicalContext->update();
            }
        }
    }

    ARAClipPlayer::ARADocument* getOrCreateDocument (const juce::PluginDescription& desc)
    {
        auto key = desc.createIdentifierString();
        auto it = araDocuments.find (key);

        if (it != araDocuments.end())
            return it->second.get();

        // Create a new document for this plugin type
        auto* doc = ARAClipPlayer::createDocument (edit, desc);

        if (doc != nullptr)
            araDocuments[key] = std::unique_ptr<ARAClipPlayer::ARADocument> (doc);

        return doc;
    }

    ARAClipPlayer::ARADocument* getDocumentForPlugin (const juce::PluginDescription& desc)
    {
        auto key = desc.createIdentifierString();
        auto it = araDocuments.find (key);

        if (it != araDocuments.end())
            return it->second.get();

        return nullptr;
    }

    ARAClipPlayer::ARADocument* getDefaultDocument()
    {
        if (! araDocuments.empty())
            return araDocuments.begin()->second.get();

        return nullptr;
    }

    Edit& edit;
    std::map<juce::String, std::unique_ptr<ARAClipPlayer::ARADocument>> araDocuments;

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
        pimpl = std::make_unique<Pimpl> (edit);
        callBlocking ([this]() { pimpl->initialise(); });
    }

    return pimpl.get();
}

void ARADocumentHolder::flushStateToValueTree()
{
    TRACKTION_ASSERT_MESSAGE_THREAD

    if (pimpl != nullptr)
    {
        // Remove old per-plugin children
        for (int i = lastState.getNumChildren(); --i >= 0;)
            if (lastState.getChild (i).hasType (IDs::ARAPLUGIN))
                lastState.removeChild (i, nullptr);

        // Remove old-format data property
        lastState.removeProperty ("data", nullptr);

        // Save each document under its own ARAPLUGIN child
        for (auto& [key, doc] : pimpl->araDocuments)
        {
            juce::ValueTree pluginState (IDs::ARAPLUGIN);
            pluginState.setProperty (IDs::id, key, nullptr);
            doc->flushStateToValueTree (pluginState);
            lastState.addChild (pluginState, -1, nullptr);
        }
    }
}

ARAClipPlayer::ARADocument* ARAClipPlayer::getDocument() const
{
    if (auto p = edit.getARADocument().getPimpl())
    {
        auto& desc = clip.araPluginDescription;

        if (desc.get().name.isNotEmpty())
            return p->getOrCreateDocument (desc);

        return p->getDefaultDocument();
    }

    return {};
}

}} // namespace tracktion { inline namespace engine

#else

//==============================================================================
namespace tracktion { inline namespace engine
{

struct ARADocumentHolder::Pimpl {};
struct ARAClipPlayer {};

ARAFileReader::ARAFileReader (Edit&, AudioClipBase&) {}
ARAFileReader::ARAFileReader (Edit&, AudioClipBase&, ARAFileReader&) {}
ARAFileReader::~ARAFileReader() {}

void ARAFileReader::cleanUpOnShutdown()                        {}
ExternalPlugin* ARAFileReader::getPlugin()                     { return {}; }
void ARAFileReader::showPluginWindow()                         {}
void ARAFileReader::hidePluginWindow()                         {}
bool ARAFileReader::isAnalysingContent()                       { return false; }
juce::MidiMessageSequence ARAFileReader::getAnalysedMIDISequence()   { return {}; }
void ARAFileReader::sourceClipChanged()                        {}
void ARAFileReader::contentHasChanged()                        {}
juce::MemoryBlock ARAFileReader::storeARAArchiveForCopy()      { return {}; }
void ARAFileReader::restoreARAArchiveForPaste (const juce::MemoryBlock&, const juce::String&, const juce::String&) {}
juce::String ARAFileReader::getAudioSourcePersistentID() const { return {}; }
juce::String ARAFileReader::getAudioModificationPersistentID() const { return {}; }

ARADocumentHolder::ARADocumentHolder (Edit& e, const juce::ValueTree&) : edit (e) { juce::ignoreUnused (edit); }
ARADocumentHolder::~ARADocumentHolder() {}
ARADocumentHolder::Pimpl* ARADocumentHolder::getPimpl()             { return {}; }
void ARADocumentHolder::flushStateToValueTree() {}

}} // namespace tracktion { inline namespace engine

#endif
