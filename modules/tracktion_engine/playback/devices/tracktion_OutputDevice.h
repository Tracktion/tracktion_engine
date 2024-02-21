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

/** Base class for audio or midi output devices, to which a track's output can be sent.
    The DeviceManager keeps a list of these objects.
*/
class OutputDevice   : public Selectable
{
public:
    OutputDevice (Engine&, const juce::String& type, const juce::String& name);
    ~OutputDevice() override;

    //==============================================================================
    virtual juce::String getName() const;

    /** the alias is the name shown in the draggable input device components */
    juce::String getAlias() const;
    void setAlias (const juce::String& alias);

    /** Called after all devices are constructed, so it can use all the device
        names in its calculations.
    */
    void initialiseDefaultAlias();

    juce::String getDeviceID() const;
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
    juce::String type, name, alias, defaultAlias;

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
