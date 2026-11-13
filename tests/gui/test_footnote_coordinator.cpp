/// @file test_footnote_coordinator.cpp
/// @brief Notes: the frame their text is written in, Insert > Footnote and Endnote, editing a
///        note again, Esc, and the text kept when the frame is left

#include <catch2/catch_test_macros.hpp>
#include "document_window.h"
#include "kalahari/core/book_project.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/editor/kalahari_text_document_layout.h"
#include "kalahari/editor/text_footnotes.h"
#include "kalahari/gui/command_registrar.h"
#include "kalahari/gui/command_registry.h"
#include "kalahari/gui/dialogs/footnote_settings_dialog.h"
#include "kalahari/gui/footnote_coordinator.h"
#include "kalahari/gui/panels/note_frame.h"
#include "kalahari/gui/settings_dialog.h"

#include <QApplication>
#include <QComboBox>
#include <QGroupBox>
#include <QKeyEvent>
#include <QLabel>
#include <QMimeData>
#include <QTextCursor>
#include <QTextEdit>
#include <QToolButton>
#include <QtMath>
#include <QTranslator>

#include <algorithm>
#include <functional>
#include <map>

using namespace kalahari;
using editor::NoteKind;
using gui::CommandRegistry;
using gui::FootnoteCoordinator;
using gui::KeyboardShortcut;
using gui::NoteFrame;
using test::OpenNovel;
using test::Window;

namespace {

/// A key pressed in a widget, with the text it types
void press(QWidget* widget, int key, Qt::KeyboardModifiers modifiers = Qt::NoModifier,
           const QString& text = QString()) {
    QKeyEvent event(QEvent::KeyPress, key, modifiers, text);
    QApplication::sendEvent(widget, &event);
}

/// Letters typed in a widget, one key each
void type(QWidget* widget, const QString& letters) {
    for (const QChar letter : letters) {
        press(widget, letter.toUpper().unicode(), Qt::NoModifier, QString(letter));
    }
}

/// The text box of a frame
QTextEdit* textOf(const NoteFrame& frame) {
    auto* text = frame.findChild<QTextEdit*>(QStringLiteral("noteFrameText"));
    REQUIRE(text != nullptr);
    return text;
}

/// Pastes into a text box what @p mime holds, as Ctrl+V does
class PastingEdit : public QTextEdit {
public:
    static void paste(QTextEdit& edit, const QMimeData& mime) {
        static_cast<PastingEdit&>(edit).insertFromMimeData(&mime);
    }
};

/// The other texts' endnotes of a context, as "number=content"
QStringList endnotesOf(const editor::FootnoteContext& context) {
    QStringList endnotes;
    for (const editor::BookEndnote& endnote : context.otherEndnotes) {
        endnotes << endnote.number + QLatin1Char('=') + endnote.content;
    }
    return endnotes;
}

/// The notes whose texts an editor shows, from the top of the document down, as "id" (an own
/// note) or "text:id" (another text's endnote)
QStringList shownNotesOf(const editor::BookEditor& text) {
    const auto* layout = qobject_cast<const editor::KalahariTextDocumentLayout*>(
        text.textDocument()->documentLayout());
    REQUIRE(layout != nullptr);
    QStringList shown;
    const int height = qCeil(layout->documentSize().height());
    for (int y = 0; y < height; ++y) {
        if (const auto area = layout->noteAreaAt(QPointF(5.0, y))) {
            const QString name = area->elementId.isEmpty()
                                     ? area->id
                                     : area->elementId + QLatin1Char(':') + area->id;
            if (shown.isEmpty() || shown.last() != name) {
                shown << name;
            }
        }
    }
    return shown;
}

/// The notes of an editor, as "kind:content" in text order
QStringList notesOf(const editor::BookEditor& text) {
    QStringList notes;
    for (const editor::NotePlace& place : editor::notesIn(*text.textDocument())) {
        notes << (place.note.kind == NoteKind::Endnote ? QStringLiteral("e:")
                                                       : QStringLiteral("f:")) +
                     place.note.content;
    }
    return notes;
}

}  // namespace

