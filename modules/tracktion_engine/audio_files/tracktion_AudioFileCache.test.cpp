/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/


#include <tracktion_graph/tracktion_graph.h>
#include "../../tracktion_graph/tracktion_graph/tracktion_TestUtilities.h"
#include <tracktion_core/utilities/tracktion_Benchmark.h>


#if TRACKTION_UNIT_TESTS && ENGINE_UNIT_TESTS_AUDIO_FILE_CACHE

namespace tracktion { inline namespace engine
{

//==============================================================================
//==============================================================================
class AudioFileCacheTests   : public juce::UnitTest
{
public:
    AudioFileCacheTests()
        : juce::UnitTest ("AudioFileCache", "tracktion_engine")
    {
    }

    void runTest() override
    {
        runCacheReadTest();
    }

private:
    void runCacheReadTest()
    {
        Engine& engine = *Engine::getEngines().getFirst();

        using namespace graph::test_utilities;
        auto tempFile = getSquareFile<juce::WavAudioFormat> (44100.0, 10.0, 2);

        auto fileReader = std::unique_ptr<juce::AudioFormatReader> (AudioFileUtils::createReaderFor (engine, tempFile->getFile()));
        juce::AudioBuffer<float> bufferFromFile ((int) fileReader->numChannels, (int) fileReader->lengthInSamples);
        fileReader->read (&bufferFromFile, 0, (int) fileReader->lengthInSamples, 0, true, true);

        auto cacheReader = engine.getAudioFileManager().cache.createReader (AudioFile (engine, tempFile->getFile()));
        juce::AudioBuffer<float> bufferFromCache ((int) fileReader->numChannels, (int) fileReader->lengthInSamples);

        for (int i = 0; i < bufferFromCache.getNumSamples(); i += 32'768)
        {
            const int numToRead = std::min ((int) fileReader->lengthInSamples - i, 32'768);
            cacheReader->readSamples (numToRead,
                                      bufferFromCache, juce::AudioChannelSet::stereo(),
                                      i, juce::AudioChannelSet::stereo(), 5'000);
        }

        beginTest ("Read a sin wav file");
        expectAudioBuffer (*this, bufferFromFile, bufferFromCache);
    }
};

static AudioFileCacheTests audioFileCacheTests;

}} // namespace tracktion { inline namespace engine

#endif


#if TRACKTION_BENCHMARKS && ENGINE_BENCHMARKS_AUDIOFILECACHE
namespace tracktion { inline namespace engine
{

//==============================================================================
struct MemoryMappedFileReader    : public FallbackReader
{
    MemoryMappedFileReader (std::unique_ptr<AudioFileUtils::MappedFileAndReader> mappedFileAndReader)
        : source (std::move (mappedFileAndReader))
    {
        sampleRate              = source->reader->sampleRate;
        bitsPerSample           = source->reader->bitsPerSample;
        lengthInSamples         = source->reader->lengthInSamples;
        numChannels             = source->reader->numChannels;
        usesFloatingPointData   = source->reader->usesFloatingPointData;
        metadataValues          = source->reader->metadataValues;
    }

    void setReadTimeout (int) override
    {
    }

    bool readSamples (int* const* destSamples, int numDestChannels, int startOffsetInDestBuffer,
                      juce::int64 startSampleInFile, int numSamples) override
    {
        return source->reader->readSamples (destSamples, numDestChannels, startOffsetInDestBuffer,
                                            startSampleInFile, numSamples);
    }

private:
    std::unique_ptr<AudioFileUtils::MappedFileAndReader> source;
};


//==============================================================================
class JUCEBufferingAudioReaderWrapper   : public FallbackReader
{
public:
    JUCEBufferingAudioReaderWrapper (std::unique_ptr<juce::BufferingAudioReader> sourceReader)
        : source (std::move (sourceReader))
    {
        sampleRate              = source->sampleRate;
        bitsPerSample           = source->bitsPerSample;
        lengthInSamples         = source->lengthInSamples;
        numChannels             = source->numChannels;
        usesFloatingPointData   = source->usesFloatingPointData;
        metadataValues          = source->metadataValues;
    }

