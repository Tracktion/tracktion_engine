/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
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
