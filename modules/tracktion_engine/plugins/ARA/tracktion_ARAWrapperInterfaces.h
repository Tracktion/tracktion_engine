/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


class MusicalContextWrapper;

/**
    As specified by the Celmony:
    - 1 document per Edit
    - 1 musical context per edit
*/
class ARADocument
{
public:
    ARADocument (Edit& sourceEdit,
                 MelodyneInstance* validPluginWrapper,
                 const ARAPlugInExtensionInstance&,
                 const ARADocumentControllerInstance& dc,
                 ARADocumentControllerHostInstance* dchi)
      : edit (sourceEdit),
        dci (dc.documentControllerInterface),
        dcRef (dc.documentControllerRef),
        wrapper (validPluginWrapper),
        hostInstance (dchi)
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
            beginEditing (true);
            musicalContext = nullptr;
            endEditing (true);
        }

        dci->destroyDocumentController (dcRef);
        wrapper = nullptr;
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

    void flushStateToValueTree (ValueTree& v)
    {
        CRASH_TRACER
        TRACKTION_ASSERT_MESSAGE_THREAD

        MemoryBlock data;
        MemoryOutputStream out (data, false);

        if (dci->storeDocumentToArchive (dcRef, &out) != kARAFalse)
        {
            out.flush();

            if (data.getSize() > 0)
                v.setProperty ("data", data.toBase64Encoding(), nullptr);
        }
    }

    /** @note Must not be editing or already restoring the document while restoring
              from a state.
    */
    void beginRestoringState (const ValueTree& state)
    {
        CRASH_TRACER
        TRACKTION_ASSERT_MESSAGE_THREAD

        jassert (state.hasType (IDs::ARADOCUMENT));

        auto data = state.getProperty ("data").toString();

        if (data.isNotEmpty())
        {
            lastArchiveState = std::make_unique<MemoryBlock>();
            lastArchiveState->fromBase64Encoding (data);

            dci->beginRestoringDocumentFromArchive (dcRef, lastArchiveState.get());
        }
        else
        {
            lastArchiveState = nullptr;
        }
    }

    void endRestoringState()
    {
        CRASH_TRACER
        TRACKTION_ASSERT_MESSAGE_THREAD

        if (lastArchiveState != nullptr)
            dci->endRestoringDocumentFromArchive (dcRef, lastArchiveState.get());

        lastArchiveState = nullptr;
    }

    Edit& edit;
    const ARADocumentControllerInterface* dci;
    ARADocumentControllerRef dcRef;
    std::unique_ptr<MusicalContextWrapper> musicalContext;
    std::unique_ptr<MemoryBlock> lastArchiveState;

private:
    std::unique_ptr<MelodyneInstance> wrapper;
    std::unique_ptr<ARADocumentControllerHostInstance> hostInstance;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ARADocument)
};

//==============================================================================
struct ARADocumentCreatorCallback : private MessageThreadCallback
{
    ARADocumentCreatorCallback (Edit& e) : edit (e) {}

    static ARADocument* perform (Edit& edit)
    {
        CRASH_TRACER
        ARADocumentCreatorCallback adcc (edit);
        adcc.triggerAndWaitForCallback();

        return adcc.result.release();
    }

    Edit& edit;
    std::unique_ptr<ARADocument> result;

    void performAction() override
    {
        CRASH_TRACER
        TRACKTION_ASSERT_MESSAGE_THREAD

        result.reset (createDocumentInternal (edit));
    }

    ARADocumentCreatorCallback() = delete;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ARADocumentCreatorCallback)
};

static ARADocument* createDocument (Edit& edit)
{
    if (MessageManager::getInstance()->isThisTheMessageThread())
        return createDocumentInternal (edit);

    return ARADocumentCreatorCallback::perform (edit);
}

