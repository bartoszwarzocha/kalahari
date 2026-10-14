/// @file test_grammar_coordinator.cpp
/// @brief Grammar as you type in the window: where the text is checked as the settings say
///        (LanguageTool on this computer, started when a document needs it, or a server in
///        the internet), the rules ignored kept, the status bar, Tools > Check Grammar as You
///        Type, the preparation in the background and Settings > Editor > Grammar

#include <catch2/catch_test_macros.hpp>

#include "../editor/editor_test_utils.h"
#include "../editor/fake_language_tool.h"
#include "grammar_test_utils.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/editor/grammar_check_service.h"
#include "kalahari/editor/text_source_adapter.h"
#include "kalahari/gui/command_registrar.h"
#include "kalahari/gui/command_registry.h"
#include "kalahari/gui/dialogs/grammar_setup_dialog.h"
#include "kalahari/gui/dialogs/message_dialog.h"
#include "kalahari/gui/grammar_components.h"
#include "kalahari/gui/grammar_coordinator.h"
#include "kalahari/gui/grammar_setup.h"
#include "kalahari/gui/language_tool_server.h"
#include "kalahari/gui/panels/editor_panel.h"
#include "kalahari/gui/settings/grammar_page.h"
#include "kalahari/gui/settings_dialog.h"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGroupBox>
#include <QHostAddress>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMap>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QStatusBar>
#include <QTabWidget>
#include <QTcpServer>
#include <QTemporaryDir>
#include <QTextBlock>
#include <QTimer>
#include <QToolButton>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>

#include <functional>
#include <string>
#include <utility>

using namespace kalahari;
using namespace kalahari::gui;
using namespace kalahari::test;
using dialogs::GrammarSetupDialog;
using dialogs::MessageDialog;
using Readiness = GrammarCoordinator::Readiness;
using ServerState = LanguageToolServer::State;

namespace {

/// How long an answer of a server, or its start, may take (ms)
constexpr int ANSWER_MS = 20000;

/// How long a preparation may take (ms)
constexpr int PREPARE_MS = 60000;

/// How often a modal window is looked for (ms)
constexpr int POLL_MS = 10;

core::SettingsManager& settings() {
    return KeptGrammarSettings::settings();
}

/// The grammar on this computer with nothing prepared, in American English
void setLocal(bool enabled = true) {
    auto& values = settings();
    values.set<bool>(GrammarCoordinator::ENABLED_KEY, enabled);
    values.set<std::string>(GrammarCoordinator::MODE_KEY, GrammarCoordinator::LOCAL_MODE);
    values.set<std::string>(GrammarCoordinator::SERVER_KEY, GrammarCoordinator::FREE_SERVER);
    values.set<std::string>(GrammarCoordinator::USERNAME_KEY, std::string());
    values.set<std::string>(GrammarCoordinator::API_KEY_KEY, std::string());
    values.set<std::string>(GrammarCoordinator::JAVA_KEY, std::string());
    values.set<std::string>(GrammarCoordinator::FOLDER_KEY, std::string());
    values.set<bool>(GrammarCoordinator::EXTERNAL_KEY, false);
    values.set<nlohmann::json>(GrammarCoordinator::IGNORED_RULES_KEY, nlohmann::json::object());
    values.set<std::string>("editor.spellCheck.language", "en_US");
}

/// A free port on this computer
int freePort() {
    QTcpServer server;
    REQUIRE(server.listen(QHostAddress::LocalHost));
    return server.serverPort();
}

/// Java and LanguageTool installed by the writer, in a folder of the test: Java 21 of
/// Eclipse Temurin, an old Java 11, LanguageTool 6.6, a folder without it and a text file
class InstalledComponents {
public:
    InstalledComponents() {
        REQUIRE(m_dir.isValid());
        add(QStringLiteral("jdk-21/release"),
            "IMPLEMENTOR=\"Eclipse Adoptium\"\nJAVA_VERSION=\"21.0.5\"\n");
        add(QStringLiteral("jdk-21/bin/") + javaProgramName(), "java");
        add(QStringLiteral("jdk-11/release"), "JAVA_VERSION=\"11.0.2\"\n");
        add(QStringLiteral("jdk-11/bin/") + javaProgramName(), "java");
        add(QStringLiteral("LanguageTool-6.6/languagetool-server.jar"), "server");
        add(QStringLiteral("empty/notes.txt"), "notes");
    }

    QString java() const { return path(QStringLiteral("jdk-21/bin/") + javaProgramName()); }
    QString oldJava() const { return path(QStringLiteral("jdk-11/bin/") + javaProgramName()); }
    QString languageTool() const { return path(QStringLiteral("LanguageTool-6.6")); }
    QString emptyFolder() const { return path(QStringLiteral("empty")); }
    QString notJava() const { return path(QStringLiteral("empty/notes.txt")); }
    QString missing(const QString& name) const { return path(QStringLiteral("missing/") + name); }

private:
    QString path(const QString& name) const {
        return QDir::toNativeSeparators(QDir(m_dir.path()).filePath(name));
    }

    void add(const QString& name, const QByteArray& content) {
        const QString file = QDir(m_dir.path()).filePath(name);
        REQUIRE(QDir().mkpath(QFileInfo(file).absolutePath()));
        QFile out(file);
        REQUIRE(out.open(QIODevice::WriteOnly));
        out.write(content);
    }

    QTemporaryDir m_dir;
};

/// Acts on the first modal window of a kind shown while this lives (looked for in turn:
/// the window is shown inside its exec())
template <typename Window>
class ModalAnswer {
public:
    explicit ModalAnswer(std::function<void(Window*)> action) : m_action(std::move(action)) {
        QObject::connect(&m_timer, &QTimer::timeout, &m_timer, [this]() {
            auto* window = qobject_cast<Window*>(QApplication::activeModalWidget());
            if (window != nullptr) {
                m_timer.stop();
                m_shown = true;
                m_action(window);
            }
        });
        m_timer.start(POLL_MS);
    }
    ModalAnswer(const ModalAnswer&) = delete;
    ModalAnswer& operator=(const ModalAnswer&) = delete;

