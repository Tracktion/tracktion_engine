/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
{

/** Holds a sequence of plugins.
    Used for tracks + clips + one of these holds the master plugins.
*/
class PluginList
{
public:
    PluginList (Edit&);
    ~PluginList();

    //==============================================================================
    Edit& getEdit() const                                   { return edit; }
    Clip* getOwnerClip() const                              { return ownerClip; }
    Track* getOwnerTrack() const                            { return ownerTrack; }

    void initialise (const juce::ValueTree&);
    void releaseObjects();

    void setTrackAndClip (Track*, Clip*);

    //==============================================================================
    Plugin** begin() const;
    Plugin** end() const;
    int size() const;

    Plugin* operator[] (int index) const;
    bool contains (const Plugin*) const;
    int indexOf (const Plugin*) const;

    Plugin::Array getPlugins() const;
    void sendMirrorUpdateToAllPlugins (Plugin&) const;

    void clear();
    bool needsConstantBufferSize();
    bool canInsertPlugin();

    Plugin::Ptr insertPlugin (const juce::ValueTree&, int index);
    void insertPlugin (const Plugin::Ptr&, int index, SelectionManager* selectionManagerToSelect);
    void addDefaultTrackPlugins (bool useVCA);
    void addPluginsFrom (const juce::ValueTree&, bool clearFirst, bool atStart);

    template <typename PluginType>
    PluginType* findFirstPluginOfType() const
    {
        for (auto af : *this)
            if (auto f = dynamic_cast<PluginType*> (af))
                return f;

        return {};
    }

    //==============================================================================
    // Creates a node that will play the whole sequence.
    AudioNode* createAudioNode (AudioNode* input, bool addNoise);
    AudioNode* attachNodesForPluginsNeedingLivePlay (AudioNode* input);

    //==============================================================================
    juce::ValueTree state;

private:
    //==============================================================================
    Edit& edit;
    Track* ownerTrack = nullptr;
    Clip* ownerClip = nullptr;

    struct ObjectList;
    std::unique_ptr<ObjectList> list;

    juce::UndoManager* getUndoManager() const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginList)
};

} // namespace tracktion_engine
