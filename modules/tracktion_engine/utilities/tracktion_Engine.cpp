/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/

static Engine* instance = nullptr;

Engine::Engine (std::unique_ptr<PropertyStorage> ps, std::unique_ptr<UIBehaviour> ub, std::unique_ptr<EngineBehaviour> eb)
{
    instance = this;

    jassert (ps != nullptr);
    propertyStorage = std::move (ps);

    if (ub == nullptr)
        uiBehaviour = std::make_unique<UIBehaviour>();
    else
        uiBehaviour = std::move (ub);

    if (! eb)
        engineBehaviour = std::make_unique<EngineBehaviour>();
    else
        engineBehaviour = std::move (eb);

    initialise();
}

Engine::Engine (String applicationName, std::unique_ptr<UIBehaviour> ub, std::unique_ptr<EngineBehaviour> eb)
    : Engine (std::make_unique<PropertyStorage> (applicationName), std::move (ub), std::move (eb))
{
}

Engine::Engine (String applicationName)  : Engine (applicationName, nullptr, nullptr)
{
}

void Engine::initialise()
{
    Selectable::initialise();

    activeEdits.reset (new ActiveEdits());
    temporaryFileManager.reset (new TemporaryFileManager (*this));
    recordingThumbnailManager.reset (new RecordingThumbnailManager (*this));
    waveInputRecordingThread.reset (new WaveInputRecordingThread (*this));
    editDeleter.reset (new EditDeleter());
    audioFileFormatManager.reset (new AudioFileFormatManager());
    midiLearnState.reset (new MidiLearnState (*this));
    renderManager.reset (new RenderManager (*this));
    audioFileManager.reset (new AudioFileManager (*this));
    deviceManager.reset (new DeviceManager (*this));
    midiProgramManager.reset (new MidiProgramManager (*this));

    externalControllerManager.reset (new ExternalControllerManager (*this));
    backgroundJobManager.reset (new BackgroundJobManager());
    pluginManager.reset (new PluginManager (*this));

    deviceManager->initialise();

    pluginManager->initialise();
    pluginManager->setUsesSeparateProcessForScanning (false);

    ProjectManager::getInstance()->initialise();

    externalControllerManager->initialise();
}

Engine::~Engine()
{
    getExternalControllerManager().shutdown();
    getDeviceManager().closeDevices();
    getBackgroundJobs().stopAndDeleteAllRunningJobs();

    temporaryFileManager->cleanUp();

    editDeleter.reset();

    getRenderManager().cleanUp();
    backgroundJobManager.reset();
    deviceManager.reset();
    midiProgramManager.reset();

    pluginManager.reset();
    MelodyneFileReader::cleanUpOnShutdown();

    temporaryFileManager.reset();
    ProjectManager::deleteInstance();

    renderManager.reset();
    externalControllerManager.reset();
    propertyStorage.reset();
    uiBehaviour.reset();
    engineBehaviour.reset();
    audioFileManager.reset();
    midiLearnState.reset();
    audioFileFormatManager.reset();

    instance = nullptr;
}

Engine& Engine::getInstance()
{
    jassert (instance != nullptr);
    return *instance;
}

AudioFileFormatManager& Engine::getAudioFileFormatManager() const
{
    jassert (audioFileFormatManager != nullptr);
    return *audioFileFormatManager;
}

DeviceManager& Engine::getDeviceManager() const
{
    jassert (deviceManager != nullptr);
    return *deviceManager;
}

MidiProgramManager& Engine::getMidiProgramManager() const
{
    jassert (midiProgramManager);
    return *midiProgramManager;
}

ExternalControllerManager& Engine::getExternalControllerManager() const
{
    jassert (externalControllerManager != nullptr);
    return *externalControllerManager;
}

RenderManager& Engine::getRenderManager() const
{
    jassert (renderManager != nullptr);
    return *renderManager;
}

BackgroundJobManager& Engine::getBackgroundJobs() const
{
    jassert (backgroundJobManager != nullptr);
    return *backgroundJobManager;
}

PropertyStorage& Engine::getPropertyStorage() const
{
    jassert (propertyStorage != nullptr);
    return *propertyStorage;
}

UIBehaviour& Engine::getUIBehaviour() const
{
    jassert (uiBehaviour != nullptr);
    return *uiBehaviour;
}

EngineBehaviour& Engine::getEngineBehaviour() const
{
    jassert (engineBehaviour != nullptr);
    return *engineBehaviour;
}

AudioFileManager& Engine::getAudioFileManager() const
{
    jassert (audioFileManager != nullptr);
    return *audioFileManager;
}

MidiLearnState& Engine::getMidiLearnState() const
{
    jassert (midiLearnState != nullptr);
    return *midiLearnState;
}

PluginManager& Engine::getPluginManager() const
{
    jassert (pluginManager != nullptr);
    return *pluginManager;
}

EditDeleter& Engine::getEditDeleter() const
{
    jassert (editDeleter != nullptr);
    return *editDeleter;
}

TemporaryFileManager& Engine::getTemporaryFileManager() const
{
    jassert (temporaryFileManager != nullptr);
    return *temporaryFileManager;
}

RecordingThumbnailManager& Engine::getRecordingThumbnailManager() const
{
    jassert (recordingThumbnailManager != nullptr);
    return *recordingThumbnailManager;
}

WaveInputRecordingThread& Engine::getWaveInputRecordingThread() const
{
    jassert (waveInputRecordingThread != nullptr);
    return *waveInputRecordingThread;
}

ActiveEdits& Engine::getActiveEdits() const noexcept
{
    jassert (activeEdits != nullptr);
    return *activeEdits;
}

GrooveTemplateManager& Engine::getGrooveTemplateManager()
{
    if (! grooveTemplateManager)
        grooveTemplateManager = std::make_unique<GrooveTemplateManager> (*this);

    return *grooveTemplateManager;
}

CompFactory& Engine::getCompFactory() const
{
    if (! compFactory)
        compFactory = std::make_unique<CompFactory>();

    return *compFactory;
}

WarpTimeFactory& Engine::getWarpTimeFactory() const
{
    if (! warpTimeFactory)
        warpTimeFactory = std::make_unique<WarpTimeFactory>();

    return *warpTimeFactory;
}