static ARADocument* createDocumentInternal (Edit& edit)
{
    CRASH_TRACER
    TRACKTION_ASSERT_MESSAGE_THREAD

    ReferenceCountedObjectPtr<ExternalPlugin> plugin (MelodyneInstanceFactory::getInstance().createPlugin (edit));

    if (plugin == nullptr || plugin->getAudioPluginInstance() == nullptr)
        return {};

    if (auto factory = MelodyneInstanceFactory::getInstance().factory)
    {
        static const ARAAudioAccessControllerInterface audioAccess =
        {
            kARAAudioAccessControllerInterfaceMinSize,
            &AudioSourceWrapper::createAudioReaderForSource,
            &AudioSourceWrapper::readAudioSamples,
            &AudioSourceWrapper::destroyAudioSourceAccessor
        };

        static const ARAArchivingControllerInterface hostArchiving =
        {
            kARAArchivingControllerInterfaceMinSize,
            &ArchivingFunctions::getArchiveSize,
            &ArchivingFunctions::readBytesFromArchive,
            &ArchivingFunctions::writeBytesToArchive,
            &ArchivingFunctions::notifyDocumentArchivingProgress,
            &ArchivingFunctions::notifyDocumentUnarchivingProgress
        };

        static const ARAContentAccessControllerInterface content =
        {
            kARAContentAccessControllerInterfaceMinSize,
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

        static const ARAModelUpdateControllerInterface modelUpdating =
        {
            kARAModelUpdateControllerInterfaceMinSize,
            &ModelUpdateFunctions::notifyAudioSourceAnalysisProgress,
            &ModelUpdateFunctions::notifyAudioSourceContentChanged,
            &ModelUpdateFunctions::notifyAudioModificationContentChanged
        };

        static const ARAPlaybackControllerInterface playback =
        {
            kARAPlaybackControllerInterfaceMinSize,
            &EditProxyFunctions::requestStartPlayback,
            &EditProxyFunctions::requestStopPlayback,
            &EditProxyFunctions::requestSetPlaybackPosition,
            &EditProxyFunctions::requestSetCycleRange,
            &EditProxyFunctions::requestEnableCycle
        };

        //NB: Can't be a stack object since it doesn't get copied when passed into the document instance!
        std::unique_ptr<ARADocumentControllerHostInstance> hostInstance (new ARADocumentControllerHostInstance());
        hostInstance->structSize                        = kARADocumentControllerHostInstanceMinSize;
        hostInstance->audioAccessControllerHostRef      = nullptr;
        hostInstance->audioAccessControllerInterface    = &audioAccess;
        hostInstance->archivingControllerHostRef        = nullptr;
        hostInstance->archivingControllerInterface      = &hostArchiving;
        hostInstance->contentAccessControllerHostRef    = &edit;
        hostInstance->contentAccessControllerInterface  = &content;
        hostInstance->modelUpdateControllerHostRef      = &edit;
        hostInstance->modelUpdateControllerInterface    = &modelUpdating;
        hostInstance->playbackControllerHostRef         = &edit.getTransport();
        hostInstance->playbackControllerInterface       = &playback;

        auto name = edit.getProjectItemID().toString().trim();

        if (name.isEmpty()) name = edit.getName().trim();
        if (name.isEmpty()) name = getEditFileFromProjectManager (edit).getFullPathName().trim();

        ARADocumentProperties documentProperties =
        {
            kARADocumentPropertiesMinSize,
            name.toRawUTF8()
        };

        if (auto dci = factory->createDocumentControllerWithDocument (hostInstance.get(), &documentProperties))
        {
            if (auto wrapper = std::unique_ptr<MelodyneInstance> (MelodyneInstanceFactory::getInstance()
                                                                    .createInstance (*plugin, dci->documentControllerRef)))
            {
                auto d = new ARADocument (edit, wrapper.get(), *wrapper->extensionInstance,
                                          *dci, hostInstance.release());
                wrapper.release();
                return d;
            }
        }
    }

    return {};
}

//==============================================================================
class MusicalContextWrapper
{
public:
    MusicalContextWrapper (ARADocument& doc)  : document (doc)
    {
        CRASH_TRACER
        TRACKTION_ASSERT_MESSAGE_THREAD

        if (document.dci != nullptr)
        {
            doc.beginEditing (true);
            const ARAMusicalContextProperties musicalContextProperties = { kARAMusicalContextPropertiesMinSize };
            musicalContextRef = document.dci->createMusicalContext (document.dcRef, &doc.edit, &musicalContextProperties);
            doc.endEditing (true);
        }
    }

    ~MusicalContextWrapper()
    {
        CRASH_TRACER
        TRACKTION_ASSERT_MESSAGE_THREAD

        if (document.dci != nullptr && musicalContextRef != nullptr)
            document.dci->destroyMusicalContext (document.dcRef, musicalContextRef);
    }

    void update()
    {
        CRASH_TRACER
        TRACKTION_ASSERT_MESSAGE_THREAD

        if (document.dci != nullptr && musicalContextRef != nullptr)
            document.dci->updateMusicalContextContent (document.dcRef, musicalContextRef,
                                                       nullptr, kARAContentUpdateEverythingChanged);
    }

    //==============================================================================
    static ARABool ARA_CALL isMusicalContextContentAvailable (ARAContentAccessControllerHostRef,
                                                              ARAMusicalContextHostRef, ARAContentType type)
    {
        return type == kARAContentTypeTempoEntries
            || type == kARAContentTypeSignatures;
    }

    static ARAContentGrade ARA_CALL getMusicalContextContentGrade (ARAContentAccessControllerHostRef,
                                                                   ARAMusicalContextHostRef, ARAContentType)
    {
        return kARAContentGradeApproved;
    }

    static ARAContentReaderRef ARA_CALL createMusicalContextContentReader (ARAContentAccessControllerHostRef controllerHostRef,
                                                                           ARAMusicalContextHostRef,
                                                                           ARAContentType type,
                                                                           const ARAContentTimeRange* range)
    {
        if (auto edit = (Edit*) controllerHostRef)
        {
            switch (type)
            {
                case kARAContentTypeTempoEntries:   return new TempoReader (*edit, range);
                case kARAContentTypeSignatures:     return new TimeSigReader (*edit, range);
                default: break;
            }
        }

        return {};
    }

    //==============================================================================
    static ARABool ARA_CALL isAudioSourceContentAvailable (ARAContentAccessControllerHostRef,
                                                           ARAAudioSourceHostRef, ARAContentType type)
    {
        return type == kARAContentTypeNotes;
    }

    static ARAContentGrade ARA_CALL getAudioSourceContentGrade (ARAContentAccessControllerHostRef,
                                                                ARAAudioSourceHostRef, ARAContentType)
    {
        return kARAContentGradeApproved;
    }

    static ARAContentReaderRef ARA_CALL createAudioSourceContentReader (ARAContentAccessControllerHostRef controllerHostRef,
                                                                        ARAAudioSourceHostRef /*audioSourceRef*/,
                                                                        ARAContentType type,
                                                                        const ARAContentTimeRange* range)
    {
        if (auto edit = (Edit*) controllerHostRef)
        {
            switch (type)
            {
                case kARAContentTypeNotes:  return new MidiNoteReader (*edit, range);
                default: break;
            }
        }

        return {};
    }

    //==============================================================================
    static ARAInt32 ARA_CALL getContentReaderEventCount (ARAContentAccessControllerHostRef,
                                                         ARAContentReaderRef contentReaderRef)
    {
        CRASH_TRACER

        if (auto t = (TimeEventReaderBase*) contentReaderRef)
            return (ARAInt32) t->getNumEvents();

        return 0;
    }

    static const void* ARA_CALL getContentReaderDataForEvent (ARAContentAccessControllerHostRef,
                                                              ARAContentReaderRef contentReaderRef, ARAInt32 eventIndex)
    {
        CRASH_TRACER

        if (auto t = (TimeEventReaderBase*) contentReaderRef)
            return t->getDataForEvent ((int) eventIndex);

        return {};
    }

    static void ARA_CALL destroyContentReader (ARAContentAccessControllerHostRef, ARAContentReaderRef contentReaderRef)
    {
        CRASH_TRACER
        delete (TimeEventReaderBase*) contentReaderRef;
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

    template <typename ContentType>
    struct TimeEventReaderHelper : public TimeEventReaderBase
    {
        TimeEventReaderHelper() {}

        int getNumEvents() const override    { return items.size(); }

        const void* getDataForEvent (int index) const override
        {
            if (isPositiveAndBelow (index, getNumEvents()))
                return &items.getReference (index);

            return {};
        }

        Array<ContentType> items;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TimeEventReaderHelper)
    };

    struct MidiNoteReader  : public TimeEventReaderHelper<ARAContentNote>
    {
        MidiNoteReader (Edit&, const ARAContentTimeRange*) {}

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiNoteReader)
    };

    struct TimeSigReader  : public TimeEventReaderHelper<ARAContentSignature>
    {
        TimeSigReader (Edit& ed, const ARAContentTimeRange* range)
        {
            for (auto t : ed.tempoSequence.getTimeSigs())
            {
                auto time = t->getPosition().getStart();

                if (range == nullptr || (time >= range->start && time < range->start + range->duration))
                {
                    ARAContentSignature item = { t->numerator, t->denominator, (ARAQuarterPosition) t->getStartBeat() };
                    items.add (item);
                }
            }
        }

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TimeSigReader)
    };

    struct TempoReader  : public TimeEventReaderHelper<ARAContentTempoEntry>
    {
        TempoReader (Edit& ed, const ARAContentTimeRange* range)
        {
            TempoSequencePosition seq (ed.tempoSequence);

            const double length = jmax (ed.getLength(), 30.0);

            for (;;)
            {
                const double time = seq.getTime();

                if (time > length)
                    break;

                const double ppq = seq.getPPQTime();

                if (range == nullptr || (time >= range->start && time < range->start + range->duration))
                {
                    ARAContentTempoEntry item = { time, ppq };
                    items.add (item);
                }

                seq.addBeats (1.0);
            }
        }

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TempoReader)
    };

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MusicalContextWrapper)
};

