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

ChordClip::ChordClip (const juce::ValueTree& v, EditItemID id, ClipTrack& targetTrack)
    : Clip (v, targetTrack, id, Type::chord)
{
    if (clipName.get().isEmpty())
        clipName = TRANS("Chord");

    auto pgen = state.getChildWithName (IDs::PATTERNGENERATOR);

    if (pgen.isValid())
    {
        patternGenerator.reset (new PatternGenerator (*this, pgen));

        if (patternGenerator->getChordProgression().isEmpty())
            patternGenerator->setChordProgressionFromChordNames ({ "i" });
    }

    state.addListener (this);
}

ChordClip::~ChordClip()
{
    notifyListenersOfDeletion();

    state.removeListener (this);
}

void ChordClip::initialise()
{
    Clip::initialise();

    speedRatio = 1.0; // not used
}

String ChordClip::getSelectableDescription()
{
    return TRANS("Chord Clip");
}

Colour ChordClip::getDefaultColour() const
{
    return Colours::red.withHue (1.0f / 9.0f);
}

void ChordClip::setTrack (ClipTrack* ct)
{
    Clip::setTrack (ct);
}

//==============================================================================
bool ChordClip::canGoOnTrack (Track& t)
{
    return t.isChordTrack();
}

void ChordClip::valueTreeChildAdded (ValueTree&, juce::ValueTree& c)
{
    if (c.hasType (IDs::PATTERNGENERATOR))
    {
        patternGenerator.reset (new PatternGenerator (*this, c));

        if (patternGenerator->getChordProgression().isEmpty())
            patternGenerator->setChordProgressionFromChordNames ({ "i" });
    }

    changed();
    triggerAsyncUpdate();
}

void ChordClip::valueTreeChildRemoved (ValueTree& p, juce::ValueTree& c, int)
{
    if (p == state && c.hasType (IDs::PATTERNGENERATOR))
    {
        patternGenerator = nullptr;
    }
    changed();
    triggerAsyncUpdate();
}

void ChordClip::valueTreePropertyChanged (ValueTree& v, const juce::Identifier& i)
{
    changed();
    triggerAsyncUpdate();

    Clip::valueTreePropertyChanged (v, i);
}

void ChordClip::valueTreeParentChanged (juce::ValueTree& v)
{
    if (! v.getParent().isValid())
        edit.sendTempoOrPitchSequenceChangedUpdates();

    Clip::valueTreeParentChanged (v);
}

PatternGenerator* ChordClip::getPatternGenerator()
{
    if (! state.getChildWithName (IDs::PATTERNGENERATOR).isValid())
        state.addChild (ValueTree (IDs::PATTERNGENERATOR), -1, &edit.getUndoManager());

    jassert (patternGenerator != nullptr);
    return patternGenerator.get();
}

void ChordClip::pitchTempoTrackChanged()
{
    if (patternGenerator != nullptr)
        patternGenerator->refreshPatternIfNeeded();

    changed();
}

Colour ChordClip::getColour() const
{
    return Clip::getColour();
}

void ChordClip::handleAsyncUpdate()
{
    // notify midi clips of chord changes
    for (auto t : getAudioTracks (edit))
    {
        for (int i = t->getNumTrackItems(); --i >= 0;)
            if (auto* mc = dynamic_cast<MidiClip*> (t->getTrackItem (i)))
                mc->pitchTempoTrackChanged();
    }
}

}
