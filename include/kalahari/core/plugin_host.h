// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file plugin_host.h
/// @brief What the program gives the plugins: the book, the text in front, messages and the
///        refreshing of their panels
///
/// The plugins live in the core (PluginManager, the kalahari_api module), the book windows in
/// the program. PluginHost is the meeting point: the program gives it the sources of the book
/// and of the text in front and the ways to show a message or refresh a panel of a plugin
/// (gui::installPluginHost()); the module of the plugins reads and calls them. Everything runs
/// on the main thread, which the plugins' calls are on.

#pragma once

#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace kalahari::core {

/// @brief One item of a book as the plugins see it: a text, the title of a part, or the place
///        of the table of contents
struct PluginText {
    std::string kind;   ///< "text", "part" (a group of texts: its title only) or "contents"
    std::string id;     ///< Its element in the book; empty: a file outside a book
    std::string title;  ///< Its title as the Navigator shows it
    std::string text;   ///< Its paragraphs, one per line ("text" only), as the editor shows them
    std::string kml;    ///< Its text in the program's format, with the formatting ("text" only)
};

/// @brief A book as the plugins see it: its texts in reading order, with their unsaved changes
struct PluginBook {
    std::string title;
    std::string author;
    std::string language;           ///< Language code of the text ("pl")
    std::vector<PluginText> items;  ///< The front matter, the main part and the back matter
};

/// @brief What the program gives the plugins (singleton, main thread)
class PluginHost {
public:
    /// @brief The book of the window in front; none while it has no book open
    using BookSource = std::function<std::optional<PluginBook>()>;
    /// @brief The text in front: of the book or a file outside it; none without one
    using TextSource = std::function<std::optional<PluginText>()>;
    /// @brief Read the page of a panel of a plugin again (plugin id, panel id), in every window
    using PanelRefresh = std::function<void(const std::string&, const std::string&)>;
    /// @brief Show a message of a plugin to the writer (title, text)
    using MessageSink = std::function<void(const std::string&, const std::string&)>;

    static PluginHost& getInstance();

    PluginHost(const PluginHost&) = delete;
    PluginHost& operator=(const PluginHost&) = delete;

    void setBookSource(BookSource source) { m_bookSource = std::move(source); }
    void setFrontTextSource(TextSource source) { m_frontTextSource = std::move(source); }
    void setPanelRefresh(PanelRefresh refresh) { m_panelRefresh = std::move(refresh); }
    void setMessageSink(MessageSink sink) { m_messageSink = std::move(sink); }

    /// @brief The language of the program's windows ("pl", "en"), for the plugins' own words
    void setProgramLanguage(std::string language) { m_language = std::move(language); }
    [[nodiscard]] const std::string& programLanguage() const { return m_language; }

    /// @brief The version of the program ("0.4.0-alpha"). Here, not in the module of the API:
    ///        on Windows the headers of Python define COMPILER and PLATFORM, which version.h has
    [[nodiscard]] static std::string programVersion();

    /// @brief The book of the window in front, as its texts are now
    [[nodiscard]] std::optional<PluginBook> book() const;

    /// @brief The text in front, as it is now
    [[nodiscard]] std::optional<PluginText> frontText() const;

    /// @brief Read the page of the panel @p panelId of the plugin @p pluginId again
    void refreshPanel(const std::string& pluginId, const std::string& panelId) const;

    /// @brief Show a message of a plugin to the writer; without a window, only the log has it
    void showMessage(const std::string& title, const std::string& text) const;

    /// @brief The book of an export written by a plugin: what the writer chose in the window
    ///        of File > Export, set while the plugin writes the file
    [[nodiscard]] const PluginBook* exportBook() const;

    /// @brief Sets the book of an export for as long as it lives
    class ExportScope {
    public:
        explicit ExportScope(PluginBook book);
        ~ExportScope();
        ExportScope(const ExportScope&) = delete;
        ExportScope& operator=(const ExportScope&) = delete;

    private:
        std::optional<PluginBook> m_previous;
    };

    /// @brief Forget every source (tests)
    void reset();

private:
    PluginHost() = default;
    ~PluginHost() = default;

    BookSource m_bookSource;
    TextSource m_frontTextSource;
    PanelRefresh m_panelRefresh;
    MessageSink m_messageSink;
    std::string m_language = "en";
    std::optional<PluginBook> m_exportBook;
};

}  // namespace kalahari::core
