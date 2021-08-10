#pragma once
#include <vector>
#include <atomic>
#include <cassert>
#include <thread>

namespace farbot
{
namespace detail { template <typename, bool, bool, bool, bool, std::size_t> class fifo_impl; }

namespace fifo_options
{
enum class concurrency
{
    single,                  // single consumer/single producer
    multiple
};

enum class full_empty_failure_mode
{
    // Setting this as producer option causes the fifo to overwrite on a push when the fifo is full
    // Setting this a s consumer option causes the fifo to return a default constructed value on pop when the fifo is empty
    overwrite_or_return_default,

    // Return false on push/pop if the fifo is full/empty respectively
    return_false_on_full_or_empty
};
}

// single consumer, single producer
template <typename T,
          fifo_options::concurrency consumer_concurrency = fifo_options::concurrency::multiple,
          fifo_options::concurrency producer_concurrency = fifo_options::concurrency::multiple,
          fifo_options::full_empty_failure_mode consumer_failure_mode = fifo_options::full_empty_failure_mode::return_false_on_full_or_empty,
          fifo_options::full_empty_failure_mode producer_failure_mode = fifo_options::full_empty_failure_mode::return_false_on_full_or_empty,
          std::size_t MAX_THREADS = 64>
class fifo
{
public:
    fifo (int capacity);

    bool push(T&& result);

    bool pop(T& result);

private:
    detail::fifo_impl<T,
                      consumer_concurrency == fifo_options::concurrency::single,
                      producer_concurrency == fifo_options::concurrency::single,
                      consumer_failure_mode == fifo_options::full_empty_failure_mode::overwrite_or_return_default,
                      producer_failure_mode == fifo_options::full_empty_failure_mode::overwrite_or_return_default,
                      MAX_THREADS> impl;
};
}

#include "detail/fifo.tcc"