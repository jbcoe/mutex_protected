
#ifndef XYZ_MUTEX_PROTECTED_H
#define XYZ_MUTEX_PROTECTED_H

#include <mutex>
#include <thread>
#include <utility>

namespace xyz {
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

  template <class T_>
  friend class mutex_protected;
};

template <class T>
class mutex_protected {
 public:
  template <typename... Args>
  mutex_protected(Args &&...args) : mutex{}, v(std::forward<Args>(args)...) {}

  mutex_protected(const T &v_) : mutex{}, v(v_) {}

  mutex_locked<T, std::lock_guard<std::mutex>> lock() {
    return mutex_locked<T, std::lock_guard<std::mutex>>(&v, mutex);
  }

  mutex_locked<T, std::unique_lock<std::mutex>> try_lock() {
    return mutex_locked<T, std::unique_lock<std::mutex>>(&v, mutex,
                                                         std::try_to_lock);
  }

  template <typename F>
  void with(F &&f) {
    std::lock_guard m_lock(mutex);
    f(v);
  }

  template <typename F>
  [[nodiscard]] bool try_with(F &&f) {
    std::unique_lock m_lock(mutex, std::try_to_lock);
    if (m_lock.owns_lock()) {
      f(v);
      return true;
    } else {
      return false;
    }
  }

 private:
  std::mutex mutex;
  T v;
};
}  // namespace xyz
#endif  // XYZ_MUTEX_PROTECTED_H
