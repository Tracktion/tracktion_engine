/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

// Defined in tracktion_core
#define TRACKTION_UNIT_TESTS_TIME          1

// Defined in tracktion_engine
#define GRAPH_UNIT_TESTS_WAVENODE          1
#define GRAPH_UNIT_TESTS_MIDINODE          1
#define GRAPH_UNIT_TESTS_RACKNODE          1
#define GRAPH_UNIT_TESTS_EDITNODE          1
#define ENGINE_UNIT_TESTS_LOOPINGMIDINODE  1
#define ENGINE_UNIT_TESTS_CLIPS            1
#define ENGINE_UNIT_TESTS_SELECTABLE       1
#define ENGINE_UNIT_TESTS_AUDIO_FILE       1
#define ENGINE_UNIT_TESTS_AUDIO_FILE_CACHE 1

// Defined in tracktion_graph
#define GRAPH_UNIT_TESTS_PLAYHEAD          1
#define GRAPH_UNIT_TESTS_PLAYHEADSTATE     1
#define GRAPH_UNIT_TESTS_NODE              1
#define GRAPH_UNIT_TESTS_NODEVISITING      1
#define GRAPH_UNIT_TESTS_SAMPLECONVERSION  1
#define GRAPH_UNIT_TESTS_CONNECTEDNODE     1

#define GRAPH_UNIT_TESTS_AUDIOBUFFERPOOL   1
#define GRAPH_UNIT_TESTS_SEMAPHORE         1
#define GRAPH_UNIT_TESTS_ALLOCATION        1

// Benchmarks
#define CORE_BENCHMARKS_TEMPO              1

#define GRAPH_BENCHMARKS_THREADS           1

#define ENGINE_BENCHMARKS_AUDIOFILECACHE   1
#define ENGINE_BENCHMARKS_MIDICLIP         1
#define ENGINE_BENCHMARKS_EDITITEMID       1
#define ENGINE_BENCHMARKS_NODE             1
#define ENGINE_BENCHMARKS_RACKS            1
