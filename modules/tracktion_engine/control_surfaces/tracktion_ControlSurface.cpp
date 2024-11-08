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

namespace control_surface_utils
{
inline void flipEndToEndIfNotAuto (InputDevice& in)
{
    switch (in.getMonitorMode())
    {
        case InputDevice::MonitorMode::automatic:
            return;
        case InputDevice::MonitorMode::on:
            in.setMonitorMode (InputDevice::MonitorMode::off);
        break;
        case InputDevice::MonitorMode::off:
            in.setMonitorMode (InputDevice::MonitorMode::on);
        return;
    }
}
}

ParameterSetting::ParameterSetting() noexcept
{
    clear();
}

void ParameterSetting::clear() noexcept
{
    label[0] = 0;
    valueDescription[0] = 0;
    value = 0.0f;
}

MarkerSetting::MarkerSetting() noexcept
{
    clear();
}

void MarkerSetting::clear() noexcept
{
    label[0] = 0;
    number = 0;
    absolute = 0;
}

//==============================================================================
ControlSurface::ControlSurface (ExternalControllerManager& ecm)
    : engine (ecm.engine), externalControllerManager (ecm)
{
}

ControlSurface::~ControlSurface()
{
    notifyListenersOfDeletion();
}

juce::String ControlSurface::getSelectableDescription()
{
    return deviceDescription;
}

Edit* ControlSurface::getEditIfOnEditScreen() const
{
    if (engine.getUIBehaviour().getCurrentlyFocusedEdit() == getEdit())
        return getEdit();

    return {};
}

void ControlSurface::sendMidiCommandToController (int idx, const void* midiData, int numBytes)
{
    sendMidiCommandToController (idx, juce::MidiMessage (midiData, numBytes));
}

void ControlSurface::sendMidiCommandToController (int idx, const juce::MidiMessage& m)
{
    if (owner != nullptr)
        if (idx >= 0 && idx < (int) owner->outputDevices.size())
            if (auto dev = owner->outputDevices[(size_t) idx])
                dev->fireMessage (m);
}

bool ControlSurface::isSafeRecording() const
{
    return edit != nullptr && edit->getTransport().isSafeRecording();
}

int ControlSurface::getMarkerBankOffset() const { return owner->getMarkerBankOffset();  }
int ControlSurface::getFaderBankOffset() const  { return owner->getFaderBankOffset();   }
int ControlSurface::getAuxBankOffset() const    { return owner->getAuxBankOffset();     }
int ControlSurface::getParamBankOffset() const  { return owner->getParamBankOffset();   }
int ControlSurface::getClipSlotOffset() const   { return owner->getClipSlotOffset();    }

#define RETURN_IF_SAFE_RECORDING  if (isSafeRecording()) return;

void ControlSurface::performIfNotSafeRecording (const std::function<void()>& f)
{
    RETURN_IF_SAFE_RECORDING
    f();
}

void ControlSurface::userMovedFader (int channelNum, float newSliderPos, bool delta)
{
    RETURN_IF_SAFE_RECORDING
    if (delta || pickedUp (ctrlFader, channelNum, newSliderPos))
        externalControllerManager.userMovedFader (owner->channelStart + channelNum, newSliderPos, delta);
}

void ControlSurface::userMovedMasterLevelFader (float newLevel, bool delta)
{
    RETURN_IF_SAFE_RECORDING
    if (delta || pickedUp (ctrlMasterFader, newLevel))
        externalControllerManager.userMovedMasterFader (getEdit(), newLevel, delta);
}

void ControlSurface::userMovedMasterPanPot (float newPan, bool delta)
{
    RETURN_IF_SAFE_RECORDING
    if (delta || pickedUp (ctrlMasterPanPot, newPan))
        externalControllerManager.userMovedMasterPanPot (getEdit(), newPan, delta);
}

void ControlSurface::userMovedQuickParam (float newLevel)
{
    RETURN_IF_SAFE_RECORDING
    externalControllerManager.userMovedQuickParam(newLevel);
}

