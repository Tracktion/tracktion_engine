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

struct StepClip::ChannelList  : public ValueTreeObjectList<StepClip::Channel>
{
    ChannelList (StepClip& c, const juce::ValueTree& v)
        : ValueTreeObjectList<Channel> (v), clip (c)
    {
        rebuildObjects();
    }

    ~ChannelList() override
    {
        freeObjects();
    }

    bool isSuitableType (const juce::ValueTree& v) const override
    {
        return v.hasType (IDs::CHANNEL);
    }

    Channel* createNewObject (const juce::ValueTree& v) override
    {
        return new Channel (clip, v);
    }

    void deleteObject (Channel* c) override
    {
        delete c;
    }

    void newObjectAdded (Channel*) override {}
    void objectRemoved (Channel*) override {}
    void objectOrderChanged() override {}

    StepClip& clip;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChannelList)
};

//==============================================================================
StepClip::StepClip (const juce::ValueTree& v, EditItemID id, ClipTrack& targetTrack)
    : Clip (v, targetTrack, id, Type::step)
{
    auto um = getUndoManager();
    channelList.reset (new ChannelList (*this, state.getOrCreateChildWithName (IDs::CHANNELS, um)));
    repeatSequence.referTo (state, IDs::repeatSequence, um);
    level->dbGain.referTo (state, IDs::volDb, um, 0.0f);
    level->mute.referTo (state, IDs::mute, um, false);

    if (getChannels().isEmpty())
    {
        for (int i = defaultNumChannels; --i >= 0;)
            insertNewChannel (-1);

        auto getDefaultNoteNumber = [] (int chanNum)
        {
            switch (chanNum)
            {
                case 0:     return 36;
                case 1:     return 38;
                case 2:     return 42;
                case 3:     return 46;
                case 4:     return 39;
                case 5:     return 45;
                case 6:     return 50;
                case 7:     return 51;
                default:    return 60 + chanNum - 4;
            }
        };

        for (int i = 0; i < std::min (getChannels().size(), 8); ++i)
        {
            if (Channel* c = getChannels()[i])
            {
                c->noteNumber   = getDefaultNoteNumber (i);
                c->name         = juce::MidiMessage::getRhythmInstrumentName (c->noteNumber);
            }
        }
    }

    createDefaultPatternIfEmpty();
    updatePatternList();
}

StepClip::~StepClip()
{
    notifyListenersOfDeletion();
    channelList = nullptr;
}

void StepClip::cloneFrom (Clip* c)
{
    if (auto other = dynamic_cast<StepClip*> (c))
    {
        Clip::cloneFrom (other);

        repeatSequence  .setValue (other->repeatSequence, nullptr);
        level->dbGain   .setValue (other->level->dbGain, nullptr);
        level->mute     .setValue (other->level->mute, nullptr);

        auto chans = state.getChildWithName (IDs::CHANNELS);
        auto patterns = state.getChildWithName (IDs::PATTERNS);

        copyValueTree (chans, other->state.getChildWithName (IDs::CHANNELS), nullptr);
        copyValueTree (patterns, other->state.getChildWithName (IDs::PATTERNS), nullptr);

        state.setProperty (IDs::sequence, other->state[IDs::sequence], nullptr);

        Selectable::changed();
    }
}

const StepClip::PatternInstance::Ptr StepClip::getPatternInstance (int i, bool repeatSeq) const
{
    auto size = patternInstanceList.size();

    return patternInstanceList[repeatSeq ? (i % size)
                                         : std::min (size - 1, i)];
}

void StepClip::updatePatternList()
{
    auto newSequence = juce::StringArray::fromTokens (state[IDs::sequence].toString(), ",", {});

    PatternArray newArray (patternInstanceList);
    newArray.removeRange (newSequence.size(), newArray.size());

    for (int i = 0; i < newSequence.size(); ++i)
        if (newArray[i] == nullptr || newArray.getUnchecked (i)->patternIndex != newSequence[i].getIntValue())
            newArray.set (i, new PatternInstance (*this, newSequence[i].getIntValue()));

    patternInstanceList.swapWith (newArray);

    // make sure that removed items get deselected
    for (auto p : patternInstanceList)
        newArray.removeObject (p);

    for (auto p : newArray)
        p->deselect();

    sendChangeMessage();
}

