/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#include "../../playback/graph/tracktion_TracktionEngineNode.h"
#include "../../playback/graph/tracktion_TracktionNodePlayer.h"
#include "../../playback/graph/tracktion_WaveNode.h"
#include "../../playback/graph/tracktion_FadeInOutNode.h"
#include "../../playback/graph/tracktion_CombiningNode.h"


namespace tracktion { inline namespace engine
{

struct CompManager::RenderTrigger   : public ValueTreeAllEventListener,
                                      private juce::Timer
{
    RenderTrigger (CompManager& o) : owner (o)      { owner.takesTree.addListener (this); }
    ~RenderTrigger() override                       { owner.takesTree.removeListener (this); }

    void trigger()                                  { startTimer (10); }
    void valueTreeChanged() override                { trigger(); }

    void timerCallback() override
    {
        if (juce::Component::isMouseButtonDownAnywhere())
            return;

        stopTimer();
        owner.triggerCompRender();
    }

    CompManager& owner;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RenderTrigger)
};

//==============================================================================
CompManager::CompManager (Clip& c, const juce::ValueTree& v)
    : takesTree (v), clip (c)
{
    clip.edit.engine.getCompFactory().addComp (*this);

    renderTrigger = std::make_unique<RenderTrigger> (*this);

    lastOffset = clip.getPosition().getOffset().inSeconds();
    lastTimeRatio = getSourceTimeMultiplier();
}

CompManager::~CompManager()
{
    takesTree.removeListener (this);
    clip.state.removeListener (this);
    clip.edit.engine.getCompFactory().removeComp (*this);
}

void CompManager::initialise()
{
    jassert (takesTree.getParent() == clip.state);

    refreshCachedTakeLengths();
    lastRenderedTake = clip.getCurrentTake();
    setActiveTakeIndex (lastRenderedTake);
    lastHash = getTakeHash (lastRenderedTake);

    lastOffset = getOffset();
    lastTimeRatio = getSourceTimeMultiplier();

    takesTree.addListener (this);
    addOrRemoveListenerIfNeeded();
}

//==============================================================================
juce::ValueTree CompManager::getSection (int takeIndex, int sectionIndex) const
{
    return takesTree.getChild (takeIndex).getChild (sectionIndex);
}

juce::ValueTree CompManager::findSectionAtTime (double time)
{
    auto activeTake = getActiveTakeTree();

    if (! isTakeComp (activeTake))
        return activeTake;

    // Could use a binary chop search here for speed
    auto numSections = activeTake.getNumChildren();

    for (int i = 0; i < numSections; ++i)
    {
        auto section = activeTake.getChild (i);

        if (time < double (section.getProperty (IDs::endTime)))
            return section;
    }

    return {};
}

int CompManager::findSectionWithEndTime (juce::Range<double> timeRange, int takeIndex, bool& timeFoundAtStartOfSection) const
{
    auto activeTree = getActiveTakeTree();
    auto numSections = activeTree.getNumChildren();

    if (numSections == 0)
        return -1;

    auto section = activeTree.getChild (0);
    int index = int (section.getProperty (IDs::takeIndex));

    for (int i = 0; i < numSections; ++i)
    {
        double endTime = section.getProperty (IDs::endTime);
        auto nextSection = activeTree.getChild (i + 1);
        int nextSectionIndex = nextSection.getProperty (IDs::takeIndex);

        if (timeRange.contains (endTime))
        {
            if (takeIndex == -1 || takeIndex == index || takeIndex == nextSectionIndex)
            {
                timeFoundAtStartOfSection = (takeIndex == nextSectionIndex);
                return i;
            }
        }

        section = nextSection;
        index = nextSectionIndex;
    }

    return -1;
}

juce::Range<double> CompManager::getSectionTimes (const juce::ValueTree& section) const
{
    jassert (section.hasType (IDs::COMPSECTION));

    return { section.getSibling (-1).getProperty (IDs::endTime, 0.0),
             section.getProperty (IDs::endTime) };
}

//==============================================================================
void CompManager::setActiveTakeIndex (int index)
{
    jassert (index < takesTree.getNumChildren());

    if (! juce::isPositiveAndBelow (index, takesTree.getNumChildren()))
        return;

    if (getActiveTakeIndex() != index)
        clip.setCurrentTake (index);

    if (isTakeComp (index))
        triggerCompRender();
}

int CompManager::getNumTakes() const
{
    for (int i = 0; i < takesTree.getNumChildren(); ++i)
        if (isTakeComp (takesTree.getChild (i)))
            return i;

    return takesTree.getNumChildren();
}

//==============================================================================
juce::String CompManager::getTakeName (int index) const
{
    return clip.getTakeDescriptions()[index].fromFirstOccurrenceOf (". ", false, false);
}

//==============================================================================
juce::Range<double> CompManager::getCompRange() const
{
    auto endTime = clip.isLooping() ? getLoopLength()
                                    : (getMaxCompLength() * getSourceTimeMultiplier());
    return juce::Range<double> (0.0, endTime) - getOffset();
}

double CompManager::getSpeedRatio() const
{
    if (effectiveTimeMultiplier == 0.0)
        return 1.0;

    return 1.0 / effectiveTimeMultiplier;
}

HashCode CompManager::getTakeHash (int takeIndex) const
{
    auto takeTree = takesTree.getChild (takeIndex);

    if (! takeTree.isValid())
        return -1;

    auto hash = getBaseTakeHash (takeIndex);
    auto lastTime = -getOffset();

    for (int i = takeTree.getNumChildren(); --i >= 0;)
    {
        auto segment = takeTree.getChild (i);
        auto end = static_cast<double> (segment.getProperty (IDs::endTime));
        auto take = static_cast<int> (segment.getProperty (IDs::takeIndex));

        hash = hash
                ^ static_cast<HashCode> (take)
                ^ static_cast<HashCode> ((end - lastTime) * 1000.0);

        lastTime = end;
    }

    return hash;
}

//==============================================================================
void CompManager::changeSectionIndexAtTime (double time, int takeIndex)
{
    auto section = findSectionAtTime (time);

    if (! section.isValid())
        return;

    if (section.hasType (IDs::TAKE))
        setActiveTakeIndex (takeIndex);
    else if (section.hasType (IDs::COMPSECTION))
        section.setProperty (IDs::takeIndex, takeIndex, getUndoManager());

    clip.changed();
}

void CompManager::removeSectionIndexAtTime (double time, int takeIndex)
{
    auto section = findSectionAtTime (time);
    const int sectionIndex = section.getProperty (IDs::takeIndex);

    if (! section.hasType (IDs::COMPSECTION)
        || (takeIndex != -1 && sectionIndex != takeIndex))
        return;

    auto um = getUndoManager();
    auto previous = section.getSibling (-1);
    auto next = section.getSibling (1);
    bool remove = false;

    // First check if we need to remove two sections
    if (next.isValid() && previous.isValid())
    {
        if (int (previous.getProperty (IDs::takeIndex)) == sectionIndex
            && int (next.getProperty (IDs::takeIndex)) == sectionIndex)
        {
            section.getParent().removeChild (section, um);
            previous.getParent().removeChild (previous, um);
            clip.changed();
            return;
        }
    }

    // Otherwise we have to modify adjacent sections
    if (next.isValid())
    {
        if (int (next.getProperty (IDs::takeIndex)) != -1)
            section.setProperty (IDs::takeIndex, -1, um);
        else
            remove = true;
    }
    else if (previous.isValid())
    {
        remove = true;
    }

    if (remove)
        section.getParent().removeChild (section, um);

    clip.changed();
}

//==============================================================================
void CompManager::moveSectionEndTime (juce::ValueTree& section, double newTime)
{
    jassert (section.hasType (IDs::COMPSECTION));

    newTime = getCompRange().clipValue (newTime);
    const double currentEndTime = section.getProperty (IDs::endTime);

    if (currentEndTime != newTime)
    {
        removeSectionsWithinRange (juce::Range<double>::between (currentEndTime, newTime), section);
        section.setProperty (IDs::endTime, newTime, getUndoManager());
    }
}

void CompManager::moveSection (juce::ValueTree& section, double timeDelta)
{
    const double currentEndTime = section.getProperty (IDs::endTime);
    moveSectionToEndAt (section, currentEndTime + timeDelta);
}

void CompManager::moveSectionToEndAt (juce::ValueTree& section, double newEndTime)
{
    jassert (section.hasType (IDs::COMPSECTION));
    newEndTime = getCompRange().clipValue (newEndTime);

    const double currentEndTime = section.getProperty (IDs::endTime);
    auto previous = section.getSibling (-1);
    const double previousEndTime = previous.getProperty (IDs::endTime);
    const double timeDelta = newEndTime - currentEndTime;

    if (newEndTime > currentEndTime)
    {
        moveSectionEndTime (section, newEndTime);

        if (previous.isValid())
            moveSectionEndTime (previous, previousEndTime + timeDelta);
    }
    else if (newEndTime < currentEndTime)
    {
        if (previous.isValid())
            moveSectionEndTime (previous, previousEndTime + timeDelta);

        moveSectionEndTime (section, newEndTime);
    }
}

juce::ValueTree CompManager::addSection (int takeIndex, double endTime)
{
    if (! isTakeComp (getActiveTakeIndex()))
        addNewComp();

    if (! isTakeComp (getActiveTakeIndex()))
    {
        jassertfalse;
        return {};
    }

    auto activeTake = getActiveTakeTree();
    jassert (activeTake.isValid());
    int insertIndex = -1;

    for (const auto& section : activeTake)
    {
        ++insertIndex;
        const double sectionEnd = section.getProperty (IDs::endTime);

        if (sectionEnd > endTime)
            break;
    }

    auto* um = getUndoManager();
    juce::ValueTree newSection (IDs::COMPSECTION);
    newSection.setProperty (IDs::takeIndex, takeIndex, um);
    newSection.setProperty (IDs::endTime, endTime, um);
    activeTake.addChild (newSection, insertIndex, um);

    return newSection;
}

void CompManager::removeSection (const juce::ValueTree& section)
{
    jassert (section.hasType (IDs::COMPSECTION));
    section.getParent().removeChild (section, getUndoManager());
}

void CompManager::removeSectionsWithinRange (juce::Range<double> timeRange, const juce::ValueTree& sectionToKeep)
{
    auto takeTree = getActiveTakeTree();
    jassert (takeTree.hasType (IDs::TAKE) && isTakeComp (takeTree));

    for (int i = takeTree.getNumChildren(); --i >= 0;)
    {
        auto section = takeTree.getChild (i);
        const double endTime = section.getProperty (IDs::endTime);

        if (timeRange.getStart() <= endTime && endTime <= timeRange.getEnd())
        {
            if (section != sectionToKeep)
            {
                // Set the take index of the next section or it will jump as we remove the section
                if (sectionToKeep.isValid() && double (sectionToKeep[IDs::endTime]) > endTime)
                {
                    auto next = section.getSibling (1);

                    if (next.isValid())
                        next.setProperty (IDs::takeIndex, section[IDs::takeIndex], getUndoManager());
                }

                removeSection (section);
            }
        }

        if (endTime < timeRange.getStart())
            break;
    }
}

juce::ValueTree CompManager::splitSectionAtTime (double time)
{
    auto existingSection = findSectionAtTime (time);

    if (! existingSection.isValid())
        return {};

    auto currentSectionIndex = existingSection.getParent().indexOf (existingSection);

    auto* um = getUndoManager();
    auto newSection = existingSection.createCopy();
    newSection.setProperty (IDs::endTime, time, um);
    existingSection.getParent().addChild (newSection, currentSectionIndex, um);

    return newSection;
}

//==============================================================================
juce::UndoManager* CompManager::getUndoManager() const
{
    return &clip.edit.getUndoManager();
}

void CompManager::keepSectionsSortedAndInRange()
{
    auto activeTakeIndex = getActiveTakeIndex();
    auto takeTree = takesTree.getChild (activeTakeIndex);

    if (! takeTree.isValid())
        return;

    auto compRange = getCompRange();

    if (compRange.getLength() > 0.0)
    {
        // first remove all out of bounds
        for (int i = takeTree.getNumChildren(); --i >= 0;)
        {
            auto sectionTimes = getSectionTimes (takeTree.getChild (i));

            if (! (sectionTimes.getStart() < compRange.getEnd() && compRange.getStart() < sectionTimes.getEnd())
                || sectionTimes.getEnd() < compRange.getStart())
            {
                takeTree.removeChild (i, getUndoManager());
            }
        }

        // then truncate the last
        const int lastSectionIndex = takeTree.getNumChildren() - 1;
        auto lastSection = takeTree.getChild (lastSectionIndex);

        if (lastSection.isValid())
        {
            auto sectionTimes = getSectionTimes (lastSection);

            if (sectionTimes.getEnd() > compRange.getEnd())
                lastSection.setProperty (IDs::endTime, compRange.getEnd(), getUndoManager());
        }
    }

    struct EndTimeSorter
    {
        static int compareElements (const juce::ValueTree& first, const juce::ValueTree& second)
        {
            const double firstTime  = first[IDs::endTime];
            const double secondTime = second[IDs::endTime];

            return secondTime < firstTime ? 1 : (secondTime > firstTime ? -1 : 0);
        }
    };

    EndTimeSorter sorter;
    takeTree.sort (sorter, getUndoManager(), false);
}

juce::ValueTree CompManager::getNewCompTree() const
{
    juce::ValueTree newTake (IDs::TAKE);

    // Need to give the comp a blank section to draw onto
    auto newSection = createValueTree (IDs::COMPSECTION,
                                       IDs::takeIndex, -1,
                                       IDs::endTime, getMaxCompLength());

    newTake.addChild (newSection, -1, nullptr);

    return newTake;
}

void CompManager::reCacheInfo()
{
    refreshCachedTakeLengths();
    updateOffsetAndRatioFromSource();
    addOrRemoveListenerIfNeeded();
}

void CompManager::refreshCachedTakeLengths()
{
    displayWarning = false;
    const bool autoTempo = getAutoTempo();

    double speedRatio = 1.0;
    double maxSourceLength = 0.0;

    for (int i = getNumTakes(); --i >= 0;)
        maxSourceLength = std::max (maxSourceLength, getTakeLength (i));

    if (autoTempo)
    {
        // need to find the tempo changes between the source start and  how long the result will be
        const double sourceTempo = getSourceTempo();
        const double sourceStart = clip.getPosition().getStart().inSeconds();
        const double takeEnd = sourceStart + maxSourceLength - getOffset();

        auto& ts = clip.edit.tempoSequence;
        auto& tempoSetting = ts.getTempoAt (TimePosition::fromSeconds (sourceStart));

        displayWarning = &tempoSetting != &ts.getTempoAt (TimePosition::fromSeconds (takeEnd));
        speedRatio = tempoSetting.getBpm() / sourceTempo;
    }
    else
    {
        speedRatio = std::max (0.001, clip.getSpeedRatio());
    }

    effectiveTimeMultiplier = 1.0 / speedRatio;
    maxCompLength = maxSourceLength;
}

void CompManager::updateOffsetAndRatioFromSource()
{
    const double newOffset = getOffset();
    const double newRatio = getSourceTimeMultiplier();

    if (newOffset == lastOffset && newRatio == lastTimeRatio)
        return;

    const double ratioDiff = newRatio / lastTimeRatio;
    const double offsetDiff = lastOffset - newOffset;

    const int numTakes = getNumTakes();
    const juce::Range<int> compRange (numTakes, numTakes + getNumComps());

    for (int i = compRange.getStart(); i < compRange.getEnd(); ++i)
    {
        auto takeTree = takesTree.getChild (i);

        for (int s = takeTree.getNumChildren(); --s >= 0;)
        {
            auto segment = takeTree.getChild (s);
            double oldEnd = segment.getProperty (IDs::endTime);

            double newEnd = (ratioDiff != 1.0) ? ((oldEnd - offsetDiff) * ratioDiff) + offsetDiff
                                               : oldEnd + offsetDiff;

            segment.setProperty (IDs::endTime, newEnd, nullptr);
        }
    }

    lastOffset = newOffset;
    lastTimeRatio = newRatio;

    triggerCompRender();
}

void CompManager::addOrRemoveListenerIfNeeded()
{
    clip.state.removeListener (this);

    if (clip.hasAnyTakes())
        clip.state.addListener (this);
}

void CompManager::valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& id)
{
    if (tree != clip.state)
        return;

    if (! clip.hasAnyTakes())
        return;

    if (id == IDs::start || id == IDs::length || id == IDs::offset
        || id == IDs::loopStart || id == IDs::loopLength
        || id == IDs::loopStartBeats || id == IDs::loopLengthBeats
        || id == IDs::transpose || id == IDs::pitchChange
        || id == IDs::elastiqueMode || id == IDs::elastiqueOptions
        || id == IDs::autoPitch || id == IDs::autoTempo
        || id == IDs::warpTime || id == IDs::speed)
    {
        discardCachedData();
        refreshCachedTakeLengths();
        updateOffsetAndRatioFromSource();
        triggerCompRender();
    }
}

