/// @file test_annotations_panel.cpp
/// @brief The Annotations panel, the frame an annotation is written in, and the commands of
/// the annotations

#include <catch2/catch_test_macros.hpp>
#include "../editor/editor_test_utils.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/core/theme.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/editor/editor_appearance.h"
#include "kalahari/gui/annotations_coordinator.h"
#include "kalahari/gui/command_registrar.h"
#include "kalahari/gui/command_registry.h"
#include "kalahari/gui/panels/annotation_card.h"
#include "kalahari/gui/panels/annotation_colors.h"
#include "kalahari/gui/panels/annotation_entry.h"
#include "kalahari/gui/panels/annotation_frame.h"
#include "kalahari/gui/panels/annotations_panel.h"
#include "kalahari/gui/panels/editor_panel.h"

#include <QAction>
#include <QApplication>
#include <QDateTime>
#include <QDockWidget>
#include <QFile>
#include <QGuiApplication>
#include <QHelpEvent>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QMouseEvent>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QTabWidget>
#include <QTextDocument>
#include <QTextEdit>
#include <QTimeZone>
#include <QToolButton>
#include <QToolTip>

#include <nlohmann/json.hpp>

#include <array>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

using namespace kalahari;
using namespace kalahari::gui;
using editor::AnnotationKind;

namespace {

constexpr std::array<AnnotationKind, 3> KINDS = {AnnotationKind::Comment, AnnotationKind::Todo,
                                                 AnnotationKind::Note};

/// An entry of an annotation in a chapter
AnnotationEntry entryOf(const QString& id, AnnotationKind kind, const QString& text,
                        int chapterOrder = 0, int textOrder = 0) {
    AnnotationEntry entry;
    entry.annotation.id = id;
    entry.annotation.kind = kind;
    entry.annotation.text = text;
    entry.elementId = QStringLiteral("ch-%1").arg(chapterOrder);
    entry.chapterTitle = QStringLiteral("Chapter %1").arg(chapterOrder);
    entry.chapterOrder = chapterOrder;
    entry.textOrder = textOrder;
    return entry;
}

/// The ids of entries, in their order
QString idsOf(const std::vector<AnnotationEntry>& entries) {
    QStringList ids;
    for (const AnnotationEntry& entry : entries) {
        ids << entry.annotation.id;
    }
    return ids.join(QLatin1Char(' '));
}

/// An <annotation> element of a chapter's KML
QString record(const QString& id, const QString& kind, const QString& text) {
    return QStringLiteral("<annotation id=\"%1\" kind=\"%2\">%3</annotation>").arg(id, kind, text);
}

/// A chapter's KML with these annotations and paragraphs
QString kmlWith(const QString& annotations, const QStringList& paragraphs) {
    QString kml = QStringLiteral("<kml><annotations>") + annotations + QStringLiteral("</annotations>");
    for (const QString& paragraph : paragraphs) {
        kml += QStringLiteral("<p>") + paragraph + QStringLiteral("</p>");
    }
    return kml + QStringLiteral("</kml>");
}

/// A key press sent to a widget (and on to its parents, as long as none takes it)
void press(QWidget* widget, int key, Qt::KeyboardModifiers modifiers = Qt::NoModifier,
           const QString& text = QString()) {
    QKeyEvent event(QEvent::KeyPress, key, modifiers, text);
    QApplication::sendEvent(widget, &event);
}

/// Whether a widget with the keys keeps a key from the window's shortcuts
bool keepsFromShortcuts(QWidget* widget, int key, Qt::KeyboardModifiers modifiers) {
    QKeyEvent event(QEvent::ShortcutOverride, key, modifiers);
    event.ignore();  // as the shortcuts ask
    QApplication::sendEvent(widget, &event);
    return event.isAccepted();
}

/// The widget of the panel's list of cards, which has the keys of the list
QWidget* listOf(const AnnotationsPanel& panel) {
    return panel.findChild<QScrollArea*>()->widget();
}

/// A theme of the application, as its file has it
core::Theme themeNamed(const QString& name) {
    QFile file(QStringLiteral(KALAHARI_SOURCE_DIR "/resources/themes/%1.json").arg(name));
    REQUIRE(file.open(QIODevice::ReadOnly));
    return core::Theme::fromJson(nlohmann::json::parse(file.readAll().toStdString()));
}

/// Counts the keys that reach a widget
struct KeyCounter : QObject {
    int keys = 0;
    bool eventFilter(QObject* watched, QEvent* event) override {
        if (event->type() == QEvent::KeyPress) {
            ++keys;
        }
        return QObject::eventFilter(watched, event);
    }
};

/// Counts the presses of a mouse button that reach a widget
struct PressCounter : QObject {
    int presses = 0;
    bool eventFilter(QObject* watched, QEvent* event) override {
        if (event->type() == QEvent::MouseButtonPress) {
            ++presses;
        }
        return QObject::eventFilter(watched, event);
    }
};

/// A click of a mouse button, the left one unless told, on a widget (and on to its parents,
/// as long as none takes it)
void click(QWidget* widget, const QPointF& pos, Qt::MouseButton button = Qt::LeftButton) {
    const QPointF globalPos = widget->mapToGlobal(pos);
    QMouseEvent press(QEvent::MouseButtonPress, pos, globalPos, button, button, Qt::NoModifier);
    QApplication::sendEvent(widget, &press);
    QMouseEvent release(QEvent::MouseButtonRelease, pos, globalPos, button, Qt::NoButton,
                        Qt::NoModifier);
    QApplication::sendEvent(widget, &release);
}

/// The second click of a double click: its press comes as a double click
void secondClick(QWidget* widget, const QPointF& pos) {
    const QPointF globalPos = widget->mapToGlobal(pos);
    QMouseEvent press(QEvent::MouseButtonDblClick, pos, globalPos, Qt::LeftButton, Qt::LeftButton,
                      Qt::NoModifier);
    QApplication::sendEvent(widget, &press);
    QMouseEvent release(QEvent::MouseButtonRelease, pos, globalPos, Qt::LeftButton, Qt::NoButton,
                        Qt::NoModifier);
    QApplication::sendEvent(widget, &release);
}

/// A document outside the book in a tab, the Annotations panel and their coordinator
struct Desk {
    QTabWidget tabs;
    EditorPanel* document;
    AnnotationsPanel panel;
    std::unique_ptr<AnnotationsCoordinator> coordinator;

    explicit Desk(const QString& kml)
        : document(new EditorPanel()) {
        document->setContent(kml);
        tabs.addTab(document, QStringLiteral("Notes"));
        panel.show();  // the panel lists the annotations while it can be seen
        coordinator =
            std::make_unique<AnnotationsCoordinator>(&panel, nullptr, &tabs, nullptr, nullptr);
    }

    editor::BookEditor& editor() { return *document->getBookEditor(); }

    /// The annotation with this id (none: an annotation without an id)
    editor::Annotation annotation(const QString& id) {
        for (const editor::AnnotationPlace& place : editor().annotations()) {
            if (place.annotation.id == id) {
                return place.annotation;
            }
        }
        return {};
    }

    /// The listed entry of the annotation with this id
    AnnotationEntry entry(const QString& id) {
        for (const AnnotationEntry& listed : panel.entries()) {
            if (listed.annotation.id == id) {
                return listed;
            }
        }
        return {};
    }

    /// The text box of the open frame
    QTextEdit* frameText() {
        REQUIRE(coordinator->frame() != nullptr);
        return coordinator->frame()->findChild<QTextEdit*>();
    }
};

}  // namespace

// =============================================================================
// Filters and orders
// =============================================================================

