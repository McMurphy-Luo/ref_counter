#ifndef REF_COUNTER_H_
#define REF_COUNTER_H_

#include <atomic>
#include <ostream>
#include <type_traits>
#include <functional>

namespace neo
{
  struct ThreadUnsafeCounter
  {
    typedef unsigned int Type;

    static unsigned int Load(Type const& counter) noexcept
    {
      return counter;
    }

    static void Increment(Type& counter) noexcept
    {
      ++counter;
    }

    static unsigned int Decrement(Type& counter) noexcept
    {
      return --counter;
    }
  };

  struct ThreadSafeCounter
  {
    typedef std::atomic<unsigned int> Type;

    static unsigned int Load(Type const& counter) noexcept
    {
      return counter.load();
    }

    static void Increment(Type& counter) noexcept
    {
      ++counter;
    }

    static unsigned int Decrement(Type& counter) noexcept
    {
      return --counter;
    }
  };

  template<typename CounterPolicy = ThreadSafeCounter>
  class RefCounter
  {
  public:
    RefCounter() noexcept
      : m_ref_counter(0)
    {

    }

    RefCounter(RefCounter const&) noexcept
      : m_ref_counter(0)
    {

    }

    RefCounter& operator= (RefCounter const&) noexcept { return *this; }

    void Increment() noexcept {
      CounterPolicy::Increment(m_ref_counter);
    }

    void Decrement() {
      if (CounterPolicy::Decrement(m_ref_counter) == 0)
        OnFinalDestroy();
    }

    unsigned int UseCount() const noexcept {
      return CounterPolicy::Load(m_ref_counter);
    }

  protected:
    virtual ~RefCounter() = default;

    virtual void OnFinalDestroy() {
      delete this;
    }

  private:
    typedef typename CounterPolicy::Type CounterType;
    CounterType m_ref_counter;
  };

  template<class T>
  class RefCounterPtr
  {
  private:
    typedef RefCounterPtr ThisType;

  public:
    constexpr RefCounterPtr() noexcept : px(0)
    {
    }

    RefCounterPtr(T* p, bool add_ref = true) : px(p)
    {
      if (px != 0 && add_ref) px->Increment();
    }

    template<class U, typename std::enable_if<std::is_convertible<U*, T*>::value, int>::type = 0>
    RefCounterPtr(RefCounterPtr<U> const& rhs)
      : px(rhs.Get())
    {
      if (px != 0) px->Increment();
    }

    RefCounterPtr(RefCounterPtr const& rhs) : px(rhs.px)
    {
      if (px != 0) px->Increment();
    }

    ~RefCounterPtr()
    {
      if (px != 0) px->Decrement();
    }

    template<class U> RefCounterPtr& operator=(RefCounterPtr<U> const& rhs)
    {
      ThisType(rhs).swap(*this);
      return *this;
    }

    // Move support
    RefCounterPtr(RefCounterPtr&& rhs) noexcept : px(rhs.px)
    {
      rhs.px = 0;
    }

    RefCounterPtr& operator=(RefCounterPtr&& rhs) noexcept
    {
      ThisType(static_cast<RefCounterPtr&&>(rhs)).swap(*this);
      return *this;
    }

    template<class U> friend class RefCounterPtr;

    template<class U, typename std::enable_if<std::is_convertible<U*, T*>::value, int>::type = 0>
    RefCounterPtr(RefCounterPtr<U>&& rhs)
      : px(rhs.px)
    {
      rhs.px = 0;
    }

    template<class U>
    RefCounterPtr& operator=(RefCounterPtr<U>&& rhs) noexcept
    {
      ThisType(static_cast<RefCounterPtr<U>&&>(rhs)).swap(*this);
      return *this;
    }

    RefCounterPtr& operator=(RefCounterPtr const& rhs)
    {
      ThisType(rhs).swap(*this);
      return *this;
    }

    RefCounterPtr& operator=(T* rhs)
    {
      ThisType(rhs).swap(*this);
      return *this;
    }

    void Reset()
    {
      ThisType().swap(*this);
    }

    void Reset(T* rhs)
    {
      ThisType(rhs).swap(*this);
    }

    void Reset(T* rhs, bool add_ref)
    {
      ThisType(rhs, add_ref).swap(*this);
    }

    T* Get() const noexcept
    {
      return px;
    }

