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

MidiLearnState::MidiLearnState (Engine& e)  : engine (e) {}
MidiLearnState::~MidiLearnState() {}

void MidiLearnState::setActive (bool shouldBeActive)
{
    CRASH_TRACER

    learnActive = shouldBeActive;
    listeners.call (&Listener::midiLearnStatusChanged, learnActive);

    // Update all the content components so they show the learn tab
    SelectionManager::refreshAllPropertyPanels();
}

void MidiLearnState::assignmentChanged (ChangeType t)
{
    listeners.call (&Listener::midiLearnAssignmentChanged, t);
}

MidiLearnState::ScopedChangeCaller::ScopedChangeCaller (MidiLearnState& s, ChangeType t) : state (s), type (t) {}
MidiLearnState::ScopedChangeCaller::~ScopedChangeCaller()   { state.assignmentChanged (type); }

MidiLearnState::Listener::Listener (MidiLearnState& s) : ownerState (s)
{
    ownerState.addListener (this);
}

MidiLearnState::Listener::~Listener()
{
    CRASH_TRACER
    ownerState.removeListener (this);
}


//==============================================================================
MidiAssignable::MidiAssignable (Engine& e) : engine (e) {}

void MidiAssignable::addAssignent (const Assignment newAssignment)
{
    for (auto ass : assignemnts)
        if (ass.id == newAssignment.id)
            return;

    assignemnts.add (newAssignment);
}