TEST_CASE("Annotations panel: an annotation passes the filters", "[gui][annotations]") {
    const QDateTime now(QDate(2026, 10, 8), QTime(12, 0), QTimeZone::utc());
    AnnotationEntry todo = entryOf(QStringLiteral("t"), AnnotationKind::Todo,
                                   QStringLiteral("Check the dates"));
    todo.annotation.created = now.addDays(-3);
    AnnotationFilter filter;

    SECTION("by state: the open ones at first") {
        CHECK(matches(todo, filter, now));
        todo.annotation.done = true;
        CHECK_FALSE(matches(todo, filter, now));
        filter.state = AnnotationStateFilter::Done;
        CHECK(matches(todo, filter, now));
        filter.state = AnnotationStateFilter::All;
        CHECK(matches(todo, filter, now));
    }

    SECTION("by kind; a kind not shown is still counted") {
        filter.todos = false;
        CHECK_FALSE(matches(todo, filter, now));
        CHECK(matchesExceptKind(todo, filter, now));
    }

    SECTION("by text, in any case") {
        filter.text = QStringLiteral("  the DATES ");
        CHECK(matches(todo, filter, now));
        filter.text = QStringLiteral("names");
        CHECK_FALSE(matches(todo, filter, now));
    }

    SECTION("by date") {
        filter.date = AnnotationDateFilter::Today;
        CHECK_FALSE(matches(todo, filter, now));
        filter.date = AnnotationDateFilter::Last7Days;
        CHECK(matches(todo, filter, now));
        todo.annotation.created = now.addDays(-8);
        CHECK_FALSE(matches(todo, filter, now));
        filter.date = AnnotationDateFilter::Last30Days;
        CHECK(matches(todo, filter, now));

        // Made at an unknown time: only for any date
        todo.annotation.created = QDateTime();
        CHECK_FALSE(matches(todo, filter, now));
        filter.date = AnnotationDateFilter::Any;
        CHECK(matches(todo, filter, now));
    }
}

TEST_CASE("Annotations panel: the orders of the annotations", "[gui][annotations]") {
    const QDateTime now(QDate(2026, 10, 8), QTime(12, 0), QTimeZone::utc());
    std::vector<AnnotationEntry> entries = {
        entryOf(QStringLiteral("b2"), AnnotationKind::Note, QString(), 1, 1),
        entryOf(QStringLiteral("a1"), AnnotationKind::Comment, QString(), 0, 0),
        entryOf(QStringLiteral("b1"), AnnotationKind::Todo, QString(), 1, 0),
        entryOf(QStringLiteral("a2"), AnnotationKind::Note, QString(), 0, 1)};
    entries[0].annotation.created = now.addDays(-1);  // b2
    entries[1].annotation.created = now.addDays(-2);  // a1
    entries[3].annotation.created = now;              // a2; b1 made at an unknown time

    SECTION("as in the book: chapter by chapter, in the order of the text") {
        sortEntries(entries, AnnotationSort::TextOrder);
        CHECK(idsOf(entries) == QStringLiteral("a1 a2 b1 b2"));
    }

    SECTION("the newest first, those made at an unknown time last") {
        sortEntries(entries, AnnotationSort::Newest);
        CHECK(idsOf(entries) == QStringLiteral("a2 b2 a1 b1"));
    }

    SECTION("by kind: comments, to-dos, notes; each kind as in the book") {
        sortEntries(entries, AnnotationSort::ByKind);
        CHECK(idsOf(entries) == QStringLiteral("a1 b1 a2 b2"));
    }
}

// =============================================================================
// Colors
// =============================================================================

TEST_CASE("Annotations: the colors of the cards are readable on any surface",
          "[gui][annotations]") {
    CHECK(std::abs(contrastRatio(Qt::black, Qt::white) - 21.0) < 0.01);
    CHECK(std::abs(contrastRatio(QColor(0x77, 0x77, 0x77), QColor(0x77, 0x77, 0x77)) - 1.0) < 0.01);

    // A color already readable stays; one too faint goes toward black or white
    CHECK(readableColor(Qt::black, Qt::white, MIN_TEXT_CONTRAST) == QColor(Qt::black));
    const QColor darker = readableColor(QColor(0xcc, 0xcc, 0xcc), Qt::white, MIN_TEXT_CONTRAST);
    CHECK(contrastRatio(darker, Qt::white) >= MIN_TEXT_CONTRAST);
    CHECK(darker.lightness() < 0xcc);
    const QColor lighter =
        readableColor(QColor(0x55, 0x55, 0x55), QColor(0x2b, 0x2b, 0x2b), MIN_TEXT_CONTRAST);
    CHECK(contrastRatio(lighter, QColor(0x2b, 0x2b, 0x2b)) >= MIN_TEXT_CONTRAST);
    CHECK(lighter.lightness() > 0x55);

    // The kinds' colors of the themes, on the panels and the papers of both themes
    for (const QString& name : {QStringLiteral("Light"), QStringLiteral("Dark")}) {
        const core::Theme theme = themeNamed(name);
        for (const auto& [key, kindColor] : theme.editor) {
            if (key.rfind("annotation", 0) != 0) {
                continue;
            }
            for (const auto& [surface, text] :
                 {std::pair{theme.palette.base, theme.palette.text},
                  std::pair{QColor(Qt::white), QColor(Qt::black)},
                  std::pair{QColor(35, 35, 40), QColor(0xe0, 0xe0, 0xe0)}}) {
                INFO(name.toStdString() << " " << key << " on " << surface.name().toStdString());
                const AnnotationCardColors colors = annotationCardColors(kindColor, surface, text);
                CHECK(colors.background != surface);  // tinted with its kind: it stands out
                CHECK(contrastRatio(colors.text, colors.background) >= MIN_TEXT_CONTRAST);
                CHECK(contrastRatio(colors.kindName, colors.background) >= MIN_TEXT_CONTRAST);
                CHECK(contrastRatio(colors.secondary, colors.background) >= MIN_TEXT_CONTRAST);
            }
        }
    }
}

TEST_CASE("Annotations panel: its texts are readable in the light and the dark theme",
          "[gui][annotations]") {
    for (const QString& name : {QStringLiteral("Light"), QStringLiteral("Dark")}) {
        INFO(name.toStdString());
        const core::Theme theme = themeNamed(name);
        AnnotationsPanel panel;
        panel.applyTheme(theme);

        // Colors written in: none of the palette of the theme before stays
        CHECK_FALSE(panel.styleSheet().contains(QStringLiteral("palette(")));

        for (const AnnotationKind kind : KINDS) {
            const AnnotationCardColors& colors = panel.cardColors(kind);
            CHECK(colors.background != theme.palette.base);
            CHECK(contrastRatio(colors.text, colors.background) >= MIN_TEXT_CONTRAST);
            CHECK(contrastRatio(colors.kindName, colors.background) >= MIN_TEXT_CONTRAST);
            CHECK(contrastRatio(colors.secondary, colors.background) >= MIN_TEXT_CONTRAST);
        }

        // A kind button that is off: its text on the panel's background
        auto* kindButton = panel.findChild<QToolButton*>(QStringLiteral("annotationKindButton"));
        REQUIRE(kindButton != nullptr);
        kindButton->setChecked(false);
        kindButton->ensurePolished();
        CHECK(contrastRatio(kindButton->palette().color(kindButton->foregroundRole()),
                            theme.palette.window) >= MIN_TEXT_CONTRAST);

        // A card in the colors of its kind
        panel.setEntries({entryOf(QStringLiteral("a"), AnnotationKind::Todo, QStringLiteral("One"))});
        const AnnotationCard* card = panel.card(QStringLiteral("ch-0/a"));
        REQUIRE(card != nullptr);
        CHECK(card->colors() == panel.cardColors(AnnotationKind::Todo));
    }
}

// =============================================================================
// The panel
// =============================================================================

TEST_CASE("Annotations panel: cards of the annotations that pass the filters",
          "[gui][annotations]") {
    AnnotationsPanel panel;
    AnnotationEntry comment =
        entryOf(QStringLiteral("c"), AnnotationKind::Comment, QStringLiteral("Too long"), 0, 0);
    AnnotationEntry done =
        entryOf(QStringLiteral("t"), AnnotationKind::Todo, QStringLiteral("Check"), 0, 1);
    done.annotation.done = true;
    const AnnotationEntry note =
        entryOf(QStringLiteral("n"), AnnotationKind::Note, QStringLiteral("Idea"), 0, 2);
    panel.setEntries({comment, done, note});

    // The open ones at first
    CHECK(idsOf(panel.shownEntries()) == QStringLiteral("c n"));
    CHECK(panel.card(done.key()) == nullptr);

    AnnotationFilter filter = panel.filter();
    filter.state = AnnotationStateFilter::All;
    panel.setFilter(filter);
    CHECK(idsOf(panel.shownEntries()) == QStringLiteral("c t n"));

    // A card stays for its annotation listed again, and shows it as it is now
    AnnotationCard* card = panel.card(comment.key());
    REQUIRE(card != nullptr);
    comment.annotation.text = QStringLiteral("Shorter");
    panel.setEntries({comment, done, note});
    CHECK(panel.card(comment.key()) == card);
    CHECK(card->text() == QStringLiteral("Shorter"));

    // Gone from the list, gone from the panel
    panel.setEntries({comment});
    CHECK(idsOf(panel.shownEntries()) == QStringLiteral("c"));
    CHECK(panel.card(note.key()) == nullptr);
}

