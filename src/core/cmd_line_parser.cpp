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
    m_parser.setApplicationDescription(appDescription);
    Logger::getInstance().debug("Set application description: {}", appName.toStdString());
}

void CmdLineParser::addSwitch(const QString& shortName,
                               const QString& longName,
                               const QString& description) {
    // Create QCommandLineOption for boolean switch
    QCommandLineOption option(QStringList() << shortName << longName, description);
    m_parser.addOption(option);

    // Track switch names for validation
    m_switches.push_back(shortName);
    m_switches.push_back(longName);

    Logger::getInstance().debug("Added command line switch: -{} / --{}",
                                shortName.toStdString(),
                                longName.toStdString());
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
        // Print help text to stdout
        QString helpText = m_parser.helpText();
        fprintf(stdout, "%s\n", helpText.toUtf8().constData());
        Logger::getInstance().info("Help requested via command line");
        return false;
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

// =============================================================================
// Private helpers
// =============================================================================

void CmdLineParser::init() {
    // Add default help switch
    m_parser.addHelpOption();

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
