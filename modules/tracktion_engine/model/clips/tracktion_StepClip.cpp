/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


struct StepClip::ChannelList  : public ValueTreeObjectList<StepClip::Channel>
{
    ChannelList (StepClip& c, const ValueTree& v)
        : ValueTreeObjectList<Channel> (v), clip (c)
    {
        rebuildObjects();
    }

    ~ChannelList()
    {
        freeObjects();
    }

    bool isSuitableType (const ValueTree& v) const override
    {
        return v.hasType (IDs::CHANNEL);
    }

    Channel* createNewObject (const ValueTree& v) override
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
StepClip::StepClip (const ValueTree& v, EditItemID id, ClipTrack& targetTrack)
    : Clip (v, targetTrack, id, Type::step)
{
    auto um = getUndoManager();
    channelList = new ChannelList (*this, state.getOrCreateChildWithName (IDs::CHANNELS, um));
    repeatSequence.referTo (state, IDs::repeatSequence, um);
    volumeDb.referTo (state, IDs::volDb, um, 0.0f);
    mute.referTo (state, IDs::mute, um, false);

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

        for (int i = 0; i < jmin (getChannels().size(), 8); ++i)
        {
            if (Channel* c = getChannels()[i])
            {
                c->noteNumber   = getDefaultNoteNumber (i);
                c->name         = MidiMessage::getRhythmInstrumentName (c->noteNumber);
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
        volumeDb        .setValue (other->volumeDb, nullptr);
        mute            .setValue (other->mute, nullptr);

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
    return patternInstanceList[repeatSeq ? (i % size) : jmin (size - 1, i)];
}

void StepClip::updatePatternList()
{
    auto newSequence = StringArray::fromTokens (state[IDs::sequence].toString(), ",", {});

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

void StepClip::valueTreePropertyChanged (ValueTree& v, const Identifier& i)
{
    Clip::valueTreePropertyChanged (v, i);

    if (i == IDs::sequence || i == IDs::repeatSequence)
        updatePatternList();

    changed();
}

void StepClip::valueTreeChildAdded (ValueTree& p, ValueTree& c)
{
    Clip::valueTreeChildAdded (p, c);

    if (p.hasType (IDs::PATTERN))
        changed();
}

void StepClip::valueTreeChildRemoved (ValueTree& p, ValueTree& c, int oldIndex)
{
    Clip::valueTreeChildRemoved (p, c, oldIndex);

    if (p.hasType (IDs::PATTERN))
        changed();
}

//==============================================================================
bool StepClip::canGoOnTrack (Track& t)
{
    return t.canContainMIDI();
}

String StepClip::getSelectableDescription()
{
    return TRANS("Step Clip") + " - \"" + getName() + "\"";
}

//==============================================================================
juce::Array<double> StepClip::getBeatTimesOfPatternStarts() const
{
    juce::Array<double> starts;

    auto& tempoSequence = edit.tempoSequence;
    auto pos = getPosition();

    auto patternStartBeats  = tempoSequence.timeToBeats (pos.getStartOfSource());
    auto endBeat            = tempoSequence.timeToBeats (pos.getEnd());
    auto ratio              = 1.0 / speedRatio;
    bool repeatSeq          = repeatSequence;

    for (int i = 0; ; ++i)
    {
        starts.add (patternStartBeats);

        double numBeats = 4.0;

        if (auto p = getPatternInstance (i, repeatSeq))
        {
            auto pattern = p->getPattern();
            numBeats = pattern.getNumNotes() * (pattern.getNoteLength() * ratio);
        }

        patternStartBeats += numBeats;

        if (patternStartBeats >= endBeat)
            break;
    }

    return starts;
}

double StepClip::getStartBeatOf (PatternInstance* instance)
{
    if (instance == nullptr
        || (instance != nullptr && ! patternInstanceList.contains (instance)))
    {
        jassertfalse;
        return 0.0;
    }

    auto index = patternInstanceList.indexOf (instance);
    return getBeatTimesOfPatternStarts()[index];
}

double StepClip::getEndBeatOf (PatternInstance* instance)
{
    if (instance == nullptr
        || (instance != nullptr && ! patternInstanceList.contains (instance)))
    {
        jassertfalse;
        return 0.0;
    }

    auto index = patternInstanceList.indexOf (instance) + 1;
    return getBeatTimesOfPatternStarts()[index];
}

int StepClip::getBeatsPerBar()
{
    return edit.tempoSequence.getTimeSigAt (getPosition().getStart()).numerator;
}

void StepClip::generateMidiSequenceForChannels (MidiMessageSequence& result,
                                                bool convertToSeconds, const Pattern& pattern,
                                                double startBeat, double clipStartBeat,
                                                double clipEndBeat, const TempoSequence& tempoSequence)
{
    CRASH_TRACER

    auto pos = getPosition();
    auto clipStartTime = convertToSeconds ? pos.getStart() : clipStartBeat;
    auto ratio = 1.0 / speedRatio;

    auto& gtm = edit.engine.getGrooveTemplateManager();
    auto numChannels = getChannels().size();
    auto numNotes = pattern.getNumNotes();
    auto noteLength = pattern.getNoteLength();

    OwnedArray<Pattern::CachedPattern> caches;
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
                    auto gate = cache->getGate (i);
                    auto beatEnd = startBeat + (noteLength * gate * ratio);
                    jassert (gate > 0.0);

                    auto start  = jmax (clipStartBeat, startBeat);
                    auto end    = jmin (clipEndBeat - 0.0001, beatEnd);

                    if (end > (start + 0.0001))
                    {
                        auto& c = *getChannels().getUnchecked (f);

                        if (convertToSeconds)
                        {
                            start = tempoSequence.beatsToTime (start) - clipStartTime;
                            end = tempoSequence.beatsToTime (end) - clipStartTime;

                            if (auto gt = gtm.getTemplateByName (c.grooveTemplate))
                            {
                                start = gt->editTimeToGroovyTime (start, edit);
                                end = gt->editTimeToGroovyTime (end, edit);
                            }
                        }
                        else
                        {
                            if (auto gt = gtm.getTemplateByName (c.grooveTemplate))
                            {
                                start = gt->beatsTimeToGroovyTime (start);
                                end = gt->beatsTimeToGroovyTime (end);
                            }
                        }

                        const float channelVelScale = c.noteValue / 127.0f;
                        const float vel = cache->getVelocity (i) / 127.0f;
                        jassert (c.channel.get().isValid());
                        auto chan = c.channel.get().getChannelNumber();
                        result.addEvent (MidiMessage::noteOn  (chan, c.noteNumber, vel * channelVelScale), start);
                        result.addEvent (MidiMessage::noteOff (chan, c.noteNumber, (uint8) jlimit (0, 127, c.noteValue.get())), end);
                    }
                }
            }
        }

        startBeat = currentBeatEnd;
    }
}

