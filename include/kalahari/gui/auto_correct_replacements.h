/// @file auto_correct_replacements.h
/// @brief The replacement table of auto-correct in the settings: the pairs of a typed word and
///        its replacement, each one on or off

#pragma once

#include <QString>
#include <QStringList>

#include <vector>

namespace kalahari {
namespace gui {

/// @brief The setting with the replacement table
inline constexpr const char* REPLACEMENTS_SETTING = "editor.autoCorrect.replacements";

/// @brief A pair of the replacement table
struct ReplacementEntry {
    QString from;          ///< The word typed: "(c)", "->"
    QString to;            ///< What takes its place: "©", "→"
    bool enabled = true;   ///< Auto-correct makes it

    bool operator==(const ReplacementEntry&) const = default;
};

/// @brief The pairs of the table, in its order; the pairs a file edited by hand spoiled (no
///        word, not texts) are left out
[[nodiscard]] std::vector<ReplacementEntry> replacementEntries();

/// @brief The program's own pairs (Restore Defaults brings them back)
[[nodiscard]] std::vector<ReplacementEntry> defaultReplacementEntries();

/// @brief Keep the pairs in the settings; the editors take them at once
void setReplacementEntries(const std::vector<ReplacementEntry>& entries);

/// @brief Add a pair to the table, on, or give the word of a pair a new replacement (and turn
///        it on)
void addReplacement(const QString& from, const QString& to);

/// @brief What auto-correct replaces with a text as it is set now: the words of the pairs
///        turned on, and the two hyphens of the en dash and the three periods of the ellipsis;
///        nothing while auto-correct is off
[[nodiscard]] QStringList autoCorrectSources(const QString& text);

} // namespace gui
} // namespace kalahari