TEST_CASE("Annotations panel: a card shows who made the annotation", "[gui][annotations]") {
    // Regression: the author was only in the tooltip of a part of the card, so the user's
    // test did not find it
    AnnotationCard card;
    AnnotationEntry entry =
        entryOf(QStringLiteral("c"), AnnotationKind::Comment, QStringLiteral("Too long"), 1, 0);
    entry.annotation.author = QStringLiteral(" Anna Nowak ");
    entry.annotation.created = QDateTime(QDate(2026, 10, 9), QTime(11, 57), QTimeZone::utc());
    card.setEntry(entry);
    card.resize(400, card.sizeHint().height());
    card.show();
    QApplication::processEvents();

    // Above the text, as comments in a word processor show it
    auto* author = card.findChild<QLabel*>(QStringLiteral("annotationAuthor"));
    REQUIRE(author != nullptr);
    CHECK(author->isVisibleTo(&card));
    CHECK(author->text() == QStringLiteral("Anna Nowak"));
    auto* text = card.findChild<QLabel*>(QStringLiteral("annotationText"));
    REQUIRE(text != nullptr);
    CHECK(author->geometry().bottom() < text->geometry().top());

    // The tooltip says who made it and when, and the chapter, wherever the card is pointed
    // at: no part of it has a tooltip of its own
    const QStringList tip = card.toolTip().split(QLatin1Char('\n'));
    REQUIRE(tip.size() == 2);
    CHECK(tip[0].startsWith(QStringLiteral("Anna Nowak, ")));
    CHECK(tip[1] == QStringLiteral("Chapter 1"));
    for (const QLabel* label : card.findChildren<QLabel*>()) {
        CHECK(label->toolTip().isEmpty());
    }
    QHelpEvent help(QEvent::ToolTip, QPoint(1, 1), text->mapToGlobal(QPoint(1, 1)));
    QApplication::sendEvent(text, &help);
    CHECK(QToolTip::text() == card.toolTip());
    QToolTip::hideText();

    // Without an author, no line for one
    entry.annotation.author.clear();
    card.setEntry(entry);
    CHECK_FALSE(author->isVisibleTo(&card));
    CHECK_FALSE(card.toolTip().contains(QStringLiteral("Anna")));
}

TEST_CASE("Annotations panel: an annotation the filters hide is shown when it is gone to",
          "[gui][annotations]") {
    AnnotationsPanel panel;
    AnnotationEntry done =
        entryOf(QStringLiteral("t"), AnnotationKind::Todo, QStringLiteral("Check"), 0, 0);
    done.annotation.done = true;
    panel.setEntries({done});
    AnnotationFilter filter = panel.filter();
    filter.text = QStringLiteral("names");
    filter.todos = false;
    panel.setFilter(filter);
    REQUIRE(panel.shownEntries().empty());

    panel.revealAnnotation(done.key());
    CHECK(panel.filter().text.isEmpty());
    CHECK(panel.filter().todos);
    CHECK(panel.filter().state == AnnotationStateFilter::All);
    CHECK(panel.card(done.key()) != nullptr);
    CHECK(panel.selectedKey() == done.key());
}

TEST_CASE("Annotations panel: the keys of the list", "[gui][annotations]") {
    AnnotationsPanel panel;
    std::vector<AnnotationEntry> entries = {
        entryOf(QStringLiteral("a"), AnnotationKind::Comment, QStringLiteral("One"), 0, 0),
        entryOf(QStringLiteral("b"), AnnotationKind::Todo, QStringLiteral("Two"), 0, 1),
        entryOf(QStringLiteral("c"), AnnotationKind::Note, QStringLiteral("Three"), 0, 2)};
    panel.setEntries(entries);
    QWidget* list = listOf(panel);

    QStringList activated;
    QStringList edited;
    QStringList toggled;
    int backToText = 0;
    QObject::connect(&panel, &AnnotationsPanel::annotationActivated,
                     [&activated](const AnnotationEntry& entry) { activated << entry.annotation.id; });
    QObject::connect(&panel, &AnnotationsPanel::editRequested,
                     [&edited](const AnnotationEntry& entry) { edited << entry.annotation.id; });
    QObject::connect(&panel, &AnnotationsPanel::editorFocusRequested, [&backToText]() { ++backToText; });
    // As the coordinator does: the annotation changes or goes, and the panel lists them again
    QObject::connect(&panel, &AnnotationsPanel::doneToggled,
                     [&panel, &entries, &toggled](const AnnotationEntry& entry, bool done) {
                         toggled << entry.annotation.id;
                         for (AnnotationEntry& e : entries) {
                             if (e.key() == entry.key()) {
                                 e.annotation.done = done;
                             }
                         }
                         panel.setEntries(entries);
                     });
    QObject::connect(&panel, &AnnotationsPanel::deleteRequested,
                     [&panel, &entries](const AnnotationEntry& entry) {
                         std::erase_if(entries, [&entry](const AnnotationEntry& e) {
                             return e.key() == entry.key();
                         });
                         panel.setEntries(entries);
                     });

    SECTION("the list keeps its keys from the window's shortcuts (F2 is the Navigator's)") {
        CHECK(keepsFromShortcuts(list, Qt::Key_F2, Qt::NoModifier));
        CHECK(keepsFromShortcuts(list, Qt::Key_Delete, Qt::NoModifier));
        CHECK(keepsFromShortcuts(list, Qt::Key_Space, Qt::NoModifier));
        CHECK(keepsFromShortcuts(list, Qt::Key_F10, Qt::ShiftModifier));
        CHECK_FALSE(keepsFromShortcuts(list, Qt::Key_F9, Qt::NoModifier));  // back to the text
        CHECK_FALSE(keepsFromShortcuts(list, Qt::Key_Z, Qt::ControlModifier));  // undo
    }

    SECTION("Up, Down, Home and End go through the cards and to their places") {
        press(list, Qt::Key_Down);
        press(list, Qt::Key_Down);
        press(list, Qt::Key_Up);
        press(list, Qt::Key_End);
        press(list, Qt::Key_End);  // the last one already: nothing to go to
        press(list, Qt::Key_Home);
        CHECK(activated.join(QLatin1Char(' ')) == QStringLiteral("a b a c a"));
        CHECK(panel.selectedKey() == entries[0].key());
    }

    SECTION("Enter and F2 edit the selected one; Esc goes back to the text") {
        press(list, Qt::Key_Down);
        press(list, Qt::Key_Return);
        press(list, Qt::Key_End);
        press(list, Qt::Key_F2);
        CHECK(edited.join(QLatin1Char(' ')) == QStringLiteral("a c"));
        press(list, Qt::Key_Escape);
        CHECK(backToText == 1);
    }

    SECTION("Space marks the selected one done; the next card is selected when it goes") {
        press(list, Qt::Key_Down);
        press(list, Qt::Key_Down);  // the to-do
        press(list, Qt::Key_Space);
        CHECK(toggled.join(QLatin1Char(' ')) == QStringLiteral("b"));
        CHECK(idsOf(panel.shownEntries()) == QStringLiteral("a c"));
        CHECK(panel.selectedKey() == entries[2].key());

        // A note has no state
        press(list, Qt::Key_Space);
        CHECK(toggled.join(QLatin1Char(' ')) == QStringLiteral("b"));
    }

    SECTION("Delete removes the selected one; the card after it, or before the last, is next") {
        press(list, Qt::Key_Down);
        press(list, Qt::Key_Delete);
        CHECK(idsOf(panel.shownEntries()) == QStringLiteral("b c"));
        CHECK(panel.selectedKey() == entries[0].key());

        press(list, Qt::Key_Down);
        press(list, Qt::Key_Delete);
        CHECK(idsOf(panel.shownEntries()) == QStringLiteral("b"));
        CHECK(panel.selectedKey() == entries[0].key());
    }
}

