/// @file test_new_book.cpp
/// @brief A new book: the palette of the kinds of a user project, the window of its kinds, and
/// a new book created without opening while another book is open

#include <catch2/catch_test_macros.hpp>
#include <kalahari/core/book_project.h>
#include <kalahari/core/book_type_registry.h>
#include <kalahari/core/project_manager.h>
#include <kalahari/core/recent_books_manager.h>
#include <kalahari/gui/dialogs/message_dialog.h>
#include <kalahari/gui/dialogs/new_item_dialog.h>
#include <kalahari/gui/dialogs/user_project_dialog.h>
#include <kalahari/gui/widgets/kind_palette.h>

#include "document_window.h"

#include <QCheckBox>
#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMouseEvent>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTimer>
#include <QTranslator>

#include <algorithm>
#include <functional>
#include <optional>
#include <string>

using namespace kalahari;
using core::BookPlace;
using core::KindRef;
using core::PaletteChoice;
using gui::KindPalette;
using gui::dialogs::MessageDialog;
using gui::dialogs::NewBookOpening;
using gui::dialogs::NewItemDialog;
using gui::dialogs::UserProjectDialog;

namespace {

/// @brief The book types of the resources folder
const core::BookTypeRegistry& builtInTypes() {
    static const core::BookTypeRegistry registry = [] {
        core::BookTypeRegistry loaded;
        loaded.load({QStringLiteral(KALAHARI_SOURCE_DIR "/resources/booktypes")});
        return loaded;
    }();
    return registry;
}

/// @brief The ids of @p kinds: "title_page, chapter"
std::string idsOf(const QList<KindRef>& kinds) {
    QStringList ids;
    for (const KindRef& kind : kinds) {
        ids << kind.kind->id;
    }
    return ids.join(QStringLiteral(", ")).toStdString();
}

/// @brief The packages of @p kinds, each once: "kalahari.base, kalahari.novel"
std::string packagesOf(const QList<KindRef>& kinds) {
    QStringList ids;
    for (const KindRef& kind : kinds) {
        if (!ids.contains(kind.package->id)) {
            ids << kind.package->id;
        }
    }
    return ids.join(QStringLiteral(", ")).toStdString();
}

/// @brief The elements of @p choice and their places: "title_page in front, chapter in main"
std::string elementsOf(const PaletteChoice& choice) {
    QStringList elements;
    for (const core::StartElement& element : choice.elements) {
        elements << element.kind.kind->id + QStringLiteral(" in ") +
                        core::bookPlaceName(element.place);
    }
    return elements.join(QStringLiteral(", ")).toStdString();
}

/// @brief The titles of @p elements: "Prologue, Chapter 1"
std::string titlesOf(const QList<core::ProjectElement>& elements) {
    QStringList titles;
    for (const core::ProjectElement& element : elements) {
        titles << element.title;
    }
    return titles.join(QStringLiteral(", ")).toStdString();
}

/// @brief A tile of the palette: the box that checks its kind, and its "Create now"
struct Tile {
    QWidget* tile = nullptr;
    QCheckBox* box = nullptr;
    QCheckBox* now = nullptr;
};

/// @brief The tiles of the kind named @p name ("Table of contents") on @p palette, in the order
/// of the groups
QList<Tile> tilesOf(const QWidget& palette, const QString& name) {
    QList<Tile> tiles;
    for (QCheckBox* box : palette.findChildren<QCheckBox*>()) {
        if (box->accessibleName() != name) {
            continue;
        }
        Tile tile{box->parentWidget(), box, nullptr};
        for (QCheckBox* now : tile.tile->findChildren<QCheckBox*>()) {
            if (now->text() == QStringLiteral("Create now")) {
                tile.now = now;
            }
        }
        tiles.append(tile);
    }
    return tiles;
}

/// @brief The only tile of the kind named @p name on @p palette, or an empty one
Tile tileOf(const QWidget& palette, const QString& name) {
    const QList<Tile> tiles = tilesOf(palette, name);
    CHECK(tiles.size() == 1);
    return tiles.size() == 1 ? tiles.first() : Tile{};
}

/// @brief A click on the name of the kind of @p tile, as the writer clicks a tile
void clickName(const Tile& tile) {
    QLabel* name = nullptr;
    for (QLabel* label : tile.tile->findChildren<QLabel*>()) {
        if (label->text() == tile.box->accessibleName()) {
            name = label;
        }
    }
    REQUIRE(name != nullptr);
    const QPointF center = QRectF(name->rect()).center();
    QMouseEvent press(QEvent::MouseButtonPress, center, name->mapToGlobal(center), Qt::LeftButton,
                      Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(name, &press);
    QMouseEvent release(QEvent::MouseButtonRelease, center, name->mapToGlobal(center),
                        Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QCoreApplication::sendEvent(name, &release);
}

/// @brief The labels of a dialog that show a text, also while the dialog is hidden
bool showsText(const QDialog& dialog, const QString& text) {
    for (const QLabel* label : dialog.findChildren<QLabel*>()) {
        if (label->isVisibleTo(&dialog) && label->text().contains(text)) {
            return true;
        }
    }
    return false;
}

/// @brief A click on the link @p text over the palette
void clickLink(const QDialog& dialog, const QString& text) {
    const QString link = QStringLiteral(">%1<").arg(text.toHtmlEscaped());
    for (QLabel* label : dialog.findChildren<QLabel*>()) {
        if (label->text().contains(link)) {
            emit label->linkActivated(QStringLiteral("#"));
            return;
        }
    }
    FAIL_CHECK("No link " << text.toStdString());
}

// ----------------------------------------------------------------------------
// The windows of a command
// ----------------------------------------------------------------------------

/// @brief What the writer does in one window a command shows: whether a window is the one, and
/// what is done in it. A step presses buttons with later(), so that the windows they open get
/// their own steps.
struct Step {
    std::function<bool(const QDialog&)> is;
    std::function<void(QDialog&)> act;
};

/// @brief Whether @p dialog is a window of class @p Window
template <typename Window>
bool is(const QDialog& dialog) {
    return qobject_cast<const Window*>(&dialog) != nullptr;
}

/// @brief Press @p button after the step that presses it, in the loop of its window
void later(QAbstractButton* button) {
    CHECK(button != nullptr);
    if (button) {
        QMetaObject::invokeMethod(button, "click", Qt::QueuedConnection);
    }
}

/// @brief How long a window that is not the window of the next step stays, in ticks of 10 ms
constexpr int GIVE_UP_TICKS = 500;

/// @brief Run @p command, doing @p steps in the windows it shows, in their order; a window that
/// is not the window of the next step is closed after a while, and its texts go to
/// @p unexpected
/// @return The number of the steps done
qsizetype doing(const std::function<void()>& command, const QList<Step>& steps,
                QStringList& unexpected) {
    qsizetype done = 0;
    int waited = 0;
    QTimer timer;
    timer.setInterval(10);
    QObject::connect(&timer, &QTimer::timeout, [&]() {
        QDialog* dialog = test::shownDialog();
        if (!dialog) {
            waited = 0;
            return;
        }
        if (done < steps.size() && steps.at(done).is(*dialog)) {
            waited = 0;
            const Step& step = steps.at(done);
            ++done;
            step.act(*dialog);
            return;
        }
        // The window of the last step may still be closing
        if (++waited < GIVE_UP_TICKS) {
            return;
        }
        waited = 0;
        unexpected.append(test::textsOf(*dialog));
        dialog->reject();
    });
    timer.start();
    command();
    return done;
}

/// @brief In the New Book window: choose template @p templateId, and type the title @p title
/// and the folder @p folder
void fillNewBook(QDialog& dialog, const char* templateId, const QString& title,
                 const QString& folder) {
    auto* templates = dialog.findChild<QListWidget*>();
    CHECK(templates != nullptr);
    for (int i = 0; templates && i < templates->count(); ++i) {
        if (templates->item(i)->data(Qt::UserRole).toString() == QLatin1String(templateId)) {
            templates->setCurrentRow(i);
        }
    }
    for (QLineEdit* field : dialog.findChildren<QLineEdit*>()) {
        if (field->placeholderText() == QStringLiteral("Enter book title...")) {
            field->setText(title);
        } else if (field->placeholderText() == QStringLiteral("Select book folder...")) {
            field->setText(folder);
        }
    }
}

/// @brief The title field of the New Book window
QLineEdit* titleField(const QDialog& dialog) {
    for (QLineEdit* field : dialog.findChildren<QLineEdit*>()) {
        if (field->placeholderText() == QStringLiteral("Enter book title...")) {
            return field;
        }
    }
    return nullptr;
}

/// @brief The button of the New Book window that creates the book, or goes on to the next
/// window
QPushButton* createButton(const QDialog& dialog) {
    const auto* box = dialog.findChild<QDialogButtonBox*>();
    if (!box) {
        return nullptr;
    }
    for (QAbstractButton* button : box->buttons()) {
        if (box->buttonRole(button) == QDialogButtonBox::AcceptRole) {
            return qobject_cast<QPushButton*>(button);
        }
    }
    return nullptr;
}

/// @brief The text of @p button; empty without a button
QString buttonText(const QAbstractButton* button) {
    CHECK(button != nullptr);
    return button ? button->text() : QString();
}

/// @brief The text of @p field; empty without a field
QString fieldText(const QLineEdit* field) {
    CHECK(field != nullptr);
    return field ? field->text() : QString();
}

/// @brief Whether the Dashboard of @p window has the card of the book @p manifest
bool hasCard(const test::Window& window, const QString& manifest) {
    const QList<QWidget*> cards =
        window.dashboard->findChildren<QWidget*>(QStringLiteral("recentFileCard"));
    return std::any_of(cards.cbegin(), cards.cend(), [&manifest](const QWidget* card) {
        return QFileInfo(card->property("filePath").toString()) == QFileInfo(manifest);
    });
}

/// @brief The first of the recent books
QString firstRecentBook() {
    const QStringList books = core::RecentBooksManager::getInstance().getRecentFiles();
    return books.isEmpty() ? QString() : books.first();
}

/// @brief The project in the file @p manifest
core::BookProject savedProject(const QString& manifest) {
    QStringList problems;
    std::optional<core::BookProject> project = core::BookProject::load(manifest, problems);
    INFO(problems.join(QStringLiteral("; ")).toStdString());
    REQUIRE(project.has_value());
    return *project;
}

/// @brief "Create now" in Polish, longer than in English
class PolishCreateNow : public QTranslator {
public:
    QString translate(const char* context, const char* sourceText, const char* /*disambiguation*/,
                      int /*n*/) const override {
        if (QLatin1String(context) == QLatin1String("KindPalette") &&
            QLatin1String(sourceText) == QLatin1String("Create now")) {
            return QStringLiteral("Utwórz od razu");
        }
        return {};
    }
    bool isEmpty() const override { return false; }
};

} // namespace

// ============================================================================
// The palette
// ============================================================================

TEST_CASE("Palette: the kinds of all book types, each in the parts of the book it can be in",
          "[gui][palette]") {
    const KindPalette palette(builtInTypes());

    // The kinds of "Base" first, then those of the types, each type in the order of its lists;
    // a table of contents can be at the beginning and at the end of a book
    CHECK(idsOf(palette.kindsIn(BookPlace::Front)) ==
          "title_page, copyright, dedication, motto, toc, preface, introduction, abbreviations");
    CHECK(idsOf(palette.kindsIn(BookPlace::Main)) ==
          "chapter, part, conclusion, prologue, epilogue, cycle, poem, act, episode, story, "
          "section");
    CHECK(idsOf(palette.kindsIn(BookPlace::Back)) ==
          "toc, afterword, acknowledgments, glossary, about_author, annex, endnotes, "
          "bibliography, index, illustrations, explanatory_notes, first_lines, publisher_note, "
          "contributors, first_publications");
    CHECK(idsOf(palette.kindsIn(BookPlace::Workshop)) ==
          "mindmap, timeline, character, location, item, work_note, quote, web_page, picture, "
          "document, publication, recording, other_file, source, draft, logline, synopsis, "
          "treatment, step_outline");

    // A tile shows the name of its kind and the type the kind comes from
    const Tile prologue = tileOf(palette, QStringLiteral("Prologue"));
    REQUIRE(prologue.tile != nullptr);
    QStringList texts;
    for (const QLabel* label : prologue.tile->findChildren<QLabel*>()) {
        texts << label->text();
    }
    CHECK(texts.contains(QStringLiteral("Prologue")));
    CHECK(texts.contains(QStringLiteral("Novel")));
    CHECK(prologue.box->accessibleDescription() == QStringLiteral("Novel"));

    // Nothing is checked at first, and "Create now" is only on a checked kind
    CHECK(palette.checkedCount() == 0);
    CHECK(palette.choice().kinds.isEmpty());
    CHECK_FALSE(prologue.now->isVisibleTo(&palette));
}

TEST_CASE("Palette: a click on a tile checks its kind on each of its tiles", "[gui][palette]") {
    KindPalette palette(builtInTypes());
    int changes = 0;
    QObject::connect(&palette, &KindPalette::choiceChanged, [&changes]() { ++changes; });

    const QList<Tile> toc = tilesOf(palette, QStringLiteral("Table of contents"));
    REQUIRE(toc.size() == 2);  // at the beginning and at the end of the book

    clickName(toc.at(1));
    CHECK(changes == 1);
    CHECK(toc.at(0).box->isChecked());
    CHECK(toc.at(1).box->isChecked());
    CHECK(palette.checkedCount() == 1);
    CHECK(idsOf(palette.choice().kinds) == "toc");

    // A table of contents is made by a window the program does not have yet: it cannot be
    // created with the book
    CHECK(toc.at(0).now->isVisibleTo(&palette));
    CHECK_FALSE(toc.at(0).now->isEnabled());
    CHECK(toc.at(0).now->toolTip() ==
          QStringLiteral("The program does not have an editor of this kind yet."));

    // The second click unchecks it on both tiles; its box does the same
    clickName(toc.at(0));
    CHECK_FALSE(toc.at(0).box->isChecked());
    CHECK_FALSE(toc.at(1).box->isChecked());
    CHECK_FALSE(toc.at(1).now->isVisibleTo(&palette));
    CHECK(palette.checkedCount() == 0);
    toc.at(1).box->click();
    CHECK(toc.at(0).box->isChecked());
    CHECK(palette.checkedCount() == 1);
    CHECK(changes == 3);
}

TEST_CASE("Palette: \"Create now\" makes an element of a checked kind in the part of its tile",
          "[gui][palette]") {
    KindPalette palette(builtInTypes());
    const Tile chapter = tileOf(palette, QStringLiteral("Chapter"));
    const Tile note = tileOf(palette, QStringLiteral("Working note"));
    REQUIRE(chapter.now != nullptr);
    REQUIRE(note.now != nullptr);

    chapter.box->click();
    REQUIRE(chapter.now->isEnabled());
    chapter.now->click();
    CHECK(elementsOf(palette.choice()) == "chapter in main");

    // A text of the Workshop is made with the book too, in the Workshop
    note.box->click();
    CHECK(note.now->isVisibleTo(&palette));
    REQUIRE(note.now->isEnabled());
    CHECK(note.now->toolTip().isEmpty());
    note.now->click();
    CHECK(elementsOf(palette.choice()) == "chapter in main, work_note in workshop");
    CHECK(idsOf(palette.choice().kinds) == "chapter, work_note");

    // A material is added later, with its source; a kind that opens in a window of its own
    // has no editor yet
    const Tile material = tileOf(palette, QStringLiteral("Picture"));
    const Tile mindMap = tileOf(palette, QStringLiteral("Mind map"));
    REQUIRE(material.now != nullptr);
    REQUIRE(mindMap.now != nullptr);
    material.box->click();
    mindMap.box->click();
    CHECK_FALSE(material.now->isEnabled());
    CHECK(material.now->toolTip() ==
          QStringLiteral("A material is added later, in the Workshop."));
    CHECK_FALSE(mindMap.now->isEnabled());
    CHECK(mindMap.now->toolTip() ==
          QStringLiteral("The program does not have an editor of this kind yet."));
    CHECK(elementsOf(palette.choice()) == "chapter in main, work_note in workshop");

    // An unchecked kind loses its "Create now", also when it is checked again
    chapter.box->click();
    CHECK_FALSE(chapter.now->isChecked());
    CHECK(elementsOf(palette.choice()) == "work_note in workshop");
    chapter.box->click();
    CHECK_FALSE(chapter.now->isChecked());
    CHECK(elementsOf(palette.choice()) == "work_note in workshop");
}

TEST_CASE("Palette: all kinds, only the kinds of \"Base\", or none at once", "[gui][palette]") {
    KindPalette palette(builtInTypes());
    const Tile chapter = tileOf(palette, QStringLiteral("Chapter"));
    const Tile prologue = tileOf(palette, QStringLiteral("Prologue"));
    REQUIRE(chapter.now != nullptr);
    REQUIRE(prologue.now != nullptr);
    palette.checkAll();
    CHECK(palette.checkedCount() == builtInTypes().allKinds().size());
    chapter.now->click();
    prologue.now->click();
    CHECK(elementsOf(palette.choice()) == "chapter in main, prologue in main");

    // The prologue of the novel goes, with its element; the chapter of "Base" stays
    palette.checkOnly(QStringLiteral("kalahari.base"));
    CHECK(palette.checkedCount() == 26);
    CHECK(packagesOf(palette.choice().kinds) == "kalahari.base");
    CHECK(elementsOf(palette.choice()) == "chapter in main");

    // All the kinds again: the elements stay as they were
    palette.checkAll();
    CHECK(elementsOf(palette.choice()) == "chapter in main");

    palette.uncheckAll();
    CHECK(palette.checkedCount() == 0);
    CHECK(palette.choice().kinds.isEmpty());
    CHECK(palette.choice().elements.isEmpty());
}

TEST_CASE("Palette: the choice is in the order of the palette, whatever the order of the clicks",
          "[gui][palette]") {
    KindPalette palette(builtInTypes());
    const Tile epilogue = tileOf(palette, QStringLiteral("Epilogue"));
    const Tile titlePage = tileOf(palette, QStringLiteral("Title page"));
    const Tile chapter = tileOf(palette, QStringLiteral("Chapter"));
    REQUIRE(epilogue.box != nullptr);
    REQUIRE(titlePage.now != nullptr);
    REQUIRE(chapter.now != nullptr);
    epilogue.box->click();
    titlePage.box->click();
    chapter.box->click();
    chapter.now->click();
    titlePage.now->click();

    const PaletteChoice choice = palette.choice();
    CHECK(idsOf(choice.kinds) == "title_page, chapter, epilogue");
    CHECK(elementsOf(choice) == "title_page in front, chapter in main");
}

TEST_CASE("Palette: \"Create now\" is not cut on any tile, also in Polish", "[gui][palette]") {
    // Regression: the tiles had the width of the mockup, and "Utwórz od razu" was cut
    PolishCreateNow polish;
    QCoreApplication::installTranslator(&polish);
    struct Restore {
        QTranslator* translator;
        ~Restore() { QCoreApplication::removeTranslator(translator); }
    } restore{&polish};
    KindPalette palette(builtInTypes());
    palette.checkAll();
    palette.show();
    QCoreApplication::processEvents();

    qsizetype boxes = 0;
    for (const QCheckBox* now : palette.findChildren<QCheckBox*>()) {
        if (now->text() != QStringLiteral("Utwórz od razu")) {
            continue;
        }
        ++boxes;
        INFO(now->parentWidget()->findChild<QCheckBox*>()->accessibleName().toStdString());
        CHECK(now->isVisible());
        CHECK(now->width() >= now->sizeHint().width());
    }
    CHECK(boxes == palette.kindsIn(BookPlace::Front).size() +
                       palette.kindsIn(BookPlace::Main).size() +
                       palette.kindsIn(BookPlace::Back).size() +
                       palette.kindsIn(BookPlace::Workshop).size());
}

// ============================================================================
// The window of the kinds of a user project
// ============================================================================

TEST_CASE("User project: the window starts with the kinds of \"Base\" and a chapter made with "
          "the book",
          "[gui][palette]") {
    UserProjectDialog dialog(builtInTypes());
    dialog.setBook(QStringLiteral("Desert"), QStringLiteral("en"), std::nullopt);

    CHECK(dialog.windowTitle() == QStringLiteral("New Book – User Project"));
    CHECK(showsText(dialog, QStringLiteral("Kinds of Elements of the Book \"Desert\"")));
    CHECK(showsText(dialog, QStringLiteral("Checked kinds: 26")));
    CHECK(dialog.acceptButton()->text() == QStringLiteral("Create Book"));
    CHECK(dialog.acceptButton()->isEnabled());

    // Back, Skip, Create Book and Cancel; Enter creates the book with the checked kinds
    dialog.show();
    QCoreApplication::processEvents();
    CHECK(dialog.backButton()->isVisible());
    CHECK(dialog.skipButton()->isVisible());
    CHECK(dialog.cancelButton()->isVisible());
    CHECK_FALSE(dialog.applyButton()->isVisible());
    CHECK(dialog.focusWidget() == dialog.acceptButton());
    dialog.hide();

    const PaletteChoice first = dialog.palette()->choice();
    CHECK(first.kinds.size() == 26);
    CHECK(packagesOf(first.kinds) == "kalahari.base");
    CHECK(elementsOf(first) == "chapter in main");
    CHECK(idsOf(dialog.firstChoice().kinds) == idsOf(first.kinds));
    CHECK(elementsOf(dialog.firstChoice()) == elementsOf(first));

    // A book without sections has a beginning, a content and an end
    const KindPalette& palette = *dialog.palette();
    CHECK(palette.groupName(BookPlace::Front) == QStringLiteral("Beginning of the Book"));
    CHECK(palette.groupName(BookPlace::Main) == QStringLiteral("Content"));
    CHECK(palette.groupName(BookPlace::Back) == QStringLiteral("End of the Book"));
    CHECK(palette.groupName(BookPlace::Workshop) == QStringLiteral("Workshop"));

    // The tools of the types are to come
    CHECK(showsText(dialog, QStringLiteral("No type has tools yet.")));
}

TEST_CASE("User project: the groups have the names of the sections of the book, in its language",
          "[gui][palette]") {
    using core::BookSections;
    UserProjectDialog dialog(builtInTypes());
    const KindPalette& palette = *dialog.palette();

    dialog.setBook(QStringLiteral("Pustynia"), QStringLiteral("pl"),
                   BookSections{true, QStringLiteral("matter"), {}});
    CHECK(palette.groupName(BookPlace::Front) == QStringLiteral("Strony początkowe"));
    CHECK(palette.groupName(BookPlace::Main) == QStringLiteral("Tekst główny"));
    CHECK(palette.groupName(BookPlace::Back) == QStringLiteral("Strony końcowe"));

    // The writer's own names; an empty one is the name of the first set
    dialog.setBook(QStringLiteral("Pustynia"), QStringLiteral("pl"),
                   BookSections{true, QString::fromLatin1(core::ProjectBook::CUSTOM_SECTIONS),
                                {QStringLiteral("Początek"), QStringLiteral("Opowieść"),
                                 QString()}});
    CHECK(palette.groupName(BookPlace::Front) == QStringLiteral("Początek"));
    CHECK(palette.groupName(BookPlace::Main) == QStringLiteral("Opowieść"));
    CHECK(palette.groupName(BookPlace::Back) == QStringLiteral("Sekcja końcowa"));

    // Sections the Navigator does not show
    dialog.setBook(QStringLiteral("Desert"), QStringLiteral("en"),
                   BookSections{false, QStringLiteral("matter"), {}});
    CHECK(palette.groupName(BookPlace::Front) == QStringLiteral("Beginning of the Book"));
    CHECK(palette.groupName(BookPlace::Main) == QStringLiteral("Content"));
    CHECK(palette.groupName(BookPlace::Back) == QStringLiteral("End of the Book"));
    CHECK(palette.groupName(BookPlace::Workshop) == QStringLiteral("Workshop"));
    CHECK(showsText(dialog, QStringLiteral("Kinds of Elements of the Book \"Desert\"")));
}

TEST_CASE("User project: the links over the palette, and Create Book with no kind",
          "[gui][palette]") {
    UserProjectDialog dialog(builtInTypes());
    dialog.setBook(QStringLiteral("Desert"), QStringLiteral("en"), std::nullopt);

    clickLink(dialog, QStringLiteral("Uncheck All"));
    CHECK(dialog.palette()->checkedCount() == 0);
    CHECK(showsText(dialog, QStringLiteral("Checked kinds: 0")));
    CHECK_FALSE(dialog.acceptButton()->isEnabled());
    CHECK(dialog.acceptButton()->toolTip() == QStringLiteral("Check at least one kind."));
    CHECK(dialog.skipButton()->isEnabled());  // Skip creates the book of "Base"

    clickLink(dialog, QStringLiteral("Check All"));
    const qsizetype all = builtInTypes().allKinds().size();
    CHECK(dialog.palette()->checkedCount() == all);
    CHECK(showsText(dialog, QStringLiteral("Checked kinds: %1").arg(all)));
    CHECK(dialog.acceptButton()->isEnabled());
    CHECK(dialog.acceptButton()->toolTip().isEmpty());

    clickLink(dialog, QStringLiteral("Only \"Base\""));
    CHECK(packagesOf(dialog.palette()->choice().kinds) == "kalahari.base");
    CHECK(showsText(dialog, QStringLiteral("Checked kinds: 26")));
}

TEST_CASE("User project: Create Book takes the palette as it is, Skip as it was at first",
          "[gui][palette]") {
    UserProjectDialog dialog(builtInTypes());
    dialog.setBook(QStringLiteral("Desert"), QStringLiteral("en"), std::nullopt);
    KindPalette& palette = *dialog.palette();
    const Tile prologue = tileOf(palette, QStringLiteral("Prologue"));
    const Tile chapter = tileOf(palette, QStringLiteral("Chapter"));
    REQUIRE(prologue.now != nullptr);
    REQUIRE(chapter.box != nullptr);
    palette.uncheckAll();
    prologue.box->click();
    prologue.now->click();
    chapter.box->click();
    dialog.show();

    SECTION("Create Book") {
        dialog.acceptButton()->click();
        CHECK(dialog.result() == QDialog::Accepted);
        CHECK(idsOf(dialog.choice().kinds) == "chapter, prologue");
        CHECK(elementsOf(dialog.choice()) == "prologue in main");
        CHECK(dialog.opens());
    }

    SECTION("Skip") {
        CHECK(dialog.skipButton()->text() == QStringLiteral("Skip"));
        dialog.skipButton()->click();
        CHECK(dialog.result() == QDialog::Accepted);
        CHECK(dialog.choice().kinds.size() == 26);
        CHECK(packagesOf(dialog.choice().kinds) == "kalahari.base");
        CHECK(elementsOf(dialog.choice()) == "chapter in main");
        CHECK(dialog.opens());
    }
}

TEST_CASE("User project: Back returns to the New Book window; Cancel of the question about the "
          "open book returns to this window",
          "[gui][palette]") {
    UserProjectDialog dialog(builtInTypes());
    dialog.setBook(QStringLiteral("Desert"), QStringLiteral("en"), std::nullopt);

    SECTION("Back") {
        CHECK(dialog.backButton()->text() == QStringLiteral("< Back"));
        QTimer::singleShot(0, dialog.backButton(), &QPushButton::click);
        CHECK(dialog.exec() == UserProjectDialog::BACK);
    }

    SECTION("The question about the open book") {
        QStringList asked;
        NewBookOpening answer = NewBookOpening::Cancel;
        dialog.setOpeningQuestion([&](QWidget* window, const QString& title) {
            CHECK(window == &dialog);
            asked << title;
            return answer;
        });
        dialog.show();

        // Cancel: the window stays as it was
        dialog.acceptButton()->click();
        CHECK(asked == QStringList{QStringLiteral("Desert")});
        CHECK(dialog.isVisible());
        CHECK(dialog.result() != QDialog::Accepted);

        // Skip asks too; the new book is then created without opening
        answer = NewBookOpening::DontOpen;
        dialog.skipButton()->click();
        CHECK(asked.size() == 2);
        CHECK_FALSE(dialog.isVisible());
        CHECK(dialog.result() == QDialog::Accepted);
        CHECK_FALSE(dialog.opens());
        CHECK(elementsOf(dialog.choice()) == "chapter in main");
    }
}

// ============================================================================
// The command
// ============================================================================

TEST_CASE("New Book: with a book open, the new book is created without opening it or in its "
          "place",
          "[gui][newbook]") {
    const test::RecentBooks recent;
    test::OpenNovel novel(QStringLiteral("Open Book"));
    const QString open = novel.pm.getManifestPath();
    QTemporaryDir folder;
    REQUIRE(folder.isValid());
    const QString created = QDir(folder.path()).filePath(QStringLiteral("Second/Second.klh"));
    test::Window window;
    QStringList unexpected;
    const auto newNovel = [&folder](QDialog& dialog) {
        fillNewBook(dialog, "template.novel", QStringLiteral("Second"), folder.path());
        CHECK(buttonText(createButton(dialog)) == QStringLiteral("Create Book"));
        later(createButton(dialog));
    };

    SECTION("Without opening") {
        QStringList asked;
        const QList<Step> steps{
            {is<NewItemDialog>, newNovel},
            // Cancel of the question returns to the New Book window, with the data
            {is<MessageDialog>,
             [&asked](QDialog& dialog) {
                 asked << test::textsOf(dialog);
                 auto& question = static_cast<MessageDialog&>(dialog);
                 CHECK(buttonText(question.acceptButton()) ==
                       QStringLiteral("Close and Open New"));
                 CHECK(buttonText(question.alternativeButton()) ==
                       QStringLiteral("Create Without Opening"));
                 later(question.cancelButton());
             }},
            {is<NewItemDialog>,
             [](QDialog& dialog) {
                 CHECK(fieldText(titleField(dialog)) == QStringLiteral("Second"));
                 later(createButton(dialog));
             }},
            {is<MessageDialog>, [](QDialog& dialog) {
                 later(static_cast<MessageDialog&>(dialog).alternativeButton());
             }}};
        CHECK(doing([&window] { window.documents.onNewProject(); }, steps, unexpected) ==
              steps.size());
        INFO(unexpected.join(QLatin1Char('\n')).toStdString());
        CHECK(unexpected.isEmpty());

        // The question names the open book and says where the new one will be
        REQUIRE(asked.size() == 1);
        CHECK(asked.first().contains(QStringLiteral("Close \"Open Book\"?")));
        CHECK(asked.first().contains(QStringLiteral(
            "The new book \"Second\" can take the place of the open one or be created without "
            "opening.")));
        CHECK(asked.first().contains(
            QStringLiteral("A book created without opening is on the Dashboard and in File > "
                           "Recent Books. Cancel returns to the New Book window with your "
                           "data.")));

        // The open book stays open; the new one is on the Dashboard and in Recent Books
        CHECK(QFileInfo(novel.pm.getManifestPath()) == QFileInfo(open));
        REQUIRE(QFileInfo::exists(created));
        CHECK(QFileInfo(firstRecentBook()) == QFileInfo(created));
        CHECK(hasCard(window, created));
        CHECK(window.statusBar.currentMessage() ==
              QStringLiteral("Book \"Second\" created without opening. It is on the Dashboard "
                             "and in File > Recent Books."));
        const core::BookProject saved = savedProject(created);
        CHECK(saved.books.first().title == QStringLiteral("Second"));
        CHECK(titlesOf(saved.books.first().mainElements) == "Chapter 1");
    }

    SECTION("In the place of the open one") {
        const QList<Step> steps{{is<NewItemDialog>, newNovel},
                                {is<MessageDialog>, [](QDialog& dialog) {
                                     later(static_cast<MessageDialog&>(dialog).acceptButton());
                                 }}};
        CHECK(doing([&window] { window.documents.onNewProject(); }, steps, unexpected) ==
              steps.size());
        INFO(unexpected.join(QLatin1Char('\n')).toStdString());
        CHECK(unexpected.isEmpty());
        CHECK(QFileInfo(novel.pm.getManifestPath()) == QFileInfo(created));
        REQUIRE(novel.pm.book() != nullptr);
        CHECK(novel.pm.book()->title == QStringLiteral("Second"));
        REQUIRE(novel.pm.closeProject(false));  // before its folder goes
    }
}

TEST_CASE("New Book: a book type that the program does not have is not on the list",
          "[gui][newbook]") {
    auto& pm = core::ProjectManager::getInstance();
    if (pm.isProjectOpen()) {
        REQUIRE(pm.closeProject(false));
    }
    const auto templatesOf = [](const NewItemDialog& dialog) {
        QStringList ids;
        const auto* list = dialog.findChild<QListWidget*>();
        REQUIRE(list != nullptr);
        for (int i = 0; i < list->count(); ++i) {
            ids << list->item(i)->data(Qt::UserRole).toString();
        }
        return ids;
    };

    // Every type of the program, and the user project
    const QStringList all = templatesOf(NewItemDialog(gui::dialogs::NewItemMode::Project));
    CHECK(all == QStringList{"template.novel", "template.shortStories", "template.nonfiction",
                             "template.screenplay", "template.poetry", "template.empty"});

    // The program has no novel (nor the collection of stories, which uses it)
    QTemporaryDir installed;
    REQUIRE(installed.isValid());
    const QDir source(QStringLiteral(KALAHARI_SOURCE_DIR "/resources/booktypes"));
    for (const QString& package : source.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        if (package == QLatin1String("kalahari.novel") ||
            package == QLatin1String("kalahari.short_stories")) {
            continue;
        }
        QDirIterator it(source.filePath(package), QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString file = source.relativeFilePath(it.next());
            const QString target = QDir(installed.path()).filePath(file);
            REQUIRE(QDir().mkpath(QFileInfo(target).absolutePath()));
            REQUIRE(QFile::copy(source.filePath(file), target));
        }
    }
    pm.loadBookTypes({installed.path()});
    REQUIRE(pm.installedBookTypes().package(QStringLiteral("kalahari.novel")) == nullptr);

    // Neither is on the list, not even greyed out; the first type is chosen
    {
        const NewItemDialog dialog(gui::dialogs::NewItemMode::Project);
        CHECK(templatesOf(dialog) == QStringList{"template.nonfiction", "template.screenplay",
                                                 "template.poetry", "template.empty"});
        const auto* list = dialog.findChild<QListWidget*>();
        REQUIRE(list->currentItem() != nullptr);
        CHECK(list->currentItem()->data(Qt::UserRole).toString() == "template.nonfiction");
        for (int i = 0; i < list->count(); ++i) {
            CHECK(list->item(i)->flags().testFlag(Qt::ItemIsEnabled));
        }
    }
    pm.loadBookTypes({source.path()});
    CHECK(templatesOf(NewItemDialog(gui::dialogs::NewItemMode::Project)) == all);
}

TEST_CASE("New Book: a user project gets the kinds and the elements chosen in the window of its "
          "kinds",
          "[gui][newbook]") {
    const test::RecentBooks recent;
    auto& pm = core::ProjectManager::getInstance();
    if (pm.isProjectOpen()) {
        REQUIRE(pm.closeProject(false));
    }
    pm.loadBookTypes({QStringLiteral(KALAHARI_SOURCE_DIR "/resources/booktypes")});
    QTemporaryDir folder;
    REQUIRE(folder.isValid());
    test::Window window;
    QStringList unexpected;

    // Back returns to the New Book window, which keeps the data; the window of the kinds keeps
    // the choice
    const QList<Step> steps{
        {is<NewItemDialog>,
         [&folder](QDialog& dialog) {
             fillNewBook(dialog, "template.empty", QStringLiteral("Desert"), folder.path());
             CHECK(buttonText(createButton(dialog)) == QStringLiteral("Next"));
             later(createButton(dialog));
         }},
        {is<UserProjectDialog>,
         [](QDialog& dialog) {
             auto& kinds = static_cast<UserProjectDialog&>(dialog);
             CHECK(showsText(kinds, QStringLiteral("Kinds of Elements of the Book \"Desert\"")));
             clickLink(kinds, QStringLiteral("Uncheck All"));
             for (const QString& name : {QStringLiteral("Chapter"), QStringLiteral("Prologue")}) {
                 const Tile tile = tileOf(*kinds.palette(), name);
                 if (tile.box && tile.now) {
                     tile.box->click();
                     tile.now->click();
                 }
             }
             later(kinds.backButton());
         }},
        {is<NewItemDialog>,
         [](QDialog& dialog) {
             QLineEdit* title = titleField(dialog);
             CHECK(fieldText(title) == QStringLiteral("Desert"));
             if (title) {
                 title->setText(QStringLiteral("Oasis"));
             }
             later(createButton(dialog));
         }},
        {is<UserProjectDialog>, [](QDialog& dialog) {
             auto& kinds = static_cast<UserProjectDialog&>(dialog);
             CHECK(showsText(kinds, QStringLiteral("Kinds of Elements of the Book \"Oasis\"")));
             CHECK(idsOf(kinds.palette()->choice().kinds) == "chapter, prologue");
             later(kinds.acceptButton());
         }}};
    CHECK(doing([&window] { window.documents.onNewProject(); }, steps, unexpected) ==
          steps.size());
    INFO(unexpected.join(QLatin1Char('\n')).toStdString());
    CHECK(unexpected.isEmpty());

    // No book was open, so nothing asks; the new book opens with its kinds and elements
    REQUIRE(pm.isProjectOpen());
    CHECK(QFileInfo(pm.getManifestPath()) ==
          QFileInfo(QDir(folder.path()).filePath(QStringLiteral("Oasis/Oasis.klh"))));
    REQUIRE(pm.project() != nullptr);
    CHECK(pm.project()->isUserProject());
    CHECK(pm.project()->kinds ==
          QList<core::KindReference>{{QStringLiteral("kalahari.base"), QStringLiteral("chapter")},
                                     {QStringLiteral("kalahari.novel"),
                                      QStringLiteral("prologue")}});
    REQUIRE(pm.book() != nullptr);
    CHECK(pm.book()->title == QStringLiteral("Oasis"));
    CHECK(titlesOf(pm.book()->mainElements) == "Prologue, Chapter 1");
    REQUIRE(pm.closeProject(false));
}

TEST_CASE("New Book: with a book open, the window of the kinds of a user project asks about it",
          "[gui][newbook]") {
    const test::RecentBooks recent;
    test::OpenNovel novel(QStringLiteral("Open Book"));
    const QString open = novel.pm.getManifestPath();
    QTemporaryDir folder;
    REQUIRE(folder.isValid());
    const QString created = QDir(folder.path()).filePath(QStringLiteral("Desert/Desert.klh"));
    test::Window window;
    QStringList unexpected;
    QStringList asked;

    // Next asks nothing: the question comes in the window of the kinds, and its Cancel returns
    // there
    const QList<Step> steps{
        {is<NewItemDialog>,
         [&folder](QDialog& dialog) {
             fillNewBook(dialog, "template.empty", QStringLiteral("Desert"), folder.path());
             later(createButton(dialog));
         }},
        {is<UserProjectDialog>,
         [](QDialog& dialog) { later(static_cast<UserProjectDialog&>(dialog).acceptButton()); }},
        {is<MessageDialog>,
         [&asked](QDialog& dialog) {
             asked << test::textsOf(dialog);
             later(static_cast<MessageDialog&>(dialog).cancelButton());
         }},
        {is<UserProjectDialog>,
         [](QDialog& dialog) { later(static_cast<UserProjectDialog&>(dialog).skipButton()); }},
        {is<MessageDialog>, [](QDialog& dialog) {
             later(static_cast<MessageDialog&>(dialog).alternativeButton());
         }}};
    CHECK(doing([&window] { window.documents.onNewProject(); }, steps, unexpected) ==
          steps.size());
    INFO(unexpected.join(QLatin1Char('\n')).toStdString());
    CHECK(unexpected.isEmpty());

    // The question says where its Cancel returns
    REQUIRE(asked.size() == 1);
    CHECK(asked.first().contains(QStringLiteral("Close \"Open Book\"?")));
    CHECK(asked.first().contains(
        QStringLiteral("Cancel returns to the New Book – User Project window with the kinds you "
                       "checked.")));

    // The open book stays open; the new one has the kinds of "Base" and a chapter
    CHECK(QFileInfo(novel.pm.getManifestPath()) == QFileInfo(open));
    REQUIRE(QFileInfo::exists(created));
    CHECK(QFileInfo(firstRecentBook()) == QFileInfo(created));
    CHECK(hasCard(window, created));
    const core::BookProject saved = savedProject(created);
    CHECK(saved.isUserProject());
    CHECK(saved.kinds.size() == 26);
    CHECK(titlesOf(saved.books.first().mainElements) == "Chapter 1");
}
