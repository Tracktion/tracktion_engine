/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#pragma once

namespace tracktion::inline graph {

//==============================================================================
//==============================================================================
/**
    Manages access to an object in a way that means it is lock-free to access
    from a real-time thread.

    This initially starts empty so call pushNonRealTime to queue an object. You
    can then get at this object using retainRealTime.
    It's thread safe to call pushNonRealTime as many times as you like, retainRealTime
    will just return the old object whilst those calls are in progress.
    Calls to pushNonRealTime may have to wait for the real time access to complete,
    signified by a call to releaseRealTime.

    Objects are stored on the heap so they have a stable identity: once an object
    has been swapped in by retainRealTime, its address never changes and its
    contents are never written by this class again. When a subsequent push is
    consumed on the real-time thread, the previous object is retired rather than
    mutated, and ownership of it is handed back to the caller from the next
    pushNonRealTime call so it can be destroyed once any concurrent readers of
    it have finished.

    Additionally, you may want to clear the objects e.g. releasing some resource
    they have stored. This can be done with the clear call.
    Whilst this is happening, retainRealTime will still be lock-free but will
    return nullptr signifying no object can be used.

    @see ScopedRealTimeAccess
*/
template<typename ObjectType>
class LockFreeObject
{
public:
    /** Constructs an initially empty object.
        Use the pushNonRealTime function to queue one for real-time access.
    */
    LockFreeObject()
    {
        static_assert (std::is_move_constructible_v<ObjectType>);
    }

    /** Clears the current and any pending object.
        N.B. This destroys the objects so only call it once no other threads can
        be concurrently reading them (including any object previously swapped in
        by retainRealTime whose pointer readers may still hold).
    */
    void clear()
    {
        // Obtain the locks for both the objects
        std::scoped_lock sl (clearObjectsMutex);
        std::scoped_lock sl2 (pushingObjectMutex);

        pendingObjectStorage.reset();
        objectStorage.reset();

        pendingObject = nullptr;
    }

    /** Pushes a new object to be picked up on the real time thread.
        Returns the object this replaces which will either be a previously
        pushed object that was never swapped in on the real-time thread, or an
        object that has been retired by a retainRealTime call swapping in a
        newer one. In the latter case, other threads may still be reading the
        retired object if they obtained its pointer before the swap, so callers
        must wait for those readers to finish before destroying it.
    */
    [[nodiscard]] std::unique_ptr<ObjectType> pushNonRealTime (ObjectType&& newObj)
    {
        // Allocate the new storage before taking the lock as retainRealTime
        // will spin on it from the real-time thread
        auto newStorage = std::make_unique<ObjectType> (std::move (newObj));

        std::unique_ptr<ObjectType> replacedObject;

        {
            // Obtain the lock on the pending object
            std::scoped_lock sl (pushingObjectMutex);

            replacedObject = std::move (pendingObjectStorage);
            pendingObjectStorage = std::move (newStorage);
            pendingObject = &pendingObjectStorage;
        }

        return replacedObject;
    }

    /** Retains the object for use in a real time thread.
        If a previous push call has finished, this will update and use the newly pushed object.
        If a clear call is in progress, or no object has been pushed yet, this
        will return nullptr.

        This must be matched with a corresponding call to releaseRealTime(). To Ensure this,
        use the ScopedRealTimeAccess helper class.
    */
    ObjectType* retainRealTime()
    {
        if (clearObjectsMutex.try_lock())
        {
            needToUnlockClearObjectsMutex = true;
        }
        else
        {
            needToUnlockClearObjectsMutex = false;
            return nullptr;
        }

        // If we get the lock, we can update
        if (pushingObjectMutex.try_lock())
        {
            needToUnlockPushingObjectMutex = true;

            // Swap any pending object in to use, retiring the current one.
            // N.B. This only swaps the owning pointers, the objects themselves
            // don't move so pointers to them held by other threads stay valid
            if (auto newObject = pendingObject.exchange (nullptr))
                std::swap (objectStorage, *newObject);
        }
        else
        {
            needToUnlockPushingObjectMutex = false;
        }

        // Then return the main stored object
        return objectStorage.get();
    }

    /** Releases the use of the object from a previous call to retainRealTime. */
    void releaseRealTime()
    {
        if (needToUnlockClearObjectsMutex)
            clearObjectsMutex.unlock();

        if (needToUnlockPushingObjectMutex)
            pushingObjectMutex.unlock();
    }

    //==============================================================================
    /**
        Helper class to automatically retain/release real time access to an object.
    */
    class ScopedRealTimeAccess
    {
    public:
        /** Retains real time access to an object. */
        ScopedRealTimeAccess (LockFreeObject& lfo)
            : lockFreeObject (lfo)
        {}

        /** Releases real time access to the object. */
        ~ScopedRealTimeAccess()
        {
            lockFreeObject.releaseRealTime();
        }

        /** Returns a pointer to the object if access was obtained. */
        ObjectType* get() const
        {
            return object;
        }

    private:
        LockFreeObject& lockFreeObject;
        ObjectType* object { lockFreeObject.retainRealTime() };
    };

    /** Creates a ScopedRealTimeAccess for this LockFreeObject. */
    ScopedRealTimeAccess getScopedAccess()
    {
        return ScopedRealTimeAccess { *this };
    }

private:
    std::unique_ptr<ObjectType> objectStorage, pendingObjectStorage;
    std::atomic<std::unique_ptr<ObjectType>*> pendingObject { nullptr };
    RealTimeSpinLock pushingObjectMutex, clearObjectsMutex;
    bool needToUnlockPushingObjectMutex = false, needToUnlockClearObjectsMutex = true;
};

} // namespace tracktion::inline graph
