/// @file book_search_coordinator.h
/// @brief Edit > Find in Book...: the search in all the texts of the book in the Search panel,
///        and Replace All in Book with Undo

#pragma once

#include "kalahari/editor/search_engine.h"
#include "kalahari/editor/text_footnotes.h"
#include "kalahari/gui/panels/search_panel.h"

#include <QElapsedTimer>
#include <QHash>
#include <QList>
#include <QMetaObject>
#include <QObject>
#include <QSet>
#include <QString>
#include <QTimer>

#include <functional>
#include <optional>
#include <vector>

class QStatusBar;
class QTabWidget;
class QWidget;

namespace kalahari::core {
class ProjectManager;
}

namespace kalahari::editor {
class BookEditor;
}

namespace kalahari::gui {

class CommandRegistry;
class EditorPanel;

/// @brief A match as the Search panel shows it: the line around it
/// @param line The paragraph, or the line of a note's text, the match is in
/// @param offset Where the match starts in @p line
/// @param length Its length
/// @param note The note it is in ("footnote 2"); empty: in the text
/// @return The context: the text before the match cut at a word with "…", the text after it
///         cut with "…", on one line
SearchPanelHit shownSearchHit(const QString& line, int offset, int length,
                              const QString& note = QString());

/// @brief Find in Book and Replace All in Book
///
/// Edit > Find in Book... (Ctrl+Shift+F) shows the Search panel with what is selected in the
/// text with the keys (else what its find bar searches for, with its options), and the panel
/// searches all the texts of the book in reading order: the open ones as their editors have
/// them, unsaved changes too, the others as the book keeps them, their files read again only
/// when they change. The texts are searched a few at a time, so that the program never stops
/// answering, and the matches of a text open in an editor follow its changes a moment later.
///
/// A match chosen in the panel shows in its text (opened if it is not). Replace All in Book
/// asks first, with the number of matches and of texts; the open texts change in their
/// editors, each as one step of its undo history, the texts not open in their files (unless
/// they have changes not saved yet: then the book keeps the new text with them). Before the
/// change the book keeps each text as a version, as before Replace All of the find bar, if the
/// settings say so. The notice under the text says how many matches changed, with Undo: the
/// open texts undo their step, the others get their text back. A later change of a text it
/// changed, or another book, makes it final.
class BookSearchCoordinator : public QObject {
    Q_OBJECT

public:
    /// @brief The text that has the keys (none: the keys are elsewhere)
    using TextWithKeys = std::function<editor::BookEditor*()>;

    /// @brief Opens a text of the book in its tab, as the Navigator opens it
    using ChapterOpener = std::function<void(const QString& elementId)>;

    /// @brief What the numbers of the notes of a text depend on outside it
    ///        (FootnoteCoordinator::contextFor())
    using NoteContexts = std::function<editor::FootnoteContext(const QString& elementId)>;

    /// @brief Shows the Search panel in front
    using PanelShower = std::function<void()>;

    /// @brief Says something under the text of a tab, with a button
    ///        (DocumentCoordinator::showNotice())
    using NoticeShower = std::function<void(EditorPanel* editor, const QString& message,
                                            const QString& actionText,
                                            const QString& actionToolTip,
                                            std::function<void()> action)>;

    /// @brief Answers whether to replace @p matches matches in @p texts texts (the window asks)
    using Confirmer = std::function<bool(int matches, int texts)>;

    /// @brief The most matches the panel lists; all of them are counted and replaced
    static constexpr int MAX_LISTED_MATCHES = 5000;

    /// @brief How long the search goes on before the program answers again, in milliseconds
    static constexpr int SLICE_MS = 15;

    /// @brief How long after a change of an open text its matches follow it, in milliseconds
    static constexpr int REFRESH_DELAY_MS = 500;

    /// @brief The matches of a text
    struct TextMatches {
        QString elementId;                         ///< The text
        QString title;                             ///< Its title
        std::vector<editor::SearchMatch> matches;  ///< All its matches, in text order
        QList<SearchPanelHit> hits;  ///< As the panel lists them: of the first matches
    };

    /// @param book The book of its window
    /// @param commands The commands of its window
    /// @param centralTabs The tabs of the book window (DocumentWindows gives all the texts)
    /// @param panel The Search panel
    /// @param showPanel Shows the panel in front
    /// @param textWithKeys The text that has the keys
    /// @param openChapter Opens a text of the book
    /// @param noteContexts The numbering of the notes of a text in the book
    /// @param showNotice Says under the text how many matches changed, with Undo (none: the
    ///        status bar says it, without Undo)
    /// @param statusBar Where short messages go (may be null)
    /// @param window Over which the window asking opens
    /// @param parent Parent object
    BookSearchCoordinator(core::ProjectManager& book, CommandRegistry& commands,
                          QTabWidget* centralTabs, SearchPanel* panel, PanelShower showPanel,
                          TextWithKeys textWithKeys, ChapterOpener openChapter,
                          NoteContexts noteContexts, NoticeShower showNotice,
                          QStatusBar* statusBar, QWidget* window, QObject* parent = nullptr);
    BookSearchCoordinator(const BookSearchCoordinator&) = delete;
    BookSearchCoordinator& operator=(const BookSearchCoordinator&) = delete;

    /// @brief Give Edit > Find in Book... its action and its state
    void connectCommands();

    /// @brief Edit > Find in Book...: the panel in front with what to find, the keys in its
    ///        field, the search
    void findInBook();

    /// @brief Search the book for what the panel says, from the first text
    void search();

    /// @brief Search again for what was searched for last (the texts of the book changed:
    ///        added, moved, removed); nothing without a search
    void searchAgain();