    void setReadTimeout (int timeoutMilliseconds) override
    {
        source->setReadTimeout (timeoutMilliseconds);
    }

    bool readSamples (int* const* destSamples, int numDestChannels, int startOffsetInDestBuffer,
                      juce::int64 startSampleInFile, int numSamples) override
    {
        return source->readSamples (destSamples, numDestChannels, startOffsetInDestBuffer,
                                    startSampleInFile, numSamples);
    }

private:
    std::unique_ptr<juce::BufferingAudioReader> source;
};


//==============================================================================
class TracktionBufferedFileReader   : public FallbackReader
{
public:
    TracktionBufferedFileReader (std::unique_ptr<BufferedFileReader> sourceReader)
        : source (std::move (sourceReader))
    {
        sampleRate              = source->sampleRate;
        bitsPerSample           = source->bitsPerSample;
        lengthInSamples         = source->lengthInSamples;
        numChannels             = source->numChannels;
        usesFloatingPointData   = source->usesFloatingPointData;
        metadataValues          = source->metadataValues;
    }

    void setReadTimeout (int timeoutMilliseconds) override
    {
        source->setReadTimeout (timeoutMilliseconds);
    }

    bool readSamples (int* const* destSamples, int numDestChannels, int startOffsetInDestBuffer,
                      juce::int64 startSampleInFile, int numSamples) override
    {
        return source->readSamples (destSamples, numDestChannels, startOffsetInDestBuffer,
                                    startSampleInFile, numSamples);
    }

    BufferedFileReader& get()
    {
        return *source;
    }

private:
    std::unique_ptr<BufferedFileReader> source;
};


//==============================================================================
//==============================================================================
class AudioFileCacheBenchmarks  : public juce::UnitTest
{
public:
    AudioFileCacheBenchmarks()
        : juce::UnitTest ("AudioFileCache", "tracktion_benchmarks")
    {
    }

