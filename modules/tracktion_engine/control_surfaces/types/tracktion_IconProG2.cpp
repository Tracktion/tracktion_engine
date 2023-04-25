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

IconProG2::IconProG2 (ExternalControllerManager& ecm)  : MackieMCU (ecm)
{
    deviceDescription = "Icon Pro G2";
}

IconProG2::~IconProG2()
{
}

void IconProG2::acceptMidiMessageInt (int deviceIndex, const juce::MidiMessage& m)
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

            panPos[chan] = juce::jlimit (-1.0f, 1.0f, panPos[chan] + diff);

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
        const float pos = juce::jlimit (0.0f, 1.0f, ((((int)d[2]) << 7) + d[1]) * (1.0f / 0x3f70));

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
            userChangedRewindButton (d[2] != 0);
            lightUpButton (deviceIndex, 0x5b, d[2] != 0);
        }
        else if (d1 == 0x5c)
        {
            userChangedFastForwardButton (d[2] != 0);
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
            else if (d1 == 0x32)
            {
                flip();
            }
            else if (d1 == 0x34)
            {
                userToggledMixerWindow (false);
            }
            else if (d1 == 0x35)
            {
                userToggledMidiEditorWindow (false);
            }
            else if (d1 >= 0x36 && d1 <= 0x44)
            {
                userPressedUserAction (d1 - 0x36);
            }
            else if (d1 == 0x44)
            {
                userPressedUserAction (15);
            }
            else if (d1 == 0x46)
            {
                userToggledTrackEditorWindow (shiftKeysDown != 0);
            }
            else if (d1 == 0x47)
            {
                userToggledBrowserWindow();
            }
            else if (d1 == 0x48)
            {
                userToggledActionsWindow();
            }
            else if (d1 == 0x4a)
            {
                setAssignmentMode (PanMode);
            }
            else if (d1 == 0x4b)
            {
                setAssignmentMode (AuxMode);
            }
            else if (d1 == 0x4c)
            {
                userToggledClickOnOff();
            }
            else if (d1 == 0x4d)
            {
                userPressedFreeze();
            }
            else if (d1 == 0x4e)
            {
                userToggledSnapOnOff();
            }
            else if (d1 == 0x4f)
            {
                userToggledScroll();
            }
            else if (d1 == 0x50)
            {
                userToggledPunchOnOff();
            }
            else if (d1 == 0x51)
            {
                userPressedAutomationReading();
            }
            else if (d1 == 0x52)
            {
                userPressedSave();
            }
            else if (d1 == 0x53)
            {
                userPressedAutomationWriting();
            }
            else if (d1 == 0x54)
            {
                userPressedUndo();
            }
            else if (d1 == 0x55)
            {
                userPressedRedo();
            }
            else if (d1 == 0x56)
            {
                userToggledLoopOnOff();
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
                userPressedMarkIn();
            }
            else if (d1 == 0x29)
            {
                userPressedMarkOut();
            }
            else if (d1 == 0x2a)
            {
                userPressedCut();
            }
            else if (d1 == 0x2b)
            {
                userPressedCopy();
            }
            else if (d1 == 0x2c)
            {
                userPressedPaste (false);
            }
            else if (d1 == 0x2d)
            {
                userPressedDelete (false);
            }
            else if (d1 == 0x66 || d1 == 0x67)
            {
                // user switch A or B
                juce::KeyPress key (d1 == 0x66 ? mcuFootswitch1Key
                                               : mcuFootswitch2Key);

                if (auto focused = juce::Component::getCurrentlyFocusedComponent())
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

void IconProG2::loopOnOffChanged (bool isLoopOn)
{
    lightUpButton (deviceIdx, 0x56, isLoopOn);
}

void IconProG2::punchOnOffChanged (bool isPunching)
{
    lightUpButton (deviceIdx, 0x50, isPunching);
}

void IconProG2::clickOnOffChanged (bool isClickOn)
{
    lightUpButton (deviceIdx, 0x4c, isClickOn);
}

void IconProG2::snapOnOffChanged (bool isSnapOn)
{
    lightUpButton (deviceIdx, 0x4e, isSnapOn);
}

void IconProG2::slaveOnOffChanged (bool)
{
}

void IconProG2::scrollOnOffChanged (bool isScroll)
{
    lightUpButton (deviceIdx, 0x4f, isScroll);
}

void IconProG2::automationReadModeChanged (bool isReading)
{
    lightUpButton (deviceIdx, 0x51, isReading);
}

void IconProG2::automationWriteModeChanged (bool isWriting)
{
    lightUpButton (deviceIdx, 0x53, isWriting);
}

void IconProG2::undoStatusChanged (bool undo, bool redo)
{
    lightUpButton (deviceIdx, 0x54, undo);
    lightUpButton (deviceIdx, 0x55, redo);
}

void IconProG2::setAssignmentMode (AssignmentMode newMode)
{
    if (assignmentMode != newMode)
    {
        CRASH_TRACER

        std::memset (currentDisplayChars, 0, sizeof (currentDisplayChars));

        auxTimer->stopTimer();
        std::memset (auxBusNames, 0, sizeof (auxBusNames));
        userMovedAuxes.clear();

        assignmentMode = newMode;
        redrawSelectedPlugin();
        startTimer (10);

        lightUpButton (deviceIdx, 0x4a, assignmentMode == PanMode);
        lightUpButton (deviceIdx, 0x4b, assignmentMode == AuxMode);

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
            userChangedAuxBank (-auxBank - 1);
            userChangedFaderBanks (0);
        }
        else if (newMode == MarkerMode)
        {
            setSignalMetersEnabled (false);
            clearAllDisplays();

            userChangedMarkerBank (0);
        }

        timerCallback();
    }
}

}} // namespace tracktion { inline namespace engine
