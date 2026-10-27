/// @file test_link_dialog.cpp
/// @brief Insert > Link... (Ctrl+K): the window, its gear and what the editor gets from it,
///        and the links the editor panel opens

#include <catch2/catch_test_macros.hpp>
#include "../editor/editor_test_utils.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/editor/text_link.h"
#include "kalahari/gui/dialogs/link_dialog.h"
#include "kalahari/gui/panels/editor_panel.h"
#include "kalahari/gui/settings_dialog.h"

#include "document_window.h"

#include <QApplication>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QPushButton>
#include <QStatusBar>
#include <QTimer>
#include <QToolButton>

using namespace kalahari;
using namespace kalahari::editor;
using kalahari::gui::SettingsDialog;
using kalahari::gui::dialogs::KalahariDialog;
using kalahari::gui::dialogs::LinkDialog;

namespace {

/// The field a label names
QLineEdit* fieldOf(const QWidget& dialog, const QString& label)
{
    for (QLabel* each : dialog.findChildren<QLabel*>()) {
        if (each->text() == label) {
            return qobject_cast<QLineEdit*>(each->buddy());
        }
    }
    return nullptr;
}

/// A push button by its text
QPushButton* buttonOf(const QWidget& dialog, const QString& text)
{
    for (QPushButton* button : dialog.findChildren<QPushButton*>()) {
        if (button->text() == text) {
            return button;
        }
    }
    return nullptr;
}

/// The gear of the heading, by its name
QToolButton* gearOf(const QWidget& dialog)
{
    for (QToolButton* button : dialog.findChildren<QToolButton*>()) {
        if (button->accessibleName() == QStringLiteral("Link Color Settings")) {
            return button;
        }
    }
    return nullptr;
}

/// Close a window of another kind than the test waited for, so that the test goes on
void closeShownDialog()
{
    if (QDialog* dialog = test::shownDialog()) {
        dialog->reject();
    }
}

/// The program's way of opening the Settings dialog, until the test ends
class SettingsOpener {
public:
    explicit SettingsOpener(std::function<void(QWidget*, const QString&)> opener) {
        KalahariDialog::setSettingsOpener(std::move(opener));
    }
    ~SettingsOpener() { KalahariDialog::setSettingsOpener(nullptr); }
    SettingsOpener(const SettingsOpener&) = delete;
    SettingsOpener& operator=(const SettingsOpener&) = delete;
};

}  // namespace

TEST_CASE("Link dialog: the text, the address and OK only with an address",
          "[gui][links][dialog]") {
    LinkDialog dialog;
    dialog.show();
    QApplication::processEvents();
    CHECK(dialog.windowTitle() == QStringLiteral("Link"));
    QLineEdit* text = fieldOf(dialog, QStringLiteral("&Text to display"));
    QLineEdit* address = fieldOf(dialog, QStringLiteral("&Address"));
    REQUIRE(text != nullptr);
    REQUIRE(address != nullptr);
    QPushButton* remove = buttonOf(dialog, QStringLiteral("&Remove Link"));
    REQUIRE(remove != nullptr);

    SECTION("A new link: no Remove Link, OK once an address is written") {
        dialog.setLink(QStringLiteral("the page"), QString(), false, true);
        CHECK(text->text() == QStringLiteral("the page"));
        CHECK_FALSE(text->isReadOnly());
        CHECK_FALSE(remove->isVisible());
        CHECK_FALSE(dialog.acceptButton()->isEnabled());
        address->setText(QStringLiteral("   "));
        CHECK_FALSE(dialog.acceptButton()->isEnabled());
        address->setText(QStringLiteral("www.example.com"));
        CHECK(dialog.acceptButton()->isEnabled());
        CHECK(QApplication::focusWidget() == address);
        dialog.acceptButton()->click();
        CHECK(dialog.result() == QDialog::Accepted);
        CHECK_FALSE(dialog.removeAsked());
        CHECK(dialog.address() == QStringLiteral("www.example.com"));
        CHECK(dialog.text() == QStringLiteral("the page"));
    }

    SECTION("A link of the text: Remove Link closes the window asking for it") {
        dialog.setLink(QStringLiteral("page"), QStringLiteral("https://example.com"), true, true);
        CHECK(address->text() == QStringLiteral("https://example.com"));
        CHECK(address->selectedText() == QStringLiteral("https://example.com"));
        CHECK(remove->isVisible());
        CHECK(dialog.acceptButton()->isEnabled());
        remove->click();
        CHECK(dialog.result() == QDialog::Accepted);
        CHECK(dialog.removeAsked());
    }

    SECTION("A selection over paragraphs: its text stays as it is") {
        dialog.setLink(QStringLiteral("One Two"), QString(), false, false);
        CHECK(text->isReadOnly());
    }
}

