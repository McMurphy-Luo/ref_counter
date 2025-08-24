#ifndef REF_COUNTER_H_
#define REF_COUNTER_H_

#include <atomic>
#include <ostream>
#include <type_traits>
#include <functional>

class ref_counter
{
public:
  ref_counter() noexcept = default;
  ref_counter(const ref_counter&) = delete;
  ref_counter(const ref_counter&&) = delete;
  ref_counter& operator=(const ref_counter&) = delete;
  ref_counter& operator=(ref_counter&&) = delete;

  unsigned int increment() noexcept {
    return ++count_;
  }

  unsigned int decrement() {
    unsigned int count = count_;
    if (--count_ == 0) {
      on_final_destroy();
    }
    return count;
  }

  unsigned int use_count() const noexcept {
    return count_;
  }

protected:
  virtual ~ref_counter() = default;

  virtual void on_final_destroy() {
    delete this;
  }

private:
  std::atomic<unsigned int> count_{ 0 };
};

template<class T>
class ref_counter_ptr
{
private:
  typedef ref_counter_ptr this_type;

public:
  constexpr ref_counter_ptr() noexcept : px(0)
  {
  }

  ref_counter_ptr(T* p, bool add_ref = true) : px(p)
  {
    if (px != 0 && add_ref) {
      px->increment();
    }
  }

  template<class U, typename std::enable_if<std::is_convertible<U*, T*>::value, int>::type = 0>
  ref_counter_ptr(ref_counter_ptr<U> const& rhs)
    : px(rhs.get())
  {
    if (px != 0) {
      px->increment();
    }
  }

  ref_counter_ptr(ref_counter_ptr const& rhs) : px(rhs.px)
  {
    if (px != 0) {
      px->increment();
    }
  }

  ~ref_counter_ptr()
  {
    if (px != 0) {
      px->decrement();
    }
  }

  template<class U> ref_counter_ptr& operator=(ref_counter_ptr<U> const& rhs)
  {
    this_type(rhs).swap(*this);
    return *this;
  }

  // Move support
  ref_counter_ptr(ref_counter_ptr&& rhs) noexcept : px(rhs.px)
  {
    rhs.px = 0;
  }

  ref_counter_ptr& operator=(ref_counter_ptr&& rhs) noexcept
  {
    this_type(static_cast<ref_counter_ptr&&>(rhs)).swap(*this);
    return *this;
  }

  template<class U> friend class ref_counter_ptr;

  template<class U, typename std::enable_if<std::is_convertible<U*, T*>::value, int>::type = 0>
  ref_counter_ptr(ref_counter_ptr<U>&& rhs)
    : px(rhs.px)
  {
    rhs.px = 0;
  }

  template<class U>
  ref_counter_ptr& operator=(ref_counter_ptr<U>&& rhs) noexcept
  {
    this_type(static_cast<ref_counter_ptr<U>&&>(rhs)).swap(*this);
    return *this;
  }

  ref_counter_ptr& operator=(ref_counter_ptr const& rhs)
  {
    this_type(rhs).swap(*this);
    return *this;
  }

  ref_counter_ptr& operator=(T* rhs)
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

  void swap(ref_counter_ptr& rhs) noexcept
  {
    T* tmp = px;
    px = rhs.px;
    rhs.px = tmp;
  }

private:
  T* px;
};

template<typename T>
class ctrl_block : public ref_counter
{
public:
  explicit ctrl_block(T* ptr) noexcept
    : ptr_(ptr)
  {
      
  }
  ctrl_block(const ctrl_block&) = delete;
  ctrl_block(const ctrl_block&&) = delete;
  ctrl_block& operator=(const ctrl_block&) = delete;
  ctrl_block& operator=(ctrl_block&&) = delete;

protected:
  ~ctrl_block() override = default;

public:
  void reset(T* ptr) {
    ptr_ = ptr;
  }

private:
  T* ptr_{ nullptr };
};

template<typename T>
class ref_weak_counter
{
public:
  ref_weak_counter() noexcept = default;
  ref_weak_counter(const ref_weak_counter&) = delete;
  ref_weak_counter(const ref_weak_counter&&) = delete;
  ref_weak_counter& operator=(const ref_weak_counter&) = delete;
  ref_weak_counter& operator=(ref_weak_counter&&) = delete;

  unsigned int increment() noexcept {
    return ++count_;
  }

  unsigned int decrement() {
    unsigned int count = count_;
    if (0 == --count_) {
      ctrl_block<T>* ctrl_block = ctrl_block_;
      if (ctrl_block) {
        ctrl_block->reset(nullptr);
        ctrl_block->decrement();
        ctrl_block = nullptr;
      }
      OnFinalDestroy();
    }
    return count;
  }

  ref_counter_ptr<ctrl_block<T>> weak_ref() {
    ctrl_block<T>* before = ctrl_block_;
    do {
      if (before) {
        return before;
      }
      ctrl_block<T>* temp = DBG_NEW ctrl_block<T>(this);
      if (!ctrl_block_.compare_exchange_strong(before, temp)) {
        delete temp;
      } else {
        temp->increment();
        return temp;
      }
    } while (true);
  }