TEST_CASE("Annotations panel: the keys go to the list with a card selected",
          "[gui][annotations]") {
    AnnotationsPanel panel;
    const std::vector<AnnotationEntry> entries = {
        entryOf(QStringLiteral("a"), AnnotationKind::Comment, QStringLiteral("One"), 0, 0),
        entryOf(QStringLiteral("b"), AnnotationKind::Note, QStringLiteral("Two"), 0, 1)};
    panel.setEntries(entries);

    // The one asked for, else the selected one, else the first
    panel.focusList();
    CHECK(panel.selectedKey() == entries[0].key());
    panel.focusList(entries[1].key());
    CHECK(panel.selectedKey() == entries[1].key());
    panel.focusList(QStringLiteral("ch-0/gone"));
    CHECK(panel.selectedKey() == entries[1].key());
}

TEST_CASE("Annotations panel: the to-do buttons follow their commands", "[gui][annotations]") {
    AnnotationsPanel panel;
    QAction previous(QStringLiteral("Previous To Do"));
    QAction next(QStringLiteral("Next To Do"));
    int triggered = 0;
    QObject::connect(&next, &QAction::triggered, [&triggered]() { ++triggered; });
    panel.setTodoActions(&previous, &next);

    QToolButton* nextButton = nullptr;
    for (QToolButton* button : panel.findChildren<QToolButton*>()) {
        if (button->text() == AnnotationsPanel::tr("Next")) {
            nextButton = button;
        }
    }
    REQUIRE(nextButton != nullptr);
    nextButton->click();
    CHECK(triggered == 1);
    next.setEnabled(false);
    CHECK_FALSE(nextButton->isEnabled());
}

// =============================================================================
// The frame
// =============================================================================

TEST_CASE("Annotations frame: Enter starts a new line, Ctrl+Enter and Save keep the text, Esc "
          "drops it",
          "[gui][annotations]") {
    QWidget editor;
    editor.resize(800, 600);
    auto* frame = new AnnotationFrame(&editor);
    frame->setKind(AnnotationKind::Todo);
    auto* text = frame->findChild<QTextEdit*>();
    auto* save = frame->findChild<QPushButton*>();
    REQUIRE(text != nullptr);
    REQUIRE(save != nullptr);
    int saved = 0;
    int cancelled = 0;
    QObject::connect(frame, &AnnotationFrame::saveRequested, [&saved]() { ++saved; });
    QObject::connect(frame, &AnnotationFrame::cancelRequested, [&cancelled]() { ++cancelled; });

    // It says how the text is kept
    const auto* hint = frame->findChild<QLabel*>(QStringLiteral("annotationFrameHint"));
    REQUIRE(hint != nullptr);
    CHECK(hint->text().contains(AnnotationFrame::saveKeysText()));
    CHECK(frame->findChild<QLabel*>(QStringLiteral("annotationFrameKind"))->text() ==
          AnnotationCard::kindTitle(AnnotationKind::Todo));

    // Without text nothing is kept
    CHECK_FALSE(frame->canSave());
    CHECK_FALSE(save->isEnabled());
    press(text, Qt::Key_Return, Qt::ControlModifier);
    CHECK(saved == 0);

    // Enter starts a new line
    press(text, Qt::Key_A, Qt::NoModifier, QStringLiteral("a"));
    press(text, Qt::Key_Return, Qt::NoModifier, QStringLiteral("\r"));
    press(text, Qt::Key_B, Qt::NoModifier, QStringLiteral("b"));
    CHECK(frame->text() == QStringLiteral("a\nb"));
    CHECK(save->isEnabled());
    CHECK(saved == 0);

    press(text, Qt::Key_Return, Qt::ControlModifier);
    press(text, Qt::Key_Enter, Qt::ControlModifier | Qt::KeypadModifier);
    save->click();
    press(save, Qt::Key_Return);  // Enter on the button with the keys
    CHECK(saved == 4);
    CHECK(frame->text() == QStringLiteral("a\nb"));

    press(text, Qt::Key_Escape);
    press(save, Qt::Key_Escape);
    CHECK(cancelled == 2);
}

TEST_CASE("Annotations frame: the keys stay in it; saving, closing and quitting go through",
          "[gui][annotations]") {
    QWidget editor;
    editor.resize(800, 600);
    auto* frame = new AnnotationFrame(&editor);
    frame->setText(QStringLiteral("Note"));
    auto* text = frame->findChild<QTextEdit*>();
    auto* save = frame->findChild<QPushButton*>();
    KeyCounter counter;
    editor.installEventFilter(&counter);

    // The window's shortcuts would change the text under the frame or take the keys away
    CHECK(keepsFromShortcuts(text, Qt::Key_B, Qt::ControlModifier));
    CHECK(keepsFromShortcuts(text, Qt::Key_M, Qt::ControlModifier | Qt::ShiftModifier));
    CHECK(keepsFromShortcuts(text, Qt::Key_Down, Qt::AltModifier));
    CHECK(keepsFromShortcuts(text, Qt::Key_F9, Qt::NoModifier));
    CHECK(keepsFromShortcuts(save, Qt::Key_F9, Qt::NoModifier));
    CHECK_FALSE(keepsFromShortcuts(text, Qt::Key_S, Qt::ControlModifier));
    for (const QKeySequence& close : QKeySequence::keyBindings(QKeySequence::Close)) {
        const QKeyCombination combination = close[0];
        CHECK_FALSE(keepsFromShortcuts(text, combination.key(), combination.keyboardModifiers()));
    }

    // A key the frame does not use does not reach the editor under it
    press(text, Qt::Key_B, Qt::ControlModifier);
    press(save, Qt::Key_F9);
    CHECK(counter.keys == 0);
}

TEST_CASE("Annotations frame: its X and a click anywhere else in the program close it, the "
          "text kept",
          "[gui][annotations]") {
    QWidget window;
    window.resize(1000, 600);
    auto* editor = new QWidget(&window);
    editor->setGeometry(0, 0, 800, 600);
    auto* scrollBar = new QScrollBar(Qt::Vertical, editor);
    scrollBar->setGeometry(780, 0, 20, 600);
    auto* panel = new QLineEdit(&window);
    panel->setGeometry(820, 20, 160, 30);
    auto* toolButton = new QToolButton(&window);  // as on a toolbar: a click leaves the keys
    toolButton->setGeometry(820, 60, 30, 30);
    PressCounter counter;
    editor->installEventFilter(&counter);
    auto* frame = new AnnotationFrame(editor);
    frame->setGeometry(100, 100, 400, 160);
    frame->setText(QStringLiteral("Note"));
    window.show();  // the frame watches the program while it is shown
    int closed = 0;
    int saved = 0;
    int cancelled = 0;
    QObject::connect(frame, &AnnotationFrame::closeRequested, [&closed]() { ++closed; });
    QObject::connect(frame, &AnnotationFrame::saveRequested, [&saved]() { ++saved; });
    QObject::connect(frame, &AnnotationFrame::cancelRequested, [&cancelled]() { ++cancelled; });

    SECTION("its X is for the mouse: the keys stay in the text") {
        auto* close = frame->findChild<QToolButton*>(QStringLiteral("annotationFrameClose"));
        REQUIRE(close != nullptr);
        CHECK(close->focusPolicy() == Qt::NoFocus);
        CHECK_FALSE(close->toolTip().isEmpty());
        close->click();
        CHECK(closed == 1);
    }

    SECTION("a click in the editor beside it goes on to the editor") {
        click(editor, QPointF(20, 20));
        CHECK(closed == 1);
        CHECK(counter.presses == 1);
    }

    SECTION("a click in another place of the window") {
        click(panel, QPointF(5, 5));
        CHECK(closed == 1);
    }

    SECTION("a click on a button that leaves the keys where they are") {
        click(toolButton, QPointF(5, 5));
        CHECK(closed == 1);
    }

    SECTION("asked once") {
        click(editor, QPointF(20, 20));
        click(panel, QPointF(5, 5));
        CHECK(closed == 1);
    }

    SECTION("not by a click on it, on the editor's scroll bar or in a menu over it") {
        click(frame, QPointF(10, 10));
        click(frame->findChild<QTextEdit*>()->viewport(), QPointF(5, 5));
        click(scrollBar, QPointF(5, 5));
        QWidget menu(&window, Qt::Popup);
        menu.resize(50, 50);
        click(&menu, QPointF(5, 5));
        CHECK(closed == 0);
        CHECK(counter.presses == 0);  // nothing reached the editor
    }

    SECTION("not after Esc: nothing is kept") {
        press(frame->findChild<QTextEdit*>(), Qt::Key_Escape);
        click(editor, QPointF(20, 20));
        CHECK(cancelled == 1);
        CHECK(closed == 0);
    }

    SECTION("not while it is hidden, nor after it is gone") {
        frame->hide();
        click(editor, QPointF(20, 20));
        delete frame;
        click(editor, QPointF(20, 20));
        CHECK(closed == 0);
        CHECK(counter.presses == 2);
    }
    CHECK(saved == 0);
}