void ControlSurface::userMovedPanPot (int channelNum, float newPan, bool delta)
{
    RETURN_IF_SAFE_RECORDING
    if (delta || pickedUp (ctrlPan, channelNum, newPan))
        externalControllerManager.userMovedPanPot (owner->channelStart + channelNum, newPan, delta);
}

void ControlSurface::userMovedAux (int channelNum, int auxNum, float newPosition, bool delta)
{
    RETURN_IF_SAFE_RECORDING
    if (delta || pickedUp (ctrlAux, channelNum, newPosition))
        externalControllerManager.userMovedAux (owner->channelStart + channelNum, owner->auxBank + auxNum, auxMode, newPosition, delta);
}

void ControlSurface::userPressedAux (int channelNum, int auxNum)
{
    RETURN_IF_SAFE_RECORDING
    externalControllerManager.userPressedAux (owner->channelStart + channelNum, owner->auxBank + auxNum);
}

void ControlSurface::userPressedSolo (int channelNum)
{
    RETURN_IF_SAFE_RECORDING
    externalControllerManager.userPressedSolo (owner->channelStart + channelNum);
}

void ControlSurface::userPressedSoloIsolate (int channelNum)
{
    RETURN_IF_SAFE_RECORDING
    externalControllerManager.userPressedSoloIsolate (owner->channelStart + channelNum);
}

void ControlSurface::userPressedMute (int channelNum, bool muteVolumeControl)
{
    RETURN_IF_SAFE_RECORDING
    externalControllerManager.userPressedMute (owner->channelStart + channelNum, muteVolumeControl);
}

void ControlSurface::userSelectedTrack (int channelNum)
{
    RETURN_IF_SAFE_RECORDING
    externalControllerManager.userSelectedTrack (owner->channelStart + channelNum);
}

void ControlSurface::userSelectedOneTrack (int channelNum)
{
    RETURN_IF_SAFE_RECORDING
    externalControllerManager.userSelectedOneTrack (owner->channelStart + channelNum);
}

void ControlSurface::userSelectedClipInTrack (int channelNum)
{
    RETURN_IF_SAFE_RECORDING
    externalControllerManager.userSelectedClipInTrack (owner->channelStart + channelNum);
}

void ControlSurface::userSelectedPluginInTrack (int channelNum)
{
    RETURN_IF_SAFE_RECORDING
    externalControllerManager.userSelectedPluginInTrack (owner->channelStart + channelNum);
}

void ControlSurface::userPressedClearAllSolo()
{
    RETURN_IF_SAFE_RECORDING

    if (auto ed = getEditIfOnEditScreen())
        for (auto t : getAllTracks (*ed))
            t->setSolo (false);
}

void ControlSurface::userPressedClearAllMute()
{
    RETURN_IF_SAFE_RECORDING

    if (auto ed = getEditIfOnEditScreen())
        for (auto t : getAllTracks (*ed))
            t->setMute (false);
}

void ControlSurface::userPressedRecEnable (int channelNum, bool enableEtoE)
{
    RETURN_IF_SAFE_RECORDING

    if (auto ed = getEditIfOnEditScreen())
    {
        channelNum += owner->channelStart;

        if (auto track = externalControllerManager.getChannelTrack (channelNum))
        {
            juce::Array<InputDeviceInstance*> activeDev, inactiveDev;

            for (auto in : ed->getAllInputDevices())
            {
                if (in->getTargets().contains (track->itemID))
                {
                    if (in->isRecordingActive (track->itemID))
                        activeDev.add (in);
                    else
                        inactiveDev.add (in);
                }
            }

            if (enableEtoE)
            {
                for (auto dev : activeDev)
                    control_surface_utils::flipEndToEndIfNotAuto (dev->owner);

                for (auto dev : inactiveDev)
                    control_surface_utils::flipEndToEndIfNotAuto (dev->owner);
            }
            else
            {
                if (activeDev.size() > 0)
                {
                    for (auto dev : activeDev)
                        dev->setRecordingEnabled (track->itemID, false);
                }
                else
                {
                    for (auto dev : inactiveDev)
                        dev->setRecordingEnabled (track->itemID, true);
                }

                if (activeDev.size() > 0 || inactiveDev.size() > 0)
                    ed->restartPlayback();
            }
        }
    }
}

