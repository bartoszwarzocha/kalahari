/// @file test_grammar_coordinator.cpp
/// @brief Grammar as you type in the window: the checking follows the settings and the
///        language of the spelling, the editors of the documents check on its server, the
///        status bar says when the server does not check the text, Tools > Check Grammar
///        as You Type turns it on and off, and Editor > Grammar in the settings sets the
///        server and tests it

#include <catch2/catch_test_macros.hpp>
#include "../editor/editor_test_utils.h"
#include "../editor/fake_language_tool.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/editor/grammar_check_service.h"
#include "kalahari/editor/text_source_adapter.h"
#include "kalahari/gui/command_registrar.h"
#include "kalahari/gui/command_registry.h"
#include "kalahari/gui/grammar_coordinator.h"
#include "kalahari/gui/panels/editor_panel.h"
#include "kalahari/gui/settings_dialog.h"

#include <QAction>
#include <QCheckBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStatusBar>
#include <QTabWidget>
#include <QTextBlock>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>

#include <cstdio>
#include <string>

using namespace kalahari;
using namespace kalahari::gui;
using kalahari::test::FakeIssue;
using kalahari::test::FakeLanguageTool;
using kalahari::test::kmlOf;
using kalahari::test::waitUntil;

namespace {

constexpr const char* ENABLED_KEY = "editor.grammarCheck.enabled";
constexpr const char* SERVER_KEY = "editor.grammarCheck.serverUrl";
constexpr const char* LANGUAGE_KEY = "editor.spellCheck.language";

/// How long an answer of the server may take (ms)
constexpr int ANSWER_MS = 20000;

/// Gives the grammar settings, and the language of the spelling, back as they were
class KeptGrammarSettings {
public:
    KeptGrammarSettings()
        : m_enabled(settings().get<bool>(ENABLED_KEY, true))
        , m_server(settings().get<std::string>(SERVER_KEY, std::string()))
        , m_language(settings().get<std::string>(LANGUAGE_KEY, std::string())) {}
    ~KeptGrammarSettings() {
        try {
            settings().set<bool>(ENABLED_KEY, m_enabled);
            settings().set<std::string>(SERVER_KEY, m_server);
            settings().set<std::string>(LANGUAGE_KEY, m_language);
        } catch (...) {
            // Left as the test made them; the next tests set what they need
            std::fputs("The grammar settings could not be given back\n", stderr);
        }
    }
    KeptGrammarSettings(const KeptGrammarSettings&) = delete;
    KeptGrammarSettings& operator=(const KeptGrammarSettings&) = delete;

    static core::SettingsManager& settings() { return core::SettingsManager::getInstance(); }

private:
    bool m_enabled;
    std::string m_server;
    std::string m_language;
};

/// The phrases with a grammar wave in a paragraph, in their order
QStringList wavyPhrases(const editor::BookEditor& bookEditor, int paragraph) {
    const QString text = bookEditor.textDocument()->findBlockByNumber(paragraph).text();
    QStringList phrases;
    for (const editor::TextHighlight& highlight :
         editor::QTextDocumentSource(bookEditor.textDocument()).paragraphHighlights(paragraph)) {
        if (highlight.kind == editor::HighlightKind::Grammar) {
            phrases.append(text.mid(highlight.start, highlight.length));
        }
    }
    return phrases;
}

/// Open the page with a title under a category of the settings
void openPage(SettingsDialog& dialog, const QString& category, const QString& title) {
    auto* tree = dialog.findChild<QTreeWidget*>();
    REQUIRE(tree != nullptr);
    for (QTreeWidgetItemIterator it(tree); *it; ++it) {
        QTreeWidgetItem* parent = (*it)->parent();
        if ((*it)->text(0) == title && parent != nullptr && parent->text(0) == category) {
            tree->setCurrentItem(*it);
            return;
        }
    }
    FAIL("No page " << title.toStdString());
}

/// The field of the server on the page
QLineEdit* serverField(const SettingsDialog& dialog) {
    for (QLineEdit* field : dialog.findChildren<QLineEdit*>()) {
        if (field->accessibleName() == QStringLiteral("LanguageTool server")) {
            return field;
        }
    }
    return nullptr;
}

/// Whether a label of the dialog shows @p text
bool shows(const SettingsDialog& dialog, const QString& text) {
    for (const QLabel* label : dialog.findChildren<QLabel*>()) {
        if (label->text() == text) {
            return true;
        }
    }
    return false;
}

template <typename Button>
Button* buttonWithText(const QWidget& parent, const QString& text) {
    for (Button* button : parent.findChildren<Button*>()) {
        if (button->text() == text) {
            return button;
        }
    }
    return nullptr;
}

}  // namespace

