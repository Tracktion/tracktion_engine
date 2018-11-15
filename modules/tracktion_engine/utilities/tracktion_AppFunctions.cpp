/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace AppFunctions
{
    UIBehaviour& getCurrentUIBehaviour()
    {
        return Engine::getInstance().getUIBehaviour();
    }

    Edit* getCurrentlyFocusedEdit()
    {
        return getCurrentUIBehaviour().getCurrentlyFocusedEdit();
    }

    TransportControl* getActiveTransport()
    {
        if (auto ed = getCurrentlyFocusedEdit())
            return &ed->getTransport();

        return {};
    }

    SelectionManager* getCurrentlyFocusedSelectionManagerWithValidEdit()
    {
        if (auto sm = getCurrentUIBehaviour().getCurrentlyFocusedSelectionManager())
            if (sm->edit != nullptr)
                return sm;

        return nullptr;
    }

    SelectableList getSelectedItems()
    {
        SelectableList items;

        if (auto ed = getCurrentlyFocusedEdit())
            for (SelectionManager::Iterator sm; sm.next();)
                if (sm->edit == ed)
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

    void nudgeSelected (const String& commandDesc)
    {
        getCurrentUIBehaviour().nudgeSelected (commandDesc);
    }

    void zoomHorizontal (float increment)
    {
        getCurrentUIBehaviour().zoomHorizontal (increment);
    }

    void zoomVertical (float amount)
    {
        getCurrentUIBehaviour().zoomHorizontal (amount);
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
                    clips->pasteInsertingAtCursorPos (*sm->edit, *sm->insertPoint, *sm);
    }

    void deleteSelected()
    {
        if (! Component::isMouseButtonDownAnywhere())
            if (auto sm = getCurrentUIBehaviour().getCurrentlyFocusedSelectionManager())
                sm->deleteSelected();
    }

    void deleteRegion()
    {
        if (auto sm = getCurrentlyFocusedSelectionManagerWithValidEdit())
        {
            if (sm->containsType<Track>())
                deleteRegionOfTracks (*sm->edit, getCurrentUIBehaviour().getEditingRange (*sm->edit), true, false, sm);
            else
                deleteRegionOfSelectedClips (*sm, getCurrentUIBehaviour().getEditingRange (*sm->edit), true, false);
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

    void markIn()
    {
        if (auto transport = getActiveTransport())
            markIn (*transport);
    }

    void markOut()
    {
        if (auto transport = getActiveTransport())
            markOut (*transport);
    }

    void start()
    {
        if (auto edit = getCurrentlyFocusedEdit())
            edit->getTransport().play (true);
    }

    void stop()
    {
        if (auto transport = getActiveTransport())
            transport->stop (false, false, true, ModifierKeys::getCurrentModifiersRealtime().isCtrlDown());
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

    void saveEdit()
    {
        if (auto ed = getCurrentlyFocusedEdit())
            EditFileOperations (*ed).save (true, true, false);
    }

    void saveEditAs()
    {
        if (auto ed = getCurrentlyFocusedEdit())
            EditFileOperations (*ed).saveAs();
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
                    if (in->isRecordingEnabled())
                        ++numArmed;
                    else
                        ++numDisarmed;
                }
            }

            for (auto in : ed->getAllInputDevices())
                if (in->isAttachedToTrack())
                    in->setRecordingEnabled (numArmed <= numDisarmed);
        }
    }

    void goToMarkIn()
    {
        if (auto transport = getActiveTransport())
            transport->setCurrentPosition (transport->getLoopRange().getStart());
    }

    void goToMarkOut()
    {
        if (auto transport = getActiveTransport())
            transport->setCurrentPosition (transport->getLoopRange().getEnd());
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
            if (auto clip = transport->edit.getMarkerManager().getNextMarker (transport->getCurrentPosition()))
                transport->setCurrentPosition (clip->getPosition().getStart());
    }

    void moveToPrevMarker()
    {
        if (auto transport = getActiveTransport())
            if (auto clip = transport->edit.getMarkerManager().getPrevMarker (transport->getCurrentPosition()))
                transport->setCurrentPosition (clip->getPosition().getStart());
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

    void stopRecordingAndDiscard()
    {
        if (auto transport = getActiveTransport())
            transport->stop (true, true);
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
            auto& edit = *sm->edit;
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
            if (auto newPitch = sm->edit->pitchSequence.insertPitch (getCurrentUIBehaviour().getEditingPosition (*sm->edit)))
                sm->selectOnly (*newPitch);
    }

    void insertTimeSigChange()
    {
        if (auto sm = getCurrentUIBehaviour().getCurrentlyFocusedSelectionManager())
        {
            auto& edit = *sm->edit;
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
            auto& edit = *sm->edit;

            if (auto ct = edit.getChordTrack())
            {
                auto start = edit.getTransport().position.get();
                auto& tempo = edit.tempoSequence.getTempoAt (start);
                auto approxOneBarLength = tempo.getMatchingTimeSig().numerator * tempo.getApproxBeatLength();

                ct->insertNewClip (TrackItem::Type::chord, { start, start + approxOneBarLength }, sm);
            }
        }
    }

    void showHideVideo()
    {
        getCurrentUIBehaviour().showHideVideo();
    }

    void showHideAllPanes()
    {
        getCurrentUIBehaviour().showHideAllPanes();
    }

    void split()
    {
        if (auto sm = getCurrentlyFocusedSelectionManagerWithValidEdit())
            splitClips (getCurrentUIBehaviour().getAssociatedClipsToEdit (sm->getSelectedObjects()),
                        getCurrentUIBehaviour().getEditingPosition (*sm->edit));
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
