/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/

LoopInfo::LoopInfo()  : state (IDs::LOOPINFO)
{
    initialiseMissingProps();
}

LoopInfo::LoopInfo (const File& f)  : state (IDs::LOOPINFO)
{
    auto& formatManager = Engine::getInstance().getAudioFileFormatManager();

    if (auto* af = formatManager.getFormatFromFileName (f))
    {
        if (auto* fin = f.createInputStream())
        {
            const ScopedPointer<AudioFormatReader> afr (af->createReaderFor (fin, true));
            init (Engine::getInstance(), afr, af);
        }
    }
}

LoopInfo::LoopInfo (const AudioFormatReader* afr, const AudioFormat* af)  : state (IDs::LOOPINFO)
{
    init (Engine::getInstance(), afr, af);
}

LoopInfo::LoopInfo (const juce::ValueTree& v, juce::UndoManager* u)
    : state (v), um (u), maintainParent (v.getParent().isValid())
{
    initialiseMissingProps();
}

LoopInfo::LoopInfo (const LoopInfo& other)
{
    *this = other;
}

LoopInfo& LoopInfo::operator= (const LoopInfo& o)
{
    return copyFrom (o.state);
}

//==============================================================================
double LoopInfo::getBpm (const AudioFileInfo& wi) const
{
    return getBeatsPerSecond (wi) * 60.0;
}

void LoopInfo::setBpm (double newBpm, double currentBpm)
{
    if (newBpm != currentBpm)
    {
        const double ratio = newBpm / currentBpm;
        setNumBeats (juce::jmax (1.0, getNumBeats() * ratio));
    }
}

void LoopInfo::setBpm (double newBpm, const AudioFileInfo& wi)
{
    // this is a bit of a round about way of calculating the number of beats which
    // will in turn update the BPM but should work until we determine a proper fix

    jassert (wi.sampleRate != 0.0);

    if (wi.sampleRate == 0.0)
        return;

    if (newBpm < 30.0 || newBpm > 1000.0)
        return;

    const double currentBpm = getBpm (wi);
    setBpm (newBpm, currentBpm);
}

double LoopInfo::getBeatsPerSecond (const AudioFileInfo& wi) const
{
    CRASH_TRACER

    if (wi.sampleRate == 0.0)
        return 2.0;

    int64 in  = getInMarker();
    int64 out = getOutMarker();

    if (out == -1 || out > wi.lengthInSamples)
        out = wi.lengthInSamples;

    if (out <= 0)
        return 2.0;

    const double length = (out - in) / wi.sampleRate;

    if (length <= 0)
        return 2.0;

    return getNumBeats() / length;
}

//==============================================================================
int LoopInfo::getDenominator() const              { return getProp<int> (IDs::denominator); }
int LoopInfo::getNumerator() const                { return getProp<int> (IDs::numerator); }
void LoopInfo::setDenominator (int dem)           { setProp (IDs::denominator, dem); }
void LoopInfo::setNumerator (int num)             { setProp (IDs::numerator, num); }
double LoopInfo::getNumBeats() const              { return getProp<double> (IDs::numBeats); }
void LoopInfo::setNumBeats (double b)             { setProp (IDs::numBeats, b); }

//==============================================================================
bool LoopInfo::isLoopable() const                  { const juce::ScopedLock sl (lock); return ! isOneShot() && getNumBeats() > 0.0 && getDenominator() > 0 && getNumerator() > 0; }
bool LoopInfo::isOneShot() const                   { return getProp<bool> (IDs::oneShot); }

//==============================================================================
int LoopInfo::getRootNote() const                  { return getProp<int> (IDs::rootNote); }
void LoopInfo::setRootNote (int note)              { setProp<int> (IDs::rootNote, note); }

//==============================================================================
juce::int64 LoopInfo::getInMarker() const          { return getProp<juce::int64> (IDs::inMarker); }
juce::int64 LoopInfo::getOutMarker() const         { return getProp<juce::int64> (IDs::outMarker); }

