#ifndef REF_COUNTER_H_
#define REF_COUNTER_H_

#include <atomic>

namespace Cmm
{
  namespace detail
  {
    template< class Y, class T > struct sp_convertible
    {
      typedef char(&yes)[1];
      typedef char(&no)[2];

      static yes f(T*);
      static no  f(...);

      enum _vt { value = sizeof((f)(static_cast<Y*>(0))) == sizeof(yes) };
    };

    template< class Y, class T > struct sp_convertible< Y, T[] >
    {
      enum _vt { value = false };
    };

    template< class Y, class T > struct sp_convertible< Y[], T[] >
    {
      enum _vt { value = sp_convertible< Y[1], T[1] >::value };
    };

    template< class Y, std::size_t N, class T > struct sp_convertible< Y[N], T[] >
    {
      enum _vt { value = sp_convertible< Y[1], T[1] >::value };
    };

    struct sp_empty
    {
    };

    template< bool > struct sp_enable_if_convertible_impl;

    template<> struct sp_enable_if_convertible_impl<true>
    {
      typedef sp_empty type;
    };

    template<> struct sp_enable_if_convertible_impl<false>
    {
    };

    template< class Y, class T > struct sp_enable_if_convertible : public sp_enable_if_convertible_impl< sp_convertible< Y, T >::value >
    {
    };
  } // namespace detail

  class ref_counter_base {
  public:
    virtual void increment() = 0;

    virtual void decrement() = 0;

    virtual unsigned int use_count() const = 0;

  protected:
    virtual ~ref_counter_base() = default;
  };

  struct thread_unsafe_counter
  {
    typedef unsigned int type;

    static unsigned int load(type const& counter)
    {
      return counter;
    }

    static void increment(type& counter)
    {
      ++counter;
    }

    static unsigned int decrement(type& counter)
    {
      return --counter;
    }
  };

  struct thread_safe_counter
  {
    typedef std::atomic<unsigned int> type;

    static unsigned int load(type const& counter) noexcept
    {
      return static_cast<unsigned int>(static_cast<long>(counter));
    }

    static void increment(type& counter) noexcept
    {
      ++counter;
    }

    static unsigned int decrement(type& counter) noexcept
    {
      return static_cast<unsigned int>(--counter);
    }
  };

  template< typename Derived, typename CounterPolicy = thread_safe_counter >
  class ref_counter;
  template< typename Derived, typename CounterPolicy >
  void ref_counter_add_ref(const ref_counter< Derived, CounterPolicy >* p) noexcept;
  template< typename Derived, typename CounterPolicy >
  void ref_counter_release(const ref_counter< Derived, CounterPolicy >* p) noexcept;

  template<typename Derived, typename CounterPolicy>
  class ref_counter
  {
  private:
    typedef typename CounterPolicy::type counter_type;
    mutable counter_type m_ref_counter;

    friend void ref_counter_add_ref< Derived, CounterPolicy >(const ref_counter< Derived, CounterPolicy >* p) noexcept;
    friend void ref_counter_release< Derived, CounterPolicy >(const ref_counter< Derived, CounterPolicy >* p) noexcept;

  public:
    ref_counter()
      : m_ref_counter(0)
    {

    }

    ref_counter(ref_counter const&)
      : m_ref_counter(0)
    {

    }

    ref_counter& operator= (ref_counter const&) { return *this; }

    void increment() {
      ref_counter_add_ref(this);
    }

    void decrement() {
      ref_counter_release(static_cast<const Derived*>(this));
    }

    unsigned int use_count() const {
      return CounterPolicy::load(m_ref_counter);
    }

  protected:
    ~ref_counter() = default;
  };

  template< typename Derived, typename CounterPolicy >
  inline void ref_counter_add_ref(const ref_counter< Derived, CounterPolicy >* p) noexcept
  {
    CounterPolicy::increment(p->m_ref_counter);
  }

  template< typename Derived, typename CounterPolicy >
  inline void ref_counter_release(const ref_counter< Derived, CounterPolicy >* p) noexcept
  {
    if (CounterPolicy::decrement(p->m_ref_counter) == 0)
      delete static_cast<const Derived*>(p);
  }

#define FORWARD_DEFINE_REF_COUNTER(X) \
  virtual void increment() override {\
    return X::increment();\
  }\
  virtual void decrement() override {\
    return X::decrement();\
  }\
  virtual unsigned int use_count() const override {\
    return X::use_count();\
  }\

  template<class T>
  class ref_counter_ptr
  {
  private:
    typedef ref_counter_ptr this_type;

  public:
    constexpr ref_counter_ptr() : px(0)
    {
    }

