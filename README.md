# `mutex_protected`: A Mutex that Owns the Resource it Protects

[![codecov][badge.codecov]][codecov] [![language][badge.language]][language]
[![license][badge.license]][license] [![issues][badge.issues]][issues]
[![pre-commit][badge.pre-commit]][pre-commit]

[badge.language]: https://img.shields.io/badge/language-C%2B%2B14-yellow.svg
[badge.codecov]:
    https://img.shields.io/codecov/c/github/jbcoe/mutex_protected/master.svg?logo=codecov
[badge.license]: https://img.shields.io/badge/license-MIT-blue.svg
[badge.issues]: https://img.shields.io/github/issues/jbcoe/mutex_protected.svg
[badge.pre-commit]: https://img.shields.io/badge/pre--commit-enabled-brightgreen?logo=pre-commit

[codecov]: https://codecov.io/gh/jbcoe/mutex_protected
[language]: https://en.wikipedia.org/wiki/C%2B%2B14
[license]: https://en.wikipedia.org/wiki/MIT_License
[issues]: http://github.com/jbcoe/mutex_protected/issues
[pre-commit]: https://github.com/pre-commit/pre-commit

This repository contains the class `mutex_protected`, which is a `mutex` that owns
the value it protects, and uses the type system and RAII to enforce that only one
thread can access it at any given time.

It is clearly possible to do the same thing with `std::mutex` and `std::lock_guard`,
but those are easy to misuse, accessing a variable without locking the mutex.

The goal of this project is to push an implementation of `mutex_protected` to the
C++ Standard Library. Similar implementations exist in
[Boost](https://www.boost.org/doc/libs/1_81_0/doc/html/thread/sds.html) and
[Folly](https://github.com/facebook/folly/blob/main/folly/docs/Synchronized.md).

## Use

The `mutex_protected` class template is header-only. To use it, include the
header `mutex_protected.h` in your project.

```cpp
#include <thread>
#include <vector>

#include "mutex_protected.h"

int main() {
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
  return *value.lock() == 100000;
}
```

### Condition Variables

Condition variables are used to wait for a particular change to occur on
internal data, but need access to either the underlying lock guard or the
underlying mutex.

#### std::mutex

`std::condition_variable` only works with a `std::mutex`, so when using
The `mutex_protected<T, std::mutex>` the `mutex_locked` instance has a `guard`
method that returns the underlying `std::unique_lock`.

```cpp
mutex_protected<std::vector<int>> data;
std::condition_variable cv;

// Wait for some other thread to add data to the vector.
auto locked = data.lock();
cv.wait(locked.guard(), [&locked]() { return !locked->empty(); });
```

#### Other mutexes, eg: std::shared_mutex

For all other mutex types, you'll need to use a `std::condition_variable_any`
which works on the mutex instead of on the guard. For that, you can use the
`mutex` method on the `mutex_locked`. This works regardless of whether the
lock is exclusive or shared.

```cpp
mutex_protected<std::vector<int>, std::shared_mutex> data;
std::condition_variable_any cv;

// Wait for some other thread to add data to the vector.
auto locked = data.lock();
cv.wait(locked.mutex(), [&locked]() { return !locked->empty(); });
```

## License

This code is licensed under the MIT License. See [LICENSE](LICENSE) for details.

## Developer Guide

For building and working with the project, please see the [developer guide](DEVELOPMENT.md).

## GitHub codespaces

Press `.` or visit [https://github.dev/jbcoe/mutex_protected] to open the project in
an instant, cloud-based, development environment. We have defined a
[devcontainer](.devcontainer/devcontainer.json) that will automatically install
the dependencies required to build and test the project.