TEST_CASE("Notes: the frame keeps a note's formatting, its links and its paragraphs",
          "[gui][footnotes]") {
    gui::registerAllCommands(gui::CommandCallbacks{});  // the keys of Bold, Italic, Underline
    QWidget editor;
    editor.resize(800, 600);
    auto* frame = new NoteFrame(&editor);
    frame->setNote(NoteKind::Footnote, QStringLiteral("3"));
    CHECK(frame->findChild<QLabel*>(QStringLiteral("noteFrameKind"))->text() ==
          QStringLiteral("Footnote 3"));
    CHECK(frame->findChild<QLabel*>(QStringLiteral("noteFrameHint"))
              ->text()
              .contains(NoteFrame::saveKeysText()));
    frame->setNote(NoteKind::Endnote, QStringLiteral("ii"));
    CHECK(frame->findChild<QLabel*>(QStringLiteral("noteFrameKind"))->text() ==
          QStringLiteral("Endnote ii"));

    // What it starts with comes back as it was
    const QString content = QStringLiteral(
        "<p>One <b>bold</b> and <a href=\"https://example.com\">a link</a></p><p>Two</p>");
    frame->setContent(content);
    CHECK(frame->content() == content);

    // Ctrl+B, Ctrl+I, Ctrl+U on the selection
    QTextEdit* text = textOf(*frame);
    frame->setContent(QStringLiteral("Plain"));
    QTextCursor all = text->textCursor();
    all.select(QTextCursor::Document);
    text->setTextCursor(all);
    press(text, Qt::Key_B, Qt::ControlModifier);
    CHECK(frame->content() == QStringLiteral("<b>Plain</b>"));
    press(text, Qt::Key_I, Qt::ControlModifier);
    press(text, Qt::Key_B, Qt::ControlModifier);
    CHECK(frame->content() == QStringLiteral("<i>Plain</i>"));
    press(text, Qt::Key_U, Qt::ControlModifier);
    press(text, Qt::Key_I, Qt::ControlModifier);
    CHECK(frame->content() == QStringLiteral("<u>Plain</u>"));

    // The keys the user gives Bold (Settings > Keyboard Shortcuts) make it bold in the frame
    // too; its old keys do nothing there
    CommandRegistry::getInstance().setCustomShortcuts(
        {{"format.bold", KeyboardShortcut(Qt::Key_F12, Qt::ControlModifier)}});
    frame->setContent(QStringLiteral("Plain"));
    all.select(QTextCursor::Document);
    text->setTextCursor(all);
    press(text, Qt::Key_B, Qt::ControlModifier);
    CHECK(frame->content() == QStringLiteral("Plain"));
    press(text, Qt::Key_F12, Qt::ControlModifier);
    CHECK(frame->content() == QStringLiteral("<b>Plain</b>"));
    CommandRegistry::getInstance().setCustomShortcuts({});

    // Pasted: only what a note keeps, no picture, no color, no font
    frame->setContent(QString());
    QMimeData pasted;
    pasted.setHtml(QStringLiteral(
        "<p><span style=\"color:#ff0000; font-family:Arial; font-size:20pt\">Red</span> "
        "<b>bold</b><img src=\"x.png\"/></p>"));
    PastingEdit::paste(*text, pasted);
    CHECK(frame->content() == QStringLiteral("Red <b>bold</b>"));

    // Enter: a new paragraph, Shift+Enter a line break
    frame->setContent(QString());
    frame->startTyping();
    type(text, QStringLiteral("a"));
    press(text, Qt::Key_Return, Qt::NoModifier, QStringLiteral("\r"));
    type(text, QStringLiteral("b"));
    press(text, Qt::Key_Return, Qt::ShiftModifier, QStringLiteral("\r"));
    type(text, QStringLiteral("c"));
    CHECK(frame->content() == QStringLiteral("<p>a</p><p>b<br/>c</p>"));
}

