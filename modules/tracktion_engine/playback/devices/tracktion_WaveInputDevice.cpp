/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion { inline namespace engine
{

class DiskSpaceCheckTask  : public juce::ThreadPoolJob,
                            private juce::Timer,
                            private juce::AsyncUpdater
{
public:
    DiskSpaceCheckTask (Engine& e, const juce::File& f)
        : juce::ThreadPoolJob ("SpaceCheck"), engine (e), file (f)
    {
        startTimer (1000);
    }

    ~DiskSpaceCheckTask() override
    {
        stopTimer();
        engine.getBackgroundJobs().getPool().removeJob (this, true, 10000);
    }

    JobStatus runJob() override
    {
        auto bytesFree = file.getBytesFreeOnVolume();

        if (bytesFree > 0 && bytesFree < 1024 * 1024 * 50)
            triggerAsyncUpdate();

        return jobHasFinished;
    }

    void handleAsyncUpdate() override
    {
        TransportControl::stopAllTransports (engine, false, false);
        engine.getUIBehaviour().showWarningMessage (TRANS("Recording error - disk space is getting dangerously low!"));
    }

    void timerCallback() override
    {
        startTimer (31111);
        engine.getBackgroundJobs().getPool().addJob (this, false);
    }

    Engine& engine;
    juce::File file;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DiskSpaceCheckTask)
};

//==============================================================================
static const char* projDirPattern  = "%projectdir%";
static const char* editPattern     = "%edit%";
static const char* trackPattern    = "%track%";
static const char* datePattern     = "%date%";
static const char* timePattern     = "%time%";
static const char* takePattern     = "%take%";

static juce::String expandPatterns (Edit& ed, const juce::String& s, Track* track, int take)
{
    juce::String editName (TRANS("Unknown"));
    juce::String trackName (TRANS("Unknown"));
    auto projDir = juce::File::getCurrentWorkingDirectory().getFullPathName();

    editName = juce::File::createLegalFileName (ed.getName());

    if (track != nullptr)
        trackName = juce::File::createLegalFileName (track->getName());

    if (auto proj = ed.engine.getProjectManager().getProject (ed))
    {
        projDir = proj->getDirectoryForMedia (ProjectItem::Category::recorded).getFullPathName();
    }
    else if (ed.editFileRetriever)
    {
        auto editFile = ed.editFileRetriever();

        if (editFile != juce::File() && editFile.getParentDirectory().isDirectory())
            projDir = editFile.getParentDirectory().getFullPathName();
    }

    auto now = juce::Time::getCurrentTime();

    juce::String date;

    date << now.getDayOfMonth()
         << juce::Time::getMonthName (now.getMonth(), true)
         << now.getYear();

    auto time = juce::String::formatted ("%d%02d%02d",
                                         now.getHours(),
                                         now.getMinutes(),
                                         now.getSeconds());

    juce::String s2;

    if (! s.contains (takePattern))
        s2 = s + "_" + juce::String (takePattern);
    else
        s2 = s;

    return juce::File::createLegalPathName (s2.replace (projDirPattern, projDir, true)
                                              .replace (editPattern, editName, true)
                                              .replace (trackPattern, trackName, true)
                                              .replace (datePattern, date, true)
                                              .replace (timePattern, time, true)
                                              .replace (takePattern, juce::String (take), true)
                                              .trim());
}


//==============================================================================
struct RetrospectiveRecordBuffer
{
    RetrospectiveRecordBuffer (Engine& e)
    {
        lengthInSeconds = e.getPropertyStorage().getProperty (SettingID::retrospectiveRecord, 30);
    }

    void updateSizeIfNeeded (int newNumChannels, double newSampleRate)
    {
        int newNumSamples = juce::roundToInt (lengthInSeconds * newSampleRate);

        if (newNumChannels != numChannels || newNumSamples != numSamples || newSampleRate != sampleRate)
        {
            numChannels = newNumChannels;
            numSamples  = newNumSamples;
            sampleRate  = newSampleRate;

            fifo.setSize (numChannels, std::max (1, numSamples));
            fifo.reset();
        }
    }

    void processBuffer (double streamTime, const juce::AudioBuffer<float>& inputBuffer, int numSamplesIn)
    {
        if (numSamplesIn < numSamples)
        {
            lastStreamTime = streamTime;

            fifo.ensureFreeSpace (numSamplesIn);
            fifo.write (inputBuffer);
        }
    }

    void syncToEdit (Edit& edit, EditPlaybackContext& context, double streamTime, int numSamplesIn)
    {
        const juce::SpinLock::ScopedLockType sl (editInfoLock);
        auto& pei = editInfo[edit.getProjectItemID()];

        if (context.isPlaying())
        {
            pei.pausedTime = 0s;
            pei.lastEditTime = context.globalStreamTimeToEditTime (streamTime);
        }
        else
        {
            pei.pausedTime = pei.pausedTime + TimeDuration::fromSamples (numSamplesIn, sampleRate);
        }
    }

    bool wasRecentlyPlaying (Edit& edit)
    {
        const juce::SpinLock::ScopedLockType sl (editInfoLock);
        auto& pei = editInfo[edit.getProjectItemID()];

        return (pei.lastEditTime >= 0s && pei.pausedTime < 20s);
    }

    void removeEditSync (Edit& edit)
    {
        const juce::SpinLock::ScopedLockType sl (editInfoLock);
        auto itr = editInfo.find (edit.getProjectItemID());

        if (itr != editInfo.end())
            editInfo.erase (itr);
    }

    double lengthInSeconds = 30.0;

    AudioFifo fifo { 1, 1 };
    double lastStreamTime = 0;

    int numChannels = 0;
    int numSamples = 0;
    double sampleRate = 0;

    struct PerEditInfo
    {
        TimePosition lastEditTime = -1s;
        TimeDuration pausedTime;
    };

    std::map<ProjectItemID, PerEditInfo> editInfo;
    juce::SpinLock editInfoLock;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RetrospectiveRecordBuffer)
};

//==============================================================================
class WaveInputDeviceInstance  : public InputDeviceInstance
{
public:
    WaveInputDeviceInstance (WaveInputDevice& dev, EditPlaybackContext& c)
        : InputDeviceInstance (dev, c),
          inputBuffer (1, 128)
    {
        getWaveInput().addInstance (this);
    }

    ~WaveInputDeviceInstance() override
    {
        stop();

        auto& wi = getWaveInput();

        if (wi.retrospectiveBuffer)
            wi.retrospectiveBuffer->removeEditSync (edit);

        getWaveInput().removeInstance (this);
    }

    bool isRecordingActive() const override
    {
        return getWaveInput().mergeMode != 2 && InputDeviceInstance::isRecordingActive();
    }

