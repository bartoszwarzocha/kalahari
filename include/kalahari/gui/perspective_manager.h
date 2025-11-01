/// @file perspective_manager.h
/// @brief Manager for wxAUI perspective (layout) persistence
///
/// Handles saving and loading panel layouts (perspectives) to/from disk.
/// Uses JSON format for storage in ~/.config/kalahari/perspectives/

#pragma once

#include <string>
#include <vector>
#include <optional>
#include <filesystem>

namespace kalahari {
namespace gui {

/// @brief Manager for wxAUI perspective persistence
///
/// Saves and loads wxAuiManager perspective strings to JSON files.
/// Each perspective is a named layout configuration.
///
/// Singleton pattern for global access.
class PerspectiveManager {
public:
    /// @brief Get singleton instance
    /// @return Reference to singleton instance
    static PerspectiveManager& getInstance();

    /// @brief Save a perspective to disk
    ///
    /// Saves the wxAuiManager perspective string with given name.
    /// Overwrites existing perspective with same name.
    ///
    /// @param name Perspective name (e.g., "Default", "Writing")
    /// @param layout wxAuiManager::SavePerspective() string
    /// @return true if saved successfully, false on error
    bool savePerspective(const std::string& name, const std::string& layout);

    /// @brief Load a perspective from disk
    ///
    /// Loads the wxAuiManager perspective string for given name.
    ///
    /// @param name Perspective name
    /// @return Perspective layout string, or nullopt if not found
    std::optional<std::string> loadPerspective(const std::string& name);

    /// @brief List all saved perspectives
    ///
    /// Returns names of all saved perspectives.
    ///
    /// @return Vector of perspective names
    std::vector<std::string> listPerspectives();

    /// @brief List all saved perspectives with modification timestamps
    ///
    /// Returns names and modification times of all saved perspectives,
    /// sorted by modification time (newest first).
    ///
    /// @return Vector of pairs: (perspective name, modification time)
    std::vector<std::pair<std::string, std::filesystem::file_time_type>> listPerspectivesWithTimestamp();

    /// @brief Delete a perspective
    ///
    /// Removes perspective file from disk.
    ///
    /// @param name Perspective name
    /// @return true if deleted successfully, false if not found or error
    bool deletePerspective(const std::string& name);

    /// @brief Rename a perspective
    ///
    /// Renames an existing perspective.
    ///
    /// @param oldName Current perspective name
    /// @param newName New perspective name
    /// @return true if renamed successfully, false if oldName not found or newName exists
    bool renamePerspective(const std::string& oldName, const std::string& newName);

    /// @brief Check if a perspective exists
    ///
    /// @param name Perspective name
    /// @return true if perspective exists
    bool perspectiveExists(const std::string& name);

    /// @brief Initialize default perspectives
    ///
    /// Creates 4 default perspectives if they don't exist:
    /// - Default: All panels visible
    /// - Writing: Navigator + Editor only
    /// - Editing: Editor + Statistics + Search
    /// - Research: All panels visible + Assistant
    ///
    /// This should be called once on first run.
    void initializeDefaults();

private:
    /// @brief Private constructor (singleton)
    PerspectiveManager() = default;

    /// @brief Get perspectives directory path
    ///
    /// Returns ~/.config/kalahari/perspectives/ (or platform equivalent).
    /// Creates directory if it doesn't exist.
    ///
    /// @return Path to perspectives directory
    std::filesystem::path getPerspectivesDir();

    /// @brief Get perspective file path
    ///
    /// Returns full path to perspective JSON file.
    ///
    /// @param name Perspective name
    /// @return Path to perspective file
    std::filesystem::path getPerspectiveFile(const std::string& name);

    /// @brief Validate perspective name
    ///
    /// Checks if name contains only allowed characters (alphanumeric, space, dash, underscore).
    ///
    /// @param name Perspective name
    /// @return true if valid, false otherwise
    bool isValidName(const std::string& name);
};

} // namespace gui
} // namespace kalahari
