/// @file reading_time.cpp
/// @brief The reading time of a text as the program shows it

#include "kalahari/gui/utils/reading_time.h"
#include "kalahari/core/text_statistics.h"

#include <QCoreApplication>
#include <QLocale>

namespace kalahari {
namespace gui {
namespace utils {

QString readingTimeText(int words) {
    constexpr int MINUTES_PER_HOUR = 60;
    const int minutes = core::readingMinutes(words);
    const QLocale locale;
    if (minutes < MINUTES_PER_HOUR) {
        return QCoreApplication::translate("kalahari::gui::utils::ReadingTime", "%1 min")
            .arg(locale.toString(minutes));
    }
    const int hours = minutes / MINUTES_PER_HOUR;
    const int rest = minutes % MINUTES_PER_HOUR;
    if (rest == 0) {
        return QCoreApplication::translate("kalahari::gui::utils::ReadingTime", "%1 h")
            .arg(locale.toString(hours));
    }
    return QCoreApplication::translate("kalahari::gui::utils::ReadingTime", "%1 h %2 min")
        .arg(locale.toString(hours), locale.toString(rest));
}

} // namespace utils
} // namespace gui
} // namespace kalahari
