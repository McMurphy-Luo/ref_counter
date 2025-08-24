#include "ref_counter_test.h"
#include "catch_amalgamated.hpp"
#include "ref_counter.h"
#include <sstream>

using stdext::ref_weak_ptr;
using stdext::ref_weak_counter;
using stdext::ref_count_ptr;

class BasicWeakReference
  : public ref_weak_counter<BasicWeakReference>
{


};

class InheritanceWeakReference
  : public BasicWeakReference
{
  
};

TEST_CASE("Test std::weak_ptr")
{
  std::shared_ptr<int> x;
  std::weak_ptr<int> y;
  CHECK(y.expired());
  std::stringstream test_stream;
  test_stream << x;
}

TEST_CASE("Basic weak reference")
{
  ref_count_ptr<BasicWeakReference> p1(DBG_NEW BasicWeakReference());
  ref_weak_ptr<BasicWeakReference> p2(p1);
}

TEST_CASE("Strong to weak reference")
{
  ref_weak_ptr<BasicWeakReference> p_weak_1;
  ref_weak_ptr<BasicWeakReference> p_weak_2;
  {
    ref_count_ptr<BasicWeakReference> p1(DBG_NEW BasicWeakReference());
    p_weak_1 = p1;
    p_weak_2 = p1.get();
    ref_count_ptr<BasicWeakReference> p2 = p_weak_1.lock();
    CHECK(p1 == p2);
  }
  CHECK(!p_weak_2.lock());
}

TEST_CASE("Inheritance weak reference")
{
  InheritanceWeakReference* derived = DBG_NEW InheritanceWeakReference();
  derived->increment();
  ref_weak_ptr<BasicWeakReference> p_weak_1 = derived;
  CHECK(p_weak_1.lock());
  CHECK(p_weak_1.lock());
  derived->decrement();
  CHECK(!p_weak_1.lock());
}
