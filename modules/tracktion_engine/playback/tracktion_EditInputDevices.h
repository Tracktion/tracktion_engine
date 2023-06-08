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

class EditInputDevices  : private juce::ChangeListener,
                          private juce::AsyncUpdater,
                          private juce::ValueTree::Listener
{
public:
    EditInputDevices (Edit&, const juce::ValueTree& state);
    ~EditInputDevices() override;

    int getMaxNumInputs() const;

    bool isInputDeviceAssigned (const InputDevice&);

    void clearAllInputs (AudioTrack&, juce::UndoManager*);
    void clearInputsOfDevice (AudioTrack&, const InputDevice&, juce::UndoManager*);
    InputDeviceInstance* getInputInstance (const AudioTrack&, int index) const;
    juce::Array<InputDeviceInstance*> getDevicesForTargetTrack (const AudioTrack&) const;

    juce::ValueTree getInstanceStateForInputDevice (const InputDevice&);

private:
    Edit& edit;
    juce::ValueTree state, editState;

    void removeNonExistantInputDeviceStates();
    void addTrackDeviceInstanceToContext (const juce::ValueTree&) const;
    void removeTrackDeviceInstanceToContext (const juce::ValueTree&) const;
    InputDevice* getTrackDeviceForState (const juce::ValueTree&) const;

    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void handleAsyncUpdate() override;

    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override  {}
    void valueTreeChildOrderChanged (juce::ValueTree&, int, int) override               {}
    void valueTreeParentChanged (juce::ValueTree&) override                             {}
    void valueTreeRedirected (juce::ValueTree&) override                                {}
    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override;
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EditInputDevices)
};

}} // namespace tracktion { inline namespace engine
