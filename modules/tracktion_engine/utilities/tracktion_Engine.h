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
    /** Constructs a default Engine with an application name. */
    Engine (juce::String applicationName);

    /** Constructs an Engine with an application name and custom UIBehaviour and EngineBehaviour. */
    Engine (juce::String applicationName, std::unique_ptr<UIBehaviour>, std::unique_ptr<EngineBehaviour>);

    /** Constructs an Engine with custom PropertyStorage, UIBehaviour and EngineBehaviour. */
    Engine (std::unique_ptr<PropertyStorage>, std::unique_ptr<UIBehaviour>, std::unique_ptr<EngineBehaviour>);

    /** Destructor. */
    ~Engine();

    /** Returns the current version of Tracktion Engine. */
    static juce::String getVersion();

   #if TRACKTION_ENABLE_SINGLETONS
    // Do not use in new projects
    static Engine& getInstance();
   #endif
    /** Returns the list of currently active engines. */
    static juce::Array<Engine*> getEngines();

    TemporaryFileManager& getTemporaryFileManager() const;              ///< Returns the TemporaryFileManager allowing to handle the default app and user temporary folders.
    AudioFileFormatManager& getAudioFileFormatManager() const;          ///< Returns the AudioFileFormatManager that maintains a list of available audio file formats.
    PropertyStorage& getPropertyStorage() const;                        ///< Returns the PropertyStorage user settings customisable XML file.
    UIBehaviour& getUIBehaviour() const;                                ///< Returns the UIBehaviour class.
    EngineBehaviour& getEngineBehaviour() const;                        ///< Returns the EngineBehaviour instance.
    DeviceManager& getDeviceManager() const;                            ///< Returns the DeviceManager instance for handling audio / MIDI devices.
    MidiProgramManager& getMidiProgramManager() const;                  ///< Returns the MidiProgramManager instance that handles MIDI banks, programs, sets or presets.
    ExternalControllerManager& getExternalControllerManager() const;    ///< Returns the ExternalControllerManager instance.
    RenderManager& getRenderManager() const;                            ///< Returns the RenderManager instance.
    BackgroundJobManager& getBackgroundJobs() const;                    ///< Returns the BackgroundJobManager instance.
    AudioFileManager& getAudioFileManager() const;                      ///< Returns the AudioFileManager instance.
    MidiLearnState& getMidiLearnState() const;                          ///< Returns the MidiLearnState instance.
    PluginManager& getPluginManager() const;                            ///< Returns the PluginManager instance.
    EditDeleter& getEditDeleter() const;                                ///< Returns the EditDeleter instance.
    RecordingThumbnailManager& getRecordingThumbnailManager() const;    ///< Returns the RecordingThumbnailManager instance.
    WaveInputRecordingThread& getWaveInputRecordingThread() const;      ///< Returns the WaveInputRecordingThread instance.
    ActiveEdits& getActiveEdits() const noexcept;                       ///< Returns the ActiveEdits instance.
    GrooveTemplateManager& getGrooveTemplateManager();                  ///< Returns the GrooveTemplateManager instance.
    CompFactory& getCompFactory() const;                                ///< Returns the CompFactory instance.
    WarpTimeFactory& getWarpTimeFactory() const;                        ///< Returns the WarpTimeFactory instance.
    ProjectManager& getProjectManager() const;                          ///< Returns the ProjectManager instance.

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

}} // namespace tracktion { inline namespace engine
