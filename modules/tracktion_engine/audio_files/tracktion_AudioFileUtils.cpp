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

juce::AudioFormatReader* AudioFileUtils::createReaderFor (Engine& engine, const juce::File& file)
{
    return engine.getAudioFileFormatManager().readFormatManager.createReaderFor (file);
}

juce::AudioFormatReader* AudioFileUtils::createReaderFindingFormat (Engine& engine, const juce::File& file, juce::AudioFormat*& format)
{
    auto& manager = engine.getAudioFileFormatManager().readFormatManager;

    for (auto af : manager)
    {
        if (af->canHandleFile (file))
        {
            if (auto in = file.createInputStream())
            {
                if (auto r = af->createReaderFor (in.release(), true))
                {
                    format = af;
                    return r;
                }
            }
        }
    }

    return {};
}

juce::MemoryMappedAudioFormatReader* AudioFileUtils::createMemoryMappedReader (Engine& engine, const juce::File& file, juce::AudioFormat*& format)
{
    auto& manager = engine.getAudioFileFormatManager().readFormatManager;

    for (auto af : manager)
    {
        if (af->canHandleFile (file))
        {
            if (auto r = af->createMemoryMappedReader (file))
            {
                format = af;
                return r;
            }
        }
    }

    return {};
}

juce::AudioFormatWriter* AudioFileUtils::createWriterFor (juce::AudioFormat* format, const juce::File& file,
                                                          double sampleRate, unsigned int numChannels, int bitsPerSample,
                                                          const juce::StringPairArray& metadata, int quality)
{
    std::unique_ptr<juce::FileOutputStream> out (file.createOutputStream());

    if (out != nullptr)
    {
        if (auto writer = format->createWriterFor (out.get(), sampleRate,
                                                   numChannels, bitsPerSample,
                                                   metadata, quality))
        {
            out.release();
            return writer;
        }
    }

    return {};
}

juce::AudioFormatWriter* AudioFileUtils::createWriterFor (Engine& engine,
                                                          const juce::File& file, double sampleRate,
                                                          unsigned int numChannels, int bitsPerSample,
                                                          const juce::StringPairArray& metadata, int quality)
{
    if (auto format = engine.getAudioFileFormatManager().getFormatFromFileName (file))
        return createWriterFor (format, file, sampleRate, numChannels, bitsPerSample, metadata, quality);

    return {};
}

SampleRange AudioFileUtils::scanForNonZeroSamples (Engine& engine, const juce::File& file, float maxZeroLevelDb)
{
    std::unique_ptr<juce::AudioFormatReader> reader (createReaderFor (engine, file));

    if (reader == nullptr)
        return {};

    auto numChans = (int) reader->numChannels;

    if (numChans == 0 || reader->lengthInSamples == 0)
        return {};

    const float floatMaxZeroLevel = 2.0f * dbToGain (maxZeroLevelDb);
    const int intMaxZeroLevel = 1 + (int) (std::numeric_limits<int>::max() * (double) floatMaxZeroLevel);
    const int sampsPerBlock = 32768;

    juce::HeapBlock<int*> chans;
    chans.calloc ((size_t) numChans + 2);

    juce::HeapBlock<int> buffer ((size_t) numChans * sampsPerBlock);

    for (int i = 0; i < numChans; ++i)
        chans[i] = buffer + i * sampsPerBlock;

    SampleCount firstNonZero = 0, lastNonZero = 0, n = 0;
    bool needFirst = true;

    while (n < reader->lengthInSamples)
    {
        for (int j = numChans; --j >= 0;)
            std::memset (chans[j], 0, sizeof (int) * sampsPerBlock);

        reader->read (chans, numChans, n, sampsPerBlock, false);

        for (int j = numChans; --j >= 0;)
        {
            if (reader->usesFloatingPointData)
            {
                const float* const chan = (const float*) chans[j];

                for (int i = 0; i < sampsPerBlock; ++i)
                {
                    if (std::abs (chan[i]) > floatMaxZeroLevel)
                    {
                        if (needFirst)
                        {
                            firstNonZero = n + i;
                            needFirst = false;
                        }

                        lastNonZero = std::max (lastNonZero, n + i);
                    }
                }
            }
            else
            {
                const int* const chan = chans[j];

                for (int i = 0; i < sampsPerBlock; ++i)
                {
                    if (std::abs (chan[i]) > intMaxZeroLevel)
                    {
                        if (needFirst)
                        {
                            firstNonZero = n + i;
                            needFirst = false;
                        }

                        lastNonZero = std::max (lastNonZero, n + i);
                    }
                }
            }
        }

        n += sampsPerBlock;
    }

    return { firstNonZero, lastNonZero };
}