//==============================================================================
CompManager::Ptr CompFactory::getCompManager (Clip& clip)
{
    {
        const juce::ScopedLock sl (compLock);

        for (auto c : comps)
            if (&c->getClip() == &clip && c->getTakesTree().getParent() == clip.state)
                return c;
    }

    if (auto wac = dynamic_cast<WaveAudioClip*> (&clip))
        return new WaveCompManager (*wac);

    if (auto mc = dynamic_cast<MidiClip*> (&clip))
        return new MidiCompManager (*mc);

    jassertfalse;
    return {};
}

void CompFactory::addComp (CompManager& cm)
{
    const juce::ScopedLock sl (compLock);
    jassert (! comps.contains (&cm));
    comps.addIfNotAlreadyThere (&cm);
}

void CompFactory::removeComp (CompManager& cm)
{
    const juce::ScopedLock sl (compLock);
    jassert (comps.contains (&cm));
    comps.removeAllInstancesOf (&cm);
}

//==============================================================================
struct WaveCompManager::CompRenderContext
{
    CompRenderContext (Engine& e, const juce::Array<ProjectItemID> takesIDs_, const juce::ValueTree& takeTree_, int activeTakeIndex_,
                       double sourceTimeMultiplier_, double offset_, double maxLength_, double xFade)
        : engine (e), takesIDs (takesIDs_), takeTree (takeTree_.createCopy()),
          activeTakeIndex (activeTakeIndex_),
          sourceTimeMultiplier (sourceTimeMultiplier_), offset (offset_), maxLength (maxLength_), crossfadeLength (xFade)
    {
    }

