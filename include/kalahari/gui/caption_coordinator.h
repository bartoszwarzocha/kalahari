// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file caption_coordinator.h
/// @brief The numbered captions of the book: the numbers of every open text in the book around
///        it, Insert Caption, Insert Cross-Reference and Insert List of Figures

#pragma once

#include "kalahari/core/book_project.h"
#include "kalahari/editor/text_captions.h"

#include <QDateTime>
#include <QHash>
#include <QList>
#include <QObject>
#include <QString>
#include <QTimer>

#include <functional>
#include <optional>
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

class EditorPanel;

/// @brief Numbers the captions of the book and inserts captions, cross-references and lists of
///        figures
///
/// A text numbers its captions itself (editor::CaptionNumbers), from what the book around it
/// says (editor::CaptionContext): the captions of the texts before it and after it, the number
/// of its chapter and the numbering of each label. This coordinator gives every open text of
/// the book its context, from the open texts as their editors have them and the others as
/// saved, and gives it again when a text changes its captions (shortly after, so that typing
/// in a caption stays quick), when a text opens or closes and when the book's numbering
/// changes. A chapter is a text of the main kind (ProjectManager::mainTextKind()) in the main
/// part, numbered from 1 in reading order; the front and back parts have no chapter numbers.
/// A file outside the book (a standalone file) is numbered alone, as the Settings say.
///
/// The book keeps its labels and numbering (core::BookCaptions) from its first caption on;
/// until then the Settings give them (Editor > Images).
class CaptionCoordinator : public QObject {
    Q_OBJECT

public:
    /// @brief Opens a text of the book in its tab (NavigatorCoordinator)
    using ChapterOpener = std::function<void(const QString& elementId)>;

    /// @brief A text of the book with its captions
    struct BookText {
        QString elementId;
        QString title;
        int chapter = 0;            ///< The number of its chapter; 0: none
        bool chapterStart = false;  ///< It is a chapter: a count anew in each chapter starts
        bool backStart = false;     ///< The first text of the back part: a count anew starts
        std::vector<editor::CaptionEntry> entries;  ///< Its captions, in text order
        QHash<QString, int> references;             ///< Its cross-references, by caption
    };

    /// @param book The book of its window
    /// @param commands The commands of its window
    /// @param centralTabs The tabs of the book window (DocumentWindows gives all the texts)
    /// @param openChapter Opens a text of the book (Ctrl+click on a caption of another text)
    /// @param window Over which the windows open
    /// @param parent Parent object
    CaptionCoordinator(core::ProjectManager& book, CommandRegistry& commands,
                       QTabWidget* centralTabs, ChapterOpener openChapter, QWidget* window,
                       QObject* parent = nullptr);

    CaptionCoordinator(const CaptionCoordinator&) = delete;
    CaptionCoordinator& operator=(const CaptionCoordinator&) = delete;

    /// @brief Give Insert > Caption..., Cross-Reference... and List of Figures their callbacks
    /// @note Call after the commands are registered
    void connectCommands();

    // =========================================================================
    // The book
    // =========================================================================

    /// @brief The labels and the numbering of the book's captions (its own, else the Settings')
    [[nodiscard]] core::BookCaptions captions() const;

    /// @brief Keep labels and numbering for the book (from then on its own)
    /// @return false without a book or when it cannot keep them
    bool setCaptions(const core::BookCaptions& captions);

    /// @brief The texts of the book with their captions, in reading order: front, main, back
    [[nodiscard]] std::vector<BookText> bookTexts() const;

    /// @brief The context of a text of the book (its element) or of a text outside it
    [[nodiscard]] editor::CaptionContext contextFor(const QString& elementId) const;

    /// @brief How many cross-references of the book name the captions of these ids (the open
    ///        texts as their editors have them)
    [[nodiscard]] int referenceCount(const QStringList& ids) const;

    /// @brief Give every open text its context now
    void refresh();

    /// @brief Give the texts their contexts shortly (after a change of captions, a text
    ///        opened, the book's order of texts changed)
    void refreshSoon();

    // =========================================================================
    // The commands
    // =========================================================================

    /// @brief Insert > Caption... (Image > Insert Caption...): the window of the caption of the
    ///        image the editor in front acts on (BookEditor::captionTargetImage())
    void insertCaption();

    /// @brief The window of the caption of the image at @p position of @p editor
    /// @return Whether the caption changed
    bool editCaption(editor::BookEditor& editor, int position);

    /// @brief Insert > Cross-Reference...: the window of a cross-reference at the cursor
    void insertCrossReference();

    /// @brief Insert > List of Figures: the list at the cursor, under a heading
    void insertFigureList();

    /// @brief Caption Numbering...: the window of the numbering of @p label in the book
    /// @return Whether the numbering changed
    bool editNumbering(const QString& label, QWidget* parent);

    /// @brief Why the commands cannot be used in the text in front; empty: they can
    [[nodiscard]] QString whyNotInsert() const;

    /// @brief The editor of the text in front (null without one)
    [[nodiscard]] editor::BookEditor* editorInFront() const;

private:
    /// @brief A saved text's captions, as read from its file or its unsaved text
    struct CachedText {
        QString signature;  ///< The file's time and size, or the unsaved text's hash
        std::vector<editor::CaptionEntry> entries;
        QHash<QString, int> references;
    };

    /// @brief The editors of the open texts, by element (a text outside the book: empty)
    [[nodiscard]] QList<std::pair<QString, editor::BookEditor*>> openEditors() const;

    /// @brief Connect to the editors not connected yet
    void attachEditors();

    /// @brief The captions of a text not open: from its file or its unsaved text, kept until
    ///        they change
    const CachedText& savedText(const QString& elementId) const;

    /// @brief The context of the text at @p index of @p texts
    [[nodiscard]] editor::CaptionContext contextAt(const std::vector<BookText>& texts,
                                                   int index) const;

    /// @brief The book keeps the Settings' labels and numbering once it has its first caption
    void keepCaptionsOnce();

    /// @brief Asks before numbered captions go whose cross-references would show "?"
    bool confirmRemoval(const QStringList& ids);

    /// @brief Ctrl+click on a caption of another text: open it and go to the caption
    void openCaption(const QString& elementId, const QString& captionId);

    QTabWidget* m_centralTabs;
    ChapterOpener m_openChapter;
    QWidget* m_window;
    QTimer m_refreshTimer;
    /// The editors whose signals it has; an editor destroyed leaves the list
    QList<editor::BookEditor*> m_attached;
    mutable QHash<QString, CachedText> m_savedTexts;
    core::ProjectManager& m_book;  ///< The book of its window
    CommandRegistry& m_commands;  ///< The commands of its window
};

}  // namespace kalahari::gui
