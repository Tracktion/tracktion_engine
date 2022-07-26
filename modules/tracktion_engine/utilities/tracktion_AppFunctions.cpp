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

namespace AppFunctions
{
    inline UIBehaviour& getCurrentUIBehaviour()
    {
        auto e = Engine::getEngines()[0];
        jassert (e != nullptr);
        return e->getUIBehaviour();
    }

    inline Edit* getCurrentlyFocusedEdit()
    {
        if (auto e = getCurrentUIBehaviour().getCurrentlyFocusedEdit())
            return e;
        
        jassertfalse;
        return nullptr;
    }

    inline TransportControl* getActiveTransport()
    {
        if (auto ed = getCurrentlyFocusedEdit())
            return &ed->getTransport();

        return {};
    }

    inline SelectionManager* getCurrentlyFocusedSelectionManagerWithValidEdit()
    {
        if (auto sm = getCurrentUIBehaviour().getCurrentlyFocusedSelectionManager())
            if (sm->edit != nullptr)
                return sm;

        return nullptr;
    }

    inline SelectableList getSelectedItems()
    {
        SelectableList items;

        if (auto ed = getCurrentlyFocusedEdit())
            for (SelectionManager::Iterator sm; sm.next();)
                if (sm->getEdit() == ed)
                    for (auto s : sm->getSelectedObjects())
                        items.addIfNotAlreadyThere (s);

        return items;
    }

    bool pasteIntoProject (Project& p)
    {
        if (auto content = Clipboard::getInstance()->getContentWithType<Clipboard::ProjectItems>())
            return content->pasteIntoProject (p);

        return false;
    }

    inline void nudgeSelected (const juce::String& commandDesc)
    {
        getCurrentUIBehaviour().nudgeSelected (commandDesc);
    }

    inline void zoomHorizontal (float increment)
    {
        getCurrentUIBehaviour().zoomHorizontal (increment);
    }

    inline void zoomVertical (float amount)
    {
        getCurrentUIBehaviour().zoomVertical (amount);
    }

    void cut()
    {
        auto& ui = getCurrentUIBehaviour();

        if (auto sm = ui.getCurrentlyFocusedSelectionManager())
            if (sm->cutSelected())
                ui.showInfoMessage (TRANS("Copied to clipboard"));
    }

    void copy()
    {
        auto& ui = getCurrentUIBehaviour();

        if (auto sm = ui.getCurrentlyFocusedSelectionManager())
            if (sm->copySelected())
                ui.showInfoMessage (TRANS("Copied to clipboard"));
    }

    void paste()
    {
        auto& ui = getCurrentUIBehaviour();

        if (ui.paste (*Clipboard::getInstance()))
            return;

        if (auto p = ui.getCurrentlyFocusedProject())
            if (pasteIntoProject (*p))
                return;

        ui.showWarningMessage (TRANS("No suitable target for pasting the items on the clipboard!"));
    }

    void insertPaste()
    {
        if (auto sm = getCurrentlyFocusedSelectionManagerWithValidEdit())
            if (sm->insertPoint != nullptr)
                if (auto clips = Clipboard::getInstance()->getContentWithType<Clipboard::Clips>())
                    clips->pasteInsertingAtCursorPos (*sm->getEdit(), *sm->insertPoint, *sm);
    }

    void deleteSelected()
    {
        if (! juce::Component::isMouseButtonDownAnywhere())
            if (auto sm = getCurrentUIBehaviour().getCurrentlyFocusedSelectionManager())
                sm->deleteSelected();
    }

    void deleteRegion()
    {
        if (auto sm = getCurrentlyFocusedSelectionManagerWithValidEdit())
        {
            if (sm->containsType<Track>())
                deleteRegionOfTracks (*sm->getEdit(), getCurrentUIBehaviour().getEditingRange (*sm->getEdit()), true, CloseGap::no, sm);
            else
                deleteRegionOfSelectedClips (*sm, getCurrentUIBehaviour().getEditingRange (*sm->getEdit()), CloseGap::no, false);
        }
    }

    void deleteRegionAndCloseGapFromSelected()
    {
        if (auto sm = getCurrentlyFocusedSelectionManagerWithValidEdit())
        {
            if (sm->containsType<Track>())
                deleteRegionOfTracks (*sm->getEdit(), getCurrentUIBehaviour().getEditingRange (*sm->getEdit()), true, CloseGap::yes, sm);
            else
                deleteRegionOfSelectedClips (*sm, getCurrentUIBehaviour().getEditingRange (*sm->getEdit()), CloseGap::yes, false);
        }
    }

