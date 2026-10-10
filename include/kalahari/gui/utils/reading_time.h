/// @file reading_time.h
/// @brief The reading time of a text as the program shows it

#ifndef KALAHARI_GUI_UTILS_READING_TIME_H
#define KALAHARI_GUI_UTILS_READING_TIME_H

#include <QString>

namespace kalahari {
namespace gui {
namespace utils {

/// @brief The reading time of @p words words (core::readingMinutes()) as the writer reads it
///
/// "18 min", from an hour on "12 h 31 min" (or "2 h" for whole hours), with the numbers in
/// the system's way. The status bar and the Properties panel show it.
/// @param words The words of the text
[[nodiscard]] QString readingTimeText(int words);

} // namespace utils
} // namespace gui
} // namespace kalahari

#endif // KALAHARI_GUI_UTILS_READING_TIME_H
