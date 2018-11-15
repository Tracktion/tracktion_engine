/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


MackieMCU::MackieMCU (ExternalControllerManager& ecm)  : ControlSurface (ecm)
{
    deviceDescription = "Mackie Control Universal";
    needsMidiBackChannel = true;
    wantsClock = false;
    numberOfFaderChannels = 8;
    numCharactersForTrackNames = 6;
    numCharactersForAuxLabels = 6;
    numCharactersForParameterLabels = 6;
    numParameterControls = 8;
    numMarkers = 8;
    numCharactersForMarkerLabels = 6;
    deletable = false;
    shiftKeysDown = 0;
    wantsAuxBanks = true;
    needsMidiChannel = true;

    cpuMeterTimer = std::make_unique<CpuMeterTimer> (*this);
    auxTimer = std::make_unique<AuxTimer> (*this);

    indicesChanged();

    oneTouchRecord = engine.getPropertyStorage().getProperty (SettingID::MCUoneTouchRecord, false);
}

MackieMCU::~MackieMCU()
{
    engine.getPropertyStorage().setProperty (SettingID::MCUoneTouchRecord, oneTouchRecord);

    notifyListenersOfDeletion();

    cpuMeterTimer = nullptr;
    auxTimer = nullptr;
}

void MackieMCU::indicesChanged()
{
    for (int i = 0; i < maxNumSurfaces; ++i)
        for (int j = 9; --j >= 0;)
           lastFaderPos[i][j] = 0x7fffffff;

    zeromem (panPos, sizeof (panPos));
    zeromem (timecodeDigits, sizeof (timecodeDigits));
    zeromem (lastChannelLevels, sizeof (lastChannelLevels));
    zeromem (recLight, sizeof (recLight));

    zeromem (currentDisplayChars, sizeof (currentDisplayChars));
    zeromem (newDisplayChars, sizeof (newDisplayChars));
}

//==============================================================================
void MackieMCU::setDisplay (int devIdx, const String& text, int pos)
{
    CRASH_TRACER

    const int len = jmin (56 * 2 - pos, (int) text.length());

    for (int i = 0; i < len; ++i)
        newDisplayChars[devIdx][pos + i] = (char) text[i];

    triggerAsyncUpdate();
}

void MackieMCU::setDisplay (int devIdx, const char* topLine, const char* bottomLine)
{
    CRASH_TRACER

    zeromem (newDisplayChars[devIdx], maxCharsOnDisplay);
    strncpy ((char*) newDisplayChars[devIdx], topLine, 56);
    strncpy ((char*) newDisplayChars[devIdx] + 56, bottomLine, 56);

    triggerAsyncUpdate();
}

void MackieMCU::clearDisplaySegment (int devIdx, int column, int row)
{
    setDisplaySegment (devIdx, column, row, "      ");
}

void MackieMCU::setDisplaySegment (int devIdx, int column, int row, const String& text)
{
    CRASH_TRACER
    setDisplay (devIdx, text.substring (0, 6).paddedRight (' ', 6), column * 7 + row * 56);
}

void MackieMCU::centreDisplaySegment (int devIdx, int column, int row, const String& text)
{
    setDisplaySegment (devIdx, column, row, String::repeatedString (" ", (6 - text.length()) / 2) + text);
}

void MackieMCU::handleAsyncUpdate()
{
    CRASH_TRACER

    for (int i = 0; i < extenders.size() + 1; ++i)
    {
        char newChars[maxCharsOnDisplay];
        memcpy (newChars, newDisplayChars[i], maxCharsOnDisplay);

        char* const currentChars = currentDisplayChars[i];

        for (int j = 0; j < maxCharsOnDisplay; ++j)
            if (newChars[j] == 0)
                newChars[j] = ' ';

        if (i == 0
             && assignmentMode == PluginMode
             && owner->startParamNumber < 2)
        {
            newChars[13]      = '|';
            newChars[56 + 13] = '|';
        }

        int end = 56 + 55;

        while (end > 0)
        {
            if (newChars[end] != currentChars[end])
            {
                ++end;
                break;
            }

            --end;
        }

        int start = 0;

        while (start < end)
        {
            if (newChars[start] != currentChars[start])
                break;

            ++start;
        }

        if (end > start)
        {
            uint8 d[256] = { 0 };
            d[0] = (uint8) 0xf0;
            d[3] = (uint8) 0x66;
            d[4] = (uint8) (i == deviceIdx ? 0x14 : 0x15);
            d[5] = (uint8) 0x12;
            d[6] = (uint8) start;
            d[7 + end - start] = (uint8) 0xf7;

            memcpy (d + 7, newChars + start, (size_t) (end - start));

            sendMidiCommandToController (i, d, 8 + end - start);

            memcpy (currentChars, newChars, maxCharsOnDisplay);
        }
    }
}

