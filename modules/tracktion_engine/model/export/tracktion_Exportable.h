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
        ProjectItemID itemID;
        double firstTimeUsed, lengthUsed;

        bool operator== (const ReferencedItem& other) const  { return firstTimeUsed == other.firstTimeUsed
                                                                        && lengthUsed == other.lengthUsed
                                                                        && itemID == other.itemID; }
        bool operator!= (const ReferencedItem& other) const  { return ! operator== (other); }
    };

    virtual juce::Array<ReferencedItem> getReferencedItems() = 0;

    virtual void reassignReferencedItem (const ReferencedItem&,
                                         ProjectItemID newID,
                                         double newStartTime) = 0;

    //==============================================================================
    /** Returns all the Exportables contained in an Edit. */
    static juce::Array<Exportable*> addAllExportables (Edit&);
};

} // namespace tracktion_engine
