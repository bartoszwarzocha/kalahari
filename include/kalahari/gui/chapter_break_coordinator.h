/// @file chapter_break_coordinator.h
/// @brief Book > Chapter Break: the text from the cursor to a new text of the book right after
///        the text it was in, with Undo

#pragma once

#include <QList>
#include <QMetaObject>
#include <QObject>
#include <QString>

#include <functional>
#include <optional>

class QStatusBar;
class QTabWidget;
class QTextDocument;
class QWidget;

namespace kalahari::editor {
class BookEditor;
}

namespace kalahari::gui {

class EditorPanel;
class NavigatorCoordinator;

/// @brief The title of a new text whose text starts at position @p from of @p document: the
///        text of its first paragraph with text when that is a heading (empty paragraphs before
///        it skipped), on one line
/// @return Empty when the text does not start with a heading
QString chapterBreakTitle(const QTextDocument& document, int from);

/// @brief Book > Chapter Break
///
/// The text from the cursor (from the start of the selection) to the end of a text of the
/// book goes to a new element of the same kind and status, right after it in its part (where
/// a new element of its kind goes after it: ProjectManager::newPlaceAfter()). The new text is
/// named after a heading at its start, else as a new element of its kind ("Chapter 4"); it
/// keeps its formatting, its notes and the annotations anchored in it. The changes of the
/// text are saved first, and the two texts are saved at once after it; the new one opens with
/// the cursor at its start. A notice under it says where the text went, with Undo: the new
/// text goes from the book with its file and the text is whole again, while neither of them
/// changed since.
class ChapterBreakCoordinator : public QObject {
    Q_OBJECT

public:
    /// @brief Id of the command in the registry
    static constexpr const char* COMMAND = "book.chapterBreak";

    /// @brief Says something under the text of a tab, with a button
    ///        (DocumentCoordinator::showNotice())
    using NoticeShower = std::function<void(EditorPanel* editor, const QString& message,
                                            const QString& actionText,
                                            const QString& actionToolTip,
                                            std::function<void()> action)>;

    /// @brief The text that has the keys (none: the keys are elsewhere)
    using TextWithKeys = std::function<editor::BookEditor*()>;

    /// @param centralTabs The tabs of the book window (DocumentWindows gives all the texts)
    /// @param navigator Saves the texts, shows the new one and closes its tabs again
    /// @param textWithKeys The text that has the keys
    /// @param showNotice Says under the new text where the text went, with Undo (none: the
    ///        status bar says it, without Undo)
    /// @param statusBar Where short messages go (may be null)
    /// @param window Over which the warnings open
    /// @param parent Parent object
    ChapterBreakCoordinator(QTabWidget* centralTabs, NavigatorCoordinator* navigator,
                            TextWithKeys textWithKeys, NoticeShower showNotice,
                            QStatusBar* statusBar, QWidget* window, QObject* parent = nullptr);
    ChapterBreakCoordinator(const ChapterBreakCoordinator&) = delete;
    ChapterBreakCoordinator& operator=(const ChapterBreakCoordinator&) = delete;

    /// @brief Give Book > Chapter Break its action and the reason why it is greyed out (its
    ///        state follows the keys: EditCoordinator::addTextCommands() with canBreak())
    void connectCommands();

    /// @brief Why @p text cannot be split at its cursor now (empty: it can)
    [[nodiscard]] QString whyNotBreak(const editor::BookEditor* text) const;

    /// @brief Whether @p text can be split at its cursor (for EditCoordinator)
    [[nodiscard]] bool canBreak(const editor::BookEditor& text) const {
        return whyNotBreak(&text).isEmpty();
    }

    /// @brief Book > Chapter Break in @p text at its cursor
    /// @return The id of the new element; empty when the text cannot be split there (the
    ///         status bar says why) or the book could not be changed (a warning says so)
    QString breakText(editor::BookEditor* text);

    /// @brief Whether the last Chapter Break can still be undone
    [[nodiscard]] bool canUndo() const;

    /// @brief Undo the last Chapter Break: the new text goes from the book with its file, and
    ///        the text it came from is whole again
    bool undo();

private:
    /// @brief The last Chapter Break
    struct Step {
        QString elementId;  ///< The text split
        QString title;      ///< Its title
        QString addedId;    ///< The new text
        /// The editors of the two texts; one of them changed or closed: the step is forgotten
        /// (forgetUndo()), so these are never left behind
        editor::BookEditor* text = nullptr;
        editor::BookEditor* added = nullptr;
    };

    /// @brief The tab of an editor of the book
    [[nodiscard]] EditorPanel* panelOf(const editor::BookEditor* text) const;

    /// @brief The editor of the open text of element @p elementId
    [[nodiscard]] editor::BookEditor* editorOf(const QString& elementId) const;

    /// @brief Forget the last Chapter Break (it cannot be undone any more)
    void forgetUndo();

    /// @brief Short message in the status bar
    void say(const QString& message) const;

    QTabWidget* m_centralTabs = nullptr;
    NavigatorCoordinator* m_navigator = nullptr;
    TextWithKeys m_textWithKeys;
    NoticeShower m_showNotice;
    QStatusBar* m_statusBar = nullptr;
    QWidget* m_window = nullptr;

    std::optional<Step> m_undo;
    QList<QMetaObject::Connection> m_undoWatch;
};

}  // namespace kalahari::gui
