/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#if TRACKTION_UNIT_TESTS && ENGINE_UNIT_TESTS_LOOP_INFO

#include <tracktion_engine/../3rd_party/doctest/tracktion_doctest.hpp>
#include "../../tracktion_graph/tracktion_graph/tracktion_TestUtilities.h"
#include "../utilities/tracktion_TestUtilities.h"

namespace tracktion::inline engine {

TEST_SUITE ("tracktion_engine")
{
    TEST_CASE ("LoopInfo: JUCE undo behaviour")
    {
        juce::UndoManager um;
        juce::ValueTree v ("ROOT"), c ("CHILD");

        um.beginNewTransaction();
        CHECK (! um.canUndo());
        CHECK (! um.canRedo());

        v.setProperty ("prop1", 42, &um);
        CHECK (um.canUndo());
        CHECK (! um.canRedo());

        um.beginNewTransaction();
        v.appendChild (c, &um);

        CHECK_EQ (static_cast<int> (v["prop1"]), 42);
        CHECK_EQ (v.getNumChildren(), 1);

        um.undo();
        CHECK_EQ (static_cast<int> (v["prop1"]), 42);
        CHECK_EQ (v.getNumChildren(), 0);
        CHECK (um.canUndo());
        CHECK (um.canRedo());

        um.undo();
        CHECK (! v.hasProperty ("prop1"));
        CHECK_EQ (static_cast<int> (v["prop1"]), 0);
        CHECK_EQ (v.getNumChildren(), 0);
        CHECK (! um.canUndo());
        CHECK (um.canRedo());

        um.redo();
        CHECK_EQ (static_cast<int> (v["prop1"]), 42);
        CHECK_EQ (v.getNumChildren(), 0);
        CHECK (um.canUndo());
        CHECK (um.canRedo());

        um.redo();
        CHECK_EQ (static_cast<int> (v["prop1"]), 42);
        CHECK_EQ (v.getNumChildren(), 1);
        CHECK (um.canUndo());
        CHECK (! um.canRedo());
    }

    TEST_CASE ("LoopInfo: Undo behaviour")
    {
        using namespace tracktion::graph::test_utilities;
        using namespace tracktion::engine::test_utilities;

        auto& engine = *Engine::getEngines()[0];
        auto edit = createTestEdit (engine);
        auto track = getAudioTracks (*edit)[0];

        juce::ValueTree newClipState {
            IDs::CONTAINERCLIP,
            {
                { IDs::autoTempo, true },
                { IDs::proxyAllowed, false },
                { IDs::start, 0 },
                { IDs::length, 2.0 },
                { IDs::loopStartBeats, 0.0 },
                { IDs::loopLengthBeats, 4.0 },
            },
            { { IDs::LOOPINFO,
                { { IDs::numBeats, 3.0 } }
            } }
        };

        auto& um = edit->getUndoManager();
        um.setMaxNumberOfStoredUnits (30000, 30); // Ensure this isn't the default "single transaction"
        um.clearUndoHistory();

        // Starting new undo transaction
        um.beginNewTransaction();
        CHECK (! um.canUndo());
        CHECK (! um.canRedo());

        // Adding a clip with numBeats = 2 in LOOPINFO
        track->insertClipWithState (newClipState);
        CHECK (um.canUndo());

        if (auto acb = dynamic_cast<AudioClipBase*> (track->getClips().getFirst()))
        {
            LoopInfo& li = acb->getLoopInfo();
            CHECK_EQ (li.getNumBeats(), 3.0);

            // Starting new undo transaction
            um.beginNewTransaction();

            li.setNumBeats (4.0);
            CHECK_EQ (li.getNumBeats(), 4.0);
        }

        // Calling undo() twice
        CHECK (um.canUndo());
        um.undo();

        // Check setting num beats was undone
        if (auto acb = dynamic_cast<AudioClipBase*> (track->getClips().getFirst()))
        {
            LoopInfo& li = acb->getLoopInfo();
            CHECK_EQ (li.getNumBeats(), 3.0);
        }

        CHECK_EQ (um.getNumActionsInCurrentTransaction(), 0);
        CHECK (um.canUndo());
        um.undo();
        CHECK (! um.canUndo());
        CHECK (dynamic_cast<AudioClipBase*> (track->getClips().getFirst()) == nullptr);

        // Calling redo() twice
        CHECK (um.canRedo());
        um.redo();
        CHECK (um.canRedo());
        um.redo();
        CHECK (! um.canRedo());

        if (auto acb = dynamic_cast<AudioClipBase*>(track->getClips().getFirst()))
        {
            LoopInfo& li = acb->getLoopInfo();
            CHECK_EQ (li.getNumBeats(), 4.0);
        }
        else
        {
            CHECK_MESSAGE (false, "No audio clip on track!");
        }
    }
}

} // namespace tracktion::inline engine

#endif