TEST_CASE("Grammar coordinator: the checking follows the settings", "[gui][grammar]") {
    KeptGrammarSettings kept;
    auto& settings = KeptGrammarSettings::settings();
    settings.set<bool>(ENABLED_KEY, true);
    settings.set<std::string>(SERVER_KEY, "");
    settings.set<std::string>(LANGUAGE_KEY, "en_US");

    GrammarCoordinator coordinator(nullptr, nullptr);
    const editor::GrammarCheckService* service = coordinator.service();

    // No server: nothing is checked or sent
    CHECK(service->isEnabled());
    CHECK_FALSE(service->isConfigured());
    CHECK_FALSE(service->isActive());
    CHECK(service->language() == QStringLiteral("en-US"));

    // A server given in the settings
    settings.set<std::string>(SERVER_KEY, "localhost:8081");
    CHECK(waitUntil([service]() {
        return service->endpoint() == QStringLiteral("http://localhost:8081/v2/check");
    }));
    CHECK(service->isActive());

    // The language of the spelling
    settings.set<std::string>(LANGUAGE_KEY, "pl_PL");
    CHECK(waitUntil([service]() { return service->language() == QStringLiteral("pl"); }));

    // Turned off and on again
    settings.set<bool>(ENABLED_KEY, false);
    CHECK(waitUntil([service]() { return !service->isEnabled(); }));
    CHECK_FALSE(service->isActive());
    settings.set<bool>(ENABLED_KEY, true);
    CHECK(waitUntil([service]() { return service->isActive(); }));

    // The server taken away
    settings.set<std::string>(SERVER_KEY, "");
    CHECK(waitUntil([service]() { return !service->isConfigured(); }));
    CHECK_FALSE(service->isActive());
}

TEST_CASE("Grammar coordinator: the editors of the documents check on its server",
          "[gui][grammar]") {
    KeptGrammarSettings kept;
    FakeLanguageTool server;
    server.addIssue(FakeIssue(QStringLiteral("a apple"), QStringLiteral("EN_A_VS_AN")));
    auto& settings = KeptGrammarSettings::settings();
    settings.set<bool>(ENABLED_KEY, true);
    settings.set<std::string>(SERVER_KEY, server.url().toStdString());
    settings.set<std::string>(LANGUAGE_KEY, "en_US");

    QTabWidget tabs;
    auto* first = new EditorPanel();
    first->setContent(kmlOf({QStringLiteral("One is a apple.")}));
    tabs.addTab(first, QStringLiteral("One"));
    GrammarCoordinator coordinator(&tabs, nullptr);
    CHECK(first->getBookEditor()->grammarCheckService() == coordinator.service());

    // A document opened later, in a tab of its own
    auto* second = new EditorPanel();
    second->setContent(kmlOf({QStringLiteral("Two is a apple too.")}));
    tabs.setCurrentIndex(tabs.addTab(second, QStringLiteral("Two")));
    CHECK(second->getBookEditor()->grammarCheckService() == coordinator.service());

    // The editor shown gets its waves, checked in the language of the spelling
    tabs.resize(700, 500);
    tabs.show();
    const editor::BookEditor* shown = second->getBookEditor();
    CHECK(waitUntil(
        [shown]() { return wavyPhrases(*shown, 0) == QStringList{QStringLiteral("a apple")}; },
        ANSWER_MS));
    CHECK(server.texts().contains(QStringLiteral("Two is a apple too.")));
    CHECK(server.lastLanguage() == QStringLiteral("en-US"));
}

TEST_CASE("Grammar coordinator: the status bar says when the server does not check the text",
          "[gui][grammar]") {
    KeptGrammarSettings kept;
    FakeLanguageTool server;
    server.setFailure(500, "Internal error.");
    auto& settings = KeptGrammarSettings::settings();
    settings.set<bool>(ENABLED_KEY, true);
    settings.set<std::string>(SERVER_KEY, server.url().toStdString());

    QStatusBar statusBar;
    GrammarCoordinator coordinator(nullptr, &statusBar);
    REQUIRE(coordinator.service()->check(QStringLiteral("A text.")) != 0);
    const QString message =
        QStringLiteral("The LanguageTool server at %1 did not check the text: Internal error.")
            .arg(server.url());
    CHECK(waitUntil([&statusBar, &message]() { return statusBar.currentMessage() == message; },
                    ANSWER_MS));
}

