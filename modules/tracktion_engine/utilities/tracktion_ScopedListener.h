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

/**
    Manages adding and removing listeners in an RAII way so you don't forget to unregister a listener.
    The BroadcasterType must have the following functions:
    BroadcasterType::addListener (ListenerType*);
    BroadcasterType::removeListener (ListenerType*);

    @code
    // In members section
    ScopedListener listener;

    // Then when you want to register the listener
    listener.reset (broadcaster, *this);

    // Listener will automatically be unregistered when the ScopedListener goes out of scope but you can manually remove the listener as follows:
    listener.reset();
    @endcode
*/
class ScopedListener
{
public:
    /** Creates an empty ScopedListener. */
    ScopedListener() = default;

    /** Creates a ScopedListener which makes ListenerType listen to BroadcasterType. */
    template<typename BroadcasterType, typename ListenerType>
    ScopedListener (BroadcasterType& b, ListenerType& l)
    {
        reset (b, l);
    }

    /** Unregisters any previous listener and registers the new one. */
    template<typename BroadcasterType, typename ListenerType>
    void reset (BroadcasterType& b, ListenerType& l)
    {
        pimpl = std::make_any<std::shared_ptr<ScopedListenerImpl<BroadcasterType, ListenerType>>>
                    (std::make_shared<ScopedListenerImpl<BroadcasterType, ListenerType>> (b, l));
    }

    /** Unregisters the listener. */
    void reset()
    {
        pimpl = {};
    }

    /** Non-copyable. */
    ScopedListener (const ScopedListener&) = delete;
    /** Non-copyable. */
    ScopedListener& operator= (const ScopedListener&) = delete;

 private:
    template<typename BroadcasterType, typename ListenerType>
    class ScopedListenerImpl
    {
    public:
        ScopedListenerImpl (BroadcasterType& broadcaster_, ListenerType& listener_)
            : broadcaster (broadcaster_), listener (listener_)
        {
            broadcaster.addListener (&listener);
        }

        ~ScopedListenerImpl()
        {
            broadcaster.removeListener (&listener);
        }

    private:
        BroadcasterType& broadcaster;
        ListenerType& listener;
    };

    std::any pimpl;
};

}} // namespace tracktion { inline namespace engine
