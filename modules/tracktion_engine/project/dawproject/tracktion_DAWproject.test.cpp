/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#if TRACKTION_UNIT_TESTS && ENGINE_UNIT_TESTS_DAWPROJECT

#include <tracktion_engine/../3rd_party/doctest/tracktion_doctest.hpp>

namespace tracktion::inline engine
{

TEST_SUITE ("tracktion_engine")
{
    //==============================================================================
    TEST_CASE ("DAWproject: velocity conversion")
    {
        using namespace dawproject;

        // Test velocity to normalized
        CHECK_EQ (velocityToNormalized (0), doctest::Approx (0.0f));
        CHECK_EQ (velocityToNormalized (127), doctest::Approx (1.0f));
        CHECK_EQ (velocityToNormalized (64), doctest::Approx (64.0f / 127.0f));

        // Test normalized to velocity
        CHECK_EQ (normalizedToVelocity (0.0f), 0);
        CHECK_EQ (normalizedToVelocity (1.0f), 127);
        CHECK_EQ (normalizedToVelocity (0.5f), 64);

        // Test round-trip
        for (int vel = 0; vel <= 127; ++vel)
        {
            auto normalized = velocityToNormalized (vel);
            auto backToVel = normalizedToVelocity (normalized);
            CHECK_EQ (backToVel, vel);
        }
    }

    //==============================================================================
    TEST_CASE ("DAWproject: time conversion")
    {
        using namespace dawproject;

        // Test ticks to beats (960 ticks per quarter note)
        CHECK_EQ (ticksToBeats (0), doctest::Approx (0.0));
        CHECK_EQ (ticksToBeats (960), doctest::Approx (1.0));
        CHECK_EQ (ticksToBeats (480), doctest::Approx (0.5));
        CHECK_EQ (ticksToBeats (1920), doctest::Approx (2.0));

        // Test beats to ticks
        CHECK_EQ (beatsToTicks (0.0), 0);
        CHECK_EQ (beatsToTicks (1.0), 960);
        CHECK_EQ (beatsToTicks (0.5), 480);
        CHECK_EQ (beatsToTicks (2.0), 1920);

        // Test round-trip
        for (int ticks = 0; ticks <= 9600; ticks += 240)
        {
            auto beats = ticksToBeats (ticks);
            auto backToTicks = beatsToTicks (beats);
            CHECK_EQ (backToTicks, ticks);
        }
    }

    //==============================================================================
    TEST_CASE ("DAWproject: color conversion")
    {
        using namespace dawproject;

        // Test colour to string (JUCE outputs uppercase hex)
        CHECK_EQ (colourToDAWprojectString (juce::Colour (0xff, 0x00, 0x00)).toLowerCase(), "#ff0000");
        CHECK_EQ (colourToDAWprojectString (juce::Colour (0x00, 0xff, 0x00)).toLowerCase(), "#00ff00");
        CHECK_EQ (colourToDAWprojectString (juce::Colour (0x00, 0x00, 0xff)).toLowerCase(), "#0000ff");

        // Test string to colour
        CHECK_EQ (dawprojectStringToColour ("#ff0000").getRed(), 255);
        CHECK_EQ (dawprojectStringToColour ("#00ff00").getGreen(), 255);
        CHECK_EQ (dawprojectStringToColour ("#0000ff").getBlue(), 255);

        // Test round-trip
        auto testColour = juce::Colour (0x12, 0x34, 0x56);
        auto str = colourToDAWprojectString (testColour);
        auto backToColour = dawprojectStringToColour (str);
        CHECK_EQ (backToColour.getRed(), testColour.getRed());
        CHECK_EQ (backToColour.getGreen(), testColour.getGreen());
        CHECK_EQ (backToColour.getBlue(), testColour.getBlue());
    }

    //==============================================================================
    TEST_CASE ("DAWproject: ID generator")
    {
        using namespace dawproject;

        IDGenerator gen;

        auto id1 = gen.generateID();
        auto id2 = gen.generateID();
        auto id3 = gen.generateID();

        CHECK_EQ (id1, "id1");
        CHECK_EQ (id2, "id2");
        CHECK_EQ (id3, "id3");

        // IDs should be unique
        CHECK_NE (id1, id2);
        CHECK_NE (id2, id3);

        // Test reset
        gen.reset();
        auto id4 = gen.generateID();
        CHECK_EQ (id4, "id1");
    }

    //==============================================================================
    TEST_CASE ("DAWproject: round-trip empty project")
    {
        using namespace dawproject;

        auto& engine = *Engine::getEngines()[0];
        auto edit = test_utilities::createTestEdit (engine, 0);
        edit->tempoSequence.getTempo (0)->setBpm (120.0);

        // Export to XML
        WriteOptions writeOpts;
        writeOpts.embedAudioFiles = false;
        auto xmlResult = createDAWproject (*edit, writeOpts);

        REQUIRE (xmlResult.has_value());
        auto& xml = *xmlResult.value();

        CHECK (xml.hasTagName ("Project"));
        CHECK (xml.getChildByName ("Application") != nullptr);
        CHECK (xml.getChildByName ("Transport") != nullptr);
        CHECK (xml.getChildByName ("Structure") != nullptr);

        // Check tempo
        auto* transport = xml.getChildByName ("Transport");
        REQUIRE (transport != nullptr);
        auto* tempo = transport->getChildByName ("Tempo");
        REQUIRE (tempo != nullptr);
        CHECK_EQ (tempo->getDoubleAttribute ("value"), doctest::Approx (120.0));
    }

    //==============================================================================
    TEST_CASE ("DAWproject: round-trip single audio track")
    {
        using namespace dawproject;

        auto& engine = *Engine::getEngines()[0];
        auto edit = test_utilities::createTestEdit (engine, 1);

        auto* track = getAudioTracks (*edit)[0];
        REQUIRE (track != nullptr);
        track->setName ("Test Track");
        track->setColour (juce::Colour (0xff, 0x00, 0x00));

        // Export to XML
        WriteOptions writeOpts;
        writeOpts.embedAudioFiles = false;
        auto xmlResult = createDAWproject (*edit, writeOpts);

        REQUIRE (xmlResult.has_value());
        auto& xml = *xmlResult.value();

        // Find the track in structure
        auto* structure = xml.getChildByName ("Structure");
        REQUIRE (structure != nullptr);

        // Find our specific track by name
        juce::XmlElement* trackElement = nullptr;
        for (auto* child : structure->getChildIterator())
        {
            if (child->hasTagName ("Track") && child->getStringAttribute ("name") == "Test Track")
            {
                trackElement = child;
                break;
            }
        }

        REQUIRE (trackElement != nullptr);
        CHECK_EQ (trackElement->getStringAttribute ("name").toStdString(), "Test Track");
        CHECK_EQ (trackElement->getStringAttribute ("color").toLowerCase().toStdString(), "#ff0000");
    }

    //==============================================================================
    TEST_CASE ("DAWproject: round-trip MIDI clip")
    {
        using namespace dawproject;

        auto& engine = *Engine::getEngines()[0];
        auto edit = test_utilities::createTestEdit (engine, 1);

        auto* track = getAudioTracks (*edit)[0];
        REQUIRE (track != nullptr);

        // Create MIDI clip with some notes
        auto midiClip = track->insertMIDIClip ({ 0_tp, 4_tp }, nullptr);
        REQUIRE (midiClip != nullptr);
        midiClip->setName ("Test MIDI Clip");

        auto& sequence = midiClip->getSequence();

        sequence.addNote (60, BeatPosition::fromBeats (0.0), BeatDuration::fromBeats (1.0), 100, 0, nullptr);
        sequence.addNote (64, BeatPosition::fromBeats (1.0), BeatDuration::fromBeats (0.5), 80, 0, nullptr);
        sequence.addNote (67, BeatPosition::fromBeats (2.0), BeatDuration::fromBeats (2.0), 127, 0, nullptr);

        // Export to XML
        WriteOptions writeOpts;
        writeOpts.embedAudioFiles = false;
        auto xmlResult = createDAWproject (*edit, writeOpts);

        REQUIRE (xmlResult.has_value());
        auto& xml = *xmlResult.value();

        // Find the arrangement
        auto* arrangement = xml.getChildByName ("Arrangement");
        REQUIRE (arrangement != nullptr);

        // Find Lanes
        auto* lanes = arrangement->getChildByName ("Lanes");
        REQUIRE (lanes != nullptr);

        // Find track lanes
        auto* trackLanes = lanes->getChildByName ("Lanes");
        REQUIRE (trackLanes != nullptr);

        // Find clips
        auto* clips = trackLanes->getChildByName ("Clips");
        REQUIRE (clips != nullptr);

        // Find our clip
        auto* clipElement = clips->getChildByName ("Clip");
        REQUIRE (clipElement != nullptr);
        CHECK_EQ (clipElement->getStringAttribute ("name"), "Test MIDI Clip");

        // Check notes (Notes is now inside a Lanes element within the Clip)
        auto* clipLanes = clipElement->getChildByName ("Lanes");
        REQUIRE (clipLanes != nullptr);

        auto* notes = clipLanes->getChildByName ("Notes");
        REQUIRE (notes != nullptr);

        int noteCount = 0;
        for (auto* child : notes->getChildIterator())
        {
            if (child->hasTagName ("Note"))
                ++noteCount;
        }
        CHECK_EQ (noteCount, 3);
    }

    //==============================================================================
    TEST_CASE ("DAWproject: write and read file")
    {
        using namespace dawproject;

        auto& engine = *Engine::getEngines()[0];
        auto edit = test_utilities::createTestEdit (engine, 2);

        auto audioTracks = getAudioTracks (*edit);
        REQUIRE (audioTracks.size() >= 2);

        audioTracks[0]->setName ("Track 1");
        audioTracks[1]->setName ("Track 2");

        edit->tempoSequence.getTempo (0)->setBpm (140.0);

        // Add MIDI clip to first track
        auto midiClip = audioTracks[0]->insertMIDIClip ({ 0_tp, 2_tp }, nullptr);
        REQUIRE (midiClip != nullptr);
        midiClip->getSequence().addNote (60, BeatPosition::fromBeats (0.0), BeatDuration::fromBeats (1.0),
                                          100, 0, nullptr);

        // Write to file
        juce::TemporaryFile tempFile (".dawproject");
        WriteOptions writeOpts;
        writeOpts.embedAudioFiles = false;

        auto result = writeDAWprojectFile (tempFile.getFile(), *edit, writeOpts);
        CHECK (result.wasOk());

        // Read back
        ParseOptions parseOpts;
        parseOpts.extractAudioFiles = false;
        auto importedEdit = parseDAWproject (engine, tempFile.getFile(), parseOpts);

        REQUIRE (importedEdit != nullptr);

        // Verify structure - find our named tracks
        auto importedTracks = getAudioTracks (*importedEdit);
        CHECK_GE (importedTracks.size(), 2);

        // Find our specific tracks by name
        bool foundTrack1 = false, foundTrack2 = false;
        for (auto* track : importedTracks)
        {
            if (track->getName() == "Track 1")
                foundTrack1 = true;
            if (track->getName() == "Track 2")
                foundTrack2 = true;
        }
        CHECK (foundTrack1);
        CHECK (foundTrack2);

        // Verify tempo
        auto* importedTempo = importedEdit->tempoSequence.getTempo (0);
        REQUIRE (importedTempo != nullptr);
        CHECK_EQ (importedTempo->getBpm(), doctest::Approx (140.0));
    }

    //==============================================================================
    TEST_CASE ("DAWproject: tempo automation export")
    {
        using namespace dawproject;

        auto& engine = *Engine::getEngines()[0];
        auto edit = test_utilities::createTestEdit (engine, 1);

        // Add tempo changes
        auto& tempoSeq = edit->tempoSequence;
        tempoSeq.getTempo (0)->setBpm (120.0);
        tempoSeq.insertTempo (2_bp, 140.0, 0.0f);
        tempoSeq.insertTempo (4_bp, 100.0, 0.0f);

        // Export to XML
        WriteOptions writeOpts;
        auto xmlResult = createDAWproject (*edit, writeOpts);

        REQUIRE (xmlResult.has_value());
        auto& xml = *xmlResult.value();

        auto* arrangement = xml.getChildByName ("Arrangement");
        REQUIRE (arrangement != nullptr);

        auto* tempoAuto = arrangement->getChildByName ("TempoAutomation");
        REQUIRE (tempoAuto != nullptr);

        // Count tempo points
        int pointCount = 0;
        for (auto* child : tempoAuto->getChildIterator())
        {
            if (child->hasTagName ("RealPoint"))
                ++pointCount;
        }
        CHECK_EQ (pointCount, 3);
    }

    //==============================================================================
    TEST_CASE ("DAWproject: time signature export")
    {
        using namespace dawproject;

        auto& engine = *Engine::getEngines()[0];
        auto edit = test_utilities::createTestEdit (engine, 1);

        // Set time signature
        auto& tempoSeq = edit->tempoSequence;
        auto* timeSig = tempoSeq.getTimeSig (0);
        REQUIRE (timeSig != nullptr);
        timeSig->setStringTimeSig ("6/8");

        // Export to XML
        WriteOptions writeOpts;
        auto xmlResult = createDAWproject (*edit, writeOpts);

        REQUIRE (xmlResult.has_value());
        auto& xml = *xmlResult.value();

        auto* transport = xml.getChildByName ("Transport");
        REQUIRE (transport != nullptr);

        auto* timeSigElement = transport->getChildByName ("TimeSignature");
        REQUIRE (timeSigElement != nullptr);
        CHECK_EQ (timeSigElement->getIntAttribute ("numerator"), 6);
        CHECK_EQ (timeSigElement->getIntAttribute ("denominator"), 8);
    }

    //==============================================================================
    TEST_CASE ("DAWproject: folder track export")
    {
        using namespace dawproject;

        auto& engine = *Engine::getEngines()[0];
        auto edit = test_utilities::createTestEdit (engine, 0);

        // Create folder track with child tracks
        TrackInsertPoint insertPoint (nullptr, nullptr);
        auto folderTrack = edit->insertNewFolderTrack (insertPoint, nullptr, false);
        REQUIRE (folderTrack != nullptr);
        folderTrack->setName ("My Folder");

        TrackInsertPoint childInsertPoint (folderTrack.get(), nullptr);
        auto childTrack = edit->insertNewAudioTrack (childInsertPoint, nullptr, false);
        REQUIRE (childTrack != nullptr);
        childTrack->setName ("Child Track");

        // Export to XML
        WriteOptions writeOpts;
        auto xmlResult = createDAWproject (*edit, writeOpts);

        REQUIRE (xmlResult.has_value());
        auto& xml = *xmlResult.value();

        auto* structure = xml.getChildByName ("Structure");
        REQUIRE (structure != nullptr);

        // Find folder track
        juce::XmlElement* folderElement = nullptr;
        for (auto* child : structure->getChildIterator())
        {
            if (child->hasTagName ("Track") && child->getStringAttribute ("name") == "My Folder")
            {
                folderElement = child;
                break;
            }
        }

        REQUIRE (folderElement != nullptr);
        CHECK (folderElement->getStringAttribute ("contentType").contains ("tracks"));

        // Check for nested track
        auto* nestedTrack = folderElement->getChildByName ("Track");
        REQUIRE (nestedTrack != nullptr);
        CHECK_EQ (nestedTrack->getStringAttribute ("name").toStdString(), "Child Track");
    }

    //==============================================================================
    TEST_CASE ("DAWproject: fuzz test with random data")
    {
        using namespace dawproject;

        // Seeded random for reproducibility
        const uint32_t seed = 12345;
        juce::Random random (seed);

        auto& engine = *Engine::getEngines()[0];
        auto edit = test_utilities::createTestEdit (engine, 0);

        // Configuration for fuzz test
        const int numTracks = 8;
        const int clipsPerTrack = 6;
        const int notesPerClip = 100;
        const int controllerEventsPerClip = 50;
        const double clipLength = 16.0; // beats

        // Track totals for verification
        int totalNotesCreated = 0;
        int totalControllerEventsCreated = 0;
        std::vector<juce::String> trackNames;

        // Create multiple tracks with MIDI clips
        for (int t = 0; t < numTracks; ++t)
        {
            auto trackName = "FuzzTrack_" + juce::String (t);
            trackNames.push_back (trackName);

            TrackInsertPoint insertPoint (nullptr, getTopLevelTracks (*edit).getLast());
            auto track = edit->insertNewAudioTrack (insertPoint, nullptr, false);
            REQUIRE (track != nullptr);
            track->setName (trackName);

            // Random track color
            track->setColour (juce::Colour::fromHSV (random.nextFloat(), 0.7f, 0.8f, 1.0f));

            // Create multiple clips on each track
            for (int c = 0; c < clipsPerTrack; ++c)
            {
                double clipStart = c * clipLength + random.nextDouble() * 2.0;
                double clipEnd = clipStart + clipLength - random.nextDouble() * 2.0;

                auto midiClip = track->insertMIDIClip ({ TimePosition::fromSeconds (clipStart),
                                                          TimePosition::fromSeconds (clipEnd) }, nullptr);
                REQUIRE (midiClip != nullptr);
                midiClip->setName ("Clip_" + juce::String (t) + "_" + juce::String (c));

                auto& sequence = midiClip->getSequence();

                // Generate many random notes
                for (int n = 0; n < notesPerClip; ++n)
                {
                    int pitch = 36 + random.nextInt (60); // C2 to C7
                    double startBeat = random.nextDouble() * (clipLength - 1.0);
                    double length = 0.1 + random.nextDouble() * 2.0;
                    int velocity = 1 + random.nextInt (127);

                    sequence.addNote (pitch,
                                      BeatPosition::fromBeats (startBeat),
                                      BeatDuration::fromBeats (length),
                                      velocity, 0, nullptr);
                    ++totalNotesCreated;
                }

                // Generate random controller events
                for (int cc = 0; cc < controllerEventsPerClip; ++cc)
                {
                    double beatPos = random.nextDouble() * clipLength;

                    // Mix of different controller types
                    int controllerType;
                    int controllerValue;

                    int typeChoice = random.nextInt (5);
                    switch (typeChoice)
                    {
                        case 0: // Modulation wheel (CC 1)
                            controllerType = 1;
                            controllerValue = random.nextInt (128) << 7;
                            break;
                        case 1: // Expression (CC 11)
                            controllerType = 11;
                            controllerValue = random.nextInt (128) << 7;
                            break;
                        case 2: // Sustain pedal (CC 64)
                            controllerType = 64;
                            controllerValue = (random.nextBool() ? 127 : 0) << 7;
                            break;
                        case 3: // Pitch bend
                            controllerType = MidiControllerEvent::pitchWheelType;
                            controllerValue = random.nextInt (16384);
                            break;
                        default: // Channel pressure
                            controllerType = MidiControllerEvent::channelPressureType;
                            controllerValue = random.nextInt (128) << 7;
                            break;
                    }

                    sequence.addControllerEvent (BeatPosition::fromBeats (beatPos),
                                                 controllerType, controllerValue, nullptr);
                    ++totalControllerEventsCreated;
                }
            }
        }

        INFO ("Created " << numTracks << " tracks with " << clipsPerTrack << " clips each");
        INFO ("Total notes created: " << totalNotesCreated);
        INFO ("Total controller events created: " << totalControllerEventsCreated);

        // Write to file
        juce::TemporaryFile tempFile (".dawproject");
        WriteOptions writeOpts;
        writeOpts.embedAudioFiles = false;

        auto result = writeDAWprojectFile (tempFile.getFile(), *edit, writeOpts);
        REQUIRE (result.wasOk());

        // Check file was created and has content
        CHECK (tempFile.getFile().existsAsFile());
        CHECK (tempFile.getFile().getSize() > 1000); // Should be substantial

        // Read back
        ParseOptions parseOpts;
        parseOpts.extractAudioFiles = false;
        auto importedEdit = parseDAWproject (engine, tempFile.getFile(), parseOpts);

        REQUIRE (importedEdit != nullptr);

        // Verify all our tracks were imported
        auto importedTracks = getAudioTracks (*importedEdit);
        for (const auto& trackName : trackNames)
        {
            bool found = false;
            for (auto* track : importedTracks)
            {
                if (track->getName() == trackName)
                {
                    found = true;
                    break;
                }
            }
            CHECK_MESSAGE (found, "Track not found: " << trackName);
        }

        // Count total imported notes and controller events
        int totalNotesImported = 0;
        int totalControllerEventsImported = 0;
        for (auto* track : importedTracks)
        {
            for (auto* clip : track->getClips())
            {
                if (auto* midiClip = dynamic_cast<MidiClip*> (clip))
                {
                    totalNotesImported += midiClip->getSequence().getNotes().size();
                    totalControllerEventsImported += midiClip->getSequence().getControllerEvents().size();
                }
            }
        }

        INFO ("Total notes imported: " << totalNotesImported);
        INFO ("Total controller events imported: " << totalControllerEventsImported);
        CHECK_EQ (totalNotesImported, totalNotesCreated);
        CHECK_EQ (totalControllerEventsImported, totalControllerEventsCreated);

        // Verify tempo was preserved
        auto* originalTempo = edit->tempoSequence.getTempo (0);
        auto* importedTempo = importedEdit->tempoSequence.getTempo (0);
        REQUIRE (originalTempo != nullptr);
        REQUIRE (importedTempo != nullptr);
        CHECK_EQ (importedTempo->getBpm(), doctest::Approx (originalTempo->getBpm()).epsilon (0.01));
    }

    //==============================================================================
    // Helpers for audio-import round-trip tests.
    static std::unique_ptr<juce::TemporaryFile> writeTestWavFile (Engine& engine,
                                                                   int numChannels = 1,
                                                                   double sampleRate = 44100.0,
                                                                   double durationSeconds = 0.25)
    {
        juce::WavAudioFormat format;
        auto tempFile = std::make_unique<juce::TemporaryFile> (".wav");

        AudioFile audioFile (engine, tempFile->getFile());
        const int bitDepth = 16;
        const int numSamples = static_cast<int> (sampleRate * durationSeconds);

        AudioFileWriter writer (audioFile, &format, numChannels, sampleRate, bitDepth, {}, 0);

        juce::AudioBuffer<float> buffer (numChannels, numSamples);
        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* data = buffer.getWritePointer (ch);
            for (int s = 0; s < numSamples; ++s)
                data[s] = std::sin (2.0f * juce::MathConstants<float>::pi * 440.0f
                                    * (static_cast<float> (s) / static_cast<float> (sampleRate)));
        }
        writer.appendBuffer (buffer, buffer.getNumSamples());

        return tempFile;
    }

