// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file book_page_numbers.cpp
/// @brief The pages of the texts in the whole book: the number the first page of each text
///        shows and whether it is a left page

#include "kalahari/gui/book_page_numbers.h"

#include "kalahari/core/book_project.h"
#include "kalahari/core/book_type_package.h"
#include "kalahari/core/logger.h"
#include "kalahari/core/page_start.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/editor/kalahari_text_document_layout.h"
#include "kalahari/editor/view_modes.h"
#include "kalahari/gui/book_texts.h"
#include "kalahari/gui/document_windows.h"
#include "kalahari/gui/panels/editor_panel.h"

#include <QHashFunctions>
#include <QTabWidget>
#include <QTextDocument>
#include <QTimer>

#include <algorithm>
#include <array>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace kalahari::gui {

namespace {

/// @brief The kind of the table of contents of a book
constexpr const char* CONTENTS_KIND = "toc";

/// @brief The settings that lay the text out otherwise: the pages are counted again
constexpr std::array<std::string_view, 12> LAYOUT_KEYS = {
    "editor.fontFamily",       "editor.fontSize",         "editor.lineHeight",
    "editor.paragraphSpacing", "editor.firstLineIndent",  "editor.indentSize",
    "editor.tabStops",         "editor.tabSize",          "editor.margins.",
    "editor.page.size",        "editor.page.custom",      "editor.sceneBreak."};

/// @brief Whether a setting lays the text out otherwise
bool isLayoutKey(const std::string& key) {
    return std::any_of(LAYOUT_KEYS.cbegin(), LAYOUT_KEYS.cend(),
                       [&key](std::string_view start) { return key.starts_with(start); });
}

/// @brief Whether a setting places the texts otherwise: their numbers, their starts
bool isPlacingKey(const std::string& key) {
    return key == BookPageNumbers::NUMBERING_KEY || key == core::TEXTS_START_KEY;
}

/// @brief What a text is now, by its hash: the same text, the same pages
QString signatureOf(const QString& prefix, const QString& text) {
    return QStringLiteral("%1:%2:%3").arg(prefix).arg(qHash(text)).arg(text.size());
}

}  // namespace

BookPageNumbers::BookPageNumbers(core::ProjectManager& book, CommandRegistry& commands,
                                 QTabWidget* centralTabs, QObject* parent)
    : QObject(parent),
      m_centralTabs(centralTabs),
      m_delay(new QTimer(this)),
      m_step(new QTimer(this)),
      m_book(book),
      m_commands(commands) {
    m_delay->setSingleShot(true);
    m_delay->setInterval(DELAY_MS);
    connect(m_delay, &QTimer::timeout, this, &BookPageNumbers::refresh);
    // One text at a time, while the program waits for the writer
    m_step->setSingleShot(true);
    m_step->setInterval(0);
    connect(m_step, &QTimer::timeout, this, &BookPageNumbers::countNext);

    // Another book, its parts or their order, its styles
    connect(&m_book, &core::ProjectManager::projectOpened, this, [this]() {
        m_counted.clear();
        schedule();
    });
    connect(&m_book, &core::ProjectManager::projectClosed, this, [this]() {
        m_delay->stop();
        m_step->stop();
        m_units.clear();
        m_waiting.clear();
        m_counted.clear();
        m_firstPages.clear();
    });
    connect(&m_book, &core::ProjectManager::projectChanged, this, &BookPageNumbers::schedule);
    connect(&m_book, &core::ProjectManager::paragraphStylesChanged, this, [this]() {
        m_counted.clear();
        schedule();
    });

    // A text opened in the window gets its page; its writing counts it again
    if (m_centralTabs != nullptr) {
        connect(&DocumentWindows::of(*m_centralTabs), &DocumentWindows::frontChanged, this,
                [this](QWidget* page) {
                    if (auto* panel = qobject_cast<EditorPanel*>(page)) {
                        follow(panel);
                        const QString elementId = panel->property("elementId").toString();
                        apply(*panel, elementId);
                        if (!firstPageOf(elementId)) {
                            schedule();  // the first text whose pages show, maybe
                        }
                    }
                });
    }

    // The settings, wherever they change: once for all those written together
    m_settingsListener =
        core::SettingsManager::getInstance().subscribe([this](const std::string& key) {
            const bool layout = isLayoutKey(key);
            if (!layout && !isPlacingKey(key)) {
                return;
            }
            if (layout) {
                m_layoutChanged = true;
            }
            if (!m_settingsPending.exchange(true)) {
                QMetaObject::invokeMethod(this, "onSettingsChanged", Qt::QueuedConnection);
            }
        });
}

