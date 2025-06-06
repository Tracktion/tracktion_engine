/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion { inline namespace engine
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
    void updateTrackProperties();

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

    template <typename PluginType>
    juce::Array<PluginType*> getPluginsOfType() const
    {
        juce::Array<PluginType*> plugins;

        for (auto af : *this)
            if (auto f = dynamic_cast<PluginType*> (af))
                plugins.add (f);

        return plugins;
    }

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

}} // namespace tracktion { inline namespace engine
