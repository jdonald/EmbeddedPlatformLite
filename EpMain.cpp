#include "EmbeddedPlatform.h"
#include "EpAllocatorScope.h"
#include "EpTest.h"
#include "EpProfiler.h"
#include "EpSettings.h"
#include "EpArray.h"
#include "EpUniquePtr.h"


#ifndef EP_USING_GOOGLE_TEST
void EpMain() {
  EpProfilerInit(); // Enabled by EpCommandId_Profiler when run externally.

  // EP_BUILD_SOFTWARE uses GoogleTest instead.
  if (g_epSettings.platform_runTestsInMain) {
    //  EpTestRunner::Singleton().SetFilterStaticString("EpArrayTest");

    // All tests already registered by global constructors.
    EpTestRunner::Singleton().ExecuteAllTests();
  }
}

extern "C" {
void main() {
  EpMain();
}

} // extern "C"
#endif // !EP_USING_GOOGLE_TEST
