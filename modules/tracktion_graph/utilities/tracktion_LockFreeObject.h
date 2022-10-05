/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#pragma once

namespace tracktion { inline namespace graph
{

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
        static_assert (std::is_default_constructible_v<ObjectType>);
        static_assert (std::is_nothrow_move_assignable_v<ObjectType>);
    }

    /** Clears the object and any pending object by assigining them defaultly constructed objects. */
    void clear()
    {
        // Obtain the locks for both the objects
        std::scoped_lock sl (clearObjectsMutex);
        std::scoped_lock sl2 (pushingObjectMutex);

        pendingObjectStorage = {};
        objectStorage = {};

        pendingObject = nullptr;
    }

    /** Pushes a new object to be picked up on the real time thread. */
    void pushNonRealTime (ObjectType&& newObj)
    {
        // Obtain the lock on the pending object
        std::scoped_lock sl (pushingObjectMutex);

        pendingObjectStorage = std::move (newObj);
        pendingObject = &pendingObjectStorage;
    }

    /** Retains the object for use in a real time thread.
        If a previous push call has finished, this will update and use the newly pushed object.
        If a clear call is in progress this will return nullptr.

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

            // Move any penidng object in to the main objectStorage
            if (auto newObject = pendingObject.exchange (nullptr))
                std::swap (objectStorage, *newObject);
        }
        else
        {
            needToUnlockPushingObjectMutex = false;
        }

        // Then return the main stored object
        return &objectStorage;
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
    ObjectType objectStorage, pendingObjectStorage;
    std::atomic<ObjectType*> pendingObject { nullptr };
    RealTimeSpinLock pushingObjectMutex, clearObjectsMutex;
    bool needToUnlockPushingObjectMutex = false, needToUnlockClearObjectsMutex = true;
};

}} // namespace tracktion_engine
