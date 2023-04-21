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

inline void dumpSegments (const juce::Array<AudioSegmentList::Segment>& segments)
{

    DBG ("******************************************");
    for (auto& s : segments)
    {
        juce::String text;

        text += "Start: " + juce::String (s.start.inSeconds()) + "(" + juce::String (s.startSample) + ")\n";
        text += "Length: " + juce::String (s.length.inSeconds()) + "(" + juce::String (s.lengthSample) + ")\n";
        text += "Transpose: " + juce::String (s.transpose) + "\n";
        text += "===============================================";

        DBG(text);
    }
}

//==============================================================================
TimeRange AudioSegmentList::Segment::getRange() const                          { return { start, start + length }; }
SampleRange AudioSegmentList::Segment::getSampleRange() const                  { return { startSample, startSample + lengthSample }; }

float AudioSegmentList::Segment::getStretchRatio() const                       { return stretchRatio; }
float AudioSegmentList::Segment::getTranspose() const                          { return transpose; }

bool AudioSegmentList::Segment::hasFadeIn() const                              { return fadeIn; }
bool AudioSegmentList::Segment::hasFadeOut() const                             { return fadeOut; }

bool AudioSegmentList::Segment::isFollowedBySilence() const                    { return followedBySilence; }

HashCode AudioSegmentList::Segment::getHashCode() const
{
    return startSample
             ^ (lengthSample * 127)
             ^ (followedBySilence ? 1234 : 5432)
             ^ static_cast<HashCode> (stretchRatio * 1003.0f)
             ^ static_cast<HashCode> (transpose * 117.0f);
}

bool AudioSegmentList::Segment::operator== (const Segment& other) const
{
    return (start           == other.start &&
            length          == other.length &&
            startSample     == other.startSample &&
            lengthSample    == other.lengthSample &&
            stretchRatio    == other.stretchRatio &&
            transpose       == other.transpose &&
            fadeIn          == other.fadeIn &&
            fadeOut         == other.fadeOut);
}

bool AudioSegmentList::Segment::operator!= (const Segment& other) const
{
    return ! operator== (other);
}

//==============================================================================
AudioSegmentList::AudioSegmentList (AudioClipBase& acb) : clip (acb)
{
}

AudioSegmentList::AudioSegmentList (AudioClipBase& acb, bool relTime, bool shouldCrossfade)
    : clip (acb), relativeTime (relTime)
{
    if (shouldCrossfade)
        crossfadeTime = TimeDuration::fromSeconds (static_cast<double> (clip.edit.engine.getPropertyStorage().getProperty (SettingID::crossfadeBlock, 12.0 / 1000.0)));

    auto& pm = acb.edit.engine.getProjectManager();

    auto anyTakesValid = [&]
    {
        for (ProjectItemID m : clip.getTakes())
            if (pm.findSourceFile (m).existsAsFile())
                return true;

        return false;
    };

   #if JUCE_DEBUG
    auto f = pm.findSourceFile (clip.getSourceFileReference().getSourceProjectItemID());
    jassert (f == juce::File() || f == clip.getSourceFileReference().getFile());
   #endif

    if (clip.getCurrentSourceFile().existsAsFile() || anyTakesValid())
        build (shouldCrossfade);
}

static float calcStretchRatio (const AudioSegmentList::Segment& seg, double sampleRate)
{
    double srcSamples = sampleRate * seg.getRange().getLength().inSeconds();

    if (srcSamples > 0)
        return (float) (seg.getSampleRange().getLength() / srcSamples);

    return 1.0f;
}

std::unique_ptr<AudioSegmentList> AudioSegmentList::create (AudioClipBase& acb, bool relativeTime, bool crossFade)
{
    return std::unique_ptr<AudioSegmentList> (new AudioSegmentList (acb, relativeTime, crossFade));
}

std::unique_ptr<AudioSegmentList> AudioSegmentList::create (AudioClipBase& acb)
{
    return create (acb, acb.getWarpTimeManager(), acb.getWaveInfo(), acb.getLoopInfo());
}

std::unique_ptr<AudioSegmentList> AudioSegmentList::create (AudioClipBase& acb, const WarpTimeManager& wtm, const AudioFile& af)
{
    auto wi = af.getInfo();
    return create (acb, wtm, wi, wi.loopInfo);
}

