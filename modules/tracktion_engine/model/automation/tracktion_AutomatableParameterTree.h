/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    You may use this code under the terms of the GPL v3 - see LICENCE.md for details.
    For the technical preview this file cannot be licensed commercially.
*/

namespace tracktion { inline namespace engine
{

class AutomatableParameterTree
{
public:
    AutomatableParameterTree()  : rootNode (new TreeNode ("root"))
    {
    }

    enum NodeType
    {
        Parameter,
        Group
    };

    struct TreeNode
    {
        TreeNode (const juce::String& groupName)
            : name (groupName), type (AutomatableParameterTree::Group)
        {}

        TreeNode (const juce::ReferenceCountedObjectPtr<AutomatableParameter>& param)
            : parameter (param), type (AutomatableParameterTree::Parameter)
        {}

        juce::String name;
        juce::ReferenceCountedObjectPtr<AutomatableParameter> parameter;
        NodeType type;
        juce::OwnedArray<TreeNode> subNodes;
        TreeNode* parent = nullptr;

        void addSubNode (TreeNode* node)
        {
            subNodes.add (node);
            node->parent = this;
        }

        juce::String getGroupName() const         { return name; }

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TreeNode)
    };

    //==============================================================================
    TreeNode* getNodeForParameter (AutomatableParameter& param) const
    {
        return getNodeForParameter (rootNode.get(), param);
    }

    void clear()
    {
        rootNode = std::make_unique<TreeNode> ("root");
    }

    std::unique_ptr<TreeNode> rootNode;

private:
    static TreeNode* getNodeForParameter (TreeNode* node, AutomatableParameter& param)
    {
        for (auto subNode : node->subNodes)
        {
            if (subNode->type == Parameter)
            {
                if (subNode->parameter.get() == &param)
                    return subNode;
            }
            else if (subNode->type == Group)
            {
                if (auto res = getNodeForParameter (subNode, param))
                    return res;
            }
        }

        return {};
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AutomatableParameterTree)
};

}} // namespace tracktion { inline namespace engine
