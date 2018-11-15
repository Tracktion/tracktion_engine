/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
{

/** */
class ChordTrack  : public ClipTrack
{
public:
    ChordTrack (Edit&, const juce::ValueTree&);
    ~ChordTrack();

    bool isChordTrack() const override                  { return true; }
    juce::String getSelectableDescription() override    { return TRANS("Chord Track"); }
    bool canContainPlugin (Plugin*) const override      { return false; }

    juce::String getTrackWarning() const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChordTrack)
};

} // namespace tracktion_engine