void MackieMCU::setSignalMetersEnabled (int dev, bool b)
{
    CRASH_TRACER

    // enable signal level meters..
    for (int j = 0; j < 8; ++j)
    {
        uint8 d[10];
        d[0] = 0xf0;
        d[1] = 0x00;
        d[2] = 0x00;
        d[3] = 0x66;
        d[4] = (dev == deviceIdx) ? 0x14 : 0x15; // xxx or 15 for extender
        d[5] = 0x20;
        d[6] = (uint8) j;
        d[7] = b ? 3 : 0;  // 1 for just led
        d[8] = 0xf7;

        sendMidiCommandToController (dev, d, 9);

        if (b)
        {
            d[5] = 0x21;
            d[6] = 0x00;
            d[7] = 0xf7;

            sendMidiCommandToController (dev, d, 8);

            d[0] = 0xd0;
            d[1] = (uint8) ((j << 4) | 0x0f);

            sendMidiCommandToController (dev, d, 2);
        }
    }
}

void MackieMCU::setSignalMetersEnabled (bool b)
{
    for (int i = 0; i < extenders.size() + 1; ++i)
        setSignalMetersEnabled (i, b);
}

void MackieMCU::initialiseDevice (bool /*connect*/)
{
    CRASH_TRACER
    lastSmpteBeatsSetting = false;
    isRecButtonDown = marker = nudge = isZooming = false;
    lastStartChan = 0;

    indicesChanged();

    zeromem (auxLevels, sizeof (auxLevels));
    zeromem (auxBusNames, sizeof (auxBusNames));

    lightUpButton (deviceIdx, 0x54, marker);
    lightUpButton (deviceIdx, 0x55, nudge);
    lightUpButton (deviceIdx, 0x64, isZooming);
    lightUpButton (deviceIdx, 0x33, editFlip);

    for (int i = 0; i < 0x20; ++i)
        lightUpButton (deviceIdx, i, false);

    memset (currentDisplayChars, 255, sizeof (currentDisplayChars));
    zeromem (newDisplayChars, sizeof (newDisplayChars));

    setDisplay (deviceIdx, "             - Mackie Control Universal - ", "");

    cpuVisible = false;
    cpuMeterTimer->stopTimer();

    Thread::sleep (100);
    setSignalMetersEnabled (deviceIdx, true);

    assignmentMode = PluginMode;
    setAssignmentMode (PanMode);

    flipped = true;
    flip();

    updateMiscFeatures();
}

void MackieMCU::shutDownDevice()
{
    cpuMeterTimer->stopTimer();

    // send a reset message:
    uint8 d[8] = { 0xf0, 0x00, 0x00, 0x66, 0x14, 0x08, 0x00, 0xf7 };
    sendMidiCommandToController (deviceIdx, d, 8);
}

bool MackieMCU::isEditValidAndNotSafeRecording() const
{
    if (auto tc = getTransport())
        return ! tc->isSafeRecording();

    return false;
}

void MackieMCU::updateMiscFeatures()
{
    auto& ecm = externalControllerManager;
    auto* ed = getEdit();
    lightUpButton (deviceIdx, 0x58, ed != nullptr && ed->playInStopEnabled);
    lightUpButton (deviceIdx, 0x59, ed != nullptr && ecm.isScrollingEnabled ? ecm.isScrollingEnabled (*ed) : false);
    lightUpButton (deviceIdx, 0x4a, engine.getUIBehaviour().getBigInputMetersMode());
}

void MackieMCU::cpuTimerCallback()
{
    if (cpuVisible)
    {
        auto cpuPercent = roundToInt (engine.getDeviceManager().getCpuUsage() * 100.0f);
        setAssignmentText (String (jlimit (0, 99, cpuPercent)).paddedLeft ('0', 2));
    }
}

void MackieMCU::auxTimerCallback()
{
    auxTimer->stopTimer();

    Array<int> auxCopy;
    auxCopy.swapWith (userMovedAuxes);

    for (int i = auxCopy.size(); --i >= 0;)
    {
        int chan = auxCopy.getUnchecked(i);
        float level = auxLevels[chan];
        auxLevels[chan] = -1000.0f;
        moveAux (chan, auxBusNames[chan], level);
    }
}

void MackieMCU::acceptMidiMessage (const MidiMessage& m)
{
    acceptMidiMessage (deviceIdx, m);
}

