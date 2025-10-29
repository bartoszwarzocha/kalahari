/// @file test_cmd_line_parser.cpp
/// @brief Unit tests for CmdLineParser

#include <catch2/catch_test_macros.hpp>
#include <kalahari/core/cmd_line_parser.h>

using namespace kalahari::core;

TEST_CASE("CmdLineParser basic functionality", "[cmdline]") {
    SECTION("Parse with no arguments") {
        char* argv[] = { const_cast<char*>("kalahari") };
        int argc = 1;

        CmdLineParser parser(argc, argv);
        parser.addSwitch("d", "diag", "Enable diagnostic mode");

        REQUIRE(parser.parse() == true);
        REQUIRE(parser.hasSwitch("diag") == false);
        REQUIRE(parser.hasSwitch("d") == false);
    }

    SECTION("Parse with short switch") {
        char* argv[] = { const_cast<char*>("kalahari"), const_cast<char*>("-d") };
        int argc = 2;

        CmdLineParser parser(argc, argv);
        parser.addSwitch("d", "diag", "Enable diagnostic mode");

        REQUIRE(parser.parse() == true);
        REQUIRE(parser.hasSwitch("d") == true);
        REQUIRE(parser.hasSwitch("diag") == true);  // Both names should work
    }

    SECTION("Parse with long switch") {
        char* argv[] = { const_cast<char*>("kalahari"), const_cast<char*>("--diag") };
        int argc = 2;

        CmdLineParser parser(argc, argv);
        parser.addSwitch("d", "diag", "Enable diagnostic mode");

        REQUIRE(parser.parse() == true);
        REQUIRE(parser.hasSwitch("diag") == true);
        REQUIRE(parser.hasSwitch("d") == true);  // Both names should work
    }

    SECTION("Parse with multiple switches") {
        char* argv[] = {
            const_cast<char*>("kalahari"),
            const_cast<char*>("-d"),
            const_cast<char*>("--verbose")
        };
        int argc = 3;

        CmdLineParser parser(argc, argv);
        parser.addSwitch("d", "diag", "Enable diagnostic mode");
        parser.addSwitch("v", "verbose", "Enable verbose logging");

        REQUIRE(parser.parse() == true);
        REQUIRE(parser.hasSwitch("d") == true);
        REQUIRE(parser.hasSwitch("verbose") == true);
    }

    SECTION("Check unknown switch") {
        char* argv[] = { const_cast<char*>("kalahari") };
        int argc = 1;

        CmdLineParser parser(argc, argv);
        parser.addSwitch("d", "diag", "Enable diagnostic mode");

        REQUIRE(parser.parse() == true);

        // hasSwitch() should return false for unknown switches
        REQUIRE(parser.hasSwitch("unknown") == false);
    }

    SECTION("hasSwitch before parse") {
        char* argv[] = { const_cast<char*>("kalahari"), const_cast<char*>("-d") };
        int argc = 2;

        CmdLineParser parser(argc, argv);
        parser.addSwitch("d", "diag", "Enable diagnostic mode");

        // hasSwitch() should return false if parse() not called yet
        REQUIRE(parser.hasSwitch("d") == false);

        // After parse, should work correctly
        REQUIRE(parser.parse() == true);
        REQUIRE(parser.hasSwitch("d") == true);
    }
}

TEST_CASE("CmdLineParser edge cases", "[cmdline]") {
    SECTION("Add switch after parse (still works)") {
        char* argv[] = { const_cast<char*>("kalahari"), const_cast<char*>("-d") };
        int argc = 2;

        CmdLineParser parser(argc, argv);
        parser.addSwitch("d", "diag", "Enable diagnostic mode");

        REQUIRE(parser.parse() == true);

        // Add another switch after parsing (shouldn't affect already parsed args)
        parser.addSwitch("v", "verbose", "Verbose mode");

        // Original switch should still work
        REQUIRE(parser.hasSwitch("d") == true);
        // New switch won't be found (wasn't on command line)
        REQUIRE(parser.hasSwitch("v") == false);
    }

    SECTION("Switch not added to parser") {
        char* argv[] = { const_cast<char*>("kalahari"), const_cast<char*>("-x") };
        int argc = 2;

        CmdLineParser parser(argc, argv);
        parser.addSwitch("d", "diag", "Enable diagnostic mode");

        // Parse will fail because -x is not a recognized switch
        REQUIRE(parser.parse() == false);
    }
}
