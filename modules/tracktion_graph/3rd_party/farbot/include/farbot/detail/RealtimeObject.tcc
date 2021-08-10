#pragma once

#include <cassert>

namespace farbot
{
namespace detail 
{ 
//==============================================================================
template <typename, bool> class NRMScopedAccessImpl;
template <typename T> class NonRealtimeMutatable
{
public:
    NonRealtimeMutatable() : storage (std::make_unique<T>()), pointer (storage.get()) {}

    explicit NonRealtimeMutatable (const T & obj) : storage (std::make_unique<T> (obj)), pointer (storage.get()) {}

    explicit NonRealtimeMutatable (T && obj) : storage (std::make_unique<T> (std::move (obj))), pointer (storage.get()) {}

    ~NonRealtimeMutatable()
    {
        assert (pointer.load() != nullptr); // <- never delete this object while the realtime thread is holding the lock

        // Spin!
        while (pointer.load() == nullptr);

        auto accquired = nonRealtimeLock.try_lock();

        ((void)(accquired));
        assert (accquired);  // <- you didn't call release on one of the non-realtime threads before deleting this object

        nonRealtimeLock.unlock();
    }

    template <typename... Args>
    static NonRealtimeMutatable create(Args && ... args)
    {
        return RealtimeObject (std::make_unique<T> (std::forward<Args>(args)...));
    }

    const T& realtimeAcquire() noexcept
    {
         assert (pointer.load() != nullptr); // <- You didn't balance your acquire and release calls!
        currentObj = pointer.exchange (nullptr);
        return *currentObj;
    }

    void realtimeRelease() noexcept
    {
        // You didn't balance your acquire and release calls
        assert (pointer.load() == nullptr); 

        // replace back
        pointer.store (currentObj);
    }

    T& nonRealtimeAcquire()
    {
        nonRealtimeLock.lock();
        copy.reset (new T (*storage));

        return *copy.get();
    }

    void nonRealtimeRelease()
    {
        T* ptr;

        // block until realtime thread is done using the object
        do {
            ptr = storage.get();
        } while (! pointer.compare_exchange_weak (ptr, copy.get()));

        storage = std::move (copy);
        nonRealtimeLock.unlock();
    }

    template <typename... Args>
    void nonRealtimeReplace(Args && ... args)
    {
        nonRealtimeLock.lock();
        copy.reset (new T (std::forward<Args>(args)...));

        nonRealtimeRelease();
    }

    template <bool isRealtimeThread>
    class ScopedAccess    : public NRMScopedAccessImpl<T, isRealtimeThread>
    {
    public:
        explicit ScopedAccess (NonRealtimeMutatable& parent)
            : NRMScopedAccessImpl<T, isRealtimeThread> (parent) {}
        ScopedAccess(const ScopedAccess&) = delete;
        ScopedAccess(ScopedAccess &&) = delete;
        ScopedAccess& operator=(const ScopedAccess&) = delete;
        ScopedAccess& operator=(ScopedAccess&&) = delete;
    };
private:
    friend class NRMScopedAccessImpl<T, true>;
    friend class NRMScopedAccessImpl<T, false>;
    explicit NonRealtimeMutatable(std::unique_ptr<T> && u);

    std::unique_ptr<T> storage;
    std::atomic<T*> pointer;

    std::mutex nonRealtimeLock;
    std::unique_ptr<T> copy;

    // only accessed by realtime thread
    T* currentObj = nullptr;
};

template <typename T, bool>
class NRMScopedAccessImpl
{
protected:
    NRMScopedAccessImpl (NonRealtimeMutatable<T>& parent)
        : p (parent), currentValue (&p.nonRealtimeAcquire()) {}
    ~NRMScopedAccessImpl() { p.nonRealtimeRelease(); }
public:
    T* get() noexcept                       { return currentValue;  }
    const T* get() const noexcept           { return currentValue;  }
    T &operator *() noexcept                { return *currentValue; }
    const T &operator *() const noexcept    { return *currentValue; }
    T* operator->() noexcept                { return currentValue; }
    const T* operator->() const noexcept    { return currentValue; }
private:
    NonRealtimeMutatable<T>& p;
    T* currentValue;
};

template <typename T>
class NRMScopedAccessImpl<T, true>
{
protected:
    NRMScopedAccessImpl (NonRealtimeMutatable<T>& parent) noexcept
        : p (parent), currentValue (&p.realtimeAcquire()) {}
    ~NRMScopedAccessImpl() noexcept { p.realtimeRelease(); }
public:
    const T* get() const noexcept           { return currentValue;  }
    const T &operator *() const noexcept    { return *currentValue; }
    const T* operator->() const noexcept    { return currentValue; }
private:
    NonRealtimeMutatable<T>& p;
    const T* currentValue;
};

//==============================================================================
//==============================================================================
template <typename, bool> class RMScopedAccessImpl;
template <typename T> class RealtimeMutatable
{
public:
    RealtimeMutatable() = default;

    explicit RealtimeMutatable (const T & obj) : data ({obj, obj}), realtimeCopy (obj) {}

    ~RealtimeMutatable()
    {
        assert ((control.load() & BUSY_BIT) == 0); // <- never delete this object while the realtime thread is still using it

        // Spin!
        while ((control.load() & BUSY_BIT) == 1);

        auto accquired = nonRealtimeLock.try_lock();

        ((void)(accquired));
        assert (accquired);  // <- you didn't call release on one of the non-realtime threads before deleting this object

        nonRealtimeLock.unlock();
    }

    template <typename... Args>
    static RealtimeMutatable create(Args && ... args)
    {
        return RealtimeObject (false, std::forward<Args>(args)...);
    }

    T& realtimeAcquire() noexcept
    {
        return realtimeCopy;
    }

    void realtimeRelease() noexcept
    {
        auto idx = acquireIndex();
        data[idx] = realtimeCopy;
        releaseIndex(idx);
    }

    template <typename... Args>
    void realtimeReplace(Args && ... args)
    {
        T obj(std::forward<Args>(args)...);

        auto idx = acquireIndex();
        data[idx] = std::move(obj);
        releaseIndex(idx);
    }

    const T& nonRealtimeAcquire()
    {
        nonRealtimeLock.lock();
        auto current = control.load(std::memory_order_acquire);

        // there is new data so flip the indices around atomically ensuring we are not inside realtimeAssign
        if ((current & NEWDATA_BIT) != 0)
        {
            int newValue;

            do
            {
                // expect the realtime thread not to be inside the realtime-assign
                current &= ~BUSY_BIT;

                // realtime thread should flip index value and clear the newdata bit
                newValue = (current ^ INDEX_BIT) & INDEX_BIT;
            } while (! control.compare_exchange_weak (current, newValue, std::memory_order_acq_rel));

            current = newValue;
        }

        // flip the index bit as we always use the index that the realtime thread is currently NOT using
        auto nonRealtimeIndex = (current & INDEX_BIT) ^ 1;

        return data[nonRealtimeIndex];
    }

    void nonRealtimeRelease()
    {
        nonRealtimeLock.unlock();
    }

    template <bool isRealtimeThread>
    class ScopedAccess : public RMScopedAccessImpl<T, isRealtimeThread>
    {
    public:
        explicit ScopedAccess (RealtimeMutatable& parent) : RMScopedAccessImpl<T, isRealtimeThread> (parent) {}
        ScopedAccess(const ScopedAccess&) = delete;
        ScopedAccess(ScopedAccess &&) = delete;
        ScopedAccess& operator=(const ScopedAccess&) = delete;
        ScopedAccess& operator=(ScopedAccess&&) = delete;
    };
private:
    friend class RMScopedAccessImpl<T, false>;
    friend class RMScopedAccessImpl<T, true>;
    
    template <typename... Args>
    explicit RealtimeMutatable(bool, Args &&... args)
        : data ({T (std::forward (args)...), T (std::forward (args)...)}), realtimeCopy (std::forward (args)...)
    {}

    enum
    {
        INDEX_BIT = (1 << 0),
        BUSY_BIT = (1 << 1),
        NEWDATA_BIT = (1 << 2)
    };

    int acquireIndex() noexcept
    {
        return control.fetch_or (BUSY_BIT, std::memory_order_acquire) & INDEX_BIT;
    }

    void releaseIndex(int idx) noexcept
    {
        control.store ((idx & INDEX_BIT) | NEWDATA_BIT, std::memory_order_release);
    }

    std::atomic<int> control = {0};

    std::array<T, 2> data;
    T realtimeCopy;

    std::mutex nonRealtimeLock;
};

template <typename T, bool>
class RMScopedAccessImpl
{
protected:
    RMScopedAccessImpl (RealtimeMutatable<T>& parent)
        : p (parent),
          currentValue(&p.realtimeAcquire())
    {}

    ~RMScopedAccessImpl() 
    {
        p.realtimeRelease();
    }
public:
    T* get() noexcept                       { return currentValue;  }
    const T* get() const noexcept           { return currentValue;  }
    T &operator *() noexcept                { return *currentValue; }
    const T &operator *() const noexcept    { return *currentValue; }
    T* operator->() noexcept                { return currentValue; }
    const T* operator->() const noexcept    { return currentValue; }
private:
    RealtimeMutatable<T>& p;
    T* currentValue;
};

template <typename T>
class RMScopedAccessImpl<T, false>
{
protected:
    RMScopedAccessImpl (RealtimeMutatable<T>& parent) noexcept
        : p (parent), currentValue (&p.nonRealtimeAcquire()) {}
    ~RMScopedAccessImpl() noexcept { p.nonRealtimeRelease(); }
public:
    const T* get() const noexcept           { return currentValue;  }
    const T &operator *() const noexcept    { return *currentValue; }
    const T* operator->() const noexcept    { return currentValue; }
private:
    RealtimeMutatable<T>& p;
    const T* currentValue;
};
}
}
