#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>
#include <random>
#include <iomanip>
#include <string>
#include "../choc/threading/choc_TaskThread.h"
#include "../choc/threading/choc_SpinLock.h"
#include "../choc/threading/choc_ThreadSafeFunctor.h"
#include "../choc/containers/choc_SingleReaderSingleWriterFIFO.h"
#include "../choc/containers/choc_SingleReaderMultipleWriterFIFO.h"
#include "../choc/containers/choc_MultipleReaderMultipleWriterFIFO.h"

struct Task
{
    int id;
    std::string description;
    std::chrono::steady_clock::time_point timestamp;

    Task() = default;
    Task (int i, const std::string& desc)
        : id (i), description (desc), timestamp (std::chrono::steady_clock::now()) {}
};

void demonstrateTaskThread()
{
    std::cout << "\n=== TaskThread Demo ===\n";

    choc::threading::TaskThread taskThread;
    std::atomic<int> counter{0};

    // Start a task that runs every 500ms
    taskThread.start (std::chrono::milliseconds (500), [&counter]()
    {
        int current = counter.fetch_add (1);
        std::cout << "Periodic task executed #" << current + 1
                  << " at " << std::chrono::duration_cast<std::chrono::milliseconds>(
                      std::chrono::steady_clock::now().time_since_epoch()).count() << "ms\n";
    });

    // Let it run for a bit
    std::this_thread::sleep_for (std::chrono::milliseconds (1200));

    // Trigger it manually a few times
    std::cout << "Triggering task manually...\n";
    taskThread.trigger();
    std::this_thread::sleep_for (std::chrono::milliseconds (100));
    taskThread.trigger();
    std::this_thread::sleep_for (std::chrono::milliseconds (100));

    // Let it run a bit more
    std::this_thread::sleep_for (std::chrono::milliseconds (800));

    std::cout << "Stopping task thread...\n";
    taskThread.stop();

    std::cout << "Task executed " << counter.load() << " times total\n";
}

void demonstrateSpinLock()
{
    std::cout << "\n=== SpinLock Demo ===\n";

    choc::threading::SpinLock spinLock;
    std::atomic<int> sharedCounter{0};
    std::vector<std::thread> threads;
    constexpr int numThreads = 4;
    constexpr int incrementsPerThread = 1000;

    auto workerFunction = [&](int threadId)
    {
        for (int i = 0; i < incrementsPerThread; ++i)
        {
            // Critical section protected by spin lock
            {
                std::scoped_lock lock (spinLock);
                int current = sharedCounter.load();
                // Simulate some work
                std::this_thread::sleep_for (std::chrono::microseconds (1));
                sharedCounter.store (current + 1);
            }
        }

        std::cout << "Thread " << threadId << " completed\n";
    };

    auto startTime = std::chrono::steady_clock::now();

    // Start worker threads
    for (int i = 0; i < numThreads; ++i)
        threads.emplace_back (workerFunction, i);

    // Wait for all threads to complete
    for (auto& t : threads)
        t.join();

    auto endTime = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

    std::cout << "SpinLock test completed in " << duration.count() << "ms\n";
    std::cout << "Expected: " << (numThreads * incrementsPerThread) << ", Actual: " << sharedCounter.load() << "\n";
    std::cout << "Result: " << (sharedCounter.load() == numThreads * incrementsPerThread ? "PASS" : "FAIL") << "\n";
}

void demonstrateThreadSafeFunctor()
{
    std::cout << "\n=== Thread-Safe Coordination Demo ===\n";

    std::atomic<int> callCount{0};
    std::mutex functorMutex;
    std::function<void()> safeFunctor;

    // Set the functor to a lambda with manual synchronization
    {
        std::lock_guard<std::mutex> lock (functorMutex);
        safeFunctor = [&callCount]()
        {
            callCount.fetch_add (1);
            std::cout << "Thread-safe function called #" << callCount.load() << "\n";
            std::this_thread::sleep_for (std::chrono::milliseconds (100));
        };
    }

    // Call it from multiple threads
    std::vector<std::thread> threads;

    for (int i = 0; i < 3; ++i)
    {
        threads.emplace_back ([&]()
        {
            std::cout << "Thread " << i << " calling function...\n";
            std::lock_guard<std::mutex> lock (functorMutex);
            if (safeFunctor)
                safeFunctor();
        });
    }

    // Wait for threads to complete
    for (auto& t : threads)
        t.join();

    // Change the functor while demonstrating thread safety
    std::cout << "Changing function implementation safely...\n";
    {
        std::lock_guard<std::mutex> lock (functorMutex);

        safeFunctor = [&callCount]()
        {
            callCount.fetch_add (1);
            std::cout << "NEW thread-safe function implementation called #" << callCount.load() << "\n";
        };
    }

    // Call the new implementation
    {
        std::lock_guard<std::mutex> lock (functorMutex);

        if (safeFunctor)
            safeFunctor();
    }

    std::cout << "Thread-safe function was called " << callCount.load() << " times total\n";
}

