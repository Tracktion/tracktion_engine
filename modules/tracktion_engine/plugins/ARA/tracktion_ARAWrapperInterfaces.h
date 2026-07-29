/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

class MusicalContextWrapper;
class RegionSequenceWrapper;
class AudioSourceWrapper;

ARA_MAP_HOST_REF(juce::MemoryOutputStream, ARAArchiveWriterHostRef)
ARA_MAP_HOST_REF(juce::MemoryBlock, ARAArchiveReaderHostRef)
ARA_MAP_HOST_REF(Edit, ARAContentAccessControllerHostRef, ARAModelUpdateControllerHostRef, ARAMusicalContextHostRef, ARARegionSequenceHostRef, ARAAudioModificationHostRef, ARAPlaybackRegionHostRef)
ARA_MAP_HOST_REF(TransportControl, ARAPlaybackControllerHostRef)
ARA_MAP_HOST_REF(AudioSourceWrapper, ARAAudioSourceHostRef)

/**
    As specified by the Celmony:
    - 1 document per Edit
    - 1 musical context per edit
*/
struct PersistentIDMapping
{
    juce::String archiveID;
    juce::String currentID;
};

struct PersistentIDMappingGroup
{
    PersistentIDMapping sourceMapping;
    juce::Array<PersistentIDMapping> modificationMappings;
};

class ARADocument
{
public:
    ARADocument (Edit& sourceEdit,
                 ARAInstance* validPluginWrapper,
                 const ARAPlugInExtensionInstance&,
                 const ARADocumentControllerInstance& dc,
                 ARADocumentControllerHostInstance* dchi,
                 ArchivingState* archState)
      : edit (sourceEdit),
        dci (dc.documentControllerInterface),
        dcRef (dc.documentControllerRef),
        wrapper (validPluginWrapper),
        hostInstance (dchi),
        archivingState (archState)
    {
        CRASH_TRACER
        jassert (wrapper != nullptr);
        musicalContext = std::make_unique<MusicalContextWrapper> (*this);
    }

    ~ARADocument()
    {
        CRASH_TRACER

        if (musicalContext != nullptr)
        {
            const ScopedEdit scope (*this, true);
            audioSources.clear();
            audioSourceRefCount.clear();
            regionSequences.clear();
            musicalContext = nullptr;
        }

        // The spec requires bound plugin instances to be destroyed before the
        // document controller they're bound to
        wrapper = nullptr;
        dci->destroyDocumentController (dcRef);
    }

    bool canEdit (bool dontCheckMusicalContext) const
    {
        return dci != nullptr && dcRef != nullptr
                && lastArchiveState == nullptr
                && (dontCheckMusicalContext ? true : musicalContext != nullptr);
    }

    void beginEditing (bool dontCheckMusicalContext)
    {
        TRACKTION_ASSERT_MESSAGE_THREAD

        if (canEdit (dontCheckMusicalContext))
            dci->beginEditing (dcRef);
    }

    void endEditing (bool dontCheckMusicalContext)
    {
        TRACKTION_ASSERT_MESSAGE_THREAD

        if (canEdit (dontCheckMusicalContext))
            dci->endEditing (dcRef);
    }

    struct ScopedEdit
    {
        ScopedEdit (ARADocument& d, bool dontCheckMusicalContext)
            : doc (d), skipContextCheck (dontCheckMusicalContext)
        {
            doc.beginEditing (skipContextCheck);
        }

        ~ScopedEdit()
        {
            doc.endEditing (skipContextCheck);
        }

    private:
        ARADocument& doc;
        const bool skipContextCheck;

        JUCE_DECLARE_NON_COPYABLE (ScopedEdit)
    };

    void flushStateToValueTree (juce::ValueTree& v)
    {
        CRASH_TRACER
        TRACKTION_ASSERT_MESSAGE_THREAD

        juce::MemoryBlock data;
        juce::MemoryOutputStream out (data, false);

        if (dci->storeObjectsToArchive (dcRef, toHostRef (&out), nullptr))
        {
            out.flush();

            if (data.getSize() > 0)
            {
                v.setProperty ("data", data.toBase64Encoding(), nullptr);

                // Store the format ID the plugin used for this archive: if the plugin
                // later changes its archive format, restore must report the ID the
                // archive was saved with, not the factory's current one
                if (wrapper != nullptr && wrapper->factory != nullptr)
                    v.setProperty (IDs::araDocumentArchiveID,
                                   juce::String::fromUTF8 (wrapper->factory->documentArchiveID), nullptr);
            }
        }
    }

    /** @note Must not be editing or already restoring the document while restoring
              from a state.
    */
    void beginRestoringState (const juce::ValueTree& state)
    {
        CRASH_TRACER
        TRACKTION_ASSERT_MESSAGE_THREAD

        jassert (state.hasType (IDs::ARADOCUMENT));

        auto data = state.getProperty ("data").toString();
        auto savedArchiveID = state.getProperty (IDs::araDocumentArchiveID).toString();

        // Refuse archives whose format the plugin doesn't declare compatible -
        // restoring foreign/incompatible data is undefined and some plugins abort
        if (data.isNotEmpty() && savedArchiveID.isNotEmpty()
             && wrapper != nullptr && wrapper->factory != nullptr)
        {
            const auto& f = *wrapper->factory;
            bool compatible = (juce::String::fromUTF8 (f.documentArchiveID) == savedArchiveID);

            for (ARASize i = 0; ! compatible && i < f.compatibleDocumentArchiveIDsCount; ++i)
                if (f.compatibleDocumentArchiveIDs != nullptr
                     && juce::String::fromUTF8 (f.compatibleDocumentArchiveIDs[i]) == savedArchiveID)
                    compatible = true;

            if (! compatible)
            {
                TRACKTION_LOG_ERROR ("ARA: not restoring document archive with incompatible ID '"
                                     + savedArchiveID + "' (plug-in archive ID '"
                                     + juce::String::fromUTF8 (f.documentArchiveID) + "')");
                data = {};
            }
        }

        if (data.isNotEmpty())
        {
            beginEditing (true);

            lastArchiveState = std::make_unique<juce::MemoryBlock>();
            lastArchiveState->fromBase64Encoding (data);
            lastArchiveDocumentArchiveID = savedArchiveID;
        }
        else
        {
            lastArchiveState = nullptr;
            lastArchiveDocumentArchiveID = {};
        }
    }

