/// @file footnote_coordinator.h
/// @brief Insert > Footnote and Endnote, the frame a note's text is written in, and the
///        numbers of the notes of every open text in the book around it

#pragma once

#include "kalahari/core/book_project.h"
#include "kalahari/editor/text_footnotes.h"

#include <QHash>
#include <QList>
#include <QObject>
#include <QString>
#include <QTimer>

#include <functional>
#include <optional>
#include <utility>
#include <vector>

class QTabWidget;
class QWidget;

namespace kalahari::core {
class ProjectManager;
}  // namespace kalahari::core
namespace kalahari::gui {
class CommandRegistry;
}  // namespace kalahari::gui

namespace kalahari::editor {
class BookEditor;
}

namespace kalahari::gui {

class NoteFrame;

/// @brief Inserts footnotes and endnotes and opens the frame their texts are written in
///
/// Insert > Footnote (Alt+Shift+F) and Endnote (Alt+Shift+E) put a new note's number at the
/// cursor of the text with the keys and open its frame at the place of the note's text: under
/// its paragraph in the continuous views, at the bottom of its page in the page view (as in
/// the mockup approved on 12.10.2026). Ctrl+Enter keeps the text, as one undo step with the
/// number; Esc or the frame's X on a new note takes the number away (not to be redone), on a
/// note edited again drops the changes. Going anywhere else keeps the text. A click on a
/// note's text, a double click on its number and Edit Footnote in its menu open the frame of
/// the note. A note being written is kept before the documents are saved or closed
/// (finishWriting()).
///
/// A text numbers its notes itself (editor::FootnoteNumbers), from what the book around it
/// says (editor::FootnoteContext): the formats of the book (core::BookFootnotes), the notes
/// of the texts before it when the book numbers them through, and the endnotes of the other
/// texts when they are at the end of the book: the editor shows them after the last paragraph
/// of the last chapter (a text of the main kind in the main part; without one, of the last
/// text). This coordinator gives every open text of the book its context, from the open texts
/// as their editors have them and the others as saved, and gives it again when a text changes
/// its notes (shortly after), when a text opens or closes and when the book's settings change
/// (Insert > Footnote Layout...). A file outside the book is numbered alone: 1, 2, 3 and i,
/// ii, iii.
class FootnoteCoordinator : public QObject {
    Q_OBJECT

public:
    /// @brief Gives the book's text with the keys (EditCoordinator::textWithKeys())
    using TextWithKeys = std::function<editor::BookEditor*()>;

    /// @brief Opens a text of the book in its tab (NavigatorCoordinator)
    using ChapterOpener = std::function<void(const QString& elementId)>;

    /// @brief A text of the book with its notes
    struct BookText {
        QString elementId;
        bool chapter = false;                 ///< Of the main kind, in the main part
        std::vector<editor::Footnote> notes;  ///< In text order
    };

    /// @param book The book of its window
    /// @param commands The commands of its window
    /// @param centralTabs The tabs of the book window (DocumentWindows gives all the texts)
    /// @param textWithKeys The text the commands work in
    /// @param openChapter Opens a text of the book (a click on its endnote at the end of the
    ///        book)
    /// @param window Over which the windows open
    /// @param parent Parent object
    FootnoteCoordinator(core::ProjectManager& book, CommandRegistry& commands,
                        QTabWidget* centralTabs, TextWithKeys textWithKeys,
                        ChapterOpener openChapter = {}, QWidget* window = nullptr,
                        QObject* parent = nullptr);

    FootnoteCoordinator(const FootnoteCoordinator&) = delete;
    FootnoteCoordinator& operator=(const FootnoteCoordinator&) = delete;

    /// @brief Give Insert > Footnote and Endnote their callbacks (their states follow the keys:
    ///        EditCoordinator::addTextCommands()), and Insert > Footnote Layout... its own
    /// @note Call after the commands are registered
    void connectCommands();

    // =========================================================================
    // The book
    // =========================================================================

    /// @brief How the book numbers its notes and where its endnotes are (its own, else 1, 2, 3
    ///        and i, ii, iii anew in each chapter, the endnotes after each one)
    [[nodiscard]] core::BookFootnotes footnotes() const;