    void runTest() override
    {
        runCacheReadBenchmark();
    }

private:
    void runCacheReadBenchmark()
    {
        // Create a 10min file
        // Create a reader for it
        // Read a single block at 1000 random places from it
        // Measure the time of the read

        Engine& engine = *Engine::getEngines().getFirst();

        const auto fileLength = 60.0 * 10.0;
        const auto sampleRate = 48000.0;
        const int numChannels = 2;
        const int blockSize = 256;

        using namespace graph::test_utilities;
        auto tempWavFile = getSquareFile<juce::WavAudioFormat> (sampleRate, fileLength, numChannels);
        auto tempOggFile = getSquareFile<juce::OggVorbisAudioFormat> (sampleRate, fileLength, numChannels, 8);

        // Read raw float from memory
        {
            auto fileReader = std::unique_ptr<juce::AudioFormatReader> (AudioFileUtils::createReaderFor (engine, tempWavFile->getFile()));
            juce::AudioBuffer<float> bufferFromFile ((int) fileReader->numChannels, (int) fileReader->lengthInSamples);
            fileReader->read (&bufferFromFile, 0, (int) fileReader->lengthInSamples, 0, true, true);

            auto bm = Benchmark (createBenchmarkDescription ("Files", "Audio file reading",
                                                             "Read 1000 random 256 sample blocks from a 10m stereo file stored in memory"));
            juce::Random r (42);
            juce::AudioBuffer<float> destBuffer ((int) fileReader->numChannels, blockSize);

            for (int i = 0; i < 1000; ++i)
            {
                const auto sourceStartSample = r.nextInt ((int) fileReader->lengthInSamples - blockSize);

                const ScopedMeasurement sm (bm);

                for (int c = 0; c < numChannels; ++c)
                    destBuffer.copyFrom (c, 0, bufferFromFile, c, sourceStartSample, blockSize);
            }

            BenchmarkList::getInstance().addResult (bm.getResult());
        }

        // Read ogg from memory
        {
            juce::MemoryBlock mb;
            tempOggFile->getFile().loadFileAsData (mb);

            auto fileReader = engine.getAudioFileFormatManager().readFormatManager.createReaderFor (std::make_unique<juce::MemoryInputStream> (mb, false));

            auto bm = Benchmark (createBenchmarkDescription ("Files", "Audio file reading",
                                                             "Read 1000 random 256 sample blocks from a 10m stereo ogg file stored in memory"));
            juce::Random r (42);
            juce::AudioBuffer<float> destBuffer ((int) fileReader->numChannels, blockSize);

            for (int i = 0; i < 1000; ++i)
            {
                const auto sourceStartSample = r.nextInt ((int) fileReader->lengthInSamples - blockSize);

                const ScopedMeasurement sm (bm);

                fileReader->read (&destBuffer, 0, blockSize,
                                  sourceStartSample, true, true);
            }

            BenchmarkList::getInstance().addResult (bm.getResult());
        }

        // Read ogg from memory mapped file
        {
            auto mappedFileAndReader = AudioFileUtils::createMappedFileAndReaderFor (engine, tempOggFile->getFile());
            auto fileReader = mappedFileAndReader->reader.get();

            auto bm = Benchmark (createBenchmarkDescription ("Files", "Audio file reading",
                                                             "Read 1000 random 256 sample blocks from a 10m stereo ogg memory mapped file"));
            juce::Random r (42);
            juce::AudioBuffer<float> destBuffer ((int) fileReader->numChannels, blockSize);

            for (int i = 0; i < 1000; ++i)
            {
                const auto sourceStartSample = r.nextInt ((int) fileReader->lengthInSamples - blockSize);

                const ScopedMeasurement sm (bm);

                fileReader->read (&destBuffer, 0, blockSize,
                                  sourceStartSample, true, true);
            }

            BenchmarkList::getInstance().addResult (bm.getResult());
        }

        // Read wav from cached reader
        {
            const AudioFile af (engine, tempWavFile->getFile());
            const auto lengthInSamples = af.getLengthInSamples();
            auto cacheReader = engine.getAudioFileManager().cache.createReader (af);

            auto bm = Benchmark (createBenchmarkDescription ("Files", "Audio file reading",
                                                             "Read 1000 random 256 sample blocks from a 10m stereo wav file"));
            juce::Random r (42);
            juce::AudioBuffer<float> destBuffer (numChannels, blockSize);

            for (int i = 0; i < 1000; ++i)
            {
                const auto sourceStartSample = r.nextInt (static_cast<int> (lengthInSamples) - blockSize);

                const ScopedMeasurement sm (bm);

                cacheReader->setReadPosition (sourceStartSample);
                cacheReader->readSamples (blockSize,
                                          destBuffer, juce::AudioChannelSet::stereo(),
                                          0, juce::AudioChannelSet::stereo(), 5'000);
            }

            BenchmarkList::getInstance().addResult (bm.getResult());
        }

        // Read ogg from buffering reader
        {
            const AudioFile af (engine, tempOggFile->getFile());
            const auto lengthInSamples = af.getLengthInSamples();
            auto cacheReader = engine.getAudioFileManager().cache.createReader (af,
                                                                                [] (juce::AudioFormatReader* sourceReader,
                                                                                    juce::TimeSliceThread& timeSliceThread,
                                                                                    int samplesToBuffer) -> std::unique_ptr<FallbackReader>
                                                                                {
                                                                                     return std::make_unique<JUCEBufferingAudioReaderWrapper> (std::make_unique<juce::BufferingAudioReader> (sourceReader,
                                                                                                                                                                                             timeSliceThread,
                                                                                                                                                                                             samplesToBuffer));
                                                                                });

            auto bm = Benchmark (createBenchmarkDescription ("Files", "Audio file reading",
                                                             "Read 1000 random 256 sample blocks from a 10m stereo ogg file at 256 kbps using juce::BufferingAudioReader"));
            juce::Random r (42);
            juce::AudioBuffer<float> destBuffer (numChannels, blockSize);

            for (int i = 0; i < 1000; ++i)
            {
                const auto sourceStartSample = r.nextInt (static_cast<int> (lengthInSamples) - blockSize);

                const ScopedMeasurement sm (bm);

                cacheReader->setReadPosition (sourceStartSample);
                cacheReader->readSamples (blockSize,
                                          destBuffer, juce::AudioChannelSet::stereo(),
                                          0, juce::AudioChannelSet::stereo(), 5'000);
            }

            BenchmarkList::getInstance().addResult (bm.getResult());
        }

        // Read ogg from cached reader
        {
//xxx            const AudioFile af (engine, tempOggFile->getFile());
//            const auto lengthInSamples = af.getLengthInSamples();
//            auto cacheReader = engine.getAudioFileManager().cache.createReader (af,
//                                                                                [] (juce::AudioFormatReader* sourceReader,
//                                                                                    juce::TimeSliceThread& timeSliceThread,
//                                                                                    int samplesToBuffer) -> std::unique_ptr<FallbackReader>
//                                                                                {
//                                                                                     return std::make_unique<TracktionBufferedFileReader> (std::make_unique<BufferedFileReader> (sourceReader,
//                                                                                                                                                                                 timeSliceThread,
//                                                                                                                                                                                 samplesToBuffer));
//                                                                                });
//
//            auto bm = Benchmark (createBenchmarkDescription ("Files", "Audio file reading",
//                                                             "Read 1000 random 256 sample blocks from a 10m stereo ogg file at 256 kbps using BufferedFileReader"));
//            juce::Random r (42);
//            juce::AudioBuffer<float> destBuffer (numChannels, blockSize);
//
//            for (int i = 0; i < 1000; ++i)
//            {
//                const auto sourceStartSample = r.nextInt (static_cast<int> (lengthInSamples) - blockSize);
//
//                const ScopedMeasurement sm (bm);
//
//                cacheReader->setReadPosition (sourceStartSample);
//                cacheReader->readSamples (blockSize,
//                                          destBuffer, juce::AudioChannelSet::stereo(),
//                                          0, juce::AudioChannelSet::stereo(), 5'000);
//            }
//
//            BenchmarkList::getInstance().addResult (bm.getResult());
        }

        // Read ogg from memory mapped reader
        {
            auto mappedFileAndReader = AudioFileUtils::createMappedFileAndReaderFor (engine, tempOggFile->getFile());

            const AudioFile af (engine, tempOggFile->getFile());
            const auto lengthInSamples = af.getLengthInSamples();
            auto cacheReader = engine.getAudioFileManager().cache.createReader (af,
                                                                                [&] (juce::AudioFormatReader*,
                                                                                     juce::TimeSliceThread&,
                                                                                     int) -> std::unique_ptr<FallbackReader>
                                                                                {
                                                                                     return std::make_unique<MemoryMappedFileReader> (std::move (mappedFileAndReader));
                                                                                });

            auto bm = Benchmark (createBenchmarkDescription ("Files", "Audio file reading",
                                                             "Read 1000 random 256 sample blocks from a 10m stereo ogg memory mapped file at 256 kbps using a Reader"));
            juce::Random r (42);
            juce::AudioBuffer<float> destBuffer (numChannels, blockSize);

            for (int i = 0; i < 1000; ++i)
            {
                const auto sourceStartSample = r.nextInt (static_cast<int> (lengthInSamples) - blockSize);

                const ScopedMeasurement sm (bm);

                cacheReader->setReadPosition (sourceStartSample);
                cacheReader->readSamples (blockSize,
                                          destBuffer, juce::AudioChannelSet::stereo(),
                                          0, juce::AudioChannelSet::stereo(), 5'000);
            }

            BenchmarkList::getInstance().addResult (bm.getResult());
        }

        // Read ogg from buffering reader
        {
            const AudioFile af (engine, tempOggFile->getFile());
            const auto lengthInSamples = af.getLengthInSamples();
            auto cacheReader = engine.getAudioFileManager().cache.createReader (af,
                                                                                [] (juce::AudioFormatReader* sourceReader,
                                                                                    juce::TimeSliceThread& timeSliceThread,
                                                                                    int samplesToBuffer) -> std::unique_ptr<FallbackReader>
                                                                                {
                                                                                     return std::make_unique<JUCEBufferingAudioReaderWrapper> (std::make_unique<juce::BufferingAudioReader> (sourceReader,
                                                                                                                                                                                             timeSliceThread,
                                                                                                                                                                                             samplesToBuffer));
                                                                                });

            auto bm = Benchmark (createBenchmarkDescription ("Files", "Audio file reading",
                                                             "Read a 10m stereo ogg file sequentially at 256 kbps using juce::BufferingAudioReader"));
            juce::AudioBuffer<float> destBuffer (numChannels, blockSize);

            for (SampleCount sourceStartSample = 0; sourceStartSample < lengthInSamples; sourceStartSample += blockSize)
            {
                const int numThisTime = std::min (blockSize,
                                                  static_cast<int> (lengthInSamples - sourceStartSample));

                const ScopedMeasurement sm (bm);

                cacheReader->setReadPosition (sourceStartSample);
                cacheReader->readSamples (numThisTime,
                                          destBuffer, juce::AudioChannelSet::stereo(),
                                          0, juce::AudioChannelSet::stereo(), 5'000);
            }

            BenchmarkList::getInstance().addResult (bm.getResult());
        }

        // Read ogg from cached reader
        {
//xxx            const AudioFile af (engine, tempOggFile->getFile());
//            const auto lengthInSamples = af.getLengthInSamples();
//            auto cacheReader = engine.getAudioFileManager().cache.createReader (af,
//                                                                                [] (juce::AudioFormatReader* sourceReader,
//                                                                                    juce::TimeSliceThread& timeSliceThread,
//                                                                                    int samplesToBuffer) -> std::unique_ptr<FallbackReader>
//                                                                                {
//                                                                                     return std::make_unique<TracktionBufferedFileReader> (std::make_unique<BufferedFileReader> (sourceReader,
//                                                                                                                                                                                 timeSliceThread,
//                                                                                                                                                                                 samplesToBuffer));
//                                                                                });
//
//            auto bm = Benchmark (createBenchmarkDescription ("Files", "Audio file reading",
//                                                             "Read a 10m stereo ogg file sequentially at 256 kbps using BufferedFileReader"));
//            juce::Random r (42);
//            juce::AudioBuffer<float> destBuffer (numChannels, blockSize);
//
//            for (SampleCount sourceStartSample = 0; sourceStartSample < lengthInSamples; sourceStartSample += blockSize)
//            {
//                const int numThisTime = std::min (blockSize,
//                                                  static_cast<int> (lengthInSamples - sourceStartSample));
//
//                const ScopedMeasurement sm (bm);
//
//                cacheReader->setReadPosition (sourceStartSample);
//                cacheReader->readSamples (numThisTime,
//                                          destBuffer, juce::AudioChannelSet::stereo(),
//                                          0, juce::AudioChannelSet::stereo(), 5'000);
//            }
//
//            BenchmarkList::getInstance().addResult (bm.getResult());
        }
    }
};

static AudioFileCacheBenchmarks audioFileCacheBenchmarks;

}} // namespace tracktion { inline namespace engine

#endif
