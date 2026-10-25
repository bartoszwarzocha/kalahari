/// @file version_texts.h
/// @brief What the program says of a version of a text (core::ChapterVersion): when it was
/// kept, where it comes from, its words and its icon, alike in the window of the versions, the
/// Properties panel and the status bar

#pragma once

#include <kalahari/core/chapter_versions.h>
#include <kalahari/core/kind_words.h>

#include <QDateTime>
#include <QString>

namespace kalahari {
namespace gui {

/// @brief When a version was kept, in the local time: "today, 14:52", "yesterday, 22:14",
/// "18.10.2026, 20:40"
/// @param created When it was kept
/// @param now The time now (invalid: the time of the computer)
QString versionTime(const QDateTime& created, const QDateTime& now = QDateTime());

/// @brief Where a version comes from: "Saved by you", "When the chapter was saved", "Before
/// Replace All: "Taun" → "Tau", 12 replacements", "Before the version of today, 14:52 was
/// restored"
/// @param version The version
/// @param kind The words of the kind of its text ("chapter")
QString versionOrigin(const core::ChapterVersion& version, const core::KindWords& kind);

/// @brief What a version is called in the lists: its description in quotes, else where it
/// comes from (versionOrigin())
QString versionTitle(const core::ChapterVersion& version, const core::KindWords& kind);

/// @brief The words of a version under the rules of the settings: "2 486 words"
QString versionWords(const core::ChapterVersion& version);

/// @brief @p words words: "2 486 words"
QString wordCountText(int words);

/// @brief The keys of File > Save Version as the writer sees them ("Alt+Shift+S"); without
/// keys, where the command is: "File > Save Version"
QString saveVersionKeys();

/// @brief Whether the program keeps new versions (Settings > Editor > Versions > Create
/// Versions of the Texts); the versions kept can be read and restored either way
bool versionsEnabled();

/// @brief Why Save Version is greyed out while the program keeps no new versions, with where
/// to turn them on
QString versionsOffReason();

/// @brief The icon of a version kept for @p origin: a pen for the writer's own one, a clock
/// for one kept on saving, Find and Replace, Restore
QString versionIconId(core::VersionOrigin origin);

} // namespace gui
} // namespace kalahari