TEST_CASE("Notes: Insert > Footnote, the text kept with Ctrl+Enter as one undo step, Esc",
          "[gui][footnotes]") {
    OpenNovel book(QStringLiteral("Notes"));
    Window window;
    window.navigator.onElementSelected(book.chapterId, QStringLiteral("The Storm"));
    editor::BookEditor* text = window.editor(window.tabs->currentIndex())->getBookEditor();
    REQUIRE(text != nullptr);
    text->fromKml(QStringLiteral("<kml><p>Rain fell.</p><p>Then it stopped.</p></kml>"));
    text->setCursorPosition({0, 4});
    FootnoteCoordinator coordinator(window.tabs, [text]() { return text; });
    coordinator.connectCommands();

    // A new note: its number in the text, its frame open with the keys
    REQUIRE(coordinator.insertNote(NoteKind::Footnote));
    NoteFrame* frame = coordinator.frame();
    REQUIRE(frame != nullptr);
    CHECK(frame->findChild<QLabel*>(QStringLiteral("noteFrameKind"))->text() ==
          QStringLiteral("Footnote 1"));
    CHECK(notesOf(*text) == QStringList{QStringLiteral("f:")});
    type(textOf(*frame), QStringLiteral("Cold"));
    press(textOf(*frame), Qt::Key_Return, Qt::ControlModifier);
    QCoreApplication::processEvents();
    CHECK(coordinator.frame() == nullptr);
    CHECK(notesOf(*text) == QStringList{QStringLiteral("f:Cold")});

    // One undo step takes the number and its text
    text->undo();
    CHECK(notesOf(*text).isEmpty());
    CHECK(text->textDocument()->firstBlock().text() == QStringLiteral("Rain fell."));
    text->redo();
    CHECK(notesOf(*text) == QStringList{QStringLiteral("f:Cold")});

    // Esc on a new note: nothing is left, nothing to redo
    text->setCursorPosition({1, 4});
    REQUIRE(coordinator.insertNote(NoteKind::Endnote));
    REQUIRE(coordinator.frame() != nullptr);
    CHECK(coordinator.frame()->findChild<QLabel*>(QStringLiteral("noteFrameKind"))->text() ==
          QStringLiteral("Endnote i"));
    type(textOf(*coordinator.frame()), QStringLiteral("Gone"));
    press(textOf(*coordinator.frame()), Qt::Key_Escape);
    QCoreApplication::processEvents();
    CHECK(coordinator.frame() == nullptr);
    CHECK(notesOf(*text) == QStringList{QStringLiteral("f:Cold")});
    CHECK_FALSE(text->textDocument()->isRedoAvailable());

    // Edited again: Esc drops the changes, the note stays
    const QString id = editor::notesIn(*text->textDocument()).front().note.id;
    REQUIRE(coordinator.editNote(*text, id));
    REQUIRE(coordinator.frame() != nullptr);
    CHECK(textOf(*coordinator.frame())->toPlainText() == QStringLiteral("Cold"));
    type(textOf(*coordinator.frame()), QStringLiteral("er"));
    press(textOf(*coordinator.frame()), Qt::Key_Escape);
    QCoreApplication::processEvents();
    CHECK(notesOf(*text) == QStringList{QStringLiteral("f:Cold")});

    // The text asks for the frame (a click on the note's text): kept when the frame is left
    emit text->noteEditRequested(id);
    REQUIRE(coordinator.frame() != nullptr);
    type(textOf(*coordinator.frame()), QStringLiteral("er"));
    coordinator.finishWriting(text);
    CHECK(coordinator.frame() == nullptr);
    CHECK(notesOf(*text) == QStringList{QStringLiteral("f:Colder")});
    text->undo();
    CHECK(notesOf(*text) == QStringList{QStringLiteral("f:Cold")});

    // A read-only text takes no note
    text->setReadOnly(true);
    CHECK_FALSE(coordinator.insertNote(NoteKind::Footnote));
    CHECK_FALSE(coordinator.editNote(*text, id));
    CHECK(coordinator.frame() == nullptr);
}

// =============================================================================
// The notes of the book
// =============================================================================

namespace {

/// KML of paragraphs
QString kmlOf(const QString& paragraphs) {
    return QStringLiteral("<kml>") + paragraphs + QStringLiteral("</kml>");
}

/// Save a text of the book in its file
void save(OpenNovel& book, const QString& id, const QString& kml) {
    book.pm.setChapterContent(id, kml);
    REQUIRE(book.pm.saveChapterContent(id));
}

/// A book of a foreword, two chapters after the type's first one and an afterword, with
/// footnotes and endnotes
struct NotedBook {
    OpenNovel book{QStringLiteral("Notes")};
    QString foreword;
    QString storm;
    QString calm;
    QString afterword;