std::unique_ptr<AudioSegmentList> AudioSegmentList::create (AudioClipBase& acb, const WarpTimeManager& wtm, const AudioFileInfo& wi, const LoopInfo& li)
{
    std::unique_ptr<AudioSegmentList> asl (new AudioSegmentList (acb));

    CRASH_TRACER
    auto in  = li.getInMarker();
    auto out = (li.getOutMarker() == -1) ? wi.lengthInSamples : li.getOutMarker();
    jassert (in <= out);

    if (in <= out)
    {
        TimeRange region (std::max (TimePosition(), wtm.getWarpedStart()),
                          wtm.getWarpEndMarkerTime());

        juce::Array<TimeRange> warpTimeRegions;
        callBlocking ([&] { warpTimeRegions = wtm.getWarpTimeRegions (region); });
        auto position = warpTimeRegions.size() > 0 ? warpTimeRegions.getUnchecked (0).getStart() : TimePosition();

        for (auto warpRegion : warpTimeRegions)
        {
            TimeRange sourceRegion (wtm.warpTimeToSourceTime (warpRegion.getStart()),
                                    wtm.warpTimeToSourceTime (warpRegion.getEnd()));

            Segment seg;

            seg.startSample    = tracktion::toSamples (sourceRegion.getStart(), wi.sampleRate) + in;
            seg.lengthSample   = tracktion::toSamples (sourceRegion.getEnd(), wi.sampleRate) + in - seg.startSample;
            seg.start          = position;
            seg.length         = warpRegion.getLength();
            seg.stretchRatio   = calcStretchRatio (seg, wi.sampleRate);
            seg.fadeIn         = false;
            seg.fadeOut        = false;
            seg.transpose      = 0.0f;

            position = position + warpRegion.getLength();
            jassert (seg.startSample >= in);
            jassert (seg.startSample + seg.lengthSample <= out);

            asl->segments.add (seg);
        }

        asl->crossfadeTime = 0.01s;
        asl->crossFadeSegments();
    }

    return asl;
}

bool AudioSegmentList::operator== (const AudioSegmentList& other) const noexcept
{
    return crossfadeTime == other.crossfadeTime
            && relativeTime == other.relativeTime
            && segments == other.segments;
}

bool AudioSegmentList::operator!= (const AudioSegmentList& other) const noexcept
{
    return ! operator== (other);
}

void AudioSegmentList::build (bool crossfade)
{
    if (clip.getAutoPitch() && clip.getAutoPitchMode() == AudioClipBase::chordTrackMono)
        if (auto pg = clip.getPatternGenerator())
            pg->getFlattenedChordProgression (progression, true);

    if (clip.getAutoTempo())
        buildAutoTempo (crossfade);
    else
        buildNormal (crossfade);

    if (relativeTime)
    {
        auto offset = toDuration (getStart());

        for (auto& s : segments)
            s.start = s.start - offset;
    }
}

void AudioSegmentList::chopSegment (Segment& seg, TimePosition at, int insertPos)
{
    Segment newSeg;

    newSeg.start  = at;
    newSeg.length = seg.getRange().getEnd() - newSeg.getRange().getStart();

    newSeg.transpose = getPitchAt (newSeg.start + 0.0001s);
    newSeg.stretchRatio = (float) clip.getSpeedRatio();

    newSeg.fadeIn  = true;
    newSeg.fadeOut = seg.fadeOut;

    newSeg.lengthSample = juce::roundToInt (seg.lengthSample * newSeg.length.inSeconds() / seg.length.inSeconds());
    newSeg.startSample  = seg.getSampleRange().getEnd() - newSeg.lengthSample;

    seg.length = seg.length - newSeg.length;
    seg.lengthSample = newSeg.startSample - seg.startSample;

    seg.fadeOut = true;
    seg.followedBySilence = false;

    jassert (newSeg.length > 0.01s);
    jassert (seg.length > 0.01s);

    segments.insert (insertPos, newSeg);
}

