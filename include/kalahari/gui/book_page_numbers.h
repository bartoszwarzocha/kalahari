// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file book_page_numbers.h
/// @brief The pages of the texts in the whole book: the number the first page of each text
///        shows and whether it is a left page

#ifndef KALAHARI_GUI_BOOK_PAGE_NUMBERS_H
#define KALAHARI_GUI_BOOK_PAGE_NUMBERS_H

#include "kalahari/core/page_start.h"

#include <QHash>
#include <QObject>
#include <QSet>
#include <QString>

#include <atomic>
#include <memory>
#include <optional>
#include <vector>

class QTabWidget;
class QTimer;

namespace kalahari::core {
class ProjectManager;
struct ProjectElement;
}  // namespace kalahari::core

namespace kalahari::gui {

class CommandRegistry;
class EditorPanel;

/// @brief The first page of a text in the book, as the editor shows it
struct TextFirstPage {
    int index = 0;      ///< Its page in its book (a volume of a series: in the volume), from 0
    int number = 1;     ///< The number it shows: index + 1, or 1 for each text on its own
    bool left = false;  ///< A left page (with mirror margins: an odd index)

    bool operator==(const TextFirstPage&) const = default;
};

/// @brief Counts the pages of the texts of the book as the editor lays them out (the Page
///        Layout view, with the settings of Settings > Editor) and gives each editor the
///        first page of its text: its number and its side
///
/// The texts go in reading order: the front part, the main part and the back part, each from
/// a new page. A part (a group) has a page of its own for its title, the table of contents
/// the pages of the titles. With mirror margins a text starts on the page its kind, the
/// settings ("editor.page.textsStart") or its properties say (core::PageStart): a right one
/// (an odd number) or a left one, a page before it left empty when needed, or the next one.
/// The pages are numbered through the book ("editor.page.numbering" "book"), or from 1 in
/// each text ("text"). Each volume of a series has its own pages.
///
/// The texts not open are counted from their files, one at a time while the program waits
/// for the writer, and only again when their files change; an open one from its editor a
/// moment after the writing stops. Only while an editor of the window shows the pages (the
/// Page Layout view): before that, a text starts on page 1 on the side its start says.
class BookPageNumbers : public QObject {
    Q_OBJECT

public:
    /// @brief How the pages are numbered: "book" (through the book) or "text" (each from 1)
    static constexpr const char* NUMBERING_KEY = "editor.page.numbering";
    static constexpr const char* NUMBERING_BOOK = "book";  ///< Through the whole book
    static constexpr const char* NUMBERING_TEXT = "text";  ///< Each text from 1

    /// @brief How long after a change the pages are counted again (ms)
    static constexpr int DELAY_MS = 1500;

    /// @param book The book of the window
    /// @param commands Its commands (the editor that counts the pages has its menus)
    /// @param centralTabs The tabs of the window: the editors of its texts there and in the
    ///                    windows of their own (DocumentWindows)
    BookPageNumbers(core::ProjectManager& book, CommandRegistry& commands,
                    QTabWidget* centralTabs, QObject* parent = nullptr);
    ~BookPageNumbers() override;

    BookPageNumbers(const BookPageNumbers&) = delete;
    BookPageNumbers& operator=(const BookPageNumbers&) = delete;
    BookPageNumbers(BookPageNumbers&&) = delete;
    BookPageNumbers& operator=(BookPageNumbers&&) = delete;

    /// @brief The first page of a text of the book; std::nullopt while its pages are not
    ///        counted, or for an element that is not a text of the book
    [[nodiscard]] std::optional<TextFirstPage> firstPageOf(const QString& elementId) const;

    /// @brief Give the editor of a text its first page: the one counted, else page 1 on the
    ///        side its start says
    void apply(EditorPanel& panel, const QString& elementId) const;

    /// @brief Count again in a moment: the book, a text or the settings changed
    void schedule();

    /// @brief Place the texts again at once with the pages counted (the start of a text
    ///        changed), and count the texts whose pages are not known in a moment
    void refresh();

    /// @brief Count every text now, at once, and give the editors their pages
    void countNow();

    /// @brief Whether texts are waiting to be counted
    [[nodiscard]] bool isCounting() const { return !m_waiting.empty(); }

signals:
    /// @brief The pages were counted again: the editors have their first pages
    void counted();

private slots:
    /// @brief The settings changed (called by name, queued)
    void onSettingsChanged();

private:
    /// @brief What a place of the book is: a text, the title of a part, the contents
    enum class UnitKind { Text, Group, Contents };

    /// @brief A place of the book that takes pages
    struct Unit {
        UnitKind kind = UnitKind::Text;
        QString elementId;
        QString title;
        core::PageStart start = core::PageStart::Right;  ///< Resolved: never Auto
        int volume = 0;  ///< Its book among the books of the project
    };

    /// @brief A unit waiting to be counted, with its text as it is now
    struct Waiting {
        size_t unit = 0;    ///< Its index among m_units
        QString kml;        ///< Its text (the contents: a line for each title)
        QString signature;  ///< What its text is now (Counted::signature)
    };

    /// @brief The pages of a text counted, and what its text was then
    struct Counted {
        QString signature;  ///< Its file's time and size, or the hash of its editor's text
        int pages = 1;
    };

    /// @brief The places of the books of the project in reading order
    [[nodiscard]] std::vector<Unit> unitsOfBook() const;
    void addUnits(const QList<core::ProjectElement>& elements, int volume,
                  std::vector<Unit>& units) const;

    /// @brief The editor of a text in the window, if it is open
    [[nodiscard]] EditorPanel* openEditor(const QString& elementId) const;

    /// @brief Whether an editor of the window shows the pages (the Page Layout view)
    [[nodiscard]] bool showsPages() const;

    /// @brief Whether the pages of a unit are known as its text is now; when not, it waits
    bool isCountedNow(size_t index);

    /// @brief The KML of a table of contents: a line for each title of its book
    [[nodiscard]] QString contentsKml(const Unit& contents) const;

    /// @brief Count the next unit waiting (m_step); place and apply when none is left
    void countNext();
    void countOne();

    /// @brief The pages of a text in the counting editor
    int countPages(const QString& elementId, const QString& kml);

    /// @brief The texts on the pages of their books, and the editors given their first pages
    void place();
    void applyAll() const;
    void finish();

    /// @brief Follow the changes of the text of an open editor (once)
    void follow(EditorPanel* panel);

    QTabWidget* m_centralTabs;
    QTimer* m_delay{nullptr};  ///< Count again a moment after a change
    QTimer* m_step{nullptr};   ///< The next text waiting, while the program waits
    std::unique_ptr<EditorPanel> m_counter;  ///< Lays out the texts as the editors do
    std::vector<Unit> m_units;               ///< The places of the books, as counted last
    std::vector<Waiting> m_waiting;          ///< Units waiting to be counted, in order
    QHash<QString, Counted> m_counted;       ///< The pages of the texts by element id
    QHash<QString, TextFirstPage> m_firstPages;  ///< The first pages by element id
    QSet<const EditorPanel*> m_followed;     ///< Editors whose changes are followed
    std::atomic<bool> m_settingsPending{false};
    std::atomic<bool> m_layoutChanged{false};  ///< A setting of the layout changed
    int m_settingsListener{0};

    core::ProjectManager& m_book;
    CommandRegistry& m_commands;
};

}  // namespace kalahari::gui

#endif  // KALAHARI_GUI_BOOK_PAGE_NUMBERS_H
