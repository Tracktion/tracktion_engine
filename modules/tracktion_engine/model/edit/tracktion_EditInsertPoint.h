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
class EditInsertPoint
{
public:
    EditInsertPoint (Edit&);

    void setNextInsertPoint (double time, const juce::ReferenceCountedObjectPtr<Track>&);
    void setNextInsertPoint (double time);
    void setNextInsertPointAfterSelected();
    void lockInsertPoint (bool lock) noexcept;

    void chooseInsertPoint (juce::ReferenceCountedObjectPtr<Track>&,
                            double& start, bool pasteAfterSelection, SelectionManager*);

private:
    Edit& edit;
    double nextInsertPointTime = 0;
    EditItemID nextInsertPointTrack;
    int lockInsertPointCount = 0;
    bool nextInsertIsAfterSelected = false;
};

} // namespace tracktion_engine
