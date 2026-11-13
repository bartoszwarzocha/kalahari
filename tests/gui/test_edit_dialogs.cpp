/// @file test_edit_dialogs.cpp
/// @brief The windows of the Edit menu: Paste Special... and Go To..., and their commands

#include <catch2/catch_test_macros.hpp>
#include "dialog_checks.h"
#include "document_window.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/editor/clipboard_handler.h"
#include "kalahari/editor/editor_appearance.h"
#include "kalahari/editor/kalahari_text_document_layout.h"
#include "kalahari/gui/command_registrar.h"
#include "kalahari/gui/command_registry.h"
#include "kalahari/gui/dialogs/edit_dialogs.h"
#include "kalahari/gui/go_to_coordinator.h"
#include "kalahari/gui/settings_dialog.h"

#include <QApplication>
#include <QClipboard>
#include <QGuiApplication>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMimeData>
#include <QPushButton>
#include <QTextBlock>
#include <QToolButton>

#include <memory>
#include <string>

using namespace kalahari;
using editor::ClipboardSource;
using editor::PasteForm;
using editor::PlaceKind;
using gui::dialogs::GoToDialog;
using gui::dialogs::PasteSpecialDialog;
using test::checkKeys;
using test::checkSmallScreen;
using test::layOut;
using test::Polish;
using test::SettingsOpener;

namespace {

std::unique_ptr<editor::BookEditor> editorWith(const QString& kml) {
    auto text = std::make_unique<editor::BookEditor>();
    text->resize(700, 500);
    editor::EditorAppearance appearance;
    appearance.cursor.blinking = false;
    text->setAppearance(appearance);
    text->fromKml(kml);
    return text;
}

/// The one widget of a type in a window
template <typename Widget>
Widget* only(const QWidget& root) {
    const QList<Widget*> found = root.findChildren<Widget*>();
    REQUIRE(found.size() == 1);
    return found.first();
}

/// The texts of the items of a list, those greyed out in brackets
QStringList itemsOf(const QListWidget& list) {
    QStringList items;
    for (int row = 0; row < list.count(); ++row) {
        const QListWidgetItem* item = list.item(row);
        items << ((item->flags() & Qt::ItemIsEnabled) != 0
                      ? item->text()
                      : QLatin1Char('[') + item->text() + QLatin1Char(']'));
    }
    return items;
}

/// The texts of the labels of a window, in one line
std::string labelsOf(const QWidget& window) {
    QStringList texts;
    for (const QLabel* label : window.findChildren<QLabel*>()) {
        texts << label->text();
    }
    return texts.join(QStringLiteral(" | ")).toStdString();
}

/// The paste mode of the setting as it was, until the test ends
class PasteModeKept {
public:
    PasteModeKept()
        : m_mode(core::SettingsManager::getInstance().get<std::string>(
              editor::PASTE_MODE_KEY, editor::PASTE_MODE_MERGE)) {}
    ~PasteModeKept() {
        try {
            core::SettingsManager::getInstance().set<std::string>(editor::PASTE_MODE_KEY, m_mode);
        } catch (...) {
            std::fputs("PasteModeKept: the paste mode was not restored\n", stderr);
        }
    }
    PasteModeKept(const PasteModeKept&) = delete;
    PasteModeKept& operator=(const PasteModeKept&) = delete;

private:
    std::string m_mode;
};

}  // namespace

// =============================================================================
// Edit > Paste Special...
// =============================================================================