    bool isRecordingActive (const Track& t) const override
    {
        return getWaveInput().mergeMode != 2 && InputDeviceInstance::isRecordingActive (t);
    }
    
    bool shouldTrackContentsBeMuted() override
    {
        const juce::ScopedLock sl (contextLock);

        return recordingContext != nullptr
                && recordingContext->recordingWithPunch
                && muteTrackNow
                && getWaveInput().mergeMode == 1;
    }

    void closeFileWriter()
    {
        const juce::ScopedLock sl (contextLock);

        if (recordingContext != nullptr)
            closeFileWriter (*recordingContext);
    }

    juce::AudioFormat* getFormatToUse() const
    {
        return edit.engine.getAudioFileFormatManager().getNamedFormat (getWaveInput().outputFormat);
    }

    juce::Result getRecordingFile (juce::File& recordedFile, const juce::AudioFormat& format) const
    {
        int take = 1;

        do
        {
            auto firstActiveTarget = getTargetTracks().getFirst();
            for (auto t : getTargetTracks())
            {
                if (activeTracks.contains (t))
                {
                    firstActiveTarget = t;
                    break;
                }
            }

            recordedFile = juce::File (expandPatterns (edit, getWaveInput().filenameMask, firstActiveTarget, take++)
                                         + format.getFileExtensions()[0]);
        } while (recordedFile.exists());

        if (! recordedFile.getParentDirectory().createDirectory())
        {
            TRACKTION_LOG_ERROR ("Record fail: can't create parent directory: " + recordedFile.getFullPathName());

            return juce::Result::fail (TRANS("The directory\nXZZX\ndoesn't exist")
                                          .replace ("XZZX", recordedFile.getParentDirectory().getFullPathName()));
        }

        if (! recordedFile.getParentDirectory().hasWriteAccess())
        {
            TRACKTION_LOG_ERROR ("Record fail: directory is read-only: " + recordedFile.getFullPathName());

            return juce::Result::fail (TRANS("The directory\nXZZX\n doesn't have write-access")
                                        .replace ("XZZX", recordedFile.getParentDirectory().getFullPathName()));
        }

        if (! recordedFile.deleteFile())
        {
            TRACKTION_LOG_ERROR ("Record fail: can't overwrite file: " + recordedFile.getFullPathName());

            return juce::Result::fail (TRANS("Can't overwrite the existing file:") + "\n" + recordedFile.getFullPathName());
        }

        return juce::Result::ok();
    }

    juce::String prepareToRecord (TimePosition playStart, TimePosition punchIn, double sr,
                                  int /*blockSizeSamples*/, bool isLivePunch) override
    {
        CRASH_TRACER

        juce::String error;

        JUCE_TRY
        {
            closeFileWriter();

            if (auto proj = owner.engine.getProjectManager().getProject (edit))
                if (proj->isReadOnly())
                    return TRANS("The current project is read-only, so new clips can't be recorded into it!");

            auto format = getFormatToUse();
            juce::File recordedFile;

            auto res = getRecordingFile (recordedFile, *format);

            if (! res.wasOk())
                return res.getErrorMessage();

            auto rc = std::make_unique<RecordingContext> (edit.engine, recordedFile);
            rc->sampleRate = sr;

            juce::StringPairArray metadata;
            AudioFileUtils::addBWAVStartToMetadata (metadata, (SampleCount) tracktion::toSamples (playStart, sr));
            auto& wi = getWaveInput();

            rc->fileWriter.reset (new AudioFileWriter (AudioFile (edit.engine, recordedFile), format,
                                                       wi.isStereoPair() ? 2 : 1,
                                                       sr, wi.bitDepth, metadata, 0));

            if (rc->fileWriter->isOpen())
            {
                CRASH_TRACER
                auto endRecTime = punchIn + Edit::getMaximumEditTimeRange().getLength();
                auto punchInTime = punchIn;

                rc->firstRecCallback = true;
                muteTrackNow = false;

                const auto adjustSeconds = wi.getAdjustmentSeconds();
                rc->adjustSamples = (int) tracktion::toSamples (adjustSeconds, sr);
                rc->adjustSamples += context.getLatencySamples();

                if (! isLivePunch)
                {
                    rc->recordingWithPunch = edit.recordingPunchInOut;

                    if (rc->recordingWithPunch)
                    {
                        auto loopRange = context.transport.getLoopRange();
                        punchInTime    = std::max (punchInTime, loopRange.getStart() - 0.5s);
                        auto muteStart = std::max (punchInTime, loopRange.getStart());
                        auto muteEnd   = endRecTime;

                        if (edit.getNumCountInBeats() > 0 && context.getLoopTimes().getStart() > loopRange.getStart())
                            punchInTime = context.getLoopTimes().getStart();

                        if (playStart < loopRange.getEnd() - 0.5s)
                        {
                            endRecTime = loopRange.getEnd() + adjustSeconds + 0.8s;
                            muteEnd    = loopRange.getEnd();
                        }

                        rc->muteTimes = { muteStart, muteEnd };
                    }
                    else if (context.isLooping())
                    {
                        punchInTime = context.getLoopTimes().getStart();
                    }
                }

                rc->punchTimes = { punchInTime, endRecTime };
                rc->hasHitThreshold = (wi.recordTriggerDb <= -50.0f);

                if (edit.engine.getUIBehaviour().shouldGenerateLiveWaveformsWhenRecording())
                {
                    if ((rc->thumbnail = edit.engine.getRecordingThumbnailManager().getThumbnailFor (recordedFile)))
                    {
                        rc->thumbnail->reset (wi.isStereoPair() ? 2 : 1, sr);
                        rc->thumbnail->punchInTime = punchInTime;
                    }
                }

                const juce::ScopedLock sl (contextLock);
                recordingContext = std::move (rc);
            }
            else
            {
                TRACKTION_LOG_ERROR ("Record fail: couldn't write to file: " + recordedFile.getFullPathName());

                return TRANS("Couldn't record!") + "\n\n"
                        + TRANS("Couldn't create the file: XZZX").replace ("XZZX", recordedFile.getFullPathName());
            }
        }
        JUCE_CATCH_EXCEPTION

        return error;
    }

    bool startRecording() override
    {
        const juce::ScopedLock sl (consumerLock);
        // This is probably where we should set up our recording context

        // We need to keep a list of tracks the are being recorded to
        // here, since user may un-arm track to stop recording
        activeTracks.clear();

        for (auto destTrack : getTargetTracks())
            if (isRecordingActive (*destTrack))
                activeTracks.add (destTrack);

        return true;
    }

    TimePosition getPunchInTime() override
    {
        const juce::ScopedLock sl (contextLock);
        return recordingContext != nullptr ? recordingContext->punchTimes.getStart()
                                           : edit.getTransport().getTimeWhenStarted();
    }

