/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#if TRACKTION_UNIT_TESTS && ENGINE_UNIT_TESTS_LOOP_INFO

#include "../../tracktion_graph/tracktion_graph/tracktion_TestUtilities.h"
#include "../utilities/tracktion_TestUtilities.h"

namespace tracktion { inline namespace engine
{

//==============================================================================
//==============================================================================
class LoopInfoTests : public juce::UnitTest
{
public:
    LoopInfoTests()
        : juce::UnitTest ("LoopInfo", "tracktion_engine")
    {
    }

    void runTest() override
    {
        beginTest ("Undo behaviour");

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
                { { IDs::numBeats, 2.0 },
                  { IDs::numerator, 4.0 },
                  { IDs::denominator, 4.0 },
                }
            } }
        };

        auto& um = edit->getUndoManager();
        um.clearUndoHistory();

        // Starting new undo transaction
        um.beginNewTransaction();
        expect  (! um.canUndo());
        expect  (! um.canRedo());

        // Adding a clip with numBeats = 2 in LOOPINFO
        track->insertClipWithState (newClipState);

        if (auto acb = dynamic_cast<AudioClipBase*> (track->getClips().getFirst()))
        {
            LoopInfo& li = acb->getLoopInfo();
            expectEquals (li.getNumBeats(), 2.0);

            // Starting new undo transaction
            um.beginNewTransaction();

            li.setNumBeats (4.0);
            expectEquals (li.getNumBeats(), 4.0);
        }

        // Calling undo() twice
        expect (um.canUndo());
        um.undo();
        expect (um.canUndo());
        um.undo();
        expect (! um.canUndo());
        expect (dynamic_cast<AudioClipBase*>(track->getClips().getFirst()) == nullptr);

        // Calling redo() twice
        expect (um.canRedo());
        um.redo();
        expect (um.canRedo());
        um.redo();
        expect (! um.canRedo());

        if (auto acb = dynamic_cast<AudioClipBase*>(track->getClips().getFirst()))
        {
            LoopInfo& li = acb->getLoopInfo();
            expectEquals (li.getNumBeats(), 4.0);
        }
        else
        {
            expect (false, "No audio clip on track!");
        }
    }
};

static LoopInfoTests loopInfoTests;

}} // namespace tracktion { inline namespace engine

#endif
