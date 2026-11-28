# Node Player Thread Pools: What Was Tried

A record of the November 2026 investigation into how `LockFreeMultiThreadedNodePlayer` puts its worker threads to sleep and wakes them, written so the work doesn't have to be repeated. It covers what was measured, what was kept, what was dropped and why, the conclusions that turned out to be wrong, and the leads nobody followed.

The aim was fewer peak overruns and better use of the threads during real-time playback. Offline rendering was a secondary concern.

## Where things stand

- **Default strategy: unchanged.** `ThreadPoolStrategy::lightweightSemHybrid` (24 pause rounds, 25 `yield()` rounds, then a semaphore).
- **Candidate to replace it: `lightweightSemaphoreWakeQueued`.** Threads go straight to the semaphore when no Nodes are queued. Only threads that are asleep are signalled, at the start of a block and again whenever a Node queues work for other threads part way through one. Selectable with `EditPlaybackContext::setThreadPoolStrategy`.
- **Tracked in CI:** the Rack benchmarks run single threaded, the default and the candidate, offline and at a real-time cadence, on three Edits (see below).
- **Still in the code at the time of writing:** eleven experimental strategies and the `wake_experiment` counters in the player. They are only reachable through `TE_RT_STRATEGIES` and can be removed; this document and the history of the pull request that added it are the record.

## How it was measured

All of this lives in `modules/tracktion_engine/playback/graph/tracktion_RackBenchmarks.test.cpp`.

- **Edits.** `RackMixBus` (86 audio tracks, 1,062 Nodes, all cheap), `RackMixBusLoaded` (a 10 us synthetic plugin load on every track) and `RackMixBusBlocked` (one 375 us load in front of a Rack, a 50 us load on each of the nine tracks sharing it). 96 kHz, 128 and 512 sample blocks.
- **Real-time cadence.** A driver thread sleeps until each block's deadline and then processes one block, so the workers really do sleep between blocks. Built with `ENGINE_BENCHMARKS_RACKS_REALTIME=1`. On macOS the threads join an audio workgroup the benchmark creates.
- **Switches** (environment variables): `TE_RT_ONLY`, `TE_RT_SECONDS`, `TE_RT_STRATEGIES` (names, including experimental ones), `TE_RT_ROTATE` (run order), `TE_RT_WORKGROUP=0`, `TE_RACK_EDITS`.
- **Numbers** are the wall-clock time of one `process()` call for the whole graph, in microseconds. Percentiles come from a log-bucketed histogram with about 3% resolution, so many values are bucket edges.
- **Machines.** An M2 Max (8 performance and 4 efficiency cores, 11 workers), and the hosted CI runners (4 vCPU Linux and Windows VMs, 3 workers).

Things that will mislead you if you forget them:

- Interleave the binaries being compared, repeat at least three times and take the median. Single runs differ by 5-10% offline.
- Two CI runs of identical code differed by 30-50%. CI shows that something builds, runs and publishes; it cannot rank a 10-20% change, and one run cannot rank maxima.
- On macOS, measure with the workgroup. Without one a 68 us single-threaded block took about 450 us at cadence and the strategy ranking changes.
- With the workgroup, a block that uses little of its period runs on slowed-down cores on the Mac (see "Corrections"). Judge strategies on the loaded and blocked Edits and at 512, not on the median of the unloaded Edit.
- "missed" on the Windows runner is mostly the driver thread waking about 400 us late. Read the block time columns and "over" there.
- The unloaded Edit only measures the fixed overhead of the player. Real sessions are dominated by plugin DSP, so "single threaded wins at 128" on that Edit is not a reason for a thread-count policy.
- The macOS CI runner (3 vCPU) misses deadlines even single threaded. Its real-time results are unusable.

## What was kept

Fixed per-block overhead in the player. RackMixBus, offline, 128, median block in us on the Mac:

