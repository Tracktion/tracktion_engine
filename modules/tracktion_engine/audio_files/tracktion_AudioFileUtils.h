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

//==============================================================================
/**
*/
struct AudioFileUtils
{
    static juce::AudioFormatReader* createReaderFor (Engine&, const juce::File&);
    static juce::AudioFormatReader* createReaderFindingFormat (Engine&, const juce::File&, juce::AudioFormat*&);
    static juce::MemoryMappedAudioFormatReader* createMemoryMappedReader (Engine&, const juce::File&, juce::AudioFormat*&);

    static juce::AudioFormatWriter* createWriterFor (Engine&, const juce::File&,
                                                     double sampleRate, unsigned int numChannels, int bitsPerSample,
                                                     const juce::StringPairArray& metadata, int quality);

    static juce::AudioFormatWriter* createWriterFor (juce::AudioFormat*, const juce::File&,
                                                     double sampleRate, unsigned int numChannels, int bitsPerSample,
                                                     const juce::StringPairArray& metadata, int quality);

    static SampleRange scanForNonZeroSamples (Engine&, const juce::File&, float maxZeroLevelDb);

    static SampleRange copyNonSilentSectionToNewFile (Engine&,
                                                      const juce::File& sourceFile,
                                                      const juce::File& destFile,
                                                      float maxZeroLevelDb);

    static SampleRange trimSilence (Engine&, const juce::File&, float maxZeroLevelDb);

    /** Reverses a file updating a progress value and checking the exit status of a given job. */
    static bool reverse (Engine&, const juce::File& source, const juce::File& destination,
                         std::atomic<float>& progress, juce::ThreadPoolJob* job = nullptr,
                         bool canCreateWavIntermediate = true);

    // returns length of file created, or -1
    static SampleCount copySectionToNewFile (Engine& e,
                                             const juce::File& sourceFile,
                                             const juce::File& destFile,
                                             SampleRange range);

    static SampleCount copySectionToNewFile (Engine& e,
                                             const juce::File& sourceFile,
                                             const juce::File& destFile,
                                             TimeRange range);

    static void addBWAVStartToMetadata (juce::StringPairArray& metadata, SampleCount start);

    static void applyBWAVStartTime (const juce::File&, SampleCount start);

    static SampleCount getFileLengthSamples (Engine& e, const juce::File&);

    //==============================================================================
    template<class TargetFormat>
    static bool convertToFormat (Engine& e, const juce::File& sourceFile, juce::OutputStream& destStream,
                                 int quality, const juce::StringPairArray& metadata)
    {
        std::unique_ptr<juce::AudioFormatReader> reader (createReaderFor (e, sourceFile));
        return convertToFormat<TargetFormat> (reader.get(), destStream, quality, metadata);
    }

    template<class TargetFormat>
    static bool convertToFormat (juce::AudioFormatReader* reader,
                                 juce::OutputStream& destStream,
                                 int quality, const juce::StringPairArray& metadata)
    {
        if (reader != nullptr)
        {
            // NB: must write to a temp file because the archiver relies on the destStream's position
            // being left pointing to the end of the data that was written, whereas some formats
            // may leave the position set elsewhere.
            juce::TemporaryFile tempFile;

            if (auto out = tempFile.getFile().createOutputStream())
            {
                TargetFormat format;

                if (auto writer = std::unique_ptr<juce::AudioFormatWriter> (format.createWriterFor (out.get(), reader->sampleRate,
                                                                                                    reader->numChannels,
                                                                                                    (int) reader->bitsPerSample,
                                                                                                    metadata, quality)))
                {
                    out.release();

                    if (writer->writeFromAudioReader (*reader, 0, -1))
                    {
                        writer = nullptr;
                        destStream << tempFile.getFile();
                        return true;
                    }
                }
            }
            else
            {
                jassertfalse;
            }
        }

        return false;
    }

    //==============================================================================
    template <typename SourceFormat>
    inline static bool readFromFormat (Engine& engine, juce::InputStream& source, const juce::File& dest)
    {
        struct ForwardingInputStream  : public juce::InputStream
        {
            ForwardingInputStream (juce::InputStream& s)  : stream (s) {}

            juce::int64 getTotalLength() override        { return stream.getTotalLength(); }
            bool isExhausted() override                  { return stream.isExhausted(); }
            int read (void* d, int sz) override          { return stream.read (d, sz); }
            juce::int64 getPosition() override           { return stream.getPosition(); }
            bool setPosition (juce::int64 pos) override  { return stream.setPosition (pos); }

            InputStream& stream;
        };

        SourceFormat sourceFormat;

        if (auto reader = std::unique_ptr<juce::AudioFormatReader> (sourceFormat.createReaderFor (new ForwardingInputStream (source), true)))
        {
            auto& manager = engine.getAudioFileFormatManager().writeFormatManager;

            auto* destFormat = manager.findFormatForFileExtension (dest.getFileExtension());

            if (destFormat == nullptr)
                destFormat = manager.findFormatForFileExtension ("wav");

            AudioFileWriter writer (AudioFile (engine, dest), destFormat,
                                    (int) reader->numChannels,
                                    reader->sampleRate,
                                    (int) reader->bitsPerSample,
                                    reader->metadataValues, 0);

            if (writer.isOpen() && writer.writeFromAudioReader (*reader, 0, -1))
                return true;
        }

        jassertfalse;
        return false;
    }

    //==============================================================================
    class EnvelopeFollower
    {
    public:
        EnvelopeFollower() {}

        void processEnvelope (const float* inputBuffer, float* outputBuffer, int numSamples) noexcept
        {
            for (int i = 0; i < numSamples; ++i)
            {
                float envIn = std::abs (inputBuffer[i]);

                if (envelope < envIn)
                    envelope += envAttack * (envIn - envelope);
                else if (envelope > envIn)
                    envelope -= envRelease * (envelope - envIn);

                outputBuffer[i] = envelope;
            }
        }

        /** Sets the times for the vaious stages of the envelope.
            1 is an instant attack/release, 0 will never change the value.
        */
        void setCoefficients (float attack, float release) noexcept
        {
            envAttack = attack;
            envRelease = release;
        }

    private:
        float envelope = 0.0f;
        float envAttack = 1.0f, envRelease = 1.0f;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EnvelopeFollower)
    };
};

}} // namespace tracktion { inline namespace engine