void ControlSurface::userLaunchedClip (int channelNum, int sceneNum, bool press)
{
    RETURN_IF_SAFE_RECORDING

    recentlyPressedPads.insert ({owner->channelStart + channelNum, owner->padStart + sceneNum});

    externalControllerManager.userLaunchedClip (owner->channelStart + channelNum, owner->padStart + sceneNum, press);
    externalControllerManager.updatePadColours();
}

void ControlSurface::userStoppedClip (int channelNum, bool press)
{
    RETURN_IF_SAFE_RECORDING

    externalControllerManager.userStoppedClip (owner->channelStart + channelNum, press);
}

void ControlSurface::userLaunchedScene (int sceneNum, bool press)
{
    RETURN_IF_SAFE_RECORDING

    externalControllerManager.userLaunchedScene (owner->padStart + sceneNum, press);
}

void ControlSurface::userPressedHome()         { performIfNotSafeRecording (&AppFunctions::goToStart); }
void ControlSurface::userPressedEnd()          { performIfNotSafeRecording (&AppFunctions::goToEnd); }
void ControlSurface::userPressedMarkIn()
{
    RETURN_IF_SAFE_RECORDING
    AppFunctions::markIn (true);
}
void ControlSurface::userPressedMarkOut()
{
    RETURN_IF_SAFE_RECORDING
    AppFunctions::markOut (true);
}
void ControlSurface::userPressedPlay()         { performIfNotSafeRecording (&AppFunctions::startStopPlay); }
void ControlSurface::userPressedRecord()       { performIfNotSafeRecording (&AppFunctions::record); }

void ControlSurface::userPressedStop()
{
    RETURN_IF_SAFE_RECORDING

    if (auto tc = getTransport())
    {
        if (tc->isPlaying() || tc->isRecording())
            tc->stop (false, false);
        else
            tc->setPosition (0s);
    }
}

void ControlSurface::userPressedAutomationReading()
{
    RETURN_IF_SAFE_RECORDING

    if (auto e = getEdit())
        e->getAutomationRecordManager().setReadingAutomation (! e->getAutomationRecordManager().isReadingAutomation());
}

void ControlSurface::userPressedAutomationWriting()
{
    RETURN_IF_SAFE_RECORDING

    if (auto e = getEdit())
        e->getAutomationRecordManager().toggleWriteAutomationMode();
}

void ControlSurface::userToggledBeatsSecondsMode()
{
    performIfNotSafeRecording (&AppFunctions::toggleTimecode);
}

void ControlSurface::userChangedFaderBanks (int channelNumDelta)
{
    RETURN_IF_SAFE_RECORDING
    owner->changeFaderBank (channelNumDelta, followsTrackSelection);
}

void ControlSurface::userChangedPadBanks (int padDelta)
{
    RETURN_IF_SAFE_RECORDING
    owner->changePadBank (padDelta);
}

void ControlSurface::userChangedAuxBank (int delta)
{
    RETURN_IF_SAFE_RECORDING
    owner->changeAuxBank (delta);
}

void ControlSurface::userSetAuxBank (int num)
{
    RETURN_IF_SAFE_RECORDING
    owner->setAuxBank (num);
}

