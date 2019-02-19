#include <iostream>
#include <future>
#include "lom/future.h"
#include <chrono>
#include <doctest.h>
using namespace std::chrono_literals;

using namespace lom;

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

//    {
//        future<int> f;
//        assert(!f.valid());
//
//        promise<int> p;
//        f = p.get_future();
//        auto f2 = f.then([](future<int> f) {
//            std::cout << "1: " << f.get() << std::endl;
//
//            return 5;
//        });
//
//
//        std::thread th([&]() {
//            std::this_thread::sleep_for(2s);
//            std::cout << "calling set value" << std::endl;
//            p.set_value(10);
//        });
//
//        std::cout << "f2.get() = " << f2.get() << std::endl;
//
//        th.join();
//    }

    {
        future<int> f;
        assert(!f.valid());

        promise<int> p;
        promise<double> p2;
        f = p.get_future();
        future<double> f2 = f.then([&](future<int> f) -> future<double> {
            std::cout << "1: " << f.get() << std::endl;

            return p2.get_future();
        });

        auto f3 = f2.then([](future<double> f){
            std::cout << "12: " << f.get() << std::endl;
            return 1234;
        });


        std::thread th([&](){
            std::this_thread::sleep_for(2s);
            std::cout << "calling set value" << std::endl;
            p.set_value(10);

            std::this_thread::sleep_for(2s);
            std::cout << "calling set value2" << std::endl;
            p2.set_value(5.5);
        });

        std::cout << "f3.get() = " << f3.get() << std::endl;

        th.join();
    }

    return res;
}