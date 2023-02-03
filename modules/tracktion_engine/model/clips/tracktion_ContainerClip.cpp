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

ContainerClip::ContainerClip (const juce::ValueTree& v, EditItemID clipID, ClipOwner& targetParent)
    : AudioClipBase (v, clipID, Type::container, targetParent)
{
}

ContainerClip::~ContainerClip()
{
    notifyListenersOfDeletion();
}

//==============================================================================
void ContainerClip::initialise()
{
    clipListState = state.getOrCreateChildWithName (IDs::CLIPLIST, getUndoManager());
    initialiseClipOwner (edit, clipListState);

    AudioClipBase::initialise();
}

void ContainerClip::cloneFrom (Clip* c)
{
    if (auto other = dynamic_cast<ContainerClip*> (c))
    {
        AudioClipBase::cloneFrom (other);
        auto list = state.getChildWithName (IDs::CLIPLIST);
        copyValueTree (list, other->state.getChildWithName (IDs::CLIPLIST), nullptr);

        Selectable::changed();
    }
}

//==============================================================================
juce::String ContainerClip::getSelectableDescription()
{
    return TRANS("Container Clip") + " - \"" + getName() + "\"";
}

bool ContainerClip::isMidi() const
{
    for (auto c : getClips())
        if (c->isMidi())
            return true;

    return false;
}

TimeDuration ContainerClip::getSourceLength() const
{
    auto l = getLoopRange().getLength();

    for (auto c : getClips())
        l = std::max (l, toDuration (c->getPosition().getEnd()));

    return l;
}

HashCode ContainerClip::getHash() const
{
    size_t hash = 0;

    for (auto c : getClipsOfType<AudioClipBase> (*this))
        hash_combine (hash, c->getHash());

    return static_cast<HashCode> (hash);
}

void ContainerClip::setLoopDefaults()
{
    auto& ts = edit.tempoSequence;
    auto pos = getPosition();

    if (loopInfo.getNumerator() == 0)
        loopInfo.setNumerator (ts.getTimeSigAt (pos.getStart()).numerator);

    if (loopInfo.getDenominator() == 0)
        loopInfo.setDenominator (ts.getTimeSigAt (pos.getStart()).denominator);

    if (loopInfo.getNumBeats() == 0.0)
        loopInfo.setNumBeats (getSourceLength().inSeconds() * (ts.getTempoAt (pos.getStart()).getBpm() / 60.0));
}

void ContainerClip::setLoopRangeBeats (BeatRange newRangeBeats)
{
    const auto newStartBeat  = juce::jmax (0_bp, newRangeBeats.getStart());
    const auto newLengthBeat = juce::jmax (0_bd, newRangeBeats.getLength());

    if (loopStartBeats != newStartBeat || loopLengthBeats != newLengthBeat)
    {
        Clip::setSpeedRatio (1.0);
        setAutoTempo (true);

        loopStartBeats  = newStartBeat;
        loopLengthBeats = newLengthBeat;
    }
}

void ContainerClip::flushStateToValueTree()
{
    for (auto c : getClips())
        c->flushStateToValueTree();

    AudioClipBase::flushStateToValueTree();
}

void ContainerClip::pitchTempoTrackChanged()
{
    for (auto c : getClips())
        c->pitchTempoTrackChanged();

    AudioClipBase::pitchTempoTrackChanged();
}

//==============================================================================
juce::ValueTree& ContainerClip::getClipOwnerState()
{
    return clipListState;
}

Selectable* ContainerClip::getClipOwnerSelectable()
{
    return this;
}

Edit& ContainerClip::getClipOwnerEdit()
{
    return edit;
}

//==============================================================================
bool ContainerClip::isUsingFile (const AudioFile& af)
{
    if (AudioClipBase::isUsingFile (af))
        return true;

    for (auto c : getClipsOfType<AudioClipBase> (*this))
        if (c->isUsingFile (af))
            return true;

    return false;
}

void ContainerClip::clipCreated (Clip&)
{
}

void ContainerClip::clipAddedOrRemoved()
{
}

void ContainerClip::clipOrderChanged()
{
}

void ContainerClip::clipPositionChanged()
{
}


}} // namespace tracktion { inline namespace engine
