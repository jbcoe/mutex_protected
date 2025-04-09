#ifndef XYZ_MUTEX_PROTECTED_H
#define XYZ_MUTEX_PROTECTED_H

#include <chrono>
#include <concepts>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <type_traits>
#include <utility>

namespace xyz {

// TODO: Does std::Mutex exist?
// https://en.cppreference.com/w/cpp/named_req/Mutex suggests so, but I haven't
// figured out how to reference it.
template <typename M>
concept Mutex = requires(M m) {
  { m.lock() } -> std::convertible_to<void>;
  { m.unlock() } -> std::convertible_to<void>;
  std::is_constructible_v<std::unique_lock<M>, M>;
};

// TODO: Does std::SharedMutex exist?
template <typename M>
concept SharedMutex = Mutex<M> && requires(M m) {
  { m.lock_shared() } -> std::convertible_to<void>;
};

// TODO: Does std::TimedMutex exist?
template <typename M>
concept TimedMutex = Mutex<M> && requires(M m) {
  { m.try_lock_for(std::chrono::milliseconds(1)) } -> std::convertible_to<bool>;
  {
    m.try_lock_until(std::chrono::system_clock::now() +
                     std::chrono::milliseconds(1))
  } -> std::convertible_to<bool>;
};

template <class T, class G>
class [[nodiscard]] mutex_locked {
 public:
  T *operator->() const { return v; }

  T &operator*() const { return *v; }

  // owns_lock and operator bool are only available when mutex_protected is
  // called with try_*.
  bool owns_lock() const noexcept
    requires std::is_member_function_pointer_v<decltype(&G::owns_lock)>
  {
    return guard.owns_lock();
  }

  operator bool() const noexcept
    requires std::is_member_function_pointer_v<decltype(&G::owns_lock)>
  {
    return guard.owns_lock();
  }

 private:
  template <typename... Args>
  mutex_locked(T *v_, Args &&...args)
      : v{v_}, guard{std::forward<Args>(args)...} {}

  T *v;
  G guard;

  template <class T_, Mutex M_>
  friend class mutex_protected;
};

template <class T, Mutex M = std::mutex>
class mutex_protected {
 public:
  template <typename... Args>
  mutex_protected(Args &&...args) : mutex{}, v(std::forward<Args>(args)...) {}

  mutex_protected(const T &v_) : mutex{}, v(v_) {}

  mutex_locked<T, std::lock_guard<M>> lock() {
    return mutex_locked<T, std::lock_guard<M>>(&v, mutex);
  }

  mutex_locked<T, std::unique_lock<M>> try_lock() {
    return mutex_locked<T, std::unique_lock<M>>(&v, mutex, std::try_to_lock);
  }

  template <class Clock, class Duration>
  mutex_locked<T, std::unique_lock<M>> try_lock_until(
      const std::chrono::time_point<Clock, Duration> &timeout_time)
    requires TimedMutex<M>
  {
    return mutex_locked<T, std::unique_lock<M>>(&v, mutex, timeout_time);
  }

  template <class Rep, class Period>
  mutex_locked<T, std::unique_lock<M>> try_lock_for(
      const std::chrono::duration<Rep, Period> &timeout_duration)
    requires TimedMutex<M>
  {
    return mutex_locked<T, std::unique_lock<M>>(&v, mutex, timeout_duration);
  }

  template <typename F>
  void with(F &&f) {
    std::lock_guard guard(mutex);
    f(v);
  }

  template <typename F>
  [[nodiscard]] bool try_with(F &&f) {
    std::unique_lock guard(mutex, std::try_to_lock);
    if (guard.owns_lock()) {
      f(v);
      return true;
    } else {
      return false;
    }
  }

  template <class Clock, class Duration, typename F>
  [[nodiscard]] bool try_with_until(
      const std::chrono::time_point<Clock, Duration> &timeout_time, F &&f)
    requires TimedMutex<M>
  {
    std::unique_lock guard(mutex, timeout_time);
    if (guard.owns_lock()) {
      f(v);
      return true;
    } else {
      return false;
    }
  }

  template <class Rep, class Period, typename F>
  [[nodiscard]] bool try_with_for(
      const std::chrono::duration<Rep, Period> &timeout_duration, F &&f)
    requires TimedMutex<M>
  {
    std::unique_lock guard(mutex, timeout_duration);
    if (guard.owns_lock()) {
      f(v);
      return true;
    } else {
      return false;
    }
  }

  mutex_locked<const T, std::shared_lock<M>> lock_shared()
    requires SharedMutex<M>
  {
    return mutex_locked<const T, std::shared_lock<M>>(&v, mutex);
  }

  mutex_locked<const T, std::shared_lock<M>> try_lock_shared()
    requires SharedMutex<M>
  {
    return mutex_locked<const T, std::shared_lock<M>>(&v, mutex,
                                                      std::try_to_lock);
  }

  template <class Clock, class Duration>
  mutex_locked<const T, std::shared_lock<M>> try_lock_shared_until(
      const std::chrono::time_point<Clock, Duration> &timeout_time)
    requires SharedMutex<M> && TimedMutex<M>
  {
    return mutex_locked<const T, std::shared_lock<M>>(&v, mutex, timeout_time);
  }

  template <class Rep, class Period>
  mutex_locked<const T, std::shared_lock<M>> try_lock_shared_for(
      const std::chrono::duration<Rep, Period> &timeout_duration)
    requires SharedMutex<M> && TimedMutex<M>
  {
    return mutex_locked<const T, std::shared_lock<M>>(&v, mutex,
                                                      timeout_duration);
  }

  template <typename F>
  void with_shared(F &&f)
    requires SharedMutex<M>
  {
    std::shared_lock guard(mutex);
    f(static_cast<const T &>(v));
  }

  template <typename F>
  [[nodiscard]] bool try_with_shared(F &&f)
    requires SharedMutex<M>
  {
    std::shared_lock guard(mutex, std::try_to_lock);
    if (guard.owns_lock()) {
      f(static_cast<const T &>(v));
      return true;
    } else {
      return false;
    }
  }

  template <class Clock, class Duration, typename F>
  [[nodiscard]] bool try_with_shared_until(
      const std::chrono::time_point<Clock, Duration> &timeout_time, F &&f)
    requires SharedMutex<M> && TimedMutex<M>
  {
    std::shared_lock m_lock(mutex, timeout_time);
    if (m_lock.owns_lock()) {
      f(static_cast<const T &>(v));
      return true;
    } else {
      return false;
    }
  }

  template <class Rep, class Period, typename F>
  [[nodiscard]] bool try_with_shared_for(
      const std::chrono::duration<Rep, Period> &timeout_duration, F &&f)
    requires SharedMutex<M> && TimedMutex<M>
  {
    std::shared_lock m_lock(mutex, timeout_duration);
    if (m_lock.owns_lock()) {
      f(static_cast<const T &>(v));
      return true;
    } else {
      return false;
    }
  }

 private:
  M mutex;
  T v;

  // Used by `xyz::lock` when locking multiple mutex_protected objects.
  mutex_locked<T, std::lock_guard<M>> adopt_lock() {
    return mutex_locked<T, std::lock_guard<M>>(&v, mutex, std::adopt_lock);
  }

  template <typename... MutexProtected>
  friend auto lock(MutexProtected &...mp);
};

template <typename... MutexProtected>
auto lock(MutexProtected &...mp) {
  std::lock(mp.mutex...);
  return std::make_tuple(mp.adopt_lock()...);
}

}  // namespace xyz

#endif  // XYZ_MUTEX_PROTECTED_H
