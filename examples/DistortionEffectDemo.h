/*******************************************************************************
 The block below describes the properties of this PIP. A PIP is a short snippet
 of code that can be read by the Projucer and used to generate a JUCE project.

 BEGIN_JUCE_PIP_METADATA

  name:             DistortionEffectDemo
  version:          0.0.1
  vendor:           Tracktion
  website:          www.tracktion.com
  description:      This example simply loads a project from the command line and plays it back in a loop.

  dependencies:     juce_audio_basics, juce_audio_devices, juce_audio_formats, juce_audio_processors, juce_audio_utils,
					juce_core, juce_data_structures, juce_dsp, juce_events, juce_graphics,
					juce_gui_basics, juce_gui_extra, juce_osc, tracktion_engine
  exporters:        linux_make, vs2017, xcode_iphone, xcode_mac

  moduleFlags:      JUCE_STRICT_REFCOUNTEDPOINTER=1, JUCE_PLUGINHOST_AU=1, JUCE_PLUGINHOST_VST3=1, JUCE_MODAL_LOOPS_PERMITTED=1

  type:             Component
  mainClass:        DistortionEffectDemo

 END_JUCE_PIP_METADATA

*******************************************************************************/

#pragma once

#include "common/Utilities.h"
#include "common/Components.h"
#include "common/PluginWindow.h"

using namespace tracktion_engine;

namespace tracktion_engine
{
	class DistortionPlugin : public Plugin
	{
	public:




		static const char* getPluginName() { return NEEDS_TRANS("Distortion"); }


		static const char* xmlTypeName;


		juce::String getName() override { return TRANS("Distortion"); }
		juce::String getPluginType() override { return xmlTypeName; }
		juce::String getShortName(int) override { return TRANS("Dist"); }
		int getNumOutputChannelsGivenInputs(int numInputChannels) override { return juce::jmin(numInputChannels, 2); }

		bool needsConstantBufferSize() override { return false; }



		juce::String getSelectableDescription() override { return TRANS("Distortion Plugin"); }





		static float getMinThreshold() { return 0.01f; }
		static float getMaxThreshold() { return 1.0f; }

		double currentLevel = 0.0;
		float lastSamp = 0.0f;




		DistortionPlugin(PluginCreationInfo info) : Plugin(info)
		{
		}

		~DistortionPlugin() override
		{
			notifyListenersOfDeletion();


		}



		void initialise(const PluginInitialisationInfo&)
		{
			currentLevel = 0.0;
			lastSamp = 0.0f;
		}

		void deinitialise()
		{
		}

		//Currently trying to see whether or not any processing is applied by attempting to apply a gain of 0.
		
		void applyToBuffer (const PluginRenderContext& fc)
		{
	
		//	clearChannels(*fc.destBuffer, 0,1, fc.bufferStartSample, fc.bufferNumSamples);
			fc.destBuffer->applyGainRamp(0, 0, fc.bufferNumSamples, 0, 0);
			auto outL = fc.destBuffer->getWritePointer(0);
			auto outR = fc.destBuffer->getWritePointer(1);
			for (int i = 0; i < fc.bufferNumSamples; i++)
			{
				fc.destBuffer->applyGain(0.0f);
				outL[i] *= 0.0f;
				outR[i] *= 0.0f;
			}

	//		fc.destBuffer->clear();

		}



		void restorePluginStateFromValueTree(const juce::ValueTree& v)
		{

			/*
			CachedValue<float>* cvsFloat[] = { &thresholdValue, &ratioValue, &attackValue, &releaseValue, &outputValue, &sidechainValue, nullptr };
			CachedValue<bool>* cvsBool[] = { &useSidechainTrigger, nullptr };
			copyPropertiesToNullTerminatedCachedValues(v, cvsFloat);
			copyPropertiesToNullTerminatedCachedValues(v, cvsBool);
			*/


			for (auto p : getAutomatableParameters())
				p->updateFromAttachedValue();

				
		}

		void valueTreePropertyChanged(ValueTree& v, const juce::Identifier& id)
		{
			if (v == state && id == IDs::sidechainTrigger)
				propertiesChanged();

			Plugin::valueTreePropertyChanged(v, id);
		}



		JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DistortionPlugin)
	};

	const char* DistortionPlugin::xmlTypeName = "Distortion";
}

//==============================================================================
#pragma once

#include "common/Utilities.h"
#include "common/PlaybackDemoAudio.h"

