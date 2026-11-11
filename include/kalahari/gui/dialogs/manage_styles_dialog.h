/// @file manage_styles_dialog.h
/// @brief Format > Text Style > Manage Styles...: all the paragraph styles of the book

#pragma once

#include "kalahari/gui/dialogs/kalahari_dialog.h"
#include "kalahari/gui/style_coordinator.h"

#include <QHash>
#include <QString>

class QLabel;
class QListWidget;
class QPushButton;

namespace kalahari::gui {
class WrappingLabel;
}

namespace kalahari::gui::dialogs {

class StylePreview;

/// @brief Manage Styles...: the styles of the book's type and the book's own, each with its
///        preview, its settings in words and how many paragraphs use it
///
/// New... makes a style based on the one selected, Modify... opens its window, Delete takes a
/// style of the writer's own away (asking when paragraphs use it) and Reset to the Type's Style
/// brings back a style of the type the book changed. Insert, Enter or F2, Delete and Esc do
/// the same from the keyboard; Ctrl+Z undoes the last change (StyleCoordinator::undo()).
class ManageStylesDialog : public KalahariDialog {
    Q_OBJECT

public:
    /// @brief The window over the styles of the open book
    /// @param styles What changes the styles
    /// @param selectedId The style selected at first (empty or unknown: Normal)
    /// @param parent Parent widget
    ManageStylesDialog(StyleCoordinator& styles, const QString& selectedId,
                       QWidget* parent = nullptr);

    /// @brief The id of the style selected; empty without one
    [[nodiscard]] QString selectedStyle() const;

    /// @brief Select a style in the list
    void selectStyle(const QString& id);

    /// @brief The parts of the window (for tests)
    [[nodiscard]] QListWidget* list() const { return m_list; }
    [[nodiscard]] QLabel* nameLabel() const { return m_name; }
    [[nodiscard]] QLabel* kindLabel() const { return m_kind; }
    [[nodiscard]] StylePreview* preview() const { return m_preview; }
    [[nodiscard]] QLabel* settingsLabel() const { return m_settings; }
    [[nodiscard]] QLabel* usageLabel() const { return m_usage; }
    [[nodiscard]] QLabel* undoLabel() const { return m_undoHint; }
    [[nodiscard]] QPushButton* newButton() const { return m_newButton; }
    [[nodiscard]] QPushButton* modifyButton() const { return m_modifyButton; }
    [[nodiscard]] QPushButton* deleteButton() const { return m_deleteButton; }
    [[nodiscard]] QPushButton* resetButton() const { return m_resetButton; }

public slots:
    /// @brief New...: a new style based on the one selected
    void newStyle();

    /// @brief Modify...: the window of the style selected
    void modifyStyle();

    /// @brief Delete: the style selected, a style of the writer's own
    void deleteStyle();

    /// @brief Reset to the Type's Style: the style selected as the type has it
    void resetStyle();

    /// @brief Ctrl+Z: undo the last change of the styles
    void undo();

protected:
    /// @brief Enter in the list opens the style, as F2 does, instead of closing the window
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    /// @brief Fill the list: the styles of the type, then the book's own; the selection stays
    void fillList();

    /// @brief Show the style selected: its name and kind, preview, settings and use
    void showStyle();

    /// @brief The settings of a style in words, a line for each part
    [[nodiscard]] QString settingsText(const editor::ParagraphStyleSet& set,
                                       const editor::ParagraphStyle& style) const;

    StyleCoordinator& m_styles;
    QHash<QString, StyleCoordinator::Usage> m_usageOf;  ///< Counted again after a change
    QListWidget* m_list = nullptr;
    QLabel* m_name = nullptr;
    QLabel* m_kind = nullptr;
    StylePreview* m_preview = nullptr;
    QLabel* m_settings = nullptr;
    QLabel* m_usage = nullptr;
    QLabel* m_undoHint = nullptr;
    QPushButton* m_newButton = nullptr;
    QPushButton* m_modifyButton = nullptr;
    QPushButton* m_deleteButton = nullptr;
    QPushButton* m_resetButton = nullptr;
};

}  // namespace kalahari::gui::dialogs
