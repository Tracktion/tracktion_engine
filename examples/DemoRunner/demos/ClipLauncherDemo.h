/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/


#pragma once

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
}

//==============================================================================
//==============================================================================
namespace cl
{
    //==============================================================================
    struct ClipComponent : public juce::Component
    {
        ClipComponent (te::Clip& c)
            : clip (c)
        {

        }

        void paint (juce::Graphics& g) override
        {
            g.fillAll (juce::Colours::white);
        }

        te::Clip& clip;
    };

    //==============================================================================
    struct ClipSlotComponent : public juce::Component,
                               private te::ValueTreeObjectList<utils::AsyncValueTreeItem<ClipComponent>>
    {
        ClipSlotComponent (te::ClipSlot& c)
            : te::ValueTreeObjectList<utils::AsyncValueTreeItem<ClipComponent>> (c.state),
              clipSlot (c)
        {
            rebuildObjects();
        }

        ~ClipSlotComponent() override
        {
            freeObjects();
        }

        void paint (juce::Graphics& g) override
        {
            g.fillAll (juce::Colours::blue);
        }

        void resized() override
        {
            if (auto clipWrapper = objects.getFirst())
                if (auto clip = clipWrapper->getObject())
                    clip->setBounds (getLocalBounds());
        }

    private:
        te::ClipSlot& clipSlot;
        utils::AsyncResizer asyncResizer { *this };

        using ObjType = utils::AsyncValueTreeItem<ClipComponent>;

        bool isSuitableType (const juce::ValueTree& v) const override
        {
            return te::Clip::isClipState (v);
        }

        ObjType* createNewObject (const juce::ValueTree& v) override
        {
            return new ObjType (v,
                                [this] (auto state)
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

    private:
        te::ClipSlotList& clipSlotList;
        utils::AsyncResizer asyncResizer { *this };

        using ObjType = utils::AsyncValueTreeItem<ClipSlotComponent>;

        bool isSuitableType (const juce::ValueTree& v) const override
        {
            return v.hasProperty (te::IDs::CLIPSLOT);
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
        TrackComponent (te::AudioTrack &t)
            : track (t)
        {

        }

        void paint (juce::Graphics& g) override
        {
            g.fillAll (juce::Colours::red);
        }

        te::AudioTrack& track;
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

        void paint (juce::Graphics& g) override
        {
            g.setColour (juce::Colours::grey);
            g.drawRect (getLocalBounds());
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

        }

        void paint (juce::Graphics& g) override
        {
            g.fillAll (juce::Colours::green);
        }

        te::Scene& scene;
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
        }

        ~SceneListComponent() override
        {
            freeObjects();
        }

        void paint (juce::Graphics& g) override
        {
            g.setColour (juce::Colours::grey);
            g.drawRect (getLocalBounds());
        }

        void resized() override
        {
            juce::Grid g;
            g.templateColumns.add (1_fr);
            g.templateRows.insertMultiple (0, 1_fr, objects.size());
            int x = 1, y = 1;

            for (auto sceneButtonWrapper : objects)
                if (auto sceneButton = sceneButtonWrapper->getObject())
                    g.items.add (juce::GridItem (*sceneButton).withArea (x, y++));

            g.performLayout (getLocalBounds());
        }

        te::SceneList& sceneList;

    private:
        utils::AsyncResizer asyncResizer { *this };

        using ObjType = utils::AsyncValueTreeItem<SceneButton>;

        bool isSuitableType (const juce::ValueTree& v) const override
        {
            return v.hasType (te::IDs::SCENES);
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

        void paint (juce::Graphics& g) override
        {
            g.fillAll (juce::Colours::yellow);
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

//==============================================================================
class ClipLauncherDemo  : public juce::Component,
                          private juce::ChangeListener
{
public:
    //==============================================================================
    ClipLauncherDemo (te::Engine& e)
        : engine (e)
    {
        Helpers::addAndMakeVisible (*this, { &playPauseButton, &metronomeButton,
                                             &transportReadout, &clipLauncherComponent });

        playPauseButton.setClickingTogglesState (false);
        playPauseButton.onClick = [this]
        {
            EngineHelpers::togglePlay (edit);
        };

        metronomeButton.getToggleStateValue().referTo (edit.clickTrackEnabled.getPropertyAsValue());
        metronomeButton.triggerClick();

        transportReadout.setFont ({ juce::Font::getDefaultMonospacedFontName(), 14, juce::Font::plain });
        transportReadoutTimer.setCallback ([this]
                                           {
                                               auto t = te::TimecodeDisplayFormat (te::TimecodeType::barsBeats)
                                                                .getString (edit.tempoSequence, transport.getPosition(), false);
                                               transportReadout.setText (t, juce::dontSendNotification);
                                           });
        transportReadoutTimer.startTimerHz (25);

        transport.addChangeListener (this);

        lookAndFeelChanged();

        edit.ensureNumberOfAudioTracks (8);
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
            g.templateColumns.insertMultiple (0, 1_fr, 3);
            g.items.add (juce::GridItem (playPauseButton).withArea (1, 1));
            g.items.add (juce::GridItem (metronomeButton).withArea (1, 2));
            g.items.add (juce::GridItem (transportReadout).withArea (1, 3));
            g.performLayout (r.removeFromTop (28));
        }

        clipLauncherComponent.setBounds (r);
    }

    void lookAndFeelChanged() override
    {
    }

private:
    //==============================================================================
    te::Engine& engine;
    te::Edit edit { engine, te::createEmptyEdit (engine), te::Edit::forEditing, nullptr, 0 };
    te::TransportControl& transport { edit.getTransport() };

    juce::TextButton playPauseButton { "Play" };
    juce::ToggleButton metronomeButton { "Metronome" };
    juce::Label transportReadout { "" };

    cl::ClipLauncherComponent clipLauncherComponent { edit };

    te::LambdaTimer transportReadoutTimer;

    void changeListenerCallback (juce::ChangeBroadcaster*) override
    {
        playPauseButton.setToggleState (transport.isPlaying(), juce::dontSendNotification);
        playPauseButton.setButtonText (transport.isPlaying() ? "Pause" : "Play");
    }
};

//==============================================================================
static DemoTypeBase<ClipLauncherDemo> clipLauncherDemo ("Clip Launcher");