TEST_CASE("Annotations frame: under its place within the text column, above it without room "
          "below",
          "[gui][annotations]") {
    QWidget editor;
    editor.resize(800, 600);
    auto* frame = new AnnotationFrame(&editor);
    AnnotationFrame::Placement placement{QRectF(200, 100, 1, 20), QRectF(100, 0, 600, 600)};
    frame->setPlacement([&placement]() { return placement; });

    // Two thirds of the column wide, a little left of the place
    CHECK(frame->width() == 400);
    CHECK(frame->geometry().top() == 124);
    CHECK(frame->geometry().left() == 184);

    // Within the column at its right edge
    placement.place = QRectF(650, 100, 1, 20);
    frame->place();
    CHECK(frame->geometry().left() == 300);

    // Above the place at the bottom of the view
    placement.place = QRectF(200, 560, 1, 20);
    frame->place();
    CHECK(frame->geometry().bottom() < 560);
    CHECK(frame->geometry().top() >= 8);

    // It follows the place when the editor changes
    placement.place = QRectF(200, 300, 1, 20);
    QResizeEvent resize(editor.size(), editor.size());
    QApplication::sendEvent(&editor, &resize);
    REQUIRE(test::waitUntil([&frame]() { return frame->geometry().top() == 324; }));
}

TEST_CASE("Annotations frame: who the annotation is by, on the right of its header",
          "[gui][annotations]") {
    QWidget editor;
    editor.resize(800, 600);
    editor.show();
    auto* frame = new AnnotationFrame(&editor);
    frame->setKind(AnnotationKind::Note);
    auto* author = frame->findChild<QLabel*>(QStringLiteral("annotationFrameAuthor"));
    auto* kind = frame->findChild<QLabel*>(QStringLiteral("annotationFrameKind"));
    REQUIRE(author != nullptr);
    REQUIRE(kind != nullptr);
    CHECK_FALSE(author->isVisibleTo(frame));  // by no one known

    frame->setAuthor(QStringLiteral(" Anna Nowak "));
    frame->setPlacement([]() {
        return AnnotationFrame::Placement{QRectF(200, 100, 1, 20), QRectF(100, 0, 600, 600)};
    });
    frame->show();
    QApplication::processEvents();
    CHECK(frame->author() == QStringLiteral("Anna Nowak"));
    CHECK(author->isVisibleTo(frame));
    CHECK(author->text() == QStringLiteral("Anna Nowak"));
    CHECK(author->geometry().left() > kind->geometry().right());
    CHECK(author->geometry().top() < kind->geometry().bottom());

    // Too long for the room: shortened, the whole name in its tooltip
    const QString longName = QStringLiteral("Anna Maria Nowak-Kowalska ").repeated(4).trimmed();
    frame->setAuthor(longName);
    CHECK(author->text() != longName);
    CHECK(author->toolTip() == longName);

    frame->setAuthor(QString());
    CHECK_FALSE(author->isVisibleTo(frame));
}

// =============================================================================
// Writing with a document
// =============================================================================

TEST_CASE("Annotations: a new annotation is written in a frame and added with its text, in one "
          "step",
          "[gui][annotations]") {
    Desk desk(test::kmlOf({QStringLiteral("One two three")}));
    desk.editor().setSelection({{0, 4}, {0, 7}});

    REQUIRE(desk.coordinator->addAnnotation(AnnotationKind::Todo));
    REQUIRE(desk.coordinator->isWriting());
    CHECK(desk.editor().annotations().empty());  // not before its text is saved
    CHECK_FALSE(desk.editor().canUndo());

    // In the paper's colors, readable
    const editor::EditorAppearance& appearance = desk.editor().appearance();
    const QColor paper = appearance.colors.background(appearance.colorMode);
    const AnnotationCardColors& colors = desk.coordinator->frame()->colors();
    CHECK(colors.background != paper);
    CHECK(contrastRatio(colors.text, colors.background) >= MIN_TEXT_CONTRAST);

    QTextEdit* text = desk.frameText();
    text->setPlainText(QStringLiteral("Check it\nand this"));
    press(text, Qt::Key_Return, Qt::ControlModifier);
    CHECK_FALSE(desk.coordinator->isWriting());
    CHECK(desk.coordinator->frame() == nullptr);

    REQUIRE(desk.editor().annotations().size() == 1);
    const editor::AnnotationPlace place = desk.editor().annotations().front();
    CHECK(place.start == 4);
    CHECK(place.end == 7);
    CHECK(place.annotation.kind == AnnotationKind::Todo);
    CHECK(place.annotation.text == QStringLiteral("Check it\nand this"));
    CHECK(place.annotation.author == AnnotationsCoordinator::author());

    // The writer goes on after the fragment
    CHECK_FALSE(desk.editor().hasSelection());
    CHECK(desk.editor().cursorPosition() == editor::CursorPosition{0, 7});

    // Listed and selected in the panel
    const AnnotationEntry listed = desk.entry(place.annotation.id);
    REQUIRE(desk.panel.card(listed.key()) != nullptr);
    CHECK(desk.panel.selectedKey() == listed.key());

    // One undo step takes it away with its text, and brings it back
    desk.editor().undo();
    CHECK(desk.editor().annotations().empty());
    desk.editor().redo();
    CHECK(desk.annotation(listed.annotation.id).text == QStringLiteral("Check it\nand this"));
}

TEST_CASE("Annotations: Esc drops what was written without a trace", "[gui][annotations]") {
    Desk desk(test::kmlOf({QStringLiteral("One two three")}));
    desk.editor().setCursorPosition({0, 3});
    desk.editor().insertText(QStringLiteral(","));
    REQUIRE(desk.editor().canUndo());

    REQUIRE(desk.coordinator->addAnnotation(AnnotationKind::Comment));
    desk.frameText()->setPlainText(QStringLiteral("Not this"));
    press(desk.frameText(), Qt::Key_Escape);
    CHECK_FALSE(desk.coordinator->isWriting());
    CHECK(desk.editor().annotations().empty());
    CHECK_FALSE(desk.editor().canRedo());

    // The typing before it is the step to undo
    desk.editor().undo();
    CHECK(desk.editor().textDocument()->toPlainText() == QStringLiteral("One two three"));
}

TEST_CASE("Annotations: the text of an annotation edited in its frame changes in a step of its "
          "own",
          "[gui][annotations]") {
    Desk desk(kmlWith(record(QStringLiteral("c1"), QStringLiteral("comment"), QStringLiteral("Old")),
                      {QStringLiteral("<anchor ref=\"c1\">One</anchor> two")}));
    REQUIRE(desk.panel.entries().size() == 1);

    // The writer typed in the text first
    desk.editor().setCursorPosition({0, 7});
    desk.editor().insertText(QStringLiteral(" three"));

    // Asked for in the panel (Enter on its card)
    emit desk.panel.editRequested(desk.entry(QStringLiteral("c1")));
    REQUIRE(desk.coordinator->isWriting());
    CHECK(desk.coordinator->frame()->text() == QStringLiteral("Old"));
    desk.frameText()->setPlainText(QStringLiteral("Newer"));
    desk.coordinator->saveWriting();
    CHECK(desk.annotation(QStringLiteral("c1")).text == QStringLiteral("Newer"));

    desk.editor().undo();
    CHECK(desk.annotation(QStringLiteral("c1")).text == QStringLiteral("Old"));
    CHECK(desk.editor().textDocument()->toPlainText() == QStringLiteral("One two three"));
    desk.editor().undo();
    CHECK(desk.editor().textDocument()->toPlainText() == QStringLiteral("One two"));
}

