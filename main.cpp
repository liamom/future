#include <iostream>
#include <future>
#include "ns_future.h"
#include <chrono>
#include <doctest.h>
using namespace std::chrono_literals;

using namespace ns;

int run_doctest(int argc, char *argv[]) {
    doctest::Context context;

    // !!! THIS IS JUST AN EXAMPLE SHOWING HOW DEFAULTS/OVERRIDES ARE SET !!!

    // defaults
    context.addFilter("test-case-exclude", "*math*"); // exclude test cases with "math" in their name
    context.setOption("abort-after", 5);              // stop test execution after 5 failed assertions
    context.setOption("order-by", "name");            // sort the test cases by their name

    context.applyCommandLine(argc, argv);

    // overrides
    context.setOption("no-breaks", true);             // don't break in the debugger when assertions fail

    int res = context.run(); // run

    if(context.shouldExit()) // important - query flags (and --exit) rely on the user doing this
        exit(res);          // propagate the result of the tests

        return res;
}

int main(int argc, char *argv[]) {
    int res = run_doctest(argc, argv);
//    future<int> f;
//    assert(!f.valid());
//
//    promise<int> p;
//    f = p.get_future();
//    f.then<int>([](future<int> f) {
//        std::cout << "1: " << f.get() << std::endl;
//
//        return 5;
//    });
//
//    p.set_value(10);
//
//    promise<void> p2;
//    p2.get_future().then<void>([](future<void> future1){
//       std::cout << "thing";
//    });

//    promise<int> p;
//    future<int> f = p.get_future();
//    p.set_value(5);
//    f.then([](future<int> f) {
//        std::cout << "blarg: \'" << f.get() << "\'" << std::endl;
//    });

    std::promise<int> p2;
    p2.set_value(5);
    std::cout << "test: " << p2.get_future().get() << std::endl;

    ns::promise<int> p3;
    auto f3 = p3.get_future();
    f3.wait_for(1ms);

//    std::this_thread::sleep_for(1s);
    std::cout << "Hello, World!" << std::endl;
    return res;
}