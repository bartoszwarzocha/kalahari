/// @file editor_panel.h
/// @brief Editor panel with BookEditor integration (OpenSpec #00042 Phase 7.1)
///
/// This file defines the EditorPanel class - a rich text editor panel
/// using the custom BookEditor widget for KML document editing.

#pragma once

#include <QWidget>
#include <QString>
#include <memory>

namespace kalahari::editor {
class BookEditor;
class KmlDocument;
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
///
/// The panel owns the KmlDocument and passes a pointer to BookEditor.
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

    /// @brief Set editor content (HTML/KML mode)
    /// @param content HTML content to display
    ///
    /// Converts HTML to KML and loads into editor.
    void setContent(const QString& content);

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

    /// @brief Get the KML document
    /// @return Pointer to the document, or nullptr if none
    editor::KmlDocument* document() { return m_document.get(); }

    /// @brief Get the KML document (const)
    /// @return Const pointer to the document, or nullptr if none
    const editor::KmlDocument* document() const { return m_document.get(); }

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

signals:
    /// @brief Emitted when editor content changes
    ///
    /// Forwarded from BookEditor/KmlDocument content changes.
    void contentChanged();

private:
    /// @brief Apply settings from SettingsManager
    ///
    /// Reads and applies editor appearance settings.
    /// Called on construction and when settings change.
    void applySettings();

    /// @brief Create an empty KML document
    /// @return New KmlDocument with one empty paragraph
    std::unique_ptr<editor::KmlDocument> createEmptyDocument();

    /// @brief Setup document observer
    void setupDocumentObserver();

    editor::BookEditor* m_bookEditor;                     ///< The BookEditor widget
    std::unique_ptr<editor::KmlDocument> m_document;      ///< Owned KML document

    class Observer;
    std::unique_ptr<Observer> m_observer;                 ///< Document observer

    /// @brief Statistics collector for tracking writing stats (OpenSpec #00042 Task 7.7)
    editor::StatisticsCollector* m_statisticsCollector{nullptr};
};

} // namespace gui
} // namespace kalahari