BookPageNumbers::~BookPageNumbers() {
    core::SettingsManager::getInstance().unsubscribe(m_settingsListener);
}

std::optional<TextFirstPage> BookPageNumbers::firstPageOf(const QString& elementId) const {
    const auto found = m_firstPages.constFind(elementId);
    if (found == m_firstPages.constEnd()) {
        return std::nullopt;
    }
    return found.value();
}

void BookPageNumbers::apply(EditorPanel& panel, const QString& elementId) const {
    if (const std::optional<TextFirstPage> first = firstPageOf(elementId)) {
        panel.setFirstPage(first->number, first->left);
        return;
    }
    // Not counted yet, or no text of the book: page 1, on the side its start says
    const core::ProjectElement* element =
        elementId.isEmpty() ? nullptr : m_book.findElement(elementId);
    panel.setFirstPage(1, element != nullptr &&
                              m_book.pageStartOf(*element) == core::PageStart::Left);
}

void BookPageNumbers::schedule() {
    if (m_book.project() != nullptr) {
        m_delay->start();
    }
}

void BookPageNumbers::onSettingsChanged() {
    m_settingsPending = false;
    if (m_layoutChanged.exchange(false)) {
        m_counted.clear();
    }
    // After the editors (the counting one too) took the settings: they are queued as well
    schedule();
}

void BookPageNumbers::refresh() {
    m_delay->stop();
    m_step->stop();
    m_waiting.clear();
    // The pages show in the Page Layout view only: counted when an editor shows them
    if (!showsPages()) {
        m_firstPages.clear();
        return;
    }
    m_units = unitsOfBook();
    bool known = true;
    for (size_t index = 0; index < m_units.size(); ++index) {
        known = m_counted.contains(m_units[index].elementId) && known;
        isCountedNow(index);
    }
    if (m_waiting.empty()) {
        finish();
        return;
    }
    // The pages counted before, until the changed texts are counted: a start changed shows
    // at once
    if (known) {
        place();
        applyAll();
    }
    m_step->start();
}

void BookPageNumbers::countNow() {
    refresh();
    if (m_waiting.empty()) {
        return;
    }
    m_step->stop();
    while (!m_waiting.empty()) {
        countOne();
    }
    finish();
}

// =============================================================================
// The places of the book
// =============================================================================

std::vector<BookPageNumbers::Unit> BookPageNumbers::unitsOfBook() const {
    std::vector<Unit> units;
    const core::BookProject* project = m_book.project();
    if (project == nullptr) {
        return units;
    }
    for (int volume = 0; volume < project->books.size(); ++volume) {
        const core::ProjectBook& book = project->books.at(volume);
        addUnits(book.frontElements, volume, units);
        addUnits(book.mainElements, volume, units);
        addUnits(book.backElements, volume, units);
    }
    return units;
}

