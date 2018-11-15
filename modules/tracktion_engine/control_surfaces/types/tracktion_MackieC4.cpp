/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace
{
    static const float panPotSpeed = 1.0f / 40.0f;

    static const char* functionNames[] =
    {
        "Proj.",
        "Sett.",
        "Edit",
        "Fit/Tracks",
        "Fit/Edit",
        "Big/Meters",
        "Show/Inputs",
        "Show/Filts",

        "Loop",
        "Punch",
        "Click",
        "Snap",
        "E-to-E",
        "Scroll",
        "Chase",
        "TC/Mode",

        "Cut",
        "Copy",
        "Paste",
        "Delete",
        "Mark-In",
        "Mark-Out",
        "Undo",
        "Redo",

        "Rtz",
        "Stop",
        "Play",
        "Record",
        "<<",
        ">>",
        "Auto/Play",
        "Auto/Rec.",

        nullptr
    };

    void performNamedFunction (const String& f)
    {
        if (f == "Proj.")               AppFunctions::showProjectScreen();
        else if (f == "Sett.")          AppFunctions::showSettingsScreen();
        else if (f == "Fit/Tracks")     AppFunctions::zoomToFitVertically();
        else if (f == "Fit/Edit")       AppFunctions::zoomToFitHorizontally();
        else if (f == "Big/Meters")     AppFunctions::showHideBigMeters();
        else if (f == "Show/Inputs")    AppFunctions::showHideInputs();
        else if (f == "Show/Filts")     AppFunctions::showHideOutputs();
        else if (f == "Loop")           AppFunctions::toggleLoop();
        else if (f == "Punch")          AppFunctions::togglePunch();
        else if (f == "Click")          AppFunctions::toggleClick();
        else if (f == "Snap")           AppFunctions::toggleSnap();
        else if (f == "E-to-E")         AppFunctions::toggleEndToEnd();
        else if (f == "Scroll")         AppFunctions::toggleScroll();
        else if (f == "Chase")          AppFunctions::toggleMidiChase();
        else if (f == "TC/Mode")        AppFunctions::toggleTimecode();
        else if (f == "Cut")            AppFunctions::cut();
        else if (f == "Copy")           AppFunctions::copy();
        else if (f == "Paste")          AppFunctions::paste();
        else if (f == "Delete")         AppFunctions::deleteSelected();
        else if (f == "Mark-In")        AppFunctions::markIn();
        else if (f == "Mark-Out")       AppFunctions::markOut();
        else if (f == "Undo")           AppFunctions::undo();
        else if (f == "Redo")           AppFunctions::redo();
        else if (f == "Rtz")            AppFunctions::goToStart();
        else if (f == "Stop")           AppFunctions::stop();
        else if (f == "Play")           AppFunctions::start();
        else if (f == "Record")         AppFunctions::record();
        else if (f == "<<")             AppFunctions::tabBack();
        else if (f == ">>")             AppFunctions::tabForward();
        else if (f == "Auto/Play")      AppFunctions::toggleAutomationReadMode();
        else if (f == "Auto/Rec.")      AppFunctions::toggleAutomationWriteMode();
    }
}

//==============================================================================
MackieC4::MackieC4 (ExternalControllerManager& ecm)
    : ControlSurface (ecm), c4 (new C4Translator())
{
    deviceDescription = "Mackie C4";
    needsMidiBackChannel = true;
    wantsClock = false;
    numberOfFaderChannels = 32;
    numCharactersForTrackNames = 6;
    numParameterControls = 32;
    numCharactersForParameterLabels = 6;
    numMarkers = 0;
    numCharactersForMarkerLabels = 0;
    initialised = false;
    deletable = false;
    numAuxes = 24;
    numCharactersForAuxLabels = 6;
    wantsAuxBanks = true;
    needsMidiChannel = true;

    zeromem (currentText, sizeof (currentText));
    zeromem (newText, sizeof (newText));
}

MackieC4::~MackieC4()
{
    notifyListenersOfDeletion();
}

