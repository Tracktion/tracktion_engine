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
class MarkerClip   : public Clip
{
public:
    MarkerClip (const juce::ValueTree&, EditItemID, ClipTrack& targetTrack);
    ~MarkerClip();

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

    AudioNode* createAudioNode (const CreateAudioNodeParams&) override      { return {}; }

protected:
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;

private:
    juce::CachedValue<int> markerID;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MarkerClip)
};

} // namespace tracktion_engine
