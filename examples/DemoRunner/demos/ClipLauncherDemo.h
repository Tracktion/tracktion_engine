/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/


#pragma once

#include "../../common/BinaryData.h"
#include "../../modules/3rd_party/magic_enum/tracktion_magic_enum.hpp"

//==============================================================================
//==============================================================================
namespace utils
{
    static String organPatch = "<PLUGIN type=\"4osc\" windowLocked=\"1\" id=\"1069\" enabled=\"1\" filterType=\"1\" presetName=\"4OSC: Organ\" filterFreq=\"127.00000000000000000000\" ampAttack=\"0.60000002384185791016\" ampDecay=\"10.00000000000000000000\" ampSustain=\"100.00000000000000000000\" ampRelease=\"0.40000000596046447754\" waveShape1=\"4\" tune2=\"-24.00000000000000000000\" waveShape2=\"4\"> <MACROPARAMETERS id=\"1069\"/> <MODIFIERASSIGNMENTS/> <MODMATRIX/> </PLUGIN>";
    static String leadPatch = "<PLUGIN type=\"4osc\" windowLocked=\"1\" id=\"1069\" enabled=\"1\" filterType=\"1\" waveShape1=\"3\" filterFreq=\"100\"><MACROPARAMETERS id=\"1069\"/><MODIFIERASSIGNMENTS/><MODMATRIX/></PLUGIN>";
    static String synthPatch = R"(<PLUGIN type="4osc" windowLocked="1" id="1063" enabled="1" remapOnTempoChange="1" filterType="1" windowX="472" windowY="201" ampAttack="0.001000000047497451" ampDecay="8.886041641235352" ampSustain="0.0" ampRelease="0.04861049354076385" ampVelocity="31.03125" filterAttack="0.0" filterDecay="7.244740962982178" filterSustain="0.0" filterRelease="1.112721562385559" filterFreq="61.53281784057617" filterAmount="0.4973124265670776" reverbSize="0.6706874966621399" reverbWidth="1.0" reverbMix="0.0617656335234642" reverbOn="1" waveShape1="2" level2="-7.216049194335938" waveShape2="4" lfoRate1="3.49334454536438" presetName="4OSC: Synth" filterKey="0.0" masterLevel="-7.8839111328125" pulseWidth1="0.05802000313997269" tune1="-12.0" detune1="0.1634765565395355" tune2="-12.0" detune2="0.1625703126192093" filterResonance="13.36249542236328" filterVelocity="34.26718902587891" reverbDamping="0.6183750033378601" filterSlope="24" fineTune1="0.0" spread1="100.0" voices1="4" spread2="100.0" voices2="2" lfoWaveShape1="2" lfoDepth1="1.0" modAttack1="0.1000000014901161" modDecay1="0.1000000014901161" modSustain1="80.0" modRelease1="0.1000000014901161" modAttack2="0.1000000014901161" modDecay2="0.1000000014901161" modSustain2="80.0" modRelease2="0.1000000014901161"><MODMATRIX><MODMATRIXITEM modParam="filterFreq" modItem="cc1" modDepth="-0.3156406283378601"/><MODMATRIXITEM modParam="fineTune1" modItem="lfo1" modDepth="0.04657812789082527"/></MODMATRIX></PLUGIN>)";

    inline te::AudioTrack& addFourOscWithPatch (te::AudioTrack& at, juce::String patch)
    {
        if (auto p = dynamic_cast<te::FourOscPlugin*> (at.edit.getPluginCache().createNewPlugin (te::FourOscPlugin::xmlTypeName, {}).get()))
        {
            if (auto vt = juce::ValueTree::fromXml (patch); vt.isValid())
                p->restorePluginStateFromValueTree (vt);

            at.pluginList.insertPlugin (*p, 0, nullptr);
        }

        return at;
    }

    //==============================================================================
    template<typename ObjectType>
    struct AsyncValueTreeItem
    {
        using InitFunctionType = std::function<std::unique_ptr<ObjectType> (const juce::ValueTree&)>;

        AsyncValueTreeItem (const juce::ValueTree& v, InitFunctionType initFunc)
            : state (v), initFunction (std::move (initFunc))
        {
            assert (initFunction);
            initialiser.setFunction ([this] { object = initFunction (state); });
            initialiser.triggerAsyncUpdate();
        }

        ObjectType* getObject()
        {
            return object.get();
        }

        const juce::ValueTree state;

    private:
        InitFunctionType initFunction;
        te::AsyncCaller initialiser;
        std::unique_ptr<ObjectType> object;
    };

    //==============================================================================
    template<typename ContainerType>
    auto findObjectForState (const ContainerType& container, const juce::ValueTree& state)
        -> decltype (container.getFirst())
    {
        for (auto c : container)
            if (c->state == state)
                return c;

        return {};
    }

    //==============================================================================
    struct AsyncResizer
    {
        AsyncResizer (juce::Component& comp)
        {
            resizer.setFunction ([c = juce::Component::SafePointer<Component> (&comp)]
                                 {
                                     if (c)
                                         c->resized();
                                 });
        }

        void resizeAsync()
        {
            resizer.triggerAsyncUpdate();
        }

    private:
        te::AsyncCaller resizer;
    };

    //==============================================================================
    //==============================================================================
    inline void pauseForSyncPointChange (te::Edit& edit)
    {
        // Pause until the position change has gone through to the clips
        if (auto epc = edit.getTransport().getCurrentPlaybackContext())
            epc->blockUntilSyncPointChange();
    }

    inline te::TimePosition getStartTimeOfBar (te::TransportControl& tc, te::TimePosition t)
    {
        auto& ts = tc.edit.tempoSequence;
        const auto barsBeats = ts.toBarsAndBeats (t);
        return ts.toTime (te::tempo::BarsAndBeats { .bars = barsBeats.bars });
    }

