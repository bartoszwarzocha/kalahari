// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file publishing_names.h
/// @brief The texts for publishers in the program's words: the names of their parts and
/// states, and the length of a synopsis in standard pages

#ifndef KALAHARI_GUI_PUBLISHING_NAMES_H
#define KALAHARI_GUI_PUBLISHING_NAMES_H

#include <QCoreApplication>
#include <QString>
#include <QStringList>

namespace kalahari::core {
class ProjectManager;
}  // namespace kalahari::core

namespace kalahari::gui {

/// @brief The words of the texts for publishers (core::PublishingTexts) in the program's
/// language
class PublishingNames {
    Q_DECLARE_TR_FUNCTIONS(kalahari::gui::PublishingNames)

public:
    /// @brief Name of @p part of a text or of a set: "Sentence about the book", "Synopsis",
    /// "Tagline", "Cover description", "About the author"; empty for an unknown part
    static QString partName(const QString& part);

    /// @brief Name of @p state: "draft" for none, "chosen for the set", "rejected"; a state
    /// this version does not know as it is written
    static QString stateName(const QString& state);

    /// @brief The short name of @p state in a list: "★ chosen", "draft", "rejected"
    static QString shortStateName(const QString& state);

    /// @brief Name of the group of the texts of @p part in a list: "Sentences about the book",
    /// "Synopses"; of the descriptions and the taglines of the cover, their group of the cover
    /// "from the Cover"; empty for an unknown part
    static QString groupName(const QString& part);

    /// @brief Icon of a text of @p part; of a description or a tagline, that of the cover;
    /// the icon of the kind for an unknown part
    static QString partIconId(const QString& part);

    /// @brief Title of a new text of @p part in the book open in @p book: "Synopsis – version
    /// 2", the number after those of its part that the book has
    static QString newTitle(const core::ProjectManager& book, const QString& part);

    /// @brief Name of template @p reference in the book open in @p book, in the program's
    /// language: "One Page – Five Steps"; "No template" for none; the reference of a template
    /// the book does not have
    static QString templateName(const core::ProjectManager& book, const QString& reference);

    /// @brief The length @p length with its target @p target in a few characters: "320 / 1,800
    /// ch.", "50 w." without a target
    static QString lengthText(int length, int target, bool words);

    /// @brief The status bar after text @p title got @p state (core::PublishingTexts::CHOSEN,
    /// REJECTED; empty: a draft), and the texts @p drafts that are drafts again
    static QString stateMessage(const QString& title, const QString& state,
                                const QStringList& drafts);

    /// @brief The standard pages of @p characters characters with spaces in a sentence:
    /// "about 0.6 of a standard page", "about 2.5 standard pages"
    static QString pagesText(int characters);
};

}  // namespace kalahari::gui

#endif  // KALAHARI_GUI_PUBLISHING_NAMES_H
