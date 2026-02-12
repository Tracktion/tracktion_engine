/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion { inline namespace engine
{

//==============================================================================
/**
    An interface for objects within an edit that can be exported.

    This allows the export/archive stuff to find all the material that
    it depends on to play back a section of an edit.
*/
class Exportable
{
public:
    virtual ~Exportable() = default;

    //==============================================================================
    struct ReferencedItem
    {
        ProjectItemRef itemRef;
        double firstTimeUsed, lengthUsed;

        bool operator== (const ReferencedItem& other) const  { return firstTimeUsed == other.firstTimeUsed
                                                                        && lengthUsed == other.lengthUsed
                                                                        && itemRef == other.itemRef; }
        bool operator!= (const ReferencedItem& other) const  { return ! operator== (other); }
    };

    virtual juce::Array<ReferencedItem> getReferencedItems() = 0;

    virtual void reassignReferencedItem (const ReferencedItem&,
                                         ProjectItemRef newRef,
                                         double newStartTime) = 0;

    //==============================================================================
    /** Returns all the Exportables contained in an Edit. */
    static juce::Array<Exportable*> addAllExportables (Edit&);
};

}} // namespace tracktion { inline namespace engine