    void deleteRegionAndCloseGap()
    {
        if (auto sm = getCurrentlyFocusedSelectionManagerWithValidEdit())
        {
            if (sm->containsType<Track>())
                deleteRegionOfTracks (*sm->getEdit(), getCurrentUIBehaviour().getEditingRange (*sm->getEdit()), true, CloseGap::yes, sm);
            else
                deleteRegionOfSelectedClips (*sm, getCurrentUIBehaviour().getEditingRange (*sm->getEdit()), CloseGap::yes, true);
        }
    }

    void goToStart()
    {
        if (auto transport = getActiveTransport())
            toStart (*transport, getSelectedItems());
    }

    void goToEnd()
    {
        if (auto transport = getActiveTransport())
            toEnd (*transport, getSelectedItems());
    }

    void markIn (bool forceAtCursor)
    {
        if (auto transport = getActiveTransport())
            transport->setLoopIn  (forceAtCursor ? transport->position.get()
                                                 : getCurrentUIBehaviour().getEditingPosition (transport->edit));
    }

    void markOut (bool forceAtCursor)
    {
        if (auto transport = getActiveTransport())
            transport->setLoopOut  (forceAtCursor ? transport->position.get()
                                                  : getCurrentUIBehaviour().getEditingPosition (transport->edit));
    }

    void start()
    {
        if (auto edit = getCurrentlyFocusedEdit())
            edit->getTransport().play (true);
    }

    void stop()
    {
        if (auto transport = getActiveTransport())
            transport->stop (false, false, true, juce::ModifierKeys::getCurrentModifiersRealtime().isCtrlDown());
    }

    void startStopPlay()
    {
        if (auto transport = getActiveTransport())
        {
            if (transport->isPlaying())
                stop();
            else
                start();
        }
    }

    void record()
    {
        if (auto transport = getActiveTransport())
        {
            const bool wasRecording = transport->isRecording();

            transport->stop (false, false);

            if (! wasRecording)
            {
                getCurrentUIBehaviour().stopPreviewPlayback();
                transport->record (true);
            }
        }
    }

    void toggleTimecode()
    {
        if (auto ed = getCurrentlyFocusedEdit())
            ed->toggleTimecodeMode();
    }

    void toggleLoop()
    {
        if (auto tc = getActiveTransport())
            tc->looping = ! tc->looping;
    }

    void togglePunch()
    {
        if (auto ed = getCurrentlyFocusedEdit())
        {
            if (ed->getTransport().isRecording())
                ed->getTransport().stop (false, false);

            ed->recordingPunchInOut = ! ed->recordingPunchInOut;
        }
    }

    void toggleSnap()
    {
        if (auto tc = getActiveTransport())
            tc->snapToTimecode = ! tc->snapToTimecode;
    }

    void toggleClick()
    {
        if (auto ed = getCurrentlyFocusedEdit())
            ed->clickTrackEnabled = ! ed->clickTrackEnabled;
    }

    void toggleMidiChase()
    {
        if (auto ed = getCurrentlyFocusedEdit())
            ed->enableTimecodeSync (! ed->isTimecodeSyncEnabled());
    }

    void tabBack()
    {
        if (auto transport = getActiveTransport())
            tabBack (*transport);
    }

    void tabForward()
    {
        if (auto transport = getActiveTransport())
            tabForward (*transport);
    }

    void nudgeUp()      { nudgeSelected ("up"); }
    void nudgeDown()    { nudgeSelected ("down"); }
    void nudgeLeft()    { nudgeSelected ("left"); }
    void nudgeRight()   { nudgeSelected ("right"); }

    void zoomIn()       { zoomHorizontal (0.2f); }
    void zoomOut()      { zoomHorizontal (-0.2f); }

    void scrollTracksUp()
    {
        getCurrentUIBehaviour().scrollTracksUp();
    }

    void scrollTracksDown()
    {
        getCurrentUIBehaviour().scrollTracksDown();
    }

    void scrollTracksLeft()
    {
        getCurrentUIBehaviour().scrollTracksLeft();
    }

    void scrollTracksRight()
    {
        getCurrentUIBehaviour().scrollTracksRight();
    }

    void toggleEndToEnd()
    {
        if (auto ed = getCurrentlyFocusedEdit())
            ed->playInStopEnabled = ! ed->playInStopEnabled;
    }

    bool saveEdit()
    {
        if (auto ed = getCurrentlyFocusedEdit())
            return EditFileOperations (*ed).save (true, true, false);
        
        return false;
    }

    bool saveEditAs()
    {
        if (auto ed = getCurrentlyFocusedEdit())
            return EditFileOperations (*ed).saveAs();

        return false;
    }

