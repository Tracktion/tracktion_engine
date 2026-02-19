#include <iostream>
#include <string>
#include <vector>
#include <iomanip>
#include "../choc/memory/choc_xxHash.h"

void demonstrateBasicHashing()
{
    std::cout << "=== Basic xxHash Demonstration ===\n\n";

    std::vector<std::string> testStrings = {
        "Hello, CHOC!",
        "Hello, world!",
        "The quick brown fox jumps over the lazy dog",
        "",  // Empty string
        "A",  // Single character
        "xxHash is a fast hashing algorithm"
    };

    uint32_t seed = 0;

    std::cout << "32-bit Hashes:\n";
    std::cout << std::setfill ('-') << std::setw (70) << "" << std::setfill (' ') << "\n";

    for (const auto& str : testStrings)
    {
        choc::hash::xxHash32 hasher32 (seed);
        hasher32.addInput (str.data(), str.length());
        uint32_t hash32 = hasher32.getHash();

        std::cout << std::left << std::setw (40) << ("\"" + str + "\"")
                  << " -> 0x" << std::hex << std::setw (8) << std::setfill ('0')
                  << hash32 << std::dec << std::setfill (' ') << "\n";
    }

    std::cout << "\n64-bit Hashes:\n";
    std::cout << std::setfill ('-') << std::setw (70) << "" << std::setfill (' ') << "\n";

    for (const auto& str : testStrings)
    {
        choc::hash::xxHash64 hasher64 (seed);
        hasher64.addInput (str.data(), str.length());
        uint64_t hash64 = hasher64.getHash();

        std::cout << std::left << std::setw (40) << ("\"" + str + "\"")
                  << " -> 0x" << std::hex << std::setw (16) << std::setfill ('0')
                  << hash64 << std::dec << std::setfill (' ') << "\n";
    }
}

void demonstrateHashConsistency()
{
    std::cout << "\n=== Hash Consistency Demo ===\n\n";

    std::string testData = "Consistency test string";
    uint32_t seed = 12345;

    std::cout << "Testing that identical inputs produce identical hashes:\n";

    // Hash the same data multiple times
    for (int i = 0; i < 3; ++i)
    {
        choc::hash::xxHash32 hasher32 (seed);
        hasher32.addInput (testData.data(), testData.length());
        uint32_t hash32 = hasher32.getHash();

        choc::hash::xxHash64 hasher64 (seed);
        hasher64.addInput (testData.data(), testData.length());
        uint64_t hash64 = hasher64.getHash();

        std::cout << "Run " << (i + 1) << ": 32-bit=0x" << std::hex << hash32
                  << ", 64-bit=0x" << hash64 << std::dec << "\n";
    }
}

void demonstrateSeedVariation()
{
    std::cout << "\n=== Seed Variation Demo ===\n\n";

    std::string testData = "Same data, different seeds";
    std::vector<uint32_t> seeds = {0, 1, 42, 12345, 0xDEADBEEF};

    std::cout << "How different seeds affect the same input:\n";
    std::cout << "Input: \"" << testData << "\"\n\n";

    for (auto seed : seeds)
    {
        choc::hash::xxHash32 hasher32 (seed);
        hasher32.addInput (testData.data(), testData.length());
        uint32_t hash32 = hasher32.getHash();

        choc::hash::xxHash64 hasher64 (seed);
        hasher64.addInput (testData.data(), testData.length());
        uint64_t hash64 = hasher64.getHash();

        std::cout << "Seed 0x" << std::hex << std::setw (8) << std::setfill ('0') << seed
                  << " -> 32-bit: 0x" << std::setw (8) << hash32
                  << ", 64-bit: 0x" << std::setw (16) << hash64 << std::dec << "\n";
    }
}

void demonstrateStreamingHash()
{
    std::cout << "\n=== Streaming Hash Demo ===\n\n";

    std::string fullData = "This is a test of streaming hash functionality!";
    uint32_t seed = 0;

    // Hash all at once
    choc::hash::xxHash32 hasher_all (seed);
    hasher_all.addInput (fullData.data(), fullData.length());
    uint32_t hash_all = hasher_all.getHash();

    // Hash in chunks
    choc::hash::xxHash32 hasher_chunks (seed);
    size_t chunkSize = 8;

    for (size_t i = 0; i < fullData.length(); i += chunkSize)
    {
        size_t currentChunkSize = std::min (chunkSize, fullData.length() - i);
        hasher_chunks.addInput (fullData.data() + i, currentChunkSize);
    }

    uint32_t hash_chunks = hasher_chunks.getHash();

    std::cout << "Full string: \"" << fullData << "\"\n";
    std::cout << "Hash (all at once):  0x" << std::hex << hash_all << "\n";
    std::cout << "Hash (8-byte chunks): 0x" << std::hex << hash_chunks << std::dec << "\n";
    std::cout << "Hashes match: " << (hash_all == hash_chunks ? "YES" : "NO") << "\n";
}

void demonstrateCollisionResistance()
{
    std::cout << "\n=== Simple Collision Test ===\n\n";

    std::vector<std::string> similarStrings = {
        "test",
        "Test",
        "test1",
        "test2",
        "tset",  // anagram
        "testing"
    };

    uint32_t seed = 0;

    std::cout << "Testing similar strings for hash collisions:\n";
    for (const auto& str : similarStrings)
    {
        choc::hash::xxHash32 hasher32 (seed);
        hasher32.addInput (str.data(), str.length());
        uint32_t hash32 = hasher32.getHash();

        std::cout << std::left << std::setw (15) << ("\"" + str + "\"")
                  << " -> 0x" << std::hex << std::setw (8) << std::setfill ('0')
                  << hash32 << std::dec << std::setfill (' ') << "\n";
    }

    std::cout << "\nNote: Each string produces a different hash (no collisions)\n";
}

int main()
{
    std::cout << "CHOC xxHash Example\n";
    std::cout << "===================\n\n";

    std::cout << "xxHash is a fast, high-quality hashing algorithm.\n";
    std::cout << "This example demonstrates various features and use cases.\n\n";

    demonstrateBasicHashing();
    demonstrateHashConsistency();
    demonstrateSeedVariation();
    demonstrateStreamingHash();
    demonstrateCollisionResistance();

    std::cout << "\n=== Example completed successfully! ===\n";

    return 0;
}