    Engine& engine;
    juce::Array<ProjectItemID> takesIDs;
    const juce::ValueTree takeTree;
    const int activeTakeIndex;
    const double sourceTimeMultiplier, offset, maxLength, crossfadeLength;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CompRenderContext)
};

/** Re-calls flatten take to allow the comp time to finish rendering if needed. */
struct WaveCompManager::FlattenRetrier  : private juce::Timer
{
    FlattenRetrier (CompManager& o, int t, bool delFiles)
        : owner (o), takeIndex (t), deleteSourceFiles (delFiles)
    {
        startTimer (250);
    }

    void setIndex (int newTakeIndex) noexcept
    {
        takeIndex = newTakeIndex;
    }

private:
    CompManager& owner;
    int takeIndex;
    bool deleteSourceFiles;

    void timerCallback() override
    {
        owner.flattenTake (takeIndex, deleteSourceFiles);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FlattenRetrier)
};

/** Updates a strip during a comp render and notifies the Clip when it finishes. */
struct WaveCompManager::CompUpdater  : private Timer
{
    CompUpdater (Clip& c)
        : clip (c), compFile (c.edit.engine) {}

    void setStrip (juce::Component* s)
    {
        if (strip == s)
            return;

        strip = s;
        startTimer (updateInterval);
        shouldStop = false;
    }