void demonstrateSingleReaderSingleWriterFIFO()
{
    std::cout << "\n=== SingleReaderSingleWriterFIFO Demo ===\n";

    choc::fifo::SingleReaderSingleWriterFIFO<Task> fifo;
    fifo.reset (10); // 10 item capacity

    std::atomic<bool> shouldStop{false};
    std::atomic<int> tasksProduced{0};
    std::atomic<int> tasksConsumed{0};

    // Producer thread
    std::thread producer ([&]()
    {
        int taskID = 0;

        while (! shouldStop.load())
        {
            ++taskID;
            Task task (taskID, "Single writer task " + std::to_string (taskID));

            if (fifo.push (task))
            {
                tasksProduced.fetch_add (1);
                std::cout << "Produced task " << task.id << "\n";
            }
            else
            {
                std::cout << "FIFO full, couldn't produce task " << task.id << "\n";
            }

            std::this_thread::sleep_for (std::chrono::milliseconds (100));
        }
    });

    // Consumer thread
    std::thread consumer ([&]()
    {
        Task task;

        while (! shouldStop.load() || fifo.getUsedSlots() > 0)
        {
            if (fifo.pop (task))
            {
                tasksConsumed.fetch_add (1);
                auto now = std::chrono::steady_clock::now();
                auto latency = std::chrono::duration_cast<std::chrono::milliseconds>(now - task.timestamp);
                std::cout << "Consumed task " << task.id << " (" << task.description << ") - latency: " << latency.count() << "ms\n";
            }

            std::this_thread::sleep_for (std::chrono::milliseconds (150));
        }
    });

    // Let it run for a while
    std::this_thread::sleep_for (std::chrono::seconds (2));
    shouldStop.store (true);

    producer.join();
    consumer.join();

    std::cout << "FIFO Demo completed - Produced: " << tasksProduced.load()
              << ", Consumed: " << tasksConsumed.load() << "\n";
}

void demonstrateSingleReaderMultipleWriterFIFO()
{
    std::cout << "\n=== SingleReaderMultipleWriterFIFO Demo ===\n";

    choc::fifo::SingleReaderMultipleWriterFIFO<Task> fifo;
    fifo.reset (20); // 20 item capacity

    std::atomic<bool> shouldStop{false};
    std::atomic<int> tasksProduced{0};
    std::atomic<int> tasksConsumed{0};

    // Multiple producer threads
    std::vector<std::thread> producers;

    for (int producerId = 0; producerId < 3; ++producerId)
    {
        producers.emplace_back ([&, producerId]()
        {
            int taskID = 0;

            while (! shouldStop.load())
            {
                ++taskID;

                Task task (producerId * 1000 + taskID,
                           "Producer " + std::to_string (producerId) + " task " + std::to_string (taskID));

                if (fifo.push (task))
                {
                    tasksProduced.fetch_add (1);
                    std::cout << "Producer " << producerId << " produced task " << task.id << "\n";
                }
                else
                {
                    std::cout << "FIFO full, producer " << producerId << " couldn't produce task " << task.id << "\n";
                }

                std::this_thread::sleep_for (std::chrono::milliseconds (200 + producerId * 50));
            }
        });
    }

    // Single consumer thread
    std::thread consumer ([&]()
    {
        Task task;
        while (! shouldStop.load() || fifo.getUsedSlots() > 0)
        {
            if (fifo.pop (task))
            {
                tasksConsumed.fetch_add (1);
                auto now = std::chrono::steady_clock::now();
                auto latency = std::chrono::duration_cast<std::chrono::milliseconds>(now - task.timestamp);
                std::cout << "Consumed task " << task.id << " (" << task.description << ") - latency: " << latency.count() << "ms\n";
            }

            std::this_thread::sleep_for (std::chrono::milliseconds (100));
        }
    });

    // Let it run for a while
    std::this_thread::sleep_for (std::chrono::seconds (3));
    shouldStop.store (true);

    for (auto& producer : producers)
        producer.join();

    consumer.join();

    std::cout << "Multiple Writer FIFO Demo completed - Produced: " << tasksProduced.load()
              << ", Consumed: " << tasksConsumed.load() << "\n";
}

