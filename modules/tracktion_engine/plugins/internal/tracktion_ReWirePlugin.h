/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


#if TRACKTION_ENABLE_REWIRE

namespace tracktion_engine
{

//==============================================================================
/** */
class ReWireSystem   : private juce::Timer
{
public:
    //==============================================================================
    /** Called at startup to make sure the app becomes the ReWire master */
    static void initialise (Engine&);

    /** Tries to get rid of any open devices, true if this succeeds */
    static bool shutdown();

    static ReWireSystem* getInstanceIfActive();

    // if returnCurrentState is false, this returns whether it will be enabled
    // next time the app runs
    static bool isReWireEnabled (Engine&, bool returnCurrentState = true);
    static void setReWireEnabled (Engine&, bool);

    static const char* getReWireLibraryName();
    static const char* getReWireFolderName();
    static const char* getPropellerheadFolderName();
    static int getRequiredVersionNumMajor();
    static int getRequiredVersionNumMinor();

    class Device;
    Device* openDevice (const juce::String& devName, juce::String& error);
    bool tryToCloseAllOpenDevices();

    juce::StringArray deviceNames;
    juce::OwnedArray<Device> devices;
    juce::String openError;
    bool isOpen = false;

private:
    Engine& engine;

    ReWireSystem (Engine&);
    ~ReWireSystem();

    Device* createDevice (int index, const juce::String& devName, juce::String& error);
    bool closeSystem();
    void timerCallback() override;
};


//==============================================================================
/** */
class ReWirePlugin  : public Plugin,
                      private juce::Timer,
                      private juce::AsyncUpdater
{
public:
    ReWirePlugin (PluginCreationInfo);
    ~ReWirePlugin();

    static const char* getPluginName()      { return NEEDS_TRANS("ReWire Device"); }

    //==============================================================================
    static const char* xmlTypeName;

    void initialiseFully() override;
    juce::String getName() override;
    juce::String getPluginType() override                                         { return xmlTypeName; }

    void initialise (const PlaybackInitialisationInfo&) override;
    void deinitialise() override;

    void getChannelNames (juce::StringArray*, juce::StringArray*) override;
    int getNumOutputChannelsGivenInputs (int) override     { return 2; }

    void prepareForNextBlock (const AudioRenderContext&) override;
    void applyToBuffer (const AudioRenderContext&) override;

    juce::String getSelectableDescription() override    { return TRANS("ReWire Filter"); }

    bool takesMidiInput() override                      { return true; }
    bool takesAudioInput() override                     { return true; }
    bool producesAudioWhenNoAudioInput() override       { return true; }
    bool canBeAddedToClip() override                    { return false; }
    bool needsConstantBufferSize() override             { return true; }

    bool hasNameForMidiNoteNumber (int note, int midiChannel, juce::String& name) override;
    bool hasNameForMidiProgram (int, int, juce::String&) override     { return false; }

    void timerCallback() override;

    juce::StringArray getDeviceChannelNames() const;

    ReWireSystem::Device* device = nullptr;
    juce::String rewireError;
    juce::StringArray buses, channels;

    juce::CachedValue<juce::String> currentDeviceName, currentChannelNameL, currentChannelNameR;
    juce::CachedValue<int> currentBus, currentChannel;

    juce::String openDevice (const juce::String& devName);
    void closeDevice();
    bool updateBusesAndChannels();
    void openExternalUI();
    bool isUIRunning() const noexcept               { return uiIsRunning; }

    void setLeftChannel (const juce::String& channelName);
    void setRightChannel (const juce::String& channelName);

    void setMidiBus (int busNum);
    void setMidiChannel (int channel);

private:
    juce::ScopedPointer<TempoSequencePosition> currentTempoPosition;
    int channelIndexL = 0, channelIndexR = 0;
    bool uiIsRunning = false;

    //==============================================================================
    void valueTreeChanged() override;
    void handleAsyncUpdate() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ReWirePlugin)
};

} // namespace tracktion_engine

#endif //TRACKTION_ENABLE_REWIRE