void StepClip::valueTreePropertyChanged (juce::ValueTree& v, const juce::Identifier& i)
{
    Clip::valueTreePropertyChanged (v, i);

    if (i == IDs::sequence || i == IDs::repeatSequence)
        updatePatternList();

    changed();

    if (v.hasType (IDs::PATTERN))
    {
        for (auto patternInstance : patternInstanceList)
            if (v == patternInstance->getPattern().state)
                patternInstance->changed();
    }
    else if (v.hasType (IDs::CHANNEL))
    {
        for (auto channel : *channelList)
            if (v == channel->state)
                channel->changed();
    }
}

void StepClip::valueTreeChildAdded (juce::ValueTree& p, juce::ValueTree& c)
{
    Clip::valueTreeChildAdded (p, c);

    if (p.hasType (IDs::PATTERN))
        changed();
}

void StepClip::valueTreeChildRemoved (juce::ValueTree& p, juce::ValueTree& c, int oldIndex)
{
    Clip::valueTreeChildRemoved (p, c, oldIndex);

    if (p.hasType (IDs::PATTERN))
        changed();
}

void StepClip::valueTreeChildOrderChanged (juce::ValueTree& p, int o, int n)
{
    Clip::valueTreeChildOrderChanged (p, o, n);

    changed();
}

//==============================================================================
bool StepClip::canGoOnTrack (Track& t)
{
    return t.canContainMIDI();
}

juce::String StepClip::getSelectableDescription()
{
    return TRANS("Step Clip") + " - \"" + getName() + "\"";
}

//==============================================================================
juce::Array<BeatPosition> StepClip::getBeatTimesOfPatternStarts() const
{
    juce::Array<BeatPosition> starts;

    auto& tempoSequence = edit.tempoSequence;
    auto pos = getPosition();

    auto patternStartBeats  = tempoSequence.toBeats (pos.getStartOfSource());
    auto endBeat            = tempoSequence.toBeats (pos.getEnd());
    auto ratio              = 1.0 / speedRatio;
    bool repeatSeq          = repeatSequence;

    for (int i = 0; ; ++i)
    {
        starts.add (patternStartBeats);

        auto numBeats = BeatDuration::fromBeats (4.0);

        if (auto p = getPatternInstance (i, repeatSeq))
        {
            auto pattern = p->getPattern();
            numBeats = pattern.getNoteLength() * pattern.getNumNotes() * ratio;
        }

        patternStartBeats = patternStartBeats + numBeats;

        if (patternStartBeats >= endBeat)
            break;
    }

    return starts;
}

BeatPosition StepClip::getStartBeatOf (PatternInstance* instance)
{
    if (instance == nullptr
        || (instance != nullptr && ! patternInstanceList.contains (instance)))
    {
        jassertfalse;
        return BeatPosition();
    }

    auto index = patternInstanceList.indexOf (instance);
    return getBeatTimesOfPatternStarts()[index];
}

BeatPosition StepClip::getEndBeatOf (PatternInstance* instance)
{
    if (instance == nullptr
        || (instance != nullptr && ! patternInstanceList.contains (instance)))
    {
        jassertfalse;
        return BeatPosition();
    }

    auto index = patternInstanceList.indexOf (instance) + 1;
    return getBeatTimesOfPatternStarts()[index];
}

void StepClip::resizeClipForPatternInstances()
{
    if (patternInstanceList.getLast().get() != nullptr)
    {
        auto end = -getOffsetInBeats();

        auto ratio              = 1.0 / speedRatio;

        for (auto p : patternInstanceList)
        {
            auto pattern = p->getPattern();
            end = end + pattern.getNoteLength() * pattern.getNumNotes() * ratio;
        }

        if (end > getLengthInBeats())
        {
            auto& ts = edit.tempoSequence;
            auto pos = getPosition();
            auto startBeat = ts.toBeats (pos.getStart());
            auto endBeat = startBeat + end;

            setEnd (ts.toTime (endBeat), false);
        }
    }
}