    ref_counter_ptr(T* p, bool add_ref = true) : px(p)
    {
      if (px != 0 && add_ref) px->increment();
    }

    template<class U>
    ref_counter_ptr(ref_counter_ptr<U> const& rhs, typename detail::sp_enable_if_convertible<U, T>::type = detail::sp_empty())
      : px(rhs.get())
    {
      if (px != 0) px->increment();
    }

    ref_counter_ptr(ref_counter_ptr const& rhs) : px(rhs.px)
    {
      if (px != 0) px->increment();
    }

    ~ref_counter_ptr()
    {
      if (px != 0) px->decrement();
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

    template<class U>
    ref_counter_ptr(ref_counter_ptr<U>&& rhs, typename detail::sp_enable_if_convertible<U, T>::type = detail::sp_empty())
      : px(rhs.px)
    {
      rhs.px = 0;
    }

    template<class U>
    ref_counter_ptr& operator=(ref_counter_ptr<U>&& rhs)
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

    T* get() const
    {
      return px;
    }

    T* detach()
    {
      T* ret = px;
      px = 0;
      return ret;
    }

    T& operator*() const
    {
      assert(px != 0);
      return *px;
    }

    T* operator->() const
    {
      assert(px != 0);
      return px;
    }

    // implicit conversion to "bool"
    explicit operator bool() const
    {
      return px != 0;
    }

    bool operator! () const
    {
      return px == 0;
    }

    void swap(ref_counter_ptr& rhs)
    {
      T* tmp = px;
      px = rhs.px;
      rhs.px = tmp;
    }

  private:
    T* px;
  };

  template<class T, class U> inline bool operator==(ref_counter_ptr<T> const& a, ref_counter_ptr<U> const& b)
  {
    return a.get() == b.get();
  }

  template<class T, class U> inline bool operator!=(ref_counter_ptr<T> const& a, ref_counter_ptr<U> const& b)
  {
    return a.get() != b.get();
  }

  template<class T, class U> inline bool operator==(ref_counter_ptr<T> const& a, U* b)
  {
    return a.get() == b;
  }

  template<class T, class U> inline bool operator!=(ref_counter_ptr<T> const& a, U* b)
  {
    return a.get() != b;
  }

  template<class T, class U> inline bool operator==(T* a, ref_counter_ptr<U> const& b)
  {
    return a == b.get();
  }

  template<class T, class U> inline bool operator!=(T* a, ref_counter_ptr<U> const& b)
  {
    return a != b.get();
  }

  template<class T> inline bool operator==(ref_counter_ptr<T> const& p, std::nullptr_t)
  {
    return p.get() == 0;
  }

  template<class T> inline bool operator==(std::nullptr_t, ref_counter_ptr<T> const& p)
  {
    return p.get() == 0;
  }

  template<class T> inline bool operator!=(ref_counter_ptr<T> const& p, std::nullptr_t)
  {
    return p.get() != 0;
  }

  template<class T> inline bool operator!=(std::nullptr_t, ref_counter_ptr<T> const& p)
  {
    return p.get() != 0;
  }

  template<class T> inline bool operator<(ref_counter_ptr<T> const& a, ref_counter_ptr<T> const& b)
  {
    return std::less<T*>()(a.get(), b.get());
  }

  template<class T> void swap(ref_counter_ptr<T>& lhs, ref_counter_ptr<T>& rhs)
  {
    lhs.swap(rhs);
  }

  // mem_fn support
  template<class T> T* get_pointer(ref_counter_ptr<T> const& p)
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

  template<class T, class U> ref_counter_ptr<T> static_pointer_cast(ref_counter_ptr<U>&& p)
  {
    return ref_counter_ptr<T>(static_cast<T*>(p.detach()), false);
  }

  template<class T, class U> ref_counter_ptr<T> const_pointer_cast(ref_counter_ptr<U>&& p)
  {
    return ref_counter_ptr<T>(const_cast<T*>(p.detach()), false);
  }

  template<class T, class U> ref_counter_ptr<T> dynamic_pointer_cast(ref_counter_ptr<U>&& p)
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

} // namespace Cmm

namespace std
{
  template<class T> struct hash< Cmm::ref_counter_ptr<T> >
  {
    std::size_t operator()(Cmm::ref_counter_ptr<T> const& p) const
    {
      return std::hash< T* >()(p.get());
    }
  };
} // namespace std

namespace Cmm
{
  template< class T > struct hash;

  template< class T > std::size_t hash_value(ref_counter_ptr<T> const& p)
  {
    return std::hash< T* >()(p.get());
  }
} // namespace Cmm

#endif // REF_COUNTER_H_
