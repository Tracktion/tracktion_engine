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

/** */
class CustomControlSurface  : public ControlSurface,
                              public juce::ChangeBroadcaster,
                              private juce::AsyncUpdater,
                              private juce::OSCReceiver::Listener<juce::OSCReceiver::MessageLoopCallback>,
                              private juce::Timer
{
public:
    //==============================================================================
    /** enum used to determine the action type. Used mainly when queueing events for MIDI learn. */
    enum ActionID
    {
        playId                      = 1,
        stopId                      = 2,
        recordId                    = 3,
        homeId                      = 4,
        endId                       = 5,
        rewindId                    = 6,
        fastForwardId               = 7,
        markInId                    = 10,
        markOutId                   = 11,
        automationReadId            = 12,
        automationRecordId          = 23,
        addMarkerId                 = 17,
        nextMarkerId                = 13,
        previousMarkerId            = 14,
        nudgeLeftId                 = 15,
        nudgeRightId                = 16,
        abortId                     = 18,
        abortRestartId              = 19,
        jogId                       = 20,
        jumpToMarkInId              = 21,
        jumpToMarkOutId             = 22,
        timecodeId                  = 25,

        marker1Id                   = 400,
        marker2Id                   = 401,
        marker3Id                   = 402,
        marker4Id                   = 403,
        marker5Id                   = 404,
        marker6Id                   = 405,
        marker7Id                   = 406,
        marker8Id                   = 407,

        toggleBeatsSecondsModeId    = 50,
        toggleLoopId                = 51,
        togglePunchId               = 52,
        toggleClickId               = 53,
        toggleSnapId                = 54,
        toggleSlaveId               = 55,
        toggleEtoEId                = 56,
        toggleScrollId              = 57,
        toggleAllArmId              = 58,

        masterVolumeId              = 8,
        masterVolumeTextId          = 26,
        masterPanId                 = 9,
        quickParamId                = 24,
        paramTrackId                = 1600,
        paramNameTrackId            = 2500,
        paramTextTrackId            = 2600,
        clearAllSoloId              = 27,

        nameTrackId                 = 2100,
        numberTrackId               = 2700,
        volTrackId                  = 1800,
        volTextTrackId              = 2200,
        panTrackId                  = 1700,
        panTextTrackId              = 2400,
        muteTrackId                 = 1100,
        soloTrackId                 = 1200,
        armTrackId                  = 1300,
        selectTrackId               = 1400,
        auxTrackId                  = 1500,
        auxTextTrackId              = 2300,

        clip1TrackId                = 3100,
        clip2TrackId                = 3200,
        clip3TrackId                = 3300,
        clip4TrackId                = 3400,
        clip5TrackId                = 3500,
        clip6TrackId                = 3600,
        clip7TrackId                = 3700,
        clip8TrackId                = 3800,
        stopClipsTrackId            = 3900,
        sceneId                     = 4000,
        clipBankUp1Id               = 320,
        clipBankUp4Id               = 321,
        clipBankUp8Id               = 322,
        clipBankDown1Id             = 323,
        clipBankDown4Id             = 324,
        clipBankDown8Id             = 325,

        zoomInId                    = 100,
        zoomOutId                   = 101,
        scrollTracksUpId            = 102,
        scrollTracksDownId          = 103,
        scrollTracksLeftId          = 104,
        scrollTracksRightId         = 105,
        zoomTracksInId              = 106,
        zoomTracksOutId             = 107,
        toggleSelectionModeId       = 108,
        selectLeftId                = 109,
        selectRightId               = 110,
        selectUpId                  = 111,
        selectDownId                = 112,
        selectClipInTrackId         = 1900,
        selectPluginInTrackId       = 2000,
        selectedPluginNameId        = 113,

        faderBankLeftId             = 208,
        faderBankLeft1Id            = 200,
        faderBankLeft4Id            = 201,
        faderBankLeft8Id            = 202,
        faderBankLeft16Id           = 206,
        faderBankRightId            = 209,
        faderBankRight1Id           = 203,
        faderBankRight4Id           = 204,
        faderBankRight8Id           = 205,
        faderBankRight16Id          = 207,

        paramBankLeftId             = 220,
        paramBankLeft1Id            = 221,
        paramBankLeft4Id            = 222,
        paramBankLeft8Id            = 223,
        paramBankLeft16Id           = 224,
        paramBankLeft24Id           = 225,
        paramBankRightId            = 226,
        paramBankRight1Id           = 227,
        paramBankRight4Id           = 228,
        paramBankRight8Id           = 229,
        paramBankRight16Id          = 230,
        paramBankRight24Id          = 231,

        userAction1Id               = 300,
        userAction2Id               = 301,
        userAction3Id               = 302,
        userAction4Id               = 303,
        userAction5Id               = 304,
        userAction6Id               = 305,
        userAction7Id               = 306,
        userAction8Id               = 307,
        userAction9Id               = 308,
        userAction10Id              = 309,
        userAction11Id              = 310,
        userAction12Id              = 311,
        userAction13Id              = 312,
        userAction14Id              = 313,
        userAction15Id              = 314,
        userAction16Id              = 315,
        userAction17Id              = 316,
        userAction18Id              = 317,
        userAction19Id              = 318,
        userAction20Id              = 319,

        emptyTextId                 = 9998,
        none                        = 9999
    };

    struct MappingSet
    {
        MappingSet() {}
        MappingSet (const MappingSet&) = default;
        MappingSet& operator= (const MappingSet&) = default;

        ActionID id = none;
        juce::String addr;
        int controllerID = -1, note = -1, channel = -1;
        juce::Colour colour { juce::Colours::transparentWhite };
        juce::String surfaceName;
    };

    //==============================================================================
    CustomControlSurface (ExternalControllerManager&, const juce::String& name, ExternalControllerManager::Protocol);
    CustomControlSurface (ExternalControllerManager&, const juce::XmlElement&);
    ~CustomControlSurface() override;

    //==============================================================================
    static juce::String getNameForActionID (ExternalControllerManager&, ActionID);
    static juce::Array<ControlSurface*> getCustomSurfaces (ExternalControllerManager&);
    static juce::Array<MappingSet> getMappingSetsForID (ExternalControllerManager&, ActionID);
    static int getControllerNumberFromId (int) noexcept;

    //==============================================================================
    void initialiseDevice (bool connect) override;
    void shutDownDevice() override;

    void updateOSCSettings (int oscInputPort, int oscOutputPort, juce::String oscOutputAddr) override;

    /** Saves the settings of all open custom surfaces. */
    static void saveAllSettings (Engine& engine);

    void updateMiscFeatures() override;
    void acceptMidiMessage (int, const juce::MidiMessage&) override;
    bool wantsMessage(int, const juce::MidiMessage&) override;
    bool eatsAllMessages() override;
    bool canSetEatsAllMessages() override;
    void setEatsAllMessages(bool eatAll) override;
    void moveFader (int channelNum, float newSliderPos) override;
    void moveMasterLevelFader (float newSliderPos) override;
    void moveMasterPanPot (float newPos) override;
    void movePanPot (int channelNum, float newPan) override;
    void moveAux (int channel, int auxNum, const char* bus, float newPos) override;
    void updateSoloAndMute (int channelNum, Track::MuteAndSoloLightState, bool isBright) override;
    void soloCountChanged (bool) override;
    void playStateChanged (bool isPlaying) override;
    void recordStateChanged (bool isRecording) override;
    void automationReadModeChanged (bool isReading) override;
    void automationWriteModeChanged (bool isWriting) override;
    void faderBankChanged (int newStartChannelNumber, const juce::StringArray& trackNames) override;
    void channelLevelChanged (int channel, float l, float r) override;
    void trackSelectionChanged (int channel, bool isSelected) override;
    void trackRecordEnabled (int channel, bool isEnabled) override;
    void masterLevelsChanged (float leftLevel, float rightLevel) override;
    void timecodeChanged (int barsOrHours, int beatsOrMinutes, int ticksOrSeconds, int millisecs, bool isBarsBeats, bool isFrames) override;
    void clickOnOffChanged (bool isClickOn) override;
    void snapOnOffChanged (bool isSnapOn) override;
    void loopOnOffChanged (bool isLoopOn) override;
    void slaveOnOffChanged (bool isSlaving) override;
    void punchOnOffChanged (bool isPunching) override;
    void scrollOnOffChanged (bool isPunching) override;
    void parameterChanged (int parameterNumber, const ParameterSetting& newValue) override;
    void clearParameter (int parameterNumber) override;
    bool canChangeSelectedPlugin() override;
    void currentSelectionChanged (juce::String) override;
    void deleteController() override;
    void markerChanged (int parameterNumber, const MarkerSetting& newValue) override;
    void clearMarker (int parameterNumber) override;

    //==============================================================================
    bool removeMapping (ActionID, int controllerID, int note, int channel);

    /** This will put the surface in listen and assign mode, launching the given dialog window.
        The call will block whilst assignments are made and return when the window is closed.
    */
    void showMappingsEditor (juce::DialogWindow::LaunchOptions&);

    //==============================================================================
    struct Mapping
    {
        bool isControllerAssigned()
        {
            return id != 0 || addr.isNotEmpty() || note != -1;
        }

        int id = 0;
        juce::String addr;
        int note = -1;
        int channel = 0;
        int function = 0;
    };

    int getNumMappings() const;
    void listenToRow (int);
    int getRowBeingListenedTo() const;
    bool allowsManualEditing() const { return needsOSCSocket; }
    void showMappingsListForRow (int);
    void setLearntParam (bool keepListening);
    void removeMapping (int index);
    Mapping* getMappingForRow (int) const;
    std::pair<juce::String, juce::String> getTextForRow (int) const;

    //==============================================================================
    // transport
    virtual void play (float val, int param);
    virtual void stop (float val, int param);
    virtual void record (float val, int param);
    virtual void home (float val, int param);
    virtual void end (float val, int param);
    virtual void rewind (float val, int param);
    virtual void fastForward (float val, int param);
    virtual void markIn (float val, int param);
    virtual void markOut (float val, int param);
    virtual void automationReading (float val, int param);
    virtual void automationWriting (float val, int param);
    virtual void addMarker (float val, int param);
    virtual void prevMarker (float val, int param);
    virtual void nextMarker (float val, int param);
    virtual void marker1 (float val, int param);
    virtual void marker2 (float val, int param);
    virtual void marker3 (float val, int param);
    virtual void marker4 (float val, int param);
    virtual void marker5 (float val, int param);
    virtual void marker6 (float val, int param);
    virtual void marker7 (float val, int param);
    virtual void marker8 (float val, int param);
    virtual void nudgeLeft (float val, int param);
    virtual void nudgeRight (float val, int param);
    virtual void abort (float val, int param);
    virtual void abortRestart (float val, int param);
    virtual void jog (float val, int param);
    virtual void jumpToMarkIn (float val, int param);
    virtual void jumpToMarkOut (float val, int param);
    virtual void clearAllSolo (float val, int param);

    // options
    virtual void toggleBeatsSecondsMode (float val, int param);
    virtual void toggleLoop (float val, int param);
    virtual void togglePunch (float val, int param);
    virtual void toggleClick (float val, int param);
    virtual void toggleSnap (float val, int param);
    virtual void toggleSlave (float val, int param);
    virtual void toggleEtoE (float val, int param);
    virtual void toggleScroll (float val, int param);
    virtual void toggleAllArm (float val, int param);

    // plugins
    virtual void masterVolume (float val, int param);
    virtual void masterPan (float val, int param);
    virtual void quickParam (float val, int param);
    virtual void paramTrack (float val, int param);

    // track
    virtual void volTrack (float val, int param);
    virtual void panTrack (float val, int param);
    virtual void muteTrack (float val, int param);
    virtual void soloTrack (float val, int param);
    virtual void armTrack (float val, int param);
    virtual void selectTrack (float val, int param);
    virtual void auxTrack (float val, int param);

    // clip launcher
    virtual void launchClip1 (float val, int param);
    virtual void launchClip2 (float val, int param);
    virtual void launchClip3 (float val, int param);
    virtual void launchClip4 (float val, int param);
    virtual void launchClip5 (float val, int param);
    virtual void launchClip6 (float val, int param);
    virtual void launchClip7 (float val, int param);
    virtual void launchClip8 (float val, int param);
    virtual void stopClips (float val, int param);

    virtual void launchScene (float val, int param);
    virtual void clipBankUp1 (float val, int param);
    virtual void clipBankUp4 (float val, int param);
    virtual void clipBankUp8 (float val, int param);
    virtual void clipBankDown1 (float val, int param);
    virtual void clipBankDown4 (float val, int param);
    virtual void clipBankDown8 (float val, int param);

    // navigation
    virtual void zoomIn (float val, int param);
    virtual void zoomOut (float val, int param);
    virtual void scrollTracksUp (float val, int param);
    virtual void scrollTracksDown (float val, int param);
    virtual void scrollTracksLeft (float val, int param);
    virtual void scrollTracksRight (float val, int param);
    virtual void zoomTracksIn (float val, int param);
    virtual void zoomTracksOut (float val, int param);
    virtual void toggleSelectionMode (float val, int param);
    virtual void selectLeft (float val, int param);
    virtual void selectRight (float val, int param);
    virtual void selectUp (float val, int param);
    virtual void selectDown (float val, int param);
    virtual void selectClipInTrack (float val, int param);
    virtual void selectFilterInTrack (float val, int param);

    // bank
    virtual void faderBankLeft    (float val, int param);
    virtual void faderBankLeft1   (float val, int param);
    virtual void faderBankLeft4   (float val, int param);
    virtual void faderBankLeft8   (float val, int param);
    virtual void faderBankLeft16  (float val, int param);
    virtual void faderBankRight   (float val, int param);
    virtual void faderBankRight1  (float val, int param);
    virtual void faderBankRight4  (float val, int param);
    virtual void faderBankRight8  (float val, int param);
    virtual void faderBankRight16 (float val, int param);

    virtual void paramBankLeft    (float val, int param);
    virtual void paramBankLeft1   (float val, int param);
    virtual void paramBankLeft4   (float val, int param);
    virtual void paramBankLeft8   (float val, int param);
    virtual void paramBankLeft16  (float val, int param);
    virtual void paramBankLeft24  (float val, int param);
    virtual void paramBankRight   (float val, int param);
    virtual void paramBankRight1  (float val, int param);
    virtual void paramBankRight4  (float val, int param);
    virtual void paramBankRight8  (float val, int param);
    virtual void paramBankRight16 (float val, int param);
    virtual void paramBankRight24 (float val, int param);

    // User functions
    virtual void userFunction1    (float val, int param);
    virtual void userFunction2    (float val, int param);
    virtual void userFunction3    (float val, int param);
    virtual void userFunction4    (float val, int param);
    virtual void userFunction5    (float val, int param);
    virtual void userFunction6    (float val, int param);
    virtual void userFunction7    (float val, int param);
    virtual void userFunction8    (float val, int param);
    virtual void userFunction9    (float val, int param);
    virtual void userFunction10   (float val, int param);
    virtual void userFunction11   (float val, int param);
    virtual void userFunction12   (float val, int param);
    virtual void userFunction13   (float val, int param);
    virtual void userFunction14   (float val, int param);
    virtual void userFunction15   (float val, int param);
    virtual void userFunction16   (float val, int param);
    virtual void userFunction17   (float val, int param);
    virtual void userFunction18   (float val, int param);
    virtual void userFunction19   (float val, int param);
    virtual void userFunction20   (float val, int param);

    void null (float, int) {} // null action for outgoing only actions

    virtual void loadFunctions();

    virtual juce::XmlElement* createXml();
    virtual void importSettings (const juce::File&);
    virtual void importSettings (const juce::String&);
    virtual void exportSettings (const juce::File&);

    int getPacketsIn()  { return packetsIn; }
    int getPacketsOut() { return packetsOut; }

private:
    //==============================================================================
    typedef void (CustomControlSurface::* ActionFunction) (float val, int param);

    struct ActionFunctionInfo
    {
        juce::String name, group;
        int id = 0;
        int param = 0;
        ActionFunction actionFunc {};
    };

    //==============================================================================
    ExternalControllerManager::Protocol protocol;
    juce::OwnedArray<Mapping> mappings;
    std::map<int, juce::SortedSet<int>*> commandGroups;
    int nextCmdGroupIndex = 50000;

    juce::String lastControllerAddr;
    int lastControllerNote = -1;
    int lastControllerID = 0;
    float lastControllerValue = 0;
    int lastControllerChannel = 0;
    int listeningOnRow = -1;

    juce::OwnedArray<ActionFunctionInfo> actionFunctionList;
    juce::PopupMenu contextMenu;
    bool pluginMoveMode = true;
    bool eatsAllMidi = false;
    bool online = false;
    int oscInputPort = 0, oscOutputPort = 0;
    int packetsIn = 0, packetsOut = 0;
    juce::String oscOutputAddr, oscActiveAddr;
    std::map<juce::String, bool> oscControlTouched;
    std::map<juce::String, int> oscControlTapsWhileTouched;
    std::map<juce::String, float> oscLastValue;
    std::map<juce::String, double> oscLastUsedTime;

    std::unique_ptr<juce::OSCSender> oscSender;
    std::unique_ptr<juce::OSCReceiver> oscReceiver;

    //==============================================================================
    void timerCallback() override;

    //==============================================================================
    struct RPNParser
    {
        void parseControllerMessage (const juce::MidiMessage& m, int& paramID,
                                     int& chan, int& val) noexcept
        {
            jassert (m.isController());

            const int number = m.getControllerNumber();
            const int value = m.getControllerValue();

            lastParamNumber = 0x10000 + number;
            paramID = lastParamNumber;
            chan = m.getChannel();
            val = value;
        }

        int lastParamNumber = 0;
        int lastRPNCoarse = 0;
        int lastRPNFine = 0;
        int lastNRPNCoarse = 0;
        int lastNRPNFine = 0;
        bool wasNRPN = false;
    };

    RPNParser rpnParser;

    //==============================================================================
    struct CustomControlSurfaceManager
    {
        void registerSurface (CustomControlSurface*);
        void unregisterSurface (CustomControlSurface*);
        void saveAllSettings (Engine&);

        juce::Array<CustomControlSurface*> surfaces;
    };

    juce::SharedResourcePointer<CustomControlSurfaceManager> manager;

    //==============================================================================
    virtual void oscMessageReceived (const juce::OSCMessage&) override;
    virtual void oscBundleReceived (const juce::OSCBundle&) override;

    bool isTextAction (ActionID);

    //==============================================================================
    virtual void addFunction (juce::PopupMenu&, juce::SortedSet<int>& commandSet, const juce::String& group, const juce::String& name, ActionID id, ActionFunction);
    virtual void addTrackFunction (juce::PopupMenu&, const juce::String& group, const juce::String& name, ActionID id, ActionFunction);
    virtual void addPluginFunction (juce::PopupMenu&, const juce::String& group, const juce::String& name, ActionID id, ActionFunction);
    virtual void addSceneFunction (juce::PopupMenu&, const juce::String& group, const juce::String& name, ActionID id, ActionFunction);
    virtual void addAllCommandItem (juce::PopupMenu&);

    virtual void sendCommandToControllerForActionID (int actionID, bool);
    virtual void sendCommandToControllerForActionID (int actionID, float value);
    virtual void sendCommandToControllerForActionID (int actionID, juce::String);

    virtual juce::String controllerIDToString (int id, int channelid) const;
    virtual juce::String noteIDToString (int note, int channelid) const;
    virtual juce::String getFunctionName (int id) const;

    virtual void init();
    virtual bool loadFromXml (const juce::XmlElement&);
    virtual void clearCommandGroups();

    virtual bool isPendingEventAssignable();
    virtual void updateOrCreateMappingForID (int id, juce::String addr, int channel, int noteNum, int controllerID);
    virtual void addMappingSetsForID (ActionID, juce::Array<MappingSet>& mappingSet);

    virtual void handleAsyncUpdate() override;
    virtual void recreateOSCSockets();

    virtual bool shouldActOnValue (float v);

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CustomControlSurface)
};

}} // namespace tracktion { inline namespace engine
