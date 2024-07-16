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

#ifndef CHOC_OBJECT_POINTER_HEADER_INCLUDED
#define CHOC_OBJECT_POINTER_HEADER_INCLUDED

#include <type_traits>

namespace choc
{

//==============================================================================
/**
    A very simple smart-pointer which simply wraps a raw pointer without owning it.

    Why use this rather than a raw pointer? Two main reasons:
     - Semantically, using this class makes it clear to the reader that the object is
       not being owned or managed by the code that has the pointer.
     - All dereferences of the smart-pointer will assert if the pointer is null. This
       means that if you make it a policy to always use this wrapper around your raw pointers,
       and also make CHOC_ASSERT throw an exception, then you can safely and cleanly capture
       all your null pointer errors with a handler, and log/report them rather than crashing.

    If you find yourself using one of these to hold pointer which can never be null, you should
    probably use choc::ObjectReference instead.

    @see choc::ObjectReference
*/
template <typename Type>
struct ObjectPointer  final
{
    using ObjectType = Type;

    /// Creates a null ObjectPointer.
    ObjectPointer() = default;

    /// Creates a null ObjectPointer.
    ObjectPointer (decltype (nullptr)) noexcept {}

    /// Creates an ObjectPointer that points to a non-null source object.
    template <typename SourceObjectType, typename = typename std::enable_if<std::is_convertible<SourceObjectType*, ObjectType*>::value>::type>
    ObjectPointer (SourceObjectType& object) noexcept : pointer (std::addressof (object)) {}

    /// Creates an ObjectPointer from a raw pointer.
    template <typename SourceObjectType, typename = typename std::enable_if<std::is_convertible<SourceObjectType*, ObjectType*>::value>::type>
    explicit ObjectPointer (SourceObjectType* object) noexcept : pointer (object) {}

    /// Creates an ObjectPointer that points to another ObjectPointer (which may be of a
    /// different object type if it can be trivially cast to the target type).
    template <typename SourceObjectType, typename = typename std::enable_if<std::is_convertible<SourceObjectType*, ObjectType*>::value>::type>
    ObjectPointer (ObjectPointer<SourceObjectType> object) noexcept : pointer (object.get()) {}

    /// Sets this ObjectPointer from another ObjectPointer (which may be of a different object type
    /// if it can be trivially cast to the target type).
    template <typename SourceObjectType, typename = typename std::enable_if<std::is_convertible<SourceObjectType*, ObjectType*>::value>::type>
    ObjectPointer& operator= (ObjectPointer<SourceObjectType> newObject) noexcept    { pointer = newObject.get(); return *this; }

    /// The dereference operator uses CHOC_ASSERT to check whether the pointer is null before returning it.
    ObjectType& operator*() const                               { CHOC_ASSERT (pointer != nullptr); return *pointer; }

    /// The dereference operator uses CHOC_ASSERT to check whether the pointer is null before returning it.
    ObjectType* operator->() const                              { CHOC_ASSERT (pointer != nullptr); return pointer; }

    /// Returns the raw pointer.
    ObjectType* get() const noexcept                            { return pointer; }

    /// Allows the pointer to be checked for nullness.
    operator bool() const noexcept                              { return pointer != nullptr; }

    /// Sets the pointer to null.
    void reset() noexcept                                       { pointer = {}; }

    /// Sets the pointer to a new value.
    void reset (ObjectType* newPointer) noexcept                { pointer = newPointer; }

    bool operator== (decltype (nullptr)) const noexcept         { return pointer == nullptr; }
    bool operator!= (decltype (nullptr)) const noexcept         { return pointer != nullptr; }

    template <typename OtherObjectType> bool operator== (const OtherObjectType& other) const noexcept  { return pointer == getPointer (other); }
    template <typename OtherObjectType> bool operator!= (const OtherObjectType& other) const noexcept  { return pointer != getPointer (other); }
    template <typename OtherObjectType> bool operator<  (const OtherObjectType& other) const noexcept  { return pointer <  getPointer (other); }

private:
    ObjectType* pointer = {};

    // These prevent accidental casts to pointers and numeric types via the bool() operator
    operator void*()  const = delete;
    operator char()   const = delete;
    operator short()  const = delete;
    operator int()    const = delete;
    operator long()   const = delete;
    operator size_t() const = delete;

    template <typename OtherType> static auto getPointer (ObjectPointer<OtherType> o) noexcept  { return o.pointer; }
    template <typename OtherType> static auto getPointer (const OtherType* o) noexcept          { return o; }
    template <typename OtherType> static auto getPointer (const OtherType& o) noexcept          { return std::addressof (o); }
};

} // namespace choc

#endif // CHOC_OBJECT_POINTER_HEADER_INCLUDED