int StepClip::getBeatsPerBar()
{
    return edit.tempoSequence.getTimeSigAt (getPosition().getStart()).numerator;
}

void StepClip::generateMidiSequenceForChannels (juce::MidiMessageSequence& result,
                                                bool convertToSeconds, const Pattern& pattern,
                                                BeatPosition startBeat, BeatPosition clipStartBeat,
                                                BeatPosition clipEndBeat, const TempoSequence& tempoSequence)
{
    CRASH_TRACER

    auto pos = getPosition();
    auto ratio = 1.0 / speedRatio;

    auto& gtm = edit.engine.getGrooveTemplateManager();
    auto numChannels = getChannels().size();
    auto numNotes = pattern.getNumNotes();
    auto noteLength = pattern.getNoteLength();

    juce::OwnedArray<Pattern::CachedPattern> caches;
    caches.ensureStorageAllocated (numChannels);

    for (int f = 0; f < numChannels; ++f)
        caches.add (new Pattern::CachedPattern (pattern, f));

    for (int i = 0; i < numNotes; ++i)
    {
        auto currentBeatEnd = startBeat + (noteLength * ratio);

        if (startBeat >= clipEndBeat)
            break;

        if (currentBeatEnd > clipStartBeat)
        {
            for (int f = 0; f < numChannels; ++f)
            {
                const auto* cache = caches.getUnchecked (f);

                if (cache == nullptr)
                {
                    jassertfalse;
                    continue;
                }

                if (cache->getNote (i))
                {
                    auto prob = cache->getProbability (i);

                    if (juce::Random::getSystemRandom().nextFloat() >= prob)
                        continue;

                    auto gate = cache->getGate (i);
                    auto beatEnd = startBeat + (noteLength * gate * ratio);
                    jassert (gate > 0.0);

                    auto start  = std::max (clipStartBeat, startBeat);
                    auto end    = std::min (clipEndBeat - BeatDuration::fromBeats (0.0001), beatEnd);

                    double eventStart = start.inBeats();
                    double eventEnd = end.inBeats();

                    if (end > (start + BeatDuration::fromBeats (0.0001)))
                    {
                        auto& c = *getChannels().getUnchecked (f);

                        if (convertToSeconds)
                        {
                            const auto startTime = tempoSequence.toTime (start) - toDuration (pos.getStart());
                            const auto endTime = tempoSequence.toTime (end) - toDuration (pos.getStart());

                            if (auto gt = gtm.getTemplateByName (c.grooveTemplate))
                            {
                                eventStart = gt->editTimeToGroovyTime (startTime, c.grooveStrength, edit).inSeconds();
                                eventEnd = gt->editTimeToGroovyTime (endTime, c.grooveStrength, edit).inSeconds();
                            }
                            else
                            {
                                eventStart = startTime.inSeconds();
                                eventEnd = endTime.inSeconds();
                            }
                        }
                        else
                        {
                            if (auto gt = gtm.getTemplateByName (c.grooveTemplate))
                            {
                                eventStart = gt->beatsTimeToGroovyTime (start, c.grooveStrength).inBeats();
                                eventEnd = gt->beatsTimeToGroovyTime (end, c.grooveStrength).inBeats();
                            }
                        }

                        const float channelVelScale = c.noteValue / 127.0f;
                        const float vel = cache->getVelocity (i) / 127.0f;
                        jassert (c.channel.get().isValid());
                        auto chan = c.channel.get().getChannelNumber();
                        result.addEvent (juce::MidiMessage::noteOn  (chan, c.noteNumber, vel * channelVelScale), eventStart);
                        result.addEvent (juce::MidiMessage::noteOff (chan, c.noteNumber, (uint8_t) juce::jlimit (0, 127, c.noteValue.get())), eventEnd);
                    }
                }
            }
        }

        startBeat = currentBeatEnd;
    }
}

