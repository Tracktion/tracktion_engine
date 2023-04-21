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

/** */
class ChordTrack  : public ClipTrack
{
public:
    ChordTrack (Edit&, const juce::ValueTree&);
    ~ChordTrack() override;

    bool isChordTrack() const override                  { return true; }
    juce::String getSelectableDescription() override    { return TRANS("Chord Track"); }
    bool canContainPlugin (Plugin*) const override      { return false; }

    juce::String getTrackWarning() const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChordTrack)
};

}} // namespace tracktion { inline namespace engine
