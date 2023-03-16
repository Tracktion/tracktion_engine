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

//==============================================================================
/**
*/
class CollectionClip   : public TrackItem
{
public:
    CollectionClip (Track&);
    ~CollectionClip() override;

    using Ptr = juce::ReferenceCountedObjectPtr<CollectionClip>;

    void addClip (Clip*);
    void removeClip (Clip*);
    bool removeClip (EditItemID);
    bool containsClip (Clip* clip) const noexcept       { return clips.contains (clip); }

    juce::String getName() const override;

    ClipPosition getPosition() const override           { return { range, TimeDuration() }; }
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

    TimeRange range;

private:
    Track* track;
    Clip::Array clips;
    bool dragging = false;
    EditItemID groupID;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CollectionClip)
};

}} // namespace tracktion { inline namespace engine
