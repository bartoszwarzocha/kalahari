/// @file setting_toggle.h
/// @brief Toggles of the View menu and the toolbars that show an on/off setting

#ifndef KALAHARI_GUI_UTILS_SETTING_TOGGLE_H
#define KALAHARI_GUI_UTILS_SETTING_TOGGLE_H

#include <string>

class QAction;

namespace kalahari {
namespace gui {
namespace utils {

/// @brief Keep a toggle checked while an on/off setting is on
///
/// Makes the action checkable and updates it whenever the setting changes, whatever
/// changes it: the toggle itself, the Settings dialog or an editor's context menu.
/// The action stops following the setting when it is destroyed.
/// @param action The toggle
/// @param key The setting (e.g. "editor.darkMode"), with its default in the settings schema
void followSetting(QAction* action, const std::string& key);

} // namespace utils
} // namespace gui
} // namespace kalahari

#endif // KALAHARI_GUI_UTILS_SETTING_TOGGLE_H
