/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#ifdef TRACKTION_GRAPH_H_INCLUDED
 /* When you add this cpp file to your project, you mustn't include it in a file where you've
    already included any other headers - just put it inside a file on its own, possibly with your config
    flags preceding it, but don't include anything else. That also includes avoiding any automatic prefix
    header files that the compiler may be using.
 */
 #error "Incorrect use of JUCE cpp file"
#endif

//==============================================================================
#include <juce_audio_formats/juce_audio_formats.h>

//==============================================================================
#include "tracktion_graph_TestConfig.h"

//==============================================================================
#include "tracktion_graph.h"

//==============================================================================
#include "tracktion_graph/tracktion_graph_tests_Utilities.h"
#include "tracktion_graph/tracktion_graph_tests_TestNodes.h"

#include "tracktion_graph/tracktion_graph_PlayHeadState.cpp"
#include "tracktion_graph/tracktion_graph_PlayHead.cpp"
#include "tracktion_graph/tracktion_graph_tests_Node.cpp"
#include "tracktion_graph/tracktion_graph_tests_NodeVisiting.cpp"
#include "tracktion_graph/tracktion_graph_Utility.cpp"

#include "tracktion_graph/tracktion_graph_LockFreeMultiThreadedNodePlayer.cpp"
#include "tracktion_graph/tracktion_graph_NodePlayerThreadPools.cpp"

#include "tracktion_graph/nodes/tracktion_graph_ConnectedNode.test.cpp"

#include "utilities/tracktion_Threads.cpp"

