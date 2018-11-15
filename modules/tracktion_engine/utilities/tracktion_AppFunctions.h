/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
{

namespace AppFunctions
{
    void cut();
    void copy();
    void paste();
    bool pasteIntoProject (Project&);
    void insertPaste();

    void deleteSelected();
    void deleteRegion();

    void goToStart();
    void goToEnd();
    void markIn();
    void markOut();

    void start();
    void stop();
    void startStopPlay();
    void record();

    void toggleTimecode();
    void toggleLoop();
    void togglePunch();
    void toggleSnap();
    void toggleClick();
    void toggleMidiChase();

    void tabBack();
    void tabForward();

    void nudgeUp();
    void nudgeDown();
    void nudgeLeft();
    void nudgeRight();
    void zoomIn();
    void zoomOut();

    void scrollTracksUp();
    void scrollTracksDown();
    void scrollTracksLeft();
    void scrollTracksRight();

    void toggleEndToEnd();
    void saveEdit();
    void saveEditAs();
    void armOrDisarmAllInputs();
    void goToMarkIn();
    void goToMarkOut();

    void zoomTracksIn();
    void zoomTracksOut();
    void zoomToFitVertically();
    void zoomToFitHorizontally();
    void zoomToFitAll();
    void zoomToSelection();

    void moveToNextMarker();
    void moveToPrevMarker();
    void redo();
    void undo();
    void toggleScroll();
    void stopRecordingAndDiscard();
    void stopRecordingAndRestart();

    void insertTempoChange();
    void insertPitchChange();
    void insertTimeSigChange();
    void insertChord();

    void showHideVideo();

    void showHideAllPanes();
    void split();

    void toggleAutomationReadMode();
    void toggleAutomationWriteMode();

    void showHideBigMeters();
    void showHideInputs();
    void showHideOutputs();

    void showProjectScreen();
    void showSettingsScreen();
    void showEditScreen();

    void resetOverloads();
    void toggleTrackFreeze();
}

} // namespace tracktion_engine