//==============================================================================
class DistortionEffectDemo : public Component,
	private ChangeListener,
	private Timer
{
public:


	//==============================================================================
	DistortionEffectDemo()
	{

		engine.getPluginManager().createBuiltInType<DistortionPlugin>();

		

		const auto editFilePath = JUCEApplication::getCommandLineParameters().replace("-NSDocumentRevisionsDebugMode YES", "").unquoted().trim();

		const File editFile(editFilePath);



		auto f = File::createTempFile(".ogg");
		f.replaceWithData(PlaybackDemoAudio::BITs_Export_2_ogg, PlaybackDemoAudio::BITs_Export_2_oggSize);

		edit = te::createEmptyEdit(engine, editFile);
		edit->getTransport().addChangeListener(this);




		//Creates clip. Loads clip from file f.
		//Creates track. Loads clip into track.
		WaveAudioClip::Ptr clip;
		auto track = EngineHelpers::getOrInsertAudioTrackAt(*edit, 0);

		if (track)
		{
			// Add a new clip to this track
			te::AudioFile audioFile(edit->engine, f);

			clip = track->insertWaveClip(f.getFileNameWithoutExtension(), f,
				{ { 0.0, audioFile.getLength() }, 0.0 }, false);

		
		}


		//Places clip on track 1, sets loop start to beginning of clip and loop end to end of clip.
		//Looping set to true, and play set to true to start the loop.
		auto& transport = clip->edit.getTransport();
		transport.setLoopRange(clip->getEditTimeRange());
		transport.looping = true;
		transport.position = 0.0;
		transport.play(false);


		//Now I need to find out how to embed transport output into the PluginRenderContext.





		editNameLabel.setText("Demo Song", dontSendNotification);

		playPauseButton.onClick = [this] { EngineHelpers::togglePlay(*edit); };

		// Show the plugin scan dialog
		// If you're loading an Edit with plugins in, you'll need to perform a scan first
		pluginsButton.onClick = [this]
		{
			DialogWindow::LaunchOptions o;
			o.dialogTitle = TRANS("Plugins");
			o.dialogBackgroundColour = Colours::black;
			o.escapeKeyTriggersCloseButton = true;
			o.useNativeTitleBar = true;
			o.resizable = true;
			o.useBottomRightCornerResizer = true;

			auto v = new PluginListComponent(engine.getPluginManager().pluginFormatManager,
				engine.getPluginManager().knownPluginList,
				engine.getTemporaryFileManager().getTempFile("PluginScanDeadMansPedal"),
				te::getApplicationSettings());
			v->setSize(800, 600);
			o.content.setOwned(v);
			o.launchAsync();
		};

		settingsButton.onClick = [this] { EngineHelpers::showAudioDeviceSettings(engine); };
		updatePlayButtonText();
		editNameLabel.setJustificationType(Justification::centred);
		cpuLabel.setJustificationType(Justification::centred);
		Helpers::addAndMakeVisible(*this, { &pluginsButton, &settingsButton, &playPauseButton, &editNameLabel, &cpuLabel });




		setSize(600, 400);
		startTimerHz(5);
	}

	~DistortionEffectDemo() override
	{
		engine.getTemporaryFileManager().getTempDirectory().deleteRecursively();
	}

	//==============================================================================
	void paint(Graphics& g) override
	{
		g.fillAll(getLookAndFeel().findColour(ResizableWindow::backgroundColourId));
	}

	void resized() override
	{
		auto r = getLocalBounds();
		auto topR = r.removeFromTop(30);
		pluginsButton.setBounds(topR.removeFromLeft(topR.getWidth() / 3).reduced(2));
		settingsButton.setBounds(topR.removeFromLeft(topR.getWidth() / 2).reduced(2));
		playPauseButton.setBounds(topR.reduced(2));

		auto middleR = r.withSizeKeepingCentre(r.getWidth(), 40);

		cpuLabel.setBounds(middleR.removeFromBottom(20));
		editNameLabel.setBounds(middleR);
	}

private:
	//==============================================================================
	te::Engine engine{ ProjectInfo::projectName };
	std::unique_ptr<te::Edit> edit;

	TextButton pluginsButton{ "Plugins" }, settingsButton{ "Settings" }, playPauseButton{ "Play" };
	Label editNameLabel{ "No Edit Loaded" }, cpuLabel;

	//==============================================================================
	void updatePlayButtonText()
	{
		if (edit != nullptr)
			playPauseButton.setButtonText(edit->getTransport().isPlaying() ? "Pause" : "Play");
	}

	void changeListenerCallback(ChangeBroadcaster*) override
	{
		updatePlayButtonText();
	}

	void timerCallback() override
	{
		cpuLabel.setText(String::formatted("CPU: %d%%", int(engine.getDeviceManager().getCpuUsage() * 100)), dontSendNotification);
	}

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DistortionEffectDemo)
};

