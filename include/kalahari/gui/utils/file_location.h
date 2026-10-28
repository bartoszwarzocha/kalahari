/// @file file_location.h
/// @brief A file or a folder shown in the file manager of the system, a file opened in the
/// program the system gives it, and a web address opened in the system's browser

#pragma once

#include <QString>
#include <QUrl>

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

/// @brief Open file @p path in the program the system gives it (a picture in the viewer of
/// pictures, a PDF file in the reader of PDF files)
/// @return false when @p path does not exist or the system could not open it; the caller
///         tells the writer
bool openWithSystem(const QString& path);

/// @brief A program in place of the system's programs, for tests: no program starts, and a
/// test sees what the program opens; an empty function gives the system's programs back
/// @param open Does openWithSystem() for a path that exists
void setFileOpenerForTests(std::function<bool(const QString&)> open);

/// @brief Open web address @p url in the system's browser
/// @return false when @p url is no http or https address or the system could not open it;
///         the caller tells the writer
bool openInBrowser(const QUrl& url);

/// @brief A browser in place of the system's, for tests: no browser starts, and a test sees
/// what the program opens; an empty function gives the system's browser back
/// @param open Does openInBrowser() for a web address
void setBrowserForTests(std::function<bool(const QUrl&)> open);

} // namespace gui
} // namespace kalahari
