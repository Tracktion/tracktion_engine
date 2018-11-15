/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
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

    int getNumPitches() const;
    PitchSetting* getPitch (int index) const;
    PitchSetting& getPitchAt (double time) const;
    PitchSetting& getPitchAtBeat (double beat) const;
    int indexOfPitch (const PitchSetting*) const;

    PitchSetting::Ptr insertPitch (double time);
    PitchSetting::Ptr insertPitch (double beatPos, int pitch);

    void movePitchStart (PitchSetting&, double deltaBeats, bool snapToBeat);

    /** Inserts space in to a sequence, shifting all PitchSettings. */
    void insertSpaceIntoSequence (double time, double amountOfSpaceInSeconds, bool snapToBeat);

    void sortEvents();

    juce::ValueTree state;

private:
    struct PitchList;
    std::unique_ptr<PitchList> list;
    Edit* edit = nullptr;

    int indexOfPitchAt (double t) const;
    int indexOfNextPitchAt (double t) const;

    juce::UndoManager* getUndoManager() const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PitchSequence)
};

} // namespace tracktion_engine
