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

static inline HashCode getAudioFileHash (const juce::File& file) noexcept
{
    return file.getFullPathName().hashCode64();
}

//==============================================================================
AudioFile::AudioFile (Engine& e, const juce::File& f) noexcept
    : engine (&e), file (f), hash (getAudioFileHash (f))
{
}

AudioFile::AudioFile (const AudioFile& other) noexcept
    : engine (other.engine), file (other.file), hash (other.hash)
{
}

AudioFile& AudioFile::operator= (const AudioFile& other) noexcept
{
    file = other.file;
    hash = other.hash;
    return *this;
}

AudioFile::~AudioFile() {}

AudioFileInfo AudioFile::getInfo() const
{
    CRASH_TRACER

    if (file == juce::File())
        return AudioFileInfo (*engine);

    return engine->getAudioFileManager().getInfo (*this);
}

juce::String AudioFileInfo::getLongDescription() const
{
    juce::String desc;

    if (sampleRate > 0.0)
    {
        desc << juce::String ((sampleRate / 1000.0) + 0.0001, 1) << " kHz, ";

        if (bitsPerSample > 0) desc << bitsPerSample << " bit ";

        desc << (numChannels == 1 ? TRANS("mono") : TRANS("stereo"))
             << ", " << juce::RelativeTime (getLengthInSeconds()).getDescription();

        juce::StringArray items;

        const int numBeats = juce::roundToInt (loopInfo.getNumBeats());
        if (numBeats == 1)
            items.add (TRANS("1 beat"));
        else if (numBeats > 1)
            items.add (TRANS("123 beats").replace ("123", juce::String (numBeats)));

        if (loopInfo.isLoopable())
        {
            double bpm = loopInfo.getNumBeats() / (getLengthInSeconds() / 60.0);
            items.add (juce::String (bpm, 1) + " bpm");
        }

        if (loopInfo.getNumerator() && loopInfo.getDenominator())
            items.add (juce::String (loopInfo.getNumerator()) + "/" + juce::String (loopInfo.getDenominator()));

        if (loopInfo.getRootNote() != -1)
            items.add (juce::MidiMessage::getMidiNoteName (loopInfo.getRootNote(), true, true,
                                                           engine->getEngineBehaviour().getMiddleCOctave()));

        if (items.size() > 0)
            desc << "\n" << items.joinIntoString (", ");
    }

    return desc;
}

bool AudioFile::isValid() const                         { return hash != 0 && getSampleRate() > 0; }
SampleCount AudioFile::getLengthInSamples() const       { return getInfo().lengthInSamples; }
double AudioFile::getLength() const                     { return getInfo().getLengthInSeconds(); }
int AudioFile::getNumChannels() const                   { return getInfo().numChannels; }
double AudioFile::getSampleRate() const                 { return getInfo().sampleRate; }
int AudioFile::getBitsPerSample() const                 { return getInfo().bitsPerSample; }
bool AudioFile::isFloatingPoint() const                 { return getInfo().isFloatingPoint; }
juce::StringPairArray AudioFile::getMetadata() const    { return getInfo().metadata; }
juce::AudioFormat* AudioFile::getFormat() const         { return getInfo().format; }

bool AudioFile::moveToTrash() const
{
    CRASH_TRACER

    auto& afm = engine->getAudioFileManager();
    afm.checkFileForChangesAsync (*this);
    afm.releaseFile (*this);

    return file.moveToTrash();
}

bool AudioFile::deleteFile() const
{
    CRASH_TRACER

    auto& afm = engine->getAudioFileManager();
    afm.checkFileForChangesAsync (*this);
    afm.releaseFile (*this);

    bool ok = file.deleteFile();
    jassert (ok);
    return ok;
}

bool AudioFile::deleteFiles (Engine& engine, const juce::Array<juce::File>& files)
{
    bool allOK = true;

    for (auto& f : files)
        if (! AudioFile (engine, f).deleteFile())
            allOK = false;

    return allOK;
}