TEST_CASE("Paste Special: the forms of the clipboard, the one Paste gives chosen first",
          "[gui][paste][dialog]") {
    const PasteModeKept kept;
    auto& settings = core::SettingsManager::getInstance();
    settings.set<std::string>(editor::PASTE_MODE_KEY, editor::PASTE_MODE_MERGE);
    auto text = editorWith(QStringLiteral("<kml><p>xy</p></kml>"));
    QMimeData word;
    word.setText(QStringLiteral("A B"));
    word.setHtml(QStringLiteral("<html xmlns:w=\"urn:schemas-microsoft-com:office:word\"><head>"
                                "<meta name=Generator content=\"Microsoft Word 15\"></head>"
                                "<body><p>A <b>B</b></p></body></html>"));

    PasteSpecialDialog::Data data = PasteSpecialDialog::dataFor(*text, &word);
    CHECK(data.source == ClipboardSource::Word);
    CHECK(data.chosen == PasteForm::Merge);
    CHECK_FALSE(data.picture);
    PasteSpecialDialog dialog(data);
    layOut(dialog);
    auto* forms = only<QListWidget>(dialog);
    CHECK(itemsOf(*forms) == QStringList{QStringLiteral("[Kalahari Format (with annotations)]"),
                                         QStringLiteral("Keep Source Formatting"),
                                         QStringLiteral("Merge Formatting"),
                                         QStringLiteral("Text Only"), QStringLiteral("[Image]")});
    CHECK(forms->item(0)->toolTip() ==
          QStringLiteral("The Kalahari format is there when the text was copied in Kalahari."));
    CHECK(forms->item(4)->toolTip() == QStringLiteral("The clipboard has no picture."));
    CHECK(dialog.form() == PasteForm::Merge);
    CHECK(QApplication::focusWidget() == forms);
    INFO(labelsOf(dialog));
    CHECK(test::textsOf(dialog).contains(QStringLiteral("Source: <b>Microsoft Word</b>")));
    CHECK(test::textsOf(dialog).contains(QStringLiteral(
        "the font, size and color are those of the book. The Kalahari format is there when")));
    forms->setCurrentRow(3);
    CHECK(dialog.form() == PasteForm::Text);
    CHECK(test::textsOf(dialog).contains(QStringLiteral("Only the text, with its paragraphs")));

    // Text copied in Kalahari: its own format first; the others without its annotations
    QMimeData own;
    own.setText(QStringLiteral("B"));
    own.setHtml(QStringLiteral("<p>B</p>"));
    own.setData(QString::fromLatin1(editor::MIME_KML), QByteArrayLiteral("<kml><p>B</p></kml>"));
    data = PasteSpecialDialog::dataFor(*text, &own);
    CHECK(data.source == ClipboardSource::Kalahari);
    CHECK(data.chosen == PasteForm::Kalahari);
    PasteSpecialDialog ownDialog(data);
    only<QListWidget>(ownDialog)->setCurrentRow(1);
    CHECK(test::textsOf(ownDialog).contains(
        QStringLiteral("Text copied in Kalahari comes without its annotations and images")));

    // The mode of the setting; text without formatting
    settings.set<std::string>(editor::PASTE_MODE_KEY, editor::PASTE_MODE_SOURCE);
    CHECK(PasteSpecialDialog::dataFor(*text, &word).chosen == PasteForm::Source);
    QMimeData plain;
    plain.setText(QStringLiteral("A"));
    data = PasteSpecialDialog::dataFor(*text, &plain);
    CHECK(data.source == ClipboardSource::Text);
    CHECK(data.chosen == PasteForm::Text);

    // A picture in a new file, which keeps no images until it is saved
    QImage image(QSize(4, 4), QImage::Format_RGB32);
    image.fill(Qt::blue);
    QMimeData picture;
    picture.setImageData(image);
    data = PasteSpecialDialog::dataFor(*text, &picture);
    CHECK(data.source == ClipboardSource::Image);
    CHECK(data.picture);
    CHECK(data.forms.empty());
    CHECK(data.whyNoImage ==
          QStringLiteral("Save the file first: its images are kept in a folder next to it"));
    PasteSpecialDialog pictureDialog(data);
    CHECK(only<QListWidget>(pictureDialog)->item(4)->toolTip() == data.whyNoImage);
    CHECK_FALSE(pictureDialog.acceptButton()->isEnabled());
}

TEST_CASE("Paste Special: Edit > Paste Special... pastes in the form chosen",
          "[gui][paste][dialog]") {
    const PasteModeKept kept;
    core::SettingsManager::getInstance().set<std::string>(editor::PASTE_MODE_KEY,
                                                          editor::PASTE_MODE_TEXT);
    gui::registerAllCommands(gui::CommandCallbacks{});
    const gui::Command* command =
        gui::CommandRegistry::getInstance().getCommand("edit.pasteSpecial");
    REQUIRE(command != nullptr);
    CHECK(command->phase == 0);
    CHECK(command->shortcut.isEmpty());

    auto text = editorWith(QStringLiteral("<kml><p>xy</p></kml>"));
    text->setCursorPosition({0, 1});
    auto* data = new QMimeData();
    data->setText(QStringLiteral("A B"));
    data->setHtml(QStringLiteral("<p>A <b>B</b></p>"));
    QGuiApplication::clipboard()->setMimeData(data);
    QStringList said;
    bool pasted = false;
    CHECK(test::accepting([&]() { pasted = PasteSpecialDialog::pasteInto(*text, nullptr); },
                          &said) == 1);
    CHECK(pasted);
    REQUIRE(said.size() == 1);
    CHECK(said.first().startsWith(QStringLiteral("Paste Special")));
    // The mode of the setting first: the text only
    CHECK(text->toKml() == QStringLiteral("<kml><p>xA By</p></kml>"));

    // A read-only text: no window
    text->setReadOnly(true);
    CHECK(test::accepting([&]() { pasted = PasteSpecialDialog::pasteInto(*text, nullptr); }) == 0);
    CHECK_FALSE(pasted);
}