//==============================================================================
class NodeReader
{
public:
    NodeReader (const AudioFile& af)
        : reader (Engine::getInstance().getAudioFileManager().cache.createReader (af))
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
        reader->readSamples (numSamples, buffer, AudioChannelSet::stereo(), 0, AudioChannelSet::stereo(), 5000);

        for (int i = 0; i < numChans; ++i)
            FloatVectorOperations::copy ((float*) buffers[i], buffer.getReadPointer (i), numSamples);

        return kARATrue;
    }

    double getSampleRate() const       { return reader != nullptr ? reader->getSampleRate() : 0.0; }
    int getNumChannels() const         { return reader != nullptr ? reader->getNumChannels() : 0; }

private:
    AudioFileCache::Reader::Ptr reader;
    juce::AudioBuffer<float> buffer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NodeReader)
};

//==============================================================================
class AudioSourceWrapper
{
public:
    AudioSourceWrapper (ARADocument& d, AudioClipBase& audioClip)
      : doc (d),
        clip (audioClip),
        sourceName (audioClip.getAudioFile().getFile().getFileName()),
        itemID (audioClip.getAudioFile().getHashString() + "_" + audioClip.itemID.toString()),
        lengthSamples (audioClip.getAudioFile().getLengthInSamples())
    {
        CRASH_TRACER
        TRACKTION_ASSERT_MESSAGE_THREAD

        {
            std::unique_ptr<NodeReader> reader (createReader());

            ARAAudioSourceProperties props =
            {
                kARAAudioSourcePropertiesMinSize,
                sourceName.toRawUTF8(),
                itemID.toRawUTF8(),
                (ARASampleCount) lengthSamples,
                (ARASampleRate) (reader != nullptr ? reader->getSampleRate() : 0.0),
                (ARAChannelCount) (reader != nullptr ? reader->getNumChannels() : 0),
                kARAFalse //merits64BitSamples
            };

            audioSourceProperties = props;
        }

        audioSourceRef = doc.dci->createAudioSource (doc.dcRef, this, &audioSourceProperties);
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

    NodeReader* createReader()
    {
        CRASH_TRACER
        TRACKTION_ASSERT_MESSAGE_THREAD
        return new NodeReader (clip.getAudioFile());
    }

    void enableAccess (bool b)
    {
        if (audioSourceRef != nullptr)
            doc.dci->enableAudioSourceSamplesAccess (doc.dcRef, audioSourceRef, b ? kARATrue : kARAFalse);
    }

    //==============================================================================
    static ARAAudioReaderHostRef ARA_CALL createAudioReaderForSource (ARAAudioAccessControllerHostRef,
                                                                      ARAAudioSourceHostRef hostAudioSourceRef,
                                                                      ARABool)
    {
        CRASH_TRACER

        if (auto source = (AudioSourceWrapper*) hostAudioSourceRef)
            return (ARAAudioReaderHostRef*) source->createReader();

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

    static void ARA_CALL destroyAudioSourceAccessor (ARAAudioAccessControllerHostRef,
                                                     ARAAudioReaderHostRef hostReaderRef)
    {
        CRASH_TRACER
        delete (NodeReader*) hostReaderRef;
    }

    //==============================================================================
    ARADocument& doc;
    AudioClipBase& clip;
    ARAAudioSourceRef audioSourceRef = {};
    ARAAudioSourceProperties audioSourceProperties;
    const String sourceName, itemID;

private:
    const int64 lengthSamples;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioSourceWrapper)
};

//==============================================================================
class AudioModificationWrapper
{
public:
    AudioModificationWrapper (ARADocument& d,
                              const AudioSourceWrapper& audioSource,
                              const String& itemID,
                              AudioModificationWrapper* instanceToClone)
      : doc (d),
        name (audioSource.sourceName),
        persistentID (itemID)
    {
        CRASH_TRACER
        TRACKTION_ASSERT_MESSAGE_THREAD

        ARAAudioModificationProperties props =
        {
            kARAAudioModificationPropertiesMinSize,
            name.toRawUTF8(),
            persistentID.toRawUTF8()
        };

        audioModificationProperties = props;

        if (instanceToClone != nullptr)
            audioModificationRef = doc.dci->cloneAudioModification (doc.dcRef, instanceToClone->audioModificationRef,
                                                                     &doc.edit, &audioModificationProperties);
        else
            audioModificationRef = doc.dci->createAudioModification (doc.dcRef, audioSource.audioSourceRef,
                                                                     &doc.edit, &audioModificationProperties);
    }

