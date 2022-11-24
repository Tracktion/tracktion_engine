/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion { inline namespace engine
{

static Engine* instance = nullptr;
static juce::Array<Engine*> engines;

Engine::Engine (std::unique_ptr<PropertyStorage> ps, std::unique_ptr<UIBehaviour> ub, std::unique_ptr<EngineBehaviour> eb)
{
    instance = this;
    engines.add (this);

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

Engine::Engine (juce::String applicationName, std::unique_ptr<UIBehaviour> ub, std::unique_ptr<EngineBehaviour> eb)
    : Engine (std::make_unique<PropertyStorage> (applicationName), std::move (ub), std::move (eb))
{
}

Engine::Engine (juce::String applicationName)  : Engine (applicationName, nullptr, nullptr)
{
}

void Engine::initialise()
{
    Selectable::initialise();
    AudioScratchBuffer::initialise();
    
    projectManager.reset (new ProjectManager (*this));
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

    if (engineBehaviour->autoInitialiseDeviceManager())
        deviceManager->initialise();

    pluginManager->initialise();
    getProjectManager().initialise();

    externalControllerManager->initialise();
}

Engine::~Engine()
{
    getProjectManager().saveList();

    getExternalControllerManager().shutdown();
    getDeviceManager().closeDevices();
    getBackgroundJobs().stopAndDeleteAllRunningJobs();

    temporaryFileManager->cleanUp();

    editDeleter.reset();

    getRenderManager().cleanUp();
    backgroundJobManager.reset();
    deviceManager.reset();
    midiProgramManager.reset();

    MelodyneFileReader::cleanUpOnShutdown();
    pluginManager.reset();

    temporaryFileManager.reset();
    projectManager.reset();

    renderManager.reset();
    externalControllerManager.reset();
    propertyStorage.reset();
    uiBehaviour.reset();
    engineBehaviour.reset();
    audioFileManager.reset();
    midiLearnState.reset();
    audioFileFormatManager.reset();

    instance = nullptr;
    engines.removeFirstMatchingValue (this);
}

juce::String Engine::getVersion()
{
    return "Tracktion Engine v2.0.0";
}

juce::Array<Engine*> Engine::getEngines()
{
    return engines;
}

#if TRACKTION_ENABLE_SINGLETONS
Engine& Engine::getInstance()
{
    jassert (instance != nullptr);
    return *instance;
}
#endif

ProjectManager& Engine::getProjectManager() const
{
    jassert (projectManager != nullptr);
    return *projectManager;
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

bool EngineBehaviour::shouldLoadPlugin (ExternalPlugin& p)
{
    return p.edit.shouldLoadPlugins();
}

}} // namespace tracktion { inline namespace engine
