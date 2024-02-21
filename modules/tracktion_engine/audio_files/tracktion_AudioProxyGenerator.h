/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    You may use this code under the terms of the GPL v3 - see LICENCE.md for details.
    For the technical preview this file cannot be licensed commercially.
*/

namespace tracktion { inline namespace engine
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
        ~GeneratorJob() override;

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

}} // namespace tracktion { inline namespace engine