    bool isRecording() override
    {
        TRACKTION_ASSERT_MESSAGE_THREAD
        return recordingContext != nullptr;
    }

    Clip::Array stopRecording() override
    {
        CRASH_TRACER
        const juce::ScopedLock sl (contextLock);

        if (recordingContext == nullptr)
            return {};

        return context.stopRecording (*this,
                                      { recordingContext->punchTimes.getStart(),
                                        context.getUnloopedPosition() },
                                      false);
    }

    void stop() override
    {
        {
            const juce::ScopedLock sl (consumerLock);
            // This is probably where we should destroy our recording context and apply the recording
        }

        closeFileWriter();
    }

    void recordWasCancelled() override
    {
        std::unique_ptr<RecordingContext> rc;

        {
            const juce::ScopedLock sl (contextLock);
            rc = std::move (recordingContext);
        }

        if (rc != nullptr)
        {
            auto f = rc->file;
            closeFileWriter (*rc);
            f.deleteFile();
        }
    }

    juce::File getRecordingFile() const override
    {
        const juce::ScopedLock sl (contextLock);

        if (recordingContext != nullptr)
            return recordingContext->file;

        return {};
    }

    struct RecordingContext
    {
        RecordingContext (Engine& e, const juce::File& f)
            : engine (e), file (f), diskSpaceChecker (e, f),
              threadInitialiser (e.getWaveInputRecordingThread())
        {}

        Engine& engine;
        juce::File file;
        double sampleRate = 44100.0;
        TimeRange punchTimes, muteTimes;
        bool hasHitThreshold = false, firstRecCallback = false, recordingWithPunch = false;
        int adjustSamples = 0;

        std::unique_ptr<AudioFileWriter> fileWriter;
        DiskSpaceCheckTask diskSpaceChecker;
        RecordingThumbnailManager::Thumbnail::Ptr thumbnail;
        WaveInputRecordingThread::ScopedInitialiser threadInitialiser;

        void addBlockToRecord (const juce::AudioBuffer<float>& buffer, int start, int numSamples)
        {
            if (fileWriter != nullptr)
                engine.getWaveInputRecordingThread().addBlockToRecord (*fileWriter, buffer,
                                                                       start, numSamples, thumbnail);
        }
    };

    Clip::Array applyLastRecordingToEdit (TimeRange recordedRange,
                                          bool isLooping, TimeRange loopRange,
                                          bool discardRecordings,
                                          SelectionManager* selectionManager) override
    {
        TRACKTION_ASSERT_MESSAGE_THREAD
        CRASH_TRACER
        
        Clip::Array clips;

        std::unique_ptr<RecordingContext> rc;

        {
            const juce::ScopedLock sl (contextLock);
            rc = std::move (recordingContext);
        }

        if (rc != nullptr)
        {
            closeFileWriter (*rc);

            if (! rc->file.existsAsFile() || rc->file.getSize() == 0)
                return {};

            const AudioFile recordedFile (edit.engine, rc->file);
            auto recordingDestTracks = getTargetTracks();

            if (discardRecordings || recordingDestTracks.size() == 0)
            {
                recordedFile.deleteFile();
                return {};
            }

            bool firstTrack = true;
            for (auto destTrack : recordingDestTracks)
            {
                if (activeTracks.contains (destTrack))
                {
                    AudioFile trackRecordedFile (edit.engine);
                    if (firstTrack)
                    {
                        trackRecordedFile = recordedFile;
                    }
                    else
                    {
                        // If this audio input is recording to multiple tracks, make
                        // a copy of the source audio for each additional track
                        int take = 1;

                        juce::File f;

                        do
                        {
                            f = juce::File (expandPatterns (edit, getWaveInput().filenameMask, destTrack, take++)
                                                + rc->file.getFileExtension());
                        } while (f.exists());

                        rc->file.copyFileTo (f);

                        trackRecordedFile = AudioFile (edit.engine, f);
                    }

                    auto clipsCreated = applyLastRecording (*rc, trackRecordedFile, *destTrack,
                                                            recordedRange, isLooping, loopRange.getEnd());

                    if (selectionManager != nullptr && ! clipsCreated.isEmpty())
                    {
                        selectionManager->selectOnly (*clipsCreated.getLast());
                        selectionManager->keepSelectedObjectsOnScreen();
                    }
                    
                    clips.addArray (clipsCreated);

                    firstTrack = false;
                }
            }

            return clips;
        }

        return {};
    }

    Clip::Array applyLastRecording (const RecordingContext& rc,
                                    const AudioFile& recordedFile, AudioTrack& destTrack,
                                    TimeRange recordedRange,
                                    bool isLooping, TimePosition loopEnd)
    {
        CRASH_TRACER
        auto& engine = edit.engine;
        auto& afm = engine.getAudioFileManager();
        afm.forceFileUpdate (recordedFile);

        auto recordedFileLength = TimeDuration::fromSeconds (recordedFile.getLength());

        if (recordedFileLength <= 0.00001s)
            return {};

        auto newClipLen = std::min (recordedFileLength,
                                    recordedRange.getLength());

        if (newClipLen <= 0.01s)
        {
            CRASH_TRACER
            juce::String s;

            if (! rc.hasHitThreshold)
                s = TRANS("The device \"XZZX\" \nnever reached the trigger threshold set for recording (THRX).")
                      .replace ("XZZX", getWaveInput().getName())
                      .replace ("THRX", gainToDbString (dbToGain (getWaveInput().recordTriggerDb)));
            else if (edit.recordingPunchInOut && rc.punchTimes.getStart() >= recordedRange.getEnd())
                s = TRANS("The device \"XZZX\" \nnever got as far as the punch-in marker, so didn't start recording!")
                      .replace ("XZZX", getWaveInput().getName());
            else
                s = TRANS("The device \"XZZX\" \nrecorded a zero-length file which won't be added to the edit")
                      .replace ("XZZX", getWaveInput().getName());

            engine.getUIBehaviour().showWarningMessage (s);

            recordedFile.deleteFile();
            return {};
        }

        if (auto proj = engine.getProjectManager().getProject (edit))
        {
            if (auto projectItem = proj->createNewItem (recordedFile.getFile(),
                                                        ProjectItem::waveItemType(),
                                                        recordedFile.getFile().getFileNameWithoutExtension(),
                                                        {}, ProjectItem::Category::recorded, true))
            {
                return applyLastRecording (rc, projectItem, recordedFile, destTrack,
                                           recordedFileLength, newClipLen, isLooping, loopEnd);
            }

            engine.getUIBehaviour().showWarningMessage (proj->isReadOnly() ? TRANS("Couldn't add the new recording to the project, because the project is read-only")
                                                                           : TRANS("Couldn't add the new recording to the project!"));
        }
        else
        {
            return applyLastRecording (rc, nullptr, recordedFile, destTrack,
                                       recordedFileLength, newClipLen, isLooping, loopEnd);
        }

        return {};
    }

