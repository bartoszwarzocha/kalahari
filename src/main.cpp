/// @file main.cpp
/// @brief Kalahari Writer's IDE - Main entry point
///
/// Phase 0 Week 1 - Basic console application
/// Full GUI implementation will be added in Phase 1

#include <iostream>
#include <kalahari/version.h>

int main([[maybe_unused]] int argc, [[maybe_unused]] char** argv) {
    // Print application header
    std::cout << "========================================" << std::endl;
    std::cout << "Kalahari Writer's IDE" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;

    // Print version information
    std::cout << "Version:      " << kalahari::VERSION << std::endl;
    std::cout << "Build Type:   " << kalahari::BUILD_TYPE << std::endl;
    std::cout << "Platform:     " << kalahari::PLATFORM << std::endl;
    std::cout << "Compiler:     " << kalahari::COMPILER << std::endl;
    std::cout << "Description:  " << kalahari::DESCRIPTION << std::endl;
    std::cout << std::endl;

    // Phase 0 placeholder message
    std::cout << "Status: Pre-Alpha (Phase 0 - Foundation)" << std::endl;
    std::cout << "Target Release: Kalahari 1.0 (Q2-Q3 2026)" << std::endl;
    std::cout << std::endl;

    std::cout << "GUI implementation will be added in Phase 1." << std::endl;
    std::cout << "For now, this is a placeholder console application." << std::endl;
    std::cout << std::endl;

    std::cout << "Press Enter to exit..." << std::endl;
    std::cin.get();

    return 0;
}
