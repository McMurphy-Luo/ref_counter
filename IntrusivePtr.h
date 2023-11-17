#ifndef INTRUSIVE_PTR_H_
#define INTRUSIVE_PTR_H_

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
  }

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

  template<typename T, typename CounterPolicy = thread_safe_counter>
  class ref_counter : public T {
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

    virtual void increment() override {
      CounterPolicy::increment(m_ref_counter);
    }

    virtual void decrement() override {
      if (CounterPolicy::decrement(m_ref_counter) == 0) {
        delete this;
      }
    }

    virtual unsigned int use_count() const override {
      return CounterPolicy::load(m_ref_counter);
    }

  private:
    typedef typename CounterPolicy::type counter_type;;
    mutable counter_type m_ref_counter;
  };

  template<class T>
  class intrusive_ptr
  {
  private:
    typedef intrusive_ptr this_type;

  public:
    constexpr intrusive_ptr() : px(0)
    {
    }

    intrusive_ptr(T* p, bool add_ref = true) : px(p)
    {
      if (px != 0 && add_ref) px->increment();
    }

    template<class U>
    intrusive_ptr(intrusive_ptr<U> const& rhs, typename detail::sp_enable_if_convertible<U, T>::type = detail::sp_empty())
      : px(rhs.get())
    {
      if (px != 0) px->increment();
    }

    intrusive_ptr(intrusive_ptr const& rhs) : px(rhs.px)
    {
      if (px != 0) px->increment();
    }

    ~intrusive_ptr()
    {
      if (px != 0) px->decrement();
    }

    template<class U> intrusive_ptr& operator=(intrusive_ptr<U> const& rhs)
    {
      this_type(rhs).swap(*this);
      return *this;
    }

    // Move support
    intrusive_ptr(intrusive_ptr&& rhs) : px(rhs.px)
    {
      rhs.px = 0;
    }

    intrusive_ptr& operator=(intrusive_ptr&& rhs)
    {
      this_type(static_cast<intrusive_ptr&&>(rhs)).swap(*this);
      return *this;
    }

    template<class U> friend class intrusive_ptr;

    template<class U>
    intrusive_ptr(intrusive_ptr<U>&& rhs, typename detail::sp_enable_if_convertible<U, T>::type = detail::sp_empty())
      : px(rhs.px)
    {
      rhs.px = 0;
    }

    template<class U>
    intrusive_ptr& operator=(intrusive_ptr<U>&& rhs)
    {
      this_type(static_cast<intrusive_ptr<U>&&>(rhs)).swap(*this);
      return *this;
    }

    intrusive_ptr& operator=(intrusive_ptr const& rhs)
    {
      this_type(rhs).swap(*this);
      return *this;
    }

    intrusive_ptr& operator=(T* rhs)
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

    void swap(intrusive_ptr& rhs)
    {
      T* tmp = px;
      px = rhs.px;
      rhs.px = tmp;
    }

  private:
    T* px;
  };
}

#endif // INTRUSIVE_PTR_H_