    inline te::BeatDuration getLaunchOffset (const te::LaunchQuantisation& lq, const te::BeatPosition pos,
                                             std::optional<tracktion::BeatRange> loopRange)
    {
        // Quantise position first, ensuring not negative
        te::BeatPosition quantisedPos = pos;

        for (;;)
        {
            quantisedPos = lq.getNext (quantisedPos);

            if (quantisedPos >= 0_bp)
                break;

            quantisedPos = quantisedPos + 0.001_bd;
        }

        if (! loopRange || quantisedPos < loopRange->getEnd())
            return quantisedPos - pos;

        // If it's after the loop range, quantise the start loop position
        quantisedPos = lq.getNext (loopRange->getStart());

        // If it still doesn't contain the position, just use the loop start pos
        if (! loopRange->contains (quantisedPos))
            quantisedPos = loopRange->getStart();

        // Find the duration from the given position to the loop end + the quantised position from the loop start
        if (loopRange->contains (pos))
            return (loopRange->getEnd() - pos) + (quantisedPos - loopRange->getStart());

        return quantisedPos - pos;
    }

    inline te::LaunchQuantisation& getLaunchQuantisation (te::Clip& c)
    {
        if (! c.usesGlobalLaunchQuatisation())
            if (auto clipLQ = c.getLaunchQuantisation())
                return *clipLQ;

        return c.edit.getLaunchQuantisation();
    }

    inline te::MonotonicBeat getLaunchPosition (te::Edit& e, const te::LaunchQuantisation& lq, te::SyncPoint syncPoint)
    {
        auto& t = e.getTransport();
        auto& ts = e.tempoSequence;
        auto currentBeat = syncPoint.beat;
        const auto offset = getLaunchOffset (lq,
                                             currentBeat,
                                             t.looping.get() ? std::optional (ts.toBeats (t.getLoopRange()))
                                                             : std::nullopt);

        return { syncPoint.monotonicBeat.v + offset };
    }

    inline te::MonotonicBeat getLaunchPosition (te::Edit& e, const te::LaunchQuantisation& lq)
    {
        auto epc = e.getTransport().getCurrentPlaybackContext();

        if (! epc)
            return {};

        auto syncPoint = epc->getSyncPoint();

        if (! syncPoint)
            return {};

        return getLaunchPosition (e, lq, *syncPoint);
    }

    inline te::MonotonicBeat getLaunchPosition (te::Clip& c)
    {
        return getLaunchPosition (c.edit, getLaunchQuantisation (c));
    }

    inline te::MonotonicBeat getLaunchPosition (te::Edit& e)
    {
        return getLaunchPosition (e, e.getLaunchQuantisation());
    }

    inline te::MonotonicBeat getStopPosition (te::Clip& c)
    {
        return getLaunchPosition (c);
    }

    inline std::shared_ptr<te::LaunchHandle> getPlayingLaunchHandleOnTrack (te::AudioTrack& t)
    {
        for (auto s : t.getClipSlotList().getClipSlots())
            if (auto c = s->getClip())
                if (auto lh = c->getLaunchHandle())
                    if (lh->getPlayingStatus() == te::LaunchHandle::PlayState::playing)
                        return lh;

        return {};
    }

    inline std::shared_ptr<te::LaunchHandle> getPlayingOrQueuedLaunchHandleOnTrack (te::AudioTrack& t)
    {
        for (auto s : t.getClipSlotList().getClipSlots())
            if (auto c = s->getClip())
                if (auto lh = c->getLaunchHandle())
                    if (lh->getPlayingStatus() == te::LaunchHandle::PlayState::playing
                        || lh->getQueuedStatus() == te::LaunchHandle::QueueState::playQueued)
                       return lh;

        return {};
    }

    inline std::vector<te::Clip*> getPlayingOrQueuedClipsOnTrack (te::AudioTrack& t)
    {
        std::vector<te::Clip*> clips;

        for (auto s : t.getClipSlotList().getClipSlots())
            if (auto c = s->getClip())
                if (auto lh = c->getLaunchHandle())
                    if (lh->getPlayingStatus() == te::LaunchHandle::PlayState::playing
                        || lh->getQueuedStatus() == te::LaunchHandle::QueueState::playQueued)
                    {
                        clips.push_back(c);
                    }


        return clips;
    }

    inline std::vector<std::shared_ptr<te::LaunchHandle>> getPlayingOrQueuedLaunchHandlesOnTrack (te::AudioTrack& t)
    {
        std::vector<std::shared_ptr<te::LaunchHandle>> handles;

        for (auto s : t.getClipSlotList().getClipSlots())
            if (auto c = s->getClip())
                if (auto lh = c->getLaunchHandle())
                    if (lh->getPlayingStatus() == te::LaunchHandle::PlayState::playing
                        || lh->getQueuedStatus() == te::LaunchHandle::QueueState::playQueued)
                       handles.push_back (std::move (lh));

        return handles;
    }

    inline std::vector<te::Clip*> getPlayingOrQueuedClips (te::Edit& e)
    {
        std::vector<te::Clip*> clips;

        for (auto at : getAudioTracks (e))
        {
            auto activeClips = getPlayingOrQueuedClipsOnTrack (*at);
            std::move (activeClips.begin(), activeClips.end(), std::back_inserter (clips));
        }

        return clips;
    }

    inline std::vector<std::shared_ptr<te::LaunchHandle>> getPlayingOrQueuedLaunchHandles (te::Edit& e)
    {
        std::vector<std::shared_ptr<te::LaunchHandle>> handles;

        for (auto at : getAudioTracks (e))
        {
            auto trackHandles = getPlayingOrQueuedLaunchHandlesOnTrack (*at);
            std::move (trackHandles.begin(), trackHandles.end(), std::back_inserter (handles));
        }

        return handles;
    }