    Clip::Array applyLastRecording (const RecordingContext& rc, const ProjectItem::Ptr projectItem,
                                    const AudioFile& recordedFile, AudioTrack& destTrack,
                                    TimeDuration recordedFileLength, TimeDuration newClipLen,
                                    bool isLooping, TimePosition loopEnd)
    {
        CRASH_TRACER
        jassert (projectItem == nullptr || projectItem->getID().isValid());
        auto& engine = edit.engine;
        auto& afm = engine.getAudioFileManager();

        juce::ReferenceCountedArray<ProjectItem> extraTakes;
        juce::Array<juce::File> filesCreated;
        filesCreated.add (recordedFile.getFile());

        if (isLooping)
        {
            if (! splitRecordingIntoMultipleTakes (recordedFile, projectItem, recordedFileLength, extraTakes, filesCreated))
            {
                engine.getUIBehaviour().showWarningAlert (TRANS("Recording"),
                                                          TRANS("Couldn't create audio files for multiple takes"));
                return {};
            }
        }

        auto endPos = rc.punchTimes.getStart() + newClipLen;

        if (edit.recordingPunchInOut || context.transport.looping)
            endPos = juce::jlimit (rc.punchTimes.getStart() + 0.5s, loopEnd, endPos);

        Clip::Ptr newClip;
        bool replaceOldClips = getWaveInput().mergeMode == 1;
        const auto loopRange = edit.getTransport().getLoopRange();

        if (replaceOldClips && edit.recordingPunchInOut)
        {
            if (projectItem != nullptr)
                newClip = destTrack.insertWaveClip (getNewClipName (destTrack), projectItem->getID(),
                                                    { { loopRange.getStart(), endPos }, {} }, true);
            else
                newClip = destTrack.insertWaveClip (getNewClipName (destTrack), recordedFile.getFile(),
                                                    { { loopRange.getStart(), endPos }, {} }, true);

            if (newClip != nullptr)
                newClip->setStart (rc.punchTimes.getStart(), false, false);
        }
        else
        {
            if (projectItem != nullptr)
                newClip = destTrack.insertWaveClip (getNewClipName (destTrack), projectItem->getID(),
                                                    { { rc.punchTimes.getStart(), endPos }, {} }, replaceOldClips);
            else
                newClip = destTrack.insertWaveClip (getNewClipName (destTrack), recordedFile.getFile(),
                                                    { { rc.punchTimes.getStart(), endPos }, {} }, replaceOldClips);
        }

        if (newClip == nullptr)
        {
            juce::String s ("Couldn't insert new clip after recording: ");
            s << rc.punchTimes.getStart() << " " << rc.punchTimes.getStart()
              << " " << endPos << " " << getWaveInput().getAdjustmentSeconds()
              << " " << recordedFileLength;

            TRACKTION_LOG_ERROR (s);

            engine.getUIBehaviour().showWarningMessage (TRANS("Couldn't add the new recording to the project!"));
            return {};
        }

        CRASH_TRACER

        if (edit.recordingPunchInOut)
        {
            if (newClip->getPosition().getStart() < loopRange.getStart())
                newClip->setStart (loopRange.getStart(), true, false);

            if (newClip->getPosition().getEnd() > loopRange.getEnd()
                 && newClip->getPosition().getStart() < loopRange.getEnd() - 0.1s)
                newClip->setEnd (loopRange.getEnd(), true);
        }

        for (auto& f : filesCreated)
        {
            AudioFileUtils::applyBWAVStartTime (f, (SampleCount) tracktion::toSamples (newClip->getPosition().getStartOfSource(), rc.sampleRate));
            afm.forceFileUpdate (AudioFile (edit.engine, f));
        }

        if (auto wc = dynamic_cast<WaveAudioClip*> (newClip.get()))
        {
            if (extraTakes.size() > 0)
            {
                for (auto& take : extraTakes)
                    wc->addTake (take->getID());
            }
            else if (filesCreated.size() > 1)
            {
                for (auto& f : filesCreated)
                    wc->addTake (f);
            }
        }

        Clip::Array clips;
        clips.add (newClip.get());
        return clips;
    }

    bool splitRecordingIntoMultipleTakes (const AudioFile& recordedFile,
                                          const ProjectItem::Ptr& projectItem,
                                          TimeDuration recordedFileLength,
                                          juce::ReferenceCountedArray<ProjectItem>& extraTakes,
                                          juce::Array<juce::File>& filesCreated)
    {
        auto& afm = edit.engine.getAudioFileManager();

        // break the wave into separate takes..
        if (projectItem != nullptr)
            extraTakes.add (projectItem);

        int take = 1;
        auto loopLength = context.transport.getLoopRange().getLength();

        for (;;)
        {
            const auto takeStart    = toPosition (loopLength * take);
            const auto takeEnd      = toPosition (std::min (loopLength * (take + 1),
                                                            recordedFileLength));

            if ((takeEnd - takeStart) < 0.1s)
                break;

            const auto takeRange = TimeRange (takeStart, takeEnd);
            auto takeFile = recordedFile.getFile()
                             .getSiblingFile (recordedFile.getFile().getFileNameWithoutExtension()
                                                + "_take_" + juce::String (take + 1))
                             .withFileExtension (recordedFile.getFile().getFileExtension())
                             .getNonexistentSibling (false);

            afm.releaseFile (recordedFile);

            if (AudioFileUtils::copySectionToNewFile (edit.engine, recordedFile.getFile(), takeFile, takeRange) < 0)
                return false;

            if (projectItem != nullptr)
            {
                if (auto takeObject = projectItem->getProject()->createNewItem (takeFile, ProjectItem::waveItemType(),
                                                                                recordedFile.getFile().getFileNameWithoutExtension()
                                                                                  + " #" + juce::String (take + 1),
                                                                                {},
                                                                                ProjectItem::Category::recorded, true))
                {
                    extraTakes.add (takeObject);
                    filesCreated.add (takeFile);
                }
            }
            else
            {
                filesCreated.add (takeFile);
            }

            ++take;
        }

        // chop down the original wave file..
        auto tempFile = recordedFile.getFile().getNonexistentSibling (false);

        if (AudioFileUtils::copySectionToNewFile (edit.engine, recordedFile.getFile(), tempFile, TimeRange (0.0s, loopLength)) > 0)
        {
            afm.releaseFile (recordedFile);
            tempFile.moveFileTo (recordedFile.getFile());
            filesCreated.add (recordedFile.getFile());
            afm.forceFileUpdate (recordedFile);

            if (projectItem != nullptr)
                projectItem->verifyLength();
            
            return true;
        }

        return false;
    }

