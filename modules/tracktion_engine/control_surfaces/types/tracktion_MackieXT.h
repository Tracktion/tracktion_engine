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
