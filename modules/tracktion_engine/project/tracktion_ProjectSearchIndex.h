/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
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
/** turns a keyword string into a search condition tree.. */
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

} // namespace tracktion_engine
