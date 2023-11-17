#ifdef _DEBUG
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#include <cassert>
#define DBG_NEW new ( _NORMAL_BLOCK , __FILE__ , __LINE__ )
// Replace _NORMAL_BLOCK with _CLIENT_BLOCK if you want the
// allocations to be of _CLIENT_BLOCK type
#define private public
#endif // _DEBUG

#include "catch.hpp"
#include "IntrusivePtr.h"
#include "Windows.h"


class TestInterface
  : public Cmm::ref_counter_base
{
public:
  virtual int Test() = 0;

protected:
  virtual ~TestInterface() = default;
};


class ReferenceCountedImpl
  : public Cmm::ref_counter<TestInterface>
{
public:
  ReferenceCountedImpl(int v);

  virtual int Test() override;

protected:
  virtual ~ReferenceCountedImpl() = default;

private:
  int value;
};

ReferenceCountedImpl::ReferenceCountedImpl(int v)
  : value(v)
{

}

int ReferenceCountedImpl::Test() {
  return value;
}

TEST_CASE("BasicTest") {
  Cmm::intrusive_ptr<TestInterface> ptr;
  CHECK(!ptr);
  ptr.reset(new ReferenceCountedImpl(5));
  CHECK(ptr);
  CHECK(ptr->Test() == 5);
  CHECK(ptr->use_count() == 1);
  ptr.reset();
  CHECK(!ptr);
  ptr.reset(new ReferenceCountedImpl(6));
  CHECK(ptr);
  CHECK(ptr->Test() == 6);
  {
    Cmm::intrusive_ptr<TestInterface> another = ptr;
    CHECK(ptr->use_count() == 2);
  }
  CHECK(ptr->use_count() == 1);
  std::vector<Cmm::intrusive_ptr<TestInterface>> container;
  container.push_back(ptr);
  container.push_back(ptr);
  container.push_back(ptr);
  container.push_back(ptr);
  CHECK(ptr->use_count() == 5);
}