    void armOrDisarmAllInputs()
    {
        if (auto ed = getCurrentlyFocusedEdit())
        {
            int numArmed = 0;
            int numDisarmed = 0;

            for (auto in : ed->getAllInputDevices())
            {
                if (in->isAttachedToTrack())
                {
                    for (auto t : in->getTargetTracks())
                    {
                        if (in->isRecordingEnabled (*t))
                            ++numArmed;
                        else
                            ++numDisarmed;
                    }
                }
            }

            for (auto in : ed->getAllInputDevices())
                if (in->isAttachedToTrack())
                    for (auto t : in->getTargetTracks())
                        in->setRecordingEnabled (*t, numArmed <= numDisarmed);
        }
    }

    void goToMarkIn()
    {
        if (auto transport = getActiveTransport())
            transport->setPosition (transport->getLoopRange().getStart());
    }

    void goToMarkOut()
    {
        if (auto transport = getActiveTransport())
            transport->setPosition (transport->getLoopRange().getEnd());
    }

    void zoomToSelection()
    {
        getCurrentUIBehaviour().zoomToSelection();
    }

    void zoomTracksIn()     { zoomVertical (1.1f); }
    void zoomTracksOut()    { zoomVertical (1.0f / 1.1f); }

    void zoomToFitHorizontally()
    {
        getCurrentUIBehaviour().zoomToFitHorizontally();
    }

    void zoomToFitVertically()
    {
        getCurrentUIBehaviour().zoomToFitVertically();
    }

    void zoomToFitAll()
    {
        zoomToFitHorizontally();
        zoomToFitVertically();
    }

    void moveToNextMarker()
    {
        if (auto transport = getActiveTransport())
            if (auto clip = transport->edit.getMarkerManager().getNextMarker (transport->getPosition()))
                transport->setPosition (clip->getPosition().getStart());
    }

    void moveToPrevMarker()
    {
        if (auto transport = getActiveTransport())
            if (auto clip = transport->edit.getMarkerManager().getPrevMarker (transport->getPosition()))
                transport->setPosition (clip->getPosition().getStart());
    }

    void redo()
    {
        if (auto ed = getCurrentlyFocusedEdit())
            ed->redo();
    }

    void undo()
    {
        if (auto ed = getCurrentlyFocusedEdit())
            ed->undo();
    }

    void toggleScroll()
    {
        getCurrentUIBehaviour().toggleScroll();
    }

    bool isScrolling()
    {
        return getCurrentUIBehaviour().isScrolling();
    }

    void stopRecordingAndDiscard()
    {
        if (auto transport = getActiveTransport())
            transport->stop (true, false);
    }

    void stopRecordingAndRestart()
    {
        if (auto transport = getActiveTransport())
        {
            if (transport->isRecording())
            {
                transport->stop (true, true);
                transport->record (true);
            }
        }
    }

    void insertTempoChange()
    {
        if (auto sm = getCurrentlyFocusedSelectionManagerWithValidEdit())
        {
            auto& edit = *sm->getEdit();
            auto& tempoSequence = edit.tempoSequence;

            if (tempoSequence.getNumTempos() >= 128)
            {
                edit.engine.getUIBehaviour().showWarningAlert (TRANS("Can't insert tempo"),
                                                               TRANS("There are already too many tempos in this edit!"));
            }
            else
            {
                auto nearestBeat = TimecodeSnapType::get1BeatSnapType()
                                     .roundTimeNearest (getCurrentUIBehaviour().getEditingPosition (edit), tempoSequence);

                if (auto newTempo = tempoSequence.insertTempo (nearestBeat))
                    sm->selectOnly (*newTempo);
                else
                    edit.engine.getUIBehaviour().showWarningMessage (TRANS("Tempo changes must be further than 1 beat apart") + "...");
            }
        }
    }

    void insertPitchChange()
    {
        if (auto sm = getCurrentlyFocusedSelectionManagerWithValidEdit())
            if (auto newPitch = sm->getEdit()->pitchSequence.insertPitch (getCurrentUIBehaviour().getEditingPosition (*sm->getEdit())))
                sm->selectOnly (*newPitch);
    }

