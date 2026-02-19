#include <iostream>
#include <cmath>

#include "../choc/audio/choc_SampleBuffers.h"
#include "../choc/audio/choc_AudioFileFormat_WAV.h"
#include "../choc/text/choc_Files.h"

int main()
{
    const double sampleRate = 44100.0;
    const double frequency = 440.0;
    const double duration = 2.0;
    const int numChannels = 1;
    const int numFrames = static_cast<int>(sampleRate * duration);

    auto buffer = choc::buffer::createChannelArrayBuffer<float> (numChannels, numFrames, [&] (choc::buffer::ChannelCount, choc::buffer::FrameCount frame) -> float
    {
        return static_cast<float> (std::sin (2.0 * M_PI * frequency * static_cast<double> (frame) / sampleRate));
    });

    choc::audio::WAVAudioFileFormat writer;
    auto wavData = writer.createData (buffer.getView(), 16);

    if (choc::file::writeToFile ("sine_wave.wav", wavData.data(), wavData.size()))
        std::cout << "Successfully wrote sine_wave.wav" << std::endl;
    else
        std::cout << "Failed to write sine_wave.wav" << std::endl;

    return 0;
}
