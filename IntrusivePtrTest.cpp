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
#include <string>
#include <unordered_map>
#include <memory>
#include <iostream>

using Cmm::RefCounter;
using Cmm::RefCounterBase;
using Cmm::RefCounterPtr;
using Cmm::ThreadUnsafeCounter;
using Cmm::ThreadSafeCounter;

class ReferenceCounted0
  : public RefCounter<>
{

};

class TestInterface1
  : public RefCounterBase
{
public:
  virtual int Test1() = 0;

protected:
  virtual ~TestInterface1() = default;
};

class ReferenceCounted1
  : public RefCounter<ThreadUnsafeCounter>
  , public TestInterface1
{
public:
  ReferenceCounted1(int v) : value(v)
  {

  }

  virtual int Test1() override {
    return value;
  }

  FORWARD_DEFINE_REF_COUNTER(RefCounter<ThreadUnsafeCounter>);

protected:
  virtual ~ReferenceCounted1() = default;

private:
  std::unique_ptr<std::string> leak_detector_{ new std::string("hello") };
  int value;
};

class TestInterface2
  : public RefCounterBase
{
public:
  virtual std::string Test2() = 0;

protected:
  virtual ~TestInterface2() = default;
};

class ReferenceCounted2
  : public TestInterface2
  , public RefCounter<ThreadUnsafeCounter>
{
public:
  ReferenceCounted2(std::string v) : v_(v) {

  }

  virtual std::string Test2() override { return v_; }

  FORWARD_DEFINE_REF_COUNTER(RefCounter<ThreadUnsafeCounter>);

protected:
  virtual ~ReferenceCounted2() = default;

private:
  std::string v_;
};

class ReferenceCounted3
  : public TestInterface2
  , public TestInterface1
  , public RefCounter<>
{
public:
  ReferenceCounted3(int v1, std::string v2)
    : v1_(v1)
    , v2_(std::make_unique<std::string>(v2))
  {

  }

  virtual int Test1() override { return v1_; }

  virtual std::string Test2() override { return *v2_; }

  FORWARD_DEFINE_REF_COUNTER(RefCounter<>);

private:
  int v1_;
  std::unique_ptr<std::string> v2_;
};

TEST_CASE("BasicTest") {
  RefCounterPtr<ReferenceCounted0> ptr;
  ptr.Reset(new ReferenceCounted0());
  ptr.Reset();
  ReferenceCounted0 second_one;
  ReferenceCounted0 third_one = second_one;
  CHECK(second_one.UseCount() == 0);
  CHECK(third_one.UseCount() == 0);
}

TEST_CASE("Test Interface Based Reference Counting") {
  RefCounterPtr<TestInterface1> ptr;
  CHECK(!ptr);
  ptr.Reset(new ReferenceCounted1(5));
  CHECK(ptr);
  CHECK(ptr->Test1() == 5);
  CHECK(ptr->UseCount() == 1);
  ptr.Reset();
  CHECK(!ptr);
  ptr.Reset(new ReferenceCounted1(6));
  CHECK(ptr);
  CHECK(ptr->Test1() == 6);
  {
    RefCounterPtr<TestInterface1> another = ptr;
    CHECK(ptr->UseCount() == 2);
  }
  CHECK(ptr->UseCount() == 1);
  {
    std::vector<RefCounterPtr<TestInterface1>> container;
    container.push_back(ptr);
    container.push_back(ptr);
    container.push_back(ptr);
    container.push_back(ptr);
    CHECK(ptr->UseCount() == 5);
  }
  RefCounterPtr<TestInterface1> move_result = std::move(ptr);
  CHECK(move_result != ptr);
  CHECK(nullptr == ptr);
  CHECK(!ptr);
  CHECK(move_result->UseCount() == 1);
}

TEST_CASE("Test Multiple Interface") {
  RefCounterPtr<ReferenceCounted3> true_object(new ReferenceCounted3(98, "Hello"));
  true_object = true_object;
  CHECK(true_object->UseCount() == 1);
  RefCounterPtr<TestInterface1> ptr = true_object;
  CHECK(ptr->UseCount() == 2);
  CHECK(ptr->Test1() == 98);
  RefCounterPtr<TestInterface2> move_result(std::move(true_object));
  CHECK(!true_object);
  CHECK(ptr->UseCount() == 2);
  CHECK(move_result->Test2() == "Hello");
  CHECK(move_result->UseCount() == 2);
  RefCounterPtr<TestInterface2> another = true_object;
}

class CustomDeletor
  : public TestInterface1
  , public RefCounter<ThreadUnsafeCounter>
{
public:
  static CustomDeletor* instance;

public:
  CustomDeletor(int v)
    : v_(v)
  {
  }

  FORWARD_DEFINE_REF_COUNTER(RefCounter<ThreadUnsafeCounter>);

  virtual int Test1() override {
    return v_;
  }

protected:
  virtual ~CustomDeletor() = default;

  virtual void Release() override {
    instance = this;
  }

private:
  int v_;
};

CustomDeletor* CustomDeletor::instance = nullptr;

TEST_CASE("Test Custom Deletor") {
  CHECK(nullptr == CustomDeletor::instance);
  RefCounterPtr<TestInterface1> ptr(new CustomDeletor(46));
  TestInterface1* raw = ptr.Get();
  ptr.Reset();
  CHECK(CustomDeletor::instance == raw);
  CHECK(raw->UseCount() == 0);
  CHECK(raw->Test1() == 46);
  free(CustomDeletor::instance);
  raw = nullptr;
}

TEST_CASE("Test Unordered Map") {
  std::unordered_map<RefCounterPtr<TestInterface2>, int> container;
  RefCounterPtr<TestInterface2> obj(new ReferenceCounted2("1"));
  container.emplace(obj, 1);
  obj = new ReferenceCounted3(2, "2");
  container.emplace(obj, 2);
  obj = new ReferenceCounted2("3");
  container.emplace(obj, 3);
  obj = new ReferenceCounted3(4, "4");
  container.emplace(obj, 4);
  CHECK(obj->UseCount() == 2);
  obj.Reset();
}

TEST_CASE("Test STL compatibility") {
  RefCounterPtr<TestInterface1> obj;
  std::cout << obj << std::endl;
  obj.Reset(new ReferenceCounted1(5));
  std::cout << obj << std::endl;
  std::cout << std::hash<RefCounterPtr<TestInterface1>>()(obj) << std::endl;
}