// =============================================================================
// Edit > Go To...
// =============================================================================

TEST_CASE("Go To: the window goes to the place of the number, the next and the previous one",
          "[gui][goto][dialog]") {
    auto text = editorWith(QStringLiteral(
        "<kml><h1>One</h1><p>a</p><h2>Two</h2><p>b</p><h3>Three</h3><p>c</p></kml>"));
    editor::BookEditor* target = text.get();
    GoToDialog dialog([&target]() { return target; });
    layOut(dialog);
    auto* kinds = only<QListWidget>(dialog);
    auto* number = only<QLineEdit>(dialog);
    CHECK(itemsOf(*kinds) == QStringList{QStringLiteral("[Page]"), QStringLiteral("Heading"),
                                         QStringLiteral("Footnote or Endnote"),
                                         QStringLiteral("Image"), QStringLiteral("Table"),
                                         QStringLiteral("Annotation")});
    CHECK(kinds->item(0)->toolTip() ==
          QStringLiteral("The pages show in the Page Layout view (View > View Mode)."));
    CHECK(dialog.kind() == PlaceKind::Heading);
    CHECK(QApplication::focusWidget() == number);
    CHECK(test::textsOf(dialog).contains(QStringLiteral("Heading n&umber")));

    // A number, Enter
    number->setText(QStringLiteral("2"));
    dialog.acceptButton()->click();
    CHECK(dialog.isVisible());  // it stays open
    CHECK(dialog.result() == QStringLiteral("Heading 2 of 3"));
    CHECK(text->cursorPosition().paragraph == 2);
    number->setText(QStringLiteral("+1"));
    dialog.accept();
    CHECK(text->cursorPosition().paragraph == 4);
    number->setText(QStringLiteral("9"));
    dialog.accept();
    CHECK(dialog.result() == QStringLiteral("The text has 3 heading(s)."));
    number->setText(QStringLiteral("abc"));
    dialog.accept();
    CHECK(dialog.result() == QStringLiteral("Type a number, as 3, +2 or -1."));

    // Without a number Go is Next; Previous goes back
    number->clear();
    dialog.accept();
    CHECK(dialog.result() == QStringLiteral("There is no heading after the cursor."));
    QPushButton* previous = nullptr;
    QPushButton* next = nullptr;
    for (QPushButton* button : dialog.findChildren<QPushButton*>()) {
        if (button->text() == QStringLiteral("&Previous")) {
            previous = button;
        } else if (button->text() == QStringLiteral("&Next")) {
            next = button;
        }
    }
    REQUIRE(previous != nullptr);
    REQUIRE(next != nullptr);
    previous->click();
    CHECK(dialog.result() == QStringLiteral("Heading 2 of 3"));
    next->click();
    CHECK(dialog.result() == QStringLiteral("Heading 3 of 3"));

    // Another kind: its label and hint, and what the text has not
    kinds->setCurrentRow(static_cast<int>(PlaceKind::Table));
    CHECK(dialog.result().isEmpty());
    CHECK(test::textsOf(dialog).contains(QStringLiteral("Table n&umber")));
    CHECK(test::textsOf(dialog).contains(QStringLiteral("“+2”: two tables on")));
    next->click();
    CHECK(dialog.result() == QStringLiteral("The text has no tables."));

    // The pages in the Page Layout view
    text->setViewMode(editor::ViewMode::Page);
    dialog.updateTarget();
    CHECK(itemsOf(*kinds).first() == QStringLiteral("Page"));
    dialog.setKind(PlaceKind::Page);
    CHECK(dialog.kind() == PlaceKind::Page);
    number->setText(QStringLiteral("1"));
    dialog.accept();
    CHECK(dialog.result() == QStringLiteral("Page 1 of 1"));
    text->setViewMode(editor::ViewMode::Continuous);
    dialog.updateTarget();
    CHECK(dialog.kind() == PlaceKind::Heading);

    // No text: the buttons greyed out, saying why
    target = nullptr;
    dialog.updateTarget();
    CHECK_FALSE(dialog.acceptButton()->isEnabled());
    CHECK_FALSE(next->isEnabled());
    CHECK(dialog.result() == QStringLiteral("No chapter or file is open"));
}

