#pragma once

#include "EpAllocator.h"

#include <new>

// ----------------------------------------------------------------------------
// EpArray
//
// A lite implementation of std::vector that does not blow out the CEVA compiler.
// Currently requires a default constructor.
// Inherits Reserve(size), GetCapacity() and "T GetStorage()[Capacity]".

template<class T, unsigned MaxDim=EpAllocatorMode_Dynamic>
struct EpArray : private EpAllocator<T, MaxDim> {
public:
  typedef T* iterator;
  typedef const T* const_iterator;
  typedef EpAllocator<T, MaxDim> allocator_type;

  // m_end will be 0 if MaxDim is 0.
  EP_FORCEINLINE EpArray() { m_end = this->GetStorage(); }

  EP_FORCEINLINE EpArray(const EpArray& rhs) {
    m_end = 0;
    assign(rhs.begin(), rhs.end());
  }

  template <class Rhs>
  EP_FORCEINLINE EpArray(const Rhs& rhs) {
    m_end = 0;
    assign(rhs.begin(), rhs.end());
  }

  EP_FORCEINLINE ~EpArray() {
    this->Destruct(this->GetStorage(), m_end);
  }

  EP_FORCEINLINE void operator=(const EpArray& rhs) {
    assign(rhs.begin(), rhs.end());
  }

  template <class Rhs>
  EP_FORCEINLINE void operator=(const Rhs& rhs) {
    assign(rhs.begin(), rhs.end());
  }

  EP_FORCEINLINE const allocator_type& get_allocator() const { return *this; }
  EP_FORCEINLINE       allocator_type& get_allocator()       { return *this; }

  EP_FORCEINLINE const T* begin() const { EpAssert(this->GetStorage()); return this->GetStorage(); }
  EP_FORCEINLINE       T* begin()       { EpAssert(this->GetStorage()); return this->GetStorage(); }

  EP_FORCEINLINE const T* end() const { EpAssert(this->GetStorage()); return m_end; }
  EP_FORCEINLINE       T* end()       { EpAssert(this->GetStorage()); return m_end; }

  EP_FORCEINLINE const T& front() const { EpAssert(size()); return *this->GetStorage(); }
  EP_FORCEINLINE       T& front()       { EpAssert(size()); return *this->GetStorage(); }

  EP_FORCEINLINE const T& back() const { EpAssert(size()); return *(m_end - 1); }
  EP_FORCEINLINE       T& back()       { EpAssert(size()); return *(m_end - 1); }

  EP_FORCEINLINE const T& operator[](unsigned index) const { EpAssert(index < size()); return this->GetStorage()[index]; }
  EP_FORCEINLINE       T& operator[](unsigned index)       { EpAssert(index < size()); return this->GetStorage()[index]; }

  EP_FORCEINLINE unsigned size() const {
    EpAssert(this->GetStorage());
    return (unsigned)(m_end - this->GetStorage());
  }

  EP_FORCEINLINE void reserve(unsigned c) {
    this->Reserve(c);
    if (m_end == 0) {
      m_end = this->GetStorage();
    }
  }

  EP_FORCEINLINE unsigned capacity() const { return this->GetCapacity(); }

  EP_FORCEINLINE void clear() {
    this->Destruct(this->GetStorage(), m_end);
    m_end = this->GetStorage();
  }

  EP_FORCEINLINE bool empty() const { return m_end == this->GetStorage(); }

  EP_FORCEINLINE void resize(unsigned sz) {
    reserve(sz);
    if (sz >= size()) {
      Construct(m_end, this->GetStorage() + sz);
    } else {
      this->Destruct(this->GetStorage() + sz, m_end);
    }
    m_end = this->GetStorage() + sz;
  }

  EP_FORCEINLINE void push_back(const T& t) {
    EpAssert(size() < capacity());
    ::new (m_end++) T(t);
  }

  EP_FORCEINLINE void pop_back() {
    EpAssert(size());
    (--m_end)->~T();
  }

  template <class Iter>
  EP_FORCEINLINE void assign(Iter begin, Iter end) {
    reserve((unsigned)(end - begin));
    T* it = this->GetStorage();
    this->Destruct(it, m_end);
    while (begin != end) { ::new (it++) T(*begin++); }
    m_end = it;
  }

  // --------------------------------------------------------------------------
  // C++11

  EP_FORCEINLINE const T* data() const { return this->GetStorage(); }
  EP_FORCEINLINE       T* data()       { return this->GetStorage(); }

  // --------------------------------------------------------------------------
  // Non-standard:

  // Returns pointer for use with placement new.
  EP_FORCEINLINE void* emplace_back_raw() {
    EpAssert(size() < capacity());
    return (void*)m_end++;
  }

  // Moves the end element down as needed.
  EP_FORCEINLINE void erase_unordered(unsigned index) {
    EpAssert(index < size());
    T* it = this->GetStorage() + index;
    if (it != --m_end) {
      *it = *m_end;
    }
    m_end->~T();
  }

  // Moves the end element down as needed.
  EP_FORCEINLINE void erase_unordered(T* it) {
    EpAssert((unsigned)(it - this->GetStorage()) < size());
    if (it != --m_end) {
      *it = *m_end;
    }
    m_end->~T();
  }

  EP_FORCEINLINE bool full() {
    return size() == capacity();
  }

private:
  EP_FORCEINLINE void Construct(T* begin, T* end) {
    while (begin != end) {
      ::new (begin++) T;
    }
  }

  EP_FORCEINLINE void Destruct(T* begin, T* end) {
    while (begin != end) {
      (begin++)->~T();
    }
  }

  T* m_end;
};
