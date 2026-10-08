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

} // namespace utils
} // namespace gui
} // namespace kalahari