    // Builds a minimal .dawproject zip on disk: project.xml plus audio entries placed
    // at user-specified zip paths. Used to simulate files exported by other DAWs
    // without depending on Tracktion's own exporter.
    static void writeMinimalDAWprojectZip (const juce::File& destFile,
                                            const juce::String& projectXml,
                                            const std::vector<std::pair<juce::String, juce::File>>& audioEntries)
    {
        juce::ZipFile::Builder builder;

        auto projectStream = std::make_unique<juce::MemoryInputStream> (projectXml.toRawUTF8(),
                                                                        projectXml.getNumBytesAsUTF8(), false);
        builder.addEntry (std::move (projectStream), 9, "project.xml", juce::Time::getCurrentTime());

        for (const auto& [archivePath, sourceFile] : audioEntries)
            builder.addFile (sourceFile, 9, archivePath);

        destFile.deleteFile();
        juce::FileOutputStream out (destFile);
        REQUIRE (out.openedOk());
        builder.writeToStream (out, nullptr);
    }

    //==============================================================================
    TEST_CASE ("DAWproject: import audio clip from zipped audio/ subfolder")
    {
        // Reproduces issue #845: a .dawproject created by another DAW (Bitwig) puts audio
        // entries in an "audio/" subfolder and references them via <File path="audio/x.wav" />.
        // Previously extractAudioFiles stripped the subfolder while resolveAudioFile kept it,
        // so the file was never found and the clip was silently dropped.
        using namespace dawproject;

        auto& engine = *Engine::getEngines()[0];
        auto sourceWav = writeTestWavFile (engine);
        REQUIRE (sourceWav != nullptr);

        const juce::String archivePath = "audio/loop.wav";
        const juce::String xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<Project version="1.0">
  <Application name="TestDAW" version="1.0"/>
  <Transport>
    <Tempo value="120.0"/>
  </Transport>
  <Structure>
    <Track name="Audio Track" id="track1" contentType="audio">
      <Channel id="channel1" role="regular"/>
    </Track>
  </Structure>
  <Arrangement>
    <Lanes>
      <Lanes track="track1">
        <Clips>
          <Clip name="ImportedAudioClip" time="0.0" duration="0.25" timeUnit="seconds">
            <Audio id="audio1" channels="1" sampleRate="44100" duration="0.25">
              <File path="audio/loop.wav"/>
            </Audio>
          </Clip>
        </Clips>
      </Lanes>
    </Lanes>
  </Arrangement>
</Project>
)";

