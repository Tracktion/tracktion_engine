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

namespace tracktion::inline engine {

using namespace ARA;

struct ARAClipPlayer
{
    #include "tracktion_ARAPluginFactory.h"
    #include "tracktion_ARAWrapperFunctions.h"
    #include "tracktion_ARAWrapperInterfaces.h"

    //==============================================================================
    ARAClipPlayer (Edit& ed, ARAFileReader& o, AudioClipBase& c)
      : owner (o),
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
        editorViewRegistration = nullptr;

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

    juce::String getDocumentArchiveID() const
    {
        if (auto f = getARAFactory())
            return juce::String::fromUTF8 (f->documentArchiveID);

        return {};
    }

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

            // Register this instance's editor view with the document so selection
            // fan-out can reach it directly; the registration removes itself when
            // this player is destroyed
            if (araInstance->extensionInstance != nullptr && araInstance->plugin != nullptr)
                editorViewRegistration = std::make_unique<ARADocument::ScopedEditorView> (*doc,
                                                                                          *araInstance->extensionInstance,
                                                                                          *araInstance->plugin);

            updateContent (clipToClone);
            updateHeadAndTailTimes();

            return playbackRegionAndSource != nullptr
                     && playbackRegionAndSource->hasPlaybackRegions();
        }

        return false;
    }

    void contentHasChanged()
    {
        CRASH_TRACER
        T_ARA_DBG ("ARA contentHasChanged: clip=" << clip.getName() << " modID=" << juce::String::toHexString (modificationID));
        updateContent (nullptr);
        updateHeadAndTailTimes();
        owner.sendChangeMessage();
    }

    //==============================================================================
    void updateContent (ARAClipPlayer* clipToClone)
    {
        CRASH_TRACER
        TRACKTION_ASSERT_MESSAGE_THREAD

        if (juce::MessageManager::getInstance()->isThisTheMessageThread())
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

            // Priority order for reading MIDI content:
            // 1. Playback region content (includes plugin edits, most complete)
            // 2. Audio modification content (contains user edits e.g. Melodyne)
            // 3. Audio source content (original analysis)
            ARAContentReaderRef contentReaderRef = nullptr;
            bool contentIsInPlaybackTime = false;

            // Try playback region content first (includes user edits, most complete)
            if (auto pr = playbackRegionAndSource->getFirstPlaybackRegion())
            {
                if (pr->playbackRegionRef != nullptr
                    && dci->isPlaybackRegionContentAvailable (dcRef, pr->playbackRegionRef, kARAContentTypeNotes))
                {
                    contentReaderRef = dci->createPlaybackRegionContentReader (dcRef, pr->playbackRegionRef,
                                                                               kARAContentTypeNotes, nullptr);
                    contentIsInPlaybackTime = true;
                }
            }

            // Fall back to audio modification content
            if (contentReaderRef == nullptr && playbackRegionAndSource->audioModification != nullptr)
            {
                auto audioModRef = playbackRegionAndSource->audioModification->audioModificationRef;

                if (audioModRef != nullptr
                    && dci->isAudioModificationContentAvailable (dcRef, audioModRef, kARAContentTypeNotes))
                {
                    contentReaderRef = dci->createAudioModificationContentReader (dcRef, audioModRef, kARAContentTypeNotes, nullptr);
                }
            }

            // Fall back to audio source content
            if (contentReaderRef == nullptr)
            {
                auto audioSourceRef = playbackRegionAndSource->audioSource->audioSourceRef;

                if (dci->isAudioSourceContentAvailable (dcRef, audioSourceRef, kARAContentTypeNotes))
                    contentReaderRef = dci->createAudioSourceContentReader (dcRef, audioSourceRef, kARAContentTypeNotes, nullptr);
            }

            if (contentReaderRef != nullptr)
            {
                // If content is from playback region, note times are in playback time
                // and need converting to source time for callers.
                auto clipStart = contentIsInPlaybackTime ? clip.getPosition().getStart().inSeconds() : 0.0;
                auto speedRatio = contentIsInPlaybackTime ? clip.getSpeedRatio() : 1.0;
                auto offset = contentIsInPlaybackTime ? clip.getPosition().getOffset().inSeconds() : 0.0;

                int numEvents = (int) dci->getContentReaderEventCount (dcRef, contentReaderRef);

                for (int i = 0; i < numEvents; ++i)
                {
                    if (auto note = static_cast<const ARAContentNote*> (dci->getContentReaderDataForEvent (dcRef, contentReaderRef, i)))
                    {
                        if (note->pitchNumber != kARAInvalidPitchNumber)
                        {
                            auto startPos = note->startPosition;
                            auto duration = note->noteDuration;

                            if (contentIsInPlaybackTime)
                            {
                                // Convert from playback time to source time:
                                // sourceTime = (playbackTime - clipStart + offset) * speedRatio
                                startPos = (startPos - clipStart + offset) * speedRatio;
                                duration = duration * speedRatio;
                            }

                            result.addEvent (juce::MidiMessage::noteOn  (midiChannel, note->pitchNumber, static_cast<float> (note->volume)),
                                             startPos);

                            result.addEvent (juce::MidiMessage::noteOff (midiChannel, note->pitchNumber),
                                             startPos + duration);
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
                auto srcID = getAudioSourcePersistentID();
                auto modID = getAudioModificationPersistentID();
                T_ARA_DBG ("ARA storeARAArchiveForCopy: srcID=" << srcID << " modID=" << modID);

                return doc->storeObjectsForCopy (playbackRegionAndSource->audioSource->audioSourceRef,
                                                 playbackRegionAndSource->audioModification->audioModificationRef);
            }
        }

        T_ARA_DBG ("ARA storeARAArchiveForCopy: SKIPPED (no playback region/source/mod)");
        return {};
    }

    void restoreARAArchiveForPaste (const juce::MemoryBlock& data,
                                    const juce::String& archivedSourceID,
                                    const juce::String& archivedModID,
                                    const juce::String& documentArchiveID = {})
    {
        T_ARA_DBG ("ARA restoreARAArchiveForPaste: dataSize=" << (int) data.getSize()
             << " archivedSrcID=" << archivedSourceID << " archivedModID=" << archivedModID);

        if (auto doc = getDocument())
        {
            if (playbackRegionAndSource != nullptr
                && playbackRegionAndSource->audioSource != nullptr
                && playbackRegionAndSource->audioModification != nullptr)
            {
                auto currentSourceID = getAudioSourcePersistentID();
                auto currentModID = getAudioModificationPersistentID();
                T_ARA_DBG ("ARA restoreARAArchiveForPaste: CALLING restore. currentSrcID=" << currentSourceID
                     << " currentModID=" << currentModID);

                doc->restoreObjectsForPaste (data, archivedSourceID, currentSourceID,
                                             archivedModID, currentModID, documentArchiveID);
            }
            else
            {
                T_ARA_DBG ("ARA restoreARAArchiveForPaste: SKIPPED (null playbackRegionAndSource/source/mod)");
            }
        }
        else
        {
            T_ARA_DBG ("ARA restoreARAArchiveForPaste: SKIPPED (no document)");
        }
    }

    juce::String getAudioSourcePersistentID() const
    {
        if (playbackRegionAndSource != nullptr && playbackRegionAndSource->audioSource != nullptr)
            return clip.getAudioFile().getHashString();

        return {};
    }

    juce::String getAudioModificationPersistentID() const
    {
        if (playbackRegionAndSource != nullptr && playbackRegionAndSource->audioModification != nullptr)
            return juce::String::toHexString (modificationID);

        return {};
    }

    //==============================================================================
    /** Sends this clip's playback regions and region sequence as the current view
        selection to every instance bound to the same document whose plugin UI is
        currently showing (QA 16250): the document's registered clip players and
        any additional editor-view bindings (the plugin panel/browser).
        Visibility is queried live, so there's no UI state to keep in sync. */
    void setViewSelection()
    {
        if (playbackRegionAndSource == nullptr)
            return;

        auto doc = getDocument();

        // Don't notify while an archive is being restored
        if (doc == nullptr || ! doc->canEdit (true))
            return;

        std::vector<ARAPlaybackRegionRef> regionRefs;

        for (auto& pr : playbackRegionAndSource->playbackRegions)
            if (pr->playbackRegionRef != nullptr)
                regionRefs.push_back (pr->playbackRegionRef);

        if (regionRefs.empty())
            return;

        ARARegionSequenceRef sequenceRef = nullptr;

        if (auto seq = doc->regionSequences.find (playbackRegionAndSource->playbackRegions[0]->trackID);
            seq != doc->regionSequences.end() && seq->second != nullptr)
            sequenceRef = seq->second->regionSequenceRef;

        ARAViewSelection selection;

        selection.structSize = sizeof (selection);
        selection.playbackRegionRefsCount = regionRefs.size();
        selection.playbackRegionRefs = regionRefs.data();
        selection.regionSequenceRefsCount = sequenceRef != nullptr ? 1 : 0;
        selection.regionSequenceRefs = sequenceRef != nullptr ? &sequenceRef : nullptr;
        selection.timeRange = nullptr;

        doc->visitEditorViews ([&] (const ARAPlugInExtensionInstance& extension, ExternalPlugin& plugin)
        {
            if (isPluginUIShowing (plugin))
                notifySelectionTo (extension, selection);
        });
    }

    /** Returns true if the plugin's UI is currently on screen: either a plugin
        window (floating or regular) or an editor embedded elsewhere, like the
        plugin panel (found via the processor's active editor). */
    static bool isPluginUIShowing (ExternalPlugin& p)
    {
        if (p.windowState != nullptr && p.windowState->isWindowShowing())
            return true;

        if (auto pi = p.getAudioPluginInstance())
            if (auto ed = pi->getActiveEditor())
                return ed->isShowing();

        return false;
    }

    static void notifySelectionTo (const ARAPlugInExtensionInstance& instance, const ARAViewSelection& selection)
    {
        if (instance.editorViewInterface != nullptr)
            instance.editorViewInterface->notifySelection (instance.editorViewRef, &selection);
    }

    int getNumPlaybackRegions() const
    {
        return playbackRegionAndSource != nullptr ? (int) playbackRegionAndSource->playbackRegions.size() : 0;
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
                    {
                        T_ARA_DBG ("ARA ContentAnalyser: requesting analysis for " << (int) typesBeingAnalyzed.size() << " content types");
                        dci->requestAudioSourceContentAnalysis (dcRef, audioSourceRef, (ARASize)typesBeingAnalyzed.size(), typesBeingAnalyzed.data());
                    }

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
        std::atomic<bool> analysingContent { false };
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

    TimeDuration getHead() const { return TimeDuration::fromSeconds (araHeadTime); }
    TimeDuration getTail() const { return TimeDuration::fromSeconds (araTailTime); }

    void updateHeadAndTailTimes()
    {
        araHeadTime = 0.0;
        araTailTime = 0.0;

        if (auto doc = getDocument())
        {
            if (playbackRegionAndSource != nullptr)
            {
                if (auto pr = playbackRegionAndSource->getFirstPlaybackRegion())
                {
                    if (pr->playbackRegionRef != nullptr)
                    {
                        doc->dci->getPlaybackRegionHeadAndTailTime (doc->dcRef,
                                                                     pr->playbackRegionRef,
                                                                     &araHeadTime,
                                                                     &araTailTime);
                    }
                }
            }
        }
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
    std::unique_ptr<ARADocument::ScopedEditorView> editorViewRegistration;
    HashCode modificationID = 0;
    double araHeadTime = 0.0;
    double araTailTime = 0.0;

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

        // Don't deactivate the renderer while the audio thread is rendering it -
        // ARANode::process try-locks this and outputs silence while we hold it
        const juce::ScopedLock sl (owner.getProcessLock());

        // ARA requires renderer deactivation before adding/removing playback regions
        if (auto p = getPlugin())
            if (auto pi = p->getAudioPluginInstance())
                pi->releaseResources();

        auto oldTrack = std::move (playbackRegionAndSource);

        playbackRegionAndSource = std::make_unique<PlaybackRegionAndSource> (*getDocument(), clip, *araInstance->factory,
                                                                             *araInstance->extensionInstance,
                                                                             juce::String::toHexString (modificationID),
                                                                             clipToClone != nullptr ? clipToClone->playbackRegionAndSource.get() : nullptr);

        // NB: the caller already holds a ScopedDocumentEditor, so the old regions are
        // destroyed without opening a nested editing cycle (plugins assert on nesting)
        oldTrack = nullptr;

        if (auto p = getPlugin())
            if (auto pi = p->getAudioPluginInstance())
                if (pi->getSampleRate() > 0 && pi->getBlockSize() > 0)
                    pi->prepareToPlay (pi->getSampleRate(), pi->getBlockSize());
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

            HashCode newModificationID = file.getHash()
                                        ^ static_cast<HashCode> (clip.itemID.getRawID());

            T_ARA_DBG ("ARA internalUpdateContent: clip=" << clip.getName()
                 << " oldModID=" << juce::String::toHexString (modificationID)
                 << " newModID=" << juce::String::toHexString (newModificationID)
                 << " willRecreate=" << (modificationID != newModificationID ? 1 : 0));

            bool regionsRecreated = false;

            if (modificationID != newModificationID)
            {
                modificationID = newModificationID;
                const ScopedDocumentEditor sde (*this, true);

                recreateTrack (clipToClone);
                regionsRecreated = true;
            }
            else
            {
                if (playbackRegionAndSource != nullptr
                     && playbackRegionAndSource->hasPlaybackRegions())
                {
                    // Only rebuild when the region layout (loop mode / repeat count) no
                    // longer matches the clip - a rebuild is a remove-all/re-add the
                    // plugin can see and hear, so it mustn't happen for no-op updates
                    if (! playbackRegionAndSource->playbackRegionLayoutMatches (clip.isLooping(),
                                                                                clip.getPosition().getLength().inSeconds(),
                                                                                clip.getLoopLength().inSeconds()))
                    {
                        const ScopedDocumentEditor sde (*this, true);

                        // Don't deactivate the renderer while the audio thread is rendering
                        // it - ARANode::process try-locks this and outputs silence meanwhile
                        const juce::ScopedLock sl (owner.getProcessLock());

                        // ARA requires renderer deactivation before adding/removing regions
                        if (auto p = getPlugin())
                            if (auto pi = p->getAudioPluginInstance())
                                pi->releaseResources();

                        playbackRegionAndSource->rebuildPlaybackRegions();
                        regionsRecreated = true;

                        if (auto p = getPlugin())
                            if (auto pi = p->getAudioPluginInstance())
                                if (pi->getSampleRate() > 0 && pi->getBlockSize() > 0)
                                    pi->prepareToPlay (pi->getSampleRate(), pi->getBlockSize());
                    }
                    else
                    {
                        const ScopedDocumentEditor sde (*this, true);

                        for (auto& pr : playbackRegionAndSource->playbackRegions)
                            pr->updateRange();
                    }
                }
            }

            // Recreating the playback regions gave them new refs, orphaning any
            // selection the plugin's editor views were holding - re-send the current
            // selection so open UIs (e.g. Tonalic's Refine page) keep following the
            // clip (QA 16465). Safe here: both document edit cycles have closed.
            if (regionsRecreated && SelectionManager::findSelectionManagerContaining (clip) != nullptr)
                setViewSelection();

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
        // Polled often enough that plugin-side analysis/content updates feel responsive
        // without burning CPU - this used to be 3s, which made the arrangement's note
        // display lag noticeably behind the plugin
        ModelUpdater (ARADocument& d) : document (d) { startTimer (250); }

        ARADocument& document;

        void timerCallback() override
        {
            CRASH_TRACER

            // notifyModelUpdates must only be called while not editing nor restoring.
            // Editing cycles are synchronous on the message thread so can't overlap the
            // timer, but restoring spans message-loop iterations - guard against it
            if (document.dci != nullptr && document.dcRef != nullptr && document.canEdit (true))
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
    if (auto p = getPlugin())
        p->showWindowExplicitly();

    // After the window is up, so the open-UI filter includes this instance
    notifyViewSelection();
}

void ARAFileReader::notifyViewSelection()
{
    if (player != nullptr)
        player->setViewSelection();
}

int ARAFileReader::getNumPlaybackRegions() const
{
    return player != nullptr ? player->getNumPlaybackRegions() : 0;
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
        // NB: deliberately no musical-context update here - this is called for *any*
        // clip property change (name, colour, drag...), and spamming the plugin with
        // "everything changed" makes it constantly rebuild its model. Tempo/key/chord
        // changes reach the context at document level via
        // ARADocumentHolder::musicalContextContentChanged().
        player->updateContent (nullptr);

        // The plugin won't notify us about content changes we caused ourselves,
        // so re-read head/tail times and tell listeners to refresh any cached
        // content (e.g. the notes shown in the arrangement)
        player->updateHeadAndTailTimes();
        sendChangeMessage();
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
                                               const juce::String& archivedModID,
                                               const juce::String& documentArchiveID)
{
    if (player != nullptr)
        player->restoreARAArchiveForPaste (data, archivedSourceID, archivedModID, documentArchiveID);
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

juce::String ARAFileReader::getDocumentArchiveID() const
{
    if (player != nullptr)
        return player->getDocumentArchiveID();

    return {};
}

TimeDuration ARAFileReader::getHead() const  { return player != nullptr ? player->getHead() : TimeDuration(); }
TimeDuration ARAFileReader::getTail() const  { return player != nullptr ? player->getTail() : TimeDuration(); }

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

                if (pluginState.hasProperty (IDs::araDocumentArchiveID))
                    tempState.setProperty (IDs::araDocumentArchiveID,
                                           pluginState.getProperty (IDs::araDocumentArchiveID), nullptr);

                doc->beginRestoringState (tempState);
            }
            else if (state.hasProperty ("data"))
            {
                // Old format: single data property on ARADOCUMENT itself (backward compat)
                doc->beginRestoringState (state);
            }
        }

        // Build old→new ID mapping groups for backward compatibility with the old ARA ID scheme.
        // Before the refactor, sourceID included the clipID suffix and modificationID
        // included lastModificationTime and trackID in the hash.
        // Each group pairs one source mapping with its associated modification mappings,
        // so that when collisions exist (multiple old sources → same new source),
        // each restoreObjectsFromArchive call only touches its own modifications.
        juce::Array<ARAClipPlayer::PersistentIDMappingGroup> mappingGroups;
        juce::StringArray seenSourceMappings, seenModMappings;

        // Collect identity mod mappings for the new-scheme identity source group
        juce::Array<ARAClipPlayer::PersistentIDMapping> identityModMappings;

        visitAllTrackItems (edit, [&] (TrackItem& i)
        {
            auto c = dynamic_cast<AudioClipBase*> (&i);
            if (c == nullptr)
                return true;

            auto audioFile = c->getAudioFile();
            auto hashString = audioFile.getHashString();

            if (hashString.isEmpty())
                return true;

            auto track = c->getTrack();
            if (track == nullptr)
                return true;

            auto fileHash = audioFile.getHash();
            auto lastModTime = audioFile.getFile().getLastModificationTime().toMilliseconds();
            auto clipRawID = static_cast<HashCode> (c->itemID.getRawID());
            auto trackRawID = static_cast<HashCode> (track->itemID.getRawID());

            auto oldModID = juce::String::toHexString (fileHash ^ lastModTime ^ clipRawID ^ trackRawID);
            auto newModID = juce::String::toHexString (fileHash ^ clipRawID);

            // Saved-ID group: the IDs persisted with the clip at save time take priority -
            // if they differ from the current ones (e.g. the source file moved, which
            // changes the path-derived hash), map the archived IDs to the current ones
            {
                auto storedSourceID = c->state.getProperty (IDs::araSourceID).toString();
                auto storedModID = c->state.getProperty (IDs::araModID).toString();

                if (storedSourceID.isNotEmpty()
                     && (storedSourceID != hashString
                          || (storedModID.isNotEmpty() && storedModID != newModID)))
                {
                    auto srcDedupKey = storedSourceID + "|" + hashString;

                    if (! seenSourceMappings.contains (srcDedupKey))
                    {
                        seenSourceMappings.add (srcDedupKey);

                        ARAClipPlayer::PersistentIDMappingGroup group;
                        group.sourceMapping = { storedSourceID, hashString };

                        if (storedModID.isNotEmpty() && storedModID != newModID)
                            group.modificationMappings.add ({ storedModID, newModID });

                        mappingGroups.add (std::move (group));
                    }
                }
            }

            // Per-clip group: old source ID → new source ID, with this clip's mod mapping
            {
                auto oldSourceID = hashString + "_" + c->itemID.toString();
                auto srcDedupKey = oldSourceID + "|" + hashString;

                if (! seenSourceMappings.contains (srcDedupKey))
                {
                    seenSourceMappings.add (srcDedupKey);

                    ARAClipPlayer::PersistentIDMappingGroup group;
                    group.sourceMapping = { oldSourceID, hashString };

                    // Only include old→new mod mapping for this clip
                    if (oldModID != newModID)
                        group.modificationMappings.add ({ oldModID, newModID });

                    mappingGroups.add (std::move (group));
                }
            }

            // Collect identity mod mappings for the shared identity source group
            {
                auto newModDedupKey = newModID + "|" + newModID;

                if (! seenModMappings.contains (newModDedupKey))
                {
                    seenModMappings.add (newModDedupKey);
                    identityModMappings.add ({ newModID, newModID });
                }
            }

            return true;
        });

        // Add one identity source group per unique file with all identity mod mappings
        // (for archives saved with the new scheme)
        {
            juce::StringArray seenIdentitySources;

            visitAllTrackItems (edit, [&] (TrackItem& i)
            {
                auto c = dynamic_cast<AudioClipBase*> (&i);
                if (c == nullptr)
                    return true;

                auto hashString = c->getAudioFile().getHashString();

                if (hashString.isNotEmpty() && ! seenIdentitySources.contains (hashString))
                {
                    seenIdentitySources.add (hashString);

                    ARAClipPlayer::PersistentIDMappingGroup group;
                    group.sourceMapping = { hashString, hashString };
                    group.modificationMappings = identityModMappings;
                    mappingGroups.add (std::move (group));
                }

                return true;
            });
        }

        // End restoring for each document, passing grouped ID mappings
        for (auto& [key, doc] : araDocuments)
            doc->endRestoringState (mappingGroups);

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
        // Save each document under its own ARAPLUGIN child. NB: never replace a
        // previously-saved archive with nothing - if a document produced no data
        // (the plugin failed to store, or no documents were loaded, e.g. an edit
        // saved by an export job that doesn't load plugins), keep whatever was
        // saved before rather than silently wiping the user's ARA edits
        for (auto& [key, doc] : pimpl->araDocuments)
        {
            juce::ValueTree pluginState (IDs::ARAPLUGIN);
            pluginState.setProperty (IDs::id, key, nullptr);
            doc->flushStateToValueTree (pluginState);

            if (! pluginState.hasProperty ("data"))
                continue;

            auto existing = lastState.getChildWithProperty (IDs::id, key);

            if (existing.isValid())
                lastState.removeChild (existing, nullptr);

            lastState.addChild (pluginState, -1, nullptr);

            // This document is now saved per-plugin, so the old-format
            // whole-document property is stale
            lastState.removeProperty ("data", nullptr);
        }
    }
}

//==============================================================================
struct ARAPluginBinding::Impl
{
    std::unique_ptr<ARAClipPlayer::ARAInstance> instance;

    // Registers the instance's editor view for selection fan-out; removes itself
    // when the binding dies with its plugin, which is torn down before the Edit's
    // ARADocumentHolder (and so before the document)
    std::unique_ptr<ARAClipPlayer::ARADocument::ScopedEditorView> editorViewRegistration;
};

ARAPluginBinding::ARAPluginBinding (std::unique_ptr<Impl> implToUse)
    : impl (std::move (implToUse))
{
}

ARAPluginBinding::~ARAPluginBinding() = default;

void ARADocumentHolder::musicalContextContentChanged()
{
    TRACKTION_ASSERT_MESSAGE_THREAD

    // Only notify documents that already exist - don't trigger initialisation
    if (pimpl != nullptr)
        for (auto& [key, doc] : pimpl->araDocuments)
            if (doc != nullptr)
                doc->musicalContextContentChanged();
}

bool ARADocumentHolder::bindPluginToDocument (ExternalPlugin& plugin,
                                              const juce::PluginDescription& desc)
{
    TRACKTION_ASSERT_MESSAGE_THREAD

    if (! desc.hasARAExtension)
        return false;

    // A plugin returned from the PluginCache may already be bound from an earlier
    // panel session - the bind lasts for the instance's lifetime (destroying the
    // binding wrapper doesn't end it), and binding twice is invalid, so keep it
    if (plugin.isBoundToARADocument())
        return true;

    auto doc = getPimpl()->getOrCreateDocument (desc);

    if (doc == nullptr)
        return false;

    auto& pluginFactory = ARAClipPlayer::ARAPluginFactory::getInstance (edit.engine, desc);

    // The browser/panel instance is assigned only the editor roles so the plugin
    // doesn't act as a playback renderer (e.g. previewing its accompaniment while
    // the DAW is playing) - see QA 16472
    std::unique_ptr<ARAClipPlayer::ARAInstance> instance (pluginFactory.createInstance (plugin, doc->dcRef,
                                                                                        kARAEditorRendererRole | kARAEditorViewRole));

    if (instance == nullptr)
        return false;

    // Its VST3 instance is ARA-bound, so it must be destroyed synchronously
    // (before the document controller) rather than via the async deleter
    plugin.setDeletesPluginInstanceSynchronously (true);

    auto impl = std::make_unique<ARAPluginBinding::Impl>();
    impl->instance = std::move (instance);

    // Register the editor view so arrangement selection changes reach it while its
    // UI is showing (queried live at notify time - see ARAClipPlayer::setViewSelection)
    if (impl->instance->extensionInstance != nullptr)
        impl->editorViewRegistration = std::make_unique<ARAClipPlayer::ARADocument::ScopedEditorView> (*doc,
                                                                                                       *impl->instance->extensionInstance,
                                                                                                       plugin);

    // The plugin owns the binding, so a strong ref back to it would be a refcount cycle
    impl->instance->plugin = nullptr;

    plugin.setARADocumentBinding (std::unique_ptr<ARAPluginBinding> (new ARAPluginBinding (std::move (impl))));
    return true;
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

juce::PluginDescription ARAFileReader::findPluginForARAArchiveID (Engine& engine, const juce::String& archiveID,
                                                                  const juce::String& suggestedPlugInName)
{
    auto araDescs = engine.getPluginManager().getARACompatiblePlugDescriptions();

    // If the file says which plugin wrote the archive, only consider matching plugins.
    // Checking a plugin's archive IDs means loading its module, which runs third-party
    // module code - some plugins start background threads that don't survive teardown,
    // so we mustn't load every installed ARA plugin on the off-chance it matches.
    if (suggestedPlugInName.isNotEmpty())
    {
        juce::Array<juce::PluginDescription> matching;

        for (auto& d : araDescs)
            if (d.name.equalsIgnoreCase (suggestedPlugInName))
                matching.add (d);

        araDescs = matching;
    }

    for (auto& desc : araDescs)
    {
        if (! desc.hasARAExtension)
            continue;

        const ARAFactory* araFactory = nullptr;

        if (auto existing = ARAClipPlayer::ARAPluginFactory::getExistingInstance (desc))
        {
            // A live factory means this plugin is (or was) in use and has already
            // initialised ARA for its module - read the IDs from it, as initialising
            // the module a second time via the lookup below is invalid
            araFactory = existing->factory;
        }
        else
        {
            // Otherwise read the ARAFactory from the module-level IMainFactory rather
            // than going through ARAPluginFactory: that would instantiate a full plugin
            // component just to inspect its archive IDs.
            // The result is cached for the session: releasing it would call the module's
            // bundleExit mid-session, tearing down module state while any background
            // threads the plugin started are still running.
            auto& cache = ARAClipPlayer::ARAPluginFactory::getLookupCache();
            auto key = desc.createIdentifierString();
            auto cached = cache.find (key);

            if (cached == cache.end())
            {
                juce::ARAFactoryResult result;
                engine.getPluginManager().pluginFormatManager
                    .createARAFactoryAsync (desc, [&result] (juce::ARAFactoryResult r) { result = std::move (r); });

                // The callback is synchronous for VST3; if a format ever completes
                // asynchronously the factory will be null here and the plugin is skipped
                cached = cache.emplace (key, std::move (result)).first;
            }

            araFactory = cached->second.araFactory.get();
        }

        if (araFactory == nullptr)
            continue;

        if (archiveID == juce::String::fromUTF8 (araFactory->documentArchiveID))
            return desc;

        for (ARASize i = 0; i < araFactory->compatibleDocumentArchiveIDsCount; ++i)
        {
            if (archiveID == juce::String::fromUTF8 (araFactory->compatibleDocumentArchiveIDs[i]))
                return desc;
        }
    }

    return {};
}

} // namespace tracktion::inline engine

#else

//==============================================================================
namespace tracktion::inline engine {

struct ARADocumentHolder::Pimpl {};
struct ARAClipPlayer {};

ARAFileReader::ARAFileReader (Edit&, AudioClipBase&) {}
ARAFileReader::ARAFileReader (Edit&, AudioClipBase&, ARAFileReader&) {}
ARAFileReader::~ARAFileReader() {}

void ARAFileReader::cleanUpOnShutdown()                        {}
ExternalPlugin* ARAFileReader::getPlugin()                     { return {}; }
void ARAFileReader::showPluginWindow()                         {}
void ARAFileReader::hidePluginWindow()                         {}
void ARAFileReader::notifyViewSelection()                      {}
int ARAFileReader::getNumPlaybackRegions() const               { return 0; }
bool ARAFileReader::isAnalysingContent()                       { return false; }
juce::MidiMessageSequence ARAFileReader::getAnalysedMIDISequence()   { return {}; }
void ARAFileReader::sourceClipChanged()                        {}
void ARAFileReader::contentHasChanged()                        {}
juce::MemoryBlock ARAFileReader::storeARAArchiveForCopy()      { return {}; }
void ARAFileReader::restoreARAArchiveForPaste (const juce::MemoryBlock&, const juce::String&, const juce::String&, const juce::String&) {}
juce::String ARAFileReader::getAudioSourcePersistentID() const { return {}; }
juce::String ARAFileReader::getAudioModificationPersistentID() const { return {}; }
juce::String ARAFileReader::getDocumentArchiveID() const             { return {}; }
TimeDuration ARAFileReader::getHead() const                    { return {}; }
TimeDuration ARAFileReader::getTail() const                    { return {}; }

juce::PluginDescription ARAFileReader::findPluginForARAArchiveID (Engine&, const juce::String&, const juce::String&) { return {}; }

ARADocumentHolder::ARADocumentHolder (Edit& e, const juce::ValueTree&) : edit (e) { juce::ignoreUnused (edit); }
ARADocumentHolder::~ARADocumentHolder() {}
ARADocumentHolder::Pimpl* ARADocumentHolder::getPimpl()             { return {}; }
void ARADocumentHolder::flushStateToValueTree() {}
void ARADocumentHolder::musicalContextContentChanged() {}

struct ARAPluginBinding::Impl {};
ARAPluginBinding::ARAPluginBinding (std::unique_ptr<Impl> implToUse) : impl (std::move (implToUse)) {}
ARAPluginBinding::~ARAPluginBinding() = default;
bool ARADocumentHolder::bindPluginToDocument (ExternalPlugin&, const juce::PluginDescription&)   { return false; }

} // namespace tracktion::inline engine

#endif

//==============================================================================
// Static utility methods that work regardless of ARA being enabled
namespace tracktion::inline engine {

juce::String ARAFileReader::readRawIXMLFromSourceFile (const juce::File& file)
{
    juce::FileInputStream stream (file);

    if (! stream.openedOk() || stream.getTotalLength() < 12)
        return {};

    char header[4];
    stream.read (header, 4);

    if (std::memcmp (header, "RIFF", 4) == 0)
    {
        stream.readInt(); // file size
        stream.read (header, 4); // WAVE

        if (std::memcmp (header, "WAVE", 4) != 0)
            return {};

        while (! stream.isExhausted())
        {
            char chunkID[4];

            if (stream.read (chunkID, 4) < 4)
                break;

            auto chunkSize = (uint32_t) stream.readInt();

            if (std::memcmp (chunkID, "iXML", 4) == 0)
            {
                juce::MemoryBlock block;
                stream.readIntoMemoryBlock (block, static_cast<std::ptrdiff_t> (chunkSize));
                return block.toString();
            }

            stream.setPosition (stream.getPosition() + (int64_t) ((chunkSize + 1) & ~1u));
        }
    }
    else if (std::memcmp (header, "FORM", 4) == 0)
    {
        stream.readInt(); // file size
        stream.read (header, 4); // AIFF or AIFC

        if (std::memcmp (header, "AIFF", 4) != 0 && std::memcmp (header, "AIFC", 4) != 0)
            return {};

        while (! stream.isExhausted())
        {
            char chunkID[4];

            if (stream.read (chunkID, 4) < 4)
                break;

            auto chunkSize = (uint32_t) stream.readIntBigEndian();

            if (std::memcmp (chunkID, "iXML", 4) == 0)
            {
                juce::MemoryBlock block;
                stream.readIntoMemoryBlock (block, static_cast<std::ptrdiff_t> (chunkSize));
                return block.toString();
            }

            stream.setPosition (stream.getPosition() + (int64_t) ((chunkSize + 1) & ~1u));
        }
    }

    return {};
}

juce::Array<ARAFileReader::ARAChunkInfo> ARAFileReader::parseARAAudioFileChunksFromIXML (const juce::String& ixmlString)
{
    juce::Array<ARAChunkInfo> results;

    auto xml = juce::parseXML (ixmlString);

    if (xml == nullptr)
        return results;

    // The <ARA> element may be top-level or nested inside iXML root
    auto araElement = xml->hasTagName ("ARA") ? xml.get()
                                              : xml->getChildByName ("ARA");

    if (araElement == nullptr)
        return results;

    auto audioSourcesElement = araElement->getChildByName ("audioSources");

    if (audioSourcesElement == nullptr)
        return results;

    for (auto audioSourceElement : audioSourcesElement->getChildWithTagNameIterator ("audioSource"))
    {
        ARAChunkInfo info;

        if (auto e = audioSourceElement->getChildByName ("documentArchiveID"))
            info.documentArchiveID = e->getAllSubText().trim();

        if (auto e = audioSourceElement->getChildByName ("openAutomatically"))
            info.openAutomatically = e->getAllSubText().trim().equalsIgnoreCase ("true");

        if (auto e = audioSourceElement->getChildByName ("persistentID"))
            info.persistentID = e->getAllSubText().trim();

        if (auto e = audioSourceElement->getChildByName ("archiveData"))
        {
                juce::MemoryOutputStream mos (info.archiveData, false);
                juce::Base64::convertFromBase64 (mos, e->getAllSubText().trim());
            }

        if (auto suggestedPlugin = audioSourceElement->getChildByName ("suggestedPlugIn"))
        {
            if (auto e = suggestedPlugin->getChildByName ("plugInName"))
                info.suggestedPlugInName = e->getAllSubText().trim();

            if (auto e = suggestedPlugin->getChildByName ("manufacturerName"))
                info.manufacturerName = e->getAllSubText().trim();
        }

        if (info.documentArchiveID.isNotEmpty())
            results.add (std::move (info));
    }

    return results;
}

ARAIXMLResult detectARAFromIXMLChunks (Engine& engine, const juce::File& sourceFile)
{
   #if TRACKTION_ENABLE_ARA
    if (! sourceFile.existsAsFile())
        return {};

    auto ixmlString = ARAFileReader::readRawIXMLFromSourceFile (sourceFile);

    if (ixmlString.isEmpty())
        return {};

    for (auto& chunk : ARAFileReader::parseARAAudioFileChunksFromIXML (ixmlString))
    {
        if (! chunk.openAutomatically)
            continue;

        auto desc = ARAFileReader::findPluginForARAArchiveID (engine, chunk.documentArchiveID, chunk.suggestedPlugInName);

        if (desc.name.isNotEmpty())
        {
            TRACKTION_LOG ("Auto-configured ARA plugin from iXML chunk: " + desc.name);
            return { desc, chunk.archiveData, chunk.persistentID, chunk.documentArchiveID };
        }

        TRACKTION_LOG ("ARA iXML chunk found for archive ID '" + chunk.documentArchiveID
                        + "' (suggested: " + chunk.suggestedPlugInName
                        + ") but no matching plugin installed");
    }
   #else
    juce::ignoreUnused (engine, sourceFile);
   #endif

    return {};
}

} // namespace tracktion::inline engine
