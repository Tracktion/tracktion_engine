/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    You may use this code under the terms of the GPL v3 - see LICENCE.md for details.
    For the technical preview this file cannot be licensed commercially.
*/

#pragma once

#if __has_include(<choc/audio/choc_SampleBuffers.h>)
 #include <choc/audio/choc_SampleBuffers.h>
#else
 #include "../../3rd_party/choc/audio/choc_SampleBuffers.h"
#endif

#include "../utilities/tracktion_Time.h"
#include "tracktion_Types.h"


namespace tracktion { inline namespace core
{

//==============================================================================
//==============================================================================
/**
    Base class for audio based readers that can be chained together.
*/
class AudioReader
{
public:
    //==============================================================================
    /** Destructor. */
    virtual ~AudioReader() = default;

    /** Must return the number of channels in the reader. */
    virtual choc::buffer::ChannelCount getNumChannels() = 0;

    /** Should set the time position to be read from next. */
    virtual double getSampleRate() = 0;

    //==============================================================================
    /** Must return the sample position that will be read from next. */
    virtual SampleCount getPosition() = 0;

    /** Should set the sample position to be read from next. */
    virtual void setPosition (SampleCount) = 0;

    /** Should set the time position to be read from next. */
    virtual void setPosition (TimePosition) = 0;

    /** Signifies a break in continuity and that the stream should reset itself. */
    virtual void reset() = 0;

    //==============================================================================
    /** Must read a number of frames from the source, filling the buffer. */
    virtual bool readSamples (choc::buffer::ChannelArrayView<float>&) = 0;
};

}} // namespace tracktion { inline namespace core
