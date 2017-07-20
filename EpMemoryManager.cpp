#include "EmbeddedPlatform.h"
#include "EpAllocatorScope.h"
#include "EpSettings.h"

#include <stdlib.h>
#include <string.h>
#include <new>

#define EP_KB 1024
#define EP_MB (1024*1024)

#define EP_MEMORY_BUDGET_PERMANENT        (5  *EP_KB)
#define EP_MEMORY_BUDGET_RESOURCE         (5  *EP_KB)
#define EP_MEMORY_BUDGET_TEMPORARY_STACK  (60 *EP_KB)

// TODO: Elaborate for your use case
#define EP_MEMORY_BUDGET_SCRATCH_PAGE              (10*EP_KB)
#define EP_MEMORY_BUDGET_SCRATCH_TEMP              (60*EP_KB)

#define EP_MEMORY_BUDGET_SCRATCH ((EP_MEMORY_BUDGET_SCRATCH_PAGE * 3) \
                                +  EP_MEMORY_BUDGET_SCRATCH_TEMP)


// Always always check malloc and halt on failure.  This is extremely important
// with hardware where null is a valid address and can be written to with
// disastrous results.
static void* EpMallocChecked(size_t size) {
#if (EP_MEM_DIAGNOSTIC_LEVEL>=3)
  static size_t count = 0;
  count += size;
  EpLog("Malloc total: %d\n", count);
#endif
  void* t = ::malloc(size);
  if(t == NULL) {
    EpExit("malloc failed.");
  }
  return t;
}


// EP_MEM_DIAGNOSTIC_LEVEL:
//
// -1: remove code entirely
//  0: normal operation
//  1: enable checking g_epSettings.platform_disableMemoryManager.
//  2: log allocator scopes
//  3: also log heap utilization
//
#define EP_MEM_DIAGNOSTIC_LEVEL 1

#if (EP_MEM_DIAGNOSTIC_LEVEL != -1)

// ----------------------------------------------------------------------------
// EpScratchpad
//

template<unsigned Bytes>
struct EpScratchpad {
  void* At() { return &*m_storage; }
  uintptr_t m_storage[Bytes / sizeof(uintptr_t)];
};

// ----------------------------------------------------------------------------
// EpMemoryAllocationHeader

#if (EP_MEM_DIAGNOSTIC_LEVEL>=1)
struct EpMemoryAllocationHeader {
  uintptr_t size;
  uintptr_t actual;
};

#endif // EP_MEM_DIAGNOSTIC_LEVEL

// ----------------------------------------------------------------------------
// EpMemoryAllocatorBase

class EpMemoryAllocatorBase {
public:
  void* Allocate(size_t size, uintptr_t alignmentMask) {
    if (size == 0) {
      size = 1; // Maintain unique pointer values
    }
    return OnAlloc(size, alignmentMask);
  }
  void Free(void* ptr) {
    if (ptr == nullptr) {
      return;
    }
    // Null ok, EpAllocator will EpReleaseAssert(ptr != null)
    OnFree(ptr);
  }
  virtual bool Contains(void* ptr) = 0;
  virtual void BeginAllocationScope(EpAllocatorScope* scope, EpMemoryAllocatorId newId) = 0;
  virtual void EndAllocationScope(EpAllocatorScope* scope, EpMemoryAllocatorId oldId) = 0;
  virtual uintptr_t GetAllocationCount(EpMemoryAllocatorId id) const = 0;
  virtual uintptr_t GetBytesAllocated(EpMemoryAllocatorId id) const = 0;
  virtual uintptr_t GetHighWater(EpMemoryAllocatorId id) const { return 0u; }
  const char* Label() const { return m_label; }

protected:
  virtual void* OnAlloc(size_t size, uintptr_t alignmentMask) = 0;
  virtual void OnFree(void* ptr) = 0;
  const char* m_label;
};

// ----------------------------------------------------------------------------
// EpMemoryAllocatorOsHeap

class EpMemoryAllocatorOsHeap : public EpMemoryAllocatorBase {
public:
  void Construct(const char* label) {
    ::new (this) EpMemoryAllocatorOsHeap(); // Set vtable ptr.
    m_allocationCount = 0;
    m_bytesAllocated = 0;
    m_label = label;
  }

  virtual bool Contains(void* ptr) override { return false; } // Dont know actually.

  virtual void BeginAllocationScope(EpAllocatorScope* scope, EpMemoryAllocatorId newId) override { }

  virtual void EndAllocationScope(EpAllocatorScope* scope, EpMemoryAllocatorId oldId) override { }