    void endRestoringState (juce::Array<PersistentIDMappingGroup> groups = {})
    {
        CRASH_TRACER
        TRACKTION_ASSERT_MESSAGE_THREAD

        if (lastArchiveState)
        {
            // While restoring, getDocumentArchiveID must report the ID stored with
            // the archive rather than the factory's current one
            archivingState->documentArchiveIDOverride = lastArchiveDocumentArchiveID;

            if (groups.isEmpty())
            {
                dci->restoreObjectsFromArchive (dcRef, toHostRef (lastArchiveState.get()), nullptr);
            }
            else
            {
                // Check if any groups share the same sourceMapping.currentID (collision)
                bool hasCollisions = false;
                {
                    juce::StringArray seenCurrentIDs;

                    for (auto& g : groups)
                    {
                        if (seenCurrentIDs.contains (g.sourceMapping.currentID))
                        {
                            hasCollisions = true;
                            break;
                        }

                        seenCurrentIDs.add (g.sourceMapping.currentID);
                    }
                }

                if (! hasCollisions)
                {
                    // Fast path: no collisions, flatten all groups into one call
                    juce::Array<PersistentIDMapping> allSources, allMods;

                    for (auto& g : groups)
                    {
                        allSources.add (g.sourceMapping);
                        allMods.addArray (g.modificationMappings);
                    }

                    restoreWithFilter (allSources, allMods, kARATrue);
                }
                else
                {
                    // Collisions: multiple groups map to the same current source.
                    // Re-restoring a source wipes its modifications (see
                    // restoreObjectsForPaste which skips source mapping for
                    // shared sources for the same reason).
                    //
                    // Strategy: for groups where archiveID != currentID (old-scheme
                    // remapping), only include the source mapping on the FIRST
                    // group per currentID. Subsequent groups with the same
                    // currentID skip the source mapping so the source isn't
                    // re-restored (which would wipe earlier modifications).
                    // ARA finds archived modifications by persistent ID globally
                    // when no source mapping scopes the search.
                    //
                    // Identity groups (archiveID == currentID) always include
                    // their source mapping — these handle new-scheme archives
                    // where the source ID already matches and won't collide
                    // with old-scheme groups (old-scheme sources aren't in the
                    // new archive, so those calls are no-ops).
                    juce::StringArray restoredSourceIDs;
                    bool isFirstCall = true;

                    for (auto& g : groups)
                    {
                        juce::Array<PersistentIDMapping> sourceForThisCall;
                        bool isIdentity = (g.sourceMapping.archiveID == g.sourceMapping.currentID);

                        if (isIdentity || ! restoredSourceIDs.contains (g.sourceMapping.currentID))
                        {
                            sourceForThisCall.add (g.sourceMapping);

                            if (! isIdentity)
                                restoredSourceIDs.add (g.sourceMapping.currentID);
                        }

                        restoreWithFilter (sourceForThisCall, g.modificationMappings,
                                           isFirstCall ? kARATrue : kARAFalse);
                        isFirstCall = false;
                    }
                }
            }

            archivingState->documentArchiveIDOverride = {};
            lastArchiveDocumentArchiveID = {};
            lastArchiveState = nullptr; // Make sure this is deleted before the call to endEditing or it won't get passed to the document

            endEditing (true);
        }
    }

private:
    void restoreWithFilter (const juce::Array<PersistentIDMapping>& sourceMappings,
                            const juce::Array<PersistentIDMapping>& modificationMappings,
                            ARABool documentData)
    {
        juce::Array<ARAPersistentID> srcArchiveIDs, srcCurrentIDs, modArchiveIDs, modCurrentIDs;

        for (auto& m : sourceMappings)
        {
            srcArchiveIDs.add (m.archiveID.toRawUTF8());
            srcCurrentIDs.add (m.currentID.toRawUTF8());
        }

        for (auto& m : modificationMappings)
        {
            modArchiveIDs.add (m.archiveID.toRawUTF8());
            modCurrentIDs.add (m.currentID.toRawUTF8());
        }

        SizedStruct<ARA_STRUCT_MEMBER (ARARestoreObjectsFilter, audioModificationCurrentIDs)> filter {};
        filter.documentData = documentData;

        if (! sourceMappings.isEmpty())
        {
            filter.audioSourceIDsCount = static_cast<ARASize> (srcArchiveIDs.size());
            filter.audioSourceArchiveIDs = srcArchiveIDs.getRawDataPointer();
            filter.audioSourceCurrentIDs = srcCurrentIDs.getRawDataPointer();
        }

        if (! modificationMappings.isEmpty())
        {
            filter.audioModificationIDsCount = static_cast<ARASize> (modArchiveIDs.size());
            filter.audioModificationArchiveIDs = modArchiveIDs.getRawDataPointer();
            filter.audioModificationCurrentIDs = modCurrentIDs.getRawDataPointer();
        }

        dci->restoreObjectsFromArchive (dcRef, toHostRef (lastArchiveState.get()), &filter);
    }

public:
    /** Store a partial ARA archive containing just the given audio source and modification.
        Used by clipboard copy to capture ARA plugin edits (e.g. Melodyne note corrections).
    */
    juce::MemoryBlock storeObjectsForCopy (ARAAudioSourceRef source, ARAAudioModificationRef modification)
    {
        CRASH_TRACER
        TRACKTION_ASSERT_MESSAGE_THREAD

        juce::MemoryBlock data;
        juce::MemoryOutputStream out (data, false);

        // Flush pending plugin state changes before archiving
        dci->notifyModelUpdates (dcRef);

        SizedStruct<ARA_STRUCT_MEMBER (ARAStoreObjectsFilter, audioModificationRefs)> filter {};
        filter.documentData = kARAFalse;
        filter.audioSourceRefsCount = 1;
        filter.audioSourceRefs = &source;
        filter.audioModificationRefsCount = 1;
        filter.audioModificationRefs = &modification;

        dci->storeObjectsToArchive (dcRef, toHostRef (&out), &filter);
        out.flush();

        T_ARA_DBG ("ARA storeObjectsForCopy: archived " << (int) data.getSize() << " bytes");
        return data;
    }

    /** Restore a partial ARA archive, mapping archived persistent IDs to current ones.
        Used by clipboard paste to restore ARA plugin edits into a newly pasted clip.
    */
    void restoreObjectsForPaste (const juce::MemoryBlock& data,
                                 const juce::String& archivedSourceID, const juce::String& currentSourceID,
                                 const juce::String& archivedModID, const juce::String& currentModID,
                                 const juce::String& documentArchiveID = {})
    {
        CRASH_TRACER
        TRACKTION_ASSERT_MESSAGE_THREAD

        T_ARA_DBG ("ARA restoreObjectsForPaste: dataSize=" << (int) data.getSize()
             << " archivedSrcID=" << archivedSourceID
             << " currentSrcID=" << currentSourceID
             << " archivedModID=" << archivedModID
             << " currentModID=" << currentModID
             << " docArchiveID=" << documentArchiveID);

        if (data.getSize() == 0)
        {
            T_ARA_DBG ("ARA restoreObjectsForPaste: SKIPPED (empty data)");
            return;
        }

        // Don't hand archive data to the plug-in unless it declares the archive's format compatible.
        // Restoring foreign/incompatible archive data (a cross-plug-in paste, or a corrupt project/
        // clipboard) is undefined per the ARA spec - well-behaved plug-ins return false, but some abort.
        // The host can cheaply reject obviously-incompatible data by matching the supplied
        // documentArchiveID against the plug-in factory's own ID and its declared compatible IDs.
        if (documentArchiveID.isNotEmpty() && wrapper != nullptr && wrapper->factory != nullptr)
        {
            const auto& f = *wrapper->factory;
            bool compatible = (juce::String::fromUTF8 (f.documentArchiveID) == documentArchiveID);

            for (ARASize i = 0; ! compatible && i < f.compatibleDocumentArchiveIDsCount; ++i)
                if (f.compatibleDocumentArchiveIDs != nullptr
                     && juce::String::fromUTF8 (f.compatibleDocumentArchiveIDs[i]) == documentArchiveID)
                    compatible = true;

            if (! compatible)
            {
                TRACKTION_LOG_ERROR ("ARA restoreObjectsForPaste: refusing archive with incompatible "
                                     "documentArchiveID '" + documentArchiveID + "' (plug-in archive ID '"
                                     + juce::String::fromUTF8 (f.documentArchiveID) + "')");
                return;
            }
        }

        juce::MemoryBlock dataCopy (data);

        // Flush so the plugin acknowledges the newly created modification before restoring into it
        dci->notifyModelUpdates (dcRef);

        SizedStruct<ARA_STRUCT_MEMBER (ARARestoreObjectsFilter, audioModificationCurrentIDs)> filter {};
        filter.documentData = kARAFalse;

        ARAPersistentID srcArchiveID = archivedSourceID.toRawUTF8();
        ARAPersistentID srcCurrentID = currentSourceID.toRawUTF8();

        auto isShared = [&]
        {
            auto it = audioSourceRefCount.find (currentSourceID);
            return it != audioSourceRefCount.end() && it->second > 1;
        };

        // If we don't skip the sourceID assignment when pasting to the same document,
        // it seems like the original Melodyne source will get its modifications wiped
        if (archivedSourceID != currentSourceID
            || ! isShared())
        {
            filter.audioSourceIDsCount = 1;
            filter.audioSourceArchiveIDs = &srcArchiveID;
            filter.audioSourceCurrentIDs = &srcCurrentID;
        }

        ARAPersistentID modArchiveID = archivedModID.toRawUTF8();
        ARAPersistentID modCurrentID = currentModID.toRawUTF8();

        if (archivedModID.isNotEmpty())
        {
            filter.audioModificationIDsCount = 1;
            filter.audioModificationArchiveIDs = &modArchiveID;
            filter.audioModificationCurrentIDs = &modCurrentID;
        }

        beginEditing (true);
        archivingState->documentArchiveIDOverride = documentArchiveID;
        auto restoreResult = dci->restoreObjectsFromArchive (dcRef, toHostRef (&dataCopy), &filter);
        T_ARA_DBG ("ARA restoreObjectsFromArchive returned: " << (restoreResult ? "TRUE" : "FALSE"));
        juce::ignoreUnused (restoreResult);
        archivingState->documentArchiveIDOverride = {};
        endEditing (true);
    }

    void willCreatePlaybackRegionOnTrack (Track* track)
    {
        auto id = track->itemID;

        if (regionSequences.count (id) == 0)
            regionSequences[id] = std::make_unique<RegionSequenceWrapper> (*this, track);

        regionSequencePlaybackRegionCount[id]++;
    }

    void willDestroyPlaybackRegionOnTrack (EditItemID trackID)
    {
        jassert (regionSequencePlaybackRegionCount.count (trackID) > 0);

        if (--regionSequencePlaybackRegionCount[trackID] == 0)
        {
            regionSequences.erase (trackID);
            regionSequencePlaybackRegionCount.erase (trackID);
        }
    }