    /// Whether the window was shown
    bool shown() const { return m_shown; }

private:
    std::function<void(Window*)> m_action;
    QTimer m_timer;
    bool m_shown{false};
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

/// Whether a label of a window, visible in it, shows a text
bool shows(const QWidget& window, const QString& text) {
    for (const QLabel* label : window.findChildren<QLabel*>()) {
        if (label->text() == text && label->isVisibleTo(&window)) {
            return true;
        }
    }
    return false;
}

/// Open the page with a title under a category of the settings
void openPage(SettingsDialog& dialog, const QString& category, const QString& title) {
    auto* tree = dialog.findChild<QTreeWidget*>();
    REQUIRE(tree != nullptr);
    for (QTreeWidgetItemIterator it(tree); *it; ++it) {
        const QTreeWidgetItem* parent = (*it)->parent();
        if ((*it)->text(0) == title && parent != nullptr && parent->text(0) == category) {
            tree->setCurrentItem(*it);
            return;
        }
    }
    FAIL("No page " << title.toStdString());
}

/// The text setting of a key
std::string textSetting(const char* key) {
    return settings().get<std::string>(key, std::string());
}

}  // namespace

// =============================================================================
// The coordinator
// =============================================================================

TEST_CASE("Grammar coordinator: what grammar as you type can do, from the settings",
          "[gui][grammar]") {
    InstalledComponents installed;
    const QString java = installed.java();
    const QString folder = installed.languageTool();

    // A server in the internet, or one started outside the program, needs nothing here
    CHECK(GrammarCoordinator::readinessOf(true, false, QString(), QString()) ==
          Readiness::Remote);
    CHECK(GrammarCoordinator::readinessOf(true, true, java, folder) == Readiness::Remote);
    CHECK(GrammarCoordinator::readinessOf(false, true, QString(), QString()) == Readiness::Ready);

    // The server the program starts: Java and the folder of LanguageTool
    CHECK(GrammarCoordinator::readinessOf(false, false, QString(), QString()) ==
          Readiness::NotPrepared);
    CHECK(GrammarCoordinator::readinessOf(false, false, java, QStringLiteral("  ")) ==
          Readiness::NotPrepared);
    CHECK(GrammarCoordinator::readinessOf(false, false, java, folder) == Readiness::Ready);
    CHECK(GrammarCoordinator::readinessOf(false, false, QStringLiteral(" %1 ").arg(java),
                                          folder) == Readiness::Ready);
    CHECK(GrammarCoordinator::readinessOf(false, false, installed.missing(QStringLiteral("java")),
                                          folder) == Readiness::Missing);
    CHECK(GrammarCoordinator::readinessOf(false, false, java, installed.emptyFolder()) ==
          Readiness::Missing);

    // What the settings say
    KeptGrammarSettings kept;
    setLocal();
    GrammarCoordinator coordinator(nullptr, nullptr);
    CHECK(coordinator.readiness() == Readiness::NotPrepared);
    CHECK_FALSE(coordinator.isChecking());
    settings().set<std::string>(GrammarCoordinator::JAVA_KEY, java.toStdString());
    settings().set<std::string>(GrammarCoordinator::FOLDER_KEY, folder.toStdString());
    CHECK(coordinator.readiness() == Readiness::Ready);
    CHECK(coordinator.isChecking());
    settings().set<bool>(GrammarCoordinator::ENABLED_KEY, false);
    CHECK_FALSE(coordinator.isChecking());
    settings().set<std::string>(GrammarCoordinator::MODE_KEY, GrammarCoordinator::REMOTE_MODE);
    CHECK(coordinator.readiness() == Readiness::Remote);
}

TEST_CASE("Grammar coordinator: on a server in the internet, with the account and in the "
          "language of the spelling",
          "[gui][grammar]") {
    KeptGrammarSettings kept;
    FakeLanguageTool languageTool;
    setLocal();
    settings().set<std::string>(GrammarCoordinator::MODE_KEY, GrammarCoordinator::REMOTE_MODE);
    settings().set<std::string>(GrammarCoordinator::SERVER_KEY,
                                languageTool.url().toStdString());
    settings().set<std::string>(GrammarCoordinator::USERNAME_KEY, "writer@example.com");
    settings().set<std::string>(GrammarCoordinator::API_KEY_KEY, "key-1");

    GrammarCoordinator coordinator(nullptr, nullptr);
    editor::GrammarCheckService* service = coordinator.service();
    CHECK(coordinator.readiness() == Readiness::Remote);
    CHECK(coordinator.isChecking());
    CHECK(service->isActive());
    CHECK(service->endpoint() == languageTool.url() + QStringLiteral("/v2/check"));
    CHECK(service->language() == QStringLiteral("en-US"));

    // Nothing runs on this computer for it
    CHECK(coordinator.server()->state() == ServerState::Stopped);

    // The text goes with the account
    REQUIRE(service->check(QStringLiteral("A text.")) != 0);
    REQUIRE(waitUntil([&languageTool]() { return languageTool.requests() == 1; }, ANSWER_MS));
    CHECK(languageTool.lastUsername() == QStringLiteral("writer@example.com"));
    CHECK(languageTool.lastApiKey() == QStringLiteral("key-1"));
    CHECK(languageTool.lastLanguage() == QStringLiteral("en-US"));

    // The language of the spelling
    settings().set<std::string>("editor.spellCheck.language", "pl_PL");
    CHECK(waitUntil([service]() { return service->language() == QStringLiteral("pl"); }));

    // Turned off and on again
    settings().set<bool>(GrammarCoordinator::ENABLED_KEY, false);
    CHECK(waitUntil([service]() { return !service->isEnabled(); }));
    CHECK_FALSE(coordinator.isChecking());
    settings().set<bool>(GrammarCoordinator::ENABLED_KEY, true);
    CHECK(waitUntil([service]() { return service->isActive(); }));

    // Back on this computer, with nothing prepared: nothing is sent anywhere
    settings().set<std::string>(GrammarCoordinator::MODE_KEY, GrammarCoordinator::LOCAL_MODE);
    CHECK(waitUntil([service]() { return !service->isConfigured(); }));
    CHECK_FALSE(service->isEnabled());
    CHECK_FALSE(service->isActive());
}

TEST_CASE("Grammar coordinator: on this computer nothing is checked or started before it is "
          "prepared",
          "[gui][grammar]") {
    KeptGrammarSettings kept;
    TemporaryGrammarFolder folder;
    InstalledComponents installed;
    setLocal();

    QTabWidget tabs;
    auto* panel = new EditorPanel();
    panel->setContent(kmlOf({QStringLiteral("One is a apple.")}));
    tabs.addTab(panel, QStringLiteral("One"));
    GrammarCoordinator coordinator(&tabs, nullptr);
    CHECK(panel->getBookEditor()->grammarCheckService() == coordinator.service());
    CHECK(coordinator.readiness() == Readiness::NotPrepared);
    CHECK_FALSE(coordinator.isChecking());
    CHECK_FALSE(coordinator.service()->isEnabled());
    CHECK_FALSE(coordinator.service()->isConfigured());
    runEventLoop(100);
    CHECK(coordinator.server()->state() == ServerState::Stopped);

    // The components set are gone
    settings().set<std::string>(GrammarCoordinator::JAVA_KEY,
                                installed.missing(QStringLiteral("java")).toStdString());
    settings().set<std::string>(GrammarCoordinator::FOLDER_KEY,
                                installed.languageTool().toStdString());
    runEventLoop(100);
    CHECK(coordinator.readiness() == Readiness::Missing);
    CHECK_FALSE(coordinator.isChecking());
    CHECK_FALSE(coordinator.service()->isEnabled());
    CHECK(coordinator.server()->state() == ServerState::Stopped);
}

TEST_CASE("Grammar coordinator: the editors of the documents check on LanguageTool on this "
          "computer, started when a document needs it",
          "[gui][grammar]") {
    KeptGrammarSettings kept;
    FakeLanguageTool languageTool;
    languageTool.addIssue(FakeIssue(QStringLiteral("a apple"), QStringLiteral("EN_A_VS_AN")));
    setLocal();
    settings().set<bool>(GrammarCoordinator::EXTERNAL_KEY, true);
    settings().set<int>(GrammarCoordinator::PORT_KEY, languageTool.port());

    QTabWidget tabs;
    QStatusBar statusBar;
    GrammarCoordinator coordinator(&tabs, &statusBar);
    const editor::GrammarCheckService* service = coordinator.service();
    const LanguageToolServer* server = coordinator.server();
    CHECK(coordinator.isChecking());

    // No document: nothing is started yet
    runEventLoop(100);
    CHECK(server->state() == ServerState::Stopped);
    CHECK_FALSE(service->isConfigured());

    // A document opened: the server is started and the editor checks on it
    auto* panel = new EditorPanel();
    panel->setContent(kmlOf({QStringLiteral("One is a apple.")}));
    tabs.setCurrentIndex(tabs.addTab(panel, QStringLiteral("One")));
    CHECK(panel->getBookEditor()->grammarCheckService() == service);
    CHECK(server->state() == ServerState::Starting);
    CHECK(statusBar.currentMessage() == QStringLiteral("Starting LanguageTool..."));
    REQUIRE(waitUntil([server]() { return server->state() == ServerState::Running; }, ANSWER_MS));
    CHECK(service->endpoint() ==
          QStringLiteral("http://localhost:%1/v2/check").arg(languageTool.port()));
    CHECK(statusBar.currentMessage().isEmpty());

    tabs.resize(700, 500);
    tabs.show();
    const editor::BookEditor* bookEditor = panel->getBookEditor();
    CHECK(waitUntil(
        [bookEditor]() {
            return wavyPhrases(*bookEditor, 0) == QStringList{QStringLiteral("a apple")};
        },
        ANSWER_MS));
    CHECK(languageTool.texts().contains(QStringLiteral("One is a apple.")));
    CHECK(languageTool.lastLanguage() == QStringLiteral("en-US"));

    // Turned off: the program does not connect to it any more
    settings().set<bool>(GrammarCoordinator::ENABLED_KEY, false);
    CHECK(waitUntil([server]() { return server->state() == ServerState::Stopped; }));
    CHECK_FALSE(service->isEnabled());
    CHECK_FALSE(service->isConfigured());
}

TEST_CASE("Grammar coordinator: a rule ignored in an editor is kept in the settings",
          "[gui][grammar]") {
    KeptGrammarSettings kept;
    setLocal();
    GrammarCoordinator coordinator(nullptr, nullptr);
    editor::GrammarCheckService* service = coordinator.service();

    service->ignoreRule(QStringLiteral("EN_A_VS_AN"), QStringLiteral("Use of 'a' vs. 'an'"));
    const QMap<QString, QString> ignored{
        {QStringLiteral("EN_A_VS_AN"), QStringLiteral("Use of 'a' vs. 'an'")}};
    CHECK(GrammarCoordinator::ignoredRules() == ignored);

    // Kept once, also when it is ignored again
    service->ignoreRule(QStringLiteral("EN_A_VS_AN"), QStringLiteral("Another description"));
    CHECK(GrammarCoordinator::ignoredRules() == ignored);

    // Restored in the settings: reported again
    GrammarCoordinator::setIgnoredRules({});
    CHECK(waitUntil(
        [service]() { return !service->isRuleIgnored(QStringLiteral("EN_A_VS_AN")); }));

    // After a restart the rules the settings keep are ignored
    GrammarCoordinator::setIgnoredRules({{QStringLiteral("COMMA_RULE"), QString()}});
    GrammarCoordinator again(nullptr, nullptr);
    CHECK(again.service()->isRuleIgnored(QStringLiteral("COMMA_RULE")));
}

TEST_CASE("Grammar coordinator: Check Grammar as You Type turns the checking on and off, and "
          "offers to prepare it",
          "[gui][grammar][command]") {
    KeptGrammarSettings kept;
    TemporaryGrammarFolder folder;
    setLocal();

    registerAllCommands(CommandCallbacks{});
    auto& registry = CommandRegistry::getInstance();
    QStatusBar statusBar;
    {
        GrammarCoordinator coordinator(nullptr, &statusBar);
        coordinator.connectCommands();
        const editor::GrammarCheckService* service = coordinator.service();
        QAction* action = registry.getAction(std::string("tools.grammar"));
        REQUIRE(action != nullptr);
        CHECK(action->text() == QStringLiteral("Check &Grammar as You Type"));
        CHECK(action->isCheckable());
        CHECK(action->isEnabled());

        // In the Tools menu only
        const Command* command = registry.getCommand("tools.grammar");
        REQUIRE(command != nullptr);
        CHECK(command->showInMenu);
        CHECK_FALSE(command->showInToolbar);

        // On in the settings, but nothing prepared: not checked; the command offers to
        // prepare it
        CHECK_FALSE(action->isChecked());
        QString question;
        QStringList buttons;
        {
            ModalAnswer<MessageDialog> notNow([&question, &buttons](MessageDialog* dialog) {
                question = dialog->clipboardText();
                buttons = {dialog->acceptButton()->text(), dialog->cancelButton()->text()};
                dialog->cancelButton()->click();
            });
            action->trigger();
            CHECK(notNow.shown());
        }
        CHECK(question.contains(QStringLiteral("Prepare grammar checking?")));
        CHECK(question.contains(QStringLiteral("about %1 to download.")
                                    .arg(GrammarComponents::downloadSizeText())));
        CHECK(buttons == QStringList({QStringLiteral("&Prepare Now"), QStringLiteral("&Not Now")}));
        CHECK_FALSE(action->isChecked());
        CHECK(settings().get<bool>(GrammarCoordinator::ENABLED_KEY, false));

        // Prepare Now shows the preparation
        {
            ModalAnswer<MessageDialog> prepareNow(
                [](MessageDialog* dialog) { dialog->acceptButton()->click(); });
            GrammarSetupDialog::Page page = GrammarSetupDialog::Page::Done;
            ModalAnswer<GrammarSetupDialog> window([&page](GrammarSetupDialog* dialog) {
                page = dialog->page();
                dialog->reject();
            });
            action->trigger();
            CHECK(prepareNow.shown());
            CHECK(window.shown());
            CHECK(page == GrammarSetupDialog::Page::Plan);
        }
        CHECK_FALSE(coordinator.setup()->isRunning());
        CHECK_FALSE(action->isChecked());

        // Ready (here on a server in the internet): the command turns it off and on
        settings().set<std::string>(GrammarCoordinator::MODE_KEY,
                                    GrammarCoordinator::REMOTE_MODE);
        settings().set<std::string>(GrammarCoordinator::SERVER_KEY, "http://localhost:8081");
        CHECK(waitUntil([action]() { return action->isChecked(); }));

        action->trigger();
        CHECK_FALSE(settings().get<bool>(GrammarCoordinator::ENABLED_KEY, true));
        CHECK_FALSE(action->isChecked());
        CHECK(statusBar.currentMessage() == QStringLiteral("Grammar as you type: off"));
        CHECK(waitUntil([service]() { return !service->isEnabled(); }));

        action->trigger();
        CHECK(settings().get<bool>(GrammarCoordinator::ENABLED_KEY, false));
        CHECK(action->isChecked());
        CHECK(statusBar.currentMessage() == QStringLiteral("Grammar as you type: on"));
        CHECK(waitUntil([service]() { return service->isEnabled(); }));

        // Turned off in the settings: the command shows it
        settings().set<bool>(GrammarCoordinator::ENABLED_KEY, false);
        CHECK(waitUntil([action]() { return !action->isChecked(); }));
    }

    // The command must not reach the coordinator that is gone
    const Command* command = registry.getCommand("tools.grammar");
    REQUIRE(command != nullptr);
    CHECK_FALSE(static_cast<bool>(command->execute));
    CHECK_FALSE(static_cast<bool>(command->isChecked));
    registry.clear();
}

TEST_CASE("Grammar coordinator: the status bar says when the text is not checked",
          "[gui][grammar]") {
    KeptGrammarSettings kept;
    FakeLanguageTool languageTool;
    InstalledComponents installed;
    setLocal();
    QStatusBar statusBar;

    SECTION("a server in the internet that does not check it") {
        settings().set<std::string>(GrammarCoordinator::MODE_KEY,
                                    GrammarCoordinator::REMOTE_MODE);
        settings().set<std::string>(GrammarCoordinator::SERVER_KEY,
                                    languageTool.url().toStdString());
        GrammarCoordinator coordinator(nullptr, &statusBar);
        languageTool.setFailure(500, "Internal error.");
        REQUIRE(coordinator.service()->check(QStringLiteral("A text.")) != 0);
        const QString message =
            QStringLiteral("The LanguageTool server at %1 did not check the text: Internal error.")
                .arg(languageTool.url());
        CHECK(waitUntil([&statusBar, &message]() { return statusBar.currentMessage() == message; },
                        ANSWER_MS));
    }

    SECTION("a server in the internet whose limit is exceeded") {
        settings().set<std::string>(GrammarCoordinator::MODE_KEY,
                                    GrammarCoordinator::REMOTE_MODE);
        settings().set<std::string>(GrammarCoordinator::SERVER_KEY,
                                    languageTool.url().toStdString());
        GrammarCoordinator coordinator(nullptr, &statusBar);
        languageTool.setFailure(429, "Too many requests.");
        REQUIRE(coordinator.service()->check(QStringLiteral("A text.")) != 0);
        const QString message =
            QStringLiteral("The server refused the request because the limit was exceeded. The "
                           "program will try again in a moment.");
        CHECK(waitUntil([&statusBar, &message]() { return statusBar.currentMessage() == message; },
                        ANSWER_MS));
    }

    SECTION("LanguageTool on this computer that does not check it") {
        settings().set<bool>(GrammarCoordinator::EXTERNAL_KEY, true);
        settings().set<int>(GrammarCoordinator::PORT_KEY, languageTool.port());
        GrammarCoordinator coordinator(nullptr, &statusBar);
        coordinator.server()->start();
        REQUIRE(waitUntil([&coordinator]() { return coordinator.service()->isActive(); },
                          ANSWER_MS));
        languageTool.setFailure(500, "Internal error.");
        REQUIRE(coordinator.service()->check(QStringLiteral("A text.")) != 0);
        const QString message =
            QStringLiteral("LanguageTool on this computer did not check the text: Internal error.");
        CHECK(waitUntil([&statusBar, &message]() { return statusBar.currentMessage() == message; },
                        ANSWER_MS));
    }

    SECTION("LanguageTool that does not start") {
        settings().set<std::string>(GrammarCoordinator::JAVA_KEY,
                                    fakeJavaProgram().toStdString());
        settings().set<std::string>(GrammarCoordinator::FOLDER_KEY,
                                    installed.languageTool().toStdString());
        settings().set<int>(GrammarCoordinator::PORT_KEY, freePort());
        ScopedEnvironment fail("KALAHARI_FAKE_JAVA_FAIL", "1");
        GrammarCoordinator coordinator(nullptr, &statusBar);
        coordinator.server()->start();
        const QString message =
            QStringLiteral("Grammar is not checked: LanguageTool did not start. Details: Edit > "
                           "Settings..., Editor > Grammar.");
        CHECK(waitUntil([&statusBar, &message]() { return statusBar.currentMessage() == message; },
                        ANSWER_MS));
    }

    SECTION("LanguageTool that stopped by itself, started again") {
        settings().set<std::string>(GrammarCoordinator::JAVA_KEY,
                                    fakeJavaProgram().toStdString());
        settings().set<std::string>(GrammarCoordinator::FOLDER_KEY,
                                    installed.languageTool().toStdString());
        settings().set<int>(GrammarCoordinator::PORT_KEY, freePort());
        QTemporaryDir marks;
        ScopedEnvironment exits("KALAHARI_FAKE_JAVA_EXIT_MS", "300");
        const QString mark = QDir(marks.path()).filePath(QStringLiteral("stopped"));
        ScopedEnvironment once("KALAHARI_FAKE_JAVA_EXIT_ONCE", mark.toLocal8Bit());
        QStringList messages;
        QObject::connect(&statusBar, &QStatusBar::messageChanged, &statusBar,
                         [&messages](const QString& message) {
                             if (!message.isEmpty()) {
                                 messages.append(message);
                             }
                         });
        GrammarCoordinator coordinator(nullptr, &statusBar);
        const LanguageToolServer* server = coordinator.server();
        coordinator.server()->start();
        const QString restarting =
            QStringLiteral("Grammar is not checked: LanguageTool stopped answering. The program "
                           "starts it again.");
        REQUIRE(waitUntil([&messages, &restarting]() { return messages.contains(restarting); },
                          ANSWER_MS));
        REQUIRE(waitUntil([server]() { return server->state() == ServerState::Running; },
                          ANSWER_MS));
        CHECK(statusBar.currentMessage().isEmpty());
        CHECK(messages.first() == QStringLiteral("Starting LanguageTool..."));
        CHECK(messages.count(QStringLiteral("Starting LanguageTool...")) == 1);
    }
}

TEST_CASE("Grammar coordinator: the preparation in the background, the settings it writes and "
          "the removal of what it downloaded",
          "[gui][grammar][setup]") {
    KeptGrammarSettings kept;
    TemporaryGrammarFolder folder;
    FakeJavaLibraries libraries;
    setLocal(false);
    settings().set<int>(GrammarCoordinator::PORT_KEY, freePort());
    FakeDownloads site;
    const QByteArray java = javaZip();
    const QByteArray languageTool = languageToolZip();
    site.add(javaFileName(), java);
    site.add(languageToolFileName(), languageTool);

    QStatusBar statusBar;
    GrammarCoordinator coordinator(nullptr, &statusBar);
    GrammarSetup* setup = coordinator.setup();
    setup->setJavaDownload(downloadOf(site, javaFileName(), java));
    setup->setLanguageToolDownload(downloadOf(site, languageToolFileName(), languageTool));
    setup->setLanguage(QStringLiteral("en_US"));

    EditorGrammarPage page(&coordinator);
    page.load();
    page.advancedToggle()->setChecked(true);
    CHECK(page.card() == EditorGrammarPage::Card::NotPrepared);
    CHECK_FALSE(page.removeButton()->isVisibleTo(&page));

    // Going on in the background: the card and a button in the status bar say how far
    site.hold();
    setup->start();
    REQUIRE(setup->isRunning());
    CHECK(page.card() == EditorGrammarPage::Card::Preparing);
    CHECK(page.cardTitle()->text() == QStringLiteral("Grammar checking is being prepared"));
    CHECK(page.cardText()->text().startsWith(QStringLiteral("Downloading Java - ")));
    CHECK_FALSE(page.prepareButton()->isVisibleTo(&page));
    CHECK_FALSE(page.checkServerButton()->isEnabled());
    auto* button = statusBar.findChild<QToolButton*>(QStringLiteral("grammarSetupButton"));
    REQUIRE(button != nullptr);
    CHECK(button->isVisibleTo(&statusBar));
    CHECK(button->text().startsWith(QStringLiteral("Grammar: downloading Java - ")));

    site.release();
    REQUIRE(waitUntil([setup]() { return !setup->isRunning(); }, PREPARE_MS));
    REQUIRE(setup->state() == GrammarSetup::State::Finished);

    // Checked from now on, with what was downloaded
    CHECK(textSetting(GrammarCoordinator::JAVA_KEY) ==
          GrammarComponents::downloadedJava().toStdString());
    CHECK(textSetting(GrammarCoordinator::FOLDER_KEY) ==
          GrammarComponents::languageToolFolder().toStdString());
    CHECK(settings().get<bool>(GrammarCoordinator::ENABLED_KEY, false));
    CHECK(coordinator.readiness() == Readiness::Ready);
    CHECK(coordinator.isChecking());
    CHECK(statusBar.currentMessage() == QStringLiteral("Grammar checking is ready"));
    CHECK_FALSE(button->isVisibleTo(&statusBar));
    CHECK(coordinator.server()->state() == ServerState::Running);
    CHECK(waitUntil([&coordinator]() { return coordinator.service()->isActive(); }));

    // The page shows it
    CHECK(page.card() == EditorGrammarPage::Card::Ready);
    CHECK(page.enabledBox()->isChecked());
    CHECK(page.enabledBox()->isEnabled());
    CHECK(page.javaField()->text() == GrammarComponents::downloadedJava());
    CHECK(page.folderField()->text() == GrammarComponents::languageToolFolder());
    CHECK(shows(page, QStringLiteral("LanguageTool 6.6 - Java 21 (Eclipse Temurin)")));
    CHECK(page.javaStatus()->text() == QStringLiteral("Java 21 (Eclipse Temurin)"));
    CHECK(page.folderStatus()->text() == QStringLiteral("LanguageTool 6.6"));
    CHECK(page.removeButton()->isVisibleTo(&page));
    CHECK(page.removeButton()->isEnabled());
    CHECK_FALSE(page.isChanged());

    // Removed after a question: Cancel keeps everything
    {
        ModalAnswer<MessageDialog> cancel(
            [](MessageDialog* dialog) { dialog->cancelButton()->click(); });
        page.removeButton()->click();
        CHECK(cancel.shown());
    }
    CHECK(GrammarComponents::languageToolDownloaded());
    CHECK(coordinator.readiness() == Readiness::Ready);

    QString question;
    {
        ModalAnswer<MessageDialog> remove([&question](MessageDialog* dialog) {
            question = dialog->clipboardText();
            dialog->acceptButton()->click();
        });
        page.removeButton()->click();
        CHECK(remove.shown());
    }
    CHECK(question.contains(QStringLiteral("Remove the downloaded components?")));
    CHECK_FALSE(GrammarComponents::anythingDownloaded());
    CHECK(coordinator.server()->state() == ServerState::Stopped);
    CHECK(textSetting(GrammarCoordinator::JAVA_KEY).empty());
    CHECK(textSetting(GrammarCoordinator::FOLDER_KEY).empty());
    CHECK_FALSE(settings().get<bool>(GrammarCoordinator::ENABLED_KEY, true));
    CHECK(coordinator.readiness() == Readiness::NotPrepared);

    // The page shows it at once
    CHECK(page.card() == EditorGrammarPage::Card::NotPrepared);
    CHECK(page.javaField()->text().isEmpty());
    CHECK_FALSE(page.enabledBox()->isChecked());
    CHECK_FALSE(page.removeButton()->isVisibleTo(&page));
    CHECK_FALSE(page.isChanged());
}

TEST_CASE("Grammar coordinator: a preparation that failed in the background is told by the "
          "status bar, whose button shows why",
          "[gui][grammar][setup]") {
    KeptGrammarSettings kept;
    TemporaryGrammarFolder folder;
    setLocal(false);
    FakeDownloads site;
    const QByteArray java = javaZip();
    site.add(javaFileName(), java);
    site.setStatus(javaFileName(), 503);

    QStatusBar statusBar;
    GrammarCoordinator coordinator(nullptr, &statusBar);
    GrammarSetup* setup = coordinator.setup();
    setup->setJavaDownload(downloadOf(site, javaFileName(), java));
    setup->start();
    REQUIRE(waitUntil([setup]() { return !setup->isRunning(); }, PREPARE_MS));
    CHECK(setup->state() == GrammarSetup::State::Failed);
    CHECK(coordinator.readiness() == Readiness::NotPrepared);

    auto* button = statusBar.findChild<QToolButton*>(QStringLiteral("grammarSetupButton"));
    REQUIRE(button != nullptr);
    CHECK(button->isVisibleTo(&statusBar));
    CHECK(button->text() ==
          QStringLiteral("Grammar checking could not be prepared - click to see the details"));

    // The button shows the window of the preparation, with why it failed
    GrammarSetupDialog::Page page = GrammarSetupDialog::Page::Plan;
    {
        ModalAnswer<GrammarSetupDialog> window([&page](GrammarSetupDialog* dialog) {
            page = dialog->page();
            dialog->reject();
        });
        button->click();
        CHECK(window.shown());
    }
    CHECK(page == GrammarSetupDialog::Page::Failed);
    CHECK_FALSE(button->isVisibleTo(&statusBar));
}

// =============================================================================
// Settings > Editor > Grammar
// =============================================================================

TEST_CASE("Grammar settings: the card says what grammar as you type can do",
          "[gui][grammar][settings]") {
    KeptGrammarSettings kept;
    TemporaryGrammarFolder folder;
    InstalledComponents installed;
    setLocal();

    // Java downloaded before, LanguageTool not yet
    REQUIRE(QDir().mkpath(GrammarComponents::archiveFolder()));
    QFile archive(QDir(GrammarComponents::archiveFolder()).filePath(javaFileName()));
    REQUIRE(archive.open(QIODevice::WriteOnly));
    archive.close();

    GrammarCoordinator coordinator(nullptr, nullptr);
    EditorGrammarPage page(&coordinator);
    page.load();
    CHECK_FALSE(page.isChanged());

    // Nothing prepared yet: the button that prepares it; the switch waits for it
    CHECK(page.card() == EditorGrammarPage::Card::NotPrepared);
    CHECK(page.cardTitle()->text() == QStringLiteral("Grammar checking is not prepared yet"));
    CHECK(page.cardText()->text().endsWith(
        QStringLiteral("About %1 to download.").arg(GrammarComponents::downloadSizeText())));
    bool javaNote = false;
    for (const QLabel* label : page.findChildren<QLabel*>()) {
        javaNote = javaNote ||
                   (label->isVisibleTo(&page) &&
                    label->text().startsWith(QStringLiteral("Java is downloaded already")));
    }
    CHECK(javaNote);
    CHECK(page.prepareButton()->isVisibleTo(&page));
    CHECK(page.prepareButton()->text() == QStringLiteral("&Prepare Grammar Checking..."));
    CHECK(page.enabledBox()->isChecked());
    CHECK_FALSE(page.enabledBox()->isEnabled());
    CHECK(page.localButton()->isChecked());
    CHECK_FALSE(page.remoteGroup()->isVisibleTo(&page));
    CHECK(page.advancedToggle()->isVisibleTo(&page));
    CHECK_FALSE(page.advancedSettings()->isVisibleTo(&page));

    // Components chosen that are not there
    page.javaField()->setText(installed.missing(QStringLiteral("java")));
    page.folderField()->setText(installed.missing(QStringLiteral("LanguageTool")));
    CHECK(page.card() == EditorGrammarPage::Card::Missing);
    CHECK(page.cardTitle()->text() == QStringLiteral("LanguageTool or Java was not found"));
    CHECK(page.prepareButton()->isVisibleTo(&page));
    CHECK_FALSE(page.enabledBox()->isEnabled());

    // Ready on this computer: what it runs with
    page.javaField()->setText(installed.java());
    page.folderField()->setText(installed.languageTool());
    CHECK(page.card() == EditorGrammarPage::Card::Ready);
    CHECK(page.cardTitle()->text() == QStringLiteral("Grammar checking is ready"));
    CHECK(page.cardText()->text() == QStringLiteral("It works on this computer - the text of the "
                                                    "book does not leave the computer."));
    CHECK(shows(page, QStringLiteral("LanguageTool 6.6 - Java 21 (Eclipse Temurin)")));
    CHECK_FALSE(page.prepareButton()->isVisibleTo(&page));
    CHECK(page.enabledBox()->isEnabled());

    // A server started outside the program
    page.javaField()->clear();
    page.externalBox()->setChecked(true);
    CHECK(page.card() == EditorGrammarPage::Card::Ready);
    CHECK(page.cardText()->text().startsWith(
        QStringLiteral("It connects to the LanguageTool server started outside the program")));

    // A server in the internet
    page.setRemoteWithoutAsking(true);
    CHECK(page.card() == EditorGrammarPage::Card::Remote);
    CHECK(page.cardTitle()->text() ==
          QStringLiteral("The grammar is checked on a server in the internet"));
    CHECK(page.cardText()->text() ==
          QStringLiteral("The paragraphs checked are sent to the server api.languagetool.org."));
    CHECK(page.remoteGroup()->isVisibleTo(&page));
    CHECK_FALSE(page.advancedToggle()->isVisibleTo(&page));
    CHECK(page.enabledBox()->isEnabled());
    CHECK(page.isChanged());

    // Applied: the coordinator follows
    page.apply();
    CHECK(textSetting(GrammarCoordinator::MODE_KEY) == GrammarCoordinator::REMOTE_MODE);
    CHECK(coordinator.readiness() == Readiness::Remote);
    CHECK_FALSE(page.isChanged());
}

TEST_CASE("Grammar settings: with both files downloaded nothing is left to download",
          "[gui][grammar][settings]") {
    KeptGrammarSettings kept;
    TemporaryGrammarFolder folder;
    setLocal();

    // Both files downloaded before, neither unpacked yet
    REQUIRE(QDir().mkpath(GrammarComponents::archiveFolder()));
    for (const QString& name : {javaFileName(), languageToolFileName()}) {
        QFile archive(QDir(GrammarComponents::archiveFolder()).filePath(name));
        REQUIRE(archive.open(QIODevice::WriteOnly));
        archive.close();
    }

    GrammarCoordinator coordinator(nullptr, nullptr);
    EditorGrammarPage page(&coordinator);
    page.load();
    CHECK(page.card() == EditorGrammarPage::Card::NotPrepared);
    CHECK(shows(page, QStringLiteral(
                          "Java and LanguageTool are downloaded already - nothing is left to "
                          "download.")));
    bool languageToolLeft = false;
    for (const QLabel* label : page.findChildren<QLabel*>()) {
        languageToolLeft = languageToolLeft ||
                           (label->isVisibleTo(&page) &&
                            label->text().startsWith(QStringLiteral("Java is downloaded already")));
    }
    CHECK_FALSE(languageToolLeft);
}

TEST_CASE("Grammar settings: Prepare Grammar Checking... shows the preparation",
          "[gui][grammar][settings]") {
    KeptGrammarSettings kept;
    TemporaryGrammarFolder folder;
    setLocal();
    GrammarCoordinator coordinator(nullptr, nullptr);
    EditorGrammarPage page(&coordinator);
    page.load();
    REQUIRE(page.prepareButton()->isVisibleTo(&page));

    GrammarSetupDialog::Page shown = GrammarSetupDialog::Page::Done;
    {
        ModalAnswer<GrammarSetupDialog> window([&shown](GrammarSetupDialog* dialog) {
            shown = dialog->page();
            dialog->reject();
        });
        page.prepareButton()->click();
        CHECK(window.shown());
    }
    CHECK(shown == GrammarSetupDialog::Page::Plan);
    CHECK(page.card() == EditorGrammarPage::Card::NotPrepared);
    CHECK_FALSE(coordinator.setup()->isRunning());

    // Without the grammar of the editors there is nothing to prepare
    EditorGrammarPage alone;
    alone.load();
    CHECK(alone.card() == EditorGrammarPage::Card::NotPrepared);
    CHECK_FALSE(alone.prepareButton()->isVisibleTo(&alone));
}

TEST_CASE("Grammar settings: the text goes to a server in the internet only after the writer "
          "agreed",
          "[gui][grammar][settings]") {
    KeptGrammarSettings kept;
    TemporaryGrammarFolder folder;
    setLocal(false);
    EditorGrammarPage page;
    page.load();
    REQUIRE(page.localButton()->isChecked());
    REQUIRE_FALSE(page.enabledBox()->isChecked());

    // Cancel: back to this computer
    QString question;
    QString sendButton;
    {
        ModalAnswer<MessageDialog> cancel([&question, &sendButton](MessageDialog* dialog) {
            question = dialog->clipboardText();
            sendButton = dialog->acceptButton()->text();
            dialog->cancelButton()->click();
        });
        page.remoteButton()->click();
        REQUIRE(waitUntil([&cancel]() { return cancel.shown(); }));
    }
    CHECK(question.contains(
        QStringLiteral("Send the text of the book to a server in the internet?")));
    CHECK(question.contains(QStringLiteral("Checking on this computer sends the text nowhere.")));
    CHECK(sendButton == QStringLiteral("&Send Text"));
    CHECK(waitUntil([&page]() { return page.localButton()->isChecked(); }));
    CHECK(page.card() == EditorGrammarPage::Card::NotPrepared);
    CHECK_FALSE(page.isChanged());

    // Send Text: the server in the internet
    {
        ModalAnswer<MessageDialog> agree(
            [](MessageDialog* dialog) { dialog->acceptButton()->click(); });
        page.remoteButton()->click();
        REQUIRE(waitUntil([&agree]() { return agree.shown(); }));
    }
    runEventLoop(50);
    CHECK(page.remoteButton()->isChecked());
    CHECK(page.card() == EditorGrammarPage::Card::Remote);
    // Chosen to be checked there: the checking is turned on with it
    CHECK(page.enabledBox()->isChecked());
    page.apply();
    CHECK(textSetting(GrammarCoordinator::MODE_KEY) == GrammarCoordinator::REMOTE_MODE);
    CHECK(KeptGrammarSettings::settings().get<bool>(GrammarCoordinator::ENABLED_KEY, false));

    // Agreed before: not asked again
    EditorGrammarPage again;
    again.load();
    REQUIRE(again.remoteButton()->isChecked());
    ModalAnswer<MessageDialog> none([](MessageDialog* dialog) { dialog->cancelButton()->click(); });
    again.localButton()->click();
    again.remoteButton()->click();
    runEventLoop(100);
    CHECK_FALSE(none.shown());
    CHECK(again.remoteButton()->isChecked());
}

TEST_CASE("Grammar settings: the server in the internet, the account and the check",
          "[gui][grammar][settings]") {
    KeptGrammarSettings kept;
    TemporaryGrammarFolder folder;
    FakeLanguageTool languageTool;
    setLocal();
    EditorGrammarPage page;
    page.load();
    page.setRemoteWithoutAsking(true);

    // The servers offered: the free one at first
    QComboBox* servers = page.serverBox();
    REQUIRE(servers->count() == 2);
    CHECK(servers->itemData(0).toString() == QLatin1String(GrammarCoordinator::FREE_SERVER));
    CHECK(servers->itemData(1).toString() == QLatin1String(GrammarCoordinator::PREMIUM_SERVER));
    CHECK(servers->currentIndex() == 0);

    // Another server typed in, and checked
    servers->setEditText(languageTool.url());
    CHECK(page.cardText()->text() ==
          QStringLiteral("The paragraphs checked are sent to the server 127.0.0.1."));
    page.checkRemoteButton()->click();
    CHECK(page.remoteState()->text() == QStringLiteral("Connecting..."));
    CHECK(waitUntil(
        [&page]() {
            return page.remoteState()->text() == QStringLiteral("The server checks the text.");
        },
        ANSWER_MS));
    CHECK(languageTool.texts() == QStringList{QStringLiteral("Test.")});
    CHECK(languageTool.lastUsername().isEmpty());

    // With the account; the check before is dropped
    page.usernameField()->setText(QStringLiteral(" writer@example.com "));
    page.apiKeyField()->setText(QStringLiteral("key-1"));
    CHECK(page.remoteState()->text().isEmpty());
    page.checkRemoteButton()->click();
    CHECK(waitUntil([&languageTool]() { return languageTool.requests() == 2; }, ANSWER_MS));
    CHECK(languageTool.lastUsername() == QStringLiteral("writer@example.com"));
    CHECK(languageTool.lastApiKey() == QStringLiteral("key-1"));

    // A server that does not check
    languageTool.setFailure(500, "Internal error.");
    page.checkRemoteButton()->click();
    CHECK(waitUntil(
        [&page]() {
            return page.remoteState()->text() ==
                   QStringLiteral("The server did not check the text: Internal error.");
        },
        ANSWER_MS));

    // Not a server
    servers->setEditText(QStringLiteral("ftp://localhost"));
    page.checkRemoteButton()->click();
    CHECK(page.remoteState()->text() == QStringLiteral("This is not the address of a server."));

    // Kept without the spaces around
    servers->setEditText(QStringLiteral("  %1  ").arg(languageTool.url()));
    page.apply();
    CHECK(textSetting(GrammarCoordinator::SERVER_KEY) == languageTool.url().toStdString());
    CHECK(textSetting(GrammarCoordinator::USERNAME_KEY) == "writer@example.com");
    CHECK(textSetting(GrammarCoordinator::API_KEY_KEY) == "key-1");

    // The Premium server chosen from the list
    servers->setCurrentIndex(1);
    page.apply();
    CHECK(textSetting(GrammarCoordinator::SERVER_KEY) == GrammarCoordinator::PREMIUM_SERVER);
}

TEST_CASE("Grammar settings: the ignored rules, restored one by one or all",
          "[gui][grammar][settings]") {
    KeptGrammarSettings kept;
    TemporaryGrammarFolder folder;
    setLocal();
    GrammarCoordinator::setIgnoredRules(
        {{QStringLiteral("EN_A_VS_AN"), QStringLiteral("Use of 'a' vs. 'an'")},
         {QStringLiteral("COMMA_RULE"), QString()}});
    EditorGrammarPage page;
    page.load();

    // By what they look for; a rule without it by its name
    QListWidget* rules = page.ignoredRulesList();
    REQUIRE(rules->count() == 2);
    CHECK(rules->item(0)->text() == QStringLiteral("COMMA_RULE"));
    CHECK(rules->item(1)->text() == QStringLiteral("Use of 'a' vs. 'an'"));
    CHECK(rules->item(1)->toolTip() == QStringLiteral("EN_A_VS_AN"));
    CHECK(rules->isVisibleTo(&page));
    CHECK_FALSE(shows(page, QStringLiteral("No rule is ignored.")));
    CHECK_FALSE(page.restoreButton()->isEnabled());
    CHECK(page.restoreAllButton()->isEnabled());

    // One restored
    rules->item(1)->setSelected(true);
    CHECK(page.restoreButton()->isEnabled());
    page.restoreButton()->click();
    REQUIRE(rules->count() == 1);
    CHECK(page.isChanged());
    page.apply();
    const QMap<QString, QString> left{{QStringLiteral("COMMA_RULE"), QString()}};
    CHECK(GrammarCoordinator::ignoredRules() == left);

    // All restored
    page.restoreAllButton()->click();
    CHECK(rules->count() == 0);
    CHECK_FALSE(rules->isVisibleTo(&page));
    CHECK(shows(page, QStringLiteral("No rule is ignored.")));
    CHECK_FALSE(page.restoreAllButton()->isEnabled());
    page.apply();
    CHECK(GrammarCoordinator::ignoredRules().isEmpty());
}

TEST_CASE("Grammar settings: the advanced settings for LanguageTool installed by the writer",
          "[gui][grammar][settings]") {
    KeptGrammarSettings kept;
    TemporaryGrammarFolder folder;
    InstalledComponents installed;
    setLocal();
    EditorGrammarPage page;
    page.load();

    // Hidden until opened
    CHECK_FALSE(page.advancedSettings()->isVisibleTo(&page));
    page.advancedToggle()->setChecked(true);
    CHECK(page.advancedSettings()->isVisibleTo(&page));
    CHECK(page.javaStatus()->text() == QStringLiteral("No Java chosen."));
    CHECK(page.folderStatus()->text() == QStringLiteral("No folder chosen."));

    // What the java program is, told when the field is left
    const auto setJava = [&page](const QString& path) {
        page.javaField()->setText(path);
        emit page.javaField()->editingFinished();
    };
    setJava(installed.notJava());
    CHECK(page.javaStatus()->text() == QStringLiteral("This is not Java of version 17 or newer."));
    setJava(installed.oldJava());
    CHECK(page.javaStatus()->text() ==
          QStringLiteral("Java 11 is too old: LanguageTool needs version 17 or newer."));
    setJava(installed.java());
    CHECK(page.javaStatus()->text() == QStringLiteral("Java 21 (Eclipse Temurin)"));

    // What the folder is
    const auto setFolder = [&page](const QString& path) {
        page.folderField()->setText(path);
        emit page.folderField()->editingFinished();
    };
    setFolder(installed.emptyFolder());
    CHECK(page.folderStatus()->text().startsWith(
        QStringLiteral("This folder has no LanguageTool (the file languagetool-server.jar).")));
    setFolder(installed.languageTool());
    CHECK(page.folderStatus()->text() == QStringLiteral("LanguageTool 6.6"));

    // Kept without the spaces around
    page.javaField()->setText(QStringLiteral(" %1 ").arg(installed.java()));
    page.portBox()->setValue(8090);
    page.externalBox()->setChecked(true);
    CHECK(page.isChanged());
    page.apply();
    CHECK(textSetting(GrammarCoordinator::JAVA_KEY) == installed.java().toStdString());
    CHECK(textSetting(GrammarCoordinator::FOLDER_KEY) == installed.languageTool().toStdString());
    CHECK(settings().get<int>(GrammarCoordinator::PORT_KEY, 0) == 8090);
    CHECK(settings().get<bool>(GrammarCoordinator::EXTERNAL_KEY, false));

    // Not for a server in the internet
    page.setRemoteWithoutAsking(true);
    CHECK_FALSE(page.advancedSettings()->isVisibleTo(&page));
}

TEST_CASE("Grammar settings: the state of LanguageTool on this computer, checked with the "
          "settings applied",
          "[gui][grammar][settings]") {
    KeptGrammarSettings kept;
    TemporaryGrammarFolder folder;
    FakeLanguageTool languageTool;
    setLocal(false);
    settings().set<bool>(GrammarCoordinator::EXTERNAL_KEY, true);
    settings().set<int>(GrammarCoordinator::PORT_KEY, languageTool.port());
    GrammarCoordinator coordinator(nullptr, nullptr);
    coordinator.server()->setStartTimeout(1500);
    EditorGrammarPage page(&coordinator);
    page.load();
    page.advancedToggle()->setChecked(true);
    CHECK(page.serverState()->text() == QStringLiteral("The server does not run."));
    CHECK(page.checkServerButton()->isEnabled());

    // Changes not applied: the button waits for them
    page.portBox()->setValue(languageTool.port() + 1);
    CHECK_FALSE(page.checkServerButton()->isEnabled());
    CHECK(page.checkServerButton()->toolTip() ==
          QStringLiteral("Apply the changes to check the server with them."));
    page.portBox()->setValue(languageTool.port());
    CHECK(page.checkServerButton()->isEnabled());

    // Checked: it runs
    page.checkServerButton()->click();
    CHECK(page.serverState()->text() == QStringLiteral("Starting the server..."));
    CHECK_FALSE(page.checkServerButton()->isEnabled());
    CHECK(waitUntil(
        [&page]() {
            return page.serverState()->text() ==
                   QStringLiteral("The server runs - started outside the program.");
        },
        ANSWER_MS));
    CHECK_FALSE(page.checkServerButton()->isEnabled());

    // Another port, where nothing answers, applied: the server is checked there
    const int nothing = freePort();
    page.portBox()->setValue(nothing);
    page.apply();
    REQUIRE(
        waitUntil([&coordinator, nothing]() { return coordinator.server()->port() == nothing; }));
    CHECK(page.serverState()->text() == QStringLiteral("The server does not run."));
    REQUIRE(page.checkServerButton()->isEnabled());
    page.checkServerButton()->click();
    const QString noServer = QStringLiteral("No server answers on the port %1.").arg(nothing);
    CHECK(waitUntil([&page, &noServer]() { return page.serverState()->text() == noServer; },
                    ANSWER_MS));
    CHECK(page.checkServerButton()->isEnabled());
}

TEST_CASE("Grammar settings: the page is under Editor, after Spelling",
          "[gui][grammar][settings]") {
    KeptGrammarSettings kept;
    TemporaryGrammarFolder folder;
    setLocal();
    GrammarCoordinator coordinator(nullptr, nullptr);
    SettingsDialog dialog(nullptr, false, nullptr, &coordinator);
    auto* tree = dialog.findChild<QTreeWidget*>();
    REQUIRE(tree != nullptr);
    QStringList editorPages;
    for (QTreeWidgetItemIterator it(tree); *it; ++it) {
        const QTreeWidgetItem* parent = (*it)->parent();
        if (parent != nullptr && parent->text(0) == QStringLiteral("Editor")) {
            editorPages.append((*it)->text(0));
        }
    }
    CHECK(editorPages.indexOf(QStringLiteral("Grammar")) >
          editorPages.indexOf(QStringLiteral("Spelling")));
    CHECK(editorPages.indexOf(QStringLiteral("Spelling")) >= 0);

    openPage(dialog, QStringLiteral("Editor"), QStringLiteral("Grammar"));
    auto* page = dialog.findChild<EditorGrammarPage*>();
    REQUIRE(page != nullptr);
    CHECK(page->prepareButton()->isVisibleTo(page));
    CHECK_FALSE(dialog.hasChanges());
}