//==============================================================================
void MackieC4::initialiseDevice (bool)
{
    CRASH_TRACER
    char midi[256];
    int midiLen = 0;

    if (c4->C4UniversalDeviceInquiryTxString (midi, &midiLen))
        sendMidiCommandToController (midi, midiLen);

    if (c4->C4LEDAllOffTxString (midi, &midiLen))
        sendMidiCommandToController (midi, midiLen);

    zeromem (currentText, sizeof (currentText));
    zeromem (newText, sizeof (newText));
    triggerAsyncUpdate();

    mode = PluginMode1;
    isLocked = false;
    wasLocked = false;
    trackViewOffset = 0;

    for (int i = 32; --i >= 0;)
    {
        currentPanPotPos[i] = -1;
        currentPotPos[i] = 0.0f;
        setPotPosition (i, -1.0f);
    }

    metersEnabled = true;
    enableMeters (false);

    updateDisplayAndPots();
}

void MackieC4::shutDownDevice()
{
    char midi[256];
    int midiLen = 0;

    if (c4->C4ResetTxString (midi, &midiLen))
        sendMidiCommandToController (midi, midiLen);

    initialised = false;
}

void MackieC4::updateMiscFeatures()
{
    updateMiscLights();
    updateDisplayAndPots();
}

void MackieC4::updateMiscLights()
{
    lightUpButton (C4SwitchNumberMarker,       mode == PluginMode1 || mode == PluginMode2 || mode == PluginMode3);
    lightUpButton (C4SwitchNumberTrack,        mode == MixerMode);
    lightUpButton (C4SwitchNumberChannelStrip, mode == AuxMode);
    lightUpButton (C4SwitchNumberFunction,     mode == ButtonMode);
    lightUpButton (C4SwitchNumberLock,         isLocked || wasLocked);
    lightUpButton (C4SwitchNumberSpotErase,    mode == PluginMode3 && bypass);
}

void MackieC4::setDisplayText (int potNumber, int row, String text)
{
    text = text.substring (0, 7);
    text = (String::repeatedString (" ", (7 - text.length()) / 2) + text).paddedRight (' ', 7);

    text.copyToUTF8 (newText + 128 * (potNumber / 8) + (potNumber & 7) * 7 + row * 64, 7);
    triggerAsyncUpdate();
}

void MackieC4::setDisplayDirty (int potNumber, int row)
{
    zeromem (currentText + 128 * (potNumber / 8) + (potNumber & 7) * 7 + row * 64, 7);
    triggerAsyncUpdate();
}

void MackieC4::handleAsyncUpdate()
{
    CRASH_TRACER

    if (! initialised)
        return;

    for (int display = 0; display < 4; ++display)
    {
        for (int row = 0; row < 2; ++row)
        {
            char* newOne = newText     + display * 128 + row * 64;
            char* oldOne = currentText + display * 128 + row * 64;

            for (int i = 0; i < 8 * 7; ++i)
                if (newOne[i] == 0)
                    newOne[i] = ' ';

            newOne[8 * 7] = 0;

            if (mode == MixerMode)
            {
                for (int i = 0; i < 3; ++i)
                    newOne[13 + i * 14] = '|';
            }
            else if (mode == PluginMode3)
            {
                if (display == 0 && owner->startParamNumber < 2)
                    newOne[13 - 7 * owner->startParamNumber] = '|';
            }

            int firstDiff = 0;
            int lastDiff = jmax ((int) strlen (newOne), (int) strlen (oldOne));

            while (newOne[firstDiff] == oldOne[firstDiff] && firstDiff < 8 * 7)
                ++firstDiff;

            while (newOne[lastDiff] == oldOne[lastDiff] && lastDiff >= 0)
                --lastDiff;

            if (lastDiff > firstDiff || (lastDiff == firstDiff && firstDiff > 0))
            {
                char midi[256];
                int midiLen = 0;

                memcpy (oldOne, newOne, 8 * 7);
                newOne[lastDiff + 1] = 0;

                if (c4->C4LCDMessageTxString (midi, &midiLen, display, row, firstDiff, newOne + firstDiff))
                    sendMidiCommandToController (midi, midiLen);
            }
        }
    }
}

void MackieC4::enableMeters (bool b)
{
    if (b != metersEnabled)
    {
        metersEnabled = b;

        char midi[256];
        int midiLen = 0;

        for (int i = 0; i < 16; ++i)
        {
            if (c4->C4LCDSetMeterControlTxString (midi, &midiLen, i * 2 + 1, b))
                sendMidiCommandToController (midi, midiLen);
        }

        if (! b)
        {
            for (int i = 0; i < 16; ++i)
                setDisplayDirty (i * 2 + 1, 1);
        }
    }
}

