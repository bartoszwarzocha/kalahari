/// @file shortcut_settings.cpp
/// @brief The user's keys of the commands in the settings and in the files

#include "kalahari/gui/shortcut_settings.h"
#include "kalahari/gui/shortcut_rules.h"
#include "kalahari/core/logger.h"
#include "kalahari/core/settings_manager.h"

#include <QKeySequence>

#include <map>

namespace kalahari {
namespace gui {

namespace {

/// What the files of Export say they are
constexpr const char* FILE_FORMAT = "kalahari-keyboard-shortcuts";
constexpr int FILE_VERSION = 1;

}  // namespace

std::string portableKeys(const KeyboardShortcut& keys) {
    if (keys.isEmpty()) {
        return {};
    }
    return keys.toQKeySequence().toString(QKeySequence::PortableText).toStdString();
}

nlohmann::json shortcutsToJson(const CommandRegistry::ShortcutMap& custom) {
    nlohmann::json json = nlohmann::json::object();
    for (const auto& [id, keys] : custom) {
        json[id] = portableKeys(keys);
    }
    return json;
}

CommandRegistry::ShortcutMap shortcutsFromJson(const nlohmann::json& json,
                                               QStringList* unreadable) {
    CommandRegistry::ShortcutMap custom;
    if (!json.is_object()) {
        return custom;
    }
    for (const auto& [id, value] : json.items()) {
        if (!value.is_string()) {
            if (unreadable != nullptr) {
                unreadable->append(QString::fromStdString(id));
            }
            continue;
        }
        const QString text = QString::fromStdString(value.get<std::string>()).trimmed();
        if (text.isEmpty()) {
            custom[id] = KeyboardShortcut();  // the user left the command without keys
            continue;
        }
        const QKeySequence keys = QKeySequence::fromString(text, QKeySequence::PortableText);
        // One key press: a shortcut of several presses (Ctrl+K, Ctrl+C) is not one of ours
        if (keys.count() != 1 || keys[0].key() == Qt::Key_unknown) {
            if (unreadable != nullptr) {
                unreadable->append(QString::fromStdString(id));
            }
            continue;
        }
        custom[id] = KeyboardShortcut::fromQKeySequence(keys);
    }
    return custom;
}

CommandRegistry::ShortcutMap validShortcuts(const CommandRegistry::ShortcutMap& custom,
                                            const ShortcutRules& rules, QStringList* dropped) {
    CommandRegistry::ShortcutMap valid;
    std::map<KeyboardShortcut, std::string> owners;
    for (const auto& [id, keys] : custom) {
        if (keys.isEmpty()) {
            valid[id] = keys;
            continue;
        }
        const KeyCheck check = rules.check(keys.toQKeySequence()[0], id);
        if (check.result == KeyCheck::Result::Refused) {
            if (dropped != nullptr) {
                dropped->append(
                    QStringLiteral("%1: %2").arg(QString::fromStdString(id), check.reason));
            }
            continue;
        }
        const auto [owner, first] = owners.emplace(keys, id);
        if (!first) {
            if (dropped != nullptr) {
                dropped->append(QStringLiteral("%1: %2 is also given to %3")
                                    .arg(QString::fromStdString(id),
                                         QString::fromStdString(portableKeys(keys)),
                                         QString::fromStdString(owner->second)));
            }
            continue;
        }
        valid[id] = keys;
    }
    return valid;
}

CommandRegistry::ShortcutMap loadCustomShortcuts() {
    auto& logger = core::Logger::getInstance();
    const nlohmann::json stored =
        core::SettingsManager::getInstance().get<nlohmann::json>(SHORTCUTS_SETTING);

    QStringList unreadable;
    const CommandRegistry::ShortcutMap custom = shortcutsFromJson(stored, &unreadable);
    for (const QString& id : unreadable) {
        logger.warn("Keyboard shortcuts: the keys of '{}' in the settings are not keys",
                    id.toStdString());
    }

    QStringList dropped;
    CommandRegistry::ShortcutMap valid = validShortcuts(custom, ShortcutRules(), &dropped);
    for (const QString& reason : dropped) {
        logger.warn("Keyboard shortcuts: left out {}", reason.toStdString());
    }
    logger.info("Keyboard shortcuts: {} changed by the user", valid.size());
    return valid;
}

nlohmann::json shortcutsFile(const CommandRegistry::ShortcutMap& custom) {
    return {{"format", FILE_FORMAT},
            {"version", FILE_VERSION},
            {"shortcuts", shortcutsToJson(custom)}};
}

std::optional<nlohmann::json> shortcutsOfFile(const nlohmann::json& file) {
    if (!file.is_object()) {
        return std::nullopt;
    }
    if (file.contains("shortcuts")) {
        const nlohmann::json& shortcuts = file.at("shortcuts");
        if (shortcuts.is_object()) {
            return shortcuts;
        }
        return std::nullopt;
    }
    // An object of command ids and keys, as the settings keep it
    for (const auto& item : file.items()) {
        if (!item.value().is_string()) {
            return std::nullopt;
        }
    }
    return file;
}

} // namespace gui
} // namespace kalahari
