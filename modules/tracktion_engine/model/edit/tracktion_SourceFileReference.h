/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
{

//==============================================================================
/**
    This class wraps a string that is generally held in a 'source' property, and which
    is a reference to some sort of file, either in the form of a local filename or
    as a project ID that can be resolved.
*/
class SourceFileReference
{
public:
    SourceFileReference (Edit&, juce::ValueTree&, const juce::Identifier& prop);
    ~SourceFileReference();

    //==============================================================================
    Edit& edit;
    juce::CachedValue<juce::String> source;

    juce::File getFile() const;
    bool isUsingProjectReference() const;
    ProjectItemID getSourceProjectItemID() const;
    ProjectItem::Ptr getSourceProjectItem() const;

    void setToDirectFileReference (const juce::File&, bool useRelativePath);

    /** Points this source at a new file via a project item.
        If updateProjectItem is true and there isn't already a media id for this file,
        it'll create one and add it to the project, or will update the current ProjectItem
        if it doesn't yet point to a real file.
    */
    void setToProjectFileReference (const juce::File&, bool updateProjectItem);

    void setToProjectFileReference (ProjectItemID);

    static juce::String findPathFromFile (Edit&, const juce::File&, bool useRelativePath);
    static juce::File findFileFromString (Edit&, const juce::String& source);

private:
    juce::ValueTree state;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SourceFileReference)
};


} // namespace tracktion_engine
