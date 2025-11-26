/// @file cmd_line_parser.cpp
/// @brief Implementation of CmdLineParser

#include <kalahari/core/cmd_line_parser.h>
#include <kalahari/core/logger.h>
#include <QCommandLineOption>
#include <QCoreApplication>
#include <algorithm>

namespace kalahari {
namespace core {

// =============================================================================
// Constructors
// =============================================================================

CmdLineParser::CmdLineParser(int argc, char** argv)
    : m_args(argsToStringList(argc, argv)), m_argc(argc) {
    init();
}

#ifdef _WIN32
CmdLineParser::CmdLineParser(int argc, wchar_t** argv)
    : m_args(argsToStringList(argc, argv)), m_argc(argc) {
    init();
}
#endif

// =============================================================================
// Public API
// =============================================================================

void CmdLineParser::setApplicationDescription(const QString& appName, const QString& appDescription) {
    QCoreApplication::setApplicationName(appName);
    QCoreApplication::setApplicationVersion("0.3.0-alpha");
    m_parser.setApplicationDescription(appDescription);
    Logger::getInstance().debug("Set application description: {}", appName.toStdString());
}

void CmdLineParser::addSwitch(const QString& shortName,
                               const QString& longName,
                               const QString& description) {
    // Create QCommandLineOption for boolean switch
    // Handle empty shortName (long-name-only switches)
    QStringList names;
    if (!shortName.isEmpty()) {
        names << shortName;
    }
    names << longName;

    QCommandLineOption option(names, description);
    m_parser.addOption(option);

    // Track switch names for validation (only non-empty ones)
    if (!shortName.isEmpty()) {
        m_switches.push_back(shortName);
    }
    m_switches.push_back(longName);

    Logger::getInstance().debug("Added command line switch: {} / --{}",
                                shortName.isEmpty() ? "" : ("-" + shortName).toStdString(),
                                longName.toStdString());
}

void CmdLineParser::addOption(const QString& shortName,
                               const QString& longName,
                               const QString& description,
                               const QString& valueName) {
    // Create QCommandLineOption with value
    // Handle empty shortName (long-name-only options)
    QStringList names;
    if (!shortName.isEmpty()) {
        names << shortName;
    }
    names << longName;

    QCommandLineOption option(names, description, valueName);
    m_parser.addOption(option);

    // Track option names for validation (only non-empty ones)
    if (!shortName.isEmpty()) {
        m_options.push_back(shortName);
    }
    m_options.push_back(longName);

    Logger::getInstance().debug("Added command line option: {} / --{} <{}>",
                                shortName.isEmpty() ? "" : ("-" + shortName).toStdString(),
                                longName.toStdString(),
                                valueName.toStdString());
}

bool CmdLineParser::parse() {
    // Check if user only provided executable name (no arguments)
    if (m_argc == 1) {
        // No arguments - this is normal, just proceed without any switches
        Logger::getInstance().debug("No command line arguments provided");
        m_parsed = true;
        return true;
    }

    // Parse command line
    if (!m_parser.parse(m_args)) {
        // Error occurred during parsing
        Logger::getInstance().error("Command line parsing error: {}",
                                    m_parser.errorText().toStdString());
        return false;
    }

    // Check if help was requested
    if (m_parser.isSet("help")) {
        // showHelp() displays help and exits the application
        // On Windows GUI apps: shows QMessageBox when stdout unavailable
        // On console apps: prints to stdout
        Logger::getInstance().info("Help requested via command line");
        m_parser.showHelp(0);  // This calls exit(0)
        // Never reached
    }

    // Parsing succeeded
    m_parsed = true;
    Logger::getInstance().info("Command line parsed successfully");
    return true;
}

bool CmdLineParser::hasSwitch(const QString& name) const {
    if (!m_parsed) {
        Logger::getInstance().warn("hasSwitch() called before parse()");
        return false;
    }

    // Check if switch name is valid (was added via addSwitch)
    auto it = std::find(m_switches.begin(), m_switches.end(), name);
    if (it == m_switches.end()) {
        Logger::getInstance().warn("Unknown switch: {}", name.toStdString());
        return false;
    }

    // Check if switch was present on command line
    return m_parser.isSet(name);
}

bool CmdLineParser::isDiagnosticMode() const {
    return hasSwitch("diag");
}

bool CmdLineParser::hasOption(const QString& name) const {
    if (!m_parsed) {
        Logger::getInstance().warn("hasOption() called before parse()");
        return false;
    }

    // Check if option name is valid (was added via addOption)
    auto it = std::find(m_options.begin(), m_options.end(), name);
    if (it == m_options.end()) {
        Logger::getInstance().warn("Unknown option: {}", name.toStdString());
        return false;
    }

    // Check if option was present on command line
    return m_parser.isSet(name);
}

QString CmdLineParser::getOptionValue(const QString& name) const {
    if (!m_parsed) {
        Logger::getInstance().warn("getOptionValue() called before parse()");
        return QString();
    }

    // Check if option name is valid
    auto it = std::find(m_options.begin(), m_options.end(), name);
    if (it == m_options.end()) {
        Logger::getInstance().warn("Unknown option: {}", name.toStdString());
        return QString();
    }

    // Return option value (empty string if not set)
    return m_parser.value(name);
}

// =============================================================================
// Private helpers
// =============================================================================

void CmdLineParser::init() {
    // Add help option manually (only -h and --help, not --help-all)
    QCommandLineOption helpOption(QStringList() << "h" << "help",
                                   "Displays help on commandline options.");
    m_parser.addOption(helpOption);

    // Track help switch
    m_switches.push_back("h");
    m_switches.push_back("help");

    Logger::getInstance().debug("Command line parser initialized");
}

QStringList CmdLineParser::argsToStringList(int argc, char** argv) {
    QStringList args;
    for (int i = 0; i < argc; ++i) {
        args << QString::fromUtf8(argv[i]);
    }
    return args;
}

#ifdef _WIN32
QStringList CmdLineParser::argsToStringList(int argc, wchar_t** argv) {
    QStringList args;
    for (int i = 0; i < argc; ++i) {
        args << QString::fromWCharArray(argv[i]);
    }
    return args;
}
#endif

} // namespace core
} // namespace kalahari
