#pragma once

#include "EmbeddedPlatform.h"
#include "EpAllocatorScope.h"
#include "EpDma.h"
#include "EpSettings.h"

#include <stdlib.h>
#include <string.h>

#define EpAllocatorMode_Dynamic     0u  // Dynamically allocated

// ----------------------------------------------------------------------------
// EpAllocator
//
// Static allocation for known capacities, dynamic allocation otherwise.

template<class T, unsigned Capacity>
struct EpAllocator {
public:
  static_assert(Capacity > 0u, "Capacity > 0");
  EP_FORCEINLINE EpAllocator() {
    if (EpIsDebug()) {
      ::memset(m_storage, 0xab, sizeof(T) * Capacity);
    }
  }

  // Because Reserve() will not actually reallocate it is also used to ensure initial capacity.
  EP_FORCEINLINE void Reserve(unsigned size) { EpReleaseAssertMsg(size <= Capacity, "EpAllocator: Overflowing fixed capacity."); }
  EP_FORCEINLINE unsigned GetCapacity() const { return Capacity; }
  EP_FORCEINLINE T* GetStorage() const { return (T*)(unsigned*)m_storage; }
  EP_FORCEINLINE T* GetStorageNoReadWrite() const { return (T*)(unsigned*)m_storage; }

private:
  // Force 32-bit alignment
  unsigned m_storage[(Capacity * sizeof(T) + 3) >> 2];
};

// ----------------------------------------------------------------------------
template<class T>
struct EpAllocator<T, EpAllocatorMode_Dynamic> {
public:
  EP_FORCEINLINE EpAllocator() {
    m_storage = NULL;
    m_capacity = 0;
  }

  EP_FORCEINLINE ~EpAllocator() {
    if (m_storage) {
      m_capacity = 0;
      EpFree(m_storage);
      m_storage = NULL;
    }
  }

  // Because Reserve() will not actually reallocate it is also used to ensure initial capacity.
  EP_FORCEINLINE void Reserve(unsigned c) {
    if (c <= m_capacity) { return; }
    EpReleaseAssertMsg(m_capacity == 0, "EpAllocator: Reallocation disallowed.");
    m_storage = EpAliasingCast<T>(EpMalloc(sizeof(T) * c));
    EpReleaseAssertMsg(m_storage != 0, "EpAllocator: Allocation failure"); // Must never fail.
    m_capacity = c;
    if (EpIsDebug()) {
      ::memset(m_storage, 0xab, sizeof(T) * c);
    }
  }

  EP_FORCEINLINE unsigned GetCapacity() const { return m_capacity; }
  EP_FORCEINLINE T* GetStorage() const { return m_storage; }

protected:
  unsigned m_capacity;
  T* m_storage;
};