static SampleCount copySection (Engine& e, std::unique_ptr<juce::AudioFormatReader>& reader,
                                const juce::File& sourceFile, const juce::File& destFile,
                                SampleRange range)
{
    if (range.contains ({ 0, reader->lengthInSamples })
         && sourceFile.getFileExtension() == destFile.getFileExtension())
    {
        reader = nullptr;

        if (sourceFile.copyFileTo (destFile))
            return range.getLength();

        return -1;
    }

    std::unique_ptr<juce::AudioFormatWriter> writer (AudioFileUtils::createWriterFor (e, destFile, reader->sampleRate,
                                                                                      reader->numChannels,
                                                                                      (int) reader->bitsPerSample,
                                                                                      reader->metadataValues,
                                                                                      0));

    if (writer != nullptr
         && writer->writeFromAudioReader (*reader, range.getStart(), range.getLength()))
        return range.getLength();

    return -1;
}

SampleCount AudioFileUtils::copySectionToNewFile (Engine& e, const juce::File& sourceFile,
                                                  const juce::File& destFile, SampleRange range)
{
    if (range.isEmpty())
        return -1;

    std::unique_ptr<juce::AudioFormatReader> reader (createReaderFor (e, sourceFile));

    if (reader != nullptr)
        return copySection (e, reader, sourceFile, destFile, range);

    return -1;
}

SampleCount AudioFileUtils::copySectionToNewFile (Engine& e,
                                                  const juce::File& sourceFile,
                                                  const juce::File& destFile,
                                                  TimeRange range)
{
    if (range.isEmpty())
        return -1;

    std::unique_ptr<juce::AudioFormatReader> reader (createReaderFor (e, sourceFile));

    if (reader != nullptr)
        return copySection (e, reader, sourceFile, destFile,
                            { (SampleCount) tracktion::toSamples (range.getStart(),   reader->sampleRate),
                              (SampleCount) tracktion::toSamples (range.getEnd(),     reader->sampleRate) });

    return -1;
}

SampleRange AudioFileUtils::copyNonSilentSectionToNewFile (Engine& e,
                                                           const juce::File& sourceFile,
                                                           const juce::File& destFile,
                                                           float maxZeroLevelDb)
{
    auto range = scanForNonZeroSamples (e, sourceFile, maxZeroLevelDb);

    if (copySectionToNewFile (e, sourceFile, destFile, range) >= 0)
        return range;

    return {};
}

SampleRange AudioFileUtils::trimSilence (Engine& e, const juce::File& file, float maxZeroLevelDb)
{
    if (file.hasWriteAccess())
    {
        juce::TemporaryFile tempFile (file);
        auto range = copyNonSilentSectionToNewFile (e, file, tempFile.getFile(), maxZeroLevelDb);

        if (! range.isEmpty())
            if (tempFile.overwriteTargetFileWithTemporary())
                return range;
    }

    return {};
}

