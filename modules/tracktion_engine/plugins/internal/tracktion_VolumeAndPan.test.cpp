/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#if TRACKTION_UNIT_TESTS && ENGINE_UNIT_TESTS_VOLPANPLUGIN

namespace tracktion { inline namespace engine
{

//==============================================================================
//==============================================================================
class VolPanTests   : public juce::UnitTest
{
public:
    VolPanTests()
        : juce::UnitTest ("VolumeAndPan Plugin", "tracktion_engine")
    {
    }

    void runTest() override
    {
        runUndoTests();
    }

private:
    void runUndoTests()
    {
        auto& engine = *Engine::getEngines()[0];
        juce::ValueTree editState;

        beginTest ("Undo/redo");
        {
            auto edit = Edit::createSingleTrackEdit (engine);
            auto& um = edit->getUndoManager();
            um.setMaxNumberOfStoredUnits (30000, 30); // Ensure this isn't the default "single transaction"
            um.clearUndoHistory();

            auto p = getAudioTracks (*edit)[0]->getVolumePlugin();

            // Starting new undo transaction
            um.beginNewTransaction();
            expect  (! um.canUndo());
            expect  (! um.canRedo());

            expectWithinAbsoluteError (p->getVolumeDb(), 0.0f, 0.001f);
            p->setVolumeDb (-60.0f);
            expectWithinAbsoluteError (p->getVolumeDb(), -60.0f, 0.001f);
            expect  (um.canUndo());

            um.beginNewTransaction();
            expectWithinAbsoluteError (p->getPan(), 0.0f, 0.001f);
            p->setPan (1.0f);
            expectWithinAbsoluteError (p->getPan(), 1.0f, 0.001f);
            expect  (um.canUndo());

            um.undo();
            expect  (um.canUndo());
            expect  (um.canRedo());

            expectWithinAbsoluteError (p->getVolumeDb(), -60.0f, 0.001f);
            expectWithinAbsoluteError (p->getPan(), 0.0f, 0.001f);

            um.undo();
            expect  (! um.canUndo());
            expect  (um.canRedo());

            expectWithinAbsoluteError (p->getVolumeDb(), 0.0f, 0.001f);
            expectWithinAbsoluteError (p->getPan(), 0.0f, 0.001f);

            um.redo();
            expect  (um.canRedo());
            expectWithinAbsoluteError (p->getVolumeDb(), -60.0f, 0.001f);
            expectWithinAbsoluteError (p->getPan(), 0.0f, 0.001f);

            um.redo();
            expectWithinAbsoluteError (p->getVolumeDb(), -60.0f, 0.001f);
            expectWithinAbsoluteError (p->getPan(), 1.0f, 0.001f);

            edit->flushState();
            editState = edit->state;
        }

        beginTest ("Load saved state");
        {
            Edit edit ({ engine, editState, ProjectItemID::createNewID (0) });
            auto p = getAudioTracks (edit)[0]->getVolumePlugin();
            expectWithinAbsoluteError (p->getVolumeDb(), -60.0f, 0.001f);
            expectWithinAbsoluteError (p->getPan(), 1.0f, 0.001f);
        }
    }
};

static VolPanTests volPanTests;


}} // namespace tracktion { inline namespace engine

#endif
