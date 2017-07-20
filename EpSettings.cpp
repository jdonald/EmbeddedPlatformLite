
#include "EmbeddedPlatform.h"
#include "EpSettings.h"

EpSettings g_epSettings;

void EpSettings::Construct() {
  platform_runTestsInMain = true; // Must disable before EpMain().
  platform_isShuttingDown = false;
  platform_assertsAllowed = 0;
#if defined(EP_BUILD_SOME_EMBEDDED_COMPILER)
  platform_isLogging = false; // Logging from constructors was crashing.
  platform_disableMemoryManager = true;
#else
  platform_isLogging = true;
  platform_disableMemoryManager = false;
#endif
}