bool AudioFile::isWavFile() const               { return file.hasFileExtension ("wav;bwav;bwf"); }
bool AudioFile::isAiffFile() const              { return file.hasFileExtension ("aiff;aif"); }
bool AudioFile::isOggFile() const               { return file.hasFileExtension ("ogg"); }
bool AudioFile::isMp3File() const               { return file.hasFileExtension ("mp3"); }
bool AudioFile::isFlacFile() const              { return file.hasFileExtension ("flac"); }
bool AudioFile::isRexFile() const               { return file.hasFileExtension ("rex;rx2;rcy"); }


//==============================================================================
const int numSamplesPerFlush = 48000 * 6;

AudioFileWriter::AudioFileWriter (const AudioFile& f,
                                  juce::AudioFormat* formatToUse,
                                  int numChannels,
                                  double sampleRate,
                                  int bitsPerSample,
                                  const juce::StringPairArray& metadata,
                                  int quality)
    : file (f), samplesUntilFlush (numSamplesPerFlush)
{
    CRASH_TRACER
    f.engine->getAudioFileManager().releaseFile (file);

    if (file.getFile().getParentDirectory().createDirectory())
    {
        const juce::ScopedLock sl (writerLock);
        writer.reset (AudioFileUtils::createWriterFor (formatToUse, file.getFile(), sampleRate,
                                                       (unsigned int) numChannels, bitsPerSample,
                                                       metadata, quality));
    }
}

AudioFileWriter::~AudioFileWriter()
{
    closeForWriting();
}

bool AudioFileWriter::isOpen() const noexcept               { const juce::ScopedLock sl (writerLock); return writer != nullptr; }
double AudioFileWriter::getSampleRate() const noexcept      { jassert (isOpen()); const juce::ScopedLock sl (writerLock); return writer->getSampleRate(); }
int AudioFileWriter::getNumChannels() const noexcept        { jassert (isOpen()); const juce::ScopedLock sl (writerLock); return writer->getNumChannels(); }

void AudioFileWriter::closeForWriting()
{
    {
        const juce::ScopedLock sl (writerLock);
        writer.reset();
    }
    
    auto& audioFileManager = file.engine->getAudioFileManager();
    audioFileManager.releaseFile (file);
    audioFileManager.checkFileForChanges (file);
}

bool AudioFileWriter::appendBuffer (juce::AudioBuffer<float>& buffer, int num)
{
    num = std::min (num, buffer.getNumSamples());
    const juce::ScopedLock sl (writerLock);

    if (writer != nullptr && writer->writeFromAudioSampleBuffer (buffer, 0, num))
    {
        samplesUntilFlush -= num;

        if (samplesUntilFlush <= 0)
        {
            samplesUntilFlush = numSamplesPerFlush;
            writer->flush();
        }

        return true;
    }

    return false;
}

bool AudioFileWriter::appendBuffer (const int** buffer, int num)
{
    const juce::ScopedLock sl (writerLock);
    return writer != nullptr && writer->write (buffer, num);
}

bool AudioFileWriter::writeFromAudioReader (juce::AudioFormatReader& reader,
                                            SampleCount startSample,
                                            SampleCount numSamples)
{
    const juce::ScopedLock sl (writerLock);
    return writer != nullptr && writer->writeFromAudioReader (reader, startSample, numSamples);
}

//==============================================================================
AudioProxyGenerator::GeneratorJob::GeneratorJob (const AudioFile& p)
    : ThreadPoolJobWithProgress ("proxy"), proxy (p)
{
}

AudioProxyGenerator::GeneratorJob::~GeneratorJob()
{
    prepareForJobDeletion();
    callBlocking ([this] { proxy.engine->getAudioFileManager().validateFile (proxy, false); });
}

