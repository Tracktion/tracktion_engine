/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion_engine
{

EditInsertPoint::EditInsertPoint (Edit& e) : edit (e)
{
}

void EditInsertPoint::setNextInsertPoint (double time)
{
    if (lockInsertPointCount == 0)
    {
        nextInsertPointTime = std::max (0.0, time);
        nextInsertIsAfterSelected = false;
    }
}

void EditInsertPoint::setNextInsertPoint (double time, const Track::Ptr& track)
{
    if (lockInsertPointCount == 0)
    {
        setNextInsertPoint (time);
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
                                         double& start, bool pasteAfterSelection, SelectionManager* sm)
{
    return chooseInsertPoint (track, start, pasteAfterSelection, sm, [] (auto& t) { return t.isAudioTrack() || t.isFolderTrack(); });
}

void EditInsertPoint::chooseInsertPoint (Track::Ptr& track, double& start, bool pasteAfterSelection, SelectionManager* sm,
                                         std::function<bool(Track&)> allowedTrackPredicate)
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

}
