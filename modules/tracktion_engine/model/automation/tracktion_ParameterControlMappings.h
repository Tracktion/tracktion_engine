/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
{

/** Controls the set of midi-controller-to-parameter mappings. */
class ParameterControlMappings  : public juce::ChangeBroadcaster,
                                  private juce::AsyncUpdater
{
public:
    //==============================================================================
    ParameterControlMappings (Edit&);
    ~ParameterControlMappings();

    void loadFrom (const juce::ValueTree&);
    void saveTo (juce::ValueTree&);

    static ParameterControlMappings* getCurrentlyFocusedMappings();

    //==============================================================================
    void checkForDeletedParams();

    //==============================================================================
    /** called by the midi input devices when they get moved. */
    void sendChange (int controllerID, float newValue, int channel);
    bool isParameterMapped (AutomatableParameter& param) const  { return parameters.contains (&param); }
    bool getParameterMapping (AutomatableParameter&, int& channel, int& controllerID) const;
    bool removeParameterMapping (AutomatableParameter&);

    //==============================================================================
    int getNumControllerIDs() const                 { return controllerIDs.size(); }
    void showMappingsListForRow (int);
    int getRowBeingListenedTo() const;
    std::pair<juce::String, juce::String> getTextForRow (int rowNumber) const;

    void listenToRow (int);
    void setLearntParam (bool keepListening);

    struct Mapping
    {
        Mapping (AutomatableParameter* p, int cont, int chan)
            : parameter (p), controllerID (cont), channelID (chan) {}

        AutomatableParameter* parameter = nullptr;
        int controllerID = -1, channelID = -1;
    };

    Mapping getMappingForRow (int row) const;

    void removeMapping (int index);

    /** This will put the surface in listen and assign mode, launching the given dialog window.
        The call will block whilst assignments are made and return when the window is closed.
    */
    void showMappingsEditor (juce::DialogWindow::LaunchOptions&);

private:
    //==============================================================================
    struct ParameterAndIndex
    {
        AutomatableParameter* param;
        int index;
    };

    juce::PopupMenu buildMenu (juce::Array<ParameterAndIndex>& allParams);
    void addPluginToMenu (AutomatableParameterTree::TreeNode*, juce::PopupMenu&, juce::Array<ParameterAndIndex>& allParams, int& index, int addAllItemIndex);
    void addPluginToMenu (Plugin::Ptr, juce::PopupMenu&, juce::Array<ParameterAndIndex>& allParams, int& index, int addAllItemIndex);
    void savePreset (int index);
    void loadPreset (int index);
    void deletePreset (int index);
    juce::StringArray getPresets() const;

    Edit& edit;

    juce::Array<int> controllerIDs, channelIDs;
    juce::ReferenceCountedArray<AutomatableParameter> parameters;
    juce::StringArray parameterFullNames;

    int lastControllerID = 0;
    float lastControllerValue = 0;
    int lastControllerChannel = 0;
    int listeningOnRow = -1;

    juce::CriticalSection lock;

    void tellEditAboutChange();
    int addMapping (int id, int channel, const AutomatableParameter::Ptr&);

    void handleAsyncUpdate() override;
};

} // namespace tracktion_engine
