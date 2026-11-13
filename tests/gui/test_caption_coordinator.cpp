/// @file test_caption_coordinator.cpp
/// @brief The captions of the book: the chapters of the main part and the numbers of the
///        captions of every text, open or saved, the Insert commands and the question before
///        a caption with cross-references goes; one filter for every image of the book, in the
///        open texts and the saved ones, with Undo

#include <catch2/catch_test_macros.hpp>
#include "document_window.h"
#include "kalahari/core/book_project.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/editor/text_captions.h"
#include "kalahari/editor/text_images.h"
#include "kalahari/gui/caption_coordinator.h"
#include "kalahari/gui/caption_settings.h"
#include "kalahari/gui/dialogs/caption_dialogs.h"
#include "kalahari/gui/image_filter_coordinator.h"

#include <QFile>
#include <QStringList>
#include <QTextDocument>
#include <QTimer>

#include <algorithm>
#include <functional>

using namespace kalahari;
using gui::CaptionCoordinator;
using gui::ImageFilterCoordinator;
using test::OpenNovel;
using test::Window;

namespace {

/// An image of its own paragraph with a numbered caption under it
QString figure(const QString& id, const QString& label = QStringLiteral("Figure"),
               const QString& filter = QString())
{
    const QString look = filter.isEmpty() ? QString() : QStringLiteral(" filter=\"%1\"").arg(filter);
    return QStringLiteral("<p align=\"center\"><img src=\"images/%1.png\"%2/></p>"
                          "<p style=\"caption\" align=\"center\">%3 <caption-number label=\"%3\" "
                          "id=\"%1\"/>. Caption %1</p>")
        .arg(id, look, label);
}

/// KML of paragraphs
QString kmlOf(const QString& paragraphs)
{
    return QStringLiteral("<kml>") + paragraphs + QStringLiteral("</kml>");
}

/// Save a text of the book in its file
void save(OpenNovel& book, const QString& id, const QString& kml)
{
    book.pm.setChapterContent(id, kml);
    REQUIRE(book.pm.saveChapterContent(id));
}

/// The file of a text of the book, as it is on the disk
QString fileOf(const OpenNovel& book, const QString& id)
{
    const core::ProjectElement* element = book.pm.findElement(id);
    REQUIRE(element != nullptr);
    QFile file(book.pm.filePathOf(*element));
    return file.open(QIODevice::ReadOnly) ? QString::fromUtf8(file.readAll()) : QString();
}

/// The texts every field of the editor shows, in text order, parted by "|"
std::string shownFields(const editor::BookEditor& text)
{
    const QTextDocument& document = *text.textDocument();
    QStringList texts;
    for (const QString& shown :
         editor::fieldTexts(document, text.captionNumbers(), 0, document.characterCount())) {
        texts << shown;
    }
    return texts.join(QLatin1Char('|')).toStdString();
}

/// The numbers of captions of a context, in their order: "a1=1"
QStringList numbersOf(const std::vector<editor::BookCaption>& captions)
{
    QStringList numbers;
    for (const editor::BookCaption& caption : captions) {
        numbers << caption.id + QLatin1Char('=') + caption.number;
    }
    return numbers;
}

/// A book of four texts: a foreword, two chapters and notes in the back part, each with
/// captions; the second chapter names a figure of the first
struct CaptionedBook {
    OpenNovel book{QStringLiteral("Captions")};
    QString foreword;
    QString storm;
    QString calm;
    QString notes;

    CaptionedBook()
    {
        storm = book.chapterId;
        foreword = book.pm.addElement(book.pm.chapterKindFor(core::BookPlace::Front),
                                      QStringLiteral("Foreword"), core::BookPlace::Front);
        calm = book.pm.addElement(book.pm.chapterKindFor(core::BookPlace::Main),
                                  QStringLiteral("The Calm"), core::BookPlace::Main);
        notes = book.pm.addElement(book.pm.chapterKindFor(core::BookPlace::Back),
                                   QStringLiteral("Notes"), core::BookPlace::Back);
        REQUIRE_FALSE(foreword.isEmpty());
        REQUIRE_FALSE(calm.isEmpty());
        REQUIRE_FALSE(notes.isEmpty());
        save(book, foreword, kmlOf(figure(QStringLiteral("a1"))));
        save(book, storm,
             kmlOf(figure(QStringLiteral("b1")) + figure(QStringLiteral("m1"), QStringLiteral("Map")) +
                   figure(QStringLiteral("b2"))));
        save(book, calm,
             kmlOf(figure(QStringLiteral("c1")) +
                   QStringLiteral("<p>See <cross-reference to=\"b2\"/>.</p>")));
        save(book, notes, kmlOf(figure(QStringLiteral("d1"))));
    }