    ~AudioModificationWrapper()
    {
        if (audioModificationRef != nullptr)
            doc.dci->destroyAudioModification (doc.dcRef, audioModificationRef);
    }

    ARADocument& doc;
    ARAAudioModificationProperties audioModificationProperties;
    ARAAudioModificationRef audioModificationRef = nullptr;
    const String name, persistentID;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioModificationWrapper)
};

//==============================================================================
class PlaybackRegionWrapper
{
public:
    PlaybackRegionWrapper (ARADocument& d,
                           AudioClipBase& audioClip,
                           const ARAFactory& factory,
                           const AudioModificationWrapper& audioModification)
      : doc (d),
        clip (audioClip),
        flags (factory.supportedPlaybackTransformationFlags)
    {
        CRASH_TRACER
        TRACKTION_ASSERT_MESSAGE_THREAD

        jassert (d.musicalContext != nullptr && d.musicalContext->musicalContextRef != nullptr);
        updatePlaybackProperties (d.musicalContext->musicalContextRef);

        playbackRegionRef = doc.dci->createPlaybackRegion (doc.dcRef,
                                                           audioModification.audioModificationRef,
                                                           &doc.edit,
                                                           &playbackRegionProperties);
    }

    ~PlaybackRegionWrapper()
    {
        if (playbackRegionRef != nullptr)
        {
            CRASH_TRACER
            TRACKTION_ASSERT_MESSAGE_THREAD
            doc.dci->destroyPlaybackRegion (doc.dcRef, playbackRegionRef);
        }
    }

