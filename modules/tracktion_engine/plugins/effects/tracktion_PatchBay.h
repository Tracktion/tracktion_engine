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

class PatchBayPlugin   : public Plugin
{
public:
    PatchBayPlugin (PluginCreationInfo);
    ~PatchBayPlugin() override;

    //==============================================================================
    struct Wire
    {
        Wire (const juce::ValueTree&, juce::UndoManager*);

        juce::ValueTree state;
        juce::CachedValue<int> sourceChannelIndex, destChannelIndex;
        juce::CachedValue<float> gainDb;
    };

    //==============================================================================
    int getNumWires() const;
    Wire* getWire (int index) const;

    void makeConnection (int inputChannel, int outputChannel, float gainDb, juce::UndoManager*);
    void breakConnection (int inputChannel, int outputChannel);

    //==============================================================================
    static const char* getPluginName()                  { return NEEDS_TRANS("Patch Bay"); }
    static const char* xmlTypeName;

    juce::String getName() const override               { return TRANS("Patch Bay Plugin"); }
    juce::String getPluginType() override               { return xmlTypeName; }
    juce::String getShortName (int) override            { return TRANS("Patch"); }
    juce::String getSelectableDescription() override    { return TRANS("Patch Bay Plugin"); }
    bool canBeAddedToClip() override                    { return false; }
    bool canBeAddedToRack() override                    { return false; }
    bool canBeDisabled() override                       { return false; }
    bool needsConstantBufferSize() override             { return false; }

    void initialise (const PluginInitialisationInfo&) override;
    void deinitialise() override;

    void getChannelNames (juce::StringArray*, juce::StringArray*) override;
    void applyToBuffer (const PluginRenderContext&) override;

private:
    struct WireList;
    std::unique_ptr<WireList> list;
    bool recursionCheck = false;
    SafeSelectable<Plugin> inputPlugin, outputPlugin;

    void cacheInputAndOutputPlugins();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PatchBayPlugin)
};

}} // namespace tracktion { inline namespace engine