    void setCompFile (const AudioFile& newCompFile)
    {
        compFile = newCompFile;
        startTimer (updateInterval);
        shouldStop = false;
    }

private:
    enum { updateInterval = 40 };

    Clip& clip;
    juce::Component::SafePointer<juce::Component> strip;
    AudioFile compFile;
    bool shouldStop = false;

    void timerCallback() override
    {
        if (strip != nullptr)
            strip->repaint();

        if (shouldStop)
        {
            clip.changed();
            stopTimer();
            return;
        }

        if (compFile.getFile() == juce::File()
             || ! clip.edit.engine.getAudioFileManager().proxyGenerator.isProxyBeingGenerated (compFile))
        {
            shouldStop = true;
        }
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CompUpdater)
};

//==============================================================================
WaveCompManager::WaveCompManager (WaveAudioClip& owner)
    : CompManager (owner, owner.state.getOrCreateChildWithName (IDs::TAKES, &owner.edit.getUndoManager())),
      clip (owner), lastCompFile (clip.edit.engine), compUpdater (new CompUpdater (clip))
{
    for (auto take : takesTree)
        if (isTakeComp (take))
            if (! ProjectItemID::fromProperty (take, IDs::source).isValid())
                take.setProperty (IDs::source, ProjectItemID::createNewID (clip.edit.getProjectItemID().getProjectID()).toString(), &owner.edit.getUndoManager());
}

WaveCompManager::~WaveCompManager() {}

void WaveCompManager::updateThumbnails (juce::Component& comp,
                                        juce::OwnedArray<SmartThumbnail>& thumbnails) const
{
    const int numTakes = getNumTakes();

    for (int i = 0; i < numTakes; ++i)
    {
        const AudioFile takeFile (getSourceFileForTake (i));

        if (takeFile.isNull())
        {
            if (thumbnails[i] != nullptr)
                thumbnails.set (i, nullptr);
            else
                thumbnails.add (nullptr);
        }
        else
        {
            if (auto existing = thumbnails[i])
                existing->setNewFile (takeFile);
            else
                thumbnails.add (new SmartThumbnail (clip.edit.engine, takeFile, comp, &clip.edit));
        }
    }
}

juce::File WaveCompManager::getCurrentCompFile() const
{
    if (lastHash != 0)
        return TemporaryFileManager::getFileForCachedCompRender (clip, lastHash).getFile();

    return {};
}

//==============================================================================
double WaveCompManager::getTakeLength (int takeIndex) const
{
    return getSourceFileForTake (takeIndex).getInfo().getLengthInSeconds();
}

double WaveCompManager::getOffset() const       { return clip.getPosition().getOffset().inSeconds(); }
double WaveCompManager::getLoopLength() const   { return clip.getLoopLength().inSeconds(); }
bool WaveCompManager::getAutoTempo()            { return clip.getAutoTempo(); }

double WaveCompManager::getSourceTempo()
{
    const AudioFileInfo info (getSourceFileForTake (0).getInfo());
    return clip.getLoopInfo().getBpm (info);
}

