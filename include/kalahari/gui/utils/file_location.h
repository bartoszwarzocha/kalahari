/// @file file_location.h
/// @brief A file or a folder shown in the file manager of the system

#pragma once

#include <QString>

#include <functional>

namespace kalahari {
namespace gui {

/// @brief Open the folder of file @p path in the file manager of the system, with the file
/// selected where the system can do it (Windows Explorer, the Finder); a folder opens itself
/// @return false when @p path does not exist or the file manager could not be started; the
///         caller tells the writer
bool showInFolder(const QString& path);

/// @brief A file manager in place of the system's, for tests: no window opens, and a test sees
/// what the program shows; an empty function gives the system's file manager back
/// @param show Does showInFolder() for a path that exists
void setFileManagerForTests(std::function<bool(const QString&)> show);

} // namespace gui
} // namespace kalahari
