/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
{

class AudioProxyGenerator
{
public:
    AudioProxyGenerator();
    ~AudioProxyGenerator();

    void deleteProxy (const AudioFile& proxyFile);

    bool isProxyBeingGenerated (const AudioFile& proxyFile) const noexcept;
    float getProportionComplete (const AudioFile& proxyFile) const noexcept;

    //==============================================================================
    struct GeneratorJob  : public ThreadPoolJobWithProgress
    {
        GeneratorJob (const AudioFile& proxy);
        ~GeneratorJob();

        virtual bool render() = 0;

        float getCurrentTaskProgress() override         { return progress; }

        ThreadPoolJob::JobStatus runJob() override;

        using Ptr = juce::ReferenceCountedObjectPtr<GeneratorJob>;

        AudioFile proxy;
        std::atomic<float> progress { 0.0f };

    private:
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GeneratorJob)
    };

    void beginJob (GeneratorJob*);

private:
    juce::Array<GeneratorJob*> activeJobs;
    juce::CriticalSection jobListLock;

    GeneratorJob* findJob (const AudioFile&) const noexcept;
    void removeFinishedJob (GeneratorJob*);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioProxyGenerator)
};

} // namespace tracktion_engine