| Step | Single threaded | MT default |
|---|---|---|
| Before | 74.8 | 145.4 |
| `hasBeenQueued` flags only in debug builds | 75.8 | 147.5 |
| Input counters re-armed by the thread that queues each Node | 72.7 | 145.4 |
| No retain/release between Nodes unless buffers are pooled | 68.6 | 121.8 |
| Only Nodes that need it are prepared at the start of a block | 67.6 | 107.5 |
| Idle pool threads stop writing a shared counter | 67.6 | 101.4 |

The serial prepare pass on the audio thread went from 48.7 to 16.4 us. Its cost was cache lines moving between cores (single threaded, the same pass cost 8.5 us), not the work itself. 481 of the 1,062 Nodes still need preparing each block. Nodes declare whether they use `prefetchBlock` with `NodeOptimisations::prefetch`, defaulting to yes.

Other changes kept:

- **Pool threads are really real-time now.** The pools were upgrading the thread that created them, normally the message thread, instead of their own threads. On the Mac without a workgroup this took the default's p99 from 344 to 250 and its worst block from 8,119 to 2,716.
- **The mid-block signal.** `ThreadPool::nodesQueuedDuringBlock` is called when a finishing Node queues work for other threads. Before this, nothing woke a sleeping thread during a block: there was one signal at the start and later Nodes were queued silently.
- **The sleeping-thread count** (`SleepingThreads`). A thread registers before its last look for Nodes; the signaller claims registered sleepers and signals exactly that many, so nothing is left on the semaphore.

## The candidate against the default

Real-time cadence, mean / p90 / worst block in us. Mac figures are the median of three 10 s runs with the workgroup; the VMs are one CI run each.

| | Default | Candidate |
|---|---|---|
| Mac, Blocked 128 | 680 / 704 / 930 | 536 / 557 / 605 |
| Mac, Blocked 512 | 2,571 / 2,621 / 3,423 | 2,007 / 2,032 / 2,195 |
| Mac, Loaded 128 | 286 / 344 / 389 | 295 / 344 / 373 |
| Mac, Loaded 512 | 1,178 / 1,442 / 1,770 | 996 / 1,212 / 1,245 |
| Mac, Unloaded 128 | 102 / 102 / 192 | 152 / 184 / 230 |
| Linux VM, Blocked 128 | 794 / 819 / 933 | 531 / 541 / 2,649 |
| Linux VM, Loaded 128 | 300 / 311 / 1,060 | 296 / 303 / 861 |
| Linux VM, Loaded 512 | 1,074 / 1,180 / 1,774 | 983 / 999 / 1,436 |
| Windows VM, Blocked 128 | 706 / 737 / 1,843 | 470 / 483 / 1,373 |
| Windows VM, Loaded 128 | 282 / 311 / 694 | 271 / 287 / 355 |
| Windows VM, Loaded 512 | 1,067 / 1,180 / 1,306 | 941 / 967 / 2,058 |

- When a graph is blocked on one slow Node, pools that sleep are no faster than single threaded (868 us). The mid-block signal brings that to about 540, within 8% of a pool that never sleeps, for about 40% of its CPU.
- The candidate's one loss is the unloaded Edit on the Mac. That is not caused by its signalling (see "Corrections").
- The Linux blocked run had single blocks of 2.6 ms for the candidate where the run before had 584 us. Not explained; one run on a VM.

## What was tried and dropped