bool AudioFileUtils::reverse (Engine& engine,
                              const juce::File& source, const juce::File& destination,
                              std::atomic<float>& progress, juce::ThreadPoolJob* job, bool canCreateWavIntermediate)
{
    CRASH_TRACER
    juce::AudioFormat* format;
    const std::unique_ptr<juce::AudioFormatReader> reader (AudioFileUtils::createReaderFindingFormat (engine, source, format));

    if (reader == nullptr || format == nullptr)
        return false;

    // Compressed formats don't like the random access required to reverse so make a wav copy first
    if (format->isCompressed() && canCreateWavIntermediate)
    {
        auto f = juce::File::createTempFile (".wav");
        juce::TemporaryFile tempFile ({}, f);

        {
            const std::unique_ptr<juce::FileOutputStream> out (tempFile.getFile().createOutputStream());

            if (out == nullptr || (! convertToFormat<juce::WavAudioFormat> (engine, source, *out, 0, {})))
                return false;
        }

        return reverse (engine, tempFile.getFile(), destination, progress, job, false);
    }

    // need to strip AIFF metadata to write to wav files
    if (reader->metadataValues.getValue ("MetaDataSource", "None") == "AIFF")
        reader->metadataValues.clear();

    AudioFileWriter writer (AudioFile (engine, destination), engine.getAudioFileFormatManager().getWavFormat(),
                            (int) reader->numChannels, reader->sampleRate,
                            std::max (16, (int) reader->bitsPerSample),
                            reader->metadataValues, 0);

    if (auto af = writer.file.getFormat())
    {
        // This is likely to mess things up if you don't supply a file with the correct extension
        jassert (af->getFileExtensions().contains (destination.getFileExtension()));
        (void) af;
    }

    if (! writer.isOpen())
        return false;

    SampleCount sourceSample = 0;
    SampleCount samplesToDo = reader->lengthInSamples;
    const int bufferSize = 65536;
    auto sampleNum = samplesToDo;

    const int numChans = (int) reader->numChannels;
    juce::AudioBuffer<float> buffer (numChans, bufferSize);
    juce::HeapBlock<int*> buffers ((size_t) numChans);
    bool shouldExit = false;

    while (! shouldExit)
    {
        auto numThisTime = (int) std::min (samplesToDo, (SampleCount) bufferSize);

        if (numThisTime <= 0)
            return true;

        for (int i = numChans; --i >= 0;)
            buffers[i] = (int*) buffer.getWritePointer (i);

        sampleNum -= numThisTime;
        reader->read (buffers, numChans, sampleNum, numThisTime, true);

        for (int i = numThisTime / 2; --i >= 0;)
        {
            const int other = (numThisTime - i) - 1;

            for (int j = numChans; --j >= 0;)
            {
                if (buffers[j] != nullptr)
                {
                    const int temp = buffers[j][i];
                    buffers[j][i] = buffers[j][other];
                    buffers[j][other] = temp;
                }
            }
        }

        if (! writer.appendBuffer ((const int**) buffers.getData(), numThisTime))
            break;

        samplesToDo -= numThisTime;
        sourceSample += numThisTime;

        progress = juce::jlimit (0.0f, 1.0f, (float) (sourceSample / (double) reader->lengthInSamples));

        if (job != nullptr)
            shouldExit = job->shouldExit();
    }

    return false;
}

void AudioFileUtils::addBWAVStartToMetadata (juce::StringPairArray& metadata, SampleCount time)
{
    metadata.addArray (juce::WavAudioFormat::createBWAVMetadata ({}, "tracktion",
                                                                 {}, juce::Time::getCurrentTime(),
                                                                 time, {}));
}

SampleCount AudioFileUtils::getFileLengthSamples (Engine& e, const juce::File& file)
{
    std::unique_ptr<juce::AudioFormatReader> reader (createReaderFor (e, file));

    if (reader != nullptr)
        return reader->lengthInSamples;

    TRACKTION_LOG_ERROR ("Couldn't read file: " + file.getFileName());
    return 0;
}

static bool isWavFile (const juce::File& file)
{
    juce::AudioFormatManager afm;
    afm.registerFormat (new juce::WavAudioFormat(), true);

    std::unique_ptr<juce::AudioFormatReader> reader (afm.createReaderFor (file));
    return reader != nullptr;
}

void AudioFileUtils::applyBWAVStartTime (const juce::File& file, SampleCount time)
{
    if (isWavFile (file))
    {
        int pos = 0;

        {
            juce::FileInputStream fi (file);

            if (fi.openedOk())
            {
                for (int i = 0; i < 2048; ++i)
                {
                    char n[5] = { 0 };
                    fi.setPosition (i);
                    fi.read (n, 4);

                    for (int j = 0; j < 4; ++j)
                        n[j] &= 127; // to avoid ASCII char assertions

                    if (juce::String (n).equalsIgnoreCase ("bext"))
                        pos = i + 8;
                }
            }
        }

        if (pos > 0)
        {
            pos += 256 + 32 + 32 + 10 + 8;

            juce::FileOutputStream fo (file);

            if (fo.openedOk())
            {
                fo.setPosition (pos);

                fo.writeInt ((int) (uint32_t) (time & 0xffffffff));
                fo.writeInt ((int) (uint32_t) (time >> 32));
            }
        }
    }
}

}} // namespace tracktion { inline namespace engine