    static bool trackContainsClipNamed (AudioTrack& targetTrack, const juce::String& name)
    {
        for (auto c : targetTrack.getClips())
            if (c->getName().equalsIgnoreCase (name))
                return true;

        return false;
    }

    static juce::String getNewClipName (AudioTrack& targetTrack)
    {
        for (int index = 1;; ++index)
        {
            auto name = targetTrack.getName() + " " + TRANS("Recording") + " " + juce::String (index);

            if (! trackContainsClipNamed (targetTrack, name))
                return name;
        }
    }

    juce::Array<Clip*> applyRetrospectiveRecord (SelectionManager* selectionManager) override
    {
        juce::Array<Clip*> clips;
        
        for (auto dstTrack : getTargetTracks())
        {
            auto& wi = getWaveInput();

            auto recordBuffer = wi.getRetrospectiveRecordBuffer();

            if (recordBuffer == nullptr)
                return nullptr;

            auto format = getFormatToUse();
            juce::File recordedFile;

            auto res = getRecordingFile (recordedFile, *format);

            if (res.failed())
                return nullptr;

            juce::StringPairArray metadata;

            {
                AudioFileWriter writer (AudioFile (dstTrack->edit.engine, recordedFile), format,
                                        recordBuffer->numChannels,
                                        recordBuffer->sampleRate,
                                        wi.bitDepth, metadata, 0);

                if (writer.isOpen())
                {
                    int numReady;
                    juce::AudioBuffer<float> scratchBuffer (recordBuffer->numChannels, 1000);

                    while ((numReady = recordBuffer->fifo.getNumReady()) > 0)
                    {
                        auto toRead = std::min (numReady, scratchBuffer.getNumSamples());

                        if (! recordBuffer->fifo.read (scratchBuffer, 0, toRead)
                             || ! writer.appendBuffer (scratchBuffer, toRead))
                            return nullptr;
                    }
                }
            }

            auto proj = owner.engine.getProjectManager().getProject (edit);

            if (proj == nullptr)
            {
                jassertfalse; // TODO
                return nullptr;
            }

            auto projectItem = proj->createNewItem (recordedFile, ProjectItem::waveItemType(),
                                                    recordedFile.getFileNameWithoutExtension(),
                                                    {}, ProjectItem::Category::recorded, true);

            if (projectItem == nullptr)
                continue;

            jassert (projectItem->getID().isValid());

            auto clipName = getNewClipName (*dstTrack);
            TimePosition start;
            const auto recordedLength = TimeDuration::fromSeconds (AudioFile (dstTrack->edit.engine, recordedFile).getLength());

            if (context.isPlaying() || recordBuffer->wasRecentlyPlaying (edit))
            {
                const auto blockSizeSeconds = edit.engine.getDeviceManager().getBlockLength();
                auto adjust = -wi.getAdjustmentSeconds() + blockSizeSeconds;
                
                adjust = adjust - TimeDuration::fromSamples (context.getLatencySamples(), edit.engine.getDeviceManager().getSampleRate());
 
                // TODO: Still not quite sure why the adjustment needs to be a block more with
                // the tracktion_graph engine, this may need correcting in the future
                if (context.getNodePlayHead() != nullptr)
                    adjust = adjust + blockSizeSeconds;

                if (context.isPlaying())
                {
                    start = context.globalStreamTimeToEditTime (recordBuffer->lastStreamTime) - recordedLength + adjust;
                }
                else
                {
                    auto& pei = recordBuffer->editInfo[edit.getProjectItemID()];
                    start = pei.lastEditTime + pei.pausedTime - recordedLength + adjust;
                    pei.lastEditTime = -1s;
                }
            }
            else
            {
                auto position = context.getPosition();

                if (position >= 5s)
                    start = position - recordedLength;
                else
                    start = std::max<TimePosition> (0.0s, position);
            }

            ClipPosition clipPos = { { start, start + recordedLength }, {} };

            if (start < 0s)
            {
                clipPos.offset = toDuration (-start);
                clipPos.time = clipPos.time.withStart (0s);
            }

            auto newClip = dstTrack->insertWaveClip (clipName, projectItem->getID(), clipPos, false);

            if (newClip == nullptr)
                continue;

            CRASH_TRACER

            AudioFileUtils::applyBWAVStartTime (recordedFile, (SampleCount) tracktion::toSamples (newClip->getPosition().getStartOfSource(), recordBuffer->sampleRate));

            edit.engine.getAudioFileManager().forceFileUpdate (AudioFile (dstTrack->edit.engine, recordedFile));

            if (selectionManager != nullptr)
            {
                selectionManager->selectOnly (*newClip);
                selectionManager->keepSelectedObjectsOnScreen();
            }
            
            clips.add (newClip.get());
        }

        return clips;
    }

    bool isLivePlayEnabled (const Track& t) const override
    {
        return owner.isEndToEndEnabled()
                && (isRecordingEnabled (t) || edit.engine.getEngineBehaviour().monitorAudioInputsWithoutRecordEnable())
                && InputDeviceInstance::isLivePlayEnabled (t);
    }

    void copyIncomingDataIntoBuffer (const float* const* allChannels, int numChannels, int numSamples)
    {
        if (numChannels == 0)
            return;

        auto& wi = getWaveInput();
        auto& channelSet = wi.getChannelSet();
        inputBuffer.setSize (channelSet.size(), numSamples);

        for (const auto& ci : wi.getChannels())
        {
            if (juce::isPositiveAndBelow (ci.indexInDevice, numChannels))
            {
                auto inputIndex = channelSet.getChannelIndexForType (ci.channel);
                juce::FloatVectorOperations::copy (inputBuffer.getWritePointer (inputIndex),
                                                   allChannels[ci.indexInDevice], numSamples);
            }
            else
            {
                jassertfalse; // Is an input device getting created with more channels than the total number of device channels?
            }
        }
    }

