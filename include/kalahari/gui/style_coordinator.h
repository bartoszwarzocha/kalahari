/// @file style_coordinator.h
/// @brief The paragraph styles of the book: new ones, changed, deleted and brought back as the
///        type has them, from Format > Text Style, the palette and Manage Styles

#pragma once

#include "kalahari/core/book_project.h"
#include "kalahari/editor/paragraph_styles.h"

#include <QHash>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>

#include <functional>
#include <optional>
#include <utility>

class QStatusBar;
class QTabWidget;
class QWidget;

namespace kalahari::editor {
class BookEditor;
}

namespace kalahari::gui {

class EditorPanel;
class StyleDraft;

/// @brief Changes the paragraph styles of the open book
///
/// The book keeps its own styles and its changes to the styles of its type in its .klh file
/// (core::BookProject::paragraphStyles), so every text of the book shows a change at once.
/// New Style... opens the window of a style (dialogs::StyleDialog) on the style selected, with
/// the look of the paragraph at the cursor, and gives the new style to that paragraph; Modify
/// Style... opens it on a style of the book. Delete Style takes a style of the writer's own
/// away: its paragraphs, in the open texts and the saved ones, go to its base style, and the
/// styles based on it keep their look (editor::withoutOwnStyle()); the window asks first when
/// paragraphs use it. Reset to the Type's Style brings a style of the type back as the type
/// has it. A style of the type cannot be deleted.
///
/// The last change can be undone: Ctrl+Z in Manage Styles, or Undo in the notice under the
/// text that a style was deleted or brought back. The open texts it changed undo their step,
/// the saved ones get their text back; a change of a text after it makes it final.
class StyleCoordinator : public QObject {
    Q_OBJECT

public:
    /// @brief Says something under the text of a tab, with a button
    /// (DocumentCoordinator::showNotice())
    using NoticeShower = std::function<void(EditorPanel* editor, const QString& message,
                                            const QString& actionText,
                                            const QString& actionToolTip,
                                            std::function<void()> action)>;

    /// @brief How many paragraphs use a style, in how many texts
    struct Usage {
        int paragraphs = 0;
        int texts = 0;

        bool operator==(const Usage&) const = default;
    };

    /// @brief Constructor
    /// @param centralTabs The tabs of the documents
    /// @param showNotice Says under the text that a style was deleted or brought back, with
    ///        Undo (none: nothing says it)
    /// @param statusBar Where short messages go (may be null)
    /// @param window The window of the program, over which the windows of the styles open
    /// @param parent Parent object
    StyleCoordinator(QTabWidget* centralTabs, NoticeShower showNotice, QStatusBar* statusBar,
                     QWidget* window, QObject* parent = nullptr);

    StyleCoordinator(const StyleCoordinator&) = delete;
    StyleCoordinator& operator=(const StyleCoordinator&) = delete;

    /// @brief Give New Style, Modify Style, Delete Style and Manage Styles their callbacks
    /// @note Call after the commands are registered
    void connectCommands();

    // =========================================================================
    // The styles of the book
    // =========================================================================

    /// @brief The styles of the open book (bookParagraphStyles())
    [[nodiscard]] editor::ParagraphStyleSet styles() const;

    /// @brief The book's entries (core::BookParagraphStyle) of its styles
    [[nodiscard]] QList<core::BookParagraphStyle> entries() const;

    /// @brief The name of the type of the open book ("Novel")
    [[nodiscard]] QString typeName() const;

    /// @brief How many paragraphs of the texts of the book use each style: the open texts as
    ///        their editors have them, the others as saved
    [[nodiscard]] QHash<QString, Usage> usage() const;

    /// @brief The editor of the text in front, a text of the book (null without one)
    [[nodiscard]] editor::BookEditor* editorInFront() const;

    /// @brief The style of the paragraph at the cursor of the text in front; Normal without one
    [[nodiscard]] QString styleAtCursor() const;

    // =========================================================================
    // Why a command cannot be used; empty: it can
    // =========================================================================

    /// @brief Why the styles of the book cannot change: no book, a book open read-only
    [[nodiscard]] QString whyNotChange() const;

    /// @brief Why style @p id cannot be deleted: a style of the type, Normal
    [[nodiscard]] QString whyNotDelete(const QString& id) const;

    /// @brief Why style @p id cannot be brought back as its type has it: a style of the
    ///        writer's own, a style the book did not change
    [[nodiscard]] QString whyNotReset(const QString& id) const;

    // =========================================================================
    // The commands
    // =========================================================================

    /// @brief New Style...: the window of a new style based on @p baseId (empty: the style at
    ///        the cursor), with the look of the paragraph at the cursor when it is in that style
    /// @param applyToText Whether the window offers to give the new style to the paragraph at
    ///        the cursor (from the text; not from Manage Styles)
    /// @param parent Over which the window opens (null: the window of the program)
    /// @return The id of the new style; empty when none was made
    QString newStyle(const QString& baseId = QString(), bool applyToText = true,
                     QWidget* parent = nullptr);

