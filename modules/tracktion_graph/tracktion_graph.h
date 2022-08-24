/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

/*******************************************************************************
 The block below describes the properties of this module, and is read by
 the Projucer to automatically generate project code that uses it.
 For details about the syntax and how to create or use a module, see the
 JUCE Module Format.txt file.


 BEGIN_JUCE_MODULE_DECLARATION

  ID:               tracktion_graph
  vendor:           Tracktion Corporation
  version:          1.0.0
  name:             The Tracktion graph audio engine
  description:      Classes for creating and processing audio graphs
  website:          http://www.tracktion.com
  license:          Proprietary

  dependencies:     juce_audio_formats

 END_JUCE_MODULE_DECLARATION

*******************************************************************************/


/*******************************************************************************
 PLEASE READ!
 
 tracktion_graph is an experimental module. It is currently in an early stage of
 development so the API or functionality it contains is in no way stable. It
 will change so if you do want to use this in your own code please be aware of
 this and prepare yourself for a lot of updating.

*******************************************************************************/


#pragma once
#define TRACKTION_GRAPH_H_INCLUDED


//==============================================================================
/** Config: GRAPH_UNIT_TESTS_QUICK_VALIDATE

    If this is enabled, only a minimal set of buffer sizes will be tested.
    This gives a good idea if the module works correctly but also optimises CI
    completion speed.
*/
#ifndef GRAPH_UNIT_TESTS_QUICK_VALIDATE
 #define GRAPH_UNIT_TESTS_QUICK_VALIDATE 0
#endif

//==============================================================================
//==============================================================================
#include <cassert>
#include <thread>
#include <optional>

//==============================================================================
#if __has_include(<choc/audio/choc_SampleBuffers.h>)
 #include <choc/audio/choc_SampleBuffers.h>
 #include <choc/audio/choc_MIDISequence.h>
 #include <choc/audio/choc_MultipleReaderMultipleWriterFIFO.h>
 #include <choc/containers/choc_NonAllocatingStableSort.h>
#else
 #include "../3rd_party/choc/audio/choc_SampleBuffers.h"
 #include "../3rd_party/choc/audio/choc_MIDISequence.h"
 #include "../3rd_party/choc/containers/choc_MultipleReaderMultipleWriterFIFO.h"
 #include "../3rd_party/choc/containers/choc_NonAllocatingStableSort.h"
#endif

//==============================================================================
#include <juce_audio_basics/juce_audio_basics.h>
#include "../tracktion_core/tracktion_core.h"

//==============================================================================
#include "utilities/tracktion_MidiMessageArray.h"
namespace tracktion_engine = tracktion::engine;

#include "tracktion_graph/tracktion_Node.h"
#include "tracktion_graph/tracktion_Utility.h"

#include "utilities/tracktion_AudioBufferPool.h"
#include "utilities/tracktion_AudioBufferStack.h"
#include "utilities/tracktion_GlueCode.h"
#include "utilities/tracktion_AudioFifo.h"
#include "utilities/tracktion_PerformanceMeasurement.h"
#include "utilities/tracktion_RealTimeSpinLock.h"
#include "utilities/tracktion_Semaphore.h"
#include "utilities/tracktion_Threads.h"
#include "utilities/tracktion_LatencyProcessor.h"
#include "utilities/tracktion_LockFreeObject.h"

#include "tracktion_graph/tracktion_PlayHead.h"

#include "tracktion_graph/tracktion_PlayHeadState.h"

#include "tracktion_graph/players/tracktion_NodePlayerUtilities.h"

#include "tracktion_graph/tracktion_NodePlayer.h"
#include "tracktion_graph/tracktion_MultiThreadedNodePlayer.h"
#include "tracktion_graph/tracktion_LockFreeMultiThreadedNodePlayer.h"
#include "tracktion_graph/tracktion_NodePlayerThreadPools.h"

#include "tracktion_graph/nodes/tracktion_ConnectedNode.h"
#include "tracktion_graph/nodes/tracktion_LatencyNode.h"
#include "tracktion_graph/nodes/tracktion_SummingNode.h"

#include "tracktion_graph/players/tracktion_SimpleNodePlayer.h"
