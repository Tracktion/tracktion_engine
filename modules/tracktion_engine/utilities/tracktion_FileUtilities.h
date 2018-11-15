/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
{

bool isMidiFile (const juce::File&);
bool isTracktionEditFile (const juce::File&);
bool isTracktionArchiveFile (const juce::File&);
bool isTracktionProjectFile (const juce::File&);
bool isTracktionPresetFile (const juce::File&);

const char* const projectFileSuffix = ".tracktion";
const char* const editFileSuffix = ".tracktionedit";
const char* const legacyEditFileSuffix = ".trkedit";
const char* const archiveFileSuffix = ".trkarch";

const char* const soundFileWildCard = "*.wav;*.aiff;*.aif;*.ogg;*.mp3;*.flac;*.au;*.voc;*.caf;*.w64;*.rx2;*.rcy;*.rex;*.wfaf";
const char* const soundFileAndMidiWildCard = "*.wav;*.aiff;*.aif;*.ogg;*.mp3;*.mid;*.midi;*.flac;*.au;*.voc;*.caf;*.w64;*.rx2;*.rcy;*.rex;*.wfaf";
const char* const midiFileWildCard = "*.midi;*.mid";

const char* const keyMapWildCard = "*.tracktionkeys";
const char* const keyMapSuffix = ".tracktionkeys";

const char* const customControllerFileSuffix = ".trkctrlr";
const char* const customProgramSetWildcard = "*.trkmidi;*.midnam";
const char* const customProgramSetFileSuffix = ".trkmidi";
const char* const presetFileSuffix = ".trkpreset";
const char* const rackFileSuffix = ".trkrack";
const char* const scriptFileSuffix = ".tracktionscript";
const char* const grooveTemplateSuffix = ".trkgroove";
const char* const grooveTemplateWildCard = "*.trkgroove";

//==============================================================================
juce::File getNonExistentSiblingWithIncrementedNumberSuffix (const juce::File&, bool addHashSymbol);


//==============================================================================
struct ScopedDirectoryDeleter
{
    ScopedDirectoryDeleter (const juce::File& dir) : f (dir)  {}
    ~ScopedDirectoryDeleter()  { f.deleteRecursively(); }

private:
    juce::File f;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ScopedDirectoryDeleter)
};

//==============================================================================
struct SortFilesFoldersFirst
{
    static int compareElements (const juce::File& first, const juce::File& second)
    {
        const bool d1 = first.isDirectory();
        const bool d2 = second.isDirectory();

        if (d1 == d2)
            return first.getFileName().compareNatural (second.getFileName());

        return d1 ? -1 : 1;
    }
};

//==============================================================================
struct FileDragList  : public juce::ReferenceCountedObject
{
    enum PreferredLayout
    {
        horizontal,
        vertical,
    };

    PreferredLayout preferredLayout = horizontal;
    juce::Array<juce::File> files;

    using Ptr = juce::ReferenceCountedObjectPtr<FileDragList>;

    static FileDragList::Ptr getFromDrag (const juce::DragAndDropTarget::SourceDetails&);
    static juce::var create (const juce::Array<juce::File>&, PreferredLayout peferredLayout);
    static juce::var create (const juce::File& file, PreferredLayout peferredLayout);
};

} // namespace tracktion_engine
