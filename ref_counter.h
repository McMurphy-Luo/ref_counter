#ifndef REF_COUNTER_H_
#define REF_COUNTER_H_

#include <atomic>
#include <ostream>
#include <type_traits>
#include <functional>

namespace stdext
{

class ref_counter
{
public:
  ref_counter() noexcept
    : count_(0)
  {

  }

  ref_counter(const ref_counter&) noexcept
    : count_(0)
  {
  }

  ref_counter& operator=(const ref_counter&) noexcept {
    return *this;
  }

  unsigned int increment() noexcept {
    return count_.fetch_add(1, std::memory_order_acq_rel) + 1;
  }

  unsigned int decrement() {
    unsigned int count = count_.fetch_add(-1, std::memory_order_acq_rel);
    if (count == 1) {
      on_final_destroy();
    }
    return count - 1;
  }

  int_least32_t use_count() const noexcept {
    return count_.load(std::memory_order_acquire);
  }

protected:
  virtual ~ref_counter() = default;

  virtual void on_final_destroy() {
    delete this;
  }

private:
  std::atomic_int_least32_t count_;
};

template<class T>
class ref_count_ptr
{
  template<typename U> friend class ref_weak_ptr;
  typedef ref_count_ptr this_type;

public:
  constexpr ref_count_ptr() noexcept : px(0)
  {
  }

  ref_count_ptr(T* p, bool add_ref = true) : px(p)
  {
    if (px != 0 && add_ref) {
      px->increment();
    }
  }

  template<class U, typename std::enable_if<std::is_convertible<U*, T*>::value, int>::type = 0>
  ref_count_ptr(ref_count_ptr<U> const& rhs)
    : px(rhs.get())
  {
    if (px != 0) {
      px->increment();
    }
  }

  ref_count_ptr(ref_count_ptr const& rhs) : px(rhs.px)
  {
    if (px != 0) {
      px->increment();
    }
  }

  ~ref_count_ptr()
  {
    if (px != 0) {
      px->decrement();
    }
  }

  template<class U> ref_count_ptr& operator=(ref_count_ptr<U> const& rhs)
  {
    this_type(rhs).swap(*this);
    return *this;
  }

  // Move support
  ref_count_ptr(ref_count_ptr&& rhs) noexcept : px(rhs.px)
  {
    rhs.px = 0;
  }

  ref_count_ptr& operator=(ref_count_ptr&& rhs) noexcept
  {
    this_type(static_cast<ref_count_ptr&&>(rhs)).swap(*this);
    return *this;
  }

  template<class U> friend class ref_count_ptr;

  template<class U, typename std::enable_if<std::is_convertible<U*, T*>::value, int>::type = 0>
  ref_count_ptr(ref_count_ptr<U>&& rhs)
    : px(rhs.px)
  {
    rhs.px = 0;
  }

  template<class U>
  ref_count_ptr& operator=(ref_count_ptr<U>&& rhs) noexcept
  {
    this_type(static_cast<ref_count_ptr<U>&&>(rhs)).swap(*this);
    return *this;
  }

  ref_count_ptr& operator=(ref_count_ptr const& rhs)
  {
    this_type(rhs).swap(*this);
    return *this;
  }

  ref_count_ptr& operator=(T* rhs)
  {
    this_type(rhs).swap(*this);
    return *this;
  }

  void reset()
  {
    this_type().swap(*this);
  }

  void reset(T* rhs)
  {
    this_type(rhs).swap(*this);
  }

  void reset(T* rhs, bool add_ref)
  {
    this_type(rhs, add_ref).swap(*this);
  }

  T* get() const noexcept
  {
    return px;
  }

  [[nodiscard]] T* detach() noexcept
  {
    T* ret = px;
    px = 0;
    return ret;
  }

  T& operator*() const noexcept
  {
    assert(px != 0);
    return *px;
  }

  T* operator->() const noexcept
  {
    assert(px != 0);
    return px;
  }

  // implicit conversion to "bool"
  explicit operator bool() const noexcept
  {
    return px != 0;
  }

  bool operator! () const noexcept
  {
    return px == 0;
  }