    std::shared_ptr<AudioSourceWrapper> acquireAudioSource (AudioClipBase& audioClip)
    {
        auto key = audioClip.getAudioFile().getHashString();

        if (audioSources.count (key) == 0)
            audioSources[key] = std::make_shared<AudioSourceWrapper> (*this, audioClip);

        audioSourceRefCount[key]++;
        return audioSources[key];
    }

    void releaseAudioSource (const juce::String& key)
    {
        jassert (audioSourceRefCount.count (key) > 0);

        if (--audioSourceRefCount[key] == 0)
        {
            audioSources.erase (key);
            audioSourceRefCount.erase (key);
        }
    }

    Edit& edit;
    const ARADocumentControllerInterface* dci;
    ARADocumentControllerRef dcRef;
    std::unique_ptr<MusicalContextWrapper> musicalContext;
    std::map<EditItemID, std::unique_ptr<RegionSequenceWrapper>> regionSequences;
    std::map<EditItemID, int> regionSequencePlaybackRegionCount;
    std::map<juce::String, std::shared_ptr<AudioSourceWrapper>> audioSources;
    std::map<juce::String, int> audioSourceRefCount;
    std::unique_ptr<juce::MemoryBlock> lastArchiveState;
    juce::String lastArchiveDocumentArchiveID;

    //==============================================================================
    /** RAII registration of an editor view bound to this document - a clip
        player's instance or an additional binding like the plugin panel's browser
        instance - for selection fan-out (see ARAClipPlayer::setViewSelection).
        Registers on construction and removes itself on destruction; the document
        must outlive the registration (clip players and plugin bindings are torn
        down before the Edit's ARADocumentHolder). */
    class ScopedEditorView
    {
    public:
        ScopedEditorView (ARADocument& d, const ARAPlugInExtensionInstance& e, ExternalPlugin& p)
            : extension (e), plugin (p), doc (d)
        {
            doc.editorViews.push_back (this);
        }

        ~ScopedEditorView()
        {
            auto& views = doc.editorViews;
            views.erase (std::remove (views.begin(), views.end(), this), views.end());
        }

        const ARAPlugInExtensionInstance& extension;
        ExternalPlugin& plugin;

    private:
        ARADocument& doc;

        JUCE_DECLARE_NON_COPYABLE (ScopedEditorView)
    };

    /** Calls the given function for every registered editor view. */
    void visitEditorViews (const std::function<void (const ARAPlugInExtensionInstance&, ExternalPlugin&)>& fn) const
    {
        for (auto v : editorViews)
            fn (v->extension, v->plugin);
    }

    //==============================================================================
    /** Notifies the plugin that the edit's tempo/key/chord content has changed so
        the musical context is re-read. Driven at document level
        (Edit::sendTempoOrPitchSequenceChangedUpdates → ARADocumentHolder) so it
        also works when no ARA clips exist to forward the change, e.g. a
        browser/panel-only instance (QA 16550). */
    void musicalContextContentChanged()
    {
        if (musicalContext != nullptr)
        {
            const ScopedEdit scope (*this, true);
            musicalContext->update (kARAContentUpdateSignalScopeRemainsUnchanged
                                     | kARAContentUpdateNoteScopeRemainsUnchanged
                                     | kARAContentUpdateTuningScopeRemainsUnchanged);
        }
    }

private:
    std::unique_ptr<ARAInstance> wrapper;
    std::unique_ptr<ARADocumentControllerHostInstance> hostInstance;
    std::unique_ptr<ArchivingState> archivingState;
    std::vector<const ScopedEditorView*> editorViews;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ARADocument)
};

//==============================================================================
struct ARADocumentCreatorCallback : private MessageThreadCallback
{
    ARADocumentCreatorCallback (Edit& e, const juce::PluginDescription& d) : edit (e), desc (d) {}

    static ARADocument* perform (Edit& edit, const juce::PluginDescription& desc)
    {
        CRASH_TRACER
        ARADocumentCreatorCallback adcc (edit, desc);
        adcc.triggerAndWaitForCallback();

        return adcc.result.release();
    }

    Edit& edit;
    juce::PluginDescription desc;
    std::unique_ptr<ARADocument> result;

    void performAction() override
    {
        CRASH_TRACER
        TRACKTION_ASSERT_MESSAGE_THREAD

        result.reset (createDocumentInternal (edit, desc));
    }

    ARADocumentCreatorCallback() = delete;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ARADocumentCreatorCallback)
};

static ARADocument* createDocument (Edit& edit, const juce::PluginDescription& desc)
{
    if (juce::MessageManager::getInstance()->isThisTheMessageThread())
        return createDocumentInternal (edit, desc);

    return ARADocumentCreatorCallback::perform (edit, desc);
}

static ARADocument* createDocumentInternal (Edit& edit, const juce::PluginDescription& desc)
{
    CRASH_TRACER
    TRACKTION_ASSERT_MESSAGE_THREAD

    auto& pluginFactory = ARAPluginFactory::getInstance (edit.engine, desc);
    auto plugin = pluginFactory.createPlugin (edit, desc);

    if (plugin == nullptr || plugin->getAudioPluginInstance() == nullptr)
        return {};

    if (auto factory = pluginFactory.factory)
    {
        static const SizedStruct<ARA_STRUCT_MEMBER (ARAAudioAccessControllerInterface, destroyAudioReader)> audioAccess =
        {
            &AudioSourceWrapper::createAudioReaderForSource,
            &AudioSourceWrapper::readAudioSamples,
            &AudioSourceWrapper::destroyAudioReader
        };

        static const SizedStruct<ARA_STRUCT_MEMBER (ARAArchivingControllerInterface, getDocumentArchiveID)> hostArchiving =
        {
            &ArchivingFunctions::getArchiveSize,
            &ArchivingFunctions::readBytesFromArchive,
            &ArchivingFunctions::writeBytesToArchive,
            &ArchivingFunctions::notifyDocumentArchivingProgress,
            &ArchivingFunctions::notifyDocumentUnarchivingProgress,
            &ArchivingFunctions::getDocumentArchiveID
        };

        static const SizedStruct<ARA_STRUCT_MEMBER (ARAContentAccessControllerInterface, destroyContentReader)>  content =
        {
            &MusicalContextWrapper::isMusicalContextContentAvailable,
            &MusicalContextWrapper::getMusicalContextContentGrade,
            &MusicalContextWrapper::createMusicalContextContentReader,
            &MusicalContextWrapper::isAudioSourceContentAvailable,
            &MusicalContextWrapper::getAudioSourceContentGrade,
            &MusicalContextWrapper::createAudioSourceContentReader,
            &MusicalContextWrapper::getContentReaderEventCount,
            &MusicalContextWrapper::getContentReaderDataForEvent,
            &MusicalContextWrapper::destroyContentReader
        };

        static const SizedStruct<ARA_STRUCT_MEMBER (ARAModelUpdateControllerInterface, notifyPlaybackRegionContentChanged)>  modelUpdating =
        {
            &ModelUpdateFunctions::notifyAudioSourceAnalysisProgress,
            &ModelUpdateFunctions::notifyAudioSourceContentChanged,
            &ModelUpdateFunctions::notifyAudioModificationContentChanged,
            &ModelUpdateFunctions::notifyPlaybackRegionContentChanged
        };

        static const SizedStruct<ARA_STRUCT_MEMBER (ARAPlaybackControllerInterface, requestEnableCycle)>  playback =
        {
            &EditProxyFunctions::requestStartPlayback,
            &EditProxyFunctions::requestStopPlayback,
            &EditProxyFunctions::requestSetPlaybackPosition,
            &EditProxyFunctions::requestSetCycleRange,
            &EditProxyFunctions::requestEnableCycle
        };

        //NB: Can't be a stack object since it doesn't get copied when passed into the document instance!
        std::unique_ptr<ARADocumentControllerHostInstance> hostInstance (new SizedStruct<ARA_STRUCT_MEMBER (ARADocumentControllerHostInstance, playbackControllerInterface)>());

        auto archivingState = std::make_unique<ArchivingState>();
        archivingState->factory = factory;

        hostInstance->audioAccessControllerHostRef      = nullptr;
        hostInstance->audioAccessControllerInterface    = &audioAccess;
        hostInstance->archivingControllerHostRef        = (ARAArchivingControllerHostRef) archivingState.get();
        hostInstance->archivingControllerInterface      = &hostArchiving;
        hostInstance->contentAccessControllerHostRef    = toHostRef (&edit);
        hostInstance->contentAccessControllerInterface  = &content;
        hostInstance->modelUpdateControllerHostRef      = toHostRef (&edit);
        hostInstance->modelUpdateControllerInterface    = &modelUpdating;
        hostInstance->playbackControllerHostRef         = toHostRef (&edit.getTransport());
        hostInstance->playbackControllerInterface       = &playback;

        auto name = edit.getProjectItemRef().toString().trim();

        if (name.isEmpty()) name = edit.getName().trim();
        if (name.isEmpty()) name = getEditFileFromProjectManager (edit).getFullPathName().trim();

        const SizedStruct<ARA_STRUCT_MEMBER (ARADocumentProperties, name)> documentProperties =
        {
            name.toRawUTF8()
        };

        if (auto dci = factory->createDocumentControllerWithDocument (hostInstance.get(), &documentProperties))
        {
            if (auto wrapper = std::unique_ptr<ARAInstance> (pluginFactory.createInstance (*plugin, dci->documentControllerRef)))
            {
                auto d = new ARADocument (edit, wrapper.get(), *wrapper->extensionInstance,
                                          *dci, hostInstance.release(), archivingState.release());
                wrapper.release();
                return d;
            }

            // Binding failed: the document controller must be destroyed, or it leaks
            // holding a pointer to the host instance we're about to free
            dci->documentControllerInterface->destroyDocumentController (dci->documentControllerRef);
        }
    }

    return {};
}

