// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file cover_names.h
/// @brief The materials of the cover in the program's words: the names of their parts, places,
/// states and consents, their icons and the lengths of their texts

#pragma once

#include "kalahari/core/book_type_registry.h"
#include "kalahari/core/cover_material.h"

#include <QCoreApplication>
#include <QString>
#include <QStringList>

namespace kalahari {
namespace core {
class ProjectManager;
struct ProjectElement;
} // namespace core

namespace gui {

/// @brief The words of the materials of the cover (core::CoverMaterial) in the program's
/// language
class CoverNames {
    Q_DECLARE_TR_FUNCTIONS(kalahari::gui::CoverNames)

public:
    /// @brief Whether @p kind is the kind of the materials of the cover of the base package
    static bool isCoverKind(const core::KindRef& kind);

    /// @brief Whether @p element is a material of the cover of the book open in @p book
    static bool isCoverMaterial(const core::ProjectManager& book,
                                const core::ProjectElement& element);

    /// @brief Name of @p part: "Description for the cover", "Tagline"...; empty for an unknown
    /// part
    static QString partName(const QString& part);

    /// @brief Name of the group of the materials of @p part in a list: "Cover descriptions";
    /// "Other" for an unknown part
    static QString groupName(const QString& part);

    /// @brief Title of a new material of @p part in the book open in @p book: "Opinion 4", the
    /// number after those of its part; empty for an unknown part
    static QString newTitle(const core::ProjectManager& book, const QString& part);

    /// @brief Icon of a material of @p part; empty for an unknown part
    static QString partIconId(const QString& part);

    /// @brief Name of @p place: "Front", "Back"...; "Not set" for none; a place this version
    /// does not know as it is written
    static QString placeName(const QString& place);

    /// @brief Name of @p state: "draft" for none, "chosen for the cover", "rejected"; a state
    /// this version does not know as it is written
    static QString stateName(const QString& state);

    /// @brief The short name of @p state in a list: "draft", "★ chosen", "rejected"
    static QString shortStateName(const QString& state);

    /// @brief Name of @p consent: "granted", "asked for", "refused"; "not said" for none; a
    /// consent this version does not know as it is written
    static QString consentName(const QString& consent);

    /// @brief The short name of @p consent in a list: "given", "waiting", "missing"
    static QString shortConsentName(const QString& consent);

    /// @brief The length @p length in a few characters: "311 ch." or "50 w."
    static QString lengthText(int length, bool words);

    /// @brief The counts of a text in a sentence: "311 characters with spaces · 50 words"
    static QString countsText(int characters, int words);

    /// @brief The target @p target in a sentence: "target: up to 600 characters"
    static QString targetText(int target, bool words);

    /// @brief How much a text is past its target, in a sentence: "12 characters too many"
    static QString overText(int excess, bool words);

    /// @brief What the status bar says after material @p title of the cover got @p state:
    /// "Chosen for the cover: "Description 2"", with the materials @p drafts (their titles)
    /// that are drafts again
    static QString stateMessage(const QString& title, const QString& state,
                                const QStringList& drafts);

    /// @brief The unit of the lengths in a few words: "characters" or "words"
    static QString unitName(bool words);

    /// @brief How an opinion is printed on the cover: its words in quotes, then a dash, its
    /// author and who the author is
    static QString printedOpinion(const QString& text, const core::CoverData& cover);
};

} // namespace gui
} // namespace kalahari
