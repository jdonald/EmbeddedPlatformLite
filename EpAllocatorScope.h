#pragma once

#include "EmbeddedPlatform.h"

class EpAllocatorScope;

// ----------------------------------------------------------------------------
// EpMemoryManager

void EpMemoryManagementInit();
void EpMemoryManagementShutDown();
void EpMemoryManagementLog();

// ----------------------------------------------------------------------------
// EpAllocatorScope (See EpMemoryManager.cpp)
//
// EpMemoryAllocatorId_Scratch* are tightly coupled with EpMemoryAllocatorScratchpad.

enum EpMemoryAllocatorId {
  EpMemoryAllocatorId_Heap = 0, // Must be 0
  EpMemoryAllocatorId_Permanent,
  EpMemoryAllocatorId_Resource,
  EpMemoryAllocatorId_TemporaryStack, // Resets to previous depth at scope closure.
  EpMemoryAllocatorId_Locked,
  EpMemoryAllocatorId_ScratchPage0,
  EpMemoryAllocatorId_ScratchPage1,
  EpMemoryAllocatorId_ScratchPage2,
  EpMemoryAllocatorId_ScratchTemp,
  EpMemoryAllocatorId_ScratchAll,
  EpMemoryAllocatorId_MAX,
  EpMemoryAllocatorId_UNSPECIFIED = -1
};

class EpAllocatorScope
{
public:
  EpAllocatorScope(EpMemoryAllocatorId id);
  ~EpAllocatorScope();

  uintptr_t GetTotalAllocationCount() const;
  uintptr_t GetTotalBytesAllocated() const;
  uintptr_t GetScopeAllocationCount() const;
  uintptr_t GetScopeBytesAllocated() const;
  uintptr_t GetPreviousAllocationCount() const { return m_previousAllocationCount; }
  uintptr_t GetPreviousBytesAllocated() const { return m_previousBytesAllocated; }

private:
  EpMemoryAllocatorId m_thisId;
  EpMemoryAllocatorId m_previousId;
  uintptr_t m_previousAllocationCount;
  uintptr_t m_previousBytesAllocated;
};
