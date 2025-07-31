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
[Clang's Thread Safety Analysis](https://clang.llvm.org/docs/ThreadSafetyAnalysis.html)
annotations attack the same problem from a different direction.
[Rust](https://doc.rust-lang.org/std/sync/struct.Mutex.html) uses this type of
mutex exclusively.

## Motivation

Mutexes are a powerful tool for avoiding race conditions and writing thread safe
code, but the mutex library in the implimentation in the C++ standard library
is error prone, primarily because the mutex is not associated with the data it
is meant to protect. It is far too easy to make a mistake. Some common mistakes:

- forget to lock the necessary mutex at all
- lock the wrong mutex
- aquire a shared lock instead of an exclusive lock

Nothing in the type system will tells you what is protected so it won't help you.
The protections are all done by comments, convention and code review, not by the
compiler. Clang's tsan and thread safety analysis can help, but is mainly after
the fact and won't catch the problems at compile time.

The general solution is to associate the mutex with the data it protects so
that you can't access the data without aquiring the lock.

## Use

The `mutex_protected` class template is header-only. To use it, include the
header `mutex_protected.h` in your project.

```cpp
#include <thread>
#include <vector>

#include "mutex_protected.h"

int main() {
  const int num_threads = 10;
  const int num_loops = 10000;
  mutex_protected<int> value(0);

  std::vector<std::thread> threads;
  threads.reserve(num_threads);
  for (int i = 0; i < num_threads; ++i) {
    threads.emplace_back([&value]() {
      for (int j = 0; j < num_loops; ++j) {
        *value.lock() += 1;
      }
    });
  }
  for (auto& thread : threads) {
    thread.join();
  }
  return *value.lock() != num_threads * num_loops;
}
```

### Footgun

While `mutex_protected` will stop you from modifying the value without aquiring
the lock, it won't stop you from taking a reference to the value within the
lock and then modifying it outside the lock. These would be caught by the borrow
checker in rust, but can't be caught in C++.

```cpp
mutex_protected<std::vector<int>> vec;
assert(*a.lock().size() == 0);
int& b = *a.lock();  // BAD: Taking a reference to the protected value.
b.push_back(1);  // VERY BAD: Modifying the protected value.
assert(*a.lock().size() == 1);
```

This will work, but is almost certainly something you do not want to do, and
will lead to thread safety problems. Thankfully this should be fairly easy to
avoid doing by accident or finding in code review.

A version that will likely happen by accident:

```cpp
// BAD: The lock is released early as the guard is destroyed as soon ass the
// range iterators are created. The range iterators point into the vector's
// data, but lock is released before executing the loop body.
for (int& n : *vec.lock()) {
  n *= 2;
}

// GOOD: The lock is held for the duration of the loop. The extra scope is
// needed to release the lock as soon as the loop ends.
{
  auto locked = vec.lock();
  for (int& n : locked) {
    n *= 2;
  }
}
```

#### `with`

An alternative to calling `lock` is to call `with` with a lambda, which will
execute your function while holding the lock. The function receives a reference
to the data as its only argument.

This has several advantages:
- the lambda implicitly introduces an additional scope making the critical
  section obvious.
- the additional scope encourages short critical sections, releasing the
  lock quickly.
- accidentally storing a reference to the protected data is harder, both due
  to the function boundary and by aquiring the lock outside the scope.
- it can be slightly faster by using `std::lock_guard` instead of `std::unique_lock`.

```cpp
// BEST: By using a lambda you introduce the extra scope that forces the lock
// to be held for the duration of the loop in a really obvious way, and avoid
// needing access to the lock guard at all.
vec.with([](auto& v) {
  for (int& n : v) {
    n *= 2;
  }
});
```

The one case where it isn't usable is when using condition variables, since you
need access to the underlying guard or mutex.

### Non-blocking

If you want to aquire the lock if it's not contended, you can use the `try`
variants.

```cpp
auto locked = vec.try_lock();
if (locked.owns_lock()) {  // or: bool(locked)
  // Aquired the lock, read/modify the vector
} else {
  // The lock was held by some other thread.
}
```

or

```cpp
bool success = vec.try_with([](auto& v) {
  v.push_back(1);
});
```

#### Timed mutexes

Sometimes you want to aquire the lock, but not wait very long if there is
contention. You'll need to construct your mutex_protected with a mutex that
supports timeouts such as `std::timed_mutex`. You can then use the
`try_lock_for` and `try_lock_until` methods, as well as their `with` variants.

```cpp
#include <chrono>
using namespace std::chrono_literals;
auto now = std::chrono::system_clock::now;

mutex_protected<int, std::timed_mutex> value;

{
  auto locked = value.try_lock_for(1ms);
  if (locked) { *locked++; }
}
{
  auto locked = value.try_lock_until(now() + 1ms);
  if (locked) { *locked++; }
}
bool success = value.try_with_for(1ms, [](auto& v) { v++; });
bool success = value.try_with_until(now() + 1ms, [](auto& v) { v++; });
```

### Shared mutexes

`mutex_protected` can be declared with a `std::shared_mutex` which adds
`shared` methods. Multiple threads can hold a shared lock simultaneously,
giving a const reference to the data, or one thread can hold an exclusive lock
giving a mutable reference to the data, but not both simultaneously.

```cpp
mutex_protected<int, std::shared_mutex> value;

// Writer
{
  auto locked = value.lock();
  sum += *locked;  // OK
  *locked++;  // OK, writing is fine with an exclusive lock
}

// Reader
{
  auto locked = value.lock_shared();
  sum += *locked;  // OK, reading is fine with a shared lock.
  *locked++;  // Compile error, value is const with a shared lock.
}
```

### Condition variables

### Locking multiple mutexes simultaneously

If you have multiple mutexes that you want to lock, you need to be careful to
avoid deadlocks that can happen if different threads lock them in a different
order. The solution is to lock them in the same order, which is best delegated
to `std::lock`, `std::try_lock` or `std::scoped_lock`, but those only work on
true mutexes. We want to return multiple guards, so supply `lock_protected` and
`try_lock_protected`.

```cpp
lock_protected
try_lock_protected
```

###

## License

This code is licensed under the MIT License. See [LICENSE](LICENSE) for details.

## Developer Guide

For building and working with the project, please see the [developer guide](DEVELOPMENT.md).

## GitHub codespaces

Press `.` or visit [https://github.dev/jbcoe/mutex_protected] to open the project in
an instant, cloud-based, development environment. We have defined a
[devcontainer](.devcontainer/devcontainer.json) that will automatically install
the dependencies required to build and test the project.
