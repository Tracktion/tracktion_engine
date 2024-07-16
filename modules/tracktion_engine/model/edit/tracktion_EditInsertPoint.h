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

/**
    An EditInsertPoint is like a "paste location".
    It can be updated as user iteraction warrents e.g. by selecting tracks/plugins/clips etc. and calling setNextInsertPoint.
    Then when new content is added, it can use the insert point to  find the track and time position where content should
    be placed @see chooseInsertPoint
    This is used by some other classes such as the Clipboard.
*/
class EditInsertPoint
{
public:
    /** Creates an EditInsertPoint for a given Edit. */
    EditInsertPoint (Edit&);

    /** Destructor. */
    virtual ~EditInsertPoint() = default;

    /** Sets the next insertion point to a given ClipOwner and Time (if provided).
        N.B. This will only be updated of the ClipOwner provides a valid EditItemID.
        @param ClipOwner The ClipOwner to insert in to
        @param std::optional<TimePosition> An optional time position to insert at
        @returns If the insert point has been changed, this will return true, false otherwise.
    */
    bool setNextInsertPoint (ClipOwner*, std::optional<TimePosition>);

    /** Sets the next insertion point to a given time and track. */
    void setNextInsertPoint (TimePosition, const juce::ReferenceCountedObjectPtr<Track>&);

    /** Updates the time but not the track. */
    void setNextInsertPoint (TimePosition);

    /** Sets a flag to signify chooseInsertPoint should use the selected track as the insertion point. */
    void setNextInsertPointAfterSelected();

    /** Locks the insert point so calls to setNextInsertPoint won't have any effect. */
    void lockInsertPoint (bool lock) noexcept;

    /** Returns the track and time position content should be pasted at.
        @returns Track  The track to insert in to
        @returns start  The time at which to insert at
        @param pasteAfterSelection  Whether to ignore the previously set Track and use the
                                    SelectionManager to set the insert point
        @param SelectionManger      The SelectionManager to use
    */
    virtual void chooseInsertPoint (juce::ReferenceCountedObjectPtr<Track>&,
                                    TimePosition& start, bool pasteAfterSelection, SelectionManager*);

    /** This is the same as the other overload except you can supply a predicate to determine if tracks are
        allowed to be considered or not. Useful for when they are hidden etc.
        @param allowedTrackPredicate    A predicate to determine if the track can be pasted in to
    */
    void chooseInsertPoint (juce::ReferenceCountedObjectPtr<Track>&,
                            TimePosition& start, bool pasteAfterSelection, SelectionManager*,
                            std::function<bool (Track&)> allowedTrackPredicate);

    /** A struct representing a resolved insertion point. */
    struct Placement
    {
        juce::ReferenceCountedObjectPtr<Track> track;   /**< The track to insert in to. */
        ClipOwner* clipOwner = nullptr;                 /**< The clip owner to insert to (might be the same as track). */
        std::optional<TimePosition> time;               /**< The time to insert at. */
    };

    /** Returns a Placement representing where content should be pasted at.
        @param pasteAfterSelection  Whether to ignore the previously set Track and use the
                                    SelectionManager to set the insert point
        @param SelectionManger      The SelectionManager to use
        @returns Placement          Where to insert content
    */
    virtual Placement chooseInsertPoint (bool pasteAfterSelection, SelectionManager*,
                                         std::function<bool (Track&)> allowedTrackPredicate);

protected:
    Edit& edit;
    TimePosition nextInsertPointTime;
    EditItemID nextInsertPointTrack, nextInsertPointClipOwner;
    int lockInsertPointCount = 0;
    bool nextInsertIsAfterSelected = false;
};

}} // namespace tracktion { inline namespace engine
