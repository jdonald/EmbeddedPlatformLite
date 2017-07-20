#include "EmbeddedPlatform.h"
#include "EpTest.h"
#include "EpAllocatorScope.h"
#include "EpSettings.h"


class EpMainTest :
  public testing::Test
{
public:
};

void EpTestMemoryAllocatorNormal(EpMemoryAllocatorId id) {
    uintptr_t startCount;
    uintptr_t startBytes;

  {
    EpLog("EpTestMemoryAllocatorNormal %d...\n", id);
    EpAllocatorScope resourceAllocator(id);

    startCount = resourceAllocator.GetTotalAllocationCount();
    startBytes = resourceAllocator.GetTotalBytesAllocated();

    void* ptr1 = EpMalloc(100);
    void* ptr2 = EpMalloc(200);
    ::memset(ptr1, 0xfe, 100);
    ::memset(ptr2, 0xfe, 200);

    {
      // GoogleTest spams new/delete with std::string operations:
      EpAllocatorScope spamGuard(EpMemoryAllocatorId_Heap);
      ASSERT_EQ(resourceAllocator.GetScopeAllocationCount(), 2u);
      ASSERT_EQ(resourceAllocator.GetPreviousAllocationCount(), startCount);
      ASSERT_EQ(resourceAllocator.GetTotalAllocationCount(), 2u + startCount);
      ASSERT_NEAR(resourceAllocator.GetScopeBytesAllocated(), 300u, 2u * EP_ALIGNMENT_MASK);
      ASSERT_NEAR(resourceAllocator.GetTotalBytesAllocated(), startBytes + 300u, 2u * EP_ALIGNMENT_MASK);
      ASSERT_EQ(resourceAllocator.GetPreviousBytesAllocated(), startBytes);
    }

    // Allow quiet deletion of a resource.
    g_epSettings.platform_isShuttingDown = true;
    EpFree(ptr1);
    EpFree(ptr2);
    g_epSettings.platform_isShuttingDown = false;

    // Special case for heaps that do not track free.
    if (resourceAllocator.GetScopeBytesAllocated() != 0) {
      // GoogleTest spams new/delete with std::string operations:
      EpAllocatorScope spamGuard(EpMemoryAllocatorId_Heap);

      // The debug heap requires EP_ALLOCATIONS_LOG_LEVEL enabled to track bytes allocated.
      ASSERT_NEAR(resourceAllocator.GetScopeBytesAllocated(), 300, 2 * EP_ALIGNMENT_MASK);
    }
    else {
      EpAllocatorScope spamGuard(EpMemoryAllocatorId_Heap);
      ASSERT_EQ(resourceAllocator.GetScopeBytesAllocated(), 0u);
      ASSERT_EQ(resourceAllocator.GetTotalBytesAllocated(), startBytes);
    }
  }

  // EpMemoryAllocatorId_Permanent and EpMemoryAllocatorId_Resource do not free.
  if (id != EpMemoryAllocatorId_Permanent && id != EpMemoryAllocatorId_Resource) {
    EpAllocatorScope resourceAllocator(id);

    // GoogleTest spams new/delete with std::string operations:
    EpAllocatorScope spamGuard(EpMemoryAllocatorId_Heap);
    ASSERT_EQ(resourceAllocator.GetPreviousAllocationCount(), startCount);
    ASSERT_EQ(resourceAllocator.GetPreviousBytesAllocated(), startBytes);
  }
}

void EpTestMemoryAllocatorLeak(EpMemoryAllocatorId id) {
  uintptr_t startCount = 0;
  uintptr_t startBytes = 0;
  void* ptr2 = NULL;
  int assertsAllowed = g_epSettings.platform_assertsAllowed;

  {
    EpAllocatorScope resourceAllocator(id);

    startCount = resourceAllocator.GetScopeAllocationCount();
    startBytes = resourceAllocator.GetScopeBytesAllocated();

    void* ptr1 = EpMalloc(100);
    ptr2 = EpMalloc(200);
    ::memset(ptr1, 0xfe, 100);
    ::memset(ptr2, 0xfe, 200);

    EpFree(ptr1); // Only free the one.

    g_epSettings.platform_assertsAllowed = 1;
    EpLog("EXPECTING FAILURE:\n");
  }
#if (EP_DEBUG==1)
  // Assert would be skipped when EP_DEBUG is off.
  ASSERT_EQ(g_epSettings.platform_assertsAllowed, 0); // EpAssert was hit, leak in scope
#endif

  EpAllocatorScope resourceAllocator(id);
  {
    // GoogleTest spams new/delete with std::string operations:
    EpAllocatorScope spamGuard(EpMemoryAllocatorId_Heap);
    ASSERT_EQ(resourceAllocator.GetPreviousAllocationCount(), startCount);
    ASSERT_EQ(resourceAllocator.GetPreviousBytesAllocated(), startBytes);
  }

  g_epSettings.platform_assertsAllowed = 1;
  EpFree(ptr2);
  int result = g_epSettings.platform_assertsAllowed; (void)result;
  g_epSettings.platform_assertsAllowed = assertsAllowed;
#if (EP_DEBUG==1)
  ASSERT_EQ(result, 0); // EpAssert was hit, free after scope closed
#endif

  EpLog("EpTestMemoryAllocatorLeak %d...\n", id);
}

TEST_F(EpMainTest, Execute) {
  bool wasDisabled = g_epSettings.platform_disableMemoryManager;
  {
    g_epSettings.platform_disableMemoryManager = false;

    // GoogleTest spams the allocators:
    EpAllocatorScope spamGuard(EpMemoryAllocatorId_Heap);

    for (int i = 0; i < EpMemoryAllocatorId_MAX; ++i) {
      if (i == EpMemoryAllocatorId_Locked) {
        continue;
      }
      EpTestMemoryAllocatorNormal((EpMemoryAllocatorId)i);
    }

    // Only the TemporaryStack expects all allocations to be free()'d.
    EpTestMemoryAllocatorLeak(EpMemoryAllocatorId_TemporaryStack);
  }

  if (wasDisabled) {
    g_epSettings.platform_disableMemoryManager = true;
#ifndef EP_BUILD_SOFTWARE
    EpReleaseAssertMsg(false, "EpMemoryManagementShutDown in tests on target.");
#endif
    EpMemoryManagementShutDown();
  }
}
