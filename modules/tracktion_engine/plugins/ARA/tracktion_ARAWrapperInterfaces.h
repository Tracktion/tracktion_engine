/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
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
            regionSequences.clear();
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

        if (dci->storeDocumentToArchive (dcRef, toHostRef (&out)) != kARAFalse)
        {
            out.flush();

            if (data.getSize() > 0)
                v.setProperty ("data", data.toBase64Encoding(), nullptr);
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

        if (data.isNotEmpty())
        {
            lastArchiveState = std::make_unique<MemoryBlock>();
            lastArchiveState->fromBase64Encoding (data);

            dci->beginRestoringDocumentFromArchive (dcRef, toHostRef (lastArchiveState.get()));
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
            dci->endRestoringDocumentFromArchive (dcRef, toHostRef (lastArchiveState.get()));

        lastArchiveState = nullptr;
    }

    void willCreatePlaybackRegionOnTrack (Track* track) 
    {
        if (regionSequences.count (track) == 0)
            regionSequences[track] = std::make_unique<RegionSequenceWrapper> (*this, track);
        regionSequencePlaybackRegionCount[track]++;
    }

    void willDestroyPlaybackRegionOnTrack (Track* track)
    {
        jassert (regionSequencePlaybackRegionCount.count (track) > 0);
        if (--regionSequencePlaybackRegionCount[track] == 0)
        {
            regionSequences.erase (track);
            regionSequencePlaybackRegionCount.erase (track);
        }
    }

    Edit& edit;
    const ARADocumentControllerInterface* dci;
    ARADocumentControllerRef dcRef;
    std::unique_ptr<MusicalContextWrapper> musicalContext;
    std::map<Track*, std::unique_ptr<RegionSequenceWrapper>> regionSequences;
    std::map<Track*, int> regionSequencePlaybackRegionCount;
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
        hostInstance->contentAccessControllerHostRef    = toHostRef (&edit);
        hostInstance->contentAccessControllerInterface  = &content;
        hostInstance->modelUpdateControllerHostRef      = toHostRef (&edit);
        hostInstance->modelUpdateControllerInterface    = &modelUpdating;
        hostInstance->playbackControllerHostRef         = toHostRef (&edit.getTransport());
        hostInstance->playbackControllerInterface       = &playback;

        auto name = edit.getProjectItemID().toString().trim();

        if (name.isEmpty()) name = edit.getName().trim();
        if (name.isEmpty()) name = getEditFileFromProjectManager (edit).getFullPathName().trim();

        const ARA::Host::Properties<ARA_MEMBER_PTR_ARGS (ARADocumentProperties, name)> documentProperties =
        {
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
            doc.beginEditing (true);
            updateMusicalContextProperties ();
            auto musicalContextProperties = getMusicalContextProperties();
            musicalContextRef = document.dci->createMusicalContext (document.dcRef, toHostRef (&doc.edit), &musicalContextProperties);
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
    
    ARA::Host::Properties<ARA_MEMBER_PTR_ARGS (ARAMusicalContextProperties, color)> getMusicalContextProperties()
    {
        return 
        { 
            nullptr, // name
            0,       // index
            nullptr  // color
        };
    }

    //==============================================================================
    static ARABool ARA_CALL isMusicalContextContentAvailable (ARAContentAccessControllerHostRef,
                                                              ARAMusicalContextHostRef, ARAContentType type)
    {
        return type == kARAContentTypeTempoEntries
            || type == kARAContentTypeBarSignatures;
    }

    static ARAContentGrade ARA_CALL getMusicalContextContentGrade (ARAContentAccessControllerHostRef,
                                                                   ARAMusicalContextHostRef, ARAContentType)
    {
        return kARAContentGradeApproved;
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
                case kARAContentTypeBarSignatures:     return toHostRef (new TimeSigReader (*edit, range));
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

    static ARAContentReaderHostRef ARA_CALL createAudioSourceContentReader (ARAContentAccessControllerHostRef controllerHostRef,
                                                                        ARAAudioSourceHostRef /*audioSourceRef*/,
                                                                        ARAContentType type,
                                                                        const ARAContentTimeRange* range)
    {
        if (auto edit = fromHostRef (controllerHostRef))
        {
            switch (type)
            {
                case kARAContentTypeNotes:  return toHostRef (new MidiNoteReader (*edit, range));
                default: break;
            }
        }

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
                                                              ARAContentReaderHostRef contentReaderRef, ARAInt32 eventIndex)
    {
        CRASH_TRACER

        if (auto t = fromHostRef (contentReaderRef))
            return t->getDataForEvent ((int) eventIndex);

        return {};
    }

    static void ARA_CALL destroyContentReader (ARAContentAccessControllerHostRef, ARAContentReaderHostRef contentReaderRef)
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

    struct TimeSigReader  : public TimeEventReaderHelper<ARAContentBarSignature>
    {
        TimeSigReader (Edit& ed, const ARAContentTimeRange* range)
        {
            for (auto t : ed.tempoSequence.getTimeSigs())
            {
                auto time = t->getPosition().getStart();

                if (range == nullptr || (time >= range->start && time < range->start + range->duration))
                {
                    ARAContentBarSignature item = { t->numerator, t->denominator, (ARAQuarterPosition) t->getStartBeat() };
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

    // TODO ARA2 does anything need to be updated here?
    void updateMusicalContextProperties () {}

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
        itemID (audioClip.getAudioFile().getHashString() + "_" + audioClip.itemID.toString())
    {
        CRASH_TRACER
        TRACKTION_ASSERT_MESSAGE_THREAD

        updateAudioSourceProperties();
        auto audioSourceProperties = getAudioSourceProperties();
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

        if (auto source = fromHostRef (hostAudioSourceRef))
            return (ARAAudioReaderHostRef) source->createReader();

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

    ARA::Host::Properties<ARA_MEMBER_PTR_ARGS (ARAAudioSourceProperties, merits64BitSamples)> getAudioSourceProperties()
    {
        std::unique_ptr<NodeReader> reader (createReader());
        return
        {
            name.toRawUTF8(),
            itemID.toRawUTF8(),
            (ARASampleCount)clip.getAudioFile().getLengthInSamples(),
            (ARASampleRate)(reader != nullptr ? reader->getSampleRate() : 0.0),
            (ARAChannelCount)(reader != nullptr ? reader->getNumChannels() : 0),
            kARAFalse //merits64BitSamples
        };
    }

    //==============================================================================
    ARADocument& doc;
    AudioClipBase& clip;
    ARAAudioSourceRef audioSourceRef = {};

private:
    void updateAudioSourceProperties ()
    {
        name = clip.getAudioFile ().getFile ().getFileName ();
    }

    const String itemID;
    String name;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioSourceWrapper)
};

//==============================================================================
class AudioModificationWrapper
{
public:
    AudioModificationWrapper (ARADocument& d,
                              AudioSourceWrapper& source,
                              const String& itemID,
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

    ARA::Host::Properties<ARA_MEMBER_PTR_ARGS (ARAAudioModificationProperties, persistentID)> getAudioModificationProperties()
    {
        return
        {
            name.toRawUTF8(),
            persistentID.toRawUTF8()
        };
    }

    ARADocument& doc;
    AudioSourceWrapper& audioSource;
    ARAAudioModificationRef audioModificationRef = nullptr;
    
private:
    void updateAudioModificationProperties ()
    {
        // TODO ARA2 do the audio source properties need to be updated?
        auto audioSourceProperties = audioSource.getAudioSourceProperties();
        name = audioSourceProperties.name;
    }

    String name;
    const String persistentID;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioModificationWrapper)
};

class RegionSequenceWrapper
{
public:
    RegionSequenceWrapper(ARADocument& d, Track* track_)
        : doc (d),
          track (track_)
    {
        CRASH_TRACER
        TRACKTION_ASSERT_MESSAGE_THREAD

        updateRegionSequenceProperties();
        auto regionSequenceProperties = getRegionSequenceProperties();
        regionSequenceRef = doc.dci->createRegionSequence (doc.dcRef, toHostRef (&doc.edit), &regionSequenceProperties);
    }
    ~RegionSequenceWrapper()
    {
        if (regionSequenceRef != nullptr)
            doc.dci->destroyRegionSequence (doc.dcRef, regionSequenceRef);
    }

    ARA::Host::Properties<ARA_MEMBER_PTR_ARGS (ARARegionSequenceProperties, color)> getRegionSequenceProperties()
    {
        return
        {
            name.toRawUTF8(),
            orderIndex,
            doc.musicalContext->musicalContextRef,
            &colour
        };
    }

    ARARegionSequenceRef regionSequenceRef = nullptr;

    ARADocument& doc;
    Track* track;

private:
    void updateRegionSequenceProperties ()
    {
        name = track->getName();
        orderIndex = track->getIndexInEditTrackList();
        Colour trackColour = track->getColour();
        colour = { trackColour.getFloatRed(), trackColour.getFloatGreen(), trackColour.getFloatBlue() };
    }

    int orderIndex;
    String name;
    ARAColor colour;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RegionSequenceWrapper)
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

        doc.willCreatePlaybackRegionOnTrack (clip.getTrack());

        jassert (d.musicalContext != nullptr && d.musicalContext->musicalContextRef != nullptr);
        updatePlaybackRegionProperties();
        auto playbackRegionProperties = getPlaybackRegionProperties();
        playbackRegionRef = doc.dci->createPlaybackRegion (doc.dcRef,
                                                           audioModification.audioModificationRef,
                                                           toHostRef (&doc.edit),
                                                           &playbackRegionProperties);
    }

    ~PlaybackRegionWrapper()
    {
        if (playbackRegionRef != nullptr)
        {
            CRASH_TRACER
            TRACKTION_ASSERT_MESSAGE_THREAD
            // TODO ARA2
            // At this point the track has already been destroyed, so this
            // function won't work properly. What to do?
            doc.willDestroyPlaybackRegionOnTrack (clip.getTrack());
            doc.dci->destroyPlaybackRegion (doc.dcRef, playbackRegionRef);
        }
    }

    void updateRange()
    {
        if (playbackRegionRef != nullptr)
        {
            CRASH_TRACER
            
            updatePlaybackRegionProperties();
            auto playbackRegionProperties = getPlaybackRegionProperties();
            doc.dci->updatePlaybackRegionProperties (doc.dcRef, playbackRegionRef, &playbackRegionProperties);
        }
    }

    ARAPlaybackRegionRef playbackRegionRef = nullptr;

    /** NB: This is where time-stretching is setup */
    ARA::Host::Properties<ARA_MEMBER_PTR_ARGS (ARAPlaybackRegionProperties, color)> getPlaybackRegionProperties()
    {
        auto regionSequenceRef = doc.regionSequences[clip.getTrack()]->regionSequenceRef;
        auto pos = clip.getPosition();

        return
        {
            flags,
            pos.getOffset() * clip.getSpeedRatio(),   // Start in modification time
            pos.getLength() * clip.getSpeedRatio(),   // Duration in modification time
            pos.getStart(),                           // Start in playback time
            pos.getLength(),                          // Duration in playback time
            doc.musicalContext->musicalContextRef,
            regionSequenceRef,
            name.toRawUTF8(),
            &colour
        };
    }

    //==============================================================================
    ARADocument& doc;
    AudioClipBase& clip;

private:
    void updatePlaybackRegionProperties ()
    {
        name = clip.getName();
        Colour clipColour = clip.getColour();
        colour = { clipColour.getFloatRed(), clipColour.getFloatGreen(), clipColour.getFloatBlue() };
    }

    String name;
    ARAColor colour;
    const ARAPlaybackTransformationFlags flags;

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

    bool supportsARAPlugInInstanceRoles() const
    {
        return (ARA_IMPLEMENTED_STRUCT_SIZE(ARAPlugInExtensionInstance, plugInExtensionInterface) < pluginInstance.structSize);
    }

    void setPlaybackRegion()
    {
        CRASH_TRACER

        if (playbackRegion != nullptr && playbackRegion->playbackRegionRef != nullptr)
        {
            if (supportsARAPlugInInstanceRoles())
            {
                if (pluginInstance.playbackRendererInterface != nullptr)
                    pluginInstance.playbackRendererInterface->addPlaybackRegion (pluginInstance.playbackRendererRef, playbackRegion->playbackRegionRef);
                if (pluginInstance.editorRendererInterface != nullptr)
                    pluginInstance.editorRendererInterface->addPlaybackRegion (pluginInstance.editorRendererRef, playbackRegion->playbackRegionRef);
            }
            else if (pluginInstance.plugInExtensionInterface != nullptr)
            {
                pluginInstance.plugInExtensionInterface->setPlaybackRegion (pluginInstance.plugInExtensionRef, playbackRegion->playbackRegionRef);
            }
        }
    }

    void removePlaybackRegion()
    {
        CRASH_TRACER

        if (playbackRegion != nullptr && playbackRegion->playbackRegionRef != nullptr)
        {
            if (supportsARAPlugInInstanceRoles())
            {
                if (pluginInstance.playbackRendererInterface != nullptr)
                    pluginInstance.playbackRendererInterface->removePlaybackRegion (pluginInstance.playbackRendererRef, playbackRegion->playbackRegionRef);
                if (pluginInstance.editorRendererInterface != nullptr)
                    pluginInstance.editorRendererInterface->removePlaybackRegion (pluginInstance.editorRendererRef, playbackRegion->playbackRegionRef);
            }
            else if (pluginInstance.plugInExtensionInterface != nullptr)
            {
                pluginInstance.plugInExtensionInterface->removePlaybackRegion (pluginInstance.plugInExtensionRef, playbackRegion->playbackRegionRef);
            }
        }
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlaybackRegionAndSource)
};
