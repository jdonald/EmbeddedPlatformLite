#include "EmbeddedPlatform.h"
#include "EpTest.h"
#include "EpAllocatorScope.h"
#include "EpSettings.h"
#include "EpDma.h"
#include "EpProfiler.h"


#include <stdio.h>
#include <stdlib.h>

#include <time.h>
#if defined(_MSC_VER)
#include <intrin.h>
#endif

#define EP_LOG_MAX 256

// ----------------------------------------------------------------------------

struct EpIeeeFloat {
  static const unsigned EXP_INFNAN = 255;

  unsigned m_frac : 23;
  unsigned m_exp : 8;
  unsigned m_sign : 1;
};

 float g_epNAN;
 bool s_epIsInit = false;
static const char* s_epInitFile = "(null)"; // For trapping code running before EpMain.
static unsigned s_epInitLine = 0;

// ----------------------------------------------------------------------------

#ifndef EP_USING_GOOGLE_TEST
static EpTestRunner s_epTestRunner;
 static bool s_epTestRunnerIsInit = false;
EpTestRunner& EpTestRunner::Singleton() {

  if(!s_epTestRunnerIsInit) {
    s_epTestRunnerIsInit = true;
    s_epTestRunner.Construct();
  }
  return s_epTestRunner;
}
#endif // EP_BUILD_SOME_EMBEDDED_COMPILER

// ----------------------------------------------------------------------------

void EpInitAt(const char* file, unsigned line) {
  EpAssert(!s_epIsInit);
  s_epIsInit = true; // Guard recursion immediately

  if(file) { s_epInitFile = file; }
  if(line) { s_epInitLine = line; }

  EpIeeeFloat nan = { 1, EpIeeeFloat::EXP_INFNAN, 0 };
  g_epNAN = EpAliasingCast<float>(nan);

  g_epSettings.Construct();
  EpMemoryManagementInit();
  EpDmaInit();
}

void EpShutdown() {
  EpDmaShutDown();
  EpProfilerShutdown();

  // This is only for tests.
  g_epSettings.platform_isShuttingDown = true;
  EpMemoryManagementShutDown();
  g_epSettings.platform_disableMemoryManager = true;
}

void EpExit(const char* msg) {
  ::fprintf(stderr, "STOPPING.  %s", msg ? msg : "");

  // Stop here before the callstack gets lost inside exit.  This is not for
  // normal termination on an embedded target.
#if defined(_MSC_VER)
  __debugbreak();
#endif
  ::abort();
}

void EpAssertHandler(const char* file, unsigned line) {
  if (g_epSettings.platform_assertsAllowed-- > 0) {
    return;
  }
  EpLog("[Embedded Platform] %s(%u): ASSERT_FAIL\n", file, line);

  // Logging may be disabled or compiled out.
  char buf[EP_LOG_MAX*2];
  ::sprintf(buf, "%s(%u)", file, line);
  EpExit(buf);
}

void EpLogHandler(EpLogLevel level, const char* format, ...) {
  va_list args;
  va_start(args, format);

  EpLogHandlerV(level, format, args);

  va_end(args);
}

void EpLogHandlerV(EpLogLevel level, const char* format, va_list args) {
  if (!g_epSettings.platform_isLogging) {
    return;
  }

  char buf[EP_LOG_MAX*2];
  int sz = ::vsprintf(buf, format, args);
  if (sz <= 0 || sz > EP_LOG_MAX) {
  ::fprintf(stderr, "WARNING: Message too long: %s", format);
    return;
  }

  if (level == EpLogLevel_Warning) {
    ::fwrite("WARNING: ", 1, sizeof "WARNING: " - 1, stderr);
    buf[sz++] = '\n';
  }
  else if (level == EpLogLevel_Assert) {
    ::fwrite("ASSERT_FAIL: ", 1, sizeof "ASSERT_FAIL: " - 1, stderr);
    buf[sz++] = '\n';
  }
  ::fwrite(buf, 1, sz, stderr);
}

void EpLogStatus() {
  EpLog((s_epIsInit ? "init at %s(%u)\n" : "NOT INIT\n"), s_epInitFile, s_epInitLine);
}

void EpHexDump(const void *p, unsigned bytes, const char* label) {
  unsigned char* ptr = (unsigned char*)p; (void)ptr;
  EpLog(" ========= %s (%u bytes) =========\n", label, bytes);
  for (unsigned i = 0; i < bytes;) {
    EpLog(" %08x  ", (uintptr_t)(ptr + i));
    for (int max=4; i < bytes && max--; i += 4) {
      EpLog("%08x ", *(unsigned*)(ptr + i));
    }
    EpLog("\n");
  }
}

void EpFloatDump(const float *ptr, unsigned count, const char* label) {
  EpLog(" ========= %s (%u values) =========\n", label, count);
  for (unsigned i = 0; i < count;) {
    EpLog(" %08x  ", (uintptr_t)(ptr + i));
    for (int max=4; i < count && max--; i++) {
      EpLog("%8f ", ptr[i]);
    }
    EpLog("\n");
  }
}

bool EpIsFinite(float f) {
  EpIeeeFloat ie3f = EpAliasingCast<EpIeeeFloat>(f);
  return ie3f.m_exp != EpIeeeFloat::EXP_INFNAN;
}

const char* EpBasename(const char* path)
 {
  const char* result = path;
  for(const char* it=path; *it !='\0'; ++it ) {
    if(*it == '/') { result = it + 1; }
  }

  return result;
}
