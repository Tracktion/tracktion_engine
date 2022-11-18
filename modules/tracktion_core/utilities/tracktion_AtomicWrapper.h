//
// Created by Jamie Pond on 18/11/2022.
//

#pragma once
#include <atomic>

template<typename Type>
struct AtomicWrapper
{
  AtomicWrapper() = default;

  template<typename OtherType>
  AtomicWrapper (const OtherType& other)
  {
    value.store (other);
  }

  AtomicWrapper (const AtomicWrapper& other)
  {
    value.store (other.value);
  }

  AtomicWrapper& operator= (const AtomicWrapper& other) noexcept
  {
    value.store (other.value);
    return *this;
  }

  bool operator== (const AtomicWrapper& other) const noexcept
  {
    return value.load() == other.value.load();
  }

  bool operator!= (const AtomicWrapper& other) const noexcept
  {
    return value.load() != other.value.load();
  }

  operator Type() const noexcept { return value.load(); }

  std::atomic<Type> value { Type() };
};
