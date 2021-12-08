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
/** @internal */
inline juce::String createSQLQueryString (std::vector<BenchmarkResult> results)
{
    if (results.empty())
        return {};
        
    juce::String result;
    result << "INSERT INTO benchmarks (benchmark_hash, benchmark_category, benchmark_name, benchmark_description, benchmark_platform, benchmark_ticks, benchmark_ticks_per_s, benchmark_duration, benchmark_time)\n";

    juce::StringArray values;

    for (auto& r : results)
    {
        juce::String valueContent ("(");
        valueContent
        << juce::String (std::to_string (static_cast<uint64_t> (r.description.hash))) << ","
        << juce::String (r.description.category).quoted ('\'') << ","
        << juce::String (r.description.name).quoted ('\'') << ","
        << juce::String (r.description.description).quoted ('\'') << ","
        << juce::String (r.description.platform).quoted ('\'') << ","
        << static_cast<int64> (r.ticksEnd - r.ticksStart) << ","
        << static_cast<int64> (r.ticksPerSecond) << ","
        << getDuration (r) << ","
        << r.date.toISO8601 (true).trimCharactersAtEnd ("Z").quoted ('\'')
        << ")";
        
        values.add (valueContent);
    }
    
    for (int i = 0; i < values.size() - 1; ++i)
        values.getReference (i) << ",";
    
    values.getReference (0) = "VALUES " + values.getReference (0);
    values.getReference (values.size() - 1) << ";";
    
    result << values.joinIntoString ("\n");
    
    return result;
}


//==============================================================================
//==============================================================================
int main (int argv, char** argc)
{
    ScopedJuceInitialiser_GUI init;
    const auto anyFailed = TestRunner::runTests ({}, "tracktion_benchmarks");
    auto results = BenchmarkList::getInstance().getResults();
    
    for (const auto& r : results)
        std::cout << r.description.name << ", " << r.description.category
                  << "\n\t" << r.description.description
                  << "\n\t" << getDuration (r) << "\n";
    
    if (argv > 1)
    {
        if (auto query = createSQLQueryString (std::move (results));
            query.isNotEmpty())
        {
            if (const juce::File destFile (argc[1]);
                destFile != juce::File()
                && destFile.getParentDirectory().createDirectory()
                && destFile.replaceWithText (query))
            {
                std::cout << "INFO: Wrote query to: " << destFile.getFullPathName() << " \n";
            }
            else
            {
                std::cout << juce::String ("ERROR: Unable to write query string at {}!\n").replace ("{}", destFile.getFullPathName());
            }
        }
        else
        {
            std::cout << "ERROR: Failed to create query string!\n";
        }
    }

    return anyFailed;
}