//==============================================================================
class MusicalContextWrapper
{
    struct TimeEventReaderBase;
    ARA_MAP_HOST_REF(TimeEventReaderBase, ARAContentReaderHostRef)

    // we  redeclare the mapping between Edit and ARAMusicalContextHostRef here because
    // otherwise accessing the static functions declared above requires qualified names
    ARA_MAP_HOST_REF(Edit, ARAContentAccessControllerHostRef, ARAMusicalContextHostRef)

public:
    MusicalContextWrapper (ARADocument& doc)  : document (doc)
    {
        CRASH_TRACER
        TRACKTION_ASSERT_MESSAGE_THREAD

        if (document.dci != nullptr)
        {
            const ARADocument::ScopedEdit scope (doc, true);
            updateMusicalContextProperties();
            auto musicalContextProperties = getMusicalContextProperties();
            musicalContextRef = document.dci->createMusicalContext (document.dcRef, toHostRef (&doc.edit), &musicalContextProperties);
        }
    }
    ~MusicalContextWrapper()
    {
        CRASH_TRACER
        TRACKTION_ASSERT_MESSAGE_THREAD

        if (document.dci != nullptr && musicalContextRef != nullptr)
            document.dci->destroyMusicalContext (document.dcRef, musicalContextRef);
    }

    void update (ARAContentUpdateFlags flags = kARAContentUpdateEverythingChanged)
    {
        CRASH_TRACER
        TRACKTION_ASSERT_MESSAGE_THREAD

        // While restoring an archive, beginEditing/endEditing are no-ops, so sending
        // a content update here would land outside an editing cycle
        if (! document.canEdit (true))
            return;

        if (document.dci != nullptr && musicalContextRef != nullptr)
            document.dci->updateMusicalContextContent (document.dcRef, musicalContextRef,
                                                       nullptr, flags);
    }

    /** Converts an edit beat position to ARA quarter notes.
        NB: a tracktion beat is the time-signature denominator note, which is only a
        quarter note in x/4 signatures - ARA positions must always be in quarters. */
    static ARAQuarterPosition beatsToQuarters (Edit& ed, BeatPosition beat)
    {
        tempo::Sequence::Position pos (ed.tempoSequence.getInternalSequence());
        pos.set (ed.tempoSequence.toTime (beat));
        return pos.getPPQTime();
    }

    SizedStruct<ARA_STRUCT_MEMBER (ARAMusicalContextProperties, color)> getMusicalContextProperties()
    {
        return
        {
            nullptr, // name
            0,       // index
            nullptr  // color
        };
    }

    //==============================================================================
    static ARABool ARA_CALL isMusicalContextContentAvailable (ARAContentAccessControllerHostRef editRef,
                                                              ARAMusicalContextHostRef, ARAContentType type)
    {
        if (type == kARAContentTypeSheetChords)
            return ! fromHostRef (editRef)->getChordTrack()->getClips().isEmpty();

        return type == kARAContentTypeTempoEntries
            || type == kARAContentTypeBarSignatures
            || type == kARAContentTypeKeySignatures;
    }

    static ARAContentGrade ARA_CALL getMusicalContextContentGrade (ARAContentAccessControllerHostRef,
                                                                   ARAMusicalContextHostRef, ARAContentType)
    {
        return kARAContentGradeAdjusted;
    }

    static ARAContentReaderHostRef ARA_CALL createMusicalContextContentReader (ARAContentAccessControllerHostRef controllerHostRef,
                                                                           ARAMusicalContextHostRef,
                                                                           ARAContentType type,
                                                                           const ARAContentTimeRange* range)
    {
        if (auto edit = fromHostRef (controllerHostRef))
        {
            switch (type)
            {
                case kARAContentTypeTempoEntries:   return toHostRef (new TempoReader (*edit, range));
                case kARAContentTypeBarSignatures:  return toHostRef (new TimeSigReader (*edit, range));
                case kARAContentTypeKeySignatures:  return toHostRef (new KeySignatureReader (*edit, range));
                case kARAContentTypeSheetChords:    return toHostRef (new ChordReader (*edit, range));
                default: break;
            }
        }

        return {};
    }

    //==============================================================================
    static ARABool ARA_CALL isAudioSourceContentAvailable (ARAContentAccessControllerHostRef,
                                                           ARAAudioSourceHostRef, ARAContentType)
    {
        return kARAFalse;
    }

    static ARAContentGrade ARA_CALL getAudioSourceContentGrade (ARAContentAccessControllerHostRef,
                                                                ARAAudioSourceHostRef, ARAContentType)
    {
        return kARAContentGradeInitial;
    }

    static ARAContentReaderHostRef ARA_CALL createAudioSourceContentReader (ARAContentAccessControllerHostRef,
                                                                            ARAAudioSourceHostRef,
                                                                            ARAContentType,
                                                                            const ARAContentTimeRange*)
    {
        return {};
    }

    //==============================================================================
    static ARAInt32 ARA_CALL getContentReaderEventCount (ARAContentAccessControllerHostRef,
                                                         ARAContentReaderHostRef contentReaderRef)
    {
        CRASH_TRACER

        if (auto t = fromHostRef (contentReaderRef))
            return (ARAInt32) t->getNumEvents();

        return 0;
    }

    static const void* ARA_CALL getContentReaderDataForEvent (ARAContentAccessControllerHostRef,
                                                              ARAContentReaderHostRef contentReaderRef,
                                                              ARAInt32 eventIndex)
    {
        CRASH_TRACER

        if (auto t = fromHostRef (contentReaderRef))
            return t->getDataForEvent ((int) eventIndex);

        return {};
    }

    static void ARA_CALL destroyContentReader (ARAContentAccessControllerHostRef,
                                               ARAContentReaderHostRef contentReaderRef)
    {
        CRASH_TRACER
        delete fromHostRef (contentReaderRef);
    }

    //==============================================================================
    ARADocument& document;
    ARAMusicalContextRef musicalContextRef = {};

private:
    //==============================================================================
    struct TimeEventReaderBase
    {
        virtual ~TimeEventReaderBase() {}
        virtual int getNumEvents() const = 0;
        virtual const void* getDataForEvent (int index) const = 0;
    };

    /** Removes entries whose position doesn't strictly increase over the previous
        entry, keeping the later entry. Plug-ins validate that content events are
        strictly ordered and can assert or abort on duplicates, which zero-length
        progression items or rounding after tempo changes can otherwise produce. */
    template <typename ContentType, typename GetPosition>
    static void removeNonIncreasingPositions (juce::Array<ContentType>& items, GetPosition&& getPosition)
    {
        for (int i = 1; i < items.size();)
        {
            if (getPosition (items.getReference (i)) <= getPosition (items.getReference (i - 1)))
                items.remove (i - 1);
            else
                ++i;
        }
    }

    template <typename ContentType>
    struct TimeEventReaderHelper : public TimeEventReaderBase
    {
        TimeEventReaderHelper() {}

        int getNumEvents() const override    { return items.size(); }

        const void* getDataForEvent (int index) const override
        {
            if (juce::isPositiveAndBelow (index, getNumEvents()))
                return &items.getReference (index);

            return {};
        }