    /// The editor of a text opened in the window
    editor::BookEditor& open(Window& window, const QString& id, const QString& title) const
    {
        window.navigator.onElementSelected(id, title);
        editor::BookEditor* text = window.editor(window.tabs->currentIndex())->getBookEditor();
        REQUIRE(text != nullptr);
        return *text;
    }
};

/// The coordinator of the captions over the tabs of a window, as the main window has it
struct Captions {
    CaptionCoordinator coordinator;

    explicit Captions(Window& window)
        : coordinator(
              window.tabs,
              [&window](const QString& id) { window.navigator.onElementSelected(id, QString()); },
              &window.window) {}
};

/// The coordinator of the filter over the tabs of a window, as the main window has it: the
/// notices under the text and the questions it asks are kept
struct Filters {
    QStringList notices;
    QList<std::function<void()>> undoButtons;
    QStringList asked;
    bool answer = true;
    ImageFilterCoordinator coordinator;

    explicit Filters(Window& window)
        : coordinator(
              window.tabs,
              [this](gui::EditorPanel*, const QString& message, const QString&, const QString&,
                     const std::function<void()>& action) {
                  notices.append(message);
                  undoButtons.append(action);
              },
              &window.statusBar, &window.window)
    {
        coordinator.setConfirmer([this](int count, const QString& name) {
            asked << QStringLiteral("%1 %2").arg(count).arg(name);
            return answer;
        });
    }
};

/// The position of the first image of an editor
int firstImage(const editor::BookEditor& text)
{
    const QTextDocument& document = *text.textDocument();
    const std::vector<int> images = editor::imagePositions(document, 0, document.characterCount());
    REQUIRE_FALSE(images.empty());
    return images.front();
}

}  // namespace

// =============================================================================
// The captions of the book
// =============================================================================

TEST_CASE("Caption coordinator: the chapters of the main part and the numbers of the book",
          "[gui][captions]") {
    CaptionedBook captioned;
    Window window;
    Captions captions(window);

    // The texts in reading order (the type's title page and first chapter first); only the
    // texts of the main kind in the main part are chapters, and the back part counts anew
    const std::vector<CaptionCoordinator::BookText> texts = captions.coordinator.bookTexts();
    const auto textOf = [&texts](const QString& id) {
        const auto found = std::find_if(texts.cbegin(), texts.cend(),
                                        [&id](const CaptionCoordinator::BookText& text) {
                                            return text.elementId == id;
                                        });
        REQUIRE(found != texts.cend());
        return found;
    };
    const auto foreword = textOf(captioned.foreword);
    const auto storm = textOf(captioned.storm);
    const auto calm = textOf(captioned.calm);
    const auto notes = textOf(captioned.notes);
    CHECK(foreword < storm);
    CHECK(storm < calm);
    CHECK(calm < notes);
    CHECK(notes == texts.cend() - 1);
    CHECK(foreword->chapter == 0);
    CHECK_FALSE(foreword->chapterStart);
    CHECK(storm->chapter == 2);  // after the type's Chapter 1
    CHECK(storm->chapterStart);
    CHECK(storm->entries.size() == 3);
    CHECK(calm->chapter == 3);
    CHECK(calm->references.value(QStringLiteral("b2")) == 1);
    CHECK(notes->chapter == 0);
    CHECK(notes->backStart);
    CHECK(captions.coordinator.referenceCount({QStringLiteral("b2")}) == 1);
    CHECK(captions.coordinator.referenceCount({QStringLiteral("b1")}) == 0);

    // Through the whole book: each label counted on its own
    editor::CaptionContext context = captions.coordinator.contextFor(captioned.calm);
    CHECK(context.chapter == 3);
    CHECK(numbersOf(context.before) == QStringList{QStringLiteral("a1=1"), QStringLiteral("b1=2"),
                                                   QStringLiteral("m1=1"), QStringLiteral("b2=3")});
    CHECK(numbersOf(context.after) == QStringList{QStringLiteral("d1=5")});
    CHECK(context.counted.value(QStringLiteral("Figure")) == 3);
    CHECK(context.counted.value(QStringLiteral("Map")) == 1);

    // The text opened: its fields show the numbers of the book
    editor::BookEditor& calmText = captioned.open(window, captioned.calm, QStringLiteral("The Calm"));
    captions.coordinator.refresh();
    CHECK(shownFields(calmText) == "4|3");

    // The book's own numbering: figures anew in each chapter, after its number
    core::BookCaptions own = captions.coordinator.captions();
    CHECK(own.labels == gui::settingsCaptionLabels());
    core::CaptionLabelNumbering chapters;
    chapters.label = QStringLiteral("Figure");
    chapters.perChapter = true;
    chapters.withChapter = true;
    own.numbering = {chapters};
    REQUIRE(captions.coordinator.setCaptions(own));
    CHECK(captioned.book.pm.bookCaptions() == own);
    CHECK(shownFields(calmText) == "3.1|2.2");
    context = captions.coordinator.contextFor(captioned.calm);
    CHECK(numbersOf(context.before) == QStringList{QStringLiteral("a1=1"), QStringLiteral("b1=2.1"),
                                                   QStringLiteral("m1=1"), QStringLiteral("b2=2.2")});
    CHECK(numbersOf(context.after) == QStringList{QStringLiteral("d1=1")});

    // A change of the open text counts at once for the others
    editor::BookEditor& stormText = captioned.open(window, captioned.storm, QStringLiteral("The Storm"));
    captions.coordinator.refresh();
    CHECK(shownFields(stormText) == "2.1|1|2.2");
    REQUIRE(stormText.setImageCaption(firstImage(stormText), editor::ImageCaption{}));
    captions.coordinator.refresh();
    CHECK(shownFields(stormText) == "1|2.1");
    CHECK(shownFields(calmText) == "3.1|2.1");

    // A text outside the book is numbered alone
    const editor::CaptionContext outside = captions.coordinator.contextFor(QString());
    CHECK(outside.before.empty());
    CHECK(outside.chapter == 0);
}

