/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#if TRACKTION_UNIT_TESTS && ENGINE_UNIT_TESTS_MIDILIST

#include <tracktion_engine/../3rd_party/doctest/tracktion_doctest.hpp>
#include "../utilities/tracktion_TestUtilities.h"

namespace tracktion::inline engine {

TEST_SUITE ("tracktion_engine")
{
    TEST_CASE ("MidiList: Undo note length change")
    {
        using namespace tracktion::graph::test_utilities;
        using namespace tracktion::engine::test_utilities;

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
        CHECK (! edit->hasChangedSinceSaved());

        note->setStartAndLength (note->getStartBeat(), note->getLengthBeats() + 1_bd, um);

        // Flush the change message before checking the status
        um->dispatchPendingMessages();
        CHECK (edit->hasChangedSinceSaved());
    }
}

} // namespace tracktion::inline engine

#endif