    NotedBook() {
        storm = book.chapterId;
        foreword = book.pm.addElement(book.pm.chapterKindFor(core::BookPlace::Front),
                                      QStringLiteral("Foreword"), core::BookPlace::Front);
        calm = book.pm.addElement(book.pm.chapterKindFor(core::BookPlace::Main),
                                  QStringLiteral("The Calm"), core::BookPlace::Main);
        afterword = book.pm.addElement(book.pm.chapterKindFor(core::BookPlace::Back),
                                       QStringLiteral("Afterword"), core::BookPlace::Back);
        REQUIRE_FALSE(foreword.isEmpty());
        REQUIRE_FALSE(calm.isEmpty());
        REQUIRE_FALSE(afterword.isEmpty());
        save(book, foreword,
             kmlOf(QStringLiteral("<p>Fore<footnote id=\"a1\">One</footnote> word"
                                  "<endnote id=\"ae\">Fore end</endnote>.</p>")));
        save(book, storm,
             kmlOf(QStringLiteral("<p>Rain<footnote id=\"s1\">Wet</footnote> fell"
                                  "<footnote id=\"s2\">Hard</footnote>.</p>"
                                  "<p>Wind<endnote id=\"se\">Storm end</endnote>.</p>")));
        save(book, calm,
             kmlOf(QStringLiteral("<p>Calm<footnote id=\"c1\">Still</footnote> came"
                                  "<endnote id=\"ce\">Calm end</endnote>.</p>"
                                  "<p>Sun<endnote id=\"cf\">Calm last</endnote>.</p>")));
        save(book, afterword,
             kmlOf(QStringLiteral("<p>After<endnote id=\"be\">Back end</endnote>.</p>")));
    }

    /// The editor of a text opened in the window
    editor::BookEditor& open(Window& window, const QString& id) const {
        window.navigator.onElementSelected(id, QString());
        editor::BookEditor* text = window.editor(window.tabs->currentIndex())->getBookEditor();
        REQUIRE(text != nullptr);
        return *text;
    }
};

/// The coordinator of the notes over the tabs of a window, as the main window has it
struct Notes {
    FootnoteCoordinator coordinator;

    explicit Notes(Window& window)
        : coordinator(
              window.tabs,
              [&window]() -> editor::BookEditor* {
                  gui::EditorPanel* panel = window.editor(window.tabs->currentIndex());
                  return panel != nullptr ? panel->getBookEditor() : nullptr;
              },
              [&window](const QString& id) { window.navigator.onElementSelected(id, QString()); },
              &window.window) {
        coordinator.connectCommands();
    }
};

}  // namespace