void BookPageNumbers::addUnits(const QList<core::ProjectElement>& elements, int volume,
                               std::vector<Unit>& units) const {
    // As the book goes to print (BookExportCoordinator): a part with its title when it has a
    // text, the texts with their files, the tables of contents. The parts are walked with a
    // stack: a part's unit goes in first and is taken out when none of its elements has one
    struct Level {
        const QList<core::ProjectElement>* elements = nullptr;
        qsizetype next = 0;
        size_t partAt = 0;  ///< The index of the part's unit in units
    };
    std::vector<Level> levels{Level{&elements, 0, units.size()}};
    while (!levels.empty()) {
        Level& level = levels.back();
        if (level.next == level.elements->size()) {
            const size_t partAt = level.partAt;
            levels.pop_back();
            if (!levels.empty() && units.size() == partAt + 1) {
                units.pop_back();
            }
            continue;
        }
        const core::ProjectElement& element = level.elements->at(level.next++);
        const core::ElementForm form = m_book.formOf(element);
        if (form == core::ElementForm::Group) {
            levels.push_back(Level{&element.elements, 0, units.size()});
            units.push_back(Unit{UnitKind::Group, element.id, element.title,
                                 m_book.pageStartOf(element), volume});
        } else if (form == core::ElementForm::Text && !element.file.isEmpty()) {
            units.push_back(Unit{UnitKind::Text, element.id, element.title,
                                 m_book.pageStartOf(element), volume});
        } else if (form == core::ElementForm::Window &&
                   element.kind.kindId == QLatin1String(CONTENTS_KIND)) {
            units.push_back(Unit{UnitKind::Contents, element.id, element.title,
                                 m_book.pageStartOf(element), volume});
        }
    }
}

bool BookPageNumbers::showsPages() const {
    if (m_centralTabs == nullptr || m_book.project() == nullptr) {
        return false;
    }
    const QList<QWidget*> pages = DocumentWindows::of(*m_centralTabs).pages();
    return std::any_of(pages.cbegin(), pages.cend(), [](QWidget* page) {
        const auto* panel = qobject_cast<EditorPanel*>(page);
        return panel != nullptr &&
               panel->getBookEditor()->viewMode() == editor::ViewMode::Page;
    });
}

EditorPanel* BookPageNumbers::openEditor(const QString& elementId) const {
    if (m_centralTabs == nullptr || elementId.isEmpty()) {
        return nullptr;
    }
    for (QWidget* page : DocumentWindows::of(*m_centralTabs).pages()) {
        auto* panel = qobject_cast<EditorPanel*>(page);
        if (panel != nullptr && panel->property("elementId").toString() == elementId) {
            return panel;
        }
    }
    return nullptr;
}

QString BookPageNumbers::contentsKml(const Unit& contents) const {
    // Its title, then a line for each title of its book
    QString kml = QStringLiteral("<kml><p>") + contents.title.toHtmlEscaped() +
                  QStringLiteral("</p>");
    for (const Unit& unit : m_units) {
        if (unit.volume == contents.volume && unit.kind != UnitKind::Contents) {
            kml += QStringLiteral("<p>") + unit.title.toHtmlEscaped() + QStringLiteral("</p>");
        }
    }
    return kml + QStringLiteral("</kml>");
}

bool BookPageNumbers::isCountedNow(size_t index) {
    const Unit& unit = m_units[index];
    if (unit.kind == UnitKind::Group) {
        m_counted.insert(unit.elementId, Counted{QString(), 1});  // the page of its title
        return true;
    }
    const auto known = m_counted.constFind(unit.elementId);
    QString signature = known != m_counted.constEnd() ? known.value().signature : QString();
    std::optional<QString> kml;
    if (unit.kind == UnitKind::Contents) {
        const QString titles = contentsKml(unit);
        const QString now = signatureOf(QStringLiteral("titles"), titles);
        if (now != signature) {
            kml = titles;
            signature = now;
        }
    } else if (const EditorPanel* panel = openEditor(unit.elementId)) {
        // Its editor: the text as it is written
        const QString text = panel->getBookEditor()->toKml();
        const QString now = signatureOf(QStringLiteral("open"), text);
        if (now != signature) {
            kml = text;
            signature = now;
        }
    } else {
        kml = savedTextIfChanged(m_book, unit.elementId, signature);
    }
    if (!kml) {
        return true;
    }
    m_waiting.push_back(Waiting{index, *kml, signature});
    return false;
}