void AudioSegmentList::buildNormal (bool crossfade)
{
    CRASH_TRACER
    auto wi = clip.getWaveInfo();

    if (wi.sampleRate == 0.0)
        return;

    auto rate = clip.getSpeedRatio() * wi.sampleRate;
    auto clipPos = clip.getPosition();

    if (clip.isLooping())
    {
        auto clipLoopLen = clip.getLoopLength();

        if (clipLoopLen <= 0s)
            return;

        auto startSamp  = std::max ((SampleCount) 0, (SampleCount) (rate * clip.getLoopStart().inSeconds()));
        auto lengthSamp = std::max ((SampleCount) 0, (SampleCount) (rate * clipLoopLen.inSeconds()));

        for (int i = 0; ; ++i)
        {
            auto startTime = clipPos.getStart() + clipLoopLen * i - clipPos.getOffset();

            if (startTime >= clipPos.getEnd())
                break;

            auto end = startTime + clipLoopLen;

            if (end < clipPos.getStart())
                continue;

            Segment seg;

            seg.startSample = startSamp;
            seg.lengthSample = lengthSamp;

            if (startTime < clipPos.getStart())
            {
                auto diff = (SampleCount) ((clipPos.getStart() - startTime).inSeconds() * rate);

                seg.startSample += diff;
                seg.lengthSample -= diff;
                startTime = clipPos.getStart();
            }

            if (end > clipPos.getEnd())
            {
                auto diff = (SampleCount) ((end - clipPos.getEnd()).inSeconds() * rate);
                seg.lengthSample -= diff;
                end = clipPos.getEnd();
            }

            if (seg.lengthSample <= 0)
                continue;

            seg.start = startTime;
            seg.length = end - startTime;

            seg.transpose = getPitchAt (startTime + 0.0001s);
            seg.stretchRatio = (float) clip.getSpeedRatio();

            seg.fadeIn  = true;
            seg.fadeOut = true;
            seg.followedBySilence = true;

            if (! segments.isEmpty())
            {
                auto& prev = segments.getReference (segments.size() - 1);

                if (tracktion::abs (prev.getRange().getEnd() - seg.getRange().getStart()) < 0.01s)
                    prev.followedBySilence = false;
            }

            segments.add (seg);
        }

        if (! segments.isEmpty())
        {
            segments.getReference (0).fadeIn = false;
            segments.getReference (segments.size() - 1).fadeOut = false;
        }
    }
    else
    {
        // not looped
        Segment seg;

        seg.start        = clipPos.getStart();
        seg.length       = clipPos.getLength();

        seg.startSample  = juce::jlimit ((SampleCount) 0, wi.lengthInSamples, (SampleCount) (clipPos.getOffset().inSeconds() * rate));
        seg.lengthSample = juce::jlimit ((SampleCount) 0, wi.lengthInSamples, (SampleCount) (clipPos.getLength().inSeconds() * rate));

        seg.transpose    = getPitchAt (clipPos.getStart() + 0.0001s);
        seg.stretchRatio = (float) clip.getSpeedRatio();

        seg.fadeIn       = false;
        seg.fadeOut      = false;

        seg.followedBySilence = true;

        if (seg.length > 0s)
            segments.add (seg);
    }

    // chop up an segments that have pitch changes in them
    if (clip.getAutoPitch())
    {
        auto& ps = clip.edit.pitchSequence;

        for (int i = 0; i < ps.getNumPitches(); ++i)
        {
            auto* pitch = ps.getPitch(i);
            jassert (pitch != nullptr);

            auto pitchTm = pitch->getPosition().getStart();

            if (pitchTm > getStart() + 0.01s && pitchTm < getEnd() - 0.01s)
            {
                for (int j = 0; j < segments.size(); ++j)
                {
                    auto& seg = segments.getReference (j);

                    if (seg.getRange().reduced (0.01s).contains (pitchTm)
                         && std::abs (getPitchAt (pitchTm) - getPitchAt (seg.getRange().getStart())) > 0.0001)
                    {
                        chopSegment (seg, pitchTm, j + 1);
                        break;
                    }
                }
            }
        }

        chopSegmentsForChords();
    }

    if (crossfade)
        crossFadeSegments();
}

void AudioSegmentList::chopSegmentsForChords()
{
    if (clip.getAutoPitchMode() == AudioClipBase::chordTrackMono && progression.size() > 0)
    {
        auto& ts = clip.edit.tempoSequence;

        BeatPosition pos;

        for (auto& p : progression)
        {
            auto chordTime = ts.toTime (pos);

            if (chordTime > getStart() + 0.01s && chordTime < getEnd() - 0.01s)
            {
                for (int j = 0; j < segments.size(); ++j)
                {
                    auto& seg = segments.getReference (j);

                    if (seg.getRange().reduced (0.01s).contains (chordTime))
                    {
                        chopSegment (seg, chordTime, j + 1);
                        break;
                    }
                }

            }

            pos = pos + p->lengthInBeats;
        }
    }
}