    enum class StartTransport
    {
        no,
        yes
    };

    inline void launchClip (te::Clip& c, bool stopOthers, StartTransport startTransport)
    {
        if (startTransport == StartTransport::yes
            && (! c.edit.getTransport().isPlaying()))
        {
            auto& tc = c.edit.getTransport();
            tc.setPosition (getStartTimeOfBar (tc, tc.startPosition));
            pauseForSyncPointChange (c.edit);
        }

        if (stopOthers)
        {
            if (auto at = dynamic_cast<te::AudioTrack*> (c.getTrack()))
            {
                for (auto slot : at->getClipSlotList().getClipSlots())
                {
                    if (slot != nullptr)
                    {
                        if (auto otherClip = slot->getClip())
                        {
                            if (&c == otherClip)
                                launchClip (c, false, StartTransport::no);
                            else if (auto lh = otherClip->getLaunchHandle())
                                lh->stop (c.edit.getTransport().isPlaying() ? std::make_optional (getStopPosition (*otherClip)) : std::nullopt);
                        }
                    }
                }
            }
        }
        else
        {
            auto lh = c.getLaunchHandle();
            lh->play (getLaunchPosition (c));
        }

        if (startTransport == StartTransport::yes)
            if (! c.edit.getTransport().isPlaying())
                c.edit.getTransport().play (false);
    }

    inline void launchScene (te::Scene& s, te::Track* t = nullptr)
    {
        const int sceneIndex = s.sceneList.getScenes().indexOf (&s);

        juce::Array<te::AudioTrack*> tracks;
        if (t && t->isFolderTrack())
            tracks = t->getAllAudioSubTracks (true);
        else
            tracks = getAudioTracks (s.edit);

        for (auto at : tracks)
        {
            auto slots = at->getClipSlotList().getClipSlots();
            const bool shouldStopClipsOnTrack = [cs = slots[sceneIndex]] { return cs == nullptr || cs->getClip() != nullptr; }();

            for (int i = 0; i < slots.size(); ++i)
            {
                if (auto slot = slots[i])
                {
                    if (auto c = slot->getClip())
                    {
                        if (i == sceneIndex)
                            launchClip (*c, false, StartTransport::yes);
                        else if (auto lh = c->getLaunchHandle(); lh && shouldStopClipsOnTrack)
                            lh->stop (at->edit.getTransport().isPlaying() ? std::make_optional (getStopPosition (*c)) : std::nullopt);
                    }
                }
            }
        }
    }

    inline void stopEdit (te::Edit& e)
    {
        for (auto at : te::getAudioTracks (e))
            for (auto slot : at->getClipSlotList().getClipSlots())
                if (auto c = slot->getClip())
                    if (auto lh = c->getLaunchHandle())
                        lh->stop (e.getTransport().isPlaying() ? std::make_optional (getStopPosition (*c)) : std::nullopt);
    }


    //==============================================================================
    // UI
    //==============================================================================
    inline void setPlayButtonColours (juce::ShapeButton& b, te::LaunchHandle& lh)
    {
        auto baseCol = juce::Colours::white;

        if (auto queuedState = lh.getQueuedStatus())
        {
            baseCol = queuedState == te::LaunchHandle::QueueState::playQueued
                        ? juce::Colours::orange
                        : juce::Colours::cornflowerblue;
        }
        else if (lh.getPlayingStatus() == te::LaunchHandle::PlayState::playing)
        {
            baseCol = juce::Colours::lightgreen;
        }

        b.setColours (baseCol, baseCol.darker (0.2f), baseCol.darker());
        b.repaint();
    }

    inline void setStopButtonColours (juce::ShapeButton& b, te::LaunchHandle* lh)
    {
        auto baseCol = juce::Colours::white;

        if (lh)
        {
            if (auto queuedState = lh->getQueuedStatus())
                if (*queuedState == te::LaunchHandle::QueueState::stopQueued)
                    baseCol = juce::Colours::cornflowerblue;
        }
        else
        {
            // No clips on track
            baseCol = juce::Colours::white.withMultipliedAlpha (0.5f);
        }

        b.setColours (baseCol, baseCol.darker (0.2f), baseCol.darker());
        b.repaint();
    }

    inline void setStopButtonColours (juce::ShapeButton& b, te::Edit& e)
    {
        auto baseCol = juce::Colours::white;

        const auto anyPlayingOrQueued = [handles = getPlayingOrQueuedLaunchHandles (e)]
                                        {
                                            for (auto lh : handles)
                                            {
                                                if (auto queuedState = lh->getQueuedStatus())
                                                    if (queuedState == te::LaunchHandle::QueueState::playQueued)
                                                        return true;

                                                if (lh->getPlayingStatus() == te::LaunchHandle::PlayState::playing)
                                                    return true;
                                            }

                                            return false;
                                        }();

        if (! anyPlayingOrQueued)
            baseCol = juce::Colours::white.withMultipliedAlpha (0.5f);

        b.setColours (baseCol, baseCol.darker (0.2f), baseCol.darker());
        b.repaint();
    }
}

//==============================================================================
//==============================================================================
namespace cl
{
    //==============================================================================
    class PlayButton : public juce::Component
    {
    public:
        PlayButton()
        {
            addAndMakeVisible (button);
            button.setShape (Icons::getPlayPath(), false, true, false);
            button.setOutline (Colours::black, 0.5f);
            button.setBorderSize (juce::BorderSize<int> (5));
        }