  void swap(ref_count_ptr& rhs) noexcept
  {
    T* tmp = px;
    px = rhs.px;
    rhs.px = tmp;
  }

private:
  T* px;
};

template<typename T>
class weak_ref : public ref_counter
{
  template<typename U> friend class ref_weak_counter;
public:
  explicit weak_ref(T* ptr) noexcept
    : ptr_(ptr)
  {

  }

protected:
  ~weak_ref() override = default;

public:
  T* get() {
    return ptr_;
  }

private:
  void reset() {
    ptr_ = nullptr;
  }
  T* ptr_{ nullptr };
};

template<typename T>
class ref_weak_counter
{
public:
  ref_weak_counter() noexcept
    : strong_(0)
    , weak_(nullptr)
  {

  }

  ref_weak_counter(const ref_weak_counter&) noexcept
    : strong_(0)
    , weak_(nullptr)
  {
  }

  ref_weak_counter& operator=(const ref_weak_counter&) noexcept {
    return *this;
  }

  unsigned int increment() noexcept {
    return strong_.fetch_add(1, std::memory_order_acq_rel) + 1;
  }

  unsigned int decrement() {
    unsigned int count = strong_.fetch_add(-1, std::memory_order_acq_rel);
    if (1 == count) {
      stdext::weak_ref<T>* weak = weak_;
      if (weak) {
        weak->reset();
        weak->decrement();
        weak = nullptr;
      }
      on_final_destroy();
    }
    return count - 1;
  }

  stdext::weak_ref<T>* weak_ref() {
    stdext::weak_ref<T>* before = weak_.load(std::memory_order_acquire);
    do {
      if (before) {
        return before;
      }
      stdext::weak_ref<T>* temp = DBG_NEW stdext::weak_ref<T>(static_cast<T*>(this));
      temp->increment();
      if (!weak_.compare_exchange_strong(before, temp, std::memory_order_acq_rel)) {
        temp->decrement();
      } else {
        return temp;
      }
    } while (true);
  }

  unsigned int use_count() const noexcept {
    return strong_.load(std::memory_order_acquire);
  }

protected:
  virtual ~ref_weak_counter() = default;

  virtual void on_final_destroy() {
    delete this;
  }

private:
  std::atomic_int_least32_t strong_;
  std::atomic<stdext::weak_ref<T>*> weak_;
};

template<class T>
class ref_weak_ptr
{
private:
  typedef ref_weak_ptr this_type;

public:
  constexpr ref_weak_ptr() noexcept : weak_ref_(nullptr)
  {
  }

  ref_weak_ptr(T* p)
  {
    if (p) {
      weak_ref_ = p->weak_ref();
    }
  }

  template<class U, typename std::enable_if<std::is_convertible<U*, T*>::value, int>::type = 0>
  ref_weak_ptr(const ref_weak_ptr<U>&  rhs)
    : weak_ref_(rhs.weak_ref_)
  {

  }

  template<class U, typename std::enable_if<std::is_convertible<U*, T*>::value, int>::type = 0>
  ref_weak_ptr(const ref_count_ptr<U>& rhs)
    : ref_weak_ptr(rhs.px)
    
  {

  }

  template<class U> ref_weak_ptr& operator=(const ref_weak_ptr<U>& rhs)
  {
    this_type(rhs).swap(*this);
    return *this;
  }

  template<class U> friend class ref_weak_ptr;

  template<class U, typename std::enable_if<std::is_convertible<U*, T*>::value, int>::type = 0>
  ref_weak_ptr(ref_weak_ptr<U>&& rhs)
    : weak_ref_(rhs.weak_ref_)
  {

  }

  template<class U>
  ref_weak_ptr& operator=(ref_weak_ptr<U>&& rhs) noexcept
  {
    this_type(static_cast<ref_weak_ptr<U>&&>(rhs)).swap(*this);
    return *this;
  }

  ref_weak_ptr& operator=(T* rhs)
  {
    this_type(rhs).swap(*this);
    return *this;
  }

  void reset()
  {
    this_type().swap(*this);
  }

  void reset(T* rhs)
  {
    this_type(rhs).swap(*this);
  }

  ref_count_ptr<T> lock()
  {
    if (!weak_ref_) {
      return nullptr;
    }
    return ref_count_ptr<T>(weak_ref_->get());
  }

