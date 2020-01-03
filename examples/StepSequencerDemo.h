/*******************************************************************************
 The block below describes the properties of this PIP. A PIP is a short snippet
 of code that can be read by the Projucer and used to generate a JUCE project.

 BEGIN_JUCE_PIP_METADATA

  name:             StepSequencerDemo
  version:          0.0.1
  vendor:           Tracktion
  website:          www.tracktion.com
  description:      This example shows how to build a step squencer and associated UI.

  dependencies:     juce_audio_basics, juce_audio_devices, juce_audio_formats, juce_audio_processors, juce_audio_utils,
                    juce_core, juce_data_structures, juce_dsp, juce_events, juce_graphics,
                    juce_gui_basics, juce_gui_extra, juce_osc, tracktion_engine
  exporters:        linux_make, vs2017, xcode_iphone, xcode_mac

  moduleFlags:      JUCE_STRICT_REFCOUNTEDPOINTER=1, JUCE_PLUGINHOST_AU=1

  type:             Component
  mainClass:        StepSequencerDemo

 END_JUCE_PIP_METADATA

*******************************************************************************/

#pragma once

#include "common/Utilities.h"
#include "common/BinaryData.h"
#include "common/BinaryData.cpp"

//==============================================================================
namespace
{
    static void loadFileIntoSamplerChannel (te::StepClip& clip, int channelIndex, const File& f)
    {
        // Find SamplerPlugin for the Clip's Track
        if (auto sampler = clip.getTrack()->pluginList.findFirstPluginOfType<te::SamplerPlugin>())
        {
            // Update the Sound layer source
            sampler->setSoundMedia (channelIndex, f.getFullPathName());

            // Then update the channel name
            clip.getChannels()[channelIndex]->name = f.getFileNameWithoutExtension();
        }
        else
        {
            jassertfalse; // No SamplerPlugin added yet?
        }
    }
}