void MackieMCU::acceptMidiMessage (int deviceIndex, const MidiMessage& m)
{
    const unsigned char* const d = m.getRawData();
    const unsigned char d1 = d[1];

    if (d[0] == 0xb0)
    {
        if (d1 == 0x3c)
        {
            userMovedJogWheel (((d[2] & 0x40) == 0) ? 0.5f : -0.5f);
        }
        else if (d[1] >= 0x10 && d[1] <= 0x17)
        {
            const int chan = (d[1] & 0x0f) + 8 * deviceIndex;
            float diff = 0.02f * (d[2] & 0x0f);

            if ((d[2] & 0x40) != 0)
                diff = -diff;

            panPos[chan] = jlimit (-1.0f, 1.0f, panPos[chan] + diff);

            if (flipped)
            {
                userMovedFader (chan, (panPos[chan] + 1.0f) * 0.5f);
            }
            else
            {
                if (assignmentMode == PluginMode)
                {
                    userMovedParameterControl (chan, (panPos[chan] + 1.0f) * 0.5f);
                }
                else if (assignmentMode == AuxMode)
                {
                    userMovedAux (chan, (panPos[chan] + 1.0f) * 0.5f);
                    userMovedAuxes.addIfNotAlreadyThere(chan);
                    auxTimer->startTimer(2000);
                }
                else if (assignmentMode == PanMode)
                {
                    userMovedPanPot (chan, panPos[chan]);
                }
            }
        }
    }
    else if (d[0] >= 0xe0 && d[0] <= 0xe8)
    {
        // fader
        const float pos = jlimit (0.0f, 1.0f, ((((int)d[2]) << 7) + d[1]) * (1.0f / 0x3f70));

        // send it back to the MCU
        sendMidiCommandToController (deviceIndex, d, 3);

        if (d[0] == 0xe8)
        {
            userMovedMasterLevelFader (pos);
        }
        else
        {
            if (flipped)
            {
                if (assignmentMode == PluginMode)
                {
                    userMovedParameterControl ((d[0] & 0x0f) + deviceIndex * 8, pos);
                }
                else if (assignmentMode == AuxMode)
                {
                    userMovedAux ((d[0] & 0x0f) + deviceIndex * 8, pos);
                    userMovedAuxes.addIfNotAlreadyThere((d[0] & 0x0f) + deviceIndex * 8);
                    auxTimer->startTimer(2000);
                }
                else if (assignmentMode == PanMode)
                {
                    userMovedPanPot ((d[0] & 0x0f) + deviceIndex * 8, pos * 2.0f - 1.0f);
                }
            }
            else
            {
                userMovedFader ((d[0] & 0x0f) + deviceIndex * 8, pos);
            }
        }
    }
    else if (d[0] == 0x90)
    {
        if (d1 == 0x5b)
        {
            if (nudge)
            {
                if (d[2] == 0)
                {
                    userChangedRewindButton (false);
                    userNudgedLeft();
                }
            }
            else if (marker)
            {
                if (d[2] == 0)
                {
                    userChangedRewindButton (false);
                    userSkippedToNextMarkerLeft();
                }
            }
            else
            {
                if (d[2] != 0)
                {
                    const uint32 now = Time::getMillisecondCounter();

                    if (now < lastRewindPress + 300)
                    {
                        userPressedHome();
                        return;
                    }

                    lastRewindPress = now;
                }

                userChangedRewindButton (d[2] != 0);
            }

            lightUpButton (deviceIndex, 0x5b, d[2] != 0);
        }
        else if (d1 == 0x5c)
        {
            if (nudge)
            {
                if (d[2] == 0)
                {
                    userChangedFastForwardButton (false);
                    userNudgedRight();
                }
            }
            else if (marker)
            {
                if (d[2] == 0)
                {
                    userChangedFastForwardButton (false);
                    userSkippedToNextMarkerRight();
                }
            }
            else
            {
                userChangedFastForwardButton (d[2] != 0);
            }

            lightUpButton (deviceIndex, 0x5c, d[2] != 0);
        }
        else if (d1 == 0x5f)
        {
            isRecButtonDown = (d[2] != 0);

            if (oneTouchRecord)
            {
                if (isRecButtonDown)
                {
                    if (shiftKeysDown != 0)
                        userPressedArmAll();
                    else
                        userPressedRecord();
                }
            }
            else
            {
                if (isRecButtonDown && (shiftKeysDown != 0))
                    userPressedArmAll();
            }
        }
        else if (d1 == 0x46)
        {
            if (d[2] != 0)
                shiftKeysDown = 1;
            else
                shiftKeysDown = 0;
        }
        else if (d1 == 0x34)
        {
            cpuVisible = (d[2] != 0);

            if (cpuVisible)
            {
                cpuMeterTimer->startTimer (497);
                cpuTimerCallback();
            }
            else
            {
                cpuMeterTimer->stopTimer();
                timerCallback();
            }
        }
        else if (d[2] != 0)
        {
            if (d1 >= 0x08 && d1 <= 0x0f)
            {
                if (shiftKeysDown)
                    userPressedSoloIsolate ((d1 - 0x08) + 8 * deviceIndex);
                else
                    userPressedSolo ((d1 - 0x08) + 8 * deviceIndex);
            }
            else if (d1 >= 0x10 && d1 <= 0x17)
            {
                userPressedMute ((d1 - 0x10) + 8 * deviceIndex, shiftKeysDown != 0);
            }
            else if (d1 >= 0x20 && d1 <= 0x27)
            {
                const int chan = (d1 & 0x0f) + 8 * deviceIndex;
                panPos[chan] = 0.0f;

                if (flipped)
                {
                    userMovedFader (chan, decibelsToVolumeFaderPosition (0.0f));
                }
                else
                {
                    if (assignmentMode == PluginMode)
                    {
                        userPressedParameterControl (chan);
                    }
                    else if (assignmentMode == AuxMode)
                    {
                        userPressedAux(chan);
                    }
                    else if (assignmentMode == MarkerMode)
                    {
                        userPressedGoToMarker(chan);
                    }
                    else if (assignmentMode == PanMode)
                    {
                        userMovedPanPot (chan, 0.0f);
                    }
                }
            }
            else if (d1 >= 0x18 && d1 <= 0x1f)
            {
                if (shiftKeysDown != 0)
                {
                    //userSelectedTrack (d1 - 0x18);
                    userSelectedClipInTrack ((d1 - 0x18) + 8 * deviceIndex);
                }
                else
                {
                    userSelectedPluginInTrack ((d1 - 0x18) + 8 * deviceIndex);
                }
            }
            else if (d1 <= 0x07)
            {
                // rec buttons
                userPressedRecEnable (d1 + 8 * deviceIndex, shiftKeysDown != 0);
            }
            else if (d1 == 0x5d)
            {
                userPressedStop();
            }
            else if (d1 == 0x5e)
            {
                if (oneTouchRecord)
                {
                    userPressedPlay();
                }
                else
                {
                    if (isRecButtonDown)
                        userPressedRecord();
                    else
                        userPressedPlay();
                }
            }
            else if (d1 == 0x2e)
            {
                userChangedFaderBanks (-(extenders.size() + 1) * 8);
            }
            else if (d1 == 0x2f)
            {
                userChangedFaderBanks ((extenders.size() + 1) * 8);
            }
            else if (d1 == 0x30)
            {
                userChangedFaderBanks (-1);
            }
            else if (d1 == 0x31)
            {
                userChangedFaderBanks (1);
            }
            else if (d1 == 0x35)
            {
                userToggledBeatsSecondsMode();
            }
            else if (d1 == 0x36)
            {
                muteOrUnmutePluginsInTrack();
            }
            else if (d1 == 0x37)
            {
                if (isEditValidAndNotSafeRecording())
                {
                    auto& masterPlugins = getEdit()->getMasterPluginList();

                    int numEnabled = 0;

                    for (auto f : masterPlugins)
                        if (f->isEnabled())
                            ++numEnabled;

                    bool enabled = numEnabled < masterPlugins.size();

                    for (auto f : masterPlugins)
                        f->setEnabled (enabled);
                }
            }
            else if (d1 == 0x38)
            {
                if (shiftKeysDown)
                    userPressedJumpToMarkIn();
                else
                    userPressedMarkIn();
            }
            else if (d1 == 0x39)
            {
                if (shiftKeysDown)
                    userPressedJumpToMarkOut();
                else
                    userPressedMarkOut();
            }
            else if (d1 == 0x3a)
            {
                userPressedCut();
            }
            else if (d1 == 0x3b)
            {
                userPressedCopy();
            }
            else if (d1 == 0x3c)
            {
                userPressedPaste (shiftKeysDown != 0);
            }
            else if (d1 == 0x3d)
            {
                userPressedDelete (shiftKeysDown != 0);
            }
            else if (d1 == 0x3e)
            {
                if (shiftKeysDown != 0)
                    userPressedZoomToFit();

                userPressedZoomFitToTracks();
            }
            else if (d1 == 0x3f)
            {
                userPressedZoomToFit();
            }
            else if (d1 == 0x40)
            {
                userPressedPreviousMarker();
            }
            else if (d1 == 0x41)
            {
                userPressedNextMarker();
            }
            else if (d1 == 0x42)
            {
                if (shiftKeysDown)
                    userPressedInsertTimeSigChange();
                else
                    userPressedInsertTempoChange();
            }
            else if (d1 == 0x43)
            {
                if (getEdit() == nullptr || isEditValidAndNotSafeRecording())
                    AppFunctions::showProjectScreen();
            }
            else if (d1 == 0x44)
            {
                if (getEdit() == nullptr || isEditValidAndNotSafeRecording())
                    AppFunctions::showSettingsScreen();
            }
            else if (d1 == 0x45)
            {
                if (getEdit() == nullptr || isEditValidAndNotSafeRecording())
                    AppFunctions::showEditScreen();
            }
            else if (d1 == 0x47)
            {
                if (shiftKeysDown)
                    userPressedInsertPitchChange();
                else
                    userPressedCreateMarker();
            }
            else if (d1 == 0x48)
            {
                userNudgedLeft();
            }
            else if (d1 == 0x49)
            {
                userNudgedRight();
            }
            else if (d1 == 0x4a)
            {
                engine.getUIBehaviour().setBigInputMetersMode (! engine.getUIBehaviour().getBigInputMetersMode());
                updateMiscFeatures();
            }
            else if (d1 == 0x4d)
            {
                if (shiftKeysDown)
                    userPressedSaveAs();
                else
                    userPressedSave();
            }
            else if (d1 == 0x4e)
            {
                userPressedUndo();
            }
            else if (d1 == 0x4f)
            {
                userPressedRedo();
            }
            else if (d1 == 0x51)
            {
                userPressedAutomationReading();
            }
            else if (d1 == 0x50)
            {
                userPressedAutomationWriting();
            }
            else if (d1 == 0x52)
            {
                AppFunctions::resetOverloads();
            }
            else if (d1 == 0x53)
            {
                userPressedFreeze();
            }
            else if (d1 == 0x54)
            {
                userToggledLoopOnOff();
            }
            else if (d1 == 0x55)
            {
                userToggledPunchOnOff();
            }
            else if (d1 == 0x56)
            {
                userToggledClickOnOff();
            }
            else if (d1 == 0x57)
            {
                userToggledSnapOnOff();
            }
            else if (d1 == 0x58)
            {
                if (auto ed = getEdit())
                    ed->playInStopEnabled = ! ed->playInStopEnabled;
            }
            else if (d1 == 0x59)
            {
                userToggledScroll();
            }
            else if (d1 == 0x5a)
            {
                if (shiftKeysDown)
                    userToggledVideoWindow();
                else
                    userToggledSlaveOnOff();
            }
            else if (d1 == 0x64)
            {
                isZooming = ! isZooming;
                lightUpButton (deviceIndex, 0x64, isZooming);

                editFlip = false;
                lightUpButton (deviceIndex, 0x33, editFlip);
            }
            else if (d1 == 0x60)
            {
                // up
                if (isZooming)
                    userZoomedTracksIn();
                else if (editFlip)
                    selectOtherObject (SelectableClass::Relationship::moveUp, assignmentMode == PluginMode);
                else
                    userScrolledTracksDown();
            }
            else if (d1 == 0x61)
            {
                // down
                if (isZooming)
                {
                    if (shiftKeysDown)
                    {
                        if (isEditValidAndNotSafeRecording())
                            AppFunctions::zoomToSelection();
                    }
                    else
                    {
                        userZoomedTracksOut();
                    }
                }
                else if (editFlip)
                {
                    selectOtherObject (SelectableClass::Relationship::moveDown, assignmentMode == PluginMode);
                }
                else
                {
                    userScrolledTracksUp();
                }
            }
            else if (d1 == 0x62)
            {
                // left
                if (isZooming)
                    userZoomedOut();
                else if (editFlip)
                    selectOtherObject (SelectableClass::Relationship::moveLeft, assignmentMode == PluginMode);
                else
                    userScrolledTracksLeft();
            }
            else if (d1 == 0x63)
            {
                // right
                if (isZooming)
                    userZoomedIn();
                else if (editFlip)
                    selectOtherObject (SelectableClass::Relationship::moveRight, assignmentMode == PluginMode);
                else
                    userScrolledTracksRight();
            }
            else if (d1 == 0x28)
            {
                setAssignmentMode (PanMode);
            }
            else if (d1 == 0x29)
            {
                setAssignmentMode (AuxMode);
            }
            else if (d1 == 0x2a)
            {
                setAssignmentMode (PluginMode);
            }
            else if (d1 == 0x2b)
            {
                setAssignmentMode (MarkerMode);
            }
            else if (d1 == 0x2c)
            {
                if (shiftKeysDown != 0)
                {
                    owner->changePluginPreset (-1);
                }
                else if (assignmentMode == PluginMode)
                {
                    userChangedParameterBank (-(extenders.size() + 1) * 8);
                }
                else if (assignmentMode == MarkerMode)
                {
                    userChangedMarkerBank(-(extenders.size() + 1) * 8);
                }
                else if (assignmentMode == AuxMode)
                {
                    userChangedAuxBank(-1);
                    userChangedFaderBanks(0);
                }
            }
            else if (d1 == 0x2d)
            {
                if (shiftKeysDown != 0)
                {
                    owner->changePluginPreset (1);
                }
                else if (assignmentMode == PluginMode)
                {
                    userChangedParameterBank ((extenders.size() + 1) * 8);
                }
                else if (assignmentMode == MarkerMode)
                {
                    userChangedMarkerBank((extenders.size() + 1) * 8);
                }
                else if (assignmentMode == AuxMode)
                {
                    userChangedAuxBank(+1);
                    userChangedFaderBanks(0);\
                }
            }
            else if (d1 == 0x32)
            {
                flip();
            }
            else if (d1 == 0x33)
            {
                editFlip = ! editFlip;
                lightUpButton (deviceIndex, 0x33, editFlip);

                isZooming = false;
                lightUpButton (deviceIndex, 0x64, isZooming);
            }
            else if (d1 == 0x66 || d1 == 0x67)
            {
                // user switch A or B
                KeyPress key (d1 == 0x66 ? mcuFootswitch1Key
                                         : mcuFootswitch2Key);

                if (auto focused = Component::getCurrentlyFocusedComponent())
                    focused->keyPressed (key);
            }
        }
    }
    else if (d[0] == 0xf0)
    {
        if (d[1] == 0
            && d[2] == 0
            && d[3] == 0x66
            && d[4] == 0x14
            && d[5] == 0x01)
        {
            // device ready message..
            if (deviceIndex == this->deviceIdx)
            {
                initialiseDevice (true);
                updateDeviceState();
            }
            else
            {
                for (int i = 0; i < extenders.size(); ++i)
                    if (extenders[i]->getDeviceIndex() == deviceIndex)
                        extenders[i]->initialiseDevice (true);
            }
        }
    }
}