        juce::TemporaryFile dawprojectFile (".dawproject");
        writeMinimalDAWprojectZip (dawprojectFile.getFile(), xml,
                                    { { archivePath, sourceWav->getFile() } });

        ParseOptions parseOpts;
        auto importedEdit = parseDAWproject (engine, dawprojectFile.getFile(), parseOpts);

        REQUIRE (importedEdit != nullptr);

        WaveAudioClip* importedClip = nullptr;
        for (auto* track : getAudioTracks (*importedEdit))
            for (auto* c : track->getClips())
                if (auto wc = dynamic_cast<WaveAudioClip*> (c))
                    if (wc->getName() == "ImportedAudioClip")
                        importedClip = wc;

        REQUIRE (importedClip != nullptr);
        CHECK (importedClip->getOriginalFile().existsAsFile());
        CHECK (importedClip->getOriginalFile().getSize() > 0);
    }

    //==============================================================================
    TEST_CASE ("DAWproject: import audio clip with external=\"true\" file reference")
    {
        // The DAWproject spec allows <File path="..." external="true"> meaning the path is
        // relative to the .dawproject file on disk, not inside the zip archive. The constant
        // for the attribute existed but was previously never read.
        using namespace dawproject;

        auto& engine = *Engine::getEngines()[0];

        // Place the audio file in the same temporary directory as the .dawproject file.
        auto tempDir = juce::File::createTempFile ("dawproject_external_test").getSiblingFile (
                            "dawproject_external_" + juce::String (juce::Time::getMillisecondCounter()));
        tempDir.createDirectory();

        struct ScopedDir
        {
            juce::File dir;
            ~ScopedDir() { dir.deleteRecursively(); }
        } scoped { tempDir };

        auto externalAudio = tempDir.getChildFile ("external_loop.wav");
        {
            juce::WavAudioFormat format;
            AudioFile audioFile (engine, externalAudio);
            AudioFileWriter writer (audioFile, &format, 1, 44100.0, 16, {}, 0);
            REQUIRE (writer.isOpen());
            juce::AudioBuffer<float> buffer (1, 4096);
            buffer.clear();
            writer.appendBuffer (buffer, buffer.getNumSamples());
        }
        REQUIRE (externalAudio.existsAsFile());

        const juce::String xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<Project version="1.0">
  <Application name="TestDAW" version="1.0"/>
  <Transport><Tempo value="120.0"/></Transport>
  <Structure>
    <Track name="Audio Track" id="track1" contentType="audio">
      <Channel id="channel1" role="regular"/>
    </Track>
  </Structure>
  <Arrangement>
    <Lanes>
      <Lanes track="track1">
        <Clips>
          <Clip name="ExternalAudioClip" time="0.0" duration="0.1" timeUnit="seconds">
            <Audio id="audio1" channels="1" sampleRate="44100" duration="0.1">
              <File path="external_loop.wav" external="true"/>
            </Audio>
          </Clip>
        </Clips>
      </Lanes>
    </Lanes>
  </Arrangement>
</Project>
)";

        auto dawprojectFile = tempDir.getChildFile ("project.dawproject");
        writeMinimalDAWprojectZip (dawprojectFile, xml, {});

        ParseOptions parseOpts;
        auto importedEdit = parseDAWproject (engine, dawprojectFile, parseOpts);

        REQUIRE (importedEdit != nullptr);

        WaveAudioClip* importedClip = nullptr;
        for (auto* track : getAudioTracks (*importedEdit))
            for (auto* c : track->getClips())
                if (auto wc = dynamic_cast<WaveAudioClip*> (c))
                    if (wc->getName() == "ExternalAudioClip")
                        importedClip = wc;

        REQUIRE (importedClip != nullptr);
        CHECK_EQ (importedClip->getOriginalFile().getFullPathName().toStdString(),
                  externalAudio.getFullPathName().toStdString());
    }

    //==============================================================================
    TEST_CASE ("DAWproject: import clip with deeply nested Lanes")
    {
        // Some DAWs nest <Audio> or <Notes> more than one <Lanes> level deep inside
        // a <Clip>. The importer must descend recursively rather than only check the
        // first level.
        using namespace dawproject;

        auto& engine = *Engine::getEngines()[0];
        auto sourceWav = writeTestWavFile (engine);
        REQUIRE (sourceWav != nullptr);

        const juce::String xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<Project version="1.0">
  <Application name="TestDAW" version="1.0"/>
  <Transport><Tempo value="120.0"/></Transport>
  <Structure>
    <Track name="Audio Track" id="track1" contentType="audio">
      <Channel id="channel1" role="regular"/>
    </Track>
  </Structure>
  <Arrangement>
    <Lanes>
      <Lanes track="track1">
        <Clips>
          <Clip name="DeeplyNestedClip" time="0.0" duration="0.25" timeUnit="seconds">
            <Lanes>
              <Lanes>
                <Audio id="audio1" channels="1" sampleRate="44100" duration="0.25">
                  <File path="audio/loop.wav"/>
                </Audio>
              </Lanes>
            </Lanes>
          </Clip>
        </Clips>
      </Lanes>
    </Lanes>
  </Arrangement>
</Project>
)";

        juce::TemporaryFile dawprojectFile (".dawproject");
        writeMinimalDAWprojectZip (dawprojectFile.getFile(), xml,
                                    { { "audio/loop.wav", sourceWav->getFile() } });

        ParseOptions parseOpts;
        auto importedEdit = parseDAWproject (engine, dawprojectFile.getFile(), parseOpts);

        REQUIRE (importedEdit != nullptr);

        WaveAudioClip* importedClip = nullptr;
        for (auto* track : getAudioTracks (*importedEdit))
            for (auto* c : track->getClips())
                if (auto wc = dynamic_cast<WaveAudioClip*> (c))
                    if (wc->getName() == "DeeplyNestedClip")
                        importedClip = wc;

        REQUIRE (importedClip != nullptr);
        CHECK (importedClip->getOriginalFile().existsAsFile());
    }

    //==============================================================================
    TEST_CASE ("DAWproject: extractAudioFiles rejects path traversal")
    {
        // Defence-in-depth: a malicious .dawproject must not be able to write outside
        // the destination directory using ".." segments in zip entry names.
        using namespace dawproject;

        auto& engine = *Engine::getEngines()[0];
        auto sourceWav = writeTestWavFile (engine);
        REQUIRE (sourceWav != nullptr);

        const juce::String xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<Project version="1.0">
  <Application name="TestDAW" version="1.0"/>
  <Transport><Tempo value="120.0"/></Transport>
  <Structure>
    <Track name="A" id="track1" contentType="audio">
      <Channel id="channel1" role="regular"/>
    </Track>
  </Structure>
  <Arrangement><Lanes/></Arrangement>
</Project>
)";

        juce::TemporaryFile dawprojectFile (".dawproject");
        writeMinimalDAWprojectZip (dawprojectFile.getFile(), xml,
                                    { { "../escape.wav", sourceWav->getFile() } });

        // Use an explicit destination directory so we can verify nothing escaped it.
        auto extractDir = juce::File::createTempFile ("dawproject_traversal");
        extractDir.deleteFile();
        extractDir.createDirectory();

        ParseOptions parseOpts;
        parseOpts.audioFileDestination = extractDir;

        auto importedEdit = parseDAWproject (engine, dawprojectFile.getFile(), parseOpts);
        REQUIRE (importedEdit != nullptr);

        // The malicious entry must NOT have been extracted next to the destination dir.
        auto escapedFile = extractDir.getParentDirectory().getChildFile ("escape.wav");
        CHECK_FALSE (escapedFile.existsAsFile());

        extractDir.deleteRecursively();
        escapedFile.deleteFile(); // belt-and-braces in case a previous run left a stray file
    }

    //==============================================================================
    TEST_CASE ("DAWproject: round-trip wave clip with embedded audio")
    {
        // Full round-trip: export an Edit containing a WaveAudioClip with embedAudioFiles
        // enabled, re-import, and verify the imported clip points at a real audio file.
        using namespace dawproject;

        auto& engine = *Engine::getEngines()[0];
        auto sourceWav = writeTestWavFile (engine, 1, 44100.0, 0.5);
        REQUIRE (sourceWav != nullptr);

        auto edit = test_utilities::createTestEdit (engine, 1);
        auto* track = getAudioTracks (*edit)[0];
        REQUIRE (track != nullptr);
        track->setName ("AudioRoundTrip");

        auto wavLength = AudioFile (engine, sourceWav->getFile()).getLength();
        REQUIRE (wavLength > 0.0);

        auto waveClip = track->insertWaveClip ("RoundTripClip",
                                                sourceWav->getFile(),
                                                { { 0_tp, TimePosition::fromSeconds (wavLength) }, {} },
                                                false);
        REQUIRE (waveClip != nullptr);

        // Export with embedded audio
        juce::TemporaryFile dawprojectFile (".dawproject");
        WriteOptions writeOpts;
        writeOpts.embedAudioFiles = true;
        auto result = writeDAWprojectFile (dawprojectFile.getFile(), *edit, writeOpts);
        REQUIRE (result.wasOk());

        // Re-import into a fresh extraction directory
        auto extractDir = juce::File::createTempFile ("dawproject_roundtrip_extract");
        extractDir.deleteFile();
        extractDir.createDirectory();
        struct ScopedDir { juce::File dir; ~ScopedDir() { dir.deleteRecursively(); } } scoped { extractDir };

        ParseOptions parseOpts;
        parseOpts.audioFileDestination = extractDir;
        auto importedEdit = parseDAWproject (engine, dawprojectFile.getFile(), parseOpts);
        REQUIRE (importedEdit != nullptr);

        // The imported track should have a wave clip that resolves to a real file.
        WaveAudioClip* importedClip = nullptr;
        for (auto* importedTrack : getAudioTracks (*importedEdit))
            for (auto* c : importedTrack->getClips())
                if (auto wc = dynamic_cast<WaveAudioClip*> (c))
                    if (wc->getName() == "RoundTripClip")
                        importedClip = wc;

        REQUIRE (importedClip != nullptr);
        CHECK (importedClip->getOriginalFile().existsAsFile());
        CHECK_GT (importedClip->getOriginalFile().getSize(), 0);

        // The audio file should sit inside the audio/ subfolder of our extract dir,
        // mirroring the archive layout — this is what bug #845 was about.
        CHECK (importedClip->getOriginalFile().getParentDirectory().getFileName() == "audio");
    }

    //==============================================================================
    TEST_CASE ("DAWproject: round-trip multiple audio clips with deduplicated filenames")
    {
        // The exporter renames duplicate filenames as foo_1.wav, foo_2.wav … each
        // becomes a distinct entry inside audio/. Round-trip must preserve all clips.
        using namespace dawproject;

        auto& engine = *Engine::getEngines()[0];
        auto wavA = writeTestWavFile (engine, 1, 44100.0, 0.1);
        auto wavB = writeTestWavFile (engine, 1, 44100.0, 0.1);
        REQUIRE (wavA != nullptr);
        REQUIRE (wavB != nullptr);

        auto edit = test_utilities::createTestEdit (engine, 1);
        auto* track = getAudioTracks (*edit)[0];
        REQUIRE (track != nullptr);
        track->setName ("MultiClipTrack");

        auto wavLength = AudioFile (engine, wavA->getFile()).getLength();

        auto clip1 = track->insertWaveClip ("ClipA",
                                             wavA->getFile(),
                                             { { 0_tp, TimePosition::fromSeconds (wavLength) }, {} },
                                             false);
        auto clip2 = track->insertWaveClip ("ClipB",
                                             wavB->getFile(),
                                             { { TimePosition::fromSeconds (wavLength),
                                                 TimePosition::fromSeconds (wavLength * 2.0) }, {} },
                                             false);
        REQUIRE (clip1 != nullptr);
        REQUIRE (clip2 != nullptr);

        juce::TemporaryFile dawprojectFile (".dawproject");
        WriteOptions writeOpts;
        writeOpts.embedAudioFiles = true;
        auto result = writeDAWprojectFile (dawprojectFile.getFile(), *edit, writeOpts);
        REQUIRE (result.wasOk());

        auto extractDir = juce::File::createTempFile ("dawproject_multi_extract");
        extractDir.deleteFile();
        extractDir.createDirectory();
        struct ScopedDir { juce::File dir; ~ScopedDir() { dir.deleteRecursively(); } } scoped { extractDir };

        ParseOptions parseOpts;
        parseOpts.audioFileDestination = extractDir;
        auto importedEdit = parseDAWproject (engine, dawprojectFile.getFile(), parseOpts);
        REQUIRE (importedEdit != nullptr);

        int foundClips = 0;
        for (auto* importedTrack : getAudioTracks (*importedEdit))
            for (auto* c : importedTrack->getClips())
                if (auto wc = dynamic_cast<WaveAudioClip*> (c))
                {
                    if (wc->getName() == "ClipA" || wc->getName() == "ClipB")
                    {
                        CHECK (wc->getOriginalFile().existsAsFile());
                        ++foundClips;
                    }
                }

        CHECK_EQ (foundClips, 2);
    }

    //==============================================================================
    TEST_CASE ("DAWproject: import clip launcher MIDI clip from <Scenes>")
    {
        // Issue #878: clip-launcher MIDI clips (in <Scenes>, not <Arrangement>) were
        // never imported. Build a hand-crafted dawproject that mirrors what Bitwig /
        // Studio One emit and verify the launcher slot ends up populated.
        using namespace dawproject;

        auto& engine = *Engine::getEngines()[0];

        const juce::String xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<Project version="1.0">
  <Application name="TestDAW" version="1.0"/>
  <Transport><Tempo value="120.0"/></Transport>
  <Structure>
    <Track name="Launcher Track" id="track1" contentType="notes">
      <Channel id="channel1" role="regular"/>
    </Track>
  </Structure>
  <Arrangement><Lanes/></Arrangement>
  <Scenes>
    <Scene name="Verse" id="scene1">
      <Content>
        <Lanes timeUnit="beats">
          <Lanes track="track1" timeUnit="beats">
            <Clips>
              <Clip name="LauncherMidi" time="0.0" duration="4.0" timeUnit="beats">
                <Notes>
                  <Note time="0.0" duration="1.0" channel="0" key="60" vel="0.7874"/>
                  <Note time="1.0" duration="0.5" channel="0" key="64" vel="0.6299"/>
                  <Note time="2.0" duration="2.0" channel="0" key="67" vel="1.0"/>
                </Notes>
              </Clip>
            </Clips>
          </Lanes>
        </Lanes>
      </Content>
    </Scene>
  </Scenes>
</Project>
)";

        juce::TemporaryFile dawprojectFile (".dawproject");
        writeMinimalDAWprojectZip (dawprojectFile.getFile(), xml, {});

        ParseOptions parseOpts;
        auto importedEdit = parseDAWproject (engine, dawprojectFile.getFile(), parseOpts);

        REQUIRE (importedEdit != nullptr);

        auto& sceneList = importedEdit->getSceneList();
        CHECK_EQ (sceneList.getNumScenes(), 1);

        REQUIRE_GE (sceneList.getScenes().size(), 1);
        CHECK_EQ (sceneList.getScenes()[0]->name.get().toStdString(), "Verse");

        AudioTrack* importedTrack = nullptr;
        for (auto* t : getAudioTracks (*importedEdit))
            if (t->getName() == "Launcher Track")
                importedTrack = t;
        REQUIRE (importedTrack != nullptr);

        auto slots = importedTrack->getClipSlotList().getClipSlots();
        REQUIRE_GE (slots.size(), 1);

        auto* slotClip = slots[0]->getClip();
        REQUIRE (slotClip != nullptr);

        auto* importedMidi = dynamic_cast<MidiClip*> (slotClip);
        REQUIRE (importedMidi != nullptr);
        CHECK_EQ (importedMidi->getName().toStdString(), "LauncherMidi");
        CHECK_EQ (importedMidi->getSequence().getNotes().size(), 3);
    }

    //==============================================================================
    TEST_CASE ("DAWproject: import multiple scenes across multiple tracks")
    {
        // Three scenes × two tracks: not every cell is populated. Verifies that
        // <Lanes track=...> matches by track id and that empty cells stay empty.
        using namespace dawproject;

        auto& engine = *Engine::getEngines()[0];

        const juce::String xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<Project version="1.0">
  <Application name="TestDAW" version="1.0"/>
  <Transport><Tempo value="120.0"/></Transport>
  <Structure>
    <Track name="T1" id="track1" contentType="notes"><Channel id="ch1" role="regular"/></Track>
    <Track name="T2" id="track2" contentType="notes"><Channel id="ch2" role="regular"/></Track>
  </Structure>
  <Arrangement><Lanes/></Arrangement>
  <Scenes>
    <Scene name="S1" id="s1">
      <Content>
        <Lanes timeUnit="beats">
          <Lanes track="track1" timeUnit="beats">
            <Clips>
              <Clip name="T1S1" time="0.0" duration="2.0" timeUnit="beats">
                <Notes><Note time="0.0" duration="1.0" channel="0" key="60" vel="0.5"/></Notes>
              </Clip>
            </Clips>
          </Lanes>
        </Lanes>
      </Content>
    </Scene>
    <Scene name="S2" id="s2">
      <Content>
        <Lanes timeUnit="beats">
          <Lanes track="track2" timeUnit="beats">
            <Clips>
              <Clip name="T2S2" time="0.0" duration="2.0" timeUnit="beats">
                <Notes><Note time="0.0" duration="1.0" channel="0" key="64" vel="0.5"/></Notes>
              </Clip>
            </Clips>
          </Lanes>
        </Lanes>
      </Content>
    </Scene>
    <Scene name="S3" id="s3">
      <Content>
        <Lanes timeUnit="beats">
          <Lanes track="track1" timeUnit="beats">
            <Clips>
              <Clip name="T1S3" time="0.0" duration="2.0" timeUnit="beats">
                <Notes><Note time="0.0" duration="1.0" channel="0" key="67" vel="0.5"/></Notes>
              </Clip>
            </Clips>
          </Lanes>
          <Lanes track="track2" timeUnit="beats">
            <Clips>
              <Clip name="T2S3" time="0.0" duration="2.0" timeUnit="beats">
                <Notes><Note time="0.0" duration="1.0" channel="0" key="72" vel="0.5"/></Notes>
              </Clip>
            </Clips>
          </Lanes>
        </Lanes>
      </Content>
    </Scene>
  </Scenes>
</Project>
)";

        juce::TemporaryFile dawprojectFile (".dawproject");
        writeMinimalDAWprojectZip (dawprojectFile.getFile(), xml, {});

        ParseOptions parseOpts;
        auto importedEdit = parseDAWproject (engine, dawprojectFile.getFile(), parseOpts);
        REQUIRE (importedEdit != nullptr);

        CHECK_EQ (importedEdit->getSceneList().getNumScenes(), 3);

        AudioTrack *t1 = nullptr, *t2 = nullptr;
        for (auto* t : getAudioTracks (*importedEdit))
        {
            if (t->getName() == "T1") t1 = t;
            if (t->getName() == "T2") t2 = t;
        }
        REQUIRE (t1 != nullptr);
        REQUIRE (t2 != nullptr);

        auto t1Slots = t1->getClipSlotList().getClipSlots();
        auto t2Slots = t2->getClipSlotList().getClipSlots();
        REQUIRE_GE (t1Slots.size(), 3);
        REQUIRE_GE (t2Slots.size(), 3);

        // Expected occupancy:
        //  scene 0: T1 yes, T2 no
        //  scene 1: T1 no,  T2 yes
        //  scene 2: T1 yes, T2 yes
        CHECK (t1Slots[0]->getClip() != nullptr);
        CHECK (t2Slots[0]->getClip() == nullptr);

        CHECK (t1Slots[1]->getClip() == nullptr);
        CHECK (t2Slots[1]->getClip() != nullptr);

        CHECK (t1Slots[2]->getClip() != nullptr);
        CHECK (t2Slots[2]->getClip() != nullptr);

        if (auto* c = t1Slots[0]->getClip()) CHECK_EQ (c->getName().toStdString(), "T1S1");
        if (auto* c = t2Slots[1]->getClip()) CHECK_EQ (c->getName().toStdString(), "T2S2");
        if (auto* c = t1Slots[2]->getClip()) CHECK_EQ (c->getName().toStdString(), "T1S3");
        if (auto* c = t2Slots[2]->getClip()) CHECK_EQ (c->getName().toStdString(), "T2S3");
    }

    //==============================================================================
    TEST_CASE ("DAWproject: import clip launcher audio clip")
    {
        // Verifies that audio clips inside <Scenes> resolve via the same
        // ClipOwner-based parseClip path that arrangement audio clips use.
        using namespace dawproject;

        auto& engine = *Engine::getEngines()[0];
        auto sourceWav = writeTestWavFile (engine);
        REQUIRE (sourceWav != nullptr);

        const juce::String xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<Project version="1.0">
  <Application name="TestDAW" version="1.0"/>
  <Transport><Tempo value="120.0"/></Transport>
  <Structure>
    <Track name="A" id="track1" contentType="audio"><Channel id="c1" role="regular"/></Track>
  </Structure>
  <Arrangement><Lanes/></Arrangement>
  <Scenes>
    <Scene name="One" id="s1">
      <Content>
        <Lanes>
          <Lanes track="track1">
            <Clips>
              <Clip name="LauncherWav" time="0.0" duration="0.25" timeUnit="seconds">
                <Audio id="a1" channels="1" sampleRate="44100" duration="0.25">
                  <File path="audio/loop.wav"/>
                </Audio>
              </Clip>
            </Clips>
          </Lanes>
        </Lanes>
      </Content>
    </Scene>
  </Scenes>
</Project>
)";

        juce::TemporaryFile dawprojectFile (".dawproject");
        writeMinimalDAWprojectZip (dawprojectFile.getFile(), xml,
                                    { { "audio/loop.wav", sourceWav->getFile() } });

        ParseOptions parseOpts;
        auto importedEdit = parseDAWproject (engine, dawprojectFile.getFile(), parseOpts);
        REQUIRE (importedEdit != nullptr);

        CHECK_EQ (importedEdit->getSceneList().getNumScenes(), 1);

        AudioTrack* importedTrack = getAudioTracks (*importedEdit)[0];
        REQUIRE (importedTrack != nullptr);

        auto slots = importedTrack->getClipSlotList().getClipSlots();
        REQUIRE_GE (slots.size(), 1);

        auto* wave = dynamic_cast<WaveAudioClip*> (slots[0]->getClip());
        REQUIRE (wave != nullptr);
        CHECK_EQ (wave->getName().toStdString(), "LauncherWav");
        CHECK (wave->getOriginalFile().existsAsFile());
    }

    //==============================================================================
    TEST_CASE ("DAWproject: round-trip clip launcher MIDI clip")
    {
        // Full round-trip: build a Tracktion edit with a launcher MIDI clip, export,
        // re-import, and verify the slot is still populated with the same notes.
        using namespace dawproject;

        auto& engine = *Engine::getEngines()[0];
        auto edit = test_utilities::createTestEdit (engine, 1);

        auto* track = getAudioTracks (*edit)[0];
        REQUIRE (track != nullptr);
        track->setName ("LaunchTrack");

        // Create one scene; this expands the track's slot list to 1.
        auto& sceneList = edit->getSceneList();
        sceneList.ensureNumberOfScenes (1);
        REQUIRE_GE (sceneList.getScenes().size(), 1);
        sceneList.getScenes()[0]->name = "Chorus";

        auto slots = track->getClipSlotList().getClipSlots();
        REQUIRE_GE (slots.size(), 1);

        // Insert a MIDI clip into the slot.
        auto launcherClip = insertMIDIClip (*slots[0], "RTLauncher", { 0_tp, 4_tp });
        REQUIRE (launcherClip != nullptr);
        launcherClip->getSequence().addNote (60, BeatPosition::fromBeats (0.0),
                                              BeatDuration::fromBeats (1.0), 100, 0, nullptr);
        launcherClip->getSequence().addNote (64, BeatPosition::fromBeats (1.0),
                                              BeatDuration::fromBeats (0.5), 80, 0, nullptr);

        // Export
        juce::TemporaryFile dawprojectFile (".dawproject");
        WriteOptions writeOpts;
        writeOpts.embedAudioFiles = false;
        auto result = writeDAWprojectFile (dawprojectFile.getFile(), *edit, writeOpts);
        REQUIRE (result.wasOk());

        // Re-import
        ParseOptions parseOpts;
        parseOpts.extractAudioFiles = false;
        auto importedEdit = parseDAWproject (engine, dawprojectFile.getFile(), parseOpts);
        REQUIRE (importedEdit != nullptr);

        CHECK_EQ (importedEdit->getSceneList().getNumScenes(), 1);
        REQUIRE_GE (importedEdit->getSceneList().getScenes().size(), 1);
        CHECK_EQ (importedEdit->getSceneList().getScenes()[0]->name.get().toStdString(), "Chorus");

        AudioTrack* importedTrack = nullptr;
        for (auto* t : getAudioTracks (*importedEdit))
            if (t->getName() == "LaunchTrack")
                importedTrack = t;
        REQUIRE (importedTrack != nullptr);

        auto importedSlots = importedTrack->getClipSlotList().getClipSlots();
        REQUIRE_GE (importedSlots.size(), 1);

        auto* importedMidi = dynamic_cast<MidiClip*> (importedSlots[0]->getClip());
        REQUIRE (importedMidi != nullptr);
        CHECK_EQ (importedMidi->getName().toStdString(), "RTLauncher");
        CHECK_EQ (importedMidi->getSequence().getNotes().size(), 2);
    }

    //==============================================================================
    TEST_CASE ("DAWproject: round-trip arrangement and scene clips coexist")
    {
        // Mixed: same edit has a clip on the arrangement timeline and a different
        // clip in the launcher. Both must round-trip independently.
        using namespace dawproject;

        auto& engine = *Engine::getEngines()[0];
        auto edit = test_utilities::createTestEdit (engine, 1);

        auto* track = getAudioTracks (*edit)[0];
        REQUIRE (track != nullptr);
        track->setName ("Mixed");

        // Arrangement clip
        auto arrClip = track->insertMIDIClip ({ 0_tp, 2_tp }, nullptr);
        REQUIRE (arrClip != nullptr);
        arrClip->setName ("ArrClip");
        arrClip->getSequence().addNote (60, BeatPosition::fromBeats (0.0),
                                         BeatDuration::fromBeats (1.0), 100, 0, nullptr);

        // Launcher clip
        auto& sceneList = edit->getSceneList();
        sceneList.ensureNumberOfScenes (1);
        sceneList.getScenes()[0]->name = "Solo";
        auto slots = track->getClipSlotList().getClipSlots();
        REQUIRE_GE (slots.size(), 1);
        auto launcherClip = insertMIDIClip (*slots[0], "LaunchClip", { 0_tp, 4_tp });
        REQUIRE (launcherClip != nullptr);
        launcherClip->getSequence().addNote (72, BeatPosition::fromBeats (0.0),
                                              BeatDuration::fromBeats (1.0), 100, 0, nullptr);

        // Round-trip
        juce::TemporaryFile dawprojectFile (".dawproject");
        WriteOptions writeOpts;
        writeOpts.embedAudioFiles = false;
        REQUIRE (writeDAWprojectFile (dawprojectFile.getFile(), *edit, writeOpts).wasOk());

        ParseOptions parseOpts;
        parseOpts.extractAudioFiles = false;
        auto importedEdit = parseDAWproject (engine, dawprojectFile.getFile(), parseOpts);
        REQUIRE (importedEdit != nullptr);

        AudioTrack* importedTrack = nullptr;
        for (auto* t : getAudioTracks (*importedEdit))
            if (t->getName() == "Mixed")
                importedTrack = t;
        REQUIRE (importedTrack != nullptr);

        // Arrangement clip
        bool foundArr = false;
        for (auto* c : importedTrack->getClips())
            if (c->getName() == "ArrClip")
                foundArr = true;
        CHECK (foundArr);

        // Launcher clip
        auto importedSlots = importedTrack->getClipSlotList().getClipSlots();
        REQUIRE_GE (importedSlots.size(), 1);
        auto* slotClip = importedSlots[0]->getClip();
        REQUIRE (slotClip != nullptr);
        CHECK_EQ (slotClip->getName().toStdString(), "LaunchClip");
    }

    //==============================================================================
    TEST_CASE ("DAWproject: stress test with very long MIDI sequence")
    {
        using namespace dawproject;

        const uint32_t seed = 67890;
        juce::Random random (seed);

        auto& engine = *Engine::getEngines()[0];
        auto edit = test_utilities::createTestEdit (engine, 1);

        auto* track = getAudioTracks (*edit)[0];
        REQUIRE (track != nullptr);
        track->setName ("StressTestTrack");

        // Create one very long clip with thousands of notes and controller data
        const int numNotes = 5000;
        const int numControllerEvents = 2000;
        const double clipLengthBeats = 1000.0;

        auto midiClip = track->insertMIDIClip ({ 0_tp, TimePosition::fromSeconds (clipLengthBeats) }, nullptr);
        REQUIRE (midiClip != nullptr);
        midiClip->setName ("MegaClip");

        auto& sequence = midiClip->getSequence();

        // Generate a realistic piano-roll style sequence
        double currentBeat = 0.0;
        for (int n = 0; n < numNotes; ++n)
        {
            // Random chord or single note
            int numSimultaneous = 1 + random.nextInt (4); // 1-4 notes at once

            for (int s = 0; s < numSimultaneous && (n + s) < numNotes; ++s)
            {
                int pitch = 48 + random.nextInt (36); // C3 to C6
                double length = 0.25 + random.nextDouble() * 1.5;
                int velocity = 40 + random.nextInt (88);

                sequence.addNote (pitch,
                                  BeatPosition::fromBeats (currentBeat),
                                  BeatDuration::fromBeats (length),
                                  velocity, 0, nullptr);
            }

            n += numSimultaneous - 1;
            currentBeat += 0.25 + random.nextDouble() * 0.75; // Advance by 1/4 to 1 beat
        }

        // Generate continuous controller data (mod wheel automation)
        for (int cc = 0; cc < numControllerEvents; ++cc)
        {
            double beatPos = (cc / static_cast<double> (numControllerEvents)) * clipLengthBeats;

            // Sine wave modulation
            double phase = beatPos * 0.1;
            int modValue = static_cast<int> ((std::sin (phase) * 0.5 + 0.5) * 127.0) << 7;

            sequence.addControllerEvent (BeatPosition::fromBeats (beatPos), 1, modValue, nullptr);
        }

        // Add pitch bend ramps
        for (int pb = 0; pb < numControllerEvents / 4; ++pb)
        {
            double beatPos = random.nextDouble() * clipLengthBeats;
            int bendValue = random.nextInt (16384);
            sequence.addControllerEvent (BeatPosition::fromBeats (beatPos),
                                         MidiControllerEvent::pitchWheelType, bendValue, nullptr);
        }

        int notesCreated = sequence.getNotes().size();
        int controllerEventsCreated = sequence.getControllerEvents().size();
        INFO ("Created " << notesCreated << " notes and " << controllerEventsCreated << " controller events");

        // Export
        juce::TemporaryFile tempFile (".dawproject");
        WriteOptions writeOpts;
        writeOpts.embedAudioFiles = false;

        auto result = writeDAWprojectFile (tempFile.getFile(), *edit, writeOpts);
        REQUIRE (result.wasOk());

        INFO ("File size: " << tempFile.getFile().getSize() << " bytes");

        // Import
        ParseOptions parseOpts;
        parseOpts.extractAudioFiles = false;
        auto importedEdit = parseDAWproject (engine, tempFile.getFile(), parseOpts);

        REQUIRE (importedEdit != nullptr);

        // Find our track and clip
        int notesImported = 0;
        int controllerEventsImported = 0;
        for (auto* importedTrack : getAudioTracks (*importedEdit))
        {
            if (importedTrack->getName() == "StressTestTrack")
            {
                for (auto* clip : importedTrack->getClips())
                {
                    if (auto* midiClip2 = dynamic_cast<MidiClip*> (clip))
                    {
                        notesImported += midiClip2->getSequence().getNotes().size();
                        controllerEventsImported += midiClip2->getSequence().getControllerEvents().size();
                    }
                }
            }
        }

        INFO ("Notes imported: " << notesImported);
        INFO ("Controller events imported: " << controllerEventsImported);
        CHECK_EQ (notesImported, notesCreated);
        CHECK_EQ (controllerEventsImported, controllerEventsCreated);
    }
}

} // namespace tracktion::inline engine

#endif // TRACKTION_UNIT_TESTS && ENGINE_UNIT_TESTS_DAWPROJECT
