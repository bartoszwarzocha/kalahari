/// @file grammar_page.cpp
/// @brief Settings page: Editor > Grammar
///
/// The coordinator of the grammar (GrammarCoordinator) follows the settings by itself.

#include "kalahari/gui/settings/settings_pages.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/editor/grammar_check_service.h"
#include "kalahari/gui/spelling_coordinator.h"

#include <QCheckBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>

#include <string>

namespace kalahari {
namespace gui {

namespace {

using json = nlohmann::json;

/// @brief The setting of the LanguageTool server
constexpr const char* SERVER_KEY = "editor.grammarCheck.serverUrl";

}  // namespace

EditorGrammarPage::EditorGrammarPage(QWidget* parent)
    : SettingsPage(parent)
{
    QFormLayout* checking = addGroup(tr("Grammar as You Type"));
    QCheckBox* enabled =
        addCheckBox(checking, tr("Check grammar as you type"), "editor.grammarCheck.enabled");

    // The server, with a button that asks it to check a text
    m_server = new QLineEdit();
    m_server->setPlaceholderText(QStringLiteral("http://localhost:8081"));
    m_server->setAccessibleName(tr("LanguageTool server"));
    m_test = new QPushButton(tr("Test"));
    auto* serverRow = new QWidget();
    auto* row = new QHBoxLayout(serverRow);
    row->setContentsMargins(0, 0, 0, 0);
    row->addWidget(m_server, 1);
    row->addWidget(m_test);
    auto* serverLabel = new QLabel(tr("LanguageTool server:"));
    serverLabel->setBuddy(m_server);
    checking->addRow(serverLabel, serverRow);
    m_testResult = new QLabel();
    m_testResult->setWordWrap(true);
    checking->addRow(QString(), m_testResult);
    addNote(checking, tr("Grammar, style and punctuation are checked by LanguageTool, a free "
                         "program that runs on your computer. The text of the book is sent "
                         "only to the server given here; without one, the grammar is not "
                         "checked. The language is the one of the spelling."));

    auto& settings = core::SettingsManager::getInstance();
    Binding server;
    server.key = []() { return std::string(SERVER_KEY); };
    server.stored = [&settings]() {
        return json(settings.get<std::string>(SERVER_KEY, std::string()));
    };
    server.shown = [this]() { return json(m_server->text().trimmed().toStdString()); };
    server.show = [this](const json& value) {
        m_server->setText(QString::fromStdString(value.is_string() ? value.get<std::string>()
                                                                   : std::string()));
    };
    server.store = [&settings](const json& value) {
        settings.set<std::string>(SERVER_KEY, value.is_string() ? value.get<std::string>()
                                                                : std::string());
    };
    bindCustom(std::move(server));

    connect(m_test, &QPushButton::clicked, this, &EditorGrammarPage::testServer);
    connect(m_server, &QLineEdit::textChanged, this, [this]() { m_testResult->clear(); });

    const auto syncServer = [this, enabled, serverLabel]() {
        m_server->setEnabled(enabled->isChecked());
        m_test->setEnabled(enabled->isChecked());
        serverLabel->setEnabled(enabled->isChecked());
    };
    connect(enabled, &QCheckBox::toggled, this, syncServer);
    whenLoaded(syncServer);
}

void EditorGrammarPage::testServer() {
    // A test asked for before is dropped
    delete m_tester;
    m_tester = new editor::GrammarCheckService(this);
    m_tester->setServer(m_server->text());
    m_tester->setLanguage(SpellingCoordinator::wantedLanguage());
    if (!m_tester->isConfigured()) {
        m_testResult->setText(tr("This is not the address of a server."));
        return;
    }
    connect(m_tester, &editor::GrammarCheckService::textChecked, this,
            [this]() { m_testResult->setText(tr("The server checks the text.")); });
    connect(m_tester, &editor::GrammarCheckService::serverError, this,
            [this](const QString& message) {
                m_testResult->setText(tr("The server did not check the text: %1").arg(message));
            });
    m_testResult->setText(tr("Connecting..."));
    m_tester->check(QStringLiteral("Test."));
}

} // namespace gui
} // namespace kalahari
