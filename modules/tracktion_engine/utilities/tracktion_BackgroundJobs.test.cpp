/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#if TRACKTION_UNIT_TESTS && ENGINE_UNIT_TESTS_BACKGROUND_JOBS

#include <tracktion_engine/../3rd_party/doctest/tracktion_doctest.hpp>

namespace tracktion { inline namespace engine
{

TEST_SUITE ("tracktion_engine")
{
    TEST_CASE ("BackgroundJobManager: setName updates JobInfo")
    {
        struct SimpleJob : public ThreadPoolJobWithProgress
        {
            using ThreadPoolJobWithProgress::ThreadPoolJobWithProgress;

            ~SimpleJob() override { prepareForJobDeletion(); }

            JobStatus runJob() override             { return jobHasFinished; }
            float getCurrentTaskProgress() override { return 0.0f; }
        };

        juce::ScopedJuceInitialiser_GUI juceInit;
        BackgroundJobManager manager;
        SimpleJob job { "Old name" };

        manager.addJob (&job, false);
        CHECK_EQ (manager.getJobInfo (0).name, juce::String ("Old name"));

        job.setName ("New name");
        CHECK_EQ (manager.getJobInfo (0).name, juce::String ("New name"));
    }
}

}} // namespace tracktion { inline namespace engine

#endif // ENGINE_UNIT_TESTS_BACKGROUND_JOBS
