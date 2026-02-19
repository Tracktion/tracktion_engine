#include <iostream>
#include <thread>
#include <vector>
#include <numeric>
#include "../choc/containers/choc_SingleReaderSingleWriterFIFO.h"

int main()
{
    choc::fifo::SingleReaderSingleWriterFIFO<int> fifo;
    fifo.reset (1024);
    std::vector<int> data (100);
    std::iota (data.begin(), data.end(), 0);

    // Producer thread
    std::thread producer ([&]()
    {
        for (int i = 0; i < 100; ++i)
        {
            while (! fifo.push (data[i]))
                std::this_thread::yield();

            std::cout << "Produced: " << data[i] << std::endl;
        }
    });

    // Consumer thread
    std::thread consumer ([&]()
    {
        for (int i = 0; i < 100; ++i)
        {
            int receivedValue;

            while (! fifo.pop (receivedValue))
                std::this_thread::yield();

            std::cout << "Consumed: " << receivedValue << std::endl;
        }
    });

    producer.join();
    consumer.join();

    std::cout << "FIFO example finished." << std::endl;

    return 0;
}
