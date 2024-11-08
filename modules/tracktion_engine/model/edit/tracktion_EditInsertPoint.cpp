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

EditInsertPoint::EditInsertPoint (Edit& e)
    : edit (e)
{
}

bool EditInsertPoint::setNextInsertPoint (ClipOwner* clipOwner, std::optional<TimePosition> time)
{
    if (lockInsertPointCount != 0)
        return false;

    if (time)
        setNextInsertPoint (*time);

    nextInsertPointClipOwner = clipOwner != nullptr ? clipOwner->getClipOwnerID()
                                                    : EditItemID();

    if (auto cs = dynamic_cast<ClipSlot*> (clipOwner))
        nextInsertPointTrack = cs->track.itemID;
    else if (dynamic_cast<Track*> (clipOwner) != nullptr)
        nextInsertPointTrack = nextInsertPointClipOwner;

    return true;
}

void EditInsertPoint::setNextInsertPoint (TimePosition time)
{
    if (lockInsertPointCount == 0)
    {
        nextInsertPointTime = std::max (0_tp, time);
        nextInsertIsAfterSelected = false;
    }
}

void EditInsertPoint::setNextInsertPoint (TimePosition time, const Track::Ptr& track)
{
    if (lockInsertPointCount == 0)
    {
        setNextInsertPoint (time);
        nextInsertPointClipOwner = {};
        nextInsertPointTrack = track != nullptr ? track->itemID : EditItemID();
    }
}

void EditInsertPoint::lockInsertPoint (bool lock) noexcept
{
    if (lock)
        ++lockInsertPointCount;
    else
        --lockInsertPointCount;
}

void EditInsertPoint::setNextInsertPointAfterSelected()
{
    nextInsertIsAfterSelected = true;
}

void EditInsertPoint::chooseInsertPoint (juce::ReferenceCountedObjectPtr<Track>& track,
                                         TimePosition& start, bool pasteAfterSelection, SelectionManager* sm)
{
    return chooseInsertPoint (track, start, pasteAfterSelection, sm,
                              [] (auto& t) { return t.isAudioTrack() || t.isFolderTrack(); });
}

void EditInsertPoint::chooseInsertPoint (Track::Ptr& track, TimePosition& start, bool pasteAfterSelection, SelectionManager* sm,
                                         std::function<bool (Track&)> allowedTrackPredicate)
{
    CRASH_TRACER
    jassert (allowedTrackPredicate);
    track = findTrackForID (edit, nextInsertPointTrack);
    start = nextInsertPointTime;

    if (nextInsertIsAfterSelected || pasteAfterSelection)
    {
        if (sm != nullptr)
        {
            if (auto firstSelectedTrack = findAllTracksContainingSelectedItems (sm->getSelectedObjects()).getFirst())
                track = firstSelectedTrack;

            auto selectedClipTimeRange = getTimeRangeForSelectedItems (sm->getSelectedObjects());

            if (! selectedClipTimeRange.isEmpty())
            {
                start = selectedClipTimeRange.getEnd();

                if (edit.getTransport().snapToTimecode)
                    start = edit.getTransport().getSnapType().roundTimeNearest (start, edit.tempoSequence);
            }
        }
    }

    while (track != nullptr && ! allowedTrackPredicate (*track))
    {
        if (auto next = track->getSiblingTrack (1, false))
            track = next;
        else
            break;
    }

    if (track == nullptr)
    {
        if (sm != nullptr)
            track = findFirstClipTrackFromSelection (sm->getSelectedObjects());

        if (track == nullptr)
        {
            edit.ensureNumberOfAudioTracks (1);
            track = getAudioTracks (edit).getFirst();
        }
    }

    jassert (track != nullptr);
}

EditInsertPoint::Placement EditInsertPoint::chooseInsertPoint (bool pasteAfterSelection, SelectionManager* sm,
                                                               std::function<bool (Track&)> allowedTrackPredicate)
{
    CRASH_TRACER
    jassert (allowedTrackPredicate);
    auto track = findTrackForID (edit, nextInsertPointTrack);
    auto clipOwner = nextInsertPointClipOwner.isValid() ? findClipOwnerForID (edit, nextInsertPointClipOwner)
                                                        : track != nullptr ? dynamic_cast<ClipOwner*> (track)
                                                                           : nullptr;
    auto time = nextInsertPointTime;

    // Find the next track after the selection if required
    if (nextInsertIsAfterSelected || pasteAfterSelection)
    {
        if (sm != nullptr)
        {
            // Get the selected track first
            if (auto firstSelectedTrack = findAllTracksContainingSelectedItems (sm->getSelectedObjects()).getFirst())
                track = firstSelectedTrack;

            // Then see if it's actually a Slot that's selected
            if (auto clipSlot = sm->getFirstItemOfType<ClipSlot>())
            {
                clipOwner = clipSlot;
                track = &clipSlot->track;
            }
            else if (auto clip = sm->getFirstItemOfType<Clip>())
            {
                // Or a Clip in a ClipSlot
                if (auto clipClipSlot = clip->getClipSlot())
                {
                    clipOwner = clipClipSlot;
                    track = &clipClipSlot->track;
                }
            }

            if (auto selectedClipTimeRange = getTimeRangeForSelectedItems (sm->getSelectedObjects());
                ! selectedClipTimeRange.isEmpty())
            {
                time = selectedClipTimeRange.getEnd();

                if (edit.getTransport().snapToTimecode)
                    time = edit.getTransport().getSnapType().roundTimeNearest (time, edit.tempoSequence);
            }
        }
    }

    // Check the Track is allowed by the predicate and if not, find the next allowed track
    while (track != nullptr && ! allowedTrackPredicate (*track))
    {
        if (auto next = track->getSiblingTrack (1, false))
            track = next;
        else
            break;
    }

    // If the Track is still nullptr, find it from the selection, otherwise, create a new one
    if (track == nullptr)
    {
        if (sm != nullptr)
            track = findFirstClipTrackFromSelection (sm->getSelectedObjects());

        if (track == nullptr)
        {
            edit.ensureNumberOfAudioTracks (1);
            track = getAudioTracks (edit).getFirst();
            clipOwner = dynamic_cast<ClipOwner*> (track);
        }
    }

    jassert (track != nullptr);
    return { track, clipOwner, { time } };
}

}} // namespace tracktion { inline namespace engine
