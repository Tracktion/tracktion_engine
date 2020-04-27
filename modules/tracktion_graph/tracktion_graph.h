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

  dependencies:     juce_audio_processors juce_dsp

 END_JUCE_MODULE_DECLARATION

*******************************************************************************/

#pragma once
#define TRACKTION_GRAPH_H_INCLUDED


#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#undef __TEXT


//==============================================================================
#include "utilities/tracktion_graph_AudioFifo.h"

#include "tracktion_graph/tracktion_graph_AudioNode.h"
#include "tracktion_graph/tracktion_graph_AudioNodeProcessor.h"
#include "tracktion_graph/tracktion_graph_UtilityNodes.h"
#include "tracktion_graph/tracktion_graph_tests_Utilities.h"
#include "tracktion_graph/tracktion_graph_tests_TestAudioNodes.h"
