/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

struct ArchivingFunctions
{
    static ARASize ARA_CALL getArchiveSize (ARAArchivingControllerHostRef,
                                            ARAArchiveReaderHostRef ref)
    {
        if (auto m = (juce::MemoryBlock*) ref)
            return (ARASize) m->getSize();

        return 0;
    }

    static ARABool ARA_CALL readBytesFromArchive (ARAArchivingControllerHostRef,
                                                  ARAArchiveReaderHostRef ref,
                                                  ARASize pos, ARASize length,
                                                  ARAByte buffer[])
    {
        CRASH_TRACER

        if (auto m = (juce::MemoryBlock*) ref)
        {
            if ((pos + length) <= m->getSize())
            {
                std::memcpy (buffer, juce::addBytesToPointer (m->getData(), pos), length);
                return kARATrue;
            }
        }

        return kARAFalse;
    }

    static ARABool ARA_CALL writeBytesToArchive (ARAArchivingControllerHostRef,
                                                 ARAArchiveWriterHostRef ref,
                                                 ARASize position, ARASize length,
                                                 const ARAByte buffer[])
    {
        CRASH_TRACER
        if (auto m = (juce::MemoryOutputStream*) ref)
            if (m->setPosition ((int64_t) position) && m->write (buffer, length))
                return kARATrue;

        return kARAFalse;
    }

    static void ARA_CALL notifyDocumentArchivingProgress (ARAArchivingControllerHostRef, float p)
    {
        juce::ignoreUnused (p);
        //TRACKTION_LOG_ARA ("Archiving progress: " << p);
    }

    static void ARA_CALL notifyDocumentUnarchivingProgress (ARAArchivingControllerHostRef, float p)
    {
        juce::ignoreUnused (p);
        //TRACKTION_LOG_ARA ("Unarchiving progress: " << p);
    }
};

//==============================================================================
struct EditProxyFunctions
{
    static void ARA_CALL requestStartPlayback (ARAPlaybackControllerHostRef ref)
    {
        CRASH_TRACER
        if (auto tc = (TransportControl*) ref)
            tc->play (false);
    }

    static void ARA_CALL requestStopPlayback (ARAPlaybackControllerHostRef ref)
    {
        CRASH_TRACER
        if (auto tc = (TransportControl*) ref)
            tc->stop (false, false);
    }

    static void ARA_CALL requestSetPlaybackPosition (ARAPlaybackControllerHostRef ref, ARATimePosition timePosition)
    {
        CRASH_TRACER
        if (auto tc = (TransportControl*) ref)
            tc->setCurrentPosition (timePosition);
    }

    static void ARA_CALL requestSetCycleRange (ARAPlaybackControllerHostRef ref, ARATimePosition startTime, ARATimeDuration duration)
    {
        CRASH_TRACER
        if (auto tc = (TransportControl*) ref)
            tc->setLoopRange ({ TimePosition::fromSeconds (startTime), TimeDuration::fromSeconds (duration) });
    }

    static void ARA_CALL requestEnableCycle (ARAPlaybackControllerHostRef ref, ARABool enable)
    {
        CRASH_TRACER
        if (auto tc = (TransportControl*) ref)
            tc->looping = enable != kARAFalse;
    }
};

//==============================================================================
struct ModelUpdateFunctions
{
    static void ARA_CALL notifyAudioSourceAnalysisProgress (ARAModelUpdateControllerHostRef,
                                                            ARAAudioSourceHostRef,
                                                            ARAAnalysisProgressState,
                                                            float)
    {
    }

    static void ARA_CALL notifyAudioSourceContentChanged (ARAModelUpdateControllerHostRef hostRef,
                                                          ARAAudioSourceHostRef,
                                                          const ARAContentTimeRange*,
                                                          ARAContentUpdateFlags)
    {
        CRASH_TRACER
        if (auto e = (Edit*) hostRef)
            e->markAsChanged();
    }

    static void ARA_CALL notifyAudioModificationContentChanged (ARAModelUpdateControllerHostRef hostRef,
                                                                ARAAudioModificationHostRef,
                                                                const ARAContentTimeRange*,
                                                                ARAContentUpdateFlags)
    {
        CRASH_TRACER
        if (auto e = (Edit*) hostRef)
            e->markAsChanged();
    }
};

