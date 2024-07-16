/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion { inline namespace engine
{

/** Base class for audio or midi output devices, to which a track's output can be sent.
    The DeviceManager keeps a list of these objects.
*/
class OutputDevice   : public Selectable
{
public:
    OutputDevice (Engine&, juce::String type, juce::String name, juce::String deviceID);
    ~OutputDevice() override;

    //==============================================================================
    virtual juce::String getName() const;

    /** the alias is the name shown in the draggable input device components */
    juce::String getAlias() const;
    void setAlias (const juce::String& alias);

    juce::String getDeviceID() const                    { return deviceID; }
    juce::String getSelectableDescription() override;

    //==============================================================================
    bool isEnabled() const;
    virtual void setEnabled (bool b) = 0;

    virtual bool isMidi() const                     { return false; }

    Engine& engine;

protected:
    //==============================================================================
    bool enabled = false;

    virtual juce::String openDevice() = 0;
    virtual void closeDevice() = 0;

private:
    const juce::String type, deviceID, name;
    juce::String alias;

    juce::String getAliasPropName() const;
};


//==============================================================================
class OutputDeviceInstance
{
public:
    OutputDeviceInstance (OutputDevice&, EditPlaybackContext&);
    virtual ~OutputDeviceInstance();

    OutputDevice& owner;
    EditPlaybackContext& context;
    Edit& edit;

protected:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OutputDeviceInstance)
};

}} // namespace tracktion { inline namespace engine