TEST_CASE("Go To: the sentences of the outcomes", "[gui][goto][dialog]") {
    editor::GoToOutcome outcome;
    outcome.ordinal = 2;
    outcome.count = 5;
    outcome.number = QStringLiteral("iii");
    CHECK(GoToDialog::outcomeText(PlaceKind::Note, outcome) == QStringLiteral("Note iii (2 of 5)"));
    CHECK(GoToDialog::outcomeText(PlaceKind::Image, outcome) == QStringLiteral("Image 2 of 5"));
    outcome.result = editor::GoToResult::NoNumber;
    CHECK(GoToDialog::outcomeText(PlaceKind::Note, outcome) ==
          QStringLiteral("The text has no note of this number; it has 5 note(s)."));
    outcome.count = 1;
    CHECK(GoToDialog::outcomeText(PlaceKind::Annotation, outcome) ==
          QStringLiteral("The text has 1 annotation(s)."));
    outcome.result = editor::GoToResult::NoPlaces;
    CHECK(GoToDialog::outcomeText(PlaceKind::Page, outcome) ==
          QStringLiteral("The pages show in the Page Layout view (View > View Mode)."));
    outcome.result = editor::GoToResult::NoEarlier;
    CHECK(GoToDialog::outcomeText(PlaceKind::Page, outcome) ==
          QStringLiteral("This is the first page of the text."));
    outcome.result = editor::GoToResult::Invalid;
    CHECK(GoToDialog::outcomeText(PlaceKind::Note, outcome) ==
          QStringLiteral("Type the note's number, as 3, +2 or -1."));
}

TEST_CASE("Go To: Edit > Go To... opens one window over the text that had the keys last",
          "[gui][goto]") {
    gui::registerAllCommands(gui::CommandCallbacks{});
    auto& registry = gui::CommandRegistry::getInstance();
    const gui::Command* command = registry.getCommand(gui::GoToCoordinator::COMMAND);
    REQUIRE(command != nullptr);
    CHECK(command->shortcut.toQKeySequence() == QKeySequence(Qt::ControlModifier | Qt::Key_G));
    CHECK(command->menuPath == "EDIT/Go To...");
    // In the Edit menu after Find in Book..., as the mockup has it; Preferences... no more,
    // as Settings... opens the same window
    const gui::Command* findInBook = registry.getCommand("edit.findInBook");
    const gui::Command* nextTodo = registry.getCommand("edit.nextTodo");
    REQUIRE(findInBook != nullptr);
    REQUIRE(nextTodo != nullptr);
    CHECK(findInBook->menuOrder < command->menuOrder);
    CHECK(command->menuOrder < nextTodo->menuOrder);
    CHECK_FALSE(findInBook->addSeparatorAfter);
    CHECK(command->addSeparatorAfter);
    const gui::Command* preferences = registry.getCommand("edit.preferences");
    REQUIRE(preferences != nullptr);
    CHECK_FALSE(preferences->showInMenu);
    REQUIRE(registry.getCommand("edit.settings") != nullptr);
    CHECK(registry.getCommand("edit.settings")->showInMenu);

    QWidget window;
    auto text = editorWith(QStringLiteral("<kml><h1>One</h1><p>a</p><h1>Two</h1></kml>"));
    editor::BookEditor* keys = text.get();
    gui::GoToCoordinator coordinator([&keys]() { return keys; }, &window);
    coordinator.connectCommands();
    registry.executeCommand(gui::GoToCoordinator::COMMAND);
    GoToDialog* dialog = coordinator.dialog();
    REQUIRE(dialog != nullptr);
    CHECK(dialog->isVisible());
    // The keys in the window: the text that had them last
    keys = nullptr;
    CHECK(coordinator.text() == text.get());
    only<QLineEdit>(*dialog)->setText(QStringLiteral("2"));
    dialog->accept();
    CHECK(text->cursorPosition().paragraph == 2);
    // Opened again: the same window
    coordinator.open();
    CHECK(coordinator.dialog() == dialog);
    dialog->close();
}