  virtual uintptr_t GetAllocationCount(EpMemoryAllocatorId id) const override { return m_allocationCount; }

  virtual uintptr_t GetBytesAllocated(EpMemoryAllocatorId id) const override { return m_bytesAllocated; }

protected:
  virtual void* OnAlloc(size_t size, uintptr_t alignmentMask) override {
    EpAssert(size != 0); // EpMemoryAllocatorBase::Allocate
    ++m_allocationCount;
    m_bytesAllocated += size; // ignore overhead

    // Place header immediately before aligned allocation.
    uintptr_t actual = (uintptr_t)EpMallocChecked(size + sizeof(EpMemoryAllocationHeader) + alignmentMask);
    uintptr_t aligned = (actual + sizeof(EpMemoryAllocationHeader) + alignmentMask) & ~alignmentMask;
    EpMemoryAllocationHeader* hdr = (EpMemoryAllocationHeader*)(aligned) - 1;
    hdr->size = size;
    hdr->actual = actual;

#if (EP_MEM_DIAGNOSTIC_LEVEL>=3)
    // Record the size of the allocation in debug.  Cast via (uintptr_t) because Mac not supporting %p.
    EpLog("%s: %x  %d  (count %d, size %d)\n", m_label, (unsigned)(uintptr_t)(hdr + 1), (int)size, (int)m_allocationCount, (int)m_bytesAllocated);
#endif
    return (void *)aligned;
  }

  virtual void OnFree(void* p) override {
    EpAssert(m_allocationCount > 0);
    --m_allocationCount;
    EpMemoryAllocationHeader* hdr = (EpMemoryAllocationHeader*)p - 1;
    EpAssert(hdr->size != 0); // EpMemoryAllocatorBase::Allocate
    m_bytesAllocated -= hdr->size;

#if (EP_MEM_DIAGNOSTIC_LEVEL>=3)
    // Record the size of the allocation in debug.  Cast via (uintptr_t) because Mac and supporting %p.
    EpLog("%s: %x -%d   (count %d, size %d)\n", m_label, (unsigned)(uintptr_t)p, (int)hdr->size, (int)m_allocationCount, (int)m_bytesAllocated);
#endif
    ::free((void*)hdr->actual);
  }

private:
  uintptr_t m_allocationCount;
  uintptr_t m_bytesAllocated;
};

// ----------------------------------------------------------------------------
// EpMemoryAllocatorStack: Nothing can be freed.

class EpMemoryAllocatorStack : public EpMemoryAllocatorBase {
public:
  void Construct(void* ptr, size_t size, const char* label) {
    ::new (this) EpMemoryAllocatorStack(); // Set vtable ptr.

    m_allocationCount = 0;
    m_label = label;
    m_begin = ((uintptr_t)ptr);
    m_end = ((uintptr_t)ptr + size);
    m_current = ((uintptr_t)ptr);

#if (EP_DEBUG==1)
    ::memset((void*)ptr, 0xfe, size);
#endif
  }

  virtual void BeginAllocationScope(EpAllocatorScope* scope, EpMemoryAllocatorId newId) override { }

  virtual void EndAllocationScope(EpAllocatorScope* scope, EpMemoryAllocatorId oldId) override { }

  virtual bool Contains(void* ptr) override {
    return (uintptr_t)ptr >= m_begin && (uintptr_t)ptr < m_end;
  }

  virtual uintptr_t GetAllocationCount(EpMemoryAllocatorId id) const override { return m_allocationCount; }

  virtual uintptr_t GetBytesAllocated(EpMemoryAllocatorId id) const override { return m_current - m_begin; }

  void* Release() {
    void* t = (void*)m_begin;
    m_begin = 0;
    return t;
  }

  void* AllocateNonVirtual(size_t size, uintptr_t alignmentMask) {
    uintptr_t aligned = (m_current + alignmentMask) & ~alignmentMask;

    if ((aligned + size) > m_end) {
      return 0;
    }

    ++m_allocationCount;
    m_current = aligned + size;
    return (void*)aligned;
  }

  void OnFreeNonVirtual(void* ptr) {
    EpAssertMsg(m_allocationCount > 0 && (uintptr_t)ptr >= m_begin && (uintptr_t)ptr < m_current, "%s free after stack reset", m_label);

    if ((uintptr_t)ptr < m_current) {
      --m_allocationCount;
    }

    return;
  }

protected:
  virtual void* OnAlloc(size_t size, uintptr_t alignmentMask) override {
    return AllocateNonVirtual(size, alignmentMask);
  }

