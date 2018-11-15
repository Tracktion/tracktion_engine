/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


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

void MidiAssignable::buildMenu (PopupMenu& m)
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
                    PopupMenu paramsMenu;
                    auto numParams = ec->getNumParameterControls();

                    auto baseName = ass.id == CustomControlSurface::paramTrackId ? TRANS("Parameter")
                                                                                 : TRANS("Track");

                    for (int i = 0; i < numParams; ++i)
                        paramsMenu.addItem (CustomControlSurface::paramTrackId + i,
                                            baseName + " #" + String (i + 1));

                    m.addSubMenu (ass.name, paramsMenu);
                }

                break;
            }

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