    void insertTimeSigChange()
    {
        if (auto sm = getCurrentlyFocusedSelectionManagerWithValidEdit())
        {
            auto& edit = *sm->getEdit();
            auto& tempoSequence = edit.tempoSequence;

            if (tempoSequence.getNumTimeSigs() >= 128)
            {
                edit.engine.getUIBehaviour().showWarningAlert (TRANS("Can't insert time signature"),
                                                               TRANS("There are already too many time signatures in this edit!"));
            }
            else
            {
                auto nearestBeat = TimecodeSnapType::get1BeatSnapType()
                                     .roundTimeNearest (getCurrentUIBehaviour().getEditingPosition (edit), tempoSequence);

                if (auto newTempo = tempoSequence.insertTimeSig (nearestBeat))
                    sm->selectOnly (*newTempo);
                else
                    edit.engine.getUIBehaviour().showWarningMessage (TRANS("Time signature changes must be further than 1 beat apart")+ "...");
            }
        }
    }

    void insertChord()
    {
        if (auto sm = getCurrentlyFocusedSelectionManagerWithValidEdit())
        {
            auto& edit = *sm->getEdit();

            if (auto ct = edit.getChordTrack())
            {
                const auto start = edit.getTransport().getPosition();
                auto& tempo = edit.tempoSequence.getTempoAt (start);
                auto approxOneBarLength = TimeDuration::fromSeconds (tempo.getMatchingTimeSig().numerator * tempo.getApproxBeatLength().inSeconds());

                ct->insertNewClip (TrackItem::Type::chord, { start, start + approxOneBarLength }, sm);
            }
        }
    }

    void showHideVideo()
    {
        getCurrentUIBehaviour().showHideVideo();
    }

    void showHideMixer (bool fs)
    {
        getCurrentUIBehaviour().showHideMixer (fs);
    }

    void showHideMidiEditor (bool fs)
    {
        getCurrentUIBehaviour().showHideMidiEditor (fs);
    }

    void showHideTrackEditor (bool zoom)
    {
        getCurrentUIBehaviour().showHideTrackEditor (zoom);
    }

    void showHideBrowser()
    {
        getCurrentUIBehaviour().showHideBrowser();
    }

    void showHideActions()
    {
        getCurrentUIBehaviour().showHideActions();
    }

    void showHideAllPanes()
    {
        getCurrentUIBehaviour().showHideAllPanes();
    }

    void performUserAction (int a)
    {
        getCurrentUIBehaviour().performUserAction (a);
    }

    void split()
    {
        if (auto sm = getCurrentlyFocusedSelectionManagerWithValidEdit())
        {
            auto selected = sm->getSelectedObjects();
            selected.mergeArray (splitClips (getCurrentUIBehaviour().getAssociatedClipsToEdit (selected),
                                             getCurrentUIBehaviour().getEditingPosition (*sm->getEdit())));
            sm->select (selected);
        }
    }

    void toggleAutomationReadMode()
    {
        if (auto ed = getCurrentlyFocusedEdit())
        {
            auto& arm = ed->getAutomationRecordManager();
            arm.setReadingAutomation (! arm.isReadingAutomation());
        }
    }

    void toggleAutomationWriteMode()
    {
        if (auto ed = getCurrentlyFocusedEdit())
            ed->getAutomationRecordManager().toggleWriteAutomationMode();
    }

    void showHideBigMeters()
    {
        auto& ui = getCurrentUIBehaviour();
        ui.setBigInputMetersMode (! ui.getBigInputMetersMode());
    }

    void showHideInputs()
    {
        getCurrentUIBehaviour().showHideInputs();
    }

    void showHideOutputs()
    {
        getCurrentUIBehaviour().showHideOutputs();
    }

    void showProjectScreen()    { getCurrentUIBehaviour().showProjectScreen(); }
    void showSettingsScreen()   { getCurrentUIBehaviour().showSettingsScreen(); }
    void showEditScreen()       { getCurrentUIBehaviour().showEditScreen(); }

    void resetOverloads()
    {
        getCurrentUIBehaviour().resetOverloads();
    }

    void resetPeaks()
    {
        getCurrentUIBehaviour().resetPeaks();
    }

    void toggleTrackFreeze()
    {
        if (auto sm = getCurrentlyFocusedSelectionManagerWithValidEdit())
        {
            auto tracks = sm->getItemsOfType<AudioTrack>();

            int freezeCount = 0;

            for (auto t : tracks)
                if (t->isFrozen (Track::anyFreeze))
                    freezeCount++;

            if (freezeCount == 0)
            {
                for (auto t : tracks)
                    if (! t->isFrozen (Track::anyFreeze))
                        t->setFrozen (true, Track::individualFreeze);
            }
            else
            {
                for (auto t : tracks)
                {
                    if (t->isFrozen (Track::groupFreeze))
                        t->setFrozen (false, Track::groupFreeze);
                    else if (t->isFrozen (Track::individualFreeze))
                        t->setFrozen (false, Track::individualFreeze);
                }
            }
        }
    }
}

}} // namespace tracktion { inline namespace engine
