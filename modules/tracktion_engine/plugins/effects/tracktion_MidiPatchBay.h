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

class MidiPatchBayPlugin  : public Plugin
{
public:
    MidiPatchBayPlugin (PluginCreationInfo);
    ~MidiPatchBayPlugin() override;

    static const char* getPluginName()      { return NEEDS_TRANS("MIDI Patch Bay"); }
    static juce::ValueTree create();

    //==============================================================================
    static const char* xmlTypeName;

    juce::String getName() const override                                   { return TRANS("Patch Bay Plugin"); }
    juce::String getPluginType() override                                   { return xmlTypeName; }
    juce::String getShortName (int) override                                { return TRANS("MIDIPatch"); }
    bool canBeAddedToClip() override                                        { return false; }
    bool canBeAddedToRack() override                                        { return true; }
    bool needsConstantBufferSize() override                                 { return false; }
    void initialise (const PluginInitialisationInfo&) override;
    void deinitialise() override;
    void applyToBuffer (const PluginRenderContext&) override;
    juce::String getSelectableDescription() override                        { return TRANS("MIDI Patch Bay Plugin"); }
    bool takesAudioInput() override                                         { return false; }
    int getNumOutputChannelsGivenInputs (int) override                      { return 0; }
    void getChannelNames (juce::StringArray*, juce::StringArray*) override  {}

    //==============================================================================
    struct Mappings
    {
        char map[17] = {};
    };

    Mappings getMappings() const;
    void setMappings (const Mappings&);

    void resetMappings();
    void makeConnection (int srcChannel, int dstChannel);
    void sanityCheckMappings();
    void blockAllMappings();
    std::pair<int, int> getFirstMapping();

private:
    //==============================================================================
    enum { blockChannel = 0 };

    juce::CriticalSection mappingsLock;
    Mappings currentMappings;

    void valueTreeChanged() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiPatchBayPlugin)
};

}} // namespace tracktion { inline namespace engine
