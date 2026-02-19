#include <iostream>
#include <vector>
#include <fstream>
#include "../choc/audio/choc_MIDI.h"
#include "../choc/audio/choc_MIDIFile.h"
#include "../choc/audio/choc_MIDISequence.h"

void demonstrateMIDINoteUtilities()
{
    std::cout << "\n=== MIDI Note Utilities Demo ===\n";

    // Demonstrate NoteNumber class
    for (int note = 60; note <= 72; ++note)
    {
        choc::midi::NoteNumber noteNum { static_cast<uint8_t>(note) };
        std::cout << "Note " << static_cast<int>(noteNum)
                  << ": " << noteNum.getNameWithOctaveNumber()
                  << " (" << noteNum.getFrequency() << " Hz)"
                  << (noteNum.isNatural() ? " [Natural]" : " [Accidental]") << "\n";
    }

    // Demonstrate frequency conversion
    std::cout << "\nFrequency conversions:\n";
    std::cout << "A440 (note 69): " << choc::midi::noteNumberToFrequency (69) << " Hz\n";
    std::cout << "Middle C (note 60): " << choc::midi::noteNumberToFrequency (60) << " Hz\n";
    std::cout << "440 Hz -> note number: " << choc::midi::frequencyToNoteNumber (440.0f) << "\n";
}

void demonstrateMIDIMessageConstruction()
{
    std::cout << "\n=== MIDI Message Construction Demo ===\n";

    // Create some MIDI messages
    auto noteOn        = choc::midi::ShortMessage (0x90, 60, 100);  // Note on, middle C, velocity 100
    auto noteOff       = choc::midi::ShortMessage (0x80, 60, 0);   // Note off, middle C
    auto controlChange = choc::midi::ShortMessage (0xB0, 7, 127);  // Volume control, max value

    std::cout << "Note On message: " << choc::midi::printHexMIDIData (noteOn.data(), noteOn.size()) << "\n";
    std::cout << "Note Off message: " << choc::midi::printHexMIDIData (noteOff.data(), noteOff.size()) << "\n";
    std::cout << "Control Change message: " << choc::midi::printHexMIDIData (controlChange.data(), controlChange.size()) << "\n";

    // Use the built-in getDescription() method for proper message analysis
    std::cout << "Note On description: " << noteOn.getDescription() << "\n";
    std::cout << "Note Off description: " << noteOff.getDescription() << "\n";
    std::cout << "Control Change description: " << controlChange.getDescription() << "\n";
}

void createSimpleMIDISequence()
{
    std::cout << "\n=== MIDI Sequence Creation Demo ===\n";

    choc::midi::Sequence sequence;

    // Create a simple melody: C-D-E-F-G (major scale)
    std::vector<uint8_t> notes = {60, 62, 64, 65, 67}; // C4, D4, E4, F4, G4
    double currentTime = 0.0;

    for (auto note : notes)
    {
        // Note on
        choc::midi::LongMessage noteOn (0x90, note, 80);
        sequence.events.push_back ({currentTime, noteOn});

        // Note off after 0.5 seconds
        choc::midi::LongMessage noteOff (0x80, note, 0);
        sequence.events.push_back ({currentTime + 0.5, noteOff});

        currentTime += 0.5; // Move to next half second
    }

    std::cout << "Created sequence with " << sequence.events.size() << " events\n";

    if (! sequence.events.empty())
    {
        auto lastEvent = sequence.events.back();
        std::cout << "Duration: " << lastEvent.timeStamp << " seconds\n";
    }

    // Print all events
    std::cout << "\nSequence events:\n";

    for (const auto& event : sequence.events)
    {
        const auto& msg = event.message;
        std::cout << "Time " << event.timeStamp << "s: " << msg.getDescription() << "\n";
    }
}

void demonstrateMIDIFileIO()
{
    std::cout << "\n=== MIDI File I/O Demo ===\n";

    // Create a simple MIDI file in memory
    std::vector<uint8_t> midiData = {
        // MIDI file header
        'M', 'T', 'h', 'd',          // Chunk type
        0x00, 0x00, 0x00, 0x06,      // Chunk length
        0x00, 0x00,                  // Format type 0
        0x00, 0x01,                  // Number of tracks
        0x00, 0x60,                  // Ticks per quarter note (96)

        // Track header
        'M', 'T', 'r', 'k',          // Chunk type
        0x00, 0x00, 0x00, 0x0B,      // Chunk length

        // Track events
        0x00, 0x90, 0x3C, 0x40,      // Delta time 0, Note on C4, velocity 64
        0x60, 0x80, 0x3C, 0x40,      // Delta time 96, Note off C4, velocity 64
        0x00, 0xFF, 0x2F, 0x00       // End of track
    };

    try
    {
        choc::midi::File midiFile;
        midiFile.load (midiData.data(), midiData.size());

        std::cout << "Successfully loaded MIDI file\n";
        std::cout << "Number of tracks: " << midiFile.tracks.size() << "\n";
        std::cout << "Time format: " << midiFile.timeFormat << " ticks per quarter note\n";

        if (! midiFile.tracks.empty())
        {
            std::cout << "Track 1 has " << midiFile.tracks[0].events.size() << " events:\n";

            for (const auto& event : midiFile.tracks[0].events)
                std::cout << "  Tick " << event.tickPosition << ": " << event.message.getDescription() << "\n";
        }

        // Convert to sequence
        auto sequence = midiFile.toSequence();
        std::cout << "\nConverted to sequence with " << sequence.events.size() << " events\n";

        // Iterate through events with timing
        std::cout << "\nEvents with timing (assuming 120 BPM):\n";

        midiFile.iterateEvents ([](const choc::midi::LongMessage& msg, double timeInSeconds)
        {
            std::cout << "  Time " << timeInSeconds << "s: " << msg.getDescription() << "\n";
        });
    }
    catch (const std::runtime_error& e)
    {
        std::cout << "Error loading MIDI file: " << e.what() << "\n";
    }
}

int main()
{
    std::cout << "CHOC MIDI File Processing Example\n";
    std::cout << "=================================\n";

    try
    {
        demonstrateMIDINoteUtilities();
        demonstrateMIDIMessageConstruction();
        createSimpleMIDISequence();
        demonstrateMIDIFileIO();

        std::cout << "\n=== All demonstrations completed successfully! ===\n";
    }
    catch (const std::exception& e)
    {
        std::cout << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}