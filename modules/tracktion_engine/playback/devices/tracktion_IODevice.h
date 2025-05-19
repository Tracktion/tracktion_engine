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

/**
    A base class for input/output devices.

    See InputDevice and OutputDevice for more details.
*/
class IODevice   : public Selectable
{
public:
    //==============================================================================
    IODevice (Engine&, juce::String name, juce::String deviceID);
    ~IODevice() override;

    //==============================================================================
    const juce::String& getName() const         { return name; }
    juce::String getDeviceID() const            { return deviceID; }

    bool isEnabled() const;
    virtual void setEnabled (bool) = 0;

    virtual bool isMidi() const = 0;
    virtual bool isTrackDevice() const = 0;
    virtual juce::String getDeviceTypeDescription() const = 0;

    /// the alias is the name shown in the draggable input device components
    juce::String getAlias() const;
    void setAlias (const juce::String& newAlias);

    juce::String getSelectableDescription() override;

    //==============================================================================
    Engine& engine;

protected:
    std::atomic<bool> enabled { false };

    const juce::String deviceID, name;

private:
    juce::String alias;
    juce::String getAliasPropName() const;

    const juce::String& getType() const; // Deprecated: use getDeviceTypeDescription() instead
};


}} // namespace tracktion { inline namespace engine
