/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#if TRACKTION_UNIT_TESTS && ENGINE_UNIT_TESTS_MIDILIST

#include "../utilities/tracktion_TestUtilities.h"

namespace tracktion { inline namespace engine
{

//==============================================================================
//==============================================================================
class MidiListTests : public juce::UnitTest
{
public:
    MidiListTests()
        : juce::UnitTest ("MidiList", "tracktion_engine")
    {
    }

    void runTest() override
    {
        using namespace tracktion::graph::test_utilities;
        using namespace tracktion::engine::test_utilities;

        beginTest ("Undo note length change");
        {
            auto& engine = *Engine::getEngines()[0];
            auto edit = createTestEdit (engine);

            // We need to pump the dispatch loop here to ensure the Edit has attached the undo manager
            // This may be handled better in a more syncronous way in the future
            juce::MessageManager::getInstance()->runDispatchLoopUntil (20);

            auto um = &edit->getUndoManager();
            auto track = getAudioTracks (*edit)[0];
            auto mc = insertMIDIClip (*track, { 0_tp, 4_tp });

            auto& list = mc->getSequence();
            auto note = list.addNote (60, 0_bp, 1_bd, 127, 0, um);

            edit->resetChangedStatus();
            expect (! edit->hasChangedSinceSaved());

            note->setStartAndLength (note->getStartBeat(), note->getLengthBeats() + 1_bd, um);

            // Flush the change message before checking the status
            um->dispatchPendingMessages();
            expect (edit->hasChangedSinceSaved());
        }
    }
};

static MidiListTests midiListTests;

}} // namespace tracktion { inline namespace engine

#endif