  bool expired() const noexcept {
    if (!weak_ref_) {
      return false;
    }
    return !!weak_ref_->get();
  }

  void swap(ref_weak_ptr& rhs) noexcept
  {
    weak_ref_.swap(rhs.weak_ref_);
  }

private:
  ref_count_ptr<weak_ref<T>> weak_ref_;
};

template<class T, class U> inline bool operator==(ref_count_ptr<T> const& a, ref_count_ptr<U> const& b) noexcept
{
  return a.get() == b.get();
}

template<class T, class U> inline bool operator!=(ref_count_ptr<T> const& a, ref_count_ptr<U> const& b) noexcept
{
  return a.get() != b.get();
}

template<class T, class U> inline bool operator==(ref_count_ptr<T> const& a, U* b) noexcept
{
  return a.get() == b;
}

template<class T, class U> inline bool operator!=(ref_count_ptr<T> const& a, U* b) noexcept
{
  return a.get() != b;
}

template<class T, class U> inline bool operator==(T* a, ref_count_ptr<U> const& b) noexcept
{
  return a == b.get();
}

template<class T, class U> inline bool operator!=(T* a, ref_count_ptr<U> const& b) noexcept
{
  return a != b.get();
}

template<class T> inline bool operator==(ref_count_ptr<T> const& p, std::nullptr_t) noexcept
{
  return p.get() == 0;
}

template<class T> inline bool operator==(std::nullptr_t, ref_count_ptr<T> const& p) noexcept
{
  return p.get() == 0;
}

template<class T> inline bool operator!=(ref_count_ptr<T> const& p, std::nullptr_t) noexcept
{
  return p.get() != 0;
}

template<class T> inline bool operator!=(std::nullptr_t, ref_count_ptr<T> const& p) noexcept
{
  return p.get() != 0;
}

template<class T> inline bool operator<(ref_count_ptr<T> const& a, ref_count_ptr<T> const& b) noexcept
{
  return std::less<T*>()(a.get(), b.get());
}

template<class T> void swap(ref_count_ptr<T>& lhs, ref_count_ptr<T>& rhs) noexcept
{
  lhs.swap(rhs);
}

// mem_fn support
template<class T> T* get_pointer(ref_count_ptr<T> const& p) noexcept
{
  return p.get();
}

template<class T, class U> ref_count_ptr<T> static_pointer_cast(ref_count_ptr<U> const& p)
{
  return static_cast<T*>(p.get());
}

template<class T, class U> ref_count_ptr<T> const_pointer_cast(ref_count_ptr<U> const& p)
{
  return const_cast<T*>(p.get());
}

template<class T, class U> ref_count_ptr<T> dynamic_pointer_cast(ref_count_ptr<U> const& p)
{
  return dynamic_cast<T*>(p.get());
}

template<class T, class U> ref_count_ptr<T> static_pointer_cast(ref_count_ptr<U>&& p) noexcept
{
  return ref_count_ptr<T>(static_cast<T*>(p.detach()), false);
}

template<class T, class U> ref_count_ptr<T> const_pointer_cast(ref_count_ptr<U>&& p) noexcept
{
  return ref_count_ptr<T>(const_cast<T*>(p.detach()), false);
}

template<class T, class U> ref_count_ptr<T> dynamic_pointer_cast(ref_count_ptr<U>&& p) noexcept
{
  T* p2 = dynamic_cast<T*>(p.get());

  ref_count_ptr<T> r(p2, false);

  if (p2) p.detach();

  return r;
}

template<class E, class T, class Y> std::basic_ostream<E, T>& operator<< (std::basic_ostream<E, T>& os, ref_count_ptr<Y> const& p)
{
  os << p.get();
  return os;
}

}

namespace std
{
  template<class T> struct hash< stdext::ref_count_ptr<T> >
  {
    std::size_t operator()(stdext::ref_count_ptr<T> const& p) const noexcept
    {
      return std::hash< T* >()(p.get());
    }
  };
} // namespace std

namespace stdext
{

template< class T > struct hash;

template< class T > std::size_t hash_value(ref_count_ptr<T> const& p) noexcept
{
  return std::hash< T* >()(p.get());
}

}

#endif // REF_COUNTER_H_
