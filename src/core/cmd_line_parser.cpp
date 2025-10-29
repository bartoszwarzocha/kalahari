/// @file cmd_line_parser.cpp
/// @brief Implementation of CmdLineParser

#include <kalahari/core/cmd_line_parser.h>
#include <kalahari/core/logger.h>
#include <algorithm>

namespace kalahari {
namespace core {

// =============================================================================
// Constructors
// =============================================================================

CmdLineParser::CmdLineParser(int argc, char** argv)
    : m_parser(argc, argv), m_argc(argc) {
    init();
}

#ifdef _WIN32
CmdLineParser::CmdLineParser(int argc, wchar_t** argv)
    : m_parser(argc, argv), m_argc(argc) {
    init();
}
#endif

// =============================================================================
// Public API
// =============================================================================

void CmdLineParser::addSwitch(const wxString& shortName,
                               const wxString& longName,
                               const wxString& description) {
    // Add switch to wxCmdLineParser
    int flags = static_cast<int>(wxCMD_LINE_VAL_NONE) | static_cast<int>(wxCMD_LINE_PARAM_OPTIONAL);
    m_parser.AddSwitch(shortName, longName, description, flags);

    // Track switch names for validation
    m_switches.push_back(shortName);
    m_switches.push_back(longName);

    Logger::getInstance().debug("Added command line switch: -{} / --{}",
                                shortName.utf8_str().data(),
                                longName.utf8_str().data());
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
    int result = m_parser.Parse();

    if (result == -1) {
        // Error occurred during parsing
        Logger::getInstance().error("Command line parsing error");
        return false;
    }

    if (result > 0) {
        // Help was requested (--help or -h)
        // wxCmdLineParser already printed help text
        Logger::getInstance().info("Help requested via command line");
        return false;
    }

    // Parsing succeeded
    m_parsed = true;
    Logger::getInstance().info("Command line parsed successfully");
    return true;
}

bool CmdLineParser::hasSwitch(const wxString& name) const {
    if (!m_parsed) {
        Logger::getInstance().warn("hasSwitch() called before parse()");
        return false;
    }

    // Check if switch name is valid (was added via addSwitch)
    auto it = std::find(m_switches.begin(), m_switches.end(), name);
    if (it == m_switches.end()) {
        Logger::getInstance().warn("Unknown switch: {}", name.utf8_str().data());
        return false;
    }

    // Check if switch was present on command line
    wxCmdLineSwitchState state = m_parser.FoundSwitch(name);
    return (state == wxCMD_SWITCH_ON);
}

// =============================================================================
// Private helpers
// =============================================================================

void CmdLineParser::init() {
    // Set switch characters (standard: - and / for Windows, - for Unix)
#ifdef _WIN32
    m_parser.SetSwitchChars("/-");
#else
    m_parser.SetSwitchChars("-");
#endif

    // Add default help switch
    int flags = static_cast<int>(wxCMD_LINE_VAL_NONE) | static_cast<int>(wxCMD_LINE_OPTION_HELP);
    m_parser.AddSwitch("h", "help", "Display this help message", flags);

    // Track help switch
    m_switches.push_back("h");
    m_switches.push_back("help");

    Logger::getInstance().debug("Command line parser initialized");
}

} // namespace core
} // namespace kalahari