    /// @brief Keep the numbering and the place of the endnotes for the book
    /// @return false without a book or when it cannot keep them (read-only)
    bool setFootnotes(const core::BookFootnotes& footnotes);

    /// @brief The texts of the book with their notes, in reading order: front, main, back
    [[nodiscard]] std::vector<BookText> bookTexts() const;

    /// @brief The context of a text of the book (its element) or of a text outside it (empty)
    [[nodiscard]] editor::FootnoteContext contextFor(const QString& elementId) const;

    /// @brief Give every open text its context now
    void refresh();

    /// @brief Give the texts their contexts shortly (after a change of notes, a text opened,
    ///        the book's order of texts changed)
    void refreshSoon();

    /// @brief Insert > Footnote Layout...: the window of the book's numbering of notes
    /// @return Whether the settings changed
    bool editSettings();

    /// @brief Why Footnote Layout... cannot be used: no book, or a read-only one; empty: it
    ///        can
    [[nodiscard]] QString whyNotEditSettings() const;

    /// @brief Insert > Footnote, Endnote: a new note at the cursor of the text with the keys,
    ///        its frame open
    /// @return Whether a note was inserted
    bool insertNote(editor::NoteKind kind);

    /// @brief Open the frame of a note of a text
    /// @return false without the note, or in a read-only text
    bool editNote(editor::BookEditor& editor, const QString& id);

    /// @brief The note being written is kept and its frame closed: before the documents are
    ///        saved or closed
    /// @param editor Only a note of this text; nullptr: of any
    void finishWriting(const editor::BookEditor* editor = nullptr);

    /// @brief The open frame (nullptr: none)
    NoteFrame* frame() const { return m_frame; }

private:
    /// @brief The note written in the frame
    struct Writing {
        /// The frame is the editor's child: the editor going takes the frame, and the writing
        /// with it (not a QPointer: clang-analyzer reports a false use after free in its
        /// reference counting)
        editor::BookEditor* editor = nullptr;
        QString id;
        bool isNew = false;  ///< Inserted for the frame: Esc takes it away
    };

    /// @brief A saved text's notes, as read from its file or its unsaved text
    struct CachedText {
        QString signature;  ///< The file's time and size, or the unsaved text's hash
        std::vector<editor::Footnote> notes;
    };

    /// @brief The editors of the open texts, by element (a text outside the book: empty)
    [[nodiscard]] QList<std::pair<QString, editor::BookEditor*>> openEditors() const;

    /// @brief Connect to the editors not connected yet
    void attachEditors();

    /// @brief The notes of a text not open: from its file or its unsaved text, kept until they
    ///        change
    const CachedText& savedText(const QString& elementId) const;

    /// @brief The context of the text at @p index of @p texts (-1: a text of the book outside
    ///        its parts, numbered alone in the book's formats)
    [[nodiscard]] static editor::FootnoteContext contextAt(const std::vector<BookText>& texts,
                                                           int index,
                                                           const core::BookFootnotes& footnotes);

    /// @brief A click on an endnote of another text at the end of the book: open that text and
    ///        the note's frame (in a read-only text: the cursor after its number)
    void openBookEndnote(const QString& elementId, const QString& id);

    /// @brief Open the frame of a note
    void openFrame(const Writing& writing);

    /// @brief Ctrl+Enter, or the program elsewhere: the text kept
    void saveWriting();

    /// @brief Esc or the X: a new note goes, an edited one stays as it was
    void cancelWriting();

    /// @brief The frame goes, the keys back to the text
    void closeFrame();

    /// @brief The frame in the colors of the paper and the note's frame
    void colorFrame();

    QTabWidget* m_centralTabs;
    TextWithKeys m_textWithKeys;
    ChapterOpener m_openChapter;
    QWidget* m_window;
    QTimer m_refreshTimer;
    mutable QHash<QString, CachedText> m_savedTexts;
    QList<editor::BookEditor*> m_attached;  ///< The editors whose signals it has
    std::optional<Writing> m_writing;
    NoteFrame* m_frame{nullptr};  ///< Destroyed with its editor (a closed tab): null again
    core::ProjectManager& m_book;  ///< The book of its window
    CommandRegistry& m_commands;  ///< The commands of its window
};

}  // namespace kalahari::gui