  virtual void OnFree(void* ptr) override {
    OnFreeNonVirtual(ptr);
    EpReleaseWarning(g_epSettings.platform_isShuttingDown, "ERROR: %s, illegal free()", m_label);
  }

protected:
  uintptr_t m_begin;
  uintptr_t m_end;
  uintptr_t m_current;
  uintptr_t m_allocationCount;
};

// ----------------------------------------------------------------------------
// EpMemoryAllocatorTempStack: Resets after a scope closes.

class EpMemoryAllocatorTempStack : public EpMemoryAllocatorStack {
public:
  void Construct(void* ptr, size_t size, const char* label) {
    ::new (this) EpMemoryAllocatorTempStack(); // Set vtable ptr.

    m_allocationCount = 0;
    m_label = label;
    m_begin = ((uintptr_t)ptr);
    m_end = ((uintptr_t)ptr + size);
    m_current = ((uintptr_t)ptr);

#if (EP_DEBUG==1)
    ::memset((void*)ptr, 0xfe, size);
#endif
  }

  virtual void EndAllocationScope(EpAllocatorScope* scope, EpMemoryAllocatorId oldId) override {
    uintptr_t previousCurrent = m_begin + scope->GetPreviousBytesAllocated();

#if (EP_DEBUG==1)
    ::memset((void*)previousCurrent, 0xfe, (size_t)(m_current - previousCurrent));
#endif

    EpAssertMsg(m_allocationCount == scope->GetPreviousAllocationCount(), "%s leaked %d allocations", m_label, (int)(m_allocationCount - scope->GetPreviousAllocationCount()));

    m_allocationCount = scope->GetPreviousAllocationCount();
    m_current = previousCurrent;
    EpReleaseAssertMsg(m_current <= m_end, "Error resetting temp stack"); // Probably overwrote the stack trashing *scope.
  }

  virtual void OnFree(void* ptr) override {
    OnFreeNonVirtual(ptr);
  }
};

// ----------------------------------------------------------------------------
// EpMemoryAllocatorLocked:  No allocations allowed.

class EpMemoryAllocatorLocked : public EpMemoryAllocatorBase {
public:
  void Construct(const char* label) {
    ::new (this) EpMemoryAllocatorLocked(); // Set vtable ptr.
    m_label = label;
  }

  virtual void BeginAllocationScope(EpAllocatorScope* scope, EpMemoryAllocatorId newId) override { }
  virtual void EndAllocationScope(EpAllocatorScope* scope, EpMemoryAllocatorId oldId) override { }
  virtual bool Contains(void* ptr) override { return false; }
  virtual uintptr_t GetAllocationCount(EpMemoryAllocatorId id) const override { return 0u; }
  virtual uintptr_t GetBytesAllocated(EpMemoryAllocatorId id) const override { return 0u; }

protected:
  virtual void* OnAlloc(size_t size, uintptr_t alignmentMask) override {
    EpReleaseWarning(false, "Allocation while locked");
    return 0;
  }

  virtual void OnFree(void* ptr) override { }
};

// ----------------------------------------------------------------------------
// EpMemoryAllocatorScratchpad: A stack allocator where allocations are expected
// to leak.  This is a system for assigning intermediate locations in algorithms
// that are aware of their temporary nature.

class EpMemoryAllocatorScratchpad : public EpMemoryAllocatorBase {
private:
  struct Section {
    uintptr_t m_begin;
    uintptr_t m_end;
    uintptr_t m_current;
    uintptr_t m_allocationCount;
    uintptr_t m_highWater;
  };

  static const unsigned c_nSections = EpMemoryAllocatorId_MAX - EpMemoryAllocatorId_ScratchPage0;
  static const unsigned c_allSection = c_nSections - 1;

public:
  void Construct(void* ptr, size_t size, const char* label) {
    ::new (this) EpMemoryAllocatorScratchpad(); // Set vtable ptr.
    m_label = label;

    uintptr_t current = (uintptr_t)ptr;

    // This could be made custom per-algorithm.
    uintptr_t sizes[c_nSections] = {
      EP_MEMORY_BUDGET_SCRATCH_PAGE,     // EpMemoryAllocatorId_ScratchPage0
      EP_MEMORY_BUDGET_SCRATCH_PAGE,     // EpMemoryAllocatorId_ScratchPage1
      EP_MEMORY_BUDGET_SCRATCH_PAGE,     // EpMemoryAllocatorId_ScratchPage2
      EP_MEMORY_BUDGET_SCRATCH_TEMP      // EpMemoryAllocatorId_ScratchTemp
    };

    for (unsigned i = 0; i < (unsigned)c_allSection; ++i) {
      m_sections[i].m_begin = current;
      m_sections[i].m_current = 0u;
      m_sections[i].m_allocationCount = 0u;
      m_sections[i].m_highWater = current;

      current += sizes[i];

      m_sections[i].m_end = current;
    }

    // Last section is set up manually.
    Section& sectionAll = m_sections[c_allSection];
    sectionAll.m_begin = (uintptr_t)ptr;
    sectionAll.m_current = 0u;
    sectionAll.m_allocationCount = 0u;
    sectionAll.m_highWater = (uintptr_t)ptr;
    sectionAll.m_end = EP_MEMORY_BUDGET_SCRATCH + (uintptr_t)ptr;

    EpAssert((current - (uintptr_t)ptr) == (uintptr_t)size);
  }

