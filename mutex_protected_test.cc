#include "mutex_protected.h"

#include <chrono>

#include "gtest/gtest.h"

using namespace std::chrono_literals;

namespace xyz {

TEST(TimedMutexProtectedTest, TimeoutWorksCorrectly) {
  mutex_protected<int, std::timed_mutex> value(1);

  auto locked = value.try_lock_for(1ms);
}

}  // namespace xyz
