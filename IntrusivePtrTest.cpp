#ifdef _DEBUG
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#include <cassert>
#define DBG_NEW new ( _NORMAL_BLOCK , __FILE__ , __LINE__ )
// Replace _NORMAL_BLOCK with _CLIENT_BLOCK if you want the
// allocations to be of _CLIENT_BLOCK type
#endif // _DEBUG

#include "catch.hpp"
#include "IntrusivePtr.h"
#include "Windows.h"
#include <string>
#include "boost/smart_ptr/intrusive_ptr.hpp"
#include "boost/smart_ptr/intrusive_ref_counter.hpp"

class TestInterface1
  : public Cmm::ref_counter_base
{
public:
  virtual int Test1() = 0;

protected:
  virtual ~TestInterface1() = default;
};

class ReferenceCounted0
  : public Cmm::ref_counter<>
{

};

class ReferenceCounted1
  : public Cmm::ref_counter<Cmm::thread_unsafe_counter>
  , public TestInterface1
{
public:
  ReferenceCounted1(int v) : value(v)
  {

  }

  virtual int Test1() override {
    return value;
  }

  FORWARD_DEFINE_REF_COUNTER(Cmm::ref_counter<Cmm::thread_unsafe_counter>)

protected:
  virtual ~ReferenceCounted1() = default;

private:
  std::unique_ptr<std::string> leak_detector_{ new std::string("hello") };
  int value;
};

TEST_CASE("BasicTest") {
  {
    Cmm::ref_counter_ptr<ReferenceCounted0> ptr;
    ptr.reset(new ReferenceCounted0());
    ptr.reset();
  }
  Cmm::ref_counter_ptr<TestInterface1> ptr;
  CHECK(!ptr);
  ptr.reset(new ReferenceCounted1(5));
  CHECK(ptr);
  CHECK(ptr->Test1() == 5);
  CHECK(ptr->use_count() == 1);
  ptr.reset();
  CHECK(!ptr);
  ptr.reset(new ReferenceCounted1(6));
  CHECK(ptr);
  CHECK(ptr->Test1() == 6);
  {
    Cmm::ref_counter_ptr<TestInterface1> another = ptr;
    CHECK(ptr->use_count() == 2);
  }
  CHECK(ptr->use_count() == 1);
  {
    std::vector<Cmm::ref_counter_ptr<TestInterface1>> container;
    container.push_back(ptr);
    container.push_back(ptr);
    container.push_back(ptr);
    container.push_back(ptr);
    CHECK(ptr->use_count() == 5);
  }
  Cmm::ref_counter_ptr<TestInterface1> move_result = std::move(ptr);
  CHECK(!ptr);
  CHECK(move_result->use_count() == 1);
}

class TestInterface2
  : public Cmm::ref_counter_base
{
public:
  virtual std::string Test2() = 0;

protected:
  virtual ~TestInterface2() = default;
};

class ReferenceCounted2
  : public TestInterface2
  , public TestInterface1
  , public Cmm::ref_counter<>
{
public:
  ReferenceCounted2(int v1, std::string v2)
    : v1_(v1)
    , v2_(v2)
  {

  }

  virtual int Test1() override { return v1_; }

  virtual std::string Test2() override { return v2_; }

  FORWARD_DEFINE_REF_COUNTER(Cmm::ref_counter<>);

private:
  int v1_;
  std::string v2_;
};