TEST_CASE("Link dialog: the gear opens the colors of the editor in the Settings dialog",
          "[gui][links][dialog][settings]") {
    SECTION("Without the program's way of opening the Settings dialog: no gear") {
        LinkDialog dialog;
        CHECK(gearOf(dialog) == nullptr);
    }

    SECTION("With it: the gear asks for Editor > Colors, from the window") {
        QWidget* askedBy = nullptr;
        QString asked;
        const SettingsOpener opener([&](QWidget* dialog, const QString& page) {
            askedBy = dialog;
            asked = page;
        });
        LinkDialog dialog;
        QToolButton* gear = gearOf(dialog);
        REQUIRE(gear != nullptr);
        CHECK(gear->toolTip() == QStringLiteral("Link Color Settings"));
        gear->click();
        CHECK(asked == QString::fromLatin1(SettingsDialog::EDITOR_COLORS_PAGE));
        CHECK(askedBy == &dialog);
    }
}

TEST_CASE("Editor panel: Insert > Link puts the window's link in the text",
          "[gui][links][dialog]") {
    gui::EditorPanel panel;
    BookEditor* editor = panel.getBookEditor();
    editor->fromKml(QStringLiteral("<kml><p>One two</p></kml>"));
    editor->setSelection({{0, 4}, {0, 7}});

    // A new link on the selected text
    QTimer::singleShot(0, [&]() {
        auto* dialog = qobject_cast<LinkDialog*>(test::shownDialog());
        CHECK(dialog != nullptr);
        if (dialog == nullptr) {
            closeShownDialog();
            return;
        }
        CHECK(dialog->text() == QStringLiteral("two"));
        CHECK(dialog->address().isEmpty());
        fieldOf(*dialog, QStringLiteral("&Address"))->setText(QStringLiteral("example.com"));
        dialog->acceptButton()->click();
    });
    panel.showLinkDialog();
    CHECK(editor->toKml() ==
          QStringLiteral("<kml><p>One <a href=\"https://example.com\">two</a></p></kml>"));

    // The link edited at the cursor: the window shows it
    editor->setCursorPosition({0, 5});
    QTimer::singleShot(0, [&]() {
        auto* dialog = qobject_cast<LinkDialog*>(test::shownDialog());
        CHECK(dialog != nullptr);
        if (dialog == nullptr) {
            closeShownDialog();
            return;
        }
        CHECK(dialog->address() == QStringLiteral("https://example.com"));
        fieldOf(*dialog, QStringLiteral("&Text to display"))->setText(QStringLiteral("2"));
        dialog->acceptButton()->click();
    });
    panel.showLinkDialog();
    CHECK(editor->toKml() ==
          QStringLiteral("<kml><p>One <a href=\"https://example.com\">2</a></p></kml>"));

    // Cancel changes nothing
    editor->setCursorPosition({0, 4});
    QTimer::singleShot(0, [&]() {
        if (QDialog* dialog = test::shownDialog()) {
            dialog->reject();
        }
    });
    panel.showLinkDialog();
    CHECK(linksIn(editor->textDocument()->firstBlock()).size() == 1);

    // Remove Link: the text stays
    QTimer::singleShot(0, [&]() {
        auto* dialog = qobject_cast<LinkDialog*>(test::shownDialog());
        CHECK(dialog != nullptr);
        if (dialog == nullptr) {
            closeShownDialog();
            return;
        }
        buttonOf(*dialog, QStringLiteral("&Remove Link"))->click();
    });
    panel.showLinkDialog();
    CHECK(editor->toKml() == QStringLiteral("<kml><p>One 2</p></kml>"));
}

TEST_CASE("Editor panel: a link to another address than a page or an e-mail is not opened",
          "[gui][links]") {
    QMainWindow window;
    auto* panel = new gui::EditorPanel();
    window.setCentralWidget(panel);
    panel->openLink(QStringLiteral("file:///C:/Windows/notepad.exe"));
    CHECK(window.statusBar()->currentMessage() ==
          QStringLiteral("Kalahari opens only the addresses of web pages and e-mail addresses: "
                         "file:///C:/Windows/notepad.exe"));
}
