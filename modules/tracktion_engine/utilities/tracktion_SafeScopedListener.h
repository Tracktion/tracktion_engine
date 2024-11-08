/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion { inline namespace engine
{

template<typename T>
concept WeakReferenceable = requires (T x)
{
    { ! std::is_pointer_v<T> };
    { x.operator->() };
    { static_cast<bool> (x) };
    { std::is_copy_constructible_v<T> };
};

/**
    Manages adding and removing listeners in an RAII way so you don't forget to unregister a listener.

    This is similar to a ScopedListener except that BroadcasterType is expected to be some kind of
    safe-pointer that is boolean checkable. This way, you can't forget to remove a registered listener
    and if the broadcaster gets deleted, this won't attempt to unregister itself and thus avoids a dangling reference.

    The BroadcasterType must have the following functions:
    BroadcasterType::addListener (ListenerType*);
    BroadcasterType::removeListener (ListenerType*);

    @code
    // In members section
    SafeScopedListener listener;

    // Then when you want to register the listener
    listener.reset (broadcasterSafePtr, *this);

    // Listener will automatically be unregistered when the SafeScopedListener goes out of scope but you can manually remove the listener as follows:
    listener.reset();
    @endcode
*/
class SafeScopedListener
{
public:
    /** Creates an empty ScopedListener. */
    SafeScopedListener() = default;

    /** Move-constructor. */
    SafeScopedListener (SafeScopedListener&& o)
    {
        pimpl = std::move (o.pimpl);
    }

    /** Move-assignment. */
    SafeScopedListener& operator= (SafeScopedListener&& o)
    {
        pimpl = std::move (o.pimpl);
        return *this;
    }

    /** Creates a ScopedListener which makes ListenerType listen to BroadcasterType. */
    template<typename BroadcasterType, typename ListenerType>
        requires WeakReferenceable<BroadcasterType>
    SafeScopedListener (BroadcasterType b, ListenerType& l)
    {
        reset (std::move (b), l);
    }

    /** Unregisters any previous listener and registers the new one. */
    template<typename BroadcasterType, typename ListenerType>
        requires WeakReferenceable<BroadcasterType>
    void reset (BroadcasterType b, ListenerType& l)
    {
        pimpl = std::make_any<std::shared_ptr<SafeScopedListenerImpl<BroadcasterType, ListenerType>>>
                    (std::make_shared<SafeScopedListenerImpl<BroadcasterType, ListenerType>> (std::move (b), l));
    }

    /** Unregisters the listener. */
    void reset()
    {
        pimpl = {};
    }

    /** Non-copyable. */
    SafeScopedListener (const SafeScopedListener&) = delete;
    /** Non-copyable. */
    SafeScopedListener& operator= (const SafeScopedListener&) = delete;

 private:
    template<typename BroadcasterType, typename ListenerType>
    class SafeScopedListenerImpl
    {
    public:
        SafeScopedListenerImpl (BroadcasterType broadcaster_, ListenerType& listener_)
            : broadcaster (std::move (broadcaster_)), listener (listener_)
        {
            if (broadcaster)
                broadcaster->addListener (&listener);
        }

        ~SafeScopedListenerImpl()
        {
            if (broadcaster)
                broadcaster->removeListener (&listener);
        }

    private:
        BroadcasterType broadcaster;
        ListenerType& listener;
    };

    std::any pimpl;
};

}} // namespace tracktion { inline namespace engine