juce::ThreadPoolJob::JobStatus AudioProxyGenerator::GeneratorJob::runJob()
{
    CRASH_TRACER

    auto& afm = proxy.engine->getAudioFileManager();
    juce::FloatVectorOperations::disableDenormalisedNumberSupport();
    proxy.deleteFile();

    if (render())
        afm.checkFileForChangesAsync (proxy);
    else
        proxy.deleteFile();

    progress = 1.0f;

    afm.proxyGenerator.removeFinishedJob (this);
    return jobHasFinished;
}

//==============================================================================
AudioProxyGenerator::AudioProxyGenerator()
{
}

AudioProxyGenerator::~AudioProxyGenerator()
{
    CRASH_TRACER
}

AudioProxyGenerator::GeneratorJob* AudioProxyGenerator::findJob (const AudioFile& proxy) const noexcept
{
    for (auto j : activeJobs)
        if (j->proxy == proxy)
            return j;

    return {};
}

static bool checkProxyStatus (const AudioFile& f)
{
    if (f.getFile().existsAsFile())
    {
        if (f.isValid())
            return true;

        f.deleteFile();
    }

    return false;
}

void AudioProxyGenerator::beginJob (GeneratorJob* j)
{
    CRASH_TRACER
    std::unique_ptr<GeneratorJob> job (j);

    if (! checkProxyStatus (job->proxy))
    {
        const juce::ScopedLock sl (jobListLock);

        if (findJob (job->proxy) == nullptr)
        {
            job->proxy.engine->getBackgroundJobs().addJob (j, true);
            activeJobs.add (job.release());
        }
    }
}

bool AudioProxyGenerator::isProxyBeingGenerated (const AudioFile& proxyFile) const noexcept
{
    const juce::ScopedLock sl (jobListLock);
    return findJob (proxyFile) != nullptr;
}

float AudioProxyGenerator::getProportionComplete (const AudioFile& proxyFile) const noexcept
{
    const juce::ScopedLock sl (jobListLock);

    if (auto j = findJob (proxyFile))
        return j->progress;

    return 1.0f;
}

void AudioProxyGenerator::removeFinishedJob (GeneratorJob* j)
{
    const juce::ScopedLock sl (jobListLock);
    activeJobs.removeAllInstancesOf (j);
}

void AudioProxyGenerator::deleteProxy (const AudioFile& proxyFile)
{
    CRASH_TRACER
    GeneratorJob* j = nullptr;

    {
        const juce::ScopedLock sl (jobListLock);
        j = findJob (proxyFile);
    }

    if (j != nullptr)
        proxyFile.engine->getBackgroundJobs().removeJob (j, true, 10000);

    proxyFile.deleteFile();
}


//==============================================================================
AudioFileInfo::AudioFileInfo (Engine& e)
    : engine (&e), loopInfo (e)
{
}

AudioFileInfo::AudioFileInfo (const AudioFile& file, juce::AudioFormatReader* reader, juce::AudioFormat* f)
    : engine (file.engine), hashCode (file.getHash()), format (f),
      fileModificationTime (file.getFile().getLastModificationTime()),
      loopInfo (*file.engine, reader, f)
{
    if (reader != nullptr)
    {
        wasParsedOk     = true;
        sampleRate      = reader->sampleRate;
        lengthInSamples = reader->lengthInSamples;
        numChannels     = (int) reader->numChannels;
        bitsPerSample   = (int) reader->bitsPerSample;
        isFloatingPoint = reader->usesFloatingPointData;
        needsCachedProxy = dynamic_cast<juce::WavAudioFormat*> (format) == nullptr
                              && dynamic_cast<juce::AiffAudioFormat*> (format) == nullptr
                              && dynamic_cast<FloatAudioFormat*> (format) == nullptr;
        metadata = reader->metadataValues;
    }
    else
    {
        wasParsedOk = false;
        format = nullptr;
        sampleRate = 0;
        lengthInSamples = 0;
        numChannels = 0;
        bitsPerSample = 0;
        isFloatingPoint = false;
        needsCachedProxy = false;
    }
}