| Variant | What it does | Result |
|---|---|---|
| `lightweightSemHybridWakeAll` | One `semaphore_signal_all` call on macOS | Saves 6 us on the audio thread, block time unchanged. A thread that has decided to sleep but not reached the kernel misses the wake-up and sits out the block |
| `lightweightSemHybridCascade` | Audio thread wakes two threads, each wakes two more | Audio thread's wake cost 20 to 3.7 us, block time unchanged. With the workgroup it got worse (bimodal, p99 176 against 109) |
| `lightweightSemHybridCapped` | Unused semaphore counts can't build up | No effect at cadence: each thread passes through the semaphore once a block, so the cap never binds |
| `...NoSemSpin` | The semaphore's own 10,000 iteration spin removed | No effect on block time, 2-14% less CPU |
| `lightweightSemHybridPause` | The yields replaced by pauses | Identical to plain `lightweightSemaphore` in every run. 49 pause rounds take well under a microsecond on the Mac |
| `blockEvent` | Threads sleep on a block counter (`std::atomic::wait`), one `notify_all` wakes them all | Level with `lightweightSemaphore` in real time, best offline on the first loaded Edit. Always wakes every thread |
| `blockEventHybrid` | As above but polls while a block is in progress | No better than `blockEvent` in real time, far worse offline |
| `blockEventSpin` | Never sleeps (diagnostic) | Fastest median, several times the CPU, missed deadlines |
| `blockEventWakeQueued` | `blockEvent` with the mid-block signal | Within a few percent of the candidate on the Mac, no cost on the unloaded Edit. Its block-start wake costs twice the semaphore's on Windows (16-24 us against 8-14) |
| `lightweightSemHybridPauseWakeQueued` | Pause hybrid with the mid-block signal | Matched the candidate; no separate figures were kept |

Ideas discussed and not built:

- **Sleep until just before the next block.** `sleep_until` woke 10-12 us late on the Mac, 61 us on the Linux runner and 400 us on the Windows runner, device callbacks aren't regular, and it does nothing for threads that run out of work mid-block.
- **A thread-count policy for small blocks.** Dropped: it only follows from a graph of cheap Nodes.
- **A threshold between signalling some threads and waking all.** Only "wake all when the count covers every sleeper" was built, and wake-all itself wasn't worth keeping.

## Findings worth keeping

- **Waking the pool costs about 12-25 us per block.** One call or eleven, the cost is the kernel making the threads runnable. Cascading moves it off the audio thread without making the block any shorter. This is the fixed price of going multi-threaded and is why single threaded beats every pool on the unloaded Edit at 128 (86 against 98-100 us with the workgroup).
- **How long an idle thread looks for work on the Mac before sleeping:** 24 pause rounds under 0.1 us, 25 yield rounds about 3 us, then the semaphore spins for about 3 us. Not measured on Intel, where a pause is much longer.
- **The yields are a CPU cost, not a scheduling trick.** They burn about 1,500 us of CPU per 1,333 us block on the Mac (11 workers). With the workgroup they buy nothing at the median. Without it they are 18-35% faster on the loaded Edits, apparently because they keep the cores busy. On the 4 vCPU runners their CPU cost is about the same as plain `lightweightSemaphore`.
- **How `signal (n)` reaches the kernel:** one `semaphore_signal` per sleeping thread on macOS, one `sem_post` each on Linux, a single `ReleaseSemaphore (handle, n)` on Windows. The lightweight semaphore only makes kernel calls for threads that are actually waiting.
- **The Windows semaphore is a kernel semaphore object, not `WaitOnAddress`.** With the sleeper count it was the fastest wake measured there.
- **`std::atomic::wait` differs a lot by library.** libc++ on macOS busy-polls for 4 us and sleeps on a proxy counter in a shared 256 entry table. libstdc++ 15 does 12 pauses and 4 `sched_yield`s, then a futex on the atomic itself. MSVC calls `WaitOnAddress` directly and its `notify_all` always makes a system call. Lost-wake bugs were fixed in LLVM 21 and GCC 16. For this use a lost wake costs one thread for one block.
- **The mid-block signal is cheap but not bounded.** About 24 calls a block on these Edits, mean 0.6-2 us, p99 up to 24 us, worst 43 us on the Mac. Most calls find nobody asleep and make no system call.
- **The loaded Edit only scales 2.7 times on 12 threads.** Traced: about 25-30 us before the first load starts, the 78 loads run on 7 busy threads (only the performance cores take part; three workers arrive about 200 us late), and the cheap Nodes cost about 3 times more thread time each when spread across threads. A block is roughly the unloaded Edit's 100 us plus the loads divided by 7.
- **A stale sleeper count over-wakes.** Before it was reset when threads are recreated, wake-ups per block on the blocked Edit were 52 (36 of them finding nothing) against 26 (11) after.

