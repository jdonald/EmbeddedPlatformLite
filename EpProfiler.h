#pragma once

#include "EmbeddedPlatform.h"
#include "EpArray.h"

// TODO: Set the processor cycles per microsecond here.
#define EP_CYCLES_PER_MICROSECOND 600

#define EP_PROFILER_MAX_RECORDS 600

// labelStaticString: must be a static string
// minCycles: minimum number of cycles taken before a timing is logged.
void EpProfileScope(const char* labelStaticString, unsigned minCycles=0u);

unsigned EpProfilerSampleInternal();

class EpProfilerRecord {
public:
  EP_FORCEINLINE EpProfilerRecord(unsigned begin, unsigned end, const char* label) :
    m_begin(begin), m_end(end), m_label(label) {
  }

  unsigned m_begin;
  unsigned m_end;
  const char* m_label;
};

// Used when passing EpProfilerRecord between cores.
class EpProfilerRecordExternal {
public:
  enum { LABEL_SIZE = 16 };
  unsigned m_begin;
  unsigned m_end;
  char m_label[LABEL_SIZE];
};

// ----------------------------------------------------------------------------------
#if (EP_PROFILE==1)

class EpProfilerData {
public:
  EpProfilerData() {
    m_isEnabled = false;
  }
  bool m_isEnabled;
  EpArray<EpProfilerRecord> m_records;
};

extern EpProfilerData ep_sProfilerData;

// EpProfilerSample
static EP_FORCEINLINE unsigned EpProfilerSample() {
#if defined(EP_BUILD_SOME_EMBEDDED_COMPILER)
#error "TODO"
#else
  return EpProfilerSampleInternal();
#endif
}

class EpProfiler {
public:
  // WARNING: A pointer to labelStaticString is kept.
  EP_FORCEINLINE EpProfiler(const char* labelStaticString, unsigned minCycles = 0u) : m_label(labelStaticString), m_minCycles(minCycles) {
    m_t0 = EpProfilerSample();
  }

  EP_FORCEINLINE ~EpProfiler() {
    unsigned t1 = EpProfilerSample();
    unsigned delta = (t1 - m_t0);
    EpProfilerData& data = ep_sProfilerData;
    if (data.m_isEnabled && !data.m_records.full() && delta >= m_minCycles) {
      new (data.m_records.emplace_back_raw()) EpProfilerRecord(m_t0, t1, m_label);
    }
  }

private:
  EpProfiler();
  EpProfiler(const EpProfiler&);
  const char* m_label;
  unsigned m_minCycles;
  unsigned m_t0;
};


void EpProfilerInit();
void EpProfilerShutdown();
void EpProfilerLog(); // clears buffer
unsigned EpProfilerQuery(EpProfilerRecordExternal* buf, unsigned maxSize); // clears buffer, returns count

#define EpProfileScope(...)  EpProfiler EP_CONCATENATE(epProfiler_,__LINE__)(__VA_ARGS__)

// ----------------------------------------------------------------------------------
#else
#define EpProfilerInit(...) ((void)0)
#define EpProfilerShutdown(...) ((void)0)
#define EpProfilerLog(...) ((void)0)
#define EpProfileScope(...) ((void)0)
#endif
// ----------------------------------------------------------------------------------

struct EpProfilerClientCommand {
  EpProfilerRecordExternal* m_records;
  unsigned m_maxRecords;
  unsigned* m_size;
};

struct EpProfilerSync {
  unsigned* m_DSPtime;
};
