/// @file settings_coordinator.cpp
/// @brief Settings dialog coordination implementation
///
/// OpenSpec #00038 - Phase 5: Extract Settings Management from MainWindow

#include "kalahari/gui/settings_coordinator.h"
#include "kalahari/gui/command_registry.h"
#include "kalahari/gui/settings_dialog.h"
#include "kalahari/gui/shortcut_settings.h"
#include "kalahari/gui/dialogs/message_dialog.h"
#include "kalahari/gui/dock_coordinator.h"
#include "kalahari/gui/panel_tabs.h"
#include "kalahari/gui/panels/dashboard_panel.h"
#include "kalahari/gui/panels/log_panel.h"
#include "kalahari/gui/widgets/central_tabs.h"
#include "kalahari/core/logger.h"
#include "kalahari/core/settings_manager.h"
#include <QApplication>
#include <QStringList>

#include <algorithm>
#include <QMainWindow>
#include <QStatusBar>

namespace kalahari {
namespace gui {

SettingsCoordinator::SettingsCoordinator(QMainWindow* mainWindow,
                                          DockCoordinator* dockCoordinator,
                                          QStatusBar* statusBar,
                                          QObject* parent)
    : QObject(parent)
    , m_mainWindow(mainWindow)
    , m_dockCoordinator(dockCoordinator)
    , m_statusBar(statusBar)
    , m_diagnosticModeGetter([]() { return false; })
{
    auto& logger = core::Logger::getInstance();
    logger.debug("SettingsCoordinator: Created");
}

void SettingsCoordinator::setDiagnosticModeGetter(std::function<bool()> callback) {
    m_diagnosticModeGetter = std::move(callback);
}

void SettingsCoordinator::setSpellCheckService(editor::SpellCheckService* service) {
    m_spellCheckService = service;
}

void SettingsCoordinator::setGrammarCoordinator(GrammarCoordinator* grammar) {
    m_grammarCoordinator = grammar;
}

void SettingsCoordinator::openSettingsDialog() {
    core::Logger::getInstance().info("Action triggered: Settings");
    runSettingsDialog(QString());
}

void SettingsCoordinator::openKeyboardShortcuts() {
    core::Logger::getInstance().info("Action triggered: Keyboard Shortcuts");
    runSettingsDialog(QString::fromLatin1(SettingsDialog::SHORTCUTS_PAGE));
}

void SettingsCoordinator::openSettingsPage(const QString& page) {
    core::Logger::getInstance().info("Action triggered: Settings at the page {}",
                                     page.toStdString());
    runSettingsDialog(page);
}

void SettingsCoordinator::runSettingsDialog(const QString& page) {
    auto& logger = core::Logger::getInstance();

    SettingsDialog dialog(m_mainWindow, m_diagnosticModeGetter(), m_spellCheckService,
                          m_grammarCoordinator);
    if (!page.isEmpty()) {
        dialog.openPage(page);
    }
    connect(&dialog, &SettingsDialog::settingsApplied,
            this, &SettingsCoordinator::onApplySettings);
    connect(&dialog, &SettingsDialog::diagnosticModeChanged, this, [this](bool enabled) {
        if (enabled) {
            emit enableDiagnosticModeRequested();
        } else {
            emit disableDiagnosticModeRequested();
        }
    });

    m_languageChanged = false;
    int result = dialog.exec();

    if (result == QDialog::Accepted) {
        logger.info("Settings dialog: OK clicked");
        if (m_statusBar) {
            m_statusBar->showMessage(QObject::tr("Settings applied"), 2000);
        }
    } else {
        logger.info("Settings dialog: Cancel clicked (changes discarded)");
        if (m_statusBar) {
            m_statusBar->showMessage(QObject::tr("Settings changes discarded"), 2000);
        }
    }

    // Apply may have changed the language even if the dialog was then cancelled
    if (m_languageChanged) {
        offerRestartForLanguage();
    }
}

void SettingsCoordinator::offerRestartForLanguage() {
    const bool restart = dialogs::MessageDialog::confirm(
        m_mainWindow,
        QObject::tr("Language Changed"),
        QObject::tr("The new language will be used after restarting Kalahari.\n\nRestart now?"),
        QObject::tr("&Restart Now"));
    if (!restart) {
        return;
    }

    // main() starts a new instance once the event loop ends. Closing the window asks
    // about unsaved changes as usual; if the user cancels there, nothing restarts.
    qApp->setProperty("kalahari.restartRequested", true);
    if (!m_mainWindow->close()) {
        qApp->setProperty("kalahari.restartRequested", false);
    }
}

void SettingsCoordinator::onApplySettings(const QStringList& changedKeys) {
    auto& logger = core::Logger::getInstance();
    logger.info("SettingsCoordinator: Reacting to {} applied settings", changedKeys.size());

    const auto changed = [&changedKeys](const QString& prefix) {
        return std::any_of(changedKeys.cbegin(), changedKeys.cend(),
                           [&prefix](const QString& key) { return key.startsWith(prefix); });
    };

    if (changed(QStringLiteral("ui.language"))) {
        m_languageChanged = true;
    }

    // The keys of the commands: the menus, the toolbars and the texts that show them follow
    // CommandRegistry::shortcutsChanged()
    if (changed(QString::fromLatin1(SHORTCUTS_SETTING))) {
        CommandRegistry::getInstance().setCustomShortcuts(loadCustomShortcuts());
        logger.info("SettingsCoordinator: Keyboard shortcuts updated");
    }

    // Log panel: buffer size and colors (theme switch or per-theme log colors, not the editor
    // colors kept for each theme as well)
    LogPanel* logPanel = m_dockCoordinator->logPanel();
    if (logPanel && changed(QStringLiteral("log.bufferSize"))) {
        const int bufferSize = core::SettingsManager::getInstance().get<int>("log.bufferSize");
        logPanel->setMaxBufferSize(static_cast<size_t>(bufferSize));
        logger.info("SettingsCoordinator: Log buffer size updated to {}", bufferSize);
    }
    const bool logColorChanged =
        std::any_of(changedKeys.cbegin(), changedKeys.cend(), [](const QString& key) {
            return key.startsWith(QStringLiteral("themes.")) &&
                   key.contains(QStringLiteral(".log."));
        });
    if (logPanel && (changed(QStringLiteral("appearance.theme")) || logColorChanged)) {
        logPanel->applyThemeColors();
    }

    // Dashboard: only its own options (it follows theme changes by itself)
    DashboardPanel* dashboardPanel = m_dockCoordinator->dashboardPanel();
    if (dashboardPanel && changed(QStringLiteral("dashboard."))) {
        dashboardPanel->onSettingsChanged();
    }

    // The Paragraph and Styles panel moved to its other place: it shows there, in front
    if (changed(QString::fromLatin1(DockCoordinator::PARAGRAPH_PLACE_KEY))) {
        m_dockCoordinator->placeParagraphPanel(true);
    }

    // The tabs of the panels: what they show, their rows, their side
    if (changed(QStringLiteral("panels.tab")) && m_dockCoordinator->panelTabs() != nullptr) {
        m_dockCoordinator->panelTabs()->applySettings();
    }

    // The tabs in the middle of the window: one row or more
    auto* centralTabs = qobject_cast<CentralTabs*>(m_dockCoordinator->centralTabs());
    if (changed(QString::fromLatin1(CentralTabs::ROWS_KEY)) && centralTabs != nullptr) {
        centralTabs->applySettings();
    }

    // The editors follow the editor settings by themselves (EditorPanel)
}

} // namespace gui
} // namespace kalahari