        juce::Array<ContentType> items;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TimeEventReaderHelper)
    };

    struct MidiNoteReader  : public TimeEventReaderHelper<ARAContentNote>
    {
        MidiNoteReader (Edit&, const ARAContentTimeRange*) {}

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiNoteReader)
    };

    struct TimeSigReader  : public TimeEventReaderHelper<ARAContentBarSignature>
    {
        TimeSigReader (Edit& ed, const ARAContentTimeRange* range)
        {
            jassert (ed.tempoSequence.getNumTimeSigs() > 0);

            // compute the range of time signature indices given the specified
            // range, or walk all time signatures if no range is specified
            int beginTimeSig, endTimeSig;

            if (range)
            {
                beginTimeSig = ed.tempoSequence.indexOfTimeSigAt (TimePosition::fromSeconds (range->start));
                endTimeSig = ed.tempoSequence.indexOfTimeSigAt (TimePosition::fromSeconds (range->start + range->duration)) + 1;
            }
            else
            {
                beginTimeSig = 0;
                endTimeSig = ed.tempoSequence.getNumTimeSigs();
            }

            for (int t = beginTimeSig; t < endTimeSig; t++)
            {
                auto timeSig = ed.tempoSequence.getTimeSig (t);
                ARAContentBarSignature item = { timeSig->numerator, timeSig->denominator,
                                                beatsToQuarters (ed, timeSig->getStartBeat()) };
                items.add (item);
            }

            removeNonIncreasingPositions (items, [] (const ARAContentBarSignature& i) { return i.position; });
        }

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TimeSigReader)
    };

    struct TempoReader  : public TimeEventReaderHelper<ARAContentTempoEntry>
    {
        TempoReader (Edit& ed, const ARAContentTimeRange* range)
        {
            TRACKTION_ASSERT_MESSAGE_THREAD
            jassert (ed.tempoSequence.getNumTempos() > 0);

            tempo::Sequence::Position tempoPosition (ed.tempoSequence.getInternalSequence());
            tempoPosition.set (range ? TimePosition::fromSeconds (range->start) : 0_tp);

            // Add first item. NB: ARA positions are quarter notes, not tracktion beats
            {
                ARAContentTempoEntry item = { tempoPosition.getTime().inSeconds(), tempoPosition.getPPQTime() };
                items.add (item);
            }

            bool foundLastTempo = false;

            for (;;)
            {
                foundLastTempo = ! tempoPosition.next();

                if (foundLastTempo)
                    break;

                const auto time = tempoPosition.getTime();

                ARAContentTempoEntry item = { time.inSeconds(), tempoPosition.getPPQTime() };
                items.add (item);

                if (range && time >= TimePosition::fromSeconds (range->start + range->duration))
                    break;
            }

            // if the last tempo setting is included, extrapolate a new entry
            // so that plug-ins can calculate tempo at the range boundary.
            // The stored bpm is quarter notes per minute, so extending by one
            // minute advances the quarter position by exactly the bpm
            if (foundLastTempo)
            {
                auto extrapolatedTempoEntry = items.getLast();
                extrapolatedTempoEntry.timePosition += 60;
                extrapolatedTempoEntry.quarterPosition += ed.tempoSequence.getBpmAt (TimePosition::fromSeconds (items.getLast().timePosition));
                items.add (extrapolatedTempoEntry);
            }

            removeNonIncreasingPositions (items, [] (const ARAContentTempoEntry& i) { return i.quarterPosition; });
        }

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TempoReader)
    };

    // make sure updates are coming when the chord track changes
    struct ChordReader : public TimeEventReaderHelper<ARAContentChord>
    {
        // store chord names in a set to maintain valid UTF8 buffer pointers
        std::set<juce::String> chordNames;

        ChordReader (Edit& ed, const ARAContentTimeRange* range)
        {
            auto chordTrack = ed.getChordTrack();
            jassert (! chordTrack->getClips().isEmpty());

            auto rangeStartBeat = range ? ed.tempoSequence.toBeats (TimePosition::fromSeconds (range->start))
                                        : BeatPosition();
            auto rangeEndBeat = range ? ed.tempoSequence.toBeats (TimePosition::fromSeconds (range->start + range->duration))
                                      : BeatPosition::fromBeats (std::numeric_limits<float>::max());

            // construct a "no chord" for representing gaps in the chord track.
            // All intervals unused marks it as undefined; the name must be left
            // null as it's "the name as displayed in the host", and we don't show one
            ARAContentChord noChord{};

            // Flatten the whole chord track into a single Edit-time progression.
            // This repeats each clip's progression across its full length and
            // resolves gaps/overlaps exactly as chord-track playback does, so a
            // looped/repeated progression is reported in full rather than just
            // the first pass of each clip's pattern.
            auto ptnGen = chordTrack->getClips().getFirst()->getPatternGenerator();
            jassert (ptnGen);

            juce::OwnedArray<PatternGenerator::ProgressionItem> progression;
            ptnGen->getFlattenedChordProgression (progression, true);

            BeatPosition itemStartBeat;

            for (auto itm : progression)
            {
                auto itemEndBeat = itemStartBeat + itm->lengthInBeats;
                auto thisItemStart = itemStartBeat;
                itemStartBeat = itemEndBeat;

                // skip items that don't intersect the requested range
                if (range != nullptr && (itemEndBeat <= rangeStartBeat || thisItemStart >= rangeEndBeat))
                    continue;

                auto position = beatsToQuarters (ed, thisItemStart);

                // an empty chord name marks a gap in the chord track ("no chord")
                if (itm->chordName.get().isEmpty())
                {
                    ARAContentChord noChordCopy = noChord;
                    noChordCopy.position = position;
                    items.add (noChordCopy);
                    continue;
                }

                ARAContentChord item{};

                // The flattened items are detached copies, so resolve the key/scale
                // from the Edit's pitch sequence at the item's Edit-time position
                // (matching how chord-track playback computes them).
                auto& pitch = ed.pitchSequence.getPitchAtBeat (thisItemStart);
                bool sharp = pitch.accidentalsSharp;
                Scale scale (pitch.getScale());
                int rootNote = itm->getRootNote (pitch.getPitch() % 12, scale);
                Chord chord = itm->getChord (scale);
                item.root = MusicalContextFunctions::getCircleOfFifthsIndexforMIDINote (rootNote, sharp);

                // The bass is the lowest note of the chord in its current inversion
                auto invertedSteps = chord.getSteps (itm->inversion);
                auto bassNote = rootNote + (invertedSteps.isEmpty() ? 0 : invertedSteps.getFirst());
                item.bass = MusicalContextFunctions::getCircleOfFifthsIndexforMIDINote (((bassNote % 12) + 12) % 12, sharp);

                auto chordIntervals = MusicalContextFunctions::getChordARAIntervalUsage (chord);
                memcpy (item.intervals, chordIntervals.data(), sizeof (item.intervals));

                auto symbol = juce::MidiMessage::getMidiNoteName (rootNote, sharp, false, 0) + chord.getSymbol();
                item.name = chordNames.insert (MusicalContextFunctions::convertAccidentalsToUnicode (symbol)).first->toRawUTF8();

                item.position = position;
                items.add (item);
            }

            // Zero-length progression items produce two chords at the same position,
            // which ARA plugins reject - keep the later (audible) one
            removeNonIncreasingPositions (items, [] (const ARAContentChord& i) { return i.position; });

            // make sure there is always at least a trailing "no chord"
            if (items.isEmpty())
            {
                noChord.position = beatsToQuarters (ed, rangeStartBeat);
                items.add (noChord);
            }
        }

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChordReader)
    };

    struct KeySignatureReader : public TimeEventReaderHelper<ARAContentKeySignature>
    {
        // store scale names in a set to maintain valid UTF8 buffer pointers
        std::set<juce::String> scaleNames;

        KeySignatureReader (Edit& ed, const ARAContentTimeRange* range)
        {
            jassert (ed.pitchSequence.getNumPitches() > 0);

            // compute the range of time signature indices given the specified
            // range, or walk all time signatures if no range is specified
            int beginKeySig, endKeySig;

            if (range)
            {
                // TODO ARA2: if indexOfPitchAt() was public, we could use that instead
                beginKeySig = ed.pitchSequence.indexOfPitch (&ed.pitchSequence.getPitchAt (TimePosition::fromSeconds (range->start)));
                endKeySig = ed.pitchSequence.indexOfPitch (&ed.pitchSequence.getPitchAt (TimePosition::fromSeconds (range->start + range->duration))) + 1;
            }
            else
            {
                beginKeySig = 0;
                endKeySig = ed.pitchSequence.getNumPitches();
            }

            for (int t = beginKeySig; t < endKeySig; t++)
            {
                auto pitchSetting = ed.pitchSequence.getPitch (t);
                ARAContentKeySignature item{};

                item.root = MusicalContextFunctions::getCircleOfFifthsIndexforMIDINote (pitchSetting->getPitch(), pitchSetting->accidentalsSharp);

                Scale scale (pitchSetting->getScale());

                for (auto s : scale.getSteps())
                    item.intervals[s] = ARA::kARAKeySignatureIntervalUsed;

                auto scaleName = juce::MidiMessage::getMidiNoteName (pitchSetting->getPitch(),
                                                                     pitchSetting->accidentalsSharp, false, 0)
                                    + " " + scale.getName();

                item.name = scaleNames.insert (MusicalContextFunctions::convertAccidentalsToUnicode (scaleName)).first->toRawUTF8();

                item.position = beatsToQuarters (ed, pitchSetting->getStartBeatNumber());
                items.add (item);
            }

            removeNonIncreasingPositions (items, [] (const ARAContentKeySignature& i) { return i.position; });
        }

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KeySignatureReader)
    };

    void updateMusicalContextProperties() {}

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MusicalContextWrapper)
};