//==============================================================================
struct StepEditor   : public Component,
                      private te::SelectableListener
{
    //==============================================================================
    StepEditor (te::StepClip& sc)
        : clip (sc), transport (sc.edit.getTransport())
    {
        for (auto c : clip.getChannels())
            addAndMakeVisible (channelConfigs.add (new ChannelConfig (*this, c->getIndex())));

        addAndMakeVisible (patternEditor);

        timer.setCallback ([this] { patternEditor.updatePaths(); });
        timer.startTimerHz (15);

        clip.addSelectableListener (this);
    }

    ~StepEditor()
    {
        clip.removeSelectableListener (this);
    }

    te::StepClip::Pattern getPattern() const
    {
        return clip.getPattern (0);
    }

    //==============================================================================
    struct ChannelConfig  : public Component
    {
        ChannelConfig (StepEditor& se, int channel)
            : editor (se), channelIndex (channel)
        {
            jassert (isPositiveAndBelow (channelIndex, editor.clip.getChannels().size()));
            nameLabel.getTextValue().referTo (editor.clip.getChannels()[channelIndex]->name.getPropertyAsValue());
            loadButton.onClick = [this] { browseForAndLoadSample(); };
            randomiseButton.onClick = [this] { randomiseChannel(); };

            volumeSlider.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
            volumeSlider.setTextBoxStyle (Slider::NoTextBox, 0, 0, false);
            volumeSlider.setRange (0, 127, 1);
            volumeSlider.getValueObject().referTo (editor.clip.getChannels()[channelIndex]->noteValue.getPropertyAsValue());
            volumeSlider.setDoubleClickReturnValue (true, te::StepClip::defaultNoteValue);

            loadButton.setShape (Icons::getFolderPath(), false, true, false);
            loadButton.setBorderSize (BorderSize<int> (2));
            randomiseButton.setShape (Icons::getDicePath(), false, true, false);

            Helpers::addAndMakeVisible (*this, { &nameLabel, &loadButton, &randomiseButton, &volumeSlider });
        }

        void browseForAndLoadSample()
        {
            EngineHelpers::browseForAudioFile (editor.clip.edit.engine,
                                               [this] (const File& f) { loadFileIntoSamplerChannel (editor.clip, channelIndex, f); });
        }

        void randomiseChannel()
        {
            editor.getPattern().randomiseChannel (channelIndex);
        }

        void resized() override
        {
            auto r = getLocalBounds();
            const int buttonH = roundToInt (r.getHeight() * 0.7);
            loadButton.setBounds (r.removeFromLeft (buttonH).reduced (2));
            randomiseButton.setBounds (r.removeFromLeft (buttonH).reduced (2));
            volumeSlider.setBounds (r.removeFromLeft (r.getHeight()));
            nameLabel.setBounds (r.reduced (2));
        }

        StepEditor& editor;
        const int channelIndex;
        ShapeButton loadButton { "L", Colours::white, Colours::white, Colours::white };
        ShapeButton randomiseButton { "R", Colours::white, Colours::white, Colours::white };
        Label nameLabel;
        Slider volumeSlider;
    };

    //==============================================================================
    struct PatternEditor    : public Component
    {
        PatternEditor (StepEditor& se)
            : editor (se)
        {
        }

        void updatePaths()
        {
            playheadRect = {};
            grid.clear();
            activeCells.clear();
            playingCells.clear();

            const auto pattern (editor.clip.getPattern (0));
            const int numChans = editor.clip.getChannels().size();
            const float indent = 2.0f;

            const bool isPlaying = editor.transport.isPlaying();
            const float playheadX = getPlayheadX();

            for (int i = 0; i < numChans; ++i)
            {
                const BigInteger cache (pattern.getChannel (i));
                const Range<float> y (editor.getChannelYRange (i));
                int index = 0;
                float lastX = 0.0f;

                grid.addWithoutMerging ({ 0.0f, y.getStart() - 0.25f, (float) getWidth(), 0.5f });

                for (float x : noteXes)
                {
                    if (cache[index])
                    {
                        Path& path = (isPlaying && playheadX >= lastX && playheadX < x)
                                         ? playingCells : activeCells;

                        const Rectangle<float> r (lastX, (float) y.getStart(), x - lastX, (float) y.getLength());
                        path.addRoundedRectangle (r.reduced (jlimit (0.5f, indent, r.getWidth()  / 8.0f),
                                                             jlimit (0.5f, indent, r.getHeight() / 8.0f)), 2.0f);
                    }

                    lastX = x;
                    ++index;
                }
            }

            // Add the vertical lines
            for (float x : noteXes)
                grid.addWithoutMerging ({ x - 0.25f, 0.0f, 0.5f, (float) getHeight() });

            // Add the missing right and bottom edges
            {
                auto r = getLocalBounds().toFloat();
                grid.addWithoutMerging (r.removeFromBottom (0.5f).translated (0.0f, -0.25f));
                grid.addWithoutMerging (r.removeFromLeft (0.5f).translated (-0.25f, 0.0f));
            }

            // Calculate playhead rect
            {
                auto r = getLocalBounds().toFloat();
                float lastX = 0.0f;

                for (float x : noteXes)
                {
                    if (playheadX >= lastX && playheadX < x)
                    {
                        playheadRect = r.withX (lastX).withRight (x);
                        break;
                    }

                    lastX = x;
                }
            }

            repaint();
        }

        void paint (Graphics& g) override
        {
            g.setColour (Colours::white.withMultipliedAlpha (0.5f));
            g.fillRectList (grid);

            g.setColour (Colours::white.withMultipliedAlpha (0.7f));
            g.fillPath (activeCells);

            g.setColour (Colours::white);
            g.fillPath (playingCells);

            g.setColour (Colours::white.withMultipliedAlpha (0.5f));
            g.fillRect (playheadRect);

            const Range<float> x = getSequenceIndexXRange (mouseOverCellIndex);
            const Range<float> y = editor.getChannelYRange (mouseOverChannel);
            g.setColour (Colours::red);
            g.drawRect (Rectangle<float>::leftTopRightBottom (x.getStart(), y.getStart(), x.getEnd(), y.getEnd()), 2.0f);
        }

        void resized() override
        {
            updateNoteXes();
            updatePaths();
        }

        void mouseEnter (const MouseEvent& e) override
        {
            updateNoteUnderMouse (e);
        }

        void mouseMove (const MouseEvent& e) override
        {
            updateNoteUnderMouse (e);
        }

        void mouseDown (const MouseEvent& e) override
        {
            updateNoteUnderMouse (e);

            paintSolidCells = true;

            if (e.mods.isCtrlDown() || e.mods.isCommandDown())
                paintSolidCells = false;
            else if (! e.mods.isShiftDown())
                paintSolidCells = ! editor.clip.getPattern (0).getNote (mouseOverChannel, mouseOverCellIndex);

            setCellAtLastMousePosition (paintSolidCells);
        }

        void mouseDrag (const MouseEvent& e) override
        {
            updateNoteUnderMouse (e);
            setCellAtLastMousePosition (paintSolidCells);
        }

        void mouseExit (const MouseEvent&) override
        {
            setNoteUnderMouse (-1, -1);
        }

    private:
        void updateNoteXes()
        {
            // We're going to do a quick bodge here just fit the notes to the current size
            noteXes.clearQuick();

            auto r = getLocalBounds().toFloat();
            const auto pattern (editor.clip.getPattern (0));
            const int numNotes = pattern.getNumNotes();
            const float w = r.getWidth() / std::max (1, numNotes);

            for (int i = 0; i < numNotes; ++i)
                noteXes.add ((i + 1) * w);
        }

        float getPlayheadX() const
        {
            auto clipRange = editor.clip.getEditTimeRange();

            if (clipRange.isEmpty())
                return 0.0f;

            const double position = editor.transport.position;
            const auto proportion = position / clipRange.getEnd();
            auto r = getLocalBounds().toFloat();

            return r.getWidth() * float (proportion);
        }

        int xToSequenceIndex (float x) const
        {
            if (x >= 0)
                for (int i = 0; i < noteXes.size(); ++i)
                    if (x < noteXes.getUnchecked (i))
                        return i;

            return -1;
        }

        Range<float> getSequenceIndexXRange (int index) const
        {
            if (! isPositiveAndBelow (index, noteXes.size()) || noteXes.size() <= 2)
                return {};

            if (index == 0)
                return { 0.0f, noteXes.getUnchecked (index) };

            return { noteXes.getUnchecked (index - 1), noteXes.getUnchecked (index) };
        }

        bool setNoteUnderMouse (int newIndex, int newChannel)
        {
            if (newIndex != mouseOverCellIndex || newChannel != mouseOverChannel)
            {
                mouseOverCellIndex = newIndex;
                mouseOverChannel = newChannel;
                resized();
                repaint();
                return true;
            }

            return false;
        }

        bool updateNoteUnderMouse (const MouseEvent& e)
        {
            return setNoteUnderMouse (xToSequenceIndex ((float) e.x), editor.yToChannel ((float) e.y));
        }

        void setCellAtLastMousePosition (bool value)
        {
            editor.clip.getPattern (0).setNote (mouseOverChannel, mouseOverCellIndex, value);
        }

        StepEditor& editor;
        Array<float> noteXes;
        RectangleList<float> grid;
        Path activeCells, playingCells;
        Rectangle<float> playheadRect;
        int mouseOverCellIndex = -1, mouseOverChannel = -1;
        bool paintSolidCells = true;
    };

    //==============================================================================
    void paint (Graphics&) override
    {
    }

    void resized() override
    {
        auto r = getLocalBounds();
        auto configR = r.removeFromLeft (150);

        for (auto c : channelConfigs)
        {
            auto channelR = configR.toFloat();
            channelR.setVerticalRange (getChannelYRange (c->channelIndex));
            c->setBounds (channelR.getSmallestIntegerContainer());
        }

        patternEditor.setBounds (r);
    }

private:
    te::StepClip& clip;
    te::TransportControl& transport;
    te::LambdaTimer timer;

    OwnedArray<ChannelConfig> channelConfigs;
    PatternEditor patternEditor { *this };

    int yToChannel (float y) const
    {
        auto r = getLocalBounds().toFloat();
        const int numChans = clip.getChannels().size();

        if (r.isEmpty())
            return -1;

        return (int) std::floor (y / r.getHeight() * numChans);
    }

    Range<float> getChannelYRange (int channelIndex) const
    {
        auto r = getLocalBounds().toFloat();
        const int numChans = clip.getChannels().size();

        if (numChans == 0)
            return {};

        const float h = r.getHeight() / numChans;

        return Range<float>::withStartAndLength (channelIndex * h, h);
    }

    void selectableObjectChanged (te::Selectable*) override
    {
        // This is our StepClip telling us that one of it's properties has changed
        patternEditor.updatePaths();
    }

    void selectableObjectAboutToBeDeleted (te::Selectable*) override
    {
        jassertfalse;
    }
};

