/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


struct PluginList::ObjectList  : public ValueTreeObjectList<Plugin>
{
    ObjectList (PluginList& l, const ValueTree& parent)
        : ValueTreeObjectList<Plugin> (parent), list (l)
    {
        // NB: rebuildObjects() is called after construction so that the edit has a valid
        // list while they're being created
    }

    ~ObjectList()
    {
        freeObjects();
    }

    bool isSuitableType (const ValueTree& v) const override
    {
        return v.hasType (IDs::PLUGIN);
    }

    Plugin* createNewObject (const ValueTree& v) override
    {
        if (auto p = list.edit.getPluginCache().getOrCreatePluginFor (v))
        {
            p->incReferenceCount();
            return p.get();
        }

        return {};
    }

    void deleteObject (Plugin* p) override
    {
        jassert (p != nullptr);

        if (! p->state.getParent().isValid())
            p->deselect();

        p->decReferenceCount();
    }

    void newObjectAdded (Plugin*) override {}
    void objectRemoved (Plugin*) override {}
    void objectOrderChanged() override {}
    void valueTreePropertyChanged (ValueTree&, const Identifier&) override  {}

    PluginList& list;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ObjectList)
};

//==============================================================================
PluginList::PluginList (Edit& ed)  : edit (ed)
{
}

PluginList::~PluginList()
{
}

void PluginList::initialise (const ValueTree& v)
{
    jassert (v.hasType (IDs::MASTERPLUGINS)
              || v.hasType (IDs::TRACK)
              || v.hasType (IDs::FOLDERTRACK)
              || v.hasType (IDs::AUDIOCLIP)
              || v.hasType (IDs::MIDICLIP)
              || v.hasType (IDs::STEPCLIP)
              || v.hasType (IDs::EDITCLIP));

    state = v;
    list.reset (new ObjectList (*this, state));
    callBlocking ([this] { list->rebuildObjects(); });
}

void PluginList::releaseObjects()
{
    list.reset();
}

UndoManager* PluginList::getUndoManager() const
{
    return &edit.getUndoManager();
}

Plugin::Array PluginList::getPlugins() const
{
    Plugin::Array plugins;

    if (list != nullptr)
        for (auto p : list->objects)
            plugins.add (p);

    return plugins;
}

Plugin** PluginList::begin() const  { return list == nullptr ? nullptr : list->objects.begin(); }
Plugin** PluginList::end() const    { return list == nullptr ? nullptr : list->objects.end(); }

int PluginList::size() const
{
    if (list != nullptr)
        return list->objects.size();

    return 0;
}

Plugin* PluginList::operator[] (int index) const
{
    if (list != nullptr)
        return list->objects[index];

    return {};
}

bool PluginList::contains (const Plugin* plugin) const
{
    return indexOf (plugin) >= 0;
}

int PluginList::indexOf (const Plugin* plugin) const
{
    int i = 0;

    for (auto p : *this)
    {
        if (p == plugin)
            return i;

        ++i;
    }

    return -1;
}

void PluginList::setTrackAndClip (Track* track, Clip* clip)
{
    ownerTrack = track;
    ownerClip = clip;
}

void PluginList::sendMirrorUpdateToAllPlugins (Plugin& plugin) const
{
    if (list != nullptr)
        for (auto p : list->objects)
            p->updateFromMirroredPluginIfNeeded (plugin);
}

bool PluginList::needsConstantBufferSize()
{
    if (list != nullptr)
        for (auto f : list->objects)
            if (f->needsConstantBufferSize())
                return true;

    return false;
}

bool PluginList::canInsertPlugin()
{
    return ! ((ownerClip != nullptr && size() >= Edit::maxPluginsOnClip)
              || (ownerTrack != nullptr && size() >= Edit::maxPluginsOnTrack)
              || (ownerTrack == nullptr && size() >= Edit::maxNumMasterPlugins));
}