static juce::Array<SampleCount> findSyncSamples (const LoopInfo& loopInfo, SampleRange range)
{
    juce::Array<SampleCount> syncSamples;
    auto numLoopPoints = loopInfo.getNumLoopPoints();

    if (numLoopPoints == 0)
    {
        for (int i = 0; i < loopInfo.getNumBeats(); ++i)
            syncSamples.add ((SampleCount) (range.getLength() / (double) loopInfo.getNumBeats() * i + range.getStart() + 0.5));
    }
    else
    {
        for (int i = 0; i < numLoopPoints; ++i)
        {
            auto pos = loopInfo.getLoopPoint (i).pos;

            if (range.contains (pos))
                syncSamples.add (pos);
        }
    }

    if (! syncSamples.contains (range.getStart()))
        syncSamples.add (range.getStart());

    std::sort (syncSamples.begin(), syncSamples.end());
    return syncSamples;
}

static juce::Array<SampleCount> trimInitialSyncSamples (const juce::Array<SampleCount>& samples, SampleCount start)
{
    juce::Array<SampleCount> result;
    result.add (start);

    for (auto& s : samples)
        if (s > start)
            result.add (s);

    return result;
}

void AudioSegmentList::initialiseSegment (Segment& seg, BeatPosition startBeat, BeatPosition endBeat, double sampleRate)
{
    auto& ts = clip.edit.tempoSequence;
    seg.start = ts.toTime (startBeat);
    seg.length = ts.toTime (endBeat) - seg.start;
    seg.stretchRatio = calcStretchRatio (seg, sampleRate);
    seg.fadeIn = false;
    seg.fadeOut = false;
    seg.transpose = getPitchAt (seg.start + 0.0001s);
}

void AudioSegmentList::removeExtraSegments()
{
    for (int i = segments.size(); --i >= 0;)
    {
        auto& seg = segments.getReference (i);
        auto segTime = seg.getRange();
        auto clipTime = clip.getPosition().time;

        if (! segTime.overlaps (clipTime))
        {
            segments.remove(i);
        }
        else if (segTime.getStart() < clipTime.getEnd() && segTime.getEnd() > clipTime.getEnd())
        {
            auto oldLen       = seg.length;
            seg.length        = getEnd() - seg.start;
            auto ratio        = oldLen / seg.length;
            seg.lengthSample  = static_cast<SampleCount> (seg.lengthSample / ratio + 0.5);
        }
        else if (segTime.getStart() < clipTime.getStart() && segTime.getEnd() > clipTime.getStart())
        {
            auto oldLen       = seg.length;
            auto delta        = getStart() - segTime.getStart();
            seg.start         = seg.start + delta;
            seg.length        = seg.length - delta;
            auto ratio        = oldLen / segTime.getLength();
            auto oldEndSamp   = seg.getSampleRange().getEnd();
            seg.lengthSample  = static_cast<SampleCount> (seg.lengthSample / ratio + 0.5);
            seg.startSample   = oldEndSamp - seg.lengthSample;
        }
    }
}

void AudioSegmentList::mergeSegments (double sampleRate)
{
    for (int i = segments.size() - 1; i >= 1; --i)
    {
        auto& s1 = segments.getReference (i - 1);
        auto& s2 = segments.getReference (i);

        if (std::abs (s1.stretchRatio - s2.stretchRatio) < 0.0001
             && std::abs (s1.transpose - s2.transpose) < 0.0001
             && tracktion::abs (s1.start + s1.length - s2.start) < 0.0001s
             && s1.startSample + s1.lengthSample == s2.startSample)
        {
            s1.length = s1.length + s2.length;
            s1.lengthSample += s2.lengthSample;
            s1.stretchRatio = calcStretchRatio (s1, sampleRate);

            segments.remove (i);
        }
    }
}

void AudioSegmentList::crossFadeSegments()
{
    for (int i = 0; i < segments.size(); ++i)
    {
        auto& s = segments.getReference(i);

        // fade out
        if (i < segments.size() - 1
             && (tracktion::abs (s.getRange().getEnd() - segments.getReference (i + 1).start) < 0.0001s))
        {
            auto oldLen = s.length;
            s.fadeOut = true;
            s.length = s.length + crossfadeTime;
            auto ratio = oldLen / s.length;
            s.lengthSample = static_cast<SampleCount> (s.lengthSample / ratio + 0.5);
            s.followedBySilence = false;
        }
        else
        {
            s.followedBySilence = true;
        }

        // fade in
        if (i > 0 && segments.getReference (i - 1).fadeOut)
            s.fadeIn = true;
    }
}