// =============================================================================
// Counting
// =============================================================================

void BookPageNumbers::countNext() {
    if (!m_waiting.empty()) {
        countOne();
    }
    if (m_waiting.empty()) {
        finish();
    } else {
        m_step->start();
    }
}

void BookPageNumbers::countOne() {
    const Waiting next = m_waiting.front();
    m_waiting.erase(m_waiting.begin());
    const Unit& unit = m_units[next.unit];
    m_counted.insert(unit.elementId, Counted{next.signature, countPages(unit.elementId, next.kml)});
}

int BookPageNumbers::countPages(const QString& elementId, const QString& kml) {
    // An editor as those of the window: the settings of the editors, the styles and the
    // images of the book, the Page Layout view; never shown
    if (!m_counter) {
        m_counter = std::make_unique<EditorPanel>(m_book, m_commands);
        m_counter->getBookEditor()->setViewMode(editor::ViewMode::Page);
    }
    m_counter->setProperty("elementId", elementId);
    m_counter->setImagesFolder(m_book.imagesBaseFolder());
    m_counter->setContent(kml);
    editor::BookEditor* editor = m_counter->getBookEditor();
    if (auto* layout = qobject_cast<editor::KalahariTextDocumentLayout*>(
            editor->textDocument() != nullptr ? editor->textDocument()->documentLayout()
                                              : nullptr)) {
        layout->layoutPendingBlocks();  // the heights of all its lines, none estimated
    }
    return std::max(1, editor->totalPages());
}

// =============================================================================
// The pages
// =============================================================================

void BookPageNumbers::place() {
    const auto& settings = core::SettingsManager::getInstance();
    const bool mirrored = settings.get<bool>("editor.margins.mirrorEnabled");
    const bool throughBook =
        settings.get<std::string>(NUMBERING_KEY, NUMBERING_BOOK) != NUMBERING_TEXT;

    // Each unit from a new page; with mirror margins on the side its start says, a page left
    // empty before it when needed (as BookPages lays out the book), except the first one
    m_firstPages.clear();
    int volume = -1;
    int index = 0;
    for (const Unit& unit : m_units) {
        if (unit.volume != volume) {
            volume = unit.volume;
            index = 0;
        } else if (mirrored && unit.start != core::PageStart::Next &&
                   (index % 2 == 1) != (unit.start == core::PageStart::Left)) {
            ++index;
        }
        m_firstPages.insert(unit.elementId,
                            TextFirstPage{index, throughBook ? index + 1 : 1,
                                          mirrored && index % 2 == 1});
        const auto counted = m_counted.constFind(unit.elementId);
        index += counted != m_counted.constEnd() ? counted.value().pages : 1;
    }
}

void BookPageNumbers::applyAll() const {
    if (m_centralTabs == nullptr) {
        return;
    }
    for (QWidget* page : DocumentWindows::of(*m_centralTabs).pages()) {
        if (auto* panel = qobject_cast<EditorPanel*>(page)) {
            apply(*panel, panel->property("elementId").toString());
        }
    }
}

void BookPageNumbers::finish() {
    place();
    applyAll();
    core::Logger::getInstance().debug("BookPageNumbers: {} places of the book counted",
                                      m_units.size());
    emit counted();
}

void BookPageNumbers::follow(EditorPanel* panel) {
    if (panel == nullptr || m_followed.contains(panel)) {
        return;
    }
    m_followed.insert(panel);
    connect(panel, &EditorPanel::contentChanged, this, &BookPageNumbers::schedule);
    // The Page Layout view: its pages at once
    connect(panel->getBookEditor(), &editor::BookEditor::viewModeChanged, this,
            [this](editor::ViewMode mode) {
                if (mode == editor::ViewMode::Page) {
                    refresh();
                }
            });
    connect(panel, &QObject::destroyed, this, [this, panel]() {
        m_followed.remove(panel);
        schedule();  // closed: its file, maybe without its last changes
    });
}

}  // namespace kalahari::gui