    void acceptInputBuffer (const float* const* allChannels, int numChannels, int numSamples,
                            double streamTime, LevelMeasurer* measurerToUpdate,
                            RetrospectiveRecordBuffer* retrospectiveBuffer, bool addToRetrospective)
    {
        CRASH_TRACER
        copyIncomingDataIntoBuffer (allChannels, numChannels, numSamples);

        auto inputGainDb = getWaveInput().inputGainDb;

        if (inputGainDb > 0.01f || inputGainDb < -0.01f)
            inputBuffer.applyGain (0, numSamples, dbToGain (inputGainDb));

        if (measurerToUpdate != nullptr)
            measurerToUpdate->processBuffer (inputBuffer, 0, numSamples);

        if (retrospectiveBuffer != nullptr)
        {
            if (addToRetrospective)
            {
                retrospectiveBuffer->updateSizeIfNeeded (inputBuffer.getNumChannels(),
                                                         edit.engine.getDeviceManager().getSampleRate());
                retrospectiveBuffer->processBuffer (streamTime, inputBuffer, numSamples);
            }

            retrospectiveBuffer->syncToEdit (edit, context, streamTime, numSamples);
        }

        {
            const juce::ScopedLock sl (consumerLock);

            for (auto n : consumers)
                n->acceptInputBuffer (choc::buffer::createChannelArrayView (inputBuffer.getArrayOfWritePointers(),
                                                                            (choc::buffer::ChannelCount) inputBuffer.getNumChannels(),
                                                                            (choc::buffer::FrameCount) numSamples));
        }

        const juce::ScopedLock sl (contextLock);

        if (recordingContext != nullptr)
        {
            auto blockStart = context.globalStreamTimeToEditTimeUnlooped (streamTime);
            const TimeRange blockRange (blockStart, TimeDuration::fromSamples (numSamples, recordingContext->sampleRate));

            muteTrackNow = recordingContext->muteTimes.overlaps (blockRange);

            if (recordingContext->punchTimes.overlaps (blockRange))
            {
                if (! recordingContext->hasHitThreshold)
                {
                    auto bufferLevelDb = gainToDb (inputBuffer.getMagnitude (0, numSamples));
                    recordingContext->hasHitThreshold = bufferLevelDb > getWaveInput().recordTriggerDb;

                    if (! recordingContext->hasHitThreshold)
                        return;

                    recordingContext->punchTimes = recordingContext->punchTimes.withStart (blockRange.getStart());

                    if (recordingContext->thumbnail != nullptr)
                        recordingContext->thumbnail->punchInTime = blockRange.getStart();
                }

                if (recordingContext->firstRecCallback)
                {
                    recordingContext->firstRecCallback = false;

                    auto timeDiff = blockRange.getStart() - recordingContext->punchTimes.getStart();
                    recordingContext->adjustSamples -= (int) tracktion::toSamples (timeDiff, recordingContext->sampleRate);
                }

                const int adjustSamples = recordingContext->adjustSamples;

                if (adjustSamples < 0)
                {
                    // add silence
                    AudioScratchBuffer silence (inputBuffer.getNumChannels(), -adjustSamples);
                    silence.buffer.clear();

                    addBlockToRecord (silence.buffer, 0, -adjustSamples);

                    recordingContext->adjustSamples = 0;
                }
                else if (adjustSamples > 0)
                {
                    // drop samples
                    if (adjustSamples >= numSamples)
                    {
                        recordingContext->adjustSamples -= numSamples;
                    }
                    else
                    {
                        addBlockToRecord (inputBuffer, adjustSamples, numSamples - adjustSamples);
                        recordingContext->adjustSamples = 0;
                    }
                }
                else
                {
                    addBlockToRecord (inputBuffer, 0, numSamples);
                }
            }
        }
    }

protected:
    juce::CriticalSection contextLock;
    std::unique_ptr<RecordingContext> recordingContext;

    volatile bool muteTrackNow = false;
    juce::AudioBuffer<float> inputBuffer;

    void addBlockToRecord (const juce::AudioBuffer<float>& buffer, int start, int numSamples)
    {
        const juce::ScopedLock sl (contextLock);
        recordingContext->addBlockToRecord (buffer, start, numSamples);
    }

    static void closeFileWriter (RecordingContext& rc)
    {
        CRASH_TRACER

        if (auto localCopy = std::move (rc.fileWriter))
            rc.engine.getWaveInputRecordingThread().waitForWriterToFinish (*localCopy);
    }

    WaveInputDevice& getWaveInput() const noexcept    { return static_cast<WaveInputDevice&> (owner); }

    //==============================================================================
    juce::Array<Consumer*> consumers;
    juce::CriticalSection consumerLock;

    void addConsumer (Consumer* consumer) override
    {
        const juce::ScopedLock sl (consumerLock);
        consumers.addIfNotAlreadyThere (consumer);
    }

    void removeConsumer (Consumer* consumer) override
    {
        const juce::ScopedLock sl (consumerLock);
        consumers.removeAllInstancesOf (consumer);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaveInputDeviceInstance)
};

//==============================================================================
WaveInputDevice::WaveInputDevice (Engine& e, const juce::String& deviceName, const juce::String& devType,
                                  const std::vector<ChannelIndex>& channels, DeviceType t)
    : InputDevice (e, devType, deviceName),
      deviceChannels (channels),
      deviceType (t),
      channelSet (createChannelSet (channels))
{
    loadProps();
}

WaveInputDevice::~WaveInputDevice()
{
    notifyListenersOfDeletion();
    closeDevice();
}

juce::StringArray WaveInputDevice::getMergeModes()
{
    juce::StringArray s;
    s.add (TRANS("Overlay newly recorded clips onto edit"));
    s.add (TRANS("Replace old clips in edit with new ones"));
    s.add (TRANS("Don't make recordings from this device"));

    return s;
}

juce::StringArray WaveInputDevice::getRecordFormatNames()
{
    juce::StringArray s;

    auto& afm = engine.getAudioFileFormatManager();
    s.add (afm.getWavFormat()->getFormatName());
    s.add (afm.getAiffFormat()->getFormatName());
    s.add (afm.getFlacFormat()->getFormatName());

    return s;
}


InputDeviceInstance* WaveInputDevice::createInstance (EditPlaybackContext& ed)
{
    if (! isTrackDevice() && retrospectiveBuffer == nullptr)
        retrospectiveBuffer.reset (new RetrospectiveRecordBuffer (ed.edit.engine));

    return new WaveInputDeviceInstance (*this, ed);
}

void WaveInputDevice::resetToDefault()
{
    juce::String propName = isTrackDevice() ? "TRACKTION_TRACK_DEVICE" : getName();
    engine.getPropertyStorage().removePropertyItem (SettingID::wavein, propName);
    loadProps();
}

void WaveInputDevice::setEnabled (bool b)
{
    if (enabled != b)
    {
        enabled = b;
        changed();

        if (! isTrackDevice())
        {
            engine.getDeviceManager().setWaveInChannelsEnabled (deviceChannels, b);
        }
        else
        {
            // reload our properties in case another track device has changed them
            loadProps();
        }
        // do nothing now! this object is probably deleted..
    }
}

juce::String WaveInputDevice::openDevice()
{
    return {};
}

void WaveInputDevice::closeDevice()
{
    saveProps();
}

