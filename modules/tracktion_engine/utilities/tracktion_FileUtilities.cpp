/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion { inline namespace engine
{

juce::File getNonExistentSiblingWithIncrementedNumberSuffix (const juce::File& file, bool addHashSymbol)
{
    if (! file.existsAsFile())
        return file;

    auto name = juce::File::createLegalFileName (file.getFileNameWithoutExtension());

    int drop = 0;
    bool foundDigit = false;
    bool foundHash  = ! addHashSymbol;

    for (int i = name.length(); --i >= 0;)
    {
        if (name[i] == '#' && foundDigit)
            foundHash = true;
        else if (juce::String ("0123456789").containsChar (name[i]))
            foundDigit = true;
        else
            break;

        drop++;
    }

    bool inForm = foundDigit && foundHash;
    int number = inForm ? name.getTrailingIntValue() : 0;
    name = inForm ? name.dropLastCharacters(drop).trimEnd() : name;

    juce::File newFileName;

    do
    {
        number++;

        auto nameWithNumber = name + (addHashSymbol ? " #" : " ")
                                + juce::String (number) + file.getFileExtension();

        newFileName = file.getParentDirectory()
                          .getChildFile (nameWithNumber);

    } while (newFileName.exists());

    return newFileName;
}

//==============================================================================
bool isMidiFile (const juce::File& f)             { return f.hasFileExtension ("mid;rmi;rmid;midi"); }
bool isTracktionEditFile (const juce::File& f)    { return f.hasFileExtension (editFileSuffix) || f.hasFileExtension (legacyEditFileSuffix); }
bool isTracktionArchiveFile (const juce::File& f) { return f.hasFileExtension (archiveFileSuffix); }
bool isTracktionProjectFile (const juce::File& f) { return f.hasFileExtension (projectFileSuffix); }
bool isTracktionPresetFile (const juce::File& f)  { return f.hasFileExtension (presetFileSuffix); }

//==============================================================================
FileDragList::Ptr FileDragList::getFromDrag (const juce::DragAndDropTarget::SourceDetails& s)
{
    return dynamic_cast<FileDragList*> (s.description.getObject());
}

juce::var FileDragList::create (const juce::Array<juce::File>& files, PreferredLayout preferredLayout)
{
    FileDragList* l = new FileDragList();
    l->preferredLayout = preferredLayout;
    l->files = files;
    return l;
}

juce::var FileDragList::create (const juce::File& file, PreferredLayout peferredLayout)
{
    juce::Array<juce::File> files;
    files.add (file);
    return create (files, peferredLayout);
}

}} // namespace tracktion { inline namespace engine
