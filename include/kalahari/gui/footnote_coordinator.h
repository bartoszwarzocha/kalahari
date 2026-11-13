/// @file footnote_coordinator.h
/// @brief Insert > Footnote and Endnote, and the frame a note's text is written in

#pragma once

#include "kalahari/editor/text_footnotes.h"

#include <QList>
#include <QObject>
#include <QString>

#include <functional>
#include <optional>

class QTabWidget;

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
class FootnoteCoordinator : public QObject {
    Q_OBJECT

public:
    /// @brief Gives the book's text with the keys (EditCoordinator::textWithKeys())
    using TextWithKeys = std::function<editor::BookEditor*()>;

    /// @param centralTabs The tabs of the book window (DocumentWindows gives all the texts)
    /// @param textWithKeys The text the commands work in
    /// @param parent Parent object
    FootnoteCoordinator(QTabWidget* centralTabs, TextWithKeys textWithKeys,
                        QObject* parent = nullptr);

    FootnoteCoordinator(const FootnoteCoordinator&) = delete;
    FootnoteCoordinator& operator=(const FootnoteCoordinator&) = delete;

    /// @brief Give Insert > Footnote and Endnote their callbacks (their states follow the keys:
    ///        EditCoordinator::addTextCommands())
    /// @note Call after the commands are registered
    void connectCommands();

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

    /// @brief Connect to the editors not connected yet
    void attachEditors();

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
    QList<editor::BookEditor*> m_attached;  ///< The editors whose signals it has
    std::optional<Writing> m_writing;
    NoteFrame* m_frame{nullptr};  ///< Destroyed with its editor (a closed tab): null again
};

}  // namespace kalahari::gui
