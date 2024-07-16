/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#pragma once

#include <tracktion_core/utilities/tracktion_Benchmark.h>

#define DOCTEST_CONFIG_IMPLEMENT
#include <tracktion_engine/../3rd_party/doctest/tracktion_doctest.hpp>
#undef DOCTEST_CONFIG_IMPLEMENT

using namespace tracktion_engine;

//==============================================================================
//==============================================================================
class TestUIBehaviour : public UIBehaviour
{
public:
    TestUIBehaviour() = default;
    
    void runTaskWithProgressBar (ThreadPoolJobWithProgress& t) override
    {
        TaskRunner runner (t);

         while (runner.isThreadRunning())
             if (! MessageManager::getInstance()->runDispatchLoopUntil (10))
                 break;
    }

private:
    //==============================================================================
    struct TaskRunner  : public Thread
    {
        TaskRunner (ThreadPoolJobWithProgress& t)
            : Thread (t.getJobName()), task (t)
        {
            startThread();
        }

        ~TaskRunner() override
        {
            task.signalJobShouldExit();
            waitForThreadToExit (10000);
        }

        void run() override
        {
            while (! threadShouldExit())
                if (task.runJob() == ThreadPoolJob::jobHasFinished)
                    break;
        }

        ThreadPoolJobWithProgress& task;
    };
};


//==============================================================================
//==============================================================================
class TestEngineBehaviour : public EngineBehaviour
{
public:
    TestEngineBehaviour() = default;
    
    bool autoInitialiseDeviceManager() override
    {
        return false;
    }
};


//==============================================================================
//==============================================================================
struct CoutLogger : public Logger
{
    void logMessage (const String& message) override
    {
        static CriticalSection mutex;
        
        const ScopedLock sl (mutex);
        std::cout << message << "\n";
    }
};

//==============================================================================
//==============================================================================
namespace JUnit
{
    /** Creates a JUnit formatted ValueTree from a UnitTestRunner's results. */
    inline ValueTree createJUnitResultValueTree (const UnitTestRunner::TestResult& result)
    {
        ValueTree testcase ("testcase");
        testcase.setProperty ("classname", result.unitTestName, nullptr);
        testcase.setProperty ("name", result.subcategoryName, nullptr);
        testcase.setProperty ("time", (result.endTime - result.startTime).inSeconds(), nullptr);

        for (auto message : result.messages)
            testcase.appendChild (ValueTree { "failure", {{ "message", message }}}, nullptr);
        
        return testcase;
    }

    /** Creates a JUnit formatted 'testsuite' ValueTree from a UnitTestRunner's results. */
    inline ValueTree createJUnitValueTree (const String& testSuiteName, const UnitTestRunner& runner, RelativeTime duration)
    {
        ValueTree suite ("testsuite");
        suite.setProperty ("name", testSuiteName, nullptr);
        suite.setProperty ("tests", runner.getNumResults(), nullptr);
        suite.setProperty ("timestamp", Time::getCurrentTime().toISO8601 (true), nullptr);
        
        if (duration.inSeconds() > 0.0)
            suite.setProperty ("time", duration.inSeconds(), nullptr);

        int numFailures = 0;
        
        for (int i = 0; i < runner.getNumResults(); ++i)
        {
            if (auto result = runner.getResult (i))
            {
                suite.appendChild (createJUnitResultValueTree (*result), nullptr);
                numFailures += result->failures;
            }
        }
        
        suite.setProperty ("failures", numFailures, nullptr);

        return suite;
    }

    /** Creates a JUnit formatted 'testsuite' XmlElement from a UnitTestRunner's results. */
    inline std::unique_ptr<XmlElement> createJUnitXML (const String& testSuiteName, const UnitTestRunner& runner, RelativeTime duration)
    {
        return createJUnitValueTree (testSuiteName, runner, duration).createXml();
    }

    /** Converts a UnitTestRunner's results to a JUnit formatted XML file. */
    inline Result createJUnitXMLFile (const File& destinationFile, const String& testSuiteName, const UnitTestRunner& runner, RelativeTime duration)
    {
        if (auto xml = createJUnitXML (testSuiteName, runner, duration))
            if (! xml->writeTo (destinationFile))
                return Result::fail ("Unable to write to file at: " + destinationFile.getFullPathName());
        
        return Result::ok();
    }
}


//==============================================================================
//==============================================================================
namespace TestRunner
{
    inline int runUnitTests (const File& junitResultsFile, Array<UnitTest*> tests, std::unique_ptr<doctest::Context> doctestContext = {})
    {
        CoutLogger logger;
        Logger::setCurrentLogger (&logger);

        tracktion_engine::Engine engine { ProjectInfo::projectName, std::make_unique<TestUIBehaviour>(), std::make_unique<TestEngineBehaviour>() };

        UnitTestRunner testRunner;
        testRunner.setAssertOnFailure (false);

        const auto startTime = Time::getCurrentTime();
        testRunner.runTests (tests);
        const auto endTime = Time::getCurrentTime();

        int numFailues = 0;

        for (int i = 0; i <= testRunner.getNumResults(); ++i)
            if (auto result = testRunner.getResult (i))
                numFailues += result->failures;

        if (junitResultsFile != File())
        {
            junitResultsFile.create();
            auto res = JUnit::createJUnitXMLFile (junitResultsFile, junitResultsFile.getFileName(), testRunner, endTime - startTime);
            
            if (res)
                Logger::writeToLog ("Wrote junit results to :" + junitResultsFile.getFullPathName());
            else
                Logger::writeToLog (res.getErrorMessage());
        }

        const bool doctestPassed = [&] { return ! doctestContext || doctestContext->run() == 0; }();
        const bool doctestFailed = ! doctestPassed;

        Logger::setCurrentLogger (nullptr);

        return (numFailues > 0 || doctestFailed) ? 1 : 0;
        return numFailues > 0 ? 1 : 0;
    }

    inline int runTests (const File& junitResultsFile, std::vector<juce::String> categories, std::unique_ptr<doctest::Context> doctestContext)
    {
        Array<UnitTest*> tests;

        for (auto& category : categories)
            tests.addArray (UnitTest::getTestsInCategory (category));

        return runUnitTests (junitResultsFile, std::move (tests), std::move (doctestContext));
    }

    inline int runTests (const File& junitResultsFile, juce::String category, std::unique_ptr<doctest::Context> doctestContext)
    {
        return runTests (junitResultsFile, std::vector<juce::String> { category }, std::move (doctestContext));
    }

    inline int runSingleTest (const File& junitResultsFile, juce::String name)
    {
        Array<UnitTest*> tests;

        for (auto test : UnitTest::getAllTests())
            if (test->getName() == name)
                tests.add (test);

        return runUnitTests (junitResultsFile, std::move (tests), nullptr);
    }
}

