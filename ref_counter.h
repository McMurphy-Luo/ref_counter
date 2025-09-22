#ifndef REF_COUNTER_H_
#define REF_COUNTER_H_

#include <atomic>
#include <ostream>
#include <type_traits>
#include <functional>
#include <assert.h>

namespace stdext
{

struct thread_unsafe_counter
{
  typedef int_least32_t value_type;
  typedef int_least32_t type;

  static value_type load(type const& counter) noexcept
  {
    return counter;
  }

  static value_type increment(type& counter) noexcept
  {
    ++counter;
  }

  static value_type decrement(type& counter) noexcept
  {
    return --counter;
  }

  static bool compare_exchange_weak(type& counter, value_type& expected, value_type desired) noexcept
  {
    if (counter == expected) {
      counter = desired;
      return true;
    }
    expected = counter;
    return false;
  }
};

struct thread_safe_counter
{
  typedef int_least32_t value_type;
  typedef std::atomic_int_least32_t type;

  static value_type load(type const& counter) noexcept
  {
    return counter.load(std::memory_order_acquire);
  }

  static value_type increment(type& counter) noexcept
  {
    return counter.fetch_add(1, std::memory_order_acq_rel) + 1;
  }

  static value_type decrement(type& counter) noexcept
  {
    return counter.fetch_add(-1, std::memory_order_acq_rel) - 1;
  }

  static bool compare_exchange_weak(type& counter, value_type& expected, value_type desired) noexcept
  {
    return counter.compare_exchange_weak(expected, desired, std::memory_order_seq_cst);
  }
};

template<typename counter_policy = thread_safe_counter>
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

  typename counter_policy::value_type increment() noexcept {
    return counter_policy::increment(count_);
  }

  typename counter_policy::value_type decrement() {
    typename counter_policy::value_type count = counter_policy::decrement(count_);
    assert(count >= 0);
    if (count == 0) {
      on_final_destroy();
    }
    return count;
  }

  typename counter_policy::value_type use_count() const noexcept {
    return counter_policy::load(count_);
  }

protected:
  virtual ~ref_counter() = default;

  virtual void on_final_destroy() {
    delete this;
  }

private:
  typename counter_policy::type count_;
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

template<typename counter_policy = thread_safe_counter> 
class ref_weak_counter;

template<typename counter_policy = thread_safe_counter>
class ref_ctrl_block : public ref_counter<counter_policy>
{
public:
  ref_ctrl_block(ref_weak_counter<counter_policy>* the_ptr)
    : ref_counter<counter_policy>()
    , strong(0)
    , ptr(the_ptr)
  {

  }

public:
  typename counter_policy::type strong;
  ref_weak_counter<counter_policy>* ptr;
};

template<typename counter_policy>
class ref_weak_counter
{
  template<class U> friend class ref_weak_ptr;
public:
  typedef counter_policy counter_policy;

public:
  ref_weak_counter() noexcept
    : ctrl_block_(new ref_ctrl_block<counter_policy>(this))
  {

  }

  ref_weak_counter(const ref_weak_counter&) noexcept
    : ctrl_block_(new ref_ctrl_block<counter_policy>(this))
  {
  }

  ref_weak_counter& operator=(const ref_weak_counter&) noexcept {
    return *this;
  }

  typename counter_policy::value_type increment() noexcept {
    return counter_policy::increment(ctrl_block_->strong);
  }

  typename counter_policy::value_type decrement() {
    typename counter_policy::value_type count = counter_policy::decrement(ctrl_block_->strong);
    assert(count >= 0);
    if (0 == count) {
      on_final_destroy();
    }
    return count;
  }

  typename counter_policy::value_type use_count() const noexcept {
    return counter_policy::load(ctrl_block_->strong);
  }

protected:
  virtual ~ref_weak_counter() = default;

  virtual void on_final_destroy() {
    delete this;
  }

private:
  ref_count_ptr<ref_ctrl_block<counter_policy>> ctrl_block_;
};

template<class T>
class ref_weak_ptr
{
private:
  typedef typename T::counter_policy counter_policy;
  typedef ref_weak_ptr this_type;

public:
  constexpr ref_weak_ptr() noexcept
    : ctrl_block_(nullptr)
  {
  }

  template<class U, typename std::enable_if<std::is_convertible<U*, T*>::value, int>::type = 0>
  ref_weak_ptr(U* p)
  {
    if (p) {
      ctrl_block_ = p->ctrl_block_;
    }
  }

  template<class U, typename std::enable_if<std::is_convertible<U*, T*>::value, int>::type = 0>
  ref_weak_ptr(const ref_weak_ptr<U>&  rhs)
    : ctrl_block_(rhs.ctrl_block_)
  {

  }

  template<class U, typename std::enable_if<std::is_convertible<U*, T*>::value, int>::type = 0>
  ref_weak_ptr(const ref_count_ptr<U>& rhs)
    : ref_weak_ptr(rhs.get())
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
    : ctrl_block_(rhs.ctrl_block_)
  {

  }

  template<class U>
  ref_weak_ptr& operator=(ref_weak_ptr<U>&& rhs) noexcept
  {
    this_type(static_cast<ref_weak_ptr<U>&&>(rhs)).swap(*this);
    return *this;
  }

  template<class U, typename std::enable_if<std::is_convertible<U*, T*>::value, int>::type = 0>
  ref_weak_ptr& operator=(U* rhs)
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
    if (!ctrl_block_) {
      return nullptr;
    }
    typename counter_policy::value_type count = counter_policy::load(ctrl_block_->strong);
    typename counter_policy::value_type new_count = 0;
    do {
      if (count == 0) {
        return nullptr;
      }
      new_count = count + 1;
    } while (!counter_policy::compare_exchange_weak(ctrl_block_->strong, count, new_count));
    return ref_count_ptr<T>(static_cast<T*>(ctrl_block_->ptr), false);
  }

  bool expired() const noexcept {
    if (!ctrl_block_) {
      return true;
    }
    return 0 == counter_policy::load(ctrl_block_->strong);
  }

  void swap(ref_weak_ptr& rhs) noexcept {
    ctrl_block_.swap(rhs.ctrl_block_);
  }

private:
  ref_count_ptr<ref_ctrl_block<counter_policy>> ctrl_block_;
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