AudioFileInfo AudioFileInfo::parse (const AudioFile& file)
{
    if (! file.isNull())
    {
        juce::AudioFormat* format = nullptr;

        if (auto reader = std::unique_ptr<juce::AudioFormatReader> (AudioFileUtils::createReaderFindingFormat (*file.engine, file.getFile(), format)))
            return AudioFileInfo (file, reader.get(), format);
    }

    return AudioFileInfo (file, nullptr, nullptr);
}

//==============================================================================
class TracktionThumbnailCache  : public juce::AudioThumbnailCache
{
public:
    TracktionThumbnailCache (Engine& e)
       #if JUCE_DEBUG
        : juce::AudioThumbnailCache (3)
       #else
        : juce::AudioThumbnailCache (50)
       #endif
        , engine (e)
    {
    }

    void saveNewlyFinishedThumbnail (const juce::AudioThumbnailBase& thumb, juce::int64 hash) override
    {
        CRASH_TRACER
        auto st = dynamic_cast<const SmartThumbnail*> (&thumb);
        auto thumbFile = getThumbFile (st, hash);

        if (thumbFile.deleteFile())
        {
            thumbFile.getParentDirectory().createDirectory();

            juce::FileOutputStream fo (thumbFile);

            if (! fo.openedOk())
                return;

            thumb.saveTo (fo);
        }
    }

    bool loadNewThumb (juce::AudioThumbnailBase& thumb, juce::int64 hash) override
    {
        CRASH_TRACER
        auto st = dynamic_cast<const SmartThumbnail*> (&thumb);
        auto thumbFile = getThumbFile (st, hash);

        if (st != nullptr
              && st->file.getFile().getLastModificationTime() > thumbFile.getLastModificationTime()
                                                                  + juce::RelativeTime::seconds (0.1))
        {
            thumbFile.deleteFile();
            return false;
        }

        juce::FileInputStream fin (thumbFile);
        return fin.openedOk() && thumb.loadFrom (fin);
    }

private:
    Engine& engine;

    juce::File getThumbFolder (Edit* edit) const
    {
        if (edit != nullptr)
            return edit->getTempDirectory (false);

        return engine.getTemporaryFileManager().getThumbnailsFolder();
    }

    juce::File getThumbFile (const SmartThumbnail* st, HashCode hash) const
    {
        auto thumbFolder = getThumbFolder (st != nullptr ? st->edit : nullptr);

        return thumbFolder.getChildFile ("thumbnail_" + juce::String::toHexString (hash) + ".thumb");
    }
};

//==============================================================================
struct AudioFileManager::KnownFile
{
    KnownFile (const AudioFile& f)
        : file (f), info (AudioFileInfo::parse (file))
    {
    }

    AudioFile file;
    AudioFileInfo info;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KnownFile)
};

//==============================================================================
enum { initialTimerDelay = 10 };

bool SmartThumbnail::enabled = true;

SmartThumbnail::SmartThumbnail (Engine& e, const AudioFile& f, juce::Component& componentToRepaint, Edit* ed)
    : TracktionThumbnail (256, e.getAudioFileFormatManager().readFormatManager,
                          e.getAudioFileManager().getAudioThumbnailCache()),
      file (f), engine (e), edit (ed), component (componentToRepaint)
{
    TRACKTION_ASSERT_MESSAGE_THREAD
    startTimer (initialTimerDelay);
    engine.getAudioFileManager().activeThumbnails.add (this);
}

SmartThumbnail::~SmartThumbnail()
{
    TRACKTION_ASSERT_MESSAGE_THREAD

    engine.getAudioFileManager().activeThumbnails.removeAllInstancesOf (this);
    clear();
}

bool SmartThumbnail::areThumbnailsFullyLoaded (Engine& engine)
{
    TRACKTION_ASSERT_MESSAGE_THREAD

    for (auto thumb : engine.getAudioFileManager().activeThumbnails)
        if (! thumb->isFullyLoaded())
            return false;
    
    return true;
}

