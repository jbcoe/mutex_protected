#include "mutex_protected.h"

#include <chrono>
#include <string>
#include <thread>
#include <vector>

#include "gtest/gtest.h"

using namespace std::chrono_literals;
auto now = std::chrono::system_clock::now;

namespace xyz {

template <typename T>
class MutexProtectedTest : public testing::Test {};

using AllMutexes =
    ::testing::Types<std::mutex, std::timed_mutex, std::recursive_mutex,
                     std::recursive_timed_mutex, std::shared_mutex,
                     std::shared_timed_mutex>;
TYPED_TEST_SUITE(MutexProtectedTest, AllMutexes);

TYPED_TEST(MutexProtectedTest, InitializedConstruction) {
  mutex_protected<int, TypeParam> value(0);
  EXPECT_EQ(*value.lock(), 0);
}

TYPED_TEST(MutexProtectedTest, DefaultConstruction) {
  mutex_protected<std::string, TypeParam> value;
  EXPECT_EQ(*value.lock(), "");
}

TYPED_TEST(MutexProtectedTest, StringConstruction) {
  mutex_protected<std::string, TypeParam> value("hello");
  *value.lock() += " world";
  EXPECT_EQ(*value.lock(), "hello world");
  EXPECT_EQ(value.lock()->substr(6), "world");
}

TYPED_TEST(MutexProtectedTest, DefaultVectorConstruction) {
  mutex_protected<std::vector<int>, TypeParam> value;
  {
    auto locked = value.lock();
    locked->push_back(1);
    locked->push_back(2);
    locked->push_back(3);
  }
  EXPECT_EQ(*value.lock(), (std::vector<int>{1, 2, 3}));
}

TYPED_TEST(MutexProtectedTest, InitializerListConstruction) {
  mutex_protected<std::vector<int>, TypeParam> value{{1, 2, 3}};
  EXPECT_EQ(*value.lock(), (std::vector<int>{1, 2, 3}));
}

TYPED_TEST(MutexProtectedTest, ProtectStruct) {
  struct MyStruct {
    int i;
    bool b;
    std::string s;
  };

  mutex_protected<MyStruct, TypeParam> value{{1, true, "hello"}};

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

TYPED_TEST(MutexProtectedTest, UseWithToModifyInLambda) {
  mutex_protected<int, TypeParam> value(0);
  value.with([](int& v) { v++; });
  EXPECT_EQ(*value.lock(), 1);
}

TYPED_TEST(MutexProtectedTest, TryLockGetsLockWithoutContention) {
  mutex_protected<int, TypeParam> value(0);

  {
    auto locked = value.try_lock();
    EXPECT_TRUE(locked.owns_lock());
    EXPECT_TRUE(locked);
    *locked += 1;
  }
  EXPECT_EQ(*value.lock(), 1);
}

TYPED_TEST(MutexProtectedTest, TryLockFailsIfLocked) {
  mutex_protected<int, TypeParam> value(0);

  auto locked = value.lock();
  std::thread t([&value]() {
    auto locked = value.try_lock();
    EXPECT_FALSE(locked.owns_lock());
    EXPECT_FALSE(locked);
  });
  t.join();
}

TYPED_TEST(MutexProtectedTest, UseTryWithToModifyInLambda) {
  mutex_protected<int, TypeParam> value(0);
  EXPECT_TRUE(value.try_with([](int& v) { v++; }));
  EXPECT_EQ(*value.lock(), 1);
}

TYPED_TEST(MutexProtectedTest, TryWithFailsIfLocked) {
  mutex_protected<int, TypeParam> value(0);
  {
    auto locked = value.lock();
    std::thread t(
        [&value]() { EXPECT_FALSE(value.try_with([](int& v) { v++; })); });
    t.join();
  }
  EXPECT_EQ(*value.lock(), 0);
}

TYPED_TEST(MutexProtectedTest, ThreadSafetyCorrectnessLock) {
  mutex_protected<int, TypeParam> value(0);

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

// LCOV_EXCL_START
// This test causes issues with negative line-count for lcov.
// It is not a problem with the code, but with lcov.

TYPED_TEST(MutexProtectedTest, ThreadSafetyCorrectnessWith) {
  mutex_protected<int, TypeParam> value(0);

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

// LCOV_EXCL_STOP

TYPED_TEST(MutexProtectedTest, LockMultiple) {
  mutex_protected<int, TypeParam> a(1);
  mutex_protected<int, TypeParam> b(2);
  {
    auto [la, lb] = xyz::lock_protected(a, b);
    EXPECT_EQ(*la, 1);
    EXPECT_EQ(*lb, 2);
    *la += 10;
    *lb += 10;
  }
  {
    auto [lb, la] = xyz::lock_protected(b, a);
    EXPECT_EQ(*la, 11);
    EXPECT_EQ(*lb, 12);
  }
}

template <typename T>
class SharedMutexProtectedTest : public testing::Test {};

using SharedMutexes =
    ::testing::Types<std::shared_mutex, std::shared_timed_mutex>;
TYPED_TEST_SUITE(SharedMutexProtectedTest, SharedMutexes);

TYPED_TEST(SharedMutexProtectedTest, SharedLockIsConst) {
  mutex_protected<int, TypeParam> value(0);

  {
    auto locked = value.lock_shared();
    static_assert(std::is_const_v<std::remove_reference_t<decltype(*locked)>>);
  }
  {
    auto locked = value.try_lock_shared();
    static_assert(std::is_const_v<std::remove_reference_t<decltype(*locked)>>);
  }
  {
    value.with_shared([](auto& v) {
      static_assert(std::is_const_v<std::remove_reference_t<decltype(v)>>);
    });
  }
  {
    ASSERT_TRUE(value.try_with_shared([](auto& v) {
      static_assert(std::is_const_v<std::remove_reference_t<decltype(v)>>);
    }));
  }
}

TYPED_TEST(SharedMutexProtectedTest, TwoSharedLockSucceeds) {
  mutex_protected<int, TypeParam> value(0);

  auto locked = value.lock_shared();
  std::thread t([&value]() {
    EXPECT_TRUE(value.try_with_shared([](const int& v) { EXPECT_EQ(v, 0); }));
    auto locked = value.try_lock_shared();
    EXPECT_TRUE(locked.owns_lock());
    EXPECT_TRUE(locked);
  });
  t.join();
}

TYPED_TEST(SharedMutexProtectedTest, LockThenSharedFails) {
  mutex_protected<int, TypeParam> value(0);

  auto locked = value.lock();
  std::thread t([&value]() {
    EXPECT_FALSE(value.try_with_shared([](const int& v) { EXPECT_EQ(v, 0); }));
    auto locked = value.try_lock_shared();
    EXPECT_FALSE(locked.owns_lock());
    EXPECT_FALSE(locked);
  });
  t.join();
}

TYPED_TEST(SharedMutexProtectedTest, SharedThenLockFails) {
  mutex_protected<int, TypeParam> value(0);

  auto locked = value.lock_shared();
  std::thread t([&value]() {
    EXPECT_FALSE(value.try_with([](int& v) { EXPECT_EQ(v, 0); }));
    auto locked = value.try_lock();
    EXPECT_FALSE(locked.owns_lock());
    EXPECT_FALSE(locked);
  });
  t.join();
}

TYPED_TEST(SharedMutexProtectedTest, ThreadSafetyCorrectness) {
  // TODO: Is there some test where a shared lock is important beyond
  // performance and where a normal mutex is not sufficient? Testing
  // performance is a good idea, but is inconsistent/flaky, so is best
  // left for benchmarks instead of a unit test.

  mutex_protected<int, TypeParam> value(0);

  const int readers = 10;
  const int writers = 10;
  const int iters = 10000;

  std::vector<std::thread> threads;
  threads.reserve(readers + writers);
  for (int i = 0; i < writers; ++i) {
    threads.emplace_back([&value]() {
      for (int j = 0; j < iters; ++j) {
        *value.lock() += 1;
      }
    });
  }
  mutex_protected<long long> grand_total = 0;
  for (int i = 0; i < readers; ++i) {
    threads.emplace_back([&value, &grand_total]() {
      int sum = 0;
      for (int j = 0; j < iters; ++j) {
        sum += *value.lock_shared();
      }
      *grand_total.lock() += sum;
    });
  }
  for (auto& thread : threads) {
    thread.join();
  }
  EXPECT_EQ(*value.lock(), writers * iters);

  // Hopefully this stops things from being optimized away, but I don't see how
  // this could fail.
  EXPECT_LE(*grand_total.lock(), (long long)readers * writers * iters * iters);
}

template <typename T>
class TimedMutexProtectedTest : public testing::Test {};

using TimedMutexes =
    ::testing::Types<std::timed_mutex, std::recursive_timed_mutex,
                     std::shared_timed_mutex>;
TYPED_TEST_SUITE(TimedMutexProtectedTest, TimedMutexes);

TYPED_TEST(TimedMutexProtectedTest, TimeoutUntilWorksCorrectly) {
  mutex_protected<int, TypeParam> value(1);

  int out = 0;
  {
    auto locked = value.try_lock_until(now() + 1ms);
    ASSERT_TRUE(locked.owns_lock());
    out += *locked;
  }
  {
    ASSERT_TRUE(
        value.try_with_until(now() + 1ms, [&out](auto& v) { out += v; }));
  }

  auto write_locked = value.lock();
  std::thread t([&value, &out]() {
    {
      auto locked = value.try_lock_until(now() + 1ms);
      ASSERT_FALSE(locked.owns_lock());
    }
    {
      ASSERT_FALSE(
          value.try_with_until(now() + 1ms, [&out](auto& v) { out += v; }));
    }
  });
  t.join();
  EXPECT_EQ(out, 2);
  EXPECT_EQ(*write_locked, 1);
}

#if !defined(THREAD_SANITIZER)
// Disable TSAN for try_lock_for, which has a known false positive.
// https://github.com/llvm/llvm-project/issues/62623
// Lots of debugging in
// https://github.com/jbcoe/mutex_protected/issues/29
// https://github.com/jbcoe/mutex_protected/pull/43
TYPED_TEST(TimedMutexProtectedTest, TimeoutForWorksCorrectly) {
  mutex_protected<int, TypeParam> value(1);

  int out = 0;

  {
    auto locked = value.try_lock_for(1ms);
    ASSERT_TRUE(locked.owns_lock());
    out += *locked;
  }
  {
    ASSERT_TRUE(value.try_with_for(1ms, [&out](auto& v) { out += v; }));
  }

  auto write_locked = value.lock();
  std::thread t([&value, &out]() {
    {
      auto locked = value.try_lock_for(1ms);
      ASSERT_FALSE(locked.owns_lock());
    }
    {
      ASSERT_FALSE(value.try_with_for(1ms, [&out](auto& v) { out += v; }));
    }
  });
  t.join();
  EXPECT_EQ(out, 1);
  EXPECT_EQ(*write_locked, 1);
}
#endif

TEST(SharedTimedMutexProtectedTest, SharedLockIsConst) {
  mutex_protected<int, std::shared_timed_mutex> value(0);

  {
    auto locked = value.try_lock_shared_until(now() + 1ms);
    static_assert(std::is_const_v<std::remove_reference_t<decltype(*locked)>>);
  }
  {
    auto locked = value.try_lock_shared_for(1ms);
    static_assert(std::is_const_v<std::remove_reference_t<decltype(*locked)>>);
  }
  {
    ASSERT_TRUE(value.try_with_shared_until(now() + 1ms, [](auto& v) {
      static_assert(std::is_const_v<std::remove_reference_t<decltype(v)>>);
    }));
  }
  {
    ASSERT_TRUE(value.try_with_shared_for(1ms, [](auto& v) {
      static_assert(std::is_const_v<std::remove_reference_t<decltype(v)>>);
    }));
  }
}

TEST(SharedTimedMutexProtectedTest, TimeoutWorksCorrectly) {
  mutex_protected<int, std::shared_timed_mutex> value(1);

  int out = 0;
  {
    auto locked = value.try_lock_shared_until(now() + 1ms);
    ASSERT_TRUE(locked.owns_lock());
    out += *locked;
  }
  {
    auto locked = value.try_lock_shared_for(1ms);
    ASSERT_TRUE(locked.owns_lock());
    out += *locked;
  }
  {
    ASSERT_TRUE(value.try_with_shared_until(now() + 1ms,
                                            [&out](auto& v) { out += v; }));
  }
  {
    ASSERT_TRUE(value.try_with_shared_for(1ms, [&out](auto& v) { out += v; }));
  }
  auto write_locked = value.lock();
  std::thread t([&value, &out]() {
    {
      auto locked = value.try_lock_shared_until(now() + 1ms);
      ASSERT_FALSE(locked.owns_lock());
    }
    {
      auto locked = value.try_lock_shared_for(1ms);
      ASSERT_FALSE(locked.owns_lock());
    }
    {
      ASSERT_FALSE(value.try_with_shared_until(now() + 1ms,
                                               [&out](auto& v) { out += v; }));
    }
    {
      ASSERT_FALSE(
          value.try_with_shared_for(1ms, [&out](auto& v) { out += v; }));
    }
  });
  t.join();
  EXPECT_EQ(out, 4);
  EXPECT_EQ(*write_locked, 1);
}

}  // namespace xyz
