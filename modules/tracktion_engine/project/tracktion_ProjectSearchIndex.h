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

struct IndexedWord;
class SearchOperation;

//==============================================================================
class ProjectSearchIndex
{
public:
    ProjectSearchIndex (Project&);

    void addClip (const ProjectItem::Ptr&);
    void findMatches (SearchOperation&, juce::Array<ProjectItemID>& results);

    void writeToStream (juce::OutputStream&);
    void readFromStream (juce::InputStream&);

    IndexedWord* findWordMatch (const juce::String& word) const;

    Project& project;
    juce::OwnedArray<IndexedWord> index;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProjectSearchIndex)
};

//==============================================================================
/** Turns a keyword string into a search condition tree. */
SearchOperation* createSearchForKeywords (const juce::String& keywords);

//==============================================================================
class SearchOperation
{
public:
    SearchOperation (SearchOperation* in1 = nullptr,
                     SearchOperation* in2 = nullptr);
    virtual ~SearchOperation();

    virtual juce::Array<int> getMatches (ProjectSearchIndex&) = 0;

protected:
    const std::unique_ptr<SearchOperation> in1, in2;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SearchOperation)
};

}} // namespace tracktion { inline namespace engine
