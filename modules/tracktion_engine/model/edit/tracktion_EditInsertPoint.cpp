/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


EditInsertPoint::EditInsertPoint (Edit& e) : edit (e)
{
}

void EditInsertPoint::setNextInsertPoint (double time)
{
    if (lockInsertPointCount == 0)
    {
        nextInsertPointTime = jmax (0.0, time);
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

void EditInsertPoint::chooseInsertPoint (Track::Ptr& track, double& start, bool pasteAfterSelection, SelectionManager* sm)
{
    CRASH_TRACER
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

    while (track != nullptr && ! (track->isAudioTrack() || track->isFolderTrack()))
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
            track = edit.getOrInsertAudioTrackAt (0);
    }

    jassert (track != nullptr);
}
