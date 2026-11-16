// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file publishing_names.h
/// @brief The texts for publishers in the program's words: the names of their parts and
/// states, and the length of a synopsis in standard pages

#ifndef KALAHARI_GUI_PUBLISHING_NAMES_H
#define KALAHARI_GUI_PUBLISHING_NAMES_H

#include <QCoreApplication>
#include <QString>

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

    /// @brief The standard pages of @p characters characters with spaces in a sentence:
    /// "about 0.6 of a standard page", "about 2.5 standard pages"
    static QString pagesText(int characters);
};

}  // namespace kalahari::gui

#endif  // KALAHARI_GUI_PUBLISHING_NAMES_H