//==============================================================================
struct MusicalContextFunctions
{
    static ARA::ARACircleOfFifthsIndex getCircleOfFifthsIndexforMIDINote (int note, bool useSharps)
    {
        static const ARA::ARACircleOfFifthsIndex sharpNoteIndices[] = { 0, 7, 2, 9, 4, -1, 6, 1, 8, 3, 10, 5 };
        static const ARA::ARACircleOfFifthsIndex flatNoteIndices[] = { 0, -5, 2, -3, 4, -1, -6, 1, -4, 3, -2, 5 };

        if (juce::isPositiveAndBelow (note, 128))
        {
            return (useSharps ? sharpNoteIndices[note % 12]
                              : flatNoteIndices[note % 12]);
        }

        return 0;
    }

    static std::array<ARA::ARAChordIntervalUsage, 12> getChordARAIntervalUsage (Chord c)
    {
        using namespace ARA;
        switch (c.getType())
        {
            case Chord::majorTriad:                    return { kARAChordDiatonicDegree1, 0, 0, 0, kARAChordDiatonicDegree3, 0, 0, kARAChordDiatonicDegree5, 0, 0, 0, 0 };
            case Chord::minorTriad:                    return { kARAChordDiatonicDegree1, 0, 0, kARAChordDiatonicDegree3, 0, 0, 0, kARAChordDiatonicDegree5, 0, 0, 0, 0 };
            case Chord::diminishedTriad:               return { kARAChordDiatonicDegree1, 0, 0, kARAChordDiatonicDegree3, 0, 0, kARAChordDiatonicDegree5, 0, 0, 0, 0, 0 };
            case Chord::augmentedTriad:                return { kARAChordDiatonicDegree1, 0, 0, 0, kARAChordDiatonicDegree3, 0, 0, 0, kARAChordDiatonicDegree5, 0, 0, 0 };
            case Chord::majorSixthChord:               return { kARAChordDiatonicDegree1, 0, 0, 0, kARAChordDiatonicDegree3, 0, 0, kARAChordDiatonicDegree5, 0, kARAChordDiatonicDegree6, 0, 0 };
            case Chord::minorSixthChord:               return { kARAChordDiatonicDegree1, 0, 0, kARAChordDiatonicDegree3, 0, 0, 0, kARAChordDiatonicDegree5, 0, kARAChordDiatonicDegree6, 0, 0 };
            case Chord::dominatSeventhChord:           return { kARAChordDiatonicDegree1, 0, 0, 0, kARAChordDiatonicDegree3, 0, 0, kARAChordDiatonicDegree5, 0, 0, kARAChordDiatonicDegree7, 0 };
            case Chord::majorSeventhChord:             return { kARAChordDiatonicDegree1, 0, 0, 0, kARAChordDiatonicDegree3, 0, 0, kARAChordDiatonicDegree5, 0, 0, 0, kARAChordDiatonicDegree7 };
            case Chord::minorSeventhChord:             return { kARAChordDiatonicDegree1, 0, 0, kARAChordDiatonicDegree3, 0, 0, 0, kARAChordDiatonicDegree5, 0, 0, kARAChordDiatonicDegree7, 0 };
            case Chord::augmentedSeventhChord:         return { kARAChordDiatonicDegree1, 0, 0, 0, kARAChordDiatonicDegree3, 0, 0, 0, kARAChordDiatonicDegree5, 0, kARAChordDiatonicDegree7, 0 };
            case Chord::diminishedSeventhChord:        return { kARAChordDiatonicDegree1, 0, 0, kARAChordDiatonicDegree3, 0, 0, kARAChordDiatonicDegree5, 0, 0, kARAChordDiatonicDegree7, 0, 0 };
            case Chord::halfDiminishedSeventhChord:    return { kARAChordDiatonicDegree1, 0, 0, kARAChordDiatonicDegree3, 0, 0, kARAChordDiatonicDegree5, 0, 0, 0, kARAChordDiatonicDegree7, 0 };
            case Chord::minorMajorSeventhChord:        return { kARAChordDiatonicDegree1, 0, 0, kARAChordDiatonicDegree3, 0, 0, 0, kARAChordDiatonicDegree5, 0, 0, 0, kARAChordDiatonicDegree7 };
            case Chord::suspendedSecond:               return { kARAChordDiatonicDegree1, 0, kARAChordDiatonicDegree2, 0, 0, 0, 0, kARAChordDiatonicDegree5, 0, 0, 0, 0 };
            case Chord::suspendedFourth:               return { kARAChordDiatonicDegree1, 0, 0, 0, 0, kARAChordDiatonicDegree4, 0, kARAChordDiatonicDegree5, 0, 0, 0, 0 };
            case Chord::powerChord:                    return { kARAChordDiatonicDegree1, 0, 0, 0, 0, 0, 0, kARAChordDiatonicDegree5, 0, 0, 0, 0 };
            case Chord::majorNinthChord:               return { kARAChordDiatonicDegree1, 0, kARAChordDiatonicDegree9, 0, kARAChordDiatonicDegree3, 0, 0, kARAChordDiatonicDegree5, 0, 0, 0, kARAChordDiatonicDegree7 };
            case Chord::dominantNinthChord:            return { kARAChordDiatonicDegree1, 0, kARAChordDiatonicDegree9, 0, kARAChordDiatonicDegree3, 0, 0, kARAChordDiatonicDegree5, 0, 0, kARAChordDiatonicDegree7, 0 };
            case Chord::minorMajorNinthChord:          return { kARAChordDiatonicDegree1, 0, kARAChordDiatonicDegree9, kARAChordDiatonicDegree3, 0, 0, 0, kARAChordDiatonicDegree5, 0, 0, 0, kARAChordDiatonicDegree7 };
            case Chord::minorDominantNinthChord:       return { kARAChordDiatonicDegree1, 0, kARAChordDiatonicDegree9, kARAChordDiatonicDegree3, 0, 0, 0, kARAChordDiatonicDegree5, 0, 0, kARAChordDiatonicDegree7, 0 };
            case Chord::augmentedMajorNinthChord:      return { kARAChordDiatonicDegree1, 0, kARAChordDiatonicDegree9, 0, kARAChordDiatonicDegree3, 0, 0, 0, kARAChordDiatonicDegree5, 0, 0, kARAChordDiatonicDegree7 };
            case Chord::augmentedDominantNinthChord:   return { kARAChordDiatonicDegree1, 0, kARAChordDiatonicDegree9, 0, kARAChordDiatonicDegree3, 0, 0, 0, kARAChordDiatonicDegree5, 0, kARAChordDiatonicDegree7, 0 };
            case Chord::halfDiminishedNinthChord:      return { kARAChordDiatonicDegree1, 0, kARAChordDiatonicDegree9, kARAChordDiatonicDegree3, 0, 0, kARAChordDiatonicDegree5, 0, 0, 0, kARAChordDiatonicDegree7, 0 };
            case Chord::halfDiminishedMinorNinthChord: return { kARAChordDiatonicDegree1, kARAChordDiatonicDegree9, 0, kARAChordDiatonicDegree3, 0, 0, kARAChordDiatonicDegree5, 0, 0, 0, kARAChordDiatonicDegree7, 0 };
            case Chord::diminishedNinthChord:          return { kARAChordDiatonicDegree1, 0, kARAChordDiatonicDegree9, kARAChordDiatonicDegree3, 0, 0, kARAChordDiatonicDegree5, 0, 0, kARAChordDiatonicDegree7, 0, 0 };
            case Chord::diminishedMinorNinthChord:     return { kARAChordDiatonicDegree1, kARAChordDiatonicDegree9, 0, kARAChordDiatonicDegree3, 0, 0, kARAChordDiatonicDegree5, 0, 0, kARAChordDiatonicDegree7, 0, 0 };
            case Chord::customChord:
            default:
            {
                std::array<ARA::ARAChordIntervalUsage, 12> chordIntervals{};
                for (auto s : c.getSteps())
                    chordIntervals[(std::size_t)s] = ARA::kARAKeySignatureIntervalUsed;
                return chordIntervals;
            }
        }
    }
};
