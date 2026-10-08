/// @file editor_panel.h
/// @brief Editor panel with BookEditor integration (OpenSpec #00042 Phase 7.1)
///
/// This file defines the EditorPanel class - a rich text editor panel
/// using the custom BookEditor widget for KML document editing.

#pragma once

#include <QWidget>
#include <QString>
#include <atomic>
#include <memory>

namespace kalahari::editor {
class BookEditor;
class StatisticsCollector;
}

namespace kalahari {
namespace gui {

/// @brief Editor panel with BookEditor integration
///
/// Wraps the BookEditor widget for KML document editing with:
/// - Document loading/saving via KML format
/// - Settings integration (font, colors, etc.)
/// - Signal forwarding for content changes
class EditorPanel : public QWidget {
    Q_OBJECT

public:
    /// @brief Constructor
    /// @param parent Parent widget
    explicit EditorPanel(QWidget* parent = nullptr);

    /// @brief Destructor
    ~EditorPanel() override;

    /// @brief Set editor text (plain text mode)
    /// @param text Text to display in editor
    ///
    /// Converts plain text to KML paragraphs.
    /// Used by Document load operations.
    void setText(const QString& text);

    /// @brief Get editor text (plain text mode)
    /// @return Current editor content as plain text
    ///
    /// Extracts plain text from KML document.
    QString getText() const;

    /// @brief Set editor content (KML)
    /// @param content KML content to display
    /// @return False when the KML is damaged, so only the text before the damaged place is shown
    bool setContent(const QString& content);

    /// @brief Tell the user that a chapter's KML is damaged: only its text before the damaged
    /// place is shown, and saving the chapter keeps only that text
    /// @param parent Parent of the message box
    /// @param chapterName Name of the chapter, as the user knows it
    static void warnDamagedChapter(QWidget* parent, const QString& chapterName);

    /// @brief Get editor content (HTML mode)
    /// @return Current editor content as HTML
    ///
    /// Converts KML to HTML for export.
    QString getContent() const;

    /// @brief Get the underlying BookEditor widget
    /// @return Pointer to BookEditor widget
    ///
    /// Use for direct access to BookEditor features (view modes, cursor, etc.)
    editor::BookEditor* getBookEditor() { return m_bookEditor; }

    /// @brief Get the underlying BookEditor widget (const)
    /// @return Const pointer to BookEditor widget
    const editor::BookEditor* getBookEditor() const { return m_bookEditor; }

    // =========================================================================
    // Statistics Integration (OpenSpec #00042 Task 7.7)
    // =========================================================================

    /// @brief Set the statistics collector for this editor
    /// @param collector Pointer to shared StatisticsCollector (nullptr to disconnect)
    ///
    /// When set, the collector will track document changes from this editor.
    /// The collector is typically owned by DocumentCoordinator and shared
    /// across all editor panels in a project.
    void setStatisticsCollector(editor::StatisticsCollector* collector);

    /// @brief Get the current statistics collector
    /// @return Pointer to StatisticsCollector, or nullptr if not set
    editor::StatisticsCollector* statisticsCollector() const { return m_statisticsCollector; }

    /// @brief Give the editor the editor settings ("editor." keys, with the defaults of the
    ///        settings schema)
    ///
    /// The one place that reads them. Runs when the panel is created and, once the event
    /// loop runs, after any change of an editor setting.
    void applySettings();

signals:
    /// @brief Emitted when editor content changes
    ///
    /// Forwarded from BookEditor content changes.
    void contentChanged();

protected:
    /// @brief Keeps the pages at their size on paper when the panel is shown or moves to
    ///        another screen
    bool event(QEvent* event) override;

private slots:
    /// @brief Apply the settings changed since scheduleSettings() (which queues this call)
    void applyScheduledSettings();

private:
    /// @brief Set the editor's paper scale from the screen the panel is on
    void applyPaperScale();

    /// @brief Run applySettings() once the event loop runs, once for all the settings
    ///        changed until then
    /// @note Safe to call from any thread
    void scheduleSettings();

    editor::BookEditor* m_bookEditor;                     ///< The BookEditor widget
    int m_settingsListener{0};                            ///< SettingsManager::subscribe() id
    std::atomic<bool> m_settingsPending{false};           ///< scheduleSettings() waits for the
                                                          ///< event loop

    /// @brief Statistics collector for tracking writing stats (OpenSpec #00042 Task 7.7)
    editor::StatisticsCollector* m_statisticsCollector{nullptr};
};

} // namespace gui
} // namespace kalahari