TEST_CASE("Caption coordinator: Insert Caption, Cross-Reference and List of Figures",
          "[gui][captions]") {
    OpenNovel book(QStringLiteral("Captions"));
    Window window;
    Captions captions(window);

    // Without a text in front the commands wait
    CHECK(captions.coordinator.whyNotInsert() == QStringLiteral("No text is in front"));
    CHECK(captions.coordinator.editorInFront() == nullptr);

    save(book, book.chapterId, kmlOf(QStringLiteral("<p>Text</p><p align=\"center\"><img "
                                                    "src=\"images/a.png\"/></p>")));
    window.navigator.onElementSelected(book.chapterId, QStringLiteral("The Storm"));
    editor::BookEditor* text = captions.coordinator.editorInFront();
    REQUIRE(text != nullptr);
    CHECK(captions.coordinator.whyNotInsert().isEmpty());

    // Insert Caption without an image at the cursor, a cross-reference without captions: the
    // window says why
    text->setCursorPosition({0, 2});
    QStringList said;
    CHECK(test::accepting([&]() { captions.coordinator.insertCaption(); }, &said) == 1);
    REQUIRE(said.size() == 1);
    CHECK(said.first().contains(QStringLiteral("Select an image, or put the cursor just after it")));
    said.clear();
    CHECK(test::accepting([&]() { captions.coordinator.insertCrossReference(); }, &said) == 1);
    REQUIRE(said.size() == 1);
    CHECK(said.first().contains(QStringLiteral("The book has no numbered captions yet.")));

    // Insert Caption with the cursor after the image: its caption, and the book keeps the
    // labels of the Settings from then on
    CHECK_FALSE(book.pm.bookCaptions().has_value());
    text->setCursorPosition({1, 1});
    REQUIRE(text->captionTargetImage().has_value());
    QTimer::singleShot(0, []() {
        auto* dialog = qobject_cast<gui::dialogs::InsertCaptionDialog*>(test::shownDialog());
        CHECK(dialog != nullptr);
        if (dialog == nullptr) {
            if (QDialog* other = test::shownDialog()) {
                other->reject();
            }
            return;
        }
        CHECK(dialog->caption().label == gui::settingsNewImageOptions().captionLabel);
        dialog->acceptButton()->click();
    });
    captions.coordinator.insertCaption();
    const std::vector<editor::CaptionEntry> entries = text->captionEntries();
    REQUIRE(entries.size() == 1);
    CHECK(entries.front().label == gui::settingsNewImageOptions().captionLabel);
    REQUIRE(book.pm.bookCaptions().has_value());
    CHECK(book.pm.bookCaptions()->labels == gui::settingsCaptionLabels());

    // A cross-reference to it, chosen first in its window
    text->setCursorPosition({0, 4});
    QTimer::singleShot(0, []() {
        auto* dialog = qobject_cast<gui::dialogs::InsertCrossReferenceDialog*>(test::shownDialog());
        CHECK(dialog != nullptr);
        if (dialog == nullptr) {
            if (QDialog* other = test::shownDialog()) {
                other->reject();
            }
            return;
        }
        dialog->acceptButton()->click();
    });
    captions.coordinator.insertCrossReference();
    CHECK(editor::crossReferenceCounts(*text->textDocument()).value(entries.front().id) == 1);

    // The list of figures under its heading
    text->setCursorPosition({0, 0});
    captions.coordinator.insertFigureList();
    CHECK(text->toKml().startsWith(
        QStringLiteral("<kml><h2>List of Figures</h2><list-of-figures/><p>Text")));
}

