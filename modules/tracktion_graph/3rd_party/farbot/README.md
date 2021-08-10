# farbot
FAbian's Realtime Box o' Tricks

This is a collection of design patterns which can be used in realtime code. This library was introduced to the public at meeting C++ on 16th of November 2019.

This library and the containing design patterns were presented at meeting C++ 2019. You can find the slides here: https://hogliux.github.io/farbot/presentations/meetingcpp_2019/index.html

It contains the following realtime design patterns:

RealtimeObject
--------------

`RealtimeObject<T, RealtimeObjectOptions>` is a templated class with which you can share data of type T between non-realtime threads and a single realtime thread. Depending on the non-type template parameter option `RealtimeObjectOptions` either only the **realtime** or the **non-realtime** thread may mutate the data.

You can use it liked this:

```
struct BiquadCoeffecients  {  float b0, b1, b2, a1, a2; };
RealtimeObject<BiquadCoeffecients, RealtimeObjectOptions::nonRealtimeMutatable> biquadCoeffs;

/* called on realtime thread */
void processAudio (float* buffer)
{
    RealtimeObject<BiquadCoeffecients, RealtimeObjectOptions::nonRealtimeMutatable>::ScopedAccess<ThreadType::realtime> coeffs(biquadCoeffs);
    processBiquad (*coeffs, buffer);
}

/* called on non-realtime thread */
void changeBiquadParameters (BiquadCoeffecients newCoeffs)
{
    RealtimeObject<BiquadCoeffecients, RealtimeObjectOptions::nonRealtimeMutatable>::ScopedAccess<ThreadType::nonRealtime> coeffs(biquadCoeffs);
    *coeffs = newCoeffs;
}
```
where the `ThreadType::Realtime`/`ThreadType::nonRealtime` template parameter of the `ScopedAccess` class indicates if the thread requiring access to the data is a realtime thread or not.

Here is an example where only the **realtime** thread can mutate the data:

```
using FrequencySpectrum = std::array<float, 512>;
RealtimeObject<FrequencySpectrum, RealtimeObjectOptions::realtimeMutatable> mostRecentSpectrum;

/* called on realtime thread */
void processAudio (const float* buffer, size_t n) {
    RealtimeObject<FrequencySpectrum, RealtimeObjectOptions::realtimeMutatable>::ScopedAccess<ThreadType::realtime> freqSpec(mostRecentSpectrum);
    *freqSpec = calculateSpectrum (buffer, n);
}

/* called on non-realtime thread */
void updateSpectrumUIButtonClicked() {
    RealtimeObject<FrequencySpectrum, RealtimeObjectOptions::realtimeMutatable>::ScopedAccess<ThreadType::nonRealtime> recentSpectrum(mostRecentSpectrum);
    displaySpectrum(*recentSpectrum);
}
```

fifo
----

`fifo` is a versatile realtime-safe ringbuffer class which supports various types of fifos: you can choose the consumer/producer to either be accessed from a single or multiple thread. This need not be the same for the consumer and producer, for example, you could have a multi-producer, single-consumer fifo. This is controlled by the `farbot::fifo_options::concurrency::single`/`farbot::fifo_options::concurrency::multiple` template parameter.

In addition, you can also choose what happens on an underrun (during a pop) or an overrun (during a push). A fifo with `farbot::fifo_options::full_empty_failure_mode::return_false_on_full_or_empty` will return `false` on a push/pop if the fifo is full/empty respectively. A fifo with `farbot::fifo_options::full_empty_failure_mode::overwrite_or_return_default` will overwrite on full (pop) or return a default constructed element on empty (pop). Again, this option can be chosen independently for the consumer or producer. Note, that with the `farbot::fifo_options::full_empty_failure_mode::overwrite_or_return_default` option the **ordering of the FIFO is lost** when overrunning or underruning, i.e. newer elements may be returned before older elements in this case.

The `fifo` will never lock nor block. Additionally, depending on the above options the push/pop operation may be wait-free: if the consumer/producer is accessed from only a single thread *or* the consumer/producer uses `overwrite_or_return_default` then the pop/push will be wait-free respectively. Otherwise the perticular (i.e. push or pop) operation will not be wait-free.

Usage:

```
fifo<std::function<void()>*,
                  fifo_options::concurrency::single,
                  fifo_options::concurrency::multiple, 
                  fifo_options::full_empty_failure_mode::return_false_on_full_or_empty,
                  fifo_options::full_empty_failure_mode::overwrite_or_return_default> my_fifo; // <- this fifo is wait-free on push and pop

fifo.push (mylambda);
std::function<void()>* almabda;
fifo.pop (alambda);
```

AsyncCaller
-----------
AsyncCaller is a class which contains a method called `callAsync` with which a lambda can be deferred to be processed on a non-realtime thread. This is useful to be able to execute potential non-realtime safe code on a realtime thread (like logging, or deallocations, ...).

Realtime traits
---------------
The farbot library also contains very limited type traits to check if a specefic type is realtime moveable/copyable. Currently this only works for trivially movable/copyable and a few STL containers.

`farbot::is_realtime_copy_assignable`, `farbot::is_realtime_copy_constructable`
`farbot::is_realtime_move_assignable`, `farbot::is_realtime_move_constructable`
