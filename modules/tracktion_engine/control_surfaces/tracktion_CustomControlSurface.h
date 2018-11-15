/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
{

/** */
class CustomControlSurface  : public ControlSurface,
                              public juce::ChangeBroadcaster,
                              private juce::AsyncUpdater
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

        toggleBeatsSecondsModeId    = 50,
        toggleLoopId                = 51,
        togglePunchId               = 52,
        toggleClickId               = 53,
        toggleSnapId                = 54,
        toggleSlaveId               = 55,
        toggleEtoEId                = 56,
        toggleScrollId              = 57,

        masterVolumeId              = 8,
        masterPanId                 = 9,
        quickParamId                = 24,
        paramTrackId                = 1600,

        volTrackId                  = 1800,
        panTrackId                  = 1700,
        muteTrackId                 = 1100,
        soloTrackId                 = 1200,
        armTrackId                  = 1300,
        selectTrackId               = 1400,
        auxTrackId                  = 1500,

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

        none                        = 9999
    };

    struct MappingSet
    {
        MappingSet() {}
        MappingSet (const MappingSet&) = default;
        MappingSet& operator= (const MappingSet&) = default;

        ActionID id = none;
        int controllerID = -1, note = -1, channel = -1;
        juce::Colour colour { juce::Colours::transparentWhite };
        juce::String surfaceName;
    };

    //==============================================================================
    CustomControlSurface (ExternalControllerManager&, const juce::String& name);
    CustomControlSurface (ExternalControllerManager&, const juce::XmlElement&);
    ~CustomControlSurface();

    //==============================================================================
    static juce::String getNameForActionID (ExternalControllerManager&, ActionID);
    static juce::Array<ControlSurface*> getCustomSurfaces (ExternalControllerManager&);
    static juce::Array<MappingSet> getMappingSetsForID (ExternalControllerManager&, ActionID);
    static int getControllerNumberFromId (int) noexcept;

    //==============================================================================
    void initialiseDevice (bool connect) override;
    void shutDownDevice() override;

    /** Saves the settings of all open custom surfaces. */
    static void saveAllSettings();

    void updateMiscFeatures() override;
    void acceptMidiMessage (const juce::MidiMessage&) override;
    bool wantsMessage(const juce::MidiMessage&) override;
    bool eatsAllMessages() override;
    bool canSetEatsAllMessages() override;
    void setEatsAllMessages(bool eatAll) override;
    void moveFader (int channelNum, float newSliderPos) override;
    void moveMasterLevelFader (float newLeftSliderPos, float newRightSliderPos) override;
    void movePanPot (int channelNum, float newPan) override;
    void moveAux (int channel, const char* bus, float newPos) override;
    void updateSoloAndMute (int channelNum, Track::MuteAndSoloLightState, bool isBright) override;
    void soloCountChanged (bool) override;
    void playStateChanged (bool isPlaying) override;
    void recordStateChanged (bool isRecording) override;
    void automationReadModeChanged (bool isReading) override;
    void automationWriteModeChanged (bool isWriting) override;
    void faderBankChanged (int newStartChannelNumber, const juce::StringArray& trackNames) override;
    void channelLevelChanged (int channel, float level) override;
    void trackSelectionChanged (int channel, bool isSelected) override;
    void trackRecordEnabled (int channel, bool isEnabled) override;
    void masterLevelsChanged (float leftLevel, float rightLevel) override;
    void timecodeChanged (int barsOrHours, int beatsOrMinutes, int ticksOrSeconds, int millisecs, bool isBarsBeats, bool isFrames) override;
    void clickOnOffChanged (bool isClickOn) override;
    void snapOnOffChanged (bool isSnapOn) override;
    void loopOnOffChanged (bool isLoopOn) override;
    void slaveOnOffChanged (bool isSlaving) override;
    void punchOnOffChanged (bool isPunching) override;
    void parameterChanged (int parameterNumber, const ParameterSetting& newValue) override;
    void clearParameter (int parameterNumber) override;
    bool canChangeSelectedPlugin() override;
    void currentSelectionChanged() override;
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
        int id = 0;
        int note = -1;
        int channel = 0;
        int function = 0;
    };

    int getNumMappings() const;
    void listenToRow (int);
    int getRowBeingListenedTo() const;
    void showMappingsListForRow (int);
    void setLearntParam(bool keepListening);
    void removeMapping (int index);
    Mapping* getMappingForRow (int) const;
    std::pair<juce::String, juce::String> getTextForRow (int) const;

    //==============================================================================
    // transport
    void play (float val, int param);
    void stop (float val, int param);
    void record (float val, int param);
    void home (float val, int param);
    void end (float val, int param);
    void rewind (float val, int param);
    void fastForward (float val, int param);
    void markIn (float val, int param);
    void markOut (float val, int param);
    void automationReading (float val, int param);
    void automationWriting (float val, int param);
    void addMarker (float val, int param);
    void prevMarker (float val, int param);
    void nextMarker (float val, int param);
    void nudgeLeft (float val, int param);
    void nudgeRight (float val, int param);
    void abort (float val, int param);
    void abortRestart (float val, int param);
    void jog (float val, int param);
    void jumpToMarkIn (float val, int param);
    void jumpToMarkOut (float val, int param);

    // options
    void toggleBeatsSecondsMode (float val, int param);
    void toggleLoop (float val, int param);
    void togglePunch (float val, int param);
    void toggleClick (float val, int param);
    void toggleSnap (float val, int param);
    void toggleSlave (float val, int param);
    void toggleEtoE (float val, int param);
    void toggleScroll (float val, int param);

    // plugins
    void masterVolume (float val, int param);
    void masterPan (float val, int param);
    void quickParam (float val, int param);
    void paramTrack (float val, int param);

    // track
    void volTrack (float val, int param);
    void panTrack (float val, int param);
    void muteTrack (float val, int param);
    void soloTrack (float val, int param);
    void armTrack (float val, int param);
    void selectTrack (float val, int param);
    void auxTrack (float val, int param);

    // navigation
    void zoomIn (float val, int param);
    void zoomOut (float val, int param);
    void scrollTracksUp (float val, int param);
    void scrollTracksDown (float val, int param);
    void scrollTracksLeft (float val, int param);
    void scrollTracksRight (float val, int param);
    void zoomTracksIn (float val, int param);
    void zoomTracksOut (float val, int param);
    void toggleSelectionMode (float val, int param);
    void selectLeft (float val, int param);
    void selectRight (float val, int param);
    void selectUp (float val, int param);
    void selectDown (float val, int param);
    void selectClipInTrack (float val, int param);
    void selectFilterInTrack (float val, int param);

    // bank
    void faderBankLeft    (float val, int param);
    void faderBankLeft1   (float val, int param);
    void faderBankLeft4   (float val, int param);
    void faderBankLeft8   (float val, int param);
    void faderBankLeft16  (float val, int param);
    void faderBankRight   (float val, int param);
    void faderBankRight1  (float val, int param);
    void faderBankRight4  (float val, int param);
    void faderBankRight8  (float val, int param);
    void faderBankRight16 (float val, int param);

    void loadFunctions();

    juce::XmlElement* createXml();
    void importSettings (const juce::File&);
    void exportSettings (const juce::File&);

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
    juce::OwnedArray<Mapping> mappings;
    std::map<int, juce::SortedSet<int>*> commandGroups;
    int nextCmdGroupIndex = 50000;

    int lastControllerNote = -1;
    int lastControllerID = 0;
    float lastControllerValue = 0;
    int lastControllerChannel = 0;
    int listeningOnRow = -1;

    juce::OwnedArray<ActionFunctionInfo> actionFunctionList;
    juce::PopupMenu contextMenu;
    bool pluginMoveMode = true;
    bool eatsAllMidi = false;

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
        void saveAllSettings();

        juce::Array<CustomControlSurface*> surfaces;
    };

    juce::SharedResourcePointer<CustomControlSurfaceManager> manager;

    //==============================================================================
    void addFunction (juce::PopupMenu&, juce::SortedSet<int>& commandSet, const juce::String& group, const juce::String& name, int id, ActionFunction);
    void addTrackFunction (juce::PopupMenu&, const juce::String& group, const juce::String& name, int id, ActionFunction);
    void addPluginFunction (juce::PopupMenu&, const juce::String& group, const juce::String& name, int id, ActionFunction);
    void createContextMenu (juce::PopupMenu&);
    void addAllCommandItem (juce::PopupMenu&);

    juce::String controllerIDToString (int id, int channelid) const;
    juce::String noteIDToString (int note, int channelid) const;
    juce::String getFunctionName (int id) const;

    void init();
    bool loadFromXml (const juce::XmlElement&);
    void clearCommandGroups();

    bool isPendingEventAssignable();
    void updateOrCreateMappingForID (int id, int channel, int noteNum, int controllerID);
    void addMappingSetsForID (ActionID, juce::Array<MappingSet>& mappingSet);

    void handleAsyncUpdate() override;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CustomControlSurface)
};

} // namespace tracktion_engine