void StepClip::generateMidiSequence (juce::MidiMessageSequence& result,
                                     bool convertToSeconds,
                                     PatternInstance* instance)
{
    if (instance != nullptr && ! patternInstanceList.contains (instance))
    {
        jassertfalse;
        return;
    }

    auto& tempoSequence = edit.tempoSequence;
    auto pos = getPosition();

    auto clipStartBeat = tempoSequence.toBeats (pos.getStart());
    auto clipEndBeat   = tempoSequence.toBeats (pos.getEnd());
    bool repeatSeq = repeatSequence;
    auto starts = getBeatTimesOfPatternStarts();

    for (int i = 0; i < starts.size(); ++i)
    {
        if (auto p = getPatternInstance (i, repeatSeq))
        {
            if (instance != nullptr && p.get() != instance)
                continue;

            auto startBeat = starts.getUnchecked (i);

            generateMidiSequenceForChannels (result, convertToSeconds, p->getPattern(), startBeat,
                                             clipStartBeat, clipEndBeat, tempoSequence);

            if (instance != nullptr)
            {
                result.addTimeToMessages (-tempoSequence.toTime (clipStartBeat + toDuration (startBeat)).inSeconds());
                break;
            }
        }
    }

    result.sort();
    result.updateMatchedPairs();
}

//==============================================================================
juce::Colour StepClip::getDefaultColour() const
{
    return juce::Colours::red.withHue (3.0f / 9.0f);
}

LiveClipLevel StepClip::getLiveClipLevel()
{
    return { level };
}

//==============================================================================
struct SelectedChannelIndex
{
    SelectedChannelIndex (int i, bool selected) noexcept
      : index (i), wasSelected (selected) {}

    const int index;
    const bool wasSelected;

    SelectedChannelIndex() = delete;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SelectedChannelIndex)
};

const juce::Array<StepClip::Channel*>& StepClip::getChannels() const noexcept
{
    return channelList->objects;
}

bool StepClip::usesProbability()
{
    for (auto seq : getPatternSequence())
    {
        auto p = seq->getPattern();
        for (int ch = 0; ch < getChannels().size(); ch++)
            for (auto v : p.getProbabilities (ch))
                if (v < 1.0f)
                    return true;
    }
    return false;
}

void StepClip::insertNewChannel (int index)
{
    if (getChannels().size() < maxNumChannels)
    {
        auto* um = getUndoManager();

        auto v = createValueTree (IDs::CHANNEL,
                                  IDs::channel, defaultMidiChannel,
                                  IDs::note, defaultNoteNumber,
                                  IDs::velocity, defaultNoteValue);

        state.getOrCreateChildWithName (IDs::CHANNELS, um)
             .addChild (v, index, um);

        for (auto& p : getPatterns())
            p.insertChannel (index);
    }
    else
    {
        edit.engine.getUIBehaviour().showWarningMessage (TRANS("This clip already contains the maximum number of channels!"));
    }
}

void StepClip::removeChannel (int index)
{
    if (auto* c = getChannels()[index])
    {
        state.getChildWithName (IDs::CHANNELS)
             .removeChild (c->state, getUndoManager());

        for (auto& p : getPatterns())
            p.removeChannel (index);
    }
}

StepClip::PatternArray StepClip::getPatternSequence() const
{
    return patternInstanceList;
}

void StepClip::setPatternSequence (const StepClip::PatternArray& newSequence)
{
    juce::StringArray s;

    for (auto* p : newSequence)
        s.add (juce::String (p->patternIndex));

    state.setProperty (IDs::sequence, s.joinIntoString (","), getUndoManager());
}

//==============================================================================
void StepClip::insertVariation (int patternIndex, int insertIndex)
{
    PatternArray s (patternInstanceList);
    s.insert (insertIndex, new PatternInstance (*this, juce::jlimit (0, getPatterns().size() - 1, patternIndex)));
    setPatternSequence (s);
}

