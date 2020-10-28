/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion_engine
{

/**
    The Engine is the central class for all tracktion sessions.

    Create a Engine before creating any edits. Pass in subclasses of behaviours to
    customise how the engine behaves or pass nullptr to use the defaults.
    To get going quickly, just use the constructor that takes an application name,
    which uses default settings.

	Typical declaration in your main component:
	@code
	private:
        tracktion_engine::Engine engine { ProjectInfo::projectName}
    @endcode

    For Extended UI use:
    @code
    private:
        tracktion_engine::Engine engine { ProjectInfo::projectName, std::make_unique<ExtendedUIBehaviour>(), nullptr };
	@endcode
	
*/
class Engine
{
public:
    Engine (juce::String applicationName);
    Engine (juce::String applicationName, std::unique_ptr<UIBehaviour>, std::unique_ptr<EngineBehaviour>);
    Engine (std::unique_ptr<PropertyStorage>, std::unique_ptr<UIBehaviour>, std::unique_ptr<EngineBehaviour>);
    ~Engine();

   #if TRACKTION_ENABLE_SINGLETONS
    // Do not use in new projects
    static Engine& getInstance();
   #endif
    static juce::Array<Engine*> getEngines();

    TemporaryFileManager& getTemporaryFileManager() const; ///< Get the shared temporary file manager allowing to handle default app and user temporary folders.
    AudioFileFormatManager& getAudioFileFormatManager() const; ///< Get the audio files format manager that maintains a list of available wave file formats.
    PropertyStorage& getPropertyStorage() const; ///< Get the top level User settings customizable xml file.
    UIBehaviour& getUIBehaviour() const; ///< Get the engine UI elements customization helper class allowing to change default ui engine elements
	EngineBehaviour& getEngineBehaviour() const; ///< Get engine customization helper class allowing to change the engine default behavior
    DeviceManager& getDeviceManager() const; ///< Get the top level class for handling audio / midi devices @see juce::AudioDeviceManager.
    MidiProgramManager& getMidiProgramManager() const; ///< Get the top level class that handles midi banks, programs, sets or presets
    ExternalControllerManager& getExternalControllerManager() const; ///< Get the external controller manager such as a Mackie Control protocol compatible control surface .
    RenderManager& getRenderManager() const; ///< Get the render jobs manager, allows to retrieve or create new render jobs that typically manipulate AudioFile objects @see RenderManager::Job.
    BackgroundJobManager& getBackgroundJobs() const;
    AudioFileManager& getAudioFileManager() const;
    MidiLearnState& getMidiLearnState() const;
    PluginManager& getPluginManager() const;
    EditDeleter& getEditDeleter() const;
    RecordingThumbnailManager& getRecordingThumbnailManager() const;
    WaveInputRecordingThread& getWaveInputRecordingThread() const;
    ActiveEdits& getActiveEdits() const noexcept;
    GrooveTemplateManager& getGrooveTemplateManager();
    CompFactory& getCompFactory() const;
    WarpTimeFactory& getWarpTimeFactory() const;
    ProjectManager& getProjectManager() const;

    using WeakRef = juce::WeakReference<Engine>;

private:
    void initialise();

    std::unique_ptr<ProjectManager> projectManager;
    std::unique_ptr<TemporaryFileManager> temporaryFileManager;
    std::unique_ptr<AudioFileFormatManager> audioFileFormatManager;
    std::unique_ptr<DeviceManager> deviceManager;
    std::unique_ptr<MidiProgramManager> midiProgramManager;
    std::unique_ptr<PropertyStorage> propertyStorage;
    std::unique_ptr<EngineBehaviour> engineBehaviour;
    std::unique_ptr<UIBehaviour> uiBehaviour;
    std::unique_ptr<ExternalControllerManager> externalControllerManager;
    std::unique_ptr<BackgroundJobManager> backgroundJobManager;
    std::unique_ptr<RenderManager> renderManager;
    std::unique_ptr<AudioFileManager> audioFileManager;
    std::unique_ptr<MidiLearnState> midiLearnState;
    std::unique_ptr<PluginManager> pluginManager;
    std::unique_ptr<EditDeleter> editDeleter;
    std::unique_ptr<RecordingThumbnailManager> recordingThumbnailManager;
    std::unique_ptr<WaveInputRecordingThread> waveInputRecordingThread;
    std::unique_ptr<ActiveEdits> activeEdits;
    mutable std::unique_ptr<GrooveTemplateManager> grooveTemplateManager;
    mutable std::unique_ptr<CompFactory> compFactory;
    mutable std::unique_ptr<WarpTimeFactory> warpTimeFactory;

    JUCE_DECLARE_WEAK_REFERENCEABLE (Engine)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Engine)
};

//==============================================================================
// If you are using the default implementation of PropertyStorage, and need access to the
// underlying PropertiesFile. Warning: If you have subclassed PropertyStorage, then this
// PropertiesFile will be empty. This should only be used to pass to juce::PluginListComponent
// and will be removed in future.
juce::PropertiesFile* getApplicationSettings();

} // namespace tracktion_engine
