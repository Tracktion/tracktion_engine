//
//    ██████ ██   ██  ██████   ██████
//   ██      ██   ██ ██    ██ ██            ** Classy Header-Only Classes **
//   ██      ███████ ██    ██ ██
//   ██      ██   ██ ██    ██ ██           https://github.com/Tracktion/choc
//    ██████ ██   ██  ██████   ██████
//
//   CHOC is (C)2022 Tracktion Corporation, and is offered under the terms of the ISC license:
//
//   Permission to use, copy, modify, and/or distribute this software for any purpose with or
//   without fee is hereby granted, provided that the above copyright notice and this permission
//   notice appear in all copies. THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
//   WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
//   AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR
//   CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
//   WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
//   CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

#ifndef CHOC_COM_HELPERS_HEADER_INCLUDED
#define CHOC_COM_HELPERS_HEADER_INCLUDED

#include <atomic>
#include <string>
#include "../platform/choc_Assert.h"

#ifdef __clang__
 #pragma clang diagnostic push
 #pragma clang diagnostic ignored "-Wnon-virtual-dtor" // COM objects can't have a virtual destructor
#elif __GNUC__
 #pragma GCC diagnostic push
 #pragma GCC diagnostic ignored "-Wnon-virtual-dtor" // COM objects can't have a virtual destructor
#endif


/*
    The venerable Microsoft "COM" object system is clunky and awful in many ways.
    But if you need to publish a pre-built DLL that exposes an object-oriented API, and you
    need it to work with random platforms and client compilers, then COM is often the
    only practical approach for doing that.

    Essentially, implementing COM just means that your library exposes some polymorphic C++
    objects, but makes sure that client code never calls new/delete on them. That is done by
    deriving all the classes from a pure virtual base class with addRef/release methods that
    manage their lifetimes by ref-counting.

    Many SDKs and APIs over the years have used the COM style, and they all inevitably end up
    implementing a bunch of classes that are subtle variations on the ones here. Hopefully
    these ones will be flexible enough to save many people the hassle of re-inventing these
    same wheels again.
*/

namespace choc::com
{

//==============================================================================
/**
    An abstract base class for a COM object.
    You can either derive your own classes directly from this one, or from the
    helper class ObjectWithAtomicRefCount, which takes case of implementing the
    addRef()/release() methods.
*/
struct Object
{
    /// This must increment the object's ref-count.
    /// It returns the incremented number of references.
    virtual int addRef() noexcept = 0;

    /// This must decrement the object's ref-count, and delete the object if
    /// the count has reached zero.
    /// It should return the number of references left after being decremented.
    virtual int release() noexcept = 0;

    /// Returns the current reference count (though of course in a mutli-threaded
    /// situation it could be different by the time the function has returned).
    virtual int getReferenceCount() const noexcept = 0;
};

//==============================================================================
/// A helper class that implements the Object base-class and takes care of managing
/// an atomic reference-count.
/// The BaseClass template should be either Object or some other class that
/// derives from Object, and this class will inherit from BaseClass.
/// DerivedClass is the name of the class that you're implementing, and
/// this is used to determine the type that will be deleted when the reference
/// count becomes zero.
/// When the object is initially created, its reference count is set to 1.
template <typename BaseClass, typename DerivedClass>
struct ObjectWithAtomicRefCount  : public BaseClass
{
    ObjectWithAtomicRefCount() = default;

    int addRef() noexcept override;
    int release() noexcept override;
    int getReferenceCount() const noexcept override;

    std::atomic<int> referenceCount { 1 };

private:
    /// A copy constructor would copy the ref-count, which makes no sense,
    /// so these are marked as deleted to stop that kind of madness.
    ObjectWithAtomicRefCount (ObjectWithAtomicRefCount&&) = delete;
    ObjectWithAtomicRefCount (const ObjectWithAtomicRefCount&) = delete;
};

//==============================================================================
/**
    A reference-counting smart-pointer that will manage a COM-compliant object.

    The object that it points to must have methods called addRef() and
    release(). Most likely it'll be derived from choc::COM::Object, but
    doesn't need to be.

    The best way to create a Ptr to an object is with the choc::com::create() method
*/
template <typename Type>
struct Ptr
{
    using ObjectType = Type;

