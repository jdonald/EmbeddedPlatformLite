#include "EmbeddedPlatform.h"
#include "EpArray.h"
#include "EpUniquePtr.h"
#include "EpTest.h"
#include <limits.h>

class EpArrayTest;

static EpArrayTest* s_currentTest = 0;

class EpArrayTest :
  public testing::Test
{
public:
  struct TestObject {
    TestObject() {
      ++s_currentTest->m_constructed;
      id = s_currentTest->m_nextId--;
    }
    TestObject(const TestObject& rhs) {
      ++s_currentTest->m_constructed;
      id = rhs.id;
    }
    explicit TestObject(int x) {
      EpAssert(x >= 0); // User supplied IDs are positive
      ++s_currentTest->m_constructed;
      id = x;
    }
    ~TestObject() {
      ++s_currentTest->m_destructed;
      id = INT_MAX;
    }

    void operator=(const TestObject& rhs) { id = rhs.id; }
    void operator=(int x) { id = x; }
    bool operator==(const TestObject& rhs) const { return id == rhs.id; }
    bool operator==(int x) const { return id == x; }

    operator float() const { return (float)id; }

    int id;
  };

  EpArrayTest() {
    m_constructed = 0;
    m_destructed = 0;
    m_nextId = -1;
    s_currentTest = this;
  }
  ~EpArrayTest() {
    s_currentTest = 0;
  }

  bool CheckTotals(int total) const {
    return m_constructed == total && m_destructed == total;
  }

  int m_constructed;
  int m_destructed;
  int m_nextId;
};

TEST_F(EpArrayTest, Null) {
  {
    TestObject to0;
    TestObject to1;
    ASSERT_EQ(to0.id, -1);
    ASSERT_EQ(to1.id, -2);
  }
  ASSERT_TRUE(CheckTotals(2));
}


TEST_F(EpArrayTest, Allocators) {
  EpArray<TestObject> objsDynamic;
  objsDynamic.reserve(10u);
  EpArray<TestObject, 10u> objsStatic;

  ASSERT_EQ(objsDynamic.size(), 0u);
  ASSERT_EQ(objsStatic.size(), 0u);

  objsDynamic.push_back(TestObject(20));
  objsDynamic.push_back(TestObject(21));
  objsStatic.push_back(TestObject(20));
  objsStatic.push_back(TestObject(21));

  ASSERT_EQ(objsDynamic.size(), 2u);
  ASSERT_EQ(objsDynamic[0], 20);
  ASSERT_EQ(objsDynamic[1], 21);
  ASSERT_EQ(objsStatic.size(), 2u);
  ASSERT_EQ(objsStatic[0], 20);
  ASSERT_EQ(objsStatic[1], 21);

  objsDynamic.clear();
  objsStatic.clear();

  ASSERT_TRUE(CheckTotals(8));
}

TEST_F(EpArrayTest, Iteration) {
  {
    static const int nums[3] = { 21, 22, 23 };

    EpArray<TestObject, 10u> objs;
    objs.push_back(TestObject(nums[0]));
    objs.push_back(TestObject(nums[1]));
    objs.push_back(TestObject(nums[2]));

    const EpArray<TestObject, 10u>& cobjs = objs;

    int counter = 0;
    for (EpArray<TestObject, 10u>::iterator it = objs.begin(); it != objs.end(); ++it) {
      ASSERT_EQ(it->id, objs[counter].id);
      ASSERT_EQ(it->id, nums[counter]);
      ++counter;
    }

    counter = 0;
    for (EpArray<TestObject, 10u>::const_iterator it = cobjs.begin(); it != cobjs.end(); ++it) {
      ASSERT_EQ(it->id, objs[counter].id);
      ASSERT_EQ(it->id, nums[counter]);
      ++counter;
    }

    ASSERT_EQ(objs.front(), nums[0]);
    ASSERT_EQ(objs.back(), nums[2]);
    ASSERT_EQ(cobjs.front(), nums[0]);
    ASSERT_EQ(cobjs.back(), nums[2]);
  }

  ASSERT_TRUE(CheckTotals(6));
}