void AudioSegmentList::buildAutoTempo (bool crossfade)
{
    CRASH_TRACER
    auto wi = clip.getWaveInfo();
    auto& li = clip.getLoopInfo();

    SampleRange range (li.getInMarker(),
                       li.getOutMarker() == -1 ? wi.lengthInSamples
                                               : li.getOutMarker());

    if (range.isEmpty())
        return;

    auto& ts = clip.edit.tempoSequence;
    auto syncSamples = findSyncSamples (li, range);
    auto clipStartBeat = clip.getStartBeat();

    if (clip.isLooping())
    {
        auto loopLengthBeats = clip.getLoopLengthBeats();

        if (loopLengthBeats == BeatDuration())
            return;

        auto offsetBeat = clip.getOffsetInBeats();

        while (offsetBeat > loopLengthBeats)
            offsetBeat = offsetBeat - loopLengthBeats;

        if (tracktion::abs (offsetBeat).inBeats() < 0.00001)
            offsetBeat = BeatDuration();

        auto loopStartBeat = clip.getLoopStartBeats() + offsetBeat;

        auto offsetTime   = TimePosition::fromSeconds (loopStartBeat.inBeats() / li.getBeatsPerSecond (wi));
        auto offsetSample = tracktion::toSamples (offsetTime, wi.sampleRate) + range.getStart();

        auto syncSamplesSubset = trimInitialSyncSamples (syncSamples, offsetSample);

        BeatPosition beatPos;
        BeatPosition loopEndBeat = toPosition (loopLengthBeats) - offsetBeat;

        for (int i = 0; i < syncSamplesSubset.size(); ++i)
        {
            Segment seg;

            seg.startSample  = syncSamplesSubset[i];
            seg.lengthSample = ((i == syncSamplesSubset.size() - 1) ? (range.getEnd() - seg.startSample)
                                                                    : (syncSamplesSubset[i + 1]) - seg.startSample);

            auto startBeat = beatPos;
            beatPos = beatPos + BeatDuration::fromBeats (TimeDuration::fromSamples (seg.lengthSample, wi.sampleRate).inSeconds() * li.getBeatsPerSecond (wi));
            auto endBeat = beatPos;

            initialiseSegment (seg, clipStartBeat + toDuration (startBeat), clipStartBeat + toDuration (endBeat), wi.sampleRate);

            if (startBeat >= loopEndBeat)
                break;

            if (endBeat > loopEndBeat)
            {
                auto oldLength = endBeat     - startBeat;
                auto newLength = loopEndBeat - startBeat;

                seg.length = ts.toTime (clipStartBeat + toDuration (loopEndBeat)) - seg.start;
                seg.lengthSample = static_cast<SampleCount> (seg.lengthSample * (newLength / oldLength) + 0.5);

                jassert (seg.startSample >= range.getStart());
                jassert (seg.startSample + seg.lengthSample <= range.getEnd());
                segments.add (seg);
                break;
            }

            jassert (seg.startSample >= range.getStart());
            jassert (seg.startSample + seg.lengthSample <= range.getEnd());
            segments.add (seg);
        }

        loopStartBeat = clip.getLoopStartBeats();

        offsetTime   = TimePosition::fromSeconds (loopStartBeat.inBeats() / li.getBeatsPerSecond (wi));
        offsetSample = tracktion::toSamples (offsetTime, wi.sampleRate);

        syncSamplesSubset = trimInitialSyncSamples (syncSamples, offsetSample);

        beatPos = loopEndBeat;
        loopEndBeat = beatPos + loopLengthBeats;

        while (beatPos < toPosition (clip.getLengthInBeats()))
        {
            for (int i = 0; i < syncSamplesSubset.size(); ++i)
            {
                Segment seg;

                seg.startSample  = syncSamplesSubset[i];
                seg.lengthSample = ((i == syncSamplesSubset.size() - 1) ? (range.getEnd() - seg.startSample)
                                                                        : (syncSamplesSubset[i + 1]) - seg.startSample);

                auto startBeat = beatPos;
                beatPos = beatPos + BeatDuration::fromBeats ((seg.lengthSample / wi.sampleRate) * li.getBeatsPerSecond (wi));
                auto endBeat = beatPos;

                initialiseSegment (seg, clipStartBeat + toDuration (startBeat), clipStartBeat + toDuration (endBeat), wi.sampleRate);

                if (startBeat >= loopEndBeat)
                    break;

                if (endBeat > loopEndBeat)
                {
                    auto oldLength = endBeat     - startBeat;
                    auto newLength = loopEndBeat - startBeat;

                    seg.length = ts.toTime (clipStartBeat + toDuration (loopEndBeat)) - seg.start;
                    seg.lengthSample = static_cast<SampleCount> (seg.lengthSample * (newLength / oldLength) + 0.5);

                    jassert (seg.startSample >= range.getStart());
                    jassert (seg.startSample + seg.lengthSample <= range.getEnd());
                    segments.add (seg);
                    break;
                }

                jassert (seg.startSample >= range.getStart());
                jassert (seg.startSample + seg.lengthSample <= range.getEnd());
                segments.add (seg);
            }

            beatPos = loopEndBeat;
            loopEndBeat = beatPos + loopLengthBeats;
        }
    }
    else
    {
        auto offsetTime = TimeDuration::fromSeconds (clip.getOffsetInBeats().inBeats() / li.getBeatsPerSecond (wi));
        auto offsetSample = tracktion::toSamples (offsetTime, wi.sampleRate) + range.getStart();
        BeatPosition beatPos;

        syncSamples = trimInitialSyncSamples (syncSamples, offsetSample);

        for (int i = 0; i < syncSamples.size(); ++i)
        {
            Segment seg;

            seg.startSample  = syncSamples[i];
            seg.lengthSample = ((i == syncSamples.size() - 1) ? (range.getEnd() - seg.startSample)
                                                              : (syncSamples[i + 1]) - seg.startSample);

            auto startBeat = beatPos;
            beatPos = beatPos + BeatDuration::fromBeats ((seg.lengthSample / wi.sampleRate) * li.getBeatsPerSecond (wi));
            auto endBeat = beatPos;

            initialiseSegment (seg, clipStartBeat + toDuration (startBeat), clipStartBeat + toDuration (endBeat), wi.sampleRate);

            jassert (seg.startSample >= range.getStart());
            jassert (seg.startSample + seg.lengthSample <= range.getEnd());
            segments.add (seg);
        }
    }

    chopSegmentsForChords();
    removeExtraSegments();
    mergeSegments (wi.sampleRate);

    if (crossfade)
        crossFadeSegments();
}

