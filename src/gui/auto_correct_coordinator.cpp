/// @file auto_correct_coordinator.cpp
/// @brief Auto-correct as you type turned on and off as a whole: the command, its check mark
///        and its tooltip

#include "kalahari/gui/auto_correct_coordinator.h"
#include "kalahari/core/logger.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/gui/command_registry.h"
#include "kalahari/gui/utils/setting_toggle.h"

#include <QMetaObject>
#include <QStatusBar>
#include <QStringList>

#include <string>

namespace kalahari::gui {

namespace {

/// @brief The command that turns auto-correct on and off
constexpr const char* COMMAND_ID = "tools.autocorrect";

/// @brief The settings of auto-correct start with it
constexpr const char* KEY_PREFIX = "editor.autoCorrect.";

/// @brief How long the message of the command stays in the status bar (ms), as the
///        messages of the other switches
constexpr int TOGGLE_MESSAGE_MS = 2000;

/// @brief A setting of auto-correct (the name after the prefix)
bool autoCorrectSetting(const char* name) {
    return core::SettingsManager::getInstance().get<bool>(std::string(KEY_PREFIX) + name);
}

}  // namespace

AutoCorrectCoordinator::AutoCorrectCoordinator(QStatusBar* statusBar, QObject* parent)
    : QObject(parent), m_statusBar(statusBar) {
    // The tooltip follows the settings, also as the Settings dialog changes them; the
    // listener runs on the thread that changes them, the tooltip is given on this one
    m_settingsListener =
        core::SettingsManager::getInstance().subscribe([this](const std::string& key) {
            if (key.rfind(KEY_PREFIX, 0) == 0) {
                QMetaObject::invokeMethod(this, "updateToolTip", Qt::QueuedConnection);
            }
        });
}

AutoCorrectCoordinator::~AutoCorrectCoordinator() {
    core::SettingsManager::getInstance().unsubscribe(m_settingsListener);

    // The command outlives the coordinator
    if (Command* command = CommandRegistry::getInstance().getCommand(COMMAND_ID)) {
        command->execute = nullptr;
    }
}

void AutoCorrectCoordinator::connectCommands() {
    auto& registry = CommandRegistry::getInstance();
    if (Command* command = registry.getCommand(COMMAND_ID)) {
        command->execute = [this]() { toggle(); };
    }

    // Checked while auto-correct is on, also after the Settings dialog changes it
    utils::followSetting(registry.getAction(std::string(COMMAND_ID)), ENABLED_KEY);
    updateToolTip();
}

void AutoCorrectCoordinator::toggle() {
    auto& settings = core::SettingsManager::getInstance();

    // A setting of all editors, kept between sessions; the replacements chosen stay
    const bool enabled = !settings.get<bool>(ENABLED_KEY);
    settings.set<bool>(ENABLED_KEY, enabled);
    core::Logger::getInstance().info("Action triggered: Auto-correct as You Type {}",
                                     enabled ? "on" : "off");
    if (m_statusBar != nullptr) {
        m_statusBar->showMessage(enabled ? tr("Auto-correct as you type: on")
                                         : tr("Auto-correct as you type: off"),
                                 TOGGLE_MESSAGE_MS);
    }
}

QString AutoCorrectCoordinator::toolTip() {
    // The replacements chosen, in the order of the Settings page
    QStringList replacements;
    if (autoCorrectSetting("quotes")) {
        replacements << tr("typographic quotes");
    }
    if (autoCorrectSetting("apostrophe")) {
        replacements << tr("apostrophe ’");
    }
    if (autoCorrectSetting("dialogueDash")) {
        const bool enDash = core::SettingsManager::getInstance().get<std::string>(
                                std::string(KEY_PREFIX) + "dialogueDashType") == "enDash";
        replacements << (enDash ? tr("dialogue dash –") : tr("dialogue dash —"));
    }
    if (autoCorrectSetting("ellipsis")) {
        replacements << tr("ellipsis …");
    }

    QString state;
    const bool enabled = autoCorrectSetting("enabled");
    if (replacements.isEmpty()) {
        state = enabled ? tr("No replacement is chosen") : tr("Off. No replacement is chosen");
    } else if (enabled) {
        //: %1 is the list of the replacements, as "typographic quotes, ellipsis …"
        state = tr("Active replacements: %1").arg(replacements.join(QStringLiteral(", ")));
    } else {
        //: %1 is the list of the replacements, as "typographic quotes, ellipsis …"
        state = tr("Off. When turned on: %1").arg(replacements.join(QStringLiteral(", ")));
    }

    QStringList lines;
    if (const Command* command = CommandRegistry::getInstance().getCommand(COMMAND_ID)) {
        lines << QString::fromStdString(command->label);
    }
    lines << state << tr("The replacements are chosen in Edit > Settings..., group Editor > "
                         "Auto-correct.");
    return lines.join(QLatin1Char('\n'));
}

void AutoCorrectCoordinator::updateToolTip() {
    auto& registry = CommandRegistry::getInstance();
    if (Command* command = registry.getCommand(COMMAND_ID)) {
        command->tooltip = toolTip().toStdString();
        registry.updateActionState(COMMAND_ID);
    }
}

}  // namespace kalahari::gui