void MackieC4::updateDisplayAndPots()
{
    static bool reentrant = false;

    if (! reentrant)
    {
        CRASH_TRACER
        reentrant = true;

        enableMeters (mode == MixerMode);

        if (mode == ButtonMode)
        {
            for (int i = 0; i < 32; ++i)
            {
                const String s1 (functionNames[i]);

                setDisplayText (i, 0, s1.upToFirstOccurrenceOf ("/", false, false));
                setDisplayText (i, 1, s1.fromFirstOccurrenceOf ("/", false, false));
                setPotPosition (i, -1.0f);
            }

            updateButtonLights();
        }
        else if (mode == PluginMode1)
        {
            turnOffPanPotLights();

            for (int i = 0; i < 32; ++i)
            {
                if (auto track = externalControllerManager.getChannelTrack (trackViewOffset + i))
                    setDisplayText (i, 0, ExternalController::shortenName (track->getName(), 6));
                else
                    setDisplayText (i, 0, {});

                setDisplayText (i, 1, {});
            }
        }
        else if (mode == PluginMode2)
        {
            turnOffPanPotLights();

            if (auto track = externalControllerManager.getChannelTrack (selectedTrackNum))
            {
                for (int i = 0; i < 32; ++i)
                {
                    if (auto f = track->pluginList[i])
                        setDisplayText (i, 0, ExternalController::shortenName (f->getShortName(6), 6));
                    else
                        setDisplayText (i, 0, {});

                    setDisplayText (i, 1, {});
                }
            }
        }
        else if (mode == PluginMode3)
        {
            updateDeviceState();
        }
        else if (mode == AuxMode)
        {
            userChangedFaderBanks(0);
            userChangedAuxBank(0);
        }
        else if (mode == MixerMode)
        {
            userChangedFaderBanks (0);
        }

        updateMiscLights();
    }

    reentrant = false;
}

void MackieC4::turnOffPanPotLights()
{
    for (int i = 32; --i >= 0;)
        setPotPosition (i, -1.0f);
}

void MackieC4::updateButtonLights()
{
    if (mode == ButtonMode)
    {
        if (auto ed = getEdit())
        {
            auto& ecm = externalControllerManager;
            auto& tc = ed->getTransport();

            lightUpPotButton (8,  tc.looping);
            lightUpPotButton (9,  ed->recordingPunchInOut);
            lightUpPotButton (10, ed->clickTrackEnabled);
            lightUpPotButton (11, tc.snapToTimecode);
            lightUpPotButton (12, ed->playInStopEnabled);
            lightUpPotButton (13, ecm.isScrollingEnabled && ecm.isScrollingEnabled (*ed));
            lightUpPotButton (14, ed->isTimecodeSyncEnabled());
            lightUpPotButton (30, ed->getAutomationRecordManager().isReadingAutomation());
            lightUpPotButton (31, ed->getAutomationRecordManager().isWritingAutomation());
        }
    }
}

void MackieC4::lightUpButton (int buttonNum, bool on)
{
    char midi[256];
    int midiLen = 0;

    if (c4->C4LEDTxString (midi, &midiLen,
                           buttonNum,
                           on ? C4SwitchSetOn
                              : C4SwitchSetOff))
    {
        sendMidiCommandToController (midi, midiLen);
    }
}

void MackieC4::setPotPosition (int potNumber, float value)
{
    const int panPos = (value < 0.0f) ? 0
                                      : jlimit (1, 11, roundToInt (value * 10.0f + 1.0f));

    if (panPos != currentPanPotPos[potNumber])
    {
        char midi[256];
        int midiLen = 0;

        currentPanPotPos[potNumber] = panPos;

        if (c4->C4VPotTxString (midi, &midiLen, potNumber, panPos, false))
            sendMidiCommandToController (midi, midiLen);
    }
}

void MackieC4::lightUpPotButton (int buttonNum, bool on)
{
    const int panPos = on ? 12 : 0;

    if (panPos != currentPanPotPos[buttonNum])
    {
        currentPanPotPos[buttonNum] = panPos;

        char midi[256];
        int midiLen = 0;

        if (c4->C4VPotTxString (midi, &midiLen,
                                buttonNum,
                                on ? 11 : 0,
                                false, C4VPotModeWrap))
        {
            sendMidiCommandToController (midi, midiLen);
        }
    }
}