void MackieMCU::flip()
{
    flipped = ! flipped;
    lightUpButton (deviceIdx, 0x32, flipped);
    updateDeviceState();
}

static uint8 convertCharToMCUCode (juce_wchar c) noexcept
{
    if (c >= 'a' && c <= 'z')
        return (uint8) ((c - 'a') + 1);

    if (c >= 'A' && c <= 'Z')
        return (uint8) ((c - 'A') + 1);

    if (c >= '0' && c <= '9')
        return (uint8) ((c - '0') + 0x30);

    return 0x20;
}

void MackieMCU::setAssignmentText (const String& text)
{
    sendMidiCommandToController (deviceIdx, 0xb0, 0x4b, convertCharToMCUCode (text[0]));
    sendMidiCommandToController (deviceIdx, 0xb0, 0x4a, convertCharToMCUCode (text[1]));
}

void MackieMCU::setAssignmentMode (AssignmentMode newMode)
{
    if (assignmentMode != newMode)
    {
        CRASH_TRACER

        zeromem (currentDisplayChars, sizeof (currentDisplayChars));

        auxTimer->stopTimer();
        zeromem (auxBusNames, sizeof (auxBusNames));
        userMovedAuxes.clear();

        assignmentMode = newMode;
        redrawSelectedPlugin();
        startTimer (10);

        lightUpButton (deviceIdx, 0x28, assignmentMode == PanMode);
        lightUpButton (deviceIdx, 0x29, assignmentMode == AuxMode);
        lightUpButton (deviceIdx, 0x2a, assignmentMode == PluginMode);
        lightUpButton (deviceIdx, 0x2b, assignmentMode == MarkerMode);

        if (newMode == PluginMode)
        {
            setSignalMetersEnabled (false);
            clearAllDisplays();
            userChangedParameterBank (0);
        }
        else if (newMode == PanMode)
        {
            setSignalMetersEnabled (true);
            userChangedFaderBanks (0);
        }
        else if (newMode == AuxMode)
        {
            setSignalMetersEnabled (false);
            clearAllDisplays();
            userChangedAuxBank(-auxBank - 1);
            userChangedFaderBanks(0);
        }
        else if (newMode == MarkerMode)
        {
            setSignalMetersEnabled(false);
            clearAllDisplays();

            userChangedMarkerBank(0);
        }

        timerCallback();
    }
}

