#pragma once

#include <atomic>
#include <memory>
#include <mutex>

#include "detail/RealtimeObject.tcc"

namespace farbot
{
//==============================================================================
enum class RealtimeObjectOptions
{
    nonRealtimeMutatable,
    realtimeMutatable
};

enum class ThreadType
{
    realtime,
    nonRealtime
};

//==============================================================================
/** Useful class to synchronise access to an object from multiple threads with the additional feature that one
 * designated thread will never wait to get access to the object. */
template <typename T, RealtimeObjectOptions Options> 
class RealtimeObject
{
public:
    /** Creates a default constructed T */
    RealtimeObject() = default;

    /** Creates a copy of T */
    explicit RealtimeObject(const T & obj) : mImpl(obj) {}

    /** Moves T into this realtime wrapper */
    explicit RealtimeObject(T && obj) : mImpl(std::move(obj)) {}

    ~RealtimeObject() = default;

    /** Create T by calling T's constructor which takes args */
    template <typename... Args>
    static RealtimeObject create(Args && ... args)    { return Impl::create(std::forward<Args>(args)...); }

    //==============================================================================
    using RealtimeAcquireReturnType    = std::conditional_t<Options == RealtimeObjectOptions::nonRealtimeMutatable, const T, T>;
    using NonRealtimeAcquireReturnType = std::conditional_t<Options == RealtimeObjectOptions::realtimeMutatable,    const T, T>;

    //==============================================================================
    /** Returns a reference to T. Use this method on the real-time thread.
     *  It must be matched by realtimeRelease when you are finished using the 
     *  object. Alternatively, use the ScopedAccess helper class below.
     * 
     *  Only a single real-time thread can acquire this object at once!
     * 
     *  This method is wait- and lock-free.
     */
    RealtimeAcquireReturnType& realtimeAcquire() noexcept     { return mImpl.realtimeAcquire(); }

    /** Releases the lock on T previously acquired by realtimeAcquire.
     * 
     *  This method is wait- and lock-free.
     */
    void realtimeRelease() noexcept                           { mImpl.realtimeRelease(); }

    /** Replace the underlying value with a new instance of T by forwarding
     *  the method's arguments to T's constructor
     */
    template <RealtimeObjectOptions O = Options, typename... Args>
    std::enable_if_t<O == RealtimeObjectOptions::realtimeMutatable, std::void_t<Args...>>
    realtimeReplace(Args && ... args) noexcept               { mImpl.realtimeReplace(std::forward<Args>(args)...); }

    //==============================================================================
    /** Returns a reference to T. Use this method on the non real-time thread.
     *  It must be matched by nonRealtimeRelease when you are finished using
     *  the object. Alternatively, use the ScopedAccess helper class below.
     * 
     *  Multiple non-realtime threads can acquire an object at the same time.
     * 
     *  This method uses a lock should not be used on a realtime thread.
     */ 
    NonRealtimeAcquireReturnType& nonRealtimeAcquire()       { return mImpl.nonRealtimeAcquire(); }

    /** Releases the lock on T previously acquired by nonRealtimeAcquire.
     * 
     *  This method uses both a lock and a spin loop and should not be used
     *  on a realtime thread.
     */
    void nonRealtimeRelease()                                { mImpl.nonRealtimeRelease(); }

    /** Replace the underlying value with a new instance of T by forwarding
     *  the method's arguments to T's constructor
     */
    template <RealtimeObjectOptions O = Options, typename... Args>
    std::enable_if_t<O == RealtimeObjectOptions::nonRealtimeMutatable, std::void_t<Args...>>
    nonRealtimeReplace(Args && ... args)                     { mImpl.nonRealtimeReplace(std::forward<Args>(args)...); }

    //==============================================================================
    /** Instead of calling acquire and release manually, you can also use this RAII
     *  version which calls acquire automatically on construction and release when
     *  destructed.
     */
    template <ThreadType threadType>
    class ScopedAccess    : public std::conditional_t<Options == RealtimeObjectOptions::realtimeMutatable,
                                                      detail::RealtimeMutatable<T>,
                                                      detail::NonRealtimeMutatable<T>>::template ScopedAccess<threadType == ThreadType::realtime>
    {
    public:
        explicit ScopedAccess (RealtimeObject& parent)
            : Impl::template ScopedAccess<threadType == ThreadType::realtime> (parent.mImpl) {}

       #if DOXYGEN
        // Various ways to get access to the underlying object.
        // Non-const method are only supported on the realtime
        // or non-realtime thread as indicated by the Options
        // template argument
        T* get() noexcept;
        const T* get() const noexcept;
        T &operator *() noexcept;
        const T &operator *() const noexcept;
        T* operator->() noexcept;
        const T* operator->() const noexcept;
       #endif

        //==============================================================================
        ScopedAccess(const ScopedAccess&) = delete;
        ScopedAccess(ScopedAccess &&) = delete;
        ScopedAccess& operator=(const ScopedAccess&) = delete;
        ScopedAccess& operator=(ScopedAccess&&) = delete;
    };
private:
    using Impl = std::conditional_t<Options == RealtimeObjectOptions::realtimeMutatable,
                       detail::RealtimeMutatable<T>,
                       detail::NonRealtimeMutatable<T>>;
    Impl mImpl;
};

}

