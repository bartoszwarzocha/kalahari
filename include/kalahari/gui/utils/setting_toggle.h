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

/// @brief Keep one choice of a setting checked while the setting has its value
///
/// Makes the action checkable and checks it whenever the setting has @p value, whatever
/// changes it. A choice triggered shows the setting again when its command has run (the
/// command decides the value: choosing a checked one again may keep it or turn it off).
/// The action stops following the setting when it is destroyed.
/// @param action The choice (e.g. View > Margin Numbering > Line Numbers)
/// @param key The setting, a text with its default in the settings schema
/// @param value The value the choice stands for
void followChoice(QAction* action, const std::string& key, const std::string& value);

/// @brief Choose a value of a setting, as a toolbar button of a choice does: the value that
///        the setting has already, chosen again, gives it @p off
/// @param key The setting, a text with its default in the settings schema
/// @param value The value chosen
/// @param off The value of no choice (e.g. "none")
/// @return The value the setting has now
std::string chooseSetting(const std::string& key, const std::string& value, const std::string& off);

} // namespace utils
} // namespace gui
} // namespace kalahari

#endif // KALAHARI_GUI_UTILS_SETTING_TOGGLE_H