void PluginList::insertPlugin (const Plugin::Ptr& plugin, int index, SelectionManager* sm)
{
    if (plugin != nullptr)
    {
        jassert (plugin->state.isValid());

        if (list == nullptr)
        {
            jassertfalse;
            return;
        }

        if (auto newPlugin = insertPlugin (plugin->state, index))
        {
            jassert (plugin == newPlugin); // maybe caused by adding a plugin that wasn't created by this edit's pluginCache?
                                           // Should use PluginCache::createNewPlugin to create the ones you add here

            if (sm != nullptr)
                sm->selectOnly (*newPlugin);
        }
    }
}

Plugin::Ptr PluginList::insertPlugin (const ValueTree& v, int index)
{
    CRASH_TRACER

    if (auto newPlugin = edit.getPluginCache().getOrCreatePluginFor (v))
    {
        if (list == nullptr)
        {
            jassertfalse;
            return {};
        }

        if (auto ft = dynamic_cast<FolderTrack*> (ownerTrack))
            if (! ft->willAcceptPlugin (*newPlugin))
                return {};

        if (ownerTrack != nullptr && ! ownerTrack->canContainPlugin (newPlugin.get()))
        {
            jassertfalse;
            return {};
        }

        auto numPlugins = size();

        if ((ownerClip != nullptr && numPlugins >= Edit::maxPluginsOnClip)
             || (ownerTrack != nullptr && numPlugins >= Edit::maxPluginsOnTrack)
             || (ownerTrack == nullptr && numPlugins >= Edit::maxNumMasterPlugins))
        {
            jassertfalse;
            return {};
        }

        int treeIndex = -1;

        if (index < 0 || index >= numPlugins)
        {
            if (auto sibling = list->objects.getLast())
            {
                jassert (sibling->state.isAChildOf (state));
                treeIndex = state.indexOf (sibling->state) + 1;
            }
        }
        else
        {
            if (auto sibling = list->objects[index])
            {
                jassert (sibling->state.isAChildOf (state));
                treeIndex = state.indexOf (sibling->state);
            }
        }

        newPlugin->removeFromParent();

        state.addChild (newPlugin->state, treeIndex, getUndoManager());
        jassert (list->objects.contains (newPlugin.get()));

        return newPlugin;
    }

    return {};
}

void PluginList::clear()
{
    for (int i = state.getNumChildren(); --i >= 0;)
        if (state.getChild(i).hasType (IDs::PLUGIN))
            state.removeChild (i, getUndoManager());
}

void PluginList::addPluginsFrom (const ValueTree& v, bool clearFirst, bool atStart)
{
    if (clearFirst)
        clear();

    if (! v.isValid())
        return;

    int index = atStart ? 0 : -1;

    for (int i = 0; i < v.getNumChildren(); ++i)
    {
        insertPlugin (v.getChild (i).createCopy(), index);
        index += atStart ? 1 : 0;
    }
}

void PluginList::addDefaultTrackPlugins (bool useVCA)
{
    jassert (list != nullptr);

    if (useVCA)
    {
        insertPlugin (VCAPlugin::create(), -1);
    }
    else
    {
        insertPlugin (VolumeAndPanPlugin::create(), -1);
        insertPlugin (LevelMeterPlugin::create(), -1);
    }

    if (edit.engine.getEngineBehaviour().arePluginsRemappedWhenTempoChanges())
    {
        if (auto vca = findFirstPluginOfType<VCAPlugin>())
            vca->remapOnTempoChange = true;

        if (auto vp = findFirstPluginOfType<VolumeAndPanPlugin>())
            vp->remapOnTempoChange = true;
    }
}

AudioNode* PluginList::createAudioNode (AudioNode* n, bool addNoise)
{
    jassert (list != nullptr);
    jassert (n != nullptr);
    int i = 0;

    if (list != nullptr)
        for (auto f : list->objects)
            if (! f->mustBePlayedLiveWhenOnAClip())
                n = f->createAudioNode (n, (i++ == 0) && addNoise);

    return n;
}

AudioNode* PluginList::attachNodesForPluginsNeedingLivePlay (AudioNode* n)
{
    jassert (list != nullptr);

    if (list != nullptr)
        for (auto f : list->objects)
            if (f->mustBePlayedLiveWhenOnAClip())
                n = f->createAudioNode (n, false);

    return n;
}
