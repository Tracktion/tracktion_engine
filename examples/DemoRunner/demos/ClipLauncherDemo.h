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
#include "../../modules/3rd_party/magic_enum/magic_enum_utility.hpp"

//==============================================================================
//==============================================================================
namespace utils
{
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
    inline te::BeatPosition getLaunchPosition (te::Edit& e, const te::LaunchHandle& lh)
    {
        if (auto currentPos = lh.getPosition())
            return e.getLaunchQuantisation().getNext (*currentPos);

        return e.getLaunchQuantisation().getNext (e.tempoSequence.toBeats (e.getTransport().getPosition()));
    }

    inline te::BeatPosition getStopPosition (te::Edit& e, const te::LaunchHandle& lh)
    {
        if (auto currentPos = lh.getPosition())
            return e.getLaunchQuantisation().getNext (*currentPos);

        return {};
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

    inline std::shared_ptr<te::LaunchHandle> stopPlayingOrQueuedClipsOnTrack (te::AudioTrack& t)
    {
        if (auto lh = getPlayingLaunchHandleOnTrack (t))
        {
            lh->stop (getStopPosition (t.edit, *lh));
            return lh;
        }

        return {};
    }

    inline void launchClip (te::Clip& c)
    {
        auto lh = c.getLaunchHandle();
        lh->play (utils::getLaunchPosition (c.edit, *lh));

        if (! c.edit.getTransport().isPlaying())
            c.edit.getTransport().play (false);
    }

    inline void launchScene (te::Scene& s)
    {
        const int sceneIndex = s.sceneList.getScenes().indexOf (&s);

        for (auto at : te::getAudioTracks (s.edit))
            if (auto slot = at->getClipSlotList().getClipSlots() [sceneIndex])
                if (auto c = slot->getClip())
                    launchClip (*c);
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
            else
            {
                utils::setStopButtonColours (button, nullptr);
            }
        }

        void buttonClicked()
        {
            if (track)
                for (auto lh : utils::getPlayingOrQueuedLaunchHandlesOnTrack (*track))
                    lh->stop (utils::getStopPosition (track->edit, *lh));
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
            playButton.getButton().onClick = [this] { utils::launchClip (clip); };

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
            if (auto startPos = launchHandle->getPlayStart())
            {
                playbackHandle->start (*startPos);

                if (auto pos = launchHandle->getPosition())
                {
                    if (auto p = playbackHandle->getProgress (*pos))
                    {
                        auto r = getLocalBounds().toFloat().reduced (0.5f, 0.0f);
                        r = r.withWidth (1.0f).withX (r.proportionOfWidth (*p) - 0.5f);
                        g.setColour (juce::Colours::white.withAlpha (0.75f));
                        g.fillRect (r);
                    }
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

        void setFile (juce::File f)
        {
            if (f.hasFileExtension (te::soundFileExtensions))
            {
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
                te::createClipFromFile (f, clipSlot, te::MidiList::looksLikeMPEData (f));
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
            return (objects.size() + 1) * 24;
        }

        void paint (juce::Graphics& g) override
        {
            g.setColour (juce::Colours::black);
            g.drawRect (getLocalBounds());
        }

        void resized() override
        {
            auto r = getLocalBounds();

            {
                juce::Grid g;
                g.templateColumns.add (1_fr);
                g.templateRows.insertMultiple (0, 24_px, objects.size());
                int x = 1, y = 1;

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
            constexpr int trackH = 40;
            constexpr int sceneW = 70;

            auto r = getLocalBounds();
            sceneListComponent.setBounds (r.removeFromLeft (sceneW).withTrimmedTop (trackH));
            trackListComponent.setBounds (r);
        }

        te::Edit& edit;
        SceneListComponent sceneListComponent { edit.getSceneList() };
        TrackListComponent trackListComponent { edit };
    };
}

#include "../../3rd_party/choc/text/choc_OpenSourceLicenseList.h"

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
            EngineHelpers::togglePlay (edit);
        };

        metronomeButton.getToggleStateValue().referTo (edit.clickTrackEnabled.getPropertyAsValue());
        metronomeButton.triggerClick();

        quantisationButton.onClick = [this] { showQuantisationMenu(); };

        transportReadout.setFont ({ juce::Font::getDefaultMonospacedFontName(), 14, juce::Font::plain });
        transportReadoutTimer.setCallback ([this]
                                           {
                                               auto t = te::TimecodeDisplayFormat (te::TimecodeType::barsBeats)
                                                                .getString (edit.tempoSequence, transport.getPosition(), false);
                                               transportReadout.setText (t, juce::dontSendNotification);
                                           });
        transportReadoutTimer.startTimerHz (25);

//ddd        edit.tempoSequence.insertTempo (1_bp, 120.0, 0.0f);
//        edit.tempoSequence.insertTempo (5_bp, 40.0, 0.0f);
        transport.setLoopRange (edit.tempoSequence.toTime ({ 0_bp, 6.75_bp }));
        transport.looping = true;
        transport.addChangeListener (this);
        quantisationValue.addListener (this);

        edit.ensureNumberOfAudioTracks (8);
        edit.getSceneList().ensureNumberOfScenes (8);

        for (auto at : te::getAudioTracks (edit))
            at->getClipSlotList().ensureNumberOfSlots (8);

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
    te::Edit edit { engine, te::createEmptyEdit (engine), te::Edit::forEditing, nullptr, 0 };
    te::TransportControl& transport { edit.getTransport() };
    te::LaunchQuantisation& launchQuantisation { edit.getLaunchQuantisation() };

    juce::TextButton playPauseButton { "Play" }, quantisationButton;
    juce::ToggleButton metronomeButton { "Metronome" };
    juce::Label transportReadout { "" };

    cl::ClipLauncherComponent clipLauncherComponent { edit };

    te::LambdaTimer transportReadoutTimer;
    juce::Value quantisationValue { launchQuantisation.type.getPropertyAsValue() };

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
