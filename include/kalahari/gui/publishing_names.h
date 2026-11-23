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
struct KindRef;
struct PublishingData;
struct PublishingTemplate;
}  // namespace kalahari::core

namespace kalahari::gui {

/// @brief The words of the texts for publishers (core::PublishingTexts) in the program's
/// language
class PublishingNames {
    Q_DECLARE_TR_FUNCTIONS(kalahari::gui::PublishingNames)

public:
    /// @brief Whether @p kind is that of the texts for publishers of the base package
    static bool isPublishingKind(const core::KindRef& kind);

    /// @brief Name of @p part of a text or of a set: "Sentence about the book", "Synopsis",
    /// "Tagline", "Cover description", "About the author"; empty for an unknown part
    static QString partName(const QString& part);

    /// @brief Name of @p state: "draft" for none, "chosen for the set", "rejected"; a state
    /// this version does not know as it is written
    static QString stateName(const QString& state);

    /// @brief Icon of a text of @p part; of a description or a tagline, that of the cover;
    /// the icon of the kind for an unknown part
    static QString partIconId(const QString& part);

    /// @brief Title of a new text of @p part in the book open in @p book: "Synopsis – version
    /// 2", the number after those of its part that the book has
    static QString newTitle(const core::ProjectManager& book, const QString& part);

    /// @brief The name of no template: "No template"
    static QString noTemplateName();

    /// @brief The length @p length with its target @p target in a few characters: "320 / 1,800
    /// ch.", "50 w." without a target
    static QString lengthText(int length, int target, bool words);

    /// @brief The standard pages of @p characters characters with spaces in a sentence:
    /// "about 0.6 of a standard page", "about 2.5 standard pages"
    static QString pagesText(int characters);

    /// @brief Where the target length @p target of text @p data comes from, in the unit
    /// @p inWords: "The target length of this text.", "The target length of the template
    /// \"One page – five steps\": 1,800 characters", "The target length from the Settings: ..."
    /// @param templ Its template; nullptr for none
    static QString targetSourceText(const core::PublishingData& data,
                                    const core::PublishingTemplate* templ, int target,
                                    bool inWords);
};

}  // namespace kalahari::gui

#endif  // KALAHARI_GUI_PUBLISHING_NAMES_H
