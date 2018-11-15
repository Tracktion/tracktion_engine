/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


File getNonExistentSiblingWithIncrementedNumberSuffix (const File& file, bool addHashSymbol)
{
    if (! file.existsAsFile())
        return file;

    auto name = File::createLegalFileName (file.getFileNameWithoutExtension());

    int drop = 0;
    bool foundDigit = false;
    bool foundHash  = ! addHashSymbol;

    for (int i = name.length(); --i >= 0;)
    {
        if (name[i] == '#' && foundDigit)
            foundHash = true;
        else if (String ("0123456789").containsChar (name[i]))
            foundDigit = true;
        else
            break;

        drop++;
    }

    bool inForm = foundDigit && foundHash;
    int number = inForm ? name.getTrailingIntValue() : 0;
    name = inForm ? name.dropLastCharacters(drop).trimEnd() : name;

    File newFileName;

    do
    {
        number++;

        String nameWithNumber = name + (addHashSymbol ? " #" : " ") + String (number) + file.getFileExtension();
        newFileName = file.getParentDirectory()
                          .getChildFile (nameWithNumber);

    } while (newFileName.exists());

    return newFileName;
}

//==============================================================================
bool isMidiFile (const File& f)             { return f.hasFileExtension ("mid;rmi;rmid;midi"); }
bool isTracktionEditFile (const File& f)    { return f.hasFileExtension (editFileSuffix) || f.hasFileExtension (legacyEditFileSuffix); }
bool isTracktionArchiveFile (const File& f) { return f.hasFileExtension (archiveFileSuffix); }
bool isTracktionProjectFile (const File& f) { return f.hasFileExtension (projectFileSuffix); }
bool isTracktionPresetFile (const File& f)  { return f.hasFileExtension (presetFileSuffix); }

//==============================================================================
FileDragList::Ptr FileDragList::getFromDrag (const DragAndDropTarget::SourceDetails& s)
{
    return dynamic_cast<FileDragList*> (s.description.getObject());
}

var FileDragList::create (const Array<File>& files, PreferredLayout preferredLayout)
{
    FileDragList* l = new FileDragList();
    l->preferredLayout = preferredLayout;
    l->files = files;
    return l;
}

var FileDragList::create (const File& file, PreferredLayout peferredLayout)
{
    Array<File> files;
    files.add (file);
    return create (files, peferredLayout);
}