TEST_CASE("Annotations: what was written is kept before the document is saved or closed",
          "[gui][annotations]") {
    Desk desk(kmlWith(record(QStringLiteral("c1"), QStringLiteral("comment"), QStringLiteral("Old")),
                      {QStringLiteral("<anchor ref=\"c1\">One</anchor> two")}));
    const QTextDocument* document = desk.editor().textDocument();

    SECTION("a new one with its text is added, and nothing changes afterwards") {
        desk.editor().setCursorPosition({0, 7});
        REQUIRE(desk.coordinator->addAnnotation(AnnotationKind::Note));
        desk.frameText()->setPlainText(QStringLiteral("Remember"));
        desk.coordinator->finishWriting();
        CHECK_FALSE(desk.coordinator->isWriting());
        REQUIRE(desk.editor().annotations().size() == 2);

        // Saved now, the document stays as saved: nothing is written to it later
        const int revision = document->revision();
        test::runEventLoop(400);
        CHECK(document->revision() == revision);
    }

    SECTION("a frame left empty keeps nothing") {
        REQUIRE(desk.coordinator->editAnnotation(QString(), QStringLiteral("c1"), false));
        desk.frameText()->setPlainText(QStringLiteral("  "));
        desk.coordinator->finishWriting();
        CHECK_FALSE(desk.coordinator->isWriting());
        CHECK(desk.annotation(QStringLiteral("c1")).text == QStringLiteral("Old"));
        CHECK_FALSE(desk.editor().canUndo());
    }

    SECTION("only the frame over the editor asked for") {
        REQUIRE(desk.coordinator->addAnnotation(AnnotationKind::Note));
        desk.frameText()->setPlainText(QStringLiteral("Remember"));
        const EditorPanel other;
        desk.coordinator->finishWriting(other.getBookEditor());
        CHECK(desk.coordinator->isWriting());
        desk.coordinator->finishWriting(&desk.editor());
        CHECK_FALSE(desk.coordinator->isWriting());
    }
}

TEST_CASE("Annotations: one frame at a time; the text of the open one is kept",
          "[gui][annotations]") {
    Desk desk(test::kmlOf({QStringLiteral("One two three")}));
    desk.editor().setCursorPosition({0, 3});
    REQUIRE(desk.coordinator->addAnnotation(AnnotationKind::Comment));
    desk.frameText()->setPlainText(QStringLiteral("First"));

    desk.editor().setCursorPosition({0, 7});
    REQUIRE(desk.coordinator->addAnnotation(AnnotationKind::Note));
    REQUIRE(desk.editor().annotations().size() == 1);
    CHECK(desk.editor().annotations().front().annotation.text == QStringLiteral("First"));
    CHECK(desk.coordinator->isWriting());
    CHECK(desk.coordinator->frame()->text().isEmpty());
}

TEST_CASE("Annotations: the X of the frame and a click anywhere else keep what was written",
          "[gui][annotations]") {
    Desk desk(kmlWith(record(QStringLiteral("c1"), QStringLiteral("comment"), QStringLiteral("Old")),
                      {QStringLiteral("One two <anchor ref=\"c1\">three</anchor>")}));
    desk.tabs.show();  // the frame watches the program while it is shown
    PressCounter counter;
    desk.editor().installEventFilter(&counter);

    SECTION("a click beside it: the new text in a step of its own; the click goes on to the "
            "text") {
        REQUIRE(desk.coordinator->editAnnotation(QString(), QStringLiteral("c1"), false));
        desk.frameText()->setPlainText(QStringLiteral("Newer"));
        click(&desk.editor(), QPointF(5, 5));
        CHECK_FALSE(desk.coordinator->isWriting());
        CHECK(desk.coordinator->frame() == nullptr);
        CHECK(counter.presses == 1);
        CHECK(desk.annotation(QStringLiteral("c1")).text == QStringLiteral("Newer"));
        desk.editor().undo();
        CHECK(desk.annotation(QStringLiteral("c1")).text == QStringLiteral("Old"));
    }

    SECTION("a click in the panel: the new text is kept first; the click goes on to the panel") {
        REQUIRE(desk.coordinator->editAnnotation(QString(), QStringLiteral("c1"), false));
        desk.frameText()->setPlainText(QStringLiteral("Newer"));
        PressCounter panelCounter;
        desk.panel.installEventFilter(&panelCounter);
        click(&desk.panel, QPointF(5, 5));
        CHECK_FALSE(desk.coordinator->isWriting());
        CHECK(panelCounter.presses == 1);
        CHECK(counter.presses == 0);
        CHECK(desk.annotation(QStringLiteral("c1")).text == QStringLiteral("Newer"));
    }

    SECTION("the editor's scroll bar only moves the view: the frame stays open") {
        REQUIRE(desk.coordinator->editAnnotation(QString(), QStringLiteral("c1"), false));
        desk.frameText()->setPlainText(QStringLiteral("Newer"));
        click(desk.editor().verticalScrollBar(), QPointF(2, 2));
        CHECK(desk.coordinator->isWriting());
        CHECK(desk.annotation(QStringLiteral("c1")).text == QStringLiteral("Old"));
    }

    SECTION("its X: a new annotation is added with its text") {
        desk.editor().setSelection({{0, 0}, {0, 3}});
        REQUIRE(desk.coordinator->addAnnotation(AnnotationKind::Todo));
        desk.frameText()->setPlainText(QStringLiteral("Check"));
        auto* close = desk.coordinator->frame()->findChild<QToolButton*>(
            QStringLiteral("annotationFrameClose"));
        REQUIRE(close != nullptr);
        close->click();
        CHECK_FALSE(desk.coordinator->isWriting());
        REQUIRE(desk.editor().annotations().size() == 2);
        const editor::AnnotationPlace added = desk.editor().annotations().front();
        CHECK(added.start == 0);
        CHECK(added.annotation.kind == AnnotationKind::Todo);
        CHECK(added.annotation.text == QStringLiteral("Check"));
    }

    SECTION("a new one left empty: nothing is added") {
        desk.editor().setCursorPosition({0, 3});
        REQUIRE(desk.coordinator->addAnnotation(AnnotationKind::Note));
        click(&desk.editor(), QPointF(5, 5));
        CHECK_FALSE(desk.coordinator->isWriting());
        CHECK(desk.editor().annotations().size() == 1);
        CHECK_FALSE(desk.editor().canUndo());
    }
}

TEST_CASE("Annotations: a click on a mark opens its annotation; the cursor stays",
          "[gui][annotations]") {
    Desk desk(kmlWith(record(QStringLiteral("t1"), QStringLiteral("todo"), QStringLiteral("Fix")),
                      {QStringLiteral("One <anchor ref=\"t1\">two</anchor> three")}));
    desk.editor().setCursorPosition({0, 0});

    emit desk.editor().annotationMarkClicked(QStringLiteral("t1"));
    REQUIRE(desk.coordinator->isWriting());
    CHECK(desk.coordinator->frame()->text() == QStringLiteral("Fix"));
    CHECK(desk.editor().cursorPosition() == editor::CursorPosition{0, 0});
    CHECK_FALSE(desk.editor().hasSelection());
    CHECK(desk.panel.selectedKey() == desk.entry(QStringLiteral("t1")).key());

    SECTION("deleted in the panel meanwhile: the frame closes") {
        emit desk.panel.deleteRequested(desk.entry(QStringLiteral("t1")));
        CHECK_FALSE(desk.coordinator->isWriting());
        CHECK(desk.editor().annotations().empty());
    }

    SECTION("taken off the text meanwhile (Edit > Undo): the frame closes") {
        desk.editor().removeAnnotation(QStringLiteral("t1"));
        CHECK_FALSE(desk.coordinator->isWriting());
    }
}

// =============================================================================
// The panel with a document
// =============================================================================