        PlayButton (std::shared_ptr<te::LaunchHandle> lh)
            : PlayButton()
        {
            launchHandle = std::move (lh);
            assert (launchHandle);

            timer.setCallback ([this] { utils::setPlayButtonColours (button, *launchHandle); });
            timer.startTimerHz (25);
            timer.timerCallback();
        }

        void resized() override
        {
            button.setBounds (getLocalBounds());
        }

        juce::Button& getButton()
        {
            return button;
        }

    private:
        std::shared_ptr<te::LaunchHandle> launchHandle;
        juce::ShapeButton button { {}, juce::Colours::white, juce::Colours::lightgrey, juce::Colours::grey };
        te::LambdaTimer timer;
    };

    //==============================================================================
    class StopButton : public juce::Component
    {
    public:
        StopButton()
        {
            addAndMakeVisible (button);
            button.setShape (Icons::getStopPath(), false, true, false);
            button.setOutline (Colours::black, 0.5f);
            button.setBorderSize (juce::BorderSize<int> (6));
            button.onClick = [this] { buttonClicked(); };

            timer.startTimerHz (25);
            timer.timerCallback();
        }

        StopButton (te::AudioTrack& t)
            : StopButton()
        {
            track = &t;
        }

        StopButton (te::Edit& e)
            : StopButton()
        {
            edit = &e;
        }

        void resized() override
        {
            button.setBounds (getLocalBounds());
        }

        juce::Button& getButton()
        {
            return button;
        }

    private:
        te::SafeSelectable<te::AudioTrack> track;
        te::SafeSelectable<te::Edit> edit;

        juce::ShapeButton button { {}, juce::Colours::white, juce::Colours::lightgrey, juce::Colours::grey };
        std::shared_ptr<te::LaunchHandle> launchHandle;
        te::LambdaTimer timer { [this] { updateStatus(); } };

        void updateStatus()
        {
            if (track)
            {
                auto lh = utils::getPlayingOrQueuedLaunchHandleOnTrack (*track);
                utils::setStopButtonColours (button, lh.get());
            }
            else if (edit)
            {
                utils::setStopButtonColours (button, *edit);
            }
            else
            {
                utils::setStopButtonColours (button, nullptr);
            }
        }

        void buttonClicked()
        {
            if (track)
                for (auto lh : utils::getPlayingOrQueuedLaunchHandlesOnTrack (*track))
                    lh->stop (utils::getLaunchPosition (track->edit));
        }
    };

    //==============================================================================
    class AddSceneButton : public juce::Component
    {
    public:
        AddSceneButton()
        {
            addAndMakeVisible (button);
            button.setShape (Icons::getPlusPath(), false, true, false);
            button.setOutline (Colours::black, 0.5f);
            button.setBorderSize (juce::BorderSize<int> (6));
        }

        void resized() override
        {
            button.setBounds (getLocalBounds());
        }

        juce::Button& getButton()
        {
            return button;
        }

    private:
        juce::ShapeButton button { {}, juce::Colours::white, juce::Colours::lightgrey, juce::Colours::grey };
    };

    //==============================================================================
    struct ClipComponent : public juce::Component,
                           private te::SelectableListener
    {
        ClipComponent (te::Clip& c)
            : clip (c)
        {
            addAndMakeVisible (playButton);
            playButton.getButton().onClick = [this] { utils::launchClip (clip, true, utils::StartTransport::yes); };

            refreshPlaybackHandle();
            clip.addSelectableListener (this);
            timer.startTimerHz (25);
        }

        ~ClipComponent() override
        {
            clip.removeSelectableListener (this);
        }

        void resized() override
        {
            auto r = getLocalBounds().reduced (1);
            playButton.setBounds (r.removeFromLeft (r.getHeight()));
        }

        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().reduced (1);

            g.setColour (getClipColour (clip));
            g.fillRect (r);

            r.removeFromLeft (r.getHeight()); // Button space

            g.setColour (juce::Colours::white);
            g.setFont (12.0f);
            g.drawText (clip.getName(), r, juce::Justification::centredLeft);
        }

        void paintOverChildren (juce::Graphics& g) override
        {
            drawProgressIndicator (g);
        }

    private:
        te::Clip& clip;
        const std::shared_ptr<te::LaunchHandle> launchHandle { clip.getLaunchHandle() };
        std::optional<te::LauncherClipPlaybackHandle> playbackHandle;
        PlayButton playButton { launchHandle };
        te::LambdaTimer timer { [this] { repaint(); } };

        static juce::Colour getClipColour (te::Clip& c)
        {
            if (c.getColour().isTransparent())
                if (auto cs = c.getClipSlot())
                    return cs->track.getColour();

            return c.getColour();
        }

        void refreshPlaybackHandle()
        {
            playbackHandle = clip.isLooping()
                                ? te::LauncherClipPlaybackHandle::forLooping (clip.getLoopRangeBeats(), clip.getOffsetInBeats())
                                : te::LauncherClipPlaybackHandle::forOneShot ({ te::toPosition (clip.getOffsetInBeats()), clip.getLengthInBeats() });
        }

        void drawProgressIndicator (juce::Graphics& g)
        {
            if (auto playedRange = launchHandle->getPlayedRange())
            {
                playbackHandle->start (playedRange->getStart());

                if (auto p = playbackHandle->getProgress (playedRange->getEnd()))
                {
                    auto r = getLocalBounds().toFloat().reduced (0.5f, 0.0f);
                    r = r.withWidth (1.0f).withX (r.proportionOfWidth (*p) - 0.5f);
                    g.setColour (juce::Colours::white.withAlpha (0.75f));
                    g.fillRect (r);
                }
            }
        }

        void selectableObjectChanged (te::Selectable*) override
        {
            refreshPlaybackHandle();
        }

