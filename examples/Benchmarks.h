/*******************************************************************************
 The block below describes the properties of this PIP. A PIP is a short snippet
 of code that can be read by the Projucer and used to generate a JUCE project.

 BEGIN_JUCE_PIP_METADATA

  name:             tracktion_graph_PerformanceTests
  version:          0.0.1
  vendor:           Tracktion
  website:          www.tracktion.com
  description:      Used for development of the new tracktion_graph.

  dependencies:     juce_audio_basics, juce_audio_devices, juce_audio_formats, juce_audio_processors, juce_audio_utils,
                    juce_core, juce_data_structures, juce_dsp, juce_events, juce_graphics,
                    juce_gui_basics, juce_gui_extra, juce_osc, tracktion_engine, tracktion_graph
  exporters:        linux_make, vs2017, xcode_iphone, xcode_mac

  moduleFlags:      JUCE_STRICT_REFCOUNTEDPOINTER=1, JUCE_PLUGINHOST_AU=1, JUCE_PLUGINHOST_VST3=1
  defines:          ENABLE_EXPERIMENTAL_TRACKTION_GRAPH=1, TRACKTION_GRAPH_PERFORMANCE_TESTS=1, JUCE_MODAL_LOOPS_PERMITTED=1

  type:             Console
  mainClass:        tracktion_graph_PerformanceTests

 END_JUCE_PIP_METADATA

*******************************************************************************/

#pragma once

#include "common/tracktion_graph_Dev.h"

//==============================================================================
//==============================================================================
/** @internal */
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


//==============================================================================
//==============================================================================
/** @internal */
inline bool publishToBenchmarkAPI (juce::String apiKey, std::vector<BenchmarkResult> results)
{
    if (apiKey.isEmpty())
    {
        jassertfalse;
        return false;
    }
    
    juce::Array<juce::var> records;

    for (auto& r : results)
    {
        juce::DynamicObject::Ptr fields = new juce::DynamicObject();
        fields->setProperty ("benchmark_hash",        juce::String (static_cast<uint64> (r.description.hash)));
        fields->setProperty ("benchmark_category",    juce::String (r.description.category).quoted ('\''));
        fields->setProperty ("benchmark_name",        juce::String (r.description.name).quoted ('\''));
        fields->setProperty ("benchmark_description", juce::String (r.description.description).quoted ('\''));
        fields->setProperty ("benchmark_platform",    juce::String (r.description.platform).quoted ('\''));
        fields->setProperty ("benchmark_ticks",       static_cast<int64> (r.ticksEnd - r.ticksStart));
        fields->setProperty ("benchmark_ticks_per_s", static_cast<int64> (r.ticksPerSecond));
        fields->setProperty ("benchmark_duration",    getDuration (r));
        fields->setProperty ("benchmark_time",        r.date.toISO8601 (true).trimCharactersAtEnd ("Z").quoted ('\''));

        records.add (fields.get());
    }

    juce::var valuesToAdd (std::move (records));
    auto jsonString = juce::JSON::toString (valuesToAdd, false);

    const auto url = juce::URL ("https://appstats.tracktion.com/benchmarkapi.php")
                        .withParameter ("api_key", apiKey)
                        .withParameter ("request", "push_results")
                        .withParameter ("content", jsonString);

    if (auto inputStream = url.createInputStream (URL::InputStreamOptions (URL::ParameterHandling::inPostData)))
    {
        const auto returnVal = inputStream->readEntireStreamAsString();
        std::cout << returnVal << "\n";

        if (returnVal.isEmpty())
            return true;
    }
    
    return false;
}


//==============================================================================
//==============================================================================
int main (int, char**)
{
    ScopedJuceInitialiser_GUI init;
    const auto anyFailed = TestRunner::runTests ({}, "tracktion_benchmarks");
    auto results = BenchmarkList::getInstance().getResults();
    
    for (const auto& r : results)
        std::cout << r.description.name << ", " << r.description.category
                  << "\n\t" << r.description.description
                  << "\n\t" << getDuration (r) << "\n";
    
    if (publishToBenchmarkAPI (SystemStats::getEnvironmentVariable ("BM_API_KEY", {}),
                               std::move (results)))
    {
        std::cout << "INFO: Published benchmark results\n";
    }
    else
    {
        std::cout << "ERROR: Failed to publish!\n";
    }
    
    return anyFailed;
}
