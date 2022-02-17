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
class EditInsertPoint
{
public:
    EditInsertPoint (Edit&);
    virtual ~EditInsertPoint() = default;

    void setNextInsertPoint (TimePosition, const juce::ReferenceCountedObjectPtr<Track>&);
    void setNextInsertPoint (TimePosition);
    void setNextInsertPointAfterSelected();
    void lockInsertPoint (bool lock) noexcept;

    virtual void chooseInsertPoint (juce::ReferenceCountedObjectPtr<Track>&,
                                    TimePosition& start, bool pasteAfterSelection, SelectionManager*);

    void chooseInsertPoint (juce::ReferenceCountedObjectPtr<Track>&,
                            TimePosition& start, bool pasteAfterSelection, SelectionManager*,
                            std::function<bool (Track&)> allowedTrackPredicate);

protected:
    Edit& edit;
    TimePosition nextInsertPointTime;
    EditItemID nextInsertPointTrack;
    int lockInsertPointCount = 0;
    bool nextInsertIsAfterSelected = false;
};

}} // namespace tracktion { inline namespace engine
