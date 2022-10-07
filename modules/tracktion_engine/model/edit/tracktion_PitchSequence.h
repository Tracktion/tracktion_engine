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

//==============================================================================
/**
*/
class PitchSequence
{
public:
    PitchSequence();
    ~PitchSequence();

    void initialise (Edit&, const juce::ValueTree&);
    void freeResources();

    Edit& getEdit() const;

    void copyFrom (const PitchSequence&);
    void clear();

    const juce::Array<PitchSetting*>& getPitches() const;
    int getNumPitches() const;
    PitchSetting* getPitch (int index) const;
    PitchSetting& getPitchAt (TimePosition) const;
    PitchSetting& getPitchAtBeat (BeatPosition) const;
    int indexOfPitch (const PitchSetting*) const;

    PitchSetting::Ptr insertPitch (TimePosition);
    PitchSetting::Ptr insertPitch (BeatPosition, int pitch);

    void movePitchStart (PitchSetting&, BeatDuration deltaBeats, bool snapToBeat);

    /** Inserts space in to a sequence, shifting all PitchSettings. */
    void insertSpaceIntoSequence (TimePosition, TimeDuration amountOfSpace, bool snapToBeat);

    void sortEvents();

    juce::ValueTree state;

private:
    struct PitchList;
    std::unique_ptr<PitchList> list;
    Edit* edit = nullptr;

    int indexOfPitchAt (TimePosition) const;
    int indexOfNextPitchAt (TimePosition) const;

    juce::UndoManager* getUndoManager() const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PitchSequence)
};

}} // namespace tracktion { inline namespace engine