//==============================================================================
class NodeReader
{
public:
    NodeReader (const AudioFile& af, bool use64BitSamplesIn)
        : reader (af.engine->getAudioFileManager().cache.createReader (af)),
          use64BitSamples (use64BitSamplesIn)
    {
        if (reader != nullptr)
            buffer.setSize (reader->getNumChannels(), 8096, false, true, true);
    }

    ARABool readAudioSamples (ARASamplePosition samplePosition,
                              ARASampleCount samplesPerChannel,
                              void* const* buffers)
    {
        if (reader == nullptr || buffers == nullptr)
            return kARAFalse;

        const int numChans = buffer.getNumChannels();
        const int numSamples = (int) samplesPerChannel;

        if (buffer.getNumSamples() != numSamples)
            buffer.setSize (numChans, numSamples, false, true, true);

        buffer.clear();

        reader->setReadPosition (samplePosition);
        const auto channels = ChannelConfiguration::canonical (numChans);

        // If the read fails, the cleared buffer is still copied out below, which
        // satisfies ARA's requirement that failed reads fill the buffers with silence
        const bool ok = reader->readSamples (numSamples, buffer, channels, 0, channels, 5000);

        for (int i = 0; i < numChans; ++i)
        {
            if (use64BitSamples)
            {
                auto src = buffer.getReadPointer (i);
                auto dst = (double*) buffers[i];

                for (int j = 0; j < numSamples; ++j)
                    dst[j] = src[j];
            }
            else
            {
                juce::FloatVectorOperations::copy ((float*) buffers[i], buffer.getReadPointer (i), numSamples);
            }
        }

        return ok ? kARATrue : kARAFalse;
    }

    double getSampleRate() const       { return reader != nullptr ? reader->getSampleRate() : 0.0; }
    int getNumChannels() const         { return reader != nullptr ? reader->getNumChannels() : 0; }

private:
    AudioFileCache::Reader::Ptr reader;
    juce::AudioBuffer<float> buffer;
    const bool use64BitSamples;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NodeReader)
};

//==============================================================================
class AudioSourceWrapper
{
public:
    AudioSourceWrapper (ARADocument& d, AudioClipBase& audioClip)
      : doc (d),
        audioFile (audioClip.getAudioFile()),
        sourceID (audioClip.getAudioFile().getHashString())
    {
        CRASH_TRACER
        TRACKTION_ASSERT_MESSAGE_THREAD

        // The cached file info can be stale or missing at this point (e.g. a freshly
        // unpacked project archive) - re-parse it rather than describing the source
        // to the plugin with a zero sample rate, which is invalid per the ARA spec
        if (audioFile.getInfo().sampleRate <= 0.0)
            audioFile.engine->getAudioFileManager().forceFileUpdate (audioFile);

        updateAudioSourceProperties();
        auto audioSourceProperties = getAudioSourceProperties();

        if (audioSourceProperties.sampleRate <= 0.0 || audioSourceProperties.sampleCount <= 0)
        {
            TRACKTION_LOG_ERROR ("ARA: not creating an audio source for an unreadable file: "
                                 + audioFile.getFile().getFullPathName());
            return;
        }

        audioSourceRef = doc.dci->createAudioSource (doc.dcRef, toHostRef (this), &audioSourceProperties);
    }

    ~AudioSourceWrapper()
    {
        CRASH_TRACER
        TRACKTION_ASSERT_MESSAGE_THREAD

        if (audioSourceRef != nullptr)
        {
            enableAccess (false);
            doc.dci->destroyAudioSource (doc.dcRef, audioSourceRef);
        }
    }

    NodeReader* createReader (bool use64BitSamples = false)
    {
        CRASH_TRACER
        TRACKTION_ASSERT_MESSAGE_THREAD
        return new NodeReader (audioFile, use64BitSamples);
    }

    void enableAccess (bool b)
    {
        if (audioSourceRef != nullptr)
            doc.dci->enableAudioSourceSamplesAccess (doc.dcRef, audioSourceRef, b ? kARATrue : kARAFalse);
    }

    void acquireAccess()
    {
        if (++accessRefCount == 1)
            enableAccess (true);
    }

    void releaseAccess()
    {
        // Defensive: an unbalanced release must not disable access for other
        // clips sharing this source
        jassert (accessRefCount > 0);

        if (accessRefCount == 0)
            return;

        if (--accessRefCount == 0)
            enableAccess (false);
    }

    //==============================================================================
    static ARAAudioReaderHostRef ARA_CALL createAudioReaderForSource (ARAAudioAccessControllerHostRef,
                                                                      ARAAudioSourceHostRef hostAudioSourceRef,
                                                                      ARABool use64BitSamples)
    {
        CRASH_TRACER

        if (auto source = fromHostRef (hostAudioSourceRef))
            return (ARAAudioReaderHostRef) source->createReader (use64BitSamples != kARAFalse);

        return {};
    }

    static ARABool ARA_CALL readAudioSamples (ARAAudioAccessControllerHostRef,
                                              ARAAudioReaderHostRef hostReaderRef, ARASamplePosition samplePosition,
                                              ARASampleCount samplesPerChannel, void* const* buffers)
    {
        if (auto node = (NodeReader*) hostReaderRef)
            return node->readAudioSamples (samplePosition, samplesPerChannel, buffers);

        jassertfalse;
        return kARAFalse;
    }

    static void ARA_CALL destroyAudioReader (ARAAudioAccessControllerHostRef,
                                             ARAAudioReaderHostRef hostReaderRef)
    {
        CRASH_TRACER
        delete (NodeReader*) hostReaderRef;
    }

    SizedStruct<ARA_STRUCT_MEMBER (ARAAudioSourceProperties, merits64BitSamples)> getAudioSourceProperties()
    {
        std::unique_ptr<NodeReader> reader (createReader());
        return
        {
            name.toRawUTF8(),
            sourceID.toRawUTF8(),
            (ARASampleCount)audioFile.getLengthInSamples(),
            (ARASampleRate)(reader != nullptr ? reader->getSampleRate() : 0.0),
            (ARAChannelCount)(reader != nullptr ? reader->getNumChannels() : 0),
            kARAFalse //merits64BitSamples
        };
    }

    const juce::String& getPersistentID() const { return sourceID; }

    //==============================================================================
    ARADocument& doc;

    // NB: this wrapper is shared between all clips using the same file, so it must not
    // refer back to the (possibly deleted) clip that created it - take a copy of the file
    const AudioFile audioFile;
    ARAAudioSourceRef audioSourceRef = {};

private:
    void updateAudioSourceProperties()
    {
        name = audioFile.getFile().getFileName();
    }

    const juce::String sourceID;
    juce::String name;
    int accessRefCount = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioSourceWrapper)
};

//==============================================================================
class AudioModificationWrapper
{
public:
    AudioModificationWrapper (ARADocument& d,
                              AudioSourceWrapper& source,
                              const juce::String& itemID,
                              AudioModificationWrapper* instanceToClone)
      : doc (d),
        audioSource (source),
        persistentID (itemID)
    {
        CRASH_TRACER
        TRACKTION_ASSERT_MESSAGE_THREAD

        updateAudioModificationProperties();
        auto audioModificationProperties = getAudioModificationProperties();
        if (instanceToClone != nullptr)
            audioModificationRef = doc.dci->cloneAudioModification (doc.dcRef, instanceToClone->audioModificationRef,
                                                                     toHostRef (&doc.edit), &audioModificationProperties);
        else
            audioModificationRef = doc.dci->createAudioModification (doc.dcRef, audioSource.audioSourceRef,
                                                                     toHostRef (&doc.edit), &audioModificationProperties);
    }
    ~AudioModificationWrapper()
    {
        if (audioModificationRef != nullptr)
            doc.dci->destroyAudioModification (doc.dcRef, audioModificationRef);
    }