    [[nodiscard]] T* Detach() noexcept
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

    void swap(RefCounterPtr& rhs) noexcept
    {
      T* tmp = px;
      px = rhs.px;
      rhs.px = tmp;
    }

  private:
    T* px;
  };

  template<class T, class U> inline bool operator==(RefCounterPtr<T> const& a, RefCounterPtr<U> const& b) noexcept
  {
    return a.Get() == b.Get();
  }

  template<class T, class U> inline bool operator!=(RefCounterPtr<T> const& a, RefCounterPtr<U> const& b) noexcept
  {
    return a.Get() != b.Get();
  }

  template<class T, class U> inline bool operator==(RefCounterPtr<T> const& a, U* b) noexcept
  {
    return a.Get() == b;
  }

  template<class T, class U> inline bool operator!=(RefCounterPtr<T> const& a, U* b) noexcept
  {
    return a.Get() != b;
  }

  template<class T, class U> inline bool operator==(T* a, RefCounterPtr<U> const& b) noexcept
  {
    return a == b.Get();
  }

  template<class T, class U> inline bool operator!=(T* a, RefCounterPtr<U> const& b) noexcept
  {
    return a != b.Get();
  }

  template<class T> inline bool operator==(RefCounterPtr<T> const& p, std::nullptr_t) noexcept
  {
    return p.Get() == 0;
  }

  template<class T> inline bool operator==(std::nullptr_t, RefCounterPtr<T> const& p) noexcept
  {
    return p.Get() == 0;
  }

  template<class T> inline bool operator!=(RefCounterPtr<T> const& p, std::nullptr_t) noexcept
  {
    return p.Get() != 0;
  }

  template<class T> inline bool operator!=(std::nullptr_t, RefCounterPtr<T> const& p) noexcept
  {
    return p.Get() != 0;
  }

  template<class T> inline bool operator<(RefCounterPtr<T> const& a, RefCounterPtr<T> const& b) noexcept
  {
    return std::less<T*>()(a.Get(), b.Get());
  }

  template<class T> void swap(RefCounterPtr<T>& lhs, RefCounterPtr<T>& rhs) noexcept
  {
    lhs.swap(rhs);
  }

  // mem_fn support
  template<class T> T* get_pointer(RefCounterPtr<T> const& p) noexcept
  {
    return p.Get();
  }

  template<class T, class U> RefCounterPtr<T> static_pointer_cast(RefCounterPtr<U> const& p)
  {
    return static_cast<T*>(p.Get());
  }

  template<class T, class U> RefCounterPtr<T> const_pointer_cast(RefCounterPtr<U> const& p)
  {
    return const_cast<T*>(p.Get());
  }

  template<class T, class U> RefCounterPtr<T> dynamic_pointer_cast(RefCounterPtr<U> const& p)
  {
    return dynamic_cast<T*>(p.Get());
  }

  template<class T, class U> RefCounterPtr<T> static_pointer_cast(RefCounterPtr<U>&& p) noexcept
  {
    return RefCounterPtr<T>(static_cast<T*>(p.Detach()), false);
  }

  template<class T, class U> RefCounterPtr<T> const_pointer_cast(RefCounterPtr<U>&& p) noexcept
  {
    return RefCounterPtr<T>(const_cast<T*>(p.Detach()), false);
  }

  template<class T, class U> RefCounterPtr<T> dynamic_pointer_cast(RefCounterPtr<U>&& p) noexcept
  {
    T* p2 = dynamic_cast<T*>(p.Get());

    RefCounterPtr<T> r(p2, false);

    if (p2) p.Detach();

    return r;
  }

  template<class E, class T, class Y> std::basic_ostream<E, T>& operator<< (std::basic_ostream<E, T>& os, RefCounterPtr<Y> const& p)
  {
    os << p.Get();
    return os;
  }

} // namespace neo

namespace std
{
  template<class T> struct hash< neo::RefCounterPtr<T> >
  {
    std::size_t operator()(neo::RefCounterPtr<T> const& p) const noexcept
    {
      return std::hash< T* >()(p.Get());
    }
  };
} // namespace std

namespace neo
{
  template< class T > struct hash;

  template< class T > std::size_t hash_value(RefCounterPtr<T> const& p) noexcept
  {
    return std::hash< T* >()(p.Get());
  }
} // namespace neo

#endif // REF_COUNTER_H_