juce::String WaveCompManager::getWarning()
{
    juce::String message;
    const bool warnAboutReverse = clip.getIsReversed();

    if (shouldDisplayWarning() || warnAboutReverse)
    {
        if (shouldDisplayWarning())
            message << TRANS("When using multiple tempo changes comp editing will not be aligned with playback.")
                    << (warnAboutReverse ? "\n\n" : "");

        if (warnAboutReverse)
            message << TRANS("Only the rendered comp will be reversed. It is best to edit your comp forwards and then reverse the clip.");
    }

    return message;
}

void WaveCompManager::setStripToUpdate (juce::Component* strip)
{
    compUpdater->setStrip (strip);
}

float WaveCompManager::getRenderProgress() const
{
    return clip.edit.engine.getAudioFileManager().proxyGenerator.getProportionComplete (lastCompFile);
}

void WaveCompManager::triggerCompRender()
{
    if (isTakeComp (getActiveTakeIndex()))
    {
        keepSectionsSortedAndInRange();
        startTimer (compGeneratorDelay);
    }
}

void WaveCompManager::flattenTake (int takeIndex, bool deleteSourceFiles)
{
    if (getRenderProgress() < 1.0f || ! lastCompFile.isValid())
    {
        if (flattenRetrier == nullptr)
            flattenRetrier = std::make_unique<FlattenRetrier> (*this, takeIndex, deleteSourceFiles);
        else
            flattenRetrier->setIndex (takeIndex);

        return;
    }

    flattenRetrier = nullptr;

    auto takeTree = takesTree.getChild (takeIndex);

    if (auto item = getOrCreateProjectItemForTake (takeTree))
    {
        auto destCompFile = getDefaultTakeFile (takeIndex);

        if (! destCompFile.existsAsFile() || item->getSourceFile() != destCompFile)
        {
            jassert (destCompFile.getParentDirectory().hasWriteAccess());
            clip.edit.engine.getAudioFileManager().releaseFile (lastCompFile);

            if (lastCompFile.getFile().moveFileTo (destCompFile))
            {
                item->setSourceFile (destCompFile);
            }
            else if (lastCompFile.getFile().copyFileTo (destCompFile))
            {
                lastCompFile.deleteFile();
                item->setSourceFile (destCompFile);
            }
            else
            {
                jassertfalse;
                item->setSourceFile ({});

                clip.edit.engine.getUIBehaviour().showWarningAlert (TRANS("Problem flattening comp"),
                                                                    TRANS("There was a problem creating the comp file at XYYX, "
                                                                          "please ensure you have write access to this directory and try again.")
                                                                    .replace ("XYYX", "\"" + destCompFile.getFullPathName() + "\""));
                return;
            }
        }

        // Keep a local reference here to avoid re-creating it during the clear process
        CompManager::Ptr ptr = &clip.getCompManager();
        clip.getTakes()[takeIndex] = item->getID();
        clip.setCurrentTake (takeIndex);
        clip.deleteAllUnusedTakes (deleteSourceFiles);
        clip.getSourceFileReference().setToProjectFileReference (item->getID());
        clip.setShowingTakes (false);
    }
    else
    {
        jassertfalse;
    }
}

/** Pastes an existing comp to this manager and returns the newly added tree. */
juce::ValueTree WaveCompManager::pasteComp (const juce::ValueTree& compTree)
{
    if (! (compTree.hasType (IDs::TAKE) || compTree.hasProperty (IDs::isComp)))
    {
        jassertfalse;
        return {};
    }

    auto comp = compTree.createCopy();
    auto dest = isCurrentTakeComp() ? getActiveTakeTree() : addNewComp();
    comp.setProperty (IDs::source, dest[IDs::source], nullptr);
    copyValueTree (dest, comp, getUndoManager());

    return dest;
}

//==============================================================================
void WaveCompManager::setProjectItemIDForTake (int takeIndex, ProjectItemID newID) const
{
    if (takeIndex < clip.getTakes().size())
        clip.getTakes()[takeIndex] = newID;

    auto takeTree = takesTree.getChild (takeIndex);
    jassert (takeTree.isValid());

    if (takeTree.isValid())
        takeTree.setProperty (IDs::source, newID.toString(), getUndoManager());
}

ProjectItemID WaveCompManager::getProjectItemIDForTake (int takeIndex) const
{
    return clip.getTakes()[takeIndex];
}

AudioFile WaveCompManager::getSourceFileForTake (int takeIndex) const
{
    auto& e = clip.edit.engine;
    return AudioFile (e, e.getProjectManager().findSourceFile (getProjectItemIDForTake (takeIndex)));
}

juce::File WaveCompManager::getDefaultTakeFile (int takeIndex) const
{
    if (auto project = getProjectForEdit (clip.edit))
    {
        auto firstTakeItem = project->getProjectItemForID (clip.getTakes()[0]);

        if (firstTakeItem == nullptr)
            return {};

        int clipNum = 0;

        if (auto ct = clip.getClipTrack())
            clipNum = ct->getClips().indexOf (&clip) + 1;

        juce::String compName;
        compName << "_clip_" << clipNum << "_comp_" << (takeIndex - getNumTakes() + 2);

        auto firstTakeFile = firstTakeItem->getSourceFile();
        auto baseFileName = firstTakeFile.getFileNameWithoutExtension().upToFirstOccurrenceOf ("_take_", false, false);
        auto compFileName = baseFileName + compName;

        return firstTakeFile.existsAsFile()
                 ? firstTakeFile.getSiblingFile (compFileName)
                     .getNonexistentSibling().withFileExtension (firstTakeFile.getFileExtension())
                 : project->getDirectoryForMedia (ProjectItem::Category::recorded)
                     .getChildFile (compFileName).withFileExtension ("wav");
    }

    return {};
}