TEST_CASE("Caption coordinator: an image whose caption another text names asks first",
          "[gui][captions]") {
    CaptionedBook captioned;
    Window window;
    Captions captions(window);
    editor::BookEditor& storm = captioned.open(window, captioned.storm, QStringLiteral("The Storm"));
    captions.coordinator.refresh();
    const QString before = storm.toKml();

    // b2 has a cross-reference in The Calm: Cancel keeps the image
    const std::vector<int> images =
        editor::imagePositions(*storm.textDocument(), 0, storm.textDocument()->characterCount());
    REQUIRE(images.size() == 3);
    QStringList said;
    CHECK(test::answering(
              [&]() {
                  storm.selectImage(images[2]);
                  storm.deleteSelectedText();
              },
              gui::dialogs::MessageDialog::Answer::Cancel, said) == 1);
    REQUIRE(said.size() == 1);
    CHECK(said.first().contains(QStringLiteral("Delete Caption")));
    CHECK(said.first().contains(
        QStringLiteral("The cross-references to this caption (1) will show \"?\" in place of its "
                       "number.")));
    CHECK(storm.toKml() == before);

    // b1 has none: it goes without asking
    CHECK(test::accepting([&]() {
              storm.selectImage(images[0]);
              storm.deleteSelectedText();
          }) == 0);
    CHECK(storm.captionEntries().size() == 2);
}

// =============================================================================
// One filter for every image of the book
// =============================================================================

