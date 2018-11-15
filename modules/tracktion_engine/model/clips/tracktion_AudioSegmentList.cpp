/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


//==============================================================================
AudioSegmentList::Segment::Segment() = default;

EditTimeRange AudioSegmentList::Segment::getRange() const                      { return { start, start + length }; }
juce::Range<juce::int64> AudioSegmentList::Segment::getSampleRange() const     { return { startSample, startSample + lengthSample }; }

float AudioSegmentList::Segment::getStretchRatio() const                       { return stretchRatio; }
float AudioSegmentList::Segment::getTranspose() const                          { return transpose; }

bool AudioSegmentList::Segment::hasFadeIn() const                              { return fadeIn; }
bool AudioSegmentList::Segment::hasFadeOut() const                             { return fadeOut; }

bool AudioSegmentList::Segment::isFollowedBySilence() const                    { return followedBySilence; };

juce::int64 AudioSegmentList::Segment::getHashCode() const
{
    return startSample
             ^ (lengthSample * 127)
             ^ (followedBySilence ? 1234 : 5432)
             ^ ((juce::int64) (stretchRatio * 1003.0f))
             ^ ((juce::int64) (transpose * 117.0f));
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
        crossfadeTime = clip.edit.engine.getPropertyStorage().getProperty (SettingID::crossfadeBlock, 12.0 / 1000.0);

    auto& pm = *ProjectManager::getInstance();

    auto anyTakesValid = [&]
    {
        for (ProjectItemID m : clip.getTakes())
            if (pm.findSourceFile (m).existsAsFile())
                return true;

        return false;
    };

   #if JUCE_DEBUG
    auto f = pm.findSourceFile (clip.getSourceFileReference().getSourceProjectItemID());
    jassert (f == File() || f == clip.getSourceFileReference().getFile());
   #endif

    if (clip.getCurrentSourceFile().existsAsFile() || anyTakesValid())
        build (shouldCrossfade);
}

static float calcStretchRatio (const AudioSegmentList::Segment& seg, double sampleRate)
{
    double srcSamples = sampleRate * seg.getRange().getLength();

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
    int64 in  = li.getInMarker();
    int64 out = (li.getOutMarker() == -1) ? wi.lengthInSamples : li.getOutMarker();
    jassert (in <= out);

    if (in <= out)
    {
        EditTimeRange region (jmax (0.0, wtm.getWarpedStart()), wtm.getWarpEndMarkerTime());

        auto warpTimeRegions = wtm.getWarpTimeRegions (region);
        double position = warpTimeRegions.size() > 0 ? warpTimeRegions.getUnchecked (0).getStart() : 0.0;

        for (auto warpRegion : warpTimeRegions)
        {
            EditTimeRange sourceRegion (wtm.warpTimeToSourceTime (warpRegion.getStart()),
                                        wtm.warpTimeToSourceTime (warpRegion.getEnd()));

            Segment seg;

            seg.startSample    = int64 (sourceRegion.getStart() * wi.sampleRate + 0.5) + in;
            seg.lengthSample   = int64 (sourceRegion.getEnd() * wi.sampleRate + 0.5) + in - seg.startSample;
            seg.start          = position;
            seg.length         = warpRegion.getLength();
            seg.stretchRatio   = calcStretchRatio (seg, wi.sampleRate);
            seg.fadeIn         = false;
            seg.fadeOut        = false;
            seg.transpose      = 0.0f;

            position += warpRegion.getLength();
            jassert (seg.startSample >= in);
            jassert (seg.startSample + seg.lengthSample <= out);

            asl->segments.add (seg);
        }

        asl->crossfadeTime = 0.01;
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
    if (clip.getAutoTempo())
        buildAutoTempo (crossfade);
    else
        buildNormal (crossfade);

    if (relativeTime)
    {
        auto offset = getStart();

        for (auto& s : segments)
            s.start -= offset;
    }
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

        if (clipLoopLen <= 0)
            return;

        auto startSamp  = jmax ((int64) 0, (int64) (rate * clip.getLoopStart()));
        auto lengthSamp = jmax ((int64) 0, (int64) (rate * clipLoopLen));

        for (int i = 0; ; ++i)
        {
            auto startTime = clipPos.getStart() + i * clipLoopLen - clipPos.getOffset();

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
                auto diff = (int64) ((clipPos.getStart() - startTime) * rate);

                seg.startSample += diff;
                seg.lengthSample -= diff;
                startTime = clipPos.getStart();
            }

            if (end > clipPos.getEnd())
            {
                auto diff = (int64) ((end - clipPos.getEnd()) * rate);
                seg.lengthSample -= diff;
                end = clipPos.getEnd();
            }

            if (seg.lengthSample <= 0)
                continue;

            seg.start = startTime;
            seg.length = end - startTime;

            seg.transpose = getPitchAt (startTime + 0.0001);
            seg.stretchRatio = (float) clip.getSpeedRatio();

            seg.fadeIn  = true;
            seg.fadeOut = true;
            seg.followedBySilence = true;

            if (! segments.isEmpty())
            {
                auto& prev = segments.getReference (segments.size() - 1);

                if (std::abs (prev.getRange().getEnd() - seg.getRange().getStart()) < 0.01)
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

        seg.startSample  = jlimit ((int64) 0, wi.lengthInSamples, (int64) (clipPos.getOffset() * rate));
        seg.lengthSample = jlimit ((int64) 0, wi.lengthInSamples, (int64) (clipPos.getLength() * rate));

        seg.transpose    = getPitchAt (clipPos.getStart() + 0.0001);
        seg.stretchRatio = (float) clip.getSpeedRatio();

        seg.fadeIn       = false;
        seg.fadeOut      = false;

        seg.followedBySilence = true;

        if (seg.length > 0)
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

            if (pitchTm > getStart() + 0.01 && pitchTm < getEnd() - 0.01)
            {
                for (int j = 0; j < segments.size(); ++j)
                {
                    auto& seg = segments.getReference(j);

                    if (seg.getRange().reduced (0.01).contains (pitchTm)
                         && std::abs (getPitchAt(pitchTm) - getPitchAt (seg.getRange().start)) > 0.0001)
                    {
                        Segment newSeg;

                        newSeg.start  = pitchTm;
                        newSeg.length = seg.getRange().end - newSeg.getRange().start;

                        newSeg.transpose = getPitchAt (newSeg.start + 0.0001);
                        newSeg.stretchRatio = (float) clip.getSpeedRatio();

                        newSeg.fadeIn  = true;
                        newSeg.fadeOut = seg.fadeOut;

                        newSeg.lengthSample = roundToInt (seg.lengthSample * newSeg.length / seg.length);
                        newSeg.startSample  = seg.getSampleRange().getEnd() - newSeg.lengthSample;

                        seg.length = seg.length - newSeg.length;
                        seg.lengthSample = newSeg.startSample - seg.startSample;

                        seg.fadeOut = true;
                        seg.followedBySilence = false;

                        segments.insert (j + i, newSeg);
                        break;
                    }
                }
            }
        }
    }

    if (crossfade)
        crossFadeSegments();
}