void StepClip::generateMidiSequence (MidiMessageSequence& result,
                                     const bool convertToSeconds,
                                     PatternInstance* const instance)
{
    if (instance != nullptr && ! patternInstanceList.contains (instance))
    {
        jassertfalse;
        return;
    }

    auto& tempoSequence = edit.tempoSequence;
    auto pos = getPosition();

    auto clipStartBeat = tempoSequence.timeToBeats (pos.getStart());
    auto clipEndBeat   = tempoSequence.timeToBeats (pos.getEnd());
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
                result.addTimeToMessages (-tempoSequence.beatsToTime (clipStartBeat + startBeat));
                break;
            }
        }
    }

    result.sort();
    result.updateMatchedPairs();
}

AudioNode* StepClip::createAudioNode (const CreateAudioNodeParams& params)
{
    CRASH_TRACER
    MidiMessageSequence sequence;
    generateMidiSequence (sequence);

    return new MidiAudioNode (std::move (sequence), { 1, 16 }, getEditTimeRange(), volumeDb, mute, *this,
                              getClipIfPresentInNode (params.audioNodeToBeReplaced, *this));
}

//==============================================================================
Colour StepClip::getDefaultColour() const
{
    return Colours::red.withHue (3.0f / 9.0f);
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

void StepClip::insertNewChannel (int index)
{
    if (getChannels().size() < maxNumChannels)
    {
        auto* um = getUndoManager();

        ValueTree v (IDs::CHANNEL);
        v.setProperty (IDs::channel, defaultMidiChannel, nullptr);
        v.setProperty (IDs::note, defaultNoteNumber, nullptr);
        v.setProperty (IDs::velocity, defaultNoteValue, nullptr);

        state.getOrCreateChildWithName (IDs::CHANNELS, getUndoManager()).addChild (v, index, um);

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
    StringArray s;

    for (auto* p : newSequence)
        s.add (String (p->patternIndex));

    state.setProperty (IDs::sequence, s.joinIntoString (","), getUndoManager());
}

//==============================================================================
void StepClip::insertVariation (int patternIndex, int insertIndex)
{
    PatternArray s (patternInstanceList);
    s.insert (insertIndex, new PatternInstance (*this, jlimit (0, getPatterns().size() - 1, patternIndex)));
    setPatternSequence (s);
}

void StepClip::removeVariation (int variationIndex)
{
    jassert (isPositiveAndBelow (variationIndex, patternInstanceList.size()));

    PatternArray s (patternInstanceList);
    s.remove (variationIndex);
    setPatternSequence (s);
}

void StepClip::removeAllVariations()
{
    state.setProperty (IDs::sequence, String(), getUndoManager());
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
    jassert (isPositiveAndBelow (variationIndex, patternInstanceList.size()));
    jassert (isPositiveAndBelow (newPatternIndex, getPatterns().size()));

    PatternArray s (patternInstanceList);
    s.set (variationIndex, new PatternInstance (*this, jlimit (0, getPatterns().size() - 1, newPatternIndex)));
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
    ValueTree v (IDs::PATTERN);
    v.setProperty (IDs::numNotes, getBeatsPerBar() * 4, nullptr);
    v.setProperty (IDs::noteLength, 0.25, nullptr);

    state.getOrCreateChildWithName (IDs::PATTERNS, getUndoManager())
         .addChild (v, index, getUndoManager());

    return index < 0 ? getPatterns().size() : index;
}

void StepClip::removeUnusedPatterns()
{
    auto patterns = state.getChildWithName (IDs::PATTERNS);
    juce::Array<int> usedPatterns;
    juce::Array<ValueTree> sequence;

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

    StringArray newSequence;

    for (int i = 0; i < sequence.size(); ++i)
    {
        const int index = patterns.indexOf (sequence.getUnchecked (i));

        if (index != -1)
            newSequence.add (String (index));
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
          && isPositiveAndBelow (channelIndex, getChannels().size()))
    {
        Pattern p (getPattern (patternIndex));

        if (isPositiveAndBelow (noteIndex, p.getNumNotes()))
            p.setNote (channelIndex, noteIndex, value);
    }
}