  virtual void BeginAllocationScope(EpAllocatorScope* scope, EpMemoryAllocatorId newId) override {
    m_currentSection = (unsigned)newId - (unsigned)EpMemoryAllocatorId_ScratchPage0;
    EpAssert(m_currentSection < c_nSections);
    Section& section = m_sections[m_currentSection];

    // Reopening is prohibited.
#if (EP_DEBUG==1)
    EpAssertMsg(section.m_current == 0u, "reopening scratchpad allocator");
    if (newId == EpMemoryAllocatorId_ScratchAll) {
      // Everything but EpMemoryAllocatorId_ScratchAll must be closed.
      for (unsigned i = 0; i < (unsigned)c_allSection; ++i) {
        EpAssertMsg(m_sections[i].m_current == 0u, "scratchpad all is exclusive");
      }
    }
    else {
      // EpMemoryAllocatorId_ScratchAll must be closed.
      EpAssertMsg(m_sections[c_allSection].m_current == 0u, "scratchpad all is exclusive");
    }

    ::memset((void*)section.m_begin, 0xfe, (size_t)(section.m_end - section.m_begin));
#endif
    section.m_current = section.m_begin;
    section.m_allocationCount = 0;
  }

  virtual void EndAllocationScope(EpAllocatorScope* scope, EpMemoryAllocatorId oldId) override {
    EpAssert(m_currentSection < c_nSections);
    Section& section = m_sections[m_currentSection];
    EpAssert(section.m_current != 0u);
    section.m_highWater = (section.m_highWater > section.m_current) ? section.m_highWater : section.m_current;
    section.m_current = 0u;
    section.m_allocationCount = 0u;

    // May not be valid.
    m_currentSection = (unsigned)oldId - (unsigned)EpMemoryAllocatorId_ScratchPage0;
  }

  virtual bool Contains(void* ptr) override {
    return (uintptr_t)ptr >= m_sections[0].m_begin && (uintptr_t)ptr < m_sections[c_nSections - 1u].m_end;
  }

  virtual uintptr_t GetAllocationCount(EpMemoryAllocatorId id) const override {
    const Section& section = m_sections[CalculateSection(id)];
    return section.m_allocationCount;
  }

  virtual uintptr_t GetBytesAllocated(EpMemoryAllocatorId id) const override {
    const Section& section = m_sections[CalculateSection(id)];
    return section.m_current ? (section.m_current - section.m_begin) : 0u;
  }

  virtual uintptr_t GetBytesRemaining(EpMemoryAllocatorId id) const {
    const Section& section = m_sections[CalculateSection(id)];
    return section.m_current ? (section.m_end - section.m_current) : (section.m_end - section.m_begin);
  }

protected:
  virtual void* OnAlloc(size_t size, uintptr_t alignmentMask) override {
    EpAssert(m_currentSection < c_nSections);
    Section& section = m_sections[m_currentSection];
    uintptr_t aligned = (section.m_current + alignmentMask) & ~alignmentMask;

    EpReleaseAssertMsg(section.m_current != 0u, "no open scope for scratchpad allocator %d", m_currentSection);
    if ((aligned + size) > section.m_end) {
      EpReleaseWarning(false, "%s overflow allocating %d bytes in section %d with %d bytes available", m_label, (int)size, (int)m_currentSection, (int)(section.m_end - section.m_current));
      return 0;
    }

    ++section.m_allocationCount;
    section.m_current = aligned + size;
    return (void*)aligned;
  }

  virtual void OnFree(void* ptr) override {
    EpAssert(Contains(ptr));
  }

