#include "EpDma.h"
#include "EpProfiler.h"
#include "EpArray.h"

#include <string.h>

//#define USING_SOME_DMA_DRIVER

#if (EP_DEBUG_DMA==1)
struct DmaDebugRecord {
  DmaDebugRecord(const void* d, const void* s, unsigned b, unsigned c, const char* l) :
    dst(d), src(s), bytes(b), counter(c), label(l) { }
  const void* dst;
  const void* src;
  unsigned bytes;
  unsigned counter;
  const char* label;
};
static EpArray<DmaDebugRecord, 16> s_epDmaDebugRecords;
static unsigned int s_epDmaCounter = 0;
#endif

void EpDmaInit() {
#ifdef USING_SOME_DMA_DRIVER
#error "TODO"
#endif
}

void EpDmaShutDown() {
#ifdef USING_SOME_DMA_DRIVER
#error "TODO"
#endif
}

void EpDmaRecycleBarriers() {
  EpDmaAwait();
#ifdef USING_SOME_DMA_DRIVER
#error "TODO"
#endif
#if (EP_DEBUG_DMA==1)
  EpReleaseAssertMsg(s_epDmaDebugRecords.empty(), "dma unrequested work");
  s_epDmaCounter = 0;
#endif
}

void EpDmaStartLabeled(void* dst, const void* src, size_t bytes, const char* label) {
  label = label ? label : "EpDmaStart";
  EpReleaseAssertMsg(src != 0 && dst != 0 && bytes != 0, "%s(0x%x, 0x%x, 0x%x): dma illegal args", label, (unsigned)dst, (unsigned)src, (unsigned)bytes);
#ifdef USING_SOME_DMA_DRIVER
#else
  ::memcpy(dst, src, bytes);
#endif
#if (EP_DEBUG_DMA==1)
  s_epDmaDebugRecords.push_back(DmaDebugRecord(dst, src, bytes, s_epDmaCounter, label));
#endif
}

void EpDmaAddBarrier(EpDmaBarrier& barrier) {
#ifdef USING_SOME_DMA_DRIVER
#error "TODO"
#endif
#if (EP_DEBUG_DMA==1)
  barrier.debug = s_epDmaCounter;
#endif
}

void EpDmaAwaitBarrierLabeled(EpDmaBarrier& barrier, const char* label) {
  EpProfileScope((label ? label : "EpDma"), EP_CYCLES_PER_MICROSECOND); // Ignore less than 1 us.
#ifdef USING_SOME_DMA_DRIVER
#error "TODO"
#endif
#if (EP_DEBUG_DMA==1)
  EpReleaseAssertMsg(barrier.debug <= s_epDmaCounter, "dma barrier corrupt: %s", label);
  for(DmaDebugRecord* it = (s_epDmaDebugRecords.end() - 1); it >= s_epDmaDebugRecords.begin(); --it ) {
  if(it->counter >= barrier.debug) {
      bool isOk = ::memcmp(it->dst, it->src, it->bytes) == 0;
    EpReleaseWarning(isOk, "%s: <-- data CORRUPTED", it->label);
    EpReleaseAssertMsg(isOk, "%s: <-- barrier CORRUPTED", label);
    s_epDmaDebugRecords.erase_unordered(it);
    }
  }
#endif
}

void EpDmaAwaitLabeled(const char* label) {
  EpDmaBarrier b;
  EpDmaAddBarrier(b);
  EpDmaAwaitBarrierLabeled(b, label);
#if (EP_DEBUG_DMA==1)
  EpReleaseAssertMsg(s_epDmaDebugRecords.empty(), "dma await failed: %s", label);
#endif
}