TEST_CASE("Same filter for all images: the open texts, the saved ones, with Undo",
          "[gui][images][filter]") {
    OpenNovel book(QStringLiteral("Filters"));
    const QString calm = book.pm.addElement(book.pm.chapterKindFor(core::BookPlace::Main),
                                            QStringLiteral("The Calm"), core::BookPlace::Main);
    const QString notes = book.pm.addElement(book.pm.chapterKindFor(core::BookPlace::Back),
                                             QStringLiteral("Notes"), core::BookPlace::Back);
    REQUIRE_FALSE(calm.isEmpty());
    REQUIRE_FALSE(notes.isEmpty());
    // The open text: a sepia image and one as its file; a saved text with an image; a text
    // with changes not saved yet, with an image, and one already sepia
    const QString stormText = kmlOf(QStringLiteral(
        "<p><img src=\"images/a.png\" filter=\"sepia\" brightness=\"20%\"/></p>"
        "<p><img src=\"images/b.png\" contrast=\"10%\"/></p>"));
    const QString calmText = kmlOf(QStringLiteral("<p>Calm <img src=\"images/c.png\"/></p>"));
    const QString notesText = kmlOf(QStringLiteral(
        "<p><img src=\"images/d.png\" filter=\"grayscale\"/></p>"
        "<p><img src=\"images/e.png\" filter=\"sepia\"/></p>"));
    save(book, book.chapterId, stormText);
    save(book, calm, calmText);
    save(book, notes, kmlOf(QStringLiteral("<p>Saved</p>")));
    book.pm.setChapterContent(notes, notesText);

    Window window;
    Filters filters(window);
    window.navigator.onElementSelected(book.chapterId, QStringLiteral("The Storm"));
    editor::BookEditor* storm = window.editor(window.tabs->currentIndex())->getBookEditor();
    REQUIRE(storm != nullptr);
    REQUIRE(storm->toKml() == stormText);
    const int sepia = firstImage(*storm);

    // Not on an image: nothing
    CHECK(filters.coordinator.applyToBook(*storm, sepia + 1) == -1);

    // The writer says no: nothing changes
    filters.answer = false;
    CHECK(filters.coordinator.applyToBook(*storm, sepia) == 0);
    CHECK(filters.asked == QStringList{QStringLiteral("3 Sepia")});
    CHECK(storm->toKml() == stormText);
    CHECK(book.pm.chapterText(calm) == calmText);
    CHECK_FALSE(filters.coordinator.canUndo());

    // Yes: the open text in its editor, the saved one in its file, the one with changes not
    // saved yet only in the program; the notice under the text, with Undo
    filters.answer = true;
    filters.asked.clear();
    CHECK(filters.coordinator.applyToBook(*storm, sepia) == 3);
    CHECK(filters.asked == QStringList{QStringLiteral("3 Sepia")});
    CHECK(storm->toKml() == kmlOf(QStringLiteral(
                                "<p><img src=\"images/a.png\" filter=\"sepia\" brightness=\"20%\"/></p>"
                                "<p><img src=\"images/b.png\" filter=\"sepia\" contrast=\"10%\"/></p>")));
    CHECK(book.pm.chapterText(calm).contains(QStringLiteral("<img src=\"images/c.png\" filter=\"sepia\"/>")));
    CHECK(fileOf(book, calm).contains(QStringLiteral("sepia")));
    const std::vector<QString> dirty = book.pm.getDirtyElements();
    CHECK(std::find(dirty.cbegin(), dirty.cend(), calm) == dirty.cend());
    CHECK(std::find(dirty.cbegin(), dirty.cend(), notes) != dirty.cend());
    CHECK(book.pm.chapterText(notes).contains(QStringLiteral("<img src=\"images/d.png\" filter=\"sepia\"/>")));
    CHECK_FALSE(fileOf(book, notes).contains(QStringLiteral("sepia")));
    CHECK(filters.notices == QStringList{QStringLiteral("3 images have the filter \"Sepia\" now.")});

    // Every image has it already: nothing to ask
    filters.asked.clear();
    CHECK(filters.coordinator.applyToBook(*storm, sepia) == 0);
    CHECK(filters.asked.isEmpty());
    CHECK(window.statusBar.currentMessage() ==
          QStringLiteral("Every image already has the filter \"Sepia\"."));

    // Undo of the notice: every text as it was
    REQUIRE(filters.coordinator.canUndo());
    REQUIRE(filters.undoButtons.size() == 1);
    filters.undoButtons.first()();
    CHECK(storm->toKml() == stormText);
    CHECK(book.pm.chapterText(calm) == calmText);
    CHECK_FALSE(fileOf(book, calm).contains(QStringLiteral("sepia")));
    CHECK(book.pm.chapterText(notes) == notesText);
    CHECK(window.statusBar.currentMessage() == QStringLiteral("The images have their filters back."));
    CHECK_FALSE(filters.coordinator.canUndo());

    // Again, and the open text written in: the change is final
    CHECK(filters.coordinator.applyToBook(*storm, sepia) == 3);
    REQUIRE(filters.coordinator.canUndo());
    storm->setCursorPosition({0, 0});
    storm->insertText(QStringLiteral("x"));
    CHECK_FALSE(filters.coordinator.canUndo());
    CHECK_FALSE(filters.coordinator.undo());
    CHECK(window.statusBar.currentMessage() ==
          QStringLiteral("This can no longer be undone: the texts changed since."));
    CHECK(book.pm.chapterText(calm).contains(QStringLiteral("filter=\"sepia\"")));
}

TEST_CASE("Same filter for all images: Black and White takes its threshold",
          "[gui][images][filter]") {
    OpenNovel book(QStringLiteral("Filters"));
    save(book, book.chapterId,
         kmlOf(QStringLiteral("<p><img src=\"images/a.png\" filter=\"black-white\" "
                              "threshold=\"70%\"/></p><p><img src=\"images/b.png\" "
                              "filter=\"black-white\"/></p>")));
    Window window;
    Filters filters(window);
    window.navigator.onElementSelected(book.chapterId, QStringLiteral("The Storm"));
    editor::BookEditor* storm = window.editor(window.tabs->currentIndex())->getBookEditor();
    REQUIRE(storm != nullptr);
    CHECK(filters.coordinator.applyToBook(*storm, firstImage(*storm)) == 1);
    CHECK(filters.asked == QStringList{QStringLiteral("1 Black and White")});
    CHECK(storm->toKml().count(QStringLiteral("threshold=\"70%\"")) == 2);
    CHECK(filters.coordinator.canUndo());
    storm->undo();  // its own Undo: the change of the book is final
    CHECK_FALSE(filters.coordinator.canUndo());
    CHECK(ImageFilterCoordinator::filterName(editor::ImageFilter::None) == QStringLiteral("Original"));
    CHECK(ImageFilterCoordinator::filterName(editor::ImageFilter::Wash) == QStringLiteral("Washout"));
}
