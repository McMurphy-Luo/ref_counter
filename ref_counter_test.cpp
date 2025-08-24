#include "ref_counter_test.h"
#include "catch_amalgamated.hpp"
#include "ref_counter.h"
#include <string>
#include <unordered_map>
#include <memory>
#include <iostream>
#include <sstream>

class ReferenceCounted0
  : public ref_counter
{

protected:
  virtual ~ReferenceCounted0() = default;
};

class TestInterface1
  : virtual public ref_counter
{
public:
  virtual int Test1() = 0;

protected:
  virtual ~TestInterface1() = default;
};

class ReferenceCounted1
  : public TestInterface1
{
public:
  ReferenceCounted1(int v) : value(v)
  {

  }

  virtual int Test1() override {
    return value;
  }

protected:
  virtual ~ReferenceCounted1() = default;

private:
  std::unique_ptr<std::string> leak_detector_{ new std::string("hello") };
  int value;
};

class TestInterface2
  : virtual public ref_counter
{
public:
  virtual std::string Test2() = 0;

protected:
  virtual ~TestInterface2() = default;
};

class ReferenceCounted2
  : public TestInterface2
{
public:
  ReferenceCounted2(std::string v) : v_(v) {

  }

  virtual std::string Test2() override { return v_; }

protected:
  virtual ~ReferenceCounted2() = default;

private:
  std::string v_;
};

class ReferenceCounted3
  : virtual public TestInterface2
  , virtual public TestInterface1
{
public:
  ReferenceCounted3(int v1, std::string v2)
    : v1_(v1)
    , v2_(std::make_unique<std::string>(v2))
  {

  }

  virtual int Test1() override { return v1_; }

  virtual std::string Test2() override { return *v2_; }

private:
  int v1_;
  std::unique_ptr<std::string> v2_;
};

TEST_CASE("BasicTest") {
  ref_counter_ptr<ReferenceCounted0> ptr;
  ptr.reset(new ReferenceCounted0());
  ptr = ptr;
  ptr.reset();
  ptr = new ReferenceCounted0;
  ReferenceCounted0* raw = ptr.detach();
  CHECK(raw->use_count() == 1);
  ptr = raw;
  CHECK(ptr->use_count() == 2);
  raw->decrement();
}

TEST_CASE("Test Interface Based Reference Counting") {
  ref_counter_ptr<TestInterface1> ptr;
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
    ref_counter_ptr<TestInterface1> another = ptr;
    CHECK(ptr->use_count() == 2);
  }
  CHECK(ptr->use_count() == 1);
  {
    std::vector<ref_counter_ptr<TestInterface1>> container;
    container.push_back(ptr);
    container.push_back(ptr);
    container.push_back(ptr);
    container.push_back(ptr);
    CHECK(ptr->use_count() == 5);
  }
  ref_counter_ptr<TestInterface1> move_result = std::move(ptr);
  CHECK(move_result != ptr);
  CHECK(nullptr == ptr);
  CHECK(!ptr);
  CHECK(move_result->use_count() == 1);
}

TEST_CASE("Test Multiple Interface") {
  ref_counter_ptr<ReferenceCounted3> true_object(new ReferenceCounted3(98, "Hello"));
  true_object = true_object;
  CHECK(true_object->use_count() == 1);
  ref_counter_ptr<TestInterface1> ptr = true_object;
  CHECK(ptr->use_count() == 2);
  CHECK(ptr->Test1() == 98);
  ref_counter_ptr<TestInterface2> move_result(std::move(true_object));
  CHECK(!true_object);
  CHECK(ptr->use_count() == 2);
  CHECK(move_result->Test2() == "Hello");
  CHECK(move_result->use_count() == 2);
  ref_counter_ptr<TestInterface2> another = true_object;
}

class CustomDeletor
  : public TestInterface1
{
public:
  static CustomDeletor* instance;

  static CustomDeletor* GetInstance() {
    if (instance) {
      instance = nullptr;
      return instance;
    }
    return new CustomDeletor;
  }

public:
  virtual int Test1() override {
    return v_;
  }

  void SetValue(int v) {
    v_ = v;
  }

protected:
  virtual ~CustomDeletor() = default;

  CustomDeletor() = default;

  virtual void on_final_destroy() override {
    instance = this;
  }

private:
  int v_ = 0;
};

CustomDeletor* CustomDeletor::instance = nullptr;

TEST_CASE("Test Custom Deletor") {
  CHECK(nullptr == CustomDeletor::instance);
  ref_counter_ptr<TestInterface1> ptr(CustomDeletor::GetInstance());
  TestInterface1* raw = ptr.get();
  ptr.reset();
  CHECK(CustomDeletor::instance == raw);
  dynamic_cast<CustomDeletor*>(raw)->SetValue(46);
  CHECK(raw->use_count() == 0);
  CHECK(raw->Test1() == 46);
  free(CustomDeletor::instance);
  raw = nullptr;
}

TEST_CASE("Test Unordered Map") {
  std::unordered_map<ref_counter_ptr<TestInterface2>, int> container;
  ref_counter_ptr<TestInterface2> obj(new ReferenceCounted2("1"));
  container.emplace(obj, 1);
  obj = new ReferenceCounted3(2, "2");
  container.emplace(obj, 2);
  obj = new ReferenceCounted2("3");
  container.emplace(obj, 3);
  obj = new ReferenceCounted3(4, "4");
  container.emplace(obj, 4);
  CHECK(obj->use_count() == 2);
  obj.reset();
}

TEST_CASE("Test STL compatibility") {
  ref_counter_ptr<TestInterface1> obj;
  std::ostringstream test_stream;
  test_stream << obj;
  CHECK(atoi(test_stream.str().c_str()) == 0);
  test_stream.str("");
  obj.reset(new ReferenceCounted1(5));
  test_stream << obj;
  std::string str = test_stream.str();
  test_stream.str("");
  test_stream << obj.get();
  CHECK(str == test_stream.str());
  test_stream.str("");
  test_stream << std::hash<ref_counter_ptr<TestInterface1>>()(obj);
  str = test_stream.str();
  test_stream.str("");
  test_stream << std::hash<TestInterface1*>()(obj.get());
  CHECK(test_stream.str() == str);
}