TEST_CASE("Annotations: done and deleted in the panel, both undone",
          "[gui][annotations]") {
    Desk desk(kmlWith(record(QStringLiteral("t1"), QStringLiteral("todo"), QStringLiteral("Fix")) +
                          record(QStringLiteral("n1"), QStringLiteral("note"), QStringLiteral("Idea")),
                      {QStringLiteral("<anchor ref=\"t1\">One</anchor> two<anchor ref=\"n1\"/>")}));
    REQUIRE(idsOf(desk.panel.shownEntries()) == QStringLiteral("t1 n1"));

    SECTION("a to-do done leaves the open ones") {
        const AnnotationEntry todo = desk.entry(QStringLiteral("t1"));
        emit desk.panel.doneToggled(todo, true);
        CHECK(desk.annotation(QStringLiteral("t1")).done);
        CHECK(idsOf(desk.panel.shownEntries()) == QStringLiteral("n1"));

        desk.editor().undo();
        CHECK_FALSE(desk.annotation(QStringLiteral("t1")).done);
        desk.coordinator->refresh();
        CHECK(idsOf(desk.panel.shownEntries()) == QStringLiteral("t1 n1"));
    }

    SECTION("a deleted note leaves the text") {
        const AnnotationEntry note = desk.entry(QStringLiteral("n1"));
        emit desk.panel.deleteRequested(note);
        CHECK(desk.annotation(QStringLiteral("n1")).id.isEmpty());
        CHECK(idsOf(desk.panel.shownEntries()) == QStringLiteral("t1"));

        desk.editor().undo();
        CHECK(desk.annotation(QStringLiteral("n1")).text == QStringLiteral("Idea"));
    }
}

TEST_CASE("Annotations: the next and previous to-do, with their cards selected",
          "[gui][annotations]") {
    Desk desk(kmlWith(record(QStringLiteral("t1"), QStringLiteral("todo"), QStringLiteral("A")) +
                          record(QStringLiteral("c1"), QStringLiteral("comment"), QStringLiteral("B")) +
                          record(QStringLiteral("t2"), QStringLiteral("todo"), QStringLiteral("C")),
                      {QStringLiteral("One <anchor ref=\"t1\">two</anchor> three<anchor ref=\"c1\"/> "
                                      "<anchor ref=\"t2\">four</anchor>")}));
    desk.editor().setCursorPosition({0, 0});

    REQUIRE(desk.coordinator->goToNextTodo());
    CHECK(desk.panel.selectedKey() == desk.entry(QStringLiteral("t1")).key());
    REQUIRE(desk.coordinator->goToNextTodo());  // past the comment
    CHECK(desk.panel.selectedKey() == desk.entry(QStringLiteral("t2")).key());
    // Its fragment selected ("four")
    CHECK(desk.editor().selection().normalized().start == editor::CursorPosition{0, 14});
    CHECK(desk.editor().selection().normalized().end == editor::CursorPosition{0, 18});
    CHECK_FALSE(desk.coordinator->goToNextTodo());
    REQUIRE(desk.coordinator->goToPreviousTodo());
    CHECK(desk.panel.selectedKey() == desk.entry(QStringLiteral("t1")).key());
}

TEST_CASE("Annotations: the tab of a chapter behind comes to the front for its annotation",
          "[gui][annotations]") {
    Desk desk(test::kmlOf({QStringLiteral("One two")}));
    auto* chapter = new EditorPanel();
    chapter->setContent(kmlWith(record(QStringLiteral("t1"), QStringLiteral("todo"), QStringLiteral("Fix")),
                                {QStringLiteral("Three <anchor ref=\"t1\">four</anchor>")}));
    chapter->setProperty("elementId", QStringLiteral("ch-2"));
    desk.tabs.addTab(chapter, QStringLiteral("Chapter Two"));
    REQUIRE(desk.tabs.currentIndex() == 0);
    editor::BookEditor& chapterEditor = *chapter->getBookEditor();

    // Its entry as the list of the whole book has it
    AnnotationEntry todo;
    todo.annotation = chapterEditor.annotations().front().annotation;
    todo.elementId = QStringLiteral("ch-2");

    SECTION("gone to: its fragment is selected in front") {
        emit desk.panel.annotationActivated(todo);
        CHECK(desk.tabs.currentWidget() == chapter);
        CHECK(chapterEditor.selection().normalized().start == editor::CursorPosition{0, 6});
        CHECK(chapterEditor.selection().normalized().end == editor::CursorPosition{0, 10});
    }

    SECTION("changed: in front, where Ctrl+Z undoes the change") {
        emit desk.panel.doneToggled(todo, true);
        CHECK(desk.tabs.currentWidget() == chapter);
        CHECK(chapterEditor.annotations().front().annotation.done);
        CHECK(chapterEditor.canUndo());
    }

    SECTION("edited: its frame opens over the chapter in front") {
        emit desk.panel.editRequested(todo);
        CHECK(desk.tabs.currentWidget() == chapter);
        REQUIRE(desk.coordinator->isWriting());
        CHECK(desk.coordinator->frame()->parentWidget() == &chapterEditor);
        CHECK(desk.coordinator->frame()->text() == QStringLiteral("Fix"));
    }
}

TEST_CASE("Annotations: the commands work on the document in front", "[gui][annotations]") {
    registerAllCommands(CommandCallbacks{});
    Desk desk(test::kmlOf({QStringLiteral("One two")}));
    desk.coordinator->connectCommands();
    auto& registry = CommandRegistry::getInstance();
    QAction* addNote = registry.getAction(QStringLiteral("insert.note"));
    QAction* nextTodo = registry.getAction(QStringLiteral("edit.nextTodo"));
    REQUIRE(addNote != nullptr);
    REQUIRE(nextTodo != nullptr);
    CHECK(addNote->isEnabled());
    CHECK(nextTodo->isEnabled());

    addNote->trigger();
    REQUIRE(desk.coordinator->isWriting());
    desk.frameText()->setPlainText(QStringLiteral("Idea"));
    desk.coordinator->saveWriting();
    REQUIRE(desk.editor().annotations().size() == 1);
    CHECK(desk.editor().annotations().front().annotation.kind == AnnotationKind::Note);

    // Nothing to work on without a document in front
    desk.tabs.addTab(new QWidget(), QStringLiteral("Dashboard"));
    desk.tabs.setCurrentIndex(1);
    CHECK_FALSE(addNote->isEnabled());
    CHECK_FALSE(nextTodo->isEnabled());
}

TEST_CASE("Annotations: F9 goes to the panel and back to the text", "[gui][annotations]") {
    if (QGuiApplication::platformName() != QStringLiteral("offscreen")) {
        SKIP("needs an active window without a window on screen: run with QT_QPA_PLATFORM=offscreen");
    }
    QMainWindow window;
    auto* tabs = new QTabWidget();
    window.setCentralWidget(tabs);
    auto* document = new EditorPanel();
    document->setContent(
        kmlWith(record(QStringLiteral("c1"), QStringLiteral("comment"), QStringLiteral("A")) +
                    record(QStringLiteral("n1"), QStringLiteral("note"), QStringLiteral("B")),
                {QStringLiteral("One <anchor ref=\"c1\">two</anchor> three <anchor ref=\"n1\">four</anchor>")}));
    tabs->addTab(document, QStringLiteral("Notes"));
    auto* dock = new QDockWidget(QStringLiteral("Annotations"), &window);
    auto* panel = new AnnotationsPanel();
    dock->setWidget(panel);
    window.addDockWidget(Qt::RightDockWidgetArea, dock);
    AnnotationsCoordinator coordinator(panel, dock, tabs, nullptr, nullptr);
    window.show();
    window.activateWindow();
    REQUIRE(test::waitUntil([&window] { return QApplication::activeWindow() == &window; }));
    dock->hide();
    editor::BookEditor* editor = document->getBookEditor();
    editor->setFocus();
    editor->setCursorPosition({0, 16});  // in "four"

    // To the panel, with the card at the cursor selected; its outline shows the keys are there
    const QString note = annotationKey(QString(), QStringLiteral("n1"));
    coordinator.togglePanelFocus();
    CHECK(dock->isVisible());
    CHECK(panel->hasFocusInside());
    CHECK(panel->selectedKey() == note);
    REQUIRE(panel->card(note) != nullptr);
    CHECK(panel->card(note)->isListFocused());

    // And back: the panel goes
    coordinator.togglePanelFocus();
    CHECK_FALSE(dock->isVisible());
    CHECK(editor->hasFocus());
    REQUIRE(panel->card(note) != nullptr);
    CHECK_FALSE(panel->card(note)->isListFocused());

    // Writing from the panel: the keys come back to the panel
    coordinator.togglePanelFocus();
    REQUIRE(coordinator.editAnnotation(QString(), QStringLiteral("c1"), true));
    REQUIRE(test::waitUntil([&coordinator] {
        const QWidget* focus = QApplication::focusWidget();
        return focus != nullptr && coordinator.frame()->isAncestorOf(focus);
    }));
    coordinator.cancelWriting();
    CHECK(panel->hasFocusInside());
    CHECK(panel->selectedKey() == annotationKey(QString(), QStringLiteral("c1")));
}

