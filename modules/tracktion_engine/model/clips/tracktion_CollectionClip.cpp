/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


CollectionClip::CollectionClip (Track& t)
   : TrackItem (t.edit, {}, Type::collection),
     track (&t)
{
}

CollectionClip::~CollectionClip()
{
    notifyListenersOfDeletion();
}

void CollectionClip::addClip (Clip* clip)
{
    if (! dragging)
    {
        jassert (clip != nullptr);
        jassert (! clips.contains (clip));
        jassert (! isGroupCollection() || (clip->getTrack() == nullptr || clip->getTrack() == getTrack()));

        if (clip != nullptr)
            clips.addIfNotAlreadyThere (clip);
    }
}

void CollectionClip::removeClip (Clip* clip)
{
    if (! dragging)
        while (clips.contains (clip))
            clips.removeObject (clip);
}

bool CollectionClip::removeClip (EditItemID clipID)
{
    if (! dragging)
    {
        for (auto& c : clips)
        {
            if (c->itemID == clipID)
            {
                clips.removeObject (c);
                return true;
            }
        }
    }

    return false;
}

bool CollectionClip::moveToTrack (Track& newTrack)
{
    auto to   = dynamic_cast<ClipTrack*> (&newTrack);
    auto from = dynamic_cast<ClipTrack*> (track);

    if (to == nullptr || dynamic_cast<AudioTrack*> (to) == nullptr)
        return false;

    if (track != to && from != nullptr)
    {
        if (! to->isFrozen (Track::anyFreeze))
        {
            CollectionClip::Ptr refHolder (this);

            from->removeCollectionClip (this);
            track = to;
            to->addCollectionClip (this);
        }
    }

    return true;
}

FolderTrack* CollectionClip::getFolderTrack() const noexcept
{
    return dynamic_cast<FolderTrack*> (track);
}

void CollectionClip::updateStartAndEnd()
{
    if (! clips.isEmpty())
        range = findUnionOfEditTimeRanges (clips);
}

String CollectionClip::getName()
{
    return TRANS("Collection Clip");
}