TEST_CASE("Grammar coordinator: Check Grammar as You Type turns the checking on and off",
          "[gui][grammar][command]") {
    KeptGrammarSettings kept;
    auto& settings = KeptGrammarSettings::settings();
    settings.set<bool>(ENABLED_KEY, true);
    settings.set<std::string>(SERVER_KEY, "");

    registerAllCommands(CommandCallbacks{});
    auto& registry = CommandRegistry::getInstance();
    QStatusBar statusBar;
    {
        GrammarCoordinator coordinator(nullptr, &statusBar);
        coordinator.connectCommands();
        const editor::GrammarCheckService* service = coordinator.service();
        QAction* action = registry.getAction(std::string("tools.grammar"));
        REQUIRE(action != nullptr);
        CHECK(action->text() == QStringLiteral("Check Grammar as You Type"));
        CHECK(action->isCheckable());
        CHECK(action->isChecked());

        // In the Tools menu only
        const Command* command = registry.getCommand("tools.grammar");
        REQUIRE(command != nullptr);
        CHECK(command->showInMenu);
        CHECK_FALSE(command->showInToolbar);

        // Greyed out while no server is set
        CHECK_FALSE(action->isEnabled());
        settings.set<std::string>(SERVER_KEY, "http://localhost:8081");
        CHECK(waitUntil([action]() { return action->isEnabled(); }));

        action->trigger();
        CHECK_FALSE(settings.get<bool>(ENABLED_KEY, true));
        CHECK_FALSE(action->isChecked());
        CHECK(statusBar.currentMessage() == QStringLiteral("Grammar as you type: off"));
        CHECK(waitUntil([service]() { return !service->isEnabled(); }));

        action->trigger();
        CHECK(settings.get<bool>(ENABLED_KEY, false));
        CHECK(action->isChecked());
        CHECK(statusBar.currentMessage() == QStringLiteral("Grammar as you type: on"));
        CHECK(waitUntil([service]() { return service->isEnabled(); }));

        // Turned off in the settings: the command shows it
        settings.set<bool>(ENABLED_KEY, false);
        CHECK_FALSE(action->isChecked());
    }

    // The command must not reach the coordinator that is gone
    const Command* command = registry.getCommand("tools.grammar");
    REQUIRE(command != nullptr);
    CHECK_FALSE(static_cast<bool>(command->execute));
    CHECK_FALSE(static_cast<bool>(command->isEnabled));
    registry.clear();
}

TEST_CASE("Grammar settings: the server, greyed out while the checking is off, and its test",
          "[gui][grammar][settings]") {
    KeptGrammarSettings kept;
    auto& settings = KeptGrammarSettings::settings();
    settings.set<bool>(ENABLED_KEY, true);
    settings.set<std::string>(SERVER_KEY, "");
    FakeLanguageTool server;

    SettingsDialog dialog(nullptr);
    dialog.show();
    openPage(dialog, QStringLiteral("Editor"), QStringLiteral("Grammar"));
    CHECK_FALSE(dialog.hasChanges());

    QLineEdit* field = serverField(dialog);
    auto* enabled = buttonWithText<QCheckBox>(dialog, QStringLiteral("Check grammar as you type"));
    auto* test = buttonWithText<QPushButton>(dialog, QStringLiteral("Test"));
    REQUIRE(field != nullptr);
    REQUIRE(enabled != nullptr);
    REQUIRE(test != nullptr);
    CHECK(field->text().isEmpty());
    CHECK(field->placeholderText() == QStringLiteral("http://localhost:8081"));
    CHECK(enabled->isChecked());
    CHECK(field->isEnabled());
    CHECK(test->isEnabled());

    // Nothing to test without the address of a server
    test->click();
    CHECK(shows(dialog, QStringLiteral("This is not the address of a server.")));
    field->setText(QStringLiteral("ftp://localhost"));
    CHECK_FALSE(shows(dialog, QStringLiteral("This is not the address of a server.")));
    test->click();
    CHECK(shows(dialog, QStringLiteral("This is not the address of a server.")));

    // A server that checks the text
    field->setText(server.url());
    test->click();
    CHECK(shows(dialog, QStringLiteral("Connecting...")));
    CHECK(waitUntil(
        [&dialog]() { return shows(dialog, QStringLiteral("The server checks the text.")); },
        ANSWER_MS));
    CHECK(server.texts() == QStringList{QStringLiteral("Test.")});

    // A server that does not
    server.setFailure(500, "Internal error.");
    test->click();
    CHECK(waitUntil(
        [&dialog]() {
            return shows(dialog,
                         QStringLiteral("The server did not check the text: Internal error."));
        },
        ANSWER_MS));

    // The address is kept without the spaces around it
    field->setText(QStringLiteral("  %1  ").arg(server.url()));
    CHECK(dialog.hasChanges());
    QStringList applied = dialog.applyChanges();
    CHECK(applied.contains(QString::fromLatin1(SERVER_KEY)));
    CHECK(settings.get<std::string>(SERVER_KEY, std::string()) == server.url().toStdString());
    CHECK_FALSE(dialog.hasChanges());

    // The server, greyed out while the checking is off
    enabled->setChecked(false);
    CHECK_FALSE(field->isEnabled());
    CHECK_FALSE(test->isEnabled());
    applied = dialog.applyChanges();
    CHECK(applied.contains(QString::fromLatin1(ENABLED_KEY)));
    CHECK_FALSE(settings.get<bool>(ENABLED_KEY, true));
}