  unsigned int use_count() const noexcept {
    return count_;
  }

protected:
  virtual ~ref_weak_counter() = default;

  virtual void OnFinalDestroy() {
    delete this;
  }

private:
  std::atomic<unsigned int> count_{ 0 };
  std::atomic<ctrl_block<T>*> ctrl_block_{ nullptr };
};

template<class T>
class ref_weak_ptr
{
private:
  typedef ref_weak_ptr this_type;

public:
  constexpr ref_weak_ptr() noexcept : ctrl_block_(nullptr)
  {
  }

  ref_weak_ptr(T* p)
  {
    if (p) {
      ctrl_block_ = p->weak_ref();
    }
  }

  template<class U, typename std::enable_if<std::is_convertible<U*, T*>::value, int>::type = 0>
  ref_weak_ptr(const ref_weak_ptr<U>&  rhs)
    : ctrl_block_(rhs.ctrl_block_)
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

  ref_counter_ptr<T> lock()
  {
    if (!ctrl_block_) {
      return nullptr;
    }
    return ref_counter_ptr<T>(ctrl_block_->ptr_);
  }

  void swap(ref_weak_ptr& rhs) noexcept
  {
    ctrl_block_.swap(rhs.ctrl_block_);
  }

private:
  ref_counter_ptr<ctrl_block<T>> ctrl_block_;
};

template<class T, class U> inline bool operator==(ref_counter_ptr<T> const& a, ref_counter_ptr<U> const& b) noexcept
{
  return a.get() == b.get();
}

template<class T, class U> inline bool operator!=(ref_counter_ptr<T> const& a, ref_counter_ptr<U> const& b) noexcept
{
  return a.get() != b.get();
}

template<class T, class U> inline bool operator==(ref_counter_ptr<T> const& a, U* b) noexcept
{
  return a.get() == b;
}

template<class T, class U> inline bool operator!=(ref_counter_ptr<T> const& a, U* b) noexcept
{
  return a.get() != b;
}

template<class T, class U> inline bool operator==(T* a, ref_counter_ptr<U> const& b) noexcept
{
  return a == b.get();
}

template<class T, class U> inline bool operator!=(T* a, ref_counter_ptr<U> const& b) noexcept
{
  return a != b.get();
}

template<class T> inline bool operator==(ref_counter_ptr<T> const& p, std::nullptr_t) noexcept
{
  return p.get() == 0;
}

template<class T> inline bool operator==(std::nullptr_t, ref_counter_ptr<T> const& p) noexcept
{
  return p.get() == 0;
}

template<class T> inline bool operator!=(ref_counter_ptr<T> const& p, std::nullptr_t) noexcept
{
  return p.get() != 0;
}

template<class T> inline bool operator!=(std::nullptr_t, ref_counter_ptr<T> const& p) noexcept
{
  return p.get() != 0;
}

template<class T> inline bool operator<(ref_counter_ptr<T> const& a, ref_counter_ptr<T> const& b) noexcept
{
  return std::less<T*>()(a.get(), b.get());
}

template<class T> void swap(ref_counter_ptr<T>& lhs, ref_counter_ptr<T>& rhs) noexcept
{
  lhs.swap(rhs);
}

// mem_fn support
template<class T> T* get_pointer(ref_counter_ptr<T> const& p) noexcept
{
  return p.get();
}

template<class T, class U> ref_counter_ptr<T> static_pointer_cast(ref_counter_ptr<U> const& p)
{
  return static_cast<T*>(p.get());
}

template<class T, class U> ref_counter_ptr<T> const_pointer_cast(ref_counter_ptr<U> const& p)
{
  return const_cast<T*>(p.get());
}

template<class T, class U> ref_counter_ptr<T> dynamic_pointer_cast(ref_counter_ptr<U> const& p)
{
  return dynamic_cast<T*>(p.get());
}

template<class T, class U> ref_counter_ptr<T> static_pointer_cast(ref_counter_ptr<U>&& p) noexcept
{
  return ref_counter_ptr<T>(static_cast<T*>(p.detach()), false);
}

template<class T, class U> ref_counter_ptr<T> const_pointer_cast(ref_counter_ptr<U>&& p) noexcept
{
  return ref_counter_ptr<T>(const_cast<T*>(p.detach()), false);
}

template<class T, class U> ref_counter_ptr<T> dynamic_pointer_cast(ref_counter_ptr<U>&& p) noexcept
{
  T* p2 = dynamic_cast<T*>(p.get());

  ref_counter_ptr<T> r(p2, false);

  if (p2) p.detach();

  return r;
}

template<class E, class T, class Y> std::basic_ostream<E, T>& operator<< (std::basic_ostream<E, T>& os, ref_counter_ptr<Y> const& p)
{
  os << p.get();
  return os;
}

namespace std
{
  template<class T> struct hash< ref_counter_ptr<T> >
  {
    std::size_t operator()(ref_counter_ptr<T> const& p) const noexcept
    {
      return std::hash< T* >()(p.get());
    }
  };
} // namespace std

template< class T > struct hash;

template< class T > std::size_t hash_value(ref_counter_ptr<T> const& p) noexcept
{
  return std::hash< T* >()(p.get());
}

#endif // REF_COUNTER_H_
