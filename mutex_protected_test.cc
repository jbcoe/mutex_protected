#include "mutex_protected.h"

#include <string>
#include <thread>
#include <vector>

#include "gtest/gtest.h"

namespace xyz {

TEST(MutexProtectedTest, InitializedConstruction) {
  mutex_protected<int> value(0);
  EXPECT_EQ(*value.lock(), 0);
}

TEST(MutexProtectedTest, DefaultConstruction) {
  mutex_protected<std::string> value;
  EXPECT_EQ(*value.lock(), "");
}

TEST(MutexProtectedTest, StringConstruction) {
  mutex_protected<std::string> value("hello");
  *value.lock() += " world";
  EXPECT_EQ(*value.lock(), "hello world");
  EXPECT_EQ(value.lock()->substr(6), "world");
}

TEST(MutexProtectedTest, DefaultVectorConstruction) {
  mutex_protected<std::vector<int>> value;
  {
    auto locked = value.lock();
    locked->push_back(1);
    locked->push_back(2);
    locked->push_back(3);
  }
  EXPECT_EQ(*value.lock(), (std::vector<int>{1, 2, 3}));
}

TEST(MutexProtectedTest, InitializerListConstruction) {
  mutex_protected<std::vector<int>> value{{1, 2, 3}};
  EXPECT_EQ(*value.lock(), (std::vector<int>{1, 2, 3}));
}

TEST(MutexProtectedTest, ProtectStruct) {
  struct MyStruct {
    int i;
    bool b;
    std::string s;
  };

  mutex_protected<MyStruct> value{{1, true, "hello"}};

  EXPECT_EQ(value.lock()->i, 1);
  EXPECT_EQ(value.lock()->b, true);
  EXPECT_EQ(value.lock()->s, "hello");

  value.lock()->i = 42;
  {
    auto locked = value.lock();
    locked->b = false;
    locked->s += " world";
  }

  EXPECT_EQ(value.lock()->i, 42);
  EXPECT_EQ(value.lock()->b, false);
  EXPECT_EQ(value.lock()->s, "hello world");
}

TEST(MutexProtectedTest, UseWithToModifyInLambda) {
  mutex_protected<int> value(0);
  value.with([](int& v) { v++; });
  EXPECT_EQ(*value.lock(), 1);
}

TEST(MutexProtectedTest, TryLockGetsLockWithoutContention) {
  mutex_protected<int> value(0);

  {
    auto locked = value.try_lock();
    EXPECT_TRUE(locked.owns_lock());
    EXPECT_TRUE(locked);
    *locked += 1;
  }
  EXPECT_EQ(*value.lock(), 1);
}

TEST(MutexProtectedTest, TryLockFailsIfLocked) {
  mutex_protected<int> value(0);

  auto locked = value.lock();
  std::thread t([&value]() {
    auto locked = value.try_lock();
    EXPECT_FALSE(locked.owns_lock());
    EXPECT_FALSE(locked);
  });
  t.join();
}

TEST(MutexProtectedTest, UseTryWithToModifyInLambda) {
  mutex_protected<int> value(0);
  EXPECT_TRUE(value.try_with([](int& v) { v++; }));
  EXPECT_EQ(*value.lock(), 1);
}

TEST(MutexProtectedTest, TryWithFailsIfLocked) {
  mutex_protected<int> value(0);
  {
    auto locked = value.lock();
    std::thread t(
        [&value]() { EXPECT_FALSE(value.try_with([](int& v) { v++; })); });
    t.join();
  }
  EXPECT_EQ(*value.lock(), 0);
}

TEST(MutexProtectedTest, ThreadSafetyCorrectnessLock) {
  mutex_protected<int> value(0);

  std::vector<std::thread> threads;
  threads.reserve(10);
  for (int i = 0; i < 10; ++i) {
    threads.emplace_back([&value]() {
      for (int j = 0; j < 10000; ++j) {
        *value.lock() += 1;
      }
    });
  }
  for (auto& thread : threads) {
    thread.join();
  }
  EXPECT_EQ(*value.lock(), 100000);
}

TEST(MutexProtectedTest, ThreadSafetyCorrectnessWith) {
  mutex_protected<int> value(0);

  std::vector<std::thread> threads;
  threads.reserve(10);
  for (int i = 0; i < 10; ++i) {
    threads.emplace_back([&value]() {
      for (int j = 0; j < 10000; ++j) {
        value.with([](int& v) { v++; });
      }
    });
  }
  for (auto& thread : threads) {
    thread.join();
  }
  EXPECT_EQ(*value.lock(), 100000);
}

}  // namespace xyz
