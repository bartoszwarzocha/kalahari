// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file book_window_layouts.cpp
/// @brief The layout of the window of each book, kept in the settings

#include "kalahari/gui/book_window_layouts.h"

#include "kalahari/core/settings_manager.h"

#include <QByteArray>
#include <QFileInfo>
#include <QString>

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include <optional>
#include <string>

namespace kalahari::gui {

namespace {

constexpr const char* BOOK = "book";
constexpr const char* GEOMETRY = "geometry";
constexpr const char* STATE = "state";

/// The layouts kept in the settings; none when the setting is not an array
nlohmann::json keptLayouts() {
    const auto layouts = core::SettingsManager::getInstance().get<nlohmann::json>(
        BOOK_LAYOUTS_KEY, nlohmann::json::array());
    return layouts.is_array() ? layouts : nlohmann::json::array();
}

/// The text under @p key of @p entry; empty when it has none
std::string textOf(const nlohmann::json& entry, const char* key) {
    if (!entry.is_object()) {
        return {};
    }
    const auto found = entry.find(key);
    return found != entry.end() && found->is_string() ? found->get<std::string>() : std::string();
}

/// @p first and @p second are the same book: the same file, or the same path when there is no
/// such file (QFileInfo finds two files that do not exist equal)
bool sameBook(const QFileInfo& first, const QFileInfo& second) {
    if (first.exists() && second.exists()) {
        return first == second;
    }
    return first.absoluteFilePath() == second.absoluteFilePath();
}

/// @p entry is the layout kept for the book @p book
bool isLayoutOf(const nlohmann::json& entry, const QFileInfo& book) {
    const std::string kept = textOf(entry, BOOK);
    return !kept.empty() && sameBook(QFileInfo(QString::fromStdString(kept)), book);
}

/// The bytes kept as base64 under @p key of @p entry
QByteArray bytesOf(const nlohmann::json& entry, const char* key) {
    return QByteArray::fromBase64(QByteArray::fromStdString(textOf(entry, key)));
}

}  // namespace

std::optional<WindowLayout> bookWindowLayout(const QString& manifest) {
    if (manifest.isEmpty()) {
        return std::nullopt;
    }
    const QFileInfo book(manifest);
    for (const nlohmann::json& entry : keptLayouts()) {
        if (isLayoutOf(entry, book)) {
            return WindowLayout{.geometry = bytesOf(entry, GEOMETRY),
                                .state = bytesOf(entry, STATE)};
        }
    }
    return std::nullopt;
}

void keepBookWindowLayout(const QString& manifest, const WindowLayout& layout) {
    const QFileInfo book(manifest);
    nlohmann::json layouts = nlohmann::json::array();
    layouts.push_back({{BOOK, book.absoluteFilePath().toStdString()},
                       {GEOMETRY, layout.geometry.toBase64().toStdString()},
                       {STATE, layout.state.toBase64().toStdString()}});
    // The other books after it, as long as there is room
    for (const nlohmann::json& entry : keptLayouts()) {
        if (layouts.size() == MAX_BOOK_LAYOUTS) {
            break;
        }
        if (!textOf(entry, BOOK).empty() && !isLayoutOf(entry, book)) {
            layouts.push_back(entry);
        }
    }
    core::SettingsManager::getInstance().set<nlohmann::json>(BOOK_LAYOUTS_KEY, layouts);
}

}  // namespace kalahari::gui
