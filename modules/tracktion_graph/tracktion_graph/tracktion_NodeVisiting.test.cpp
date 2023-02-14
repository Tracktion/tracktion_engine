/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/


namespace tracktion { inline namespace graph
{

#if GRAPH_UNIT_TESTS_NODEVISITING

using namespace test_utilities;

//==============================================================================
//==============================================================================
class NodeVistingTests : public juce::UnitTest
{
public:
    NodeVistingTests()
        : juce::UnitTest ("NodeVisting", "tracktion_graph")
    {
    }
    
    void runTest() override
    {
        runVisitTests();
    }

private:
    //==============================================================================
    /** Calls the visitor for any direct inputs to the node and then calls
        the visitor function on this node.
        
        This method is not stateful so may end up calling nodes more than once and
        could be infinite if there are cycles in the graph.
     
        @param Visitor has the signature @code void (Node&) @endcode
    */
    template<typename Visitor>
    void visitInputs (Node& node, Visitor&& visitor)
    {
        for (auto n : node.getDirectInputNodes())
            visitInputs (*n, visitor);
        
        visitor (node);
    }

    //==============================================================================
    //==============================================================================
    void runVisitTests()
    {
        /* This test creates a graph in this topology and tests the visiting algorithms.
           Here E is an input to F.
         
                    A
                   /|\
                  B C E
                 /| | |
                D F G |
                  |___|
        */
        
        auto d = makeNode<SinNode> (1.0f);
        auto D = d.get();
        auto f = makeNode<ReturnNode> (makeNode<SinNode> (2.0f), 1);
        auto F = f.get();
        auto g = makeNode<SinNode> (3.0f);
        auto G = g.get();
        auto c = makeNode<SinkNode> (std::move (g));
        auto C = c.get();
        auto e = makeNode<SendNode> (makeNode<SinNode> (4.0f), 1);
        auto E = e.get();
        auto b = makeBaicSummingNode ({ d.release(), f.release() });
        auto B = b.get();
        auto a = makeBaicSummingNode ({ b.release(), c.release(), e.release() });
        auto A = a.get();
        
        // Prepare the topology
        auto nodeGraph = createNodeGraph (std::move (a));
        
        std::vector<Node*> allNodes { A, B, C, D, E, F, G };

        for (auto n : allNodes)
            n->initialise ({ 44100.0, 512, *nodeGraph });
        
        auto trimEndNodes = [&] (std::vector<Node*> nodes)
        {
            nodes.erase (std::remove_if (nodes.begin(), nodes.end(),
                                         [&] (auto n) { return std::find (allNodes.begin(), allNodes.end(), n) == allNodes.end(); }),
                         nodes.end());

            return nodes;
        };

        beginTest ("Basic visting");
        {
            expectEquals<uint64_t> (allNodes.size(), 7);
        
            std::vector<Node*> nodes;
            visitInputs (*A, [&] (auto& node)
            {
                if (std::find (allNodes.begin(), allNodes.end(), &node) == allNodes.end())
                    return;
                
                std::cout << getNodeLetter (allNodes, &node) << "\n";
                nodes.push_back (&node);
            });
            
            // E will be visited twice in this example, once through the return and once directly
            expectNodeOrder (allNodes, nodes, { D, E, F, B, G, C, E, A });
        }
        
        beginTest ("Visit nodes once");
        {
            std::vector<Node*> nodes;
            visitNodes (*A, [&] (auto& node)
            {
                if (std::find (allNodes.begin(), allNodes.end(), &node) == allNodes.end())
                    return;
                
                std::cout << getNodeLetter (allNodes, &node) << "\n";
                nodes.push_back (&node);
            }, false);
            
            expectNodeOrder (allNodes, nodes, { D, E, F, B, G, C, A });
            expectNodeOrder (allNodes, nodes, trimEndNodes (getNodes (*A, VertexOrdering::postordering)));
        }
        
        beginTest ("Vist nodes with ordering");
        {
            expectNodeOrder (allNodes, trimEndNodes (getNodes (*A, VertexOrdering::preordering)),
                             { A, B, D, F, E, C, G });
            expectNodeOrder (allNodes, trimEndNodes (getNodes (*A, VertexOrdering::reversePreordering)),
                             { G, C, E, F, D, B, A });
            expectNodeOrder (allNodes, trimEndNodes (getNodes (*A, VertexOrdering::postordering)),
                             { D, E, F, B, G, C, A });
            expectNodeOrder (allNodes, trimEndNodes (getNodes (*A, VertexOrdering::reversePostordering)),
                             { A, C, G, B, F, E, D });
            expectNodeOrder (allNodes, trimEndNodes (getNodes (*A, VertexOrdering::bfsPreordering)),
                             { A, B, C, E, D, F, G });
            expectNodeOrder (allNodes, trimEndNodes (getNodes (*A, VertexOrdering::bfsReversePreordering)),
                             { G, F, D, E, C, B, A });
        }
    }
    
    static std::string getNodeLetter (const std::vector<Node*>& nodes, Node* node)
    {
        auto found = std::find (nodes.begin(), nodes.end(), node);
        
        if (found != nodes.end())
            return { 1, static_cast<char> (std::distance (nodes.begin(), found) + 65) };
        
        return "-";
    }
    
    void expectNodeOrder (const std::vector<Node*>& ascendingNodes,
                          const std::vector<Node*>& actual, const std::vector<Node*>& expected)
    {
        jassert (actual.size() == expected.size());
        auto areEqual = std::equal (actual.begin(), actual.end(), expected.begin(), expected.end());
        expect (areEqual, "Node order not equal");
        
        if (! areEqual)
        {
            std::cout << "Expected:\tActual:\n";

            for (size_t i = 0; i < actual.size(); ++i)
                std::cout << getNodeLetter (ascendingNodes, expected[i]) << "\t\t\t" << getNodeLetter (ascendingNodes, actual[i]) << "\n";
        }
    }
};

static NodeVistingTests nodeVistingTests;

#endif

}}
