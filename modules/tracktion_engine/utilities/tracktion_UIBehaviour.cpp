/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


bool UIBehaviour::paste (const Clipboard& clipboard)
{
    if (auto content = clipboard.getContent())
    {
        if (auto sm = getCurrentlyFocusedSelectionManager())
        {
            if (sm->pasteSelected())
                return true;

            if (sm->insertPoint != nullptr)
                return content->pasteIntoEdit (*sm->edit, *sm->insertPoint, sm);
        }
    }

    return false;
}

//==============================================================================
void UIBehaviour::showWarningAlert (const String& title, const String& message)
{
    AlertWindow::showMessageBoxAsync (AlertWindow::WarningIcon, title, message);
}

bool UIBehaviour::showOkCancelAlertBox (const String& title, const String& message, const String& ok, const String& cancel)
{
    return AlertWindow::showOkCancelBox (AlertWindow::QuestionIcon, title, message, ok, cancel);
}

int UIBehaviour::showYesNoCancelAlertBox (const String& title, const String& message,
                                          const String& yes, const String& no, const String& cancel)
{
    return AlertWindow::showYesNoCancelBox (AlertWindow::QuestionIcon, title, message, yes, no, cancel);
}

void UIBehaviour::showInfoMessage (const String& message)
{
    if (auto c = Desktop::getInstance().getMainMouseSource().getComponentUnderMouse())
    {
        auto bmc = new BubbleMessageComponent();
        bmc->addToDesktop (0);
        bmc->showAt (c, AttributedString (message), 2000);
    }
}

void UIBehaviour::showWarningMessage (const String& message)
{
    showInfoMessage (message);
}

//==============================================================================
void UIBehaviour::nudgeSelectedClips (TimecodeSnapType snapType, const String& commandDesc,
                                      SelectionManager& sm, const Array<Clip*>& clips, bool automationLocked)
{
    jassert (clips.size() > 0);
    auto& ed = clips.getFirst()->edit;
    sm.keepSelectedObjectsOnScreen();

    double delta = 0.0;
    int trackDelta = 0;

    if (commandDesc.contains ("right"))        delta =  0.00001;
    else if (commandDesc.contains ("left"))    delta = -0.00001;
    else if (commandDesc.contains ("up"))      trackDelta = -1;
    else if (commandDesc.contains ("down"))    trackDelta =  1;

    if (trackDelta != 0)
    {
        int minTrack = 0xffff;
        int maxTrack = 0;
        auto allTracks = getAllTracks (ed);

        for (int i = clips.size(); --i >= 0;)
        {
            minTrack = jmin (minTrack, allTracks.indexOf (clips.getUnchecked (i)->getTrack()));
            maxTrack = jmax (maxTrack, allTracks.indexOf (clips.getUnchecked (i)->getTrack()));
        }

        if (trackDelta < 0)
        {
            if (minTrack == 0)
                trackDelta = 0;
        }
        else
        {
            if (allTracks.size() - maxTrack < 2)
                trackDelta = 0;
        }

        if (trackDelta != 0)
        {
            Array<TrackAutomationSection> sections;

            if (automationLocked)
            {
                for (auto&& clip : clips)
                {
                    TrackAutomationSection ts (*clip);

                    if (auto t = allTracks [allTracks.indexOf (clip->getTrack()) + trackDelta])
                        ts.dst = t;

                    sections.add (ts);
                }
            }

            for (int i = clips.size(); --i >= 0;)
                if (auto targetTrack = allTracks [allTracks.indexOf (clips.getUnchecked (i)->getTrack()) + trackDelta])
                    clips.getUnchecked(i)->moveToTrack (*targetTrack);

            if (sections.size())
                moveAutomation (sections, 0, false);
        }
    }
    else if (delta != 0)
    {
        Array<TrackAutomationSection> sections;

        if (automationLocked)
            for (auto&& clip : clips)
                sections.add (TrackAutomationSection (*clip));

        if (auto first = clips.getFirst())
        {
            // if there's one clip, move it to the nearest interval, else
            // move all the clips by the same amount
            auto origClip1Start = first->getPosition().getStart();
            auto clipStart = origClip1Start;

            if (delta > 0)
                first->setStart (snapType.roundTimeUp   (origClip1Start + delta, ed.tempoSequence), false, true);
            else
                first->setStart (snapType.roundTimeDown (origClip1Start + delta, ed.tempoSequence), false, true);

            if (clips.size() > 1)
            {
                delta = first->getPosition().getStart() - origClip1Start;

                for (int i = clips.size(); --i > 0;)
                    clips[i]->setStart (clips.getUnchecked (i)->getPosition().getStart() + delta, false, true);
            }

            if (sections.size())
                moveAutomation (sections, first->getPosition().getStart() - clipStart, false);
        }
        else
        {
            jassertfalse;
        }
    }
}

void UIBehaviour::nudgeSelected (TimecodeSnapType snapType, const String& commandDesc, bool automationLocked)
{
    if (auto sm = getCurrentlyFocusedSelectionManager())
    {
        auto clips = sm->getItemsOfType<Clip>();

        for (auto cc : sm->getItemsOfType<CollectionClip>())
            clips.addArray (cc->getClips());

        clips = getAssociatedClipsToEdit (clips).getItemsOfType<Clip>();

        if (clips.size() > 0)
        {
            nudgeSelectedClips (snapType, commandDesc, *sm, clips, automationLocked);
        }
        else
        {
            if (auto s = sm->getFirstItemOfType<SelectedMidiEvents>())
            {
                int leftRight = 0, upDown = 0;

                if (commandDesc.contains ("right"))          leftRight =  1;
                else if (commandDesc.contains ("left"))      leftRight = -1;
                else if (commandDesc.contains ("up"))        upDown =  1;
                else if (commandDesc.contains ("down"))      upDown = -1;

                s->nudge (snapType, leftRight, upDown);
            }
        }
    }
}

void UIBehaviour::nudgeSelected (const String& commandDesc)
{
    if (auto edit = getCurrentlyFocusedEdit())
        nudgeSelected (edit->getTransport().getSnapType(), commandDesc, false);
}

//==============================================================================
double UIBehaviour::getEditingPosition (Edit& e)
{
    return e.getTransport().position;
}

EditTimeRange UIBehaviour::getEditingRange (Edit& e)
{
    return e.getTransport().getLoopRange();
}