    Ptr() noexcept = default;
    ~Ptr() noexcept;

    Ptr (const Ptr&) noexcept;
    Ptr (Ptr&&) noexcept;
    Ptr (decltype(nullptr)) noexcept;

    /// Captures a raw pointer: Note that this does NOT increase its reference count!
    explicit Ptr (Type*) noexcept;

    Ptr& operator= (const Ptr&) noexcept;
    Ptr& operator= (Ptr&&) noexcept;
    Ptr& operator= (decltype(nullptr)) noexcept;

    operator bool() const noexcept                      { return pointer != nullptr; }
    ObjectType& operator*() const                       { CHOC_ASSERT (pointer != nullptr); return *pointer; }
    ObjectType* operator->() const                      { CHOC_ASSERT (pointer != nullptr); return pointer; }

    /// Gets a raw pointer to the object
    ObjectType* get() const noexcept                    { return pointer; }

    /// Often with COM APIs, you'll have functions which return a raw pointer to an object
    /// for the caller to use, where the caller is expected to release it when they're done.
    /// This method makes it easy to get a raw pointer and bump up the ref-count in one call.
    ObjectType* getWithIncrementedRefCount() const noexcept                             { inc(); return get(); }

    /// Allow implicit casting to a base class pointer
    template <typename BaseClass>
    operator Ptr<BaseClass>() const noexcept                                            { return Ptr<BaseClass> (getWithIncrementedRefCount()); }

    /// Sets the pointer to null, releasing any object that it was pointing to.
    void reset() noexcept;

    bool operator== (decltype (nullptr)) const noexcept                                 { return pointer == nullptr; }
    bool operator!= (decltype (nullptr)) const noexcept                                 { return pointer != nullptr; }
    bool operator== (Ptr other) const noexcept                                          { return pointer == other.pointer; }
    bool operator!= (Ptr other) const noexcept                                          { return pointer != other.pointer; }
    bool operator<  (Ptr other) const noexcept                                          { return pointer <  other.pointer; }
    template <typename OtherType> bool operator== (OtherType& other) const noexcept     { return pointer == std::addressof (other); }
    template <typename OtherType> bool operator!= (OtherType& other) const noexcept     { return pointer != std::addressof (other); }
    template <typename OtherType> bool operator<  (OtherType& other) const noexcept     { return pointer <  std::addressof (other); }

private:
    Type* pointer = nullptr;

    void inc() const noexcept;
    void dec() noexcept;

    Ptr (bool) = delete;
    Ptr (const void*) = delete;
};

//==============================================================================
/// Creates an object of the given type, and returns it as a Ptr smart-pointer.
/// This is the best way to get a new object with the ref-count set up correctly.
template <typename ObjectType, typename... Args>
Ptr<ObjectType> create (Args&&... args)
{
    return Ptr<ObjectType> (new ObjectType (std::forward<Args> (args)...));
}

//==============================================================================
/// A handy COM class for a string.
/// You can implement a sub-class of this yourself, or just use the
/// choc::com::createString() function to create one.
struct String  : public Object
{
    virtual const char* begin() const = 0;
    virtual const char* end() const = 0;

    std::string_view get() const;
};

/// Gets a std::string_view from a Ptr<String>, or returns
/// an empty view if the Ptr is null.
std::string_view toString (const Ptr<String>&);

/// This is just a subclass of Ptr<String> which adds some handy casts and getter
/// methods to make it easy to use the string without calling toString() on it.
struct StringPtr  : public Ptr<String>
{
    StringPtr() = default;
    StringPtr (const StringPtr&) = default;
    StringPtr& operator= (const StringPtr&) = default;
    StringPtr (StringPtr&&) = default;
    StringPtr& operator= (StringPtr&&) = default;

    explicit StringPtr (String* s) : Ptr<String> (s) {}
    StringPtr (Ptr<String> p) : Ptr<String> (std::move (p)) {}

    std::string_view get() const            { return toString (*this); }