void MidiAssignable::buildMenu (juce::PopupMenu& m)
{
    CRASH_TRACER

    for (auto ass : assignemnts)
    {
        switch (ass.id)
        {
            case CustomControlSurface::paramTrackId:
            case CustomControlSurface::selectClipInTrackId:
            case CustomControlSurface::selectPluginInTrackId:
            {
                if (auto ec = engine.getExternalControllerManager().getActiveCustomController())
                {
                    juce::PopupMenu paramsMenu;
                    auto numParams = ec->getNumParameterControls();

                    auto baseName = ass.id == CustomControlSurface::paramTrackId ? TRANS("Parameter")
                                                                                 : TRANS("Track");

                    for (int i = 0; i < numParams; ++i)
                        paramsMenu.addItem (CustomControlSurface::paramTrackId + i,
                                            baseName + " #" + juce::String (i + 1));

                    m.addSubMenu (ass.name, paramsMenu);
                }

                break;
            }

            case CustomControlSurface::playId:
            case CustomControlSurface::stopId:
            case CustomControlSurface::recordId:
            case CustomControlSurface::homeId:
            case CustomControlSurface::endId:
            case CustomControlSurface::rewindId:
            case CustomControlSurface::fastForwardId:
            case CustomControlSurface::markInId:
            case CustomControlSurface::markOutId:
            case CustomControlSurface::automationReadId:
            case CustomControlSurface::automationRecordId:
            case CustomControlSurface::addMarkerId:
            case CustomControlSurface::nextMarkerId:
            case CustomControlSurface::previousMarkerId:
            case CustomControlSurface::nudgeLeftId:
            case CustomControlSurface::nudgeRightId:
            case CustomControlSurface::abortId:
            case CustomControlSurface::abortRestartId:
            case CustomControlSurface::jogId:
            case CustomControlSurface::jumpToMarkInId:
            case CustomControlSurface::jumpToMarkOutId:
            case CustomControlSurface::timecodeId:
            case CustomControlSurface::toggleBeatsSecondsModeId:
            case CustomControlSurface::toggleLoopId:
            case CustomControlSurface::togglePunchId:
            case CustomControlSurface::toggleClickId:
            case CustomControlSurface::toggleSnapId:
            case CustomControlSurface::toggleSlaveId:
            case CustomControlSurface::toggleEtoEId:
            case CustomControlSurface::toggleScrollId:
            case CustomControlSurface::toggleAllArmId:
            case CustomControlSurface::masterVolumeId:
            case CustomControlSurface::masterVolumeTextId:
            case CustomControlSurface::masterPanId:
            case CustomControlSurface::quickParamId:
            case CustomControlSurface::paramNameTrackId:
            case CustomControlSurface::paramTextTrackId:
            case CustomControlSurface::clearAllSoloId:
            case CustomControlSurface::nameTrackId:
            case CustomControlSurface::numberTrackId:
            case CustomControlSurface::volTrackId:
            case CustomControlSurface::volTextTrackId:
            case CustomControlSurface::panTrackId:
            case CustomControlSurface::panTextTrackId:
            case CustomControlSurface::muteTrackId:
            case CustomControlSurface::soloTrackId:
            case CustomControlSurface::armTrackId:
            case CustomControlSurface::selectTrackId:
            case CustomControlSurface::auxTrackId:
            case CustomControlSurface::auxTextTrackId:
            case CustomControlSurface::zoomInId:
            case CustomControlSurface::zoomOutId:
            case CustomControlSurface::scrollTracksUpId:
            case CustomControlSurface::scrollTracksDownId:
            case CustomControlSurface::scrollTracksLeftId:
            case CustomControlSurface::scrollTracksRightId:
            case CustomControlSurface::zoomTracksInId:
            case CustomControlSurface::zoomTracksOutId:
            case CustomControlSurface::toggleSelectionModeId:
            case CustomControlSurface::selectLeftId:
            case CustomControlSurface::selectRightId:
            case CustomControlSurface::selectUpId:
            case CustomControlSurface::selectDownId:
            case CustomControlSurface::selectedPluginNameId:
            case CustomControlSurface::faderBankLeftId:
            case CustomControlSurface::faderBankLeft1Id:
            case CustomControlSurface::faderBankLeft4Id:
            case CustomControlSurface::faderBankLeft8Id:
            case CustomControlSurface::faderBankLeft16Id:
            case CustomControlSurface::faderBankRightId:
            case CustomControlSurface::faderBankRight1Id:
            case CustomControlSurface::faderBankRight4Id:
            case CustomControlSurface::faderBankRight8Id:
            case CustomControlSurface::faderBankRight16Id:
            case CustomControlSurface::paramBankLeftId:
            case CustomControlSurface::paramBankLeft1Id:
            case CustomControlSurface::paramBankLeft4Id:
            case CustomControlSurface::paramBankLeft8Id:
            case CustomControlSurface::paramBankLeft16Id:
            case CustomControlSurface::paramBankLeft24Id:
            case CustomControlSurface::paramBankRightId:
            case CustomControlSurface::paramBankRight1Id:
            case CustomControlSurface::paramBankRight4Id:
            case CustomControlSurface::paramBankRight8Id:
            case CustomControlSurface::paramBankRight16Id:
            case CustomControlSurface::paramBankRight24Id:
            case CustomControlSurface::userAction1Id:
            case CustomControlSurface::userAction2Id:
            case CustomControlSurface::userAction3Id:
            case CustomControlSurface::userAction4Id:
            case CustomControlSurface::userAction5Id:
            case CustomControlSurface::userAction6Id:
            case CustomControlSurface::userAction7Id:
            case CustomControlSurface::userAction8Id:
            case CustomControlSurface::userAction9Id:
            case CustomControlSurface::userAction10Id:
            case CustomControlSurface::userAction11Id:
            case CustomControlSurface::userAction12Id:
            case CustomControlSurface::userAction13Id:
            case CustomControlSurface::userAction14Id:
            case CustomControlSurface::userAction15Id:
            case CustomControlSurface::userAction16Id:
            case CustomControlSurface::userAction17Id:
            case CustomControlSurface::userAction18Id:
            case CustomControlSurface::userAction19Id:
            case CustomControlSurface::userAction20Id:
            case CustomControlSurface::emptyTextId:
            case CustomControlSurface::none:
            default:
            {
                int faderIndex = 0;

                if (getControlledTrack() != nullptr)
                    faderIndex = getFaderIndex();

                m.addItem (ass.id + faderIndex, ass.name);
                break;
            }
        }
    }
}

void MidiAssignable::handleMenuResult (int menuResult)
{
    CRASH_TRACER

    if (! engine.getMidiLearnState().isActive())
        return;

    if (auto edit = engine.getUIBehaviour().getCurrentlyFocusedEdit())
        if (menuResult > 0)
            edit->getParameterChangeHandler().actionFunctionTriggered (menuResult);
}

int MidiAssignable::getFaderIndex()
{
    CRASH_TRACER

    if (auto t = getControlledTrack())
    {
        auto& ecm = engine.getExternalControllerManager();

        if (auto ec = ecm.getActiveCustomController())
            return ec->getFaderIndexInActiveRegion (ecm.mapTrackNumToChannelNum (t->getIndexInEditTrackList()));
    }

    return -1;
}

}} // namespace tracktion { inline namespace engine
