/// @file setting_toggle.cpp
/// @brief Implementation of the toggles of on/off settings

#include "kalahari/gui/utils/setting_toggle.h"

#include "kalahari/core/settings_manager.h"

#include <QAction>
#include <QMetaObject>

namespace kalahari {
namespace gui {
namespace utils {

void followSetting(QAction* action, const std::string& key) {
    if (action == nullptr) {
        return;
    }
    auto& settings = core::SettingsManager::getInstance();
    action->setCheckable(true);
    action->setChecked(settings.get<bool>(key));

    // A listener runs on the thread that changed the setting. The slot called by name
    // runs at once on the action's own thread and is queued from any other one.
    const int id = settings.subscribe([action, key](const std::string& changed) {
        if (changed == key) {
            const bool on = core::SettingsManager::getInstance().get<bool>(key);
            QMetaObject::invokeMethod(action, "setChecked", Q_ARG(bool, on));
        }
    });
    QObject::connect(action, &QObject::destroyed, [id]() {
        core::SettingsManager::getInstance().unsubscribe(id);
    });
}

void followChoice(QAction* action, const std::string& key, const std::string& value) {
    if (action == nullptr) {
        return;
    }
    const auto chosen = [key, value]() {
        return core::SettingsManager::getInstance().get<std::string>(key) == value;
    };
    action->setCheckable(true);
    action->setChecked(chosen());

    // As followSetting()
    auto& settings = core::SettingsManager::getInstance();
    const int id = settings.subscribe([action, key, chosen](const std::string& changed) {
        if (changed == key) {
            QMetaObject::invokeMethod(action, "setChecked", Q_ARG(bool, chosen()));
        }
    });
    QObject::connect(action, &QObject::destroyed, [id]() {
        core::SettingsManager::getInstance().unsubscribe(id);
    });

    // A click toggles the action before its command runs; a command that leaves the setting
    // as it was changes nothing the action would follow. Connected after the command (the
    // registry connects it when it makes the action), this runs after it.
    QObject::connect(action, &QAction::triggered, action,
                     [action, chosen]() { action->setChecked(chosen()); });
}

std::string chooseSetting(const std::string& key, const std::string& value, const std::string& off) {
    auto& settings = core::SettingsManager::getInstance();
    const std::string chosen = settings.get<std::string>(key) == value ? off : value;
    settings.set<std::string>(key, chosen);
    return chosen;
}

} // namespace utils
} // namespace gui
} // namespace kalahari