void WaveInputDevice::loadProps()
{
    filenameMask = getDefaultMask();
    inputGainDb = 0.0f;
    endToEndEnabled = false;
    outputFormat = engine.getAudioFileFormatManager().getDefaultFormat()->getFormatName();

    recordTriggerDb = -50.0f;
    mergeMode = 0;
    bitDepth = 24;
    recordAdjustMs = 0;

    juce::String propName = isTrackDevice() ? "TRACKTION_TRACK_DEVICE" : getName();

    if (auto n = engine.getPropertyStorage().getXmlPropertyItem (SettingID::wavein, propName))
    {
        filenameMask = n->getStringAttribute ("filename", filenameMask);
        inputGainDb = (float) n->getDoubleAttribute ("gainDb", inputGainDb);
        endToEndEnabled = n->getBoolAttribute ("etoe", endToEndEnabled);
        outputFormat = n->getStringAttribute ("format", outputFormat);
        bitDepth = n->getIntAttribute ("bits", bitDepth);

        if (! getRecordFormatNames().contains (outputFormat))
            outputFormat = engine.getAudioFileFormatManager().getDefaultFormat()->getFormatName();

        recordTriggerDb = (float) n->getDoubleAttribute ("triggerDb", recordTriggerDb);
        mergeMode = n->getIntAttribute ("mode", mergeMode);
        recordAdjustMs = n->getDoubleAttribute ("adjustMs", recordAdjustMs);

        if (recordAdjustMs != 0)
            TRACKTION_LOG ("Manual record adjustment: " + juce::String (recordAdjustMs) + "ms");
    }

    checkBitDepth();
}

void WaveInputDevice::saveProps()
{
    juce::XmlElement n ("SETTINGS");

    n.setAttribute ("filename", filenameMask);
    n.setAttribute ("gainDb", inputGainDb);
    n.setAttribute ("etoe", endToEndEnabled);
    n.setAttribute ("format", outputFormat);
    n.setAttribute ("bits", bitDepth);
    n.setAttribute ("triggerDb", recordTriggerDb);
    n.setAttribute ("mode", mergeMode);
    n.setAttribute ("adjustMs", recordAdjustMs);

    juce::String propName = isTrackDevice() ? "TRACKTION_TRACK_DEVICE" : getName();

    engine.getPropertyStorage().setXmlPropertyItem (SettingID::wavein, propName, n);
}

//==============================================================================
juce::String WaveInputDevice::getSelectableDescription()
{
    if (getDeviceType() == trackWaveDevice)
        return getAlias() + " (" + getType() + ")";

    return InputDevice::getSelectableDescription();
}

bool WaveInputDevice::isStereoPair() const
{
    return deviceChannels.size() == 2;
}

void WaveInputDevice::setStereoPair (bool stereo)
{
    if (isTrackDevice())
    {
        jassertfalse;
        return;
    }

    auto& dm = engine.getDeviceManager();

    if (deviceChannels.size() == 2)
        dm.setDeviceInChannelStereo (std::max (deviceChannels[0].indexInDevice, deviceChannels[1].indexInDevice), stereo);
    else if (deviceChannels.size() == 1)
        dm.setDeviceInChannelStereo (deviceChannels[0].indexInDevice, stereo);
}

void WaveInputDevice::setEndToEnd (bool newEtoE)
{
    if (endToEndEnabled != newEtoE)
    {
        endToEndEnabled = newEtoE;
        TransportControl::restartAllTransports (engine, false);
        changed();
        saveProps();
    }
}

void WaveInputDevice::flipEndToEnd()
{
    setEndToEnd (! endToEndEnabled);
}

void WaveInputDevice::setRecordAdjustmentMs (double ms)
{
    recordAdjustMs = juce::jlimit (-500.0, 500.0, ms);
    changed();
    saveProps();
}

void WaveInputDevice::setInputGainDb (float newGain)
{
    if (newGain != inputGainDb)
    {
        inputGainDb = newGain;
        changed();
        saveProps();
    }
}

void WaveInputDevice::setRecordTriggerDb (float newDB)
{
    if (recordTriggerDb != newDB)
    {
        recordTriggerDb = newDB;
        changed();
        saveProps();
    }
}

juce::String WaveInputDevice::getDefaultMask()
{
    juce::String defaultFile;
    defaultFile << projDirPattern << juce::File::getSeparatorChar() << editPattern << '_'
                << trackPattern<< '_'  << TRANS("Take") << '_' << takePattern;

    return defaultFile;
}

void WaveInputDevice::setFilenameMask (const juce::String& newMask)
{
    if (filenameMask != newMask)
    {
        filenameMask = newMask.isNotEmpty() ? newMask
                                            : getDefaultMask();
        changed();
        saveProps();
    }
}

void WaveInputDevice::setFilenameMaskToDefault()
{
    if (getDefaultMask() != filenameMask)
        setFilenameMask ({});
}

void WaveInputDevice::setBitDepth (int newDepth)
{
    if (bitDepth != newDepth)
    {
        bitDepth = newDepth;
        changed();
        saveProps();
    }
}

void WaveInputDevice::checkBitDepth()
{
    if (! getAvailableBitDepths().contains (bitDepth))
        bitDepth = getAvailableBitDepths().getLast();
}

juce::Array<int> WaveInputDevice::getAvailableBitDepths() const
{
    return getFormatToUse()->getPossibleBitDepths();
}

juce::AudioFormat* WaveInputDevice::getFormatToUse() const
{
    return engine.getAudioFileFormatManager().getNamedFormat (outputFormat);
}

void WaveInputDevice::setOutputFormat (const juce::String& newFormat)
{
    if (outputFormat != newFormat)
    {
        outputFormat = newFormat;
        checkBitDepth();
        changed();
        saveProps();
    }
}

juce::String WaveInputDevice::getMergeMode() const
{
    return getMergeModes() [mergeMode];
}

void WaveInputDevice::setMergeMode (const juce::String& newMode)
{
    int newIndex = getMergeModes().indexOf (newMode);

    if (mergeMode != newIndex)
    {
        mergeMode = newIndex;
        changed();
        saveProps();
    }
}

TimeDuration WaveInputDevice::getAdjustmentSeconds()
{
    auto& dm = engine.getDeviceManager();
    const double autoAdjustMs = isTrackDevice() ? 0.0 : dm.getRecordAdjustmentMs();

    return TimeDuration::fromSeconds (juce::jlimit (-3.0, 3.0, 0.001 * (recordAdjustMs + autoAdjustMs)));
}

//==============================================================================
void WaveInputDevice::addInstance (WaveInputDeviceInstance* i)
{
    const juce::ScopedLock sl (instanceLock);
    instances.addIfNotAlreadyThere (i);
}

void WaveInputDevice::removeInstance (WaveInputDeviceInstance* i)
{
    const juce::ScopedLock sl (instanceLock);
    instances.removeAllInstancesOf (i);
}