static juce::Array<int64> findSyncSamples (const LoopInfo& loopInfo, juce::Range<juce::int64> range)
{
    juce::Array<int64> syncSamples;
    auto numLoopPoints = loopInfo.getNumLoopPoints();

    if (numLoopPoints == 0)
    {
        for (int i = 0; i < loopInfo.getNumBeats(); ++i)
            syncSamples.add ((juce::int64) (range.getLength() / (double) loopInfo.getNumBeats() * i + range.getStart() + 0.5));
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

static juce::Array<juce::int64> trimInitialSyncSamples (const juce::Array<juce::int64>& samples, juce::int64 start)
{
    juce::Array<juce::int64> result;
    result.add (start);

    for (auto& s : samples)
        if (s > start)
            result.add (s);

    return result;
}

void AudioSegmentList::initialiseSegment (Segment& seg, double startBeat, double endBeat, double sampleRate)
{
    auto& ts = clip.edit.tempoSequence;
    seg.start = ts.beatsToTime (startBeat);
    seg.length = ts.beatsToTime (endBeat) - seg.start;
    seg.stretchRatio = calcStretchRatio (seg, sampleRate);
    seg.fadeIn = false;
    seg.fadeOut = false;
    seg.transpose = getPitchAt (seg.start + 0.0001);
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
        else if (segTime.start < clipTime.end && segTime.end > clipTime.end)
        {
            auto oldLen       = seg.length;
            seg.length        = getEnd() - seg.start;
            auto ratio        = oldLen / seg.length;
            seg.lengthSample  = (int64) (seg.lengthSample / ratio + 0.5);
        }
        else if (segTime.start < clipTime.start && segTime.end > clipTime.start)
        {
            auto oldLen       = seg.length;
            auto delta        = getStart() - segTime.start;
            seg.start        += delta;
            seg.length       -= delta;
            auto ratio        = oldLen / segTime.getLength();
            auto oldEndSamp   = seg.getSampleRange().getEnd();
            seg.lengthSample  = (int64) (seg.lengthSample / ratio + 0.5);
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
             && std::abs (s1.start + s1.length - s2.start) < 0.0001
             && s1.startSample + s1.lengthSample == s2.startSample)
        {
            s1.length       += s2.length;
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
             && (std::abs (s.getRange().getEnd() - segments.getReference (i + 1).start) < 0.0001))
        {
            auto oldLen = s.length;
            s.fadeOut = true;
            s.length += crossfadeTime;
            auto ratio = oldLen / s.length;
            s.lengthSample = (int64) (s.lengthSample / ratio + 0.5);
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

    juce::Range<juce::int64> range (li.getInMarker(),
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

        if (loopLengthBeats == 0)
            return;

        auto offsetBeat = clip.getOffsetInBeats();

        while (offsetBeat > loopLengthBeats)
            offsetBeat -= loopLengthBeats;

        if (std::abs (offsetBeat) < 0.00001)
            offsetBeat = 0;

        auto loopStartBeat = clip.getLoopStartBeats() + offsetBeat;

        auto offsetTime   = loopStartBeat / li.getBeatsPerSecond (wi);
        auto offsetSample = (int64) (offsetTime * wi.sampleRate + 0.5) + range.getStart();

        auto syncSamplesSubset = trimInitialSyncSamples (syncSamples, offsetSample);

        double beatPos = 0;
        double loopEndBeat = loopLengthBeats - offsetBeat;

        for (int i = 0; i < syncSamplesSubset.size(); ++i)
        {
            Segment seg;

            seg.startSample  = syncSamplesSubset[i];
            seg.lengthSample = ((i == syncSamplesSubset.size() - 1) ? (range.getEnd() - seg.startSample)
                                                                    : (syncSamplesSubset[i + 1]) - seg.startSample);

            auto startBeat = beatPos;
            beatPos += (seg.lengthSample / wi.sampleRate) * li.getBeatsPerSecond (wi);
            auto endBeat = beatPos;

            initialiseSegment (seg, clipStartBeat + startBeat, clipStartBeat + endBeat, wi.sampleRate);

            if (startBeat >= loopEndBeat)
                break;

            if (endBeat > loopEndBeat)
            {
                auto oldLength = endBeat     - startBeat;
                auto newLength = loopEndBeat - startBeat;

                seg.length = ts.beatsToTime (clipStartBeat + loopEndBeat) - seg.start;
                seg.lengthSample = (int64) (seg.lengthSample * (newLength / oldLength) + 0.5);

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

        offsetTime   = loopStartBeat / li.getBeatsPerSecond (wi);
        offsetSample = (int64) (offsetTime * wi.sampleRate + 0.5);

        syncSamplesSubset = trimInitialSyncSamples (syncSamples, offsetSample);

        beatPos = loopEndBeat;
        loopEndBeat = beatPos + loopLengthBeats;

        while (beatPos < clip.getLengthInBeats())
        {
            for (int i = 0; i < syncSamplesSubset.size(); ++i)
            {
                Segment seg;

                seg.startSample  = syncSamplesSubset[i];
                seg.lengthSample = ((i == syncSamplesSubset.size() - 1) ? (range.getEnd() - seg.startSample)
                                                                        : (syncSamplesSubset[i + 1]) - seg.startSample);

                auto startBeat = beatPos;
                beatPos += (seg.lengthSample / wi.sampleRate) * li.getBeatsPerSecond (wi);
                auto endBeat = beatPos;

                initialiseSegment (seg, clipStartBeat + startBeat, clipStartBeat + endBeat, wi.sampleRate);

                if (startBeat >= loopEndBeat)
                    break;

                if (endBeat > loopEndBeat)
                {
                    auto oldLength = endBeat     - startBeat;
                    auto newLength = loopEndBeat - startBeat;

                    seg.length = ts.beatsToTime (clipStartBeat + loopEndBeat) - seg.start;
                    seg.lengthSample = (int64) (seg.lengthSample * (newLength / oldLength) + 0.5);

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
        auto offsetTime = clip.getOffsetInBeats() / li.getBeatsPerSecond (wi);
        auto offsetSample = (int64) (offsetTime * wi.sampleRate + 0.5) + range.getStart();
        double beatPos = 0;

        syncSamples = trimInitialSyncSamples (syncSamples, offsetSample);

        for (int i = 0; i < syncSamples.size(); ++i)
        {
            Segment seg;

            seg.startSample  = syncSamples[i];
            seg.lengthSample = ((i == syncSamples.size() - 1) ? (range.getEnd() - seg.startSample)
                                                              : (syncSamples[i + 1]) - seg.startSample);

            auto startBeat = beatPos;
            beatPos += (seg.lengthSample / wi.sampleRate) * li.getBeatsPerSecond (wi);
            auto endBeat = beatPos;

            initialiseSegment (seg, clipStartBeat + startBeat, clipStartBeat + endBeat, wi.sampleRate);

            jassert (seg.startSample >= range.getStart());
            jassert (seg.startSample + seg.lengthSample <= range.getEnd());
            segments.add (seg);
        }
    }

    removeExtraSegments();
    mergeSegments (wi.sampleRate);

    if (crossfade)
        crossFadeSegments();
}

double AudioSegmentList::getStart() const
{
    if (! segments.isEmpty())
        return segments.getReference (0).getRange().getStart();

    return 0.0;
}

double AudioSegmentList::getEnd() const
{
    if (! segments.isEmpty())
        return segments.getReference (segments.size() - 1).getRange().getEnd();

    return 0.0;
}

float AudioSegmentList::getPitchAt (double t)
{
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
