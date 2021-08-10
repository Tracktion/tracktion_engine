#include <gtest/gtest.h>
#include <atomic>
#include <random>
#include <array>
#include <unordered_set>

#include <mutex>
#include <condition_variable>

#include "farbot/RealtimeTraits.hpp"
#include "farbot/fifo.hpp"
#include "farbot/AsyncCaller.hpp"
#include "farbot/RealtimeObject.hpp"

using TestData = std::array<long long, 8>;

static_assert (farbot::is_realtime_move_assignable<TestData>::value);

// ensure that the object could be torn
static_assert(! std::atomic<TestData>::is_always_lock_free);

TestData create (int num)
{
    return TestData {num, num, num, num, num, num, num, num};
}

bool operator==(const TestData& o, int num)
{
     for (auto n : o)
        if (n != num)
            return false;

    return true;
}

TEST(fifo, no_data_test)
{
    farbot::fifo<TestData> fifo (256);

    TestData test; 
    EXPECT_FALSE (fifo.pop (test));
}

TEST(fifo, one_in_one_out_test)
{
    farbot::fifo<TestData> fifo (256);

    EXPECT_TRUE (fifo.push (create (1)));
    TestData test;
    EXPECT_TRUE (fifo.pop (test));
    EXPECT_TRUE (test == 1);
}

TEST(fifo, has_claimed_capacity)
{
    farbot::fifo<TestData> fifo (256);

    for (int i = 0; i < 256; ++i)
        EXPECT_TRUE (fifo.push (create(0)));

    EXPECT_FALSE (fifo.push (create(0)));
}

TEST (fifo, random_push_pop)
{
    farbot::fifo<TestData> fifo (256);

    int writeidx = 1, readidx = 1;
    int read_available = 0, write_available = 256;

    std::default_random_engine generator;

    for (int i = 0; i < 10000; ++i)
    {
        if (write_available > 0)
        {
            std::uniform_int_distribution<int> distribution (0, write_available);
            auto consec_writes = distribution (generator);

            for (int j = 0; j < consec_writes; ++j)
                EXPECT_TRUE (fifo.push (create(writeidx++)));

            write_available -= consec_writes;
            read_available += consec_writes;

            if (write_available == 0)
            {
                EXPECT_FALSE (fifo.push (create(writeidx + 1)));
            }
        }

        if (read_available > 0)
        {
            std::uniform_int_distribution<int> distribution (0, read_available);
            auto consec_reads = distribution (generator);

            for (int j = 0; j < consec_reads; ++j)
            {
                TestData test;

                EXPECT_TRUE (fifo.pop (test));
                EXPECT_TRUE (test == readidx++);
            }

            write_available += consec_reads;
            read_available -= consec_reads;

            if (read_available == 0)
            {
                TestData test;
                EXPECT_FALSE (fifo.pop (test));
            }
        }
    }
}

template <int number_of_reader_threads, int number_of_writer_threads,
          farbot::fifo_options::concurrency consumer_concurrency,
          farbot::fifo_options::concurrency producer_concurrency>
void do_thread_test()
{
    farbot::fifo<TestData, consumer_concurrency, producer_concurrency> fifo (256);
    std::atomic<bool> running = {true};

    constexpr auto highest_write = 1000000;

    std::array<std::unordered_set<long long>, number_of_reader_threads> readValues;
    std::array<std::unique_ptr<std::thread>, number_of_reader_threads> readThreads;
    std::array<std::unique_ptr<std::thread>, number_of_writer_threads> writeThreads;
    
    for (int i = 0; i < number_of_reader_threads; ++i)
    {
        readThreads[i] = 
        std::make_unique<std::thread> ([&fifo, &running, &readValues, i] ()
        {
            auto& values = readValues[i];
            long long lastValue = -1;

            while (running.load (std::memory_order_relaxed))
            {
                TestData test;
                auto success = false;

                while (running.load (std::memory_order_relaxed))
                {
                    success = fifo.pop (test);
                    if (success)
                        break;
                }

                auto value = test[0];
                EXPECT_TRUE (test == static_cast<int> (value));

                if (success)
                {
                    EXPECT_TRUE (values.emplace (value).second);

                    if (number_of_writer_threads == 1)
                    {
                        EXPECT_GT (value, lastValue);
                    }
                    
                    lastValue = value;
                }
            }

            // read remaining data
            {
                TestData test;
                while (fifo.pop (test))
                    EXPECT_TRUE (values.emplace (test[0]).second);
            }
        });
    }

    std::atomic<int> atomic_counter = 1;

    for (int i = 0; i < number_of_writer_threads; ++i)
    {
        writeThreads[i] = 
        std::make_unique<std::thread> ([&fifo, &atomic_counter] ()
        {
            while (true)
            {
                auto value = atomic_counter.fetch_add(1);

                if (value >= highest_write)
                    return;

                while (! fifo.push (create(value)));
            }
        });
    }

    for (int i = 0; i < number_of_writer_threads; ++i)
        writeThreads[i]->join();

    running.store (false, std::memory_order_relaxed);

    for (int i = 0; i < number_of_reader_threads; ++i)
        readThreads[i]->join();

    // ensure each value is picked up by each thread exactly once
    for (int i = 1; i < highest_write; ++i)
    {
        int j;
        for (j = 0; j < number_of_reader_threads; ++j)
        {
            if (readValues[j].find (i) != readValues[j].end())
                break;
        }

        EXPECT_LT (j, number_of_reader_threads);
        for (++j; j < number_of_reader_threads; ++j)
            EXPECT_EQ (readValues[j].find (i), readValues[j].end());
    }
}