//==============================================================================
void WaveInputDevice::consumeNextAudioBlock (const float* const* allChannels, int numChannels, int numSamples, double streamTime)
{
    if (enabled)
    {
        bool isFirst = true;
        const juce::ScopedLock sl (instanceLock);

        // do this first, as writing to file trashes the buffer
        for (auto i : instances)
        {
            i->acceptInputBuffer (allChannels, numChannels, numSamples, streamTime,
                                  isFirst ? &levelMeasurer : nullptr,
                                  (! retrospectiveRecordLock) ? retrospectiveBuffer.get() : nullptr, isFirst);
            isFirst = false;
        }
    }
}

//==============================================================================
void WaveInputDevice::updateRetrospectiveBufferLength (double length)
{
    if (retrospectiveBuffer != nullptr)
        retrospectiveBuffer->lengthInSeconds = length;
}

//==============================================================================
struct WaveInputRecordingThread::BlockQueue
{
    BlockQueue()
    {
        for (int i = 32; --i >= 0;)
            addToFreeQueue (new QueuedBlock());
    }

    struct QueuedBlock
    {
        QueuedBlock() = default;

        void load (AudioFileWriter& w, const juce::AudioBuffer<float>& newBuffer,
                   int start, int numSamples, const RecordingThumbnailManager::Thumbnail::Ptr& thumb)
        {
            buffer.setSize (newBuffer.getNumChannels(), numSamples);

            for (int i = buffer.getNumChannels(); --i >= 0;)
                buffer.copyFrom (i, 0, newBuffer, i, start, numSamples);

            writer = &w;
            thumbnail = thumb;
        }

        std::atomic<AudioFileWriter*> writer { nullptr };
        QueuedBlock* next = nullptr;
        juce::AudioBuffer<float> buffer { 2, 512 };
        RecordingThumbnailManager::Thumbnail::Ptr thumbnail;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (QueuedBlock)
    };

    juce::CriticalSection freeQueueLock, pendingQueueLock;
    QueuedBlock* firstPending = nullptr;
    QueuedBlock* lastPending = nullptr;
    QueuedBlock* firstFree = nullptr;
    std::atomic<int> numPending { 0 };

    QueuedBlock* findFreeBlock()
    {
        const juce::ScopedLock sl (freeQueueLock);

        if (firstFree == nullptr)
            return new QueuedBlock();

        auto* b = firstFree;
        firstFree = b->next;
        b->next = nullptr;
        return b;
    }

    void addToFreeQueue (QueuedBlock* b) noexcept
    {
        jassert (b != nullptr);
        const juce::ScopedLock sl (freeQueueLock);
        b->writer = nullptr;
        b->next = firstFree;
        firstFree = b;
    }

    void addToPendingQueue (QueuedBlock* b) noexcept
    {
        jassert (b != nullptr);
        b->next = nullptr;
        const juce::ScopedLock sl (pendingQueueLock);

        if (lastPending != nullptr)
            lastPending->next = b;
        else
            firstPending = b;

        lastPending = b;
        ++numPending;
    }

    QueuedBlock* removeFirstPending() noexcept
    {
        const juce::ScopedLock sl (pendingQueueLock);

        if (auto b = firstPending)
        {
            firstPending = b->next;

            if (firstPending == nullptr)
                lastPending = nullptr;

            --numPending;
            return b;
        }

        return {};
    }

    void moveAnyPendingBlocksToFree() noexcept
    {
        while (auto b = removeFirstPending())
            addToFreeQueue (b);

        jassert (numPending == 0);
        numPending = 0;
    }

    bool isWriterInQueue (AudioFileWriter& writer) const
    {
        const juce::ScopedLock sl (pendingQueueLock);

        for (auto b = firstPending; b != nullptr; b = b->next)
            if (b->writer == &writer)
                return true;

        return false;
    }

    void deleteFreeQueue() noexcept
    {
        auto b = firstFree;
        firstFree = nullptr;

        while (b != nullptr)
        {
            std::unique_ptr<QueuedBlock> toDelete (b);
            b = b->next;
        }
    }
};

//==============================================================================
WaveInputRecordingThread::WaveInputRecordingThread (Engine& e)
    : Thread ("WaveInputRecordingThread"),
      engine (e),
      queue (new BlockQueue())
{
}

WaveInputRecordingThread::~WaveInputRecordingThread()
{
    flushAndStop();
    queue->deleteFreeQueue();
    queue.reset();
}

void WaveInputRecordingThread::addUser()
{
    if (activeUsers++ == 0)
        prepareToStart();
}

void WaveInputRecordingThread::removeUser()
{
    if (--activeUsers == 0)
        flushAndStop();
}

//==============================================================================
void WaveInputRecordingThread::addBlockToRecord (AudioFileWriter& writer, const juce::AudioBuffer<float>& buffer,
                                                 int start, int numSamples, const RecordingThumbnailManager::Thumbnail::Ptr& thumbnail)
{
    if (! threadShouldExit())
    {
        auto block = queue->findFreeBlock();
        block->load (writer, buffer, start, numSamples, thumbnail);
        queue->addToPendingQueue (block);
        notify();
    }
}

void WaveInputRecordingThread::waitForWriterToFinish (AudioFileWriter& writer)
{
    while (queue->isWriterInQueue (writer) && isThreadRunning())
        Thread::sleep (2);
}

void WaveInputRecordingThread::run()
{
    CRASH_TRACER
    juce::FloatVectorOperations::disableDenormalisedNumberSupport();

    for (;;)
    {
        if (queue->numPending > 500 && ! hasWarned)
        {
            hasWarned = true;
            TRACKTION_LOG_ERROR ("Audio recording can't keep up!");
        }

        if (auto block = queue->removeFirstPending())
        {
            if (! block->writer.load()->appendBuffer (block->buffer, block->buffer.getNumSamples()))
            {
                if (! hasSentStop)
                {
                    hasSentStop = true;
                    TRACKTION_LOG_ERROR ("Audio recording failed to write to disk!");
                    startTimer (1);
                }
            }

            if (block->thumbnail != nullptr)
            {
                block->thumbnail->addBlock (block->buffer, 0, block->buffer.getNumSamples());
                block->thumbnail = nullptr;
            }

            queue->addToFreeQueue (block);
        }
        else
        {
            if (threadShouldExit())
                break;

            wait (401);
        }
    }
}

void WaveInputRecordingThread::timerCallback()
{
    stopTimer();
    TransportControl::stopAllTransports (engine, false, false);
}

void WaveInputRecordingThread::prepareToStart()
{
    flushAndStop();
    sleep (2);
    jassert (! isThreadRunning());
    startThread (juce::Thread::Priority::normal);
}

void WaveInputRecordingThread::flushAndStop()
{
    signalThreadShouldExit();
    notify();
    stopThread (30000);
    queue->moveAnyPendingBlocksToFree();
    hasSentStop = false;
    hasWarned = false;
}

}} // namespace tracktion { inline namespace engine
