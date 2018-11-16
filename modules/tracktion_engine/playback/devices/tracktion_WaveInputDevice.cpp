/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


class DiskSpaceCheckTask  : public ThreadPoolJob,
                            private Timer,
                            private AsyncUpdater
{
public:
    DiskSpaceCheckTask (Engine& e, const File& f)
        : ThreadPoolJob ("SpaceCheck"), engine (e), file (f)
    {
        startTimer (1000);
    }

    ~DiskSpaceCheckTask()
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
    File file;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DiskSpaceCheckTask)
};

//==============================================================================
static const char* projDirPattern  = "%projectdir%";
static const char* editPattern     = "%edit%";
static const char* trackPattern    = "%track%";
static const char* datePattern     = "%date%";
static const char* timePattern     = "%time%";
static const char* takePattern     = "%take%";

static String expandPatterns (Edit& ed, const String& s, Track* track, int take)
{
    String editName (TRANS("Unknown"));
    String trackName (TRANS("Unknown"));
    auto projDir = File::getCurrentWorkingDirectory().getFullPathName();

    editName = File::createLegalFileName (ed.getName());

    if (track != nullptr)
        trackName = File::createLegalFileName (track->getName());

    if (auto proj = ProjectManager::getInstance()->getProject (ed))
        projDir = proj->getDirectoryForMedia (ProjectItem::Category::recorded).getFullPathName();

    auto now = Time::getCurrentTime();

    String date;
    date << now.getDayOfMonth()
         << Time::getMonthName (now.getMonth(), true)
         << now.getYear();

    auto time = String::formatted ("%d%02d%02d",
                                   now.getHours(),
                                   now.getMinutes(),
                                   now.getSeconds());

    String s2;
    if (! s.contains (takePattern))
        s2 = s + "_" + String (takePattern);
    else
        s2 = s;

    return File::createLegalPathName (s2.replace (projDirPattern, projDir, true)
                                        .replace (editPattern, editName, true)
                                        .replace (trackPattern, trackName, true)
                                        .replace (datePattern, date, true)
                                        .replace (timePattern, time, true)
                                        .replace (takePattern, String (take), true)
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
        int newNumSamples = roundToInt (lengthInSeconds * newSampleRate);

        if (newNumChannels != numChannels || newNumSamples != numSamples || newSampleRate != sampleRate)
        {
            numChannels = newNumChannels;
            numSamples  = newNumSamples;
            sampleRate  = newSampleRate;

            fifo.setSize (numChannels, jmax (1, numSamples));
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
        auto& pei = editInfo[edit.getProjectItemID()];

        if (context.playhead.isPlaying())
        {
            pei.pausedTime = 0;
            pei.lastEditTime = context.playhead.streamTimeToSourceTime (streamTime);
        }
        else
        {
            pei.pausedTime += numSamplesIn / sampleRate;
        }
    }

    bool wasRecentlyPlaying (Edit& edit)
    {
        auto& pei = editInfo[edit.getProjectItemID()];

        return (pei.lastEditTime >= 0 && pei.pausedTime < 20);
    }

    void removeEditSync (Edit& edit)
    {
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
        double lastEditTime = -1;
        double pausedTime = 0;
    };

    std::map<ProjectItemID, PerEditInfo> editInfo;

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

    ~WaveInputDeviceInstance()
    {
        auto& wi = getWaveInput();

        if (wi.retrospectiveBuffer)
            wi.retrospectiveBuffer->removeEditSync (edit);

        getWaveInput().removeInstance (this);
    }

    bool isRecordingActive() const override
    {
        return getWaveInput().mergeMode != 2 && InputDeviceInstance::isRecordingActive();
    }

    bool shouldTrackContentsBeMuted() override
    {
        const ScopedLock sl (contextLock);

        return recordingContext != nullptr
                && recordingContext->recordingWithPunch
                && muteTrackNow
                && getWaveInput().mergeMode == 1;
    }

    void closeFileWriter()
    {
        const ScopedLock sl (contextLock);

        if (recordingContext != nullptr)
            closeFileWriter (*recordingContext);
    }

    AudioFormat* getFormatToUse() const
    {
        return edit.engine.getAudioFileFormatManager().getNamedFormat (getWaveInput().outputFormat);
    }

    Result getRecordingFile (File& recordedFile, const AudioFormat& format) const
    {
        int take = 1;

        do
        {
            recordedFile = File (expandPatterns (edit, getWaveInput().filenameMask, getTargetTrack(), take++)
                                    + format.getFileExtensions()[0]);
        } while (recordedFile.exists());

        if (! recordedFile.getParentDirectory().createDirectory())
        {
            TRACKTION_LOG_ERROR ("Record fail: can't create parent directory: " + recordedFile.getFullPathName());

            return Result::fail (TRANS("The directory\nXZZX\ndoesn't exist")
                                 .replace ("XZZX", recordedFile.getParentDirectory().getFullPathName()));
        }

        if (! recordedFile.getParentDirectory().hasWriteAccess())
        {
            TRACKTION_LOG_ERROR ("Record fail: directory is read-only: " + recordedFile.getFullPathName());

            return Result::fail (TRANS("The directory\nXZZX\n doesn't have write-access")
                                 .replace ("XZZX", recordedFile.getParentDirectory().getFullPathName()));
        }

        if (! recordedFile.deleteFile())
        {
            TRACKTION_LOG_ERROR ("Record fail: can't overwrite file: " + recordedFile.getFullPathName());

            return Result::fail (TRANS("Can't overwrite the existing file:") + "\n" + recordedFile.getFullPathName());
        }

        return Result::ok();
    }

    String prepareToRecord (double playStart, double punchIn, double sr, int blockSizeSamples, bool isLivePunch) override
    {
        CRASH_TRACER

        String error;
        inputBuffer.setSize (2, blockSizeSamples);

        JUCE_TRY
        {
            closeFileWriter();

            if (auto proj = ProjectManager::getInstance()->getProject (edit))
                if (proj->isReadOnly())
                    return TRANS("The current project is read-only, so new clips can't be recorded into it!");

            auto format = getFormatToUse();
            File recordedFile;

            auto res = getRecordingFile (recordedFile, *format);

            if (! res.wasOk())
                return res.getErrorMessage();

            auto rc = std::make_unique<RecordingContext> (edit.engine, recordedFile);
            rc->sampleRate = sr;

            StringPairArray metadata;
            AudioFileUtils::addBWAVStartToMetadata (metadata, (int64) (playStart * sr));
            auto& wi = getWaveInput();

            rc->fileWriter.reset (new AudioFileWriter (AudioFile (recordedFile), format,
                                                       wi.isStereoPair() ? 2 : 1,
                                                       sr, wi.bitDepth, metadata, 0));

            if (rc->fileWriter->isOpen())
            {
                CRASH_TRACER
                auto endRecTime = punchIn + Edit::maximumLength;
                auto punchInTime = punchIn;

                rc->firstRecCallback = true;
                muteTrackNow = false;

                auto adjustSeconds = wi.getAdjustmentSeconds();
                rc->adjustSamples = roundToInt (adjustSeconds * sr);

                if (! isLivePunch)
                {
                    rc->recordingWithPunch = edit.recordingPunchInOut;

                    if (rc->recordingWithPunch)
                    {
                        const auto loopRange = context.transport.getLoopRange();
                        punchInTime = jmax (punchInTime, loopRange.getStart() - 0.5);
                        auto muteStart = jmax (punchInTime, loopRange.getStart());
                        auto muteEnd = endRecTime;

                        if (edit.getNumCountInBeats() > 0 && context.playhead.getLoopTimes().start > loopRange.getStart())
                            punchInTime = context.playhead.getLoopTimes().start;

                        if (playStart < loopRange.getEnd() - 0.5)
                        {
                            endRecTime = loopRange.getEnd() + adjustSeconds + 0.8;
                            muteEnd    = loopRange.getEnd();
                        }

                        rc->muteTimes = { muteStart, muteEnd };
                    }
                    else if (context.playhead.isLooping())
                    {
                        punchInTime = context.playhead.getLoopTimes().start;
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

                const ScopedLock sl (contextLock);
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
        const ScopedLock sl (consumerLock);
        // This is probably where we should set up our recording context

        return true;
    }

    double getPunchInTime() override
    {
        const ScopedLock sl (contextLock);
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
        const ScopedLock sl (contextLock);

        if (recordingContext == nullptr)
            return {};

        return context.stopRecording (*this,
                                      { recordingContext->punchTimes.getStart(),
                                        context.playhead.getUnloopedPosition() },
                                      false);
    }

    void stop() override
    {
        {
            const ScopedLock sl (consumerLock);
            // This is probably where we should destroy our recording context and apply the recording
        }

        closeFileWriter();
    }

    void recordWasCancelled() override
    {
        std::unique_ptr<RecordingContext> rc;

        {
            const ScopedLock sl (contextLock);
            rc = std::move (recordingContext);
        }

        if (rc != nullptr)
        {
            const File f (rc->file);
            closeFileWriter (*rc);
            f.deleteFile();
        }
    }

    File getRecordingFile() const override
    {
        const ScopedLock sl (contextLock);

        if (recordingContext != nullptr)
            return recordingContext->file;

        return {};
    }

    struct RecordingContext
    {
        RecordingContext (Engine& e, const File& f)
            : engine (e), file (f), diskSpaceChecker (e, f),
              threadInitialiser (e.getWaveInputRecordingThread())
        {}

        Engine& engine;
        File file;
        double sampleRate = 44100.0;
        EditTimeRange punchTimes, muteTimes;
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

    Clip::Array applyLastRecordingToEdit (EditTimeRange recordedRange,
                                          bool isLooping, EditTimeRange loopRange,
                                          bool discardRecordings,
                                          SelectionManager* selectionManager) override
    {
        TRACKTION_ASSERT_MESSAGE_THREAD
        CRASH_TRACER

        std::unique_ptr<RecordingContext> rc;

        {
            const ScopedLock sl (contextLock);
            rc = std::move (recordingContext);
        }

        if (rc != nullptr)
        {
            closeFileWriter (*rc);

            if (! rc->file.existsAsFile() || rc->file.getSize() == 0)
                return {};

            const AudioFile recordedFile (rc->file);
            auto destTrack = getTargetTrack();

            if (discardRecordings || destTrack == nullptr)
            {
                recordedFile.deleteFile();
                return {};
            }

            auto clipsCreated = applyLastRecording (*rc, recordedFile, *destTrack,
                                                    recordedRange, isLooping, loopRange.end);

            if (selectionManager != nullptr && ! clipsCreated.isEmpty())
            {
                selectionManager->selectOnly (*clipsCreated.getLast());
                selectionManager->keepSelectedObjectsOnScreen();
            }

            return clipsCreated;
        }

        return {};
    }

    Clip::Array applyLastRecording (const RecordingContext& rc,
                                    const AudioFile& recordedFile, AudioTrack& destTrack,
                                    EditTimeRange recordedRange,
                                    bool isLooping, double loopEnd)
    {
        CRASH_TRACER
        auto& engine = edit.engine;
        auto& afm = engine.getAudioFileManager();
        afm.forceFileUpdate (recordedFile);

        auto recordedFileLength = recordedFile.getLength();

        if (recordedFileLength <= 0.00001)
            return {};

        auto newClipLen = jmin (recordedFileLength, recordedRange.getLength());

        if (newClipLen <= 0.01)
        {
            CRASH_TRACER
            String s;

            if (! rc.hasHitThreshold)
                s = TRANS("The device \"XZZX\" \nnever reached the trigger threshold set for recording (THRX).")
                      .replace ("XZZX", getWaveInput().getName())
                      .replace ("THRX", gainToDbString (dbToGain (getWaveInput().recordTriggerDb)));
            else if (edit.recordingPunchInOut && rc.punchTimes.getStart() >= recordedRange.end)
                s = TRANS("The device \"XZZX\" \nnever got as far as the punch-in marker, so didn't start recording!")
                      .replace ("XZZX", getWaveInput().getName());
            else
                s = TRANS("The device \"XZZX\" \nrecorded a zero-length file which won't be added to the edit")
                      .replace ("XZZX", getWaveInput().getName());

            engine.getUIBehaviour().showWarningMessage (s);

            recordedFile.deleteFile();
            return {};
        }

        if (auto proj = ProjectManager::getInstance()->getProject (edit))
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
            jassertfalse; // TODO
        }

        return {};
    }

    Clip::Array applyLastRecording (const RecordingContext& rc, const ProjectItem::Ptr& projectItem,
                                    const AudioFile& recordedFile, AudioTrack& destTrack,
                                    double recordedFileLength, double newClipLen,
                                    bool isLooping, double loopEnd)
    {
        CRASH_TRACER
        jassert (projectItem == nullptr || projectItem->getID().isValid());
        auto& engine = edit.engine;
        auto& afm = engine.getAudioFileManager();

        ReferenceCountedArray<ProjectItem> extraTakes;
        Array<File> filesCreated;
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

        double endPos = rc.punchTimes.getStart() + newClipLen;

        if (edit.recordingPunchInOut || context.transport.looping)
            endPos = jlimit (rc.punchTimes.getStart() + 0.5, loopEnd, endPos);

        Clip::Ptr newClip;
        bool replaceOldClips = getWaveInput().mergeMode == 1;
        const auto loopRange = edit.getTransport().getLoopRange();

        if (replaceOldClips && edit.recordingPunchInOut)
        {
            newClip = destTrack.insertWaveClip (getNewClipName (destTrack), projectItem->getID(),
                                                { { loopRange.getStart(), endPos }, 0.0 }, true);

            if (newClip != nullptr)
                newClip->setStart (rc.punchTimes.getStart(), false, false);
        }
        else
        {
            newClip = destTrack.insertWaveClip (getNewClipName (destTrack), projectItem->getID(),
                                                { { rc.punchTimes.getStart(), endPos }, 0.0 }, replaceOldClips);
        }

        if (newClip == nullptr)
        {
            String s ("Couldn't insert new clip after recording: ");
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
                 && newClip->getPosition().getStart() < loopRange.getEnd() - 0.1)
                newClip->setEnd (loopRange.getEnd(), true);
        }

        for (auto& f : filesCreated)
        {
            AudioFileUtils::applyBWAVStartTime (f, (int64) (newClip->getPosition().getStartOfSource() * rc.sampleRate));
            afm.forceFileUpdate (AudioFile (f));
        }

        if (auto wc = dynamic_cast<WaveAudioClip*> (newClip.get()))
            for (auto& take : extraTakes)
                wc->addTake (take->getID());

        Clip::Array clips;
        clips.add (newClip.get());
        return clips;
    }

    bool splitRecordingIntoMultipleTakes (const AudioFile& recordedFile, const ProjectItem::Ptr& projectItem, double recordedFileLength,
                                          ReferenceCountedArray<ProjectItem>& extraTakes, Array<File>& filesCreated)
    {
        auto& afm = edit.engine.getAudioFileManager();

        // break the wave into separate takes..
        extraTakes.add (projectItem);

        int take = 1;
        auto loopLength = context.transport.getLoopRange().getLength();

        for (;;)
        {
            EditTimeRange takeRange (Range<double> (loopLength * take,
                                                    jmin (loopLength * (take + 1), recordedFileLength)));

            if (takeRange.getLength() < 0.1)
                break;

            auto takeFile = recordedFile.getFile()
                             .getSiblingFile (recordedFile.getFile().getFileNameWithoutExtension() + "_take_" + String (take + 1))
                             .withFileExtension (recordedFile.getFile().getFileExtension())
                             .getNonexistentSibling (false);

            afm.releaseFile (recordedFile);

            if (AudioFileUtils::copySectionToNewFile (recordedFile.getFile(), takeFile, takeRange) < 0)
                return false;

            if (projectItem != nullptr)
            {
                if (auto takeObject = projectItem->getProject()->createNewItem (takeFile, ProjectItem::waveItemType(),
                                                                                recordedFile.getFile().getFileNameWithoutExtension()
                                                                                  + " #" + String (take + 1),
                                                                                {},
                                                                                ProjectItem::Category::recorded, true))
                {
                    extraTakes.add (takeObject);
                    filesCreated.add (takeFile);
                }
            }

            ++take;
        }

        // chop down the original wave file..
        auto tempFile = recordedFile.getFile().getNonexistentSibling (false);

        if (AudioFileUtils::copySectionToNewFile (recordedFile.getFile(), tempFile, EditTimeRange (0.0, loopLength)) > 0)
        {
            afm.releaseFile (recordedFile);
            tempFile.moveFileTo (recordedFile.getFile());
            filesCreated.add (recordedFile.getFile());
            afm.forceFileUpdate (recordedFile);
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

    Clip* applyRetrospectiveRecord (SelectionManager* selectionManager) override
    {
        auto dstTrack = getTargetTrack();

        if (dstTrack == nullptr)
            return nullptr;

        auto& wi = getWaveInput();

        auto recordBuffer = wi.getRetrospectiveRecordBuffer();

        if (recordBuffer == nullptr)
            return nullptr;

        auto format = getFormatToUse();
        File recordedFile;

        auto res = getRecordingFile (recordedFile, *format);

        if (res.failed())
            return nullptr;

        StringPairArray metadata;

        {
            AudioFileWriter writer (AudioFile (recordedFile), format,
                                    recordBuffer->numChannels,
                                    recordBuffer->sampleRate,
                                    wi.bitDepth, metadata, 0);

            if (writer.isOpen())
            {
                int numReady;
                juce::AudioBuffer<float> scratchBuffer (recordBuffer->numChannels, 1000);

                while ((numReady = recordBuffer->fifo.getNumReady()) > 0)
                {
                    auto toRead = jmin (numReady, scratchBuffer.getNumSamples());

                    if (! recordBuffer->fifo.read (scratchBuffer, 0, toRead)
                         || ! writer.appendBuffer (scratchBuffer, toRead))
                        return nullptr;
                }
            }
        }

        auto proj = ProjectManager::getInstance()->getProject (edit);

        if (proj == nullptr)
        {
            jassertfalse; // TODO
            return nullptr;
        }

        auto projectItem = proj->createNewItem (recordedFile, ProjectItem::waveItemType(),
                                                recordedFile.getFileNameWithoutExtension(),
                                                {}, ProjectItem::Category::recorded, true);

        if (projectItem == nullptr)
            return nullptr;

        jassert (projectItem->getID().isValid());

        auto clipName = getNewClipName (*dstTrack);
        double start = 0;
        double recordedLength = AudioFile (recordedFile).getLength();

        if (context.playhead.isPlaying() || recordBuffer->wasRecentlyPlaying (edit))
        {
            auto adjust = -wi.getAdjustmentSeconds() + edit.engine.getDeviceManager().getBlockSizeMs() / 1000.0;

            if (context.playhead.isPlaying())
            {
                start = context.playhead.streamTimeToSourceTime (recordBuffer->lastStreamTime) - recordedLength + adjust;
            }
            else
            {
                auto& pei = recordBuffer->editInfo[edit.getProjectItemID()];
                start = pei.lastEditTime + pei.pausedTime - recordedLength + adjust;
                pei.lastEditTime = -1;
            }
        }
        else
        {
            auto position = context.playhead.getPosition();

            if (position >= 5)
                start = position - recordedLength;
            else
                start = jmax (0.0, position);
        }

        ClipPosition clipPos = { { start, start + recordedLength }, 0.0 };

        if (start < 0)
        {
            clipPos.offset = -start;
            clipPos.time.start = 0;
        }

        auto newClip = dstTrack->insertWaveClip (clipName, projectItem->getID(), clipPos, false);

        if (newClip == nullptr)
            return nullptr;

        CRASH_TRACER

        AudioFileUtils::applyBWAVStartTime (recordedFile, (int64) (newClip->getPosition().getStartOfSource() * recordBuffer->sampleRate));
        edit.engine.getAudioFileManager().forceFileUpdate (AudioFile (recordedFile));

        if (selectionManager != nullptr)
        {
            selectionManager->selectOnly (*newClip);
            selectionManager->keepSelectedObjectsOnScreen();
        }

        return newClip.get();
    }

    bool isLivePlayEnabled() const override
    {
        return owner.isEndToEndEnabled()
                && isRecordingEnabled()
                && InputDeviceInstance::isLivePlayEnabled();
    }

    AudioNode* createLiveInputNode() override
    {
        return new InputAudioNode (*this);
    }

    void copyIncomingDataIntoBuffer (const float** allChannels, int numChannels, int numSamples)
    {
        if (numChannels == 0)
            return;

        auto& wi = getWaveInput();
        auto& channelSet = wi.getChannelSet();
        inputBuffer.setSize (channelSet.size(), numSamples);

        for (const auto& ci : wi.getChannels())
        {
            jassert (isPositiveAndBelow (ci.indexInDevice, numChannels));
            const int inputIndex = channelSet.getChannelIndexForType (ci.channel);
            FloatVectorOperations::copy (inputBuffer.getWritePointer (inputIndex), allChannels[ci.indexInDevice], numSamples);
        }
    }

    void acceptInputBuffer (const float** allChannels, int numChannels, int numSamples,
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
            const ScopedLock sl (consumerLock);

            for (auto n : consumers)
                n->acceptInputBuffer (inputBuffer, numSamples);
        }

        const ScopedLock sl (contextLock);

        if (recordingContext != nullptr)
        {
            auto blockStart = context.playhead.streamTimeToSourceTimeUnlooped (streamTime);
            const EditTimeRange blockRange (blockStart, blockStart + numSamples / recordingContext->sampleRate);

            muteTrackNow = recordingContext->muteTimes.overlaps (blockRange);

            if (recordingContext->punchTimes.overlaps (blockRange))
            {
                if (! recordingContext->hasHitThreshold)
                {
                    auto bufferLevelDb = gainToDb (inputBuffer.getMagnitude (0, numSamples));
                    recordingContext->hasHitThreshold = bufferLevelDb > getWaveInput().recordTriggerDb;

                    if (! recordingContext->hasHitThreshold)
                        return;

                    recordingContext->punchTimes.start = blockRange.getStart();

                    if (recordingContext->thumbnail != nullptr)
                        recordingContext->thumbnail->punchInTime = blockRange.getStart();
                }

                if (recordingContext->firstRecCallback)
                {
                    recordingContext->firstRecCallback = false;

                    auto timeDiff = blockRange.getStart() - recordingContext->punchTimes.getStart();
                    recordingContext->adjustSamples -= roundToInt (timeDiff * recordingContext->sampleRate);
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
    CriticalSection contextLock;
    std::unique_ptr<RecordingContext> recordingContext;

    volatile bool muteTrackNow = false;
    juce::AudioBuffer<float> inputBuffer;

    void addBlockToRecord (const juce::AudioBuffer<float>& buffer, int start, int numSamples)
    {
        const ScopedLock sl (contextLock);
        recordingContext->addBlockToRecord (buffer, start, numSamples);
    }

    static void closeFileWriter (RecordingContext& rc)
    {
        CRASH_TRACER
        auto localCopy = std::move (rc.fileWriter);

        if (localCopy != nullptr)
            rc.engine.getWaveInputRecordingThread().waitForWriterToFinish (*localCopy);
    }

    WaveInputDevice& getWaveInput() const noexcept    { return static_cast<WaveInputDevice&> (owner); }

    //==============================================================================
    class InputAudioNode  : public AudioNode
    {
    public:
        InputAudioNode (WaveInputDeviceInstance& d)
            : owner (d),
              buffer (owner.getWaveInput().isStereoPair() ? 2 : 1, 512)
        {
            sampleRate = owner.edit.engine.getDeviceManager().getSampleRate();
        }

        ~InputAudioNode()
        {
            releaseAudioNodeResources();
        }

        void getAudioNodeProperties (AudioNodeProperties& info) override
        {
            info.hasAudio = true;
            info.hasMidi = false;
            info.numberOfChannels = (owner.getWaveInput().isStereoPair()) ? 2 : 1;

            buffer.setSize (info.numberOfChannels, 512);
        }

        void visitNodes (const VisitorFn& v) override
        {
            v (*this);
        }

        void prepareAudioNodeToPlay (const PlaybackInitialisationInfo& info) override
        {
            sampleRate = info.sampleRate;
            validSamples = 0;
            lastCallbackTime = Time::getMillisecondCounter();
            buffer.setSize (buffer.getNumChannels(), info.blockSizeSamples * 8);

            owner.addConsumer (this);
        }

        bool isReadyToRender() override
        {
            return true;
        }

        void releaseAudioNodeResources() override
        {
            owner.removeConsumer (this);
        }

        bool purgeSubNodes (bool keepAudio, bool) override
        {
            return keepAudio;
        }

        void renderOver (const AudioRenderContext& rc) override
        {
            callRenderAdding (rc);
        }

        void renderAdding (const AudioRenderContext& rc) override
        {
            auto now = Time::getMillisecondCounter();

            if (now > lastCallbackTime + 150)
            {
                // if there's been a break in transmission, reset the valid samps to keep in in sync
                const ScopedLock sl (bufferLock);
                validSamples = 0;
            }

            lastCallbackTime = now;

            if (rc.destBuffer != nullptr)
            {
                const ScopedLock sl (bufferLock);

                const int numToDo = jmin (rc.bufferNumSamples, validSamples);

                for (int i = jmin (2, rc.destBuffer->getNumChannels()); --i >= 0;)
                {
                    const int srcChan = jmin (buffer.getNumChannels() - 1, i);
                    rc.destBuffer->addFrom (i, rc.bufferStartSample, buffer, srcChan, 0, numToDo);
                }

                validSamples = jmax (0, validSamples - numToDo);

                if (validSamples > numToDo)
                {
                    validSamples = 0;
                }
                else
                {
                    for (int i = buffer.getNumChannels(); --i >= 0;)
                    {
                        float* d = buffer.getWritePointer (i, 0);
                        const float* s = buffer.getReadPointer (i, numToDo);

                        for (int j = 0; j < validSamples; ++j)
                            d[j] = s[j];
                    }
                }
            }
        }

        void acceptInputBuffer (juce::AudioBuffer<float>& newBuffer, int newSamps)
        {
            const ScopedLock sl (bufferLock);

            if (validSamples > buffer.getNumSamples() - newSamps)
                validSamples = 0;

            for (int i = buffer.getNumChannels(); --i >= 0;)
                buffer.copyFrom (i, validSamples,
                                 newBuffer, jmin (newBuffer.getNumChannels() - 1, i), 0,
                                 newSamps);

            validSamples += newSamps;
        }

    private:
        WaveInputDeviceInstance& owner;
        double sampleRate = 0;
        CriticalSection bufferLock;
        int validSamples = 0;
        uint32 lastCallbackTime = 0;
        juce::AudioBuffer<float> buffer;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InputAudioNode)
    };

    Array<InputAudioNode*> consumers;
    CriticalSection consumerLock;

    void addConsumer (InputAudioNode* consumer)
    {
        const ScopedLock sl (consumerLock);
        consumers.addIfNotAlreadyThere (consumer);
    }

    void removeConsumer (InputAudioNode* consumer)
    {
        const ScopedLock sl (consumerLock);
        consumers.removeAllInstancesOf (consumer);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaveInputDeviceInstance)
};

//==============================================================================
WaveInputDevice::WaveInputDevice (Engine& e, const juce::String& name, const juce::String& type,
                                  const std::vector<ChannelIndex>& channels, DeviceType t)
    : InputDevice (e, type, name),
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

StringArray WaveInputDevice::getMergeModes()
{
    StringArray s;
    s.add (TRANS("Overlay newly recorded clips onto edit"));
    s.add (TRANS("Replace old clips in edit with new ones"));
    s.add (TRANS("Don't make recordings from this device"));

    return s;
}

StringArray WaveInputDevice::getRecordFormatNames()
{
    StringArray s;

    auto& afm = engine.getAudioFileFormatManager();
    s.add (afm.getWavFormat()->getFormatName());
    s.add (afm.getAiffFormat()->getFormatName());
    s.add (afm.getFlacFormat()->getFormatName());

    return s;
}


InputDeviceInstance* WaveInputDevice::createInstance (EditPlaybackContext& ed)
{
    if (! isTrackDevice() && retrospectiveBuffer == nullptr)
        retrospectiveBuffer = new RetrospectiveRecordBuffer (ed.edit.engine);

    return new WaveInputDeviceInstance (*this, ed);
}

void WaveInputDevice::resetToDefault()
{
    const String propName = isTrackDevice() ? "TRACKTION_TRACK_DEVICE" : getName();
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

String WaveInputDevice::openDevice()
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

    const String propName = isTrackDevice() ? "TRACKTION_TRACK_DEVICE" : getName();

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
            TRACKTION_LOG ("Manual record adjustment: " + String (recordAdjustMs) + "ms");
    }

    checkBitDepth();
}

void WaveInputDevice::saveProps()
{
    XmlElement n ("SETTINGS");

    n.setAttribute ("filename", filenameMask);
    n.setAttribute ("gainDb", inputGainDb);
    n.setAttribute ("etoe", endToEndEnabled);
    n.setAttribute ("format", outputFormat);
    n.setAttribute ("bits", bitDepth);
    n.setAttribute ("triggerDb", recordTriggerDb);
    n.setAttribute ("mode", mergeMode);
    n.setAttribute ("adjustMs", recordAdjustMs);

    const String propName = isTrackDevice() ? "TRACKTION_TRACK_DEVICE" : getName();

    engine.getPropertyStorage().setXmlPropertyItem (SettingID::wavein, propName, n);
}

//==============================================================================
String WaveInputDevice::getSelectableDescription()
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
        dm.setDeviceInChannelStereo (jmax (deviceChannels[0].indexInDevice, deviceChannels[1].indexInDevice), stereo);
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
    recordAdjustMs = jlimit (-500.0, 500.0, ms);
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

String WaveInputDevice::getDefaultMask()
{
    String defaultFile;
    defaultFile << projDirPattern << File::getSeparatorChar() << editPattern << '_'
                << trackPattern<< '_'  << TRANS("Take") << '_' << takePattern;

    return defaultFile;
}

void WaveInputDevice::setFilenameMask (const String& newMask)
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

Array<int> WaveInputDevice::getAvailableBitDepths() const
{
    return getFormatToUse()->getPossibleBitDepths();
}

AudioFormat* WaveInputDevice::getFormatToUse() const
{
    return engine.getAudioFileFormatManager().getNamedFormat (outputFormat);
}

void WaveInputDevice::setOutputFormat (const String& newFormat)
{
    if (outputFormat != newFormat)
    {
        outputFormat = newFormat;
        checkBitDepth();
        changed();
        saveProps();
    }
}

String WaveInputDevice::getMergeMode() const
{
    return getMergeModes() [mergeMode];
}

void WaveInputDevice::setMergeMode (const String& newMode)
{
    int newIndex = getMergeModes().indexOf (newMode);

    if (mergeMode != newIndex)
    {
        mergeMode = newIndex;
        changed();
        saveProps();
    }
}

double WaveInputDevice::getAdjustmentSeconds()
{
    auto& dm = engine.getDeviceManager();
    const double autoAdjustMs = isTrackDevice() ? 0.0 : dm.getRecordAdjustmentMs();

    return jlimit (-3.0, 3.0, 0.001 * (recordAdjustMs + autoAdjustMs));
}

//==============================================================================
void WaveInputDevice::addInstance (WaveInputDeviceInstance* i)
{
    const ScopedLock sl (instanceLock);
    instances.addIfNotAlreadyThere (i);
}

void WaveInputDevice::removeInstance (WaveInputDeviceInstance* i)
{
    const ScopedLock sl (instanceLock);
    instances.removeAllInstancesOf (i);
}

//==============================================================================
void WaveInputDevice::consumeNextAudioBlock (const float** allChannels, int numChannels, int numSamples, double streamTime)
{
    if (enabled)
    {
        bool isFirst = true;
        const ScopedLock sl (instanceLock);

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
class WaveInputDevice::WaveInputDeviceAudioNode : public SingleInputAudioNode
{
public:
    WaveInputDeviceAudioNode (AudioNode* input, WaveInputDevice& d)
        : SingleInputAudioNode (input),
          owner (d)
    {
    }

    ~WaveInputDeviceAudioNode()
    {
        releaseAudioNodeResources();
    }

    void prepareAudioNodeToPlay (const PlaybackInitialisationInfo& info) override
    {
        if (input != nullptr)
            input->prepareAudioNodeToPlay (info);

        previousBuffer.setSize (2, info.blockSizeSamples * 2);
        previousBuffer.clear();

        if (info.sampleRate > 0.0)
            offsetSeconds = info.blockSizeSamples / info.sampleRate;

        sampleRate = info.sampleRate;
    }

    void prepareForNextBlock (const AudioRenderContext& rc) override
    {
        if (input != nullptr)
            input->prepareForNextBlock (rc);
    }

    void renderOver (const AudioRenderContext& rc) override
    {
        callRenderAdding (rc);
    }

    void renderAdding (const AudioRenderContext& rc) override
    {
        if (rc.destBuffer == nullptr)
            return;

        // Use a temp buffer so the previous track's contents aren't included
        AudioScratchBuffer scratch (*rc.destBuffer);
        scratch.buffer.clear();

        AudioRenderContext rc2 (rc);
        rc2.destBuffer = &scratch.buffer;
        rc2.bufferStartSample = 0;

        renderContent (rc2);

        for (int i = rc.destBuffer->getNumChannels(); --i >= 0;)
            rc.destBuffer->addFrom (i, rc.bufferStartSample,
                                    scratch.buffer, i, 0,
                                    rc.bufferNumSamples);
    }

    void releaseAudioNodeResources() override
    {
        if (input != nullptr)
            input->releaseAudioNodeResources();

        previousBuffer.setSize (2, 32);
        offsetSeconds = 0.0;
    }

private:
    WaveInputDevice& owner;
    juce::AudioBuffer<float> previousBuffer;
    double sampleRate = 0.0;
    double offsetSeconds = 0.0;

    void renderContent (const AudioRenderContext& rc)
    {
        if (input != nullptr)
            input->renderAdding (rc);

        const int numChans = jmin (2, previousBuffer.getNumChannels());
        const float* chans[3] = { previousBuffer.getReadPointer (0, 0),
                                  numChans > 1 ? previousBuffer.getReadPointer (1, 0) : nullptr,
                                  nullptr };
        owner.consumeNextAudioBlock (chans, numChans, rc.bufferNumSamples, rc.streamTime.getStart() - offsetSeconds);

        for (int i = numChans; --i >= 0;)
            previousBuffer.copyFrom (i, 0,
                                     *rc.destBuffer, i, rc.bufferStartSample,
                                     rc.bufferNumSamples);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaveInputDeviceAudioNode)
};

AudioNode* WaveInputDevice::createWaveInputDeviceNode (AudioNode* input)
{
    return new WaveInputDeviceAudioNode (input, *this);
}

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
        QueuedBlock() {}

        void load (AudioFileWriter& w, const juce::AudioBuffer<float>& newBuffer,
                   int start, int numSamples, const RecordingThumbnailManager::Thumbnail::Ptr& thumb)
        {
            buffer.setSize (newBuffer.getNumChannels(), numSamples);

            for (int i = buffer.getNumChannels(); --i >= 0;)
                buffer.copyFrom (i, 0, newBuffer, i, start, numSamples);

            writer = &w;
            thumbnail = thumb;
        }

        AudioFileWriter* writer = nullptr;
        QueuedBlock* next = nullptr;
        juce::AudioBuffer<float> buffer { 2, 512 };
        RecordingThumbnailManager::Thumbnail::Ptr thumbnail;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (QueuedBlock)
    };

    CriticalSection freeQueueLock, pendingQueueLock;
    QueuedBlock* firstPending = nullptr;
    QueuedBlock* lastPending = nullptr;
    QueuedBlock* firstFree = nullptr;
    int numPending = 0;

    QueuedBlock* findFreeBlock()
    {
        const ScopedLock sl (freeQueueLock);

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
        const ScopedLock sl (freeQueueLock);
        b->next = firstFree;
        firstFree = b;
    }

    void addToPendingQueue (QueuedBlock* b) noexcept
    {
        jassert (b != nullptr);
        b->next = nullptr;
        const ScopedLock sl (pendingQueueLock);

        if (lastPending != nullptr)
            lastPending->next = b;
        else
            firstPending = b;

        lastPending = b;
        ++numPending;
    }

    QueuedBlock* removeFirstPending() noexcept
    {
        const ScopedLock sl (pendingQueueLock);

        if (auto* b = firstPending)
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
        while (auto* b = removeFirstPending())
            addToFreeQueue (b);

        jassert (numPending == 0);
        numPending = 0;
    }

    bool isWriterInQueue (AudioFileWriter& writer) const
    {
        const ScopedLock sl (pendingQueueLock);

        for (auto* b = firstPending; b != nullptr; b = b->next)
            if (b->writer == &writer)
                return true;

        return false;
    }

    void deleteFreeQueue() noexcept
    {
        auto* b = firstFree;
        firstFree = nullptr;

        while (b != nullptr)
        {
            ScopedPointer<QueuedBlock> toDelete (b);
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
        auto* block = queue->findFreeBlock();
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
    FloatVectorOperations::disableDenormalisedNumberSupport();

    for (;;)
    {
        if (queue->numPending > 500 && ! hasWarned)
        {
            hasWarned = true;
            TRACKTION_LOG_ERROR ("Audio recording can't keep up!");
        }

        if (auto* block = queue->removeFirstPending())
        {
            if (! block->writer->appendBuffer (block->buffer, block->buffer.getNumSamples()))
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
    startThread (5);
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