void LoopInfo::setInMarker (juce::int64 in)        { setProp<juce::int64> (IDs::inMarker, in); }
void LoopInfo::setOutMarker (juce::int64 out)      { setProp<juce::int64> (IDs::outMarker, out); }

//==============================================================================
int LoopInfo::getNumLoopPoints() const
{
    const juce::ScopedLock sl (lock);
    return getLoopPoints().getNumChildren();
}

LoopInfo::LoopPoint LoopInfo::getLoopPoint (int idx) const
{
    const juce::ScopedLock sl (lock);
    auto lp = getLoopPoints().getChild (idx);

    if (lp.isValid())
        return { lp.getProperty (IDs::value),
                 (LoopPointType) static_cast<int> (lp.getProperty (IDs::type)) };

    return {};
}

void LoopInfo::addLoopPoint (juce::int64 pos, LoopPointType type)
{
    const juce::ScopedLock sl (lock);
    duplicateIfShared();

    juce::ValueTree t (IDs::LOOPPOINT);
    t.setProperty (IDs::value, pos, nullptr);
    t.setProperty (IDs::type, (int) type, nullptr);
    getOrCreateLoopPoints().addChild (t, -1, nullptr);
}

void LoopInfo::changeLoopPoint (int idx, juce::int64 pos, LoopPointType type)
{
    const juce::ScopedLock sl (lock);
    duplicateIfShared();

    auto lp = getLoopPoints().getChild (idx);
    lp.setProperty (IDs::value, pos, um);
    lp.setProperty (IDs::type, (int) type, um);
}

void LoopInfo::deleteLoopPoint (int idx)
{
    const juce::ScopedLock sl (lock);
    duplicateIfShared();
    getLoopPoints().removeChild (idx, um);
    removeChildIfEmpty (IDs::LOOPPOINTS);
}

void LoopInfo::clearLoopPoints()
{
    const juce::ScopedLock sl (lock);
    duplicateIfShared();
    state.removeChild (getLoopPoints(), um);
}

void LoopInfo::clearLoopPoints (LoopPointType type)
{
    const juce::ScopedLock sl (lock);
    auto lps = getLoopPoints();

    for (int i = lps.getNumChildren(); --i >= 0;)
        if (((LoopPointType) int (lps.getChild (i).getProperty (IDs::type))) == type)
            lps.removeChild (i, nullptr);
}

int LoopInfo::getNumTags() const                { const juce::ScopedLock sl (lock); return getTags().getNumChildren(); }
void LoopInfo::clearTags()                      { const juce::ScopedLock sl (lock); duplicateIfShared(); state.removeChild (getTags(), um); }
juce::String LoopInfo::getTag (int idx) const   { const juce::ScopedLock sl (lock); return getTags().getChild (idx).getProperty (IDs::name).toString(); }

void LoopInfo::addTag (const juce::String& tag)
{
    const juce::ScopedLock sl (lock);
    duplicateIfShared();

    juce::ValueTree t (IDs::TAG);
    t.setProperty (IDs::name, tag, um);
    getOrCreateTags().addChild (t, -1, um);
}

void LoopInfo::addTags (const juce::StringArray& tags)
{
    for (const auto& t : tags)
        addTag (t);
}

void LoopInfo::initialiseMissingProps()
{
    const juce::ScopedLock sl (lock);
    setPropertyIfMissing (state, IDs::numBeats, 0.0, um);
    setPropertyIfMissing (state, IDs::denominator, 0, um);
    setPropertyIfMissing (state, IDs::numerator, 0, um);
    setPropertyIfMissing (state, IDs::oneShot, 0, um);
    setPropertyIfMissing (state, IDs::bpm, 0, um);
    setPropertyIfMissing (state, IDs::rootNote, -1, um);
    setPropertyIfMissing (state, IDs::inMarker, 0, um);
    setPropertyIfMissing (state, IDs::outMarker, -1, um);
}

LoopInfo& LoopInfo::copyFrom (const juce::ValueTree& o)
{
    const juce::ScopedLock sl (lock);
    copyValueTree (state, o, um);
    return *this;
}

void LoopInfo::removeChildIfEmpty (const juce::Identifier& i)
{
    auto v = state.getChildWithName (i);

    if (v.getNumChildren() == 0)
        state.removeChild (v, um);
}

