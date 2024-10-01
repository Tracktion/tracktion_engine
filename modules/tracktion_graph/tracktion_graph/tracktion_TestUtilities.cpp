/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#include "tracktion_TestUtilities.h"
#include "../../3rd_party/choc/text/choc_StringUtilities.h"

#if __has_include (<cxxabi.h>)
#include <cxxabi.h>
#endif

namespace tracktion { inline namespace graph { namespace test_utilities
{
    inline std::string demangle (std::string name)
    {
       #if __has_include (<cxxabi.h>)
        int status;

        if (char* demangled = abi::__cxa_demangle (name.c_str(), nullptr, nullptr, &status); status == 0)
        {
            std::string demangledString (demangled);
            free (demangled);
            return demangledString;
        }
       #endif

        return name;
    }

    std::string createGraphDescription (Node& node)
    {
        struct NodeInfo
        {
            std::string label;
            size_t memorySizeBytes = 0;
            int numOutputs = -1;
            bool containsInternalNodes = false;
            int latencyNumSamples = 0;
            size_t nodeID = 0;
        };

        struct Visitor
        {
            static std::pair<std::string, std::string> getNodeName (Node& n)
            {
                return { std::to_string (reinterpret_cast<uintptr_t> (&n)),
                         demangle (typeid (n).name()) };
            }

            static void visitInputs (Node& n, std::vector<std::string>& edges, std::map<std::string, NodeInfo>& map)
            {
                const auto [destNodeID, destNodeString] = getNodeName (n);
                auto& destNodeInfo = map[destNodeID];
                destNodeInfo.label = destNodeString;
                destNodeInfo.memorySizeBytes = n.getAllocatedBytes();
                destNodeInfo.containsInternalNodes = ! n.getInternalNodes().empty();
                destNodeInfo.numOutputs = n.numOutputNodes;
                destNodeInfo.latencyNumSamples = n.getNodeProperties().latencyNumSamples;
                destNodeInfo.nodeID = n.getNodeProperties().nodeID;

                for (auto input : n.getDirectInputNodes())
                {
                    const auto [sourceNodeID, sourceNodeString] = getNodeName (*input);
                    auto& sourceNodeInfo = map[sourceNodeID];
                    sourceNodeInfo.label = sourceNodeString;

                    const auto edge = sourceNodeID + "->" + destNodeID;

                    if (std::find (edges.begin(), edges.end(), edge) == edges.end())
                        edges.push_back (edge);

                    visitInputs (*input, edges, map);
                }
            }
        };

        std::map<std::string, NodeInfo> idNameMap;
        std::vector<std::string> edges;
        Visitor::visitInputs (node, edges, idNameMap);

        // Build graph
        std::string output;
        output += "digraph {\n";

        for (auto [id, info] : idNameMap)
        {
            std::string label = choc::text::replace (info.label,
                                                     "tracktion::engine", "te",
                                                     "tracktion::graph", "tg");

            std::string colourString, shapeString;

            if (info.memorySizeBytes > 0)
            {
                label += "*";
                colourString += "color=red ";
            }

            label += " (" + std::to_string (info.numOutputs) + ")";
            label += choc::text::replace ("\nID: {}", "{}", std::to_string (info.nodeID));

            if (info.latencyNumSamples > 0)
                label += choc::text::replace ("\nLt: {}", "{}", std::to_string (info.latencyNumSamples));

            if (info.containsInternalNodes)
                shapeString += "shape=box";

            output += id + "[label=\"" + label + "\" " + colourString + shapeString + "]\n";
        }

        for (auto edge : edges)
            output += edge += "\n";

        output += "\n}";

        return output;
    }
}}}
