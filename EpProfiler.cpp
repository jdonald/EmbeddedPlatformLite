#include "EpProfiler.h"
#include "EpArray.h"

#include <string.h>

#if (EP_PROFILE==1)

 EpProfilerData ep_sProfilerData;

// C++11 version
#ifndef EP_BUILD_SOME_EMBEDDED_COMPILER
#include <chrono>

static std::chrono::high_resolution_clock::time_point g_epStart;

unsigned EpProfilerSampleInternal() {
  return (unsigned)(std::chrono::high_resolution_clock::now() - g_epStart).count();
}
#endif // !EP_BUILD_SOME_EMBEDDED_COMPILER

void EpProfilerInit() {
  EpProfilerData& data = ep_sProfilerData;
  if (data.m_isEnabled) {
    return;
  }
  EpLogHandler(EpLogLevel_Log, "EpProfilerInit... %u cycles\n", EpProfilerSample()); // Logging may easily be off at this point.

  data.m_isEnabled = true;
  data.m_records.reserve(EP_PROFILER_MAX_RECORDS);

#if defined(EP_BUILD_SOME_EMBEDDED_COMPILER)
#error "TODO"
#else
  g_epStart = std::chrono::high_resolution_clock::now();
#endif
}

void EpProfilerShutdown() {
  EpProfilerData& data = ep_sProfilerData;
  data.m_records.clear();
  data.m_isEnabled = false;
  EpLogHandler(EpLogLevel_Log, "EpProfilerShutdown... %u cycles\n", EpProfilerSample());
}

void EpProfilerLog() {
  EpProfilerData& data = ep_sProfilerData;
  if (!data.m_isEnabled) {
    EpProfilerInit();
    EpDebugWarning(false, "Error unexpected profiler init... ");
  }

  for (unsigned i = 0; i < data.m_records.size(); ++i) {
    const EpProfilerRecord& rec = data.m_records[i];

    unsigned delta = rec.m_end - rec.m_begin;
    EpLog("EpProfiler %s: %u cycles %f ms\n", EpBasename(rec.m_label), delta, (float)delta / (EP_CYCLES_PER_MICROSECOND * 1000.0f));
  }

  if(data.m_records.empty()) {
    EpLog("EpProfiler no samples\n");
  }

  data.m_records.clear();
}

unsigned EpProfilerQuery(EpProfilerRecordExternal* buf, unsigned maxSize) {
  EpProfilerData& data = ep_sProfilerData;

  if (!data.m_isEnabled) {
    EpProfilerInit();
    EpDebugWarning(false, "Error unexpected profiler init... ");
  }

  unsigned size = EpMin(data.m_records.size(), maxSize);

  // Copy live data into a buffer to send back...
  for (unsigned i = 0; i < size; ++i) {
    const EpProfilerRecord& rec = data.m_records[i];
    EpProfilerRecordExternal& ext = buf[i];

    ext.m_begin = rec.m_begin;
    ext.m_end = rec.m_end;

    const char* src = EpBasename(rec.m_label);
    char* dst = ext.m_label;

    // safe portable strncpy.
    int j = 0;
    for (; j < (EpProfilerRecordExternal::LABEL_SIZE-1) && src[j] != '\0'; ++j) {
      dst[j] = src[j];
    }
    dst[j] = '\0';
    EpAssert(::strlen(dst) < EpProfilerRecordExternal::LABEL_SIZE);
  }

  data.m_records.clear();
  return size;
}

#endif // (EP_PROFILE==1)
