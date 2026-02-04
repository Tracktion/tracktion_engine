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
