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

class RemoteSLCompact  : public NovationRemoteSl
{
public:
    RemoteSLCompact (ExternalControllerManager&);
    ~RemoteSLCompact();

    bool wantsMessage (int, const juce::MidiMessage&) override;
    bool eatsAllMessages() override;
    bool canSetEatsAllMessages() override;
    bool canChangeSelectedPlugin() override;
    bool showingPluginParams() override;
    bool showingMarkers() override;
    bool showingTracks() override;
    bool isPluginSelected (Plugin*) override;
};

}} // namespace tracktion { inline namespace engine
