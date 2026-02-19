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

#ifndef CHOC_OBJECT_REFERENCE_HEADER_INCLUDED
#define CHOC_OBJECT_REFERENCE_HEADER_INCLUDED

#include <type_traits>

namespace choc
{

//==============================================================================
/**
    A very simple smart-pointer which simply wraps a non-null reference to an object.

    This is similar to std::reference_wrapper, but is stricter about not allowing the
    creation of uninitialised references, which means that you can be pretty certain that
    you can't ever read or write a nullptr via this class.

    @see choc::ObjectPointer
*/
template <typename Type>
struct ObjectReference  final
{
    using ObjectType = Type;

    /// An ObjectReference can only be created for a valid, non-null object reference.
    ObjectReference() = delete;

    /// An ObjectReference can never point to a nullptr!
    ObjectReference (decltype (nullptr)) = delete;

    template <typename SourceObjectType, typename = typename std::enable_if<std::is_convertible<SourceObjectType*, ObjectType*>::value>::type>
    ObjectReference (SourceObjectType& objectToReference) noexcept  : target (std::addressof (objectToReference)) {}

    template <typename SourceObjectType, typename = typename std::enable_if<std::is_convertible<SourceObjectType*, ObjectType*>::value>::type>
    ObjectReference (ObjectReference<SourceObjectType> objectToReference) noexcept  : target (objectToReference.getPointer()) {}

    /// Allows the reference to be changed to a new non-null object.
    template <typename SourceObjectType, typename = typename std::enable_if<std::is_convertible<SourceObjectType*, ObjectType*>::value>::type>
    ObjectReference& operator= (SourceObjectType& newObject) noexcept       { target = std::addressof (static_cast<ObjectType&> (newObject)); return *this; }

    operator ObjectType&() const noexcept                         { return *target; }
    ObjectType* operator->() const                                { return target; }

    ObjectType& get() const noexcept                              { return *target; }
    ObjectType* getPointer() const noexcept                       { return target; }
    ObjectType& getReference() const noexcept                     { return *target; }

    template <typename OtherObjectType> bool operator== (const OtherObjectType& other) const noexcept  { return target == getPointer (other); }
    template <typename OtherObjectType> bool operator!= (const OtherObjectType& other) const noexcept  { return target != getPointer (other); }
    template <typename OtherObjectType> bool operator<  (const OtherObjectType& other) const noexcept  { return target <  getPointer (other); }

private:
    ObjectType* target;

    template <typename OtherType> static auto getPointer (ObjectReference<OtherType> o) noexcept  { return o.target; }
    template <typename OtherType> static auto getPointer (const OtherType* o) noexcept            { return o; }
    template <typename OtherType> static auto getPointer (const OtherType& o) noexcept            { return std::addressof (o); }
};

} // namespace choc

#endif  // CHOC_OBJECT_REFERENCE_HEADER_INCLUDED
