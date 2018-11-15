/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


MidiPatchBayPlugin::MidiPatchBayPlugin (PluginCreationInfo info)  : Plugin (info)
{
    if (! state.hasProperty (IDs::mappings))
        resetMappings();

    sanityCheckMappings();
    currentMappings = getMappings();
}

MidiPatchBayPlugin::~MidiPatchBayPlugin()
{
    notifyListenersOfDeletion();
}

ValueTree MidiPatchBayPlugin::create()
{
    ValueTree v (IDs::PLUGIN);
    v.setProperty (IDs::type, xmlTypeName, nullptr);
    return v;
}

const char* MidiPatchBayPlugin::xmlTypeName = "midipatchbay";

void MidiPatchBayPlugin::initialise (const PlaybackInitialisationInfo&) {}
void MidiPatchBayPlugin::deinitialise() {}

void MidiPatchBayPlugin::applyToBuffer (const AudioRenderContext& fc)
{
    if (fc.bufferForMidiMessages != nullptr)
    {
        SCOPED_REALTIME_CHECK

        if (auto* mma = fc.bufferForMidiMessages)
        {
            const ScopedLock sl (mappingsLock);

            for (int i = mma->size(); --i >= 0;)
            {
                auto& m = (*mma)[i];

                auto newChan = jlimit (0, 16, (int) currentMappings.map[m.getChannel()]);

                if (newChan == blockChannel)
                    mma->remove (i);
                else
                    m.setChannel (newChan);
            }
        }
    }
}

//==============================================================================
void MidiPatchBayPlugin::makeConnection (int srcChannel, int dstChannel)
{
    Mappings mappings (getMappings());

    jassert (isPositiveAndNotGreaterThan (srcChannel, 16));
    jassert (isPositiveAndNotGreaterThan (dstChannel, 16));

    if (isPositiveAndNotGreaterThan (srcChannel, 16)
         && isPositiveAndNotGreaterThan (dstChannel, 16))
        mappings.map[srcChannel] = (char) dstChannel;

    setMappings (mappings);
}

void MidiPatchBayPlugin::valueTreeChanged()
{
    Plugin::valueTreeChanged();

    Mappings newMappings (getMappings());

    {
        const ScopedLock sl (mappingsLock);
        currentMappings = newMappings;
    }

    changed();
}

MidiPatchBayPlugin::Mappings MidiPatchBayPlugin::getMappings() const
{
    Mappings m;
    zeromem (m.map, sizeof (m.map));

    auto saved = StringArray::fromTokens (state[IDs::mappings].toString(), false);

    for (int i = 0; i < saved.size(); ++i)
        m.map[i + 1] = (char) saved[i].getIntValue();

    return m;
}

void MidiPatchBayPlugin::setMappings (const Mappings& newMappings)
{
    String m;
    m.preallocateBytes (64);

    for (int i = 1; i < numElementsInArray (newMappings.map); ++i)
        m << (int) newMappings.map[i] << ' ';

    state.setProperty (IDs::mappings, m.trimEnd(), getUndoManager());
}

void MidiPatchBayPlugin::resetMappings()
{
    Mappings m;

    for (int i = numElementsInArray (m.map); --i >= 0;)
        m.map[i] = (char) i;

    setMappings (m);
}

void MidiPatchBayPlugin::blockAllMappings()
{
    Mappings m;

    for (int i = numElementsInArray (m.map); --i >= 0;)
        m.map[i] = (char) blockChannel;

    setMappings (m);
}

void MidiPatchBayPlugin::sanityCheckMappings()
{
    Mappings m (getMappings());

    for (int i = numElementsInArray (m.map); --i >= 1;)
    {
        int dst = m.map[i];
        jassert (isPositiveAndNotGreaterThan (dst, 16));

        if (! isPositiveAndNotGreaterThan (dst, 16))
            dst = 0;

        m.map[i] = (char) dst;
    }

    setMappings (m);
}

std::pair<int, int> MidiPatchBayPlugin::getFirstMapping()
{
    int i = 0;

    for (auto m : getMappings().map)
    {
        if (m > 0 && m <= 16)
            return std::pair<int, int> (i, (int) m);

        ++i;
    }

    return std::pair<int, int> (-1, -1);
}
