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

bool UIBehaviour::paste (const Clipboard& clipboard)
{
    if (auto content = clipboard.getContent())
    {
        if (auto sm = getCurrentlyFocusedSelectionManager())
        {
            if (sm->pasteSelected())
                return true;

            if (sm->insertPoint != nullptr && sm->getEdit() != nullptr)
                return content->pasteIntoEdit (*sm->getEdit(), *sm->insertPoint, sm);
        }
    }

    return false;
}

//==============================================================================
void UIBehaviour::showWarningAlert (const juce::String& title, const juce::String& message)
{
    juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon, title, message);
}

bool UIBehaviour::showOkCancelAlertBox (const juce::String& title, const juce::String& message,
                                        const juce::String& ok, const juce::String& cancel)
{
   #if JUCE_MODAL_LOOPS_PERMITTED
    return juce::AlertWindow::showOkCancelBox (juce::AlertWindow::QuestionIcon, title, message, ok, cancel);
   #else
    jassertfalse; // These methods are currently unsupported in non-modal mode and calls to them from within the Engine will be replaced
    return true;
   #endif
}

int UIBehaviour::showYesNoCancelAlertBox (const juce::String& title, const juce::String& message,
                                          const juce::String& yes, const juce::String& no, const juce::String& cancel)
{
   #if JUCE_MODAL_LOOPS_PERMITTED
    return juce::AlertWindow::showYesNoCancelBox (juce::AlertWindow::QuestionIcon, title, message, yes, no, cancel);
   #else
    jassertfalse; // These methods are currently unsupported in non-modal mode and calls to them from within the Engine will be replaced
    return 1;
   #endif
}

void UIBehaviour::showInfoMessage (const juce::String& message)
{
    if (auto c = juce::Desktop::getInstance().getMainMouseSource().getComponentUnderMouse())
    {
        auto bmc = new juce::BubbleMessageComponent();
        bmc->addToDesktop (0);
        bmc->showAt (c, juce::AttributedString (message), 2000, true, true);
    }
}

void UIBehaviour::showWarningMessage (const juce::String& message)
{
    showInfoMessage (message);
}

//==============================================================================
void UIBehaviour::nudgeSelectedClips (TimecodeSnapType snapType, const juce::String& commandDesc,
                                      SelectionManager& sm, const juce::Array<Clip*>& clips, bool automationLocked)
{
    jassert (clips.size() > 0);
    auto& ed = clips.getFirst()->edit;
    sm.keepSelectedObjectsOnScreen();

    TimeDuration delta;
    int trackDelta = 0;

    if (commandDesc.contains ("right"))        delta = TimeDuration::fromSeconds (0.00001);
    else if (commandDesc.contains ("left"))    delta = TimeDuration::fromSeconds (-0.00001);
    else if (commandDesc.contains ("up"))      trackDelta = -1;
    else if (commandDesc.contains ("down"))    trackDelta =  1;

    if (trackDelta != 0)
    {
        int minTrack = 0xffff;
        int maxTrack = 0;
        auto allTracks = getAllTracks (ed);

        for (int i = clips.size(); --i >= 0;)
        {
            minTrack = std::min (minTrack, allTracks.indexOf (clips.getUnchecked (i)->getTrack()));
            maxTrack = std::max (maxTrack, allTracks.indexOf (clips.getUnchecked (i)->getTrack()));
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
            juce::Array<TrackAutomationSection> sections;

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
                moveAutomation (sections, {}, false);
        }
    }
    else if (delta != TimeDuration())
    {
        juce::Array<TrackAutomationSection> sections;

        if (automationLocked)
            for (auto&& clip : clips)
                sections.add (TrackAutomationSection (*clip));

        if (auto first = clips.getFirst())
        {
            // if there's one clip, move it to the nearest interval, else
            // move all the clips by the same amount
            auto origClip1Start = first->getPosition().getStart();
            auto clipStart = origClip1Start;

            if (delta > TimeDuration())
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

void UIBehaviour::nudgeSelected (TimecodeSnapType snapType, const juce::String& commandDesc, bool automationLocked)
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

void UIBehaviour::nudgeSelected (const juce::String& commandDesc)
{
    if (auto edit = getCurrentlyFocusedEdit())
        nudgeSelected (edit->getTransport().getSnapType(), commandDesc, false);
}

//==============================================================================
TimePosition UIBehaviour::getEditingPosition (Edit& e)
{
    return e.getTransport().getPosition();
}

TimeRange UIBehaviour::getEditingRange (Edit& e)
{
    return e.getTransport().getLoopRange();
}

void UIBehaviour::recreatePluginWindowContentAsync (Plugin& p)
{
    p.windowState->recreateWindowIfShowing();
}

}} // namespace tracktion { inline namespace engine
