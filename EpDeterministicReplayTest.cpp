#include "EpDeterministicReplay.h"
#include "EpTest.h"

// ----------------------------------------------------------------------------
#if (EP_DETERMINISTIC_REPLAY == 1)

static const int s_epTestFiles = 3;
static const char s_epTestData[] = "This is a test.";
static const char* s_epFilename = "DeterministicReplayTest_%d.bin";

class EpDeterministicReplayTest :
  public testing::Test
{
public:
};

// Either records or checks that recorded data matches:
static void SharedCodeSection() {
  EpDetermineLabel("label_3");

  const int32_t nums[3] = { 7, 13, 17 };
  EpDetermineData(nums, sizeof nums);

  EpDetermineLabel("label_77");

  EpDetermineNumber(77);
}


TEST_F(EpDeterministicReplayTest, Record) {
  EpDetermineInstance().Reset(); // Testing only

  for(int i=0; i < s_epTestFiles; ++i) {
    const bool isRunning = EpDetermineTick(s_epFilename, false, 0, s_epTestFiles);
    ASSERT_TRUE(isRunning); (void)isRunning;

    // Captures data when recording, use copy anyway to be safe.
    char buf[256];
    ::strcpy(buf, s_epTestData);
    EpDeterminePlayback(buf, sizeof s_epTestData);

    SharedCodeSection();
  }

  const bool isRunning = EpDetermineTick(s_epFilename, false, 0, s_epTestFiles);
  ASSERT_FALSE(isRunning); (void)isRunning;
}

TEST_F(EpDeterministicReplayTest, Replay) {
  EpDetermineInstance().Reset(); // Testing only

  for(int i=0; i < s_epTestFiles; ++i) {
    const bool isRunning = EpDetermineTick(s_epFilename, true, 0, s_epTestFiles);
    ASSERT_TRUE(isRunning); (void)isRunning;

    // Restores saved data when playing back:
    char buf[256];
    EpDeterminePlayback(buf, sizeof s_epTestData);
    ASSERT_TRUE(0 == ::strcmp(buf, s_epTestData));

    SharedCodeSection();
  }

  const bool isRunning = EpDetermineTick(s_epFilename, false, 0, s_epTestFiles);
  ASSERT_FALSE(isRunning); (void)isRunning;
}

#endif // (EP_DETERMINISTIC_REPLAY == 1)