TEST_CASE("Notes in the book: anew in each chapter, through the book, at the end of the book",
          "[gui][footnotes]") {
    NotedBook noted;
    Window window;
    Notes notes(window);

    // The texts in reading order with their notes; the texts of the main kind in the main part
    // are chapters
    const std::vector<FootnoteCoordinator::BookText> texts = notes.coordinator.bookTexts();
    const auto bookText = [&texts](const QString& id) {
        const auto found = std::find_if(texts.cbegin(), texts.cend(),
                                        [&id](const FootnoteCoordinator::BookText& text) {
                                            return text.elementId == id;
                                        });
        REQUIRE(found != texts.cend());
        return found;
    };
    CHECK(bookText(noted.foreword) < bookText(noted.storm));
    CHECK(bookText(noted.storm) < bookText(noted.calm));
    CHECK(bookText(noted.calm) < bookText(noted.afterword));
    CHECK_FALSE(bookText(noted.foreword)->chapter);
    CHECK(bookText(noted.storm)->chapter);
    CHECK(bookText(noted.calm)->chapter);
    CHECK_FALSE(bookText(noted.afterword)->chapter);
    CHECK(bookText(noted.storm)->notes.size() == 3);

    // By default each text from 1, its endnotes after its last paragraph
    CHECK(notes.coordinator.footnotes() == core::BookFootnotes{});
    editor::FootnoteContext context = notes.coordinator.contextFor(noted.calm);
    CHECK(context == editor::FootnoteContext{});

    // Through the book, the endnotes at its end: after the last chapter, in reading order
    core::BookFootnotes through;
    through.footnotes = {QStringLiteral("a"), false, {}};
    through.endnotes = {QStringLiteral("I"), false, {}};
    through.endnotesAtBookEnd = true;
    REQUIRE(notes.coordinator.setFootnotes(through));
    CHECK(noted.book.pm.bookFootnotes() == through);
    context = notes.coordinator.contextFor(noted.calm);
    CHECK(context.footnoteFormat == editor::NoteNumberFormat::LowerAlpha);
    CHECK(context.endnoteFormat == editor::NoteNumberFormat::UpperRoman);
    CHECK(context.footnotesBefore == 3);
    CHECK(context.endnotesBefore == 2);
    CHECK(context.endnotesHere);
    CHECK(endnotesOf(context) == QStringList{QStringLiteral("I=Fore end"),
                                             QStringLiteral("II=Storm end"),
                                             QStringLiteral("V=Back end")});
    CHECK(context.otherEndnotesBefore == 2);
    const editor::FootnoteContext storm = notes.coordinator.contextFor(noted.storm);
    CHECK(storm.footnotesBefore == 1);
    CHECK_FALSE(storm.endnotesHere);
    CHECK(storm.otherEndnotes.empty());

    // The last chapter opened: its numbers go on, the book's endnotes after its last
    // paragraph, its own among them
    editor::BookEditor& calmText = noted.open(window, noted.calm);
    notes.coordinator.refresh();
    CHECK(calmText.noteNumber(QStringLiteral("c1")) == QStringLiteral("d"));
    CHECK(calmText.noteNumber(QStringLiteral("ce")) == QStringLiteral("III"));
    CHECK(calmText.noteNumber(QStringLiteral("cf")) == QStringLiteral("IV"));
    const QString foreword = noted.foreword + QStringLiteral(":ae");
    const QString stormEnd = noted.storm + QStringLiteral(":se");
    const QString back = noted.afterword + QStringLiteral(":be");
    CHECK(shownNotesOf(calmText) == QStringList{QStringLiteral("c1"), foreword, stormEnd,
                                                QStringLiteral("ce"), QStringLiteral("cf"),
                                                back});
    // In the page view after the last line, the footnote at the bottom of the page
    calmText.setViewMode(editor::ViewMode::Page);
    CHECK(shownNotesOf(calmText) == QStringList{foreword, stormEnd, QStringLiteral("ce"),
                                                QStringLiteral("cf"), back,
                                                QStringLiteral("c1")});
    calmText.setViewMode(editor::ViewMode::Continuous);

    // An endnote of another text clicked: that text, the note's frame after its number (its
    // text is at the end of the book)
    emit calmText.bookEndnoteOpenAsked(noted.storm, QStringLiteral("se"));
    editor::BookEditor& stormText = *window.editor(window.tabs->currentIndex())->getBookEditor();
    REQUIRE(&stormText != &calmText);
    CHECK(stormText.noteNumber(QStringLiteral("se")) == QStringLiteral("II"));
    CHECK(shownNotesOf(stormText) == QStringList{QStringLiteral("s1"), QStringLiteral("s2")});
    REQUIRE(notes.coordinator.frame() != nullptr);
    CHECK(notes.coordinator.frame()->findChild<QLabel*>(QStringLiteral("noteFrameKind"))->text() ==
          QStringLiteral("Endnote II"));
    press(textOf(*notes.coordinator.frame()), Qt::Key_Escape);
    QCoreApplication::processEvents();
    CHECK(notes.coordinator.frame() == nullptr);

    // A change of notes in an open text counts for the others once it is announced
    REQUIRE(stormText.removeNote(QStringLiteral("s2")));
    QCoreApplication::processEvents();  // notesChanged()
    notes.coordinator.refresh();
    CHECK(calmText.noteNumber(QStringLiteral("c1")) == QStringLiteral("c"));
    REQUIRE(stormText.setNoteContent(QStringLiteral("se"), QStringLiteral("Storm over")));
    QCoreApplication::processEvents();
    notes.coordinator.refresh();
    CHECK(endnotesOf(calmText.footnoteContext()) ==
          QStringList{QStringLiteral("I=Fore end"), QStringLiteral("II=Storm over"),
                      QStringLiteral("V=Back end")});

    // Anew in each chapter, at the end of the book: the numbers of their texts
    core::BookFootnotes anew = through;
    anew.footnotes.perChapter = true;
    anew.endnotes.perChapter = true;
    REQUIRE(notes.coordinator.setFootnotes(anew));  // refreshes the open texts at once
    CHECK(calmText.noteNumber(QStringLiteral("c1")) == QStringLiteral("a"));
    CHECK(calmText.noteNumber(QStringLiteral("ce")) == QStringLiteral("I"));
    CHECK(endnotesOf(calmText.footnoteContext()) ==
          QStringList{QStringLiteral("I=Fore end"), QStringLiteral("I=Storm over"),
                      QStringLiteral("I=Back end")});

    // At the end of each chapter again: each text shows its own
    anew.endnotesAtBookEnd = false;
    REQUIRE(notes.coordinator.setFootnotes(anew));
    CHECK(calmText.footnoteContext().otherEndnotes.empty());
    CHECK(shownNotesOf(stormText) == QStringList{QStringLiteral("s1"), QStringLiteral("se")});

    // A text outside the book is numbered alone
    CHECK(notes.coordinator.contextFor(QString()) == editor::FootnoteContext{});
    CHECK(notes.coordinator.whyNotEditSettings().isEmpty());
}

