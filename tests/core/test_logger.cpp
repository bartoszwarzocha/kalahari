/// @file test_logger.cpp
/// @brief Unit tests for Logger log file selection

#include <catch2/catch_test_macros.hpp>
#include <kalahari/core/logger.h>

#include <QUuid>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

using namespace kalahari::core;
namespace fs = std::filesystem;

namespace {

/// The text of the file @p path
std::string readText(const fs::path& path) {
    std::ifstream file(path);
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

} // anonymous namespace

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

TEST_CASE("Logger keeps the messages until the start opens the log file", "[logger]") {
    const fs::path path = fs::temp_directory_path() /
        ("kalahari_log_" + QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString() + ".log");
    const std::string running = "The log of the program that runs\n";
    std::ofstream(path) << running;

    // A logger of its own, whatever the tests before did with the one of the process
    auto& logger = Logger::getInstance();
    logger.shutdown();
    logger.initWithoutFile();
    logger.info("Before the file");

    // A start that hands its request to the program that runs ends here: the log of that
    // program stays as it is
    CHECK(readText(path) == running);

    // A start that runs on its own starts the file with what it logged so far
    logger.openFile(path.string());
    logger.info("After the file");
    logger.flush();
    CHECK(logger.getLogFilePath() == path.string());
    const std::string text = readText(path);
    CHECK(text.find(running) == std::string::npos);
    const auto before = text.find("Before the file");
    const auto after = text.find("After the file");
    CHECK(before != std::string::npos);
    CHECK(after != std::string::npos);
    CHECK(before < after);

    logger.shutdown();
    std::error_code ec;
    fs::remove(path, ec);
}