void ControlSurface::userToggledLoopOnOff()            { performIfNotSafeRecording (&AppFunctions::toggleLoop); }
void ControlSurface::userToggledPunchOnOff()           { performIfNotSafeRecording (&AppFunctions::togglePunch); }
void ControlSurface::userToggledSnapOnOff()            { performIfNotSafeRecording (&AppFunctions::toggleSnap); }
void ControlSurface::userToggledClickOnOff()           { performIfNotSafeRecording (&AppFunctions::toggleClick); }
void ControlSurface::userToggledSlaveOnOff()           { performIfNotSafeRecording (&AppFunctions::toggleMidiChase); }
void ControlSurface::userSkippedToNextMarkerLeft()     { performIfNotSafeRecording (&AppFunctions::tabBack); }
void ControlSurface::userSkippedToNextMarkerRight()    { performIfNotSafeRecording (&AppFunctions::tabForward); }
void ControlSurface::userNudgedLeft()                  { performIfNotSafeRecording (&AppFunctions::nudgeLeft); }
void ControlSurface::userNudgedRight()                 { performIfNotSafeRecording (&AppFunctions::nudgeRight); }
void ControlSurface::userZoomedIn()                    { performIfNotSafeRecording (&AppFunctions::zoomIn); }
void ControlSurface::userZoomedOut()                   { performIfNotSafeRecording (&AppFunctions::zoomOut); }
void ControlSurface::userScrolledTracksUp()            { performIfNotSafeRecording (&AppFunctions::scrollTracksUp); }
void ControlSurface::userScrolledTracksDown()          { performIfNotSafeRecording (&AppFunctions::scrollTracksDown); }
void ControlSurface::userScrolledTracksLeft()          { performIfNotSafeRecording (&AppFunctions::scrollTracksLeft); }
void ControlSurface::userScrolledTracksRight()         { performIfNotSafeRecording (&AppFunctions::scrollTracksRight); }
void ControlSurface::userZoomedTracksIn()              { performIfNotSafeRecording (&AppFunctions::zoomTracksIn); }
void ControlSurface::userZoomedTracksOut()             { performIfNotSafeRecording (&AppFunctions::zoomTracksOut); }

void ControlSurface::userChangedRewindButton (bool isButtonDown)
{
    RETURN_IF_SAFE_RECORDING

    if (auto tc = getTransport())
        tc->setRewindButtonDown (isButtonDown);
}

void ControlSurface::userChangedFastForwardButton (bool isButtonDown)
{
    RETURN_IF_SAFE_RECORDING

    if (auto tc = getTransport())
        tc->setFastForwardButtonDown (isButtonDown);
}

void ControlSurface::userMovedJogWheel (float amount)
{
    RETURN_IF_SAFE_RECORDING

    if (auto tc = getTransport())
        scrub (*tc, amount);
}

void ControlSurface::userMovedParameterControl (int parameter, float newValue, bool delta)
{
    RETURN_IF_SAFE_RECORDING
    if (delta || pickedUp (ctrlParam, parameter, newValue))
        owner->userMovedParameterControl (parameter, newValue, delta);
}

void ControlSurface::userPressedParameterControl (int paramNumber)
{
    RETURN_IF_SAFE_RECORDING
    owner->userPressedParameterControl (paramNumber);
}

void ControlSurface::userPressedGoToMarker(int marker)
{
    RETURN_IF_SAFE_RECORDING
    owner->userPressedGoToMarker (marker);
}

void ControlSurface::selectOtherObject (SelectableClass::Relationship relationship, bool moveFromCurrentPlugin)
{
    owner->selectOtherObject (relationship, moveFromCurrentPlugin);
}

void ControlSurface::muteOrUnmutePluginsInTrack()
{
    owner->muteOrUnmutePluginsInTrack();
}

void ControlSurface::userChangedParameterBank (int deltaParams)
{
    RETURN_IF_SAFE_RECORDING
    owner->changeParamBank (deltaParams);
}

void ControlSurface::userChangedMarkerBank (int deltaMarkers)
{
    RETURN_IF_SAFE_RECORDING
    owner->changeMarkerBank (deltaMarkers);
}

void ControlSurface::updateDeviceState()
{
    owner->updateDeviceState();
    owner->changeParamBank (0);
}