//==============================================================================
class StepSequencerDemo : public Component,
                          private ChangeListener,
                          private Slider::Listener
{
public:
    //==============================================================================
    StepSequencerDemo()
    {
        transport.addChangeListener (this);

        createStepClip();
        createSamplerPlugin (createSampleFiles());

        stepEditor = std::make_unique<StepEditor> (*getClip());
        Helpers::addAndMakeVisible (*this, { &settingsButton, &playPauseButton, &randomiseButton,
                                             &clearButton, &tempoSlider, stepEditor.get() });

        updatePlayButtonText();

        settingsButton.onClick  = [this] { EngineHelpers::showAudioDeviceSettings (engine); };
        playPauseButton.onClick = [this] { EngineHelpers::togglePlay (edit); };
        randomiseButton.onClick = [this] { getClip()->getPattern (0).randomiseSteps(); };
        clearButton.onClick     = [this] { getClip()->getPattern (0).clear(); };

        tempoSlider.setRange (30.0, 220.0, 0.1);
        tempoSlider.setValue (edit.tempoSequence.getTempos()[0]->getBpm(), dontSendNotification);
        tempoSlider.addListener (this);

        setSize (600, 400);
    }

    ~StepSequencerDemo()
    {
        // Clean up our temporary sample files and projects
        engine.getTemporaryFileManager().getTempDirectory().deleteRecursively();
    }

    //==============================================================================
    void sliderValueChanged (Slider*) override
    {        
        if (! ModifierKeys::getCurrentModifiers().isAnyMouseButtonDown())
            edit.tempoSequence.getTempos()[0]->setBpm (tempoSlider.getValue());
    }

    void sliderDragEnded (Slider*) override
    {
        edit.tempoSequence.getTempos()[0]->setBpm (tempoSlider.getValue());
    }

    void paint (Graphics& g) override
    {
        g.fillAll (getLookAndFeel().findColour (ResizableWindow::backgroundColourId));
    }

    void resized() override
    {
        auto r = getLocalBounds();

        {
            auto topR = r.removeFromTop (30);
            settingsButton.setBounds (topR.removeFromLeft (topR.getWidth() / 3).reduced (2));
            playPauseButton.setBounds (topR.removeFromLeft (topR.getWidth() / 2).reduced (2));
            randomiseButton.setBounds (topR.removeFromLeft (topR.getWidth() / 2).reduced (2));
            clearButton.setBounds (topR.reduced (2));
        }

        {
            auto bottomR = r.removeFromBottom (60);
            tempoSlider.setBounds (bottomR.reduced (2));
        }

        stepEditor->setBounds (r);
    }

private:
    //==============================================================================
    te::Engine engine { ProjectInfo::projectName };
    te::Edit edit { engine, te::createEmptyEdit(), te::Edit::forEditing, nullptr, 0 };
    te::TransportControl& transport { edit.getTransport() };

    TextButton settingsButton { "Settings" }, playPauseButton { "Play" }, randomiseButton { "Randomise" }, clearButton { "Clear" };
    Slider tempoSlider;
    std::unique_ptr<StepEditor> stepEditor;

    //==============================================================================
    te::StepClip::Ptr createStepClip()
    {
        if (auto track = EngineHelpers::getOrInsertAudioTrackAt (edit, 0))
        {
            // Find length of 1 bar
            const te::EditTimeRange editTimeRange (0, edit.tempoSequence.barsBeatsToTime ({ 1, 0.0 }));
            track->insertNewClip (te::TrackItem::Type::step, "Step Clip", editTimeRange, nullptr);

            if (auto stepClip = getClip())
                return EngineHelpers::loopAroundClip (*stepClip);
        }

        return {};
    }

    Array<File> createSampleFiles()
    {
        Array<File> files;
        const auto destDir = edit.getTempDirectory (true);
        jassert (destDir != File());

        using namespace DemoBinaryData;

        for (int i = 0; i < namedResourceListSize; ++i)
        {
            const auto f = destDir.getChildFile (originalFilenames[i]);

            int dataSizeInBytes = 0;
            const char* data = getNamedResource (namedResourceList[i], dataSizeInBytes);
            jassert (data != nullptr);
            f.replaceWithData (data, (size_t) dataSizeInBytes);
            files.add (f);
        }

        return files;
    }

    void createSamplerPlugin (Array<File> defaultSampleFiles)
    {
        if (auto stepClip = getClip())
        {
            if (auto sampler = dynamic_cast<te::SamplerPlugin*> (edit.getPluginCache().createNewPlugin (te::SamplerPlugin::xmlTypeName, {}).get()))
            {
                stepClip->getTrack()->pluginList.insertPlugin (*sampler, 0, nullptr);

                int channelCount = 0;

                for (auto channel : stepClip->getChannels())
                {
                    const auto error = sampler->addSound (defaultSampleFiles[channelCount++].getFullPathName(), channel->name.get(), 0.0, 0.0, 1.0f);
                    sampler->setSoundParams (sampler->getNumSounds() - 1, channel->noteNumber, channel->noteNumber, channel->noteNumber);
                    jassert (error.isEmpty());

                    for (auto& pattern : stepClip->getPatterns())
                        pattern.randomiseChannel (channel->getIndex());
                }
            }
        }
        else
        {
            jassertfalse; // StepClip not been created yet?
        }
    }

    //==============================================================================
    te::StepClip::Ptr getClip()
    {
        if (auto track = EngineHelpers::getOrInsertAudioTrackAt (edit, 0))
            if (auto clip = dynamic_cast<te::StepClip*> (track->getClips()[0]))
                return *clip;

        return {};
    }

    void updatePlayButtonText()
    {
        playPauseButton.setButtonText (transport.isPlaying() ? "Pause" : "Play");
    }

    void changeListenerCallback (ChangeBroadcaster*) override
    {
        updatePlayButtonText();
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StepSequencerDemo)
};