void MackieC4::timerCallback()
{
    if (initialised)
    {
        stopTimer();
        updateDeviceState();
    }
}

void MackieC4::acceptMidiMessage (const MidiMessage& m)
{
    CRASH_TRACER

    const unsigned char* const d = m.getRawData();
    const int numBytes = m.getRawDataSize();

    C4RxResult result;

    if (c4->TranslateRxResult (&result, (char*)d, numBytes))
    {
        if (result.theType == C4RxTypeWakeUp)
        {
            //  theExt1 = length of the string
            //  theExt2 = not used
            //  theString = 11 byte device indentification
            initialiseDevice (true);
            updateDeviceState();
            startTimer (500);
            return;
        }

        if (! initialised)
        {
            if (result.theType == C4RxTypeDeviceInquiry)
                initialised = true;

            return;
        }

        if (result.theType == C4RxTypeSwitch
             && (result.theExt1 == C4SwitchNumberVPot28 || result.theExt1 == C4SwitchNumberVPot29)
             && mode == ButtonMode)
        {
            if (result.theExt1 == C4SwitchNumberVPot29)
                userChangedFastForwardButton (result.theExt2 != 0);
            else
                userChangedRewindButton (result.theExt2 != 0);
        }
        else if (result.theType == C4RxTypeSwitch && result.theExt2 != 0)
        {
            if (result.theExt1 == C4SwitchNumberBankLeft)
            {
                if (mode == PluginMode1)
                {
                    if (getEdit() != nullptr)
                    {
                        userChangedFaderBanks (-8);
                        trackViewOffset = owner->channelStart;
                        updateDisplayAndPots();
                    }

                    redrawSelectedTracks();
                }
                else if (mode == PluginMode3)
                {
                    userChangedParameterBank (-8);
                }
                else if (mode == MixerMode)
                {
                    userChangedFaderBanks (-8);
                }
                else if (mode == AuxMode)
                {
                    userChangedFaderBanks(-8);
                }
            }
            else if (result.theExt1 == C4SwitchNumberBankRight)
            {
                if (mode == PluginMode1)
                {
                    if (getEdit() != nullptr)
                    {
                        userChangedFaderBanks (+8);
                        trackViewOffset = owner->channelStart;
                        updateDisplayAndPots();
                    }
                }
                else if (mode == PluginMode3)
                {
                    userChangedParameterBank (8);
                }
                else if (mode == MixerMode)
                {
                    userChangedFaderBanks (8);
                }
                else if (mode == AuxMode)
                {
                    userChangedFaderBanks(8);
                }
            }
            else if (result.theExt1 == C4SwitchNumberParameterLeft)
            {
                if (mode == PluginMode1)
                {
                    trackViewOffset = jmax (0, jmin (externalControllerManager.getNumChannelTracks() - 32, trackViewOffset - 1));
                    updateDisplayAndPots();
                }
                else if (mode == PluginMode3)
                {
                    userChangedParameterBank (-1);
                }
                else if (mode == MixerMode)
                {
                    userChangedFaderBanks (-1);
                }
            }
            else if (result.theExt1 == C4SwitchNumberParameterRight)
            {
                if (mode == PluginMode1)
                {
                    trackViewOffset = jmax (0, jmin (externalControllerManager.getNumChannelTracks() - 32, trackViewOffset + 1));
                    updateDisplayAndPots();
                }
                else if (mode == PluginMode3)
                {
                    userChangedParameterBank (1);
                }
                else if (mode == MixerMode)
                {
                    userChangedFaderBanks (1);
                }
            }
            else if (result.theExt1 == C4SwitchNumberLock)
            {
                // lock
                if (mode == PluginMode3)
                {
                    if (isLocked || wasLocked)
                    {
                        isLocked  = false;
                        wasLocked = false;
                    }
                    else
                    {
                        isLocked  = true;
                        wasLocked = false;
                    }
                    updateMiscLights();
                }
            }
            else if (result.theExt1 == C4SwitchNumberSpotErase)
            {
                // bypass
                owner->muteOrUnmutePlugin();
            }
            else if (result.theExt1 == C4SwitchNumberMarker)
            {
                // plugin
                mode = PluginMode1;
                numberOfFaderChannels = 32;
                selectedTrackNum = 0;
                if (isLocked)
                {
                    isLocked  = false;
                    wasLocked = false;
                }

                userChangedFaderBanks(0);
                updateDisplayAndPots();
                updateMiscLights();
                redrawSelectedPlugin();
                redrawSelectedTracks();
            }
            else if (result.theExt1 == C4SwitchNumberChannelStrip)
            {
                // aux
                mode = AuxMode;
                isLocked = false;
                wasLocked = false;
                numberOfFaderChannels = 24;
                updateDisplayAndPots();
                updateMiscLights();
                redrawSelectedPlugin();
                redrawSelectedTracks();
            }
            else if (result.theExt1 == C4SwitchNumberTrack)
            {
                // mix
                mode = MixerMode;
                isLocked = false;
                wasLocked = false;
                numberOfFaderChannels = 16;
                updateDisplayAndPots();
                updateMiscLights();
                redrawSelectedPlugin();
                redrawSelectedTracks();
            }
            else if (result.theExt1 == C4SwitchNumberFunction)
            {
                // edit
                mode = ButtonMode;
                numberOfFaderChannels = 16;
                isLocked = false;
                wasLocked = false;
                updateDisplayAndPots();
                updateMiscLights();
                redrawSelectedPlugin();
                redrawSelectedTracks();
            }
            else if (result.theExt1 == C4SwitchNumberShift)
            {
                // back
                if (mode == PluginMode2)
                {
                    mode = PluginMode1;
                    numberOfFaderChannels = 32;
                    updateMiscLights();
                }
                else if (mode == PluginMode3)
                {
                    mode = PluginMode2;
                    numberOfFaderChannels = 16;
                    updateMiscLights();
                }

                if (isLocked)
                {
                    isLocked  = false;
                    wasLocked = true;
                }
                userChangedFaderBanks(0);
                updateDisplayAndPots();
                redrawSelectedPlugin();
                redrawSelectedTracks();
            }
            else if (result.theExt1 == C4SwitchNumberOption)
            {
                // solo
                if (mode == PluginMode3)
                {
                    owner->soloPluginTrack();
                }
            }
            else if (result.theExt1 == C4SwitchNumberControl)
            {
                // dec
                owner->changePluginPreset (-1);
            }
            else if (result.theExt1 == C4SwitchNumberAlt)
            {
                // inc
                owner->changePluginPreset (1);
            }
            else if (result.theExt1 == C4SwitchNumberSlotUp)
            {
                // up
                if (isLocked)
                {
                    wasLocked = true;
                    isLocked = false;
                }
                selectOtherObject (SelectableClass::Relationship::moveUp, true);
            }
            else if (result.theExt1 == C4SwitchNumberSlotDown)
            {
                // down
                if (isLocked)
                {
                    wasLocked = true;
                    isLocked = false;
                }
                selectOtherObject (SelectableClass::Relationship::moveDown, true);
            }
            else if (result.theExt1 == C4SwitchNumberTrackLeft)
            {
                // left
                if (isLocked)
                {
                    wasLocked = true;
                    isLocked = false;
                }
                selectOtherObject (SelectableClass::Relationship::moveLeft, true);
            }
            else if (result.theExt1 == C4SwitchNumberTrackRight)
            {
                // right
                if (isLocked)
                {
                    wasLocked = true;
                    isLocked = false;
                }
                selectOtherObject (SelectableClass::Relationship::moveRight, true);
            }
            else if (result.theExt1 == C4SwitchNumberSplit)
            {
                // split
                engine.getUIBehaviour().showWarningMessage (TRANS("Sorry - C4-split mode is not yet implemented!"));
            }
            else if (result.theExt1 >= C4SwitchNumberVPot0
                       && result.theExt1 <= C4SwitchNumberVPot31)
            {
                // pot pressed
                const int potNum = result.theExt1 - C4SwitchNumberVPot0;

                if (mode == ButtonMode)
                {
                    const String command (functionNames[potNum]);

                    //xxx is this the same thing as before?
                    if (auto tc = getTransport())
                        if (command.isNotEmpty() && ! tc->isSafeRecording())
                            performNamedFunction (command);
                }
                else if (mode == PluginMode1)
                {
                    selectedTrackNum = trackViewOffset + potNum;
                    mode = PluginMode2;

                    numberOfFaderChannels = 16;
                    updateDisplayAndPots();
                    updateMiscLights();
                    redrawSelectedPlugin();
                    redrawSelectedTracks();
                }
                else if (mode == PluginMode2)
                {
                    if (auto t = externalControllerManager.getChannelTrack (selectedTrackNum))
                    {
                        if (auto af = t->pluginList[potNum])
                        {
                            mode = PluginMode3;
                            isLocked = wasLocked;
                            wasLocked = false;
                            numberOfFaderChannels = 16;

                            if (auto sm = externalControllerManager.getSelectionManager())
                                sm->selectOnly (af);

                            updateMiscLights();
                            redrawSelectedPlugin();
                            redrawSelectedTracks();

                            if (isLocked)
                            {
                                isLocked  = false;
                                wasLocked = true;
                            }
                        }
                    }
                }
                else if (mode == PluginMode3)
                {
                    userPressedParameterControl(potNum);
                }
                else if (mode == MixerMode)
                {
                    if ((potNum & 1) == 0)
                        userMovedFader (potNum / 2, decibelsToVolumeFaderPosition (0.0f));
                    else
                        userMovedPanPot (potNum / 2, 0);
                }
                else if (mode == AuxMode)
                {
                    if (potNum >= 0 && potNum < 8)
                        userChangedAuxBank (potNum - auxBank);
                    else
                        userPressedAux (potNum - 8);
                }
            }
        }
        else if (result.theType == C4RxTypeVPot)
        {
            // pot turned
            currentPotPos[result.theExt1] = jlimit (0.0f, 1.0f, currentPotPos[result.theExt1]  + result.theExt2 * panPotSpeed);

            if (mode == PluginMode3)
            {
                userMovedParameterControl (result.theExt1, currentPotPos[result.theExt1]);
            }
            else if (mode == MixerMode)
            {
                const int chan = result.theExt1 / 2;

                if ((result.theExt1 & 1) == 0)
                    userMovedFader (chan, currentPotPos[result.theExt1]);
                else
                    userMovedPanPot (chan, currentPotPos[result.theExt1] * 2.0f - 1.0f);
            }
            else if (mode == AuxMode)
            {
                const int chan = result.theExt1 - 8;
                userMovedAux (chan, currentPotPos[result.theExt1]);
            }
        }
    }
}

