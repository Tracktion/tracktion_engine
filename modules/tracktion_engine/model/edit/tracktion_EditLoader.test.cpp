/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#if TRACKTION_UNIT_TESTS && ENGINE_UNIT_TESTS_EDIT_LOADER

#include "../../../3rd_party/doctest/tracktion_doctest.hpp"

namespace tracktion::inline engine
{

TEST_SUITE("tracktion_engine")
{
    TEST_CASE ("EditLoad")
    {
        auto& engine = *Engine::getEngines()[0];
        juce::TemporaryFile tempEditFile;

        // Create Edit with 100 tracks and save to file
        {
            auto edit = createEmptyEdit (engine, tempEditFile.getFile());
            edit->ensureNumberOfAudioTracks (100);
            CHECK(EditFileOperations (*edit).save (false, true, false));
        }

        // Load the on a background thread after pre-loading the ValueTree and check it has the 100 tracks
        {
            std::unique_ptr<Edit> loadedEdit;
            std::atomic<bool> callbackFinished { false };

            auto editLoadedCallback = [&callbackFinished, &loadedEdit] (std::unique_ptr<Edit> edit)
            {
                loadedEdit = std::move (edit);
                callbackFinished = true;
            };

            Edit::Options opts
            {
                .engine = engine,
                .editState = loadValueTree (tempEditFile.getFile(), IDs::EDIT),
                .editProjectItemID = {},
                .role = Edit::forEditing
            };
            auto handle = EditLoader::loadEdit (std::move (opts), editLoadedCallback);
            CHECK (handle);

            test_utilities::runDispatchLoopUntilTrue (callbackFinished);
            CHECK (loadedEdit != nullptr);
            CHECK_EQ (getAudioTracks (*loadedEdit).size(), 100);
        }

        // Load the edit again
        {
            std::unique_ptr<Edit> loadedEdit;
            std::atomic<bool> callbackFinished { false };

            auto editLoadedCallback = [&callbackFinished, &loadedEdit] (std::unique_ptr<Edit> edit)
            {
                loadedEdit = std::move (edit);
                callbackFinished = true;
            };

            auto handle = EditLoader::loadEdit (engine, tempEditFile.getFile(), editLoadedCallback, Edit::forExamining);
            CHECK (handle);

            test_utilities::runDispatchLoopUntilTrue (callbackFinished);
            CHECK (loadedEdit != nullptr);
            CHECK_EQ (getAudioTracks (*loadedEdit).size(), 100);
        }

        // Start to load the edit but cancel it
        {
            std::unique_ptr<Edit> loadedEdit;
            std::atomic<bool> callbackFinished { false };

            auto editLoadedCallback = [&callbackFinished, &loadedEdit] (std::unique_ptr<Edit> edit)
            {
                loadedEdit = std::move (edit);
                callbackFinished = true;
            };

            auto handle = EditLoader::loadEdit (engine, tempEditFile.getFile(), editLoadedCallback, Edit::forExamining);
            CHECK (handle);
            handle.reset(); // Stop the loading

            // If we don't run the dispatch loop, Edit loading can't finish and should be cancelled before completion
            CHECK (loadedEdit == nullptr);
        }
    }

    TEST_CASE ("Failed Edit Load")
    {
        auto& engine = *Engine::getEngines()[0];

        juce::File invalidFile;
        std::atomic<bool> done { false };

        auto editLoader = EditLoader::loadEdit (engine, invalidFile, [&done] (auto) { done = true; });

        while (! done)
            std::this_thread::sleep_for (20ms);

        editLoader.reset();

    }
}

} // namespace tracktion::inline engine

#endif // TRACKTION_UNIT_TESTS && ENGINE_UNIT_TESTS_EDIT_LOADER
