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
class MarkerClip   : public Clip
{
public:
    MarkerClip (const juce::ValueTree&, EditItemID, ClipTrack& targetTrack);
    ~MarkerClip() override;

    int getMarkerID()                                               { return markerID; }
    void setMarkerID (int newID);

    bool isSyncAbsolute() const noexcept                            { return syncType == syncAbsolute; }
    bool isSyncBarsBeats() const noexcept                           { return syncType == syncBarsBeats; }

    juce::String getSelectableDescription() override;
    bool canGoOnTrack (Track&) override;
    juce::Colour getColour() const override;
    juce::Colour getDefaultColour() const override;
    void initialise() override;

    bool isMidi() const override                                    { return false; }
    bool isMuted() const override                                   { return false; }

protected:
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;

private:
    juce::CachedValue<int> markerID;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MarkerClip)
};

}} // namespace tracktion { inline namespace engine
