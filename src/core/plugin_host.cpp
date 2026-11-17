// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file plugin_host.cpp
/// @brief What the program gives the plugins

#include "kalahari/core/plugin_host.h"

#include "kalahari/core/logger.h"
#include "kalahari/version.h"

namespace kalahari::core {

PluginHost& PluginHost::getInstance() {
    static PluginHost instance;
    return instance;
}

std::string PluginHost::programVersion() {
    return VERSION_STRING;
}

std::optional<PluginBook> PluginHost::book() const {
    return m_bookSource ? m_bookSource() : std::nullopt;
}

std::optional<PluginText> PluginHost::frontText() const {
    return m_frontTextSource ? m_frontTextSource() : std::nullopt;
}

void PluginHost::refreshPanel(const std::string& pluginId, const std::string& panelId) const {
    if (m_panelRefresh) {
        m_panelRefresh(pluginId, panelId);
    }
}

void PluginHost::showMessage(const std::string& title, const std::string& text) const {
    Logger::getInstance().info("Plugin message: {}: {}", title, text);
    if (m_messageSink) {
        m_messageSink(title, text);
    }
}

const PluginBook* PluginHost::exportBook() const {
    return m_exportBook ? &*m_exportBook : nullptr;
}

PluginHost::ExportScope::ExportScope(PluginBook book)
    : m_previous(std::move(PluginHost::getInstance().m_exportBook)) {
    PluginHost::getInstance().m_exportBook = std::move(book);
}

PluginHost::ExportScope::~ExportScope() {
    PluginHost::getInstance().m_exportBook = std::move(m_previous);
}

void PluginHost::reset() {
    m_bookSource = nullptr;
    m_frontTextSource = nullptr;
    m_panelRefresh = nullptr;
    m_messageSink = nullptr;
    m_language = "en";
    m_exportBook.reset();
}

}  // namespace kalahari::core
