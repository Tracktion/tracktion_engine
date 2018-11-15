/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
{

//==============================================================================
/**
*/
class MidiProgramManager
{
public:
    MidiProgramManager (Engine&);

    juce::StringArray getMidiProgramSetNames();
    juce::String getProgramName (int set, int bank, int program);
    juce::String getBankName (int set, int bank);
    int getBankID (int set, int bank);
    void setBankID (int set, int bank, int id);
    void setProgramName (int set, int bank, int program, const juce::String& newName);
    void setBankName (int set, int bank, const juce::String& newName);
    void clearProgramName (int set, int bank, int program);
    juce::String getDefaultName (int bank, int program, bool zeroBased);
    juce::String getDefaultCustomName();
    int getSetIndex (const juce::String& setName);
    bool isZeroBased (int set);
    void setZeroBased (int set, bool zeroBased);

    bool doesSetExist (const juce::String& name) const noexcept;
    bool canEditProgramSet (int set) const noexcept;
    bool canDeleteProgramSet (int set) const noexcept;
    void addNewSet (const juce::String& name);
    void addNewSet (const juce::String& name, const juce::XmlElement&);
    void deleteSet (int set);
    bool importSet (int set, const juce::File&);
    bool exportSet (int set, const juce::File&);

    static juce::StringArray getListOfPresets (Engine&);
    juce::String getPresetXml (juce::String presetName);

    void saveAll();

    Engine& engine;

protected:
    struct MidiBank
    {
        juce::XmlElement* createXml();
        void updateFromXml (const juce::XmlElement&);

        juce::String name;
        int id;
        std::map<int, juce::String> programNames;
    };

    struct MidiProgramSet
    {
        MidiProgramSet();
        juce::XmlElement* createXml();
        void updateFromXml (const juce::XmlElement&);

        juce::String name;
        bool zeroBased = true;
        MidiBank midiBanks[16];
    };

    juce::OwnedArray<MidiProgramSet> programSets;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiProgramManager)
};

} // namespace tracktion_engine