void StepClip::removeVariation (int variationIndex)
{
    jassert (juce::isPositiveAndBelow (variationIndex, patternInstanceList.size()));

    PatternArray s (patternInstanceList);
    s.remove (variationIndex);
    setPatternSequence (s);
}

void StepClip::removeAllVariations()
{
    state.setProperty (IDs::sequence, juce::String(), getUndoManager());
}

void StepClip::createDefaultPatternIfEmpty()
{
    while (getChannels().size() < minNumChannels)
        insertNewChannel (getChannels().size());

    if (! state.getChildWithName (IDs::PATTERNS).isValid())
        insertNewPattern (-1);

    if (! state.hasProperty (IDs::sequence))
        insertVariation (0, -1);
}

void StepClip::setPatternForVariation (int variationIndex, int newPatternIndex)
{
    jassert (juce::isPositiveAndBelow (variationIndex, patternInstanceList.size()));
    jassert (juce::isPositiveAndBelow (newPatternIndex, getPatterns().size()));

    PatternArray s (patternInstanceList);
    s.set (variationIndex, new PatternInstance (*this, juce::jlimit (0, getPatterns().size() - 1, newPatternIndex)));
    setPatternSequence (s);
}

//==============================================================================
juce::Array<StepClip::Pattern> StepClip::getPatterns()
{
    juce::Array<Pattern> p;
    auto patterns = state.getChildWithName (IDs::PATTERNS);

    for (int i = 0; i < patterns.getNumChildren(); ++i)
    {
        auto v = patterns.getChild (i);

        if (v.hasType (IDs::PATTERN))
            p.add (Pattern (*this, v));
    }

    return p;
}

StepClip::Pattern StepClip::getPattern (int index)
{
    return Pattern (*this, state.getChildWithName (IDs::PATTERNS).getChild (index));
}

int StepClip::insertPattern (const Pattern& p, int index)
{
    state.getOrCreateChildWithName (IDs::PATTERNS, getUndoManager())
         .addChild (p.state.createCopy(), index, getUndoManager());

    return index < 0 ? getPatterns().size() : index;
}

int StepClip::insertNewPattern (int index)
{
    auto v = createValueTree (IDs::PATTERN,
                              IDs::numNotes, getBeatsPerBar() * 4,
                              IDs::noteLength, 0.25);

    state.getOrCreateChildWithName (IDs::PATTERNS, getUndoManager())
         .addChild (v, index, getUndoManager());

    return index < 0 ? getPatterns().size() : index;
}

void StepClip::removeUnusedPatterns()
{
    auto patterns = state.getChildWithName (IDs::PATTERNS);
    juce::Array<int> usedPatterns;
    juce::Array<juce::ValueTree> sequence;

    for (auto* p : patternInstanceList)
    {
        const int index = p->patternIndex;
        usedPatterns.addIfNotAlreadyThere (index);
        sequence.add (patterns.getChild (index));
    }

    auto um = getUndoManager();

    for (int i = patterns.getNumChildren(); --i >= 0;)
        if (! usedPatterns.contains (i))
            patterns.removeChild (i, um);

    juce::StringArray newSequence;

    for (int i = 0; i < sequence.size(); ++i)
    {
        const int index = patterns.indexOf (sequence.getUnchecked (i));

        if (index != -1)
            newSequence.add (juce::String (index));
    }

    state.setProperty (IDs::sequence, newSequence.joinIntoString (","), um);
}

//==============================================================================
bool StepClip::getCell (int patternIndex, int channelIndex, int noteIndex)
{
    return getPattern (patternIndex).getNote (channelIndex, noteIndex);
}

void StepClip::setCell (int patternIndex, int channelIndex,
                        int noteIndex, bool value)
{
    if (getCell (patternIndex, channelIndex, noteIndex) != value
          && juce::isPositiveAndBelow (channelIndex, getChannels().size()))
    {
        Pattern p (getPattern (patternIndex));

        if (juce::isPositiveAndBelow (noteIndex, p.getNumNotes()))
            p.setNote (channelIndex, noteIndex, value);
    }
}

}} // namespace tracktion { inline namespace engine