juce::ValueTree LoopInfo::getLoopPoints() const       { const juce::ScopedLock sl (lock); return state.getChildWithName (IDs::LOOPPOINTS); }
juce::ValueTree LoopInfo::getOrCreateLoopPoints()     { const juce::ScopedLock sl (lock); return state.getOrCreateChildWithName (IDs::LOOPPOINTS, um); }

juce::ValueTree LoopInfo::getTags() const             { const juce::ScopedLock sl (lock); return state.getChildWithName (IDs::TAGS); }
juce::ValueTree LoopInfo::getOrCreateTags()           { const juce::ScopedLock sl (lock); return state.getOrCreateChildWithName (IDs::TAGS, um); }

void LoopInfo::duplicateIfShared()
{
    const juce::ScopedLock sl (lock);

    if (state.getReferenceCount() <= 1)
        return;

    if (maintainParent)
    {
        auto parent = state.getParent();
        int index = -1;

        if (parent.isValid())
        {
            index = parent.indexOf (state);
            parent.removeChild (index, um);
        }

        state = state.createCopy();

        if (parent.isValid())
            parent.addChild (state, index, um);
    }
    else
    {
        state = state.createCopy();
    }
}

static bool isSameFormat (const AudioFormat* af1, const AudioFormat* af2)
{
    return af2 != nullptr && af1->getFormatName() == af2->getFormatName();
}

