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

TEST(MutexProtectedTest, ThreadSafetyCorrectness) {
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

}  // namespace xyz