    /// @brief Create Style...: the window of a new style for the paragraphs of style @p id,
    ///        which the book does not have (pasted from another book); the style keeps the id,
    ///        so the paragraphs show it at once (StyleDraft::forMissing())
    /// @return Whether it was made
    bool createMissingStyle(const QString& id, QWidget* parent = nullptr);

    /// @brief Modify Style...: the window of style @p id (empty: the style at the cursor)
    /// @return Whether the style changed
    bool modifyStyle(const QString& id = QString(), QWidget* parent = nullptr);

    /// @brief Delete Style: delete style @p id (empty: the style at the cursor), after asking
    ///        when paragraphs use it; under the text a notice says so, with Undo, when
    ///        @p notice
    /// @return Whether it was deleted
    bool deleteStyle(const QString& id = QString(), QWidget* parent = nullptr, bool notice = true);

    /// @brief Reset to the Type's Style: style @p id as the book's type has it; a notice says
    ///        so, with Undo, when @p notice
    bool resetStyle(const QString& id, bool notice = true);

    /// @brief Manage Styles...: the window of all the styles of the book, style @p selectedId
    ///        selected (empty: the style at the cursor)
    void manageStyles(const QString& selectedId = QString());

    // =========================================================================
    // The changes themselves, without asking (for the windows and tests)
    // =========================================================================

    /// @brief Keep the style of a draft: a new style gets an id of its name; its paragraphs and
    ///        the texts show it at once
    /// @return The id of the style; empty when the book cannot keep it
    QString saveDraft(const StyleDraft& draft);

    /// @brief Delete style @p id: its paragraphs go to its base style in every text
    /// @return The number of paragraphs that went to another style; -1 when it was not deleted
    int removeStyle(const QString& id);

    // =========================================================================
    // Undo of the last change
    // =========================================================================

    /// @brief Whether the last change can be undone
    [[nodiscard]] bool canUndo() const;

    /// @brief What undo would undo ("Deleting the style \"Memory\"")
    [[nodiscard]] QString undoText() const;

    /// @brief Undo the last change
    /// @return Whether it was undone
    bool undo();

    /// @brief Forget the last change: it can no longer be undone
    void forgetUndo();

signals:
    /// @brief The styles of the book changed here, or the change was undone
    void stylesChanged();

    /// @brief Whether the last change can be undone changed
    void undoChanged();

private:
    /// @brief A text that was not open, as it was before a change
    struct SavedText {
        QString elementId;
        QString kml;         ///< Its text before the change
        bool saved = false;  ///< The change was saved to its file (it had no other changes)
    };

    /// @brief What a change did, so that it can be undone
    struct Step {
        QString text;                                 ///< undoText()
        QList<core::BookParagraphStyle> entries;      ///< The book's entries before it
        QList<SavedText> savedTexts;                  ///< Texts not open that it changed
        QList<QPointer<editor::BookEditor>> editors;  ///< Open texts that made a step of it
    };

    /// @brief Set the book's entries as one change, keeping what undoes it
    /// @param text What undo would undo
    /// @param replace A style whose paragraphs go to another: (from, to); none to move none
    /// @return The number of paragraphs that went to another style; -1 when the book did not
    ///         keep the entries
    int change(const QString& text, const QList<core::BookParagraphStyle>& entries,
               const std::optional<std::pair<QString, QString>>& replace);

    /// @brief The editors of the open texts of the book, by element
    [[nodiscard]] QHash<QString, editor::BookEditor*> openEditors() const;

    /// @brief The panel of the text in front, a text of the book (null without one)
    [[nodiscard]] EditorPanel* panelInFront() const;

    /// @brief Say @p message under the text in front, with Undo
    void notify(const QString& message);

    /// @brief Show @p message in the status bar for a while
    void say(const QString& message) const;

    /// @brief The name of style @p id in the book
    [[nodiscard]] QString nameOf(const QString& id) const;

    /// @brief The style a command acts on: @p id, else the style at the cursor
    [[nodiscard]] QString styleFor(const QString& id) const;

    /// @brief Follow the cursor of the text in front: the commands of the style at the cursor
    void followEditor();

private slots:
    /// @brief Enable the commands for the text in front and the style at its cursor
    void updateCommandStates();

private:
    QTabWidget* m_centralTabs;
    NoticeShower m_showNotice;
    QStatusBar* m_statusBar;
    QWidget* m_window;
    std::optional<Step> m_undo;
    bool m_changing = false;  ///< The styles change here: not a change from elsewhere
    QPointer<editor::BookEditor> m_followed;  ///< The editor whose cursor the commands follow
    QList<QMetaObject::Connection> m_undoWatch;  ///< Changes of texts that make a change final
};

}  // namespace kalahari::gui
