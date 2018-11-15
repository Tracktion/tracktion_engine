/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
{

//==============================================================================
/**
*/
struct AudioFileUtils
{
    static juce::AudioFormatReader* createReaderFor (const juce::File&);
    static juce::AudioFormatReader* createReaderFindingFormat (const juce::File&, juce::AudioFormat*&);
    static juce::MemoryMappedAudioFormatReader* createMemoryMappedReader (const juce::File&, juce::AudioFormat*&);

    static juce::AudioFormatWriter* createWriterFor (const juce::File&,
                                                     double sampleRate, unsigned int numChannels, int bitsPerSample,
                                                     const juce::StringPairArray& metadata, int quality);

    static juce::AudioFormatWriter* createWriterFor (juce::AudioFormat*, const juce::File&,
                                                     double sampleRate, unsigned int numChannels, int bitsPerSample,
                                                     const juce::StringPairArray& metadata, int quality);

    static juce::Range<juce::int64> scanForNonZeroSamples (const juce::File&, float maxZeroLevelDb);

    static juce::Range<juce::int64> copyNonSilentSectionToNewFile (const juce::File& sourceFile,
                                                                   const juce::File& destFile,
                                                                   float maxZeroLevelDb);

    static juce::Range<juce::int64> trimSilence (const juce::File&, float maxZeroLevelDb);

    /** Reverses a file updating a progress value and checking the exit status of a given job. */
    static bool reverse (const juce::File& source, const juce::File& destination,
                         std::atomic<float>& progress, juce::ThreadPoolJob* job = nullptr,
                         bool canCreateWavIntermediate = true);

    // returns length of file created, or -1
    static juce::int64 copySectionToNewFile (const juce::File& sourceFile,
                                             const juce::File& destFile,
                                             const juce::Range<juce::int64>& range);

    static juce::int64 copySectionToNewFile (const juce::File& sourceFile,
                                             const juce::File& destFile,
                                             EditTimeRange range);

    static void addBWAVStartToMetadata (juce::StringPairArray& metadata, juce::int64 time);

    static void applyBWAVStartTime (const juce::File&, juce::int64 time);

    static juce::int64 getFileLengthSamples (const juce::File&);

    //==============================================================================
    template<class TargetFormat>
    static bool convertToFormat (const juce::File& sourceFile, juce::OutputStream& destStream,
                                 int quality, const juce::StringPairArray& metadata)
    {
        std::unique_ptr<juce::AudioFormatReader> reader (createReaderFor (sourceFile));
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

            if (auto out = std::unique_ptr<juce::FileOutputStream> (tempFile.getFile().createOutputStream()))
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
    inline static bool readFromFormat (juce::InputStream& source, const juce::File& dest)
    {
        struct ForwardingInputStream  : public juce::InputStream
        {
            ForwardingInputStream (juce::InputStream& s)  : stream (s) {}

            juce::int64 getTotalLength()        { return stream.getTotalLength(); }
            bool isExhausted()                  { return stream.isExhausted(); }
            int read (void* d, int sz)          { return stream.read (d, sz); }
            juce::int64 getPosition()           { return stream.getPosition(); }
            bool setPosition (juce::int64 pos)  { return stream.setPosition (pos); }

            InputStream& stream;
        };

        SourceFormat sourceFormat;

        if (auto reader = std::unique_ptr<juce::AudioFormatReader> (sourceFormat.createReaderFor (new ForwardingInputStream (source), true)))
        {
            auto& manager = Engine::getInstance().getAudioFileFormatManager().writeFormatManager;

            auto* destFormat = manager.findFormatForFileExtension (dest.getFileExtension());

            if (destFormat == nullptr)
                destFormat = manager.findFormatForFileExtension ("wav");

            AudioFileWriter writer (AudioFile (dest), destFormat,
                                    (int) reader->numChannels,
                                    reader->sampleRate,
                                    (int) reader->bitsPerSample,
                                    reader->metadataValues, 0);

            if (writer.isOpen() && writer.writer->writeFromAudioReader (*reader, 0, -1))
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

} // namespace tracktion_engine
