/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


struct ArchivingFunctions
{
    static ARASize ARA_CALL getArchiveSize (ARAArchivingControllerHostRef,
                                            ARAArchiveReaderHostRef ref)
    {
        if (auto m = (MemoryBlock*) ref)
            return (ARASize) m->getSize();

        return 0;
    }

    static ARABool ARA_CALL readBytesFromArchive (ARAArchivingControllerHostRef,
                                                  ARAArchiveReaderHostRef ref,
                                                  ARASize pos, ARASize length,
                                                  void* const buffer)
    {
        CRASH_TRACER

        if (auto m = (MemoryBlock*) ref)
        {
            if ((pos + length) <= m->getSize())
            {
                std::memcpy (buffer, addBytesToPointer (m->getData(), pos), length);
                return kARATrue;
            }
        }

        return kARAFalse;
    }

    static ARABool ARA_CALL writeBytesToArchive (ARAArchivingControllerHostRef,
                                                 ARAArchiveWriterHostRef ref,
                                                 ARASize position, ARASize length,
                                                 const void* const buffer)
    {
        CRASH_TRACER
        if (auto m = (MemoryOutputStream*) ref)
            if (m->setPosition ((int64) position) && m->write (buffer, length))
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
            tc->setLoopRange ({ startTime, startTime + duration });
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
