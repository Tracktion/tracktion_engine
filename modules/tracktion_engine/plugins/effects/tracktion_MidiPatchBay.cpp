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

juce::ValueTree MidiPatchBayPlugin::create()
{
    return createValueTree (IDs::PLUGIN,
                            IDs::type, xmlTypeName);
}

const char* MidiPatchBayPlugin::xmlTypeName = "midipatchbay";

void MidiPatchBayPlugin::initialise (const PluginInitialisationInfo&) {}
void MidiPatchBayPlugin::deinitialise() {}

void MidiPatchBayPlugin::applyToBuffer (const PluginRenderContext& fc)
{
    if (fc.bufferForMidiMessages != nullptr)
    {
        SCOPED_REALTIME_CHECK

        if (auto* mma = fc.bufferForMidiMessages)
        {
            const juce::ScopedLock sl (mappingsLock);

            for (int i = mma->size(); --i >= 0;)
            {
                auto& m = (*mma)[i];

                auto newChan = juce::jlimit (0, 16, (int) currentMappings.map[m.getChannel()]);

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

    jassert (juce::isPositiveAndNotGreaterThan (srcChannel, 16));
    jassert (juce::isPositiveAndNotGreaterThan (dstChannel, 16));

    if (juce::isPositiveAndNotGreaterThan (srcChannel, 16)
         && juce::isPositiveAndNotGreaterThan (dstChannel, 16))
        mappings.map[srcChannel] = (char) dstChannel;

    setMappings (mappings);
}

void MidiPatchBayPlugin::valueTreeChanged()
{
    Plugin::valueTreeChanged();

    Mappings newMappings (getMappings());

    {
        const juce::ScopedLock sl (mappingsLock);
        currentMappings = newMappings;
    }

    changed();
}

MidiPatchBayPlugin::Mappings MidiPatchBayPlugin::getMappings() const
{
    Mappings m;
    auto saved = juce::StringArray::fromTokens (state[IDs::mappings].toString(), false);

    for (int i = 0; i < saved.size(); ++i)
        m.map[i + 1] = (char) saved[i].getIntValue();

    return m;
}

void MidiPatchBayPlugin::setMappings (const Mappings& newMappings)
{
    juce::String m;
    m.preallocateBytes (64);

    for (int i = 1; i < juce::numElementsInArray (newMappings.map); ++i)
        m << (int) newMappings.map[i] << ' ';

    state.setProperty (IDs::mappings, m.trimEnd(), getUndoManager());
}

void MidiPatchBayPlugin::resetMappings()
{
    Mappings m;

    for (int i = juce::numElementsInArray (m.map); --i >= 0;)
        m.map[i] = (char) i;

    setMappings (m);
}

void MidiPatchBayPlugin::blockAllMappings()
{
    Mappings m;

    for (int i = juce::numElementsInArray (m.map); --i >= 0;)
        m.map[i] = (char) blockChannel;

    setMappings (m);
}

void MidiPatchBayPlugin::sanityCheckMappings()
{
    Mappings m (getMappings());

    for (int i = juce::numElementsInArray (m.map); --i >= 1;)
    {
        int dst = m.map[i];
        jassert (juce::isPositiveAndNotGreaterThan (dst, 16));

        if (! juce::isPositiveAndNotGreaterThan (dst, 16))
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

}} // namespace tracktion { inline namespace engine
