#pragma once
#include <functional>
#include "fifo.hpp"

namespace farbot
{
/** AsyncCaller
 * 
 *  Dispatches lambdas on a non-realtime thread. If caller_concurrency is 
 *  fifo_options::concurrency::single then callAsync is wait- and block-free otherwise
 *  it is only block-free (there is a very small chance it may spin if two realtime threads
 *  call callAsync at the same time).
 * 
 *  The non-realtime thread must call process() to process the lambdas.
 */
template <fifo_options::concurrency caller_concurrency = fifo_options::concurrency::multiple>
class AsyncCaller
{
public:
    AsyncCaller (int fifoCapacity = 512) : ringbuffer (fifoCapacity) {}

    /** Defer the execution of lambda onto a non-realtime thread.
     * 
     * This is a useful method to execute non-realtime-safe lambdas from a realtime thread
     *  (for example logging, deallocations etc.). The execution of the lambda will be deferred
     *  and executed on the thread which calls the process method below.
     * 
     *  NOTE: callAsync is only realtime-safe if moving your lambda is block-free and wait-free.
     * For example, if your lambda captures any objects with blocking or non wait-free
     * move operators then this call will not be realtime-safe.
     * 
     * Return false if there was not enough room in the underlying fifo.
     */
    bool callAsync (std::function<void()> && lambda)
    {
        return ringbuffer.push (std::move (lambda));
    }

    /** Process all the lambdas that have been deferred with callAsync
     * 
     *  Call this from your non-realtime thread to process lambdas. Note that
     *  you must periodically execute this function as the non-realtime thread
     *  will not signal for you to wake up.
     * 
     *  NOTE: process may only be called from a single thread.
     * 
     *  Returns false if no lambdas were processed.
     */
    bool process()
    {
        auto didProcess = false;
        std::function<void()> lambda;

        while (ringbuffer.pop (lambda))
        {
            didProcess = true;

            if (lambda)
                lambda();
        }

        return didProcess;
    }
private:
    fifo<std::function<void()>, fifo_options::concurrency::single, caller_concurrency> ringbuffer;
};
}