void MackieMCU::timerCallback()
{
    stopTimer();

    // update the assignment display..

    if (assignmentMode == PanMode)
    {
        setAssignmentText("pn");
    }
    else if (assignmentMode == PluginMode)
    {
        setAssignmentText("pl");
    }
    else if (assignmentMode == AuxMode)
    {
        if (auxBank == -1)
            setAssignmentText("Au");
        else
            setAssignmentText("a" + String (auxBank + 1));
    }
    else if (assignmentMode == MarkerMode)
    {
        setAssignmentText("mm");
    }
}

void MackieMCU::moveFaderInt (int dev, int channelNum, float newSliderPos)
{
    jassert (channelNum <= 8);

    int faderPos = jlimit (0, 0x3fff, (int)(newSliderPos * 0x3fff));

    if (std::abs ((int) lastFaderPos[dev][channelNum] - faderPos) > 2)
    {
        lastFaderPos[dev][channelNum] = (uint32) faderPos;

        sendMidiCommandToController (dev,
                                     (uint8) (0xe0 + channelNum),
                                     (uint8) (faderPos & 0x7f),
                                     (uint8) (faderPos >> 7));
    }
}

void MackieMCU::moveFader (int channelNum_, float newSliderPos)
{
    int channelNum = channelNum_ % 8;
    int dev        = channelNum_ / 8;

    if (channelNum < 8)
    {
        if (flipped)
            movePanPotInt (dev, channelNum, newSliderPos * 2.0f - 1.0f);
        else
            moveFaderInt (dev, channelNum, newSliderPos);
    }
}

