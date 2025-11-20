/// @file cmd_line_parser.h
/// @brief Command line argument parser for Kalahari
///
/// Provides a simplified wrapper over QCommandLineParser for parsing
/// command line arguments. Inspired by bwx_sdk's bwxCmdLineParser architecture.
///
/// Key features:
/// - Easy-to-use API for adding switches (flags)
/// - Automatic help generation
/// - Cross-platform (Windows, Linux, macOS)
/// - Extensible for future options (string, number, etc.)

#pragma once

#include <QCommandLineParser>
#include <QStringList>
#include <QString>
#include <string>
#include <vector>

namespace kalahari {
namespace core {

/// @brief Command line parser for Kalahari application
///
/// Wrapper over QCommandLineParser providing simplified API for parsing
/// command line arguments. Currently supports switches (boolean flags).
///
/// Usage:
/// @code
/// CmdLineParser parser(argc, argv);
/// parser.addSwitch("d", "diag", "Enable diagnostic mode");
/// parser.addSwitch("v", "version", "Show version information");
///
/// if (!parser.parse()) {
///     return false;  // Help shown or error occurred
/// }
///
/// if (parser.hasSwitch("diag")) {
///     // Enable diagnostic mode
/// }
/// @endcode
class CmdLineParser {
public:
    /// @brief Constructor for ANSI command line arguments
    /// @param argc Number of arguments
    /// @param argv Array of argument strings (char**)
    CmdLineParser(int argc, char** argv);

#ifdef _WIN32
    /// @brief Constructor for wide-character command line arguments (Windows)
    /// @param argc Number of arguments
    /// @param argv Array of argument strings (wchar_t**)
    CmdLineParser(int argc, wchar_t** argv);
#endif

    /// @brief Set application name and description for help text
    /// @param appName Application name
    /// @param appDescription Application description
    void setApplicationDescription(const QString& appName, const QString& appDescription);

    /// @brief Destructor
    ~CmdLineParser() = default;

    // Prevent copying and moving
    CmdLineParser(const CmdLineParser&) = delete;
    CmdLineParser& operator=(const CmdLineParser&) = delete;
    CmdLineParser(CmdLineParser&&) = delete;
    CmdLineParser& operator=(CmdLineParser&&) = delete;

    /// @brief Add a command line switch (boolean flag)
    ///
    /// Adds a switch that can be specified on command line with either
    /// short name (-d) or long name (--diag).
    ///
    /// @param shortName Short name (single character, e.g., "d")
    /// @param longName Long name (full word, e.g., "diag")
    /// @param description Description shown in help text
    void addSwitch(const QString& shortName,
                   const QString& longName,
                   const QString& description);

    /// @brief Parse command line arguments
    ///
    /// Parses the command line arguments according to the options
    /// added with addSwitch(). If --help is specified, prints help
    /// and returns false.
    ///
    /// @return true if parsing succeeded, false if error or help requested
    bool parse();

    /// @brief Check if a switch was specified
    ///
    /// Checks if the given switch (flag) was present on the command line.
    /// Works with either short or long name.
    ///
    /// @param name Switch name (short or long)
    /// @return true if switch was present, false otherwise
    bool hasSwitch(const QString& name) const;

private:
    /// @brief Initialize parser with default settings
    void init();

    /// @brief Convert argc/argv to QStringList
    /// @param argc Number of arguments
    /// @param argv Array of argument strings
    /// @return QStringList with arguments
    static QStringList argsToStringList(int argc, char** argv);

#ifdef _WIN32
    /// @brief Convert argc/argv to QStringList (wide char version)
    static QStringList argsToStringList(int argc, wchar_t** argv);
#endif

    /// @brief Underlying QCommandLineParser
    QCommandLineParser m_parser;

    /// @brief Arguments as QStringList
    QStringList m_args;

    /// @brief Number of command line arguments
    int m_argc;

    /// @brief Tracks if parse() has been called
    bool m_parsed = false;

    /// @brief List of added switch names (for validation)
    std::vector<QString> m_switches;
};

} // namespace core
} // namespace kalahari