void SmartThumbnail::setNewFile (const AudioFile& newFile)
{
    if (file != newFile)
    {
        file = newFile;

        audioFileChanged();
        component.repaint();
    }
}

void SmartThumbnail::releaseFile()
{
    clear();
    thumbnailIsInvalid = true;
    juce::MessageManager::callAsync ([ref = juce::WeakReference<SmartThumbnail> (this), this]() mutable
                                     {
                                         if (ref != nullptr)
                                             startTimer (400);
                                     });
}

void SmartThumbnail::createThumbnailReader()
{
    // this breaks thumbnails in 64-bit mode
    // setReader (new CacheAudioFormatReader (file), file.getHash());

    if (enabled)
    {
        setReader (AudioFileUtils::createReaderFor (engine, file.getFile()), file.getHash());
        thumbnailIsInvalid = false;
    }
    else
    {
        thumbnailIsInvalid = true;
    }
}

void SmartThumbnail::audioFileChanged()
{
    CRASH_TRACER
    auto& proxyGen = engine.getAudioFileManager().proxyGenerator;

    wasGeneratingProxy = proxyGen.isProxyBeingGenerated (file);

    releaseResources();

    if (file.getFile().exists())
        createThumbnailReader();
    else
        thumbnailIsInvalid = true;

    lastProgress = 0.0f;
    component.repaint();
    startTimer (200);
}

void SmartThumbnail::timerCallback()
{
    CRASH_TRACER

    auto& afm = engine.getAudioFileManager();
    auto& proxyGen = afm.proxyGenerator;

    if (getTimerInterval() == initialTimerDelay)
        audioFileChanged();

    const bool isGeneratingNow = proxyGen.isProxyBeingGenerated (file);

    if (wasGeneratingProxy != isGeneratingNow || (thumbnailIsInvalid && file.getFile().exists()))
    {
        wasGeneratingProxy = isGeneratingNow;

        if (! isGeneratingNow)
        {
            afm.checkFileForChanges (file);
            createThumbnailReader();
        }
        else
        {
            thumbnailIsInvalid = true;
        }

        component.repaint();
    }

    if (isGeneratingNow || ! isFullyLoaded())
    {
        float progress = isGeneratingNow ? proxyGen.getProportionComplete (file)
                                         : (float) getProportionComplete();

        if (lastProgress != progress)
        {
            lastProgress = progress;
            component.repaint();
        }
    }
    else if (! thumbnailIsInvalid || ! file.getFile().exists())
    {
        component.repaint();
        stopTimer();
    }
}

//==============================================================================
AudioFileManager::AudioFileManager (Engine& e)
    : engine (e), cache (e), thumbnailCache (new TracktionThumbnailCache (e))
{
}

AudioFileManager::~AudioFileManager()
{
    clearFiles();
}

AudioFileManager::KnownFile& AudioFileManager::findOrCreateKnown (const AudioFile& f)
{
    auto hash = f.getHash();
    auto kf = knownFiles.find (hash);

    if (kf != knownFiles.end())
        return *kf->second.get();

    knownFiles[hash] = std::make_unique<KnownFile> (f);
    return *knownFiles[hash].get();
}

void AudioFileManager::clearFiles()
{
    CRASH_TRACER
    const juce::ScopedLock sl (knownFilesLock);
    knownFiles.clear();
}

void AudioFileManager::removeFile (HashCode hash)
{
    const juce::ScopedLock sl (knownFilesLock);

    auto f = knownFiles.find (hash);

    if (f != knownFiles.end())
        knownFiles.erase (f);
}

AudioFile AudioFileManager::getAudioFile (ProjectItemID sourceID)
{
    return AudioFile (engine, engine.getProjectManager().findSourceFile (sourceID));
}

AudioFileInfo AudioFileManager::getInfo (const AudioFile& file)
{
    const juce::ScopedLock sl (knownFilesLock);
    return findOrCreateKnown (file).info;
}

