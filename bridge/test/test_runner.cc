#include <iostream>

extern void run_router_tests();
extern void run_converter_tests();

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "Starting C++ Unit Tests" << std::endl;
    std::cout << "========================================" << std::endl;

    run_router_tests();
    run_converter_tests();

    std::cout << "========================================" << std::endl;
    std::cout << "All C++ Unit Tests Passed Successfully!" << std::endl;
    std::cout << "========================================" << std::endl;

    return 0;
}
