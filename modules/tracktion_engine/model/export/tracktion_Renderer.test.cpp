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

#if TRACKTION_UNIT_TESTS

//==============================================================================
//==============================================================================
class TrackFreezeTests  : public juce::UnitTest
{
public:
    TrackFreezeTests()
        : juce::UnitTest ("Track Freeze", "Tracktion:Longer")
    {
    }

    void runTest() override
    {
        auto& engine = *Engine::getEngines()[0];

        beginTest ("End allowance");
        {
            auto edit = test_utilities::createTestEdit (engine);
            auto track = getAudioTracks (*edit)[0];

            // Create a track with a plugin with end allowance
            auto synth = dynamic_cast<FourOscPlugin*> (edit->getPluginCache().createNewPlugin (FourOscPlugin::xmlTypeName, {}).get());
            static auto organPatch = "<PLUGIN type=\"4osc\" id=\"1069\" enabled=\"1\" filterType=\"1\" presetName=\"4OSC: Organ\" filterFreq=\"127.0\" ampAttack=\"0.60000002384185791016\" ampDecay=\"10.0\" ampSustain=\"100.0\" ampRelease=\"0.40000000596046447754\" waveShape1=\"4\" tune2=\"-24.0\" waveShape2=\"4\"> <MODMATRIX/> </PLUGIN>";

            if (auto e = juce::parseXML (organPatch))
                if (auto v = juce::ValueTree::fromXml (*e); v.isValid())
                    synth->restorePluginStateFromValueTree (v);

            const auto tailLength = synth->getTailLength();
            expectGreaterThan (tailLength, 0.0);

            track->pluginList.insertPlugin (*synth, 0, nullptr);

            // Insert a MIDI clip with a 1-beat note
            auto midiClip = track->insertMIDIClip ({ 0.0s, TimePosition (1.0s) }, nullptr);
            midiClip->getSequence().addNote (69, BeatPosition::fromBeats (0.0), BeatDuration::fromBeats (1.0), 127, 0, nullptr);
            const auto trackLength = track->getLengthIncludingInputTracks();
            expectWithinAbsoluteError (trackLength.inSeconds(), 1.0, 0.001);

            // Check expected end allowance
            juce::Array<EditItemID> trackIDs { track->itemID };
            juce::Array<Clip*> clips { midiClip.get() };
            const auto endAllowance = RenderOptions::findEndAllowance (*edit, &trackIDs, &clips);
            expectWithinAbsoluteError (endAllowance.inSeconds(), tailLength, 0.001);

            // Ensure freezing that track has the track length plus the end allowance
            const auto expectedTotalLength = trackLength + TimeDuration::fromSeconds (tailLength);

            {
                track->setFrozen (true, AudioTrack::individualFreeze);
                const auto freezeFile = AudioFile (engine, TemporaryFileManager::getFreezeFileForTrack (*track));
                expectWithinAbsoluteError (freezeFile.getLength(), expectedTotalLength.inSeconds(), 0.001);

                engine.getAudioFileManager().releaseAllFiles();
                edit->getTempDirectory (false).deleteRecursively();
                expect (! freezeFile.getFile().exists());
            }

            {
                track->setFrozen (false, AudioTrack::individualFreeze);

                // Create a new track and set the destination of the first one to this
                auto track2 = edit->insertNewAudioTrack ({ {},{} }, nullptr);
                track->getOutput().setOutputToTrack (track2.get());
                trackIDs = juce::Array<EditItemID> { track2->itemID };
                expectEquals (RenderOptions::findEndAllowance (*edit, &trackIDs, nullptr), TimeDuration(),
                              "End allowance of new empty track is not 0");

                // Now freeze this track and it should contain track's end allowance
                track2->setFrozen (true, AudioTrack::individualFreeze);
                const auto freezeFile = AudioFile (engine, TemporaryFileManager::getFreezeFileForTrack (*track2));
                expectWithinAbsoluteError (freezeFile.getLength(), expectedTotalLength.inSeconds(), 0.001);

                engine.getAudioFileManager().releaseAllFiles();
                edit->getTempDirectory (false).deleteRecursively();
                expect (! freezeFile.getFile().exists());
            }
        }
    }
};

static TrackFreezeTests trackFreezeTests;

#endif

}} // namespace tracktion { inline namespace engine
