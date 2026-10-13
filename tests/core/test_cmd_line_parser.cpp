/// @file test_cmd_line_parser.cpp
/// @brief Unit tests for CmdLineParser

#include <catch2/catch_test_macros.hpp>
#include <kalahari/core/cmd_line_parser.h>
#include <kalahari/version.h>

#include <QCoreApplication>
#include <QString>
#include <QStringList>

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

    SECTION("--help leaves the help to the caller instead of exiting") {
        char* argv[] = { const_cast<char*>("kalahari"), const_cast<char*>("--help") };
        CmdLineParser parser(2, argv);
        parser.addSwitch("d", "diag", "Enable diagnostic mode");

        REQUIRE(parser.parse() == false);
        REQUIRE(parser.helpRequested());
        REQUIRE(parser.helpText().contains(QStringLiteral("--diag")));
        REQUIRE(parser.helpText().contains(QStringLiteral("Enable diagnostic mode")));
    }

    SECTION("An unknown switch is an error the caller can show") {
        char* argv[] = { const_cast<char*>("kalahari"), const_cast<char*>("--unknown") };
        CmdLineParser parser(2, argv);
        parser.addSwitch("d", "diag", "Enable diagnostic mode");

        REQUIRE(parser.parse() == false);
        REQUIRE_FALSE(parser.helpRequested());
        REQUIRE(parser.errorText().contains(QStringLiteral("unknown")));
    }
}

TEST_CASE("CmdLineParser: the book or the file to open, without a name", "[cmdline]") {
    SECTION("A book after a switch") {
        char* argv[] = { const_cast<char*>("kalahari"), const_cast<char*>("-d"),
                         const_cast<char*>("Burza.klh") };
        CmdLineParser parser(3, argv);
        parser.addSwitch("d", "diag", "Enable diagnostic mode");
        parser.addPositionalArgument("file", "A book or another file to open", "[file]");

        REQUIRE(parser.positionalArguments().isEmpty());  // before parse()
        REQUIRE(parser.parse() == true);
        REQUIRE(parser.hasSwitch("diag"));
        REQUIRE(parser.positionalArguments() == QStringList{QStringLiteral("Burza.klh")});
        REQUIRE(parser.helpText().contains(QStringLiteral("[file]")));
    }

    SECTION("Nothing to open") {
        char* argv[] = { const_cast<char*>("kalahari") };
        CmdLineParser parser(1, argv);
        parser.addPositionalArgument("file", "A book or another file to open", "[file]");

        REQUIRE(parser.parse() == true);
        REQUIRE(parser.positionalArguments().isEmpty());
    }
}

TEST_CASE("The program version comes from the project version", "[cmdline][version]") {
    // One source: project(VERSION) in CMakeLists.txt, plus the release stage
    const QString shown = QString::fromLatin1(kalahari::VERSION_STRING);
    REQUIRE(shown.startsWith(QString::fromLatin1(kalahari::VERSION)));

    char* argv[] = { const_cast<char*>("kalahari") };
    CmdLineParser parser(1, argv);
    // The test program keeps its own name: under the program's name, files that later tests
    // write through QStandardPaths would land in the user's Kalahari folder
    const QString testProgramName = QCoreApplication::applicationName();
    parser.setApplicationDescription(testProgramName, "Writer's IDE");
    REQUIRE(QCoreApplication::applicationVersion() == shown);
    REQUIRE(QCoreApplication::applicationName() == testProgramName);
}