void ControlSurface::userToggledEtoE()             { performIfNotSafeRecording (&AppFunctions::toggleEndToEnd); }
void ControlSurface::userPressedSave()             { performIfNotSafeRecording (&AppFunctions::saveEdit); }
void ControlSurface::userPressedSaveAs()           { performIfNotSafeRecording (&AppFunctions::saveEditAs); }
void ControlSurface::userPressedArmAll()           { performIfNotSafeRecording (&AppFunctions::armOrDisarmAllInputs); }
void ControlSurface::userPressedJumpToMarkIn()     { performIfNotSafeRecording (&AppFunctions::goToMarkIn); }
void ControlSurface::userPressedJumpToMarkOut()    { performIfNotSafeRecording (&AppFunctions::goToMarkOut); }
void ControlSurface::userPressedZoomIn()           { performIfNotSafeRecording (&AppFunctions::zoomIn); }
void ControlSurface::userPressedZoomOut()          { performIfNotSafeRecording (&AppFunctions::zoomOut); }
void ControlSurface::userPressedZoomToFit()        { performIfNotSafeRecording (&AppFunctions::zoomToFitHorizontally); }

void ControlSurface::userPressedCreateMarker()
{
    RETURN_IF_SAFE_RECORDING

    if (auto ed = getEditIfOnEditScreen())
        ed->getMarkerManager().createMarker (-1, ed->getTransport().getPosition(), {}, externalControllerManager.getSelectionManager());
}

void ControlSurface::userPressedNextMarker()       { performIfNotSafeRecording (&AppFunctions::moveToNextMarker); }
void ControlSurface::userPressedPreviousMarker()   { performIfNotSafeRecording (&AppFunctions::moveToPrevMarker); }
void ControlSurface::userPressedRedo()             { performIfNotSafeRecording (&AppFunctions::redo); }
void ControlSurface::userPressedUndo()             { performIfNotSafeRecording (&AppFunctions::undo); }
void ControlSurface::userToggledScroll()           { performIfNotSafeRecording (&AppFunctions::toggleScroll); }
void ControlSurface::userPressedAbort()            { performIfNotSafeRecording (&AppFunctions::stopRecordingAndDiscard); }
void ControlSurface::userPressedAbortRestart()     { performIfNotSafeRecording (&AppFunctions::stopRecordingAndRestart); }
void ControlSurface::userPressedCut()              { performIfNotSafeRecording (&AppFunctions::cut); }
void ControlSurface::userPressedCopy()             { performIfNotSafeRecording (&AppFunctions::copy); }
void ControlSurface::userPressedFreeze()           { performIfNotSafeRecording (&AppFunctions::toggleTrackFreeze); }

void ControlSurface::userPressedPaste (bool insert)
{
    performIfNotSafeRecording (insert ? &AppFunctions::insertPaste
                                      : &AppFunctions::paste);
}

void ControlSurface::userPressedDelete (bool marked)
{
    performIfNotSafeRecording (marked ? &AppFunctions::deleteRegion
                                      : &AppFunctions::deleteSelected);
}

void ControlSurface::userPressedZoomFitToTracks()       { performIfNotSafeRecording (&AppFunctions::zoomToFitVertically); }
void ControlSurface::userPressedInsertTempoChange()     { performIfNotSafeRecording (&AppFunctions::insertTempoChange); }
void ControlSurface::userPressedInsertPitchChange()     { performIfNotSafeRecording (&AppFunctions::insertPitchChange); }
void ControlSurface::userPressedInsertTimeSigChange()   { performIfNotSafeRecording (&AppFunctions::insertTimeSigChange); }

void ControlSurface::userToggledVideoWindow()           { performIfNotSafeRecording (&AppFunctions::showHideVideo); }
void ControlSurface::userToggledMixerWindow (bool fs)
{
    RETURN_IF_SAFE_RECORDING
    AppFunctions::showHideMixer (fs);
}
void ControlSurface::userToggledMidiEditorWindow (bool fs)
{
    RETURN_IF_SAFE_RECORDING
    AppFunctions::showHideMidiEditor (fs);
}
void ControlSurface::userToggledTrackEditorWindow (bool zoom)
{
    RETURN_IF_SAFE_RECORDING
    AppFunctions::showHideTrackEditor (zoom);
}
void ControlSurface::userToggledBrowserWindow()         { performIfNotSafeRecording (&AppFunctions::showHideBrowser); }
void ControlSurface::userToggledActionsWindow()         { performIfNotSafeRecording (&AppFunctions::showHideActions); }
void ControlSurface::userPressedUserAction (int action)
{
    RETURN_IF_SAFE_RECORDING
    AppFunctions::performUserAction (action);
}