TEST_CASE("Annotations: the keys going elsewhere in the program close the frame, the text "
          "kept",
          "[gui][annotations]") {
    if (QGuiApplication::platformName() != QStringLiteral("offscreen")) {
        SKIP("needs an active window without a window on screen: run with QT_QPA_PLATFORM=offscreen");
    }
    QMainWindow window;
    auto* tabs = new QTabWidget();
    window.setCentralWidget(tabs);
    auto* document = new EditorPanel();
    document->setContent(test::kmlOf({QStringLiteral("One two three")}));
    tabs->addTab(document, QStringLiteral("Notes"));
    auto* dock = new QDockWidget(QStringLiteral("Annotations"), &window);
    auto* panel = new AnnotationsPanel();
    dock->setWidget(panel);
    window.addDockWidget(Qt::RightDockWidgetArea, dock);
    AnnotationsCoordinator coordinator(panel, dock, tabs, nullptr, nullptr);
    window.show();
    window.activateWindow();
    REQUIRE(test::waitUntil([&window] { return QApplication::activeWindow() == &window; }));
    editor::BookEditor* editor = document->getBookEditor();
    editor->setFocus();
    editor->setSelection({{0, 4}, {0, 7}});
    REQUIRE(coordinator.addAnnotation(AnnotationKind::Comment));
    QTextEdit* text = coordinator.frame()->findChild<QTextEdit*>();
    REQUIRE(text->hasFocus());
    text->setPlainText(QStringLiteral("Check"));
    auto* search = panel->findChild<QLineEdit*>();
    REQUIRE(search != nullptr);

    SECTION("to a panel: the annotation is added with its text, and the keys stay there") {
        search->setFocus(Qt::OtherFocusReason);
        CHECK_FALSE(coordinator.isWriting());
        REQUIRE(editor->annotations().size() == 1);
        CHECK(editor->annotations().front().annotation.text == QStringLiteral("Check"));
        CHECK(search->hasFocus());
    }

    SECTION("to another application and back: the frame stays open") {
        text->clearFocus();  // no window of the program has the keys
        CHECK(coordinator.isWriting());
        text->setFocus(Qt::ActiveWindowFocusReason);
        CHECK(coordinator.isWriting());
        CHECK(editor->annotations().empty());
    }

    SECTION("to another application, and then to another place of the program") {
        text->clearFocus();
        search->setFocus(Qt::ActiveWindowFocusReason);
        CHECK_FALSE(coordinator.isWriting());
        CHECK(editor->annotations().size() == 1);
    }

    SECTION("to a menu or a list over it: the frame stays open") {
        QWidget popup(&window, Qt::Popup);
        auto* field = new QLineEdit(&popup);
        popup.show();
        field->setFocus(Qt::PopupFocusReason);
        CHECK(field->hasFocus());
        CHECK(coordinator.isWriting());
        popup.close();
        CHECK(coordinator.isWriting());
        CHECK(editor->annotations().empty());
    }

    SECTION("after Esc nothing is kept, though the keys leave the frame") {
        press(text, Qt::Key_Escape);
        CHECK_FALSE(coordinator.isWriting());
        CHECK(editor->annotations().empty());
        CHECK(editor->hasFocus());
    }

    // Qt gives the keys to the first widget under a click that takes them, before the click
    // arrives (the editor, for a click on its scroll bar): done so here before each click

    SECTION("a click on the editor's scroll bar, also of the right button and with nothing to "
            "scroll: the frame stays open, the keys come back") {
        QScrollBar* bar = editor->verticalScrollBar();
        REQUIRE(bar->isVisible());
        REQUIRE(bar->maximum() == bar->minimum());  // the short text fits in the view
        editor->setFocus(Qt::MouseFocusReason);
        click(bar, QPointF(2, 2));
        QApplication::processEvents();
        REQUIRE(coordinator.isWriting());
        CHECK(text->hasFocus());
        editor->setFocus(Qt::MouseFocusReason);
        secondClick(bar, QPointF(2, 2));
        QApplication::processEvents();
        REQUIRE(coordinator.isWriting());
        CHECK(text->hasFocus());
        editor->setFocus(Qt::MouseFocusReason);
        click(bar, QPointF(2, 2), Qt::RightButton);
        QApplication::processEvents();
        REQUIRE(coordinator.isWriting());
        CHECK(text->hasFocus());
        CHECK(editor->annotations().empty());
    }

    SECTION("a click in the text: the annotation is added, the keys stay in the editor") {
        editor->setFocus(Qt::MouseFocusReason);
        click(editor, QPointF(5, 5));
        QApplication::processEvents();
        CHECK_FALSE(coordinator.isWriting());
        CHECK(editor->annotations().size() == 1);
        CHECK(editor->hasFocus());
    }

    SECTION("the wheel turning a field takes the keys there, with no click: the annotation is "
            "added") {
        search->setFocus(Qt::MouseFocusReason);
        QApplication::processEvents();
        CHECK_FALSE(coordinator.isWriting());
        CHECK(editor->annotations().size() == 1);
        CHECK(search->hasFocus());
    }
}

TEST_CASE("Annotations: who the new annotations are by", "[gui][annotations]") {
    auto& settings = core::SettingsManager::getInstance();
    settings.set<std::string>("annotations.author", "  Anna Nowak ");
    CHECK(AnnotationsCoordinator::author() == QStringLiteral("Anna Nowak"));

    // Without the setting: by no one named, neither the book's author nor the computer's user
    settings.set<std::string>("annotations.author", "");
    CHECK(AnnotationsCoordinator::author().isEmpty());
}

TEST_CASE("Annotations: the frame shows who the annotation is by", "[gui][annotations]") {
    auto& settings = core::SettingsManager::getInstance();
    settings.set<std::string>("annotations.author", "Anna Nowak");
    Desk desk(kmlWith(QStringLiteral("<annotation id=\"c1\" kind=\"comment\" "
                                     "author=\"Jan Kowalski\">Old</annotation>"),
                      {QStringLiteral("<anchor ref=\"c1\">One</anchor> two three")}));

    SECTION("a new one: by the author of the settings, as it is added") {
        desk.editor().setSelection({{0, 4}, {0, 7}});
        REQUIRE(desk.coordinator->addAnnotation(AnnotationKind::Note));
        CHECK(desk.coordinator->frame()->author() == QStringLiteral("Anna Nowak"));
        desk.frameText()->setPlainText(QStringLiteral("New"));
        desk.coordinator->saveWriting();
        QString added;
        for (const editor::AnnotationPlace& place : desk.editor().annotations()) {
            if (place.annotation.text == QStringLiteral("New")) {
                added = place.annotation.author;
            }
        }
        CHECK(added == QStringLiteral("Anna Nowak"));
    }
    SECTION("a new one without the setting: by no one named, the frame and the card show none") {
        settings.set<std::string>("annotations.author", "");
        desk.editor().setSelection({{0, 4}, {0, 7}});
        REQUIRE(desk.coordinator->addAnnotation(AnnotationKind::Note));
        CHECK(desk.coordinator->frame()->author().isEmpty());
        desk.frameText()->setPlainText(QStringLiteral("Nameless"));
        desk.coordinator->saveWriting();
        QString id;
        for (const editor::AnnotationPlace& place : desk.editor().annotations()) {
            if (place.annotation.text == QStringLiteral("Nameless")) {
                id = place.annotation.id;
                CHECK(place.annotation.author.isEmpty());
            }
        }
        REQUIRE_FALSE(id.isEmpty());
        desk.coordinator->refresh();
        const AnnotationCard* card = desk.panel.card(annotationKey(QString(), id));
        REQUIRE(card != nullptr);
        const auto* author = card->findChild<QLabel*>(QStringLiteral("annotationAuthor"));
        REQUIRE(author != nullptr);
        CHECK_FALSE(author->isVisibleTo(card));
    }
    SECTION("one edited again: by whom it was made") {
        REQUIRE(desk.coordinator->editAnnotation(QString(), QStringLiteral("c1"), false));
        CHECK(desk.coordinator->frame()->author() == QStringLiteral("Jan Kowalski"));
        desk.coordinator->cancelWriting();
    }
    settings.set<std::string>("annotations.author", "");
}