void MackieC4::currentSelectionChanged()
{
    if (mode == PluginMode1 || mode == PluginMode2)
    {
        numberOfFaderChannels = 16;
        mode = PluginMode3;
        isLocked = wasLocked;
        wasLocked = false;
        updateMiscLights();
    }
    else if (mode == PluginMode3)
    {
        isLocked = wasLocked;
        wasLocked = false;
    }
}

void MackieC4::parameterChanged (int parameterNumber, const ParameterSetting& newValue)
{
    if (mode == PluginMode3
        && parameterNumber >= 0
        && parameterNumber < 32)
    {
        currentPotPos[parameterNumber] = newValue.value;
        setPotPosition (parameterNumber, newValue.value);

        setDisplayText (parameterNumber, 0, String::fromUTF8 (newValue.label));
        setDisplayText (parameterNumber, 1, String::fromUTF8 (newValue.valueDescription));
    }
}

void MackieC4::clearParameter (int parameterNumber)
{
    if (mode == PluginMode3
        && parameterNumber >= 0
        && parameterNumber < 32)
    {
        currentPotPos[parameterNumber] = 0.0f;
        setPotPosition (parameterNumber, -1.0f);

        setDisplayText (parameterNumber, 0, "       ");
        setDisplayText (parameterNumber, 1, "       ");
    }
}