ProjectItem::Ptr WaveCompManager::getOrCreateProjectItemForTake (juce::ValueTree& takeTree)
{
    if (auto project = getProjectForEdit (clip.edit))
    {
        auto takeIndex = takeTree.getParent().indexOf (takeTree);

        if (auto item = project->getProjectItemForID (getProjectItemIDForTake (takeIndex)))
            return item;

        auto destCompFile = getDefaultTakeFile (takeIndex);

        if (auto item = project->createNewItem (destCompFile, ProjectItem::waveItemType(),
                                                destCompFile.getFileNameWithoutExtension(),
                                                {}, ProjectItem::Category::recorded, false))
        {
            // If we've had to create a new ProjectItem we need to update the take ProjectItemID to reflect it
            setProjectItemIDForTake (takeIndex, item->getID());
            return item;
        }
    }

    return {};
}

juce::ValueTree WaveCompManager::addNewComp()
{
    auto newTake = getNewCompTree();
    auto newID = ProjectItemID::createNewID (clip.edit.getProjectItemID().getProjectID());

    newTake.setProperty (IDs::source, newID.toString(), nullptr);
    newTake.setProperty (IDs::isComp, true, nullptr);

    // Add last so all the properties are set
    takesTree.addChild (newTake, -1, getUndoManager());
    clip.setCurrentTake (takesTree.getNumChildren() - 1);

    return newTake;
}

//==============================================================================
WaveCompManager::CompRenderContext* WaveCompManager::createRenderContext() const
{
    const double xFadeMs = clip.edit.engine.getPropertyStorage().getProperty (SettingID::compCrossfadeMs, 20.0);

    return new CompRenderContext (clip.edit.engine, clip.getTakes(), getActiveTakeTree(), getActiveTakeIndex(),
                                  getSourceTimeMultiplier(), getOffset(), getMaxCompLength(), xFadeMs / 1000.0);
}

bool WaveCompManager::renderTake (CompRenderContext& context, Edit& edit, AudioFileWriter& writer,
                                  juce::ThreadPoolJob& job, std::atomic<float>& progress)
{
    CRASH_TRACER

    const int blockSize = 32768;
    auto totalRange = TimeRange (0s, TimeDuration::fromSeconds (context.maxLength));
    auto crossfadeLength = TimeDuration::fromSeconds (context.crossfadeLength);
    auto halfCrossfade = crossfadeLength / 2.0;

    auto numSegments = context.takeTree.getNumChildren();
    auto timeRatio = context.sourceTimeMultiplier;
    auto offset = context.offset / timeRatio;
    double startTime = 0.0;

    tracktion::graph::PlayHead playHead;
    tracktion::graph::PlayHeadState playHeadState { playHead };
    ProcessState processState { playHeadState, edit.tempoSequence };

    auto combiningNode = std::make_unique<CombiningNode> (EditItemID(), processState);

    for (int i = 0; i < numSegments; ++i)
    {
        if (job.shouldExit())
            return false;

        auto compSegment = context.takeTree.getChild (i);
        auto takeIndex = (int) compSegment.getProperty (IDs::takeIndex);
        auto endTime = double (compSegment.getProperty (IDs::endTime)) / timeRatio;

        if (juce::isPositiveAndBelow (takeIndex, context.takesIDs.size()))
        {
            const ProjectItemID takeID (context.takesIDs[takeIndex]);
            jassert (takeID.isValid());

            auto takeFile = AudioFile (context.engine, context.engine.getProjectManager().findSourceFile (takeID));

            auto node = tracktion::graph::makeNode<WaveNode> (takeFile, totalRange, TimeDuration(), TimeRange(), LiveClipLevel(), 1.0,
                                                              juce::AudioChannelSet::canonicalChannelSet (takeFile.getInfo().numChannels),
                                                              juce::AudioChannelSet::stereo(),
                                                              processState,
                                                              EditItemID(), true);

            auto segmentRange = TimeRange (TimePosition::fromSeconds (startTime),
                                           TimePosition::fromSeconds (endTime)).expanded (halfCrossfade) + TimeDuration::fromSeconds (offset);

            TimeRange fadeIn, fadeOut;

            if (i != 0)
                fadeIn = TimeRange (segmentRange.getStart(), crossfadeLength);

            if (i != (numSegments - 1))
                fadeOut = TimeRange::endingAt (segmentRange.getEnd(), crossfadeLength);

            if (! (fadeIn.isEmpty() && fadeOut.isEmpty()))
                node = tracktion::graph::makeNode<FadeInOutNode> (std::move (node), processState, fadeIn, fadeOut,
                                                                  AudioFadeCurve::convex, AudioFadeCurve::convex, true);

            combiningNode->addInput (std::move (node), segmentRange.getIntersectionWith (totalRange));
        }

        startTime = endTime;
    }

    if (job.shouldExit())
        return false;

    auto sampleRate = writer.getSampleRate();
    auto nodePlayer = std::make_unique<TracktionNodePlayer> (std::move (combiningNode), processState, sampleRate, blockSize,
                                                             getPoolCreatorFunction (ThreadPoolStrategy::realTime));

    nodePlayer->setNumThreads (0);
    nodePlayer->prepareToPlay (sampleRate, (int) blockSize);

    auto audioBuffer = choc::buffer::ChannelArrayBuffer<float> ((choc::buffer::ChannelCount) writer.getNumChannels(),
                                                                (choc::buffer::FrameCount) blockSize + 256);
    MidiMessageArray midiBuffer;

    if (job.shouldExit())
        return false;

    // now perform the render
    const SampleCount totalSamples = juce::roundToInt (totalRange.getLength().inSeconds() * writer.getSampleRate());
    SampleCount samplesDone = 0;

    playHead.playSyncedToRange ({ 0, totalSamples });

    for (;;)
    {
        auto samplesToDo = (choc::buffer::FrameCount) std::min ((SampleCount) blockSize, (SampleCount) (totalSamples - samplesDone));

        if (samplesToDo <= 0)
            break;

        tracktion::graph::Node::ProcessContext pc
        {
            samplesToDo,
            juce::Range<int64_t> (samplesDone, samplesDone + samplesToDo),

            {
                audioBuffer.getStart (samplesToDo),
                midiBuffer
            }
        };

        pc.buffers.audio.clear();
        pc.buffers.midi.clear();

        auto misses = nodePlayer->process (pc);
        jassert (misses == 0); (void) misses;

        samplesDone += samplesToDo;
        progress = juce::jlimit (0.0f, 0.9f, (float) (0.9 * samplesDone / (double) totalSamples));

        if (job.shouldExit() || ! writer.isOpen())
            return false;

        juce::AudioBuffer<float> buffer (pc.buffers.audio.data.channels,
                                         (int) pc.buffers.audio.getNumChannels(),
                                         (int) pc.buffers.audio.getNumFrames());

        if (! writer.appendBuffer (buffer, buffer.getNumSamples()))
            break;
    }

    writer.closeForWriting();
    progress = 1.0f;

    return true;
}