  virtual uintptr_t GetHighWater(EpMemoryAllocatorId id) const override {
    const Section& section = m_sections[CalculateSection(id)];
    return section.m_highWater - section.m_begin;
  }

  unsigned CalculateSection(EpMemoryAllocatorId id) const {
    unsigned section = (unsigned)id - (unsigned)EpMemoryAllocatorId_ScratchPage0;
    EpAssert(section < c_nSections);
    return section;
  }

protected:
  unsigned m_currentSection;
  Section m_sections[c_nSections];
};

// ----------------------------------------------------------------------------
// EpMemoryManager

class EpMemoryManager {
public:
  void Construct();
  void Destruct();
  void LogAllocations();

  EpMemoryAllocatorId BeginAllocationScope(EpAllocatorScope* scope, EpMemoryAllocatorId newId);
  void EndAllocationScope(EpAllocatorScope* scope, EpMemoryAllocatorId previousId);

  EpMemoryAllocatorId CurrentAllocatorId() { return m_currentMemoryAllocator; }

  EpMemoryAllocatorBase& GetAllocator(EpMemoryAllocatorId id) {
    EpAssert(m_isInitialized && id >= 0 && id < EpMemoryAllocatorId_MAX);
    return *m_memoryAllocators[id];
  }

  void* Allocate(size_t size);
  void* AllocateExtended(size_t size, uintptr_t alignmentMask, EpMemoryAllocatorId id);
  void Free(void* ptr);

private:
  friend class EpAllocatorScope;
  EpMemoryAllocatorBase* m_memoryAllocators[EpMemoryAllocatorId_MAX];
  EpMemoryAllocatorId m_currentMemoryAllocator;
  bool m_isInitialized; // Statically initialized to zero.
};

// For g_epMemoryAllocatorScratch:
EP_LINK_SCRATCHPAD EpScratchpad<EP_MEMORY_BUDGET_SCRATCH> g_epScratchpadObject;

// MemoryAllocators.
 EpMemoryAllocatorOsHeap    g_epMemoryAllocatorHeap;
 EpMemoryAllocatorStack     g_epMemoryAllocatorPermanent;
 EpMemoryAllocatorStack     g_epMemoryAllocatorResource;
 EpMemoryAllocatorTempStack g_epMemoryAllocatorTemporaryStack;
EpMemoryAllocatorLocked    g_epMemoryAllocatorLocked;
 EpMemoryAllocatorScratchpad g_epMemoryAllocatorScratch;

// Must be explicitly constructed by first global constructor that allocates.  m_currentMemoryAllocator
// will be set to 0/EpMemoryAllocatorId_Heap by virtue of being in the static section.
 EpMemoryManager s_epMemoryManager;

void EpMemoryManager::Construct() {
  EpInit();
#if (EP_MEM_DIAGNOSTIC_LEVEL>=1)
  if (g_epSettings.platform_disableMemoryManager) {
    return;
  }
#endif

  if (m_isInitialized) { return; }
  m_isInitialized = true;

  EpLog("EpMemoryManager.Construct...\n");

  EpAssert(m_currentMemoryAllocator == EpMemoryAllocatorId_Heap); // Static variables set to 0.

  m_memoryAllocators[EpMemoryAllocatorId_Heap] =           &g_epMemoryAllocatorHeap;
  m_memoryAllocators[EpMemoryAllocatorId_Permanent] =      &g_epMemoryAllocatorPermanent;
  m_memoryAllocators[EpMemoryAllocatorId_Resource] =       &g_epMemoryAllocatorResource;
  m_memoryAllocators[EpMemoryAllocatorId_TemporaryStack] = &g_epMemoryAllocatorTemporaryStack;
  m_memoryAllocators[EpMemoryAllocatorId_Locked] =         &g_epMemoryAllocatorLocked;

  for (int i = EpMemoryAllocatorId_ScratchPage0; i != EpMemoryAllocatorId_MAX; ++i) {
    m_memoryAllocators[i] = &g_epMemoryAllocatorScratch;
  }

  g_epMemoryAllocatorHeap.Construct("heap");
  g_epMemoryAllocatorPermanent.Construct(EpMallocChecked(EP_MEMORY_BUDGET_PERMANENT), EP_MEMORY_BUDGET_PERMANENT, "perm");
  g_epMemoryAllocatorResource.Construct(EpMallocChecked(EP_MEMORY_BUDGET_RESOURCE), EP_MEMORY_BUDGET_RESOURCE, "resource");
  g_epMemoryAllocatorTemporaryStack.Construct(EpMallocChecked(EP_MEMORY_BUDGET_TEMPORARY_STACK), EP_MEMORY_BUDGET_TEMPORARY_STACK, "temp");
  g_epMemoryAllocatorLocked.Construct("locked");
  g_epMemoryAllocatorScratch.Construct(g_epScratchpadObject.At(), sizeof g_epScratchpadObject, "scratchpad");
}

