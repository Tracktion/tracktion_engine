/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

// Defined in tracktion_core
#define TRACKTION_UNIT_TESTS_TIME                       1
#define TRACKTION_UNIT_TESTS_ALGORITHM                  1

// Defined in tracktion_engine
#define GRAPH_UNIT_TESTS_WAVENODE                       1
#define GRAPH_UNIT_TESTS_MIDINODE                       1
#define GRAPH_UNIT_TESTS_RACKNODE                       1
#define GRAPH_UNIT_TESTS_EDITNODE                       1

#define ENGINE_UNIT_TESTS_AUTOMATION                    1
#define ENGINE_UNIT_TESTS_AUX_SEND                      1
#define ENGINE_UNIT_TESTS_CLIPBOARD                     1
#define ENGINE_UNIT_TESTS_CLIPSLOT                      1
#define ENGINE_UNIT_TESTS_CONSTRAINED_CACHED_VALUE      1
#define ENGINE_UNIT_TESTS_DELAY_PLUGIN                  1
#define ENGINE_UNIT_TESTS_EDIT                          1
#define ENGINE_UNIT_TESTS_EDIT_LOADER                   1
#define ENGINE_UNIT_TESTS_EDIT_TIME                     1
#define ENGINE_UNIT_TESTS_FREEZE                        1
#define ENGINE_UNIT_TESTS_FOLLOW_ACTIONS                1
#define ENGINE_UNIT_TESTS_LATENCY                       1
#define ENGINE_UNIT_TESTS_LAUNCH_HANDLE                 1
#define ENGINE_UNIT_TESTS_LAUNCHER_CLIP_PLAYBACK_HANDLE 1
#define ENGINE_UNIT_TESTS_LAUNCH_QUANTISATION           1
#define ENGINE_UNIT_TESTS_LOOPINGMIDINODE               1
#define ENGINE_UNIT_TESTS_LOOP_INFO                     1
#define ENGINE_UNIT_TESTS_MIDILIST                      1
#define ENGINE_UNIT_TESTS_MODIFIERS                     1
#define ENGINE_UNIT_TESTS_PAN_LAW                       1
#define ENGINE_UNIT_TESTS_PLAYBACK                      1
#define ENGINE_UNIT_TESTS_PLUGINS                       1
#define ENGINE_UNIT_TESTS_PDC                           1
#define ENGINE_UNIT_TESTS_RECORDING                     1
#define ENGINE_UNIT_TESTS_RENDERING                     1
#define ENGINE_UNIT_TESTS_TIMESTRETCHER                 1
#define ENGINE_UNIT_TESTS_CLIPS                         1
#define ENGINE_UNIT_TESTS_SELECTABLE                    1
#define ENGINE_UNIT_TESTS_AUDIO_FILE                    1
#define ENGINE_UNIT_TESTS_AUDIO_FILE_CACHE              1
#define ENGINE_UNIT_TESTS_VOLPANPLUGIN                  1
#define ENGINE_UNIT_TESTS_TEMPO_SEQUENCE                1
#define ENGINE_UNIT_TESTS_QUANTISATION_TYPE             1
#define ENGINE_UNIT_TESTS_WAVE_INPUT_DEVICE             1

// Defined in tracktion_graph
#define GRAPH_UNIT_TESTS_PLAYHEAD                       1
#define GRAPH_UNIT_TESTS_PLAYHEADSTATE                  1
#define GRAPH_UNIT_TESTS_NODE                           1
#define GRAPH_UNIT_TESTS_NODEVISITING                   1
#define GRAPH_UNIT_TESTS_SAMPLECONVERSION               1
#define GRAPH_UNIT_TESTS_CONNECTEDNODE                  1

#define GRAPH_UNIT_TESTS_AUDIOBUFFERPOOL                1
#define GRAPH_UNIT_TESTS_SEMAPHORE                      1
#define GRAPH_UNIT_TESTS_ALLOCATION                     1

// Benchmarks
#define CORE_BENCHMARKS_TEMPO                           1

#define GRAPH_BENCHMARKS_THREADS                        1

#define ENGINE_BENCHMARKS_AUDIOFILECACHE                1
#define ENGINE_BENCHMARKS_CONTAINERCLIP                 1
#define ENGINE_BENCHMARKS_MIDICLIP                      1
#define ENGINE_BENCHMARKS_EDITITEMID                    1
#define ENGINE_BENCHMARKS_WAVENODE                      1
#define ENGINE_BENCHMARKS_RESAMPLING                    1
#define ENGINE_BENCHMARKS_RACKS                         1
#define ENGINE_BENCHMARKS_SELECTABLE                    1
#define ENGINE_BENCHMARKS_PLUGINNODE                    1