//==============================================================================
class CompGeneratorJob  : public AudioProxyGenerator::GeneratorJob
{
public:
    using Ptr = juce::ReferenceCountedObjectPtr<GeneratorJob>;

    CompGeneratorJob (WaveAudioClip& wc, const AudioFile& comp)
        : GeneratorJob (comp), engine (wc.edit.engine), edit (wc.edit),
          context (wc.getCompManager().createRenderContext())
    {
        setName (TRANS("Creating Comp") + ": " + wc.getName());
    }

private:
    Engine& engine;
    Edit& edit;
    std::unique_ptr<WaveCompManager::CompRenderContext> context;

    bool render() override
    {
        CRASH_TRACER
        AudioFile tempFile (*proxy.engine,
                            proxy.getFile().getSiblingFile ("temp_comp_" + juce::String::toHexString (juce::Random::getSystemRandom().nextInt64()))
                            .withFileExtension (proxy.getFile().getFileExtension()));

        bool ok = render (tempFile);

        if (ok)
        {
            ok = proxy.deleteFile();
            (void) ok;
            jassert (ok);
            ok = tempFile.getFile().moveFileTo (proxy.getFile());
            jassert (ok);
        }

        tempFile.deleteFile();
        engine.getAudioFileManager().releaseFile (proxy);

        return ok;
    }

    bool render (const AudioFile& tempFile)
    {
        CRASH_TRACER

        // just use the first existing take as the basis for the comp render
        juce::File takeFile;

        for (auto& takeID : context->takesIDs)
        {
            takeFile = engine.getProjectManager().findSourceFile (takeID);

            if (takeFile.existsAsFile())
                break;
        }

        if (! takeFile.existsAsFile())
            return false;

        const AudioFile firstTakeAudioFile (engine, takeFile);
        AudioFileInfo sourceInfo (firstTakeAudioFile.getInfo());

        // need to strip AIFF metadata to write to wav files
        if (sourceInfo.metadata.getValue ("MetaDataSource", "None") == "AIFF")
            sourceInfo.metadata.clear();

        AudioFileWriter writer (tempFile, engine.getAudioFileFormatManager().getWavFormat(),
                                sourceInfo.numChannels, sourceInfo.sampleRate,
                                std::max (16, sourceInfo.bitsPerSample),
                                sourceInfo.metadata, 0);

        return writer.isOpen()
                && context != nullptr
                && WaveCompManager::renderTake (*context, edit, writer, *this, progress);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CompGeneratorJob)
};

static void beginCompGeneration (WaveAudioClip& clip, int takeIndex)
{
    CRASH_TRACER
    auto& cm = clip.getCompManager();

    clip.edit.engine.getAudioFileManager()
       .proxyGenerator.beginJob (new CompGeneratorJob (clip, TemporaryFileManager::getFileForCachedCompRender (clip, cm.getTakeHash (takeIndex))));
}

void WaveCompManager::timerCallback()
{
    stopTimer();

    if (! clip.hasAnyTakes())
    {
        lastHash = 0;
        return;
    }

    auto takeIndex = getActiveTakeIndex();
    auto hash = getTakeHash (takeIndex);

    if (isTakeComp (lastRenderedTake) && hash != lastHash)
    {
        // stops the last render job and deletes the source
        clip.edit.engine.getAudioFileManager().proxyGenerator
           .deleteProxy (TemporaryFileManager::getFileForCachedCompRender (clip, lastHash));
    }

    lastRenderedTake = takeIndex;
    lastHash = hash;

    lastCompFile = TemporaryFileManager::getFileForCachedCompRender (clip, lastHash);
    const bool isComp = isTakeComp (lastRenderedTake);

    if (isComp && (! lastCompFile.isValid()))
    {
        auto takeTree = takesTree.getChild (lastRenderedTake);
        beginCompGeneration (clip, lastRenderedTake);
        compUpdater->setCompFile (lastCompFile);
    }
    else if (! isComp)
    {
        lastHash = 0;
    }

    if (clip.getCurrentSourceFile() != lastCompFile.getFile())
    {
        clip.setCurrentSourceFile (lastCompFile.getFile());
        SelectionManager::refreshAllPropertyPanels();
    }
}


//==============================================================================
MidiCompManager::MidiCompManager (MidiClip& owner)
    : CompManager (owner, owner.state.getOrCreateChildWithName (IDs::COMPS, &owner.edit.getUndoManager())),
      clip (owner), midiTakes (owner.state.getChildWithName (IDs::TAKES))
{
    CRASH_TRACER

    // For MidiClips this needs to have a "COMPS" type or it will conflict with the actual takes
    // For MIDI comps the offset is taken care of by the tempo sequence calculations
    lastOffset = 0.0;
}

MidiCompManager::~MidiCompManager() {}