TimePosition AudioSegmentList::getStart() const
{
    if (! segments.isEmpty())
        return segments.getReference (0).getRange().getStart();

    return 0.0s;
}

TimePosition AudioSegmentList::getEnd() const
{
    if (! segments.isEmpty())
        return segments.getReference (segments.size() - 1).getRange().getEnd();

    return 0.0s;
}

float AudioSegmentList::getPitchAt (TimePosition t)
{
    if (clip.getAutoPitch() && clip.getAutoPitchMode() == AudioClipBase::chordTrackMono && progression.size() > 0)
    {
        auto& ts = clip.edit.tempoSequence;

        auto& ps = clip.edit.pitchSequence;
        auto& pitchSetting = ps.getPitchAt (t);

        auto beat = ts.toBeats (t);
        BeatPosition pos;

        for (auto& p : progression)
        {
            if (beat >= pos && beat < pos + p->lengthInBeats)
            {
                int key = pitchSetting.getPitch() % 12;

                auto scale = pitchSetting.getScale();

                if (p->chordName.get().isNotEmpty())
                {
                    int scaleNote = key;
                    int chordNote = p->getRootNote (key, scale);

                    int delta = chordNote - scaleNote;

                    int transposeBase = scaleNote - (clip.getLoopInfo().getRootNote() % 12);

                    while (transposeBase > 6)  transposeBase -= 12;
                    while (transposeBase < -6) transposeBase += 12;

                    transposeBase += p->octave * 12;

                    return (float) (transposeBase + delta + clip.getTransposeSemiTones (false));
                }
            }

            pos = pos + p->lengthInBeats.get();
        }
    }

    if (clip.getAutoPitch())
    {
        auto& ps = clip.edit.pitchSequence;
        auto& pitchSetting = ps.getPitchAt (t);

        int pitch = pitchSetting.getPitch();
        int transposeBase = pitch - clip.getLoopInfo().getRootNote();

        while (transposeBase > 6)  transposeBase -= 12;
        while (transposeBase < -6) transposeBase += 12;

        return (float) (transposeBase + clip.getTransposeSemiTones (false));
    }

    return clip.getPitchChange();
}

}} // namespace tracktion { inline namespace engine
