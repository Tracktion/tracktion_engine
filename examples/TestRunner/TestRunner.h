/*******************************************************************************
 The block below describes the properties of this PIP. A PIP is a short snippet
 of code that can be read by the Projucer and used to generate a JUCE project.

 BEGIN_JUCE_PIP_METADATA

  name:             TestRunner
  version:          0.0.1
  vendor:           Tracktion
  website:          www.tracktion.com
  description:      This simply runs the unit tests within Tracktion Engine.

  dependencies:     juce_audio_basics, juce_audio_devices, juce_audio_formats, juce_audio_processors, juce_audio_utils,
                    juce_core, juce_data_structures, juce_dsp, juce_events, juce_graphics,
                    juce_gui_basics, juce_gui_extra, juce_osc, tracktion_engine, tracktion_graph
  exporters:        linux_make, vs2017, xcode_iphone, xcode_mac

  moduleFlags:      JUCE_STRICT_REFCOUNTEDPOINTER=1, JUCE_PLUGINHOST_AU=1, JUCE_PLUGINHOST_VST3=1
  defines:          TRACKTION_UNIT_TESTS=1, JUCE_MODAL_LOOPS_PERMITTED=1, TRACKTION_ENABLE_SOUNDTOUCH=1

  type:             Console
  mainClass:        TestRunner

 END_JUCE_PIP_METADATA

*******************************************************************************/

#pragma once

#define DOCTEST_CONFIG_IMPLEMENT
#include <tracktion_engine/../3rd_party/doctest/tracktion_doctest.hpp>

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
class TestPropertyStorage : public PropertyStorage
{
public:
    TestPropertyStorage (juce::StringRef appName_)
        : PropertyStorage (appName_)
    {
        getAppCacheFolder().deleteRecursively (false);
        getAppPrefsFolder().deleteRecursively (false);
    }

    ~TestPropertyStorage() override
    {
        getAppCacheFolder().deleteRecursively (false);
        getAppPrefsFolder().deleteRecursively (false);
    }

    //==============================================================================
    void removeProperty (SettingID) override {}
    juce::var getProperty (SettingID, const juce::var& defaultValue) override { return defaultValue; }
    void setProperty (SettingID, const juce::var&) override {}
    std::unique_ptr<juce::XmlElement> getXmlProperty (SettingID) override { return {}; }
    void setXmlProperty (SettingID, const juce::XmlElement&) override {}
    
    //==============================================================================
    void removePropertyItem (SettingID, juce::StringRef) override {}
    juce::var getPropertyItem (SettingID, juce::StringRef, const juce::var& defaultValue) override { return defaultValue; }
    void setPropertyItem (SettingID, juce::StringRef, const juce::var&) override {}
    std::unique_ptr<juce::XmlElement> getXmlPropertyItem (SettingID, juce::StringRef) override { return {}; }
    void setXmlPropertyItem (SettingID, juce::StringRef, const juce::XmlElement&) override {}
};


//==============================================================================
//==============================================================================
struct CoutLogger : public Logger
{
    void logMessage (const String& message) override
    {
        static tracktion::RealTimeSpinLock mutex;

        const std::scoped_lock lock (mutex);
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
    inline int runTests (const File& junitResultsFile)
    {
        CoutLogger logger;
        Logger::setCurrentLogger (&logger);

        tracktion_engine::Engine engine { std::make_unique<TestPropertyStorage> (ProjectInfo::projectName),
                                          std::make_unique<TestUIBehaviour>(),
                                          std::make_unique<TestEngineBehaviour>() };
        engine.getTemporaryFileManager().getTempDirectory().deleteRecursively (false);

        UnitTestRunner testRunner;
        testRunner.setAssertOnFailure (true);

        Array<UnitTest*> tests;
        tests.addArray (UnitTest::getTestsInCategory ("tracktion_core"));
        tests.addArray (UnitTest::getTestsInCategory ("tracktion_graph"));
        tests.addArray (UnitTest::getTestsInCategory ("tracktion_engine"));
        tests.addArray (UnitTest::getTestsInCategory ("Tracktion"));
        tests.addArray (UnitTest::getTestsInCategory ("Tracktion:Longer"));
        
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
            auto res = JUnit::createJUnitXMLFile (junitResultsFile, "Tracktion", testRunner, endTime - startTime);
            
            if (res)
                Logger::writeToLog ("Wrote junit results to :" + junitResultsFile.getFullPathName());
            else
                Logger::writeToLog (res.getErrorMessage());
        }

        doctest::Context doctestContext;
        // doctestContext.setOption ("success", true);  // Shows passed tests
        // doctestContext.setOption ("abort-after", 1); // Aborts after the first fail
        doctestContext.setOption ("duration", true);
        doctestContext.addFilter("test-suite", "tracktion_core");
        doctestContext.addFilter("test-suite", "tracktion_graph");
        doctestContext.addFilter("test-suite", "tracktion_engine");
        const auto doctestFailed = doctestContext.run();

        Logger::setCurrentLogger (nullptr);
        engine.getTemporaryFileManager().getTempDirectory().deleteRecursively (false);

        return (numFailues > 0 || doctestFailed) ? 1 : 0;
    }
}


//==============================================================================
//==============================================================================
int main (int argv, char** argc)
{
    File junitFile;
    
    for (int i = 1; i < argv; ++i)
        if (String (argc[i]) == "--junit-xml-file")
            if ((i + 1) < argv)
                junitFile = String (argc[i + 1]);
    
    ScopedJuceInitialiser_GUI init;
    return TestRunner::runTests (junitFile);
}
