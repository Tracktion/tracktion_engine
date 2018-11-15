/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
{

/** Base class for audio or midi output devices, to which a track's output can be sent.
    The DeviceManager keeps a list of these objects.
*/
class OutputDevice   : public Selectable
{
public:
    OutputDevice (Engine&, const juce::String& type, const juce::String& name);
    ~OutputDevice();

    //==============================================================================
    virtual juce::String getName() const;

    /** the alias is the name shown in the draggable input device components */
    juce::String getAlias() const;
    void setAlias (const juce::String& alias);

    /** called after all devices are constructed, so it can use all the device
        names in its calculations..
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

    AudioNode* replaceAudioNode (AudioNode*); // (returns the old one)
    AudioNode* getAudioNode() const noexcept        { return audioNode; }

    OutputDevice& owner;
    EditPlaybackContext& context;
    Edit& edit;

protected:
    juce::ScopedPointer<AudioNode> audioNode;
    juce::CriticalSection audioNodeLock;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OutputDeviceInstance)
};

} // namespace tracktion_engine