        void selectableObjectAboutToBeDeleted (te::Selectable*) override {}
    };

    //==============================================================================
    struct ClipSlotComponent : public juce::Component,
                               public juce::FileDragAndDropTarget,
                               private te::ValueTreeObjectList<utils::AsyncValueTreeItem<ClipComponent>>
    {
        ClipSlotComponent (te::ClipSlot& c)
            : te::ValueTreeObjectList<utils::AsyncValueTreeItem<ClipComponent>> (c.state),
              clipSlot (c)
        {
            rebuildObjects();

            addAndMakeVisible (stopButton);
            stopButton.setAlpha (0.3f);
        }

        ~ClipSlotComponent() override
        {
            freeObjects();
        }

        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds();
            g.setColour (juce::Colours::darkgrey.darker (isDragging ? 0.0f : 0.4f));
            g.fillRect (r);

            g.setColour (juce::Colours::black);
            g.drawRect (r);
        }

        void resized() override
        {
            const auto r = getLocalBounds();

            if (auto clipWrapper = objects.getFirst())
                if (auto clip = clipWrapper->getObject())
                    return clip->setBounds (r);

            stopButton.setBounds (r.reduced (1).removeFromLeft (r.getHeight()));
        }

        bool isInterestedInFileDrag (const juce::StringArray& files) override
        {
            for (auto f : files)
                if (juce::File (f).hasFileExtension (te::soundFileAndMidiExtensions))
                    return true;

            return false;
        }

        void fileDragEnter (const juce::StringArray&, int, int) override
        {
            isDragging = true;
            repaint();
        }

        void fileDragExit (const juce::StringArray&) override
        {
            isDragging = false;
            repaint();
        }

        void filesDropped (const juce::StringArray& files, int, int) override
        {
            for (auto f : files)
                if (juce::File (f).hasFileExtension (te::soundFileAndMidiExtensions))
                    setFile (f);

            isDragging = false;
            repaint();
        }

    private:
        te::ClipSlot& clipSlot;
        utils::AsyncResizer asyncResizer { *this };
        bool isDragging = false;
        StopButton stopButton;

        void removeCurrentClip()
        {
            if (auto c = clipSlot.getClip())
                c->removeFromParent();
        }

        void setFile (juce::File f)
        {
            if (f.hasFileExtension (te::soundFileExtensions))
            {
                removeCurrentClip();

                te::AudioFile af (clipSlot.edit.engine, f);
                auto wc = insertWaveClip (clipSlot, f.getFileName(), f,
                                          {{ 0_tp, te::TimeDuration::fromSeconds (af.getLength()) }},
                                          te::DeleteExistingClips::yes);
                wc->setUsesProxy (false);
                wc->setAutoTempo (true);
                wc->setLoopRangeBeats ({ 0_bp, wc->getLengthInBeats() });
            }
            else if (f.hasFileExtension (te::midiFileExtensions))
            {
                removeCurrentClip();

                if (auto mc = te::createClipFromFile (f, clipSlot, te::MidiList::looksLikeMPEData (f)))
                {
                    mc->setUsesProxy (false);
                    mc->setLoopRangeBeats (mc->getEditBeatRange());
                }
            }
        }

        using ObjType = utils::AsyncValueTreeItem<ClipComponent>;

        bool isSuitableType (const juce::ValueTree& v) const override
        {
            return te::Clip::isClipState (v);
        }

        ObjType* createNewObject (const juce::ValueTree& v) override
        {
            return new ObjType (v,
                                [this] ([[ maybe_unused ]] auto state)
                                {
                                    auto c = clipSlot.getClip();
                                    assert (c);
                                    assert (c->state == state);
                                    auto cc = std::make_unique<ClipComponent> (*c);
                                    addAndMakeVisible (*cc);
                                    asyncResizer.resizeAsync();

                                    return cc;
                                });
        }

        void deleteObject (ObjType* obj) override
        {
            delete obj;
        }

        void newObjectAdded (ObjType*) override { asyncResizer.resizeAsync(); }
        void objectRemoved (ObjType*) override  { asyncResizer.resizeAsync(); }
        void objectOrderChanged() override      { asyncResizer.resizeAsync(); }
    };

    //==============================================================================
    struct SlotListComponent : public juce::Component,
                               private te::ValueTreeObjectList<utils::AsyncValueTreeItem<ClipSlotComponent>>
    {
        SlotListComponent (te::ClipSlotList& l)
            : te::ValueTreeObjectList<utils::AsyncValueTreeItem<ClipSlotComponent>> (l.state),
              clipSlotList (l)
        {
            rebuildObjects();
        }

        ~SlotListComponent() override
        {
            freeObjects();
        }

        void paint (juce::Graphics& g) override
        {
            g.setColour (juce::Colours::black);
            g.drawRect (getLocalBounds());
        }

        void resized() override
        {
            juce::Grid g;
            g.templateColumns.add (1_fr);
            g.templateRows.insertMultiple (0, 24_px, objects.size());
            int x = 1, y = 1;

            for (auto objectWrapper : objects)
                if (auto object = objectWrapper->getObject())
                    g.items.add (juce::GridItem (*object).withArea (y++, x));

            g.performLayout (getLocalBounds());
        }

    private:
        te::ClipSlotList& clipSlotList;
        utils::AsyncResizer asyncResizer { *this };

        using ObjType = utils::AsyncValueTreeItem<ClipSlotComponent>;

        bool isSuitableType (const juce::ValueTree& v) const override
        {
            return v.hasType (te::IDs::CLIPSLOT);
        }

        ObjType* createNewObject (const juce::ValueTree& v) override
        {
            return new ObjType (v,
                                [this] (auto state)
                                {
                                    auto cs = utils::findObjectForState (clipSlotList.getClipSlots(), state);
                                    assert (cs);
                                    auto csc = std::make_unique<ClipSlotComponent> (*cs);
                                    addAndMakeVisible (*csc);
                                    asyncResizer.resizeAsync();

                                    return csc;
                                });
        }

        void deleteObject (ObjType* obj) override
        {
            delete obj;
        }

        void newObjectAdded (ObjType*) override { asyncResizer.resizeAsync(); }
        void objectRemoved (ObjType*) override  { asyncResizer.resizeAsync(); }
        void objectOrderChanged() override      { asyncResizer.resizeAsync(); }
    };

    //==============================================================================
    struct TrackComponent : public juce::Component
    {
        TrackComponent (te::AudioTrack& t)
            : track (t)
        {
            addAndMakeVisible (slotList);
            addAndMakeVisible (stopButton);
        }

        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds();
            paintTrackHeader (g, r.removeFromTop (40));

            g.setColour (juce::Colours::white.withAlpha (0.2f));
            g.drawRect (r);
        }

        void resized() override
        {
            auto r = getLocalBounds();
            auto topR = r.removeFromTop (40);
            stopButton.setBounds (topR.reduced (2).removeFromLeft (22));

            slotList.setBounds (r);
        }

    private:
        te::AudioTrack& track;
        SlotListComponent slotList { track.getClipSlotList() };
        StopButton stopButton { track };

        void paintTrackHeader (juce::Graphics& g, juce::Rectangle<int> r)
        {
            g.setColour (juce::Colours::darkgrey);
            g.fillRect (r);

            g.setColour (juce::Colours::black);
            g.drawRect (r);

            g.setColour (track.getColour().isTransparent() ? juce::Colours::white.darker()
                                                           : track.getColour());
            g.fillRect (r.reduced (1).removeFromTop (2));

            g.setColour (juce::Colours::white);
            g.setFont (13.0f);
            g.drawText (track.getName(), r.reduced (4).withTrimmedLeft (22), juce::Justification::centredLeft);
        }
    };

    //==============================================================================
    struct TrackListComponent : public juce::Component,
                                private te::ValueTreeObjectList<utils::AsyncValueTreeItem<TrackComponent>>
    {
        TrackListComponent (te::Edit& e)
            : te::ValueTreeObjectList<utils::AsyncValueTreeItem<TrackComponent>> (e.state),
              edit (e)
        {
            rebuildObjects();
        }

        ~TrackListComponent() override
        {
            freeObjects();
        }

        void paint (juce::Graphics&) override
        {
        }

        void resized() override
        {
            juce::Grid g;
            g.templateRows.add (1_fr);
            g.templateColumns.insertMultiple (0, 1_fr, objects.size());
            int x = 1, y = 1;

            for (auto objectWrapper : objects)
                if (auto object = objectWrapper->getObject())
                    g.items.add (juce::GridItem (*object).withArea (x, y++));

            g.performLayout (getLocalBounds());
        }

        te::Edit& edit;

    private:
        utils::AsyncResizer asyncResizer { *this };

        using ObjType = utils::AsyncValueTreeItem<TrackComponent>;

        bool isSuitableType (const juce::ValueTree& v) const override
        {
            return v.hasType (te::IDs::TRACK);
        }

        ObjType* createNewObject (const juce::ValueTree& v) override
        {
            return new ObjType (v,
                                [this] (auto state)
                                {
                                    auto t = utils::findObjectForState (te::getAudioTracks (edit), state);
                                    assert (t);
                                    auto tc = std::make_unique<TrackComponent> (*t);
                                    addAndMakeVisible (*tc);
                                    asyncResizer.resizeAsync();

                                    return tc;
                                });
        }

        void deleteObject (ObjType* obj) override
        {
            delete obj;
        }

        void newObjectAdded (ObjType*) override { asyncResizer.resizeAsync(); }
        void objectRemoved (ObjType*) override  { asyncResizer.resizeAsync(); }
        void objectOrderChanged() override      { asyncResizer.resizeAsync(); }
    };

    //==============================================================================
    struct SceneButton : public juce::Component
    {
        SceneButton (te::Scene& s)
            : scene (s)
        {
            addAndMakeVisible (playButton);
            playButton.getButton().onClick = [this] { utils::launchScene (scene); };
        }

        void resized() override
        {
            auto r = getLocalBounds().reduced (1);
            playButton.setBounds (r.removeFromLeft (r.getHeight() + 2));
        }

        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds();
            g.setColour (juce::Colours::darkgrey);
            g.fillRect (r);

            g.setColour (juce::Colours::black);
            g.drawRect (r.toFloat());

            auto leftR = r.removeFromLeft (r.getHeight()).reduced (1);
            g.setColour (scene.colour.get().isTransparent() ? juce::Colours::white.darker()
                                                            : scene.colour.get());
            g.fillRect (leftR.removeFromLeft (2));

            g.setColour (juce::Colours::white);
            g.setFont (13.0f);
            g.drawText (getSceneName (scene), r, juce::Justification::centredLeft);
        }

        void mouseUp (const juce::MouseEvent& e) override
        {
            if (! e.mods.isPopupMenu())
                return;

            juce::PopupMenu m;
            m.addItem (TRANS("Delete Scene"),
                       [sl = te::SafeSelectable (scene)]() mutable
                       {
                           if (sl)
                               sl->sceneList.deleteScene (*sl);
                       });
            m.showMenuAsync ({});
        }

        te::Scene& scene;

    private:
        PlayButton playButton;

        static juce::String getSceneName (te::Scene& s)
        {
            if (auto n = s.name.get(); n.isNotEmpty())
                return n;

            return juce::String ("Scene 123")
                    .replace ("123", juce::String (s.sceneList.getScenes().indexOf (&s) + 1));
        }
    };

    //==============================================================================
    struct SceneListComponent : public juce::Component,
                                private te::ValueTreeObjectList<utils::AsyncValueTreeItem<SceneButton>>
    {
        SceneListComponent (te::SceneList& s)
            : te::ValueTreeObjectList<utils::AsyncValueTreeItem<SceneButton>> (s.state),
              sceneList (s)
        {
            rebuildObjects();

            addAndMakeVisible (stopButton);
            stopButton.getButton().onClick = [this] { utils::stopEdit (sceneList.edit); };

            addAndMakeVisible (addButton);
            addButton.getButton().onClick
                = [this] { sceneList.ensureNumberOfScenes (sceneList.getScenes().size() + 1); };
        }

        ~SceneListComponent() override
        {
            freeObjects();
        }

        int getIdealHeight() const
        {
            return 40 + (objects.size() + 1) * 24;
        }

        void paint (juce::Graphics& g) override
        {
            g.setColour (juce::Colours::darkgrey);
            g.fillRect (getLocalBounds().removeFromTop (40));

            g.setColour (juce::Colours::black);
            g.drawRect (getLocalBounds());
        }

        void resized() override
        {
            auto r = getLocalBounds();

            {
                juce::Grid g;
                g.templateColumns.add (1_fr);
                g.templateRows.add (40_px);
                g.templateRows.insertMultiple (1, 24_px, objects.size());
                int x = 1, y = 1;

                g.items.add (juce::GridItem (stopButton).withArea (y++, x).withMargin (9));

                for (auto sceneButtonWrapper: objects)
                    if (auto sceneButton = sceneButtonWrapper->getObject ())
                        g.items.add (juce::GridItem (*sceneButton).withArea (y++, x));

                g.performLayout (r);
            }

            {
                r = r.withHeight (24);

                if (auto last = objects.getLast ())
                    if (auto sceneButton = last->getObject ())
                        r = r.withY (sceneButton->getBottom ());

                addButton.setBounds (r);
            }
        }

        te::SceneList& sceneList;

    private:
        StopButton stopButton { sceneList.edit };
        AddSceneButton addButton;
        utils::AsyncResizer asyncResizer { *this };

        using ObjType = utils::AsyncValueTreeItem<SceneButton>;

        bool isSuitableType (const juce::ValueTree& v) const override
        {
            return v.hasType (te::IDs::SCENE);
        }

        ObjType* createNewObject (const juce::ValueTree& v) override
        {
            return new ObjType (v,
                                [this] (auto state)
                                {
                                    auto o = utils::findObjectForState (sceneList.getScenes(), state);
                                    assert (o);
                                    assert (o->state == state);
                                    auto sb = std::make_unique<SceneButton> (*o);
                                    addAndMakeVisible (*sb);
                                    asyncResizer.resizeAsync();

                                    return sb;
                                });
        }

        void deleteObject (ObjType* obj) override
        {
            delete obj;
        }

        void newObjectAdded (ObjType*) override { asyncResizer.resizeAsync(); }
        void objectRemoved (ObjType*) override  { asyncResizer.resizeAsync(); }
        void objectOrderChanged() override      { asyncResizer.resizeAsync(); }
    };

    //==============================================================================
    struct ClipLauncherComponent : public juce::Component
    {
        ClipLauncherComponent(te::Edit &e)
            : edit (e)
        {
            addAndMakeVisible (sceneListComponent);
            addAndMakeVisible (trackListComponent);
        }

        void paint (juce::Graphics&) override
        {
        }

        void resized() override
        {
            constexpr int sceneW = 70;

            auto r = getLocalBounds();
            sceneListComponent.setBounds (r.removeFromLeft (sceneW));
            trackListComponent.setBounds (r);
        }

        te::Edit& edit;
        SceneListComponent sceneListComponent { edit.getSceneList() };
        TrackListComponent trackListComponent { edit };
    };
}