void MackieMCU::moveMasterLevelFader (float newLeftSliderPos, float newRightSliderPos)
{
    moveFaderInt (deviceIdx, 8, (newLeftSliderPos + newRightSliderPos) * 0.5f);
}

void MackieMCU::movePanPotInt (int dev, int channelNum, float newPan)
{
    jassert (channelNum <= 7);
    panPos [dev * 8 + channelNum] = newPan;

    sendMidiCommandToController (dev, 0xb0, (uint8) (0x30 + channelNum),
                                 (uint8) jlimit (0x01, 0x0b, 6 + roundToInt (5 * newPan)));
}

void MackieMCU::movePanPot (int channelNum_, float newPan)
{
    int channelNum = channelNum_ % 8;
    int dev        = channelNum_ / 8;

    if (flipped)
        moveFaderInt (dev, channelNum, (newPan + 1.0f) * 0.5f);
    else if (assignmentMode == PanMode)
        movePanPotInt (dev, channelNum, newPan);
}

juce::String MackieMCU::auxString (int chan) const
{
    return Decibels::toString (volumeFaderPositionToDB (auxLevels[chan]), 1, -96.0f);
}

void MackieMCU::moveAux (int channelNum_, const char* bus, float newPos)
{
    int channelNum = channelNum_ % 8;
    int dev        = channelNum_ / 8;

    if ((auxLevels[channelNum_] != newPos || strcmp (bus, auxBusNames[channelNum_]) != 0)
         && channelNum >= 0 && channelNum < 8)
    {
        auxLevels[channelNum_] = newPos;
        strcpy (auxBusNames[channelNum_], bus);

        if (assignmentMode == AuxMode)
        {
            if (flipped)
                moveFaderInt (dev, channelNum, newPos);
            else
                movePanPotInt (dev, channelNum, newPos * 2.0f - 1.0f);

            if (userMovedAuxes.contains (channelNum_))
                setDisplaySegment (dev, channelNum, 1, auxString (channelNum_));
            else
                setDisplaySegment (dev, channelNum, 1, String (bus));
        }
    }
}

void MackieMCU::clearAux (int channel_)
{
    int channel = channel_ % 8;
    int dev     = channel_ / 8;

    if (assignmentMode == AuxMode)
    {
        clearDisplaySegment (dev, channel, 1);

        if (flipped)
            moveFaderInt (dev, channel, 0);
        else
            movePanPotInt (dev, channel, 0);
    }
}