namespace {

/// The access key of a text: the letter after a single "&"
QChar accessKey(const QString& text) {
    for (qsizetype i = 0; i + 1 < text.size(); ++i) {
        if (text.at(i) == QLatin1Char('&')) {
            if (text.at(i + 1) == QLatin1Char('&')) {
                ++i;
                continue;
            }
            return text.at(i + 1).toLower();
        }
    }
    return QChar();
}

/// The texts with access keys of a window: of the labels with a field, the buttons, the groups
QStringList keyTexts(const QWidget& root) {
    QStringList texts;
    for (const QWidget* widget : root.findChildren<QWidget*>()) {
        if (const auto* label = qobject_cast<const QLabel*>(widget); label && label->buddy()) {
            texts << label->text();
        } else if (const auto* button = qobject_cast<const QAbstractButton*>(widget)) {
            texts << button->text();
        } else if (const auto* group = qobject_cast<const QGroupBox*>(widget)) {
            texts << group->title();
        }
    }
    return texts;
}

/// The window in Polish while it lives: the translation the program ships
class Polish {
public:
    Polish() {
        m_loaded = m_translator.load(QStringLiteral(":/i18n/kalahari_pl.qm"));
        if (m_loaded) {
            QCoreApplication::installTranslator(&m_translator);
        }
    }
    ~Polish() { QCoreApplication::removeTranslator(&m_translator); }
    Polish(const Polish&) = delete;
    Polish& operator=(const Polish&) = delete;

    [[nodiscard]] bool loaded() const { return m_loaded; }

private:
    QTranslator m_translator;
    bool m_loaded = false;
};

/// The program's way of opening the Settings dialog, until the test ends
class SettingsOpener {
public:
    explicit SettingsOpener(std::function<void(QWidget*, const QString&)> opener) {
        gui::dialogs::KalahariDialog::setSettingsOpener(std::move(opener));
    }
    ~SettingsOpener() { gui::dialogs::KalahariDialog::setSettingsOpener(nullptr); }
    SettingsOpener(const SettingsOpener&) = delete;
    SettingsOpener& operator=(const SettingsOpener&) = delete;
};

}  // namespace

