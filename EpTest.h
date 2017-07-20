#pragma once

#include <string.h>
#include "EpAllocatorScope.h"
#include "EpProfiler.h"

// Enable this to use GoogleTest
#if 0 //def EP_BUILD_SOFTWARE
#include <gtest/gtest.h>
#define EP_USING_GOOGLE_TEST
#else

namespace testing {
class Test {
  virtual void EpTest_Execute(void) = 0;
};
} // namespace testing

// ----------------------------------------------------------------------------
struct EpTestRunner {
public:
  enum TestState {
    TEST_NOTHING_ASSERTED,
    TEST_PASS,
    TEST_FAIL,
    MAX_TESTS = 256
  };

  struct FactoryBase {
    virtual void EpTest_ConstructAndExecute(void) = 0;
    virtual const char* EpTest_ClassName(void) = 0;
    virtual const char* EpTest_FunctionName(void) = 0;
    virtual const char* EpTest_File(void) = 0;
    virtual int EpTest_Line(void) = 0;
  };

  // Ensure this constructor runs before tests are registered by other global constructors.
  static EpTestRunner& Singleton();

  void Construct() {
    mNumFactories = 0;
    mCurrentTest = 0;
    mFilterClassName = NULL;
  }

  void SetFilterStaticString(const char* className) { mFilterClassName = className; }
  void AddTest(const char* className, FactoryBase* fn) {
    if(mNumFactories < MAX_TESTS) {
      mFactories[mNumFactories++] = fn;
    }
  }

  void Assert(const char* file, int line, bool condition, const char* format, ... ) {
    mTestState = (condition && mTestState != TEST_FAIL) ? TEST_PASS : TEST_FAIL;
    if (!condition) {
      EpLog("ASSERT FAIL: %s.%s\n", mCurrentTest->EpTest_ClassName(), mCurrentTest->EpTest_FunctionName());
      EpLog("ASSERT FAIL: %s(%d)\n", file, line);

      va_list args;
      va_start(args, format);
      EpLogHandlerV(EpLogLevel_Assert, format, args);
      va_end(args);
    }
  }

  void ExecuteAllTests() {
    EpReleaseWarning(EP_DEBUG, "Running tests with EP_DEBUG off");
    EpProfilerInit();

    mPassCount = mFailCount = 0;
    EpLog("EpTestRunner: %s...\n", (mFilterClassName ? mFilterClassName : "All"));
    EpLog("--------\n");
    for (FactoryBase** it = mFactories; it != (mFactories+mNumFactories); ++it) {
      if (mFilterClassName == NULL || ::strcmp(mFilterClassName, (*it)->EpTest_ClassName()) == 0) {
        EpLog("%s.%s...\n", (*it)->EpTest_ClassName(), (*it)->EpTest_FunctionName());

        mTestState = TEST_NOTHING_ASSERTED;
        mCurrentTest = *it;

        {
          // Tests should have no side effects.  Therefore all allocations should be safe to reset.
          EpProfileScope((*it)->EpTest_FunctionName());
          EpAllocatorScope testTempScope(EpMemoryAllocatorId_TemporaryStack);
          (*it)->EpTest_ConstructAndExecute();
        }

        if (mTestState == TEST_NOTHING_ASSERTED) {
          Assert((*it)->EpTest_File(), (*it)->EpTest_Line(), false, "Nothing was asserted!");
        }
        if (mTestState == TEST_PASS) {
          ++mPassCount;
        } else {
          ++mFailCount;
        }

        EpProfilerLog();
      } else {
        EpLog("Skipping %s.%s..\n", (*it)->EpTest_ClassName(), (*it)->EpTest_FunctionName());
      }
    }
    EpLog("--------\n");
    if (mPassCount > 0 && mFailCount == 0) {
      EpLog("EpTestRunner: All %d tests PASSED SUCCESSFULLY.\n", mPassCount);
    } else {
      EpLog("EpTestRunner TESTS FAILED: %d tests FAILED out of %d.\n", mFailCount, mFailCount+mPassCount);
    }
    EpProfilerShutdown(); // Logs cycle count for comparison with timing in kernel logs.
  }

private:
  FactoryBase* mFactories[MAX_TESTS];
  int mNumFactories;
  TestState mTestState;
  int mPassCount;
  int mFailCount;
  FactoryBase* mCurrentTest;
  const char* mFilterClassName;
};

// ----------------------------------------------------------------------------
// TestClassName must subclass testing::Test.

#define TEST_F(TestClassName, TestFunctionName) \
struct EP_CONCATENATE(TestClassName, TestFunctionName) : EpTestRunner::FactoryBase { \
  struct TestExecutor : TestClassName { virtual void EpTest_Execute(); }; \
  EP_CONCATENATE(TestClassName, TestFunctionName)(void) { EpTestRunner::Singleton().AddTest(#TestClassName, this);  } \
  virtual void EpTest_ConstructAndExecute(void) { \
    TestExecutor executor; \
    executor.EpTest_Execute(); \
  } \
  virtual const char* EpTest_ClassName(void) { return #TestClassName; } \
  virtual const char* EpTest_FunctionName(void) { return #TestFunctionName; } \
  virtual const char* EpTest_File(void) { return __FILE__; } \
  virtual int EpTest_Line(void) { return __LINE__; } \
}; \
static EP_CONCATENATE(TestClassName, TestFunctionName) EP_CONCATENATE(s_epTest_, TestFunctionName); \
void EP_CONCATENATE(TestClassName, TestFunctionName)::TestExecutor::EpTest_Execute(void) // { Test code follows:

// ----------------------------------------------------------------------------

#define ASSERT_TRUE(a) EpTestRunner::Singleton().Assert(__FILE__, __LINE__, (a), #a" : %d", (int)a)
#define ASSERT_FALSE(a) EpTestRunner::Singleton().Assert(__FILE__, __LINE__, !(a), "!("#a") : %d", (int)a)
#define ASSERT_NEAR(a, b, c) EpTestRunner::Singleton().Assert(__FILE__, __LINE__, EpAbs((a)-(b)) <= c, "abs("#a" - "#b") <= "#c" : %g %g %g", (float)a,  (float)b,  (float)c)
#define ASSERT_EQ(a, b) EpTestRunner::Singleton().Assert(__FILE__, __LINE__, (a) == (b), #a" == "#b" : %g %g", (float)a, (float)b)
#define ASSERT_LE(a, b) EpTestRunner::Singleton().Assert(__FILE__, __LINE__, (a) <= (b), #a" <= "#b" : %g %g", (float)a, (float)b)
#define ASSERT_GE(a, b) EpTestRunner::Singleton().Assert(__FILE__, __LINE__, (a) >= (b), #a" >= "#b" : %g %g", (float)a, (float)b)

#endif // EP_BUILD_SOFTWARE