bool AudioFileManager::checkFileTime (KnownFile& f)
{
    if (! f.info.wasParsedOk
        || f.info.fileModificationTime != f.file.getFile().getLastModificationTime())
    {
        f.info = AudioFileInfo::parse (f.file);
        return true;
    }

    return false;
}

void AudioFileManager::checkFileForChanges (const AudioFile& file)
{
    CRASH_TRACER

    bool changed = false;

    {
        const juce::ScopedLock sl (knownFilesLock);

        auto f = knownFiles.find (file.getHash());

        if (f != knownFiles.end())
            changed = checkFileTime (*f->second);
    }

    if (changed)
    {
        releaseFile (file);
        callListenersOnMessageThread (file);
    }
}

void AudioFileManager::checkFilesForChanges()
{
    TRACKTION_ASSERT_MESSAGE_THREAD

    juce::Array<AudioFile> changedFiles;

    {
        const juce::ScopedLock sl (knownFilesLock);

        for (auto& f : knownFiles)
            if (checkFileTime (*f.second))
                changedFiles.add (f.second->file);
    }

    for (auto& f : changedFiles)
    {
        releaseFile (f);
        callListeners (f);
    }
}

void AudioFileManager::releaseAllFiles()
{
    cache.releaseAllFiles();

    const juce::ScopedLock sl (activeThumbnailLock);

    for (auto t : activeThumbnails)
        t->releaseFile();
}

void AudioFileManager::releaseFile (const AudioFile& file)
{
    cache.releaseFile (file);

    const juce::ScopedLock sl (activeThumbnailLock);

    for (auto t : activeThumbnails)
        if (t->file == file)
            t->releaseFile();
}

void AudioFileManager::callListeners (const AudioFile& file)
{
    CRASH_TRACER
    TRACKTION_ASSERT_MESSAGE_THREAD

    thumbnailCache->removeThumb (file.getHash());

    const juce::ScopedLock sl (activeThumbnailLock);

    for (auto t : activeThumbnails)
        if (t->file == file)
            t->audioFileChanged();
}

void AudioFileManager::callListenersOnMessageThread (const AudioFile& file)
{
    if (juce::MessageManager::existsAndIsCurrentThread())
        callListeners (file);
    else
        juce::MessageManager::callAsync ([file, eng = Engine::WeakRef (&engine)]
                                         {
                                             if (eng != nullptr)
                                                 eng->getAudioFileManager().callListeners (file);
                                         });
}

void AudioFileManager::forceFileUpdate (const AudioFile& file)
{
    CRASH_TRACER
    TRACKTION_ASSERT_MESSAGE_THREAD

    // this doesn't check for file time and is used when files are changed rapidly such as when recording
    const juce::ScopedLock sl (knownFilesLock);

    auto f = knownFiles.find (file.getHash());

    if (f != knownFiles.end())
    {
        f->second->info = AudioFileInfo::parse (f->second->file);
        releaseFile (file);
        callListeners (file);
    }
}

void AudioFileManager::validateFile (const AudioFile& file, bool updateInfo)
{
    if (updateInfo)
    {
        getInfo (file);
        forceFileUpdate (file);
    }

    cache.validateFile (file);
}

void AudioFileManager::checkFileForChangesAsync (const AudioFile& file)
{
    const juce::ScopedLock sl (knownFilesLock);
    filesToCheck.addIfNotAlreadyThere (file);
    triggerAsyncUpdate();
}

void AudioFileManager::handleAsyncUpdate()
{
    CRASH_TRACER
    AudioFile fileToCheck (engine);

    {
        const juce::ScopedLock sl (knownFilesLock);
        fileToCheck = filesToCheck.getUnchecked (filesToCheck.size() - 1);
        filesToCheck.removeLast();

        if (filesToCheck.size() > 0)
            triggerAsyncUpdate();
    }

    if (! fileToCheck.isNull())
        checkFileForChanges (fileToCheck);
}

}} // namespace tracktion { inline namespace engine