//==============================================================================
//==============================================================================
class ClipLauncherDemo  : public juce::Component,
                          private juce::ChangeListener,
                          private juce::Value::Listener
{
public:
    //==============================================================================
    ClipLauncherDemo (te::Engine& e)
        : engine (e)
    {
        Helpers::addAndMakeVisible (*this, { &playPauseButton, &quantisationButton, &metronomeButton,
                                             &transportReadout, &clipLauncherComponent });

        playPauseButton.setClickingTogglesState (false);
        playPauseButton.onClick = [this]
        {
            EngineHelpers::togglePlay (edit, EngineHelpers::ReturnToStart::yes);
        };

        metronomeButton.getToggleStateValue().referTo (edit.clickTrackEnabled.getPropertyAsValue());
        metronomeButton.triggerClick();

        quantisationButton.onClick = [this] { showQuantisationMenu(); };

       #if JUCE_MAJOR_VERSION == 8
        transportReadout.setFont (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 14, juce::Font::plain));
       #else
        transportReadout.setFont ({ juce::Font::getDefaultMonospacedFontName(), 14, juce::Font::plain });
       #endif

        transportReadoutTimer.setCallback ([this]
                                           {
                                               auto t = te::TimecodeDisplayFormat (te::TimecodeType::barsBeats)
                                                                .getString (edit.tempoSequence, transport.getPosition(), false);
                                               transportReadout.setText (t, juce::dontSendNotification);
                                           });
        transportReadoutTimer.startTimerHz (25);

        transport.addChangeListener (this);
        quantisationValue.addListener (this);

        edit.ensureNumberOfAudioTracks (8);
        edit.getSceneList().ensureNumberOfScenes (8);

        for (auto at : te::getAudioTracks (edit))
            at->getClipSlotList().ensureNumberOfSlots (8);

        auto& ts = edit.tempoSequence;
        auto um = &edit.getUndoManager();

        // Create 4OSC on track 1 & 2
        {
            auto& at = utils::addFourOscWithPatch (*te::getAudioTracks (edit)[0], utils::organPatch);
            at.setName ("Organ");
            at.setColour (juce::Colours::blue);

            {
                auto mc = te::insertMIDIClip (*at.getClipSlotList().getClipSlots()[0], ts.toTime ({ 0_bp, 16_bp }));
                auto& seq = mc->getSequence();
                seq.addNote (36, 0_bp, 4_bd, 127, 0, um);
                seq.addNote (41, 4_bp, 4_bd, 100, 0, um);
                seq.addNote (39, 8_bp, 4_bd, 110, 0, um);
                seq.addNote (38, 12_bp, 4_bd, 100, 0, um);
            }

            {
                auto mc = te::insertMIDIClip (*at.getClipSlotList().getClipSlots()[1], ts.toTime ({ 0_bp, 16_bp }));
                auto& seq = mc->getSequence();
                seq.addNote (45, 0_bp, 4_bd, 127, 0, um);
                seq.addNote (43, 4_bp, 4_bd, 100, 0, um);
                seq.addNote (36, 8_bp, 4_bd, 110, 0, um);
                seq.addNote (43, 12_bp, 4_bd, 100, 0, um);
            }
        }

        {
            auto& at = utils::addFourOscWithPatch (*te::getAudioTracks (edit)[1], utils::synthPatch);
            at.setName ("Lead");
            at.setColour (juce::Colours::green);

            {
                juce::TemporaryFile tf;
                tf.getFile().replaceWithData (BinaryData::midi_arp_mid, BinaryData::midi_arp_midSize);
                auto mc = createClipFromFile (tf.getFile(), *at.getClipSlotList().getClipSlots()[0], false);
                mc->setName ("Lead");
                mc->setVolumeDb (-6.0f);
            }

            {
                juce::TemporaryFile tf;
                tf.getFile().replaceWithData (BinaryData::midi_arp_2_mid, BinaryData::midi_arp_2_midSize);
                auto mc = createClipFromFile (tf.getFile(), *at.getClipSlotList().getClipSlots()[1], false);
                mc->setName ("Lead");
                mc->setVolumeDb (-6.0f);
            }
        }

        // Drums on track 3
        {
            drumTempFile = std::make_unique<TemporaryFile> (".wav");
            drumTempFile->getFile().replaceWithData (BinaryData::drum_loop_wav, BinaryData::drum_loop_wavSize);

            auto at = te::getAudioTracks (edit)[2];
            auto clip = insertWaveClip (*at->getClipSlotList().getClipSlots()[0], "Drums", drumTempFile->getFile(),
                                        createClipPosition (ts, { 0_bp, 8_bp }), te::DeleteExistingClips::no);

            clip->setAutoTempo (true);
            clip->setUsesProxy (false);
        }

        updateQuantisationButtonText();

        lookAndFeelChanged();
    }

    void paint (juce::Graphics&) override
    {
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (2);

        r.removeFromBottom (4);

        {
            juce::Grid g;
            g.templateRows.add (1_fr);
            g.templateColumns.insertMultiple (0, 1_fr, 4);
            g.items.add (juce::GridItem (playPauseButton).withArea (1, 1));
            g.items.add (juce::GridItem (metronomeButton).withArea (1, 2));
            g.items.add (juce::GridItem (transportReadout).withArea (1, 3));
            g.items.add (juce::GridItem (quantisationButton).withArea (1, 4)
                            .withMargin (juce::GridItem::Margin (2)));
            g.performLayout (r.removeFromTop (28));
        }

        clipLauncherComponent.setBounds (r.reduced (2).translated (0, 2));
    }

    void lookAndFeelChanged() override
    {
    }

