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
*/
class CollectionClip   : public TrackItem
{
public:
    CollectionClip (Track&);
    ~CollectionClip();

    using Ptr = juce::ReferenceCountedObjectPtr<CollectionClip>;

    void addClip (Clip*);
    void removeClip (Clip*);
    bool removeClip (EditItemID);
    bool containsClip (Clip* clip) const noexcept       { return clips.contains (clip); }

    juce::String getName() override;

    ClipPosition getPosition() const override           { return { range, 0 }; }
    Track* getTrack() const override                    { return track; }

    bool moveToTrack (Track&);
    void updateStartAndEnd();

    FolderTrack* getFolderTrack() const noexcept;

    juce::String getSelectableDescription() override    { return TRANS("Collection clips"); }
    int getNumClips() const noexcept                    { return clips.size(); }
    Clip::Ptr getClip (int index) const                 { return clips[index]; }
    const Clip::Array& getClips() const noexcept        { return clips; }

    void setDragging (bool nowDragging) noexcept        { dragging = nowDragging; }
    bool isDragging() const noexcept                    { return dragging; }


    EditItemID getGroupID() const noexcept              { return groupID; }
    void setGroupID (EditItemID newGroupID)             { groupID = newGroupID; }

    bool isGroupCollection() const                      { return getFolderTrack() == nullptr; }

    EditTimeRange range;

private:
    Track* track;
    Clip::Array clips;
    bool dragging = false;
    EditItemID groupID;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CollectionClip)
};

} // namespace tracktion_engine
