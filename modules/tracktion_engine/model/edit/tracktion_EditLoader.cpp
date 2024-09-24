/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion::inline engine
{

EditLoader::Handle::~Handle()
{
    cancel();

    if (! loadContext.completed)
        signalThreadShouldExit (loadThread.get_id());

    loadThread.join();
}

void EditLoader::Handle::cancel()
{
    loadContext.shouldExit = true;
}

float EditLoader::Handle::getProgress() const
{
    return loadContext.progress;
}

std::shared_ptr<EditLoader::Handle> EditLoader::loadEdit (Edit::Options editOptions, std::function<void(std::unique_ptr<Edit>)> editLoadedCallback)
{
    assert (editLoadedCallback && "Completion callback must be valid");
    assert (editOptions.loadContext == nullptr && "This function will return its own LoadContext to use so don't provide one");
    assert (editOptions.editState.hasType (IDs::EDIT) && "This must contain a valid Edit state");

    auto handle = std::shared_ptr<Handle> (new Handle());
    editOptions.loadContext = &handle->loadContext;
    handle->loadThread = std::thread ([options = std::move (editOptions),
                                       completionCallback = std::move (editLoadedCallback)]
                                      {
                                          const ScopedThreadExitStatusEnabler threadExitEnabler;

                                          auto opts = std::move (options);
                                          auto id = ProjectItemID::fromProperty (opts.editState, IDs::projectID);

                                          if (! id.isValid())
                                              id = ProjectItemID::createNewID (0);

                                          opts.editProjectItemID = id;
                                          completionCallback (Edit::createEdit (std::move (opts)));
                                      });

    return handle;
}

std::shared_ptr<EditLoader::Handle> EditLoader::loadEdit (Engine& engine, juce::File editFile,
                                                          std::function<void (std::unique_ptr<Edit>)> editLoadedCallback,
                                                          Edit::EditRole role, int numUndoLevelsToStore)
{
    assert (editLoadedCallback && "Completion callback must be valid");

    auto handle = std::shared_ptr<Handle> (new Handle());
    Edit::Options editOptions
    {
        .engine = engine,
        .editState = {},
        .editProjectItemID = {},
        .role = role,
        .loadContext = &handle->loadContext,
        .numUndoLevelsToStore = numUndoLevelsToStore,
        .editFileRetriever = [editFile] { return editFile; }
    };

    handle->loadThread = std::thread ([options = std::move (editOptions), file = std::move (editFile),
                                       completionCallback = std::move (editLoadedCallback)]
                                      {
                                          const ScopedThreadExitStatusEnabler threadExitEnabler;

                                          auto opts = std::move (options);
                                          opts.editState = loadValueTree (file, IDs::EDIT);

                                          if (! opts.editState.isValid())
                                              return completionCallback ({});

                                          auto id = ProjectItemID::fromProperty (opts.editState, IDs::projectID);

                                          if (! id.isValid())
                                              id = ProjectItemID::createNewID (0);

                                          opts.editProjectItemID = id;

                                          // Load the Edit
                                          completionCallback (Edit::createEdit (opts));
                                      });

    return handle;
}

} // namespace tracktion::inline engine
