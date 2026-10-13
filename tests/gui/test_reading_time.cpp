/// @file test_reading_time.cpp
/// @brief The reading time of a text as the program shows it

#include <catch2/catch_test_macros.hpp>
#include "kalahari/core/text_statistics.h"
#include "kalahari/gui/utils/reading_time.h"

#include <QLocale>

using kalahari::core::READING_WORDS_PER_MINUTE;
using kalahari::gui::utils::readingTimeText;

TEST_CASE("Reading time: minutes, from an hour on hours and minutes", "[gui][statistics]") {
    constexpr int perMinute = READING_WORDS_PER_MINUTE;
    CHECK(readingTimeText(0) == QStringLiteral("0 min"));
    CHECK(readingTimeText(1) == QStringLiteral("1 min"));
    CHECK(readingTimeText(perMinute * 59) == QStringLiteral("59 min"));
    // A minute begun is a minute, so the 60th minute makes an hour
    CHECK(readingTimeText(perMinute * 59 + 1) == QStringLiteral("1 h"));
    CHECK(readingTimeText(perMinute * 60) == QStringLiteral("1 h"));
    CHECK(readingTimeText(perMinute * 60 + 1) == QStringLiteral("1 h 1 min"));
    CHECK(readingTimeText(perMinute * 120) == QStringLiteral("2 h"));
    // The long chapter of the example book
    CHECK(readingTimeText(150017) == QStringLiteral("12 h 31 min"));
}

TEST_CASE("Reading time: large numbers in the system's way", "[gui][statistics]") {
    // "1,500 h" in English, "1 500 h" in Polish
    CHECK(readingTimeText(READING_WORDS_PER_MINUTE * 60 * 1500) ==
          QStringLiteral("%1 h").arg(QLocale().toString(1500)));
    CHECK(readingTimeText(READING_WORDS_PER_MINUTE * (60 * 1500 + 1)) ==
          QStringLiteral("%1 h 1 min").arg(QLocale().toString(1500)));
}