void MackieC4::moveFader (int channelNum, float newSliderPos)
{
    if (mode == MixerMode)
    {
        currentPotPos[channelNum * 2] = newSliderPos;
        setPotPosition (channelNum * 2, newSliderPos);
    }
}

void MackieC4::moveMasterLevelFader (float, float)
{
}

void MackieC4::movePanPot (int channelNum, float newPan)
{
    if (mode == MixerMode)
    {
        currentPotPos[channelNum * 2 + 1] = newPan * 0.5f + 0.5f;
        setPotPosition (channelNum * 2 + 1, currentPotPos[channelNum * 2 + 1]);
    }
}

void MackieC4::faderBankChanged (int, const StringArray& trackNames)
{
    if (mode == MixerMode)
    {
        for (int i = 0; i < 16; ++i)
        {
            if (trackNames[i].isNotEmpty())
            {
                setDisplayText (i * 2,     1, trackNames[i]);
                setDisplayText (i * 2,     0, TRANS("Level"));
                setDisplayText (i * 2 + 1, 0, TRANS("Pan"));
            }
            else
            {
                setDisplayText (i * 2,     0, "       ");
                setDisplayText (i * 2,     1, "       ");
                setDisplayText (i * 2 + 1, 0, "       ");
                setDisplayText (i * 2 + 1, 1, "       ");
            }
        }
    }
    else if (mode == AuxMode)
    {
        turnOffPanPotLights();

        if (auto edit = getEdit())
        {
            lightUpPotButton (auxBank, true);

            for (int i = 0; i < 8; ++i)
            {
                auto busName = edit->getAuxBusName (i);

                if (busName.isEmpty())
                    busName = (TRANS("Bus") + " #" + String (i + 1));

                setDisplayText (i, 0, busName);
                setDisplayText (i, 1, {});
            }

            for (int i = 0; i < 24; ++i)
            {
                if (trackNames[i].isNotEmpty())
                {
                    setDisplayText (i + 8, 0, trackNames[i]);
                    setDisplayText (i + 8, 1, "       ");
                }
                else
                {
                    setDisplayText (i + 8, 0, "       ");
                    setDisplayText (i + 8, 1, "       ");
                }
            }
        }
    }
}

