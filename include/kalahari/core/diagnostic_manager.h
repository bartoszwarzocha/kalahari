/// @file diagnostic_manager.h
/// @brief Diagnostic mode manager for Kalahari
///
/// Manages the global diagnostic mode state. When enabled, additional
/// diagnostic tools and menus are available in the application UI.

#pragma once

namespace kalahari {
namespace core {

/// @brief Singleton manager for diagnostic mode state
///
/// Controls whether the application is running in diagnostic mode.
/// Diagnostic mode enables additional tools for troubleshooting:
/// - Diagnostics menu in GUI
/// - Python integration tests
/// - System information
/// - Log file access
/// - Future: plugin diagnostics, database checks, etc.
///
/// Usage:
/// @code
/// // Enable diagnostic mode (typically from command line --diag flag)
/// DiagnosticManager::getInstance().setEnabled(true);
///
/// // Check if diagnostic mode is enabled
/// if (DiagnosticManager::getInstance().isEnabled()) {
///     // Show diagnostics menu
/// }
/// @endcode
class DiagnosticManager {
public:
    /// @brief Get singleton instance
    /// @return Reference to the singleton DiagnosticManager
    static DiagnosticManager& getInstance();

    // Prevent copying and moving
    DiagnosticManager(const DiagnosticManager&) = delete;
    DiagnosticManager& operator=(const DiagnosticManager&) = delete;
    DiagnosticManager(DiagnosticManager&&) = delete;
    DiagnosticManager& operator=(DiagnosticManager&&) = delete;

    /// @brief Check if diagnostic mode is enabled
    /// @return true if diagnostic mode is enabled, false otherwise
    bool isEnabled() const { return m_enabled; }

    /// @brief Set diagnostic mode state
    /// @param enabled true to enable diagnostic mode, false to disable
    void setEnabled(bool enabled) { m_enabled = enabled; }

private:
    /// @brief Private constructor (singleton pattern)
    DiagnosticManager() = default;

    /// @brief Destructor
    ~DiagnosticManager() = default;

    /// @brief Diagnostic mode enabled flag
    bool m_enabled = false;
};

} // namespace core
} // namespace kalahari
