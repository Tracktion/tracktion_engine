/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#if TRACKTION_UNIT_TESTS && ENGINE_UNIT_TESTS_EDITCLIP

#include "../../../3rd_party/doctest/tracktion_doctest.hpp"
#include "../../utilities/tracktion_TestUtilities.h"
#include "../../../tracktion_graph/tracktion_graph/tracktion_TestUtilities.h"

namespace tracktion::inline engine
{

TEST_SUITE("tracktion_engine")
{
    TEST_CASE("EditClip")
    {
        auto& engine = *tracktion::engine::Engine::getEngines()[0];
        auto& pm = engine.getProjectManager();

        // - Create a sin wave file
        // - Add it to an Edit
        // - Create a new Edit
        // - Add the first Edit as an EditClip to the second Edit
        // - Render the second Edit
        // - Output should be the same as the first Edit

        juce::TemporaryFile tempProjectFile (projectFileSuffix);
        ProjectManager::TempProject tempProject (pm, tempProjectFile.getFile(), true);
        CHECK (tempProject.project);
        auto& proj = *tempProject.project;

        ProjectItem::Ptr sourceEditProjectItem;

        auto sourceEdit = test_utilities::createTestEdit (engine);
        auto destEdit = test_utilities::createTestEdit (engine);

        {
            sourceEditProjectItem = proj.createNewEdit();
            sourceEdit->setProjectItemID (sourceEditProjectItem->getID());

            CHECK (EditFileOperations (*sourceEdit).save (false, true, false));
            CHECK (sourceEditProjectItem);
            CHECK (sourceEditProjectItem->isEdit());
            CHECK (sourceEditProjectItem->getID().isValid());
            CHECK (sourceEditProjectItem->getSourceFile().existsAsFile());
        }

        test_utilities::BufferAndSampleRate tempSourceRender;

        // Sin file must outlive everything!
        auto fileLength = 5_td;
        auto sinFile = graph::test_utilities::getSinFile<juce::WavAudioFormat> (44100.0, fileLength.inSeconds());

        // Source Edit
        {
            auto track = getAudioTracks (*sourceEdit)[0];
            insertWaveClip (*track, {}, sinFile->getFile(), { .time = { 0_tp, fileLength } },
                            DeleteExistingClips::no);

            tempSourceRender = test_utilities::renderToAudioBuffer (*sourceEdit);
            CHECK (test_utilities::getRMSLevel (tempSourceRender, { 0_tp, 5_tp }, 0)
                    == doctest::Approx (0.707f).epsilon (0.01));
            CHECK (EditFileOperations (*sourceEdit).save (false, true, false));
        }

        // Dest Edit
        {
            auto destEditProjectItem = proj.createNewEdit();
            destEdit->setProjectItemID (destEditProjectItem->getID());

            auto track = getAudioTracks (*destEdit)[0];
            auto editClip = insertEditClip (*track, { 0_tp, TimeDuration::fromSeconds (sourceEditProjectItem->getLength()) },
                                            sourceEditProjectItem->getID());
            CHECK(editClip);
            editClip->setUsesProxy (false);

            CHECK(editClip->getPosition().getLength().inSeconds() == doctest::Approx (5.0));
            CHECK(editClip->getAutoTempo());
            CHECK(editClip->getAutoPitch());

            CHECK(destEdit->getLength().inSeconds() == doctest::Approx (5.0));

            // Render
            auto tempDestRender = test_utilities::renderToAudioBufferDispatchingMessageThread (*destEdit);

            // Check level
            CHECK (test_utilities::getRMSLevel (tempDestRender, { 0_tp, 5_tp }, 0)
                    == doctest::Approx (0.707f).epsilon (0.01));

            // Finally, sum the buffers together negating one of them, they should cancel
            CHECK(tempSourceRender.buffer.getNumSamples() == tempDestRender.buffer.getNumSamples());
            tempDestRender.buffer.addFrom (0, 0, tempSourceRender.buffer,
                                           0, 0, tempSourceRender.buffer.getNumSamples(),
                                           -1.0f);
            CHECK (test_utilities::getRMSLevel (tempDestRender, { 0_tp, 5_tp }, 0)
                    == doctest::Approx (0.0f).epsilon (0.03));
        }
    }
}

} // namespace tracktion::inline engine

#endif //TRACKTION_UNIT_TESTS && ENGINE_UNIT_TESTS_EDITCLIP