    SizedStruct<ARA_STRUCT_MEMBER (ARAAudioModificationProperties, persistentID)> getAudioModificationProperties()
    {
        return
        {
            nullptr,    // name
            persistentID.toRawUTF8()
        };
    }

    const juce::String& getPersistentID() const { return persistentID; }

    ARADocument& doc;
    AudioSourceWrapper& audioSource;
    ARAAudioModificationRef audioModificationRef = nullptr;

private:
    void updateAudioModificationProperties() {}

    const juce::String persistentID;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioModificationWrapper)
};

class RegionSequenceWrapper  : private SelectableListener
{
public:
    RegionSequenceWrapper (ARADocument& d, Track* t) : doc (d), track (t)
    {
        CRASH_TRACER
        TRACKTION_ASSERT_MESSAGE_THREAD

        updateRegionSequenceProperties();
        auto regionSequenceProperties = getRegionSequenceProperties();
        regionSequenceRef = doc.dci->createRegionSequence (doc.dcRef, toHostRef (&doc.edit), &regionSequenceProperties);

        // A track rename/recolour with no accompanying clip change must still
        // reach the plugin, so watch the track itself
        resetSelectableListener (trackListener, track, *this);
    }

    ~RegionSequenceWrapper()
    {
        trackListener.reset();

        if (regionSequenceRef != nullptr)
            doc.dci->destroyRegionSequence (doc.dcRef, regionSequenceRef);
    }

    SizedStruct<ARA_STRUCT_MEMBER (ARARegionSequenceProperties, color)> getRegionSequenceProperties()
    {
        return
        {
            name.toRawUTF8(),
            orderIndex,
            doc.musicalContext->musicalContextRef,
            &colour
        };
    }

    /** Re-sends the track name/order/colour to the plugin.
        Must be called from within a document editing cycle. */
    void updateProperties()
    {
        // The track can be deleted while sibling playback regions are still alive and
        // calling in here, so this must be guarded like selectableObjectChanged is
        if (track != nullptr && regionSequenceRef != nullptr)
        {
            updateRegionSequenceProperties();
            auto props = getRegionSequenceProperties();
            doc.dci->updateRegionSequenceProperties (doc.dcRef, regionSequenceRef, &props);
        }
    }

    ARARegionSequenceRef regionSequenceRef = nullptr;
    ARADocument& doc;
    Track* track;

private:
    void selectableObjectChanged (Selectable*) override
    {
        // Selectable changes arrive asynchronously on the message thread, so it's
        // safe to open an editing cycle here. Tracks change for many reasons besides
        // the properties a region sequence mirrors, so skip no-op notifications.
        if (track == nullptr || regionSequenceRef == nullptr)
            return;

        auto trackColour = track->getColour();
        const ARAColor newColour { trackColour.getFloatRed(), trackColour.getFloatGreen(), trackColour.getFloatBlue() };

        if (name == track->getName()
             && orderIndex == track->getIndexInEditTrackList()
             && colour.r == newColour.r && colour.g == newColour.g && colour.b == newColour.b)
            return;

        if (doc.canEdit (false))
        {
            const ARADocument::ScopedEdit scope (doc, false);
            updateProperties();
        }
    }

    void selectableObjectAboutToBeDeleted (Selectable*) override
    {
        // track is read as data elsewhere in this class, so it must still be nulled
        // here; trackListener unregisters itself when the track goes away
        track = nullptr;
    }

    void updateRegionSequenceProperties()
    {
        name = track->getName();
        orderIndex = track->getIndexInEditTrackList();
        auto trackColour = track->getColour();
        colour = { trackColour.getFloatRed(), trackColour.getFloatGreen(), trackColour.getFloatBlue() };
    }

    SafeScopedListener trackListener;
    int orderIndex;
    juce::String name;
    ARAColor colour;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RegionSequenceWrapper)
};

//==============================================================================
class PlaybackRegionWrapper
{
public:
    /** Creates a playback region covering the whole clip (non-looping case). */
    PlaybackRegionWrapper (ARADocument& d,
                           AudioClipBase& audioClip,
                           const ARAFactory& factory,
                           const AudioModificationWrapper& audioModification)
      : doc (d),
        clip (audioClip),
        trackID (audioClip.getTrack()->itemID),
        supportedFlags (factory.supportedPlaybackTransformationFlags),
        audioModificationRef (audioModification.audioModificationRef)
    {
        CRASH_TRACER
        TRACKTION_ASSERT_MESSAGE_THREAD

        doc.willCreatePlaybackRegionOnTrack (clip.getTrack());

        jassert (d.musicalContext != nullptr && d.musicalContext->musicalContextRef != nullptr);
        updatePlaybackRegionProperties();
        auto playbackRegionProperties = getPlaybackRegionProperties();
        playbackRegionRef = doc.dci->createPlaybackRegion (doc.dcRef,
                                                           audioModificationRef,
                                                           toHostRef (&doc.edit),
                                                           &playbackRegionProperties);
    }

    /** Creates a playback region for one loop iteration of a looping clip.
        The time ranges are recomputed from the current clip state whenever the
        properties are requested, so they stay correct as the clip is moved or resized. */
    PlaybackRegionWrapper (ARADocument& d,
                           AudioClipBase& audioClip,
                           const ARAFactory& factory,
                           const AudioModificationWrapper& audioModification,
                           int loopIterationIndex)
      : doc (d),
        clip (audioClip),
        trackID (audioClip.getTrack()->itemID),
        supportedFlags (factory.supportedPlaybackTransformationFlags),
        audioModificationRef (audioModification.audioModificationRef),
        isLoopIteration (true),
        loopIteration (loopIterationIndex)
    {
        CRASH_TRACER
        TRACKTION_ASSERT_MESSAGE_THREAD

        doc.willCreatePlaybackRegionOnTrack (clip.getTrack());

        jassert (d.musicalContext != nullptr && d.musicalContext->musicalContextRef != nullptr);
        updatePlaybackRegionProperties();
        auto playbackRegionProperties = getPlaybackRegionProperties();
        playbackRegionRef = doc.dci->createPlaybackRegion (doc.dcRef,
                                                           audioModificationRef,
                                                           toHostRef (&doc.edit),
                                                           &playbackRegionProperties);
    }

    ~PlaybackRegionWrapper()
    {
        if (playbackRegionRef != nullptr)
        {
            CRASH_TRACER
            TRACKTION_ASSERT_MESSAGE_THREAD
            doc.dci->destroyPlaybackRegion (doc.dcRef, playbackRegionRef);
            doc.willDestroyPlaybackRegionOnTrack (trackID);
        }
    }

    void updateRange()
    {
        if (playbackRegionRef != nullptr)
        {
            CRASH_TRACER

            // Keep the owning track's name/order/colour in sync too - these are
            // never propagated otherwise
            if (auto seq = doc.regionSequences.find (trackID);
                seq != doc.regionSequences.end() && seq->second != nullptr)
                seq->second->updateProperties();

            updatePlaybackRegionProperties();
            auto playbackRegionProperties = getPlaybackRegionProperties();
            doc.dci->updatePlaybackRegionProperties (doc.dcRef, playbackRegionRef, &playbackRegionProperties);
        }
    }

    ARAPlaybackRegionRef playbackRegionRef = nullptr;