void MackieC4::moveAux (int channelNum, const char*, float newPos)
{
    if (mode == AuxMode)
    {
        currentPotPos[channelNum + 8] = newPos;
        setDisplayText (channelNum + 8, 1, Decibels::toString (volumeFaderPositionToDB (newPos), 1, -96.0f));
        setPotPosition (channelNum + 8, newPos);
    }
}

void MackieC4::clearAux (int channel)
{
    if (mode == AuxMode)
    {
        setDisplayText (channel + 8, 1, {});
        lightUpPotButton(channel + 8, false);
    }
}

void MackieC4::channelLevelChanged (int channel, float level)
{
    if (mode == MixerMode)
    {
        CRASH_TRACER

        char midi[256];
        int midiLen = 0;

        if (c4->C4LCDSetMeterLevelTxString (midi, &midiLen, channel * 2 + 1,
                                            jlimit (0, 12, (roundToInt (12.0f * level)))))
            sendMidiCommandToController (midi, midiLen);
    }
}

void MackieC4::updateSoloAndMute (int, Track::MuteAndSoloLightState, bool) {}
void MackieC4::soloCountChanged (bool) {}
void MackieC4::trackSelectionChanged (int, bool) {}

void MackieC4::playStateChanged (bool)                { updateButtonLights(); }
void MackieC4::recordStateChanged (bool)              { updateButtonLights(); }
void MackieC4::automationReadModeChanged (bool)       { updateButtonLights(); }
void MackieC4::automationWriteModeChanged (bool)      { updateButtonLights(); }
void MackieC4::clickOnOffChanged (bool)               { updateButtonLights(); }
void MackieC4::snapOnOffChanged (bool)                { updateButtonLights(); }
void MackieC4::loopOnOffChanged (bool)                { updateButtonLights(); }
void MackieC4::slaveOnOffChanged (bool)               { updateButtonLights(); }
void MackieC4::punchOnOffChanged (bool)               { updateButtonLights(); }

void MackieC4::masterLevelsChanged (float, float) {}
void MackieC4::timecodeChanged (int, int, int, int, bool, bool) {}
void MackieC4::trackRecordEnabled (int, bool) {}

bool MackieC4::canChangeSelectedPlugin()
{
    return ! isLocked;
}

bool MackieC4::showingPluginParams()
{
    return mode == PluginMode2 || mode == PluginMode3;
}

bool MackieC4::showingTracks()
{
    return mode == MixerMode;
}

void MackieC4::markerChanged (int, const MarkerSetting&) {}
void MackieC4::clearMarker (int) {}

void MackieC4::auxBankChanged (int bank)
{
    if (mode == AuxMode)
    {
        lightUpPotButton (auxBank, false);
        lightUpPotButton (bank, true);
        auxBank = bank;
    }
}

void MackieC4::pluginBypass (bool b)
{
    bypass = b;
    updateMiscLights();
}