void MidiCompManager::initialise()
{
    CRASH_TRACER
    int takeIndex = 0;
    const int numTakes (clip.getNumTakes (true));
    auto um = getUndoManager();

    // Check current takes are up to date and add new ones
    for (int i = 0; i < numTakes; ++i)
    {
        auto takeTree = takesTree.getChild (takeIndex);

        if (! takeTree.isValid())
        {
            if (auto ml = clip.getTakeSequence (i))
            {
                juce::ValueTree newTake (IDs::TAKE);
                newTake.setProperty (IDs::isComp, ml->isCompList(), um);
                takesTree.addChild (newTake, takeIndex, um);
            }
        }

        ++takeIndex;
    }

    // Then remove any left over
    for (int i = takeIndex; i < takesTree.getNumChildren(); ++i)
        takesTree.removeChild (i, getUndoManager());

    lastHash = ! lastHash;
    CompManager::initialise();
}

MidiList* MidiCompManager::getSequenceLooped (int index)
{
    auto sourceSequence = clip.getTakeSequence (index);

    if (! clip.isLooping())
        return sourceSequence;

    if (auto ml = cachedLoopSequences[index])
        return ml;

    if (sourceSequence == nullptr)
        return {};

    return cachedLoopSequences.set (index, clip.createSequenceLooped (*sourceSequence).release());
}

//==============================================================================
HashCode MidiCompManager::getBaseTakeHash (int takeIndex) const
{
    return takeIndex
             ^ static_cast<HashCode> (clip.getLoopLengthBeats().inBeats() * 153.0)
             ^ static_cast<HashCode> (clip.getLoopStartBeats().inBeats() * 264.0);
}

double MidiCompManager::getTakeLength (int takeIndex) const
{
    if (clip.isLooping())
        return clip.getLoopLengthBeats().inBeats();

    if (auto ml = clip.getTakeSequence (takeIndex))
        return ml->getLastBeatNumber().inBeats();

    return 0.0;
}

void MidiCompManager::discardCachedData()
{
    cachedLoopSequences.clear();
}

void MidiCompManager::triggerCompRender()
{
    CRASH_TRACER

    if (! isTakeComp (getActiveTakeIndex()))
        return;

    keepSectionsSortedAndInRange();

    if (! clip.hasAnyTakes())
    {
        lastHash = 0;
        return;
    }

    auto takeIndex = getActiveTakeIndex();
    auto hash = getTakeHash (takeIndex);

    if (hash == lastHash)
        return;

    lastRenderedTake = takeIndex;
    lastHash = hash;

    if (isTakeComp (lastRenderedTake))
        createComp (getActiveTakeTree());
    else
        lastHash = 0;

    if (clip.getCurrentTake() != takeIndex)
        clip.setCurrentTake (takeIndex);

    clip.changed();
    clip.edit.restartPlayback();
}

void MidiCompManager::flattenTake (int takeIndex, bool /*deleteSourceFiles*/)
{
    if (clip.getCurrentTake() != takeIndex)
        clip.setCurrentTake (takeIndex);

    clip.deleteAllUnusedTakesConfirmingWithUser();
}

juce::ValueTree MidiCompManager::addNewComp()
{
    CRASH_TRACER
    auto newTake = getNewCompTree();
    newTake.setProperty (IDs::isComp, true, nullptr);

    // Now add the take to to the clip list
    juce::MidiMessageSequence blankSeq;
    clip.addTake (blankSeq, MidiList::NoteAutomationType::none);

    if (auto ml = clip.getTakeSequence (clip.getNumTakes (true) - 1))
        ml->setCompList (true);
    else
        jassertfalse;

    takesTree.addChild (newTake, -1, getUndoManager());
    clip.setCurrentTake (clip.getNumTakes (true) - 1);

    return newTake;
}

void MidiCompManager::createComp (const juce::ValueTree& takeTree)
{
    CRASH_TRACER

    if (! takeTree.isValid())
    {
        clip.edit.engine.getUIBehaviour().showWarningMessage (TRANS("There was a problem creating the MIDI comp"));
        return;
    }

    if (auto dest = clip.getTakeSequence (lastRenderedTake))
    {
        jassert (dest->isCompList());
        auto um = getUndoManager();
        dest->clear (um);

        const auto numSegments = takeTree.getNumChildren();
        const auto numTakes = getNumTakes();
        const auto loopStart = toDuration (clip.getLoopStartBeats());
        BeatPosition startBeat;

        for (int i = 0; i < numSegments; ++i)
        {
            auto compSegment = takeTree.getChild (i);
            const auto takeIndex = static_cast<int> (compSegment.getProperty (IDs::takeIndex));
            const auto endBeat = BeatPosition::fromBeats (static_cast<double> (compSegment.getProperty (IDs::endTime)));

            if (juce::isPositiveAndBelow (takeIndex, numTakes))
            {
                if (auto src = getSequenceLooped (takeIndex))
                {
                    const BeatRange beats (startBeat, endBeat);

                    for (auto n : src->getNotes())
                    {
                        auto nBeats = n->getRangeBeats();

                        if (nBeats.getStart() > endBeat)
                            break;

                        auto newRange = beats.getIntersectionWith (nBeats);

                        if (! newRange.isEmpty())
                        {
                            MidiNote newNote (n->state.createCopy());
                            newRange = newRange + loopStart;
                            newNote.setStartAndLength (newRange.getStart(), newRange.getLength(), nullptr);
                            dest->addNote (newNote, um);
                        }
                    }

                    for (auto e : src->getControllerEvents())
                    {
                        auto b = e->getBeatPosition();

                        if (b > endBeat)
                            break;

                        if (b > startBeat)
                            dest->addControllerEvent (b + loopStart, e->getType(), e->getControllerValue(), e->getMetadata(), um);
                    }

                    for (auto e : src->getSysexEvents())
                    {
                        auto b = e->getBeatPosition();

                        if (b > endBeat)
                            break;

                        if (b > startBeat)
                            dest->addSysExEvent (e->getMessage(), b + loopStart, um);
                    }
                }
                else
                {
                    jassertfalse;
                }
            }

            startBeat = endBeat;
        }

        dest->setCompList (true);
    }
}

}} // namespace tracktion { inline namespace engine