void ControlSurface::redrawSelectedPlugin()             { owner->repaintParamSource(); }
void ControlSurface::redrawSelectedTracks()             { owner->redrawTracks(); }

bool ControlSurface::pickedUp (ControlType type, float value)
{
    return pickedUp (type, 0, value);
}

void ControlSurface::moveFader (int channelNum, float newSliderPos)
{
    if (! pickUpMode) return;

    auto& info = pickUpMap[{ctrlFader, channelNum}];
    info.lastOut = newSliderPos;

    if (info.lastIn.has_value())
        info.pickedUp = std::abs (info.lastOut - *info.lastIn) <= 1.0f / 127.0f;
    else
        info.pickedUp = false;
}

void ControlSurface::movePanPot (int channelNum, float newPan)
{
    if (! pickUpMode) return;

    auto& info = pickUpMap[{ctrlPan, channelNum}];
    info.lastOut = newPan;

    if (info.lastIn.has_value())
        info.pickedUp = std::abs (info.lastOut - *info.lastIn) <= 1.0f / 127.0f;
    else
        info.pickedUp = false;
}

void ControlSurface::moveAux (int channel, int auxNum, const char*, float newPos)
{
    if (! pickUpMode) return;

    auto& info = pickUpMap[{ControlType (ctrlAux + auxNum), channel}];
    info.lastOut = newPos;

    if (info.lastIn.has_value())
        info.pickedUp = std::abs (info.lastOut - *info.lastIn) <= 1.0f / 127.0f;
    else
        info.pickedUp = false;
}

void ControlSurface::moveMasterLevelFader (float newPos)
{
    if (! pickUpMode) return;

    auto& info = pickUpMap[{ctrlMasterFader, 0}];
    info.lastOut = newPos;

    if (info.lastIn.has_value())
        info.pickedUp = std::abs (info.lastOut - *info.lastIn) <= 1.0f / 127.0f;
    else
        info.pickedUp = false;
}

void ControlSurface::moveMasterPanPot (float newPan)
{
    if (! pickUpMode) return;

    auto& info = pickUpMap[{ctrlMasterPanPot, 0}];
    info.lastOut = newPan;

    if (info.lastIn.has_value())
        info.pickedUp = std::abs (info.lastOut - *info.lastIn) <= 1.0f / 127.0f;
    else
        info.pickedUp = false;
}

void ControlSurface::parameterChanged (int parameterNumber, const ParameterSetting& newValue)
{
    if (! pickUpMode) return;

    auto& info = pickUpMap[{ctrlParam, parameterNumber}];
    info.lastOut = newValue.value;

    if (info.lastIn.has_value())
        info.pickedUp = std::abs (info.lastOut - *info.lastIn) <= 1.0f / 127.0f;
    else
        info.pickedUp = false;
}

bool ControlSurface::pickedUp (ControlType type, int index, float value)
{
    if (! pickUpMode) return true;

    bool crossed = false;

    auto& info = pickUpMap[{type, index}];
    if (! info.lastIn.has_value())
    {
        crossed = std::abs (value - info.lastOut) <= 1.0f / 127.0f;
    }
    else
    {
        auto v1 = value;
        auto v2 = *info.lastIn;

        if (v1 > v2)
            std::swap (v1, v2);

        crossed = (info.lastOut >= v1 && info.lastOut <= v2);
    }

    info.lastIn = value;
    if (crossed)
        info.pickedUp = true;

    return info.pickedUp;
}

void ControlSurface::setFollowsTrackSelection (bool f)
{
    followsTrackSelection = f;
    if (owner)
        owner->followsTrackSelection = f;
}

}} // namespace tracktion { inline namespace engine