TEST_F(EpArrayTest, Modification) {
  {
    static const int nums[5] = { 91, 92, 93, 94, 95 };

    EpArray<TestObject> objs;
    objs.assign(nums, nums + (sizeof nums / sizeof *nums));

    ASSERT_EQ(objs.capacity(), 5u);
    ASSERT_EQ(objs.size(), 5u);

    // 91, 92, 93, 94

    objs.pop_back();
    objs.pop_back();
    objs.pop_back();

    TestObject to;
    objs.push_back(to);
    objs.push_back((const TestObject&)to);

    ::new (objs.emplace_back_raw()) TestObject;

    // 91, 92, -1, -2, -3

    objs.erase_unordered(1);

    // 91, -2, -1

    ASSERT_EQ(objs[0].id, 91);
    ASSERT_EQ(objs[1].id, -2);
    ASSERT_EQ(objs[2].id, -1);
  }

  ASSERT_TRUE(CheckTotals(9));
}

TEST_F(EpArrayTest, Resizing) {
  {
    static const int nums[5] = { 51, 52, 53, 54, 55 };

    EpArray<TestObject> objs;
    objs.reserve(10);
    objs.assign(nums, nums + (sizeof nums / sizeof *nums));

    objs.resize(3);

    ASSERT_EQ(objs.size(), 3u);
    ASSERT_EQ(objs[0].id, 51);
    ASSERT_EQ(objs[2].id, 53);

    objs.resize(4u);

    ASSERT_EQ(objs.size(), 4u);
    ASSERT_EQ(objs[0].id, 51);
    ASSERT_EQ(objs[2].id, 53);
    ASSERT_EQ(objs[3].id, -1);
    ASSERT_EQ(objs.capacity(), 10u);

    objs.resize(10u);
    ASSERT_EQ(objs.size(), 10u);
    ASSERT_EQ(objs[9].id, -7);

    ASSERT_FALSE(objs.empty());
    objs.clear();
    ASSERT_EQ(objs.size(), 0u);
    ASSERT_TRUE(objs.empty());

    ASSERT_EQ(objs.capacity(), 10u);
  }

  ASSERT_TRUE(CheckTotals(12));
}

TEST_F(EpArrayTest, Assignment) {
  {
    EpArray<TestObject> objs;
    objs.reserve(1);

    TestObject to;
    to.id = 67;
    objs.push_back(to);

    EpArray<TestObject> objs2;
    objs2 = objs; // Assign to same type

    EpArray<TestObject, 1> objs3;
    objs3 = objs; // Assign to different type

    EpArray<TestObject> objs4(objs); // Construct from same type

    EpArray<TestObject, 1> objs5(objs); // Construct from different type

    ASSERT_EQ(objs2.size(), 1u);
    ASSERT_EQ(objs3.size(), 1u);
    ASSERT_EQ(objs4.size(), 1u);
    ASSERT_EQ(objs5.size(), 1u);

    ASSERT_EQ(objs2[0].id, 67);
    ASSERT_EQ(objs3[0].id, 67);
    ASSERT_EQ(objs4[0].id, 67);
    ASSERT_EQ(objs5[0].id, 67);
  }

  ASSERT_TRUE(CheckTotals(6));
}

TEST_F(EpArrayTest, UniquePtr) {
  {
    EpArray<EpUniquePtr<TestObject> > objs;
    objs.reserve(10u);
    objs.resize(5u);

    objs[2].reset(EpNew<TestObject>(EpMemoryAllocatorId_Heap));
    objs[4].reset(EpNew<TestObject>(EpMemoryAllocatorId_Heap));

    objs.resize(4u);
    objs.erase_unordered(2);

    ASSERT_TRUE(CheckTotals(2));

    objs[0].reset(EpNew<TestObject>(EpMemoryAllocatorId_Heap));
    objs[2].reset(EpNew<TestObject>(EpMemoryAllocatorId_Heap));
  }

  ASSERT_TRUE(CheckTotals(4));
}