    /// @brief Whether texts are still to be searched
    [[nodiscard]] bool isSearching() const { return m_next < m_queue.size(); }

    /// @brief Search the texts left at once
    void finishSearch();

    /// @brief The texts with matches, in the order of the book (as the panel lists them)
    [[nodiscard]] const std::vector<TextMatches>& found() const { return m_found; }

    /// @brief Show a match in its text, opened if it is not
    ///
    /// A text changed since it was searched is searched again first, and the match with the
    /// same index shown (its last one when it has fewer now).
    /// @param text Its text: an index of found()
    /// @param match Its index there
    /// @param keys The keys to the text; false: they stay in the Search panel when they are there
    /// @return False when found() has no such match, or its text is gone or has no matches any
    ///         more
    bool goToMatch(int text, int match, bool keys);

    /// @brief Replace All in Book: every match of the search with what the panel says, after
    ///        the window asks
    /// @return How many matches were replaced; 0 when none or the writer said no; -1 when the
    ///         book cannot change
    int replaceAllInBook();

    /// @brief Why Replace All in Book cannot be done now (empty: it can)
    [[nodiscard]] QString whyNotReplace() const;

    /// @brief Whether the last Replace All in Book can still be undone
    [[nodiscard]] bool canUndo() const;

    /// @brief Undo the last Replace All in Book
    bool undo();

    /// @brief Replace the window that asks first (for the tests)
    void setConfirmer(Confirmer confirm) { m_confirm = std::move(confirm); }

signals:
    /// @brief The search went through all the texts (the panel shows what it found)
    void searchFinished();

private:
    /// @brief What is searched for
    struct Query {
        QString text;
        editor::SearchOptions options;
    };

    /// @brief The paragraphs of a text as they were last read
    struct ReadText {
        QString signature;     ///< Of its kept text or file (savedTextIfChanged()); empty: none
        bool current = false;  ///< Of an open text: read since its last change
        std::vector<editor::SearchableParagraph> paragraphs;
    };

    /// @brief A text to search
    struct QueuedText {
        QString elementId;
        QString title;
    };

    /// @brief A text it changed, kept so that it can be undone
    struct SavedText {
        QString elementId;
        QString kml;         ///< Its text before
        bool saved = false;  ///< Its file was saved with the change
    };

    /// @brief The last Replace All in Book
    struct Step {
        /// The open texts it changed; one of them changed again or closed: the step is
        /// forgotten (forgetUndo()), so these are never left behind
        QList<editor::BookEditor*> editors;
        QList<SavedText> savedTexts;  ///< The texts not open it changed
    };

    /// @brief The open texts of the book: their elements and editors
    [[nodiscard]] QHash<QString, editor::BookEditor*> openEditors() const;

    /// @brief The tab of an editor
    [[nodiscard]] EditorPanel* panelOf(const editor::BookEditor* editor) const;

    /// @brief The paragraphs of a text as they are now: of its editor, else as the book keeps it
    const std::vector<editor::SearchableParagraph>& paragraphsOf(const QString& elementId,
                                                                 editor::BookEditor* editor);

    /// @brief The matches of a text as it is now
    [[nodiscard]] TextMatches searchText(const QString& elementId, const QString& title,
                                         editor::BookEditor* editor);

    /// @brief Search the next texts, for SLICE_MS
    void searchSlice();

    /// @brief The matches of the texts changed since they were searched, as they are now
    void refreshChanged();

    /// @brief The panel shows the texts found and says what was found
    void publish();

    /// @brief Follow the changes of an open text
    void follow(editor::BookEditor* editor, const QString& elementId);

    /// @brief A text open in an editor changed
    void onTextChanged(const QString& elementId);

    /// @brief The color of the search's hits for the panel
    void updateMarkColor();

    /// @brief Forget the last Replace All in Book (it cannot be undone any more)
    void forgetUndo();

    /// @brief Ask whether to replace
    [[nodiscard]] bool confirm(int matches, int texts) const;

    /// @brief Short message in the status bar
    void say(const QString& message) const;

    /// @brief Forget what was found and read: another book, or none
    void reset();

    core::ProjectManager& m_book;  ///< The book of its window
    CommandRegistry& m_commands;   ///< The commands of its window
    QTabWidget* m_centralTabs = nullptr;
    SearchPanel* m_panel = nullptr;
    PanelShower m_showPanel;
    TextWithKeys m_textWithKeys;
    ChapterOpener m_openChapter;
    NoteContexts m_noteContexts;
    NoticeShower m_showNotice;
    QStatusBar* m_statusBar = nullptr;
    QWidget* m_window = nullptr;
    Confirmer m_confirm;

    editor::SearchEngine m_engine;            ///< Matches the query (without a document)
    std::optional<Query> m_query;             ///< What was searched for last; none: nothing
    QList<QueuedText> m_queue;                ///< The texts of the search
    qsizetype m_next = 0;                     ///< The next of them to search
    std::vector<TextMatches> m_found;         ///< The texts searched with matches
    QHash<QString, ReadText> m_read;          ///< The texts read, by element
    QSet<QString> m_changed;                  ///< The open texts changed since searched
    QTimer m_sliceTimer;                      ///< The next slice of the search
    QTimer m_refreshTimer;                    ///< refreshChanged() after a change
    QElapsedTimer m_published;                ///< Since the panel showed the matches
    QHash<const editor::BookEditor*, QMetaObject::Connection> m_followed;  ///< Their documents

    std::optional<Step> m_undo;
    QList<QMetaObject::Connection> m_undoWatch;
};

}  // namespace kalahari::gui
