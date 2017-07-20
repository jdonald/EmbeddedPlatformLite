#pragma once

#include "EpAllocator.h"

// ----------------------------------------------------------------------------
// EpUniquePtr
//
// An implementation of std::unique_ptr.  Only for objects managed with new and delete.

template<class T>
struct EpUniquePtr {
public:
  EpUniquePtr() { m_ptr = 0; }

  explicit EpUniquePtr(T* t) { m_ptr = t; }

  EpUniquePtr(EpUniquePtr& rhs) { m_ptr = rhs.release(); } // move semantics

  ~EpUniquePtr() {
    reset();
  }

  void operator=(EpUniquePtr& rhs) { reset(rhs.release()); }

  T* release() {
    T* ptr = m_ptr;
    m_ptr = 0;
    return ptr;
  }

  // Check for reassignment is non-standard.
  void reset(T* ptr=0) {
    if (m_ptr && m_ptr != ptr) {
      EpDelete(m_ptr);
    }
    m_ptr = ptr;
  }

  T* get() const { return m_ptr; }

  T& operator*() const { return *m_ptr; }
  T* operator->() const { return m_ptr; }
  operator bool() const { return m_ptr != 0; }

  bool operator==(const T* ptr) const { return ptr == m_ptr; }
  bool operator==(const EpUniquePtr& rhs) const { return m_ptr == rhs.m_ptr; }
  bool operator!=(const T* ptr) const { return ptr != m_ptr; }
  bool operator!=(const EpUniquePtr& rhs) const { return m_ptr != rhs.m_ptr; }

  // --------------------------------------------------------------------------
  // Non-standard:

  static EpUniquePtr make(T* ptr) { return EpUniquePtr(ptr); }

private:
  T* m_ptr;
};
