/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#if TRACKTION_UNIT_TESTS && ENGINE_UNIT_TESTS_CHANNELCONFIGURATION

#include <tracktion_engine/../3rd_party/doctest/tracktion_doctest.hpp>

namespace tracktion { inline namespace engine
{

TEST_SUITE ("tracktion_engine")
{
    TEST_CASE ("ChannelConfiguration")
    {
        SUBCASE ("Factory methods")
        {
            {
                auto mono = ChannelConfiguration::mono (5);
                CHECK_EQ (mono.getNumChannels(), 1);
                CHECK_EQ (mono[0].indexInDevice, 5);
                CHECK (mono[0].channel == juce::AudioChannelSet::centre);
            }

            {
                auto stereo = ChannelConfiguration::stereo (2);
                CHECK_EQ (stereo.getNumChannels(), 2);
                CHECK_EQ (stereo[0].indexInDevice, 2);
                CHECK_EQ (stereo[1].indexInDevice, 3);
                CHECK (stereo[0].channel == juce::AudioChannelSet::left);
                CHECK (stereo[1].channel == juce::AudioChannelSet::right);
            }

            {
                auto surround = ChannelConfiguration::surround5_1 (0);
                CHECK_EQ (surround.getNumChannels(), 6);
                CHECK (surround[0].channel == juce::AudioChannelSet::left);
                CHECK (surround[1].channel == juce::AudioChannelSet::right);
                CHECK (surround[2].channel == juce::AudioChannelSet::centre);
                CHECK (surround[3].channel == juce::AudioChannelSet::LFE);
                CHECK (surround[4].channel == juce::AudioChannelSet::leftSurround);
                CHECK (surround[5].channel == juce::AudioChannelSet::rightSurround);
            }

            {
                auto surround71 = ChannelConfiguration::surround7_1 (0);
                CHECK_EQ (surround71.getNumChannels(), 8);
            }

            {
                auto discrete = ChannelConfiguration::discreteChannels (16, 0);
                CHECK_EQ (discrete.getNumChannels(), 16);

                for (int i = 0; i < 16; ++i)
                    CHECK_EQ (discrete[static_cast<size_t> (i)].indexInDevice, i);
            }
        }

        SUBCASE ("Query methods")
        {
            auto mono = ChannelConfiguration::mono();
            CHECK (mono.isMono());
            CHECK (! mono.isStereo());
            CHECK (mono.isMonoOrStereo());
            CHECK (! mono.isMultichannel());

            auto stereo = ChannelConfiguration::stereo();
            CHECK (! stereo.isMono());
            CHECK (stereo.isStereo());
            CHECK (stereo.isMonoOrStereo());
            CHECK (! stereo.isMultichannel());

            auto surround = ChannelConfiguration::surround5_1();
            CHECK (! surround.isMono());
            CHECK (! surround.isStereo());
            CHECK (! surround.isMonoOrStereo());
            CHECK (surround.isMultichannel());

            {
                auto config = ChannelConfiguration::stereo (4);
                auto range = config.getChannelRange();
                CHECK_EQ (range.first, 4);
                CHECK_EQ (range.second, 6);
            }
        }

        SUBCASE ("Serialization")
        {
            {
                auto original = ChannelConfiguration::stereo (2);
                auto json = original.toJSON();
                auto restored = ChannelConfiguration::fromJSON (json);

                CHECK_EQ (restored.getNumChannels(), 2);
                CHECK_EQ (restored[0].indexInDevice, 2);
                CHECK_EQ (restored[1].indexInDevice, 3);
                CHECK (restored[0].channel == juce::AudioChannelSet::left);
                CHECK (restored[1].channel == juce::AudioChannelSet::right);
            }

            {
                auto original = ChannelConfiguration::surround5_1 (0);
                auto str = original.toString();
                auto restored = ChannelConfiguration::fromString (str);

                CHECK_EQ (restored.getNumChannels(), 6);
                CHECK (original == restored);
            }

            {
                auto empty = ChannelConfiguration::fromString ("invalid json");
                CHECK (empty.isEmpty());
            }
        }

        SUBCASE ("Comparison")
        {
            auto stereo1 = ChannelConfiguration::stereo (0);
            auto stereo2 = ChannelConfiguration::stereo (0);
            auto stereo3 = ChannelConfiguration::stereo (2);

            CHECK (stereo1 == stereo2);
            CHECK (stereo1 != stereo3);

            auto mono = ChannelConfiguration::mono();
            CHECK (stereo1 != mono);
        }

        SUBCASE ("Intersection")
        {
            {
                // Partial overlap: stereo(0) channels 0,1 intersected with discrete 4 channels 0-3
                auto result = ChannelConfiguration::stereo (0).intersection (ChannelConfiguration::discreteChannels (4, 0));
                CHECK_EQ (result.getNumChannels(), 2);
            }

            {
                // No overlap: stereo(4) channels 4,5 vs discrete 2 channels 0,1
                auto result = ChannelConfiguration::stereo (4).intersection (ChannelConfiguration::discreteChannels (2, 0));
                CHECK (result.isEmpty());
            }

            {
                // Subset: mono(0) is a subset of stereo(0)
                auto result = ChannelConfiguration::mono (0).intersection (ChannelConfiguration::stereo (0));
                CHECK_EQ (result.getNumChannels(), 1);
            }

            {
                // Empty left operand
                auto result = ChannelConfiguration().intersection (ChannelConfiguration::stereo());
                CHECK (result.isEmpty());
            }

            {
                // Empty right operand
                auto result = ChannelConfiguration::stereo().intersection (ChannelConfiguration());
                CHECK (result.isEmpty());
            }

            {
                // Both empty
                auto result = ChannelConfiguration().intersection (ChannelConfiguration());
                CHECK (result.isEmpty());
            }

            {
                // Identical configs
                auto result = ChannelConfiguration::stereo (0).intersection (ChannelConfiguration::stereo (0));
                CHECK_EQ (result.getNumChannels(), 2);
                CHECK (result == ChannelConfiguration::stereo (0));
            }
        }

        SUBCASE ("Reversal - stereo")
        {
            auto stereo = ChannelConfiguration::stereo (0);
            auto rev = stereo.reversed();

            CHECK_EQ (rev.getNumChannels(), 2);
            // Device indices should be swapped
            CHECK_EQ (rev[0].indexInDevice, 1);
            CHECK_EQ (rev[1].indexInDevice, 0);
            // Channel types should be preserved in original order
            CHECK (rev[0].channel == juce::AudioChannelSet::left);
            CHECK (rev[1].channel == juce::AudioChannelSet::right);
        }

        SUBCASE ("Reversal - 4-channel discrete")
        {
            auto discrete = ChannelConfiguration::discreteChannels (4, 0);
            auto rev = discrete.reversed();

            CHECK_EQ (rev.getNumChannels(), 4);
            CHECK_EQ (rev[0].indexInDevice, 3);
            CHECK_EQ (rev[1].indexInDevice, 2);
            CHECK_EQ (rev[2].indexInDevice, 1);
            CHECK_EQ (rev[3].indexInDevice, 0);
        }

        SUBCASE ("Reversal - mono is unchanged")
        {
            auto mono = ChannelConfiguration::mono (5);
            auto rev = mono.reversed();

            CHECK_EQ (rev.getNumChannels(), 1);
            CHECK_EQ (rev[0].indexInDevice, 5);
            CHECK (rev[0].channel == juce::AudioChannelSet::centre);
        }

        SUBCASE ("Reversal - empty is empty")
        {
            auto empty = ChannelConfiguration();
            auto rev = empty.reversed();
            CHECK (rev.isEmpty());
        }

        SUBCASE ("Reversal - double reversal is identity")
        {
            auto original = ChannelConfiguration::surround5_1 (0);
            auto doubleReversed = original.reversed().reversed();
            CHECK (original == doubleReversed);
        }

        SUBCASE ("Reversal - preserves channel types")
        {
            auto surround = ChannelConfiguration::surround5_1 (0);
            auto rev = surround.reversed();

            // Channel types should stay in same positions
            CHECK (rev[0].channel == juce::AudioChannelSet::left);
            CHECK (rev[1].channel == juce::AudioChannelSet::right);
            CHECK (rev[2].channel == juce::AudioChannelSet::centre);
            CHECK (rev[3].channel == juce::AudioChannelSet::LFE);
            CHECK (rev[4].channel == juce::AudioChannelSet::leftSurround);
            CHECK (rev[5].channel == juce::AudioChannelSet::rightSurround);

            // Device indices should be reversed
            CHECK_EQ (rev[0].indexInDevice, 5);
            CHECK_EQ (rev[1].indexInDevice, 4);
            CHECK_EQ (rev[2].indexInDevice, 3);
            CHECK_EQ (rev[3].indexInDevice, 2);
            CHECK_EQ (rev[4].indexInDevice, 1);
            CHECK_EQ (rev[5].indexInDevice, 0);
        }

        SUBCASE ("Reversal - serialization round-trip")
        {
            auto original = ChannelConfiguration::stereo (0).reversed();
            auto json = original.toJSON();
            auto restored = ChannelConfiguration::fromJSON (json);

            CHECK_EQ (restored.getNumChannels(), 2);
            CHECK_EQ (restored[0].indexInDevice, 1);
            CHECK_EQ (restored[1].indexInDevice, 0);
            CHECK (restored[0].channel == juce::AudioChannelSet::left);
            CHECK (restored[1].channel == juce::AudioChannelSet::right);
        }

        SUBCASE ("Reversal - non-zero start index")
        {
            auto stereo = ChannelConfiguration::stereo (4);
            auto rev = stereo.reversed();

            CHECK_EQ (rev[0].indexInDevice, 5);
            CHECK_EQ (rev[1].indexInDevice, 4);
        }
    }
}

}} // namespace tracktion { inline namespace engine

#endif // ENGINE_UNIT_TESTS_CHANNELCONFIGURATION
