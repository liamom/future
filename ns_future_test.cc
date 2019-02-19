#include <future>
#include "ns_future.h"

#define DOCTEST_CONFIG_IMPLEMENT
#include "doctest.h"

#include <chrono>
using namespace std::chrono_literals;


namespace {
class fixture {
public:
    fixture() {
        ns::g_default_executor = [](auto f) {
            f();
        };
    }

    ~fixture() {
        ns::g_default_executor = ns::default_executor_impl;
    }
};
}

TEST_SUITE("future and promise") {

TEST_CASE_TEMPLATE ("test basics", T, std::promise<int>, ns::promise<int>) {
    fixture fix;
    T p;
    SUBCASE("set value first") {
        p.set_value(5);

        auto f = p.get_future();
        CHECK(f.get() == 5);
    }

    SUBCASE("set value second") {
        auto f = p.get_future();
        p.set_value(5);
        CHECK(f.get() == 5);
    }
}

TEST_CASE_TEMPLATE ("double set exception", T, std::promise<int>, ns::promise<int>) {
    fixture fix;
    T p;
    auto type_str = typeid(p).name();
    CAPTURE(type_str);

    SUBCASE("set value twice") {
        p.set_value(5);
        CHECK_THROWS_AS(p.set_value(4), const std::future_error&);
    }

    SUBCASE("set exception twice") {
        p.set_exception({});
        CHECK_THROWS_AS(p.set_exception({}), const std::future_error&);
    }

    SUBCASE("get future twice, set future first") {
        p.set_value(5);
        auto f = p.get_future();
        CHECK_THROWS_AS(p.get_future(), const std::future_error&);
    }

    SUBCASE("get future twice") {
        auto f = p.get_future();
        CHECK_THROWS_AS(p.get_future(), const std::future_error&);
    }

    SUBCASE("call get twice") {
        auto f = p.get_future();
        p.set_value(10);
        CHECK(f.get() == 10);
        CHECK_THROWS_AS(f.get(), const std::future_error&);
    }

}


TEST_CASE_FIXTURE (fixture, "test then") {
    ns::promise<int> p;
    auto f = p.get_future();
    bool called = false;

    SUBCASE("set the value before calling then") {
        p.set_value(5);
    }

    f.then([&called](ns::future<int> t) {
        CHECK(t.get() == 5);
        called = true;
    });

    SUBCASE("set the value after calling then") {
        p.set_value(5);
    }

    CHECK(called);
}

TEST_CASE("test then async" * doctest::timeout(0.500)) {
    // good thing these tests are run in a single thread
    static struct {
        std::function<void()> call_back;
        std::thread::id id = std::this_thread::get_id();
    } static_test_data;

    ns::g_default_executor = [](auto cb) {
        CHECK(static_test_data.id == std::this_thread::get_id());
        static_test_data.call_back = cb;
    };

    auto end = gsl::finally([](){
        ns::g_default_executor = ns::default_executor_impl;
    });

    ns::promise<int> p;
    auto f = p.get_future();

    std::promise<void> waiter;

    f.then([&waiter](ns::future<int> t) {
        CHECK(t.get() == 5);
        CHECK(static_test_data.id == std::this_thread::get_id());
        waiter.set_value();
    });

    REQUIRE_FALSE(static_test_data.call_back);
    p.set_value(5);
    REQUIRE(static_test_data.call_back);
    static_test_data.call_back();

    auto status = waiter.get_future().wait_for(250ms);
    CHECK(status == std::future_status::ready);
}

}