void MackieMCU::lightUpButton (int dev, int buttonNum, bool on)
{
    CRASH_TRACER
    sendMidiCommandToController (dev, 0x90, (uint8) buttonNum, on ? (uint8) 0x7f : (uint8) 0);
}

void MackieMCU::updateSoloAndMute (int channelNum, Track::MuteAndSoloLightState state, bool isBright)
{
    lightUpButton (channelNum / 8, 0x08 + channelNum % 8, (state & Track::soloLit) != 0 || (isBright && (state & Track::soloFlashing) != 0));
    lightUpButton (channelNum / 8, 0x10 + channelNum % 8, (state & Track::muteLit) != 0 || (isBright && (state & Track::muteFlashing) != 0));
}

void MackieMCU::soloCountChanged (bool anySoloTracks)
{
    // (rude solo light)
    lightUpButton (deviceIdx, 0x73, anySoloTracks);
}

void MackieMCU::trackSelectionChanged (int channel, bool isSelected)
{
    lightUpButton (channel / 8, 0x18 + channel % 8, isSelected);
}

void MackieMCU::playStateChanged (bool isPlaying)
{
    lightUpButton (deviceIdx, 0x5e, isPlaying);
    lightUpButton (deviceIdx, 0x5d, ! isPlaying);
}

void MackieMCU::recordStateChanged (bool isRecording)
{
    lightUpButton (deviceIdx, 0x5f, isRecording);

    if (isRecording)
        lightUpButton (deviceIdx, 0x5d, false);
}

void MackieMCU::loopOnOffChanged (bool isLoopOn)
{
    lightUpButton (deviceIdx, 0x54, isLoopOn);
}

void MackieMCU::punchOnOffChanged (bool isPunching)
{
    lightUpButton (deviceIdx, 0x55, isPunching);
}

void MackieMCU::clickOnOffChanged (bool isClickOn)
{
    lightUpButton (deviceIdx, 0x56, isClickOn);
}

void MackieMCU::snapOnOffChanged (bool isSnapOn)
{
    lightUpButton (deviceIdx, 0x57, isSnapOn);
}

void MackieMCU::slaveOnOffChanged (bool isSlaving)
{
    lightUpButton (deviceIdx, 0x5a, isSlaving);
}

void MackieMCU::automationReadModeChanged (bool isReading)
{
    lightUpButton (deviceIdx, 0x51, isReading);
}

void MackieMCU::automationWriteModeChanged (bool isWriting)
{
    lightUpButton (deviceIdx, 0x50, isWriting);
}

void MackieMCU::parameterChanged (int parameterNumber_, const ParameterSetting& newValue)
{
    int parameterNumber = parameterNumber_ % 8;
    int dev             = parameterNumber_ / 8;

    if (assignmentMode == PluginMode)
    {
        centreDisplaySegment (dev, parameterNumber, 0, String::fromUTF8 (newValue.label));
        centreDisplaySegment (dev, parameterNumber, 1, String::fromUTF8 (newValue.valueDescription));

        if (flipped)
            moveFaderInt (dev, parameterNumber, newValue.value);
        else
            movePanPotInt (dev, parameterNumber, newValue.value * 2.0f - 1.0f);
    }
}

void MackieMCU::clearParameter (int parameterNumber_)
{
    int parameterNumber = parameterNumber_ % 8;
    int dev             = parameterNumber_ / 8;

    if (assignmentMode == PluginMode)
    {
        clearDisplaySegment (dev, parameterNumber, 0);
        clearDisplaySegment (dev, parameterNumber, 1);

        if (flipped)
            moveFaderInt (dev, parameterNumber, 0);
        else
            movePanPotInt (dev, parameterNumber, 0);
    }
}

void MackieMCU::faderBankChanged (int newStartChannelNumber, const StringArray& trackNames)
{
    if (assignmentMode == PanMode || assignmentMode == AuxMode)
    {
        if (newStartChannelNumber != lastStartChan)
        {
            lastStartChan = newStartChannelNumber;

            ++newStartChannelNumber;

            sendMidiCommandToController (deviceIdx, 0xb0, 0x4a, (uint8) (0x30 + newStartChannelNumber % 10));
            sendMidiCommandToController (deviceIdx, 0xb0, 0x4b, (uint8) (0x30 + newStartChannelNumber / 10));
        }

        clearAllDisplays();

        for (int i = 0; i < extenders.size() + 1; ++i)
        {
            for (int j = 0; j < 8; ++j)
                centreDisplaySegment (i, j, 0, trackNames[j + i * 8]);

            if (assignmentMode != PanMode)
                for (int j = 0; j < 8; j++)
                    setDisplaySegment (i, j, 1, auxString (j + i * 8));
        }

        startTimer (1500);
    }
}

void MackieMCU::channelLevelChanged (int channelNum_, float level)
{
    int channel = channelNum_ % 8;
    int dev     = channelNum_ / 8;

    if (assignmentMode == PanMode)
    {
        const uint8 newValue = (uint8) jlimit (0, 13, roundToInt (13.0f * level));

        if (lastChannelLevels[channelNum_] != newValue)
        {
            lastChannelLevels[channelNum_] = newValue;

            unsigned char d[4];
            d[0] = 0xd0;
            d[1] = (uint8) ((channel << 4) | newValue);

            sendMidiCommandToController (dev, d, 2);
        }
    }
}

