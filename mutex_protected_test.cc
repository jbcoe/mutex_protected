#include "mutex_protected.h"

#include <chrono>
#include <string>
#include <thread>
#include <vector>

#include "gtest/gtest.h"

using namespace std::chrono_literals;
auto now = std::chrono::system_clock::now;

namespace xyz {

TEST(TimedMutexProtectedTest, TimeoutWorksCorrectly) {
  mutex_protected<int, std::timed_mutex> value(1);

  {
    auto locked = value.try_lock_for(1ms);
    ASSERT_TRUE(locked.owns_lock());
  }
}

}  // namespace xyz
