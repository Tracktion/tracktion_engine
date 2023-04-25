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

    void flushStateToValueTree (juce::ValueTree& v)
    {
        CRASH_TRACER
        TRACKTION_ASSERT_MESSAGE_THREAD

        juce::MemoryBlock data;
        juce::MemoryOutputStream out (data, false);

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
            lastArchiveState = std::make_unique<juce::MemoryBlock>();
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
    std::unique_ptr<juce::MemoryBlock> lastArchiveState;

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
    if (juce::MessageManager::getInstance()->isThisTheMessageThread())
        return createDocumentInternal (edit);

    return ARADocumentCreatorCallback::perform (edit);
}

static ARADocument* createDocumentInternal (Edit& edit)
{
    CRASH_TRACER
    TRACKTION_ASSERT_MESSAGE_THREAD

    auto plugin = MelodyneInstanceFactory::getInstance (edit.engine).createPlugin (edit);

    if (plugin == nullptr || plugin->getAudioPluginInstance() == nullptr)
        return {};

    if (auto factory = MelodyneInstanceFactory::getInstance (edit.engine).factory)
    {
        static const SizedStruct<ARA_MEMBER_PTR_ARGS (ARAAudioAccessControllerInterface, destroyAudioReader)> audioAccess =
        {
            &AudioSourceWrapper::createAudioReaderForSource,
            &AudioSourceWrapper::readAudioSamples,
            &AudioSourceWrapper::destroyAudioReader
        };

        static const SizedStruct<ARA_MEMBER_PTR_ARGS (ARAArchivingControllerInterface, notifyDocumentUnarchivingProgress)> hostArchiving =
        {
            &ArchivingFunctions::getArchiveSize,
            &ArchivingFunctions::readBytesFromArchive,
            &ArchivingFunctions::writeBytesToArchive,
            &ArchivingFunctions::notifyDocumentArchivingProgress,
            &ArchivingFunctions::notifyDocumentUnarchivingProgress
        };

        static const SizedStruct<ARA_MEMBER_PTR_ARGS (ARAContentAccessControllerInterface, destroyContentReader)>  content =
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

        static const SizedStruct<ARA_MEMBER_PTR_ARGS (ARAModelUpdateControllerInterface, notifyAudioModificationContentChanged)>  modelUpdating =
        {
            &ModelUpdateFunctions::notifyAudioSourceAnalysisProgress,
            &ModelUpdateFunctions::notifyAudioSourceContentChanged,
            &ModelUpdateFunctions::notifyAudioModificationContentChanged
        };

        static const SizedStruct<ARA_MEMBER_PTR_ARGS (ARAPlaybackControllerInterface, requestEnableCycle)>  playback =
        {
            &EditProxyFunctions::requestStartPlayback,
            &EditProxyFunctions::requestStopPlayback,
            &EditProxyFunctions::requestSetPlaybackPosition,
            &EditProxyFunctions::requestSetCycleRange,
            &EditProxyFunctions::requestEnableCycle
        };

        //NB: Can't be a stack object since it doesn't get copied when passed into the document instance!
        std::unique_ptr<ARADocumentControllerHostInstance> hostInstance (new SizedStruct<ARA_MEMBER_PTR_ARGS (ARADocumentControllerHostInstance, playbackControllerInterface)>());
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

        const SizedStruct<ARA_MEMBER_PTR_ARGS (ARADocumentProperties, name)> documentProperties =
        {
            name.toRawUTF8()
        };

        if (auto dci = factory->createDocumentControllerWithDocument (hostInstance.get(), &documentProperties))
        {
            if (auto wrapper = std::unique_ptr<MelodyneInstance> (MelodyneInstanceFactory::getInstance (edit.engine)
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
            updateMusicalContextProperties();
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
    
    SizedStruct<ARA_MEMBER_PTR_ARGS (ARAMusicalContextProperties, color)> getMusicalContextProperties()
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
                ARAContentBarSignature item = { timeSig->numerator, timeSig->denominator, (ARAQuarterPosition)timeSig->getStartBeat().inBeats() };
                items.add (item);
            }
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

            // Add first item
            {
                ARAContentTempoEntry item = { tempoPosition.getTime().inSeconds(), tempoPosition.getBeats().inBeats() };
                items.add (item);
            }

            bool foundLastTempo = false;

            for (;;)
            {
                foundLastTempo = ! tempoPosition.next();

                if (foundLastTempo)
                    break;

                const auto time = tempoPosition.getTime();

                ARAContentTempoEntry item = { time.inSeconds(), tempoPosition.getBeats().inBeats() };
                items.add (item);

                if (range && time >= TimePosition::fromSeconds (range->start + range->duration))
                    break;
            }

            // if the last tempo setting is included, extrapolate a new entry 
            // so that plug-ins can calculate tempo at the range boundary
            if (foundLastTempo)
            {
                auto extrapolatedTempoEntry = items.getLast();
                extrapolatedTempoEntry.timePosition += 60;
                extrapolatedTempoEntry.quarterPosition += ed.tempoSequence.getBpmAt (TimePosition::fromSeconds (items.getLast().timePosition));
                items.add (extrapolatedTempoEntry);
            }
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

            auto rangeStartBeat = range ? ed.tempoSequence.toBeats (TimePosition::fromSeconds (range->start)) : BeatPosition::fromBeats (-std::numeric_limits<float>::max());
            auto rangeEndBeat = range ? ed.tempoSequence.toBeats (TimePosition::fromSeconds (range->start + range->duration)) : BeatPosition::fromBeats (std::numeric_limits<float>::max());
            auto endBeatOfPreviousClip = BeatPosition::fromBeats (-std::numeric_limits<float>::max());

            // construct a "no chord" for representing gaps in the chord track
            ARAContentChord noChord{};
            noChord.name = chordNames.insert ("NoChord").first->toRawUTF8();

            for (auto chordClip : chordTrack->getClips())
            {
                // auto position = chordClip->getPosition();
                auto chordStartBeat = chordClip->getStartBeat();
                auto chordEndBeat = chordClip->getEndBeat();

                if (range == nullptr || (chordStartBeat < rangeEndBeat && rangeStartBeat < chordEndBeat))
                {
                    // insert a no chord between gaps in chord clips
                    if (endBeatOfPreviousClip < chordStartBeat)
                    {
                        ARAContentChord noChordCopy = noChord;
                        noChordCopy.position = endBeatOfPreviousClip.inBeats();
                        items.add (noChordCopy);
                    }

                    endBeatOfPreviousClip = chordEndBeat;
                    BeatPosition patternBeat;
                    auto ptnGen = chordClip->getPatternGenerator();
                    jassert (ptnGen);

                    for (auto itm : ptnGen->getChordProgression())
                    {
                        ARAContentChord item{};
                        auto timelineBeat = patternBeat + toDuration (chordStartBeat);

                        bool sharp = ed.pitchSequence.getPitchAtBeat (timelineBeat).accidentalsSharp;
                        Scale scale = ptnGen->getScaleAtBeat (patternBeat);
                        int rootNote = itm->getRootNote (ptnGen->getNoteAtBeat (patternBeat), scale);
                        item.root = MusicalContextFunctions::getCircleOfFifthsIndexforMIDINote (rootNote, sharp);
                        item.bass = item.root;

                        auto chordIntervals = MusicalContextFunctions::getChordARAIntervalUsage (itm->getChord (scale));
                        memcpy (item.intervals, chordIntervals.data(), sizeof (item.intervals));

                        item.name = chordNames.insert (itm->getChordSymbol()).first->toRawUTF8();

                        item.position = timelineBeat.inBeats();
                        items.add (item);

                        patternBeat = patternBeat + itm->lengthInBeats;
                    }
                }
            }

            // if the range is null or goes beyond the last chord clip, 
            // add the no chord here
            if (items.isEmpty() || endBeatOfPreviousClip < rangeEndBeat)
            {
                noChord.position = endBeatOfPreviousClip.inBeats();
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

                item.name = scaleNames.insert (scaleName).first->toRawUTF8();

                item.position = pitchSetting->getStartBeatNumber().inBeats();
                items.add (item);
            }
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
    NodeReader (const AudioFile& af)
        : reader (af.engine->getAudioFileManager().cache.createReader (af))
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
        reader->readSamples (numSamples, buffer, juce::AudioChannelSet::stereo(),
                             0, juce::AudioChannelSet::stereo(), 5000);

        for (int i = 0; i < numChans; ++i)
            juce::FloatVectorOperations::copy ((float*) buffers[i], buffer.getReadPointer (i), numSamples);

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

    static void ARA_CALL destroyAudioReader (ARAAudioAccessControllerHostRef,
                                             ARAAudioReaderHostRef hostReaderRef)
    {
        CRASH_TRACER
        delete (NodeReader*) hostReaderRef;
    }

    SizedStruct<ARA_MEMBER_PTR_ARGS (ARAAudioSourceProperties, merits64BitSamples)> getAudioSourceProperties()
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
    void updateAudioSourceProperties()
    {
        name = clip.getAudioFile().getFile().getFileName();
    }

    const juce::String itemID;
    juce::String name;
    
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

    SizedStruct<ARA_MEMBER_PTR_ARGS (ARAAudioModificationProperties, persistentID)> getAudioModificationProperties()
    {
        return
        {
            nullptr,    // name
            persistentID.toRawUTF8()
        };
    }

    ARADocument& doc;
    AudioSourceWrapper& audioSource;
    ARAAudioModificationRef audioModificationRef = nullptr;
    
private:
    void updateAudioModificationProperties() {}

    const juce::String persistentID;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioModificationWrapper)
};

class RegionSequenceWrapper
{
public:
    RegionSequenceWrapper (ARADocument& d, Track* t) : doc (d), track (t)
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

    SizedStruct<ARA_MEMBER_PTR_ARGS (ARARegionSequenceProperties, color)> getRegionSequenceProperties()
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
    void updateRegionSequenceProperties()
    {
        name = track->getName();
        orderIndex = track->getIndexInEditTrackList();
        auto trackColour = track->getColour();
        colour = { trackColour.getFloatRed(), trackColour.getFloatGreen(), trackColour.getFloatBlue() };
    }

    int orderIndex;
    juce::String name;
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
    SizedStruct<ARA_MEMBER_PTR_ARGS (ARAPlaybackRegionProperties, color)> getPlaybackRegionProperties()
    {
        auto regionSequenceRef = doc.regionSequences[clip.getTrack()]->regionSequenceRef;
        auto pos = clip.getPosition();

        return
        {
            flags,
            pos.getOffset().inSeconds() * clip.getSpeedRatio(),   // Start in modification time
            pos.getLength().inSeconds() * clip.getSpeedRatio(),   // Duration in modification time
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

private:
    void updatePlaybackRegionProperties()
    {
        name = clip.getName();
        auto clipColour = clip.getColour();
        colour = { clipColour.getFloatRed(), clipColour.getFloatGreen(), clipColour.getFloatBlue() };
    }

    juce::String name;
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
                             const juce::String& itemID,
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

    void setViewSelection()
    {
        if (pluginInstance.editorViewInterface != nullptr)
        {
            ARAViewSelection selection;

            ARAPlaybackRegionRef refs[1];
            refs[0] = playbackRegion->playbackRegionRef;

            selection.structSize = sizeof (selection);

            selection.playbackRegionRefsCount = 1;
            selection.playbackRegionRefs = refs;

            selection.regionSequenceRefsCount = 0;
            selection.regionSequenceRefs = nullptr;
            selection.timeRange = nullptr;

            pluginInstance.editorViewInterface->notifySelection (pluginInstance.editorViewRef, &selection);
        }
    }

    std::unique_ptr<PlaybackRegionWrapper> playbackRegion;
    std::unique_ptr<AudioSourceWrapper> audioSource;

private:
    const ARAPlugInExtensionInstance& pluginInstance;
    std::unique_ptr<AudioModificationWrapper> audioModification;

    void setPlaybackRegion()
    {
        CRASH_TRACER

        if (playbackRegion != nullptr && playbackRegion->playbackRegionRef != nullptr)
        {
            if (pluginInstance.playbackRendererInterface != nullptr)
                pluginInstance.playbackRendererInterface->addPlaybackRegion (pluginInstance.playbackRendererRef,
                                                                             playbackRegion->playbackRegionRef);
            if (pluginInstance.editorRendererInterface != nullptr)
                pluginInstance.editorRendererInterface->addPlaybackRegion (pluginInstance.editorRendererRef,
                                                                           playbackRegion->playbackRegionRef);
        }
    }

    void removePlaybackRegion()
    {
        CRASH_TRACER

        if (playbackRegion != nullptr && playbackRegion->playbackRegionRef != nullptr)
        {
            if (pluginInstance.playbackRendererInterface != nullptr)
                pluginInstance.playbackRendererInterface->removePlaybackRegion (pluginInstance.playbackRendererRef,
                                                                                playbackRegion->playbackRegionRef);
            if (pluginInstance.editorRendererInterface != nullptr)
                pluginInstance.editorRendererInterface->removePlaybackRegion (pluginInstance.editorRendererRef,
                                                                              playbackRegion->playbackRegionRef);
        }
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlaybackRegionAndSource)
};
