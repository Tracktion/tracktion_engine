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
