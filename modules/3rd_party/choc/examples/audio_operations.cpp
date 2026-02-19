#include <iostream>
#include <cmath>

#include "../choc/audio/choc_SampleBuffers.h"
#include "../choc/audio/choc_AudioFileFormat.h"
#include "../choc/audio/choc_AudioFileFormat.h"
#include "../choc/audio/choc_AudioFileFormat_WAV.h"
#include "../choc/text/choc_Files.h"
#include "../choc/audio/choc_Oscillators.h"
#include "../choc/audio/choc_SampleBufferUtilities.h"
#include "../choc/audio/choc_SincInterpolator.h"

// Helper to save a buffer to a WAV file
bool saveBufferToWAV (const std::string& filename, const choc::buffer::ChannelArrayBuffer<float>& buffer)
{
    choc::audio::WAVAudioFileFormat<true> writerFormat;
    choc::audio::AudioFileProperties properties;
    properties.sampleRate = 44100.0;
    properties.numChannels = buffer.getNumChannels();
    properties.numFrames = buffer.getNumFrames();
    properties.bitDepth = choc::audio::BitDepth::int16;

    auto writer = writerFormat.createWriter (filename, properties);

    if (writer == nullptr)
    {
        std::cout << "Failed to create writer for " << filename << std::endl;
        return false;
    }

    if (! writer->appendFrames (buffer.getView()))
    {
        std::cout << "Failed to write frames to " << filename << std::endl;
        return false;
    }

    std::cout << "Successfully wrote " << filename << std::endl;
    return true;
}

int main()
{
    const double sampleRate = 44100.0;
    const double duration = 2.0;
    const int numChannels = 1;
    const int numFrames = static_cast<int> (sampleRate * duration);

    // 1. Generate a sine wave
    auto sineWaveBuffer = choc::oscillator::createChannelArraySine<float> ({ (choc::buffer::ChannelCount) numChannels, (choc::buffer::FrameCount) numFrames }, 440.0, sampleRate);
    saveBufferToWAV ("sine_wave_original.wav", sineWaveBuffer);

    // 2. Apply a simple gain
    auto gainedBuffer = sineWaveBuffer;
    choc::buffer::applyGain (gainedBuffer, 0.5f);
    saveBufferToWAV ("sine_wave_gained.wav", gainedBuffer);

    // 3. Mix another sine wave
    auto mixedBuffer = gainedBuffer;
    auto secondSineWave = choc::oscillator::createChannelArraySine<float> ({ (choc::buffer::ChannelCount) numChannels, (choc::buffer::FrameCount) numFrames }, 660.0, sampleRate);
    choc::buffer::add (mixedBuffer, secondSineWave);
    saveBufferToWAV ("sine_wave_mixed.wav", mixedBuffer);

    // 4. Read the WAV file back (demonstrates reading)
    choc::audio::AudioFileFormatList formatList;
    formatList.addFormat<choc::audio::WAVAudioFileFormat<false>>();

    choc::audio::AudioFileData audioFile;
    try
    {
        audioFile = formatList.loadFileContent (std::make_shared<std::ifstream> ("sine_wave_mixed.wav", std::ios::binary | std::ios::in));
    }
    catch (const std::exception& e)
    {
        std::cerr << "Failed to load audio from sine_wave_mixed.wav: " << e.what() << std::endl;
        return 1;
    }

    if (audioFile.frames.getNumFrames() == 0)
    {
        std::cerr << "Failed to load audio from sine_wave_mixed.wav" << std::endl;
        return 1;
    }

    choc::buffer::ChannelArrayBuffer<float> loadedBuffer = audioFile.frames;
    std::cout << "Successfully loaded sine_wave_mixed.wav" << std::endl;

    // 5. Perform a simple pitch shift using sinc interpolation
    const double pitchShiftRatio = 1.2; // Shift up by 20%
    const int pitchShiftedFrames = static_cast<int> (loadedBuffer.getNumFrames() / pitchShiftRatio);
    choc::buffer::ChannelArrayBuffer<float> pitchShiftedBuffer (loadedBuffer.getNumChannels(), pitchShiftedFrames);
    choc::interpolation::sincInterpolate (pitchShiftedBuffer, loadedBuffer);
    saveBufferToWAV ("sine_wave_pitch_shifted.wav", pitchShiftedBuffer);

    return 0;
}
