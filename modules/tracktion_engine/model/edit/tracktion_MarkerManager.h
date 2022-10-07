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
class MarkerManager   : public juce::ChangeBroadcaster,
                        private juce::ValueTree::Listener
{
public:
    MarkerManager (Edit&, const juce::ValueTree&);
    ~MarkerManager() override;

    int getNextUniqueID (int start = 1);
    void checkForDuplicates (MarkerClip&, bool changeOthers);

    /** Creates a MarkerClip with using getNewMarkerMode(). */
    MarkerClip::Ptr createMarker (int number, TimePosition, TimeDuration, SelectionManager*);
    MarkerClip::Ptr createMarker (int number, TimePosition, TimeDuration, Clip::SyncType, SelectionManager*);

    Clip::SyncType getNewMarkerMode() const;

    MarkerClip* getMarkerByID (int);
    MarkerClip* getNextMarker (TimePosition);
    MarkerClip* getPrevMarker (TimePosition);

    juce::ReferenceCountedArray<MarkerClip> getMarkers() const;

private:
    Edit& edit;
    juce::ValueTree state;

    void valueTreeChanged (const juce::ValueTree&);

    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;
    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override;
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override;
    void valueTreeChildOrderChanged (juce::ValueTree&, int, int) override {}
    void valueTreeParentChanged (juce::ValueTree&) override;
    void valueTreeRedirected (juce::ValueTree&) override {}
};

}} // namespace tracktion { inline namespace engine
