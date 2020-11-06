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
  version:          0.0.1
  name:             The Tracktion graph audio engine
  description:      Classes for creating and processing audio graphs
  website:          http://www.tracktion.com
  license:          Proprietary

  dependencies:     juce_audio_processors juce_dsp

 END_JUCE_MODULE_DECLARATION

*******************************************************************************/


/*******************************************************************************
 PLEASE READ!
 
 tracktion_graph is an experimental module. It is currently in an early stage of
 development so the API or functionaility it contains is in no way stable. It
 will change so if you do want to use this in your own code please be aware of
 this and prepare yourself for a lot of updating.

*******************************************************************************/


#pragma once
#define TRACKTION_GRAPH_H_INCLUDED

#include <cassert>
#include <thread>

//==============================================================================
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>


//==============================================================================
#include "tracktion_graph/tracktion_graph_Utility.h"

#include "utilities/tracktion_AudioFifo.h"
#include "utilities/tracktion_MidiMessageArray.h"
#include "utilities/tracktion_RealTimeSpinLock.h"
#include "utilities/tracktion_Threads.h"
#include "utilities/tracktion_graph_LatencyProcessor.h"

#include "tracktion_graph/tracktion_graph_PlayHead.h"

#include "tracktion_graph/tracktion_graph_Node.h"
#include "tracktion_graph/tracktion_graph_PlayHeadState.h"

#include "tracktion_graph/players/tracktion_graph_NodePlayerUtilities.h"

#include "tracktion_graph/tracktion_graph_NodePlayer.h"
#include "tracktion_graph/tracktion_graph_LockFreeMultiThreadedNodePlayer.h"
#include "tracktion_graph/tracktion_graph_NodePlayerThreadPools.h"

#include "tracktion_graph/nodes/tracktion_graph_LatencyNode.h"
#include "tracktion_graph/nodes/tracktion_graph_SummingNode.h"

#include "tracktion_graph/players/tracktion_graph_SimpleNodePlayer.h"
