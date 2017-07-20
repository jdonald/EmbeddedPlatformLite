#pragma once

#include "EmbeddedPlatform.h"

// ----------------------------------------------------------------------------
// DMA

// Set to 1 or 0 as needed
#define EP_DEBUG_DMA EP_DEBUG

struct EpDmaBarrier {
  unsigned int value;
#if (EP_DEBUG_DMA==1)
  unsigned int debug;
#endif
};

void EpDmaInit();
void EpDmaShutDown();

// Must be called once a frame while no DMA is in flight.
void EpDmaRecycleBarriers();

// Initiates a DMA transfer from src to dst of bytes length.  An async ::memcpy.
void EpDmaStartLabeled(void* dst, const void* src, size_t bytes, const char* label=0);

// Introduces a barrier in the DMA command stream.  The EpDmaBarrier object itself
// will not be modified when that barrier is reached.
void EpDmaAddBarrier(EpDmaBarrier& barrier);

// Waits until all DMA proceeding call to EpDmaGetBarrier is completed.
void EpDmaAwaitBarrierLabeled(EpDmaBarrier& barrier, const char* label=0);

// Waits until all DMA is completed.
void EpDmaAwaitLabeled(const char* label=0);


#if (EP_PROFILE==1)
#define EpDmaStart(dst, src, bytes) EpDmaStartLabeled(dst, src, bytes, __FILE__ "(" EP_QUOTE(__LINE__) ") start dma")
#define EpDmaAwaitBarrier(barrier) EpDmaAwaitBarrierLabeled(barrier, __FILE__ "(" EP_QUOTE(__LINE__) ") wait dma")
#define EpDmaAwait() EpDmaAwaitLabeled(__FILE__ "(" EP_QUOTE(__LINE__) ") wait dma")
#else
#define EpDmaStart EpDmaStartLabeled
#define EpDmaAwaitBarrier EpDmaAwaitBarrierLabeled
#define EpDmaAwait EpDmaAwaitLabeled
#endif