TEST_CASE("Notes in the book: the window of their numbering and place", "[gui][footnotes]") {
    using gui::dialogs::FootnoteSettingsDialog;
    CHECK(FootnoteSettingsDialog::formatSample(QStringLiteral("1")) == QStringLiteral("1, 2, 3"));
    CHECK(FootnoteSettingsDialog::formatSample(QStringLiteral("A")) == QStringLiteral("A, B, C"));
    CHECK(FootnoteSettingsDialog::formatSample(QStringLiteral("i")) ==
          QStringLiteral("i, ii, iii"));
    CHECK(FootnoteSettingsDialog::formatSample(QStringLiteral("*")) ==
          QString::fromUtf8("*, \u2020, \u2021"));

    // What the book has, the fields it does not know kept
    core::BookFootnotes book;
    book.endnotes = {QStringLiteral("1"), false, {}};
    book.extra.insert(QStringLiteral("later"), 1);
    FootnoteSettingsDialog dialog(book);
    CHECK(dialog.footnotes() == book);
    const QList<QComboBox*> lists = dialog.findChildren<QComboBox*>();
    REQUIRE(lists.size() == 5);
    for (QComboBox* list : lists) {
        list->setCurrentIndex(list->count() - 1);  // *, through the book, the end of the book
    }
    const core::BookFootnotes chosen = dialog.footnotes();
    CHECK(chosen.footnotes.format == QStringLiteral("*"));
    CHECK_FALSE(chosen.footnotes.perChapter);
    CHECK(chosen.endnotes.format == QStringLiteral("*"));
    CHECK_FALSE(chosen.endnotes.perChapter);
    CHECK(chosen.endnotesAtBookEnd);
    CHECK(chosen.extra == book.extra);

    // Without a book there is nothing to set
    Window window;
    Notes notes(window);
    CHECK(notes.coordinator.whyNotEditSettings() == QStringLiteral("No book is open"));
    CHECK_FALSE(notes.coordinator.editSettings());
}

TEST_CASE("Notes in the book: the window in Polish, its keys, its gear, a small screen",
          "[gui][footnotes][dialog]") {
    const Polish polish;
    REQUIRE(polish.loaded());
    QString openedPage;
    const SettingsOpener opener(
        [&openedPage](QWidget*, const QString& page) { openedPage = page; });
    gui::dialogs::FootnoteSettingsDialog dialog{core::BookFootnotes{}};
    dialog.show();
    for (int i = 0; i < 5; ++i) {
        QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
        QCoreApplication::processEvents();
    }
    CHECK(dialog.windowTitle() == QStringLiteral("Ustawienia przypisów"));
    const QList<QComboBox*> lists = dialog.findChildren<QComboBox*>();
    REQUIRE(lists.size() == 5);
    CHECK(lists[0]->currentText() == QStringLiteral("1, 2, 3"));
    CHECK(lists[1]->currentText() == QStringLiteral("W każdym rozdziale od 1"));
    CHECK(lists[2]->currentText() == QStringLiteral("Koniec rozdziału"));
    CHECK(lists[3]->currentText() == QStringLiteral("i, ii, iii"));
    CHECK(dialog.focusWidget() == lists[0]);  // also where the window is not active

    // Each key once, Latin letters only (Alt does not reach "ś")
    std::map<QChar, QStringList> byKey;
    for (const QString& text : keyTexts(dialog)) {
        const QChar key = accessKey(text);
        if (!key.isNull()) {
            CHECK((key.unicode() < 0x80 && key.isLetterOrNumber()));
            byKey[key] << text;
        }
    }
    for (const auto& [key, texts] : byKey) {
        INFO(texts.join(QStringLiteral(" / ")).toStdString());
        CHECK(texts.size() == 1);
    }

    // The gear: Settings > Editor > General, where the words of the notes are counted
    QToolButton* gear = nullptr;
    for (QToolButton* each : dialog.findChildren<QToolButton*>()) {
        if (each->toolTip() == QStringLiteral("Ustawienia edytora")) {
            gear = each;
        }
    }
    REQUIRE(gear != nullptr);
    gear->click();
    CHECK(openedPage == QString::fromLatin1(gui::SettingsDialog::WORD_COUNT_PAGE));

    // 1366x768 at 150%: 911 pixels wide and 460 high (a Mac's larger controls need a few more)
#ifdef Q_OS_MACOS
    constexpr int MAX_HEIGHT = 480;
#else
    constexpr int MAX_HEIGHT = 460;
#endif
    CHECK(dialog.sizeHint().width() <= 911);
    CHECK(dialog.sizeHint().height() <= MAX_HEIGHT);
}
