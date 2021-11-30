/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#pragma once

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
    ValueTree createJUnitResultValueTree (const UnitTestRunner::TestResult& result)
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
    ValueTree createJUnitValueTree (const String& testSuiteName, const UnitTestRunner& runner, RelativeTime duration)
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
    std::unique_ptr<XmlElement> createJUnitXML (const String& testSuiteName, const UnitTestRunner& runner, RelativeTime duration)
    {
        return createJUnitValueTree (testSuiteName, runner, duration).createXml();
    }

    /** Converts a UnitTestRunner's results to a JUnit formatted XML file. */
    Result createJUnitXMLFile (const File& destinationFile, const String& testSuiteName, const UnitTestRunner& runner, RelativeTime duration)
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
    int runTests (const File& junitResultsFile, StringRef category)
    {
        CoutLogger logger;
        Logger::setCurrentLogger (&logger);

        tracktion_engine::Engine engine { ProjectInfo::projectName, std::make_unique<TestUIBehaviour>(), std::make_unique<TestEngineBehaviour>() };

        UnitTestRunner testRunner;
        testRunner.setAssertOnFailure (false);

        auto tests = UnitTest::getTestsInCategory (category);
        
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

        Logger::setCurrentLogger (nullptr);

        return numFailues > 0 ? 1 : 0;
    }
}


//==============================================================================
//==============================================================================
struct BenchmarkDescription
{
    size_t hash = 0;
    std::string category;
    std::string name;
    std::string description;
    std::string platform { juce::SystemStats::getOperatingSystemName().toStdString() };
};

//==============================================================================
struct BenchmarkResult
{
    BenchmarkDescription description;
    int64_t ticksStart = 0, ticksEnd = 0, ticksPerSecond = 0;
    juce::Time date { juce::Time::getCurrentTime() };
};

inline double getDuration (const BenchmarkResult& r)
{
    return (r.ticksEnd - r.ticksStart) / static_cast<double> (r.ticksPerSecond);
}

//==============================================================================
class Benchmark
{
public:
    Benchmark (BenchmarkDescription desc)
        : description (std::move (desc))
    {
    }
    
    void start()
    {
        ticksStart = Time::getHighResolutionTicks();
    }
    
    void stop()
    {
        ticksEnd = Time::getHighResolutionTicks();
    }
    
    BenchmarkResult getResult() const
    {
        return { description, ticksStart, ticksEnd, ticksPerSecond };
    }
    
private:
    BenchmarkDescription description;
    int64 ticksStart = 0, ticksEnd = 0;
    const int64 ticksPerSecond { Time::getHighResolutionTicksPerSecond() };
};

//==============================================================================
class BenchmarkList
{
public:
    BenchmarkList() = default;
    
    void addResult (BenchmarkResult r)
    {
        std::lock_guard lock (mutex);
        results.emplace_back (std::move (r));
    }
    
    std::vector<BenchmarkResult> getResults() const
    {
        std::lock_guard lock (mutex);
        return results;
    }
    
    static BenchmarkList& getInstance()
    {
        static BenchmarkList list;
        return list;
    }
    
private:
    mutable std::mutex mutex;
    std::vector<BenchmarkResult> results;
};

//==============================================================================
struct ScopedBenchmark
{
    ScopedBenchmark (BenchmarkDescription desc)
        : benchmark (std::move (desc))
    {
        benchmark.start();
    }
    
    ~ScopedBenchmark()
    {
        benchmark.stop();
        BenchmarkList::getInstance().addResult (benchmark.getResult());
    }
    
private:
    Benchmark benchmark;
};

//==============================================================================
//==============================================================================
inline bool publishToAirtable (std::string baseID, std::string apiKey,
                               std::vector<BenchmarkResult> results)
{
    if (baseID.empty() || apiKey.empty())
    {
        jassertfalse;
        return false;
    }
    
    juce::Array<juce::var> records;

    for (auto& r : results)
    {
        juce::DynamicObject::Ptr fields = new juce::DynamicObject();
        fields->setProperty ("Hash",        juce::String (std::to_string (static_cast<uint64_t> (r.description.hash))));
        fields->setProperty ("Category",    juce::String (r.description.category));
        fields->setProperty ("Name",        juce::String (r.description.name));
        fields->setProperty ("Description", juce::String (r.description.description));
        fields->setProperty ("Platform",    juce::String (r.description.platform));
        fields->setProperty ("Duration",    getDuration (r));
        fields->setProperty ("Ticks",       static_cast<int64> (r.ticksEnd - r.ticksStart));
        fields->setProperty ("Ticks/s",     static_cast<int64> (r.ticksPerSecond));
        fields->setProperty ("Date",        r.date.toISO8601 (true));

        juce::DynamicObject::Ptr record = new juce::DynamicObject();
        record->setProperty ("fields", fields.get());
        records.add (record.get());
    }

    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    obj->setProperty ("records", records);
    
    const auto url = juce::URL (juce::String ("https://api.airtable.com/v0/{}/benchmarks").replace ("{}", baseID))
                        .withPOSTData (juce::JSON::toString (obj.get()));

    juce::StringArray headers;
    headers.add (juce::String ("Authorization: Bearer {}").replace ("{}", apiKey));
    headers.add ("Content-Type: application/json");

    for (int numRetries = 2; --numRetries > 0;)
    {
        int statusCodeResult = -1;
        
        if (auto inputStream = url.createInputStream (URL::InputStreamOptions (URL::ParameterHandling::inPostData)
                                                           .withExtraHeaders (headers.joinIntoString ("\n"))
                                                           .withStatusCode (&statusCodeResult)))
        {
            inputStream->readEntireStreamAsString();
            
            // Success
            if (statusCodeResult == 200)
                return true;
            
            // Rate limit reached
            if (statusCodeResult == 429)
                std::this_thread::sleep_for (std::chrono::seconds (30));
        }
    }
    
    return false;
}