void EpMemoryManager::Destruct() {
  EpInit();
#if (EP_MEM_DIAGNOSTIC_LEVEL>=1)
  if (g_epSettings.platform_disableMemoryManager) {
    return;
  }
#endif

#if !defined(EP_BUILD_SOFTWARE)
  EpDebugWarning(false, "Trying to shut down EpMemoryManager on a target device.");
#endif

  EpAssertMsg(g_epMemoryAllocatorPermanent.GetAllocationCount(EpMemoryAllocatorId_Permanent) == 0, "Leaked permanent allocation");
  EpAssertMsg(g_epMemoryAllocatorResource.GetAllocationCount(EpMemoryAllocatorId_Resource) == 0, "Leaked resource allocation");
  EpAssertMsg(g_epMemoryAllocatorTemporaryStack.GetAllocationCount(EpMemoryAllocatorId_TemporaryStack) == 0, "Leaked temporary allocation");

  ::free(g_epMemoryAllocatorPermanent.Release());
  ::free(g_epMemoryAllocatorResource.Release());
  ::free(g_epMemoryAllocatorTemporaryStack.Release());

  m_isInitialized = false;
}

void EpMemoryManager::LogAllocations() {
  EpInit();
#if (EP_MEM_DIAGNOSTIC_LEVEL>=1)
  if (g_epSettings.platform_disableMemoryManager) {
    return;
  }
#endif

  EpLog("MemoryManager listing:\n");
  for (int i = 0; i != EpMemoryAllocatorId_MAX; ++i) {
    const EpMemoryAllocatorBase& al = *m_memoryAllocators[i]; (void)al;
    EpLog(" == %s, count %u, size %u, high_water %u\n", al.Label(),
      (unsigned)al.GetAllocationCount((EpMemoryAllocatorId)i),
      (unsigned)al.GetBytesAllocated((EpMemoryAllocatorId)i),
      (unsigned)al.GetHighWater((EpMemoryAllocatorId)i));
  }
}

EpMemoryAllocatorId EpMemoryManager::BeginAllocationScope(EpAllocatorScope* scope, EpMemoryAllocatorId newId) {
  EpAssert(newId >= 0 && newId < EpMemoryAllocatorId_MAX);
  if (!m_isInitialized) {
    Construct();
  }

  EpAssertMsg(m_currentMemoryAllocator != EpMemoryAllocatorId_Locked, "Begin scope while locked");

  EpMemoryAllocatorId previousId = m_currentMemoryAllocator;
  m_currentMemoryAllocator = newId;
  m_memoryAllocators[m_currentMemoryAllocator]->BeginAllocationScope(scope, newId);
  return previousId;
}

void EpMemoryManager::EndAllocationScope(EpAllocatorScope* scope, EpMemoryAllocatorId previousId) {
  EpAssert(m_isInitialized && previousId >= 0 && previousId < EpMemoryAllocatorId_MAX);

  m_memoryAllocators[m_currentMemoryAllocator]->EndAllocationScope(scope, previousId);
  m_currentMemoryAllocator = previousId;
}

void* EpMemoryManager::Allocate(size_t size) {
  EpInit();
#if (EP_MEM_DIAGNOSTIC_LEVEL>=1)
  if (g_epSettings.platform_disableMemoryManager) {
    return EpMallocChecked(size);
  }
#endif

  // Having a default value for m_currentMemoryAllocator enables a fast path.
  EpAssert(m_isInitialized || m_currentMemoryAllocator == EpMemoryAllocatorId_Heap);
  if (m_currentMemoryAllocator == EpMemoryAllocatorId_TemporaryStack) {
    void* ptr = g_epMemoryAllocatorTemporaryStack.AllocateNonVirtual(size, EP_ALIGNMENT_MASK);
    if (ptr) {
      return ptr; // This is the fast path.
    }
  }

  if (!m_isInitialized) {
    Construct();
  }

  EpAssert(m_currentMemoryAllocator >= 0 && m_currentMemoryAllocator < EpMemoryAllocatorId_MAX);
  void* ptr = m_memoryAllocators[m_currentMemoryAllocator]->Allocate(size, EP_ALIGNMENT_MASK);
  EpReleaseAssertMsg(((uintptr_t)ptr & EP_ALIGNMENT_MASK) == 0, "Alignment wrong %x, al %d", (unsigned)(uintptr_t)ptr, m_currentMemoryAllocator);
  if (ptr) { return ptr; }
  EpReleaseWarning(false, "%s is overflowing to heap, size %d", m_memoryAllocators[m_currentMemoryAllocator]->Label(), (int)size);
  ptr = g_epMemoryAllocatorHeap.Allocate(size, EP_ALIGNMENT_MASK); // May be null.
  EpReleaseAssertMsg(ptr, "Out of memory.");
  EpReleaseAssertMsg(((uintptr_t)ptr & EP_ALIGNMENT_MASK) == 0, "Alignment wrong %x, al-heap", (unsigned)(uintptr_t)ptr);
  return ptr;
}