    operator std::string_view() const       { return get(); }
    operator std::string() const            { return std::string (get()); }
};

/// Returns a COM-friendly string.
StringPtr createString (std::string);

/// Returns a COM-friendly string object as a raw pointer with a ref-count
/// of 1, ready for use as a return value from a function.
String* createRawString (std::string);


//==============================================================================
//        _        _           _  _
//     __| |  ___ | |_   __ _ (_)| | ___
//    / _` | / _ \| __| / _` || || |/ __|
//   | (_| ||  __/| |_ | (_| || || |\__ \ _  _  _
//    \__,_| \___| \__| \__,_||_||_||___/(_)(_)(_)
//
//   Code beyond this point is implementation detail...
//
//==============================================================================

template <typename BaseClass, typename DerivedClass>
int ObjectWithAtomicRefCount<BaseClass, DerivedClass>::addRef() noexcept
{
    return ++referenceCount;
}

template <typename BaseClass, typename DerivedClass>
int ObjectWithAtomicRefCount<BaseClass, DerivedClass>::release() noexcept
{
    auto count = --referenceCount;

    if (count > 0)
        return count;

    delete static_cast<DerivedClass*> (this);

    // A negative ref-count means that something has gone very wrong...
    CHOC_ASSERT (count == 0);
    return 0;
}

template <typename BaseClass, typename DerivedClass>
int ObjectWithAtomicRefCount<BaseClass, DerivedClass>::getReferenceCount() const noexcept
{
    return referenceCount.load();
}

//==============================================================================
template <typename Type>
Ptr<Type>::~Ptr() noexcept
{
    dec();
}

template <typename Type> Ptr<Type>::Ptr (decltype(nullptr)) noexcept {}
template <typename Type> Ptr<Type>::Ptr (const Ptr& other) noexcept  : pointer (other.pointer) { inc(); }
template <typename Type> Ptr<Type>::Ptr (Ptr&& other) noexcept  : pointer (other.pointer) { other.pointer = nullptr; }
template <typename Type> Ptr<Type>::Ptr (Type* target) noexcept  : pointer (target) {}

template <typename Type>
Ptr<Type>& Ptr<Type>::operator= (const Ptr& other) noexcept
{
     other.inc();
     dec();
     pointer = other.pointer;
     return *this;
}

template <typename Type>
Ptr<Type>& Ptr<Type>::operator= (Ptr&& other) noexcept
{
    dec();
    pointer = other.pointer;
    other.pointer = nullptr;
    return *this;
}

template <typename Type>
Ptr<Type>& Ptr<Type>::operator= (decltype(nullptr)) noexcept
{
    reset();
    return *this;
}

template <typename Type>
void Ptr<Type>::reset() noexcept
{
    dec();
    pointer = nullptr;
}

template <typename Type>
void Ptr<Type>::inc() const noexcept
{
    if (pointer != nullptr)
        pointer->addRef();
}

template <typename Type>
void Ptr<Type>::dec() noexcept
{
    if (pointer != nullptr)
        pointer->release();
}

//==============================================================================
inline std::string_view String::get() const
{
    auto start = begin();
    auto len = static_cast<std::string_view::size_type> (end() - start);
    return std::string_view (start, len);
}

inline std::string_view toString (const Ptr<String>& s)
{
    if (s)
        return s->get();

     return {};
}

inline String* createRawString (std::string stringToUse)
{
    struct StringHolder final  : public ObjectWithAtomicRefCount<String, StringHolder>
    {
        StringHolder (std::string&& s)  : str (std::move (s)) {}

        const char* begin() const override  { return str.data(); }
        const char* end() const override    { return str.data() + str.length(); }

        std::string str;
    };

    return new StringHolder (std::move (stringToUse));
}

inline StringPtr createString (std::string stringToUse)
{
    return StringPtr (createRawString (std::move (stringToUse)));
}

} // namespace choc::com

#ifdef __clang__
 #pragma clang diagnostic pop
#elif __GNUC__
 #pragma GCC diagnostic pop
#endif

#endif // CHOC_COM_HELPERS_HEADER_INCLUDED
