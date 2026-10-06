/// @file test_logger.cpp
/// @brief Unit tests for Logger log file selection

#include <catch2/catch_test_macros.hpp>
#include <kalahari/core/logger.h>

#include <QUuid>

#include <filesystem>
#include <fstream>

using namespace kalahari::core;
namespace fs = std::filesystem;

TEST_CASE("Logger falls back to the temp directory when the log file is not writable", "[logger]") {
    // A regular file used as the log file's parent directory cannot be written into
    const fs::path blocker = fs::temp_directory_path() /
        ("kalahari_log_blocker_" + QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString());
    std::ofstream(blocker) << "not a directory";
    const std::string unwritable = (blocker / "kalahari.log").string();

    std::string opened;
    auto sink = Logger::openFileSink(unwritable, opened);

    CHECK(sink != nullptr);
    CHECK(opened == (fs::temp_directory_path() / "kalahari.log").string());

    sink.reset();
    std::error_code ec;
    fs::remove(blocker, ec);
}

TEST_CASE("Logger uses the requested log file when it is writable", "[logger]") {
    const fs::path path = fs::temp_directory_path() /
        ("kalahari_log_" + QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString() + ".log");

    std::string opened;
    auto sink = Logger::openFileSink(path.string(), opened);

    CHECK(sink != nullptr);
    CHECK(opened == path.string());

    sink.reset();
    std::error_code ec;
    fs::remove(path, ec);
}