TEST_CASE("Go To and Paste Special: the windows in Polish, their keys, a small screen",
          "[gui][goto][paste][dialog]") {
    const Polish polish;
    REQUIRE(polish.loaded());
    QString openedPage;
    const SettingsOpener opener(
        [&openedPage](QWidget*, const QString& page) { openedPage = page; });

    auto text = editorWith(QStringLiteral("<kml><h1>Jeden</h1><p>a</p><h1>Dwa</h1>"
                                          "<h1>Trzy</h1></kml>"));
    GoToDialog goTo([&text]() { return text.get(); });
    layOut(goTo);
    CHECK(goTo.windowTitle() == QStringLiteral("Przejdź do"));
    CHECK(itemsOf(*only<QListWidget>(goTo)) ==
          QStringList{QStringLiteral("[Strona]"), QStringLiteral("Nagłówek"),
                      QStringLiteral("Przypis"), QStringLiteral("Obraz"), QStringLiteral("Tabela"),
                      QStringLiteral("Uwaga")});
    CHECK(test::textsOf(goTo).contains(QStringLiteral("Nu&mer nagłówka")));
    CHECK(test::textsOf(goTo).contains(QStringLiteral(
        "„3” – trzeci nagłówek tekstu, „+2” – dwa nagłówki dalej, „-1” – jeden nagłówek "
        "wcześniej. „Następny” i „Poprzedni” przechodzą do sąsiedniego nagłówka.")));
    only<QLineEdit>(goTo)->setText(QStringLiteral("2"));
    goTo.accept();
    CHECK(goTo.result() == QStringLiteral("Nagłówek 2 z 3"));
    only<QLineEdit>(goTo)->setText(QStringLiteral("7"));
    goTo.accept();
    CHECK(goTo.result() == QStringLiteral("Tekst ma 3 nagłówki."));
    checkKeys(goTo);
    // Every kind: the window keeps its size, small enough
    const QSize size = goTo.sizeHint();
    for (int row = 1; row < only<QListWidget>(goTo)->count(); ++row) {
        only<QListWidget>(goTo)->setCurrentRow(row);
        layOut(goTo);
        INFO(row);
        CHECK(goTo.sizeHint().height() <= size.height() + 2);
        checkSmallScreen(goTo);
    }

    PasteSpecialDialog::Data data;
    data.source = ClipboardSource::Word;
    data.forms = {PasteForm::Source, PasteForm::Merge, PasteForm::Text, PasteForm::Image};
    data.chosen = PasteForm::Merge;
    PasteSpecialDialog paste(data);
    layOut(paste);
    CHECK(paste.windowTitle() == QStringLiteral("Wklej specjalnie"));
    CHECK(itemsOf(*only<QListWidget>(paste)) ==
          QStringList{QStringLiteral("[Format Kalahari (z uwagami)]"),
                      QStringLiteral("Zachowaj formatowanie źródła"),
                      QStringLiteral("Scal formatowanie"), QStringLiteral("Tylko tekst"),
                      QStringLiteral("Obraz")});
    // As the mockup has it
    CHECK(test::textsOf(paste).contains(QStringLiteral("Źródło: <b>Microsoft Word</b>")));
    CHECK(test::textsOf(paste).contains(QStringLiteral(
        "Zostają nagłówki, cytaty, akapity, wyróżnienia, listy i tabele; czcionka, rozmiar i "
        "kolor są takie jak w książce. Format Kalahari jest dostępny, gdy tekst skopiowano "
        "w Kalahari.")));
    checkKeys(paste);
    const QSize pasteSize = paste.sizeHint();
    for (int row = 1; row < only<QListWidget>(paste)->count(); ++row) {
        only<QListWidget>(paste)->setCurrentRow(row);
        layOut(paste);
        INFO(row);
        CHECK(paste.sizeHint().height() <= pasteSize.height() + 2);
        checkSmallScreen(paste);
    }

    // The gear: the Paste Command in Settings > Editor > General
    QToolButton* gear = nullptr;
    for (QToolButton* each : paste.findChildren<QToolButton*>()) {
        if (each->toolTip() == QStringLiteral("Ustawienia wklejania")) {
            gear = each;
        }
    }
    REQUIRE(gear != nullptr);
    gear->click();
    CHECK(openedPage == QString::fromLatin1(gui::SettingsDialog::PASTE_PAGE));
}