void LoopInfo::init (Engine& engine, const AudioFormatReader* afr, const AudioFormat* af)
{
    if (afr == nullptr || af == nullptr)
        return;

    auto& formatManager = engine.getAudioFileFormatManager();
    const ScopedLock sl (lock);

   #if TRACKTION_ENABLE_REX
    if (isSameFormat (af, formatManager.getRexFormat()))
    {
        setDenominator (afr->metadataValues [RexAudioFormat::rexDenominator].getIntValue());
        setNumerator (afr->metadataValues [RexAudioFormat::rexDenominator].getIntValue());
        const double bpm = afr->metadataValues [RexAudioFormat::rexTempo].getDoubleValue();
        setProp (IDs::bpm, bpm);
        setProp (IDs::numBeats, bpm * ((afr->lengthInSamples / afr->sampleRate) / 60.0));

        StringArray beatPoints;
        beatPoints.addTokens (afr->metadataValues [RexAudioFormat::rexBeatPoints], ";", {});
        beatPoints.removeEmptyStrings();

        for (int i = 0; i < beatPoints.size(); ++i)
            addLoopPoint (beatPoints[i].getIntValue(), LoopInfo::LoopPointType::manual);
    }
    else
   #endif
    if (isSameFormat (af, formatManager.getAiffFormat()))
    {
        const int rootNote = afr->metadataValues [AiffAudioFormat::appleRootSet] == "1"
                                ? afr->metadataValues [AiffAudioFormat::appleRootNote].getIntValue() : -1;

        setRootNote (rootNote);
        setProp (IDs::oneShot, afr->metadataValues [AiffAudioFormat::appleOneShot] == "1");
        setDenominator (afr->metadataValues [AiffAudioFormat::appleDenominator].getIntValue());
        setNumerator (afr->metadataValues [AiffAudioFormat::appleNumerator].getIntValue());
        const double numBeats = afr->metadataValues [AiffAudioFormat::appleBeats].getDoubleValue();
        setProp (IDs::numBeats, numBeats);
        setProp (IDs::bpm, (numBeats * 60.0) / (afr->lengthInSamples / afr->sampleRate));

        StringArray t;
        t.addTokens (afr->metadataValues [AiffAudioFormat::appleTag], ";", {});
        t.add (afr->metadataValues [AiffAudioFormat::appleKey]);
        t.removeEmptyStrings();

        for (int i = 0; i < t.size(); ++i)
            addTag (t[i]);
    }
    else if (isSameFormat (af, formatManager.getWavFormat()))
    {
        String s = afr->metadataValues [WavAudioFormat::tracktionLoopInfo];

        if (s.isNotEmpty())
        {
            const ScopedPointer<XmlElement> n (XmlDocument::parse (s));

            if (n != nullptr)
                copyFrom (ValueTree::fromXml (*n));
        }
        else
        {
            const int rootNote = afr->metadataValues [WavAudioFormat::acidRootSet] == "1"
                                    ? afr->metadataValues [WavAudioFormat::acidRootNote].getIntValue() : -1;
            const double numBeats = afr->metadataValues [WavAudioFormat::acidBeats].getDoubleValue();

            setRootNote (rootNote);
            setNumBeats (numBeats);
            setProp (IDs::oneShot, afr->metadataValues [WavAudioFormat::acidOneShot] == "1");
            setDenominator (afr->metadataValues [WavAudioFormat::acidDenominator].getIntValue());
            setNumerator (afr->metadataValues [WavAudioFormat::acidNumerator].getIntValue());
            setNumBeats (afr->metadataValues [WavAudioFormat::acidBeats].getDoubleValue());
            setProp (IDs::bpm, (numBeats * 60.0) / (afr->lengthInSamples / afr->sampleRate));
        }
    }
    else if (isSameFormat (af, formatManager.getNativeAudioFormat()))
    {
        String s = afr->metadataValues [WavAudioFormat::tracktionLoopInfo];

        if (s.isNotEmpty())
        {
            const ScopedPointer<XmlElement> n (XmlDocument::parse (s));

            if (n != nullptr)
                copyFrom (ValueTree::fromXml (*n));
        }
        else
        {
            const String tempoString = afr->metadataValues ["tempo"];
            const double bpm = tempoString.getDoubleValue();

            setProp (IDs::bpm, bpm);

            if (tempoString.isNotEmpty())
            {
                const double fileDuration = afr->lengthInSamples / afr->sampleRate;
                setNumBeats ((fileDuration / 60.0) * bpm);
            }

            const String beatCount = afr->metadataValues ["beat count"];

            if (beatCount.isNotEmpty() && bpm == 0)
            {
                const double fileDuration = afr->lengthInSamples / afr->sampleRate;
                const int beats = beatCount.getIntValue();

                setProp (IDs::bpm, beats / fileDuration / 60.0);
                setNumBeats (beats);
            }

            const String timeSig (afr->metadataValues ["time signature"]);

            if (timeSig.isNotEmpty())
            {
                setDenominator (timeSig.upToFirstOccurrenceOf ("/", false, false).getIntValue());
                setNumerator (timeSig.fromFirstOccurrenceOf ("/", false, false).getIntValue());
            }

            const String keySig (afr->metadataValues ["key signature"]);

            if (keySig.isNotEmpty())
            {
                bool sharpOrFlat = keySig[1] == '#' || keySig[1] == 'b';
                setRootNote (Pitch::getPitchFromString (engine, keySig.substring (0, sharpOrFlat ? 2 : 1)));
                addTag (keySig.getLastCharacter() == 'm' ? "minor" : "major");
            }
        }
    }
    else if (afr->metadataValues.containsKey (WavAudioFormat::acidBeats) || afr->metadataValues.containsKey (WavAudioFormat::acidNumerator))
    {
        const int rootNote = afr->metadataValues [WavAudioFormat::acidRootSet] == "1"
                                ? afr->metadataValues [WavAudioFormat::acidRootNote].getIntValue() : -1;
        const double numBeats = afr->metadataValues [WavAudioFormat::acidBeats].getDoubleValue();

        setRootNote (rootNote);
        setNumBeats (numBeats);
        setProp (IDs::oneShot, afr->metadataValues [WavAudioFormat::acidOneShot] == "1");
        setDenominator (afr->metadataValues [WavAudioFormat::acidDenominator].getIntValue());
        setNumerator (afr->metadataValues [WavAudioFormat::acidNumerator].getIntValue());
        setNumBeats (afr->metadataValues [WavAudioFormat::acidBeats].getDoubleValue());
        setProp (IDs::bpm, (numBeats * 60.0) / (afr->lengthInSamples / afr->sampleRate));
    }

    initialiseMissingProps();
}