private:
    //==============================================================================
    te::Engine& engine;
    te::Edit edit { engine, te::Edit::EditRole::forEditing };
    te::TransportControl& transport { edit.getTransport() };
    te::LaunchQuantisation& launchQuantisation { edit.getLaunchQuantisation() };

    juce::TextButton playPauseButton { "Play" }, quantisationButton;
    juce::ToggleButton metronomeButton { "Metronome" };
    juce::Label transportReadout { "" };

    cl::ClipLauncherComponent clipLauncherComponent { edit };

    te::LambdaTimer transportReadoutTimer;
    juce::Value quantisationValue { launchQuantisation.type.getPropertyAsValue() };

    std::unique_ptr<TemporaryFile> drumTempFile;

    void showQuantisationMenu()
    {
        juce::PopupMenu m;

        magic_enum::enum_for_each<te::LaunchQType> ([this, &m] (auto t)
        {
            m.addItem (te::getName (t),
                       [this, t] { launchQuantisation.type = t; });
        });

        m.showMenuAsync ({});
    }

    void updateQuantisationButtonText()
    {
        quantisationButton.setButtonText ("Quantisation: " + te::getName (launchQuantisation.type));
    }

    void changeListenerCallback (juce::ChangeBroadcaster*) override
    {
        playPauseButton.setToggleState (transport.isPlaying(), juce::dontSendNotification);
        playPauseButton.setButtonText (transport.isPlaying() ? "Pause" : "Play");
    }

    void valueChanged (juce::Value& v) override
    {
        if (v.refersToSameSourceAs (quantisationValue))
            updateQuantisationButtonText();
    }
};

//==============================================================================
static DemoTypeBase<ClipLauncherDemo> clipLauncherDemo ("Clip Launcher");