TEST (fifo, multi_consumer_single_producer)
{
    do_thread_test<10, 1, farbot::fifo_options::concurrency::multiple, farbot::fifo_options::concurrency::single>();
}

TEST (fifo, multi_consumer_multi_producer)
{
    do_thread_test<10, 10, farbot::fifo_options::concurrency::multiple, farbot::fifo_options::concurrency::multiple>();
}

TEST(fifo, async_caller_test)
{
    std::mutex init_mutex;
    std::condition_variable init_cv;

    std::atomic<bool> finish(false);
    farbot::AsyncCaller<> asyncCaller;

    EXPECT_FALSE (asyncCaller.process());

    std::unique_lock<std::mutex> init_lock (init_mutex);

    std::thread test ([&asyncCaller, &finish, &init_mutex, &init_cv] ()
    {

        asyncCaller.callAsync ([&finish] () { finish.store (true, std::memory_order_relaxed); } );

        // TODO: this is not quite right as this will cause a synchronisation event and doesn't
        // fully test if AsyncCaller is no data races
        {
            std::unique_lock<std::mutex> l (init_mutex);
            init_cv.notify_all();
        }
    });

    init_cv.wait (init_lock);
    EXPECT_TRUE (asyncCaller.process());
    EXPECT_TRUE (finish.load());
    test.join();
}

TEST(RealtimeMutatable, tester)
{
    struct BiquadCoeffecients { 
        BiquadCoeffecients() = default;
        BiquadCoeffecients(float _a1, float _a2, float _b1, float _b2, float _b3) : a1(_a1), a2(_a2), b1(_b1), b2(_b2), b3(_b3) {}
        float a1, a2, b1, b2, b3; } biquads;
    using RealtimeBiquads = farbot::RealtimeObject<BiquadCoeffecients, farbot::RealtimeObjectOptions::nonRealtimeMutatable>;
    RealtimeBiquads realtime(biquads);

    {
        RealtimeBiquads::ScopedAccess<farbot::ThreadType::nonRealtime> scopedAccess(realtime);

        scopedAccess->a1 = 1.0;
        scopedAccess->a2 = 1.4;
    }

    {
        RealtimeBiquads::ScopedAccess<farbot::ThreadType::realtime> scopedAccess(realtime);

        std::cout << scopedAccess->a1 << std::endl;
    }

    realtime.nonRealtimeReplace(1.0, 1.2, 3.4, 5.4, 5.4);
}

TEST(NonRealtimeMutatable, tester)
{
    struct BiquadCoeffecients { 
        BiquadCoeffecients() = default;
        BiquadCoeffecients(float _a1, float _a2, float _b1, float _b2, float _b3) : a1(_a1), a2(_a2), b1(_b1), b2(_b2), b3(_b3) {}
        float a1, a2, b1, b2, b3; } biquads;
    using RealtimeBiquads = farbot::RealtimeObject<BiquadCoeffecients, farbot::RealtimeObjectOptions::realtimeMutatable>;
    RealtimeBiquads realtime(biquads);

    {
        RealtimeBiquads::ScopedAccess<farbot::ThreadType::realtime> scopedAccess(realtime);

        scopedAccess->a1 = 1.0;
        scopedAccess->a2 = 1.4;
    }

    {
        RealtimeBiquads::ScopedAccess<farbot::ThreadType::nonRealtime> scopedAccess(realtime);

        std::cout << scopedAccess->a1 << std::endl;
    }

    realtime.realtimeReplace(1.0, 1.2, 3.4, 5.4, 5.4);
}

TEST(RealtimeMutatable, valueIsPreserved)
{
    using RealtimeHistogram = farbot::RealtimeObject<std::array<int, 256>, farbot::RealtimeObjectOptions::realtimeMutatable>;
    RealtimeHistogram histogram(std::array<int, 256>{});

    {
        RealtimeHistogram::ScopedAccess<farbot::ThreadType::realtime> hist(histogram);

        std::fill(hist->begin(), hist->end(), 0);
    }

    for (int i = 0; i < 100; ++i)
    {
        RealtimeHistogram::ScopedAccess<farbot::ThreadType::realtime> hist(histogram);

        ASSERT_EQ((*hist)[0], i);

        (*hist)[0]++;
    }

    {
        RealtimeHistogram::ScopedAccess<farbot::ThreadType::realtime> hist(histogram);

        std::fill(hist->begin(), hist->end(), 0);
    }

    for (int i = 0; i < 100; ++i)
    {
        {
            RealtimeHistogram::ScopedAccess<farbot::ThreadType::realtime> hist(histogram);

            ASSERT_EQ((*hist)[0], i);

            (*hist)[0]++;
        }

        {
            RealtimeHistogram::ScopedAccess<farbot::ThreadType::nonRealtime> hist(histogram);
            ASSERT_EQ((*hist)[0], (i + 1));
        }
    }
}