void MackieMCU::masterLevelsChanged (float, float)
{
}

void MackieMCU::updateTCDisplay (const char* newDigits)
{
    for (int i = 0; i < numElementsInArray (timecodeDigits); ++i)
    {
        const char c = newDigits[i];

        if (timecodeDigits[i] != c)
        {
            timecodeDigits[i] = c;
            sendMidiCommandToController (deviceIdx, (uint8) 0xb0, (uint8) (0x49 - i), convertCharToMCUCode (c));
        }
    }
}

void MackieMCU::timecodeChanged (int barsOrHours,
                                 int beatsOrMinutes,
                                 int ticksOrSeconds,
                                 int millisecs,
                                 bool isBarsBeats,
                                 bool isFrames)
{
    char d[16];

    if (isBarsBeats)
        sprintf (d, sizeof (d), "%3d%02d  %03d", barsOrHours, beatsOrMinutes, ticksOrSeconds);
    else if (isFrames)
        sprintf (d, sizeof (d), " %02d%02d%02d %02d", barsOrHours, beatsOrMinutes, ticksOrSeconds, millisecs);
    else
        sprintf (d, sizeof (d), " %02d%02d%02d%03d", barsOrHours, beatsOrMinutes, ticksOrSeconds, millisecs);

    updateTCDisplay (d);

    if (lastSmpteBeatsSetting != isBarsBeats)
    {
        lastSmpteBeatsSetting = isBarsBeats;

        lightUpButton (deviceIdx, 0x71, ! isBarsBeats);
        lightUpButton (deviceIdx, 0x72, isBarsBeats);
    }
}

void MackieMCU::trackRecordEnabled (int channel, bool isEnabled)
{
    if (recLight[channel] != isEnabled)
    {
        recLight[channel] = isEnabled;
        lightUpButton (channel / 8, channel % 8, isEnabled);
    }
}

bool MackieMCU::showingPluginParams()
{
    return assignmentMode == PluginMode;
}

bool MackieMCU::showingMarkers()
{
    return assignmentMode == MarkerMode;
}

bool MackieMCU::showingTracks()
{
    return true;
}

void MackieMCU::registerXT (MackieXT* xt)
{
    extenders.add(xt);
    numberOfFaderChannels = 8 * (extenders.size() + 1);
    numParameterControls  = 8 * (extenders.size() + 1);
    numMarkers            = 8 * (extenders.size() + 1);
}

void MackieMCU::unregisterXT (MackieXT* xt)
{
    extenders.removeAllInstancesOf (xt);
    numberOfFaderChannels = 8 * (extenders.size() + 1);
    numParameterControls  = 8 * (extenders.size() + 1);
}

void MackieMCU::sendMidiCommandToController (int devIdx, uint8 byte1, uint8 byte2, uint8 byte3)
{
    uint8 bytes[] = { byte1, byte2, byte3 };
    sendMidiCommandToController (devIdx, bytes, 3);
}

void MackieMCU::sendMidiCommandToController (int devIdx, const unsigned char* midiData, int numBytes)
{
    if (devIdx == deviceIdx)
    {
        ControlSurface::sendMidiCommandToController (midiData, numBytes);
    }
    else
    {
        for (int i = 0; i < extenders.size(); ++i)
        {
            if (extenders[i]->getDeviceIndex() == devIdx)
            {
                extenders[i]->sendMidiCommandToController (midiData, numBytes);
                break;
            }
        }
    }
}

void MackieMCU::clearAllDisplays()
{
    CRASH_TRACER
    setDisplay (deviceIdx, "", "");

    for (auto& e : extenders)
        setDisplay (e->getDeviceIndex(), "", "");
}

void MackieMCU::markerChanged (int parameterNumber_, const MarkerSetting& newValue)
{
    int parameterNumber = parameterNumber_ % 8;
    int dev             = parameterNumber_ / 8;

    if (assignmentMode == MarkerMode)
    {
        setDisplaySegment (dev, parameterNumber, 0, String (newValue.number));
        setDisplaySegment (dev, parameterNumber, 1, String::fromUTF8 (newValue.label));

        if (flipped)
            moveFaderInt (dev, parameterNumber, 0);
        else
            movePanPotInt (dev, parameterNumber, 0);
    }
}

void MackieMCU::clearMarker (int parameterNumber_)
{
    int parameterNumber = parameterNumber_ % 8;
    int dev             = parameterNumber_ / 8;

    if (assignmentMode == MarkerMode)
    {
        clearDisplaySegment (dev, parameterNumber, 0);
        clearDisplaySegment (dev, parameterNumber, 1);

        if (flipped)
            moveFaderInt (dev, parameterNumber, 0);
        else
            movePanPotInt (dev, parameterNumber, 0);
    }
}

void MackieMCU::auxBankChanged (int bank)
{
    for (int i = 0; i < maxNumChannels; ++i)
        auxLevels[i] = -1000.0f;

    auxTimer->stopTimer();
    zeromem (auxBusNames, sizeof (auxBusNames));
    userMovedAuxes.clear();

    auxBank = bank;
    startTimer(10);
}

void MackieMCU::undoStatusChanged (bool undo, bool redo)
{
    lightUpButton (deviceIdx, 0x4e, undo);
    lightUpButton (deviceIdx, 0x4f, redo);
}
