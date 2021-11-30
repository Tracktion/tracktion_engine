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
class ValueTreeBenchmarks  : public juce::UnitTest
{
public:
    ValueTreeBenchmarks()
        : UnitTest ("ValueTrees", "benchmarks")
    {}

    void runTest() override
    {
        auto r = Random (424242);
        const auto seed = r.getSeed();
        const int depth = 7;
        
        ValueTree v1;
        MemoryOutputStream mo;
        std::unique_ptr<XmlElement> xml;
        juce::String xmlString;
        
        beginTest ("Create random tree");
        {
            ScopedBenchmark sb (getDescription ("Create random tree", seed, depth));
            v1 = createRandomTree (nullptr, depth, r);
        }

        beginTest ("Copy random tree");
        {
            ScopedBenchmark sb (getDescription ("Copy random tree", seed, depth));
            ValueTree v2 = v1.createCopy();
        }

        beginTest ("Save random tree as XML string");
        {
            ScopedBenchmark sb (getDescription ("Save random tree as XML string", seed, depth));
            xmlString = v1.toXmlString();
        }

        beginTest ("Save random tree as XML");
        {
            ScopedBenchmark sb (getDescription ("Save random tree as XML", seed, depth));
            xml = v1.createXml();
        }

        beginTest ("Save random tree as binary");
        {
            ScopedBenchmark sb (getDescription ("Save random tree as binary", seed, depth));
            v1.writeToStream (mo);
        }

        beginTest ("Load from XML string");
        {
            ScopedBenchmark sb (getDescription ("Load from XML string", seed, depth));
            auto v2 = ValueTree::fromXml (xmlString);
        }
        
        beginTest ("Load from XML");
        {
            ScopedBenchmark sb (getDescription ("Load from XML", seed, depth));
            auto v2 = ValueTree::fromXml (*xml);
        }
        
        beginTest ("Load from binary");
        {
            ScopedBenchmark sb (getDescription ("Load from binary", seed, depth));
            MemoryInputStream mi (mo.getData(), mo.getDataSize(), false);
            auto v2 = ValueTree::readFromStream (mi);
        }
    }

private:
    BenchmarkDescription getDescription (std::string bmName, int64_t seed, int depth)
    {
        const auto bmCategory = (getName() + "/" + getCategory()).toStdString();
        const auto bmDescription = "seed: " + std::to_string (seed) + ", depth: " + std::to_string (depth);
        
        return { std::hash<std::string>{} (bmName + bmCategory + bmDescription),
                 bmCategory, bmName, bmDescription };
    }
    
    static String createRandomIdentifier (Random& r)
    {
        char buffer[50] = { 0 };
        const char chars[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-:";

        for (int i = 1 + r.nextInt (numElementsInArray (buffer) - 2); --i >= 0;)
            buffer[i] = chars[r.nextInt (sizeof (chars) - 1)];

        String result (buffer);

        if (! XmlElement::isValidXmlName (result))
            result = createRandomIdentifier (r);

        return result;
    }

    static String createRandomWideCharString (Random& r)
    {
        juce_wchar buffer[50] = { 0 };

        for (int i = r.nextInt (numElementsInArray (buffer) - 1); --i >= 0;)
        {
            if (r.nextBool())
            {
                do
                {
                    buffer[i] = (juce_wchar) (1 + r.nextInt (0x10ffff - 1));
                }
                while (! CharPointer_UTF16::canRepresent (buffer[i]));
            }
            else
                buffer[i] = (juce_wchar) (1 + r.nextInt (0x7e));
        }

        return CharPointer_UTF32 (buffer);
    }

    static ValueTree createRandomTree (UndoManager* undoManager, int depth, Random& r)
    {
        ValueTree v (createRandomIdentifier (r));

        for (int i = depth; --i >= 0;)
        {
            switch (r.nextInt (4))
            {
                case 0: v.setProperty (createRandomIdentifier (r), createRandomWideCharString (r), undoManager); break;
                case 1: v.setProperty (createRandomIdentifier (r), r.nextInt(), undoManager); break;
                case 2: v.setProperty (createRandomIdentifier (r), r.nextBool(), undoManager); break;
                case 3: v.setProperty (createRandomIdentifier (r), r.nextDouble(), undoManager); break;
                default: break;
            }
            
            if (depth > 0)
                v.addChild (createRandomTree (undoManager, depth - 1, r), r.nextInt (v.getNumChildren() + 1), undoManager);
        }

        return v;
    }

};

static ValueTreeBenchmarks valueTreeBenchmarks;


//==============================================================================
//==============================================================================
int main (int, char**)
{
    ScopedJuceInitialiser_GUI init;
    const auto anyFailed = TestRunner::runTests ({}, "benchmarks");

    if (publishToAirtable (SystemStats::getEnvironmentVariable ("AT_BASE_ID", {}).toStdString(),
                           SystemStats::getEnvironmentVariable ("AT_API_KEY", {}).toStdString(),
                           BenchmarkList::getInstance().getResults())
    {
        std::cout << "INFO: Published benchmark results\n";
    }
    else
    {
        std::cout << "INFO: Failed to publish!\n";
    }

    return anyFailed;
}