    void updateRange()
    {
        if (playbackRegionRef != nullptr)
        {
            CRASH_TRACER
            updatePlaybackProperties (playbackRegionProperties.musicalContextRef);
            doc.dci->updatePlaybackRegionProperties (doc.dcRef, playbackRegionRef, &playbackRegionProperties);
        }
    }

    ARAPlaybackRegionRef playbackRegionRef = nullptr;

private:
    ARADocument& doc;
    AudioClipBase& clip;
    const ARAPlaybackTransformationFlags flags;
    ARAPlaybackRegionProperties playbackRegionProperties;

    /** NB: This is where time-stretching is setup */
    void updatePlaybackProperties (ARAMusicalContextRef musicalContextRef)
    {
        auto pos = clip.getPosition();

        ARAPlaybackRegionProperties props =
        {
            kARAPlaybackRegionPropertiesMinSize,
            flags,
            pos.getOffset() * clip.getSpeedRatio(),   // Start in modification time
            pos.getLength() * clip.getSpeedRatio(),   // Duration in modification time
            pos.getStart(),                           // Start in playback time
            pos.getLength(),                          // Duration in playback time
            musicalContextRef
        };

        jassert (props.startInPlaybackTime >= 0.0);
        jassert (props.durationInPlaybackTime >= 0.0);
        jassert (props.startInModificationTime >= 0.0);
        jassert (props.durationInModificationTime >= 0.0);

        playbackRegionProperties = props;
    }

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
                             const String& itemID,
                             PlaybackRegionAndSource* instanceToClone)
        : pluginInstance (pluginExtensionInstance)
    {
        CRASH_TRACER

        audioSource = std::make_unique<AudioSourceWrapper> (doc, audioClip);

        if (audioSource->audioSourceRef != nullptr)
        {
            audioModification = std::make_unique<AudioModificationWrapper> (doc, *audioSource, itemID,
                                                                            instanceToClone != nullptr ? instanceToClone->audioModification.get()
                                                                                                       : nullptr);

            if (audioModification->audioModificationRef != nullptr)
            {
                playbackRegion = std::make_unique<PlaybackRegionWrapper> (doc, audioClip, f, *audioModification);
                setPlaybackRegion();

                enable();
                playbackRegion->updateRange();
            }
        }
    }

    /** @note Destruction order matters here */
    ~PlaybackRegionAndSource()
    {
        CRASH_TRACER
        disable();
        removePlaybackRegion();
        playbackRegion = nullptr;
        audioModification = nullptr;
        audioSource = nullptr;
    }

    void enable()
    {
        CRASH_TRACER

        if (audioSource != nullptr)
            audioSource->enableAccess (true);
    }

    void disable()
    {
        CRASH_TRACER

        if (audioSource != nullptr)
            audioSource->enableAccess (false);
    }

    std::unique_ptr<PlaybackRegionWrapper> playbackRegion;
    std::unique_ptr<AudioSourceWrapper> audioSource;

private:
    const ARAPlugInExtensionInstance& pluginInstance;
    std::unique_ptr<AudioModificationWrapper> audioModification;

    const ARAPlugInExtensionInterface& getInterface() noexcept
    {
        jassert (pluginInstance.plugInExtensionInterface != nullptr);
        return *pluginInstance.plugInExtensionInterface;
    }

    void setPlaybackRegion()
    {
        CRASH_TRACER

        if (playbackRegion != nullptr && playbackRegion->playbackRegionRef != nullptr)
            getInterface().setPlaybackRegion (pluginInstance.plugInExtensionRef, playbackRegion->playbackRegionRef);
    }

    void removePlaybackRegion()
    {
        CRASH_TRACER

        if (playbackRegion != nullptr && playbackRegion->playbackRegionRef != nullptr)
            getInterface().removePlaybackRegion (pluginInstance.plugInExtensionRef, playbackRegion->playbackRegionRef);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlaybackRegionAndSource)
};
