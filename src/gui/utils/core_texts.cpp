/// @file core_texts.cpp
/// @brief The core library's texts that Kalahari translates
///
/// The core library (kalahari_core) translates the messages it shows to the writer
/// through QCoreApplication::translate() with its classes' names as the contexts.
/// lupdate reads only the application's sources, so these entries put the texts into
/// kalahari_*.ts (lupdate reads the QT_TRANSLATE_NOOP markers; the array is never used).
/// Keep them in step with the core sources named in each group.

#include <QtGlobal>

namespace {

[[maybe_unused]] const char* const CORE_TEXTS[] = {
    // core/cmd_line_parser.cpp: the parser's own --help switch
    QT_TRANSLATE_NOOP("kalahari::core::CmdLineParser", "Displays help on commandline options."),

    // core/utils/icon_downloader.cpp: why an icon could not be downloaded
    QT_TRANSLATE_NOOP("kalahari::core::IconDownloader", "Invalid URL: %1"),
    QT_TRANSLATE_NOOP("kalahari::core::IconDownloader", "Not found (404): %1"),
    QT_TRANSLATE_NOOP("kalahari::core::IconDownloader", "Access denied (403): %1"),
    QT_TRANSLATE_NOOP("kalahari::core::IconDownloader", "The server did not answer within %1 seconds"),
    QT_TRANSLATE_NOOP("kalahari::core::IconDownloader", "Cannot connect to the server"),
    QT_TRANSLATE_NOOP("kalahari::core::IconDownloader", "The proxy server did not let the connection through"),
    QT_TRANSLATE_NOOP("kalahari::core::IconDownloader", "A secure connection to the server failed"),
    QT_TRANSLATE_NOOP("kalahari::core::IconDownloader", "Download failed: %1"),
    QT_TRANSLATE_NOOP("kalahari::core::IconDownloader", "The downloaded file is empty"),

    // core/utils/svg_converter.cpp: why a downloaded icon is not a usable SVG
    QT_TRANSLATE_NOOP("kalahari::core::SvgConverter", "The SVG file is empty"),
    QT_TRANSLATE_NOOP("kalahari::core::SvgConverter", "Invalid XML in line %1, column %2: %3"),
    QT_TRANSLATE_NOOP("kalahari::core::SvgConverter", "The file is not an SVG image (its root element is <%1>)"),
    QT_TRANSLATE_NOOP("kalahari::core::SvgConverter", "The <svg> element has no viewBox attribute"),
    QT_TRANSLATE_NOOP("kalahari::core::SvgConverter", "Conversion produced invalid SVG: %1"),
};

}  // namespace
