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

class MackieXT  : public ControlSurface
{
public:
    MackieXT (ExternalControllerManager&, MackieMCU&, int);
    ~MackieXT();

    void initialiseDevice (bool connect) override;
    void shutDownDevice() override;
    void acceptMidiMessage (int, const juce::MidiMessage& m) override;

    int getDeviceIndex() const noexcept         { return deviceIdx; }
    void setDeviceIndex (int idx)               { deviceIdx = idx; }

private:
    int deviceIdx = 1;
    MackieMCU& mcu;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MackieXT)
};

}} // namespace tracktion { inline namespace engine
