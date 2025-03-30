
#ifndef XYZ_MUTEX_PROTECTED_H
#define XYZ_MUTEX_PROTECTED_H

#include <thread>
#include <utility>

namespace xyz {
template <class T>
class mutex_locked {
 public:
  T *operator->() const { return v; }

  T &operator*() const { return *v; }

 private:
  mutex_locked(std::mutex &mutex, T *v_)
      : m_lock{std::lock_guard(mutex)}, v{v_} {}

  std::lock_guard<std::mutex> m_lock;
  T *v;

  template <class T_>
  friend class mutex_protected;
};

template <class T>
class mutex_protected {
 public:
  template <typename... Args>
  mutex_protected(Args &&...args) : mutex{}, v(std::forward<Args>(args)...) {}

  mutex_protected(const T &v_) : mutex{}, v(v_) {}

  mutex_locked<T> lock() { return mutex_locked(mutex, &v); }

  template <typename F>
  void with(F &&f) {
    std::lock_guard m_lock(mutex);
    f(v);
  }

 private:
  std::mutex mutex;
  T v;
};
}  // namespace xyz
#endif  // XYZ_MUTEX_PROTECTED_H