void demonstrateMultipleReaderMultipleWriterFIFO()
{
    std::cout << "\n=== MultipleReaderMultipleWriterFIFO Demo ===\n";

    choc::fifo::MultipleReaderMultipleWriterFIFO<Task> fifo;
    fifo.reset (30); // 30 item capacity

    std::atomic<bool> shouldStop{false};
    std::atomic<int> tasksProduced{0};
    std::atomic<int> tasksConsumed{0};

    // Multiple producer threads
    std::vector<std::thread> producers;

    for (int producerId = 0; producerId < 2; ++producerId)
    {
        producers.emplace_back ([&, producerId]()
        {
            int taskID = 0;

            while (! shouldStop.load())
            {
                ++taskID;

                Task task (producerId * 1000 + taskID,
                           "Producer " + std::to_string (producerId) + " task " + std::to_string (taskID));

                if (fifo.push (task))
                {
                    tasksProduced.fetch_add (1);
                    std::cout << "Producer " << producerId << " produced task " << task.id << "\n";
                }

                std::this_thread::sleep_for (std::chrono::milliseconds (150));
            }
        });
    }

    // Multiple consumer threads
    std::vector<std::thread> consumers;

    for (int consumerId = 0; consumerId < 2; ++consumerId)
    {
        consumers.emplace_back ([&, consumerId]()
        {
            Task task;

            while (! shouldStop.load() || fifo.getUsedSlots() > 0)
            {
                if (fifo.pop (task))
                {
                    tasksConsumed.fetch_add (1);
                    auto now = std::chrono::steady_clock::now();
                    auto latency = std::chrono::duration_cast<std::chrono::milliseconds>(now - task.timestamp);
                    std::cout << "Consumer " << consumerId << " consumed task " << task.id
                              << " (" << task.description << ") - latency: " << latency.count() << "ms\n";
                }

                std::this_thread::sleep_for (std::chrono::milliseconds (120 + consumerId * 30));
            }
        });
    }

    // Let it run for a while
    std::this_thread::sleep_for (std::chrono::seconds (3));
    shouldStop.store (true);

    for (auto& producer : producers)
        producer.join();

    for (auto& consumer : consumers)
        consumer.join();

    std::cout << "Multiple Reader/Writer FIFO Demo completed - Produced: " << tasksProduced.load()
              << ", Consumed: " << tasksConsumed.load() << "\n";
}

void demonstrateRealTimeAudioPattern()
{
    std::cout << "\n=== Real-Time Audio Pattern Demo ===\n";

    // Simulate a real-time audio callback pattern
    constexpr int sampleRate = 44100;
    constexpr int bufferSize = 512;
    constexpr int numChannels = 2;

    choc::fifo::SingleReaderSingleWriterFIFO<std::vector<float>> audioFIFO;
    audioFIFO.reset (8); // 8 buffer capacity

    std::atomic<bool> isRunning{true};
    std::atomic<int> buffersProcessed{0};
    std::atomic<int> underruns{0};

    // Audio generator thread (simulates audio callback)
    std::thread audioThread ([&]()
    {
        std::random_device rd;
        std::mt19937 gen (rd());
        std::uniform_real_distribution<float> dist (-0.1f, 0.1f);

        while (isRunning.load())
        {
            std::vector<float> audioBuffer (bufferSize * numChannels);

            // Generate some audio data (white noise)
            for (auto& sample : audioBuffer)
                sample = dist (gen);

            if (audioFIFO.push (std::move (audioBuffer)))
            {
                buffersProcessed.fetch_add (1);
            }
            else
            {
                underruns.fetch_add (1);
                std::cout << "Audio buffer underrun!\n";
            }

            // Simulate audio callback timing
            std::this_thread::sleep_for (std::chrono::microseconds ((bufferSize * 1000000) / sampleRate));
        }
    });

    // Audio processing thread
    std::thread processingThread ([&]()
    {
        std::vector<float> buffer;

        while (isRunning.load() || audioFIFO.getUsedSlots() > 0)
        {
            if (audioFIFO.pop (buffer))
            {
                // Simulate audio processing (calculate RMS)
                float rms = 0.0f;

                for (float sample : buffer)
                    rms += sample * sample;

                rms = std::sqrt (rms / buffer.size());

                static int processedCount = 0;

                if (++processedCount % 100 == 0)
                    std::cout << "Processed " << processedCount << " audio buffers, RMS: "
                              << std::fixed << std::setprecision (6) << rms << "\n";
            }

            // Simulate processing time
            std::this_thread::sleep_for (std::chrono::microseconds (100));
        }
    });

    // Let it run for a while
    std::this_thread::sleep_for (std::chrono::seconds (2));
    isRunning.store (false);

    audioThread.join();
    processingThread.join();

    std::cout << "Real-time audio pattern completed:\n";
    std::cout << "  Buffers processed: " << buffersProcessed.load() << "\n";
    std::cout << "  Underruns: " << underruns.load() << "\n";
    std::cout << "  Success rate: " << (100.0 * buffersProcessed.load() /
                                       (buffersProcessed.load() + underruns.load())) << "%\n";
}

int main()
{
    std::cout << "CHOC Advanced Threading Patterns Example\n";
    std::cout << "========================================\n";

    try
    {
        demonstrateTaskThread();
        demonstrateSpinLock();
        demonstrateThreadSafeFunctor();
        demonstrateSingleReaderSingleWriterFIFO();
        demonstrateSingleReaderMultipleWriterFIFO();
        demonstrateMultipleReaderMultipleWriterFIFO();
        demonstrateRealTimeAudioPattern();

        std::cout << "\n=== All threading demonstrations completed successfully! ===\n";
    }
    catch (const std::exception& e)
    {
        std::cout << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}