    /** NB: This is where time-stretching is setup */
    SizedStruct<ARA_STRUCT_MEMBER (ARAPlaybackRegionProperties, color)> getPlaybackRegionProperties()
    {
        auto regionSequenceRef = doc.regionSequences[trackID]->regionSequenceRef;

        // Only request the transformations the clip actually needs: plain linear
        // time-stretching when the speed ratio isn't 1. Reflecting-tempo stretch and
        // content-based fades are deliberately not requested even if supported.
        // When the plugin won't be stretching, the modification and playback
        // durations must be equal, so the speed ratio mustn't be applied.
        const bool useStretch = clip.getSpeedRatio() != 1.0
                                 && (supportedFlags & kARAPlaybackTransformationTimestretch) != 0;
        const ARAPlaybackTransformationFlags flags = useStretch ? kARAPlaybackTransformationTimestretch
                                                                : kARAPlaybackTransformationNoChanges;
        const double speedRatio = useStretch ? clip.getSpeedRatio() : 1.0;

        if (isLoopIteration)
        {
            // Recompute this iteration's times from the current clip state so that
            // updateRange() sends fresh values after the clip is moved or resized
            auto pos = clip.getPosition();
            auto loopLengthSecs = clip.getLoopLength().inSeconds();
            auto iterStart = loopIteration * loopLengthSecs;
            auto iterDuration = std::max (0.0, std::min (loopLengthSecs,
                                                         pos.getLength().inSeconds() - iterStart));

            return
            {
                flags,
                clip.getLoopStart().inSeconds() * speedRatio,     // Start in modification time
                iterDuration * speedRatio,                        // Duration in modification time
                pos.getStart().inSeconds() + iterStart,           // Start in playback time
                iterDuration,                                     // Duration in playback time
                doc.musicalContext->musicalContextRef,
                regionSequenceRef,
                name.toRawUTF8(),
                &colour
            };
        }

        auto pos = clip.getPosition();

        return
        {
            flags,
            pos.getOffset().inSeconds() * speedRatio,             // Start in modification time
            pos.getLength().inSeconds() * speedRatio,             // Duration in modification time
            pos.getStart().inSeconds(),                           // Start in playback time
            pos.getLength().inSeconds(),                          // Duration in playback time
            doc.musicalContext->musicalContextRef,
            regionSequenceRef,
            name.toRawUTF8(),
            &colour
        };
    }

    //==============================================================================
    ARADocument& doc;
    AudioClipBase& clip;
    EditItemID trackID;

private:
    void updatePlaybackRegionProperties()
    {
        name = clip.getName();
        auto clipColour = clip.getColour();
        colour = { clipColour.getFloatRed(), clipColour.getFloatGreen(), clipColour.getFloatBlue() };
    }

    juce::String name;
    ARAColor colour;
    const ARAPlaybackTransformationFlags supportedFlags;
    ARAAudioModificationRef audioModificationRef = nullptr;
    const bool isLoopIteration = false;
    const int loopIteration = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlaybackRegionWrapper)
};

//==============================================================================
class PlaybackRegionAndSource
{
public:
    PlaybackRegionAndSource (ARADocument& doc,
                             AudioClipBase& audioClip,
                             const ARAFactory& f,
                             const ARAPlugInExtensionInstance& pluginExtensionInstance,
                             const juce::String& itemID,
                             PlaybackRegionAndSource* instanceToClone)
        : clip (audioClip),
          araDoc (doc),
          araFactory (f),
          pluginInstance (pluginExtensionInstance)
    {
        CRASH_TRACER

        audioSource = doc.acquireAudioSource (audioClip);
        audioSourceKey = audioClip.getAudioFile().getHashString();

        if (audioSource->audioSourceRef != nullptr)
        {
            audioModification = std::make_unique<AudioModificationWrapper> (doc, *audioSource, itemID,
                                                                            instanceToClone != nullptr ? instanceToClone->audioModification.get()
                                                                                                       : nullptr);

            if (audioModification->audioModificationRef != nullptr)
            {
                rebuildPlaybackRegions();
                enable();
                enabledInConstructor = true;

                for (auto& pr : playbackRegions)
                    pr->updateRange();
            }
        }
    }

    /** @note Destruction order matters here */
    ~PlaybackRegionAndSource()
    {
        CRASH_TRACER

        // Only balance the constructor's enable() - if construction failed part-way,
        // an unmatched disable() would steal a sample-access reference from other
        // clips sharing this audio source
        if (enabledInConstructor)
            disable();
        removeAllPlaybackRegions();
        playbackRegions.clear();
        audioModification = nullptr;
        audioSource = nullptr;
        araDoc.releaseAudioSource (audioSourceKey);
    }

    void enable()
    {
        CRASH_TRACER

        if (audioSource != nullptr)
            audioSource->acquireAccess();
    }

    void disable()
    {
        CRASH_TRACER

        if (audioSource != nullptr)
            audioSource->releaseAccess();
    }

    /** Returns the number of playback regions a looping clip of the given length needs.
        Uses a small tolerance so a length sitting on a loop boundary always produces a
        stable count - deriving it any other way (e.g. repeated subtraction) can disagree
        with ceil() by one and make the layout check rebuild the regions endlessly. */
    static size_t getNumLoopRegions (double clipLengthSecs, double loopLengthSecs)
    {
        if (loopLengthSecs <= 0.0)
            return 1;

        auto count = static_cast<ptrdiff_t> (std::ceil ((clipLengthSecs / loopLengthSecs) - 1.0e-9));
        return static_cast<size_t> (std::max<ptrdiff_t> (1, count));
    }

    /** Returns true if the current set of playback regions still matches the given
        clip state, i.e. no rebuild is needed and updateRange() suffices. */
    bool playbackRegionLayoutMatches (bool isLooping, double clipLengthSecs, double loopLengthSecs) const
    {
        if (builtForLooping != (isLooping && loopLengthSecs > 0.0))
            return false;

        if (! builtForLooping)
            return true;

        return playbackRegions.size() == getNumLoopRegions (clipLengthSecs, loopLengthSecs);
    }

    /** Rebuilds all playback regions based on whether the clip is looping.
        If looping, creates one PlaybackRegionWrapper per loop iteration.
        If not looping, creates a single region covering the whole clip. */
    void rebuildPlaybackRegions()
    {
        CRASH_TRACER

        removeAllPlaybackRegions();
        playbackRegions.clear();

        if (audioModification == nullptr || audioModification->audioModificationRef == nullptr)
            return;

        // Snapshot the clip state once so the region count always agrees with
        // playbackRegionLayoutMatches() for the same state
        auto loopLengthSecs = clip.isLooping() ? clip.getLoopLength().inSeconds() : 0.0;
        builtForLooping = loopLengthSecs > 0.0;

        if (builtForLooping)
        {
            auto numRegions = getNumLoopRegions (clip.getPosition().getLength().inSeconds(), loopLengthSecs);

            for (size_t iteration = 0; iteration < numRegions; ++iteration)
            {
                auto pr = std::make_unique<PlaybackRegionWrapper> (araDoc, clip, araFactory, *audioModification,
                                                                   (int) iteration);
                addPlaybackRegion (*pr);
                playbackRegions.push_back (std::move (pr));
            }
        }
        else
        {
            auto pr = std::make_unique<PlaybackRegionWrapper> (araDoc, clip, araFactory, *audioModification);
            addPlaybackRegion (*pr);
            playbackRegions.push_back (std::move (pr));
        }
    }

    /** Returns the first playback region, or nullptr if none exist. */
    PlaybackRegionWrapper* getFirstPlaybackRegion() const
    {
        return playbackRegions.empty() ? nullptr : playbackRegions[0].get();
    }

    /** Returns true if there is at least one playback region. */
    bool hasPlaybackRegions() const { return ! playbackRegions.empty(); }

    std::vector<std::unique_ptr<PlaybackRegionWrapper>> playbackRegions;
    std::shared_ptr<AudioSourceWrapper> audioSource;
    std::unique_ptr<AudioModificationWrapper> audioModification;
    juce::String audioSourceKey;

private:
    AudioClipBase& clip;
    ARADocument& araDoc;
    const ARAFactory& araFactory;
    const ARAPlugInExtensionInstance& pluginInstance;
    bool enabledInConstructor = false;
    bool builtForLooping = false;

    void addPlaybackRegion (PlaybackRegionWrapper& pr)
    {
        CRASH_TRACER

        if (pr.playbackRegionRef != nullptr)
        {
            if (pluginInstance.playbackRendererInterface != nullptr)
                pluginInstance.playbackRendererInterface->addPlaybackRegion (pluginInstance.playbackRendererRef,
                                                                             pr.playbackRegionRef);
            if (pluginInstance.editorRendererInterface != nullptr)
                pluginInstance.editorRendererInterface->addPlaybackRegion (pluginInstance.editorRendererRef,
                                                                           pr.playbackRegionRef);
        }
    }

    void removeAllPlaybackRegions()
    {
        CRASH_TRACER

        for (auto& pr : playbackRegions)
        {
            if (pr != nullptr && pr->playbackRegionRef != nullptr)
            {
                if (pluginInstance.playbackRendererInterface != nullptr)
                    pluginInstance.playbackRendererInterface->removePlaybackRegion (pluginInstance.playbackRendererRef,
                                                                                    pr->playbackRegionRef);
                if (pluginInstance.editorRendererInterface != nullptr)
                    pluginInstance.editorRendererInterface->removePlaybackRegion (pluginInstance.editorRendererRef,
                                                                                  pr->playbackRegionRef);
            }
        }
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlaybackRegionAndSource)
};