## Corrections

Things that were believed for a while and are wrong or unproven. Don't rediscover them.

- **"The yields cause one-second stalls offline."** Wrong. The stalls appeared with 13 of 18 strategies and predate the priority fix. See the next section for the cause.
- **"Switch the default to `lightweightSemaphore`."** Withdrawn. It is level with the default when every track has work, but on the blocked Edit it is no faster than single threaded while the default's polling saves 20%.
- **"The candidate is slow on the Mac's unloaded Edit because of empty wake-ups."** Wrong. Signalling only sleeping threads didn't change it. In a slow block each thread processes the same number of Nodes but takes about 1.7 times longer, a pure CPU load takes 65 us multi-threaded against 37 single threaded, and the gap disappears without the workgroup (149 against 155). The inference is that macOS slows the cores when the workgroup has slack. This was never confirmed with `powermetrics`.
- **"The hybrid pool uses 2.7 times the CPU."** Only on the 12 core Mac with 11 workers.
- **"The hybrid's lower median is smarter scheduling."** More likely warm cores: `blockEventHybrid` polls for the whole block and gains nothing, and the gain vanishes with the workgroup. Not proved.
- **"Real-time threads that never sleep get penalised by the OS"** was offered as the reason the default got 36% slower offline after the priority fix. Never proved; the stall mechanism below explains at least part of it.

## Known issue: stalled blocks in offline renders

Not fixed. About 1% of unloaded and 10-15% of loaded multi-threaded offline benchmark renders have one block that takes up to a second.

- **Cause.** The render thread runs at normal priority and the pool threads are real-time. The Node queue (`rigtorp::MPMCQueue`) claims a slot and then publishes it. If the render thread is descheduled between the two, every pop fails while `numNodesQueued` is above zero, so the pool threads spin without sleeping and starve it.
- **How it was caught.** An in-process watchdog thread that, when a block ran over 50 ms, dumped every thread's run state, priority, CPU time and stack. A sampling profiler started too late.
- **Two fixes, each tested over 72 loaded renders:** leaving pool threads at normal priority for offline renders (0 stalls, worst block 20 ms, 5-15% faster on average) and backing off when a dequeue fails (0 stalls, worst 21 ms). Unchanged: 8 renders with a block over 50 ms, worst 1,369 ms.
- **Not known:** why semaphore pools end the stall at almost exactly one second, and what priority an application's export thread runs at, so how often real exports hit this.

## Leads nobody followed

- **Real desktops.** Every Windows and Linux number here is from a 4 vCPU VM. Many cores, hyperthreading, core parking and mixed core types are untested. The x86 runs should include a variant with a pause hint in the semaphore's spin, which has none.
- **Signal only when the queued work is worth it.** The mid-block signal fires on every fan-out however cheap the queued Nodes are, and cheap Nodes cost about 3 times more thread time when spread across threads.
- **The offline stall fix** above.
- **macOS with the workgroup setting off**, where only the audio callback thread is in the workgroup. Without a workgroup the yields were still ahead on the loaded Edits.
- **Confirming the slowed-cores explanation** with `powermetrics`.
- **A block-counter pool on OS primitives** instead of `std::atomic::wait` (no proxy table, no hidden yields, no library bugs). A manual-reset event or direct `WaitOnAddress` on Windows was never tried either.
- **Why three of eleven workers arrive 200 us late** on the Mac and only the performance cores do the work.
- **Merging the player's two serial passes** and **collapsing linear chains of Nodes into one schedulable unit**. Both were on the original list and never tried.
- **Per-Node timing** and synthetic graphs with controlled work per Node. `SyntheticLoadPlugin` is a first step.
- **The Linux blocked run's 2.6-4.5 ms blocks** with the mid-block signal: one CI run, not reproduced or explained.