void* EpMemoryManager::AllocateExtended(size_t size, uintptr_t alignmentMask, EpMemoryAllocatorId id) {
  EpInit();
#if (EP_MEM_DIAGNOSTIC_LEVEL>=1)
  if (g_epSettings.platform_disableMemoryManager) {
    EpAssert(alignmentMask == alignmentMask); // No support for alignment when disabled.
    return EpMallocChecked(size);
  }
#endif

  if (!m_isInitialized) {
    Construct();
  }
  if(id == EpMemoryAllocatorId_UNSPECIFIED) {
    id = m_currentMemoryAllocator;
  }

  EpAssert(((alignmentMask+1) & (alignmentMask)) == 0u);
  EpAssert(id >= 0 && id < EpMemoryAllocatorId_MAX);

  void* ptr = m_memoryAllocators[id]->Allocate(size, alignmentMask);
  EpReleaseAssertMsg(((uintptr_t)ptr & alignmentMask) == 0, "Alignment wrong %x, al %d", (unsigned)(uintptr_t)ptr, id);
  if (ptr) { return ptr; }
  EpReleaseWarning(false, "%s is overflowing to heap, size %d", m_memoryAllocators[id]->Label(), (int)size);
  ptr = g_epMemoryAllocatorHeap.Allocate(size, alignmentMask); // May be null.
  EpReleaseAssertMsg(ptr, "Out of memory.");
  EpReleaseAssertMsg(((uintptr_t)ptr & alignmentMask) == 0, "Alignment wrong %x, al-heap", (unsigned)(uintptr_t)ptr);
  return ptr;
}

void EpMemoryManager::Free(void* ptr) {
  EpInit();
#if (EP_MEM_DIAGNOSTIC_LEVEL>=1)
  if (g_epSettings.platform_disableMemoryManager) {
    ::free(ptr);
    return;
  }
#endif

  EpAssert(m_isInitialized);
  if (g_epMemoryAllocatorTemporaryStack.Contains(ptr)) {
    g_epMemoryAllocatorTemporaryStack.OnFreeNonVirtual(ptr);
    return; // This is the fast path.
  }

  for (int i = EpMemoryAllocatorId_MAX; i--;) {
    if (m_memoryAllocators[i]->Contains(ptr)) {
      m_memoryAllocators[i]->Free(ptr);
      return;
    }
  }

  // Fall though to the Os heap which would have declined ownership above.
  g_epMemoryAllocatorHeap.Free(ptr);
}

// ----------------------------------------------------------------------------
// EpAllocatorScope

EpAllocatorScope::EpAllocatorScope(EpMemoryAllocatorId id)
{
  EpInit();
#if (EP_MEM_DIAGNOSTIC_LEVEL>=1)
  if (g_epSettings.platform_disableMemoryManager) {
    m_thisId = id;
    m_previousId = EpMemoryAllocatorId_UNSPECIFIED;
    m_previousAllocationCount = 0;
    m_previousBytesAllocated = 0;
    return;
  }
#endif

  // Sets CurrentAllocator():
  m_thisId = id;
  m_previousId = s_epMemoryManager.BeginAllocationScope(this, id);
  EpMemoryAllocatorBase& al = s_epMemoryManager.GetAllocator(id);
  m_previousAllocationCount = al.GetAllocationCount(id);
  m_previousBytesAllocated = al.GetBytesAllocated(id);
#if (EP_MEM_DIAGNOSTIC_LEVEL>=2)
  EpLog(" => %s, count %d, size %d\n", al.Label(), (int)GetTotalAllocationCount(), (int)GetTotalBytesAllocated());
#endif
}

