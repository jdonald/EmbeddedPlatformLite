#pragma once

#include "EmbeddedPlatform.h"

// ----------------------------------------------------------------------------
// EpSettings

struct EpSettings {
public:
  void Construct();

  bool platform_runTestsInMain;
  bool platform_isShuttingDown; // Allows destruction of permanent resources
  bool platform_isLogging;
  int platform_assertsAllowed;
  bool platform_disableMemoryManager;
};

// Constructed by EpInit().
extern EpSettings g_epSettings;
