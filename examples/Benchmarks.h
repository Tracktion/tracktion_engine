/*******************************************************************************
 The block below describes the properties of this PIP. A PIP is a short snippet
 of code that can be read by the Projucer and used to generate a JUCE project.

 BEGIN_JUCE_PIP_METADATA

  name:             Benchmarks
  version:          0.0.1
  vendor:           Tracktion
  website:          www.tracktion.com
  description:      Runs the benchmarks for the engine.

  dependencies:     juce_audio_basics, juce_audio_devices, juce_audio_formats, juce_audio_processors, juce_audio_utils,
                    juce_core, juce_data_structures, juce_dsp, juce_events, juce_graphics,
                    juce_gui_basics, juce_gui_extra, juce_osc, tracktion_engine, tracktion_graph
  exporters:        linux_make, vs2017, xcode_iphone, xcode_mac

  moduleFlags:      JUCE_STRICT_REFCOUNTEDPOINTER=1, JUCE_PLUGINHOST_AU=1, JUCE_PLUGINHOST_VST3=1
  defines:          TRACKTION_BENCHMARKS=1, JUCE_MODAL_LOOPS_PERMITTED=1

  type:             Console
  mainClass:        Benchmarks

 END_JUCE_PIP_METADATA

*******************************************************************************/

#pragma once

#include "common/tracktion_graph_Dev.h"


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
        fields->setProperty ("benchmark_hash",              juce::String (static_cast<uint64> (r.description.hash)));
        fields->setProperty ("benchmark_category",          juce::String (r.description.category).quoted ('\''));
        fields->setProperty ("benchmark_name",              juce::String (r.description.name).quoted ('\''));
        fields->setProperty ("benchmark_description",       juce::String (r.description.description).quoted ('\''));
        fields->setProperty ("benchmark_platform",          juce::String (r.description.platform).quoted ('\''));
        fields->setProperty ("benchmark_time",              r.date.toISO8601 (true).trimCharactersAtEnd ("Z").quoted ('\''));
        fields->setProperty ("benchmark_duration",          r.totalSeconds);
        fields->setProperty ("benchmark_duration_min",      r.minSeconds);
        fields->setProperty ("benchmark_duration_max",      r.maxSeconds);
        fields->setProperty ("benchmark_duration_mean",     r.meanSeconds);
        fields->setProperty ("benchmark_duration_variance", r.varianceSeconds);
        fields->setProperty ("benchmark_cycles_total",      (juce::int64) r.totalCycles);
        fields->setProperty ("benchmark_cycles_min",        (juce::int64) r.minCycles);
        fields->setProperty ("benchmark_cycles_max",        (juce::int64) r.maxCycles);
        fields->setProperty ("benchmark_cycles_mean",       (juce::int64) r.meanCycles);
        fields->setProperty ("benchmark_cycles_variance",   r.varianceCycles);

        records.add (fields.get());
    }

    juce::var valuesToAdd (std::move (records));
    auto jsonString = juce::JSON::toString (valuesToAdd, false);

    const auto url = juce::URL ("https://appstats.tracktion.com/benchmarkapi.php")
                        .withParameter ("api_key", apiKey)
                        .withParameter ("request", "push_results")
                        .withParameter ("content", jsonString);

    if (auto inputStream = url.createInputStream (URL::InputStreamOptions (URL::ParameterHandling::inPostData)
                                                                        .withExtraHeaders ("User-Agent: Mozilla/2.2")))
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
                  << "\n\t[seconds]\t" << r.totalSeconds << "\t(min: " << r.minSeconds << ", max: " << r.maxSeconds  << ", mean: " << r.meanSeconds << ", var: " << r.varianceSeconds << ")"
                  << "\n\t[cycles]\t" << r.totalCycles << "\t(min: " << r.minCycles << ", max: " << r.maxCycles  << ", mean: " << r.meanCycles << ", var: " << r.varianceCycles << ")\n\n";

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