EpAllocatorScope::~EpAllocatorScope() {
  EpInit();
#if (EP_MEM_DIAGNOSTIC_LEVEL>=1)
  if (g_epSettings.platform_disableMemoryManager || m_previousId == EpMemoryAllocatorId_UNSPECIFIED) {
    return;
  }
#if (EP_MEM_DIAGNOSTIC_LEVEL>=2)
  EpLog(" <= %s, count %d/%d, size %d/%d\n", s_epMemoryManager.GetAllocator(m_thisId).Label(), (int)GetScopeAllocationCount(), (int)GetTotalAllocationCount(), (int)GetScopeBytesAllocated(), (int)GetTotalBytesAllocated());
#endif
#endif
  s_epMemoryManager.EndAllocationScope(this, m_previousId);
}

uintptr_t EpAllocatorScope::GetTotalAllocationCount() const {
  EpInit();
#if (EP_MEM_DIAGNOSTIC_LEVEL>=1)
  if (g_epSettings.platform_disableMemoryManager) {
    return 0;
  }
#endif
  return s_epMemoryManager.GetAllocator(m_thisId).GetAllocationCount(m_thisId);
}

uintptr_t EpAllocatorScope::GetTotalBytesAllocated() const {
  EpInit();
#if (EP_MEM_DIAGNOSTIC_LEVEL>=1)
  if (g_epSettings.platform_disableMemoryManager) {
    return 0;
  }
#endif
  return s_epMemoryManager.GetAllocator(m_thisId).GetBytesAllocated(m_thisId);
}

uintptr_t EpAllocatorScope::GetScopeAllocationCount() const {
  EpInit();
#if (EP_MEM_DIAGNOSTIC_LEVEL>=1)
  if (g_epSettings.platform_disableMemoryManager) {
    return 0;
  }
#endif
  return s_epMemoryManager.GetAllocator(m_thisId).GetAllocationCount(m_thisId) - m_previousAllocationCount;
}

uintptr_t EpAllocatorScope::GetScopeBytesAllocated() const {
  EpInit();
#if (EP_MEM_DIAGNOSTIC_LEVEL>=1)
  if (g_epSettings.platform_disableMemoryManager) {
    return 0;
  }
#endif
  return s_epMemoryManager.GetAllocator(m_thisId).GetBytesAllocated(m_thisId) - m_previousBytesAllocated;
}

// ----------------------------------------------------------------------------
// new, delete and C API

void* EpMalloc(size_t size) {
  return s_epMemoryManager.Allocate(size);
}

void* EpMallocExtended(size_t size, uintptr_t alignmentMask, int memoryAllocatorId) {
  return s_epMemoryManager.AllocateExtended(size, alignmentMask, (EpMemoryAllocatorId)memoryAllocatorId);
}

void EpFree(void *ptr) {
  s_epMemoryManager.Free(ptr);
}

void EpMemoryManagementInit() {
  // Contains platform_disableMemoryManager checks
  s_epMemoryManager.Construct();
}

void EpMemoryManagementShutDown() {
  // Contains platform_disableMemoryManager checks
  s_epMemoryManager.Destruct();
}

void EpMemoryManagementLog() {
  // Contains platform_disableMemoryManager checks
  s_epMemoryManager.LogAllocations();
}

bool EpIsScratchpad(void * ptr) {
  return g_epMemoryAllocatorScratch.Contains(ptr);
}

#else // (EP_MEM_DIAGNOSTIC_LEVEL == -1)

EpAllocatorScope::EpAllocatorScope(EpMemoryAllocatorId id)
{
  m_previousId = EpMemoryAllocatorId_Heap;
  m_previousAllocationCount = 0;
  m_previousBytesAllocated = 0;
}

EpAllocatorScope::~EpAllocatorScope() { }

uintptr_t EpAllocatorScope::GetTotalAllocationCount() const { return 0; }

uintptr_t EpAllocatorScope::GetTotalBytesAllocated() const { return 0; }

uintptr_t EpAllocatorScope::GetScopeAllocationCount() const { return 0; }

uintptr_t EpAllocatorScope::GetScopeBytesAllocated() const { return 0; }

// ----------------------------------------------------------------------------

void* EpMalloc(size_t size) { return EpMallocChecked(size); }

// TODO: EpMallocExtended unsupported.

void EpFree(void *ptr) { ::free(ptr); }

void EpMemoryManagementInit() { }

void EpMemoryManagementShutDown() { }

void EpMemoryManagementLog() { }

bool EpIsScratchpad(void * ptr) { return false; }

#endif // (EP_MEM_DIAGNOSTIC_LEVEL == -1)


