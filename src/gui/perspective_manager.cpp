/// @file perspective_manager.cpp
/// @brief Implementation of PerspectiveManager

#include "kalahari/gui/perspective_manager.h"
#include <kalahari/core/logger.h>
#include <nlohmann/json.hpp>
#include <fstream>
#include <algorithm>
#include <wx/stdpaths.h>

using json = nlohmann::json;

namespace kalahari {
namespace gui {

PerspectiveManager& PerspectiveManager::getInstance() {
    static PerspectiveManager instance;
    return instance;
}

std::filesystem::path PerspectiveManager::getPerspectivesDir() {
    // Get user config directory (cross-platform)
    wxStandardPaths& paths = wxStandardPaths::Get();
    wxString userDataDir = paths.GetUserDataDir();

    std::filesystem::path perspectivesDir = userDataDir.ToStdString();
    perspectivesDir /= "perspectives";

    // Create directory if it doesn't exist
    if (!std::filesystem::exists(perspectivesDir)) {
        try {
            std::filesystem::create_directories(perspectivesDir);
            core::Logger::getInstance().info("Created perspectives directory: {}", perspectivesDir.string());
        } catch (const std::filesystem::filesystem_error& e) {
            core::Logger::getInstance().error("Failed to create perspectives directory: {}", e.what());
        }
    }

    return perspectivesDir;
}

std::filesystem::path PerspectiveManager::getPerspectiveFile(const std::string& name) {
    return getPerspectivesDir() / (name + ".json");
}

bool PerspectiveManager::isValidName(const std::string& name) {
    if (name.empty() || name.length() > 64) {
        return false;
    }

    // Only allow alphanumeric, space, dash, underscore
    return std::all_of(name.begin(), name.end(), [](char c) {
        return std::isalnum(c) || c == ' ' || c == '-' || c == '_';
    });
}

bool PerspectiveManager::savePerspective(const std::string& name, const std::string& layout) {
    if (!isValidName(name)) {
        core::Logger::getInstance().error("Invalid perspective name: {}", name);
        return false;
    }

    try {
        json j;
        j["name"] = name;
        j["layout"] = layout;
        j["version"] = "1.0";

        std::filesystem::path filePath = getPerspectiveFile(name);
        std::ofstream file(filePath);
        if (!file.is_open()) {
            core::Logger::getInstance().error("Failed to open file for writing: {}", filePath.string());
            return false;
        }

        file << j.dump(2);  // Pretty print with 2-space indent
        file.close();

        core::Logger::getInstance().info("Saved perspective: {} to {}", name, filePath.string());
        return true;

    } catch (const std::exception& e) {
        core::Logger::getInstance().error("Failed to save perspective '{}': {}", name, e.what());
        return false;
    }
}

std::optional<std::string> PerspectiveManager::loadPerspective(const std::string& name) {
    if (!isValidName(name)) {
        core::Logger::getInstance().error("Invalid perspective name: {}", name);
        return std::nullopt;
    }

    std::filesystem::path filePath = getPerspectiveFile(name);
    if (!std::filesystem::exists(filePath)) {
        core::Logger::getInstance().warn("Perspective file not found: {}", filePath.string());
        return std::nullopt;
    }

    try {
        std::ifstream file(filePath);
        if (!file.is_open()) {
            core::Logger::getInstance().error("Failed to open file for reading: {}", filePath.string());
            return std::nullopt;
        }

        json j;
        file >> j;
        file.close();

        if (!j.contains("layout")) {
            core::Logger::getInstance().error("Perspective file missing 'layout' field: {}", filePath.string());
            return std::nullopt;
        }

        std::string layout = j["layout"].get<std::string>();
        core::Logger::getInstance().info("Loaded perspective: {} from {}", name, filePath.string());
        return layout;

    } catch (const std::exception& e) {
        core::Logger::getInstance().error("Failed to load perspective '{}': {}", name, e.what());
        return std::nullopt;
    }
}

std::vector<std::string> PerspectiveManager::listPerspectives() {
    std::vector<std::string> perspectives;
    std::filesystem::path dir = getPerspectivesDir();

    if (!std::filesystem::exists(dir)) {
        return perspectives;
    }

    try {
        for (const auto& entry : std::filesystem::directory_iterator(dir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".json") {
                std::string name = entry.path().stem().string();
                perspectives.push_back(name);
            }
        }

        // Sort alphabetically
        std::sort(perspectives.begin(), perspectives.end());

    } catch (const std::filesystem::filesystem_error& e) {
        core::Logger::getInstance().error("Failed to list perspectives: {}", e.what());
    }

    return perspectives;
}

std::vector<std::pair<std::string, std::filesystem::file_time_type>>
PerspectiveManager::listPerspectivesWithTimestamp() {
    std::vector<std::pair<std::string, std::filesystem::file_time_type>> perspectives;
    std::filesystem::path dir = getPerspectivesDir();

    if (!std::filesystem::exists(dir)) {
        return perspectives;
    }

    try {
        for (const auto& entry : std::filesystem::directory_iterator(dir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".json") {
                std::string name = entry.path().stem().string();
                auto modified = std::filesystem::last_write_time(entry.path());
                perspectives.push_back({name, modified});
            }
        }

        // Sort by modification time (newest first)
        std::sort(perspectives.begin(), perspectives.end(),
            [](const auto& a, const auto& b) {
                return a.second > b.second;
            });

    } catch (const std::filesystem::filesystem_error& e) {
        core::Logger::getInstance().error("Failed to list perspectives with timestamps: {}", e.what());
    }

    return perspectives;
}

bool PerspectiveManager::deletePerspective(const std::string& name) {
    if (!isValidName(name)) {
        core::Logger::getInstance().error("Invalid perspective name: {}", name);
        return false;
    }

    std::filesystem::path filePath = getPerspectiveFile(name);
    if (!std::filesystem::exists(filePath)) {
        core::Logger::getInstance().warn("Perspective file not found for deletion: {}", filePath.string());
        return false;
    }

    try {
        std::filesystem::remove(filePath);
        core::Logger::getInstance().info("Deleted perspective: {} ({})", name, filePath.string());
        return true;

    } catch (const std::filesystem::filesystem_error& e) {
        core::Logger::getInstance().error("Failed to delete perspective '{}': {}", name, e.what());
        return false;
    }
}

bool PerspectiveManager::renamePerspective(const std::string& oldName, const std::string& newName) {
    if (!isValidName(oldName) || !isValidName(newName)) {
        core::Logger::getInstance().error("Invalid perspective name (old='{}', new='{}')", oldName, newName);
        return false;
    }

    std::filesystem::path oldPath = getPerspectiveFile(oldName);
    std::filesystem::path newPath = getPerspectiveFile(newName);

    if (!std::filesystem::exists(oldPath)) {
        core::Logger::getInstance().error("Old perspective not found: {}", oldPath.string());
        return false;
    }

    if (std::filesystem::exists(newPath)) {
        core::Logger::getInstance().error("New perspective name already exists: {}", newPath.string());
        return false;
    }

    try {
        // Load old perspective
        auto layout = loadPerspective(oldName);
        if (!layout) {
            return false;
        }

        // Save with new name
        if (!savePerspective(newName, *layout)) {
            return false;
        }

        // Delete old file
        std::filesystem::remove(oldPath);

        core::Logger::getInstance().info("Renamed perspective: '{}' -> '{}'", oldName, newName);
        return true;

    } catch (const std::exception& e) {
        core::Logger::getInstance().error("Failed to rename perspective '{}' to '{}': {}",
            oldName, newName, e.what());
        return false;
    }
}

bool PerspectiveManager::perspectiveExists(const std::string& name) {
    std::filesystem::path filePath = getPerspectiveFile(name);
    return std::filesystem::exists(filePath);
}

void PerspectiveManager::initializeDefaults() {
    core::Logger::getInstance().info("Initializing default perspectives...");

    // Note: These are placeholder layouts. Real layouts will be generated
    // by wxAuiManager::SavePerspective() when the panels are first created.
    // The actual implementation will happen when we call this from MainWindow.

    // For now, just create empty placeholders
    // The real layouts will be saved when initializeAUI() creates the panels

    core::Logger::getInstance().info("Default perspectives will be created from actual panel layouts");
}

} // namespace gui
} // namespace kalahari
