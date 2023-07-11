/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#include "tracktion_TestUtilities.h"

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
            bool containsInternalNodes = false;
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
            std::string colourString, shapeString;

            if (info.memorySizeBytes > 0)
                colourString += "color=red ";

            if (info.containsInternalNodes)
                shapeString += "shape=box";

            output += id + "[label=\"" + info.label + "\" " + colourString + shapeString + "]\n";
        }

        for (auto edge : edges)
            output += edge += "\n";
        
        output += "\n}";
        
        return output;
    }
}}}
