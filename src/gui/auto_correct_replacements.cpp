/// @file auto_correct_replacements.cpp
/// @brief The replacement table of auto-correct in the settings

#include "kalahari/gui/auto_correct_replacements.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/core/settings_schema.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <string>

namespace kalahari {
namespace gui {

namespace {

/// @brief The pairs of a value of the setting
std::vector<ReplacementEntry> entriesOf(const nlohmann::json& pairs)
{
    std::vector<ReplacementEntry> entries;
    if (!pairs.is_array()) {
        return entries;
    }
    for (const nlohmann::json& pair : pairs) {
        if (!pair.is_object() || !pair.value("from", nlohmann::json()).is_string() ||
            !pair.value("to", nlohmann::json()).is_string()) {
            continue;
        }
        ReplacementEntry entry;
        entry.from = QString::fromStdString(pair.at("from").get<std::string>()).trimmed();
        entry.to = QString::fromStdString(pair.at("to").get<std::string>());
        const nlohmann::json enabled = pair.value("enabled", nlohmann::json(true));
        entry.enabled = !enabled.is_boolean() || enabled.get<bool>();
        if (!entry.from.isEmpty()) {
            entries.push_back(entry);
        }
    }
    return entries;
}

} // namespace

std::vector<ReplacementEntry> replacementEntries()
{
    return entriesOf(
        core::SettingsManager::getInstance().get<nlohmann::json>(REPLACEMENTS_SETTING));
}

std::vector<ReplacementEntry> defaultReplacementEntries()
{
    const nlohmann::json* defaults = core::settings_schema::defaultValue(REPLACEMENTS_SETTING);
    return defaults != nullptr ? entriesOf(*defaults) : std::vector<ReplacementEntry>();
}

void setReplacementEntries(const std::vector<ReplacementEntry>& entries)
{
    nlohmann::json pairs = nlohmann::json::array();
    for (const ReplacementEntry& entry : entries) {
        pairs.push_back({{"from", entry.from.toStdString()},
                         {"to", entry.to.toStdString()},
                         {"enabled", entry.enabled}});
    }
    core::SettingsManager::getInstance().set<nlohmann::json>(REPLACEMENTS_SETTING, pairs);
}

void addReplacement(const QString& from, const QString& to)
{
    std::vector<ReplacementEntry> entries = replacementEntries();
    const QString word = from.trimmed();
    const auto found = std::find_if(entries.begin(), entries.end(),
                                    [&word](const ReplacementEntry& entry) {
                                        return entry.from == word;
                                    });
    if (found != entries.end()) {
        found->to = to;
        found->enabled = true;
    } else {
        entries.push_back({word, to, true});
    }
    setReplacementEntries(entries);
}

QStringList autoCorrectSources(const QString& text)
{
    auto& settings = core::SettingsManager::getInstance();
    QStringList sources;
    if (!settings.get<bool>("editor.autoCorrect.enabled", true)) {
        return sources;
    }
    if (text == QStringLiteral("–") &&
        settings.get<bool>("editor.autoCorrect.doubleHyphen", true)) {
        sources.append(QStringLiteral("--"));
    }
    if (text == QStringLiteral("…") && settings.get<bool>("editor.autoCorrect.ellipsis", true)) {
        sources.append(QStringLiteral("..."));
    }
    if (settings.get<bool>("editor.autoCorrect.replacementTable", true)) {
        for (const ReplacementEntry& entry : replacementEntries()) {
            if (entry.enabled && entry.to == text && !sources.contains(entry.from)) {
                sources.append(entry.from);
            }
        }
    }
    return sources;
}

} // namespace gui
} // namespace kalahari
