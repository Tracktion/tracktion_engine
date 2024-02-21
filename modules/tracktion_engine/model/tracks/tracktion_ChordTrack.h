/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    You may use this code under the terms of the GPL v3 - see LICENCE.md for details.
    For the technical preview this file cannot be licensed commercially.
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
